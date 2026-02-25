#pragma once

#include <memory>

#include "IPayloadSerializer.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Create the payload serializer for Level cooked data.
 * @return Serializer instance.
 * @remarks Serializer encodes/decodes `LevelGraphPayload` byte format.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateLevelGraphPayloadSerializer();
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
