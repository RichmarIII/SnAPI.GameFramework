#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "RenderAssetPayloads.h"
#include "TypeName.h"

namespace SnAPI::GameFramework::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Target GPU compression family for editor-managed texture imports.
 *
 * This enum expresses the broad platform target rather than an exact compressed format.
 * The editor uses it to choose a compatible family when `TextureImportSettings::Format`
 * remains `Auto`.
 */
enum class ETextureCompressionTarget : std::uint8_t
{
    BCn = 0, /**< @brief Block-compression family commonly used on desktop-class GPUs. */
    ASTC = 1, /**< @brief ASTC family commonly used on mobile and platform-specific GPUs that support ASTC. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Exact compressed texture format requested by editor-managed imports.
 *
 * `Auto` delegates the exact format choice to the importer/cooker based on texture role and
 * `ETextureCompressionTarget`. All other values request a specific compressed output format.
 */
enum class ETextureCompressionFormat : std::uint8_t
{
    Auto = 0, /**< @brief Let the import pipeline choose a suitable format automatically. */
    BC1, /**< @brief BC1/DXT1 style opaque color compression. */
    BC3, /**< @brief BC3/DXT5 style alpha-capable color compression. */
    BC4, /**< @brief Single-channel BC4 compression. */
    BC5, /**< @brief Two-channel BC5 compression, commonly used for tangent-space normal maps. */
    BC6H, /**< @brief HDR-oriented BC6H compression. */
    BC7, /**< @brief High-quality BC7 color compression. */
    ASTC_4x4, /**< @brief ASTC 4x4 block compression. */
    ASTC_5x5, /**< @brief ASTC 5x5 block compression. */
    ASTC_6x6, /**< @brief ASTC 6x6 block compression. */
    ASTC_8x8, /**< @brief ASTC 8x8 block compression. */
    ASTC_10x10, /**< @brief ASTC 10x10 block compression. */
    ASTC_12x12, /**< @brief ASTC 12x12 block compression. */
    ASTC_4x4_HDR, /**< @brief ASTC HDR 4x4 block compression. */
    ASTC_6x6_HDR, /**< @brief ASTC HDR 6x6 block compression. */
    ASTC_8x8_HDR, /**< @brief ASTC HDR 8x8 block compression. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Reflected settings for Assimp-driven model imports.
 *
 * These settings are stored as editor-facing import metadata and translated into the concrete
 * Assimp importer configuration used during import or reimport. They do not directly mutate
 * already cooked assets; changes take effect on the next import or reimport.
 */
struct AssimpImportSettings
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.AssimpImportSettings";

    bool GenerateNormals = true; /**< @brief Generate missing normals during import when source data does not provide them. */
    bool GenerateTangents = true; /**< @brief Generate tangent frames needed for normal mapping workflows. */
    bool FlipUVs = false; /**< @brief Flip imported UVs vertically before cooking. */
    bool OptimizeMeshes = true; /**< @brief Allow the importer to merge or optimize mesh data for runtime use. */
    bool ForceSkeletal = false; /**< @brief Treat the import as skeletal even when source heuristics would not. */
    bool ForceStatic = false; /**< @brief Treat the import as static even when source heuristics would suggest a skeletal asset. */
    bool ImportMaterials = true; /**< @brief Create or import material assets referenced by the source file. */
    bool ImportTextures = true; /**< @brief Create or import texture assets referenced by the source file. */
    bool ImportAnimations = true; /**< @brief Import animation clips when present in the source file. */
    bool ImportSkeleton = true; /**< @brief Import skeleton or bone hierarchy data when present. */
    uint32_t MaxBonesPerVertex = 4; /**< @brief Maximum bone influences kept per vertex after import truncation. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Reflected settings for texture compression imports.
 *
 * These values control how source textures are cooked into compressed runtime payloads.
 * As with other import settings, edits are metadata only until a reimport occurs.
 */
struct TextureImportSettings
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.TextureImportSettings";

    ETextureCompressionTarget Target = ETextureCompressionTarget::BCn; /**< @brief Compression family preferred for the cooked output. */
    ETextureCompressionFormat Format = ETextureCompressionFormat::Auto; /**< @brief Exact compressed format override, or `Auto` for cooker-selected output. */
    float Quality = 0.6f; /**< @brief Normalized quality hint in the `[0, 1]` range used by the texture cooker. */
    bool ForceSrgb = false; /**< @brief Force sRGB color-space handling during cook. */
    bool ForceLinear = false; /**< @brief Force linear color-space handling during cook. */
    bool ForceNormalMap = false; /**< @brief Treat the source as a normal map even if heuristics disagree. */
    uint32_t MaxMips = 0; /**< @brief Maximum number of mip levels to keep; `0` means cooker default or full chain. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Editor-facing view of cooked texture payload metadata.
 *
 * This structure is used for inspector presentation and dirty-state tracking of texture assets.
 * In the current editor flow, most texture knobs are reimport settings rather than direct
 * runtime payload edits, so this payload mainly exposes preview metadata and derived state.
 */
struct TextureAssetEditorPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.TextureAssetEditorPayload";

    ETextureCompressionTarget Target = ETextureCompressionTarget::BCn; /**< @brief Compression family reported by the cooked payload. */
    ETextureCompressionFormat Format = ETextureCompressionFormat::Auto; /**< @brief Exact cooked format reported by the payload when known. */
    float Quality = 0.6f; /**< @brief Normalized cooker quality hint recorded for preview. */
    uint32_t Width = 0; /**< @brief Base-level texture width in texels. */
    uint32_t Height = 0; /**< @brief Base-level texture height in texels. */
    uint32_t MipCount = 0; /**< @brief Number of mip levels present in the cooked payload. */
    bool SRGB = true; /**< @brief `true` when the cooked payload is interpreted as sRGB data. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Editor-facing view of static mesh payload fields that are directly editable.
 */
struct StaticMeshAssetEditorPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.StaticMeshAssetEditorPayload";

    std::string Name{}; /**< @brief Logical mesh name stored in the cooked payload. */
    std::vector<AssetRefPayload> MaterialInstances{}; /**< @brief Ordered material-instance overrides referenced by the mesh sections. */
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
