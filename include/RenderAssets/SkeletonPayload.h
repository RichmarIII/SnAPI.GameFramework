#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Expected.h"
#include "ReflectionAnnotations.h"
#include "TypeName.h"

namespace SnAPI::GameFramework
{

SnType()
struct SkeletalBonePayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletalBonePayload";

    SnField(SnKey("Name"))
    std::string Name{};
    SnField(SnKey("ParentIndex"))
    int32_t ParentIndex = -1;
    SnField(SnKey("BindPose"))
    std::array<float, 16> BindPose{};

    bool operator==(const SkeletalBonePayload&) const = default;
};

struct SkeletonPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletonPayload";

    std::string Name{};
    std::vector<SkeletalBonePayload> Bones{};

    bool operator==(const SkeletonPayload&) const = default;
};

TExpected<void> SerializeSkeletonPayload(const SkeletonPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<SkeletonPayload> DeserializeSkeletonPayload(const uint8_t* Bytes, size_t Size);

SNAPI_DEFINE_TYPE_NAME(std::vector<SkeletalBonePayload>, "std::vector<SnAPI::GameFramework::SkeletalBonePayload>")

} // namespace SnAPI::GameFramework
