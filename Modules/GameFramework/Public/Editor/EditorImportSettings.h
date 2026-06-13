#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "BuiltinTypes.h"
#include "RenderAssetImportSettings.h"
#include "RenderAssets/MaterialInstanceAsset.h"
#include "RenderAssetPayloads.h"
#include "TypeName.h"
#include "ReflectionAnnotations.h"

namespace SnAPI::GameFramework::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Editor-facing view of cooked texture payload metadata.
 *
 * This structure is used for inspector presentation and dirty-state tracking of texture assets.
 * In the current editor flow, most texture knobs are reimport settings rather than direct
 * runtime payload edits, so this payload mainly exposes preview metadata and derived state.
 */
SnType()
struct TextureAssetEditorPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.TextureAssetEditorPayload";

    SnField(SnKey("Target"), SnReadOnly)
    ETextureCompressionTarget Target = ETextureCompressionTarget::BCn; /**< @brief Compression family reported by the cooked payload. */
    SnField(SnKey("Format"), SnReadOnly)
    ETextureCompressionFormat Format = ETextureCompressionFormat::Auto; /**< @brief Exact cooked format reported by the payload when known. */
    SnField(SnKey("Quality"), SnReadOnly)
    float Quality = 0.6f; /**< @brief Normalized cooker quality hint recorded for preview. */
    SnField(SnKey("Width"), SnReadOnly)
    uint32_t Width = 0; /**< @brief Base-level texture width in texels. */
    SnField(SnKey("Height"), SnReadOnly)
    uint32_t Height = 0; /**< @brief Base-level texture height in texels. */
    SnField(SnKey("MipCount"), SnReadOnly)
    uint32_t MipCount = 0; /**< @brief Number of mip levels present in the cooked payload. */
    SnField(SnKey("SRGB"), SnReadOnly)
    bool SRGB = true; /**< @brief `true` when the cooked payload is interpreted as sRGB data. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Editor-facing view of static mesh payload fields that are directly editable.
 */
SnType()
struct StaticMeshAssetEditorPayload
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Editor.StaticMeshAssetEditorPayload";

    SnField(SnKey("Name"), SnReadOnly)
    std::string Name{}; /**< @brief Logical mesh name stored in the cooked payload. */
    std::vector<MaterialInstanceAssetRef> MaterialInstances{}; /**< @brief Ordered material-instance overrides referenced by the mesh sections. */
};

} // namespace SnAPI::GameFramework::Editor

namespace SnAPI::GameFramework
{
} // namespace SnAPI::GameFramework
