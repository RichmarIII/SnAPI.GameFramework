#pragma once

#include "AssetManager.h"
#include "PayloadRegistry.h"

namespace SnAPI::GameFramework
{

void RegisterAssetPipelinePayloads(::SnAPI::AssetPipeline::PayloadRegistry& Registry);
void RegisterAssetPipelineFactories(::SnAPI::AssetPipeline::AssetManager& Manager);

} // namespace SnAPI::GameFramework
