#pragma once

#include <array>
#include <string>
#include <vector>

#include "Expected.h"
#include "ReflectionAnnotations.h"
#include "TypeName.h"

namespace SnAPI::GameFramework
{

struct AnimationKeyFramePayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AnimationKeyFramePayload";

    float Time = 0.0f;
    std::array<float, 3> Translation{0.0f, 0.0f, 0.0f};
    std::array<float, 4> Rotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> Scale{1.0f, 1.0f, 1.0f};

    bool operator==(const AnimationKeyFramePayload&) const = default;
};

struct AnimationTrackPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AnimationTrackPayload";

    std::string BoneName{};
    std::vector<AnimationKeyFramePayload> KeyFrames{};

    bool operator==(const AnimationTrackPayload&) const = default;
};

struct AnimationPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AnimationPayload";

    std::string Name{};
    float DurationSeconds = 0.0f;
    float TicksPerSecond = 0.0f;
    std::vector<AnimationTrackPayload> Tracks{};

    bool operator==(const AnimationPayload&) const = default;
};

TExpected<void> SerializeAnimationPayload(const AnimationPayload& Payload, std::vector<uint8_t>& OutBytes);
TExpected<AnimationPayload> DeserializeAnimationPayload(const uint8_t* Bytes, size_t Size);

SNAPI_DEFINE_TYPE_NAME(std::vector<AnimationKeyFramePayload>, "std::vector<SnAPI::GameFramework::AnimationKeyFramePayload>")
SNAPI_DEFINE_TYPE_NAME(std::vector<AnimationTrackPayload>, "std::vector<SnAPI::GameFramework::AnimationTrackPayload>")

} // namespace SnAPI::GameFramework
