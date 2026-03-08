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
 * @ingroup SnAPI_GameFramework
 * @brief Runtime load parameters for node assets.
 *
 * `NodeAssetLoadParams` controls whether a node asset is returned as an unattached heap object or is
 * instantiated directly into a live world. Asset factories inspect this payload through the generic
 * `AssetLoadContext::Params` channel.
 *
 * Core semantics:
 * - When `TargetWorld` is null, the node asset factory returns a detached runtime object.
 * - When `TargetWorld` is non-null, deserialization materializes the node subtree directly into that
 *   world under `Parent` or the world root.
 * - `InstantiateAsCopy` controls UUID regeneration during deserialization and should be left enabled
 *   for most editor placement and duplication paths.
 */
struct NodeAssetLoadParams
{
    IWorld* TargetWorld = nullptr; /**< @brief Borrowed target world that will receive the instantiated node subtree, or `nullptr` for detached loads. */
    NodeHandle Parent{}; /**< @brief Parent node under which the created root should be attached. A null handle means the world root. */
    bool InstantiateAsCopy = true; /**< @brief When true, regenerate node/component UUIDs during load to avoid collisions. */
    NodeHandle* OutCreatedRoot = nullptr; /**< @brief Optional out-pointer receiving the created node handle when instantiated into a world. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime load parameters for level assets.
 *
 * When `TargetWorld` is supplied, the level asset factory creates a new level in that world and
 * deserializes the cooked payload into it.
 */
struct LevelAssetLoadParams
{
    World* TargetWorld = nullptr; /**< @brief Borrowed destination world that will receive a newly created level, or `nullptr` for detached loads. */
    std::string NameOverride{}; /**< @brief Optional replacement name for the created level. Empty keeps the payload or catalog name. */
    bool InstantiateAsCopy = true; /**< @brief When true, regenerate node/component UUIDs during load to avoid collisions. */
    NodeHandle* OutCreatedLevel = nullptr; /**< @brief Optional out-pointer receiving the created level handle when deserialized into a world. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime load parameters for world assets.
 *
 * World payloads can either be loaded as detached runtime objects or deserialized directly into an
 * existing destination world.
 */
struct WorldAssetLoadParams
{
    World* TargetWorld = nullptr; /**< @brief Borrowed destination world that will receive the payload contents, or `nullptr` for detached loads. */
    bool InstantiateAsCopy = true; /**< @brief When true, regenerate node/component UUIDs during load to avoid collisions. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Register GameFramework payload serializers with an AssetPipeline payload registry.
 * @param Registry Registry to populate.
 *
 * This registers every cooked and source-intermediate payload serializer used by the GameFramework
 * asset types. It is a prerequisite for any code path that serializes, deserializes, imports, or cooks
 * GameFramework-owned payloads through the AssetPipeline.
 *
 * Threading:
 * - Not documented as thread-safe; perform registration during bootstrap.
 */
void RegisterAssetPipelinePayloads(::SnAPI::AssetPipeline::PayloadRegistry& Registry);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Register GameFramework runtime factories and payload migrations with an asset manager.
 * @param Manager Asset manager to extend.
 *
 * This installs:
 * - payload migrations for legacy node, level, and world payload schemas
 * - runtime factories for node, world, level, mesh, skeleton, animation, material, and material-instance assets
 * - renderer texture factories when renderer support is compiled in
 *
 * Call this before resolving GameFramework asset references through the supplied manager.
 */
void RegisterAssetPipelineFactories(::SnAPI::AssetPipeline::AssetManager& Manager);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Register source import and cook stages used by GameFramework asset ingestion.
 * @param Manager Asset manager to extend.
 *
 * This installs the importers, cookers, and auxiliary serializers used to transform source authoring
 * inputs such as DCC files, textures, and intermediate mesh payloads into cooked runtime assets.
 */
void RegisterAssetPipelineSourceStages(::SnAPI::AssetPipeline::AssetManager& Manager);

} // namespace SnAPI::GameFramework
