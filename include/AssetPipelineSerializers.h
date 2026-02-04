#pragma once

#include <memory>

#include "IPayloadSerializer.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Create the payload serializer for NodeGraph cooked data.
 * @return Serializer instance.
 * @remarks Uses cereal-based binary serialization.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateNodeGraphPayloadSerializer();
/**
 * @brief Create the payload serializer for Level cooked data.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateLevelPayloadSerializer();
/**
 * @brief Create the payload serializer for World cooked data.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateWorldPayloadSerializer();

} // namespace SnAPI::GameFramework
