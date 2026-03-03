#pragma once

#include <memory>

#include "IPayloadSerializer.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Create the payload serializer for Node cooked data.
 * @return Serializer instance.
 * @remarks Serializer encodes/decodes `NodePayload` byte format.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateNodePayloadSerializer();
/**
 * @brief Create the payload serializer for Level cooked data.
 * @return Serializer instance.
 * @remarks Serializer encodes/decodes `LevelPayload` byte format.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateLevelPayloadSerializer();
/**
 * @brief Create the payload serializer for World cooked data.
 * @return Serializer instance.
 * @remarks Serializer encodes/decodes `WorldPayload` byte format.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateWorldPayloadSerializer();
/**
 * @brief Create the payload serializer for StaticMesh cooked data.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateStaticMeshPayloadSerializer();
/**
 * @brief Create the payload serializer for SkeletalMesh cooked data.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletalMeshPayloadSerializer();
/**
 * @brief Create the payload serializer for Material cooked data.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateMaterialPayloadSerializer();
/**
 * @brief Create the payload serializer for MaterialInstance cooked data.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateMaterialInstancePayloadSerializer();
/**
 * @brief Create the payload serializer for Skeleton cooked data.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletonPayloadSerializer();
/**
 * @brief Create the payload serializer for Animation cooked data.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateAnimationPayloadSerializer();
/**
 * @brief Create the payload serializer for StaticMesh source-intermediate data.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateStaticMeshSourcePayloadSerializer();
/**
 * @brief Create the payload serializer for SkeletalMesh source-intermediate data.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletalMeshSourcePayloadSerializer();

} // namespace SnAPI::GameFramework
