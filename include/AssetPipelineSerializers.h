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

} // namespace SnAPI::GameFramework
