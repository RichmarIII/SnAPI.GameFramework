#pragma once

#include <string>
#include <string_view>

#include "IPayloadSerializer.h"

namespace SnAPI::GameFramework
{

inline ::SnAPI::AssetPipeline::Uuid AssetPipelineNamespace()
{
    static const auto Namespace = ::SnAPI::AssetPipeline::Uuid::FromString("8b76c145-755f-4bda-b3a7-593eb5c9129d");
    return Namespace;
}

inline ::SnAPI::AssetPipeline::TypeId AssetPipelineTypeIdFromName(std::string_view Name)
{
    return ::SnAPI::AssetPipeline::Uuid::GenerateV5(AssetPipelineNamespace(), std::string(Name));
}

inline ::SnAPI::AssetPipeline::AssetId AssetPipelineAssetIdFromName(std::string_view Name)
{
    return ::SnAPI::AssetPipeline::Uuid::GenerateV5(AssetPipelineNamespace(), std::string(Name));
}

constexpr const char* kAssetKindNodeGraphName = "SnAPI.GameFramework.AssetKind.NodeGraph";
constexpr const char* kAssetKindLevelName = "SnAPI.GameFramework.AssetKind.Level";
constexpr const char* kAssetKindWorldName = "SnAPI.GameFramework.AssetKind.World";

constexpr const char* kPayloadNodeGraphName = "SnAPI.GameFramework.NodeGraphPayload";
constexpr const char* kPayloadLevelName = "SnAPI.GameFramework.LevelPayload";
constexpr const char* kPayloadWorldName = "SnAPI.GameFramework.WorldPayload";

inline ::SnAPI::AssetPipeline::TypeId AssetKindNodeGraph()
{
    return AssetPipelineTypeIdFromName(kAssetKindNodeGraphName);
}

inline ::SnAPI::AssetPipeline::TypeId AssetKindLevel()
{
    return AssetPipelineTypeIdFromName(kAssetKindLevelName);
}

inline ::SnAPI::AssetPipeline::TypeId AssetKindWorld()
{
    return AssetPipelineTypeIdFromName(kAssetKindWorldName);
}

inline ::SnAPI::AssetPipeline::TypeId PayloadNodeGraph()
{
    return AssetPipelineTypeIdFromName(kPayloadNodeGraphName);
}

inline ::SnAPI::AssetPipeline::TypeId PayloadLevel()
{
    return AssetPipelineTypeIdFromName(kPayloadLevelName);
}

inline ::SnAPI::AssetPipeline::TypeId PayloadWorld()
{
    return AssetPipelineTypeIdFromName(kPayloadWorldName);
}

} // namespace SnAPI::GameFramework
