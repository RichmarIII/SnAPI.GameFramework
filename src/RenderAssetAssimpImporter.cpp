#include "AssetPipelineIds.h"
#include "RenderAssetImportSettings.h"
#include "RenderAssetPayloads.h"
#include "RenderAssetSourcePayloads.h"

#include "IAssetImporter.h"
#include "IPipelineContext.h"
#include "IPayloadSerializer.h"
#include "TextureCompressorIds.h"
#include "TextureCompressorPayloads.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#if defined(SNAPI_GF_HAS_FREEIMAGE) && SNAPI_GF_HAS_FREEIMAGE
#include <FreeImage.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace SnAPI::GameFramework
{
namespace
{
using SnAPI::AssetPipeline::ImportedItem;
using SnAPI::AssetPipeline::IPipelineContext;
using SnAPI::AssetPipeline::SourceRef;

constexpr std::array<std::string_view, 14> kSupportedModelExtensions{
    ".fbx", ".gltf", ".glb", ".obj", ".dae", ".blend", ".3ds", ".ply",
    ".stl", ".x", ".x3d", ".usd", ".usdz", ".abc"};

[[nodiscard]] std::string ToLowerAscii(std::string_view Value)
{
    std::string Out(Value);
    std::transform(Out.begin(), Out.end(), Out.begin(), [](const unsigned char Ch) {
        return static_cast<char>(std::tolower(Ch));
    });
    return Out;
}

[[nodiscard]] bool EndsWithInsensitive(const std::string& Value, std::string_view Suffix)
{
    if (Suffix.size() > Value.size())
    {
        return false;
    }

    const size_t Offset = Value.size() - Suffix.size();
    for (size_t Index = 0; Index < Suffix.size(); ++Index)
    {
        const char Left = static_cast<char>(std::tolower(static_cast<unsigned char>(Value[Offset + Index])));
        const char Right = static_cast<char>(std::tolower(static_cast<unsigned char>(Suffix[Index])));
        if (Left != Right)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool HasSupportedModelExtension(const std::string& Uri)
{
    for (const std::string_view Ext : kSupportedModelExtensions)
    {
        if (EndsWithInsensitive(Uri, Ext))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string SourceExtensionHint(const std::string& Uri)
{
    std::string Extension = std::filesystem::path(Uri).extension().string();
    if (!Extension.empty() && Extension.front() == '.')
    {
        Extension.erase(Extension.begin());
    }
    return ToLowerAscii(Extension);
}

[[nodiscard]] bool ReadUInt32LE(const std::vector<std::uint8_t>& Bytes, const size_t Offset, std::uint32_t& OutValue)
{
    if (Offset + sizeof(std::uint32_t) > Bytes.size())
    {
        return false;
    }

    std::memcpy(&OutValue, Bytes.data() + Offset, sizeof(std::uint32_t));
    return true;
}

struct ExtractedGlbSource
{
    std::filesystem::path TempDirectory{};
    std::filesystem::path GltfPath{};
};

[[nodiscard]] std::optional<ExtractedGlbSource> ExtractGlbSource(
    const std::string& SourceUri,
    const std::vector<std::uint8_t>& SourceBytes,
    IPipelineContext& Ctx)
{
    constexpr std::uint32_t kGlbMagic = 0x46546C67u;
    constexpr std::uint32_t kGlbVersion2 = 2u;
    constexpr std::uint32_t kJsonChunkType = 0x4E4F534Au;
    constexpr std::uint32_t kBinChunkType = 0x004E4942u;

    std::uint32_t Magic = 0u;
    std::uint32_t Version = 0u;
    std::uint32_t DeclaredLength = 0u;
    if (!ReadUInt32LE(SourceBytes, 0u, Magic) ||
        !ReadUInt32LE(SourceBytes, 4u, Version) ||
        !ReadUInt32LE(SourceBytes, 8u, DeclaredLength))
    {
        Ctx.LogError("RenderAsset Assimp importer failed to parse GLB header for %s", SourceUri.c_str());
        return std::nullopt;
    }

    if (Magic != kGlbMagic || Version != kGlbVersion2)
    {
        Ctx.LogError("RenderAsset Assimp importer encountered unsupported GLB header for %s", SourceUri.c_str());
        return std::nullopt;
    }

    if (DeclaredLength > SourceBytes.size())
    {
        Ctx.LogError("RenderAsset Assimp importer encountered truncated GLB payload for %s", SourceUri.c_str());
        return std::nullopt;
    }

    std::string JsonChunk{};
    std::vector<std::uint8_t> BinChunk{};
    size_t Offset = 12u;
    while (Offset + 8u <= DeclaredLength)
    {
        std::uint32_t ChunkLength = 0u;
        std::uint32_t ChunkType = 0u;
        if (!ReadUInt32LE(SourceBytes, Offset, ChunkLength) ||
            !ReadUInt32LE(SourceBytes, Offset + 4u, ChunkType))
        {
            break;
        }
        Offset += 8u;
        if (Offset + ChunkLength > DeclaredLength)
        {
            Ctx.LogError("RenderAsset Assimp importer encountered invalid GLB chunk bounds for %s", SourceUri.c_str());
            return std::nullopt;
        }

        const auto* ChunkBegin = reinterpret_cast<const char*>(SourceBytes.data() + Offset);
        if (ChunkType == kJsonChunkType)
        {
            JsonChunk.assign(ChunkBegin, ChunkBegin + ChunkLength);
            while (!JsonChunk.empty() && (JsonChunk.back() == '\0' || JsonChunk.back() == ' ' || JsonChunk.back() == '\n' || JsonChunk.back() == '\r' || JsonChunk.back() == '\t'))
            {
                JsonChunk.pop_back();
            }
        }
        else if (ChunkType == kBinChunkType)
        {
            BinChunk.assign(SourceBytes.begin() + static_cast<std::ptrdiff_t>(Offset),
                            SourceBytes.begin() + static_cast<std::ptrdiff_t>(Offset + ChunkLength));
        }

        Offset += ChunkLength;
    }

    if (JsonChunk.empty())
    {
        Ctx.LogError("RenderAsset Assimp importer failed to locate GLB JSON chunk for %s", SourceUri.c_str());
        return std::nullopt;
    }

    nlohmann::ordered_json Document = nlohmann::ordered_json::parse(JsonChunk, nullptr, false);
    if (Document.is_discarded())
    {
        Ctx.LogError("RenderAsset Assimp importer failed to parse GLB JSON chunk for %s", SourceUri.c_str());
        return std::nullopt;
    }

    const std::string SourceStem = std::filesystem::path(SourceUri).stem().string();
    const std::string SafeStem = SourceStem.empty() ? "imported_glb" : SourceStem;

    if (Document.contains("buffers") && Document["buffers"].is_array())
    {
        int EmbeddedBufferIndex = -1;
        std::uint32_t ExpectedByteLength = static_cast<std::uint32_t>(BinChunk.size());
        for (size_t Index = 0; Index < Document["buffers"].size(); ++Index)
        {
            nlohmann::ordered_json& Buffer = Document["buffers"][Index];
            if (!Buffer.is_object() || Buffer.contains("uri"))
            {
                continue;
            }

            if (EmbeddedBufferIndex >= 0)
            {
                Ctx.LogError("RenderAsset Assimp importer encountered multiple embedded GLB buffers for %s", SourceUri.c_str());
                return std::nullopt;
            }

            EmbeddedBufferIndex = static_cast<int>(Index);
            if (Buffer.contains("byteLength") && Buffer["byteLength"].is_number_unsigned())
            {
                ExpectedByteLength = Buffer["byteLength"].get<std::uint32_t>();
            }
        }

        if (EmbeddedBufferIndex >= 0)
        {
            if (BinChunk.empty())
            {
                Ctx.LogError("RenderAsset Assimp importer expected a GLB BIN chunk for %s", SourceUri.c_str());
                return std::nullopt;
            }
            if (ExpectedByteLength > BinChunk.size())
            {
                Ctx.LogError("RenderAsset Assimp importer encountered undersized GLB BIN data for %s", SourceUri.c_str());
                return std::nullopt;
            }

            Document["buffers"][EmbeddedBufferIndex]["uri"] = SafeStem + ".bin";
            nlohmann::ordered_json OrderedBuffer = nlohmann::ordered_json::object();
            OrderedBuffer["uri"] = SafeStem + ".bin";
            for (auto MemberIt = Document["buffers"][EmbeddedBufferIndex].begin();
                 MemberIt != Document["buffers"][EmbeddedBufferIndex].end();
                 ++MemberIt)
            {
                if (MemberIt.key() == "uri")
                {
                    continue;
                }
                OrderedBuffer[MemberIt.key()] = MemberIt.value();
            }
            Document["buffers"][EmbeddedBufferIndex] = std::move(OrderedBuffer);
            BinChunk.resize(ExpectedByteLength);
        }
    }

    const auto Stamp = std::to_string(static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    ExtractedGlbSource Extracted{};
    Extracted.TempDirectory = std::filesystem::temp_directory_path() / ("snapi_gf_glb_import_" + Stamp);
    std::error_code DirectoryError{};
    std::filesystem::create_directories(Extracted.TempDirectory, DirectoryError);
    if (DirectoryError)
    {
        Ctx.LogError("RenderAsset Assimp importer failed to create temporary GLB extraction directory for %s: %s",
                     SourceUri.c_str(),
                     DirectoryError.message().c_str());
        return std::nullopt;
    }

    if (!BinChunk.empty())
    {
        const std::filesystem::path BinPath = Extracted.TempDirectory / (SafeStem + ".bin");
        std::ofstream BinFile(BinPath, std::ios::binary | std::ios::trunc);
        if (!BinFile.is_open())
        {
            Ctx.LogError("RenderAsset Assimp importer failed to write temporary GLB BIN payload for %s", SourceUri.c_str());
            return std::nullopt;
        }
        BinFile.write(reinterpret_cast<const char*>(BinChunk.data()), static_cast<std::streamsize>(BinChunk.size()));
        if (!BinFile.good())
        {
            Ctx.LogError("RenderAsset Assimp importer failed to flush temporary GLB BIN payload for %s", SourceUri.c_str());
            return std::nullopt;
        }
    }

    Extracted.GltfPath = Extracted.TempDirectory / (SafeStem + ".gltf");
    std::ofstream JsonFile(Extracted.GltfPath, std::ios::binary | std::ios::trunc);
    if (!JsonFile.is_open())
    {
        Ctx.LogError("RenderAsset Assimp importer failed to write temporary GLB JSON payload for %s", SourceUri.c_str());
        return std::nullopt;
    }
    const std::string SerializedDocument = Document.dump(2);
    JsonFile.write(SerializedDocument.data(), static_cast<std::streamsize>(SerializedDocument.size()));
    if (!JsonFile.good())
    {
        Ctx.LogError("RenderAsset Assimp importer failed to flush temporary GLB JSON payload for %s", SourceUri.c_str());
        return std::nullopt;
    }

    return Extracted;
}

[[nodiscard]] bool ParseBool(std::string Value, const bool DefaultValue)
{
    Value = ToLowerAscii(Value);
    if (Value == "1" || Value == "true" || Value == "yes" || Value == "on")
    {
        return true;
    }
    if (Value == "0" || Value == "false" || Value == "no" || Value == "off")
    {
        return false;
    }
    return DefaultValue;
}

[[nodiscard]] uint32_t ParseUInt(const std::string& Value, const uint32_t DefaultValue)
{
    try
    {
        const auto Parsed = std::stoul(Value);
        if (Parsed > std::numeric_limits<uint32_t>::max())
        {
            return DefaultValue;
        }
        return static_cast<uint32_t>(Parsed);
    }
    catch (...)
    {
        return DefaultValue;
    }
}

[[nodiscard]] AssimpImporterSettings ReadAssimpImportSettings(
    const ::SnAPI::AssetPipeline::IAssetImportSettings* ImportSettings,
    IPipelineContext& Ctx)
{
    if (const auto* Typed = dynamic_cast<const AssimpImporterSettings*>(ImportSettings))
    {
        return *Typed;
    }

    AssimpImporterSettings Settings{};
    Settings.Mesh.GenerateNormals =
        ParseBool(Ctx.GetOption("SnAPI.GF.Assimp.GenerateNormals", Settings.Mesh.GenerateNormals ? "true" : "false"),
                  Settings.Mesh.GenerateNormals);
    Settings.Mesh.GenerateTangents =
        ParseBool(Ctx.GetOption("SnAPI.GF.Assimp.GenerateTangents", Settings.Mesh.GenerateTangents ? "true" : "false"),
                  Settings.Mesh.GenerateTangents);
    Settings.Mesh.FlipUVs =
        ParseBool(Ctx.GetOption("SnAPI.GF.Assimp.FlipUVs", Settings.Mesh.FlipUVs ? "true" : "false"),
                  Settings.Mesh.FlipUVs);
    Settings.Mesh.OptimizeMeshes =
        ParseBool(Ctx.GetOption("SnAPI.GF.Assimp.OptimizeMeshes", Settings.Mesh.OptimizeMeshes ? "true" : "false"),
                  Settings.Mesh.OptimizeMeshes);
    Settings.Mesh.ForceSkeletal =
        ParseBool(Ctx.GetOption("SnAPI.GF.Assimp.ForceSkeletal", Settings.Mesh.ForceSkeletal ? "true" : "false"),
                  Settings.Mesh.ForceSkeletal);
    Settings.Mesh.ForceStatic =
        ParseBool(Ctx.GetOption("SnAPI.GF.Assimp.ForceStatic", Settings.Mesh.ForceStatic ? "true" : "false"),
                  Settings.Mesh.ForceStatic);
    Settings.Mesh.ImportMaterials =
        ParseBool(Ctx.GetOption("SnAPI.GF.Assimp.ImportMaterials", Settings.Mesh.ImportMaterials ? "true" : "false"),
                  Settings.Mesh.ImportMaterials);
    Settings.Mesh.ImportTextures =
        ParseBool(Ctx.GetOption("SnAPI.GF.Assimp.ImportTextures", Settings.Mesh.ImportTextures ? "true" : "false"),
                  Settings.Mesh.ImportTextures);
    Settings.Mesh.ImportAnimations =
        ParseBool(Ctx.GetOption("SnAPI.GF.Assimp.ImportAnimations", Settings.Mesh.ImportAnimations ? "true" : "false"),
                  Settings.Mesh.ImportAnimations);
    Settings.Mesh.ImportSkeleton =
        ParseBool(Ctx.GetOption("SnAPI.GF.Assimp.ImportSkeleton", Settings.Mesh.ImportSkeleton ? "true" : "false"),
                  Settings.Mesh.ImportSkeleton);
    Settings.Mesh.MaxBonesPerVertex = std::max<uint32_t>(
        1u,
        ParseUInt(Ctx.GetOption("SnAPI.GF.Assimp.MaxBonesPerVertex", std::to_string(Settings.Mesh.MaxBonesPerVertex)),
                  Settings.Mesh.MaxBonesPerVertex));

    Settings.LogicalNameOverride = Ctx.GetOption("SnAPI.GF.Assimp.LogicalName", "");
    Settings.DefaultShaderModule = Ctx.GetOption("SnAPI.GF.Assimp.DefaultShaderModule", "DefaultGBufferMaterial");
    Settings.DefaultShadingModel = Ctx.GetOption("SnAPI.GF.Assimp.DefaultShadingModel", "GBufferShadingModel");
    return Settings;
}

[[nodiscard]] std::string BuildImportVariantKey(const MeshImportSettingsPayload& Settings)
{
    return "assimp-v1"
        "|gn=" + std::to_string(Settings.GenerateNormals ? 1 : 0)
        + "|gt=" + std::to_string(Settings.GenerateTangents ? 1 : 0)
        + "|flip=" + std::to_string(Settings.FlipUVs ? 1 : 0)
        + "|opt=" + std::to_string(Settings.OptimizeMeshes ? 1 : 0)
        + "|skeletal=" + std::to_string(Settings.ForceSkeletal ? 1 : 0)
        + "|static=" + std::to_string(Settings.ForceStatic ? 1 : 0)
        + "|mats=" + std::to_string(Settings.ImportMaterials ? 1 : 0)
        + "|tex=" + std::to_string(Settings.ImportTextures ? 1 : 0)
        + "|anim=" + std::to_string(Settings.ImportAnimations ? 1 : 0)
        + "|skel=" + std::to_string(Settings.ImportSkeleton ? 1 : 0)
        + "|mbv=" + std::to_string(Settings.MaxBonesPerVertex);
}

[[nodiscard]] std::string SanitizeName(std::string_view Name, const uint32_t FallbackIndex)
{
    std::string Out{};
    Out.reserve(Name.size() + 16);
    for (const char Ch : Name)
    {
        const bool IsAlphaNum = std::isalnum(static_cast<unsigned char>(Ch)) != 0;
        Out.push_back(IsAlphaNum ? Ch : '_');
    }

    while (!Out.empty() && Out.back() == '_')
    {
        Out.pop_back();
    }

    if (Out.empty())
    {
        Out = "item_" + std::to_string(FallbackIndex);
    }
    return Out;
}

[[nodiscard]] std::string MakeScopedLogicalName(
    std::string_view BaseLogicalName,
    std::string_view Scope,
    std::string_view Name,
    const uint32_t Index)
{
    return std::string(BaseLogicalName) + "::" + std::string(Scope) + "::" + SanitizeName(Name, Index) + "_" + std::to_string(Index);
}

void AppendDependencyUnique(std::vector<SourceRef>& Dependencies, std::unordered_set<std::string>& SeenUris, const std::string& Uri)
{
    if (Uri.empty() || SeenUris.contains(Uri))
    {
        return;
    }
    SeenUris.insert(Uri);
    Dependencies.emplace_back(Uri, 0);
}

[[nodiscard]] std::string ResolveUriRelativeToSource(std::string_view SourceUri, const std::string& Uri)
{
    if (Uri.empty())
    {
        return {};
    }

    if (Uri.find("://") != std::string::npos)
    {
        return Uri;
    }

    const std::filesystem::path UriPath{Uri};
    if (UriPath.is_absolute())
    {
        return UriPath.lexically_normal().string();
    }

    const std::filesystem::path SourcePath{std::string(SourceUri)};
    const std::filesystem::path Resolved = SourcePath.parent_path() / UriPath;
    return Resolved.lexically_normal().string();
}

[[nodiscard]] AssetRefPayload MakeAssetRef(const ImportedItem& Item)
{
    AssetRefPayload Ref{};
    Ref.AssetName = Item.LogicalName;
    Ref.AssetId = Item.Id.ToString();
    return Ref;
}

[[nodiscard]] MaterialInstanceAssetRef MakeMaterialInstanceAssetRef(const ImportedItem& Item)
{
    return MaterialInstanceAssetRef(Item.LogicalName, Item.Id.ToString());
}

template<typename TValue>
void AppendValueBytes(std::vector<uint8_t>& Bytes, const TValue& Value)
{
    const size_t Offset = Bytes.size();
    Bytes.resize(Offset + sizeof(TValue));
    std::memcpy(Bytes.data() + Offset, &Value, sizeof(TValue));
}

template<typename TValue, size_t N>
void AppendArrayBytes(std::vector<uint8_t>& Bytes, const std::array<TValue, N>& Values)
{
    const size_t Offset = Bytes.size();
    const size_t ByteCount = sizeof(TValue) * N;
    Bytes.resize(Offset + ByteCount);
    std::memcpy(Bytes.data() + Offset, Values.data(), ByteCount);
}

[[nodiscard]] std::array<float, 16> MatrixToArray(const aiMatrix4x4& Matrix)
{
    return {
        Matrix.a1, Matrix.a2, Matrix.a3, Matrix.a4,
        Matrix.b1, Matrix.b2, Matrix.b3, Matrix.b4,
        Matrix.c1, Matrix.c2, Matrix.c3, Matrix.c4,
        Matrix.d1, Matrix.d2, Matrix.d3, Matrix.d4};
}

[[nodiscard]] std::array<float, 16> IdentityMatrixArray()
{
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
}

[[nodiscard]] bool IsFiniteVec3(const aiVector3D& Value)
{
    return std::isfinite(Value.x) && std::isfinite(Value.y) && std::isfinite(Value.z);
}

[[nodiscard]] bool IsFiniteQuat(const aiQuaternion& Value)
{
    return std::isfinite(Value.x) && std::isfinite(Value.y) && std::isfinite(Value.z) && std::isfinite(Value.w);
}

[[nodiscard]] std::vector<aiNode*> BuildNodeList(aiNode* Root)
{
    std::vector<aiNode*> Nodes{};
    if (!Root)
    {
        return Nodes;
    }

    std::vector<aiNode*> Stack{Root};
    while (!Stack.empty())
    {
        aiNode* Node = Stack.back();
        Stack.pop_back();
        Nodes.push_back(Node);
        for (uint32_t ChildIndex = 0; ChildIndex < Node->mNumChildren; ++ChildIndex)
        {
            Stack.push_back(Node->mChildren[ChildIndex]);
        }
    }
    return Nodes;
}

[[nodiscard]] aiNode* FindNodeByName(aiNode* Root, const std::string& Name)
{
    if (!Root)
    {
        return nullptr;
    }

    for (aiNode* Node : BuildNodeList(Root))
    {
        if (Node && Name == std::string(Node->mName.C_Str()))
        {
            return Node;
        }
    }
    return nullptr;
}

struct StaticMeshNodeReference
{
    const aiMesh* Mesh = nullptr;
    aiMatrix4x4 WorldTransform{};
};

[[nodiscard]] aiVector3D TransformPoint(const aiMatrix4x4& Transform, const aiVector3D& Value)
{
    return {
        Transform.a1 * Value.x + Transform.a2 * Value.y + Transform.a3 * Value.z + Transform.a4,
        Transform.b1 * Value.x + Transform.b2 * Value.y + Transform.b3 * Value.z + Transform.b4,
        Transform.c1 * Value.x + Transform.c2 * Value.y + Transform.c3 * Value.z + Transform.c4,
    };
}

[[nodiscard]] aiVector3D TransformDirection(const aiMatrix3x3& Transform, const aiVector3D& Value)
{
    return {
        Transform.a1 * Value.x + Transform.a2 * Value.y + Transform.a3 * Value.z,
        Transform.b1 * Value.x + Transform.b2 * Value.y + Transform.b3 * Value.z,
        Transform.c1 * Value.x + Transform.c2 * Value.y + Transform.c3 * Value.z,
    };
}

[[nodiscard]] aiVector3D NormalizeOrFallback(const aiVector3D& Value, const aiVector3D& Fallback)
{
    const float LengthSquared = (Value.x * Value.x) + (Value.y * Value.y) + (Value.z * Value.z);
    if (!std::isfinite(LengthSquared) || LengthSquared <= 1e-12f)
    {
        return Fallback;
    }

    const float InvLength = 1.0f / std::sqrt(LengthSquared);
    return {Value.x * InvLength, Value.y * InvLength, Value.z * InvLength};
}

void CollectStaticMeshNodeReferences(
    const aiScene& Scene,
    aiNode* Node,
    const aiMatrix4x4& ParentTransform,
    std::vector<StaticMeshNodeReference>& OutReferences)
{
    if (!Node)
    {
        return;
    }

    const aiMatrix4x4 WorldTransform = ParentTransform * Node->mTransformation;
    for (uint32_t MeshListIndex = 0; MeshListIndex < Node->mNumMeshes; ++MeshListIndex)
    {
        const uint32_t MeshIndex = Node->mMeshes[MeshListIndex];
        if (MeshIndex >= Scene.mNumMeshes)
        {
            continue;
        }

        const aiMesh* Mesh = Scene.mMeshes[MeshIndex];
        if (!Mesh)
        {
            continue;
        }

        OutReferences.push_back(StaticMeshNodeReference{
            .Mesh = Mesh,
            .WorldTransform = WorldTransform,
        });
    }

    for (uint32_t ChildIndex = 0; ChildIndex < Node->mNumChildren; ++ChildIndex)
    {
        CollectStaticMeshNodeReferences(Scene, Node->mChildren[ChildIndex], WorldTransform, OutReferences);
    }
}

void InsertBoneInfluence(
    std::array<uint32_t, 4>& BoneIndices,
    std::array<float, 4>& BoneWeights,
    const uint32_t BoneIndex,
    const float Weight)
{
    size_t MinWeightSlot = 0;
    for (size_t Slot = 1; Slot < BoneWeights.size(); ++Slot)
    {
        if (BoneWeights[Slot] < BoneWeights[MinWeightSlot])
        {
            MinWeightSlot = Slot;
        }
    }

    if (Weight <= BoneWeights[MinWeightSlot])
    {
        return;
    }

    BoneIndices[MinWeightSlot] = BoneIndex;
    BoneWeights[MinWeightSlot] = Weight;
}

[[nodiscard]] aiVector3D SampleVectorKeys(const aiVectorKey* Keys, const uint32_t KeyCount, const double Time)
{
    if (KeyCount == 0 || !Keys)
    {
        return aiVector3D(0.0f, 0.0f, 0.0f);
    }
    if (KeyCount == 1 || Time <= Keys[0].mTime)
    {
        return Keys[0].mValue;
    }
    if (Time >= Keys[KeyCount - 1].mTime)
    {
        return Keys[KeyCount - 1].mValue;
    }

    for (uint32_t Index = 0; Index + 1 < KeyCount; ++Index)
    {
        const aiVectorKey& Left = Keys[Index];
        const aiVectorKey& Right = Keys[Index + 1];
        if (Time <= Right.mTime)
        {
            const double Delta = Right.mTime - Left.mTime;
            const float Alpha = (Delta <= 0.0) ? 0.0f : static_cast<float>((Time - Left.mTime) / Delta);
            return Left.mValue + (Right.mValue - Left.mValue) * Alpha;
        }
    }

    return Keys[KeyCount - 1].mValue;
}

[[nodiscard]] aiQuaternion SampleQuatKeys(const aiQuatKey* Keys, const uint32_t KeyCount, const double Time)
{
    if (KeyCount == 0 || !Keys)
    {
        return aiQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
    }
    if (KeyCount == 1 || Time <= Keys[0].mTime)
    {
        return Keys[0].mValue;
    }
    if (Time >= Keys[KeyCount - 1].mTime)
    {
        return Keys[KeyCount - 1].mValue;
    }

    for (uint32_t Index = 0; Index + 1 < KeyCount; ++Index)
    {
        const aiQuatKey& Left = Keys[Index];
        const aiQuatKey& Right = Keys[Index + 1];
        if (Time <= Right.mTime)
        {
            const double Delta = Right.mTime - Left.mTime;
            const float Alpha = (Delta <= 0.0) ? 0.0f : static_cast<float>((Time - Left.mTime) / Delta);
            aiQuaternion Out{};
            aiQuaternion::Interpolate(Out, Left.mValue, Right.mValue, Alpha);
            Out.Normalize();
            return Out;
        }
    }

    return Keys[KeyCount - 1].mValue;
}

struct MaterialImportOutputs
{
    std::unordered_map<uint32_t, MaterialInstanceAssetRef> MaterialInstanceRefsBySlot{};
    std::unordered_set<std::string> TextureDependencies{};
};

struct EmbeddedTextureImportOutputs
{
    std::unordered_map<uint32_t, AssetRefPayload> RefsByIndex{};
    std::unordered_map<std::string, AssetRefPayload> RefsByToken{};
};

void UpdateNonTrivialAlphaFlag(TextureCompressorPlugin::ImageIntermediate& Out)
{
    Out.bHasNonTrivialAlpha = false;
    if (Out.Channels < 4u)
    {
        return;
    }

    for (size_t PixelIndex = 0; PixelIndex + 3u < Out.Pixels.size(); PixelIndex += static_cast<size_t>(Out.Channels))
    {
        if (Out.Pixels[PixelIndex + 3u] != 255u)
        {
            Out.bHasNonTrivialAlpha = true;
            return;
        }
    }
}

[[nodiscard]] std::optional<uint32_t> ParseEmbeddedTextureIndex(const std::string& Token)
{
    if (Token.size() <= 1 || Token[0] != '*')
    {
        return std::nullopt;
    }

    const std::string_view IndexText(Token.c_str() + 1, Token.size() - 1);
    if (IndexText.empty())
    {
        return std::nullopt;
    }

    for (const char Ch : IndexText)
    {
        if (std::isdigit(static_cast<unsigned char>(Ch)) == 0)
        {
            return std::nullopt;
        }
    }

    try
    {
        const auto Parsed = std::stoul(std::string(IndexText));
        if (Parsed > std::numeric_limits<uint32_t>::max())
        {
            return std::nullopt;
        }
        return static_cast<uint32_t>(Parsed);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

[[nodiscard]] bool ResolveEmbeddedTextureRef(
    const std::string& Token,
    const EmbeddedTextureImportOutputs& EmbeddedTextures,
    AssetRefPayload& OutRef)
{
    if (const auto Exact = EmbeddedTextures.RefsByToken.find(Token); Exact != EmbeddedTextures.RefsByToken.end())
    {
        OutRef = Exact->second;
        return true;
    }

    const auto ParsedIndex = ParseEmbeddedTextureIndex(Token);
    if (!ParsedIndex.has_value())
    {
        return false;
    }

    if (const auto ByIndex = EmbeddedTextures.RefsByIndex.find(*ParsedIndex); ByIndex != EmbeddedTextures.RefsByIndex.end())
    {
        OutRef = ByIndex->second;
        return true;
    }

    return false;
}

[[nodiscard]] bool DecodeRawAssimpTexture(
    const aiTexture& Texture,
    TextureCompressorPlugin::ImageIntermediate& Out)
{
    if (!Texture.pcData || Texture.mWidth == 0 || Texture.mHeight == 0)
    {
        return false;
    }

    const size_t PixelCount = static_cast<size_t>(Texture.mWidth) * static_cast<size_t>(Texture.mHeight);
    if (PixelCount == 0 || PixelCount > (std::numeric_limits<size_t>::max() / 4))
    {
        return false;
    }

    Out.Width = Texture.mWidth;
    Out.Height = Texture.mHeight;
    Out.Channels = 4;
    Out.BitsPerChannel = 8;
    Out.bIsFloat = false;
    Out.bSRGB = true;
    Out.Pixels.resize(PixelCount * 4);

    for (size_t PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
    {
        const aiTexel& Texel = Texture.pcData[PixelIndex];
        const size_t Dst = PixelIndex * 4;
        Out.Pixels[Dst + 0] = Texel.r;
        Out.Pixels[Dst + 1] = Texel.g;
        Out.Pixels[Dst + 2] = Texel.b;
        Out.Pixels[Dst + 3] = Texel.a;
    }

    UpdateNonTrivialAlphaFlag(Out);
    return true;
}

#if defined(SNAPI_GF_HAS_FREEIMAGE) && SNAPI_GF_HAS_FREEIMAGE
void EnsureFreeImageInitialized()
{
    static std::once_flag InitOnce{};
    std::call_once(InitOnce, []() {
        FreeImage_Initialise(FALSE);
    });
}

[[nodiscard]] FREE_IMAGE_FORMAT GuessFreeImageFormat(const aiTexture& Texture)
{
    if (Texture.achFormatHint[0] == '\0')
    {
        return FIF_UNKNOWN;
    }

    std::string Hint = ToLowerAscii(Texture.achFormatHint);
    if (!Hint.empty() && Hint.front() == '.')
    {
        Hint.erase(Hint.begin());
    }
    if (Hint.empty())
    {
        return FIF_UNKNOWN;
    }

    const std::string FakeName = "embedded." + Hint;
    return FreeImage_GetFIFFromFilename(FakeName.c_str());
}

[[nodiscard]] bool DecodeCompressedAssimpTextureWithFreeImage(
    const aiTexture& Texture,
    TextureCompressorPlugin::ImageIntermediate& Out)
{
    if (!Texture.pcData || Texture.mWidth == 0 || Texture.mHeight != 0 || Texture.mWidth > std::numeric_limits<DWORD>::max())
    {
        return false;
    }

    EnsureFreeImageInitialized();

    BYTE* MemoryBytes = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(Texture.pcData));
    FIMEMORY* Memory = FreeImage_OpenMemory(MemoryBytes, static_cast<DWORD>(Texture.mWidth));
    if (!Memory)
    {
        return false;
    }

    FREE_IMAGE_FORMAT Format = FreeImage_GetFileTypeFromMemory(Memory, 0);
    if (Format == FIF_UNKNOWN)
    {
        Format = GuessFreeImageFormat(Texture);
    }

    if (Format == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(Format))
    {
        FreeImage_CloseMemory(Memory);
        return false;
    }

    FIBITMAP* Bitmap = FreeImage_LoadFromMemory(Format, Memory, 0);
    FreeImage_CloseMemory(Memory);
    if (!Bitmap)
    {
        return false;
    }

    FIBITMAP* Bitmap32 = FreeImage_ConvertTo32Bits(Bitmap);
    FreeImage_Unload(Bitmap);
    if (!Bitmap32)
    {
        return false;
    }

    const uint32_t Width = FreeImage_GetWidth(Bitmap32);
    const uint32_t Height = FreeImage_GetHeight(Bitmap32);
    const uint32_t Pitch = FreeImage_GetPitch(Bitmap32);
    const BYTE* Bits = FreeImage_GetBits(Bitmap32);

    if (!Bits || Width == 0 || Height == 0)
    {
        FreeImage_Unload(Bitmap32);
        return false;
    }

    const size_t PixelCount = static_cast<size_t>(Width) * static_cast<size_t>(Height);
    if (PixelCount > (std::numeric_limits<size_t>::max() / 4))
    {
        FreeImage_Unload(Bitmap32);
        return false;
    }

    Out.Width = Width;
    Out.Height = Height;
    Out.Channels = 4;
    Out.BitsPerChannel = 8;
    Out.bIsFloat = false;
    Out.bSRGB = true;
    Out.Pixels.resize(PixelCount * 4);

    for (uint32_t Y = 0; Y < Height; ++Y)
    {
        const uint32_t SrcY = Height - 1u - Y;
        const BYTE* Row = Bits + static_cast<size_t>(SrcY) * Pitch;
        for (uint32_t X = 0; X < Width; ++X)
        {
            const BYTE B = Row[X * 4 + FI_RGBA_BLUE];
            const BYTE G = Row[X * 4 + FI_RGBA_GREEN];
            const BYTE R = Row[X * 4 + FI_RGBA_RED];
            const BYTE A = Row[X * 4 + FI_RGBA_ALPHA];

            const size_t Dst = (static_cast<size_t>(Y) * Width + X) * 4;
            Out.Pixels[Dst + 0] = R;
            Out.Pixels[Dst + 1] = G;
            Out.Pixels[Dst + 2] = B;
            Out.Pixels[Dst + 3] = A;
        }
    }

    UpdateNonTrivialAlphaFlag(Out);
    FreeImage_Unload(Bitmap32);
    return true;
}
#endif

[[nodiscard]] bool DecodeAssimpEmbeddedTexture(
    const aiTexture& Texture,
    TextureCompressorPlugin::ImageIntermediate& Out,
    IPipelineContext& Ctx)
{
    if (DecodeRawAssimpTexture(Texture, Out))
    {
        return true;
    }

#if defined(SNAPI_GF_HAS_FREEIMAGE) && SNAPI_GF_HAS_FREEIMAGE
    if (DecodeCompressedAssimpTextureWithFreeImage(Texture, Out))
    {
        return true;
    }
#else
    if (Texture.mWidth > 0 && Texture.mHeight == 0)
    {
        Ctx.LogWarn("RenderAsset Assimp importer cannot decode compressed embedded texture without FreeImage support");
    }
#endif

    return false;
}

[[nodiscard]] std::string BuildEmbeddedTextureLabel(const aiTexture& Texture, const uint32_t TextureIndex)
{
    if (Texture.mFilename.length > 0)
    {
        return Texture.mFilename.C_Str();
    }
    if (Texture.achFormatHint[0] != '\0')
    {
        return std::string("embedded_") + Texture.achFormatHint;
    }
    return "embedded_" + std::to_string(TextureIndex);
}

[[nodiscard]] bool BuildEmbeddedTextureItems(
    const SourceRef& Source,
    const aiScene& Scene,
    std::string_view BaseLogicalName,
    std::string_view VariantKey,
    std::vector<ImportedItem>& GeneratedItems,
    EmbeddedTextureImportOutputs& OutEmbeddedTextures,
    IPipelineContext& Ctx)
{
    if (Scene.mNumTextures == 0)
    {
        return true;
    }

    const auto* TextureIntermediateSerializer = Ctx.FindSerializer(TextureCompressorPlugin::Payload_CompressorImageIntermediate);
    if (!TextureIntermediateSerializer)
    {
        Ctx.LogWarn(
            "RenderAsset Assimp importer found %u embedded textures but TextureCompressor serializer is unavailable; skipping embedded textures",
            Scene.mNumTextures);
        return true;
    }

    const uint32_t TextureSchemaVersion = TextureIntermediateSerializer->GetSchemaVersion();
    if (TextureSchemaVersion != 1u)
    {
        Ctx.LogWarn(
            "RenderAsset Assimp importer only supports TextureCompressor.ImageIntermediate schema 1 (found %u); skipping embedded textures",
            TextureSchemaVersion);
        return true;
    }

    for (uint32_t TextureIndex = 0; TextureIndex < Scene.mNumTextures; ++TextureIndex)
    {
        const aiTexture* EmbeddedTexture = Scene.mTextures[TextureIndex];
        if (!EmbeddedTexture)
        {
            continue;
        }

        TextureCompressorPlugin::ImageIntermediate IntermediateTexture{};
        const std::string EmbeddedTextureLabel = BuildEmbeddedTextureLabel(*EmbeddedTexture, TextureIndex);
        if (!DecodeAssimpEmbeddedTexture(*EmbeddedTexture, IntermediateTexture, Ctx))
        {
            Ctx.LogWarn(
                "RenderAsset Assimp importer failed to decode embedded texture %u in %s (w=%u h=%u hint=%s name=%s)",
                TextureIndex,
                Source.Uri.c_str(),
                EmbeddedTexture->mWidth,
                EmbeddedTexture->mHeight,
                EmbeddedTexture->achFormatHint,
                EmbeddedTexture->mFilename.C_Str());
            continue;
        }
        IntermediateTexture.SourceFilename = EmbeddedTextureLabel;

        ImportedItem TextureItem{};
        TextureItem.LogicalName = MakeScopedLogicalName(
            BaseLogicalName,
            "texture",
            EmbeddedTextureLabel,
            TextureIndex);
        TextureItem.AssetKind = TextureCompressorPlugin::AssetKind_CompressedTexture;
        TextureItem.VariantKey = std::string(VariantKey);
        TextureItem.Id = Ctx.MakeDeterministicAssetId(TextureItem.LogicalName, TextureItem.VariantKey);
        TextureItem.Dependencies.emplace_back(Source.Uri, Source.ContentHash);
        TextureItem.Intermediate.PayloadType = TextureCompressorPlugin::Payload_CompressorImageIntermediate;
        TextureItem.Intermediate.SchemaVersion = TextureSchemaVersion;
        TextureIntermediateSerializer->SerializeToBytes(&IntermediateTexture, TextureItem.Intermediate.Bytes);

        AssetRefPayload Ref = MakeAssetRef(TextureItem);
        OutEmbeddedTextures.RefsByIndex[TextureIndex] = Ref;
        OutEmbeddedTextures.RefsByToken["*" + std::to_string(TextureIndex)] = Ref;
        if (EmbeddedTexture->mFilename.length > 0)
        {
            OutEmbeddedTextures.RefsByToken[EmbeddedTexture->mFilename.C_Str()] = Ref;
        }

        GeneratedItems.push_back(std::move(TextureItem));
    }

    return true;
}

struct MeshImportBuffers
{
    uint32_t VertexCount = 0;
    uint32_t MaxMaterialSlot = 0;
    std::array<float, 3> BoundsMin{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    std::array<float, 3> BoundsMax{
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()};

    std::vector<uint8_t> Positions{};
    std::vector<uint8_t> Normals{};
    std::vector<uint8_t> Tangents{};
    std::vector<uint8_t> UV0{};
    std::vector<uint8_t> UV1{};
    std::vector<uint8_t> Colors{};
    std::vector<uint8_t> BoneIndices{};
    std::vector<uint8_t> BoneWeights{};
    std::vector<uint8_t> Indices{};

    std::vector<StaticSubMeshPayload> SubMeshes{};
    std::vector<SkeletalBonePayload> Bones{};
    std::vector<AnimationPayload> Animations{};
};

[[nodiscard]] bool BuildAssimpItems(
    const SourceRef& Source,
    const aiScene& Scene,
    const AssimpImporterSettings& ImportConfig,
    std::vector<ImportedItem>& OutItems,
    IPipelineContext& Ctx)
{
    const MeshImportSettingsPayload& ImportSettings = ImportConfig.Mesh;
    const std::string BaseLogicalName = ImportConfig.LogicalNameOverride.empty()
                                            ? Source.Uri
                                            : ImportConfig.LogicalNameOverride;
    const std::string VariantKey = BuildImportVariantKey(ImportSettings);
    const std::string& DefaultShaderModule = ImportConfig.DefaultShaderModule;
    const std::string& DefaultShadingModel = ImportConfig.DefaultShadingModel;

    const auto* MaterialSerializer = Ctx.FindSerializer(PayloadMaterial());
    const auto* MaterialInstanceSerializer = Ctx.FindSerializer(PayloadMaterialInstance());
    const auto* SkeletonSerializer = Ctx.FindSerializer(PayloadSkeleton());
    const auto* AnimationSerializer = Ctx.FindSerializer(PayloadAnimation());
    const auto* StaticMeshSourceSerializer = Ctx.FindSerializer(PayloadStaticMeshSource());
    const auto* SkeletalMeshSourceSerializer = Ctx.FindSerializer(PayloadSkeletalMeshSource());

    if (!MaterialSerializer || !MaterialInstanceSerializer || !SkeletonSerializer || !AnimationSerializer
        || !StaticMeshSourceSerializer || !SkeletalMeshSourceSerializer)
    {
        Ctx.LogError("RenderAsset Assimp importer missing one or more serializers");
        return false;
    }

    bool SceneHasBones = false;
    bool SceneHasAnimations = Scene.HasAnimations();
    bool HasNormals = ImportSettings.GenerateNormals;
    bool HasTangents = false;
    bool HasUV0 = false;
    bool HasUV1 = false;
    bool HasColors = false;
    for (uint32_t MeshIndex = 0; MeshIndex < Scene.mNumMeshes; ++MeshIndex)
    {
        const aiMesh* Mesh = Scene.mMeshes[MeshIndex];
        if (!Mesh)
        {
            continue;
        }
        SceneHasBones = SceneHasBones || Mesh->HasBones();
        HasNormals = HasNormals || Mesh->HasNormals();
        HasTangents = HasTangents || Mesh->HasTangentsAndBitangents();
        HasUV0 = HasUV0 || Mesh->HasTextureCoords(0);
        HasUV1 = HasUV1 || Mesh->HasTextureCoords(1);
        HasColors = HasColors || Mesh->HasVertexColors(0);
    }

    const bool ImportAsSkeletal = (ImportSettings.ForceSkeletal || SceneHasBones || SceneHasAnimations) && !ImportSettings.ForceStatic;

    std::vector<StaticMeshNodeReference> StaticMeshReferences{};
    if (!ImportAsSkeletal)
    {
        StaticMeshReferences.reserve(Scene.mNumMeshes);
        CollectStaticMeshNodeReferences(Scene, Scene.mRootNode, aiMatrix4x4(), StaticMeshReferences);
        if (StaticMeshReferences.empty())
        {
            for (uint32_t MeshIndex = 0; MeshIndex < Scene.mNumMeshes; ++MeshIndex)
            {
                if (const aiMesh* Mesh = Scene.mMeshes[MeshIndex])
                {
                    StaticMeshReferences.push_back(StaticMeshNodeReference{
                        .Mesh = Mesh,
                        .WorldTransform = aiMatrix4x4(),
                    });
                }
            }
        }
    }

    MaterialImportOutputs MaterialOutputs{};
    EmbeddedTextureImportOutputs EmbeddedTextureOutputs{};
    std::vector<ImportedItem> GeneratedItems{};
    if (ImportSettings.ImportTextures &&
        !BuildEmbeddedTextureItems(
            Source,
            Scene,
            BaseLogicalName,
            VariantKey,
            GeneratedItems,
            EmbeddedTextureOutputs,
            Ctx))
    {
        return false;
    }

    if (ImportSettings.ImportMaterials)
    {
        for (uint32_t MaterialIndex = 0; MaterialIndex < Scene.mNumMaterials; ++MaterialIndex)
        {
            aiMaterial* AiMat = Scene.mMaterials[MaterialIndex];
            if (!AiMat)
            {
                continue;
            }

            aiString MaterialName{};
            if (AiMat->Get(AI_MATKEY_NAME, MaterialName) != AI_SUCCESS)
            {
                MaterialName = aiString("Material");
            }
            const std::string MaterialLabel = MaterialName.C_Str();

        ImportedItem MaterialItem{};
        MaterialItem.LogicalName = MakeScopedLogicalName(BaseLogicalName, "material", MaterialLabel, MaterialIndex);
        MaterialItem.AssetKind = AssetKindMaterial();
        MaterialItem.VariantKey = VariantKey;
        MaterialItem.Id = Ctx.MakeDeterministicAssetId(MaterialItem.LogicalName, MaterialItem.VariantKey);
        MaterialItem.Dependencies.emplace_back(Source.Uri, Source.ContentHash);
        MaterialItem.Intermediate.PayloadType = PayloadMaterial();
        MaterialItem.Intermediate.SchemaVersion = MaterialSerializer->GetSchemaVersion();

        MaterialAsset MaterialPayloadData{};
        MaterialPayloadData.ShaderModule = DefaultShaderModule;
        MaterialPayloadData.ShadingModel = DefaultShadingModel;

        ImportedItem MaterialInstanceItem{};
        MaterialInstanceItem.LogicalName = MakeScopedLogicalName(BaseLogicalName, "matinst", MaterialLabel, MaterialIndex);
        MaterialInstanceItem.AssetKind = AssetKindMaterialInstance();
        MaterialInstanceItem.VariantKey = VariantKey;
        MaterialInstanceItem.Id = Ctx.MakeDeterministicAssetId(MaterialInstanceItem.LogicalName, MaterialInstanceItem.VariantKey);
        MaterialInstanceItem.Dependencies.emplace_back(Source.Uri, Source.ContentHash);
        MaterialInstanceItem.Intermediate.PayloadType = PayloadMaterialInstance();
        MaterialInstanceItem.Intermediate.SchemaVersion = MaterialInstanceSerializer->GetSchemaVersion();

        MaterialInstanceAsset MaterialInstancePayloadData{};
        MaterialInstancePayloadData.ParentMaterial = MakeAssetRef(MaterialItem);

        const bool IsGBufferShadingModel = ToLowerAscii(DefaultShadingModel) == "gbuffershadingmodel";
        const auto UpsertScalar = [&MaterialInstancePayloadData](std::string_view Name, const float Value) {
            for (MaterialScalarParamPayload& Existing : MaterialInstancePayloadData.Scalars)
            {
                if (Existing.Name == Name)
                {
                    Existing.Value = Value;
                    return;
                }
            }
            MaterialScalarParamPayload Param{};
            Param.Name = std::string(Name);
            Param.Value = Value;
            MaterialInstancePayloadData.Scalars.push_back(std::move(Param));
        };
        const auto UpsertVector = [&MaterialInstancePayloadData](std::string_view Name, const std::array<float, 4>& Value) {
            for (MaterialVectorParamPayload& Existing : MaterialInstancePayloadData.Vectors)
            {
                if (Existing.Name == Name)
                {
                    Existing.Value = Value;
                    return;
                }
            }
            MaterialVectorParamPayload Param{};
            Param.Name = std::string(Name);
            Param.Value = Value;
            MaterialInstancePayloadData.Vectors.push_back(std::move(Param));
        };

        if (IsGBufferShadingModel)
        {
            // Keep importer defaults aligned with GBufferShadingModel EditorDefault() values.
            UpsertVector("Color", {1.0f, 1.0f, 1.0f, 1.0f});
            UpsertScalar("Roughness", 0.8f);
            UpsertScalar("Metallic", 0.0f);
            UpsertScalar("Occlusion", 1.0f);
        }

        aiColor4D BaseColor{};
        if (AiMat->Get(AI_MATKEY_BASE_COLOR, BaseColor) == AI_SUCCESS
            || AiMat->Get(AI_MATKEY_COLOR_DIFFUSE, BaseColor) == AI_SUCCESS)
        {
            if (std::isfinite(BaseColor.r) && std::isfinite(BaseColor.g) &&
                std::isfinite(BaseColor.b) && std::isfinite(BaseColor.a))
            {
                UpsertVector("Color", {BaseColor.r, BaseColor.g, BaseColor.b, BaseColor.a});
            }
        }

        aiColor4D EmissiveColor{};
        if (AiMat->Get(AI_MATKEY_COLOR_EMISSIVE, EmissiveColor) == AI_SUCCESS)
        {
            if (std::isfinite(EmissiveColor.r) && std::isfinite(EmissiveColor.g) &&
                std::isfinite(EmissiveColor.b) && std::isfinite(EmissiveColor.a))
            {
                UpsertVector("EmissiveColor", {EmissiveColor.r, EmissiveColor.g, EmissiveColor.b, EmissiveColor.a});
            }
        }

        float Roughness = 0.8f;
        if (AiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, Roughness) == AI_SUCCESS)
        {
            if (std::isfinite(Roughness))
            {
                UpsertScalar("Roughness", Roughness);
            }
        }

        float Metallic = 0.0f;
        if (AiMat->Get(AI_MATKEY_METALLIC_FACTOR, Metallic) == AI_SUCCESS)
        {
            if (std::isfinite(Metallic))
            {
                UpsertScalar("Metallic", Metallic);
            }
        }

        std::unordered_set<std::string> AddedTextureSlots{};
        const auto EnableMaterialFeatureForSlot = [&MaterialPayloadData](const std::string& SlotKey) {
            if (SlotKey == "Material_Albedo")
            {
                MaterialPayloadData.FeatureAlbedoMap = true;
                return;
            }
            if (SlotKey == "Material_Normal")
            {
                MaterialPayloadData.FeatureNormalMap = true;
                return;
            }
            if (SlotKey == "Material_ORM")
            {
                MaterialPayloadData.FeatureRoughnessMap = true;
                MaterialPayloadData.FeatureMetalnessMap = true;
                MaterialPayloadData.FeatureOcclusionMap = true;
            }
        };

        const auto AddTextureParam = [&](const aiTextureType TextureType, std::string_view SlotName, const bool SRGB) {
            const std::string SlotKey(SlotName);
            if (AddedTextureSlots.contains(SlotKey))
            {
                return;
            }
            if (AiMat->GetTextureCount(TextureType) == 0)
            {
                return;
            }

            aiString TexturePath{};
            if (AiMat->GetTexture(TextureType, 0, &TexturePath) != AI_SUCCESS)
            {
                return;
            }

            const std::string RawPath = TexturePath.C_Str();
            if (RawPath.empty())
            {
                return;
            }

            AssetRefPayload EmbeddedTextureRef{};
            if (ResolveEmbeddedTextureRef(RawPath, EmbeddedTextureOutputs, EmbeddedTextureRef))
            {
                MaterialTextureParamPayload Param{};
                Param.SlotName = std::string(SlotName);
                Param.Texture = std::move(EmbeddedTextureRef);
                Param.SRGB = SRGB;
                MaterialInstancePayloadData.Textures.push_back(std::move(Param));
                AddedTextureSlots.insert(SlotKey);
                EnableMaterialFeatureForSlot(SlotKey);
                return;
            }

            if (RawPath[0] == '*')
            {
                Ctx.LogWarn(
                    "RenderAsset Assimp importer could not resolve embedded texture token %s in %s",
                    RawPath.c_str(),
                    Source.Uri.c_str());
                return;
            }

            const std::string ResolvedTextureUri = ResolveUriRelativeToSource(Source.Uri, RawPath);
            if (ResolvedTextureUri.empty())
            {
                return;
            }

            MaterialTextureParamPayload Param{};
            Param.SlotName = std::string(SlotName);
            Param.Texture.AssetName = ResolvedTextureUri;
            Param.Texture.AssetId.clear();
            Param.SRGB = SRGB;
            MaterialInstancePayloadData.Textures.push_back(std::move(Param));
            AddedTextureSlots.insert(SlotKey);
            MaterialOutputs.TextureDependencies.insert(ResolvedTextureUri);
            EnableMaterialFeatureForSlot(SlotKey);
        };

            if (ImportSettings.ImportTextures)
            {
                AddTextureParam(aiTextureType_BASE_COLOR, "Material_Albedo", true);
                AddTextureParam(aiTextureType_DIFFUSE, "Material_Albedo", true);
                AddTextureParam(aiTextureType_NORMALS, "Material_Normal", false);
                AddTextureParam(aiTextureType_METALNESS, "Material_ORM", false);
                AddTextureParam(aiTextureType_DIFFUSE_ROUGHNESS, "Material_ORM", false);
                AddTextureParam(aiTextureType_AMBIENT_OCCLUSION, "Material_ORM", false);
            }

        for (const MaterialTextureParamPayload& TextureParam : MaterialInstancePayloadData.Textures)
        {
            MaterialInstanceItem.Dependencies.emplace_back(TextureParam.Texture.AssetName, 0);
        }

            MaterialSerializer->SerializeToBytes(&MaterialPayloadData, MaterialItem.Intermediate.Bytes);
            MaterialInstanceSerializer->SerializeToBytes(&MaterialInstancePayloadData, MaterialInstanceItem.Intermediate.Bytes);
            MaterialOutputs.MaterialInstanceRefsBySlot[MaterialIndex] = MakeMaterialInstanceAssetRef(MaterialInstanceItem);
            GeneratedItems.push_back(std::move(MaterialItem));
            GeneratedItems.push_back(std::move(MaterialInstanceItem));
        }
    }

    MeshImportBuffers MeshBuffers{};
    std::vector<std::array<uint32_t, 4>> BoneIndicesPerVertex{};
    std::vector<std::array<float, 4>> BoneWeightsPerVertex{};
    std::unordered_map<std::string, uint32_t> BoneNameToIndex{};
    std::vector<bool> BoneHasBindPose{};

    auto AcquireBoneIndex = [&](const std::string& BoneName) -> uint32_t {
        const auto Existing = BoneNameToIndex.find(BoneName);
        if (Existing != BoneNameToIndex.end())
        {
            return Existing->second;
        }

        const uint32_t NewIndex = static_cast<uint32_t>(MeshBuffers.Bones.size());
        BoneNameToIndex.emplace(BoneName, NewIndex);
        SkeletalBonePayload Bone{};
        Bone.Name = BoneName;
        Bone.ParentIndex = -1;
        Bone.BindPose = IdentityMatrixArray();
        MeshBuffers.Bones.push_back(std::move(Bone));
        BoneHasBindPose.push_back(false);
        return NewIndex;
    };

    const size_t MeshIterationCount = ImportAsSkeletal
        ? static_cast<size_t>(Scene.mNumMeshes)
        : StaticMeshReferences.size();
    for (size_t MeshIndex = 0; MeshIndex < MeshIterationCount; ++MeshIndex)
    {
        const aiMesh* Mesh = ImportAsSkeletal
            ? Scene.mMeshes[MeshIndex]
            : StaticMeshReferences[MeshIndex].Mesh;
        if (!Mesh || Mesh->mNumVertices == 0 || Mesh->mNumFaces == 0)
        {
            continue;
        }

        aiMatrix4x4 StaticWorldTransform{};
        aiMatrix3x3 StaticNormalTransform{};
        if (!ImportAsSkeletal)
        {
            StaticWorldTransform = StaticMeshReferences[MeshIndex].WorldTransform;
            StaticNormalTransform = aiMatrix3x3(StaticWorldTransform);
            StaticNormalTransform.Inverse().Transpose();
        }

        const uint32_t VertexOffset = MeshBuffers.VertexCount;
        const uint32_t IndexOffset = static_cast<uint32_t>(MeshBuffers.Indices.size() / sizeof(uint32_t));

        MeshBuffers.VertexCount += Mesh->mNumVertices;
        BoneIndicesPerVertex.resize(MeshBuffers.VertexCount, {0u, 0u, 0u, 0u});
        BoneWeightsPerVertex.resize(MeshBuffers.VertexCount, {0.0f, 0.0f, 0.0f, 0.0f});

        std::array<float, 3> SubMeshBoundsMin{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
        std::array<float, 3> SubMeshBoundsMax{
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max()};

        for (uint32_t VertexIndex = 0; VertexIndex < Mesh->mNumVertices; ++VertexIndex)
        {
            const aiVector3D Position = ImportAsSkeletal
                ? Mesh->mVertices[VertexIndex]
                : TransformPoint(StaticWorldTransform, Mesh->mVertices[VertexIndex]);
            AppendArrayBytes(MeshBuffers.Positions, std::array<float, 3>{Position.x, Position.y, Position.z});

            for (size_t Axis = 0; Axis < 3; ++Axis)
            {
                const float Value = (&Position.x)[Axis];
                MeshBuffers.BoundsMin[Axis] = std::min(MeshBuffers.BoundsMin[Axis], Value);
                MeshBuffers.BoundsMax[Axis] = std::max(MeshBuffers.BoundsMax[Axis], Value);
                SubMeshBoundsMin[Axis] = std::min(SubMeshBoundsMin[Axis], Value);
                SubMeshBoundsMax[Axis] = std::max(SubMeshBoundsMax[Axis], Value);
            }

            if (HasNormals)
            {
                aiVector3D Normal = Mesh->HasNormals() ? Mesh->mNormals[VertexIndex] : aiVector3D(0.0f, 0.0f, 1.0f);
                if (!ImportAsSkeletal)
                {
                    Normal = NormalizeOrFallback(TransformDirection(StaticNormalTransform, Normal), aiVector3D(0.0f, 0.0f, 1.0f));
                }
                AppendArrayBytes(MeshBuffers.Normals, std::array<float, 3>{Normal.x, Normal.y, Normal.z});
            }

            if (HasTangents)
            {
                std::array<float, 4> Tangent{1.0f, 0.0f, 0.0f, 1.0f};
                if (Mesh->HasTangentsAndBitangents())
                {
                    aiVector3D T = Mesh->mTangents[VertexIndex];
                    if (!ImportAsSkeletal)
                    {
                        T = NormalizeOrFallback(TransformDirection(StaticNormalTransform, T), aiVector3D(1.0f, 0.0f, 0.0f));
                    }
                    Tangent = {T.x, T.y, T.z, 1.0f};
                }
                AppendArrayBytes(MeshBuffers.Tangents, Tangent);
            }

            if (HasUV0)
            {
                const aiVector3D UV = Mesh->HasTextureCoords(0) ? Mesh->mTextureCoords[0][VertexIndex] : aiVector3D(0.0f, 0.0f, 0.0f);
                AppendArrayBytes(MeshBuffers.UV0, std::array<float, 2>{UV.x, UV.y});
            }

            if (HasUV1)
            {
                const aiVector3D UV = Mesh->HasTextureCoords(1) ? Mesh->mTextureCoords[1][VertexIndex] : aiVector3D(0.0f, 0.0f, 0.0f);
                AppendArrayBytes(MeshBuffers.UV1, std::array<float, 2>{UV.x, UV.y});
            }

            if (HasColors)
            {
                const aiColor4D Color = Mesh->HasVertexColors(0) ? Mesh->mColors[0][VertexIndex] : aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
                AppendArrayBytes(MeshBuffers.Colors, std::array<float, 4>{Color.r, Color.g, Color.b, Color.a});
            }
        }

        for (uint32_t FaceIndex = 0; FaceIndex < Mesh->mNumFaces; ++FaceIndex)
        {
            const aiFace& Face = Mesh->mFaces[FaceIndex];
            if (Face.mNumIndices < 3)
            {
                continue;
            }

            for (uint32_t Corner = 0; Corner < Face.mNumIndices; ++Corner)
            {
                const uint32_t IndexValue = VertexOffset + Face.mIndices[Corner];
                AppendValueBytes(MeshBuffers.Indices, IndexValue);
            }
        }

        if (ImportAsSkeletal && Mesh->HasBones())
        {
            for (uint32_t BoneIndex = 0; BoneIndex < Mesh->mNumBones; ++BoneIndex)
            {
                const aiBone* Bone = Mesh->mBones[BoneIndex];
                if (!Bone)
                {
                    continue;
                }

                const uint32_t RuntimeBoneIndex = AcquireBoneIndex(Bone->mName.C_Str());
                if (RuntimeBoneIndex < BoneHasBindPose.size() && !BoneHasBindPose[RuntimeBoneIndex])
                {
                    MeshBuffers.Bones[RuntimeBoneIndex].BindPose = MatrixToArray(Bone->mOffsetMatrix);
                    BoneHasBindPose[RuntimeBoneIndex] = true;
                }

                for (uint32_t WeightIndex = 0; WeightIndex < Bone->mNumWeights; ++WeightIndex)
                {
                    const aiVertexWeight& Weight = Bone->mWeights[WeightIndex];
                    const uint32_t GlobalVertexIndex = VertexOffset + Weight.mVertexId;
                    if (GlobalVertexIndex >= BoneWeightsPerVertex.size())
                    {
                        continue;
                    }

                    InsertBoneInfluence(
                        BoneIndicesPerVertex[GlobalVertexIndex],
                        BoneWeightsPerVertex[GlobalVertexIndex],
                        RuntimeBoneIndex,
                        Weight.mWeight);
                }
            }
        }

        StaticSubMeshPayload SubMesh{};
        SubMesh.IndexOffset = IndexOffset;
        SubMesh.IndexCount = static_cast<uint32_t>(MeshBuffers.Indices.size() / sizeof(uint32_t)) - IndexOffset;
        SubMesh.MaterialSlot = Mesh->mMaterialIndex;
        SubMesh.BoundsMin = SubMeshBoundsMin;
        SubMesh.BoundsMax = SubMeshBoundsMax;
        MeshBuffers.SubMeshes.push_back(SubMesh);
        MeshBuffers.MaxMaterialSlot = std::max(MeshBuffers.MaxMaterialSlot, SubMesh.MaterialSlot);
    }

    if (MeshBuffers.Positions.empty() || MeshBuffers.Indices.empty())
    {
        Ctx.LogError("RenderAsset Assimp importer produced empty mesh buffers: %s", Source.Uri.c_str());
        return false;
    }

    if (ImportAsSkeletal)
    {
        if (Scene.mRootNode)
        {
            for (auto& Bone : MeshBuffers.Bones)
            {
                aiNode* BoneNode = FindNodeByName(Scene.mRootNode, Bone.Name);
                if (!BoneNode)
                {
                    continue;
                }

                aiNode* Parent = BoneNode->mParent;
                while (Parent)
                {
                    const auto ParentIt = BoneNameToIndex.find(Parent->mName.C_Str());
                    if (ParentIt != BoneNameToIndex.end())
                    {
                        Bone.ParentIndex = static_cast<int32_t>(ParentIt->second);
                        break;
                    }
                    Parent = Parent->mParent;
                }
            }
        }

        for (uint32_t VertexIndex = 0; VertexIndex < MeshBuffers.VertexCount; ++VertexIndex)
        {
            std::array<uint32_t, 4> VertexBoneIndices = BoneIndicesPerVertex[VertexIndex];
            std::array<float, 4> VertexBoneWeights = BoneWeightsPerVertex[VertexIndex];
            float WeightSum = 0.0f;
            for (float Weight : VertexBoneWeights)
            {
                WeightSum += Weight;
            }
            if (WeightSum > 0.0f)
            {
                for (float& Weight : VertexBoneWeights)
                {
                    Weight /= WeightSum;
                }
            }

            AppendArrayBytes(MeshBuffers.BoneIndices, VertexBoneIndices);
            AppendArrayBytes(MeshBuffers.BoneWeights, VertexBoneWeights);
        }

        if (ImportSettings.ImportAnimations)
        {
            for (uint32_t AnimationIndex = 0; AnimationIndex < Scene.mNumAnimations; ++AnimationIndex)
            {
                const aiAnimation* Animation = Scene.mAnimations[AnimationIndex];
                if (!Animation)
                {
                    continue;
                }

            AnimationPayload AnimationData{};
            AnimationData.Name = Animation->mName.length > 0 ? Animation->mName.C_Str() : ("Animation_" + std::to_string(AnimationIndex));
            const double TicksPerSecond = (Animation->mTicksPerSecond > 0.0) ? Animation->mTicksPerSecond : 30.0;
            AnimationData.TicksPerSecond = static_cast<float>(TicksPerSecond);
            AnimationData.DurationSeconds = static_cast<float>(
                (Animation->mDuration > 0.0 && TicksPerSecond > 0.0) ? (Animation->mDuration / TicksPerSecond) : 0.0);

            for (uint32_t ChannelIndex = 0; ChannelIndex < Animation->mNumChannels; ++ChannelIndex)
            {
                const aiNodeAnim* Channel = Animation->mChannels[ChannelIndex];
                if (!Channel)
                {
                    continue;
                }

                std::set<double> KeyTimes{};
                for (uint32_t KeyIndex = 0; KeyIndex < Channel->mNumPositionKeys; ++KeyIndex)
                {
                    KeyTimes.insert(Channel->mPositionKeys[KeyIndex].mTime);
                }
                for (uint32_t KeyIndex = 0; KeyIndex < Channel->mNumRotationKeys; ++KeyIndex)
                {
                    KeyTimes.insert(Channel->mRotationKeys[KeyIndex].mTime);
                }
                for (uint32_t KeyIndex = 0; KeyIndex < Channel->mNumScalingKeys; ++KeyIndex)
                {
                    KeyTimes.insert(Channel->mScalingKeys[KeyIndex].mTime);
                }
                if (KeyTimes.empty())
                {
                    KeyTimes.insert(0.0);
                }

                AnimationTrackPayload Track{};
                Track.BoneName = Channel->mNodeName.C_Str();
                Track.KeyFrames.reserve(KeyTimes.size());

                for (const double KeyTime : KeyTimes)
                {
                    const aiVector3D Translation = SampleVectorKeys(Channel->mPositionKeys, Channel->mNumPositionKeys, KeyTime);
                    aiQuaternion Rotation = SampleQuatKeys(Channel->mRotationKeys, Channel->mNumRotationKeys, KeyTime);
                    const aiVector3D Scale = SampleVectorKeys(Channel->mScalingKeys, Channel->mNumScalingKeys, KeyTime);

                    if (!IsFiniteVec3(Translation) || !IsFiniteVec3(Scale) || !IsFiniteQuat(Rotation))
                    {
                        continue;
                    }

                    AnimationKeyFramePayload KeyFrame{};
                    KeyFrame.Time = static_cast<float>((TicksPerSecond > 0.0) ? (KeyTime / TicksPerSecond) : 0.0);
                    KeyFrame.Translation = {Translation.x, Translation.y, Translation.z};
                    KeyFrame.Rotation = {Rotation.x, Rotation.y, Rotation.z, Rotation.w};
                    KeyFrame.Scale = {Scale.x, Scale.y, Scale.z};
                    Track.KeyFrames.push_back(std::move(KeyFrame));
                }

                if (!Track.KeyFrames.empty())
                {
                    AnimationData.Tracks.push_back(std::move(Track));
                }
            }

                if (!AnimationData.Tracks.empty())
                {
                    MeshBuffers.Animations.push_back(std::move(AnimationData));
                }
            }
        }
    }

    std::optional<ImportedItem> SkeletonItem{};
    std::vector<ImportedItem> AnimationItems{};
    if (ImportAsSkeletal)
    {
        if (ImportSettings.ImportSkeleton)
        {
            ImportedItem BuiltSkeletonItem{};
            BuiltSkeletonItem.LogicalName = MakeScopedLogicalName(BaseLogicalName, "skeleton", "default", 0u);
            BuiltSkeletonItem.AssetKind = AssetKindSkeleton();
            BuiltSkeletonItem.VariantKey = VariantKey;
            BuiltSkeletonItem.Id = Ctx.MakeDeterministicAssetId(BuiltSkeletonItem.LogicalName, BuiltSkeletonItem.VariantKey);
            BuiltSkeletonItem.Dependencies.emplace_back(Source.Uri, Source.ContentHash);
            BuiltSkeletonItem.Intermediate.PayloadType = PayloadSkeleton();
            BuiltSkeletonItem.Intermediate.SchemaVersion = SkeletonSerializer->GetSchemaVersion();

            SkeletonPayload SkeletonData{};
            SkeletonData.Name = std::filesystem::path(Source.Uri).stem().string();
            SkeletonData.Bones = MeshBuffers.Bones;
            SkeletonSerializer->SerializeToBytes(&SkeletonData, BuiltSkeletonItem.Intermediate.Bytes);
            GeneratedItems.push_back(BuiltSkeletonItem);
            SkeletonItem = std::move(BuiltSkeletonItem);
        }

        if (ImportSettings.ImportAnimations)
        {
            for (uint32_t AnimationIndex = 0; AnimationIndex < MeshBuffers.Animations.size(); ++AnimationIndex)
            {
                ImportedItem AnimationItem{};
                AnimationItem.LogicalName =
                    MakeScopedLogicalName(BaseLogicalName, "animation", MeshBuffers.Animations[AnimationIndex].Name, AnimationIndex);
                AnimationItem.AssetKind = AssetKindAnimation();
                AnimationItem.VariantKey = VariantKey;
                AnimationItem.Id = Ctx.MakeDeterministicAssetId(AnimationItem.LogicalName, AnimationItem.VariantKey);
                AnimationItem.Dependencies.emplace_back(Source.Uri, Source.ContentHash);
                AnimationItem.Intermediate.PayloadType = PayloadAnimation();
                AnimationItem.Intermediate.SchemaVersion = AnimationSerializer->GetSchemaVersion();
                AnimationSerializer->SerializeToBytes(&MeshBuffers.Animations[AnimationIndex], AnimationItem.Intermediate.Bytes);
                AnimationItems.push_back(AnimationItem);
                GeneratedItems.push_back(std::move(AnimationItem));
            }
        }
    }

    if (!ImportAsSkeletal)
    {
        ImportedItem MeshItem{};
        MeshItem.LogicalName = MakeScopedLogicalName(BaseLogicalName, "mesh", "static", 0u);
        MeshItem.AssetKind = AssetKindStaticMesh();
        MeshItem.VariantKey = VariantKey;
        MeshItem.Id = Ctx.MakeDeterministicAssetId(MeshItem.LogicalName, MeshItem.VariantKey);
        MeshItem.Dependencies.emplace_back(Source.Uri, Source.ContentHash);
        MeshItem.Intermediate.PayloadType = PayloadStaticMeshSource();
        MeshItem.Intermediate.SchemaVersion = StaticMeshSourceSerializer->GetSchemaVersion();

        StaticMeshAsset SourcePayload{};
        SourcePayload.ImportSettings = ImportConfig;
        SourcePayload.Mesh.Name = std::filesystem::path(Source.Uri).stem().string();
        SourcePayload.Mesh.BoundsMin = MeshBuffers.BoundsMin;
        SourcePayload.Mesh.BoundsMax = MeshBuffers.BoundsMax;
        SourcePayload.Mesh.SubMeshes = MeshBuffers.SubMeshes;

        const uint32_t MaterialSlotCount = std::max<uint32_t>(1u, MeshBuffers.MaxMaterialSlot + 1);
        SourcePayload.Mesh.MaterialInstances.resize(MaterialSlotCount);
        for (uint32_t Slot = 0; Slot < MaterialSlotCount; ++Slot)
        {
            const auto It = MaterialOutputs.MaterialInstanceRefsBySlot.find(Slot);
            if (It != MaterialOutputs.MaterialInstanceRefsBySlot.end())
            {
                SourcePayload.Mesh.MaterialInstances[Slot] = It->second;
            }
        }

        auto AddStream = [&](const EMeshStreamSemantic Semantic, const uint32_t StrideBytes, const std::vector<uint8_t>& Bytes) {
            if (Bytes.empty())
            {
                return;
            }
            MeshStreamSourcePayload Stream{};
            Stream.Semantic = Semantic;
            Stream.SubIndex = static_cast<uint32_t>(Semantic);
            Stream.Bytes = Bytes;
            Stream.ElementCount = static_cast<uint32_t>(Bytes.size() / StrideBytes);
            Stream.StrideBytes = StrideBytes;
            Stream.Compress = true;
            SourcePayload.Streams.push_back(std::move(Stream));
        };

        AddStream(EMeshStreamSemantic::Position, sizeof(float) * 3u, MeshBuffers.Positions);
        AddStream(EMeshStreamSemantic::Normal, sizeof(float) * 3u, MeshBuffers.Normals);
        AddStream(EMeshStreamSemantic::Tangent, sizeof(float) * 4u, MeshBuffers.Tangents);
        AddStream(EMeshStreamSemantic::UV0, sizeof(float) * 2u, MeshBuffers.UV0);
        AddStream(EMeshStreamSemantic::UV1, sizeof(float) * 2u, MeshBuffers.UV1);
        AddStream(EMeshStreamSemantic::Color, sizeof(float) * 4u, MeshBuffers.Colors);
        AddStream(EMeshStreamSemantic::Index, sizeof(uint32_t), MeshBuffers.Indices);

        StaticMeshSourceSerializer->SerializeToBytes(&SourcePayload, MeshItem.Intermediate.Bytes);
        GeneratedItems.push_back(std::move(MeshItem));
    }
    else
    {
        ImportedItem MeshItem{};
        MeshItem.LogicalName = MakeScopedLogicalName(BaseLogicalName, "mesh", "skeletal", 0u);
        MeshItem.AssetKind = AssetKindSkeletalMesh();
        MeshItem.VariantKey = VariantKey;
        MeshItem.Id = Ctx.MakeDeterministicAssetId(MeshItem.LogicalName, MeshItem.VariantKey);
        MeshItem.Dependencies.emplace_back(Source.Uri, Source.ContentHash);
        MeshItem.Intermediate.PayloadType = PayloadSkeletalMeshSource();
        MeshItem.Intermediate.SchemaVersion = SkeletalMeshSourceSerializer->GetSchemaVersion();

        SkeletalMeshAsset SourcePayload{};
        SourcePayload.BaseMesh.ImportSettings = ImportConfig;
        SourcePayload.BaseMesh.Mesh.Name = std::filesystem::path(Source.Uri).stem().string();
        SourcePayload.BaseMesh.Mesh.BoundsMin = MeshBuffers.BoundsMin;
        SourcePayload.BaseMesh.Mesh.BoundsMax = MeshBuffers.BoundsMax;
        SourcePayload.BaseMesh.Mesh.SubMeshes = MeshBuffers.SubMeshes;
        SourcePayload.Bones = MeshBuffers.Bones;
        if (SkeletonItem.has_value())
        {
            SourcePayload.Skeleton = MakeAssetRef(*SkeletonItem);
        }
        SourcePayload.Animations.reserve(AnimationItems.size());
        for (const ImportedItem& AnimationItem : AnimationItems)
        {
            SourcePayload.Animations.push_back(MakeAssetRef(AnimationItem));
        }

        const uint32_t MaterialSlotCount = std::max<uint32_t>(1u, MeshBuffers.MaxMaterialSlot + 1);
        SourcePayload.BaseMesh.Mesh.MaterialInstances.resize(MaterialSlotCount);
        for (uint32_t Slot = 0; Slot < MaterialSlotCount; ++Slot)
        {
            const auto It = MaterialOutputs.MaterialInstanceRefsBySlot.find(Slot);
            if (It != MaterialOutputs.MaterialInstanceRefsBySlot.end())
            {
                SourcePayload.BaseMesh.Mesh.MaterialInstances[Slot] = It->second;
            }
        }

        auto AddStream = [&](const EMeshStreamSemantic Semantic, const uint32_t StrideBytes, const std::vector<uint8_t>& Bytes) {
            if (Bytes.empty())
            {
                return;
            }
            MeshStreamSourcePayload Stream{};
            Stream.Semantic = Semantic;
            Stream.SubIndex = static_cast<uint32_t>(Semantic);
            Stream.Bytes = Bytes;
            Stream.ElementCount = static_cast<uint32_t>(Bytes.size() / StrideBytes);
            Stream.StrideBytes = StrideBytes;
            Stream.Compress = true;
            SourcePayload.BaseMesh.Streams.push_back(std::move(Stream));
        };

        AddStream(EMeshStreamSemantic::Position, sizeof(float) * 3u, MeshBuffers.Positions);
        AddStream(EMeshStreamSemantic::Normal, sizeof(float) * 3u, MeshBuffers.Normals);
        AddStream(EMeshStreamSemantic::Tangent, sizeof(float) * 4u, MeshBuffers.Tangents);
        AddStream(EMeshStreamSemantic::UV0, sizeof(float) * 2u, MeshBuffers.UV0);
        AddStream(EMeshStreamSemantic::UV1, sizeof(float) * 2u, MeshBuffers.UV1);
        AddStream(EMeshStreamSemantic::Color, sizeof(float) * 4u, MeshBuffers.Colors);
        AddStream(EMeshStreamSemantic::BoneIndices, sizeof(uint32_t) * 4u, MeshBuffers.BoneIndices);
        AddStream(EMeshStreamSemantic::BoneWeights, sizeof(float) * 4u, MeshBuffers.BoneWeights);
        AddStream(EMeshStreamSemantic::Index, sizeof(uint32_t), MeshBuffers.Indices);

        SkeletalMeshSourceSerializer->SerializeToBytes(&SourcePayload, MeshItem.Intermediate.Bytes);
        GeneratedItems.push_back(std::move(MeshItem));
    }

    for (ImportedItem& Item : GeneratedItems)
    {
        std::unordered_set<std::string> SeenDependencies{};
        std::vector<SourceRef> UniqueDependencies{};
        for (const SourceRef& Existing : Item.Dependencies)
        {
            AppendDependencyUnique(UniqueDependencies, SeenDependencies, Existing.Uri);
        }
        for (const std::string& TextureUri : MaterialOutputs.TextureDependencies)
        {
            AppendDependencyUnique(UniqueDependencies, SeenDependencies, TextureUri);
        }
        Item.Dependencies = std::move(UniqueDependencies);
        OutItems.push_back(std::move(Item));
    }

    return true;
}

class RenderAssetAssimpImporter final : public ::SnAPI::AssetPipeline::IAssetImporter
{
public:
    const char* GetName() const override
    {
        return "SnAPI.GameFramework.RenderAssetAssimpImporter";
    }

    bool CanImport(const SourceRef& Source) const override
    {
        return HasSupportedModelExtension(Source.Uri);
    }

    bool Import(const SourceRef& Source, std::vector<ImportedItem>& OutItems, IPipelineContext& Ctx) override
    {
        return ImportWithSettings(Source, nullptr, OutItems, Ctx);
    }

    bool ImportWithSettings(const SourceRef& Source,
                            const ::SnAPI::AssetPipeline::IAssetImportSettings* Settings,
                            std::vector<ImportedItem>& OutItems,
                            IPipelineContext& Ctx) override
    {
        const AssimpImporterSettings ImportSettings = ReadAssimpImportSettings(Settings, Ctx);

        unsigned int Flags = aiProcess_Triangulate
            | aiProcess_JoinIdenticalVertices
            | aiProcess_SortByPType
            | aiProcess_ImproveCacheLocality
            | aiProcess_ValidateDataStructure
            | aiProcess_LimitBoneWeights;

        if (ImportSettings.Mesh.GenerateNormals)
        {
            Flags |= aiProcess_GenSmoothNormals;
        }
        if (ImportSettings.Mesh.GenerateTangents)
        {
            Flags |= aiProcess_CalcTangentSpace;
        }
        if (ImportSettings.Mesh.OptimizeMeshes)
        {
            Flags |= aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph;
        }
        if (ImportSettings.Mesh.FlipUVs)
        {
            Flags |= aiProcess_FlipUVs;
        }

        Assimp::Importer Importer{};
        const aiScene* Scene = nullptr;

        std::vector<std::uint8_t> SourceBytes{};
        const std::string ExtensionHint = SourceExtensionHint(Source.Uri);
        bool UsedMemoryImport = false;
        std::optional<ExtractedGlbSource> ExtractedGlb{};
        auto CleanupExtractedGlb = [&ExtractedGlb]() {
            if (!ExtractedGlb)
            {
                return;
            }

            std::error_code RemoveError{};
            std::filesystem::remove_all(ExtractedGlb->TempDirectory, RemoveError);
        };
        if (ExtensionHint == "glb" && Ctx.ReadAllBytes(Source.Uri, SourceBytes) && !SourceBytes.empty())
        {
            ExtractedGlb = ExtractGlbSource(Source.Uri, SourceBytes, Ctx);
            if (ExtractedGlb)
            {
                Scene = Importer.ReadFile(ExtractedGlb->GltfPath.string(), Flags);
                if (!Scene || !Scene->mRootNode)
                {
                    Ctx.LogError("RenderAsset Assimp importer failed to load extracted GLB surrogate %s for %s: %s",
                                 ExtractedGlb->GltfPath.string().c_str(),
                                 Source.Uri.c_str(),
                                 Importer.GetErrorString());
                    CleanupExtractedGlb();
                    return false;
                }
            }
        }

        if (!Scene || !Scene->mRootNode)
        {
            Scene = Importer.ReadFile(Source.Uri, Flags);
        }
        if ((!Scene || !Scene->mRootNode) && Ctx.ReadAllBytes(Source.Uri, SourceBytes) && !SourceBytes.empty())
        {
            UsedMemoryImport = true;
            Scene = Importer.ReadFileFromMemory(
                SourceBytes.data(),
                SourceBytes.size(),
                Flags,
                ExtensionHint.c_str());
        }
        if (!Scene || !Scene->mRootNode)
        {
            if (UsedMemoryImport)
            {
                Ctx.LogError("RenderAsset Assimp importer failed to load %s from memory (%zu bytes): %s",
                             Source.Uri.c_str(),
                             SourceBytes.size(),
                             Importer.GetErrorString());
            }
            else
            {
                Ctx.LogError("RenderAsset Assimp importer failed to load %s: %s",
                             Source.Uri.c_str(),
                             Importer.GetErrorString());
            }
            CleanupExtractedGlb();
            return false;
        }

        CleanupExtractedGlb();

        const std::size_t ExistingCount = OutItems.size();
        if (!BuildAssimpItems(Source, *Scene, ImportSettings, OutItems, Ctx))
        {
            Ctx.LogError("RenderAsset Assimp importer failed to build imported items for %s", Source.Uri.c_str());
            return false;
        }

        if (Settings)
        {
            auto Cloned = Settings->Clone();
            if (Cloned)
            {
                const ::SnAPI::AssetPipeline::AssetImportSettingsPtr SharedSettings(std::move(Cloned));
                for (std::size_t Index = ExistingCount; Index < OutItems.size(); ++Index)
                {
                    OutItems[Index].ImportSettings = SharedSettings;
                }
            }
        }

        Ctx.LogInfo("RenderAsset Assimp importer produced %zu assets from %s", OutItems.size(), Source.Uri.c_str());
        return true;
    }
};

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetAssimpImporter()
{
    return std::make_unique<RenderAssetAssimpImporter>();
}

} // namespace SnAPI::GameFramework
