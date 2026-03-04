#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>

#include "AssetPipelineIds.h"
#include "AssetPipelineSerializers.h"
#include "IAssetImporter.h"
#include "IPipelineContext.h"
#include "IPayloadSerializer.h"
#include "RenderAssetPayloads.h"
#include "RenderAssetSourcePayloads.h"
#include "TextureCompressorIds.h"

namespace SnAPI::GameFramework
{
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetAssimpImporter();
}

namespace
{
using SnAPI::AssetPipeline::AssetId;
using SnAPI::AssetPipeline::ImportedItem;
using SnAPI::AssetPipeline::IPayloadSerializer;
using SnAPI::AssetPipeline::SourceRef;
using SnAPI::AssetPipeline::TypeId;
using SnAPI::AssetPipeline::Uuid;
using SnAPI::AssetPipeline::UuidHash;
using SnAPI::GameFramework::AssetKindMaterialInstance;
using SnAPI::GameFramework::AssetKindMaterial;
using SnAPI::GameFramework::AssetKindStaticMesh;
using SnAPI::GameFramework::CreateAnimationPayloadSerializer;
using SnAPI::GameFramework::CreateMaterialInstancePayloadSerializer;
using SnAPI::GameFramework::CreateMaterialPayloadSerializer;
using SnAPI::GameFramework::CreateSkeletalMeshSourcePayloadSerializer;
using SnAPI::GameFramework::CreateSkeletonPayloadSerializer;
using SnAPI::GameFramework::CreateStaticMeshSourcePayloadSerializer;
using SnAPI::GameFramework::DeserializeMaterialPayload;
using SnAPI::GameFramework::DeserializeMaterialInstancePayload;
using SnAPI::GameFramework::MaterialPayload;
using SnAPI::GameFramework::MaterialInstancePayload;

class DummyTextureIntermediateSerializer final : public IPayloadSerializer
{
public:
    TypeId GetTypeId() const override
    {
        return TextureCompressorPlugin::Payload_CompressorImageIntermediate;
    }

    const char* GetTypeName() const override
    {
        return "TextureCompressor.ImageIntermediate";
    }

    uint32_t GetSchemaVersion() const override
    {
        return 1u;
    }

    void SerializeToBytes(const void*, std::vector<uint8_t>& OutBytes) const override
    {
        OutBytes.clear();
    }

    bool DeserializeFromBytes(void*, const uint8_t*, std::size_t) const override
    {
        return false;
    }
};

class TestPipelineContext final : public SnAPI::AssetPipeline::IPipelineContext
{
public:
    void RegisterSerializer(std::unique_ptr<IPayloadSerializer> Serializer)
    {
        if (!Serializer)
        {
            return;
        }
        const TypeId Id = Serializer->GetTypeId();
        m_serializersById[Id] = Serializer.get();
        m_serializers.emplace_back(std::move(Serializer));
    }

    void SetOption(std::string Key, std::string Value)
    {
        m_options[std::move(Key)] = std::move(Value);
    }

    [[nodiscard]] const std::vector<std::string>& Warnings() const
    {
        return m_warnings;
    }

    [[nodiscard]] const std::vector<std::string>& Errors() const
    {
        return m_errors;
    }

    void LogInfo(const char* Fmt, ...) override
    {
        va_list Args{};
        va_start(Args, Fmt);
        m_info.emplace_back(VFormat(Fmt, Args));
        va_end(Args);
    }

    void LogWarn(const char* Fmt, ...) override
    {
        va_list Args{};
        va_start(Args, Fmt);
        m_warnings.emplace_back(VFormat(Fmt, Args));
        va_end(Args);
    }

    void LogError(const char* Fmt, ...) override
    {
        va_list Args{};
        va_start(Args, Fmt);
        m_errors.emplace_back(VFormat(Fmt, Args));
        va_end(Args);
    }

    bool ReadAllBytes(const std::string& Uri, std::vector<uint8_t>& Out) override
    {
        std::ifstream File(Uri, std::ios::binary | std::ios::ate);
        if (!File.is_open())
        {
            return false;
        }

        const std::streamsize Size = File.tellg();
        if (Size < 0)
        {
            return false;
        }
        File.seekg(0, std::ios::beg);
        Out.resize(static_cast<size_t>(Size));
        if (Size > 0)
        {
            File.read(reinterpret_cast<char*>(Out.data()), Size);
        }
        return File.good() || File.eof();
    }

    uint64_t HashBytes64(const void* Data, std::size_t Size) override
    {
        constexpr uint64_t Offset = 1469598103934665603ull;
        constexpr uint64_t Prime = 1099511628211ull;
        uint64_t Hash = Offset;
        const auto* Bytes = static_cast<const uint8_t*>(Data);
        for (std::size_t Index = 0; Index < Size; ++Index)
        {
            Hash ^= static_cast<uint64_t>(Bytes[Index]);
            Hash *= Prime;
        }
        return Hash;
    }

    void HashBytes128(const void* Data, std::size_t Size, uint64_t& OutHi, uint64_t& OutLo) override
    {
        OutLo = HashBytes64(Data, Size);
        OutHi = HashBytes64(Data, Size / 2u + (Size % 2u));
    }

    AssetId MakeDeterministicAssetId(std::string_view LogicalName, std::string_view VariantKey) override
    {
        static const auto Namespace = Uuid::FromString("84e51592-7be4-4f44-a3ef-0d8e2f288290");
        std::string Key(LogicalName);
        Key.push_back('|');
        Key.append(VariantKey);
        return Uuid::GenerateV5(Namespace, Key);
    }

    const IPayloadSerializer* FindSerializer(TypeId Id) const override
    {
        const auto It = m_serializersById.find(Id);
        return (It == m_serializersById.end()) ? nullptr : It->second;
    }

    std::string GetOption(std::string_view Key, std::string_view Default = {}) const override
    {
        const auto It = m_options.find(std::string(Key));
        if (It != m_options.end())
        {
            return It->second;
        }
        return std::string(Default);
    }

private:
    static std::string VFormat(const char* Fmt, va_list Args)
    {
        std::array<char, 2048> Buffer{};
        va_list Copy{};
        va_copy(Copy, Args);
        const int Written = std::vsnprintf(Buffer.data(), Buffer.size(), Fmt, Copy);
        va_end(Copy);
        if (Written <= 0)
        {
            return {};
        }
        if (static_cast<std::size_t>(Written) < Buffer.size())
        {
            return std::string(Buffer.data(), static_cast<std::size_t>(Written));
        }
        return std::string(Buffer.data(), Buffer.size() - 1u);
    }

    std::vector<std::unique_ptr<IPayloadSerializer>> m_serializers{};
    std::unordered_map<TypeId, const IPayloadSerializer*, UuidHash> m_serializersById{};
    std::unordered_map<std::string, std::string> m_options{};
    std::vector<std::string> m_info{};
    std::vector<std::string> m_warnings{};
    std::vector<std::string> m_errors{};
};

struct TempDir
{
    std::filesystem::path Path{};

    TempDir()
    {
        const auto Stamp = std::to_string(static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        Path = std::filesystem::temp_directory_path() / ("snapi_gf_assimp_test_" + Stamp);
        std::filesystem::create_directories(Path);
    }

    ~TempDir()
    {
        std::error_code Ec{};
        std::filesystem::remove_all(Path, Ec);
    }
};

void AppendFloat(std::vector<uint8_t>& Bytes, const float Value)
{
    const size_t Offset = Bytes.size();
    Bytes.resize(Offset + sizeof(float));
    std::memcpy(Bytes.data() + Offset, &Value, sizeof(float));
}

void AppendU16(std::vector<uint8_t>& Bytes, const uint16_t Value)
{
    const size_t Offset = Bytes.size();
    Bytes.resize(Offset + sizeof(uint16_t));
    std::memcpy(Bytes.data() + Offset, &Value, sizeof(uint16_t));
}

void WriteBinaryFile(const std::filesystem::path& Path, const std::vector<uint8_t>& Bytes)
{
    std::ofstream Out(Path, std::ios::binary);
    REQUIRE(Out.is_open());
    Out.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
    REQUIRE(Out.good());
}

void WriteTextFile(const std::filesystem::path& Path, const std::string& Text)
{
    std::ofstream Out(Path, std::ios::binary);
    REQUIRE(Out.is_open());
    Out.write(Text.data(), static_cast<std::streamsize>(Text.size()));
    REQUIRE(Out.good());
}

[[nodiscard]] std::string JoinMessages(const std::vector<std::string>& Messages)
{
    if (Messages.empty())
    {
        return {};
    }

    std::ostringstream Stream{};
    for (size_t Index = 0; Index < Messages.size(); ++Index)
    {
        if (Index > 0u)
        {
            Stream << " | ";
        }
        Stream << Messages[Index];
    }
    return Stream.str();
}

std::filesystem::path WriteEmbeddedTextureGltfFixture(const std::filesystem::path& RootDir)
{
    std::filesystem::create_directories(RootDir);

    std::vector<uint8_t> Buffer{};
    Buffer.reserve(256);

    // POSITION (3 * vec3 float)
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    AppendFloat(Buffer, 0.0f);
    constexpr uint32_t PositionByteLength = 9u * sizeof(float);

    // TEXCOORD_0 (3 * vec2 float)
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 0.0f);
    AppendFloat(Buffer, 1.0f);
    constexpr uint32_t TexcoordByteOffset = PositionByteLength;
    constexpr uint32_t TexcoordByteLength = 6u * sizeof(float);

    // INDICES (3 * uint16)
    AppendU16(Buffer, 0u);
    AppendU16(Buffer, 1u);
    AppendU16(Buffer, 2u);
    constexpr uint32_t IndexByteOffset = TexcoordByteOffset + TexcoordByteLength;
    constexpr uint32_t IndexByteLength = 3u * sizeof(uint16_t);

    while ((Buffer.size() % 4u) != 0u)
    {
        Buffer.push_back(0u);
    }

    // Tiny 1x1 PNG generated by FreeImage to match importer decode expectations.
    static constexpr std::array<uint8_t, 90> kPng{
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
        0xDE,0x00,0x00,0x00,0x09,0x70,0x48,0x59,
        0x73,0x00,0x00,0x0B,0x13,0x00,0x00,0x0B,
        0x13,0x01,0x00,0x9A,0x9C,0x18,0x00,0x00,
        0x00,0x0C,0x49,0x44,0x41,0x54,0x08,0x99,
        0x63,0xF8,0xCF,0xC0,0x00,0x00,0x03,0x01,
        0x01,0x00,0x9C,0xE3,0xBF,0x59,0x00,0x00,
        0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,
        0x60,0x82
    };

    const uint32_t ImageByteOffset = static_cast<uint32_t>(Buffer.size());
    Buffer.insert(Buffer.end(), kPng.begin(), kPng.end());
    const uint32_t ImageByteLength = static_cast<uint32_t>(kPng.size());
    const uint32_t BufferByteLength = static_cast<uint32_t>(Buffer.size());

    const std::filesystem::path BinPath = RootDir / "mesh.bin";
    WriteBinaryFile(BinPath, Buffer);

    std::ostringstream Json{};
    Json
        << "{\n"
        << "  \"asset\": {\"version\": \"2.0\"},\n"
        << "  \"buffers\": [{\"uri\": \"mesh.bin\", \"byteLength\": " << BufferByteLength << "}],\n"
        << "  \"bufferViews\": [\n"
        << "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": " << PositionByteLength << ", \"target\": 34962},\n"
        << "    {\"buffer\": 0, \"byteOffset\": " << TexcoordByteOffset << ", \"byteLength\": " << TexcoordByteLength << ", \"target\": 34962},\n"
        << "    {\"buffer\": 0, \"byteOffset\": " << IndexByteOffset << ", \"byteLength\": " << IndexByteLength << ", \"target\": 34963},\n"
        << "    {\"buffer\": 0, \"byteOffset\": " << ImageByteOffset << ", \"byteLength\": " << ImageByteLength << "}\n"
        << "  ],\n"
        << "  \"accessors\": [\n"
        << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\", \"min\": [0, 0, 0], \"max\": [1, 1, 0]},\n"
        << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC2\"},\n"
        << "    {\"bufferView\": 2, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\"}\n"
        << "  ],\n"
        << "  \"images\": [{\"bufferView\": 3, \"mimeType\": \"image/png\", \"name\": \"EmbeddedTexture\"}],\n"
        << "  \"samplers\": [{\"magFilter\": 9729, \"minFilter\": 9729, \"wrapS\": 10497, \"wrapT\": 10497}],\n"
        << "  \"textures\": [{\"source\": 0, \"sampler\": 0}],\n"
        << "  \"materials\": [{\"name\": \"Mat0\", \"pbrMetallicRoughness\": {\"baseColorTexture\": {\"index\": 0}, \"metallicFactor\": 0.0, \"roughnessFactor\": 1.0}}],\n"
        << "  \"meshes\": [{\"primitives\": [{\"attributes\": {\"POSITION\": 0, \"TEXCOORD_0\": 1}, \"indices\": 2, \"material\": 0}]}],\n"
        << "  \"nodes\": [{\"mesh\": 0}],\n"
        << "  \"scenes\": [{\"nodes\": [0]}],\n"
        << "  \"scene\": 0\n"
        << "}\n";

    const std::filesystem::path GltfPath = RootDir / "embedded_textures.gltf";
    WriteTextFile(GltfPath, Json.str());
    return GltfPath;
}

} // namespace

TEST_CASE("Assimp importer emits texture assets for embedded model textures and material refs point to them", "[asset][assimp]")
{
    TestPipelineContext Context{};
    Context.RegisterSerializer(CreateMaterialPayloadSerializer());
    Context.RegisterSerializer(CreateMaterialInstancePayloadSerializer());
    Context.RegisterSerializer(CreateSkeletonPayloadSerializer());
    Context.RegisterSerializer(CreateAnimationPayloadSerializer());
    Context.RegisterSerializer(CreateStaticMeshSourcePayloadSerializer());
    Context.RegisterSerializer(CreateSkeletalMeshSourcePayloadSerializer());
    Context.RegisterSerializer(std::make_unique<DummyTextureIntermediateSerializer>());

    Context.SetOption("SnAPI.GF.Assimp.ForceStatic", "true");
    Context.SetOption("SnAPI.GF.Assimp.GenerateNormals", "true");
    Context.SetOption("SnAPI.GF.Assimp.GenerateTangents", "true");

    TempDir Dir{};
    const std::filesystem::path SourcePath = WriteEmbeddedTextureGltfFixture(Dir.Path);

    auto Importer = SnAPI::GameFramework::CreateRenderAssetAssimpImporter();
    REQUIRE(Importer != nullptr);
    REQUIRE(Importer->CanImport(SourceRef(SourcePath.string())));

    std::vector<ImportedItem> Items{};
    const bool Imported = Importer->Import(SourceRef(SourcePath.string(), 123u), Items, Context);
    REQUIRE(Imported);
    REQUIRE(Context.Errors().empty());
    REQUIRE_FALSE(Items.empty());

    INFO("warnings: " << JoinMessages(Context.Warnings()));
    INFO("imported item count: " << Items.size());
    for (const ImportedItem& Item : Items)
    {
        INFO("item kind=" << Item.AssetKind.ToString()
            << " logical=" << Item.LogicalName
            << " payload=" << Item.Intermediate.PayloadType.ToString()
            << " bytes=" << Item.Intermediate.Bytes.size());
    }

    std::vector<const ImportedItem*> TextureItems{};
    std::vector<const ImportedItem*> MaterialItems{};
    std::vector<const ImportedItem*> MaterialInstanceItems{};
    bool HasStaticMesh = false;

    for (const ImportedItem& Item : Items)
    {
        if (Item.AssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture)
        {
            TextureItems.push_back(&Item);
        }
        if (Item.AssetKind == AssetKindMaterial())
        {
            MaterialItems.push_back(&Item);
        }
        if (Item.AssetKind == AssetKindMaterialInstance())
        {
            MaterialInstanceItems.push_back(&Item);
        }
        if (Item.AssetKind == AssetKindStaticMesh())
        {
            HasStaticMesh = true;
        }
    }

    REQUIRE_FALSE(TextureItems.empty());
    REQUIRE_FALSE(MaterialItems.empty());
    REQUIRE_FALSE(MaterialInstanceItems.empty());
    REQUIRE(HasStaticMesh);

    std::unordered_map<std::string, std::string> ImportedTextureNameToId{};
    for (const ImportedItem* Item : TextureItems)
    {
        REQUIRE(Item != nullptr);
        REQUIRE(Item->Intermediate.PayloadType == TextureCompressorPlugin::Payload_CompressorImageIntermediate);
        ImportedTextureNameToId.emplace(Item->LogicalName, Item->Id.ToString());
    }

    std::unordered_map<std::string, std::string> MaterialNameToId{};
    std::unordered_map<std::string, MaterialPayload> MaterialByName{};
    for (const ImportedItem* Item : MaterialItems)
    {
        REQUIRE(Item != nullptr);
        auto MaterialPayloadResult = DeserializeMaterialPayload(
            Item->Intermediate.Bytes.data(),
            Item->Intermediate.Bytes.size());
        REQUIRE(MaterialPayloadResult.has_value());
        const MaterialPayload& Material = MaterialPayloadResult.value();
        REQUIRE(Material.ShadingModel == "GBufferShadingModel");
        REQUIRE(Material.ShaderModule == "DefaultGBufferMaterial");
        MaterialNameToId.emplace(Item->LogicalName, Item->Id.ToString());
        MaterialByName.emplace(Item->LogicalName, Material);
    }

    bool FoundEmbeddedTextureRef = false;
    bool FoundAlbedoSlot = false;
    bool FoundNonZeroColorDefault = false;
    bool FoundOcclusionDefault = false;
    for (const ImportedItem* MatItem : MaterialInstanceItems)
    {
        REQUIRE(MatItem != nullptr);
        auto MatPayloadResult = DeserializeMaterialInstancePayload(
            MatItem->Intermediate.Bytes.data(),
            MatItem->Intermediate.Bytes.size());
        REQUIRE(MatPayloadResult.has_value());
        const MaterialInstancePayload& MatPayload = MatPayloadResult.value();

        REQUIRE_FALSE(MatPayload.ParentMaterial.AssetName.empty());
        REQUIRE_FALSE(MatPayload.ParentMaterial.AssetId.empty());
        const auto ParentIt = MaterialNameToId.find(MatPayload.ParentMaterial.AssetName);
        REQUIRE(ParentIt != MaterialNameToId.end());
        REQUIRE(MatPayload.ParentMaterial.AssetId == ParentIt->second);

        bool HasColor = false;
        bool HasRoughness = false;
        bool HasMetallic = false;
        bool HasOcclusion = false;
        for (const auto& VectorParam : MatPayload.Vectors)
        {
            if (VectorParam.Name == "Color")
            {
                HasColor = true;
                if (VectorParam.Value[0] > 0.0f &&
                    VectorParam.Value[1] > 0.0f &&
                    VectorParam.Value[2] > 0.0f &&
                    VectorParam.Value[3] > 0.0f)
                {
                    FoundNonZeroColorDefault = true;
                }
            }
        }
        for (const auto& ScalarParam : MatPayload.Scalars)
        {
            if (ScalarParam.Name == "Roughness")
            {
                HasRoughness = true;
                REQUIRE(ScalarParam.Value >= 0.0f);
            }
            if (ScalarParam.Name == "Metallic")
            {
                HasMetallic = true;
                REQUIRE(ScalarParam.Value >= 0.0f);
            }
            if (ScalarParam.Name == "Occlusion")
            {
                HasOcclusion = true;
                if (ScalarParam.Value > 0.0f)
                {
                    FoundOcclusionDefault = true;
                }
            }
        }
        REQUIRE(HasColor);
        REQUIRE(HasRoughness);
        REQUIRE(HasMetallic);
        REQUIRE(HasOcclusion);

        for (const auto& TextureParam : MatPayload.Textures)
        {
            if (TextureParam.SlotName == "Material_Albedo")
            {
                FoundAlbedoSlot = true;
                REQUIRE(MaterialByName.at(MatPayload.ParentMaterial.AssetName).FeatureAlbedoMap);
            }
            if (TextureParam.SlotName == "Material_Normal")
            {
                REQUIRE(MaterialByName.at(MatPayload.ParentMaterial.AssetName).FeatureNormalMap);
            }
            if (TextureParam.SlotName == "Material_ORM")
            {
                const auto& ParentMaterial = MaterialByName.at(MatPayload.ParentMaterial.AssetName);
                REQUIRE(ParentMaterial.FeatureRoughnessMap);
                REQUIRE(ParentMaterial.FeatureMetalnessMap);
                REQUIRE(ParentMaterial.FeatureOcclusionMap);
            }
            REQUIRE_FALSE(TextureParam.Texture.AssetName.empty());
            REQUIRE(TextureParam.Texture.AssetName[0] != '*');
            const auto It = ImportedTextureNameToId.find(TextureParam.Texture.AssetName);
            if (It != ImportedTextureNameToId.end())
            {
                REQUIRE(TextureParam.Texture.AssetId == It->second);
                FoundEmbeddedTextureRef = true;
            }
        }
    }

    REQUIRE(FoundAlbedoSlot);
    REQUIRE(FoundEmbeddedTextureRef);
    REQUIRE(FoundNonZeroColorDefault);
    REQUIRE(FoundOcclusionDefault);
}

TEST_CASE("Assimp importer honors disabled material and texture import options", "[asset][assimp]")
{
    TestPipelineContext Context{};
    Context.RegisterSerializer(CreateMaterialPayloadSerializer());
    Context.RegisterSerializer(CreateMaterialInstancePayloadSerializer());
    Context.RegisterSerializer(CreateSkeletonPayloadSerializer());
    Context.RegisterSerializer(CreateAnimationPayloadSerializer());
    Context.RegisterSerializer(CreateStaticMeshSourcePayloadSerializer());
    Context.RegisterSerializer(CreateSkeletalMeshSourcePayloadSerializer());
    Context.RegisterSerializer(std::make_unique<DummyTextureIntermediateSerializer>());

    Context.SetOption("SnAPI.GF.Assimp.ForceStatic", "true");
    Context.SetOption("SnAPI.GF.Assimp.ImportMaterials", "false");
    Context.SetOption("SnAPI.GF.Assimp.ImportTextures", "false");

    TempDir Dir{};
    const std::filesystem::path SourcePath = WriteEmbeddedTextureGltfFixture(Dir.Path);

    auto Importer = SnAPI::GameFramework::CreateRenderAssetAssimpImporter();
    REQUIRE(Importer != nullptr);
    REQUIRE(Importer->CanImport(SourceRef(SourcePath.string())));

    std::vector<ImportedItem> Items{};
    const bool Imported = Importer->Import(SourceRef(SourcePath.string(), 456u), Items, Context);
    REQUIRE(Imported);
    REQUIRE(Context.Errors().empty());
    REQUIRE_FALSE(Items.empty());

    const auto CountByKind = [&Items](const TypeId Kind) {
        return std::count_if(Items.begin(), Items.end(), [Kind](const ImportedItem& Item) {
            return Item.AssetKind == Kind;
        });
    };

    REQUIRE(CountByKind(TextureCompressorPlugin::AssetKind_CompressedTexture) == 0);
    REQUIRE(CountByKind(AssetKindMaterial()) == 0);
    REQUIRE(CountByKind(AssetKindMaterialInstance()) == 0);
    REQUIRE(CountByKind(AssetKindStaticMesh()) == 1);

    const auto MeshIt = std::ranges::find_if(Items, [](const ImportedItem& Item) {
        return Item.AssetKind == AssetKindStaticMesh();
    });
    REQUIRE(MeshIt != Items.end());
    REQUIRE(MeshIt->Intermediate.PayloadType == SnAPI::GameFramework::PayloadStaticMeshSource());

    auto MeshPayloadResult = SnAPI::GameFramework::DeserializeStaticMeshSourcePayload(
        MeshIt->Intermediate.Bytes.data(),
        MeshIt->Intermediate.Bytes.size());
    REQUIRE(MeshPayloadResult.has_value());

    const auto& MaterialRefs = MeshPayloadResult->Mesh.MaterialInstances;
    for (const auto& MaterialRef : MaterialRefs)
    {
        REQUIRE(MaterialRef.AssetName.empty());
        REQUIRE(MaterialRef.AssetId.empty());
    }
}
