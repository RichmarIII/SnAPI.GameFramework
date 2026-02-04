#include "Export.h"

#include "AssetPipelineSerializers.h"
#include "IPluginRegistrar.h"

#include <memory>

namespace SnAPI::GameFramework
{

static void RegisterAssetPipelinePlugin(::SnAPI::AssetPipeline::IPluginRegistrar& Registrar)
{
    Registrar.RegisterPluginInfo("SnAPI.GameFramework", "0.1.0");
    Registrar.RegisterPayloadSerializer(CreateNodeGraphPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateLevelPayloadSerializer());
    Registrar.RegisterPayloadSerializer(CreateWorldPayloadSerializer());
}

} // namespace SnAPI::GameFramework

SNAPI_DEFINE_PLUGIN(SnAPI::GameFramework::RegisterAssetPipelinePlugin)
