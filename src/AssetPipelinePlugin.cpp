#include "Export.h"

#include "AssetPipelineSerializers.h"
#include "IPluginRegistrar.h"

#include <memory>

namespace SnAPI::GameFramework
{

/**
 * @brief Register the GameFramework AssetPipeline plugin.
 * @param Registrar AssetPipeline plugin registrar.
 * @remarks Registers payload serializers for Level, Level, and World.
 */
static void RegisterAssetPipelinePlugin(::SnAPI::AssetPipeline::IPluginRegistrar& Registrar)
{
    Registrar.RegisterPluginInfo("SnAPI.GameFramework", "0.1.0");
    Registrar.RegisterPayloadSerializer(CreateLevelGraphPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateLevelPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateWorldPayloadSerializer());
}

} // namespace SnAPI::GameFramework

/**
 * @brief Define the AssetPipeline plugin entry point.
 * @remarks Exposes RegisterAssetPipelinePlugin as a plugin symbol.
 */
SNAPI_DEFINE_PLUGIN(SnAPI::GameFramework::RegisterAssetPipelinePlugin)
