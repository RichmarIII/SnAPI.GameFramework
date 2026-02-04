#pragma once

#include <memory>

#include "IPayloadSerializer.h"

namespace SnAPI::GameFramework
{

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateNodeGraphPayloadSerializer();
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateLevelPayloadSerializer();
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateWorldPayloadSerializer();

} // namespace SnAPI::GameFramework
