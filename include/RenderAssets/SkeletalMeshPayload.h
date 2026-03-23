#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "BuiltinTypes.h"
#include "Expected.h"
#include "RenderAssets/SkeletalAnimationAsset.h"
#include "RenderAssets/SkeletonAsset.h"
#include "RenderAssets/SkeletonPayload.h"
#include "RenderAssets/StaticMeshPayload.h"

namespace SnAPI::GameFramework
{

struct SkeletalMeshPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletalMeshPayload";

    StaticMeshPayload BaseMesh{};
    std::vector<SkeletalBonePayload> Bones{};
    SkeletonAssetRef Skeleton{};
    std::vector<SkeletalAnimationAssetRef> Animations{};
    uint32_t SkeletonAnimationBulkIndex = std::numeric_limits<uint32_t>::max();

    bool operator==(const SkeletalMeshPayload&) const = default;
};

TExpected<void> SerializeSkeletalMeshPayload(const SkeletalMeshPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<SkeletalMeshPayload> DeserializeSkeletalMeshPayload(const uint8_t* Bytes, size_t Size);

} // namespace SnAPI::GameFramework
