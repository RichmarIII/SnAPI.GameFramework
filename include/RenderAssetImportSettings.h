#pragma once

#include "RenderAssetSourcePayloads.h"

#include "IAssetImportSettings.h"

#include <memory>
#include <string>

namespace SnAPI::GameFramework
{

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
struct AssimpImporterSettings final : public ::SnAPI::AssetPipeline::IAssetImportSettings
{
    MeshImportSettingsPayload Mesh{}; /**< @brief Mesh-import policy flags forwarded into the source payload. */
    std::string LogicalNameOverride{}; /**< @brief Optional asset display-name override applied during import. */
    std::string DefaultShaderModule{"DefaultGBufferMaterial"}; /**< @brief Shader module used when imported materials need a default runtime material. */
    std::string DefaultShadingModel{"GBufferShadingModel"}; /**< @brief Shading model used when imported materials need a default runtime material. */

    /** @brief Deep-clone this settings object through the `IAssetImportSettings` interface. */
    [[nodiscard]] std::unique_ptr<::SnAPI::AssetPipeline::IAssetImportSettings> Clone() const override
    {
        return std::make_unique<AssimpImporterSettings>(*this);
    }
};

} // namespace SnAPI::GameFramework
