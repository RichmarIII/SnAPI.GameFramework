#include "Export.h"

#include "AssetPipelineSerializers.h"
#include "IAssetCooker.h"
#include "IAssetImporter.h"
#include "IPluginRegistrar.h"

#include <memory>

namespace SnAPI::GameFramework
{
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetJsonImporter();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetAssimpImporter();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialInstanceCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderSkeletonCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderAnimationCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderStaticMeshCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderSkeletalMeshCooker();

/**
 * @brief Register the GameFramework AssetPipeline plugin.
 * @param Registrar AssetPipeline plugin registrar.
 * @remarks Registers payload serializers/importers/cookers for GameFramework assets.
 */
static void RegisterAssetPipelinePlugin(::SnAPI::AssetPipeline::IPluginRegistrar& Registrar)
{
    Registrar.RegisterPluginInfo("SnAPI.GameFramework", "0.1.0");
    Registrar.RegisterPayloadSerializer(CreateNodePayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateLevelPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateWorldPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateStaticMeshPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateSkeletalMeshPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateMaterialPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateMaterialInstancePayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateSkeletonPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateAnimationPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateStaticMeshSourcePayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateSkeletalMeshSourcePayloadSerializer());

    Registrar.RegisterImporter(CreateRenderAssetAssimpImporter());
    Registrar.RegisterImporter(CreateRenderAssetJsonImporter());

    Registrar.RegisterCooker(CreateRenderMaterialCooker());
    Registrar.RegisterCooker(CreateRenderMaterialInstanceCooker());
    Registrar.RegisterCooker(CreateRenderSkeletonCooker());
    Registrar.RegisterCooker(CreateRenderAnimationCooker());
    Registrar.RegisterCooker(CreateRenderStaticMeshCooker());
    Registrar.RegisterCooker(CreateRenderSkeletalMeshCooker());
}

} // namespace SnAPI::GameFramework

/**
 * @brief Define the AssetPipeline plugin entry point.
 * @remarks Exposes RegisterAssetPipelinePlugin as a plugin symbol.
 */
SNAPI_DEFINE_PLUGIN(SnAPI::GameFramework::RegisterAssetPipelinePlugin)
