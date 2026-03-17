#include "Editor/EditorAssetService.h"

#include "AuthoredAssetJson.h"
#include "AuthoredAssetRegistry.h"
#include "AssetRef.h"
#include "BaseNode.h"
#include "AssetPackReader.h"
#include "AssetPackWriter.h"
#include "AssetPipelineFactories.h"
#include "AssetPipelineIds.h"
#include "Conduit/Editor/Service.h"
#include "GameRuntime.h"
#include "IAssetCooker.h"
#include "IAssetImporter.h"
#include "IPipelineContext.h"
#include "Level.h"
#include "NodeAsset.h"
#include "NodeCast.h"
#include "PathResolver.h"
#include "PawnBase.h"
#include "PayloadRegistry.h"
#include "PlayerStart.h"
#include "RenderAssetPayloads.h"
#include "RenderAssetImportSettings.h"
#include "Serialization.h"
#include "StaticTypeId.h"
#include "TransformComponent.h"
#include "TypeAutoRegistry.h"
#include "TypeRegistry.h"
#include "World.h"
#include "WorldEcsRuntime.h"
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <Definitions.hpp>
#include <Material.hpp>
#include <MaterialInstance.hpp>
#include <MaterialRuntimeDescriptor.hpp>
#include <TMaterialFor.hpp>
#include "StaticMeshComponent.h"
#include "WorldRenderSettings.h"
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "ColliderComponent.h"
#include "CollisionFilters.h"
#include "RigidBodyComponent.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cctype>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#define XXH_INLINE_ALL
#include <xxhash.h>
#include <TextureCompressorIds.h>
#include <TextureCompressorImportSettings.h>

namespace TextureCompressorPlugin
{
std::unique_ptr<SnAPI::AssetPipeline::IPayloadSerializer> CreateCompressorImageIntermediateSerializer();
std::unique_ptr<SnAPI::AssetPipeline::IPayloadSerializer> CreateCompressorCookedInfoSerializer();
std::unique_ptr<SnAPI::AssetPipeline::IAssetImporter> CreateTextureCompressorImporter();
std::unique_ptr<SnAPI::AssetPipeline::IAssetCooker> CreateTextureCompressorCooker();
} // namespace TextureCompressorPlugin

namespace SnAPI::GameFramework
{
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateAuthoredAssetJsonImporter();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateAuthoredAssetPassThroughCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateNodeSourceCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateLevelSourceCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateWorldSourceCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetJsonImporter();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetAssimpImporter();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderTextureCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialInstanceCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderSkeletonCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderAnimationCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderStaticMeshCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderSkeletalMeshCooker();
} // namespace SnAPI::GameFramework

namespace cereal
{
template<class Archive>
void serialize(Archive& Ar, SnAPI::GameFramework::Editor::AssimpImportSettings& Value)
{
    Ar(cereal::make_nvp("GenerateNormals", Value.GenerateNormals),
       cereal::make_nvp("GenerateTangents", Value.GenerateTangents),
       cereal::make_nvp("FlipUVs", Value.FlipUVs),
       cereal::make_nvp("OptimizeMeshes", Value.OptimizeMeshes),
       cereal::make_nvp("ForceSkeletal", Value.ForceSkeletal),
       cereal::make_nvp("ForceStatic", Value.ForceStatic),
       cereal::make_nvp("ImportMaterials", Value.ImportMaterials),
       cereal::make_nvp("ImportTextures", Value.ImportTextures),
       cereal::make_nvp("ImportAnimations", Value.ImportAnimations),
       cereal::make_nvp("ImportSkeleton", Value.ImportSkeleton),
       cereal::make_nvp("MaxBonesPerVertex", Value.MaxBonesPerVertex));
}

template<class Archive>
void serialize(Archive& Ar, SnAPI::GameFramework::Editor::TextureImportSettings& Value)
{
    Ar(cereal::make_nvp("Target", Value.Target),
       cereal::make_nvp("Format", Value.Format),
       cereal::make_nvp("Quality", Value.Quality),
       cereal::make_nvp("ForceSrgb", Value.ForceSrgb),
       cereal::make_nvp("ForceLinear", Value.ForceLinear),
       cereal::make_nvp("ForceNormalMap", Value.ForceNormalMap),
       cereal::make_nvp("MaxMips", Value.MaxMips));
}
} // namespace cereal

namespace SnAPI::GameFramework::Editor
{
namespace
{
[[nodiscard]] std::string ToLowerCopy(std::string_view Text)
{
    std::string Output(Text);
    std::transform(Output.begin(), Output.end(), Output.begin(), [](const unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });
    return Output;
}

constexpr std::string_view kDefaultProjectFileName = "project.snproj.json";
constexpr std::string_view kDefaultProjectAssetRoot = "Assets";
constexpr std::string_view kDefaultProjectStartupLevelAsset = "Levels/StarterLevel.level";
constexpr std::string_view kEditorStarterLevelTemplateAssetFileName = "StarterLevelTemplate.level";
constexpr std::string_view kEditorStarterScriptFileName = "platform_bob.lua";
constexpr uint32_t kProjectConfigVersion = 1u;
constexpr uint32_t kMaterialPayloadSchemaVersion = 2u;
constexpr uint32_t kMaterialInstancePayloadSchemaVersion = 1u;
constexpr std::string_view kDefaultMaterialShaderModule = "DefaultGBufferMaterial";
constexpr std::string_view kDefaultMaterialShadingModel = "GBufferShadingModel";
constexpr std::string_view kAssetImportMetadataDirectoryName = ".snapi_editor";
constexpr std::string_view kAssetImportMetadataFileName = "asset_import_metadata.json";
constexpr uint32_t kAssetImportMetadataVersion = 1u;

using EImportProfile = EAssetImportProfile;

#if defined(SNAPI_GF_ENABLE_RENDERER)
struct WorldRenderSettingsRootSet
{
    std::vector<NodeHandle> AuthoredRoots{};
    std::vector<NodeHandle> TransientRoots{};
};

[[nodiscard]] WorldRenderSettingsRootSet CollectWorldRenderSettingsRoots(World& WorldRef)
{
    WorldRenderSettingsRootSet Result{};
    WorldRef.ForEachNode([&Result](const NodeHandle& Handle, BaseNode& Node) {
        if (NodeCast<WorldRenderSettings>(&Node) == nullptr)
        {
            return;
        }

        if (Node.EditorTransient())
        {
            Result.TransientRoots.push_back(Handle);
        }
        else
        {
            Result.AuthoredRoots.push_back(Handle);
        }
    });
    return Result;
}

void DestroyNodes(World& WorldRef, std::vector<NodeHandle>& Handles)
{
    for (NodeHandle& Handle : Handles)
    {
        (void)WorldRef.DestroyNode(Handle);
    }
}
#endif

struct AssetImportMetadataEntryDisk
{
    std::string AssetId{};
    std::string SourcePath{};
    std::string DestinationFolder{};
    std::string ImporterName{};
    std::string Profile{};
    std::unordered_map<std::string, std::string> BuildOptions{};
    Editor::AssimpImportSettings Assimp{};
    Editor::TextureImportSettings Texture{};

    template<class Archive>
    void serialize(Archive& Ar)
    {
        Ar(cereal::make_nvp("AssetId", AssetId),
           cereal::make_nvp("SourcePath", SourcePath),
           cereal::make_nvp("DestinationFolder", DestinationFolder),
           cereal::make_nvp("ImporterName", ImporterName),
           cereal::make_nvp("Profile", Profile),
           cereal::make_nvp("BuildOptions", BuildOptions),
           cereal::make_nvp("Assimp", Assimp),
           cereal::make_nvp("Texture", Texture));
    }
};

struct AssetImportMetadataDatabaseDisk
{
    uint32_t Version = kAssetImportMetadataVersion;
    std::vector<AssetImportMetadataEntryDisk> Entries{};

    template<class Archive>
    void serialize(Archive& Ar)
    {
        Ar(cereal::make_nvp("Version", Version),
           cereal::make_nvp("Entries", Entries));
    }
};

[[nodiscard]] std::string FormatLogMessage(const char* Prefix, const char* Fmt, va_list Args)
{
    if (!Fmt)
    {
        return Prefix ? std::string(Prefix) : std::string{};
    }

    va_list CopyArgs{};
    va_copy(CopyArgs, Args);
    const int Required = std::vsnprintf(nullptr, 0, Fmt, CopyArgs);
    va_end(CopyArgs);
    if (Required <= 0)
    {
        return Prefix ? std::string(Prefix) : std::string{};
    }

    std::string Buffer{};
    Buffer.resize(static_cast<std::size_t>(Required) + 1u);
    std::vsnprintf(Buffer.data(), Buffer.size(), Fmt, Args);
    Buffer.resize(static_cast<std::size_t>(Required));
    if (Prefix && Prefix[0] != '\0')
    {
        return std::string(Prefix) + Buffer;
    }
    return Buffer;
}

[[nodiscard]] Editor::ETextureCompressionTarget ToEditorTextureTarget(
    const TextureCompressorPlugin::ECompressionTarget Target)
{
    return Target == TextureCompressorPlugin::ECompressionTarget::ASTC
        ? Editor::ETextureCompressionTarget::ASTC
        : Editor::ETextureCompressionTarget::BCn;
}

[[nodiscard]] TextureCompressorPlugin::ECompressionTarget ToCookedTextureTarget(
    const Editor::ETextureCompressionTarget Target)
{
    return Target == Editor::ETextureCompressionTarget::ASTC
        ? TextureCompressorPlugin::ECompressionTarget::ASTC
        : TextureCompressorPlugin::ECompressionTarget::BCn;
}

[[nodiscard]] Editor::ETextureCompressionFormat ToEditorTextureFormat(
    const TextureCompressorPlugin::ECompressedFormat Format)
{
    using Editor::ETextureCompressionFormat;
    using TextureCompressorPlugin::ECompressedFormat;
    switch (Format)
    {
    case ECompressedFormat::BC1: return ETextureCompressionFormat::BC1;
    case ECompressedFormat::BC3: return ETextureCompressionFormat::BC3;
    case ECompressedFormat::BC4: return ETextureCompressionFormat::BC4;
    case ECompressedFormat::BC5: return ETextureCompressionFormat::BC5;
    case ECompressedFormat::BC6H: return ETextureCompressionFormat::BC6H;
    case ECompressedFormat::BC7: return ETextureCompressionFormat::BC7;
    case ECompressedFormat::ASTC_4x4: return ETextureCompressionFormat::ASTC_4x4;
    case ECompressedFormat::ASTC_5x5: return ETextureCompressionFormat::ASTC_5x5;
    case ECompressedFormat::ASTC_6x6: return ETextureCompressionFormat::ASTC_6x6;
    case ECompressedFormat::ASTC_8x8: return ETextureCompressionFormat::ASTC_8x8;
    case ECompressedFormat::ASTC_10x10: return ETextureCompressionFormat::ASTC_10x10;
    case ECompressedFormat::ASTC_12x12: return ETextureCompressionFormat::ASTC_12x12;
    case ECompressedFormat::ASTC_4x4_HDR: return ETextureCompressionFormat::ASTC_4x4_HDR;
    case ECompressedFormat::ASTC_6x6_HDR: return ETextureCompressionFormat::ASTC_6x6_HDR;
    case ECompressedFormat::ASTC_8x8_HDR: return ETextureCompressionFormat::ASTC_8x8_HDR;
    default: return ETextureCompressionFormat::Auto;
    }
}

[[nodiscard]] TextureCompressorPlugin::ECompressedFormat ToCookedTextureFormat(
    const Editor::ETextureCompressionFormat Format)
{
    using Editor::ETextureCompressionFormat;
    using TextureCompressorPlugin::ECompressedFormat;
    switch (Format)
    {
    case ETextureCompressionFormat::BC1: return ECompressedFormat::BC1;
    case ETextureCompressionFormat::BC3: return ECompressedFormat::BC3;
    case ETextureCompressionFormat::BC4: return ECompressedFormat::BC4;
    case ETextureCompressionFormat::BC5: return ECompressedFormat::BC5;
    case ETextureCompressionFormat::BC6H: return ECompressedFormat::BC6H;
    case ETextureCompressionFormat::BC7: return ECompressedFormat::BC7;
    case ETextureCompressionFormat::ASTC_4x4: return ECompressedFormat::ASTC_4x4;
    case ETextureCompressionFormat::ASTC_5x5: return ECompressedFormat::ASTC_5x5;
    case ETextureCompressionFormat::ASTC_6x6: return ECompressedFormat::ASTC_6x6;
    case ETextureCompressionFormat::ASTC_8x8: return ECompressedFormat::ASTC_8x8;
    case ETextureCompressionFormat::ASTC_10x10: return ECompressedFormat::ASTC_10x10;
    case ETextureCompressionFormat::ASTC_12x12: return ECompressedFormat::ASTC_12x12;
    case ETextureCompressionFormat::ASTC_4x4_HDR: return ECompressedFormat::ASTC_4x4_HDR;
    case ETextureCompressionFormat::ASTC_6x6_HDR: return ECompressedFormat::ASTC_6x6_HDR;
    case ETextureCompressionFormat::ASTC_8x8_HDR: return ECompressedFormat::ASTC_8x8_HDR;
    default: return ECompressedFormat::Unknown;
    }
}

[[nodiscard]] TextureCompressorPlugin::ECompressionTarget ResolveCookedCompressionTarget(
    const TextureCompressorPlugin::TextureCompressorCookedInfo& Cooked)
{
    if (Cooked.Format != TextureCompressorPlugin::ECompressedFormat::Unknown)
    {
        return TextureCompressorPlugin::IsASTCFormat(Cooked.Format)
            ? TextureCompressorPlugin::ECompressionTarget::ASTC
            : TextureCompressorPlugin::ECompressionTarget::BCn;
    }
    return Cooked.RequestedTarget;
}

[[nodiscard]] std::string CompressionTargetName(const TextureCompressorPlugin::ECompressionTarget Target)
{
    switch (Target)
    {
    case TextureCompressorPlugin::ECompressionTarget::ASTC:
        return "ASTC";
    case TextureCompressorPlugin::ECompressionTarget::BCn:
    default:
        return "BCn";
    }
}

[[nodiscard]] std::uint64_t ComputeTextureGpuSizeBytes(
    const TextureCompressorPlugin::TextureCompressorCookedInfo& Cooked)
{
    std::uint64_t TotalBytes = 0;
    for (const auto& Mip : Cooked.MipLevels)
    {
        TotalBytes += static_cast<std::uint64_t>(Mip.CompressedSize);
    }
    if (TotalBytes > 0)
    {
        return TotalBytes;
    }

    if (Cooked.BaseWidth == 0 ||
        Cooked.BaseHeight == 0 ||
        Cooked.MipCount == 0 ||
        Cooked.Format == TextureCompressorPlugin::ECompressedFormat::Unknown)
    {
        return 0;
    }

    std::uint32_t BlockWidth = 4;
    std::uint32_t BlockHeight = 4;
    TextureCompressorPlugin::GetBlockDimensions(Cooked.Format, BlockWidth, BlockHeight);
    const std::uint32_t BytesPerBlock = TextureCompressorPlugin::GetBytesPerBlock(Cooked.Format);
    if (BytesPerBlock == 0 || BlockWidth == 0 || BlockHeight == 0)
    {
        return 0;
    }

    std::uint32_t MipWidth = Cooked.BaseWidth;
    std::uint32_t MipHeight = Cooked.BaseHeight;
    const std::uint32_t MipCount = std::max(1u, Cooked.MipCount);
    for (std::uint32_t MipIndex = 0; MipIndex < MipCount; ++MipIndex)
    {
        const std::uint64_t BlocksX =
            (static_cast<std::uint64_t>(MipWidth) + static_cast<std::uint64_t>(BlockWidth) - 1u) /
            static_cast<std::uint64_t>(BlockWidth);
        const std::uint64_t BlocksY =
            (static_cast<std::uint64_t>(MipHeight) + static_cast<std::uint64_t>(BlockHeight) - 1u) /
            static_cast<std::uint64_t>(BlockHeight);
        TotalBytes += BlocksX * BlocksY * static_cast<std::uint64_t>(BytesPerBlock);

        if (MipWidth == 1u && MipHeight == 1u)
        {
            break;
        }
        MipWidth = std::max(1u, MipWidth / 2u);
        MipHeight = std::max(1u, MipHeight / 2u);
    }

    return TotalBytes;
}

[[nodiscard]] std::string OptionValueOr(
    const std::unordered_map<std::string, std::string>& Options,
    const std::string_view Key,
    std::string_view Default = {})
{
    if (const auto It = Options.find(std::string(Key)); It != Options.end())
    {
        return It->second;
    }
    return std::string(Default);
}

[[nodiscard]] bool ParseBoolOption(const std::string_view Text, const bool DefaultValue)
{
    const std::string Lower = ToLowerCopy(Text);
    if (Lower == "1" || Lower == "true" || Lower == "yes" || Lower == "on")
    {
        return true;
    }
    if (Lower == "0" || Lower == "false" || Lower == "no" || Lower == "off")
    {
        return false;
    }
    return DefaultValue;
}

[[nodiscard]] std::optional<int32_t> ParseIntOption(const std::string_view Text)
{
    std::string Buffer(Text);
    if (Buffer.empty())
    {
        return std::nullopt;
    }
    char* End = nullptr;
    const long Parsed = std::strtol(Buffer.c_str(), &End, 10);
    if (End == Buffer.c_str())
    {
        return std::nullopt;
    }
    while (End && *End != '\0')
    {
        if (!std::isspace(static_cast<unsigned char>(*End)))
        {
            return std::nullopt;
        }
        ++End;
    }
    if (Parsed < static_cast<long>(std::numeric_limits<int32_t>::min()) ||
        Parsed > static_cast<long>(std::numeric_limits<int32_t>::max()))
    {
        return std::nullopt;
    }
    return static_cast<int32_t>(Parsed);
}

[[nodiscard]] TextureCompressorPlugin::ECompressedFormat ParseTextureFormatOption(std::string_view Value)
{
    const std::string Lower = ToLowerCopy(Value);
    using TextureCompressorPlugin::ECompressedFormat;
    if (Lower == "bc1") return ECompressedFormat::BC1;
    if (Lower == "bc3") return ECompressedFormat::BC3;
    if (Lower == "bc4") return ECompressedFormat::BC4;
    if (Lower == "bc5") return ECompressedFormat::BC5;
    if (Lower == "bc6h") return ECompressedFormat::BC6H;
    if (Lower == "bc7") return ECompressedFormat::BC7;
    if (Lower == "astc_4x4") return ECompressedFormat::ASTC_4x4;
    if (Lower == "astc_5x5") return ECompressedFormat::ASTC_5x5;
    if (Lower == "astc_6x6") return ECompressedFormat::ASTC_6x6;
    if (Lower == "astc_8x8") return ECompressedFormat::ASTC_8x8;
    if (Lower == "astc_10x10") return ECompressedFormat::ASTC_10x10;
    if (Lower == "astc_12x12") return ECompressedFormat::ASTC_12x12;
    if (Lower == "astc_4x4_hdr") return ECompressedFormat::ASTC_4x4_HDR;
    if (Lower == "astc_6x6_hdr") return ECompressedFormat::ASTC_6x6_HDR;
    if (Lower == "astc_8x8_hdr") return ECompressedFormat::ASTC_8x8_HDR;
    return ECompressedFormat::Unknown;
}

[[nodiscard]] std::string TextureFormatToAuthoredString(TextureCompressorPlugin::ECompressedFormat Format);

[[nodiscard]] std::string ImportProfileToString(const EImportProfile Profile)
{
    switch (Profile)
    {
    case EImportProfile::AssimpModel: return "assimp_model";
    case EImportProfile::Texture: return "texture";
    case EImportProfile::Unknown:
    default: return "unknown";
    }
}

[[nodiscard]] EImportProfile ImportProfileFromString(std::string_view ProfileText)
{
    const std::string Lower = ToLowerCopy(ProfileText);
    if (Lower == "assimp_model")
    {
        return EImportProfile::AssimpModel;
    }
    if (Lower == "texture")
    {
        return EImportProfile::Texture;
    }
    return EImportProfile::Unknown;
}

[[nodiscard]] EImportProfile ImportProfileFromImporterName(std::string_view ImporterName)
{
    const std::string Lower = ToLowerCopy(ImporterName);
    if (Lower.find("renderassetassimpimporter") != std::string::npos)
    {
        return EImportProfile::AssimpModel;
    }
    if (Lower.find("texturecompressor") != std::string::npos)
    {
        return EImportProfile::Texture;
    }
    return EImportProfile::Unknown;
}

[[nodiscard]] std::unordered_map<std::string, std::string> BuildOptionsFromAssimpImportSettings(
    const Editor::AssimpImportSettings& Settings)
{
    const auto BoolToText = [](const bool Value) {
        return Value ? std::string("true") : std::string("false");
    };

    std::unordered_map<std::string, std::string> BuildOptions{};
    BuildOptions.emplace("SnAPI.GF.Assimp.GenerateNormals", BoolToText(Settings.GenerateNormals));
    BuildOptions.emplace("SnAPI.GF.Assimp.GenerateTangents", BoolToText(Settings.GenerateTangents));
    BuildOptions.emplace("SnAPI.GF.Assimp.FlipUVs", BoolToText(Settings.FlipUVs));
    BuildOptions.emplace("SnAPI.GF.Assimp.OptimizeMeshes", BoolToText(Settings.OptimizeMeshes));
    BuildOptions.emplace("SnAPI.GF.Assimp.ForceSkeletal", BoolToText(Settings.ForceSkeletal));
    BuildOptions.emplace("SnAPI.GF.Assimp.ForceStatic", BoolToText(Settings.ForceStatic));
    BuildOptions.emplace("SnAPI.GF.Assimp.ImportMaterials", BoolToText(Settings.ImportMaterials));
    BuildOptions.emplace("SnAPI.GF.Assimp.ImportTextures", BoolToText(Settings.ImportTextures));
    BuildOptions.emplace("SnAPI.GF.Assimp.ImportAnimations", BoolToText(Settings.ImportAnimations));
    BuildOptions.emplace("SnAPI.GF.Assimp.ImportSkeleton", BoolToText(Settings.ImportSkeleton));
    BuildOptions.emplace("SnAPI.GF.Assimp.MaxBonesPerVertex", std::to_string(std::max(1u, Settings.MaxBonesPerVertex)));
    return BuildOptions;
}

[[nodiscard]] std::unordered_map<std::string, std::string> BuildOptionsFromTextureImportSettings(
    const Editor::TextureImportSettings& Settings)
{
    const auto TargetToOption = [](const Editor::ETextureCompressionTarget Target) {
        return Target == Editor::ETextureCompressionTarget::ASTC ? std::string("astc") : std::string("bcn");
    };
    const auto FormatToOption = [](const Editor::ETextureCompressionFormat Format) -> std::string {
        switch (Format)
        {
        case Editor::ETextureCompressionFormat::Auto: return {};
        case Editor::ETextureCompressionFormat::BC1: return "bc1";
        case Editor::ETextureCompressionFormat::BC3: return "bc3";
        case Editor::ETextureCompressionFormat::BC4: return "bc4";
        case Editor::ETextureCompressionFormat::BC5: return "bc5";
        case Editor::ETextureCompressionFormat::BC6H: return "bc6h";
        case Editor::ETextureCompressionFormat::BC7: return "bc7";
        case Editor::ETextureCompressionFormat::ASTC_4x4: return "astc_4x4";
        case Editor::ETextureCompressionFormat::ASTC_5x5: return "astc_5x5";
        case Editor::ETextureCompressionFormat::ASTC_6x6: return "astc_6x6";
        case Editor::ETextureCompressionFormat::ASTC_8x8: return "astc_8x8";
        case Editor::ETextureCompressionFormat::ASTC_10x10: return "astc_10x10";
        case Editor::ETextureCompressionFormat::ASTC_12x12: return "astc_12x12";
        case Editor::ETextureCompressionFormat::ASTC_4x4_HDR: return "astc_4x4_hdr";
        case Editor::ETextureCompressionFormat::ASTC_6x6_HDR: return "astc_6x6_hdr";
        case Editor::ETextureCompressionFormat::ASTC_8x8_HDR: return "astc_8x8_hdr";
        default: return {};
        }
    };

    std::unordered_map<std::string, std::string> BuildOptions{};
    BuildOptions.emplace("texture.target", TargetToOption(Settings.Target));

    if (const std::string FormatOption = FormatToOption(Settings.Format); !FormatOption.empty())
    {
        BuildOptions.emplace("texture.format", FormatOption);
    }

    const float ClampedQuality = std::clamp(Settings.Quality, 0.0f, 1.0f);
    BuildOptions.emplace("texture.quality", std::to_string(ClampedQuality));

    if (Settings.ForceLinear)
    {
        BuildOptions.emplace("texture.srgb", "false");
    }
    else if (Settings.ForceSrgb)
    {
        BuildOptions.emplace("texture.srgb", "true");
    }

    if (Settings.ForceNormalMap)
    {
        BuildOptions.emplace("texture.normal_map", "true");
    }

    if (Settings.MaxMips > 0u)
    {
        BuildOptions.emplace("texture.max_mips", std::to_string(Settings.MaxMips));
    }

    return BuildOptions;
}

template<std::size_t N>
void RemoveManagedBuildOptions(std::unordered_map<std::string, std::string>& BuildOptions,
                               const std::array<std::string_view, N>& Keys)
{
    for (const std::string_view Key : Keys)
    {
        BuildOptions.erase(std::string(Key));
    }
}

constexpr std::array<std::string_view, 11> kAssimpManagedBuildOptionKeys{
    "SnAPI.GF.Assimp.GenerateNormals",
    "SnAPI.GF.Assimp.GenerateTangents",
    "SnAPI.GF.Assimp.FlipUVs",
    "SnAPI.GF.Assimp.OptimizeMeshes",
    "SnAPI.GF.Assimp.ForceSkeletal",
    "SnAPI.GF.Assimp.ForceStatic",
    "SnAPI.GF.Assimp.ImportMaterials",
    "SnAPI.GF.Assimp.ImportTextures",
    "SnAPI.GF.Assimp.ImportAnimations",
    "SnAPI.GF.Assimp.ImportSkeleton",
    "SnAPI.GF.Assimp.MaxBonesPerVertex"};

constexpr std::array<std::string_view, 6> kTextureManagedBuildOptionKeys{
    "texture.target",
    "texture.format",
    "texture.quality",
    "texture.srgb",
    "texture.normal_map",
    "texture.max_mips"};

void FillAssimpImportSettingsFromTyped(const AssimpImporterSettings& Typed, Editor::AssimpImportSettings& Out)
{
    Out.GenerateNormals = Typed.Mesh.GenerateNormals;
    Out.GenerateTangents = Typed.Mesh.GenerateTangents;
    Out.FlipUVs = Typed.Mesh.FlipUVs;
    Out.OptimizeMeshes = Typed.Mesh.OptimizeMeshes;
    Out.ForceSkeletal = Typed.Mesh.ForceSkeletal;
    Out.ForceStatic = Typed.Mesh.ForceStatic;
    Out.ImportMaterials = Typed.Mesh.ImportMaterials;
    Out.ImportTextures = Typed.Mesh.ImportTextures;
    Out.ImportAnimations = Typed.Mesh.ImportAnimations;
    Out.ImportSkeleton = Typed.Mesh.ImportSkeleton;
    Out.MaxBonesPerVertex = std::max(1u, Typed.Mesh.MaxBonesPerVertex);
}

void FillTextureImportSettingsFromTyped(const TextureCompressorPlugin::TextureCompressorImportSettings& Typed,
                                        Editor::TextureImportSettings& Out)
{
    Out.Target = ToEditorTextureTarget(Typed.Target);
    Out.Format = ToEditorTextureFormat(Typed.Format);
    Out.Quality = std::clamp(Typed.Quality, 0.0f, 1.0f);
    Out.ForceNormalMap = Typed.ForceNormalMap;
    Out.MaxMips = Typed.MaxMipCount > 0 ? static_cast<uint32_t>(Typed.MaxMipCount) : 0u;
    Out.ForceSrgb = Typed.ColorSpacePolicy == TextureCompressorPlugin::ETextureColorSpacePolicy::ForceSrgb;
    Out.ForceLinear = Typed.ColorSpacePolicy == TextureCompressorPlugin::ETextureColorSpacePolicy::ForceLinear;
}

void FillAssimpImportSettingsFromPayload(const MeshImportSettingsPayload& Payload, Editor::AssimpImportSettings& Out)
{
    Out.GenerateNormals = Payload.GenerateNormals;
    Out.GenerateTangents = Payload.GenerateTangents;
    Out.FlipUVs = Payload.FlipUVs;
    Out.OptimizeMeshes = Payload.OptimizeMeshes;
    Out.ForceSkeletal = Payload.ForceSkeletal;
    Out.ForceStatic = Payload.ForceStatic;
    Out.ImportMaterials = Payload.ImportMaterials;
    Out.ImportTextures = Payload.ImportTextures;
    Out.ImportAnimations = Payload.ImportAnimations;
    Out.ImportSkeleton = Payload.ImportSkeleton;
    Out.MaxBonesPerVertex = std::max(1u, Payload.MaxBonesPerVertex);
}

[[nodiscard]] MeshImportSettingsPayload BuildMeshImportSettingsPayload(const Editor::AssimpImportSettings& Settings)
{
    MeshImportSettingsPayload Payload{};
    Payload.GenerateNormals = Settings.GenerateNormals;
    Payload.GenerateTangents = Settings.GenerateTangents;
    Payload.FlipUVs = Settings.FlipUVs;
    Payload.OptimizeMeshes = Settings.OptimizeMeshes;
    Payload.ForceSkeletal = Settings.ForceSkeletal;
    Payload.ForceStatic = Settings.ForceStatic;
    Payload.ImportMaterials = Settings.ImportMaterials;
    Payload.ImportTextures = Settings.ImportTextures;
    Payload.ImportAnimations = Settings.ImportAnimations;
    Payload.ImportSkeleton = Settings.ImportSkeleton;
    Payload.MaxBonesPerVertex = std::max(1u, Settings.MaxBonesPerVertex);
    return Payload;
}

[[nodiscard]] Editor::ETextureCompressionTarget ParseAuthoredTextureTarget(std::string_view Value)
{
    const std::string Lower = ToLowerCopy(Value);
    return Lower == "astc"
        ? Editor::ETextureCompressionTarget::ASTC
        : Editor::ETextureCompressionTarget::BCn;
}

[[nodiscard]] Editor::ETextureCompressionFormat ParseAuthoredTextureFormat(std::string_view Value)
{
    const TextureCompressorPlugin::ECompressedFormat Format = ParseTextureFormatOption(Value);
    return ToEditorTextureFormat(Format);
}

void FillTextureImportSettingsFromPayload(const TextureImportSettingsPayload& Payload,
                                          Editor::TextureImportSettings& Out)
{
    Out.Target = ParseAuthoredTextureTarget(Payload.Target);
    Out.Format = ParseAuthoredTextureFormat(Payload.Format);
    Out.Quality = std::clamp(Payload.Quality, 0.0f, 1.0f);
    Out.ForceSrgb = Payload.ForceSrgb;
    Out.ForceLinear = Payload.ForceLinear;
    Out.ForceNormalMap = Payload.ForceNormalMap;
    Out.MaxMips = Payload.MaxMips;
}

[[nodiscard]] TextureImportSettingsPayload BuildTextureImportSettingsPayload(const Editor::TextureImportSettings& Settings)
{
    TextureImportSettingsPayload Payload{};
    Payload.Target = Settings.Target == Editor::ETextureCompressionTarget::ASTC ? "ASTC" : "BCn";
    Payload.Format = TextureFormatToAuthoredString(ToCookedTextureFormat(Settings.Format));
    Payload.Quality = std::clamp(Settings.Quality, 0.0f, 1.0f);
    Payload.ForceSrgb = Settings.ForceSrgb;
    Payload.ForceLinear = Settings.ForceLinear;
    Payload.ForceNormalMap = Settings.ForceNormalMap;
    Payload.MaxMips = Settings.MaxMips;
    return Payload;
}

[[nodiscard]] ::SnAPI::AssetPipeline::AssetImportSettingsPtr BuildTypedImportSettingsForImporter(
    const ::SnAPI::AssetPipeline::IAssetImporter& Importer,
    const std::unordered_map<std::string, std::string>& BuildOptions)
{
    const std::string ImporterName = Importer.GetName() ? Importer.GetName() : "";

    if (ImporterName == "SnAPI.GameFramework.RenderAssetAssimpImporter")
    {
        auto Settings = std::make_shared<AssimpImporterSettings>();
        Settings->Mesh.GenerateNormals = ParseBoolOption(
            OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.GenerateNormals", Settings->Mesh.GenerateNormals ? "true" : "false"),
            Settings->Mesh.GenerateNormals);
        Settings->Mesh.GenerateTangents = ParseBoolOption(
            OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.GenerateTangents", Settings->Mesh.GenerateTangents ? "true" : "false"),
            Settings->Mesh.GenerateTangents);
        Settings->Mesh.FlipUVs = ParseBoolOption(
            OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.FlipUVs", Settings->Mesh.FlipUVs ? "true" : "false"),
            Settings->Mesh.FlipUVs);
        Settings->Mesh.OptimizeMeshes = ParseBoolOption(
            OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.OptimizeMeshes", Settings->Mesh.OptimizeMeshes ? "true" : "false"),
            Settings->Mesh.OptimizeMeshes);
        Settings->Mesh.ForceSkeletal = ParseBoolOption(
            OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.ForceSkeletal", Settings->Mesh.ForceSkeletal ? "true" : "false"),
            Settings->Mesh.ForceSkeletal);
        Settings->Mesh.ForceStatic = ParseBoolOption(
            OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.ForceStatic", Settings->Mesh.ForceStatic ? "true" : "false"),
            Settings->Mesh.ForceStatic);
        Settings->Mesh.ImportMaterials = ParseBoolOption(
            OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.ImportMaterials", Settings->Mesh.ImportMaterials ? "true" : "false"),
            Settings->Mesh.ImportMaterials);
        Settings->Mesh.ImportTextures = ParseBoolOption(
            OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.ImportTextures", Settings->Mesh.ImportTextures ? "true" : "false"),
            Settings->Mesh.ImportTextures);
        Settings->Mesh.ImportAnimations = ParseBoolOption(
            OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.ImportAnimations", Settings->Mesh.ImportAnimations ? "true" : "false"),
            Settings->Mesh.ImportAnimations);
        Settings->Mesh.ImportSkeleton = ParseBoolOption(
            OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.ImportSkeleton", Settings->Mesh.ImportSkeleton ? "true" : "false"),
            Settings->Mesh.ImportSkeleton);
        if (const std::optional<int32_t> MaxBones = ParseIntOption(OptionValueOr(
                BuildOptions,
                "SnAPI.GF.Assimp.MaxBonesPerVertex",
                std::to_string(Settings->Mesh.MaxBonesPerVertex)));
            MaxBones.has_value())
        {
            Settings->Mesh.MaxBonesPerVertex = std::max(1, *MaxBones);
        }
        Settings->LogicalNameOverride = OptionValueOr(BuildOptions, "SnAPI.GF.Assimp.LogicalName", "");
        Settings->DefaultShaderModule = OptionValueOr(
            BuildOptions,
            "SnAPI.GF.Assimp.DefaultShaderModule",
            kDefaultMaterialShaderModule);
        Settings->DefaultShadingModel = OptionValueOr(
            BuildOptions,
            "SnAPI.GF.Assimp.DefaultShadingModel",
            kDefaultMaterialShadingModel);
        return Settings;
    }

    if (ImporterName == "TextureCompressor.Importer")
    {
        auto Settings = std::make_shared<TextureCompressorPlugin::TextureCompressorImportSettings>();
        const std::string Target = ToLowerCopy(OptionValueOr(BuildOptions, "texture.target", "bcn"));
        Settings->Target = (Target == "astc")
            ? TextureCompressorPlugin::ECompressionTarget::ASTC
            : TextureCompressorPlugin::ECompressionTarget::BCn;

        Settings->Format = ParseTextureFormatOption(OptionValueOr(BuildOptions, "texture.format", ""));

        const std::string QualityText = OptionValueOr(BuildOptions, "texture.quality", std::to_string(Settings->Quality));
        try
        {
            Settings->Quality = std::clamp(std::stof(QualityText), 0.0f, 1.0f);
        }
        catch (...)
        {
        }

        if (const auto SrgbText = OptionValueOr(BuildOptions, "texture.srgb", "");
            !SrgbText.empty())
        {
            const bool ForceSrgb = ParseBoolOption(SrgbText, true);
            Settings->ColorSpacePolicy = ForceSrgb
                ? TextureCompressorPlugin::ETextureColorSpacePolicy::ForceSrgb
                : TextureCompressorPlugin::ETextureColorSpacePolicy::ForceLinear;
        }

        Settings->ForceNormalMap = ParseBoolOption(
            OptionValueOr(BuildOptions, "texture.normal_map", "false"),
            false);

        if (const std::optional<int32_t> MaxMips = ParseIntOption(OptionValueOr(BuildOptions, "texture.max_mips", ""));
            MaxMips.has_value())
        {
            Settings->MaxMipCount = *MaxMips;
        }
        return Settings;
    }

    return {};
}

void PopulateTextureEditorPayloadFromCooked(
    const TextureCompressorPlugin::TextureCompressorCookedInfo& Cooked,
    Editor::TextureAssetEditorPayload& Out)
{
    Out.Target = ToEditorTextureTarget(Cooked.RequestedTarget);
    Out.Format = ToEditorTextureFormat(
        Cooked.RequestedFormat == TextureCompressorPlugin::ECompressedFormat::Unknown
            ? Cooked.Format
            : Cooked.RequestedFormat);
    Out.Quality = Cooked.RequestedQuality;
    Out.Width = Cooked.BaseWidth;
    Out.Height = Cooked.BaseHeight;
    Out.MipCount = Cooked.MipCount;
    Out.SRGB = Cooked.bSRGB;
}

void ApplyTextureEditorPayloadToCooked(
    const Editor::TextureAssetEditorPayload& EditorPayload,
    TextureCompressorPlugin::TextureCompressorCookedInfo& InOutCooked)
{
    InOutCooked.RequestedTarget = ToCookedTextureTarget(EditorPayload.Target);
    InOutCooked.RequestedFormat = ToCookedTextureFormat(EditorPayload.Format);
    InOutCooked.RequestedQuality = std::clamp(EditorPayload.Quality, 0.0f, 1.0f);
}

void PopulateStaticMeshEditorPayloadFromCooked(
    const StaticMeshPayload& Cooked,
    Editor::StaticMeshAssetEditorPayload& Out)
{
    Out.Name = Cooked.Name;
    Out.MaterialInstances = Cooked.MaterialInstances;
}

void ApplyStaticMeshEditorPayloadToCooked(
    const Editor::StaticMeshAssetEditorPayload& EditorPayload,
    StaticMeshPayload& InOutCooked)
{
    InOutCooked.Name = EditorPayload.Name;
    InOutCooked.MaterialInstances = EditorPayload.MaterialInstances;
}

#if defined(SNAPI_GF_ENABLE_RENDERER)
[[nodiscard]] bool EqualsIgnoreCase(std::string_view Left, std::string_view Right)
{
    if (Left.size() != Right.size())
    {
        return false;
    }
    for (size_t Index = 0; Index < Left.size(); ++Index)
    {
        if (static_cast<char>(std::tolower(static_cast<unsigned char>(Left[Index]))) !=
            static_cast<char>(std::tolower(static_cast<unsigned char>(Right[Index]))))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] SnAPI::Graphics::MaterialDomain DomainFromShadingModelName(std::string_view ShadingModel)
{
    if (ShadingModel == "GBufferShadingModel")
    {
        return SnAPI::Graphics::MaterialDomain::GBuffer;
    }
    if (ShadingModel == "ShadowShadingModel")
    {
        return SnAPI::Graphics::MaterialDomain::ShadowCaster;
    }
    if (ShadingModel == "UIShadingModel")
    {
        return SnAPI::Graphics::MaterialDomain::UI;
    }
    if (ShadingModel == "PostProcessShadingModel")
    {
        return SnAPI::Graphics::MaterialDomain::PostProcess;
    }
    if (ShadingModel == "DeferredShadingShadingModel")
    {
        return SnAPI::Graphics::MaterialDomain::DeferredLit;
    }
    return SnAPI::Graphics::MaterialDomain::GBuffer;
}

[[nodiscard]] bool TryParseFloatValue(std::string_view Text, float& OutValue)
{
    std::string Buffer(Text);
    char* End = nullptr;
    const float Parsed = std::strtof(Buffer.c_str(), &End);
    if (End == Buffer.c_str())
    {
        return false;
    }
    while (End && *End != '\0' && std::isspace(static_cast<unsigned char>(*End)))
    {
        ++End;
    }
    if (End && *End != '\0')
    {
        return false;
    }
    OutValue = Parsed;
    return true;
}

[[nodiscard]] std::string BuildAssetRefIdentity(const AssetRefPayload& Ref)
{
    const auto TrimText = [](std::string_view Text) {
        size_t Begin = 0;
        while (Begin < Text.size() && std::isspace(static_cast<unsigned char>(Text[Begin])))
        {
            ++Begin;
        }

        size_t End = Text.size();
        while (End > Begin && std::isspace(static_cast<unsigned char>(Text[End - 1])))
        {
            --End;
        }

        return std::string(Text.substr(Begin, End - Begin));
    };

    const std::string AssetId = TrimText(Ref.AssetId);
    if (!AssetId.empty())
    {
        return std::string("id://") + AssetId;
    }
    const std::string AssetName = TrimText(Ref.AssetName);
    if (!AssetName.empty())
    {
        return std::string("name://") + AssetName;
    }
    return {};
}

[[nodiscard]] const SnAPI::Graphics::ShaderMetaData::UserAttribute* FindDefaultAttribute(
    const SnAPI::Graphics::MaterialRuntimeParameterDesc& Parameter)
{
    for (const auto& Attribute : Parameter.Attributes)
    {
        if (EqualsIgnoreCase(Attribute.Name, "EditorDefault") || EqualsIgnoreCase(Attribute.Name, "Default"))
        {
            return &Attribute;
        }
    }
    return nullptr;
}

[[nodiscard]] bool TryReadAttributeNumber(
    const SnAPI::Graphics::ShaderMetaData::UserAttribute& Attribute,
    const size_t Index,
    float& OutValue)
{
    if (Index < Attribute.FloatArguments.size())
    {
        OutValue = Attribute.FloatArguments[Index];
        return true;
    }
    if (Index < Attribute.IntArguments.size())
    {
        OutValue = static_cast<float>(Attribute.IntArguments[Index]);
        return true;
    }
    if (Index < Attribute.Arguments.size())
    {
        return TryParseFloatValue(Attribute.Arguments[Index], OutValue);
    }
    return false;
}

[[nodiscard]] bool TryResolveDefaultScalar(
    const SnAPI::Graphics::MaterialRuntimeParameterDesc& Parameter,
    float& OutValue)
{
    const auto* Attribute = FindDefaultAttribute(Parameter);
    if (!Attribute)
    {
        return false;
    }
    return TryReadAttributeNumber(*Attribute, 0, OutValue);
}

[[nodiscard]] bool TryResolveDefaultVector(
    const SnAPI::Graphics::MaterialRuntimeParameterDesc& Parameter,
    const size_t ComponentCount,
    std::array<float, 4>& OutValue)
{
    if (ComponentCount == 0 || ComponentCount > OutValue.size())
    {
        return false;
    }
    const auto* Attribute = FindDefaultAttribute(Parameter);
    if (!Attribute)
    {
        return false;
    }

    float FirstValue = 0.0f;
    if (!TryReadAttributeNumber(*Attribute, 0, FirstValue))
    {
        return false;
    }

    OutValue = {0.0f, 0.0f, 0.0f, 0.0f};
    for (size_t Component = 0; Component < ComponentCount; ++Component)
    {
        float Value = FirstValue;
        (void)TryReadAttributeNumber(*Attribute, Component, Value);
        OutValue[Component] = Value;
    }
    return true;
}

void CollectParameterLookupKeys(
    const SnAPI::Graphics::MaterialRuntimeParameterDesc& Parameter,
    std::vector<std::string>& OutKeys)
{
    OutKeys.clear();
    OutKeys.reserve(6);

    const auto Append = [&OutKeys](std::string_view Key) {
        if (Key.empty())
        {
            return;
        }
        const std::string Lower = ToLowerCopy(Key);
        if (std::ranges::find(OutKeys, Lower) == OutKeys.end())
        {
            OutKeys.push_back(Lower);
        }
    };

    Append(Parameter.Path);
    Append(Parameter.Name);
    if (const size_t DotIndex = Parameter.Path.rfind('.'); DotIndex != std::string::npos)
    {
        Append(std::string_view(Parameter.Path).substr(DotIndex + 1));
    }
}

[[nodiscard]] std::expected<SnAPI::Graphics::MaterialRuntimeDescriptor, std::string> BuildDescriptorForMaterialAsset(
    const MaterialAsset& ParentMaterial)
{
    if (ParentMaterial.ShaderModule.empty())
    {
        return std::unexpected("Parent material has no shader module");
    }

    const std::string ShadingModelName = ParentMaterial.ShadingModel.empty()
        ? std::string(kDefaultMaterialShadingModel)
        : ParentMaterial.ShadingModel;

    std::shared_ptr<SnAPI::Graphics::Material> Material{};
    if (EqualsIgnoreCase(ShadingModelName, SnAPI::Graphics::GBufferContract::ShadingModelModuleName))
    {
        auto TypedMaterial = std::make_shared<SnAPI::Graphics::GBufferMaterial>(ParentMaterial.ShaderModule);
        TypedMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlbedoMap, ParentMaterial.FeatureAlbedoMap);
        TypedMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::NormalMap, ParentMaterial.FeatureNormalMap);
        TypedMaterial->SetFeature(
            SnAPI::Graphics::GBufferContract::Feature::RoughnessMap,
            ParentMaterial.FeatureRoughnessMap);
        TypedMaterial->SetFeature(
            SnAPI::Graphics::GBufferContract::Feature::MetalnessMap,
            ParentMaterial.FeatureMetalnessMap);
        TypedMaterial->SetFeature(
            SnAPI::Graphics::GBufferContract::Feature::OcclusionMap,
            ParentMaterial.FeatureOcclusionMap);
        TypedMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlphaTest, ParentMaterial.FeatureAlphaTest);
        TypedMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlphaBlend, ParentMaterial.FeatureAlphaBlend);
        TypedMaterial->SetFeature(
            SnAPI::Graphics::GBufferContract::Feature::DoubleSided,
            ParentMaterial.FeatureDoubleSided);
        TypedMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::Instancing, ParentMaterial.FeatureInstancing);
        TypedMaterial->BakeCompileTimeParams();
        Material = std::move(TypedMaterial);
    }
    else if (EqualsIgnoreCase(ShadingModelName, SnAPI::Graphics::ShadowContract::ShadingModelModuleName))
    {
        auto TypedMaterial = std::make_shared<SnAPI::Graphics::ShadowMaterial>(ParentMaterial.ShaderModule);
        TypedMaterial->SetFeature(SnAPI::Graphics::ShadowContract::Feature::AlphaTest, ParentMaterial.FeatureAlphaTest);
        TypedMaterial->SetFeature(SnAPI::Graphics::ShadowContract::Feature::Instancing, ParentMaterial.FeatureInstancing);
        TypedMaterial->BakeCompileTimeParams();
        Material = std::move(TypedMaterial);
    }
    else
    {
        auto GenericMaterial = std::make_shared<SnAPI::Graphics::Material>(
            ParentMaterial.ShaderModule,
            ShadingModelName,
            DomainFromShadingModelName(ShadingModelName));
        GenericMaterial->BakeAndCompile();
        Material = std::move(GenericMaterial);
    }

    auto Instance = Material->CreateMaterialInstance();
    if (!Instance)
    {
        return std::unexpected("Failed to instantiate runtime material for descriptor reflection");
    }

    return SnAPI::Graphics::BuildMaterialRuntimeDescriptor(*Instance);
}

[[nodiscard]] bool SyncMaterialInstanceAssetToRuntimeDescriptor(
    MaterialInstanceAsset& Payload,
    const SnAPI::Graphics::MaterialRuntimeDescriptor& Descriptor)
{
    std::vector<MaterialScalarParamPayload> SyncedScalars{};
    std::vector<MaterialVectorParamPayload> SyncedVectors{};
    std::vector<MaterialTextureParamPayload> SyncedTextures{};

    std::vector<bool> UsedScalars(Payload.Scalars.size(), false);
    std::vector<bool> UsedVectors(Payload.Vectors.size(), false);
    std::vector<bool> UsedTextures(Payload.Textures.size(), false);

    std::unordered_map<std::string, size_t> ScalarByName{};
    std::unordered_map<std::string, size_t> VectorByName{};
    std::unordered_map<std::string, size_t> TextureByName{};
    ScalarByName.reserve(Payload.Scalars.size());
    VectorByName.reserve(Payload.Vectors.size());
    TextureByName.reserve(Payload.Textures.size());

    for (size_t Index = 0; Index < Payload.Scalars.size(); ++Index)
    {
        ScalarByName.try_emplace(ToLowerCopy(Payload.Scalars[Index].Name), Index);
    }
    for (size_t Index = 0; Index < Payload.Vectors.size(); ++Index)
    {
        VectorByName.try_emplace(ToLowerCopy(Payload.Vectors[Index].Name), Index);
    }
    for (size_t Index = 0; Index < Payload.Textures.size(); ++Index)
    {
        TextureByName.try_emplace(ToLowerCopy(Payload.Textures[Index].SlotName), Index);
    }

    std::vector<std::string> LookupKeys{};
    for (const auto& Parameter : Descriptor.Parameters)
    {
        if (Parameter.Set != 0)
        {
            continue;
        }

        CollectParameterLookupKeys(Parameter, LookupKeys);
        const auto FindIndex = [&LookupKeys](const std::unordered_map<std::string, size_t>& ByName) -> std::optional<size_t> {
            for (const std::string& Key : LookupKeys)
            {
                if (const auto It = ByName.find(Key); It != ByName.end())
                {
                    return It->second;
                }
            }
            return std::nullopt;
        };

        switch (Parameter.eValueType)
        {
        case SnAPI::Graphics::ShaderMetaData::EValueType::Bool:
        case SnAPI::Graphics::ShaderMetaData::EValueType::Int:
        case SnAPI::Graphics::ShaderMetaData::EValueType::UInt:
        case SnAPI::Graphics::ShaderMetaData::EValueType::Float:
            {
                MaterialScalarParamPayload Scalar{};
                if (const auto Existing = FindIndex(ScalarByName); Existing && *Existing < Payload.Scalars.size())
                {
                    Scalar = Payload.Scalars[*Existing];
                    UsedScalars[*Existing] = true;
                }
                else
                {
                    (void)TryResolveDefaultScalar(Parameter, Scalar.Value);
                }
                Scalar.Name = Parameter.Name;
                SyncedScalars.push_back(std::move(Scalar));
            }
            break;
        case SnAPI::Graphics::ShaderMetaData::EValueType::Float2:
        case SnAPI::Graphics::ShaderMetaData::EValueType::Float3:
        case SnAPI::Graphics::ShaderMetaData::EValueType::Float4:
            {
                MaterialVectorParamPayload Vector{};
                if (const auto Existing = FindIndex(VectorByName); Existing && *Existing < Payload.Vectors.size())
                {
                    Vector = Payload.Vectors[*Existing];
                    UsedVectors[*Existing] = true;
                }
                else
                {
                    size_t Components = 4;
                    if (Parameter.eValueType == SnAPI::Graphics::ShaderMetaData::EValueType::Float2)
                    {
                        Components = 2;
                    }
                    else if (Parameter.eValueType == SnAPI::Graphics::ShaderMetaData::EValueType::Float3)
                    {
                        Components = 3;
                    }
                    (void)TryResolveDefaultVector(Parameter, Components, Vector.Value);
                }
                Vector.Name = Parameter.Name;
                SyncedVectors.push_back(std::move(Vector));
            }
            break;
        default:
            break;
        }
    }

    for (const auto& Resource : Descriptor.Resources)
    {
        if (Resource.Set != 0 ||
            Resource.eBindingType != SnAPI::Graphics::ShaderMetaData::EBindingType::SampledImage)
        {
            continue;
        }

        MaterialTextureParamPayload Texture{};
        if (const auto Existing = TextureByName.find(ToLowerCopy(Resource.Name));
            Existing != TextureByName.end() && Existing->second < Payload.Textures.size())
        {
            Texture = Payload.Textures[Existing->second];
            UsedTextures[Existing->second] = true;
        }
        Texture.SlotName = Resource.Name;
        SyncedTextures.push_back(std::move(Texture));
    }

    for (size_t Index = 0; Index < Payload.Scalars.size(); ++Index)
    {
        if (!UsedScalars[Index])
        {
            SyncedScalars.push_back(Payload.Scalars[Index]);
        }
    }
    for (size_t Index = 0; Index < Payload.Vectors.size(); ++Index)
    {
        if (!UsedVectors[Index])
        {
            SyncedVectors.push_back(Payload.Vectors[Index]);
        }
    }
    for (size_t Index = 0; Index < Payload.Textures.size(); ++Index)
    {
        if (!UsedTextures[Index])
        {
            SyncedTextures.push_back(Payload.Textures[Index]);
        }
    }

    const bool Changed =
        Payload.Scalars != SyncedScalars ||
        Payload.Vectors != SyncedVectors ||
        Payload.Textures != SyncedTextures;

    if (Changed)
    {
        Payload.Scalars = std::move(SyncedScalars);
        Payload.Vectors = std::move(SyncedVectors);
        Payload.Textures = std::move(SyncedTextures);
    }
    return Changed;
}
#endif

class InlineImportPipelineContext final : public ::SnAPI::AssetPipeline::IPipelineContext
{
public:
    InlineImportPipelineContext(const ::SnAPI::AssetPipeline::PayloadRegistry& Registry,
                                const std::unordered_map<std::string, std::string>& Options,
                                std::vector<std::string>& Infos,
                                std::vector<std::string>& Warnings,
                                std::vector<std::string>& Errors)
        : m_registry(Registry)
        , m_options(Options)
        , m_infos(Infos)
        , m_warnings(Warnings)
        , m_errors(Errors)
    {
    }

    void LogInfo(const char* Fmt, ...) override
    {
        va_list Args{};
        va_start(Args, Fmt);
        std::lock_guard Lock(m_logMutex);
        m_infos.push_back(FormatLogMessage("[INFO] ", Fmt, Args));
        va_end(Args);
    }

    void LogWarn(const char* Fmt, ...) override
    {
        va_list Args{};
        va_start(Args, Fmt);
        std::lock_guard Lock(m_logMutex);
        m_warnings.push_back(FormatLogMessage("[WARN] ", Fmt, Args));
        va_end(Args);
    }

    void LogError(const char* Fmt, ...) override
    {
        va_list Args{};
        va_start(Args, Fmt);
        std::lock_guard Lock(m_logMutex);
        m_errors.push_back(FormatLogMessage("[ERROR] ", Fmt, Args));
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
        if (Size <= 0)
        {
            Out.clear();
            return true;
        }

        Out.resize(static_cast<std::size_t>(Size));
        File.seekg(0, std::ios::beg);
        File.read(reinterpret_cast<char*>(Out.data()), Size);
        return File.good();
    }

    uint64_t HashBytes64(const void* Data, const std::size_t Size) override
    {
        return XXH3_64bits(Data, Size);
    }

    void HashBytes128(const void* Data, const std::size_t Size, uint64_t& OutHi, uint64_t& OutLo) override
    {
        const XXH128_hash_t Hash = XXH3_128bits(Data, Size);
        OutHi = Hash.high64;
        OutLo = Hash.low64;
    }

    ::SnAPI::AssetPipeline::AssetId MakeDeterministicAssetId(std::string_view LogicalName, std::string_view VariantKey) override
    {
        return SourceAssetIdFromLogicalName(LogicalName, VariantKey);
    }

    const ::SnAPI::AssetPipeline::IPayloadSerializer* FindSerializer(const ::SnAPI::AssetPipeline::TypeId Id) const override
    {
        return m_registry.Find(Id);
    }

    std::string GetOption(std::string_view Key, std::string_view Default = {}) const override
    {
        if (const auto It = m_options.find(std::string(Key)); It != m_options.end())
        {
            return It->second;
        }
        return std::string(Default);
    }

private:
    const ::SnAPI::AssetPipeline::PayloadRegistry& m_registry;
    const std::unordered_map<std::string, std::string>& m_options;
    std::vector<std::string>& m_infos;
    std::vector<std::string>& m_warnings;
    std::vector<std::string>& m_errors;
    std::mutex m_logMutex{};
};

[[nodiscard]] std::string TrimCopy(std::string Value)
{
    while (!Value.empty() && std::isspace(static_cast<unsigned char>(Value.front())) != 0)
    {
        Value.erase(Value.begin());
    }
    while (!Value.empty() && std::isspace(static_cast<unsigned char>(Value.back())) != 0)
    {
        Value.pop_back();
    }
    return Value;
}

[[nodiscard]] bool HasUriScheme(const std::string_view Value)
{
    const std::size_t Delimiter = Value.find("://");
    if (Delimiter == std::string_view::npos || Delimiter == 0)
    {
        return false;
    }

    const unsigned char First = static_cast<unsigned char>(Value.front());
    if (std::isalpha(First) == 0)
    {
        return false;
    }

    for (std::size_t Index = 1; Index < Delimiter; ++Index)
    {
        const unsigned char Character = static_cast<unsigned char>(Value[Index]);
        if (std::isalnum(Character) != 0 || Character == '+' || Character == '-' || Character == '.')
        {
            continue;
        }
        return false;
    }

    return true;
}

[[nodiscard]] std::filesystem::path ResolveAppDataRootPath()
{
#if defined(_WIN32)
    if (const char* LocalAppData = std::getenv("LOCALAPPDATA"))
    {
        return std::filesystem::path(LocalAppData);
    }
    if (const char* RoamingAppData = std::getenv("APPDATA"))
    {
        return std::filesystem::path(RoamingAppData);
    }
#elif defined(__APPLE__)
    if (const char* Home = std::getenv("HOME"))
    {
        return std::filesystem::path(Home) / "Library" / "Application Support";
    }
#else
    if (const char* XdgDataHome = std::getenv("XDG_DATA_HOME"))
    {
        return std::filesystem::path(XdgDataHome);
    }
    if (const char* Home = std::getenv("HOME"))
    {
        return std::filesystem::path(Home) / ".local" / "share";
    }
#endif
    return {};
}

[[nodiscard]] std::filesystem::path EditorTemplateAssetDirectory()
{
    const std::filesystem::path AppDataRoot = ResolveAppDataRootPath();
    if (AppDataRoot.empty())
    {
        return {};
    }
    return AppDataRoot / "SnAPI" / "GameFramework" / "Editor" / "Assets";
}

[[nodiscard]] std::filesystem::path ResolveEditorAssetSourceDirectory()
{
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> Candidates{
        fs::path(__FILE__).parent_path() / "Assets",
        fs::current_path() / "src" / "Editor" / "Assets",
        fs::current_path() / "Editor" / "Assets",
    };

    std::error_code Error{};
    for (const fs::path& Candidate : Candidates)
    {
        Error.clear();
        if (!fs::exists(Candidate, Error) || Error)
        {
            continue;
        }

        Error.clear();
        if (fs::is_directory(Candidate, Error) && !Error)
        {
            return Candidate;
        }
    }

    return {};
}

[[nodiscard]] std::filesystem::path ResolveEditorScriptTemplateSource()
{
    namespace fs = std::filesystem;
    const fs::path SourceDirectory = ResolveEditorAssetSourceDirectory();
    if (SourceDirectory.empty())
    {
        return {};
    }

    const fs::path Candidate = SourceDirectory / std::string(kEditorStarterScriptFileName);
    std::error_code Error{};
    if (fs::exists(Candidate, Error) && !Error)
    {
        return Candidate;
    }
    return {};
}

[[nodiscard]] std::expected<void, std::string> CopyDirectoryContentsRecursive(const std::filesystem::path& SourceDirectory,
                                                                               const std::filesystem::path& DestinationDirectory)
{
    namespace fs = std::filesystem;
    std::error_code Error{};

    if (SourceDirectory.empty() || DestinationDirectory.empty())
    {
        return {};
    }

    if (!fs::exists(SourceDirectory, Error) || Error)
    {
        return {};
    }

    Error.clear();
    if (!fs::is_directory(SourceDirectory, Error) || Error)
    {
        return {};
    }

    Error.clear();
    fs::create_directories(DestinationDirectory, Error);
    if (Error)
    {
        return std::unexpected("Failed to create destination directory '" + DestinationDirectory.string() + "': " + Error.message());
    }

    for (fs::recursive_directory_iterator It(SourceDirectory, fs::directory_options::skip_permission_denied, Error), End;
         !Error && It != End;
         It.increment(Error))
    {
        const fs::directory_entry& Entry = *It;

        std::error_code RelativeError{};
        const fs::path RelativePath = fs::relative(Entry.path(), SourceDirectory, RelativeError);
        if (RelativeError)
        {
            continue;
        }
        if (RelativePath.empty() || RelativePath == ".")
        {
            continue;
        }

        const fs::path DestinationPath = DestinationDirectory / RelativePath;
        std::error_code EntryError{};
        if (Entry.is_directory(EntryError) && !EntryError)
        {
            fs::create_directories(DestinationPath, Error);
            if (Error)
            {
                return std::unexpected("Failed to create directory '" + DestinationPath.string() + "': " + Error.message());
            }
            continue;
        }

        EntryError.clear();
        if (!Entry.is_regular_file(EntryError) || EntryError)
        {
            continue;
        }

        Error.clear();
        fs::create_directories(DestinationPath.parent_path(), Error);
        if (Error)
        {
            return std::unexpected("Failed to create parent directory '" + DestinationPath.parent_path().string() + "': " + Error.message());
        }

        Error.clear();
        fs::copy_file(Entry.path(), DestinationPath, fs::copy_options::overwrite_existing, Error);
        if (Error)
        {
            return std::unexpected("Failed to copy '" + Entry.path().string() + "' to '" + DestinationPath.string() + "': " + Error.message());
        }
    }

    if (Error)
    {
        return std::unexpected("Failed to enumerate source directory '" + SourceDirectory.string() + "': " + Error.message());
    }

    return {};
}

[[nodiscard]] std::filesystem::path ResolveRendererShaderSourceDirectory()
{
    namespace fs = std::filesystem;
    std::vector<fs::path> Candidates{};
#if defined(SNAPI_GF_RENDERER_SHADER_SOURCE_DIR)
    Candidates.emplace_back(fs::path(SNAPI_GF_RENDERER_SHADER_SOURCE_DIR));
#endif
    Candidates.emplace_back(fs::path(__FILE__).parent_path() / ".." / ".." / ".." / "SnAPI.Renderer" / "shaders");
    Candidates.emplace_back(fs::current_path() / ".." / "SnAPI.Renderer" / "shaders");
    Candidates.emplace_back(fs::current_path() / "shaders");

    std::error_code Error{};
    for (const fs::path& Candidate : Candidates)
    {
        Error.clear();
        if (!fs::exists(Candidate, Error) || Error)
        {
            continue;
        }

        Error.clear();
        if (!fs::is_directory(Candidate, Error) || Error)
        {
            continue;
        }

        return Candidate;
    }

    return {};
}

#if defined(SNAPI_GF_ENABLE_RENDERER)
void ConfigureRendererShaderSearchRootForAssetRoot(GameRuntime& Runtime, const std::filesystem::path& AssetRoot)
{
    if (auto* WorldPtr = Runtime.WorldPtr(); WorldPtr)
    {
        (void)WorldPtr->Renderer().SetProjectShaderSearchRoot(AssetRoot);
    }
}
#endif

[[nodiscard]] std::filesystem::path EditorDefaultShapeAssetDirectory()
{
    const std::filesystem::path TemplateDirectory = EditorTemplateAssetDirectory();
    if (!TemplateDirectory.empty())
    {
        return TemplateDirectory;
    }
    std::error_code Error{};
    const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
    if (!Error && !CurrentPath.empty())
    {
        return CurrentPath / "Editor" / "Assets";
    }

    return {};
}

[[nodiscard]] std::string JsonEscape(std::string_view Value)
{
    std::string Escaped{};
    Escaped.reserve(Value.size() + 8u);
    for (const char Character : Value)
    {
        switch (Character)
        {
        case '\\':
            Escaped += "\\\\";
            break;
        case '"':
            Escaped += "\\\"";
            break;
        case '\n':
            Escaped += "\\n";
            break;
        case '\r':
            Escaped += "\\r";
            break;
        case '\t':
            Escaped += "\\t";
            break;
        default:
            Escaped.push_back(Character);
            break;
        }
    }
    return Escaped;
}

[[nodiscard]] std::expected<std::string, std::string> JsonParseString(const std::string& Text, std::size_t& Position)
{
    if (Position >= Text.size() || Text[Position] != '"')
    {
        return std::unexpected("Expected JSON string");
    }
    ++Position;

    std::string Output{};
    while (Position < Text.size())
    {
        const char Character = Text[Position++];
        if (Character == '"')
        {
            return Output;
        }
        if (Character != '\\')
        {
            Output.push_back(Character);
            continue;
        }
        if (Position >= Text.size())
        {
            return std::unexpected("Invalid JSON escape sequence");
        }
        const char Escape = Text[Position++];
        switch (Escape)
        {
        case '"':
            Output.push_back('"');
            break;
        case '\\':
            Output.push_back('\\');
            break;
        case '/':
            Output.push_back('/');
            break;
        case 'b':
            Output.push_back('\b');
            break;
        case 'f':
            Output.push_back('\f');
            break;
        case 'n':
            Output.push_back('\n');
            break;
        case 'r':
            Output.push_back('\r');
            break;
        case 't':
            Output.push_back('\t');
            break;
        default:
            return std::unexpected("Unsupported JSON escape sequence");
        }
    }
    return std::unexpected("Unterminated JSON string");
}

[[nodiscard]] bool JsonTryReadStringField(const std::string& Text, std::string_view Key, std::string& OutValue)
{
    const std::string KeyToken = "\"" + std::string(Key) + "\"";
    std::size_t SearchOffset = 0;
    while (true)
    {
        const std::size_t KeyPos = Text.find(KeyToken, SearchOffset);
        if (KeyPos == std::string::npos)
        {
            return false;
        }
        std::size_t ValuePos = KeyPos + KeyToken.size();
        while (ValuePos < Text.size() && std::isspace(static_cast<unsigned char>(Text[ValuePos])) != 0)
        {
            ++ValuePos;
        }
        if (ValuePos >= Text.size() || Text[ValuePos] != ':')
        {
            SearchOffset = KeyPos + KeyToken.size();
            continue;
        }
        ++ValuePos;
        while (ValuePos < Text.size() && std::isspace(static_cast<unsigned char>(Text[ValuePos])) != 0)
        {
            ++ValuePos;
        }
        auto Parsed = JsonParseString(Text, ValuePos);
        if (!Parsed)
        {
            SearchOffset = KeyPos + KeyToken.size();
            continue;
        }
        OutValue = std::move(*Parsed);
        return true;
    }
}

[[nodiscard]] bool JsonTryReadUnsignedField(const std::string& Text, std::string_view Key, uint32_t& OutValue)
{
    const std::string KeyToken = "\"" + std::string(Key) + "\"";
    const std::size_t KeyPos = Text.find(KeyToken);
    if (KeyPos == std::string::npos)
    {
        return false;
    }

    std::size_t ValuePos = Text.find(':', KeyPos + KeyToken.size());
    if (ValuePos == std::string::npos)
    {
        return false;
    }
    ++ValuePos;
    while (ValuePos < Text.size() && std::isspace(static_cast<unsigned char>(Text[ValuePos])) != 0)
    {
        ++ValuePos;
    }
    std::size_t EndPos = ValuePos;
    while (EndPos < Text.size() && std::isdigit(static_cast<unsigned char>(Text[EndPos])) != 0)
    {
        ++EndPos;
    }
    if (EndPos <= ValuePos)
    {
        return false;
    }

    try
    {
        OutValue = static_cast<uint32_t>(std::stoul(Text.substr(ValuePos, EndPos - ValuePos)));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

[[nodiscard]] std::string NormalizeProjectPathField(const std::string_view RawValue)
{
    std::string Value = TrimCopy(std::string(RawValue));
    if (Value.empty())
    {
        return {};
    }

    std::replace(Value.begin(), Value.end(), '\\', '/');
    return std::filesystem::path(Value).lexically_normal().generic_string();
}

[[nodiscard]] std::string ToProjectRelativePathField(const std::string_view RawValue, const std::filesystem::path& BaseRoot)
{
    std::string Value = TrimCopy(std::string(RawValue));
    if (Value.empty())
    {
        return {};
    }
    if (HasUriScheme(Value))
    {
        return Value;
    }

    std::filesystem::path ValuePath = std::filesystem::path(Value).lexically_normal();
    if (ValuePath.is_absolute() && !BaseRoot.empty())
    {
        std::error_code RelativeError{};
        std::filesystem::path RelativePath = std::filesystem::relative(ValuePath, BaseRoot, RelativeError);
        if (!RelativeError && !RelativePath.empty())
        {
            const std::string RelativeText = RelativePath.generic_string();
            if (!RelativeText.starts_with("../") && RelativeText != "..")
            {
                return std::filesystem::path(RelativeText).lexically_normal().generic_string();
            }
        }
    }

    return ValuePath.generic_string();
}

[[nodiscard]] std::expected<void, std::string> WriteProjectConfigFile(const std::filesystem::path& ProjectFilePath,
                                                                      const std::string_view Name,
                                                                      const std::string_view AssetRoot,
                                                                      const std::string_view StartupLevelAsset,
                                                                      const std::string_view DefaultRenderSettingsAssetId)
{
    std::ofstream ProjectFile(ProjectFilePath, std::ios::binary | std::ios::trunc);
    if (!ProjectFile.is_open())
    {
        return std::unexpected("Failed to open project file for writing");
    }

    ProjectFile << "{\n";
    ProjectFile << "  \"version\": " << kProjectConfigVersion << ",\n";
    ProjectFile << "  \"name\": \"" << JsonEscape(Name) << "\",\n";
    ProjectFile << "  \"assetRoot\": \"" << JsonEscape(AssetRoot) << "\",\n";
    ProjectFile << "  \"startupLevelAsset\": \"" << JsonEscape(StartupLevelAsset) << "\",\n";
    ProjectFile << "  \"defaultRenderSettings\": \"" << JsonEscape(DefaultRenderSettingsAssetId) << "\"\n";
    ProjectFile << "}\n";

    if (!ProjectFile.good())
    {
        return std::unexpected("Failed to write project file");
    }
    ProjectFile.flush();
    if (!ProjectFile.good())
    {
        return std::unexpected("Failed to flush project file");
    }
    ProjectFile.close();
    if (!ProjectFile.good())
    {
        return std::unexpected("Failed to close project file");
    }
    return {};
}

struct DefaultShapePackSpec
{
    const char* PackFileName = "";
    const char* AssetName = "";
    const char* PrimitiveMeshPathToken = "primitive://box";
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    SnAPI::Physics::EShapeType ColliderShape = SnAPI::Physics::EShapeType::Box;
    Vec3 ColliderHalfExtent{0.5f, 0.5f, 0.5f};
    float ColliderRadius = 0.5f;
    float ColliderHalfHeight = 0.5f;
    float ColliderFriction = 0.8f;
    float ColliderRestitution = 0.05f;
#endif
};

[[nodiscard]] std::array<DefaultShapePackSpec, 4> DefaultShapePackSpecs()
{
    std::array<DefaultShapePackSpec, 4> Specs{};

    Specs[0].PackFileName = "BoxShape.snpak";
    Specs[0].AssetName = "BoxShape";
    Specs[0].PrimitiveMeshPathToken = "primitive://box";
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Specs[0].ColliderShape = SnAPI::Physics::EShapeType::Box;
    Specs[0].ColliderHalfExtent = Vec3(0.5f, 0.5f, 0.5f);
#endif

    Specs[1].PackFileName = "SphereShape.snpak";
    Specs[1].AssetName = "SphereShape";
    Specs[1].PrimitiveMeshPathToken = "primitive://sphere";
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Specs[1].ColliderShape = SnAPI::Physics::EShapeType::Sphere;
    Specs[1].ColliderRadius = 0.5f;
#endif

    Specs[2].PackFileName = "ConeShape.snpak";
    Specs[2].AssetName = "ConeShape";
    Specs[2].PrimitiveMeshPathToken = "primitive://cone";
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Specs[2].ColliderShape = SnAPI::Physics::EShapeType::Capsule;
    Specs[2].ColliderRadius = 0.4f;
    Specs[2].ColliderHalfHeight = 0.5f;
#endif

    Specs[3].PackFileName = "PyramidShape.snpak";
    Specs[3].AssetName = "PyramidShape";
    Specs[3].PrimitiveMeshPathToken = "primitive://pyramid";
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Specs[3].ColliderShape = SnAPI::Physics::EShapeType::Box;
    Specs[3].ColliderHalfExtent = Vec3(0.5f, 0.5f, 0.5f);
#endif

    return Specs;
}

[[nodiscard]] std::expected<::SnAPI::AssetPipeline::AssetPackEntry, std::string> BuildDefaultShapePackEntry(
    const DefaultShapePackSpec& Spec)
{
    World PrefabWorld(std::string(Spec.AssetName) + ".PrefabWorld");
    auto NodeResult = PrefabWorld.CreateNode(std::string(Spec.AssetName));
    if (!NodeResult)
    {
        return std::unexpected(NodeResult.error().Message);
    }

    auto NodeHandle = NodeResult.value();
    auto* Node = NodeHandle.Borrowed();
    if (!Node)
    {
        return std::unexpected("Failed to resolve created node for default shape asset");
    }

    ScopedComponentOnCreateSuppression SuppressOnCreate{};

    auto TransformResult = Node->Add<TransformComponent>();
    if (!TransformResult)
    {
        return std::unexpected(TransformResult.error().Message);
    }
    TransformResult->Position = Vec3(0.0f, 0.0f, 0.0f);
    TransformResult->Rotation = Quat::Identity();
    TransformResult->Scale = Vec3(1.0f, 1.0f, 1.0f);

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto MeshResult = Node->Add<StaticMeshComponent>();
    if (!MeshResult)
    {
        return std::unexpected(MeshResult.error().Message);
    }

    auto& MeshSettings = MeshResult->EditSettings();
    MeshSettings.MeshPath = Spec.PrimitiveMeshPathToken;
    MeshSettings.Visible = true;
    MeshSettings.CastShadows = true;
    MeshSettings.SyncFromTransform = true;
    MeshSettings.RegisterWithRenderer = true;
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    // Physics metadata is optional for editor template primitives.
    // Keep pack generation robust even if physics components cannot be created in this bootstrap context.
    auto ColliderResult = Node->Add<ColliderComponent>();
    if (ColliderResult)
    {
        auto& ColliderSettings = ColliderResult->EditSettings();
        ColliderSettings.Shape = Spec.ColliderShape;
        ColliderSettings.HalfExtent = Spec.ColliderHalfExtent;
        ColliderSettings.Radius = Spec.ColliderRadius;
        ColliderSettings.HalfHeight = Spec.ColliderHalfHeight;
        ColliderSettings.Friction = Spec.ColliderFriction;
        ColliderSettings.Restitution = Spec.ColliderRestitution;
        ColliderSettings.Density = 1.0f;
        ColliderSettings.Layer = CollisionLayerFlags(ECollisionFilterBits::WorldStatic);
        ColliderSettings.Mask = kCollisionMaskAll;
        ColliderSettings.IsTrigger = false;

        RigidBodyComponent::Settings BodySettings{};
        BodySettings.BodyType = SnAPI::Physics::EBodyType::Static;
        BodySettings.Mass = 1.0f;
        BodySettings.EnableCcd = false;
        BodySettings.SyncFromPhysics = false;
        BodySettings.SyncToPhysics = true;
        BodySettings.StartActive = true;
        BodySettings.EnableRenderInterpolation = false;
        BodySettings.AutoDeactivateWhenSleeping = true;

        (void)Node->Add<RigidBodyComponent>(BodySettings);
    }
#endif

    auto NodePayloadResult = NodeSerializer::Serialize(*Node);
    if (!NodePayloadResult)
    {
        return std::unexpected(NodePayloadResult.error().Message);
    }

    std::vector<uint8_t> CookedBytes{};
    auto SerializeResult = SerializeNodePayload(*NodePayloadResult, CookedBytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error().Message);
    }

    ::SnAPI::AssetPipeline::AssetPackEntry Entry{};
    Entry.Id = AssetPipelineAssetIdFromName(std::string("SnAPI.Editor.DefaultShape.") + Spec.AssetName);
    Entry.AssetKind = AssetKindNode();
    Entry.Name = Spec.AssetName;
    Entry.VariantKey = "default";
    Entry.Cooked = ::SnAPI::AssetPipeline::TypedPayload(PayloadNode(),
                                                         NodeSerializer::kSchemaVersion,
                                                         std::move(CookedBytes));
    return Entry;
}

[[nodiscard]] std::expected<::SnAPI::AssetPipeline::AssetPackEntry, std::string> BuildDefaultShapePackEntryFromRuntimeWorld(
    const DefaultShapePackSpec& Spec,
    IWorld& RuntimeWorld)
{
    auto NodeResult = RuntimeWorld.CreateNode(StaticTypeId<BaseNode>(), std::string(Spec.AssetName));
    if (!NodeResult)
    {
        return std::unexpected(NodeResult.error().Message);
    }

    NodeHandle CreatedHandle = *NodeResult;
    BaseNode* Node = CreatedHandle.Borrowed();
    if (!Node)
    {
        Node = CreatedHandle.BorrowedSlowByUuid();
    }
    if (!Node)
    {
        (void)RuntimeWorld.DestroyNode(CreatedHandle);
        return std::unexpected("Failed to resolve created runtime node for default shape asset");
    }

    auto CleanupNode = [&RuntimeWorld, CreatedHandle]() mutable {
        (void)RuntimeWorld.DestroyNode(CreatedHandle);
    };

    ScopedComponentOnCreateSuppression SuppressOnCreate{};

    auto TransformResult = Node->Add<TransformComponent>();
    if (!TransformResult)
    {
        CleanupNode();
        return std::unexpected(TransformResult.error().Message);
    }
    TransformResult->Position = Vec3(0.0f, 0.0f, 0.0f);
    TransformResult->Rotation = Quat::Identity();
    TransformResult->Scale = Vec3(1.0f, 1.0f, 1.0f);

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto MeshResult = Node->Add<StaticMeshComponent>();
    if (!MeshResult)
    {
        CleanupNode();
        return std::unexpected(MeshResult.error().Message);
    }

    auto& MeshSettings = MeshResult->EditSettings();
    MeshSettings.MeshPath = Spec.PrimitiveMeshPathToken;
    MeshSettings.Visible = true;
    MeshSettings.CastShadows = true;
    MeshSettings.SyncFromTransform = true;
    MeshSettings.RegisterWithRenderer = true;
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    auto ColliderResult = Node->Add<ColliderComponent>();
    if (ColliderResult)
    {
        auto& ColliderSettings = ColliderResult->EditSettings();
        ColliderSettings.Shape = Spec.ColliderShape;
        ColliderSettings.HalfExtent = Spec.ColliderHalfExtent;
        ColliderSettings.Radius = Spec.ColliderRadius;
        ColliderSettings.HalfHeight = Spec.ColliderHalfHeight;
        ColliderSettings.Friction = Spec.ColliderFriction;
        ColliderSettings.Restitution = Spec.ColliderRestitution;
        ColliderSettings.Density = 1.0f;
        ColliderSettings.Layer = CollisionLayerFlags(ECollisionFilterBits::WorldStatic);
        ColliderSettings.Mask = kCollisionMaskAll;
        ColliderSettings.IsTrigger = false;

        RigidBodyComponent::Settings BodySettings{};
        BodySettings.BodyType = SnAPI::Physics::EBodyType::Static;
        BodySettings.Mass = 1.0f;
        BodySettings.EnableCcd = false;
        BodySettings.SyncFromPhysics = false;
        BodySettings.SyncToPhysics = true;
        BodySettings.StartActive = true;
        BodySettings.EnableRenderInterpolation = false;
        BodySettings.AutoDeactivateWhenSleeping = true;

        (void)Node->Add<RigidBodyComponent>(BodySettings);
    }
#endif

    auto NodePayloadResult = NodeSerializer::Serialize(*Node);
    CleanupNode();
    if (!NodePayloadResult)
    {
        return std::unexpected(NodePayloadResult.error().Message);
    }

    std::vector<uint8_t> CookedBytes{};
    auto SerializeResult = SerializeNodePayload(*NodePayloadResult, CookedBytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error().Message);
    }

    ::SnAPI::AssetPipeline::AssetPackEntry Entry{};
    Entry.Id = AssetPipelineAssetIdFromName(std::string("SnAPI.Editor.DefaultShape.") + Spec.AssetName);
    Entry.AssetKind = AssetKindNode();
    Entry.Name = Spec.AssetName;
    Entry.VariantKey = "default";
    Entry.Cooked = ::SnAPI::AssetPipeline::TypedPayload(PayloadNode(),
                                                         NodeSerializer::kSchemaVersion,
                                                         std::move(CookedBytes));
    return Entry;
}

[[nodiscard]] std::expected<std::size_t, std::string> EnsureDefaultShapePacks(const std::filesystem::path& PackDirectory,
                                                                               IWorld* RuntimeWorld)
{
    if (PackDirectory.empty())
    {
        return std::unexpected("Unable to resolve default path for editor default assets");
    }

    std::error_code Error{};
    std::filesystem::create_directories(PackDirectory, Error);
    if (Error)
    {
        return std::unexpected("Failed to create default asset directory: " + Error.message());
    }

    std::size_t CreatedCount = 0;
    for (const auto& Spec : DefaultShapePackSpecs())
    {
        const std::filesystem::path PackPath = PackDirectory / Spec.PackFileName;

        Error.clear();
        if (std::filesystem::exists(PackPath, Error) && !Error)
        {
            ::SnAPI::AssetPipeline::AssetPackReader ExistingReader{};
            if (auto OpenResult = ExistingReader.Open(PackPath.string()); OpenResult)
            {
                const uint32_t ExistingAssetCount = ExistingReader.GetAssetCount();
                bool HasNodeAsset = false;
                for (uint32_t Index = 0; Index < ExistingAssetCount; ++Index)
                {
                    auto InfoResult = ExistingReader.GetAssetInfo(Index);
                    if (InfoResult && InfoResult->AssetKind == AssetKindNode())
                    {
                        HasNodeAsset = true;
                        break;
                    }
                }
                if (HasNodeAsset)
                {
                    continue;
                }
            }
        }

        auto EntryResult = BuildDefaultShapePackEntry(Spec);
        if (!EntryResult && RuntimeWorld)
        {
            EntryResult = BuildDefaultShapePackEntryFromRuntimeWorld(Spec, *RuntimeWorld);
        }
        if (!EntryResult)
        {
            return std::unexpected("Failed to build default shape '" + std::string(Spec.AssetName) + "': " + EntryResult.error());
        }

        ::SnAPI::AssetPipeline::AssetPackWriter Writer{};
        Writer.AddAsset(std::move(*EntryResult));
        auto WriteResult = Writer.Write(PackPath.string());
        if (!WriteResult)
        {
            return std::unexpected("Failed to write pack '" + PackPath.string() + "': " + WriteResult.error());
        }

        ++CreatedCount;
    }

    return CreatedCount;
}

struct RuntimeWorldCounts
{
    std::size_t Nodes = 0;
    std::size_t Components = 0;
};

[[nodiscard]] RuntimeWorldCounts CountRuntimeWorldObjects(World& WorldRef)
{
    RuntimeWorldCounts Counts{};
    WorldRef.ForEachNode([&](const NodeHandle&, BaseNode& NodeRef) {
        ++Counts.Nodes;
        Counts.Components += NodeRef.ComponentTypes().size();
    });
    return Counts;
}

void AppendUniquePath(std::vector<std::string>& Paths,
                      std::unordered_set<std::string>& SeenPaths,
                      const std::filesystem::path& InputPath)
{
    std::error_code Error{};
    std::filesystem::path PathToUse = InputPath;
    if (auto ResolvedPath = SPathResolver::Instance().Resolve(InputPath.string()); ResolvedPath)
    {
        PathToUse = *ResolvedPath;
    }

    if (PathToUse.empty())
    {
        return;
    }

    const std::filesystem::path Canonical = std::filesystem::weakly_canonical(PathToUse, Error);
    if (!Error)
    {
        PathToUse = Canonical;
    }
    else
    {
        Error.clear();
        const auto Absolute = std::filesystem::absolute(PathToUse, Error);
        if (!Error)
        {
            PathToUse = Absolute;
        }
    }

    Error.clear();
    if (!std::filesystem::exists(PathToUse, Error) || Error)
    {
        return;
    }

    Error.clear();
    if (!std::filesystem::is_directory(PathToUse, Error) || Error)
    {
        return;
    }

    const std::string Key = ToLowerCopy(PathToUse.generic_string());
    if (!SeenPaths.insert(Key).second)
    {
        return;
    }

    Paths.push_back(PathToUse.string());
}

[[nodiscard]] std::string NormalizeAssetLogicalName(std::string_view RawName)
{
    std::string Name(RawName);
    std::replace(Name.begin(), Name.end(), '\\', '/');

    const auto IsWhitespace = [](const unsigned char Character) {
        return std::isspace(Character) != 0;
    };

    while (!Name.empty() && IsWhitespace(static_cast<unsigned char>(Name.front())))
    {
        Name.erase(Name.begin());
    }
    while (!Name.empty() && IsWhitespace(static_cast<unsigned char>(Name.back())))
    {
        Name.pop_back();
    }

    while (Name.find("//") != std::string::npos)
    {
        Name.replace(Name.find("//"), 2u, "/");
    }

    while (Name.rfind("./", 0) == 0)
    {
        Name.erase(0, 2);
    }

    while (!Name.empty() && Name.front() == '/')
    {
        Name.erase(Name.begin());
    }

    if (Name == ".")
    {
        Name.clear();
    }

    return Name;
}

[[nodiscard]] std::string ShortTypeName(std::string_view QualifiedTypeName)
{
    return PrettyReflectedTypeName(QualifiedTypeName);
}

[[nodiscard]] std::string LeafLogicalName(std::string Value)
{
    Value = NormalizeAssetLogicalName(Value);
    const std::size_t Delimiter = Value.rfind('/');
    if (Delimiter == std::string::npos)
    {
        return Value;
    }
    return Value.substr(Delimiter + 1u);
}

void InitializeCreatedNodeDefaults(IWorld& WorldRef, BaseNode& Node)
{
    NodeHandle NodeHandleValue = Node.Handle();
    (void)WorldRef.RequestNodeOnCreate(NodeHandleValue);
}

[[nodiscard]] std::string MakeUniqueLogicalName(::SnAPI::AssetPipeline::AssetManager& AssetManagerRef,
                                                const std::string& Prefix,
                                                std::string BaseName)
{
    BaseName = NormalizeAssetLogicalName(BaseName);
    if (BaseName.empty())
    {
        BaseName = "Asset";
    }

    std::string CandidateBase = NormalizeAssetLogicalName(Prefix + "/" + BaseName);
    if (CandidateBase.empty())
    {
        CandidateBase = BaseName;
    }

    std::string Candidate = CandidateBase;
    std::size_t SuffixIndex = 1;
    while (AssetManagerRef.FindAsset(Candidate).has_value())
    {
        Candidate = CandidateBase + "_" + std::to_string(SuffixIndex++);
    }

    return Candidate;
}

[[nodiscard]] ::SnAPI::AssetPipeline::AssetId MakeDeterministicSourceAssetId(const std::string_view LogicalName,
                                                                             const std::string_view VariantKey = {})
{
    return SourceAssetIdFromLogicalName(LogicalName, VariantKey);
}

[[nodiscard]] std::string NormalizeAssetExtension(std::string Extension)
{
    if (Extension.empty())
    {
        return Extension;
    }
    if (Extension.front() != '.')
    {
        Extension.insert(Extension.begin(), '.');
    }
    return ToLowerCopy(Extension);
}

[[nodiscard]] bool IsCookedPackFile(const std::filesystem::path& Path)
{
    return NormalizeAssetExtension(Path.extension().string()) == ".snpak";
}

[[nodiscard]] std::string BuildSourceLogicalName(const std::filesystem::path& AssetRoot,
                                                 const std::filesystem::path& SourceFile)
{
    std::error_code Error{};
    std::filesystem::path RelativePath = std::filesystem::relative(SourceFile, AssetRoot, Error);
    if (Error)
    {
        RelativePath = SourceFile.filename();
    }
    return NormalizeAssetLogicalName(RelativePath.generic_string());
}

[[nodiscard]] std::string MakeUniqueSourceLogicalName(const std::filesystem::path& AssetRoot,
                                                      const std::string& FolderPath,
                                                      std::string BaseName,
                                                      std::string Extension)
{
    BaseName = LeafLogicalName(std::move(BaseName));
    if (BaseName.empty())
    {
        BaseName = "Asset";
    }

    Extension = NormalizeAssetExtension(std::move(Extension));
    const std::string NormalizedFolder = NormalizeAssetLogicalName(FolderPath);

    const auto BuildCandidate = [&](const std::string& CandidateBase) {
        const std::string Leaf = CandidateBase + Extension;
        return NormalizedFolder.empty()
            ? NormalizeAssetLogicalName(Leaf)
            : NormalizeAssetLogicalName(NormalizedFolder + "/" + Leaf);
    };

    std::string CandidateBase = BaseName;
    std::string CandidateLogical = BuildCandidate(CandidateBase);
    std::size_t SuffixIndex = 1;

    while (!CandidateLogical.empty())
    {
        const std::filesystem::path CandidatePath = AssetRoot / std::filesystem::path(CandidateLogical);
        std::error_code ExistsError{};
        if (!std::filesystem::exists(CandidatePath, ExistsError))
        {
            break;
        }
        CandidateBase = BaseName + "_" + std::to_string(SuffixIndex++);
        CandidateLogical = BuildCandidate(CandidateBase);
    }

    return CandidateLogical;
}

[[nodiscard]] std::filesystem::path ResolveImportAssetRootDirectory(const EditorAssetService::ProjectInfo& Project)
{
    if (Project.IsLoaded && !Project.AssetRootDirectory.empty())
    {
        if (auto Resolved = SPathResolver::Instance().Resolve(Project.AssetRootDirectory); Resolved && !Resolved->empty())
        {
            return *Resolved;
        }
        return std::filesystem::path(Project.AssetRootDirectory);
    }

    if (const std::filesystem::path ResolverRoot = SPathResolver::Instance().AssetRoot(); !ResolverRoot.empty())
    {
        return ResolverRoot;
    }

    std::error_code Error{};
    const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
    if (!Error && !CurrentPath.empty())
    {
        return CurrentPath / "Assets";
    }

    return {};
}

[[nodiscard]] std::string SanitizePackFileStem(std::string_view Raw)
{
    std::string Out{};
    Out.reserve(Raw.size());
    for (const char Character : Raw)
    {
        const unsigned char Byte = static_cast<unsigned char>(Character);
        if (std::isalnum(Byte) != 0 || Character == '_' || Character == '-' || Character == '.')
        {
            Out.push_back(Character);
        }
        else
        {
            Out.push_back('_');
        }
    }

    while (!Out.empty() && Out.front() == '.')
    {
        Out.erase(Out.begin());
    }

    if (Out.empty())
    {
        Out = "ImportedAsset";
    }

    return Out;
}

[[nodiscard]] std::vector<std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter>> CreateEditorImporters()
{
    std::vector<std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter>> Importers{};
    Importers.reserve(4);
    if (auto TextureImporter = TextureCompressorPlugin::CreateTextureCompressorImporter())
    {
        Importers.emplace_back(std::move(TextureImporter));
    }
    if (auto AssimpImporter = CreateRenderAssetAssimpImporter())
    {
        Importers.emplace_back(std::move(AssimpImporter));
    }
    if (auto AuthoredImporter = CreateAuthoredAssetJsonImporter())
    {
        Importers.emplace_back(std::move(AuthoredImporter));
    }
    if (auto JsonImporter = CreateRenderAssetJsonImporter())
    {
        Importers.emplace_back(std::move(JsonImporter));
    }
    return Importers;
}

[[nodiscard]] std::vector<std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker>> CreateEditorCookers()
{
    std::vector<std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker>> Cookers{};
    Cookers.reserve(12);
    if (auto TextureCooker = TextureCompressorPlugin::CreateTextureCompressorCooker())
    {
        Cookers.emplace_back(std::move(TextureCooker));
    }
    if (auto AuthoredCooker = CreateAuthoredAssetPassThroughCooker())
    {
        Cookers.emplace_back(std::move(AuthoredCooker));
    }
    if (auto NodeCooker = CreateNodeSourceCooker())
    {
        Cookers.emplace_back(std::move(NodeCooker));
    }
    if (auto LevelCooker = CreateLevelSourceCooker())
    {
        Cookers.emplace_back(std::move(LevelCooker));
    }
    if (auto WorldCooker = CreateWorldSourceCooker())
    {
        Cookers.emplace_back(std::move(WorldCooker));
    }
    if (auto TextureSourceCooker = CreateRenderTextureCooker())
    {
        Cookers.emplace_back(std::move(TextureSourceCooker));
    }
    if (auto MaterialCooker = CreateRenderMaterialCooker())
    {
        Cookers.emplace_back(std::move(MaterialCooker));
    }
    if (auto MaterialInstanceCooker = CreateRenderMaterialInstanceCooker())
    {
        Cookers.emplace_back(std::move(MaterialInstanceCooker));
    }
    if (auto SkeletonCooker = CreateRenderSkeletonCooker())
    {
        Cookers.emplace_back(std::move(SkeletonCooker));
    }
    if (auto AnimationCooker = CreateRenderAnimationCooker())
    {
        Cookers.emplace_back(std::move(AnimationCooker));
    }
    if (auto StaticMeshCooker = CreateRenderStaticMeshCooker())
    {
        Cookers.emplace_back(std::move(StaticMeshCooker));
    }
    if (auto SkeletalMeshCooker = CreateRenderSkeletalMeshCooker())
    {
        Cookers.emplace_back(std::move(SkeletalMeshCooker));
    }
    return Cookers;
}

[[nodiscard]] ::SnAPI::AssetPipeline::IAssetImporter* FindMatchingImporter(
    const ::SnAPI::AssetPipeline::SourceRef& Source,
    const std::vector<std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter>>& Importers)
{
    for (const auto& Importer : Importers)
    {
        if (Importer && Importer->CanImport(Source))
        {
            return Importer.get();
        }
    }
    return nullptr;
}

[[nodiscard]] ::SnAPI::AssetPipeline::IAssetCooker* FindMatchingCooker(
    const ::SnAPI::AssetPipeline::TypeId& AssetKind,
    const ::SnAPI::AssetPipeline::TypeId& IntermediateType,
    const std::vector<std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker>>& Cookers)
{
    for (const auto& Cooker : Cookers)
    {
        if (Cooker && Cooker->CanCook(AssetKind, IntermediateType))
        {
            return Cooker.get();
        }
    }
    return nullptr;
}

[[nodiscard]] bool IsTextureImporter(const ::SnAPI::AssetPipeline::IAssetImporter& Importer)
{
    const std::string Name = ToLowerCopy(Importer.GetName() ? Importer.GetName() : "");
    return Name.find("texturecompressor") != std::string::npos;
}

struct ImportedNameParts
{
    std::string Scope{};
    std::string Leaf{};
};

using ImportedSourceAssetVariant = std::variant<
    TextureAsset,
    MaterialAsset,
    MaterialInstanceAsset,
    StaticMeshAsset,
    SkeletalMeshAsset,
    SkeletonAsset,
    SkeletalAnimationAsset>;

struct ImportedSourceDocument
{
    std::string OriginalLogicalName{};
    std::string FinalLogicalName{};
    ::SnAPI::AssetPipeline::AssetId FinalAssetId{};
    ImportedSourceAssetVariant Asset{};
};

struct ScopedStagedImportDirectory
{
    std::filesystem::path Root{};

    ~ScopedStagedImportDirectory()
    {
        if (Root.empty())
        {
            return;
        }

        std::error_code RemoveError{};
        (void)std::filesystem::remove_all(Root, RemoveError);
    }
};

[[nodiscard]] std::expected<std::filesystem::path, std::string> StageImportSourceDirectory(
    const std::filesystem::path& SourceFile,
    ScopedStagedImportDirectory& OutStage)
{
    std::error_code PathError{};
    const std::filesystem::path TempRoot = std::filesystem::temp_directory_path(PathError);
    if (PathError || TempRoot.empty())
    {
        return std::unexpected("Failed to resolve temporary directory for staged import");
    }

    const auto Stamp = std::to_string(static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::filesystem::path StageRoot = (TempRoot / ("snapi_gf_import_" + Stamp)).lexically_normal();
    const std::filesystem::path StageSourceRoot = StageRoot / "src";

    PathError.clear();
    std::filesystem::create_directories(StageRoot, PathError);
    if (PathError)
    {
        return std::unexpected("Failed to create staged import directory: " + PathError.message());
    }

    PathError.clear();
    std::filesystem::copy(
        SourceFile.parent_path(),
        StageSourceRoot,
        std::filesystem::copy_options::recursive,
        PathError);
    if (PathError)
    {
        return std::unexpected("Failed to stage import source directory: " + PathError.message());
    }

    const std::filesystem::path StagedSourceFile = StageSourceRoot / SourceFile.filename();
    PathError.clear();
    if (!std::filesystem::exists(StagedSourceFile, PathError) || PathError)
    {
        return std::unexpected("Staged import source file was not found after copy");
    }

    OutStage.Root = StageRoot;
    return StagedSourceFile;
}

[[nodiscard]] ImportedNameParts ParseImportedNameParts(std::string_view LogicalName)
{
    ImportedNameParts Parts{};
    const std::size_t LastScope = LogicalName.rfind("::");
    if (LastScope == std::string_view::npos)
    {
        Parts.Leaf = LeafLogicalName(std::string(LogicalName));
        return Parts;
    }

    Parts.Leaf = std::string(LogicalName.substr(LastScope + 2u));
    if (LastScope > 0u)
    {
        const std::size_t ScopeStart = LogicalName.rfind("::", LastScope - 1u);
        if (ScopeStart != std::string_view::npos)
        {
            Parts.Scope = std::string(LogicalName.substr(ScopeStart + 2u, LastScope - (ScopeStart + 2u)));
        }
    }
    return Parts;
}

[[nodiscard]] std::string SanitizeImportedLeaf(std::string_view Value, std::string_view Fallback)
{
    std::string Candidate(Value);
    if (Candidate.find('/') != std::string::npos || Candidate.find('\\') != std::string::npos)
    {
        Candidate = std::filesystem::path(Candidate).stem().string();
    }
    if (const std::filesystem::path PathCandidate(Candidate); PathCandidate.has_extension())
    {
        Candidate = PathCandidate.stem().string();
    }
    if (Candidate.empty())
    {
        Candidate = std::string(Fallback);
    }

    std::string Out{};
    Out.reserve(Candidate.size() + 8u);
    for (const char Ch : Candidate)
    {
        const bool AlphaNum = std::isalnum(static_cast<unsigned char>(Ch)) != 0;
        Out.push_back(AlphaNum ? Ch : '_');
    }
    while (!Out.empty() && Out.back() == '_')
    {
        Out.pop_back();
    }
    while (!Out.empty() && Out.front() == '_')
    {
        Out.erase(Out.begin());
    }
    if (Out.empty())
    {
        Out = std::string(Fallback);
    }
    return Out;
}

[[nodiscard]] std::string TextureFormatToAuthoredString(const TextureCompressorPlugin::ECompressedFormat Format)
{
    using TextureCompressorPlugin::ECompressedFormat;
    switch (Format)
    {
    case ECompressedFormat::Unknown: return "Auto";
    case ECompressedFormat::BC1: return "BC1";
    case ECompressedFormat::BC3: return "BC3";
    case ECompressedFormat::BC4: return "BC4";
    case ECompressedFormat::BC5: return "BC5";
    case ECompressedFormat::BC6H: return "BC6H";
    case ECompressedFormat::BC7: return "BC7";
    case ECompressedFormat::ASTC_4x4: return "ASTC_4x4";
    case ECompressedFormat::ASTC_5x5: return "ASTC_5x5";
    case ECompressedFormat::ASTC_6x6: return "ASTC_6x6";
    case ECompressedFormat::ASTC_8x8: return "ASTC_8x8";
    case ECompressedFormat::ASTC_10x10: return "ASTC_10x10";
    case ECompressedFormat::ASTC_12x12: return "ASTC_12x12";
    case ECompressedFormat::ASTC_4x4_HDR: return "ASTC_4x4_HDR";
    case ECompressedFormat::ASTC_6x6_HDR: return "ASTC_6x6_HDR";
    case ECompressedFormat::ASTC_8x8_HDR: return "ASTC_8x8_HDR";
    default: return "Auto";
    }
}

[[nodiscard]] TextureImportSettingsPayload BuildTextureAssetImportSettingsPayload(
    const TextureCompressorPlugin::TextureCompressorImportSettings* TypedSettings)
{
    TextureImportSettingsPayload Payload{};
    if (!TypedSettings)
    {
        return Payload;
    }

    Payload.Target = TypedSettings->Target == TextureCompressorPlugin::ECompressionTarget::ASTC ? "ASTC" : "BCn";
    Payload.Format = TextureFormatToAuthoredString(TypedSettings->Format);
    Payload.Quality = std::clamp(TypedSettings->Quality, 0.0f, 1.0f);
    Payload.ForceNormalMap = TypedSettings->ForceNormalMap;
    Payload.MaxMips = TypedSettings->MaxMipCount > 0 ? static_cast<uint32_t>(TypedSettings->MaxMipCount) : 0u;
    Payload.ForceSrgb = TypedSettings->ColorSpacePolicy == TextureCompressorPlugin::ETextureColorSpacePolicy::ForceSrgb;
    Payload.ForceLinear = TypedSettings->ColorSpacePolicy == TextureCompressorPlugin::ETextureColorSpacePolicy::ForceLinear;
    return Payload;
}

[[nodiscard]] TextureAsset BuildTextureAssetFromIntermediate(
    const TextureCompressorPlugin::ImageIntermediate& Intermediate,
    const TextureImportSettingsPayload& ImportSettings)
{
    TextureAsset Asset{};
    Asset.Image.Width = Intermediate.Width;
    Asset.Image.Height = Intermediate.Height;
    Asset.Image.Channels = Intermediate.Channels;
    Asset.Image.BitsPerChannel = Intermediate.BitsPerChannel;
    Asset.Image.IsFloat = Intermediate.bIsFloat;
    Asset.Image.HasNonTrivialAlpha = Intermediate.bHasNonTrivialAlpha;
    Asset.Image.SRGB = Intermediate.bSRGB;
    Asset.Image.SourceFilename = Intermediate.SourceFilename;
    Asset.Image.Pixels = Intermediate.Pixels;
    Asset.ImportSettings = ImportSettings;
    return Asset;
}

[[nodiscard]] std::vector<ImportBuildOptionPayload> BuildImportBuildOptionPayloads(
    const std::unordered_map<std::string, std::string>& BuildOptions)
{
    std::vector<ImportBuildOptionPayload> Result{};
    Result.reserve(BuildOptions.size());
    for (const auto& [Key, Value] : BuildOptions)
    {
        Result.push_back(ImportBuildOptionPayload{.Key = Key, .Value = Value});
    }
    std::sort(Result.begin(), Result.end(), [](const ImportBuildOptionPayload& Left, const ImportBuildOptionPayload& Right) {
        return Left.Key < Right.Key || (Left.Key == Right.Key && Left.Value < Right.Value);
    });
    return Result;
}

[[nodiscard]] std::unordered_map<std::string, std::string> BuildImportBuildOptionMap(
    const std::vector<ImportBuildOptionPayload>& BuildOptions)
{
    std::unordered_map<std::string, std::string> Result{};
    for (const ImportBuildOptionPayload& Option : BuildOptions)
    {
        if (!Option.Key.empty())
        {
            Result[Option.Key] = Option.Value;
        }
    }
    return Result;
}

[[nodiscard]] ImportedAssetProvenancePayload BuildImportedAssetProvenancePayload(
    const std::string_view SourcePath,
    const std::string_view DestinationFolder,
    const std::string_view ImporterName,
    const EImportProfile Profile,
    const std::unordered_map<std::string, std::string>& BuildOptions)
{
    ImportedAssetProvenancePayload Payload{};
    Payload.Profile = ImportProfileToString(Profile);
    Payload.SourcePath = std::string(SourcePath);
    Payload.DestinationFolder = NormalizeAssetLogicalName(DestinationFolder);
    Payload.ImporterName = std::string(ImporterName);
    Payload.BuildOptions = BuildImportBuildOptionPayloads(BuildOptions);
    return Payload;
}

void ApplyImportedSourceAssetProvenance(ImportedSourceAssetVariant& Asset,
                                        const ImportedAssetProvenancePayload& Provenance)
{
    std::visit([&Provenance](auto& TypedAsset) {
        TypedAsset.Provenance = Provenance;
    }, Asset);
}

[[nodiscard]] std::expected<ImportedSourceAssetVariant, std::string> BuildImportedSourceAssetVariant(
    const ::SnAPI::AssetPipeline::ImportedItem& Item,
    const ::SnAPI::AssetPipeline::PayloadRegistry& Registry,
    const TextureImportSettingsPayload& TextureSettings)
{
    const auto* Serializer = Registry.Find(Item.Intermediate.PayloadType);
    if (!Serializer)
    {
        return std::unexpected("Missing serializer for imported source payload");
    }

    if (Item.Intermediate.PayloadType == TextureCompressorPlugin::Payload_CompressorImageIntermediate)
    {
        TextureCompressorPlugin::ImageIntermediate Intermediate{};
        if (!Serializer->DeserializeFromBytes(&Intermediate, Item.Intermediate.Bytes.data(), Item.Intermediate.Bytes.size()))
        {
            return std::unexpected("Failed to deserialize imported texture intermediate");
        }
        return ImportedSourceAssetVariant(BuildTextureAssetFromIntermediate(Intermediate, TextureSettings));
    }

    if (Item.Intermediate.PayloadType == PayloadMaterial())
    {
        MaterialAsset Asset{};
        if (!Serializer->DeserializeFromBytes(&Asset, Item.Intermediate.Bytes.data(), Item.Intermediate.Bytes.size()))
        {
            return std::unexpected("Failed to deserialize imported material payload");
        }
        return ImportedSourceAssetVariant(std::move(Asset));
    }

    if (Item.Intermediate.PayloadType == PayloadMaterialInstance())
    {
        MaterialInstanceAsset Asset{};
        if (!Serializer->DeserializeFromBytes(&Asset, Item.Intermediate.Bytes.data(), Item.Intermediate.Bytes.size()))
        {
            return std::unexpected("Failed to deserialize imported material-instance payload");
        }
        return ImportedSourceAssetVariant(std::move(Asset));
    }

    if (Item.Intermediate.PayloadType == PayloadStaticMeshSource())
    {
        StaticMeshAsset Asset{};
        if (!Serializer->DeserializeFromBytes(&Asset, Item.Intermediate.Bytes.data(), Item.Intermediate.Bytes.size()))
        {
            return std::unexpected("Failed to deserialize imported static-mesh payload");
        }
        return ImportedSourceAssetVariant(std::move(Asset));
    }

    if (Item.Intermediate.PayloadType == PayloadSkeletalMeshSource())
    {
        SkeletalMeshAsset Asset{};
        if (!Serializer->DeserializeFromBytes(&Asset, Item.Intermediate.Bytes.data(), Item.Intermediate.Bytes.size()))
        {
            return std::unexpected("Failed to deserialize imported skeletal-mesh payload");
        }
        return ImportedSourceAssetVariant(std::move(Asset));
    }

    if (Item.Intermediate.PayloadType == PayloadSkeleton())
    {
        SkeletonPayload Payload{};
        if (!Serializer->DeserializeFromBytes(&Payload, Item.Intermediate.Bytes.data(), Item.Intermediate.Bytes.size()))
        {
            return std::unexpected("Failed to deserialize imported skeleton payload");
        }
        SkeletonAsset Asset{};
        Asset.Skeleton = std::move(Payload);
        return ImportedSourceAssetVariant(std::move(Asset));
    }

    if (Item.Intermediate.PayloadType == PayloadAnimation())
    {
        AnimationPayload Payload{};
        if (!Serializer->DeserializeFromBytes(&Payload, Item.Intermediate.Bytes.data(), Item.Intermediate.Bytes.size()))
        {
            return std::unexpected("Failed to deserialize imported animation payload");
        }
        SkeletalAnimationAsset Asset{};
        Asset.Animation = std::move(Payload);
        return ImportedSourceAssetVariant(std::move(Asset));
    }

    return std::unexpected("Imported payload type is not supported by authored-source import");
}

[[nodiscard]] bool IsImportedSourceAssetType(const TypeId& AssetType)
{
    return AssetType == StaticTypeId<TextureAsset>() ||
           AssetType == StaticTypeId<MaterialAsset>() ||
           AssetType == StaticTypeId<MaterialInstanceAsset>() ||
           AssetType == StaticTypeId<StaticMeshAsset>() ||
           AssetType == StaticTypeId<SkeletalMeshAsset>() ||
           AssetType == StaticTypeId<SkeletonAsset>() ||
           AssetType == StaticTypeId<SkeletalAnimationAsset>();
}

[[nodiscard]] std::expected<ImportedSourceAssetVariant, std::string> LoadImportedSourceAssetVariant(
    const Editor::EditorAssetService::DiscoveredAsset& Asset)
{
    if (Asset.SourceFilePath.empty())
    {
        return std::unexpected("Imported source asset has no source file path");
    }

    std::ifstream File(Asset.SourceFilePath, std::ios::binary | std::ios::ate);
    if (!File.is_open())
    {
        return std::unexpected("Failed to open imported source asset: " + Asset.SourceFilePath);
    }

    const std::streamsize Size = File.tellg();
    std::string SourceJson{};
    if (Size > 0)
    {
        SourceJson.resize(static_cast<std::size_t>(Size));
        File.seekg(0, std::ios::beg);
        File.read(SourceJson.data(), Size);
        if (!File.good())
        {
            return std::unexpected("Failed to read imported source asset: " + Asset.SourceFilePath);
        }
    }

    if (Asset.AssetType == StaticTypeId<TextureAsset>())
    {
        TextureAsset TypedAsset{};
        if (auto ParseResult = DeserializeAuthoredAssetFromJson(SourceJson, TypedAsset); !ParseResult)
        {
            return std::unexpected(ParseResult.error().Message);
        }
        return ImportedSourceAssetVariant(std::move(TypedAsset));
    }
    if (Asset.AssetType == StaticTypeId<MaterialAsset>())
    {
        MaterialAsset TypedAsset{};
        if (auto ParseResult = DeserializeAuthoredAssetFromJson(SourceJson, TypedAsset); !ParseResult)
        {
            return std::unexpected(ParseResult.error().Message);
        }
        return ImportedSourceAssetVariant(std::move(TypedAsset));
    }
    if (Asset.AssetType == StaticTypeId<MaterialInstanceAsset>())
    {
        MaterialInstanceAsset TypedAsset{};
        if (auto ParseResult = DeserializeAuthoredAssetFromJson(SourceJson, TypedAsset); !ParseResult)
        {
            return std::unexpected(ParseResult.error().Message);
        }
        return ImportedSourceAssetVariant(std::move(TypedAsset));
    }
    if (Asset.AssetType == StaticTypeId<StaticMeshAsset>())
    {
        StaticMeshAsset TypedAsset{};
        if (auto ParseResult = DeserializeAuthoredAssetFromJson(SourceJson, TypedAsset); !ParseResult)
        {
            return std::unexpected(ParseResult.error().Message);
        }
        return ImportedSourceAssetVariant(std::move(TypedAsset));
    }
    if (Asset.AssetType == StaticTypeId<SkeletalMeshAsset>())
    {
        SkeletalMeshAsset TypedAsset{};
        if (auto ParseResult = DeserializeAuthoredAssetFromJson(SourceJson, TypedAsset); !ParseResult)
        {
            return std::unexpected(ParseResult.error().Message);
        }
        return ImportedSourceAssetVariant(std::move(TypedAsset));
    }
    if (Asset.AssetType == StaticTypeId<SkeletonAsset>())
    {
        SkeletonAsset TypedAsset{};
        if (auto ParseResult = DeserializeAuthoredAssetFromJson(SourceJson, TypedAsset); !ParseResult)
        {
            return std::unexpected(ParseResult.error().Message);
        }
        return ImportedSourceAssetVariant(std::move(TypedAsset));
    }
    if (Asset.AssetType == StaticTypeId<SkeletalAnimationAsset>())
    {
        SkeletalAnimationAsset TypedAsset{};
        if (auto ParseResult = DeserializeAuthoredAssetFromJson(SourceJson, TypedAsset); !ParseResult)
        {
            return std::unexpected(ParseResult.error().Message);
        }
        return ImportedSourceAssetVariant(std::move(TypedAsset));
    }

    return std::unexpected("Authored asset type is not an imported source asset");
}

[[nodiscard]] bool ImportedProvenanceMatchesGroup(const ImportedAssetProvenancePayload& Provenance,
                                                  const std::string_view SourcePath,
                                                  const std::string_view DestinationFolder,
                                                  const EImportProfile Profile)
{
    return Provenance.SourcePath == SourcePath &&
           NormalizeAssetLogicalName(Provenance.DestinationFolder) == NormalizeAssetLogicalName(DestinationFolder) &&
           ImportProfileFromString(Provenance.Profile) == Profile;
}

[[nodiscard]] bool ImportedSourceAssetMatchesGroup(const ImportedSourceAssetVariant& Asset,
                                                   const std::string_view SourcePath,
                                                   const std::string_view DestinationFolder,
                                                   const EImportProfile Profile)
{
    return std::visit(
        [SourcePath, DestinationFolder, Profile](const auto& TypedAsset) {
            return ImportedProvenanceMatchesGroup(TypedAsset.Provenance, SourcePath, DestinationFolder, Profile);
        },
        Asset);
}

[[nodiscard]] bool UpdateImportedSourceAssetProvenance(ImportedSourceAssetVariant& Asset,
                                                       const ImportedAssetProvenancePayload& Provenance)
{
    bool Changed = false;
    std::visit(
        [&Changed, &Provenance](auto& TypedAsset) {
            if (!(TypedAsset.Provenance == Provenance))
            {
                TypedAsset.Provenance = Provenance;
                Changed = true;
            }
        },
        Asset);
    return Changed;
}

void RewriteImportedAssetRefs(ImportedSourceAssetVariant& Asset,
                              const std::unordered_map<std::string, AssetRefPayload>& RefByOriginalName)
{
    std::visit([&RefByOriginalName](auto& TypedAsset) {
        using TAsset = std::remove_cvref_t<decltype(TypedAsset)>;
        if constexpr (std::is_same_v<TAsset, MaterialInstanceAsset>)
        {
            for (MaterialTextureParamPayload& Texture : TypedAsset.Textures)
            {
                if (const auto It = RefByOriginalName.find(Texture.Texture.AssetName); It != RefByOriginalName.end())
                {
                    Texture.Texture = It->second;
                }
            }
        }
        else if constexpr (std::is_same_v<TAsset, StaticMeshAsset>)
        {
            for (AssetRefPayload& MaterialRef : TypedAsset.Mesh.MaterialInstances)
            {
                if (const auto It = RefByOriginalName.find(MaterialRef.AssetName); It != RefByOriginalName.end())
                {
                    MaterialRef = It->second;
                }
            }
        }
        else if constexpr (std::is_same_v<TAsset, SkeletalMeshAsset>)
        {
            for (AssetRefPayload& MaterialRef : TypedAsset.BaseMesh.Mesh.MaterialInstances)
            {
                if (const auto It = RefByOriginalName.find(MaterialRef.AssetName); It != RefByOriginalName.end())
                {
                    MaterialRef = It->second;
                }
            }
            if (const auto It = RefByOriginalName.find(TypedAsset.Skeleton.AssetName); It != RefByOriginalName.end())
            {
                TypedAsset.Skeleton = It->second;
            }
            for (AssetRefPayload& AnimationRef : TypedAsset.Animations)
            {
                if (const auto It = RefByOriginalName.find(AnimationRef.AssetName); It != RefByOriginalName.end())
                {
                    AnimationRef = It->second;
                }
            }
        }
    }, Asset);
}

[[nodiscard]] std::expected<std::string, std::string> SerializeImportedSourceAssetJson(const ImportedSourceAssetVariant& Asset)
{
    return std::visit([](const auto& TypedAsset) -> std::expected<std::string, std::string> {
        auto JsonResult = SerializeAuthoredAssetToJson(TypedAsset);
        if (!JsonResult)
        {
            return std::unexpected(JsonResult.error().Message);
        }
        return *JsonResult;
    }, Asset);
}

[[nodiscard]] std::string FileExtensionForImportedSourceAsset(const ImportedSourceAssetVariant& Asset)
{
    return std::visit([](const auto& TypedAsset) {
        return std::string(TypedAsset.FileExtension());
    }, Asset);
}

[[nodiscard]] std::string FallbackLeafForImportedSourceAsset(const ImportedSourceAssetVariant& Asset)
{
    return std::visit([](const auto& TypedAsset) -> std::string {
        using TAsset = std::remove_cvref_t<decltype(TypedAsset)>;
        if constexpr (std::is_same_v<TAsset, TextureAsset>)
        {
            return !TypedAsset.Image.SourceFilename.empty() ? TypedAsset.Image.SourceFilename : "Texture";
        }
        else if constexpr (std::is_same_v<TAsset, MaterialAsset>)
        {
            return "Material";
        }
        else if constexpr (std::is_same_v<TAsset, MaterialInstanceAsset>)
        {
            return "MaterialInstance";
        }
        else if constexpr (std::is_same_v<TAsset, StaticMeshAsset>)
        {
            return !TypedAsset.Mesh.Name.empty() ? TypedAsset.Mesh.Name : "StaticMesh";
        }
        else if constexpr (std::is_same_v<TAsset, SkeletalMeshAsset>)
        {
            return !TypedAsset.BaseMesh.Mesh.Name.empty() ? TypedAsset.BaseMesh.Mesh.Name : "SkeletalMesh";
        }
        else if constexpr (std::is_same_v<TAsset, SkeletonAsset>)
        {
            return !TypedAsset.Skeleton.Name.empty() ? TypedAsset.Skeleton.Name : "Skeleton";
        }
        else if constexpr (std::is_same_v<TAsset, SkeletalAnimationAsset>)
        {
            return !TypedAsset.Animation.Name.empty() ? TypedAsset.Animation.Name : "Animation";
        }
        return std::string("Asset");
    }, Asset);
}

[[nodiscard]] std::string BuildImportedSourceAssetLeaf(
    const std::string& GroupLeaf,
    const ImportedSourceDocument& Document)
{
    const ImportedNameParts Parts = ParseImportedNameParts(Document.OriginalLogicalName);
    const std::string Label = SanitizeImportedLeaf(
        Parts.Leaf.empty() ? FallbackLeafForImportedSourceAsset(Document.Asset) : Parts.Leaf,
        "Asset");

    return std::visit([&GroupLeaf, &Label](const auto& TypedAsset) -> std::string {
        using TAsset = std::remove_cvref_t<decltype(TypedAsset)>;
        if constexpr (std::is_same_v<TAsset, StaticMeshAsset> || std::is_same_v<TAsset, SkeletalMeshAsset>)
        {
            return GroupLeaf;
        }
        if constexpr (std::is_same_v<TAsset, TextureAsset>)
        {
            return GroupLeaf + "__texture__" + Label;
        }
        if constexpr (std::is_same_v<TAsset, MaterialAsset>)
        {
            return GroupLeaf + "__material__" + Label;
        }
        if constexpr (std::is_same_v<TAsset, MaterialInstanceAsset>)
        {
            return GroupLeaf + "__materialinstance__" + Label;
        }
        if constexpr (std::is_same_v<TAsset, SkeletonAsset>)
        {
            return GroupLeaf + "__skeleton";
        }
        if constexpr (std::is_same_v<TAsset, SkeletalAnimationAsset>)
        {
            return GroupLeaf + "__skeletalanim__" + Label;
        }
        return GroupLeaf + "__asset__" + Label;
    }, Document.Asset);
}

[[nodiscard]] int ImportedPrimarySelectionPriority(const ImportedSourceAssetVariant& Asset)
{
    return std::visit([](const auto& TypedAsset) -> int {
        using TAsset = std::remove_cvref_t<decltype(TypedAsset)>;
        if constexpr (std::is_same_v<TAsset, StaticMeshAsset> || std::is_same_v<TAsset, SkeletalMeshAsset>)
        {
            return 3;
        }
        if constexpr (std::is_same_v<TAsset, TextureAsset>)
        {
            return 2;
        }
        return 1;
    }, Asset);
}

[[nodiscard]] bool IsExistingImportGroupFolder(const std::filesystem::path& AssetRoot,
                                               const std::string& LogicalFolder,
                                               const std::string& SourceStem)
{
    if (LogicalFolder.empty())
    {
        return false;
    }

    const std::filesystem::path FolderPath = AssetRoot / std::filesystem::path(LogicalFolder);
    std::error_code Error{};
    if (!std::filesystem::exists(FolderPath, Error) || Error)
    {
        return false;
    }

    const std::string FolderLeaf = LeafLogicalName(LogicalFolder);
    const std::string StemLeaf = SanitizeImportedLeaf(SourceStem, "Asset");
    return FolderLeaf == StemLeaf || FolderLeaf.rfind(StemLeaf + "_", 0u) == 0u;
}

[[nodiscard]] std::string ResolveImportGroupFolder(const std::filesystem::path& AssetRoot,
                                                   const std::string& DestinationFolder,
                                                   const std::string& SourceStem)
{
    const std::string NormalizedDestination = NormalizeAssetLogicalName(DestinationFolder);
    if (IsExistingImportGroupFolder(AssetRoot, NormalizedDestination, SourceStem))
    {
        return NormalizedDestination;
    }

    const std::string GroupLeaf = SanitizeImportedLeaf(SourceStem, "Asset");
    std::string Candidate = NormalizedDestination.empty()
        ? GroupLeaf
        : NormalizeAssetLogicalName(NormalizedDestination + "/" + GroupLeaf);
    std::size_t SuffixIndex = 1u;
    while (!Candidate.empty())
    {
        std::error_code ExistsError{};
        if (!std::filesystem::exists(AssetRoot / std::filesystem::path(Candidate), ExistsError) || ExistsError)
        {
            break;
        }

        Candidate = NormalizedDestination.empty()
            ? (GroupLeaf + "_" + std::to_string(SuffixIndex++))
            : NormalizeAssetLogicalName(NormalizedDestination + "/" + GroupLeaf + "_" + std::to_string(SuffixIndex++));
    }
    return Candidate;
}
} // namespace

std::string_view EditorAssetService::Name() const
{
    return "EditorAssetService";
}

Result EditorAssetService::Initialize(EditorServiceContext& Context)
{
    m_assets.clear();
    m_assetIndexByKey.clear();
    m_assetRenameOverrides.clear();
    m_assetPayloadOverrides.clear();
    m_assetImportMetadata.clear();
    m_assetImportMetadataPath.clear();
    m_assetImportMetadataDirty = false;
    m_selectedAssetKey.clear();
    m_placementAssetKey.clear();
    m_previewSummary.clear();
    m_statusMessage.clear();
    m_editorTemplateAssetDirectory.clear();
    m_editorStarterLevelTemplateAssetPath.clear();
    m_editorStarterScriptTemplatePath.clear();
    m_currentProject = {};
    m_loadedDefaultRenderSettingsNode = {};
    m_defaultRenderSettingsApplyPending = false;
    m_defaultRenderSettingsLastPassGraphRevision = 0;
    ClearAssetEditorState();

    if (const std::filesystem::path ExistingAssetRoot = SPathResolver::Instance().AssetRoot();
        !ExistingAssetRoot.empty())
    {
        m_currentProject.AssetRootDirectory = ExistingAssetRoot.string();
    }

    std::string TemplateError{};
    if (Result TemplateResult = EnsureEditorTemplateAssets(Context); !TemplateResult)
    {
        TemplateError = TemplateResult.error().Message;
    }
    else
    {
        const std::filesystem::path EditorRootDirectory = m_editorTemplateAssetDirectory.parent_path();
        if (!EditorRootDirectory.empty())
        {
            if (Result SetEditorRootResult = SPathResolver::Instance().SetEditorRoot(EditorRootDirectory); !SetEditorRootResult)
            {
                if (!TemplateError.empty())
                {
                    TemplateError += " ";
                }
                TemplateError += "Failed to set editor:// root: " + SetEditorRootResult.error().Message;
            }
        }

        if (!m_currentProject.IsLoaded && !m_editorTemplateAssetDirectory.empty())
        {
            // In editor mode without an open project, default asset:// to editor templates under appdata.
            if (Result SetRootResult = SPathResolver::Instance().SetAssetRoot(m_editorTemplateAssetDirectory); !SetRootResult)
            {
                if (!TemplateError.empty())
                {
                    TemplateError += " ";
                }
                TemplateError += "Failed to set editor template asset root: " + SetRootResult.error().Message;
            }
            else
            {
                m_currentProject.AssetRootDirectory = m_editorTemplateAssetDirectory.string();
            }
        }
    }

    if (Result RebuildResult = RebuildAssetManager(); !RebuildResult)
    {
        return RebuildResult;
    }

    if (!TemplateError.empty())
    {
        std::fprintf(stdout, "Warning: Editor template bootstrap failed: %s\n", TemplateError.c_str());
        std::fflush(stdout);
        if (!m_statusMessage.empty())
        {
            m_statusMessage += ' ';
        }
        m_statusMessage += "Editor template bootstrap failed: " + TemplateError;
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    ConfigureRendererShaderSearchRootForAssetRoot(Context.Runtime(), SPathResolver::Instance().AssetRoot());
#endif

    return Ok();
}

void EditorAssetService::Tick(EditorServiceContext& Context, float DeltaSeconds)
{
    MaybeReportStatusMessageToStdout();
#if defined(SNAPI_GF_ENABLE_RENDERER)
    (void)DeltaSeconds;
    auto* RuntimeWorld = Context.Runtime().WorldPtr();
    if (!RuntimeWorld)
    {
        return;
    }

    const std::string DefaultSettingsAssetId = TrimCopy(m_currentProject.DefaultRenderSettingsAssetId);
    if (DefaultSettingsAssetId.empty())
    {
        return;
    }

    WorldRenderSettingsRootSet ExistingRoots = CollectWorldRenderSettingsRoots(*RuntimeWorld);
    if (!ExistingRoots.AuthoredRoots.empty())
    {
        DestroyNodes(*RuntimeWorld, ExistingRoots.TransientRoots);
        m_loadedDefaultRenderSettingsNode = {};
        m_defaultRenderSettingsApplyPending = false;
        return;
    }

    if (!ExistingRoots.TransientRoots.empty())
    {
        m_loadedDefaultRenderSettingsNode = ExistingRoots.TransientRoots.front();
        for (std::size_t Index = 1; Index < ExistingRoots.TransientRoots.size(); ++Index)
        {
            NodeHandle Duplicate = ExistingRoots.TransientRoots[Index];
            (void)RuntimeWorld->DestroyNode(Duplicate);
        }
    }

    BaseNode* LoadedNode = nullptr;
    if (!m_loadedDefaultRenderSettingsNode.IsNull())
    {
        LoadedNode = RuntimeWorld->BorrowedNode(m_loadedDefaultRenderSettingsNode);
        if (!LoadedNode)
        {
            if (auto HandleResult = RuntimeWorld->NodeHandleById(m_loadedDefaultRenderSettingsNode.Id); HandleResult)
            {
                m_loadedDefaultRenderSettingsNode = *HandleResult;
                LoadedNode = RuntimeWorld->BorrowedNode(m_loadedDefaultRenderSettingsNode);
            }
            else
            {
                m_loadedDefaultRenderSettingsNode = {};
            }
        }
    }

    if (!LoadedNode)
    {
        (void)LoadProjectDefaultRenderSettings(Context);
        LoadedNode = RuntimeWorld->BorrowedNode(m_loadedDefaultRenderSettingsNode);
        if (!LoadedNode)
        {
            return;
        }
    }

    const std::uint64_t PassGraphRevision = RuntimeWorld->Renderer().RenderViewportPassGraphRevision();
    const bool ShouldReapply = m_defaultRenderSettingsApplyPending
                               || (PassGraphRevision != m_defaultRenderSettingsLastPassGraphRevision);
    if (!ShouldReapply)
    {
        return;
    }

    m_defaultRenderSettingsLastPassGraphRevision = PassGraphRevision;
    if (NodeCast<WorldRenderSettings>(LoadedNode) != nullptr)
    {
        if (const Result RequestResult = RuntimeWorld->RequestNodeOnCreate(m_loadedDefaultRenderSettingsNode); RequestResult)
        {
            m_defaultRenderSettingsApplyPending = false;
        }
    }
    else
    {
        m_loadedDefaultRenderSettingsNode = {};
    }
#else
    (void)Context;
    (void)DeltaSeconds;
#endif
}

void EditorAssetService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    ClearDefaultAssetManagerResolver();
    m_assetManager.reset();
    m_assets.clear();
    m_assetIndexByKey.clear();
    m_assetRenameOverrides.clear();
    m_assetPayloadOverrides.clear();
    m_assetImportMetadata.clear();
    m_assetImportMetadataPath.clear();
    m_assetImportMetadataDirty = false;
    m_selectedAssetKey.clear();
    m_placementAssetKey.clear();
    m_previewSummary.clear();
    m_statusMessage.clear();
    m_editorTemplateAssetDirectory.clear();
    m_editorStarterLevelTemplateAssetPath.clear();
    m_editorStarterScriptTemplatePath.clear();
    m_currentProject = {};
    m_loadedDefaultRenderSettingsNode = {};
    m_defaultRenderSettingsApplyPending = false;
    m_defaultRenderSettingsLastPassGraphRevision = 0;
    ClearAssetEditorState();
}

const EditorAssetService::DiscoveredAsset* EditorAssetService::SelectedAsset() const
{
    if (m_selectedAssetKey.empty())
    {
        return nullptr;
    }
    return FindAssetByKey(m_selectedAssetKey);
}

const std::string& EditorAssetService::StatusMessage() const
{
    MaybeReportStatusMessageToStdout();
    return m_statusMessage;
}

void EditorAssetService::MaybeReportStatusMessageToStdout() const
{
#if defined(WITH_EDITOR) && WITH_EDITOR
    if (m_statusMessage.empty())
    {
        m_lastReportedStatusMessage.clear();
        return;
    }

    if (m_statusMessage == m_lastReportedStatusMessage)
    {
        return;
    }

    const std::string LowerMessage = ToLowerCopy(m_statusMessage);
    const bool IsErrorLike = LowerMessage.find("error") != std::string::npos ||
                             LowerMessage.find("failed") != std::string::npos ||
                             LowerMessage.find("warning") != std::string::npos ||
                             LowerMessage.find("missing") != std::string::npos ||
                             LowerMessage.find("not found") != std::string::npos ||
                             LowerMessage.find("unable") != std::string::npos ||
                             LowerMessage.find("could not") != std::string::npos ||
                             LowerMessage.find("invalid") != std::string::npos;

    m_lastReportedStatusMessage = m_statusMessage;
    if (!IsErrorLike)
    {
        return;
    }

    std::fprintf(stdout, "[SnAPI][EditorStatus] %s\n", m_statusMessage.c_str());
    std::fflush(stdout);
#endif
}

bool EditorAssetService::SelectAssetByKey(const std::string_view Key)
{
    const auto* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return false;
    }

    m_selectedAssetKey = Asset->Key;
    if (!m_previewSummary.empty())
    {
        m_statusMessage = m_previewSummary;
    }
    else
    {
        m_statusMessage = "Selected asset: " + Asset->Name;
    }

    return true;
}

Result EditorAssetService::ArmPlacementByKey(const std::string_view Key)
{
    const auto* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found for placement"));
    }

    if (Asset->AssetKind != AssetKindNode() &&
        Asset->AssetKind != AssetKindLevel() &&
        Asset->AssetKind != AssetKindWorld())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Selected asset kind cannot be placed in scene"));
    }

    m_selectedAssetKey = Asset->Key;
    m_placementAssetKey = Asset->Key;
    m_statusMessage = "Placement armed: " + Asset->Name + ". Click inside the viewport to instantiate.";
    return Ok();
}

void EditorAssetService::ClearPlacement()
{
    m_placementAssetKey.clear();
}

Result EditorAssetService::RefreshDiscovery()
{
    AuthoredAssetRegistry::Instance().EnsureBuilt();

    const std::filesystem::path AssetRoot = ResolveImportAssetRootDirectory(m_currentProject);
    std::vector<DiscoveredAsset> NextAssets{};
    std::unordered_set<std::string> SeenKeys{};
    std::unordered_set<::SnAPI::AssetPipeline::AssetId, ::SnAPI::AssetPipeline::UuidHash> SeenAssetIds{};
    std::string MetadataWarning{};
    std::size_t DirtyAssetCount = 0;

    std::error_code RootError{};
    if (!AssetRoot.empty() && std::filesystem::exists(AssetRoot, RootError))
    {
        std::filesystem::recursive_directory_iterator It(
            AssetRoot,
            std::filesystem::directory_options::skip_permission_denied,
            RootError);
        const std::filesystem::recursive_directory_iterator End{};
        for (; !RootError && It != End; It.increment(RootError))
        {
            const std::filesystem::directory_entry& EntryRef = *It;
            if (!EntryRef.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path SourcePath = EntryRef.path().lexically_normal();
            const std::string SourcePathText = SourcePath.string();
            if (SourcePathText.find(std::string(kAssetImportMetadataDirectoryName)) != std::string::npos)
            {
                continue;
            }
            if (IsCookedPackFile(SourcePath))
            {
                continue;
            }

            const std::string LogicalName = BuildSourceLogicalName(AssetRoot, SourcePath);
            if (LogicalName.empty() || !SeenKeys.insert(LogicalName).second)
            {
                continue;
            }

            const ::SnAPI::AssetPipeline::AssetId AssetId = MakeDeterministicSourceAssetId(LogicalName);
            if (!SeenAssetIds.insert(AssetId).second)
            {
                continue;
            }

            const std::string Extension = NormalizeAssetExtension(SourcePath.extension().string());
            const AuthoredAssetDescriptor* Descriptor = AuthoredAssetRegistry::Instance().FindByExtension(Extension);

            DiscoveredAsset Asset{};
            Asset.Key = LogicalName;
            Asset.Name = LogicalName;
            Asset.TypeLabel = Descriptor ? Descriptor->DisplayName : (Extension.empty() ? "Source File" : Extension + " Source");
            Asset.Variant.clear();
            Asset.AssetId = AssetId;
            Asset.AssetType = Descriptor ? Descriptor->AssetType : TypeId{};
            Asset.AssetKind = Descriptor ? Descriptor->CookedAssetKind : ::SnAPI::AssetPipeline::TypeId{};
            Asset.CookedPayloadType = Descriptor ? Descriptor->CookedPayloadType : ::SnAPI::AssetPipeline::TypeId{};
            Asset.SchemaVersion = 0;
            Asset.IsRuntime = false;
            Asset.IsDirty = (!m_assetEditorAssetKey.empty() &&
                             m_assetEditorAssetKey == LogicalName &&
                             m_assetEditorSourceAssetType != TypeId{} &&
                             m_assetEditorDirty);
            Asset.CanSave = Descriptor ? Descriptor->CanSave : false;
            Asset.OwningPackPath.clear();
            Asset.SourceFilePath = SourcePathText;

            if (!Descriptor)
            {
                if (const auto MetadataIt = m_assetImportMetadata.find(Asset.AssetId);
                    MetadataIt != m_assetImportMetadata.end())
                {
                    switch (MetadataIt->second.Profile)
                    {
                    case EImportProfile::Texture:
                        Asset.TypeLabel = "Texture Source";
                        break;
                    case EImportProfile::AssimpModel:
                        Asset.TypeLabel = "Model Source";
                        break;
                    case EImportProfile::Unknown:
                    default:
                        break;
                    }
                }
            }

            if (Asset.IsDirty)
            {
                ++DirtyAssetCount;
            }

            NextAssets.push_back(std::move(Asset));
        }
    }

    m_assetRenameOverrides.clear();
    m_assetPayloadOverrides.clear();

    bool MetadataPruned = false;
    for (auto It = m_assetImportMetadata.begin(); It != m_assetImportMetadata.end();)
    {
        if (SeenAssetIds.contains(It->first))
        {
            ++It;
            continue;
        }

        It = m_assetImportMetadata.erase(It);
        MetadataPruned = true;
    }
    if (MetadataPruned)
    {
        m_assetImportMetadataDirty = true;
        if (auto SaveResult = SaveAssetImportMetadataDatabase(); SaveResult)
        {
            m_assetImportMetadataDirty = false;
        }
        else
        {
            MetadataWarning = "Import metadata prune warning: " + SaveResult.error();
        }
    }

    std::sort(NextAssets.begin(), NextAssets.end(), [](const DiscoveredAsset& Left, const DiscoveredAsset& Right) {
        if (Left.IsDirty != Right.IsDirty)
        {
            return Left.IsDirty && !Right.IsDirty;
        }
        if (Left.Name != Right.Name)
        {
            return Left.Name < Right.Name;
        }
        if (Left.TypeLabel != Right.TypeLabel)
        {
            return Left.TypeLabel < Right.TypeLabel;
        }
        return Left.Key < Right.Key;
    });

    m_assets = std::move(NextAssets);
    m_assetIndexByKey.clear();
    m_assetIndexByKey.reserve(m_assets.size());
    for (std::size_t Index = 0; Index < m_assets.size(); ++Index)
    {
        m_assetIndexByKey[m_assets[Index].Key] = Index;
    }

    if (!m_selectedAssetKey.empty() && !m_assetIndexByKey.contains(m_selectedAssetKey))
    {
        m_selectedAssetKey.clear();
    }
    if (!m_placementAssetKey.empty() && !m_assetIndexByKey.contains(m_placementAssetKey))
    {
        m_placementAssetKey.clear();
    }
    if (!m_assetEditorAssetKey.empty() && !m_assetIndexByKey.contains(m_assetEditorAssetKey))
    {
        ClearAssetEditorState();
    }

    std::ostringstream Message;
    Message << "Discovered " << m_assets.size() << " source asset(s)";
    if (!AssetRoot.empty())
    {
        Message << " under " << AssetRoot.string();
    }
    Message << ", " << DirtyAssetCount << " unsaved.";
    if (!MetadataWarning.empty())
    {
        Message << ' ' << MetadataWarning;
    }
    m_statusMessage = Message.str();
    return Ok();
}

Result EditorAssetService::OpenSelectedAssetPreview()
{
    const DiscoveredAsset* Asset = SelectedAsset();
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No selected asset to preview"));
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    std::ostringstream Summary;
    Summary << Asset->TypeLabel << " '" << Asset->Name << "'";

    if (Asset->AssetKind == AssetKindNode())
    {
        World PreviewWorld("EditorNodePreview");
        NodeAssetLoadParams LoadParams{};
        LoadParams.TargetWorld = &PreviewWorld;
        auto NodeResult = Asset->SourceFilePath.empty()
            ? m_assetManager->Load<BaseNode>(Asset->AssetId, LoadParams)
            : m_assetManager->Load<BaseNode>(Asset->Name, LoadParams);
        if (!NodeResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, NodeResult.error()));
        }

        const RuntimeWorldCounts Counts = CountRuntimeWorldObjects(PreviewWorld);
        Summary << " preview loaded (" << Counts.Nodes << " nodes, " << Counts.Components << " components).";
    }
    else if (Asset->AssetKind == AssetKindLevel())
    {
        World PreviewWorld("EditorLevelPreview");
        LevelAssetLoadParams LoadParams{};
        LoadParams.TargetWorld = &PreviewWorld;
        auto LevelResult = Asset->SourceFilePath.empty()
            ? m_assetManager->Load<Level>(Asset->AssetId, LoadParams)
            : m_assetManager->Load<Level>(Asset->Name, LoadParams);
        if (!LevelResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, LevelResult.error()));
        }

        const RuntimeWorldCounts Counts = CountRuntimeWorldObjects(PreviewWorld);
        Summary << " preview loaded (" << Counts.Nodes << " nodes, " << Counts.Components << " components).";
    }
    else if (Asset->AssetKind == AssetKindWorld())
    {
        auto WorldResult = Asset->SourceFilePath.empty()
            ? m_assetManager->Load<World>(Asset->AssetId)
            : m_assetManager->Load<World>(Asset->Name);
        if (!WorldResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, WorldResult.error()));
        }

        const RuntimeWorldCounts Counts = CountRuntimeWorldObjects(*WorldResult.value());
        Summary << " preview loaded (" << Counts.Nodes << " nodes, " << Counts.Components << " components).";
    }
    else
    {
        Summary << " preview is not implemented for this asset kind.";
    }

    m_previewSummary = Summary.str();
    m_statusMessage = m_previewSummary;
    return Ok();
}

Result EditorAssetService::SaveSelectedAssetUpdate()
{
    if (m_selectedAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No selected asset to save"));
    }

    return SaveAssetByKey(m_selectedAssetKey);
}

Result EditorAssetService::SaveSelectedAssetUpdate(EditorServiceContext& Context)
{
    if (m_selectedAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No selected asset to save"));
    }

    return SaveAssetByKey(Context, m_selectedAssetKey);
}

Result EditorAssetService::SaveAssetByKey(const std::string_view Key)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found"));
    }

    if (!Asset->SourceFilePath.empty() && Asset->AssetType != TypeId{})
    {
        const std::string AssetName = Asset->Name;

        if (m_assetEditorAssetKey == Asset->Key)
        {
            ApplyAssetEditorImportSettingsToActiveSourceAsset();
        }

        if (!Asset->CanSave)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Selected source asset cannot be saved"));
        }

        if (Asset->AssetKind == AssetKindNode() ||
            Asset->AssetKind == AssetKindLevel() ||
            Asset->AssetKind == AssetKindWorld())
        {
            if (m_assetEditorAssetKey != Asset->Key)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "Open the source asset in the inspector before saving hierarchy changes"));
            }

            std::expected<std::string, Error> SourceJson{};
            if (Asset->AssetKind == AssetKindNode())
            {
                BaseNode* RootNode = ResolveAssetEditorNode(m_assetEditorRootHandle);
                if (!RootNode)
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                     "Opened prefab source asset has no loaded root node"));
                }

                auto AssetResult = CaptureNodeAsset(*RootNode);
                if (!AssetResult)
                {
                    return std::unexpected(AssetResult.error());
                }
                SourceJson = SerializeAuthoredAssetToJson(*AssetResult);
            }
            else if (Asset->AssetKind == AssetKindLevel())
            {
                auto* LevelNode = NodeCast<Level>(ResolveAssetEditorNode(m_assetEditorRootHandle));
                if (!LevelNode)
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                     "Opened level source asset has no loaded level root"));
                }

                auto AssetResult = CaptureLevelAsset(*LevelNode);
                if (!AssetResult)
                {
                    return std::unexpected(AssetResult.error());
                }
                SourceJson = SerializeAuthoredAssetToJson(*AssetResult);
            }
            else
            {
                if (!m_assetEditorWorld)
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                     "Opened world source asset has no loaded world"));
                }

                auto AssetResult = CaptureWorldAsset(*m_assetEditorWorld);
                if (!AssetResult)
                {
                    return std::unexpected(AssetResult.error());
                }
                SourceJson = SerializeAuthoredAssetToJson(*AssetResult);
            }

            if (!SourceJson)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, SourceJson.error().Message));
            }

            const std::filesystem::path SourcePath = std::filesystem::path(Asset->SourceFilePath).lexically_normal();
            std::error_code DirectoryError{};
            std::filesystem::create_directories(SourcePath.parent_path(), DirectoryError);
            if (DirectoryError)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError,
                                                 "Failed to create source-asset parent directory: " +
                                                     DirectoryError.message()));
            }

            std::ofstream File(SourcePath, std::ios::binary | std::ios::trunc);
            if (!File.is_open())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError,
                                                 "Failed to open source asset for save: " + SourcePath.string()));
            }

            File.write(SourceJson->data(), static_cast<std::streamsize>(SourceJson->size()));
            if (!File.good())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError,
                                                 "Failed to write source asset: " + SourcePath.string()));
            }

            if (m_assetEditorAssetKey == Asset->Key &&
                m_assetEditorImportSettingsObject != nullptr &&
                m_assetEditorImportSettingsType != TypeId{})
            {
                if (auto CurrentImportMetadata = BuildAssetEditorImportMetadataFromCurrentState(); CurrentImportMetadata.has_value())
                {
                    m_assetImportMetadata[Asset->AssetId] = *CurrentImportMetadata;
                    if (auto SyncResult = SyncImportedAssetGroupProvenance(*Asset, *CurrentImportMetadata); !SyncResult)
                    {
                        return std::unexpected(MakeError(EErrorCode::InternalError, SyncResult.error().Message));
                    }
                    m_assetImportMetadataDirty = true;
                    if (auto SaveMetadataResult = SaveAssetImportMetadataDatabase(); !SaveMetadataResult)
                    {
                        return std::unexpected(MakeError(EErrorCode::InternalError, SaveMetadataResult.error()));
                    }
                    m_assetImportMetadataDirty = false;
                    m_assetEditorImportMetadataBaseline = *CurrentImportMetadata;
                    MarkAssetEditorImportSettingsSaved();
                }
            }
            MarkAssetEditorRuntimeSaved();
            RefreshAssetEditorDirtyFlags(false);

            auto RebuildResult = RebuildAssetManager();
            if (!RebuildResult)
            {
                return RebuildResult;
            }

            m_statusMessage = "Saved source asset: " + AssetName;
            return Ok();
        }

        auto SerializeResult = SerializeAssetEditorSourceJson();
        if (!SerializeResult)
        {
            if (m_assetEditorAssetKey != Asset->Key || m_assetEditorSourceAssetType != Asset->AssetType)
            {
                return Ok();
            }
            return std::unexpected(MakeError(EErrorCode::InternalError, SerializeResult.error()));
        }

        const std::filesystem::path SourcePath = std::filesystem::path(Asset->SourceFilePath).lexically_normal();
        std::error_code DirectoryError{};
        std::filesystem::create_directories(SourcePath.parent_path(), DirectoryError);
        if (DirectoryError)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to create source-asset parent directory: " +
                                                 DirectoryError.message()));
        }

        std::ofstream File(SourcePath, std::ios::binary | std::ios::trunc);
        if (!File.is_open())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to open source asset for save: " + SourcePath.string()));
        }

        File.write(SerializeResult->data(), static_cast<std::streamsize>(SerializeResult->size()));
        if (!File.good())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to write source asset: " + SourcePath.string()));
        }

        if (m_assetEditorAssetKey == Asset->Key)
        {
            if (m_assetEditorImportSettingsObject != nullptr && m_assetEditorImportSettingsType != TypeId{})
            {
                if (auto CurrentImportMetadata = BuildAssetEditorImportMetadataFromCurrentState(); CurrentImportMetadata.has_value())
                {
                    m_assetImportMetadata[Asset->AssetId] = *CurrentImportMetadata;
                    if (auto SyncResult = SyncImportedAssetGroupProvenance(*Asset, *CurrentImportMetadata); !SyncResult)
                    {
                        return std::unexpected(MakeError(EErrorCode::InternalError, SyncResult.error().Message));
                    }
                    m_assetImportMetadataDirty = true;
                    if (auto SaveMetadataResult = SaveAssetImportMetadataDatabase(); !SaveMetadataResult)
                    {
                        return std::unexpected(MakeError(EErrorCode::InternalError, SaveMetadataResult.error()));
                    }
                    m_assetImportMetadataDirty = false;
                    m_assetEditorImportMetadataBaseline = *CurrentImportMetadata;
                    MarkAssetEditorImportSettingsSaved();
                }
            }
            MarkAssetEditorRuntimeSaved();
            RefreshAssetEditorDirtyFlags(false);
        }

        auto RebuildResult = RebuildAssetManager();
        if (!RebuildResult)
        {
            return RebuildResult;
        }

        m_statusMessage = "Saved source asset: " + AssetName;
        return Ok();
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    const std::string AssetKeySnapshot = Asset->Key;

    if (Asset->IsRuntime)
    {
        if (m_assetEditorAssetKey == Asset->Key && m_assetEditorRuntimeDirty)
        {
            if (auto SyncResult = SyncAssetEditorRuntimeOverrideFromCurrentState(); !SyncResult)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, SyncResult.error()));
            }
        }

        if (const auto PayloadOverrideIt = m_assetPayloadOverrides.find(Asset->AssetId);
            PayloadOverrideIt != m_assetPayloadOverrides.end())
        {
            ::SnAPI::AssetPipeline::RuntimeAssetUpsert RuntimeAsset{};
            RuntimeAsset.Id = Asset->AssetId;
            RuntimeAsset.Name = Asset->Name;
            RuntimeAsset.AssetKind = Asset->AssetKind;
            RuntimeAsset.Cooked = PayloadOverrideIt->second;
            RuntimeAsset.Bulk.clear();
            RuntimeAsset.Dirty = true;

            auto UpsertResult = m_assetManager->UpsertRuntimeAsset(std::move(RuntimeAsset));
            if (!UpsertResult)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, UpsertResult.error()));
            }
        }

        auto SavePathResult = ResolveRuntimeSavePath(*Asset);
        if (!SavePathResult)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, SavePathResult.error()));
        }

        auto SaveResult = m_assetManager->SaveRuntimeAsset(Asset->AssetId, SavePathResult.value());
        if (!SaveResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, SaveResult.error()));
        }

        m_assetManager->ClearCache();

        m_assetRenameOverrides.erase(Asset->AssetId);
        m_assetPayloadOverrides.erase(Asset->AssetId);
        auto RefreshResult = RefreshDiscovery();
        if (!RefreshResult)
        {
            return RefreshResult;
        }

        if (m_assetEditorAssetKey == AssetKeySnapshot)
        {
            MarkAssetEditorRuntimeSaved();
            RefreshAssetEditorDirtyFlags(false);
        }

        m_statusMessage = "Saved runtime asset to pack: " + SavePathResult.value();
        return Ok();
    }

    auto PackPathResult = ResolveOwningPackPath(*Asset);
    if (!PackPathResult)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, PackPathResult.error()));
    }

    if (m_assetEditorAssetKey == Asset->Key && m_assetEditorRuntimeDirty)
    {
        if (auto SyncResult = SyncAssetEditorRuntimeOverrideFromCurrentState(); !SyncResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, SyncResult.error()));
        }
    }

    ::SnAPI::AssetPipeline::TypedPayload CookedPayload{};
    if (const auto PayloadOverrideIt = m_assetPayloadOverrides.find(Asset->AssetId);
        PayloadOverrideIt != m_assetPayloadOverrides.end())
    {
        CookedPayload = PayloadOverrideIt->second;
    }
    else
    {
        auto CookedPayloadResult = BuildCookedPayloadForAsset(*Asset);
        if (!CookedPayloadResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, CookedPayloadResult.error()));
        }
        CookedPayload = std::move(CookedPayloadResult.value());
    }

    ::SnAPI::AssetPipeline::AssetPackEntry Entry{};
    Entry.Id = Asset->AssetId;
    Entry.AssetKind = Asset->AssetKind;
    Entry.Name = Asset->Name;
    Entry.VariantKey = Asset->Variant;
    Entry.Cooked = std::move(CookedPayload);

    ::SnAPI::AssetPipeline::AssetPackWriter Writer{};
    Writer.AddAsset(std::move(Entry));
    auto WriteResult = Writer.AppendUpdate(PackPathResult.value());
    if (!WriteResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, WriteResult.error()));
    }

    // Refresh mounted pack readers so immediate reopen reflects freshly written payloads.
    const bool WasHotReloadEnabled = m_assetManager->IsHotReloadEnabled();
    if (!WasHotReloadEnabled)
    {
        m_assetManager->SetHotReloadEnabled(true);
    }
    const std::vector<std::string> ReloadedPacks = m_assetManager->CheckForChanges();
    if (!WasHotReloadEnabled)
    {
        m_assetManager->SetHotReloadEnabled(false);
    }

    const bool PackReloaded = std::ranges::any_of(ReloadedPacks, [&PackPathResult](const std::string& Path) {
        return Path == PackPathResult.value();
    });
    if (!PackReloaded)
    {
        m_assetManager->UnmountPack(PackPathResult.value());
        auto RemountResult = m_assetManager->MountPack(PackPathResult.value());
        if (!RemountResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, RemountResult.error()));
        }
    }
    m_assetManager->ClearCache();

    m_assetRenameOverrides.erase(Asset->AssetId);
    m_assetPayloadOverrides.erase(Asset->AssetId);
    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    if (m_assetEditorAssetKey == AssetKeySnapshot)
    {
        MarkAssetEditorRuntimeSaved();
        RefreshAssetEditorDirtyFlags(false);
    }

    m_statusMessage = "Saved asset update into pack: " + PackPathResult.value();
    return Ok();
}

Result EditorAssetService::SaveAssetByKey(EditorServiceContext& Context, const std::string_view Key)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found"));
    }

    if (!Asset->SourceFilePath.empty() && Asset->AssetType != TypeId{})
    {
        const std::string AssetName = Asset->Name;

        if (Asset->AssetKind != AssetKindConduitGraph() && Asset->AssetKind != AssetKindConduitClass())
        {
            return SaveAssetByKey(Key);
        }

        auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
        if (!ConduitService)
        {
            return SaveAssetByKey(Key);
        }

        const Conduit::GraphAsset* GraphAsset = nullptr;
        const Conduit::ClassAsset* ClassAsset = nullptr;
        if (const auto* GraphDocument = ConduitService->FindDocument(Key))
        {
            GraphAsset = &GraphDocument->Asset();
        }
        else if (const auto* ClassDocument = ConduitService->FindClassDocument(Key))
        {
            ClassAsset = &ClassDocument->Asset();
        }

        if (!GraphAsset && !ClassAsset)
        {
            return SaveAssetByKey(Key);
        }

        std::expected<std::string, Error> JsonResult = GraphAsset
            ? SerializeAuthoredAssetToJson(*GraphAsset)
            : SerializeAuthoredAssetToJson(*ClassAsset);
        if (!JsonResult)
        {
            return std::unexpected(JsonResult.error());
        }

        const std::filesystem::path SourcePath = std::filesystem::path(Asset->SourceFilePath).lexically_normal();
        std::error_code DirectoryError{};
        std::filesystem::create_directories(SourcePath.parent_path(), DirectoryError);
        if (DirectoryError)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to create source-asset parent directory: " +
                                                 DirectoryError.message()));
        }

        std::ofstream File(SourcePath, std::ios::binary | std::ios::trunc);
        if (!File.is_open())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to open source asset for save: " + SourcePath.string()));
        }

        File.write(JsonResult->data(), static_cast<std::streamsize>(JsonResult->size()));
        if (!File.good())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to write source asset: " + SourcePath.string()));
        }

        if (auto* GraphDocument = ConduitService->FindDocument(Key))
        {
            GraphDocument->MarkSaved();
        }
        if (auto* ClassDocument = ConduitService->FindClassDocument(Key))
        {
            ClassDocument->MarkSaved();
        }

        auto RebuildResult = RebuildAssetManager();
        if (!RebuildResult)
        {
            return RebuildResult;
        }

        m_statusMessage = Asset->AssetKind == AssetKindConduitGraph()
            ? "Saved Conduit graph: " + AssetName
            : "Saved Conduit class: " + AssetName;
        return Ok();
    }

    if (Asset->AssetKind != AssetKindConduitGraph() && Asset->AssetKind != AssetKindConduitClass())
    {
        return SaveAssetByKey(Key);
    }

    auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
    if (!ConduitService)
    {
        return SaveAssetByKey(Key);
    }

    if (Asset->AssetKind == AssetKindConduitGraph())
    {
        Conduit::Editor::GraphDocument* Document = ConduitService->FindDocument(Key);
        if (!Document)
        {
            return SaveAssetByKey(Key);
        }

        std::vector<std::uint8_t> Bytes{};
        auto SerializeResult = Conduit::SerializeGraphAsset(Document->Asset(), Bytes);
        if (!SerializeResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, SerializeResult.error().Message));
        }

        m_assetPayloadOverrides[Asset->AssetId] =
            ::SnAPI::AssetPipeline::TypedPayload(PayloadConduitGraph(), Conduit::GraphAsset::kSchemaVersion, std::move(Bytes));

        auto SaveResult = SaveAssetByKey(Key);
        if (!SaveResult)
        {
            return SaveResult;
        }

        Document->MarkSaved();
        m_statusMessage = "Saved Conduit graph: " + std::string(Asset->Name);
        return Ok();
    }

    Conduit::Editor::ClassDocument* Document = ConduitService->FindClassDocument(Key);
    if (!Document)
    {
        return SaveAssetByKey(Key);
    }

    std::vector<std::uint8_t> Bytes{};
    auto SerializeResult = Conduit::SerializeClassAsset(Document->Asset(), Bytes);
    if (!SerializeResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, SerializeResult.error().Message));
    }

    m_assetPayloadOverrides[Asset->AssetId] =
        ::SnAPI::AssetPipeline::TypedPayload(PayloadConduitClass(), Conduit::ClassAsset::kSchemaVersion, std::move(Bytes));

    auto SaveResult = SaveAssetByKey(Key);
    if (!SaveResult)
    {
        return SaveResult;
    }

    Document->MarkSaved();
    m_statusMessage = "Saved Conduit class: " + std::string(Asset->Name);
    return Ok();
}

Result EditorAssetService::DeleteAssetByKey(const std::string_view Key)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found"));
    }

    if (!Asset->SourceFilePath.empty())
    {
        const DiscoveredAsset AssetSnapshot = *Asset;
        std::error_code Error{};
        const bool Removed = std::filesystem::remove(std::filesystem::path(AssetSnapshot.SourceFilePath), Error);
        if (!Removed || Error)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to delete source asset: " +
                                                 (Error ? Error.message() : std::string("file was not removed"))));
        }

        m_assetImportMetadata.erase(AssetSnapshot.AssetId);
        if (m_selectedAssetKey == AssetSnapshot.Key)
        {
            m_selectedAssetKey.clear();
            m_previewSummary.clear();
        }
        if (m_placementAssetKey == AssetSnapshot.Key)
        {
            m_placementAssetKey.clear();
        }
        if (m_assetEditorAssetKey == AssetSnapshot.Key)
        {
            ClearAssetEditorState();
        }

        auto RefreshResult = RefreshDiscovery();
        if (!RefreshResult)
        {
            return RefreshResult;
        }

        m_statusMessage = "Deleted source asset: " + AssetSnapshot.Name;
        return Ok();
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    const DiscoveredAsset AssetSnapshot = *Asset;
    const std::string DeletedAssetKey = AssetSnapshot.Key;
    const std::string DeletedAssetName = AssetSnapshot.Name.empty() ? DeletedAssetKey : AssetSnapshot.Name;

    if (AssetSnapshot.IsRuntime)
    {
        auto DeleteResult = m_assetManager->DeleteRuntimeAsset(AssetSnapshot.AssetId);
        if (!DeleteResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, DeleteResult.error()));
        }

        m_assetManager->ClearCache();
        m_assetRenameOverrides.erase(AssetSnapshot.AssetId);
        m_assetPayloadOverrides.erase(AssetSnapshot.AssetId);
        if (m_assetImportMetadata.erase(AssetSnapshot.AssetId) > 0)
        {
            m_assetImportMetadataDirty = true;
            if (auto SaveResult = SaveAssetImportMetadataDatabase(); SaveResult)
            {
                m_assetImportMetadataDirty = false;
            }
            else
            {
                if (!m_statusMessage.empty())
                {
                    m_statusMessage += ' ';
                }
                m_statusMessage += "Import metadata update warning: " + SaveResult.error();
            }
        }
        if (m_selectedAssetKey == DeletedAssetKey)
        {
            m_selectedAssetKey.clear();
            m_previewSummary.clear();
        }
        if (m_placementAssetKey == DeletedAssetKey)
        {
            m_placementAssetKey.clear();
        }
        if (m_assetEditorAssetKey == DeletedAssetKey)
        {
            ClearAssetEditorState();
        }

        auto RefreshResult = RefreshDiscovery();
        if (!RefreshResult)
        {
            return RefreshResult;
        }

        m_statusMessage = "Deleted runtime asset: " + DeletedAssetName;
        return Ok();
    }

    auto PackPathResult = ResolveOwningPackPath(AssetSnapshot);
    if (!PackPathResult)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, PackPathResult.error()));
    }

    const std::string PackPath = PackPathResult.value();
    ::SnAPI::AssetPipeline::AssetPackReader Reader{};
    auto OpenResult = Reader.Open(PackPath);
    if (!OpenResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, OpenResult.error()));
    }

    ::SnAPI::AssetPipeline::AssetPackWriter Writer{};
    const uint32_t AssetCount = Reader.GetAssetCount();
    bool Removed = false;
    uint32_t RemainingAssets = 0;
    for (uint32_t Index = 0; Index < AssetCount; ++Index)
    {
        auto InfoResult = Reader.GetAssetInfo(Index);
        if (!InfoResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, InfoResult.error()));
        }

        const ::SnAPI::AssetPipeline::AssetInfo& Info = *InfoResult;
        if (Info.Id == AssetSnapshot.AssetId)
        {
            Removed = true;
            continue;
        }

        auto CookedResult = Reader.LoadCookedPayload(Info.Id);
        if (!CookedResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, CookedResult.error()));
        }

        ::SnAPI::AssetPipeline::AssetPackEntry Entry{};
        Entry.Id = Info.Id;
        Entry.AssetKind = Info.AssetKind;
        Entry.Name = Info.Name;
        Entry.VariantKey = Info.VariantKey;
        Entry.Cooked = std::move(*CookedResult);

        Entry.Bulk.reserve(Info.BulkChunkCount);
        for (uint32_t BulkIndex = 0; BulkIndex < Info.BulkChunkCount; ++BulkIndex)
        {
            auto BulkResult = Reader.LoadBulkChunk(Info.Id, BulkIndex);
            if (!BulkResult)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, BulkResult.error()));
            }

            auto BulkInfoResult = Reader.GetBulkChunkInfo(Info.Id, BulkIndex);
            if (!BulkInfoResult)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, BulkInfoResult.error()));
            }

            ::SnAPI::AssetPipeline::BulkChunk Chunk(BulkInfoResult->Semantic, BulkInfoResult->SubIndex, true);
            Chunk.Bytes = std::move(*BulkResult);
            Entry.Bulk.push_back(std::move(Chunk));
        }

        Writer.AddAsset(std::move(Entry));
        ++RemainingAssets;
    }

    if (!Removed)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found in its owning pack"));
    }

    m_assetManager->UnmountPack(PackPath);
    if (RemainingAssets == 0)
    {
        std::error_code Error{};
        std::filesystem::remove(PackPath, Error);
        if (Error)
        {
            (void)m_assetManager->MountPack(PackPath);
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to delete empty pack '" + PackPath + "': " + Error.message()));
        }
    }
    else
    {
        auto WriteResult = Writer.Write(PackPath);
        if (!WriteResult)
        {
            (void)m_assetManager->MountPack(PackPath);
            return std::unexpected(MakeError(EErrorCode::InternalError, WriteResult.error()));
        }

        auto MountResult = m_assetManager->MountPack(PackPath);
        if (!MountResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, MountResult.error()));
        }
    }

    m_assetManager->ClearCache();
    m_assetRenameOverrides.erase(AssetSnapshot.AssetId);
    m_assetPayloadOverrides.erase(AssetSnapshot.AssetId);
    if (m_assetImportMetadata.erase(AssetSnapshot.AssetId) > 0)
    {
        m_assetImportMetadataDirty = true;
        if (auto SaveResult = SaveAssetImportMetadataDatabase(); SaveResult)
        {
            m_assetImportMetadataDirty = false;
        }
        else
        {
            if (!m_statusMessage.empty())
            {
                m_statusMessage += ' ';
            }
            m_statusMessage += "Import metadata update warning: " + SaveResult.error();
        }
    }
    if (m_selectedAssetKey == DeletedAssetKey)
    {
        m_selectedAssetKey.clear();
        m_previewSummary.clear();
    }
    if (m_placementAssetKey == DeletedAssetKey)
    {
        m_placementAssetKey.clear();
    }
    if (m_assetEditorAssetKey == DeletedAssetKey)
    {
        ClearAssetEditorState();
    }

    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    if (RemainingAssets == 0)
    {
        m_statusMessage = "Deleted asset and removed empty pack: " + DeletedAssetName;
    }
    else
    {
        m_statusMessage = "Deleted asset: " + DeletedAssetName;
    }
    return Ok();
}

Result EditorAssetService::DeleteSelectedAsset()
{
    if (m_selectedAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No selected asset to delete"));
    }
    return DeleteAssetByKey(m_selectedAssetKey);
}

Result EditorAssetService::RenameAssetByKey(const std::string_view Key, const std::string_view NewName)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found"));
    }

    if (!Asset->SourceFilePath.empty())
    {
        const std::filesystem::path OldPath = std::filesystem::path(Asset->SourceFilePath).lexically_normal();
        const std::string CurrentExtension = NormalizeAssetExtension(OldPath.extension().string());
        std::string RequestedLeaf = LeafLogicalName(std::string(NewName));
        RequestedLeaf = RequestedLeaf.empty() ? LeafLogicalName(Asset->Name) : RequestedLeaf;
        if (RequestedLeaf.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Asset name cannot be empty"));
        }

        std::filesystem::path RequestedPath(RequestedLeaf);
        std::string RequestedStem = RequestedPath.stem().string();
        std::string RequestedExtension = NormalizeAssetExtension(RequestedPath.extension().string());
        if (RequestedStem.empty())
        {
            RequestedStem = RequestedLeaf;
        }
        if (!RequestedExtension.empty() && RequestedExtension != CurrentExtension)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Changing source asset file extensions is not supported"));
        }

        const std::filesystem::path AssetRoot = ResolveImportAssetRootDirectory(m_currentProject);
        const std::filesystem::path SourceDirectory = OldPath.parent_path();
        std::error_code RelativeError{};
        const std::filesystem::path RelativeDirectory = std::filesystem::relative(SourceDirectory, AssetRoot, RelativeError);
        const std::string FolderPath = RelativeError ? std::string{} : NormalizeAssetLogicalName(RelativeDirectory.generic_string());
        const std::string NewLogicalName = MakeUniqueSourceLogicalName(AssetRoot, FolderPath, RequestedStem, CurrentExtension);
        if (NewLogicalName.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to build a valid renamed source asset path"));
        }

        const ::SnAPI::AssetPipeline::AssetId OldAssetId = Asset->AssetId;
        const ::SnAPI::AssetPipeline::AssetId NewAssetId = MakeDeterministicSourceAssetId(NewLogicalName);
        const std::filesystem::path NewPath = AssetRoot / std::filesystem::path(NewLogicalName);
        std::error_code DirectoryError{};
        std::filesystem::create_directories(NewPath.parent_path(), DirectoryError);
        if (DirectoryError)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to create destination folder for source asset rename: " +
                                                 DirectoryError.message()));
        }

        std::error_code RenameError{};
        std::filesystem::rename(OldPath, NewPath, RenameError);
        if (RenameError)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to rename source asset: " + RenameError.message()));
        }

        if (const auto MetadataIt = m_assetImportMetadata.find(OldAssetId); MetadataIt != m_assetImportMetadata.end())
        {
            auto Record = std::move(MetadataIt->second);
            m_assetImportMetadata.erase(MetadataIt);
            m_assetImportMetadata.emplace(NewAssetId, std::move(Record));
            m_assetImportMetadataDirty = true;
            if (auto SaveResult = SaveAssetImportMetadataDatabase(); SaveResult)
            {
                m_assetImportMetadataDirty = false;
            }
        }

        if (m_selectedAssetKey == Asset->Key)
        {
            m_selectedAssetKey = NewLogicalName;
        }
        if (m_assetEditorAssetKey == Asset->Key)
        {
            m_assetEditorAssetKey = NewLogicalName;
            m_assetEditorAssetId = NewAssetId;
        }

        auto RefreshResult = RefreshDiscovery();
        if (!RefreshResult)
        {
            return RefreshResult;
        }

        (void)SelectAssetByKey(NewLogicalName);
        m_statusMessage = "Renamed source asset to: " + NewLogicalName;
        return Ok();
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    const std::string NormalizedName = NormalizeAssetLogicalName(NewName);
    if (NormalizedName.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Asset name cannot be empty"));
    }

    if (Asset->IsRuntime)
    {
        auto RenameResult = m_assetManager->RenameRuntimeAsset(Asset->AssetId, NormalizedName);
        if (!RenameResult)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, RenameResult.error()));
        }
        auto RefreshResult = RefreshDiscovery();
        if (!RefreshResult)
        {
            return RefreshResult;
        }
        m_statusMessage = "Renamed runtime asset to: " + NormalizedName;
        return Ok();
    }

    if (NormalizedName == Asset->Name)
    {
        m_assetRenameOverrides.erase(Asset->AssetId);
    }
    else
    {
        m_assetRenameOverrides[Asset->AssetId] = NormalizedName;
    }

    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    m_statusMessage = "Renamed asset in editor: " + NormalizedName + " (save to persist)";
    return Ok();
}

Result EditorAssetService::RenameSelectedAsset(const std::string_view NewName)
{
    if (m_selectedAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No selected asset to rename"));
    }
    return RenameAssetByKey(m_selectedAssetKey, NewName);
}

Result EditorAssetService::CreateRuntimePrefabFromNode(EditorServiceContext& Context, const NodeHandle& SourceHandle)
{
    (void)Context;

    NodeHandle ResolvedSourceHandle = SourceHandle;
    BaseNode* SourceNode = ResolvedSourceHandle.Borrowed();
    if (!SourceNode)
    {
        SourceNode = ResolvedSourceHandle.BorrowedSlowByUuid();
    }
    if (!SourceNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Source node not found for prefab creation"));
    }

    std::string BaseName = SourceNode->Name();
    if (BaseName.empty())
    {
        if (const TypeInfo* Type = TypeRegistry::Instance().Find(SourceNode->TypeKey()))
        {
            BaseName = ShortTypeName(Type->Name);
        }
        else
        {
            BaseName = "Node";
        }
    }

    std::filesystem::path AssetRoot = ResolveImportAssetRootDirectory(m_currentProject);
    if (AssetRoot.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset root directory is not available"));
    }

    std::string LogicalName{};
    std::expected<std::string, Error> SourceJson{};
    if (TypeRegistry::Instance().IsA(SourceNode->TypeKey(), StaticTypeId<Level>()))
    {
        auto* LevelNode = NodeCast<Level>(SourceNode);
        if (!LevelNode)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Selected level node could not be resolved"));
        }

        auto AssetResult = CaptureLevelAsset(*LevelNode);
        if (!AssetResult)
        {
            return std::unexpected(AssetResult.error());
        }

        SourceJson = SerializeAuthoredAssetToJson(*AssetResult);
        if (!SourceJson)
        {
            return std::unexpected(SourceJson.error());
        }
        LogicalName = MakeUniqueSourceLogicalName(AssetRoot, "Levels", BaseName, ".level");
    }
    else
    {
        auto AssetResult = CaptureNodeAsset(*SourceNode);
        if (!AssetResult)
        {
            return std::unexpected(AssetResult.error());
        }

        SourceJson = SerializeAuthoredAssetToJson(*AssetResult);
        if (!SourceJson)
        {
            return std::unexpected(SourceJson.error());
        }
        LogicalName = MakeUniqueSourceLogicalName(AssetRoot, "Prefabs", BaseName, ".prefab");
    }

    if (LogicalName.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to determine a unique source asset name"));
    }

    const std::filesystem::path OutputPath = AssetRoot / std::filesystem::path(LogicalName);
    std::error_code DirectoryError{};
    std::filesystem::create_directories(OutputPath.parent_path(), DirectoryError);
    if (DirectoryError)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create source-asset directory: " + DirectoryError.message()));
    }

    std::ofstream File(OutputPath, std::ios::binary | std::ios::trunc);
    if (!File.is_open())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to open authored asset for write: " + OutputPath.string()));
    }

    File.write(SourceJson->data(), static_cast<std::streamsize>(SourceJson->size()));
    if (!File.good())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to write source asset: " + OutputPath.string()));
    }
    File.flush();
    if (!File.good())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to flush source asset: " + OutputPath.string()));
    }
    File.close();
    if (File.fail())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to finalize source asset: " + OutputPath.string()));
    }

    auto RebuildResult = RebuildAssetManager();
    if (!RebuildResult)
    {
        return RebuildResult;
    }

    (void)SelectAssetByKey(LogicalName);
    m_statusMessage = "Created source asset: " + LogicalName;
    return Ok();
}

Result EditorAssetService::CreateSourceAssetByType(EditorServiceContext& Context,
                                                   const TypeId& AssetType,
                                                   const std::string_view AssetName,
                                                   const std::string_view FolderPath)
{
    (void)Context;
    if (TypeRegistry::Instance().Find(AssetType) == nullptr)
    {
        (void)TypeAutoRegistry::Instance().Ensure(AssetType);
    }

    AuthoredAssetRegistry::Instance().EnsureBuilt();
    const AuthoredAssetDescriptor* Descriptor = AuthoredAssetRegistry::Instance().FindByType(AssetType);
    if (!Descriptor || !Descriptor->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Authored asset type is not registered"));
    }
    if (!Descriptor->CanCreate)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Selected authored asset type cannot be created"));
    }
    if (!Descriptor->Type->RuntimeOps || !Descriptor->Type->RuntimeOps->DefaultConstruct || !Descriptor->Type->RuntimeOps->Destroy)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Authored asset type is missing JSON or construction support"));
    }

    std::filesystem::path AssetRoot = ResolveImportAssetRootDirectory(m_currentProject);
    if (AssetRoot.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset root directory is not available"));
    }

    std::error_code RootError{};
    std::filesystem::create_directories(AssetRoot, RootError);
    if (RootError)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create asset root directory: " + RootError.message()));
    }

    std::string BaseName = LeafLogicalName(std::string(AssetName));
    if (BaseName.empty())
    {
        BaseName = Descriptor->DisplayName;
        if (BaseName.empty())
        {
            BaseName = "Asset";
        }
    }

    const std::string LogicalName =
        MakeUniqueSourceLogicalName(AssetRoot, NormalizeAssetLogicalName(FolderPath), BaseName, Descriptor->FileExtension);
    if (LogicalName.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to determine a unique authored asset name"));
    }

    const std::filesystem::path OutputPath = AssetRoot / std::filesystem::path(LogicalName);
    std::error_code DirectoryError{};
    std::filesystem::create_directories(OutputPath.parent_path(), DirectoryError);
    if (DirectoryError)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create authored asset directory: " + DirectoryError.message()));
    }

    void* Storage = ::operator new(Descriptor->Type->Size, std::align_val_t(Descriptor->Type->Align));
    Descriptor->Type->RuntimeOps->DefaultConstruct(Storage);
    const auto DestroyStorage = [&]() {
        Descriptor->Type->RuntimeOps->Destroy(Storage);
        ::operator delete(Storage, std::align_val_t(Descriptor->Type->Align));
    };

    const auto* Asset = static_cast<const IAsset*>(TypeRegistry::Instance().Cast(Descriptor->Type->Id, StaticTypeId<IAsset>(), Storage));
    if (!Asset)
    {
        DestroyStorage();
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Authored asset type does not cast to IAsset"));
    }

    std::ostringstream JsonOutput{};
    const Result SaveResult = Asset->Save(JsonOutput);
    DestroyStorage();
    if (!SaveResult)
    {
        return std::unexpected(SaveResult.error());
    }

    std::ofstream File(OutputPath, std::ios::binary | std::ios::trunc);
    if (!File.is_open())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to open authored asset for write: " + OutputPath.string()));
    }
    const std::string JsonText = JsonOutput.str();
    File.write(JsonText.data(), static_cast<std::streamsize>(JsonText.size()));
    if (!File.good())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to write authored asset: " + OutputPath.string()));
    }

    const bool RequiresAssetManagerReload =
        Descriptor->CookedAssetKind == AssetKindNode() ||
        Descriptor->CookedAssetKind == AssetKindLevel() ||
        Descriptor->CookedAssetKind == AssetKindWorld();

    if (RequiresAssetManagerReload)
    {
        auto RebuildResult = RebuildAssetManager();
        if (!RebuildResult)
        {
            return RebuildResult;
        }
    }
    else
    {
        auto RefreshResult = RefreshDiscovery();
        if (!RefreshResult)
        {
            return RefreshResult;
        }
    }

    (void)SelectAssetByKey(LogicalName);
    m_statusMessage = "Created authored asset: " + LogicalName;
    return Ok();
}

Result EditorAssetService::CreatePrefabSourceAssetByNodeType(EditorServiceContext& Context,
                                                             const TypeId& NodeType,
                                                             const std::string_view AssetName,
                                                             const std::string_view FolderPath)
{
    (void)Context;
    if (TypeRegistry::Instance().Find(NodeType) == nullptr)
    {
        (void)TypeAutoRegistry::Instance().Ensure(NodeType);
    }

    const TypeInfo* Type = TypeRegistry::Instance().Find(NodeType);
    if (!Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node type is not registered"));
    }
    if (!TypeRegistry::Instance().IsA(NodeType, StaticTypeId<BaseNode>()))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Requested type is not a node type"));
    }
    if (TypeRegistry::Instance().IsA(NodeType, StaticTypeId<World>()) ||
        TypeRegistry::Instance().IsA(NodeType, StaticTypeId<Level>()))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "World/Level types cannot be created as prefabs"));
    }

    bool HasDefaultCtor = false;
    for (const ConstructorInfo& Ctor : Type->Constructors)
    {
        if (Ctor.ParamTypes.empty())
        {
            HasDefaultCtor = true;
            break;
        }
    }
    if (!HasDefaultCtor)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Requested node type has no default constructor"));
    }

    std::filesystem::path AssetRoot = ResolveImportAssetRootDirectory(m_currentProject);
    if (AssetRoot.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset root directory is not available"));
    }

    std::error_code RootError{};
    std::filesystem::create_directories(AssetRoot, RootError);
    if (RootError)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create asset root directory: " + RootError.message()));
    }

    std::string BaseName = LeafLogicalName(std::string(AssetName));
    if (BaseName.empty())
    {
        BaseName = ShortTypeName(Type->Name);
        if (BaseName.empty())
        {
            BaseName = "Prefab";
        }
    }

    const std::string LogicalName =
        MakeUniqueSourceLogicalName(AssetRoot, NormalizeAssetLogicalName(FolderPath), BaseName, ".prefab");
    if (LogicalName.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to determine a unique prefab asset name"));
    }

    World ScratchWorld(std::string(BaseName) + ".PrefabCreateWorld");
    auto RootHandleResult = ScratchWorld.CreateNode(NodeType, BaseName);
    if (!RootHandleResult)
    {
        return std::unexpected(RootHandleResult.error());
    }

    BaseNode* RootNode = RootHandleResult->Borrowed();
    if (!RootNode)
    {
        RootNode = RootHandleResult->BorrowedSlowByUuid();
    }
    if (!RootNode)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Created prefab root node could not be resolved"));
    }

    InitializeCreatedNodeDefaults(ScratchWorld, *RootNode);

    auto AssetResult = CaptureNodeAsset(*RootNode);
    if (!AssetResult)
    {
        return std::unexpected(AssetResult.error());
    }

    auto SourceJson = SerializeAuthoredAssetToJson(*AssetResult);
    if (!SourceJson)
    {
        return std::unexpected(SourceJson.error());
    }

    const std::filesystem::path OutputPath = AssetRoot / std::filesystem::path(LogicalName);
    std::error_code DirectoryError{};
    std::filesystem::create_directories(OutputPath.parent_path(), DirectoryError);
    if (DirectoryError)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create prefab asset directory: " + DirectoryError.message()));
    }

    std::ofstream File(OutputPath, std::ios::binary | std::ios::trunc);
    if (!File.is_open())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to open prefab asset for write: " + OutputPath.string()));
    }
    File.write(SourceJson->data(), static_cast<std::streamsize>(SourceJson->size()));
    if (!File.good())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to write prefab asset: " + OutputPath.string()));
    }
    File.flush();
    if (!File.good())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to flush prefab asset: " + OutputPath.string()));
    }
    File.close();
    if (File.fail())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to finalize prefab asset: " + OutputPath.string()));
    }

    auto RebuildResult = RebuildAssetManager();
    if (!RebuildResult)
    {
        return RebuildResult;
    }

    (void)SelectAssetByKey(LogicalName);
    m_statusMessage = "Created prefab asset: " + LogicalName;
    return Ok();
}

Result EditorAssetService::CreateRuntimeMaterialAsset(EditorServiceContext& Context,
                                                      const std::string_view AssetName,
                                                      const std::string_view FolderPath)
{
    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    std::string BaseName = LeafLogicalName(std::string(AssetName));
    if (BaseName.empty())
    {
        BaseName = "Material";
    }
    const std::string TargetFolder = NormalizeAssetLogicalName(FolderPath);
    const std::string LogicalName = MakeUniqueLogicalName(*m_assetManager, TargetFolder, BaseName);

    MaterialAsset Payload{};
    Payload.ShaderModule = std::string(kDefaultMaterialShaderModule);
    Payload.ShadingModel = std::string(kDefaultMaterialShadingModel);

    std::vector<uint8_t> Bytes{};
    auto SerializeResult = SerializeMaterialPayload(Payload, Bytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error());
    }

    ::SnAPI::AssetPipeline::RuntimeAssetUpsert RuntimeAsset{};
    RuntimeAsset.Id = ::SnAPI::AssetPipeline::AssetId::Generate();
    RuntimeAsset.Name = LogicalName;
    RuntimeAsset.AssetKind = AssetKindMaterial();
    RuntimeAsset.Cooked = ::SnAPI::AssetPipeline::TypedPayload(
        PayloadMaterial(),
        kMaterialPayloadSchemaVersion,
        std::move(Bytes));
    RuntimeAsset.Bulk.clear();
    RuntimeAsset.Dirty = true;

    auto UpsertResult = m_assetManager->UpsertRuntimeAsset(std::move(RuntimeAsset));
    if (!UpsertResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, UpsertResult.error()));
    }

    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    const std::string AssetKey = UpsertResult->ToString();
    (void)SelectAssetByKey(AssetKey);
    m_statusMessage = "Created runtime material asset: " + LogicalName;
    (void)Context;
    return Ok();
}

Result EditorAssetService::CreateRuntimeMaterialInstanceAsset(EditorServiceContext& Context,
                                                              const std::string_view AssetName,
                                                              const std::string_view FolderPath)
{
    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    std::string BaseName = LeafLogicalName(std::string(AssetName));
    if (BaseName.empty())
    {
        BaseName = "MaterialInstance";
    }
    const std::string TargetFolder = NormalizeAssetLogicalName(FolderPath);
    const std::string LogicalName = MakeUniqueLogicalName(*m_assetManager, TargetFolder, BaseName);

    MaterialInstanceAsset Payload{};

    std::vector<uint8_t> Bytes{};
    auto SerializeResult = SerializeMaterialInstancePayload(Payload, Bytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error());
    }

    ::SnAPI::AssetPipeline::RuntimeAssetUpsert RuntimeAsset{};
    RuntimeAsset.Id = ::SnAPI::AssetPipeline::AssetId::Generate();
    RuntimeAsset.Name = LogicalName;
    RuntimeAsset.AssetKind = AssetKindMaterialInstance();
    RuntimeAsset.Cooked = ::SnAPI::AssetPipeline::TypedPayload(
        PayloadMaterialInstance(),
        kMaterialInstancePayloadSchemaVersion,
        std::move(Bytes));
    RuntimeAsset.Bulk.clear();
    RuntimeAsset.Dirty = true;

    auto UpsertResult = m_assetManager->UpsertRuntimeAsset(std::move(RuntimeAsset));
    if (!UpsertResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, UpsertResult.error()));
    }

    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    const std::string AssetKey = UpsertResult->ToString();
    (void)SelectAssetByKey(AssetKey);
    m_statusMessage = "Created runtime material instance asset: " + LogicalName;
    (void)Context;
    return Ok();
}

Result EditorAssetService::ImportSourceAsset(EditorServiceContext& Context,
                                             const std::string_view SourcePath,
                                             const std::string_view DestinationFolderPath,
                                             const std::unordered_map<std::string, std::string>& BuildOptions,
                                             ::SnAPI::AssetPipeline::AssetImportSettingsPtr ImportSettings)
{
    (void)Context;

    std::string SourcePathText = TrimCopy(std::string(SourcePath));
    if (SourcePathText.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Import source path cannot be empty"));
    }

    std::filesystem::path SourceFile = std::filesystem::path(SourcePathText);
    if (auto Resolved = SPathResolver::Instance().Resolve(SourcePathText); Resolved)
    {
        SourceFile = *Resolved;
    }
    else if (!SourceFile.is_absolute())
    {
        std::error_code Error{};
        const std::filesystem::path AbsolutePath = std::filesystem::absolute(SourceFile, Error);
        if (!Error)
        {
            SourceFile = AbsolutePath;
        }
    }

    std::error_code PathError{};
    const std::filesystem::path CanonicalSource = std::filesystem::weakly_canonical(SourceFile, PathError);
    if (!PathError)
    {
        SourceFile = CanonicalSource;
    }
    PathError.clear();
    if (!std::filesystem::exists(SourceFile, PathError) || PathError)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Import source was not found: " + SourceFile.string()));
    }
    PathError.clear();
    if (!std::filesystem::is_regular_file(SourceFile, PathError) || PathError)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Import source must be a regular file"));
    }

    std::filesystem::path AssetRoot = ResolveImportAssetRootDirectory(m_currentProject);
    if (AssetRoot.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset root directory is not available for import"));
    }

    std::string FolderPath = NormalizeAssetLogicalName(DestinationFolderPath);
    const std::string SourceLeaf = SourceFile.filename().string();
    const std::string SourceStem = SourceFile.stem().string();
    if (SourceLeaf.empty() || SourceStem.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Import source filename could not be resolved"));
    }

    AuthoredAssetRegistry::Instance().EnsureBuilt();
    const std::string SourceExtension = NormalizeAssetExtension(SourceFile.extension().string());
    if (const AuthoredAssetDescriptor* Descriptor = AuthoredAssetRegistry::Instance().FindByExtension(SourceExtension))
    {
        const std::string LogicalSourceName =
            MakeUniqueSourceLogicalName(AssetRoot, FolderPath, SourceStem, SourceExtension);
        if (LogicalSourceName.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to determine a unique import destination"));
        }

        std::filesystem::path DestinationDirectory = AssetRoot;
        if (!FolderPath.empty())
        {
            DestinationDirectory /= std::filesystem::path(FolderPath);
        }

        PathError.clear();
        std::filesystem::create_directories(DestinationDirectory, PathError);
        if (PathError)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to create import destination folder: " + PathError.message()));
        }

        const std::filesystem::path DestinationPath = (AssetRoot / std::filesystem::path(LogicalSourceName)).lexically_normal();
        std::filesystem::copy_file(SourceFile, DestinationPath, std::filesystem::copy_options::overwrite_existing, PathError);
        if (PathError)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to copy source asset into project: " + PathError.message()));
        }

        auto RefreshResult = RefreshDiscovery();
        if (!RefreshResult)
        {
            return RefreshResult;
        }

        (void)SelectAssetByKey(LogicalSourceName);
        m_statusMessage = "Imported authored asset into " + DestinationPath.string();
        (void)Descriptor;
        return Ok();
    }

    ::SnAPI::AssetPipeline::PayloadRegistry Registry{};
    RegisterAssetPipelinePayloads(Registry);
    Registry.Register(TextureCompressorPlugin::CreateCompressorImageIntermediateSerializer());
    Registry.Register(TextureCompressorPlugin::CreateCompressorCookedInfoSerializer());

    std::vector<std::string> Infos{};
    std::vector<std::string> Warnings{};
    std::vector<std::string> Errors{};
    InlineImportPipelineContext PipelineContext(Registry, BuildOptions, Infos, Warnings, Errors);

    auto Importers = CreateEditorImporters();
    const ::SnAPI::AssetPipeline::SourceRef SourceCandidate{SourceFile.string(), 0ull};
    ::SnAPI::AssetPipeline::IAssetImporter* Importer = FindMatchingImporter(SourceCandidate, Importers);
    if (!Importer)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "No importer is registered for source: " + SourceFile.string()));
    }

    const ::SnAPI::AssetPipeline::SourceRef Source{SourceFile.string(), 0ull};

    ::SnAPI::AssetPipeline::AssetImportSettingsPtr EffectiveImportSettings = ImportSettings;
    const bool UseImporterDefaults = !EffectiveImportSettings && BuildOptions.empty();
    if (!EffectiveImportSettings && !UseImporterDefaults)
    {
        EffectiveImportSettings = BuildTypedImportSettingsForImporter(*Importer, BuildOptions);
    }

    std::vector<::SnAPI::AssetPipeline::ImportedItem> ImportedItems{};
    const bool ImportedSuccessfully = EffectiveImportSettings
        ? Importer->ImportWithSettings(Source, EffectiveImportSettings.get(), ImportedItems, PipelineContext)
        : Importer->Import(Source, ImportedItems, PipelineContext);
    if (!ImportedSuccessfully)
    {
        std::ostringstream Message{};
        Message << "Import failed for " << SourceFile.string();
        if (!Errors.empty())
        {
            Message << ": " << Errors.front();
        }
        return std::unexpected(MakeError(EErrorCode::InternalError, Message.str()));
    }
    if (ImportedItems.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Import produced no authored asset outputs"));
    }

    EImportProfile Profile = ImportProfileFromImporterName(Importer->GetName() ? Importer->GetName() : "");
    if (const auto* AssimpTyped = dynamic_cast<const AssimpImporterSettings*>(EffectiveImportSettings.get()))
    {
        (void)AssimpTyped;
        Profile = EImportProfile::AssimpModel;
    }
    else if (const auto* TextureTyped = dynamic_cast<const TextureCompressorPlugin::TextureCompressorImportSettings*>(
                 EffectiveImportSettings.get()))
    {
        (void)TextureTyped;
        Profile = EImportProfile::Texture;
    }

    const std::string ImportGroupFolder = ResolveImportGroupFolder(AssetRoot, FolderPath, SourceStem);
    if (ImportGroupFolder.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to determine import group folder"));
    }

    const std::filesystem::path ImportGroupPath = AssetRoot / std::filesystem::path(ImportGroupFolder);
    PathError.clear();
    std::filesystem::create_directories(ImportGroupPath, PathError);
    if (PathError)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create import destination folder: " + PathError.message()));
    }

    TextureImportSettingsPayload ImportedTextureSettings{};
    if (const auto* TextureTyped = dynamic_cast<const TextureCompressorPlugin::TextureCompressorImportSettings*>(
            EffectiveImportSettings.get()))
    {
        ImportedTextureSettings = BuildTextureAssetImportSettingsPayload(TextureTyped);
    }

    std::vector<ImportedSourceDocument> Documents{};
    Documents.reserve(ImportedItems.size());
    for (const auto& Item : ImportedItems)
    {
        auto AssetResult = BuildImportedSourceAssetVariant(Item, Registry, ImportedTextureSettings);
        if (!AssetResult)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, AssetResult.error()));
        }

        ImportedSourceDocument Document{};
        Document.OriginalLogicalName = Item.LogicalName;
        Document.Asset = std::move(*AssetResult);
        Documents.push_back(std::move(Document));
    }

    if (Profile == EImportProfile::AssimpModel)
    {
        std::unordered_set<std::string> KnownOriginalNames{};
        for (const ImportedSourceDocument& Document : Documents)
        {
            if (!Document.OriginalLogicalName.empty())
            {
                KnownOriginalNames.insert(Document.OriginalLogicalName);
            }
        }

        std::vector<std::string> ExternalTextureUris{};
        for (const ImportedSourceDocument& Document : Documents)
        {
            const auto* MaterialInstance = std::get_if<MaterialInstanceAsset>(&Document.Asset);
            if (!MaterialInstance)
            {
                continue;
            }

            for (const MaterialTextureParamPayload& TextureParam : MaterialInstance->Textures)
            {
                if (TextureParam.Texture.AssetName.empty() || KnownOriginalNames.contains(TextureParam.Texture.AssetName))
                {
                    continue;
                }
                KnownOriginalNames.insert(TextureParam.Texture.AssetName);
                ExternalTextureUris.push_back(TextureParam.Texture.AssetName);
            }
        }

        if (!ExternalTextureUris.empty())
        {
            auto ExternalTextureSettings = std::make_shared<TextureCompressorPlugin::TextureCompressorImportSettings>();
            const TextureImportSettingsPayload ExternalTexturePayload =
                BuildTextureAssetImportSettingsPayload(ExternalTextureSettings.get());
            for (const std::string& TextureUri : ExternalTextureUris)
            {
                const ::SnAPI::AssetPipeline::SourceRef TextureSource{TextureUri, 0ull};
                ::SnAPI::AssetPipeline::IAssetImporter* TextureImporter = FindMatchingImporter(TextureSource, Importers);
                if (!TextureImporter)
                {
                    Warnings.push_back("No texture importer found for referenced texture: " + TextureUri);
                    continue;
                }

                std::vector<::SnAPI::AssetPipeline::ImportedItem> TextureItems{};
                if (!TextureImporter->ImportWithSettings(TextureSource, ExternalTextureSettings.get(), TextureItems, PipelineContext))
                {
                    Warnings.push_back("Failed to import referenced texture: " + TextureUri);
                    continue;
                }

                for (const auto& TextureItem : TextureItems)
                {
                    auto AssetResult = BuildImportedSourceAssetVariant(TextureItem, Registry, ExternalTexturePayload);
                    if (!AssetResult)
                    {
                        Warnings.push_back("Failed to convert referenced texture to authored asset: " + TextureUri);
                        continue;
                    }

                    ImportedSourceDocument Document{};
                    Document.OriginalLogicalName = TextureItem.LogicalName;
                    Document.Asset = std::move(*AssetResult);
                    Documents.push_back(std::move(Document));
                }
            }
        }
    }

    const std::string GroupLeaf = LeafLogicalName(ImportGroupFolder);
    for (ImportedSourceDocument& Document : Documents)
    {
        const std::string LeafName = BuildImportedSourceAssetLeaf(GroupLeaf, Document);
        const std::string Extension = FileExtensionForImportedSourceAsset(Document.Asset);
        Document.FinalLogicalName = NormalizeAssetLogicalName(ImportGroupFolder + "/" + LeafName + Extension);
        Document.FinalAssetId = MakeDeterministicSourceAssetId(Document.FinalLogicalName);
    }

    std::unordered_map<std::string, AssetRefPayload> RefsByOriginalName{};
    for (const ImportedSourceDocument& Document : Documents)
    {
        if (Document.OriginalLogicalName.empty())
        {
            continue;
        }

        AssetRefPayload Ref{};
        Ref.AssetName = Document.FinalLogicalName;
        Ref.AssetId = Document.FinalAssetId.ToString();
        RefsByOriginalName.emplace(Document.OriginalLogicalName, std::move(Ref));
    }

    for (ImportedSourceDocument& Document : Documents)
    {
        RewriteImportedAssetRefs(Document.Asset, RefsByOriginalName);
    }

    AssetImportMetadataEntry Record{};
    Record.SourcePath = SourceFile.string();
    Record.DestinationFolder = ImportGroupFolder;
    Record.ImporterName = Importer->GetName() ? Importer->GetName() : "";
    Record.Profile = Profile;
    Record.BuildOptions = BuildOptions;
    if (const auto* AssimpTyped = dynamic_cast<const AssimpImporterSettings*>(EffectiveImportSettings.get()))
    {
        FillAssimpImportSettingsFromTyped(*AssimpTyped, Record.Assimp);
    }
    if (const auto* TextureTyped = dynamic_cast<const TextureCompressorPlugin::TextureCompressorImportSettings*>(
            EffectiveImportSettings.get()))
    {
        FillTextureImportSettingsFromTyped(*TextureTyped, Record.Texture);
    }
    const ImportedAssetProvenancePayload ImportedProvenance = BuildImportedAssetProvenancePayload(
        Record.SourcePath,
        Record.DestinationFolder,
        Record.ImporterName,
        Record.Profile,
        Record.BuildOptions);
    for (ImportedSourceDocument& Document : Documents)
    {
        ApplyImportedSourceAssetProvenance(Document.Asset, ImportedProvenance);
    }

    std::unordered_set<::SnAPI::AssetPipeline::AssetId, ::SnAPI::AssetPipeline::UuidHash> NewAssetIds{};
    std::string PrimaryLogicalName{};
    int PrimaryLogicalPriority = -1;
    for (ImportedSourceDocument& Document : Documents)
    {
        const std::filesystem::path OutputPath = (AssetRoot / std::filesystem::path(Document.FinalLogicalName)).lexically_normal();
        std::error_code DirectoryError{};
        std::filesystem::create_directories(OutputPath.parent_path(), DirectoryError);
        if (DirectoryError)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to create import destination folder: " + DirectoryError.message()));
        }

        auto JsonResult = SerializeImportedSourceAssetJson(Document.Asset);
        if (!JsonResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, JsonResult.error()));
        }

        std::ofstream File(OutputPath, std::ios::binary | std::ios::trunc);
        if (!File.is_open())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to open authored asset for write: " + OutputPath.string()));
        }

        File.write(JsonResult->data(), static_cast<std::streamsize>(JsonResult->size()));
        if (!File.good())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to write authored asset: " + OutputPath.string()));
        }

        NewAssetIds.insert(Document.FinalAssetId);
        const int DocumentPriority = ImportedPrimarySelectionPriority(Document.Asset);
        if (PrimaryLogicalName.empty() || DocumentPriority > PrimaryLogicalPriority)
        {
            PrimaryLogicalName = Document.FinalLogicalName;
            PrimaryLogicalPriority = DocumentPriority;
        }
    }

    std::vector<std::filesystem::path> StaleFiles{};
    for (const auto& Asset : m_assets)
    {
        if (Asset.SourceFilePath.empty())
        {
            continue;
        }

        const auto MetadataIt = m_assetImportMetadata.find(Asset.AssetId);
        if (MetadataIt == m_assetImportMetadata.end())
        {
            continue;
        }

        const AssetImportMetadataEntry& ExistingRecord = MetadataIt->second;
        if (ExistingRecord.SourcePath != SourceFile.string() ||
            ExistingRecord.DestinationFolder != ImportGroupFolder ||
            ExistingRecord.Profile != Profile)
        {
            continue;
        }

        if (NewAssetIds.contains(Asset.AssetId))
        {
            continue;
        }

        StaleFiles.push_back(std::filesystem::path(Asset.SourceFilePath));
        m_assetImportMetadata.erase(MetadataIt);
    }

    for (const std::filesystem::path& StaleFile : StaleFiles)
    {
        std::error_code RemoveError{};
        (void)std::filesystem::remove(StaleFile, RemoveError);
    }

    for (const ImportedSourceDocument& Document : Documents)
    {
        m_assetImportMetadata[Document.FinalAssetId] = Record;
    }

    m_assetImportMetadataDirty = true;
    if (auto SaveResult = SaveAssetImportMetadataDatabase(); SaveResult)
    {
        m_assetImportMetadataDirty = false;
    }
    else
    {
        m_statusMessage = "Import metadata save warning: " + SaveResult.error();
    }

    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    if (!PrimaryLogicalName.empty())
    {
        (void)SelectAssetByKey(PrimaryLogicalName);
    }

    std::ostringstream Status{};
    Status << "Imported authored assets from " << SourceLeaf << " into " << ImportGroupPath.string();
    if (!Warnings.empty())
    {
        Status << " (" << Warnings.size() << " warning";
        if (Warnings.size() != 1u)
        {
            Status << "s";
        }
        Status << ")";
    }
    m_statusMessage = Status.str();
    return Ok();
}

Result EditorAssetService::OpenAssetEditorByKey(const std::string_view Key)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found for editing"));
    }

    if (!Asset->SourceFilePath.empty() && Asset->AssetType != TypeId{})
    {
        if (Asset->AssetKind == AssetKindNode() ||
            Asset->AssetKind == AssetKindLevel() ||
            Asset->AssetKind == AssetKindWorld())
        {
            // Source-backed hierarchy assets still open through the live hierarchy inspector path.
        }
        else
        {
        AuthoredAssetRegistry::Instance().EnsureBuilt();
        const AuthoredAssetDescriptor* Descriptor = AuthoredAssetRegistry::Instance().FindByType(Asset->AssetType);
        if (!Descriptor || !Descriptor->Type || !Descriptor->Type->RuntimeOps || !Descriptor->Type->RuntimeOps->DefaultConstruct ||
            !Descriptor->Type->RuntimeOps->Destroy)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Source asset type does not support editor JSON loading"));
        }

        std::ifstream File(Asset->SourceFilePath, std::ios::binary | std::ios::ate);
        if (!File.is_open())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Failed to open source asset: " + Asset->SourceFilePath));
        }

        const std::streamsize Size = File.tellg();
        std::string SourceJson{};
        if (Size > 0)
        {
            SourceJson.resize(static_cast<std::size_t>(Size));
            File.seekg(0, std::ios::beg);
            File.read(SourceJson.data(), Size);
        }

        void* Storage = ::operator new(Descriptor->Type->Size, std::align_val_t(Descriptor->Type->Align));
        Descriptor->Type->RuntimeOps->DefaultConstruct(Storage);
        if (!TypeRegistry::Instance().Cast(Descriptor->Type->Id, StaticTypeId<IAsset>(), Storage))
        {
            Descriptor->Type->RuntimeOps->Destroy(Storage);
            ::operator delete(Storage, std::align_val_t(Descriptor->Type->Align));
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Source asset type does not cast to IAsset"));
        }

        const Result LoadResult = DeserializeAuthoredAssetFromJson(Descriptor->Type->Id, SourceJson, Storage);
        if (!LoadResult)
        {
            Descriptor->Type->RuntimeOps->Destroy(Storage);
            ::operator delete(Storage, std::align_val_t(Descriptor->Type->Align));
            return std::unexpected(LoadResult.error());
        }

        ClearAssetEditorState();
        m_assetEditorGenericSourceObject = {
            Storage,
            [Type = Descriptor->Type](void* Instance) {
                if (!Instance || !Type || !Type->RuntimeOps)
                {
                    return;
                }
                Type->RuntimeOps->Destroy(Instance);
                ::operator delete(Instance, std::align_val_t(Type->Align));
            }
        };
        m_assetEditorAssetKey = Asset->Key;
        m_assetEditorAssetId = Asset->AssetId;
        m_assetEditorAssetKind = Asset->AssetKind;
        m_assetEditorCanSave = Asset->CanSave;
        m_assetEditorTargetType = Asset->AssetType;
        m_assetEditorTargetObject = m_assetEditorGenericSourceObject.get();
        m_assetEditorSourceAssetType = Asset->AssetType;
        (void)RefreshAssetEditorImportSettingsBinding(*Asset);
        m_assetEditorTitle = Asset->TypeLabel + " - " + Asset->Name;
        m_assetEditorDocumentState.Reset();
        RefreshAssetEditorDirtyFlags(false);
        ++m_assetEditorSessionRevision;
        m_statusMessage = "Opened source asset: " + Asset->Name;
        return Ok();
        }
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    ClearAssetEditorState();
    m_assetEditorAssetKey = Asset->Key;
    m_assetEditorAssetId = Asset->AssetId;
    m_assetEditorAssetKind = Asset->AssetKind;
    m_assetEditorCanSave = Asset->CanSave;
    m_assetEditorCanEditHierarchy = false;
    m_assetEditorSelectedNode = {};
    m_assetEditorHierarchy.clear();
    m_assetEditorTitle = Asset->TypeLabel + " - " + Asset->Name;

    if (Asset->AssetKind == AssetKindNode())
    {
        m_assetEditorWorld = std::make_unique<World>("Editor.AssetInspector.NodeWorld");
        NodeAssetLoadParams LoadParams{};
        LoadParams.TargetWorld = m_assetEditorWorld.get();
        LoadParams.InstantiateAsCopy = false;
        LoadParams.OutCreatedRoot = &m_assetEditorRootHandle;
        auto LoadResult = Asset->SourceFilePath.empty()
            ? m_assetManager->Load<BaseNode>(Asset->AssetId, LoadParams)
            : m_assetManager->Load<BaseNode>(Asset->Name, LoadParams);
        if (!LoadResult)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, LoadResult.error()));
        }

        BaseNode* RootNode = ResolveAssetEditorNode(m_assetEditorRootHandle);
        if (!RootNode)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, "Loaded node asset root was not created"));
        }

        m_assetEditorTargetObject = RootNode;
        m_assetEditorTargetType = RootNode->TypeKey();
        m_assetEditorCanEditHierarchy = true;
        m_assetEditorSelectedNode = m_assetEditorRootHandle;
        if (!Asset->SourceFilePath.empty() && Asset->AssetType != TypeId{})
        {
            m_assetEditorSourceAssetType = Asset->AssetType;
        }
    }
    else if (Asset->AssetKind == AssetKindLevel())
    {
        m_assetEditorWorld = std::make_unique<World>("Editor.AssetInspector.LevelWorld");
        LevelAssetLoadParams LoadParams{};
        LoadParams.TargetWorld = m_assetEditorWorld.get();
        LoadParams.InstantiateAsCopy = false;
        LoadParams.OutCreatedLevel = &m_assetEditorRootHandle;
        auto LoadResult = Asset->SourceFilePath.empty()
            ? m_assetManager->Load<Level>(Asset->AssetId, LoadParams)
            : m_assetManager->Load<Level>(Asset->Name, LoadParams);
        if (!LoadResult)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, LoadResult.error()));
        }

        auto* LevelNode = NodeCast<Level>(ResolveAssetEditorNode(m_assetEditorRootHandle));
        if (!LevelNode)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, "Loaded level asset root was not created"));
        }

        m_assetEditorTargetObject = LevelNode;
        m_assetEditorTargetType = StaticTypeId<Level>();
        m_assetEditorCanEditHierarchy = true;
        m_assetEditorSelectedNode = m_assetEditorRootHandle;
        if (!Asset->SourceFilePath.empty() && Asset->AssetType != TypeId{})
        {
            m_assetEditorSourceAssetType = Asset->AssetType;
        }
    }
    else if (Asset->AssetKind == AssetKindWorld())
    {
        auto LoadResult = Asset->SourceFilePath.empty()
            ? m_assetManager->Load<World>(Asset->AssetId)
            : m_assetManager->Load<World>(Asset->Name);
        if (!LoadResult)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, LoadResult.error()));
        }

        m_assetEditorWorld = std::move(*LoadResult);
        m_assetEditorTargetObject = m_assetEditorWorld.get();
        m_assetEditorTargetType = StaticTypeId<World>();
        if (!Asset->SourceFilePath.empty() && Asset->AssetType != TypeId{})
        {
            m_assetEditorSourceAssetType = Asset->AssetType;
        }
    }
    else if (Asset->AssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture)
    {
        auto CookedPayloadResult = BuildCookedPayloadForAsset(*Asset);
        if (!CookedPayloadResult)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, CookedPayloadResult.error()));
        }

        if (CookedPayloadResult->PayloadType != TextureCompressorPlugin::Payload_CompressorCookedInfo)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Texture asset has unexpected payload type"));
        }

        const auto* TextureSerializer = m_assetManager->GetRegistry().Find(TextureCompressorPlugin::Payload_CompressorCookedInfo);
        if (!TextureSerializer)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::NotReady, "Texture serializer is not registered"));
        }

        TextureCompressorPlugin::TextureCompressorCookedInfo Cooked{};
        if (!TextureSerializer->DeserializeFromBytes(
                &Cooked,
                CookedPayloadResult->Bytes.data(),
                CookedPayloadResult->Bytes.size()))
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to deserialize cooked texture payload"));
        }

        m_assetEditorTextureCookedInfo = Cooked;
        Editor::TextureAssetEditorPayload Payload{};
        PopulateTextureEditorPayloadFromCooked(Cooked, Payload);
        m_assetEditorTexturePayload = std::move(Payload);
        // Texture compression knobs are import settings and require reimport.
        // Keep cooked payload around for preview/intrinsic metadata, but do not expose it as runtime-editable state.
        m_assetEditorTargetObject = nullptr;
        m_assetEditorTargetType = {};
    }
    else if (Asset->AssetKind == AssetKindStaticMesh())
    {
        auto CookedPayloadResult = BuildCookedPayloadForAsset(*Asset);
        if (!CookedPayloadResult)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, CookedPayloadResult.error()));
        }

        if (CookedPayloadResult->PayloadType != PayloadStaticMesh())
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Static mesh asset has unexpected payload type"));
        }

        auto PayloadResult = DeserializeStaticMeshPayload(
            CookedPayloadResult->Bytes.data(),
            CookedPayloadResult->Bytes.size());
        if (!PayloadResult)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, PayloadResult.error().Message));
        }

        m_assetEditorStaticMeshPayload = std::move(PayloadResult.value());
        Editor::StaticMeshAssetEditorPayload EditorPayload{};
        PopulateStaticMeshEditorPayloadFromCooked(*m_assetEditorStaticMeshPayload, EditorPayload);
        m_assetEditorStaticMeshEditorPayload = std::move(EditorPayload);
        m_assetEditorTargetObject = &(*m_assetEditorStaticMeshEditorPayload);
        m_assetEditorTargetType = StaticTypeId<Editor::StaticMeshAssetEditorPayload>();
    }
    else if (Asset->AssetKind == AssetKindMaterial())
    {
        auto CookedPayloadResult = BuildCookedPayloadForAsset(*Asset);
        if (!CookedPayloadResult)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, CookedPayloadResult.error()));
        }

        if (CookedPayloadResult->PayloadType != PayloadMaterial())
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Material asset has unexpected payload type"));
        }

        auto PayloadResult = DeserializeMaterialPayload(
            CookedPayloadResult->Bytes.data(),
            CookedPayloadResult->Bytes.size());
        if (!PayloadResult)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, PayloadResult.error().Message));
        }

        m_assetEditorMaterialPayload = std::move(PayloadResult.value());
        m_assetEditorTargetObject = &(*m_assetEditorMaterialPayload);
        m_assetEditorTargetType = StaticTypeId<MaterialAsset>();
    }
    else if (Asset->AssetKind == AssetKindMaterialInstance())
    {
        auto CookedPayloadResult = BuildCookedPayloadForAsset(*Asset);
        if (!CookedPayloadResult)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, CookedPayloadResult.error()));
        }

        if (CookedPayloadResult->PayloadType != PayloadMaterialInstance())
        {
            ClearAssetEditorState();
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "Material instance asset has unexpected payload type"));
        }

        auto PayloadResult = DeserializeMaterialInstancePayload(
            CookedPayloadResult->Bytes.data(),
            CookedPayloadResult->Bytes.size());
        if (!PayloadResult)
        {
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, PayloadResult.error().Message));
        }

        m_assetEditorMaterialInstancePayload = std::move(PayloadResult.value());
        m_assetEditorTargetObject = &(*m_assetEditorMaterialInstancePayload);
        m_assetEditorTargetType = StaticTypeId<MaterialInstanceAsset>();
        if (auto SyncResult = SyncMaterialInstanceEditorPayloadFromDescriptor(); !SyncResult)
        {
            m_statusMessage = "Material instance descriptor sync warning: " + SyncResult.error().Message;
        }
    }
    else
    {
        ClearAssetEditorState();
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported asset kind for inspector editing"));
    }

    (void)RefreshAssetEditorImportSettingsBinding(*Asset);

    if (HasAssetEditorRuntimeTarget() && m_assetEditorSourceAssetType == TypeId{})
    {
        auto InitialPayloadResult = SerializeAssetEditorPayload();
        if (!InitialPayloadResult)
        {
            const std::string ErrorMessage = InitialPayloadResult.error();
            ClearAssetEditorState();
            return std::unexpected(MakeError(EErrorCode::InternalError, ErrorMessage));
        }
    }

    m_assetPayloadOverrides.erase(m_assetEditorAssetId);
    m_assetEditorDocumentState.Reset();
    RefreshAssetEditorDirtyFlags(false);
    RefreshAssetEditorHierarchy();
    m_statusMessage = "Opened asset inspector: " + Asset->Name;
    return Ok();
}

Result EditorAssetService::OpenAssetEditorByKey(EditorServiceContext& Context, const std::string_view Key)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found for editing"));
    }

    if (!Asset->SourceFilePath.empty() && Asset->AssetType != TypeId{})
    {
        if (Asset->AssetKind != AssetKindConduitGraph() && Asset->AssetKind != AssetKindConduitClass())
        {
            return OpenAssetEditorByKey(Key);
        }

        auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
        if (!ConduitService)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Conduit editor service is not available"));
        }

        std::ifstream File(Asset->SourceFilePath, std::ios::binary | std::ios::ate);
        if (!File.is_open())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Failed to open source asset: " + Asset->SourceFilePath));
        }

        const std::streamsize Size = File.tellg();
        std::string SourceJson{};
        if (Size > 0)
        {
            SourceJson.resize(static_cast<std::size_t>(Size));
            File.seekg(0, std::ios::beg);
            File.read(SourceJson.data(), Size);
        }

        ClearAssetEditorState();
        if (Asset->AssetKind == AssetKindConduitGraph())
        {
            Conduit::GraphAsset AssetValue{};
            auto ParseResult = DeserializeAuthoredAssetFromJson(SourceJson, AssetValue);
            if (!ParseResult)
            {
                return std::unexpected(ParseResult.error());
            }

            auto OpenResult = ConduitService->OpenDocument(Asset->Key, Asset->Name, AssetValue);
            if (!OpenResult)
            {
                return std::unexpected(OpenResult.error());
            }

            m_statusMessage = "Opened Conduit graph: " + Asset->Name;
            return Ok();
        }

        Conduit::ClassAsset AssetValue{};
        auto ParseResult = DeserializeAuthoredAssetFromJson(SourceJson, AssetValue);
        if (!ParseResult)
        {
            return std::unexpected(ParseResult.error());
        }

        auto OpenResult = ConduitService->OpenClassDocument(Asset->Key, Asset->Name, AssetValue);
        if (!OpenResult)
        {
            return std::unexpected(OpenResult.error());
        }

        m_statusMessage = "Opened Conduit class: " + Asset->Name;
        return Ok();
    }

    if (Asset->AssetKind != AssetKindConduitGraph() && Asset->AssetKind != AssetKindConduitClass())
    {
        return OpenAssetEditorByKey(Key);
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
    if (!ConduitService)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Conduit editor service is not available"));
    }

    ClearAssetEditorState();

    if (Asset->AssetKind == AssetKindConduitGraph())
    {
        auto LoadResult = m_assetManager->Load<Conduit::GraphAsset>(Asset->AssetId);
        if (!LoadResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, LoadResult.error()));
        }

        auto OpenResult = ConduitService->OpenDocument(Asset->Key, Asset->Name, *LoadResult.value());
        if (!OpenResult)
        {
            return std::unexpected(OpenResult.error());
        }

        m_statusMessage = "Opened Conduit graph: " + Asset->Name;
        return Ok();
    }

    auto LoadResult = m_assetManager->Load<Conduit::ClassAsset>(Asset->AssetId);
    if (!LoadResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, LoadResult.error()));
    }

    auto OpenResult = ConduitService->OpenClassDocument(Asset->Key, Asset->Name, *LoadResult.value());
    if (!OpenResult)
    {
        return std::unexpected(OpenResult.error());
    }

    m_statusMessage = "Opened Conduit class: " + Asset->Name;
    return Ok();
}

void EditorAssetService::CloseAssetEditor()
{
    ClearAssetEditorState();
}

void EditorAssetService::CloseAssetEditor(EditorServiceContext& Context)
{
    if (!m_assetEditorAssetKey.empty())
    {
        CloseAssetEditor();
        return;
    }

    auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
    if (!ConduitService)
    {
        return;
    }

    const auto WorkspaceView = ConduitService->ActiveWorkspaceView();
    if (!WorkspaceView.Open)
    {
        return;
    }

    if (ConduitService->CloseAnyDocument(WorkspaceView.AssetKey))
    {
        m_statusMessage = WorkspaceView.Kind == Conduit::Editor::EWorkspaceDocumentKind::Graph
            ? "Closed Conduit graph: " + WorkspaceView.Title
            : "Closed Conduit class: " + WorkspaceView.Title;
    }
}

Result EditorAssetService::SelectAssetEditorNode(const NodeHandle& Node)
{
    if (!m_assetEditorCanEditHierarchy || m_assetEditorAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset editor hierarchy is not available"));
    }

    NodeHandle RequestedNode = Node;
    if (RequestedNode.IsNull())
    {
        RequestedNode = m_assetEditorRootHandle;
    }

    BaseNode* ResolvedNode = ResolveAssetEditorNode(RequestedNode);
    if (!ResolvedNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset editor node was not found"));
    }

    m_assetEditorSelectedNode = ResolvedNode->Handle();
    RefreshAssetEditorHierarchy();
    return Ok();
}

Result EditorAssetService::AddAssetEditorNode(const NodeHandle& Parent, const TypeId& NodeType)
{
    if (!m_assetEditorWorld || !m_assetEditorCanEditHierarchy || m_assetEditorAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset editor hierarchy is not available"));
    }
    if (NodeType == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node type is required"));
    }

    const TypeInfo* Type = TypeRegistry::Instance().Find(NodeType);
    if (!Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Requested node type is not registered"));
    }
    if (!TypeRegistry::Instance().IsA(NodeType, StaticTypeId<BaseNode>()))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Requested type is not a node type"));
    }
    if (TypeRegistry::Instance().IsA(NodeType, StaticTypeId<World>()) ||
        TypeRegistry::Instance().IsA(NodeType, StaticTypeId<Level>()))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "World/Level types cannot be added inside node assets"));
    }

    NodeHandle ParentHandle = Parent.IsNull() ? m_assetEditorRootHandle : Parent;
    BaseNode* ParentNode = ResolveAssetEditorNode(ParentHandle);
    if (!ParentNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Parent node was not found"));
    }

    std::string NodeName = ShortTypeName(Type->Name);
    if (NodeName.empty())
    {
        NodeName = "Node";
    }

    auto CreateResult = m_assetEditorWorld->CreateNode(NodeType, NodeName);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    NodeHandle ResolvedParentHandle = ParentNode->Handle();
    auto AttachResult = m_assetEditorWorld->AttachChild(ResolvedParentHandle, *CreateResult);
    if (!AttachResult)
    {
        return std::unexpected(AttachResult.error());
    }

    if (BaseNode* CreatedNode = CreateResult->Borrowed())
    {
        InitializeCreatedNodeDefaults(*m_assetEditorWorld, *CreatedNode);
        m_assetEditorSelectedNode = CreatedNode->Handle();
    }
    else
    {
        m_assetEditorSelectedNode = *CreateResult;
    }

    RefreshAssetEditorHierarchy();
    MarkAssetEditorRuntimeChanged(m_assetEditorSourceAssetType != TypeId{});
    return Ok();
}

Result EditorAssetService::DeleteAssetEditorNode(const NodeHandle& Node)
{
    if (!m_assetEditorWorld || !m_assetEditorCanEditHierarchy || m_assetEditorAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset editor hierarchy is not available"));
    }
    if (Node.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node handle is required"));
    }

    NodeHandle TargetHandle = Node;
    BaseNode* TargetNode = ResolveAssetEditorNode(TargetHandle);
    if (!TargetNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Target node was not found"));
    }
    if (TargetNode->Handle() == m_assetEditorRootHandle)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Root node cannot be deleted"));
    }

    const NodeHandle NextSelection = !TargetNode->Parent().IsNull() ? TargetNode->Parent() : m_assetEditorRootHandle;
    NodeHandle DestroyHandle = TargetNode->Handle();
    auto DestroyResult = m_assetEditorWorld->DestroyNode(DestroyHandle);
    if (!DestroyResult)
    {
        return std::unexpected(DestroyResult.error());
    }

    m_assetEditorSelectedNode = NextSelection;
    RefreshAssetEditorHierarchy();
    MarkAssetEditorRuntimeChanged(m_assetEditorSourceAssetType != TypeId{});
    return Ok();
}

Result EditorAssetService::AddAssetEditorComponent(const NodeHandle& Owner, const TypeId& ComponentType)
{
    if (!m_assetEditorWorld || !m_assetEditorCanEditHierarchy || m_assetEditorAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset editor hierarchy is not available"));
    }
    if (Owner.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Owner node is required"));
    }
    if (ComponentType == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Component type is required"));
    }
    if (!ComponentSerializationRegistry::Instance().Has(ComponentType))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Requested type is not a component type"));
    }

    NodeHandle OwnerHandle = Owner;
    BaseNode* OwnerNode = ResolveAssetEditorNode(OwnerHandle);
    if (!OwnerNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node was not found"));
    }

    NodeHandle OwnerRuntimeHandle = OwnerNode->Handle();
    auto AddResult = m_assetEditorWorld->CreateComponent(OwnerRuntimeHandle, ComponentType);
    if (!AddResult)
    {
        return std::unexpected(AddResult.error());
    }

    m_assetEditorSelectedNode = OwnerNode->Handle();
    RefreshAssetEditorHierarchy();
    MarkAssetEditorRuntimeChanged(m_assetEditorSourceAssetType != TypeId{});
    return Ok();
}

Result EditorAssetService::RemoveAssetEditorComponent(const NodeHandle& Owner, const TypeId& ComponentType)
{
    if (!m_assetEditorWorld || !m_assetEditorCanEditHierarchy || m_assetEditorAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset editor hierarchy is not available"));
    }
    if (Owner.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Owner node is required"));
    }
    if (ComponentType == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Component type is required"));
    }

    NodeHandle OwnerHandle = Owner;
    BaseNode* OwnerNode = ResolveAssetEditorNode(OwnerHandle);
    if (!OwnerNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node was not found"));
    }

    NodeHandle OwnerRuntimeHandle = OwnerNode->Handle();
    auto RemoveResult = m_assetEditorWorld->RemoveComponentByType(OwnerRuntimeHandle, ComponentType);
    if (!RemoveResult)
    {
        return std::unexpected(RemoveResult.error());
    }

    m_assetEditorSelectedNode = OwnerNode->Handle();
    RefreshAssetEditorHierarchy();
    MarkAssetEditorRuntimeChanged(m_assetEditorSourceAssetType != TypeId{});
    return Ok();
}

void EditorAssetService::TickAssetEditorSession(const float DeltaSeconds)
{
    (void)DeltaSeconds;

    if (m_assetEditorAssetKey.empty())
    {
        return;
    }

    const DiscoveredAsset* Asset = FindAssetByKey(m_assetEditorAssetKey);
    if (!Asset)
    {
        ClearAssetEditorState();
        return;
    }

    const bool PreviousCanSave = m_assetEditorCanSave;
    const std::string PreviousTitle = m_assetEditorTitle;
    m_assetEditorCanSave = Asset->CanSave;
    m_assetEditorTitle = Asset->TypeLabel + " - " + Asset->Name;
    if (PreviousCanSave != m_assetEditorCanSave || PreviousTitle != m_assetEditorTitle)
    {
        ++m_assetEditorSessionRevision;
    }

    if (Asset->AssetKind == AssetKindNode() || Asset->AssetKind == AssetKindLevel())
    {
        BaseNode* RootNode = ResolveAssetEditorNode(m_assetEditorRootHandle);
        if (!RootNode)
        {
            ClearAssetEditorState();
            return;
        }

        if (Asset->AssetKind == AssetKindLevel())
        {
            auto* LevelNode = NodeCast<Level>(RootNode);
            if (!LevelNode)
            {
                ClearAssetEditorState();
                return;
            }
            m_assetEditorTargetObject = LevelNode;
            m_assetEditorTargetType = StaticTypeId<Level>();
        }
        else
        {
            m_assetEditorTargetObject = RootNode;
            m_assetEditorTargetType = RootNode->TypeKey();
        }
    }

    if (m_assetEditorCanEditHierarchy && m_assetEditorHierarchyDirty)
    {
        RefreshAssetEditorHierarchy();
    }

    #if defined(SNAPI_GF_ENABLE_RENDERER)
    if (Asset->AssetKind == AssetKindMaterialInstance())
    {
        const std::string CurrentParentKey = m_assetEditorMaterialInstancePayload
            ? BuildAssetRefIdentity(m_assetEditorMaterialInstancePayload->ParentMaterial)
            : std::string{};
        if (CurrentParentKey != m_assetEditorMaterialInstanceDescriptorParentKey)
        {
            if (auto SyncResult = SyncMaterialInstanceEditorPayloadFromDescriptor(); !SyncResult)
            {
                m_statusMessage = "Material instance descriptor sync warning: " + SyncResult.error().Message;
            }
            else if (m_assetEditorRuntimeDirty)
            {
                if (auto CaptureResult = SyncAssetEditorRuntimeOverrideFromCurrentState(); !CaptureResult)
                {
                    m_statusMessage = "Asset editor dirty-state warning: " + CaptureResult.error();
                }
            }
            else
            {
                MarkAssetEditorRuntimeChanged(m_assetEditorSourceAssetType != TypeId{});
            }
        }
    }
    #endif
}

Result EditorAssetService::SaveActiveAssetEditor()
{
    if (m_assetEditorAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active asset editor to save"));
    }

    if (m_assetEditorSourceAssetType != TypeId{})
    {
        return SaveAssetByKey(m_assetEditorAssetKey);
    }

    const bool HadRuntimeDirty = m_assetEditorRuntimeDirty;
    const bool HadImportDirty = m_assetEditorImportSettingsDirty;

    if (HadRuntimeDirty)
    {
        if (auto SyncResult = SyncAssetEditorRuntimeOverrideFromCurrentState(); !SyncResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, SyncResult.error()));
        }

        Result SaveResult = SaveAssetByKey(m_assetEditorAssetKey);
        if (!SaveResult)
        {
            return SaveResult;
        }
    }

    if (m_assetEditorImportSettingsObject != nullptr && m_assetEditorImportSettingsType != TypeId{})
    {
        if (auto CurrentImportMetadata = BuildAssetEditorImportMetadataFromCurrentState(); CurrentImportMetadata.has_value())
        {
            if (!m_assetEditorImportMetadataBaseline.has_value() ||
                !ImportMetadataRecordsEqual(*CurrentImportMetadata, *m_assetEditorImportMetadataBaseline))
            {
                m_assetImportMetadata[m_assetEditorAssetId] = *CurrentImportMetadata;
                m_assetImportMetadataDirty = true;
                if (auto SaveMetadataResult = SaveAssetImportMetadataDatabase(); !SaveMetadataResult)
                {
                    return std::unexpected(MakeError(EErrorCode::InternalError, SaveMetadataResult.error()));
                }
                m_assetImportMetadataDirty = false;
                m_assetEditorImportMetadataBaseline = *CurrentImportMetadata;
                MarkAssetEditorImportSettingsSaved();
            }
        }
    }

    const bool WasDirty = m_assetEditorDirty;
    if (HadRuntimeDirty)
    {
        MarkAssetEditorRuntimeSaved();
    }

    RefreshAssetEditorDirtyFlags(false);
    if (m_assetEditorCanEditHierarchy)
    {
        RefreshAssetEditorHierarchy();
    }
    if (WasDirty || HadRuntimeDirty || HadImportDirty)
    {
        ++m_assetEditorSessionRevision;
    }
    if (!HadRuntimeDirty && HadImportDirty)
    {
        m_statusMessage = "Saved import settings for: " + m_assetEditorTitle;
    }
    return Ok();
}

Result EditorAssetService::SaveActiveAssetEditor(EditorServiceContext& Context)
{
    if (!m_assetEditorAssetKey.empty())
    {
        return SaveActiveAssetEditor();
    }

    auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
    if (!ConduitService)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Conduit editor service is not available"));
    }

    const auto WorkspaceView = ConduitService->ActiveWorkspaceView();
    if (!WorkspaceView.Open)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active asset editor or Conduit document to save"));
    }

    return SaveAssetByKey(Context, WorkspaceView.AssetKey);
}

EditorAssetService::AssetEditorSessionView EditorAssetService::AssetEditorSession() const
{
    AssetEditorSessionView View{};
    if (m_assetEditorAssetKey.empty())
    {
        return View;
    }
    const bool HasRuntimeTarget = HasAssetEditorRuntimeTarget();
    const bool HasImportTarget = m_assetEditorImportSettingsObject != nullptr && m_assetEditorImportSettingsType != TypeId{};
    if (!HasRuntimeTarget && !HasImportTarget)
    {
        return View;
    }

    View.IsOpen = true;
    View.AssetKey = m_assetEditorAssetKey;
    View.Title = m_assetEditorTitle;
    View.TargetType = m_assetEditorTargetType;
    View.TargetObject = ResolveAssetEditorRuntimeTargetObject();
    View.ImportSettingsType = m_assetEditorImportSettingsType;
    View.ImportSettingsObject = m_assetEditorImportSettingsObject;
    View.Nodes = m_assetEditorHierarchy;
    View.SelectedNode = m_assetEditorSelectedNode;
    View.CanEditHierarchy = m_assetEditorCanEditHierarchy;
    View.HasImportSettings = HasImportTarget;
    View.RuntimeDirty = m_assetEditorRuntimeDirty;
    View.ImportSettingsDirty = m_assetEditorImportSettingsDirty;
    View.IsDirty = m_assetEditorDirty;
    View.CanSave = m_assetEditorCanSave;
    View.CanReimport = m_assetEditorCanReimport;
    if (m_assetEditorTextureCookedInfo.has_value())
    {
        const auto& Cooked = *m_assetEditorTextureCookedInfo;
        View.HasTexturePreviewStats = true;
        View.TexturePreviewWidth = Cooked.BaseWidth;
        View.TexturePreviewHeight = Cooked.BaseHeight;
        View.TexturePreviewMipCount = (Cooked.MipCount > 0u)
            ? Cooked.MipCount
            : static_cast<std::uint32_t>(Cooked.MipLevels.size());
        View.TexturePreviewTarget = CompressionTargetName(ResolveCookedCompressionTarget(Cooked));
        View.TexturePreviewFormat = TextureCompressorPlugin::GetFormatName(Cooked.Format);
        View.TexturePreviewGpuSizeBytes = ComputeTextureGpuSizeBytes(Cooked);
    }
    return View;
}

Result EditorAssetService::InstantiateArmedAsset(EditorServiceContext& Context)
{
    if (m_placementAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "No placement-armed asset"));
    }

    const std::string PlacementKey = m_placementAssetKey;
    auto InstantiateResult = InstantiateAssetByKey(Context, PlacementKey);
    if (!InstantiateResult)
    {
        m_statusMessage = "Placement failed: " + InstantiateResult.error().Message;
        return InstantiateResult;
    }

    m_placementAssetKey.clear();
    return Ok();
}

Result EditorAssetService::InstantiateAssetByKey(EditorServiceContext& Context, const std::string_view Key)
{
    const DiscoveredAsset* Asset = FindAssetByKey(Key);
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset was not found for instantiation"));
    }

    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    if (Asset->AssetKind == AssetKindLevel())
    {
        return InstantiateLevelAsset(Context, *Asset);
    }
    if (Asset->AssetKind == AssetKindNode())
    {
        return InstantiateNodeAsset(Context, *Asset);
    }
    if (Asset->AssetKind == AssetKindWorld())
    {
        return InstantiateWorldAsset(Context, *Asset);
    }

    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported asset kind for instantiation"));
}

Result EditorAssetService::RebuildAssetManager()
{
    m_assetRenameOverrides.clear();
    m_assetPayloadOverrides.clear();
    m_assetImportMetadata.clear();
    m_assetImportMetadataPath.clear();
    m_assetImportMetadataDirty = false;
    m_selectedAssetKey.clear();
    m_placementAssetKey.clear();
    m_previewSummary.clear();
    ClearAssetEditorState();

    ClearDefaultAssetManagerResolver();
    m_assetManager.reset();

    ::SnAPI::AssetPipeline::AssetManagerConfig Config{};
    Config.bEnableSourceAssets = true;
    if (const std::filesystem::path AssetRoot = ResolveImportAssetRootDirectory(m_currentProject); !AssetRoot.empty())
    {
        Config.SourceRoots.push_back(::SnAPI::AssetPipeline::SourceMountConfig{
            .RootPath = AssetRoot.string(),
            .Priority = 0,
            .MountPoint = "",
        });
    }
    Config.PackSearchPaths = BuildPackSearchPaths();
    m_assetManager = std::make_unique<::SnAPI::AssetPipeline::AssetManager>(Config);
    SetDefaultAssetManagerResolver([this]() -> ::SnAPI::AssetPipeline::AssetManager* {
        return m_assetManager.get();
    });

    RegisterAssetPipelinePayloads(m_assetManager->GetRegistry());
    RegisterAssetPipelineFactories(*m_assetManager);
    RegisterAssetPipelineSourceStages(*m_assetManager);

    auto RefreshResult = RefreshDiscovery();
    if (!RefreshResult)
    {
        return RefreshResult;
    }

    if (auto LoadMetadataResult = LoadAssetImportMetadataDatabase(); !LoadMetadataResult)
    {
        if (!m_statusMessage.empty())
        {
            m_statusMessage += ' ';
        }
        m_statusMessage += "Import metadata load warning: " + LoadMetadataResult.error();
    }
    else
    {
        (void)RefreshDiscovery();
    }

    return Ok();
}

Result EditorAssetService::EnsureEditorTemplateAssets(EditorServiceContext& Context)
{
    (void)TypeAutoRegistry::Instance().EnsureAll();

    m_editorTemplateAssetDirectory = EditorDefaultShapeAssetDirectory();
    if (m_editorTemplateAssetDirectory.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Unable to resolve editor template asset directory"));
    }

    std::error_code Error{};
    std::filesystem::create_directories(m_editorTemplateAssetDirectory, Error);
    if (Error)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create editor template asset directory: " + Error.message()));
    }

    const std::filesystem::path SourceEditorAssetDirectory = ResolveEditorAssetSourceDirectory();
    if (!SourceEditorAssetDirectory.empty())
    {
        if (auto CopyResult = CopyDirectoryContentsRecursive(SourceEditorAssetDirectory, m_editorTemplateAssetDirectory); !CopyResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, CopyResult.error()));
        }
    }

    const std::filesystem::path RendererShaderSourceDirectory = ResolveRendererShaderSourceDirectory();
    if (!RendererShaderSourceDirectory.empty())
    {
        const std::filesystem::path TemplateShaderDirectory = m_editorTemplateAssetDirectory / "Shaders";
        if (auto CopyResult = CopyDirectoryContentsRecursive(RendererShaderSourceDirectory, TemplateShaderDirectory); !CopyResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, CopyResult.error()));
        }
    }

    auto DefaultShapeResult = EnsureDefaultShapePacks(m_editorTemplateAssetDirectory, Context.Runtime().WorldPtr());
    if (!DefaultShapeResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, DefaultShapeResult.error()));
    }

    m_editorStarterLevelTemplateAssetPath = m_editorTemplateAssetDirectory / std::string(kEditorStarterLevelTemplateAssetFileName);
    Error.clear();
    const bool NeedStarterTemplateAsset = !std::filesystem::exists(m_editorStarterLevelTemplateAssetPath, Error) || Error;
    if (NeedStarterTemplateAsset)
    {
        Level* SourceLevel = nullptr;
        if (auto* RuntimeWorld = Context.Runtime().WorldPtr(); RuntimeWorld)
        {
            const std::vector<NodeHandle> Levels = RuntimeWorld->Levels();
            if (!Levels.empty())
            {
                NodeHandle SourceLevelHandle = Levels.front();
                SourceLevel = NodeCast<Level>(SourceLevelHandle.Borrowed());
                if (!SourceLevel)
                {
                    SourceLevel = NodeCast<Level>(SourceLevelHandle.BorrowedSlowByUuid());
                }
            }
        }

        TExpected<LevelAsset> StarterLevelAsset{};
        if (SourceLevel)
        {
            StarterLevelAsset = CaptureLevelAsset(*SourceLevel);
        }

        if (!StarterLevelAsset)
        {
            World ScratchWorld("Editor.StarterLevelTemplateScratch");
            auto LevelResult = ScratchWorld.CreateLevel("StarterLevel");
            if (!LevelResult)
            {
                return std::unexpected(LevelResult.error());
            }
            auto* ScratchLevel = NodeCast<Level>(LevelResult->Borrowed());
            if (!ScratchLevel)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create scratch level template"));
            }
            StarterLevelAsset = CaptureLevelAsset(*ScratchLevel);
            if (!StarterLevelAsset)
            {
                return std::unexpected(StarterLevelAsset.error());
            }
        }

        auto JsonResult = SerializeAuthoredAssetToJson(*StarterLevelAsset);
        if (!JsonResult)
        {
            return std::unexpected(JsonResult.error());
        }

        std::ofstream TemplateFile(m_editorStarterLevelTemplateAssetPath, std::ios::binary | std::ios::trunc);
        if (!TemplateFile.is_open())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to open starter level template source asset for write"));
        }
        TemplateFile.write(JsonResult->data(), static_cast<std::streamsize>(JsonResult->size()));
        if (!TemplateFile.good())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to write starter level template source asset"));
        }
    }

    m_editorStarterScriptTemplatePath = m_editorTemplateAssetDirectory / std::string(kEditorStarterScriptFileName);
    Error.clear();
    if (!std::filesystem::exists(m_editorStarterScriptTemplatePath, Error) || Error)
    {
        const std::filesystem::path SourceScript = ResolveEditorScriptTemplateSource();
        if (!SourceScript.empty())
        {
            std::filesystem::copy_file(SourceScript,
                                       m_editorStarterScriptTemplatePath,
                                       std::filesystem::copy_options::overwrite_existing,
                                       Error);
            if (Error)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError,
                                                 "Failed to copy editor starter script template: " + Error.message()));
            }
        }
    }

    return Ok();
}

Result EditorAssetService::EnsureProjectShaderDirectory(const std::filesystem::path& ProjectAssetRoot)
{
    if (ProjectAssetRoot.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project shader directory root is invalid"));
    }

    std::filesystem::path ShaderSourceDirectory = m_editorTemplateAssetDirectory / "Shaders";
    std::error_code Error{};
    if (!std::filesystem::exists(ShaderSourceDirectory, Error) || Error)
    {
        ShaderSourceDirectory = ResolveRendererShaderSourceDirectory();
    }

    Error.clear();
    if (!std::filesystem::exists(ShaderSourceDirectory, Error) || Error)
    {
        return Ok();
    }

    const std::filesystem::path ShaderDestinationDirectory = ProjectAssetRoot / "Shaders";
    if (auto CopyResult = CopyDirectoryContentsRecursive(ShaderSourceDirectory, ShaderDestinationDirectory); !CopyResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, CopyResult.error()));
    }

    return Ok();
}

Result EditorAssetService::EnsureProjectStarterLevelAsset(const std::filesystem::path& ProjectAssetRoot,
                                                         const std::filesystem::path& StartupAssetPath)
{
    (void)ProjectAssetRoot;

    if (ProjectAssetRoot.empty() || StartupAssetPath.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project starter asset path is invalid"));
    }

    std::error_code Error{};
    std::filesystem::create_directories(StartupAssetPath.parent_path(), Error);
    if (Error)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create project starter asset directory: " + Error.message()));
    }

    Error.clear();
    if (std::filesystem::exists(StartupAssetPath, Error) && !Error)
    {
        return Ok();
    }

    if (m_editorStarterLevelTemplateAssetPath.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Starter level template asset is not initialized"));
    }

    Error.clear();
    std::filesystem::copy_file(m_editorStarterLevelTemplateAssetPath,
                               StartupAssetPath,
                               std::filesystem::copy_options::overwrite_existing,
                               Error);
    if (Error)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to copy project starter level asset: " + Error.message()));
    }

    return Ok();
}

Result EditorAssetService::LoadProjectStartupLevelAsset(EditorServiceContext& Context, const std::filesystem::path& StartupAssetPath)
{
    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Asset manager is not initialized"));
    }

    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    WorldPtr->Clear();
    m_loadedDefaultRenderSettingsNode = {};
    m_defaultRenderSettingsApplyPending = false;
    m_defaultRenderSettingsLastPassGraphRevision = 0;

    std::error_code Error{};
    if (!std::filesystem::exists(StartupAssetPath, Error) || Error)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Project startup level asset was not found"));
    }

    const std::filesystem::path AssetRootPath = ResolveImportAssetRootDirectory(m_currentProject);
    const std::string LogicalName = BuildSourceLogicalName(AssetRootPath, StartupAssetPath);
    if (LogicalName.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Project startup level asset is not under the configured asset root"));
    }

    LevelAssetLoadParams LoadParams{};
    LoadParams.TargetWorld = WorldPtr;
    LoadParams.NameOverride = std::string("Level");
    auto LoadResult = m_assetManager->Load<Level>(LogicalName, LoadParams);
    if (!LoadResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, LoadResult.error()));
    }

    return Ok();
}

Result EditorAssetService::CreateProject(EditorServiceContext& Context,
                                         const std::string_view ProjectName,
                                         const std::string_view ParentDirectory)
{
    const std::string Name = TrimCopy(std::string(ProjectName));
    const std::string ParentDirectoryText = TrimCopy(std::string(ParentDirectory));
    if (Name.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project name cannot be empty"));
    }
    if (ParentDirectoryText.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project directory cannot be empty"));
    }

    std::error_code Error{};
    std::filesystem::path ParentPath(ParentDirectoryText);
    if (!ParentPath.is_absolute())
    {
        ParentPath = std::filesystem::absolute(ParentPath, Error);
        if (Error)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to resolve project directory: " + Error.message()));
        }
    }

    const std::filesystem::path ProjectRoot = ParentPath / Name;
    const std::filesystem::path AssetRoot = ProjectRoot / std::string(kDefaultProjectAssetRoot);
    const std::filesystem::path ProjectFilePath = ProjectRoot / std::string(kDefaultProjectFileName);
    const std::filesystem::path StartupAssetPath = AssetRoot / std::filesystem::path(kDefaultProjectStartupLevelAsset);

    std::filesystem::create_directories(AssetRoot, Error);
    if (Error)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create project directory: " + Error.message()));
    }

    if (Result TemplateResult = EnsureEditorTemplateAssets(Context); !TemplateResult)
    {
        return TemplateResult;
    }

    if (Result StarterResult = EnsureProjectStarterLevelAsset(AssetRoot, StartupAssetPath); !StarterResult)
    {
        return StarterResult;
    }
    if (Result ShaderResult = EnsureProjectShaderDirectory(AssetRoot); !ShaderResult)
    {
        return ShaderResult;
    }

    if (!m_editorStarterScriptTemplatePath.empty())
    {
        const std::filesystem::path ProjectScriptPath = AssetRoot / std::string(kEditorStarterScriptFileName);
        Error.clear();
        if (!std::filesystem::exists(ProjectScriptPath, Error) || Error)
        {
            Error.clear();
            std::filesystem::copy_file(m_editorStarterScriptTemplatePath,
                                       ProjectScriptPath,
                                       std::filesystem::copy_options::overwrite_existing,
                                       Error);
            if (Error)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError,
                                                 "Failed to copy starter script into project assets: " + Error.message()));
            }
        }
    }

    if (auto WriteResult = WriteProjectConfigFile(ProjectFilePath,
                                                  Name,
                                                  std::string(kDefaultProjectAssetRoot),
                                                  std::string(kDefaultProjectStartupLevelAsset),
                                                  std::string{}); !WriteResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, WriteResult.error()));
    }

    auto LoadResult = LoadProject(Context, ProjectFilePath.string());
    if (!LoadResult)
    {
        return LoadResult;
    }

    m_statusMessage = "Created and loaded project: " + Name;
    return Ok();
}

Result EditorAssetService::LoadProject(EditorServiceContext& Context, const std::string_view ProjectFilePath)
{
    std::string ProjectFileText = TrimCopy(std::string(ProjectFilePath));
    if (ProjectFileText.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project file path cannot be empty"));
    }

    std::filesystem::path ProjectFile = std::filesystem::path(ProjectFileText);
    if (auto Resolved = SPathResolver::Instance().Resolve(ProjectFileText); Resolved)
    {
        ProjectFile = *Resolved;
    }
    else if (!ProjectFile.is_absolute())
    {
        std::error_code Error{};
        ProjectFile = std::filesystem::absolute(ProjectFile, Error);
        if (Error)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to resolve project file path: " + Error.message()));
        }
    }

    std::error_code Error{};
    if (!std::filesystem::exists(ProjectFile, Error) || Error)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Project file was not found: " + ProjectFile.string()));
    }

    std::ifstream Input(ProjectFile, std::ios::binary);
    if (!Input.is_open())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to open project file"));
    }
    std::ostringstream Buffer{};
    Buffer << Input.rdbuf();
    const std::string JsonText = Buffer.str();
    if (JsonText.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project file is empty"));
    }

    uint32_t Version = kProjectConfigVersion;
    (void)JsonTryReadUnsignedField(JsonText, "version", Version);
    if (Version != kProjectConfigVersion)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported project file version"));
    }

    std::string Name = ProjectFile.stem().string();
    (void)JsonTryReadStringField(JsonText, "name", Name);
    Name = TrimCopy(Name);
    if (Name.empty())
    {
        Name = "Project";
    }

    std::string AssetRootField = std::string(kDefaultProjectAssetRoot);
    (void)JsonTryReadStringField(JsonText, "assetRoot", AssetRootField);
    AssetRootField = TrimCopy(AssetRootField);
    if (AssetRootField.empty())
    {
        AssetRootField = std::string(kDefaultProjectAssetRoot);
    }
    else if (!HasUriScheme(AssetRootField))
    {
        AssetRootField = NormalizeProjectPathField(AssetRootField);
    }

    std::string StartupLevelAssetField{};
    (void)JsonTryReadStringField(JsonText, "startupLevelAsset", StartupLevelAssetField);
    StartupLevelAssetField = TrimCopy(StartupLevelAssetField);
    if (StartupLevelAssetField.empty())
    {
        std::string LegacyStartupLevelPack{};
        (void)JsonTryReadStringField(JsonText, "startupLevelPack", LegacyStartupLevelPack);
        LegacyStartupLevelPack = TrimCopy(LegacyStartupLevelPack);
        if (!LegacyStartupLevelPack.empty())
        {
            if (!HasUriScheme(LegacyStartupLevelPack))
            {
                LegacyStartupLevelPack = NormalizeProjectPathField(LegacyStartupLevelPack);
                std::filesystem::path LegacyPath = std::filesystem::path(LegacyStartupLevelPack);
                if (NormalizeAssetExtension(LegacyPath.extension().string()) == ".snpak")
                {
                    LegacyPath.replace_extension(".level");
                }
                StartupLevelAssetField = LegacyPath.lexically_normal().generic_string();
            }
            else
            {
                StartupLevelAssetField = LegacyStartupLevelPack;
            }
        }
    }
    if (StartupLevelAssetField.empty())
    {
        StartupLevelAssetField = std::string(kDefaultProjectStartupLevelAsset);
    }
    else if (!HasUriScheme(StartupLevelAssetField))
    {
        StartupLevelAssetField = NormalizeProjectPathField(StartupLevelAssetField);
    }

    std::string DefaultRenderSettingsField{};
    (void)JsonTryReadStringField(JsonText, "defaultRenderSettings", DefaultRenderSettingsField);
    DefaultRenderSettingsField = TrimCopy(DefaultRenderSettingsField);

    const std::filesystem::path ProjectRoot = ProjectFile.parent_path();
    std::filesystem::path ResolvedAssetRoot = std::filesystem::path(AssetRootField);
    if (HasUriScheme(AssetRootField))
    {
        auto Resolved = SPathResolver::Instance().Resolve(AssetRootField);
        if (!Resolved)
        {
            return std::unexpected(Resolved.error());
        }
        ResolvedAssetRoot = *Resolved;
    }
    else if (!ResolvedAssetRoot.is_absolute())
    {
        ResolvedAssetRoot = ProjectRoot / ResolvedAssetRoot;
    }
    ResolvedAssetRoot = ResolvedAssetRoot.lexically_normal();

    std::filesystem::create_directories(ResolvedAssetRoot, Error);
    if (Error)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create project asset root directory: " + Error.message()));
    }

    if (Result SetRootResult = SPathResolver::Instance().SetAssetRoot(ResolvedAssetRoot); !SetRootResult)
    {
        return SetRootResult;
    }

    m_currentProject = {};
    m_currentProject.IsLoaded = true;
    m_currentProject.Name = Name;
    m_currentProject.ProjectFilePath = ProjectFile.string();
    m_currentProject.ProjectRootDirectory = ProjectRoot.string();
    m_currentProject.AssetRoot = AssetRootField;
    m_currentProject.AssetRootDirectory = ResolvedAssetRoot.string();
    m_currentProject.StartupLevelAsset = StartupLevelAssetField;
    m_currentProject.DefaultRenderSettingsAssetId = DefaultRenderSettingsField;

    std::filesystem::path StartupAssetPath = std::filesystem::path(StartupLevelAssetField);
    if (HasUriScheme(StartupLevelAssetField))
    {
        auto Resolved = SPathResolver::Instance().Resolve(StartupLevelAssetField);
        if (!Resolved)
        {
            return std::unexpected(Resolved.error());
        }
        StartupAssetPath = *Resolved;
    }
    else if (!StartupAssetPath.is_absolute())
    {
        StartupAssetPath = ResolvedAssetRoot / StartupAssetPath;
    }
    StartupAssetPath = StartupAssetPath.lexically_normal();

    if (Result TemplateResult = EnsureEditorTemplateAssets(Context); !TemplateResult)
    {
        return TemplateResult;
    }
    if (Result StarterResult = EnsureProjectStarterLevelAsset(ResolvedAssetRoot, StartupAssetPath); !StarterResult)
    {
        return StarterResult;
    }
    if (Result ShaderResult = EnsureProjectShaderDirectory(ResolvedAssetRoot); !ShaderResult)
    {
        return ShaderResult;
    }

    if (!m_editorStarterScriptTemplatePath.empty())
    {
        const std::filesystem::path ProjectScriptPath = ResolvedAssetRoot / std::string(kEditorStarterScriptFileName);
        Error.clear();
        if (!std::filesystem::exists(ProjectScriptPath, Error) || Error)
        {
            Error.clear();
            std::filesystem::copy_file(m_editorStarterScriptTemplatePath,
                                       ProjectScriptPath,
                                       std::filesystem::copy_options::overwrite_existing,
                                       Error);
            if (Error)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError,
                                                 "Failed to copy starter script into project assets: " + Error.message()));
            }
        }
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    ConfigureRendererShaderSearchRootForAssetRoot(Context.Runtime(), ResolvedAssetRoot);
#endif

    if (Result RebuildResult = RebuildAssetManager(); !RebuildResult)
    {
        return RebuildResult;
    }
    if (Result LoadStartupResult = LoadProjectStartupLevelAsset(Context, StartupAssetPath); !LoadStartupResult)
    {
        return LoadStartupResult;
    }
    if (Result LoadDefaultsResult = LoadProjectDefaultRenderSettings(Context); !LoadDefaultsResult)
    {
        return LoadDefaultsResult;
    }

    m_statusMessage = "Loaded project: " + m_currentProject.Name;
    return Ok();
}

Result EditorAssetService::SaveProjectSettings(EditorServiceContext& Context,
                                               const std::string_view ProjectName,
                                               const std::string_view StartupLevelAsset,
                                               const std::string_view DefaultRenderSettingsAssetId)
{
    if (!m_currentProject.IsLoaded || m_currentProject.ProjectFilePath.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "No loaded project to save settings for"));
    }

    const std::filesystem::path ProjectFilePath = std::filesystem::path(m_currentProject.ProjectFilePath).lexically_normal();
    const std::filesystem::path ProjectRoot = ProjectFilePath.parent_path();

    std::string NextName = TrimCopy(std::string(ProjectName));
    if (NextName.empty())
    {
        NextName = TrimCopy(m_currentProject.Name);
    }
    if (NextName.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project name cannot be empty"));
    }

    std::string NextAssetRoot = TrimCopy(m_currentProject.AssetRoot);
    if (NextAssetRoot.empty())
    {
        if (!m_currentProject.AssetRootDirectory.empty())
        {
            NextAssetRoot = ToProjectRelativePathField(m_currentProject.AssetRootDirectory, ProjectRoot);
        }
        if (NextAssetRoot.empty())
        {
            NextAssetRoot = std::string(kDefaultProjectAssetRoot);
        }
    }

    std::filesystem::path AssetRootPath = std::filesystem::path(m_currentProject.AssetRootDirectory);
    if (AssetRootPath.empty())
    {
        AssetRootPath = std::filesystem::path(NextAssetRoot);
        if (!HasUriScheme(NextAssetRoot) && !AssetRootPath.is_absolute())
        {
            AssetRootPath = ProjectRoot / AssetRootPath;
        }
    }
    AssetRootPath = AssetRootPath.lexically_normal();

    std::string NextStartupLevelAsset = TrimCopy(std::string(StartupLevelAsset));
    if (NextStartupLevelAsset.empty())
    {
        NextStartupLevelAsset = TrimCopy(m_currentProject.StartupLevelAsset);
    }
    if (NextStartupLevelAsset.empty())
    {
        NextStartupLevelAsset = std::string(kDefaultProjectStartupLevelAsset);
    }
    if (!HasUriScheme(NextStartupLevelAsset))
    {
        NextStartupLevelAsset = ToProjectRelativePathField(NextStartupLevelAsset, AssetRootPath);
    }

    std::string NextDefaultRenderSettingsAssetId = TrimCopy(std::string(DefaultRenderSettingsAssetId));
    if (NextDefaultRenderSettingsAssetId.empty())
    {
        NextDefaultRenderSettingsAssetId = TrimCopy(m_currentProject.DefaultRenderSettingsAssetId);
    }

    if (auto WriteResult = WriteProjectConfigFile(ProjectFilePath,
                                                  NextName,
                                                  NextAssetRoot,
                                                  NextStartupLevelAsset,
                                                  NextDefaultRenderSettingsAssetId); !WriteResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, WriteResult.error()));
    }

    m_currentProject.Name = std::move(NextName);
    m_currentProject.AssetRoot = std::move(NextAssetRoot);
    m_currentProject.StartupLevelAsset = std::move(NextStartupLevelAsset);
    m_currentProject.DefaultRenderSettingsAssetId = std::move(NextDefaultRenderSettingsAssetId);

    if (Result LoadDefaultsResult = LoadProjectDefaultRenderSettings(Context); !LoadDefaultsResult)
    {
        return LoadDefaultsResult;
    }

    m_statusMessage = "Saved project settings: " + m_currentProject.Name;
    return Ok();
}

Result EditorAssetService::LoadProjectDefaultRenderSettings(EditorServiceContext& Context)
{
#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto* RuntimeWorld = Context.Runtime().WorldPtr();
    if (!RuntimeWorld || !m_assetManager)
    {
        return Ok();
    }
    m_defaultRenderSettingsApplyPending = false;
    // Force one deferred re-apply in Tick() after initial load.
    // Editor viewport pass graphs can be registered after this call.
    m_defaultRenderSettingsLastPassGraphRevision = 0;

    const std::string DefaultSettingsAssetId = TrimCopy(m_currentProject.DefaultRenderSettingsAssetId);
    if (DefaultSettingsAssetId.empty())
    {
        WorldRenderSettingsRootSet ExistingRoots = CollectWorldRenderSettingsRoots(*RuntimeWorld);
        DestroyNodes(*RuntimeWorld, ExistingRoots.TransientRoots);
        m_loadedDefaultRenderSettingsNode = {};
        return Ok();
    }

    WorldRenderSettingsRootSet ExistingRoots = CollectWorldRenderSettingsRoots(*RuntimeWorld);
    if (!ExistingRoots.AuthoredRoots.empty())
    {
        DestroyNodes(*RuntimeWorld, ExistingRoots.TransientRoots);
        m_loadedDefaultRenderSettingsNode = {};
        return Ok();
    }

    if (!ExistingRoots.TransientRoots.empty())
    {
        m_loadedDefaultRenderSettingsNode = ExistingRoots.TransientRoots.front();
        for (std::size_t Index = 1; Index < ExistingRoots.TransientRoots.size(); ++Index)
        {
            NodeHandle Duplicate = ExistingRoots.TransientRoots[Index];
            (void)RuntimeWorld->DestroyNode(Duplicate);
        }

        if (auto* ExistingNode = RuntimeWorld->BorrowedNode(m_loadedDefaultRenderSettingsNode);
            NodeCast<WorldRenderSettings>(ExistingNode) != nullptr)
        {
            ExistingNode->EditorTransient(true);
            m_defaultRenderSettingsApplyPending = true;
            return Ok();
        }

        m_loadedDefaultRenderSettingsNode = {};
    }

    TAssetRef<WorldRenderSettings> SettingsRef{};
    if (const ::SnAPI::AssetPipeline::AssetId ParsedAssetId = ::SnAPI::AssetPipeline::AssetId::FromString(DefaultSettingsAssetId);
        !ParsedAssetId.IsNull())
    {
        if (const auto It = std::ranges::find_if(m_assets, [&ParsedAssetId](const DiscoveredAsset& Asset) {
            return Asset.AssetId == ParsedAssetId;
        }); It != m_assets.end())
        {
            SettingsRef.EditAssetName() = It->Name;
        }
    }
    SettingsRef.EditAssetId() = DefaultSettingsAssetId;
    auto InstantiateResult = SettingsRef.Instantiate(*m_assetManager, *RuntimeWorld);
    if (!InstantiateResult)
    {
        m_statusMessage = "Default render settings load failed: " + InstantiateResult.error();
        m_defaultRenderSettingsApplyPending = false;
    }
    else
    {
        m_loadedDefaultRenderSettingsNode = *InstantiateResult;
        if (auto* CreatedNode = RuntimeWorld->BorrowedNode(m_loadedDefaultRenderSettingsNode);
            NodeCast<WorldRenderSettings>(CreatedNode) != nullptr)
        {
            CreatedNode->EditorTransient(true);
            // Apply immediately for already-ready pass graphs.
            (void)RuntimeWorld->RequestNodeOnCreate(m_loadedDefaultRenderSettingsNode);
            // Also schedule one deferred apply when the pass graph revision is available/stable.
            m_defaultRenderSettingsApplyPending = true;
        }
        else
        {
            m_loadedDefaultRenderSettingsNode = {};
            m_defaultRenderSettingsApplyPending = false;
        }
    }
#else
    (void)Context;
#endif
    return Ok();
}

std::filesystem::path EditorAssetService::ResolveImportMetadataPath() const
{
    std::filesystem::path AssetRoot{};
    if (m_currentProject.IsLoaded && !m_currentProject.AssetRootDirectory.empty())
    {
        AssetRoot = std::filesystem::path(m_currentProject.AssetRootDirectory);
    }
    else if (const std::filesystem::path ResolverAssetRoot = SPathResolver::Instance().AssetRoot();
             !ResolverAssetRoot.empty())
    {
        AssetRoot = ResolverAssetRoot;
    }

    if (AssetRoot.empty())
    {
        return {};
    }

    std::error_code Error{};
    auto Canonical = std::filesystem::weakly_canonical(AssetRoot, Error);
    if (!Error)
    {
        AssetRoot = std::move(Canonical);
    }
    else
    {
        Error.clear();
        auto Absolute = std::filesystem::absolute(AssetRoot, Error);
        if (!Error)
        {
            AssetRoot = std::move(Absolute);
        }
    }

    return AssetRoot /
        std::filesystem::path(std::string(kAssetImportMetadataDirectoryName)) /
        std::filesystem::path(std::string(kAssetImportMetadataFileName));
}

std::expected<void, std::string> EditorAssetService::LoadAssetImportMetadataDatabase()
{
    m_assetImportMetadata.clear();
    m_assetImportMetadataDirty = false;
    m_assetImportMetadataPath = ResolveImportMetadataPath();
    if (m_assetImportMetadataPath.empty())
    {
        return {};
    }

    std::error_code Error{};
    if (!std::filesystem::exists(m_assetImportMetadataPath, Error))
    {
        return {};
    }
    if (Error)
    {
        return std::unexpected(
            "Failed to query import metadata file '" + m_assetImportMetadataPath.string() + "': " + Error.message());
    }

    std::ifstream Input(m_assetImportMetadataPath, std::ios::binary);
    if (!Input.is_open())
    {
        return std::unexpected("Failed to open import metadata file: " + m_assetImportMetadataPath.string());
    }

    AssetImportMetadataDatabaseDisk Database{};
    bool Loaded = false;
    try
    {
        cereal::JSONInputArchive Archive(Input);
        Archive(cereal::make_nvp("Database", Database));
        Loaded = true;
    }
    catch (...)
    {
        Input.clear();
        Input.seekg(0, std::ios::beg);
        try
        {
            cereal::JSONInputArchive Archive(Input);
            Archive(Database);
            Loaded = true;
        }
        catch (...)
        {
            Loaded = false;
        }
    }

    if (!Loaded)
    {
        return std::unexpected(
            "Failed to parse import metadata file '" + m_assetImportMetadataPath.string() + "'");
    }

    if (Database.Version != kAssetImportMetadataVersion)
    {
        return std::unexpected(
            "Unsupported import metadata version " + std::to_string(Database.Version));
    }

    for (const auto& DiskEntry : Database.Entries)
    {
        const ::SnAPI::AssetPipeline::AssetId AssetId = ::SnAPI::AssetPipeline::AssetId::FromString(DiskEntry.AssetId);
        if (AssetId.IsNull())
        {
            continue;
        }

        AssetImportMetadataEntry Entry{};
        Entry.Profile = ImportProfileFromString(DiskEntry.Profile);
        Entry.SourcePath = DiskEntry.SourcePath;
        Entry.DestinationFolder = NormalizeAssetLogicalName(DiskEntry.DestinationFolder);
        Entry.ImporterName = DiskEntry.ImporterName;
        Entry.BuildOptions = DiskEntry.BuildOptions;
        Entry.Assimp = DiskEntry.Assimp;
        Entry.Texture = DiskEntry.Texture;

        if (Entry.Profile == EImportProfile::Unknown)
        {
            Entry.Profile = ImportProfileFromImporterName(Entry.ImporterName);
        }
        if (Entry.Profile == EImportProfile::AssimpModel && Entry.BuildOptions.empty())
        {
            Entry.BuildOptions = BuildOptionsFromAssimpImportSettings(Entry.Assimp);
        }
        if (Entry.Profile == EImportProfile::Texture && Entry.BuildOptions.empty())
        {
            Entry.BuildOptions = BuildOptionsFromTextureImportSettings(Entry.Texture);
        }

        m_assetImportMetadata[AssetId] = std::move(Entry);
    }

    return {};
}

std::expected<void, std::string> EditorAssetService::SaveAssetImportMetadataDatabase() const
{
    if (m_assetImportMetadataPath.empty())
    {
        return {};
    }

    std::error_code Error{};
    const std::filesystem::path ParentPath = m_assetImportMetadataPath.parent_path();
    if (!ParentPath.empty())
    {
        std::filesystem::create_directories(ParentPath, Error);
        if (Error)
        {
            return std::unexpected(
                "Failed to create import metadata directory '" + ParentPath.string() + "': " + Error.message());
        }
    }

    AssetImportMetadataDatabaseDisk Database{};
    Database.Version = kAssetImportMetadataVersion;
    Database.Entries.reserve(m_assetImportMetadata.size());
    for (const auto& [AssetId, Entry] : m_assetImportMetadata)
    {
        AssetImportMetadataEntryDisk DiskEntry{};
        DiskEntry.AssetId = AssetId.ToString();
        DiskEntry.SourcePath = Entry.SourcePath;
        DiskEntry.DestinationFolder = Entry.DestinationFolder;
        DiskEntry.ImporterName = Entry.ImporterName;
        DiskEntry.Profile = ImportProfileToString(Entry.Profile);
        DiskEntry.BuildOptions = Entry.BuildOptions;
        DiskEntry.Assimp = Entry.Assimp;
        DiskEntry.Texture = Entry.Texture;
        Database.Entries.push_back(std::move(DiskEntry));
    }

    std::sort(
        Database.Entries.begin(),
        Database.Entries.end(),
        [](const AssetImportMetadataEntryDisk& Left, const AssetImportMetadataEntryDisk& Right) {
            return Left.AssetId < Right.AssetId;
        });

    const std::filesystem::path TempPath = m_assetImportMetadataPath.string() + ".tmp";
    std::ofstream Output(TempPath, std::ios::binary | std::ios::trunc);
    if (!Output.is_open())
    {
        return std::unexpected("Failed to open temp import metadata file: " + TempPath.string());
    }

    try
    {
        cereal::JSONOutputArchive Archive(Output);
        Archive(cereal::make_nvp("Database", Database));
    }
    catch (...)
    {
        return std::unexpected(
            "Failed to serialize import metadata to file: " + TempPath.string());
    }

    Output.flush();
    if (!Output.good())
    {
        return std::unexpected("Failed to flush import metadata file: " + TempPath.string());
    }
    Output.close();
    if (!Output.good())
    {
        return std::unexpected("Failed to close import metadata file: " + TempPath.string());
    }

    Error.clear();
    std::filesystem::rename(TempPath, m_assetImportMetadataPath, Error);
    if (Error)
    {
        Error.clear();
        std::filesystem::remove(m_assetImportMetadataPath, Error);
        Error.clear();
        std::filesystem::rename(TempPath, m_assetImportMetadataPath, Error);
    }

    if (Error)
    {
        return std::unexpected(
            "Failed to commit import metadata file '" + m_assetImportMetadataPath.string() + "': " + Error.message());
    }

    return {};
}

void EditorAssetService::ApplyImportedAssetProvenanceToMetadata(const ImportedAssetProvenancePayload& Provenance,
                                                                AssetImportMetadataEntry& Metadata)
{
    if (!Provenance.Profile.empty())
    {
        Metadata.Profile = ImportProfileFromString(Provenance.Profile);
    }
    if (!Provenance.SourcePath.empty())
    {
        Metadata.SourcePath = Provenance.SourcePath;
    }
    if (!Provenance.DestinationFolder.empty())
    {
        Metadata.DestinationFolder = NormalizeAssetLogicalName(Provenance.DestinationFolder);
    }
    if (!Provenance.ImporterName.empty())
    {
        Metadata.ImporterName = Provenance.ImporterName;
    }
    if (!Provenance.BuildOptions.empty())
    {
        Metadata.BuildOptions = BuildImportBuildOptionMap(Provenance.BuildOptions);
    }
}

Result EditorAssetService::SyncImportedAssetGroupProvenance(const DiscoveredAsset& ActiveAsset,
                                                            const AssetImportMetadataEntry& Metadata)
{
    if (!IsImportedSourceAssetType(ActiveAsset.AssetType) ||
        Metadata.SourcePath.empty() ||
        Metadata.Profile == EImportProfile::Unknown)
    {
        return Ok();
    }

    const ImportedAssetProvenancePayload Provenance = BuildImportedAssetProvenancePayload(
        Metadata.SourcePath,
        Metadata.DestinationFolder,
        Metadata.ImporterName,
        Metadata.Profile,
        Metadata.BuildOptions);

    for (const DiscoveredAsset& Candidate : m_assets)
    {
        if (Candidate.AssetId == ActiveAsset.AssetId ||
            Candidate.SourceFilePath.empty() ||
            Candidate.AssetType == TypeId{} ||
            !IsImportedSourceAssetType(Candidate.AssetType))
        {
            continue;
        }

        auto LoadedAsset = LoadImportedSourceAssetVariant(Candidate);
        if (!LoadedAsset)
        {
            return std::unexpected(MakeError(
                EErrorCode::InternalError,
                "Failed to load imported source asset '" + Candidate.Key + "' for provenance sync: " + LoadedAsset.error()));
        }
        if (!ImportedSourceAssetMatchesGroup(*LoadedAsset, Metadata.SourcePath, Metadata.DestinationFolder, Metadata.Profile))
        {
            continue;
        }

        if (UpdateImportedSourceAssetProvenance(*LoadedAsset, Provenance))
        {
            auto JsonResult = SerializeImportedSourceAssetJson(*LoadedAsset);
            if (!JsonResult)
            {
                return std::unexpected(MakeError(
                    EErrorCode::InternalError,
                    "Failed to serialize imported source asset '" + Candidate.Key + "' during provenance sync: " +
                        JsonResult.error()));
            }

            const std::filesystem::path SourcePath = std::filesystem::path(Candidate.SourceFilePath).lexically_normal();
            std::ofstream File(SourcePath, std::ios::binary | std::ios::trunc);
            if (!File.is_open())
            {
                return std::unexpected(MakeError(
                    EErrorCode::InternalError,
                    "Failed to open imported source asset for provenance sync: " + SourcePath.string()));
            }

            File.write(JsonResult->data(), static_cast<std::streamsize>(JsonResult->size()));
            if (!File.good())
            {
                return std::unexpected(MakeError(
                    EErrorCode::InternalError,
                    "Failed to write imported source asset during provenance sync: " + SourcePath.string()));
            }
        }

        AssetImportMetadataEntry SyncedMetadata = Metadata;
        if (Metadata.Profile == EImportProfile::AssimpModel)
        {
            SyncedMetadata.Assimp = Metadata.Assimp;
        }
        else if (Metadata.Profile == EImportProfile::Texture)
        {
            SyncedMetadata.Texture = Metadata.Texture;
        }
        m_assetImportMetadata[Candidate.AssetId] = std::move(SyncedMetadata);
    }

    return Ok();
}

bool EditorAssetService::RefreshAssetEditorImportSettingsBinding(const DiscoveredAsset& Asset)
{
    ClearAssetEditorImportSettingsBinding();

    AssetImportMetadataEntry Metadata{};
    bool HasMetadata = false;
    const auto MetadataIt = m_assetImportMetadata.find(Asset.AssetId);
    if (MetadataIt != m_assetImportMetadata.end())
    {
        Metadata = MetadataIt->second;
        HasMetadata = true;
    }

    if (Metadata.Profile == EImportProfile::Unknown)
    {
        Metadata.Profile = ImportProfileFromImporterName(Metadata.ImporterName);
    }

    bool PopulatedFromSource = false;
    if (Asset.Key == m_assetEditorAssetKey)
    {
        PopulatedFromSource = PopulateImportSettingsBindingFromActiveSourceAsset(Metadata);
    }

    if (!PopulatedFromSource && !HasMetadata)
    {
        return false;
    }

    if (m_assetEditorImportSettingsObject == nullptr && Metadata.Profile == EImportProfile::AssimpModel)
    {
        if (auto Typed = BuildTypedImportSettingsForRecord(Metadata);
            Typed && dynamic_cast<const AssimpImporterSettings*>(Typed.get()))
        {
            FillAssimpImportSettingsFromTyped(*static_cast<const AssimpImporterSettings*>(Typed.get()), Metadata.Assimp);
        }
        if (Metadata.BuildOptions.empty())
        {
            Metadata.BuildOptions = BuildOptionsFromAssimpImportSettings(Metadata.Assimp);
            Metadata.BuildOptions["SnAPI.GF.Assimp.DefaultShaderModule"] = std::string(kDefaultMaterialShaderModule);
            Metadata.BuildOptions["SnAPI.GF.Assimp.DefaultShadingModel"] = std::string(kDefaultMaterialShadingModel);
        }

        m_assetEditorAssimpImportSettings = Metadata.Assimp;
        m_assetEditorImportSettingsType = StaticTypeId<Editor::AssimpImportSettings>();
        m_assetEditorImportSettingsObject = &(*m_assetEditorAssimpImportSettings);
    }
    else if (m_assetEditorImportSettingsObject == nullptr && Metadata.Profile == EImportProfile::Texture)
    {
        if (auto Typed = BuildTypedImportSettingsForRecord(Metadata);
            Typed && dynamic_cast<const TextureCompressorPlugin::TextureCompressorImportSettings*>(Typed.get()))
        {
            FillTextureImportSettingsFromTyped(
                *static_cast<const TextureCompressorPlugin::TextureCompressorImportSettings*>(Typed.get()),
                Metadata.Texture);
        }
        if (Metadata.BuildOptions.empty())
        {
            Metadata.BuildOptions = BuildOptionsFromTextureImportSettings(Metadata.Texture);
        }

        m_assetEditorTextureImportSettings = Metadata.Texture;
        m_assetEditorImportSettingsType = StaticTypeId<Editor::TextureImportSettings>();
        m_assetEditorImportSettingsObject = &(*m_assetEditorTextureImportSettings);
    }

    if (m_assetEditorImportSettingsObject == nullptr || m_assetEditorImportSettingsType == TypeId{})
    {
        return false;
    }

    m_assetEditorImportMetadataBaseline = std::move(Metadata);
    m_assetEditorImportSettingsDirty = false;
    m_assetEditorCanReimport = !m_assetEditorImportMetadataBaseline->SourcePath.empty();
    if (m_assetEditorCanReimport)
    {
        std::error_code Error{};
        std::filesystem::path SourcePath = std::filesystem::path(m_assetEditorImportMetadataBaseline->SourcePath);
        if (auto Resolved = SPathResolver::Instance().Resolve(m_assetEditorImportMetadataBaseline->SourcePath); Resolved)
        {
            SourcePath = *Resolved;
        }
        m_assetEditorCanReimport = std::filesystem::exists(SourcePath, Error) && !Error;
    }
    return true;
}

bool EditorAssetService::PopulateImportSettingsBindingFromActiveSourceAsset(AssetImportMetadataEntry& InOutMetadata)
{
    if (m_assetEditorSourceAssetType == TypeId{} || !m_assetEditorGenericSourceObject)
    {
        return false;
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<TextureAsset>())
    {
        auto* Asset = static_cast<TextureAsset*>(m_assetEditorGenericSourceObject.get());
        if (!Asset)
        {
            return false;
        }

        ApplyImportedAssetProvenanceToMetadata(Asset->Provenance, InOutMetadata);
        Editor::TextureImportSettings Settings{};
        FillTextureImportSettingsFromPayload(Asset->ImportSettings, Settings);
        InOutMetadata.Profile = EImportProfile::Texture;
        InOutMetadata.Texture = Settings;
        RemoveManagedBuildOptions(InOutMetadata.BuildOptions, kTextureManagedBuildOptionKeys);
        for (const auto& [OptionKey, OptionValue] : BuildOptionsFromTextureImportSettings(Settings))
        {
            InOutMetadata.BuildOptions[OptionKey] = OptionValue;
        }
        if (InOutMetadata.ImporterName.empty())
        {
            InOutMetadata.ImporterName = "TextureCompressor.Importer";
        }

        m_assetEditorTextureImportSettings = Settings;
        m_assetEditorImportSettingsType = StaticTypeId<Editor::TextureImportSettings>();
        m_assetEditorImportSettingsObject = &(*m_assetEditorTextureImportSettings);
        return true;
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<StaticMeshAsset>())
    {
        auto* Asset = static_cast<StaticMeshAsset*>(m_assetEditorGenericSourceObject.get());
        if (!Asset)
        {
            return false;
        }

        ApplyImportedAssetProvenanceToMetadata(Asset->Provenance, InOutMetadata);
        Editor::AssimpImportSettings Settings{};
        FillAssimpImportSettingsFromPayload(Asset->ImportSettings, Settings);
        InOutMetadata.Profile = EImportProfile::AssimpModel;
        InOutMetadata.Assimp = Settings;
        RemoveManagedBuildOptions(InOutMetadata.BuildOptions, kAssimpManagedBuildOptionKeys);
        for (const auto& [OptionKey, OptionValue] : BuildOptionsFromAssimpImportSettings(Settings))
        {
            InOutMetadata.BuildOptions[OptionKey] = OptionValue;
        }
        InOutMetadata.BuildOptions["SnAPI.GF.Assimp.DefaultShaderModule"] = OptionValueOr(
            InOutMetadata.BuildOptions,
            "SnAPI.GF.Assimp.DefaultShaderModule",
            kDefaultMaterialShaderModule);
        InOutMetadata.BuildOptions["SnAPI.GF.Assimp.DefaultShadingModel"] = OptionValueOr(
            InOutMetadata.BuildOptions,
            "SnAPI.GF.Assimp.DefaultShadingModel",
            kDefaultMaterialShadingModel);
        if (InOutMetadata.ImporterName.empty())
        {
            InOutMetadata.ImporterName = "SnAPI.GameFramework.RenderAssetAssimpImporter";
        }

        m_assetEditorAssimpImportSettings = Settings;
        m_assetEditorImportSettingsType = StaticTypeId<Editor::AssimpImportSettings>();
        m_assetEditorImportSettingsObject = &(*m_assetEditorAssimpImportSettings);
        return true;
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<SkeletalMeshAsset>())
    {
        auto* Asset = static_cast<SkeletalMeshAsset*>(m_assetEditorGenericSourceObject.get());
        if (!Asset)
        {
            return false;
        }

        ApplyImportedAssetProvenanceToMetadata(Asset->Provenance, InOutMetadata);
        Editor::AssimpImportSettings Settings{};
        FillAssimpImportSettingsFromPayload(Asset->BaseMesh.ImportSettings, Settings);
        InOutMetadata.Profile = EImportProfile::AssimpModel;
        InOutMetadata.Assimp = Settings;
        RemoveManagedBuildOptions(InOutMetadata.BuildOptions, kAssimpManagedBuildOptionKeys);
        for (const auto& [OptionKey, OptionValue] : BuildOptionsFromAssimpImportSettings(Settings))
        {
            InOutMetadata.BuildOptions[OptionKey] = OptionValue;
        }
        InOutMetadata.BuildOptions["SnAPI.GF.Assimp.DefaultShaderModule"] = OptionValueOr(
            InOutMetadata.BuildOptions,
            "SnAPI.GF.Assimp.DefaultShaderModule",
            kDefaultMaterialShaderModule);
        InOutMetadata.BuildOptions["SnAPI.GF.Assimp.DefaultShadingModel"] = OptionValueOr(
            InOutMetadata.BuildOptions,
            "SnAPI.GF.Assimp.DefaultShadingModel",
            kDefaultMaterialShadingModel);
        if (InOutMetadata.ImporterName.empty())
        {
            InOutMetadata.ImporterName = "SnAPI.GameFramework.RenderAssetAssimpImporter";
        }

        m_assetEditorAssimpImportSettings = Settings;
        m_assetEditorImportSettingsType = StaticTypeId<Editor::AssimpImportSettings>();
        m_assetEditorImportSettingsObject = &(*m_assetEditorAssimpImportSettings);
        return true;
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<MaterialAsset>())
    {
        auto* Asset = static_cast<MaterialAsset*>(m_assetEditorGenericSourceObject.get());
        if (!Asset)
        {
            return false;
        }
        ApplyImportedAssetProvenanceToMetadata(Asset->Provenance, InOutMetadata);
        return !Asset->Provenance.Profile.empty() ||
               !Asset->Provenance.SourcePath.empty() ||
               !Asset->Provenance.ImporterName.empty();
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<MaterialInstanceAsset>())
    {
        auto* Asset = static_cast<MaterialInstanceAsset*>(m_assetEditorGenericSourceObject.get());
        if (!Asset)
        {
            return false;
        }
        ApplyImportedAssetProvenanceToMetadata(Asset->Provenance, InOutMetadata);
        return !Asset->Provenance.Profile.empty() ||
               !Asset->Provenance.SourcePath.empty() ||
               !Asset->Provenance.ImporterName.empty();
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<SkeletonAsset>())
    {
        auto* Asset = static_cast<SkeletonAsset*>(m_assetEditorGenericSourceObject.get());
        if (!Asset)
        {
            return false;
        }
        ApplyImportedAssetProvenanceToMetadata(Asset->Provenance, InOutMetadata);
        return !Asset->Provenance.Profile.empty() ||
               !Asset->Provenance.SourcePath.empty() ||
               !Asset->Provenance.ImporterName.empty();
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<SkeletalAnimationAsset>())
    {
        auto* Asset = static_cast<SkeletalAnimationAsset*>(m_assetEditorGenericSourceObject.get());
        if (!Asset)
        {
            return false;
        }
        ApplyImportedAssetProvenanceToMetadata(Asset->Provenance, InOutMetadata);
        return !Asset->Provenance.Profile.empty() ||
               !Asset->Provenance.SourcePath.empty() ||
               !Asset->Provenance.ImporterName.empty();
    }

    return false;
}

void EditorAssetService::ApplyAssetEditorImportSettingsToActiveSourceAsset()
{
    if (m_assetEditorSourceAssetType == TypeId{} ||
        !m_assetEditorGenericSourceObject ||
        m_assetEditorImportSettingsType == TypeId{} ||
        m_assetEditorImportSettingsObject == nullptr)
    {
        return;
    }

    const AssetImportMetadataEntry Metadata = BuildAssetEditorImportMetadataFromCurrentState()
        .value_or(m_assetEditorImportMetadataBaseline.value_or(AssetImportMetadataEntry{}));
    const ImportedAssetProvenancePayload Provenance = BuildImportedAssetProvenancePayload(
        Metadata.SourcePath,
        Metadata.DestinationFolder,
        Metadata.ImporterName,
        Metadata.Profile,
        Metadata.BuildOptions);

    if (m_assetEditorSourceAssetType == StaticTypeId<TextureAsset>() &&
        m_assetEditorImportSettingsType == StaticTypeId<Editor::TextureImportSettings>())
    {
        auto* Asset = static_cast<TextureAsset*>(m_assetEditorGenericSourceObject.get());
        const auto* Settings = static_cast<const Editor::TextureImportSettings*>(m_assetEditorImportSettingsObject);
        if (Asset && Settings)
        {
            Asset->ImportSettings = BuildTextureImportSettingsPayload(*Settings);
            Asset->Provenance = Provenance;
        }
        return;
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<StaticMeshAsset>() &&
        m_assetEditorImportSettingsType == StaticTypeId<Editor::AssimpImportSettings>())
    {
        auto* Asset = static_cast<StaticMeshAsset*>(m_assetEditorGenericSourceObject.get());
        const auto* Settings = static_cast<const Editor::AssimpImportSettings*>(m_assetEditorImportSettingsObject);
        if (Asset && Settings)
        {
            Asset->ImportSettings = BuildMeshImportSettingsPayload(*Settings);
            Asset->Provenance = Provenance;
        }
        return;
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<SkeletalMeshAsset>() &&
        m_assetEditorImportSettingsType == StaticTypeId<Editor::AssimpImportSettings>())
    {
        auto* Asset = static_cast<SkeletalMeshAsset*>(m_assetEditorGenericSourceObject.get());
        const auto* Settings = static_cast<const Editor::AssimpImportSettings*>(m_assetEditorImportSettingsObject);
        if (Asset && Settings)
        {
            Asset->BaseMesh.ImportSettings = BuildMeshImportSettingsPayload(*Settings);
            Asset->Provenance = Provenance;
        }
        return;
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<MaterialAsset>())
    {
        if (auto* Asset = static_cast<MaterialAsset*>(m_assetEditorGenericSourceObject.get()))
        {
            Asset->Provenance = Provenance;
        }
        return;
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<MaterialInstanceAsset>())
    {
        if (auto* Asset = static_cast<MaterialInstanceAsset*>(m_assetEditorGenericSourceObject.get()))
        {
            Asset->Provenance = Provenance;
        }
        return;
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<SkeletonAsset>())
    {
        if (auto* Asset = static_cast<SkeletonAsset*>(m_assetEditorGenericSourceObject.get()))
        {
            Asset->Provenance = Provenance;
        }
        return;
    }

    if (m_assetEditorSourceAssetType == StaticTypeId<SkeletalAnimationAsset>())
    {
        if (auto* Asset = static_cast<SkeletalAnimationAsset*>(m_assetEditorGenericSourceObject.get()))
        {
            Asset->Provenance = Provenance;
        }
    }
}

std::optional<EditorAssetService::AssetImportMetadataEntry> EditorAssetService::BuildAssetEditorImportMetadataFromCurrentState() const
{
    if (m_assetEditorImportSettingsObject == nullptr || m_assetEditorImportSettingsType == TypeId{})
    {
        return std::nullopt;
    }

    AssetImportMetadataEntry Metadata = m_assetEditorImportMetadataBaseline.value_or(AssetImportMetadataEntry{});
    const auto PreviousBuildOptions = m_assetEditorImportMetadataBaseline
        ? m_assetEditorImportMetadataBaseline->BuildOptions
        : Metadata.BuildOptions;

    if (m_assetEditorImportSettingsType == StaticTypeId<Editor::AssimpImportSettings>())
    {
        const auto* Settings = static_cast<const Editor::AssimpImportSettings*>(m_assetEditorImportSettingsObject);
        if (!Settings)
        {
            return std::nullopt;
        }

        Metadata.Profile = EImportProfile::AssimpModel;
        Metadata.Assimp = *Settings;
        Metadata.BuildOptions = PreviousBuildOptions;
        RemoveManagedBuildOptions(Metadata.BuildOptions, kAssimpManagedBuildOptionKeys);
        const auto TypedOptions = BuildOptionsFromAssimpImportSettings(*Settings);
        for (const auto& [OptionKey, OptionValue] : TypedOptions)
        {
            Metadata.BuildOptions[OptionKey] = OptionValue;
        }
        Metadata.BuildOptions["SnAPI.GF.Assimp.DefaultShaderModule"] = OptionValueOr(
            PreviousBuildOptions,
            "SnAPI.GF.Assimp.DefaultShaderModule",
            kDefaultMaterialShaderModule);
        Metadata.BuildOptions["SnAPI.GF.Assimp.DefaultShadingModel"] = OptionValueOr(
            PreviousBuildOptions,
            "SnAPI.GF.Assimp.DefaultShadingModel",
            kDefaultMaterialShadingModel);

        if (Metadata.ImporterName.empty())
        {
            Metadata.ImporterName = "SnAPI.GameFramework.RenderAssetAssimpImporter";
        }
        return Metadata;
    }

    if (m_assetEditorImportSettingsType == StaticTypeId<Editor::TextureImportSettings>())
    {
        const auto* Settings = static_cast<const Editor::TextureImportSettings*>(m_assetEditorImportSettingsObject);
        if (!Settings)
        {
            return std::nullopt;
        }

        Metadata.Profile = EImportProfile::Texture;
        Metadata.Texture = *Settings;
        Metadata.BuildOptions = PreviousBuildOptions;
        RemoveManagedBuildOptions(Metadata.BuildOptions, kTextureManagedBuildOptionKeys);
        const auto TypedOptions = BuildOptionsFromTextureImportSettings(*Settings);
        for (const auto& [OptionKey, OptionValue] : TypedOptions)
        {
            Metadata.BuildOptions[OptionKey] = OptionValue;
        }
        if (Metadata.ImporterName.empty())
        {
            Metadata.ImporterName = "TextureCompressor.Importer";
        }
        return Metadata;
    }

    return std::nullopt;
}

bool EditorAssetService::ImportMetadataRecordsEqual(const AssetImportMetadataEntry& Left,
                                                    const AssetImportMetadataEntry& Right) const
{
    const auto AssimpEqual = [](const Editor::AssimpImportSettings& A, const Editor::AssimpImportSettings& B) {
        return A.GenerateNormals == B.GenerateNormals &&
               A.GenerateTangents == B.GenerateTangents &&
               A.FlipUVs == B.FlipUVs &&
               A.OptimizeMeshes == B.OptimizeMeshes &&
               A.ForceSkeletal == B.ForceSkeletal &&
               A.ForceStatic == B.ForceStatic &&
               A.ImportMaterials == B.ImportMaterials &&
               A.ImportTextures == B.ImportTextures &&
               A.ImportAnimations == B.ImportAnimations &&
               A.ImportSkeleton == B.ImportSkeleton &&
               A.MaxBonesPerVertex == B.MaxBonesPerVertex;
    };
    const auto TextureEqual = [](const Editor::TextureImportSettings& A, const Editor::TextureImportSettings& B) {
        return A.Target == B.Target &&
               A.Format == B.Format &&
               A.Quality == B.Quality &&
               A.ForceSrgb == B.ForceSrgb &&
               A.ForceLinear == B.ForceLinear &&
               A.ForceNormalMap == B.ForceNormalMap &&
               A.MaxMips == B.MaxMips;
    };

    return Left.Profile == Right.Profile &&
           Left.SourcePath == Right.SourcePath &&
           Left.DestinationFolder == Right.DestinationFolder &&
           Left.ImporterName == Right.ImporterName &&
           Left.BuildOptions == Right.BuildOptions &&
           AssimpEqual(Left.Assimp, Right.Assimp) &&
           TextureEqual(Left.Texture, Right.Texture);
}

::SnAPI::AssetPipeline::AssetImportSettingsPtr EditorAssetService::BuildTypedImportSettingsForRecord(
    const AssetImportMetadataEntry& Record) const
{
    if (Record.Profile == EImportProfile::AssimpModel)
    {
        auto Settings = std::make_shared<AssimpImporterSettings>();
        Settings->Mesh.GenerateNormals = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "SnAPI.GF.Assimp.GenerateNormals",
                Record.Assimp.GenerateNormals ? "true" : "false"),
            Record.Assimp.GenerateNormals);
        Settings->Mesh.GenerateTangents = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "SnAPI.GF.Assimp.GenerateTangents",
                Record.Assimp.GenerateTangents ? "true" : "false"),
            Record.Assimp.GenerateTangents);
        Settings->Mesh.FlipUVs = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "SnAPI.GF.Assimp.FlipUVs",
                Record.Assimp.FlipUVs ? "true" : "false"),
            Record.Assimp.FlipUVs);
        Settings->Mesh.OptimizeMeshes = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "SnAPI.GF.Assimp.OptimizeMeshes",
                Record.Assimp.OptimizeMeshes ? "true" : "false"),
            Record.Assimp.OptimizeMeshes);
        Settings->Mesh.ForceSkeletal = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "SnAPI.GF.Assimp.ForceSkeletal",
                Record.Assimp.ForceSkeletal ? "true" : "false"),
            Record.Assimp.ForceSkeletal);
        Settings->Mesh.ForceStatic = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "SnAPI.GF.Assimp.ForceStatic",
                Record.Assimp.ForceStatic ? "true" : "false"),
            Record.Assimp.ForceStatic);
        Settings->Mesh.ImportMaterials = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "SnAPI.GF.Assimp.ImportMaterials",
                Record.Assimp.ImportMaterials ? "true" : "false"),
            Record.Assimp.ImportMaterials);
        Settings->Mesh.ImportTextures = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "SnAPI.GF.Assimp.ImportTextures",
                Record.Assimp.ImportTextures ? "true" : "false"),
            Record.Assimp.ImportTextures);
        Settings->Mesh.ImportAnimations = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "SnAPI.GF.Assimp.ImportAnimations",
                Record.Assimp.ImportAnimations ? "true" : "false"),
            Record.Assimp.ImportAnimations);
        Settings->Mesh.ImportSkeleton = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "SnAPI.GF.Assimp.ImportSkeleton",
                Record.Assimp.ImportSkeleton ? "true" : "false"),
            Record.Assimp.ImportSkeleton);
        Settings->Mesh.MaxBonesPerVertex = std::max(1u, Record.Assimp.MaxBonesPerVertex);
        if (const auto MaxBones = ParseIntOption(
                OptionValueOr(
                    Record.BuildOptions,
                    "SnAPI.GF.Assimp.MaxBonesPerVertex",
                    std::to_string(Settings->Mesh.MaxBonesPerVertex)));
            MaxBones.has_value())
        {
            Settings->Mesh.MaxBonesPerVertex = std::max(1, *MaxBones);
        }
        Settings->LogicalNameOverride = OptionValueOr(Record.BuildOptions, "SnAPI.GF.Assimp.LogicalName", "");
        Settings->DefaultShaderModule = OptionValueOr(
            Record.BuildOptions,
            "SnAPI.GF.Assimp.DefaultShaderModule",
            kDefaultMaterialShaderModule);
        Settings->DefaultShadingModel = OptionValueOr(
            Record.BuildOptions,
            "SnAPI.GF.Assimp.DefaultShadingModel",
            kDefaultMaterialShadingModel);
        return Settings;
    }

    if (Record.Profile == EImportProfile::Texture)
    {
        auto Settings = std::make_shared<TextureCompressorPlugin::TextureCompressorImportSettings>();
        const std::string TargetText = ToLowerCopy(OptionValueOr(
            Record.BuildOptions,
            "texture.target",
            Record.Texture.Target == Editor::ETextureCompressionTarget::ASTC ? "astc" : "bcn"));
        Settings->Target = TargetText == "astc"
            ? TextureCompressorPlugin::ECompressionTarget::ASTC
            : TextureCompressorPlugin::ECompressionTarget::BCn;

        const TextureCompressorPlugin::ECompressedFormat RequestedFormat = ParseTextureFormatOption(
            OptionValueOr(Record.BuildOptions, "texture.format", ""));
        Settings->Format = RequestedFormat == TextureCompressorPlugin::ECompressedFormat::Unknown
            ? ToCookedTextureFormat(Record.Texture.Format)
            : RequestedFormat;

        const std::string QualityText = OptionValueOr(
            Record.BuildOptions,
            "texture.quality",
            std::to_string(std::clamp(Record.Texture.Quality, 0.0f, 1.0f)));
        try
        {
            Settings->Quality = std::clamp(std::stof(QualityText), 0.0f, 1.0f);
        }
        catch (...)
        {
            Settings->Quality = std::clamp(Record.Texture.Quality, 0.0f, 1.0f);
        }

        Settings->ForceNormalMap = ParseBoolOption(
            OptionValueOr(
                Record.BuildOptions,
                "texture.normal_map",
                Record.Texture.ForceNormalMap ? "true" : "false"),
            Record.Texture.ForceNormalMap);

        Settings->MaxMipCount = Record.Texture.MaxMips > 0
            ? static_cast<int32_t>(Record.Texture.MaxMips)
            : 0;
        if (const auto MaxMips = ParseIntOption(OptionValueOr(
                Record.BuildOptions,
                "texture.max_mips",
                std::to_string(Settings->MaxMipCount)));
            MaxMips.has_value())
        {
            Settings->MaxMipCount = *MaxMips;
        }

        if (const auto SrgbText = OptionValueOr(Record.BuildOptions, "texture.srgb", "");
            !SrgbText.empty())
        {
            Settings->ColorSpacePolicy = ParseBoolOption(SrgbText, true)
                ? TextureCompressorPlugin::ETextureColorSpacePolicy::ForceSrgb
                : TextureCompressorPlugin::ETextureColorSpacePolicy::ForceLinear;
        }
        else if (Record.Texture.ForceLinear)
        {
            Settings->ColorSpacePolicy = TextureCompressorPlugin::ETextureColorSpacePolicy::ForceLinear;
        }
        else if (Record.Texture.ForceSrgb)
        {
            Settings->ColorSpacePolicy = TextureCompressorPlugin::ETextureColorSpacePolicy::ForceSrgb;
        }
        else
        {
            Settings->ColorSpacePolicy = TextureCompressorPlugin::ETextureColorSpacePolicy::Auto;
        }
        return Settings;
    }

    return {};
}

void EditorAssetService::ClearAssetEditorImportSettingsBinding()
{
    m_assetEditorAssimpImportSettings.reset();
    m_assetEditorTextureImportSettings.reset();
    m_assetEditorImportSettingsType = {};
    m_assetEditorImportSettingsObject = nullptr;
    m_assetEditorImportSettingsDirty = false;
    m_assetEditorCanReimport = false;
    m_assetEditorImportMetadataBaseline.reset();
}

Result EditorAssetService::ReimportActiveAsset(EditorServiceContext& Context)
{
    if (m_assetEditorAssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active asset editor to reimport"));
    }
    if (m_assetEditorRuntimeDirty)
    {
        return std::unexpected(MakeError(
            EErrorCode::InvalidArgument,
            "Save runtime asset changes before reimporting"));
    }
    if (!m_assetEditorImportMetadataBaseline.has_value())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Active asset has no import metadata"));
    }
    if (!m_assetEditorCanReimport)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Active asset is not reimportable"));
    }

    const DiscoveredAsset* ActiveAsset = FindAssetByKey(m_assetEditorAssetKey);
    if (!ActiveAsset)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Active asset was not found"));
    }
    const DiscoveredAsset AssetSnapshot = *ActiveAsset;

    const auto CurrentImportMetadata = BuildAssetEditorImportMetadataFromCurrentState();
    const AssetImportMetadataEntry Metadata =
        CurrentImportMetadata.value_or(*m_assetEditorImportMetadataBaseline);

    if (Metadata.SourcePath.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Import source path is not recorded"));
    }

    std::filesystem::path SourcePath = std::filesystem::path(Metadata.SourcePath);
    if (auto Resolved = SPathResolver::Instance().Resolve(Metadata.SourcePath); Resolved)
    {
        SourcePath = *Resolved;
    }

    std::error_code Error{};
    if (!std::filesystem::exists(SourcePath, Error) || Error)
    {
        return std::unexpected(MakeError(
            EErrorCode::NotFound,
            "Reimport source path was not found: " + SourcePath.string()));
    }

    auto TypedImportSettings = BuildTypedImportSettingsForRecord(Metadata);
    auto ReimportResult = ImportSourceAsset(
        Context,
        SourcePath.string(),
        Metadata.DestinationFolder,
        Metadata.BuildOptions,
        std::move(TypedImportSettings));
    if (!ReimportResult)
    {
        return ReimportResult;
    }

    std::string ReopenKey{};
    for (const auto& Asset : m_assets)
    {
        if (Asset.AssetId == AssetSnapshot.AssetId)
        {
            ReopenKey = Asset.Key;
            break;
        }
    }
    if (ReopenKey.empty())
    {
        for (const auto& Asset : m_assets)
        {
            if (Asset.AssetKind == AssetSnapshot.AssetKind &&
                Asset.Name == AssetSnapshot.Name &&
                Asset.Variant == AssetSnapshot.Variant)
            {
                ReopenKey = Asset.Key;
                break;
            }
        }
    }
    if (!ReopenKey.empty())
    {
        (void)SelectAssetByKey(ReopenKey);
        (void)OpenAssetEditorByKey(ReopenKey);
    }

    return Ok();
}

BaseNode* EditorAssetService::ResolveAssetEditorNode(NodeHandle& InOutNode)
{
    if (InOutNode.IsNull())
    {
        return nullptr;
    }

    if (m_assetEditorWorld)
    {
        if (BaseNode* Direct = m_assetEditorWorld->BorrowedNode(InOutNode))
        {
            return Direct;
        }
    }

    if (BaseNode* Direct = InOutNode.Borrowed())
    {
        return Direct;
    }

    if (m_assetEditorWorld)
    {
        if (const auto HandleResult = m_assetEditorWorld->NodeHandleById(InOutNode.Id); HandleResult.has_value())
        {
            InOutNode = *HandleResult;
            if (BaseNode* Resolved = m_assetEditorWorld->BorrowedNode(InOutNode))
            {
                return Resolved;
            }
        }
    }

    return InOutNode.BorrowedSlowByUuid();
}

const BaseNode* EditorAssetService::ResolveAssetEditorNode(const NodeHandle& Node) const
{
    NodeHandle ResolvedNode = Node;
    if (ResolvedNode.IsNull())
    {
        return nullptr;
    }

    if (m_assetEditorWorld)
    {
        if (const BaseNode* Direct = m_assetEditorWorld->BorrowedNode(ResolvedNode))
        {
            return Direct;
        }
    }

    if (const BaseNode* Direct = ResolvedNode.Borrowed())
    {
        return Direct;
    }

    if (m_assetEditorWorld)
    {
        if (const auto HandleResult = m_assetEditorWorld->NodeHandleById(ResolvedNode.Id); HandleResult.has_value())
        {
            ResolvedNode = *HandleResult;
            if (const BaseNode* Resolved = m_assetEditorWorld->BorrowedNode(ResolvedNode))
            {
                return Resolved;
            }
        }
    }

    return ResolvedNode.BorrowedSlowByUuid();
}

void* EditorAssetService::ResolveAssetEditorRuntimeTargetObject() const
{
    if (m_assetEditorTargetType == TypeId{})
    {
        return nullptr;
    }

    if (m_assetEditorAssetKind == AssetKindNode() || m_assetEditorAssetKind == AssetKindLevel())
    {
        return const_cast<BaseNode*>(ResolveAssetEditorNode(m_assetEditorRootHandle));
    }

    return m_assetEditorTargetObject;
}

bool EditorAssetService::HasAssetEditorRuntimeTarget() const
{
    return ResolveAssetEditorRuntimeTargetObject() != nullptr && m_assetEditorTargetType != TypeId{};
}

bool EditorAssetService::IsAssetEditorRuntimeDirty() const
{
    return m_assetEditorDocumentState.RuntimeRevision != m_assetEditorDocumentState.SavedRuntimeRevision;
}

bool EditorAssetService::IsAssetEditorImportDirty() const
{
    return m_assetEditorDocumentState.ImportRevision != m_assetEditorDocumentState.SavedImportRevision;
}

void EditorAssetService::RefreshAssetEditorDirtyFlags(const bool RefreshDiscoveryState)
{
    const bool PreviousRuntimeDirty = m_assetEditorRuntimeDirty;
    const bool PreviousImportDirty = m_assetEditorImportSettingsDirty;
    const bool PreviousAnyDirty = m_assetEditorDirty;

    m_assetEditorRuntimeDirty = IsAssetEditorRuntimeDirty();
    m_assetEditorImportSettingsDirty = IsAssetEditorImportDirty();
    m_assetEditorDirty = m_assetEditorRuntimeDirty || m_assetEditorImportSettingsDirty;

    if (RefreshDiscoveryState &&
        m_assetEditorSourceAssetType != TypeId{} &&
        PreviousAnyDirty != m_assetEditorDirty)
    {
        (void)RefreshDiscovery();
    }

    if (PreviousRuntimeDirty != m_assetEditorRuntimeDirty ||
        PreviousImportDirty != m_assetEditorImportSettingsDirty ||
        PreviousAnyDirty != m_assetEditorDirty)
    {
        ++m_assetEditorSessionRevision;
    }
}

std::expected<void, std::string> EditorAssetService::SyncAssetEditorRuntimeOverrideFromCurrentState()
{
    if (m_assetEditorAssetKey.empty() || m_assetEditorSourceAssetType != TypeId{} || !HasAssetEditorRuntimeTarget())
    {
        return {};
    }

    auto SerializedPayloadResult = SerializeAssetEditorPayload();
    if (!SerializedPayloadResult)
    {
        return std::unexpected(SerializedPayloadResult.error());
    }

    m_assetPayloadOverrides[m_assetEditorAssetId] = std::move(*SerializedPayloadResult);
    return {};
}

void EditorAssetService::MarkAssetEditorRuntimeChanged(const bool RefreshDiscoveryState)
{
    if (m_assetEditorAssetKey.empty())
    {
        return;
    }

    ++m_assetEditorDocumentState.RuntimeRevision;
    if (auto SyncResult = SyncAssetEditorRuntimeOverrideFromCurrentState(); !SyncResult)
    {
        m_statusMessage = "Asset editor dirty-state warning: " + SyncResult.error();
    }

    RefreshAssetEditorDirtyFlags(RefreshDiscoveryState);
}

void EditorAssetService::MarkAssetEditorImportSettingsChanged(const bool RefreshDiscoveryState)
{
    if (m_assetEditorAssetKey.empty() ||
        m_assetEditorImportSettingsObject == nullptr ||
        m_assetEditorImportSettingsType == TypeId{})
    {
        return;
    }

    ++m_assetEditorDocumentState.ImportRevision;
    if (m_assetEditorSourceAssetType != TypeId{})
    {
        ApplyAssetEditorImportSettingsToActiveSourceAsset();
        ++m_assetEditorDocumentState.RuntimeRevision;
    }

    RefreshAssetEditorDirtyFlags(RefreshDiscoveryState || m_assetEditorSourceAssetType != TypeId{});
}

void EditorAssetService::MarkAssetEditorRuntimeSaved()
{
    m_assetEditorDocumentState.SavedRuntimeRevision = m_assetEditorDocumentState.RuntimeRevision;
    m_assetPayloadOverrides.erase(m_assetEditorAssetId);
}

void EditorAssetService::MarkAssetEditorImportSettingsSaved()
{
    m_assetEditorDocumentState.SavedImportRevision = m_assetEditorDocumentState.ImportRevision;
}

void EditorAssetService::NotifyActiveAssetEditorRuntimeMutated(const TypeId& RootType, void* RootInstance)
{
    (void)RootType;
    (void)RootInstance;

    if (m_assetEditorAssetKey.empty() || !HasAssetEditorRuntimeTarget())
    {
        return;
    }

    MarkAssetEditorRuntimeChanged(m_assetEditorSourceAssetType != TypeId{});
}

void EditorAssetService::NotifyActiveAssetEditorImportSettingsMutated(const TypeId& RootType, void* RootInstance)
{
    (void)RootType;
    (void)RootInstance;

    if (m_assetEditorAssetKey.empty() ||
        m_assetEditorImportSettingsObject == nullptr ||
        m_assetEditorImportSettingsType == TypeId{})
    {
        return;
    }

    MarkAssetEditorImportSettingsChanged();
}

void EditorAssetService::RefreshAssetEditorHierarchy()
{
    const NodeHandle PreviousSelection = m_assetEditorSelectedNode;
    const std::vector<AssetEditorSessionView::NodeEntry> PreviousHierarchy = m_assetEditorHierarchy;
    std::vector<AssetEditorSessionView::NodeEntry> NextHierarchy{};
    NodeHandle NextSelection = m_assetEditorSelectedNode;

    if (!m_assetEditorCanEditHierarchy || !m_assetEditorWorld || m_assetEditorRootHandle.IsNull())
    {
        NextSelection = {};
        m_assetEditorHierarchy = std::move(NextHierarchy);
        m_assetEditorSelectedNode = NextSelection;
        m_assetEditorHierarchyDirty = false;
        if (!PreviousHierarchy.empty() || !PreviousSelection.IsNull())
        {
            ++m_assetEditorSessionRevision;
        }
        return;
    }

    BaseNode* RootNode = ResolveAssetEditorNode(m_assetEditorRootHandle);
    if (!RootNode)
    {
        NextSelection = {};
        m_assetEditorHierarchy = std::move(NextHierarchy);
        m_assetEditorSelectedNode = NextSelection;
        m_assetEditorHierarchyDirty = false;
        if (!PreviousHierarchy.empty() || !PreviousSelection.IsNull())
        {
            ++m_assetEditorSessionRevision;
        }
        return;
    }

    struct PendingNode
    {
        NodeHandle Handle{};
        int Depth = 0;
    };
    std::vector<PendingNode> Stack{};
    Stack.push_back(PendingNode{RootNode->Handle(), 0});

    while (!Stack.empty())
    {
        PendingNode Current = Stack.back();
        Stack.pop_back();

        BaseNode* Node = ResolveAssetEditorNode(Current.Handle);
        if (!Node)
        {
            continue;
        }

        std::string Label = Node->Name();
        if (const TypeInfo* Type = TypeRegistry::Instance().Find(Node->TypeKey()))
        {
            const std::string TypeLabel = ShortTypeName(Type->Name);
            if (Label.empty())
            {
                Label = TypeLabel;
            }
            else if (!TypeLabel.empty())
            {
                Label += " (" + TypeLabel + ")";
            }
        }
        if (Label.empty())
        {
            Label = "<unnamed>";
        }

        NextHierarchy.push_back(AssetEditorSessionView::NodeEntry{
            .Handle = Node->Handle(),
            .Depth = Current.Depth,
            .Label = std::move(Label),
        });

        const auto& Children = Node->Children();
        for (auto It = Children.rbegin(); It != Children.rend(); ++It)
        {
            if (It->IsNull())
            {
                continue;
            }
            Stack.push_back(PendingNode{*It, Current.Depth + 1});
        }
    }

    if (NextSelection.IsNull())
    {
        NextSelection = m_assetEditorRootHandle;
    }
    if (!ResolveAssetEditorNode(NextSelection))
    {
        NextSelection = m_assetEditorRootHandle;
    }
    const bool SelectionPresent = std::ranges::any_of(
        NextHierarchy, [&NextSelection](const AssetEditorSessionView::NodeEntry& Entry) {
            return Entry.Handle == NextSelection;
        });
    if (!SelectionPresent)
    {
        NextSelection = NextHierarchy.empty() ? NodeHandle{} : NextHierarchy.front().Handle;
    }

    const bool HierarchyChanged = [&PreviousHierarchy, &NextHierarchy]() -> bool {
        if (PreviousHierarchy.size() != NextHierarchy.size())
        {
            return true;
        }

        for (std::size_t Index = 0; Index < NextHierarchy.size(); ++Index)
        {
            const auto& Left = PreviousHierarchy[Index];
            const auto& Right = NextHierarchy[Index];
            if (Left.Handle != Right.Handle || Left.Depth != Right.Depth || Left.Label != Right.Label)
            {
                return true;
            }
        }

        return false;
    }();
    const bool SelectionChanged = PreviousSelection != NextSelection;

    m_assetEditorHierarchy = std::move(NextHierarchy);
    m_assetEditorSelectedNode = NextSelection;
    m_assetEditorHierarchyDirty = false;
    if (HierarchyChanged || SelectionChanged)
    {
        ++m_assetEditorSessionRevision;
    }
}

Result EditorAssetService::SyncMaterialInstanceEditorPayloadFromDescriptor()
{
#if !defined(SNAPI_GF_ENABLE_RENDERER)
    m_assetEditorMaterialInstanceDescriptorParentKey.clear();
    return Ok();
#else
    if (!m_assetEditorMaterialInstancePayload || !m_assetManager)
    {
        m_assetEditorMaterialInstanceDescriptorParentKey.clear();
        return Ok();
    }

    MaterialInstanceAsset& Payload = *m_assetEditorMaterialInstancePayload;
    const std::string ParentIdentity = BuildAssetRefIdentity(Payload.ParentMaterial);
    m_assetEditorMaterialInstanceDescriptorParentKey = ParentIdentity;
    if (ParentIdentity.empty())
    {
        return Ok();
    }

    MaterialAsset ParentMaterial{};
    bool ParentResolved = false;

    TAssetRef<MaterialAsset> ParentMaterialRef(Payload.ParentMaterial.AssetName, Payload.ParentMaterial.AssetId);
    if (auto ParentAssetResult = ParentMaterialRef.LoadAsset(); ParentAssetResult && *ParentAssetResult)
    {
        ParentMaterial = std::move(**ParentAssetResult);
        ParentResolved = true;
    }

    if (!ParentResolved)
    {
        const std::string ParentAssetName = ToLowerCopy(Payload.ParentMaterial.AssetName);
        const std::string ParentAssetIdText = Payload.ParentMaterial.AssetId;
        ::SnAPI::AssetPipeline::AssetId ParentAssetId{};
        const bool HasParentId = !ParentAssetIdText.empty();
        if (HasParentId)
        {
            ParentAssetId = ::SnAPI::AssetPipeline::AssetId::FromString(ParentAssetIdText);
        }

        const DiscoveredAsset* ParentAsset = nullptr;
        if (HasParentId && !ParentAssetId.IsNull())
        {
            const auto It = std::ranges::find_if(m_assets, [&ParentAssetId](const DiscoveredAsset& Asset) {
                return Asset.AssetId == ParentAssetId;
            });
            if (It != m_assets.end())
            {
                ParentAsset = &(*It);
            }
        }
        if (!ParentAsset && !ParentAssetName.empty())
        {
            const auto It = std::ranges::find_if(m_assets, [&ParentAssetName](const DiscoveredAsset& Asset) {
                return ToLowerCopy(Asset.Name) == ParentAssetName;
            });
            if (It != m_assets.end())
            {
                ParentAsset = &(*It);
            }
        }
        if (!ParentAsset)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Parent material asset could not be resolved"));
        }

        auto CookedPayloadResult = BuildCookedPayloadForAsset(*ParentAsset);
        if (!CookedPayloadResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, CookedPayloadResult.error()));
        }
        if (CookedPayloadResult->PayloadType != PayloadMaterial())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Parent material payload type mismatch"));
        }

        auto MaterialPayloadResult = DeserializeMaterialPayload(
            CookedPayloadResult->Bytes.data(),
            CookedPayloadResult->Bytes.size());
        if (!MaterialPayloadResult)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, MaterialPayloadResult.error().Message));
        }

        ParentMaterial = std::move(MaterialPayloadResult.value());
        ParentResolved = true;
    }

    if (!ParentResolved)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Unable to resolve parent material payload"));
    }

    auto DescriptorResult = BuildDescriptorForMaterialAsset(ParentMaterial);
    if (!DescriptorResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, DescriptorResult.error()));
    }

    if (SyncMaterialInstanceAssetToRuntimeDescriptor(Payload, *DescriptorResult))
    {
        ++m_assetEditorSessionRevision;
    }

    return Ok();
#endif
}

std::expected<::SnAPI::AssetPipeline::TypedPayload, std::string> EditorAssetService::SerializeAssetEditorPayload() const
{
    if (!HasAssetEditorRuntimeTarget())
    {
        return std::unexpected("No active asset editor object is available");
    }

    if (m_assetEditorAssetKind == AssetKindNode())
    {
        NodeHandle RootHandle = m_assetEditorRootHandle;
        auto* Node = const_cast<BaseNode*>(ResolveAssetEditorNode(RootHandle));
        if (!Node)
        {
            return std::unexpected("Asset editor node target is null");
        }

        auto PayloadResult = NodeSerializer::Serialize(*Node);
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error().Message);
        }

        std::vector<uint8_t> Bytes{};
        auto SerializeResult = SerializeNodePayload(*PayloadResult, Bytes);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error().Message);
        }

        return ::SnAPI::AssetPipeline::TypedPayload(PayloadNode(), NodeSerializer::kSchemaVersion, std::move(Bytes));
    }

    if (m_assetEditorAssetKind == AssetKindLevel())
    {
        NodeHandle RootHandle = m_assetEditorRootHandle;
        auto* LevelNode = NodeCast<Level>(const_cast<BaseNode*>(ResolveAssetEditorNode(RootHandle)));
        if (!LevelNode)
        {
            return std::unexpected("Asset editor level target is null");
        }

        auto PayloadResult = LevelSerializer::Serialize(*LevelNode);
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error().Message);
        }

        std::vector<uint8_t> Bytes{};
        auto SerializeResult = SerializeLevelPayload(*PayloadResult, Bytes);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error().Message);
        }

        return ::SnAPI::AssetPipeline::TypedPayload(PayloadLevel(), LevelSerializer::kSchemaVersion, std::move(Bytes));
    }

    if (m_assetEditorAssetKind == AssetKindWorld())
    {
        auto* WorldRef = static_cast<World*>(ResolveAssetEditorRuntimeTargetObject());
        if (!WorldRef)
        {
            return std::unexpected("Asset editor world target is null");
        }

        auto PayloadResult = WorldSerializer::Serialize(*WorldRef);
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error().Message);
        }

        std::vector<uint8_t> Bytes{};
        auto SerializeResult = SerializeWorldPayload(*PayloadResult, Bytes);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error().Message);
        }

        return ::SnAPI::AssetPipeline::TypedPayload(PayloadWorld(), WorldSerializer::kSchemaVersion, std::move(Bytes));
    }

    if (m_assetEditorAssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture)
    {
        if (!m_assetEditorTextureCookedInfo)
        {
            return std::unexpected("Texture cooked payload is not loaded");
        }

        auto* TextureEditor = static_cast<Editor::TextureAssetEditorPayload*>(ResolveAssetEditorRuntimeTargetObject());
        if (!TextureEditor)
        {
            return std::unexpected("Asset editor texture target is null");
        }

        TextureCompressorPlugin::TextureCompressorCookedInfo Cooked = *m_assetEditorTextureCookedInfo;
        ApplyTextureEditorPayloadToCooked(*TextureEditor, Cooked);

        const auto* TextureSerializer = m_assetManager
            ? m_assetManager->GetRegistry().Find(TextureCompressorPlugin::Payload_CompressorCookedInfo)
            : nullptr;
        if (!TextureSerializer)
        {
            return std::unexpected("Texture serializer is not registered");
        }

        std::vector<uint8_t> Bytes{};
        TextureSerializer->SerializeToBytes(&Cooked, Bytes);
        if (Bytes.empty())
        {
            return std::unexpected("Failed to serialize cooked texture payload");
        }

        return ::SnAPI::AssetPipeline::TypedPayload(
            TextureCompressorPlugin::Payload_CompressorCookedInfo,
            TextureSerializer->GetSchemaVersion(),
            std::move(Bytes));
    }

    if (m_assetEditorAssetKind == AssetKindStaticMesh())
    {
        if (!m_assetEditorStaticMeshPayload)
        {
            return std::unexpected("Static mesh payload is not loaded");
        }

        auto* StaticMeshEditor = static_cast<Editor::StaticMeshAssetEditorPayload*>(ResolveAssetEditorRuntimeTargetObject());
        if (!StaticMeshEditor)
        {
            return std::unexpected("Asset editor static mesh target is null");
        }

        StaticMeshPayload Cooked = *m_assetEditorStaticMeshPayload;
        ApplyStaticMeshEditorPayloadToCooked(*StaticMeshEditor, Cooked);

        std::vector<uint8_t> Bytes{};
        auto SerializeResult = SerializeStaticMeshPayload(Cooked, Bytes);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error().Message);
        }

        const auto* StaticMeshSerializer = m_assetManager
            ? m_assetManager->GetRegistry().Find(PayloadStaticMesh())
            : nullptr;
        const uint32_t SchemaVersion = StaticMeshSerializer ? StaticMeshSerializer->GetSchemaVersion() : 2u;
        return ::SnAPI::AssetPipeline::TypedPayload(PayloadStaticMesh(), SchemaVersion, std::move(Bytes));
    }

    if (m_assetEditorAssetKind == AssetKindMaterial())
    {
        auto* Material = static_cast<MaterialAsset*>(ResolveAssetEditorRuntimeTargetObject());
        if (!Material)
        {
            return std::unexpected("Asset editor material target is null");
        }

        std::vector<uint8_t> Bytes{};
        auto SerializeResult = SerializeMaterialPayload(*Material, Bytes);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error().Message);
        }

        return ::SnAPI::AssetPipeline::TypedPayload(PayloadMaterial(), kMaterialPayloadSchemaVersion, std::move(Bytes));
    }

    if (m_assetEditorAssetKind == AssetKindMaterialInstance())
    {
        auto* MaterialInstance = static_cast<MaterialInstanceAsset*>(ResolveAssetEditorRuntimeTargetObject());
        if (!MaterialInstance)
        {
            return std::unexpected("Asset editor material instance target is null");
        }

        std::vector<uint8_t> Bytes{};
        auto SerializeResult = SerializeMaterialInstancePayload(*MaterialInstance, Bytes);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error().Message);
        }

        return ::SnAPI::AssetPipeline::TypedPayload(
            PayloadMaterialInstance(),
            kMaterialInstancePayloadSchemaVersion,
            std::move(Bytes));
    }

    return std::unexpected("Unsupported asset kind for serialization");
}

std::expected<std::string, std::string> EditorAssetService::SerializeAssetEditorSourceJson() const
{
    if (m_assetEditorSourceAssetType == TypeId{})
    {
        return std::unexpected("No active authored source asset is open");
    }

    if (m_assetEditorAssetKind == AssetKindNode())
    {
        const BaseNode* RootNode = ResolveAssetEditorNode(m_assetEditorRootHandle);
        if (!RootNode)
        {
            return std::unexpected("Opened prefab source asset has no loaded root node");
        }

        auto AssetResult = CaptureNodeAsset(*RootNode);
        if (!AssetResult)
        {
            return std::unexpected(AssetResult.error().Message);
        }

        auto SaveResult = SerializeAuthoredAssetToJson(*AssetResult);
        if (!SaveResult)
        {
            return std::unexpected(SaveResult.error().Message);
        }
        return *SaveResult;
    }

    if (m_assetEditorAssetKind == AssetKindLevel())
    {
        auto* LevelNode = NodeCast<Level>(ResolveAssetEditorNode(m_assetEditorRootHandle));
        if (!LevelNode)
        {
            return std::unexpected("Opened level source asset has no loaded level root");
        }

        auto AssetResult = CaptureLevelAsset(*LevelNode);
        if (!AssetResult)
        {
            return std::unexpected(AssetResult.error().Message);
        }

        auto SaveResult = SerializeAuthoredAssetToJson(*AssetResult);
        if (!SaveResult)
        {
            return std::unexpected(SaveResult.error().Message);
        }
        return *SaveResult;
    }

    if (m_assetEditorAssetKind == AssetKindWorld())
    {
        if (!m_assetEditorWorld)
        {
            return std::unexpected("Opened world source asset has no loaded world");
        }

        auto AssetResult = CaptureWorldAsset(*m_assetEditorWorld);
        if (!AssetResult)
        {
            return std::unexpected(AssetResult.error().Message);
        }

        auto SaveResult = SerializeAuthoredAssetToJson(*AssetResult);
        if (!SaveResult)
        {
            return std::unexpected(SaveResult.error().Message);
        }
        return *SaveResult;
    }

    if (!m_assetEditorGenericSourceObject)
    {
        return std::unexpected("No active authored source asset is open");
    }

    const TypeInfo* Type = TypeRegistry::Instance().Find(m_assetEditorSourceAssetType);
    if (!Type)
    {
        return std::unexpected("Authored asset type does not support JSON serialization");
    }

    const auto* Asset = static_cast<const IAsset*>(TypeRegistry::Instance().Cast(m_assetEditorSourceAssetType,
                                                                                  StaticTypeId<IAsset>(),
                                                                                  m_assetEditorGenericSourceObject.get()));
    if (!Asset)
    {
        return std::unexpected("Authored asset type does not cast to IAsset");
    }

    std::ostringstream Output{};
    const Result SaveResult = Asset->Save(Output);
    if (!SaveResult)
    {
        return std::unexpected(SaveResult.error().Message);
    }
    return Output.str();
}

void EditorAssetService::ClearAssetEditorState()
{
    const bool HadActiveSession = !m_assetEditorAssetKey.empty() ||
                                  m_assetEditorTargetObject != nullptr ||
                                  m_assetEditorTargetType != TypeId{} ||
                                  m_assetEditorImportSettingsObject != nullptr ||
                                  m_assetEditorImportSettingsType != TypeId{} ||
                                  !m_assetEditorHierarchy.empty() ||
                                  !m_assetEditorSelectedNode.IsNull() ||
                                  m_assetEditorDirty;
    const ::SnAPI::AssetPipeline::AssetId PreviousAssetId = m_assetEditorAssetId;
    m_assetEditorWorld.reset();
    m_assetEditorRootHandle = {};
    m_assetEditorAssetKey.clear();
    m_assetEditorAssetId = {};
    m_assetEditorAssetKind = {};
    m_assetEditorTargetType = {};
    m_assetEditorTargetObject = nullptr;
    m_assetEditorSourceAssetType = {};
    m_assetEditorDirty = false;
    m_assetEditorRuntimeDirty = false;
    m_assetEditorCanSave = false;
    m_assetEditorCanEditHierarchy = false;
    m_assetEditorMaterialPayload.reset();
    m_assetEditorMaterialInstancePayload.reset();
    m_assetEditorTextureCookedInfo.reset();
    m_assetEditorTexturePayload.reset();
    m_assetEditorStaticMeshPayload.reset();
    m_assetEditorStaticMeshEditorPayload.reset();
    ClearAssetEditorImportSettingsBinding();
    m_assetEditorMaterialInstanceDescriptorParentKey.clear();
    m_assetEditorTitle.clear();
    m_assetEditorSelectedNode = {};
    m_assetEditorHierarchy.clear();
    m_assetEditorHierarchyDirty = false;
    m_assetEditorDocumentState.Reset();
    m_assetPayloadOverrides.erase(PreviousAssetId);
    m_assetEditorGenericSourceObject = {nullptr, [](void*) {}};
    if (HadActiveSession)
    {
        ++m_assetEditorSessionRevision;
    }
}

std::vector<std::string> EditorAssetService::BuildPackSearchPaths() const
{
    std::vector<std::string> Paths{};
    std::unordered_set<std::string> SeenPaths{};

    if (m_currentProject.IsLoaded && !m_currentProject.AssetRootDirectory.empty())
    {
        AppendUniquePath(Paths, SeenPaths, std::filesystem::path(m_currentProject.AssetRootDirectory));
        return Paths;
    }

    if (!m_editorTemplateAssetDirectory.empty())
    {
        AppendUniquePath(Paths, SeenPaths, m_editorTemplateAssetDirectory);
    }
    else
    {
        const std::filesystem::path DefaultPackDirectory = EditorDefaultShapeAssetDirectory();
        if (!DefaultPackDirectory.empty())
        {
            AppendUniquePath(Paths, SeenPaths, DefaultPackDirectory);
        }
    }

    if (const std::filesystem::path ResolverAssetRoot = SPathResolver::Instance().AssetRoot();
        !ResolverAssetRoot.empty())
    {
        AppendUniquePath(Paths, SeenPaths, ResolverAssetRoot);
    }

    std::error_code Error{};
    const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
    if (!Error)
    {
        AppendUniquePath(Paths, SeenPaths, CurrentPath);
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "Content");
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "Assets");
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "Packs");
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "build");
    }

    if (const char* EnvRaw = std::getenv("SNAPI_EDITOR_ASSET_PATHS"))
    {
        const auto ExtraPaths = ParsePackSearchPathEnv(std::string_view(EnvRaw));
        for (const std::string& Path : ExtraPaths)
        {
            AppendUniquePath(Paths, SeenPaths, std::filesystem::path(Path));
        }
    }

    return Paths;
}

std::vector<std::string> EditorAssetService::ParsePackSearchPathEnv(const std::string_view Raw)
{
    std::vector<std::string> Paths{};
    std::string Token{};
    Token.reserve(Raw.size());

    for (const char Character : Raw)
    {
        if (Character == ';' || Character == ':')
        {
            if (!Token.empty())
            {
                Paths.push_back(Token);
                Token.clear();
            }
            continue;
        }
        Token.push_back(Character);
    }

    if (!Token.empty())
    {
        Paths.push_back(Token);
    }

    return Paths;
}

std::string EditorAssetService::AssetKindToLabel(const ::SnAPI::AssetPipeline::TypeId& AssetKind)
{
    if (AssetKind == AssetKindNode())
    {
        return "Node";
    }
    if (AssetKind == AssetKindWorld())
    {
        return "World";
    }
    if (AssetKind == AssetKindLevel())
    {
        return "Level";
    }
    if (AssetKind == AssetKindMaterial())
    {
        return "Material";
    }
    if (AssetKind == AssetKindMaterialInstance())
    {
        return "Material Instance";
    }
    if (AssetKind == AssetKindConduitGraph())
    {
        return "Conduit Graph";
    }
    if (AssetKind == AssetKindConduitClass())
    {
        return "Conduit Class";
    }
    return "Asset";
}

const EditorAssetService::DiscoveredAsset* EditorAssetService::FindAssetByKey(const std::string_view Key) const
{
    const auto It = m_assetIndexByKey.find(std::string(Key));
    if (It == m_assetIndexByKey.end())
    {
        return nullptr;
    }

    const std::size_t Index = It->second;
    if (Index >= m_assets.size())
    {
        return nullptr;
    }
    return &m_assets[Index];
}

std::expected<std::string, std::string> EditorAssetService::ResolveOwningPackPath(const DiscoveredAsset& Asset) const
{
    if (!m_assetManager)
    {
        return std::unexpected("Asset manager is not initialized");
    }

    if (!Asset.OwningPackPath.empty())
    {
        if (auto ResolvedPath = SPathResolver::Instance().ResolveToString(Asset.OwningPackPath);
            ResolvedPath && !ResolvedPath->empty())
        {
            return *ResolvedPath;
        }
        return Asset.OwningPackPath;
    }

    if (Asset.IsRuntime)
    {
        return std::unexpected("Runtime memory assets do not have an owning pack");
    }

    const auto MountedPacks = m_assetManager->GetMountedPacks();
    for (const std::string& RawPackPath : MountedPacks)
    {
        std::string PackPath = RawPackPath;
        if (auto ResolvedPackPath = SPathResolver::Instance().ResolveToString(RawPackPath);
            ResolvedPackPath && !ResolvedPackPath->empty())
        {
            PackPath = *ResolvedPackPath;
        }

        ::SnAPI::AssetPipeline::AssetPackReader Reader{};
        auto OpenResult = Reader.Open(PackPath);
        if (!OpenResult)
        {
            continue;
        }

        auto AssetInfoResult = Reader.FindAsset(Asset.AssetId);
        if (AssetInfoResult)
        {
            return PackPath;
        }
    }

    return std::unexpected("Unable to resolve owning pack for selected asset");
}

std::expected<std::string, std::string> EditorAssetService::ResolveRuntimeSavePath(const DiscoveredAsset& Asset) const
{
    if (!Asset.IsRuntime)
    {
        return std::unexpected("Selected asset is not a runtime memory asset");
    }

    std::filesystem::path AssetsRoot = SPathResolver::Instance().AssetRoot();
    if (AssetsRoot.empty())
    {
        std::error_code Error{};
        const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
        if (Error)
        {
            return std::unexpected("Failed to resolve current directory: " + Error.message());
        }
        AssetsRoot = CurrentPath / "Assets";
    }

    std::error_code Error{};
    std::filesystem::create_directories(AssetsRoot, Error);
    if (Error)
    {
        return std::unexpected("Failed to create Assets directory: " + Error.message());
    }

    const std::string RelativeName = NormalizeAssetLogicalName(Asset.Name);
    if (RelativeName.empty())
    {
        return std::unexpected("Runtime asset has an empty logical name");
    }

    std::filesystem::path OutputPath = AssetsRoot / std::filesystem::path(RelativeName);
    const std::string ExtensionLower = ToLowerCopy(OutputPath.extension().string());
    if (ExtensionLower != ".snpak")
    {
        OutputPath += ".snpak";
    }

    if (auto ResolvedPath = SPathResolver::Instance().ResolveToString(OutputPath.string());
        ResolvedPath && !ResolvedPath->empty())
    {
        return *ResolvedPath;
    }

    return OutputPath.lexically_normal().string();
}

std::expected<::SnAPI::AssetPipeline::TypedPayload, std::string> EditorAssetService::BuildCookedPayloadForAsset(
    const DiscoveredAsset& Asset)
{
    if (!m_assetManager)
    {
        return std::unexpected("Asset manager is not initialized");
    }

    if (Asset.IsRuntime)
    {
        return m_assetManager->LoadCookedPayload(Asset.AssetId);
    }

    auto PackPathResult = ResolveOwningPackPath(Asset);
    if (!PackPathResult)
    {
        return std::unexpected(PackPathResult.error());
    }

    ::SnAPI::AssetPipeline::AssetPackReader Reader{};
    auto OpenResult = Reader.Open(PackPathResult.value());
    if (!OpenResult)
    {
        return std::unexpected(OpenResult.error());
    }

    auto CookedResult = Reader.LoadCookedPayload(Asset.AssetId);
    if (!CookedResult)
    {
        return std::unexpected(CookedResult.error());
    }

    return std::move(*CookedResult);
}

Result EditorAssetService::InstantiateLevelAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    LevelAssetLoadParams LoadParams{};
    LoadParams.TargetWorld = WorldPtr;
    LoadParams.NameOverride = Asset.Name.empty() ? std::string("LevelAsset") : Asset.Name;
    auto LoadResult = Asset.SourceFilePath.empty()
        ? m_assetManager->Load<Level>(Asset.AssetId, LoadParams)
        : m_assetManager->Load<Level>(Asset.Name, LoadParams);
    if (!LoadResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, LoadResult.error()));
    }

    m_statusMessage = "Instantiated Level asset: " + Asset.Name;
    return Ok();
}

Result EditorAssetService::InstantiateNodeAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    NodeHandle ParentHandle{};
    const auto Levels = WorldPtr->Levels();
    if (!Levels.empty())
    {
        ParentHandle = Levels.front();
    }
    else
    {
        auto DefaultLevelResult = WorldPtr->CreateLevel("Level");
        if (DefaultLevelResult)
        {
            ParentHandle = *DefaultLevelResult;
        }
    }

    NodeAssetLoadParams LoadParams{};
    LoadParams.TargetWorld = WorldPtr;
    LoadParams.Parent = ParentHandle;
    auto LoadResult = Asset.SourceFilePath.empty()
        ? m_assetManager->Load<BaseNode>(Asset.AssetId, LoadParams)
        : m_assetManager->Load<BaseNode>(Asset.Name, LoadParams);
    if (!LoadResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, LoadResult.error()));
    }

    m_statusMessage = "Instantiated Node asset: " + Asset.Name;
    return Ok();
}

Result EditorAssetService::InstantiateWorldAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    WorldAssetLoadParams LoadParams{};
    LoadParams.TargetWorld = WorldPtr;
    auto LoadResult = Asset.SourceFilePath.empty()
        ? m_assetManager->Load<World>(Asset.AssetId, LoadParams)
        : m_assetManager->Load<World>(Asset.Name, LoadParams);
    if (!LoadResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, LoadResult.error()));
    }

    m_statusMessage = "Instantiated World asset: " + Asset.Name;
    return Ok();
}

} // namespace SnAPI::GameFramework::Editor
