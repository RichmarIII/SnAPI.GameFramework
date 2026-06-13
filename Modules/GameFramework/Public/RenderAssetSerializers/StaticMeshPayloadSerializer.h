#pragma once

#include <memory>

#include "IPayloadSerializer.h"

namespace SnAPI::GameFramework
{

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateStaticMeshPayloadSerializer();

} // namespace SnAPI::GameFramework
