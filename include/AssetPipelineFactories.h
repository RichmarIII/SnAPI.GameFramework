#pragma once

#include "AssetManager.h"
#include "PayloadRegistry.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Register GameFramework payload serializers with the AssetPipeline registry.
 * @param Registry Payload registry to populate.
 * @remarks Must be called before loading/cooking GameFramework payload-bearing assets.
 */
void RegisterAssetPipelinePayloads(::SnAPI::AssetPipeline::PayloadRegistry& Registry);
/**
 * @brief Register GameFramework runtime factories with the AssetManager.
 * @param Manager Asset manager to register factories with.
 * @remarks Enables runtime object materialization for `World`, `Level`, and `NodeGraph` assets.
 */
void RegisterAssetPipelineFactories(::SnAPI::AssetPipeline::AssetManager& Manager);

} // namespace SnAPI::GameFramework
