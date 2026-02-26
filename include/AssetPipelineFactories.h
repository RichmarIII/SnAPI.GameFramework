#pragma once

#include <string>

#include "AssetManager.h"
#include "Handle.h"
#include "Handles.h"
#include "PayloadRegistry.h"

namespace SnAPI::GameFramework
{

class Level;
class World;
class IWorld;

/**
 * @brief Runtime load params for node assets.
 * @remarks
 * When `TargetWorld` is provided, node payload content is instantiated directly
 * into that world under `Parent` (or world root if null).
 */
struct NodeAssetLoadParams
{
    IWorld* TargetWorld = nullptr;
    NodeHandle Parent{};
    bool InstantiateAsCopy = true; /**< @brief When true, regenerate node/component UUIDs during load to avoid collisions. */
    NodeHandle* OutCreatedRoot = nullptr; /**< @brief Optional out-pointer receiving the created node handle when instantiated into a world. */
};

/**
 * @brief Runtime load params for level assets.
 * @remarks
 * When `TargetWorld` is provided, a new level is created and deserialized into
 * the target world. Optional name override is applied to the created level.
 */
struct LevelAssetLoadParams
{
    World* TargetWorld = nullptr;
    std::string NameOverride{};
    bool InstantiateAsCopy = true; /**< @brief When true, regenerate node/component UUIDs during load to avoid collisions. */
    NodeHandle* OutCreatedLevel = nullptr; /**< @brief Optional out-pointer receiving the created level handle when deserialized into a world. */
};

/**
 * @brief Runtime load params for world assets.
 * @remarks
 * When `TargetWorld` is provided, payload content is deserialized into that world.
 */
struct WorldAssetLoadParams
{
    World* TargetWorld = nullptr;
    bool InstantiateAsCopy = true; /**< @brief When true, regenerate node/component UUIDs during load to avoid collisions. */
};

/**
 * @brief Register GameFramework payload serializers with the AssetPipeline registry.
 * @param Registry Payload registry to populate.
 * @remarks Must be called before loading/cooking GameFramework payload-bearing assets.
 */
void RegisterAssetPipelinePayloads(::SnAPI::AssetPipeline::PayloadRegistry& Registry);
/**
 * @brief Register GameFramework runtime factories with the AssetManager.
 * @param Manager Asset manager to register factories with.
 * @remarks Enables runtime object materialization for `BaseNode`, `Level`, and `World` assets.
 */
void RegisterAssetPipelineFactories(::SnAPI::AssetPipeline::AssetManager& Manager);

} // namespace SnAPI::GameFramework
