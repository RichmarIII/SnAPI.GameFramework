#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "RenderAssetPayloads.h"
#include "TypeName.h"

namespace SnAPI::GameFramework::Editor
{

enum class ETextureCompressionTarget : std::uint8_t
{
    BCn = 0,
    ASTC = 1,
};

enum class ETextureCompressionFormat : std::uint8_t
{
    Auto = 0,
    BC1,
    BC3,
    BC4,
    BC5,
    BC6H,
    BC7,
    ASTC_4x4,
    ASTC_5x5,
    ASTC_6x6,
    ASTC_8x8,
    ASTC_10x10,
    ASTC_12x12,
    ASTC_4x4_HDR,
    ASTC_6x6_HDR,
    ASTC_8x8_HDR,
};

/**
 * @brief Reflected settings for Assimp-driven model imports.
 */
struct AssimpImportSettings
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.AssimpImportSettings";

    bool GenerateNormals = true;
    bool GenerateTangents = true;
    bool FlipUVs = false;
    bool OptimizeMeshes = true;
    bool ForceSkeletal = false;
    bool ForceStatic = false;
    bool ImportMaterials = true;
    bool ImportTextures = true;
    bool ImportAnimations = true;
    bool ImportSkeleton = true;
    uint32_t MaxBonesPerVertex = 4;
};

/**
 * @brief Reflected settings for TextureCompressor-driven texture imports.
 */
struct TextureImportSettings
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.TextureImportSettings";

    ETextureCompressionTarget Target = ETextureCompressionTarget::BCn;
    ETextureCompressionFormat Format = ETextureCompressionFormat::Auto;
    float Quality = 0.6f;
    bool ForceSrgb = false;
    bool ForceLinear = false;
    bool ForceNormalMap = false;
    uint32_t MaxMips = 0;
};

struct TextureAssetEditorPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.TextureAssetEditorPayload";

    ETextureCompressionTarget Target = ETextureCompressionTarget::BCn;
    ETextureCompressionFormat Format = ETextureCompressionFormat::Auto;
    float Quality = 0.6f;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t MipCount = 0;
    bool SRGB = true;
};

struct StaticMeshAssetEditorPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.StaticMeshAssetEditorPayload";

    std::string Name{};
    std::vector<AssetRefPayload> MaterialInstances{};
};

} // namespace SnAPI::GameFramework::Editor

namespace SnAPI::GameFramework
{
SNAPI_DEFINE_TYPE_NAME(
    Editor::ETextureCompressionTarget,
    "SnAPI.GameFramework.Editor.ETextureCompressionTarget");
SNAPI_DEFINE_TYPE_NAME(
    Editor::ETextureCompressionFormat,
    "SnAPI.GameFramework.Editor.ETextureCompressionFormat");
} // namespace SnAPI::GameFramework
