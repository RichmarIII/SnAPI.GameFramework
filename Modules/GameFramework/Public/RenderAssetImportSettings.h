#pragma once

#include "IAssetImportSettings.h"
#include "ReflectionAnnotations.h"
#include "RenderAssets/MeshImportSettingsPayload.h"

#include <cstdint>
#include <memory>
#include <string>

namespace SnAPI::GameFramework
{

SnType()
enum class ETextureCompressionTarget : std::uint8_t
{
    SnEnumValue(SnDisplayName("BCn"))
    BCn = 0,
    SnEnumValue(SnDisplayName("ASTC"))
    ASTC = 1,
};

SnType()
enum class ETextureCompressionFormat : std::uint8_t
{
    SnEnumValue(SnDisplayName("Auto"))
    Auto = 0,
    SnEnumValue(SnDisplayName("BC1"))
    BC1,
    SnEnumValue(SnDisplayName("BC3"))
    BC3,
    SnEnumValue(SnDisplayName("BC4"))
    BC4,
    SnEnumValue(SnDisplayName("BC5"))
    BC5,
    SnEnumValue(SnDisplayName("BC6H"))
    BC6H,
    SnEnumValue(SnDisplayName("BC7"))
    BC7,
    SnEnumValue(SnDisplayName("ASTC 4x4"))
    ASTC_4x4,
    SnEnumValue(SnDisplayName("ASTC 5x5"))
    ASTC_5x5,
    SnEnumValue(SnDisplayName("ASTC 6x6"))
    ASTC_6x6,
    SnEnumValue(SnDisplayName("ASTC 8x8"))
    ASTC_8x8,
    SnEnumValue(SnDisplayName("ASTC 10x10"))
    ASTC_10x10,
    SnEnumValue(SnDisplayName("ASTC 12x12"))
    ASTC_12x12,
    SnEnumValue(SnDisplayName("ASTC 4x4 HDR"))
    ASTC_4x4_HDR,
    SnEnumValue(SnDisplayName("ASTC 6x6 HDR"))
    ASTC_6x6_HDR,
    SnEnumValue(SnDisplayName("ASTC 8x8 HDR"))
    ASTC_8x8_HDR,
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Typed importer settings for Assimp-driven mesh and animation imports.
 *
 * `AssimpImporterSettings` is the reflected settings object used by editor import flows and any
 * import path that wants a strongly typed replacement for stringly typed build options.
 *
 * Ownership and lifetime:
 * - Callers typically pass this through `IAssetImportSettings` ownership channels.
 * - `Clone()` returns a deep copy suitable for persistence by the asset pipeline.
 */
SnType()
struct AssimpImporterSettings final : public ::SnAPI::AssetPipeline::IAssetImportSettings
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AssimpImporterSettings";

    SnField(SnDisplayName("Mesh"))
    MeshImportSettingsPayload Mesh{};

    SnField(SnDisplayName("Logical Name Override"), SnAdvanced)
    std::string LogicalNameOverride{};

    SnField(SnDisplayName("Default Shader Module"), SnAdvanced)
    std::string DefaultShaderModule{"DefaultGBufferMaterial"};

    SnField(SnDisplayName("Default Shading Model"), SnAdvanced)
    std::string DefaultShadingModel{"GBufferShadingModel"};

    [[nodiscard]] std::unique_ptr<::SnAPI::AssetPipeline::IAssetImportSettings> Clone() const override
    {
        return std::make_unique<AssimpImporterSettings>(*this);
    }

    bool operator==(const AssimpImporterSettings& Other) const
    {
        return Mesh == Other.Mesh &&
               LogicalNameOverride == Other.LogicalNameOverride &&
               DefaultShaderModule == Other.DefaultShaderModule &&
               DefaultShadingModel == Other.DefaultShadingModel;
    }
};

SnType()
struct TextureImporterSettings final : public ::SnAPI::AssetPipeline::IAssetImportSettings
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TextureImporterSettings";

    SnField(SnDisplayName("Target"))
    ETextureCompressionTarget Target = ETextureCompressionTarget::BCn;

    SnField(SnDisplayName("Format"))
    ETextureCompressionFormat Format = ETextureCompressionFormat::Auto;

    SnField(SnDisplayName("Quality"))
    float Quality = 0.6f;

    SnField(SnDisplayName("Force sRGB"))
    bool ForceSrgb = false;

    SnField(SnDisplayName("Force Linear"))
    bool ForceLinear = false;

    SnField(SnDisplayName("Force Normal Map"))
    bool ForceNormalMap = false;

    SnField(SnDisplayName("Max Mips"))
    std::uint32_t MaxMips = 0;

    [[nodiscard]] std::unique_ptr<::SnAPI::AssetPipeline::IAssetImportSettings> Clone() const override
    {
        return std::make_unique<TextureImporterSettings>(*this);
    }

    bool operator==(const TextureImporterSettings& Other) const
    {
        return Target == Other.Target &&
               Format == Other.Format &&
               Quality == Other.Quality &&
               ForceSrgb == Other.ForceSrgb &&
               ForceLinear == Other.ForceLinear &&
               ForceNormalMap == Other.ForceNormalMap &&
               MaxMips == Other.MaxMips;
    }
};

} // namespace SnAPI::GameFramework
