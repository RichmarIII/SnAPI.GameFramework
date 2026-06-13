#pragma once

#include <memory>

#include "IPayloadSerializer.h"
#include "RenderAssetSerializers/AnimationPayloadSerializer.h"
#include "RenderAssetSerializers/MaterialInstancePayloadSerializer.h"
#include "RenderAssetSerializers/MaterialPayloadSerializer.h"
#include "RenderAssetSerializers/SkeletalMeshPayloadSerializer.h"
#include "RenderAssetSerializers/SkeletalMeshSourcePayloadSerializer.h"
#include "RenderAssetSerializers/SkeletonPayloadSerializer.h"
#include "RenderAssetSerializers/StaticMeshPayloadSerializer.h"
#include "RenderAssetSerializers/StaticMeshSourcePayloadSerializer.h"
#include "RenderAssetSerializers/TextureSourcePayloadSerializer.h"

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
} // namespace SnAPI::GameFramework
