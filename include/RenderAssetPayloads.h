#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "RenderAssets/AnimationPayload.h"
#include "RenderAssets/AssetRefPayload.h"
#include "RenderAssets/ImportedAssetProvenancePayload.h"
#include "RenderAssets/MaterialAsset.h"
#include "RenderAssets/MaterialInstanceAsset.h"
#include "RenderAssets/SkeletalMeshPayload.h"
#include "RenderAssets/SkeletonPayload.h"
#include "RenderAssets/StaticMeshPayload.h"

namespace SnAPI::GameFramework
{

using MaterialPayload = MaterialAsset;
using MaterialInstancePayload = MaterialInstanceAsset;

} // namespace SnAPI::GameFramework
