#include "RenderAssets/SkeletonPayload.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    SkeletalBonePayload,
    (TTypeBuilder<SkeletalBonePayload>(SkeletalBonePayload::kTypeName)
        .Field("Name", &SkeletalBonePayload::Name, EFieldFlagBits::Serialized)
        .Field("ParentIndex", &SkeletalBonePayload::ParentIndex, EFieldFlagBits::Serialized)
        .Field("BindPose", &SkeletalBonePayload::BindPose, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    SkeletonPayload,
    (TTypeBuilder<SkeletonPayload>(SkeletonPayload::kTypeName)
        .Field("Name", &SkeletonPayload::Name, EFieldFlagBits::Serialized)
        .Field("Bones", &SkeletonPayload::Bones, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

TExpected<void> SerializeSkeletonPayload(const SkeletonPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return Detail::SerializeBinaryPayload(Payload, OutBytes);
}

TExpected<SkeletonPayload> DeserializeSkeletonPayload(const uint8_t* Bytes, const size_t Size)
{
    return Detail::DeserializeBinaryPayload<SkeletonPayload>(Bytes, Size, "Null payload bytes");
}

} // namespace SnAPI::GameFramework
