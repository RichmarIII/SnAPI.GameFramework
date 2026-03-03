#pragma once

#include "RenderAssetSourcePayloads.h"

#include "IAssetImportSettings.h"

#include <memory>
#include <string>

namespace SnAPI::GameFramework
{

/**
 * @brief Typed importer settings for Assimp-driven DCC imports.
 * @remarks This replaces stringly-typed build-option lookups for importer-side settings.
 */
struct AssimpImporterSettings final : public ::SnAPI::AssetPipeline::IAssetImportSettings
{
    MeshImportSettingsPayload Mesh{};
    std::string LogicalNameOverride{};
    std::string DefaultShaderModule{"DefaultGBufferMaterial"};
    std::string DefaultShadingModel{"GBufferShadingModel"};

    [[nodiscard]] std::unique_ptr<::SnAPI::AssetPipeline::IAssetImportSettings> Clone() const override
    {
        return std::make_unique<AssimpImporterSettings>(*this);
    }
};

} // namespace SnAPI::GameFramework

