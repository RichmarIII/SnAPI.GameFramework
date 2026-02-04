#pragma once

#include <string>
#include <string_view>

#include "IPayloadSerializer.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Namespace UUID for AssetPipeline ids.
 * @return Namespace UUID.
 * @remarks Used to generate deterministic ids from names.
 */
inline ::SnAPI::AssetPipeline::Uuid AssetPipelineNamespace()
{
    static const auto Namespace = ::SnAPI::AssetPipeline::Uuid::FromString("8b76c145-755f-4bda-b3a7-593eb5c9129d");
    return Namespace;
}

/**
 * @brief Generate a deterministic TypeId from a name.
 * @param Name Name string.
 * @return UUIDv5-based TypeId.
 * @remarks Uses AssetPipelineNamespace.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetPipelineTypeIdFromName(std::string_view Name)
{
    return ::SnAPI::AssetPipeline::Uuid::GenerateV5(AssetPipelineNamespace(), std::string(Name));
}

/**
 * @brief Generate a deterministic AssetId from a name.
 * @param Name Name string.
 * @return UUIDv5-based AssetId.
 * @remarks Uses AssetPipelineNamespace.
 */
inline ::SnAPI::AssetPipeline::AssetId AssetPipelineAssetIdFromName(std::string_view Name)
{
    return ::SnAPI::AssetPipeline::Uuid::GenerateV5(AssetPipelineNamespace(), std::string(Name));
}

/** @brief Asset kind name for NodeGraph assets. */
constexpr const char* kAssetKindNodeGraphName = "SnAPI.GameFramework.AssetKind.NodeGraph";
/** @brief Asset kind name for Level assets. */
constexpr const char* kAssetKindLevelName = "SnAPI.GameFramework.AssetKind.Level";
/** @brief Asset kind name for World assets. */
constexpr const char* kAssetKindWorldName = "SnAPI.GameFramework.AssetKind.World";

/** @brief Payload type name for NodeGraph cooked data. */
constexpr const char* kPayloadNodeGraphName = "SnAPI.GameFramework.NodeGraphPayload";
/** @brief Payload type name for Level cooked data. */
constexpr const char* kPayloadLevelName = "SnAPI.GameFramework.LevelPayload";
/** @brief Payload type name for World cooked data. */
constexpr const char* kPayloadWorldName = "SnAPI.GameFramework.WorldPayload";

/**
 * @brief Get the AssetPipeline TypeId for NodeGraph assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindNodeGraph()
{
    return AssetPipelineTypeIdFromName(kAssetKindNodeGraphName);
}

/**
 * @brief Get the AssetPipeline TypeId for Level assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindLevel()
{
    return AssetPipelineTypeIdFromName(kAssetKindLevelName);
}

/**
 * @brief Get the AssetPipeline TypeId for World assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindWorld()
{
    return AssetPipelineTypeIdFromName(kAssetKindWorldName);
}

/**
 * @brief Get the payload TypeId for NodeGraph payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadNodeGraph()
{
    return AssetPipelineTypeIdFromName(kPayloadNodeGraphName);
}

/**
 * @brief Get the payload TypeId for Level payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadLevel()
{
    return AssetPipelineTypeIdFromName(kPayloadLevelName);
}

/**
 * @brief Get the payload TypeId for World payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadWorld()
{
    return AssetPipelineTypeIdFromName(kPayloadWorldName);
}

} // namespace SnAPI::GameFramework
