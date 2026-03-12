#pragma once

#include <string>
#include <string_view>

#include "IPayloadSerializer.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Shared UUID namespace for deterministic GameFramework AssetPipeline identifiers.
 * @return UUID namespace value.
 *
 * Every helper in this header derives stable UUIDv5 identifiers from this namespace plus a stable
 * string literal. Changing the namespace or the input names would invalidate cooked compatibility.
 */
inline ::SnAPI::AssetPipeline::Uuid AssetPipelineNamespace()
{
    static const auto Namespace = ::SnAPI::AssetPipeline::Uuid::FromString("8b76c145-755f-4bda-b3a7-593eb5c9129d");
    return Namespace;
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Generate a deterministic AssetPipeline `TypeId` from a stable name.
 * @param Name Stable symbolic name.
 * @return UUIDv5-derived type id.
 *
 * This is used for both asset-kind ids and payload-type ids. The generated value is stable across
 * processes and machines as long as the name remains unchanged.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetPipelineTypeIdFromName(std::string_view Name)
{
    return ::SnAPI::AssetPipeline::Uuid::GenerateV5(AssetPipelineNamespace(), std::string(Name));
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Generate a deterministic `AssetId` from a stable name.
 * @param Name Stable symbolic name.
 * @return UUIDv5-derived asset id.
 *
 * This helper is intended for built-in or synthetic assets whose identity is derived from a known
 * symbolic name rather than editor-generated UUIDs.
 */
inline ::SnAPI::AssetPipeline::AssetId AssetPipelineAssetIdFromName(std::string_view Name)
{
    return ::SnAPI::AssetPipeline::Uuid::GenerateV5(AssetPipelineNamespace(), std::string(Name));
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Shared UUID namespace for deterministic source-asset identifiers.
 * @return UUID namespace value.
 *
 * Source-authored assets use a separate stable namespace so a logical source path can map to the
 * same id before the asset manager has catalogued it.
 */
inline ::SnAPI::AssetPipeline::Uuid SourceAssetNamespace()
{
    static const auto Namespace = ::SnAPI::AssetPipeline::Uuid::FromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
    return Namespace;
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Generate the deterministic id used for source-authored assets.
 * @param LogicalName Logical asset path relative to the asset root.
 * @param VariantKey Optional variant key.
 * @return UUIDv5-derived source asset id.
 */
inline ::SnAPI::AssetPipeline::AssetId SourceAssetIdFromLogicalName(std::string_view LogicalName,
                                                                    std::string_view VariantKey = {})
{
    std::string Combined{};
    Combined.reserve(LogicalName.size() + VariantKey.size() + 1u);
    Combined.append(LogicalName);
    Combined.push_back('|');
    Combined.append(VariantKey);
    return ::SnAPI::AssetPipeline::Uuid::GenerateV5(SourceAssetNamespace(), Combined);
}

/** @brief Asset kind name for Level assets. */
constexpr const char* kAssetKindLevelName = "SnAPI.GameFramework.AssetKind.Level";
/** @brief Asset kind name for Node assets. */
constexpr const char* kAssetKindNodeName = "SnAPI.GameFramework.AssetKind.Node";
/** @brief Asset kind name for World assets. */
constexpr const char* kAssetKindWorldName = "SnAPI.GameFramework.AssetKind.World";
/** @brief Asset kind name for StaticMesh assets. */
constexpr const char* kAssetKindStaticMeshName = "SnAPI.GameFramework.AssetKind.StaticMesh";
/** @brief Asset kind name for SkeletalMesh assets. */
constexpr const char* kAssetKindSkeletalMeshName = "SnAPI.GameFramework.AssetKind.SkeletalMesh";
/** @brief Asset kind name for Material assets. */
constexpr const char* kAssetKindMaterialName = "SnAPI.GameFramework.AssetKind.Material";
/** @brief Asset kind name for MaterialInstance assets. */
constexpr const char* kAssetKindMaterialInstanceName = "SnAPI.GameFramework.AssetKind.MaterialInstance";
/** @brief Asset kind name for Skeleton assets. */
constexpr const char* kAssetKindSkeletonName = "SnAPI.GameFramework.AssetKind.Skeleton";
/** @brief Asset kind name for Animation assets. */
constexpr const char* kAssetKindAnimationName = "SnAPI.GameFramework.AssetKind.Animation";
/** @brief Asset kind name for Conduit graph assets. */
constexpr const char* kAssetKindConduitGraphName = "SnAPI.GameFramework.AssetKind.ConduitGraph";
/** @brief Asset kind name for Conduit class assets. */
constexpr const char* kAssetKindConduitClassName = "SnAPI.GameFramework.AssetKind.ConduitClass";

/** @brief Payload type name for Node cooked data. */
constexpr const char* kPayloadNodeName = "SnAPI.GameFramework.NodePayload";
/** @brief Payload type name for authored Node source data. */
constexpr const char* kPayloadNodeSourceName = "SnAPI.GameFramework.NodeAssetSourcePayload";
/** @brief Payload type name for Level cooked data. */
constexpr const char* kPayloadLevelName = "SnAPI.GameFramework.LevelPayload";
/** @brief Payload type name for authored Level source data. */
constexpr const char* kPayloadLevelSourceName = "SnAPI.GameFramework.LevelAssetSourcePayload";
/** @brief Payload type name for World cooked data. */
constexpr const char* kPayloadWorldName = "SnAPI.GameFramework.WorldPayload";
/** @brief Payload type name for authored World source data. */
constexpr const char* kPayloadWorldSourceName = "SnAPI.GameFramework.WorldAssetSourcePayload";
/** @brief Payload type name for StaticMesh cooked data. */
constexpr const char* kPayloadStaticMeshName = "SnAPI.GameFramework.StaticMeshPayload";
/** @brief Payload type name for SkeletalMesh cooked data. */
constexpr const char* kPayloadSkeletalMeshName = "SnAPI.GameFramework.SkeletalMeshPayload";
/** @brief Payload type name for Material cooked data. */
constexpr const char* kPayloadMaterialName = "SnAPI.GameFramework.MaterialPayload";
/** @brief Payload type name for MaterialInstance cooked data. */
constexpr const char* kPayloadMaterialInstanceName = "SnAPI.GameFramework.MaterialInstancePayload";
/** @brief Payload type name for Skeleton cooked data. */
constexpr const char* kPayloadSkeletonName = "SnAPI.GameFramework.SkeletonPayload";
/** @brief Payload type name for Animation cooked data. */
constexpr const char* kPayloadAnimationName = "SnAPI.GameFramework.AnimationPayload";
/** @brief Payload type name for StaticMesh source-intermediate data. */
constexpr const char* kPayloadStaticMeshSourceName = "SnAPI.GameFramework.StaticMeshSourcePayload";
/** @brief Payload type name for SkeletalMesh source-intermediate data. */
constexpr const char* kPayloadSkeletalMeshSourceName = "SnAPI.GameFramework.SkeletalMeshSourcePayload";
/** @brief Payload type name for authored Conduit graph data. */
constexpr const char* kPayloadConduitGraphName = "SnAPI.GameFramework.ConduitGraphPayload";
/** @brief Payload type name for authored Conduit class data. */
constexpr const char* kPayloadConduitClassName = "SnAPI.GameFramework.ConduitClassPayload";

/**
 * @brief Get the AssetPipeline TypeId for Level assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindLevel()
{
    return AssetPipelineTypeIdFromName(kAssetKindLevelName);
}

/**
 * @brief Get the AssetPipeline TypeId for Node assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindNode()
{
    return AssetPipelineTypeIdFromName(kAssetKindNodeName);
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
 * @brief Get the AssetPipeline TypeId for StaticMesh assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindStaticMesh()
{
    return AssetPipelineTypeIdFromName(kAssetKindStaticMeshName);
}

/**
 * @brief Get the AssetPipeline TypeId for SkeletalMesh assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindSkeletalMesh()
{
    return AssetPipelineTypeIdFromName(kAssetKindSkeletalMeshName);
}

/**
 * @brief Get the AssetPipeline TypeId for Material assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindMaterial()
{
    return AssetPipelineTypeIdFromName(kAssetKindMaterialName);
}

/**
 * @brief Get the AssetPipeline TypeId for MaterialInstance assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindMaterialInstance()
{
    return AssetPipelineTypeIdFromName(kAssetKindMaterialInstanceName);
}

/**
 * @brief Get the AssetPipeline TypeId for Skeleton assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindSkeleton()
{
    return AssetPipelineTypeIdFromName(kAssetKindSkeletonName);
}

/**
 * @brief Get the AssetPipeline TypeId for Animation assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindAnimation()
{
    return AssetPipelineTypeIdFromName(kAssetKindAnimationName);
}

/**
 * @brief Get the AssetPipeline TypeId for Conduit class assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindConduitClass()
{
    return AssetPipelineTypeIdFromName(kAssetKindConduitClassName);
}

/**
 * @brief Get the AssetPipeline TypeId for Conduit graph assets.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId AssetKindConduitGraph()
{
    return AssetPipelineTypeIdFromName(kAssetKindConduitGraphName);
}

/**
 * @brief Get the payload TypeId for Node payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadNode()
{
    return AssetPipelineTypeIdFromName(kPayloadNodeName);
}

/**
 * @brief Get the payload TypeId for authored Node source payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadNodeSource()
{
    return AssetPipelineTypeIdFromName(kPayloadNodeSourceName);
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
 * @brief Get the payload TypeId for authored Level source payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadLevelSource()
{
    return AssetPipelineTypeIdFromName(kPayloadLevelSourceName);
}

/**
 * @brief Get the payload TypeId for World payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadWorld()
{
    return AssetPipelineTypeIdFromName(kPayloadWorldName);
}

/**
 * @brief Get the payload TypeId for authored World source payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadWorldSource()
{
    return AssetPipelineTypeIdFromName(kPayloadWorldSourceName);
}

/**
 * @brief Get the payload TypeId for StaticMesh payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadStaticMesh()
{
    return AssetPipelineTypeIdFromName(kPayloadStaticMeshName);
}

/**
 * @brief Get the payload TypeId for SkeletalMesh payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadSkeletalMesh()
{
    return AssetPipelineTypeIdFromName(kPayloadSkeletalMeshName);
}

/**
 * @brief Get the payload TypeId for Material payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadMaterial()
{
    return AssetPipelineTypeIdFromName(kPayloadMaterialName);
}

/**
 * @brief Get the payload TypeId for MaterialInstance payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadMaterialInstance()
{
    return AssetPipelineTypeIdFromName(kPayloadMaterialInstanceName);
}

/**
 * @brief Get the payload TypeId for Skeleton payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadSkeleton()
{
    return AssetPipelineTypeIdFromName(kPayloadSkeletonName);
}

/**
 * @brief Get the payload TypeId for Animation payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadAnimation()
{
    return AssetPipelineTypeIdFromName(kPayloadAnimationName);
}

/**
 * @brief Get the payload TypeId for authored Conduit graph payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadConduitGraph()
{
    return AssetPipelineTypeIdFromName(kPayloadConduitGraphName);
}

/**
 * @brief Get the payload TypeId for authored Conduit class payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadConduitClass()
{
    return AssetPipelineTypeIdFromName(kPayloadConduitClassName);
}

/**
 * @brief Get the payload TypeId for StaticMesh source-intermediate payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadStaticMeshSource()
{
    return AssetPipelineTypeIdFromName(kPayloadStaticMeshSourceName);
}

/**
 * @brief Get the payload TypeId for SkeletalMesh source-intermediate payloads.
 * @return TypeId value.
 */
inline ::SnAPI::AssetPipeline::TypeId PayloadSkeletalMeshSource()
{
    return AssetPipelineTypeIdFromName(kPayloadSkeletalMeshSourceName);
}

} // namespace SnAPI::GameFramework
