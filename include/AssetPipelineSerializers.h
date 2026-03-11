#pragma once

#include <memory>

#include "IPayloadSerializer.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the AssetPipeline serializer for cooked `NodePayload` data.
 * @return Owning serializer instance.
 *
 * The returned serializer bridges the AssetPipeline `IPayloadSerializer` interface to the
 * GameFramework `NodeSerializer` binary format and schema version.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateNodePayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the AssetPipeline serializer for cooked `LevelPayload` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateLevelPayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the AssetPipeline serializer for cooked `WorldPayload` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateWorldPayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the AssetPipeline serializer for authored `NodeAsset` source data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateNodeSourcePayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the AssetPipeline serializer for authored `LevelAsset` source data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateLevelSourcePayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the AssetPipeline serializer for authored `WorldAsset` source data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateWorldSourcePayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the AssetPipeline serializer for authored `Conduit::GraphAsset` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateConduitGraphPayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the AssetPipeline serializer for authored `Conduit::ClassAsset` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateConduitClassPayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the serializer for cooked `StaticMeshPayload` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateStaticMeshPayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the serializer for cooked `SkeletalMeshPayload` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletalMeshPayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the serializer for cooked `MaterialPayload` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateMaterialPayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the serializer for cooked `MaterialInstancePayload` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateMaterialInstancePayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the serializer for cooked `SkeletonPayload` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletonPayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the serializer for cooked `AnimationPayload` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateAnimationPayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the serializer for source-intermediate `StaticMeshSourcePayload` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateStaticMeshSourcePayloadSerializer();
/**
 * @ingroup SnAPI_GameFramework
 * @brief Create the serializer for source-intermediate `SkeletalMeshSourcePayload` data.
 * @return Owning serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletalMeshSourcePayloadSerializer();

} // namespace SnAPI::GameFramework
