#include "RenderAssets/AnimationPayload.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    AnimationKeyFramePayload,
    (TTypeBuilder<AnimationKeyFramePayload>(AnimationKeyFramePayload::kTypeName)
        .Field("Time", &AnimationKeyFramePayload::Time, EFieldFlagBits::Serialized)
        .Field("Translation", &AnimationKeyFramePayload::Translation, EFieldFlagBits::Serialized)
        .Field("Rotation", &AnimationKeyFramePayload::Rotation, EFieldFlagBits::Serialized)
        .Field("Scale", &AnimationKeyFramePayload::Scale, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    AnimationTrackPayload,
    (TTypeBuilder<AnimationTrackPayload>(AnimationTrackPayload::kTypeName)
        .Field("BoneName", &AnimationTrackPayload::BoneName, EFieldFlagBits::Serialized)
        .Field("KeyFrames", &AnimationTrackPayload::KeyFrames, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    AnimationPayload,
    (TTypeBuilder<AnimationPayload>(AnimationPayload::kTypeName)
        .Field("Name", &AnimationPayload::Name, EFieldFlagBits::Serialized)
        .Field("DurationSeconds", &AnimationPayload::DurationSeconds, EFieldFlagBits::Serialized)
        .Field("TicksPerSecond", &AnimationPayload::TicksPerSecond, EFieldFlagBits::Serialized)
        .Field("Tracks", &AnimationPayload::Tracks, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

TExpected<void> SerializeAnimationPayload(const AnimationPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return Detail::SerializeBinaryPayload(Payload, OutBytes);
}

TExpected<AnimationPayload> DeserializeAnimationPayload(const uint8_t* Bytes, const size_t Size)
{
    return Detail::DeserializeBinaryPayload<AnimationPayload>(Bytes, Size, "Null payload bytes");
}

} // namespace SnAPI::GameFramework
