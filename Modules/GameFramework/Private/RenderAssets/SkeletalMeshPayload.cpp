#include "RenderAssets/SkeletalMeshPayload.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    SkeletalMeshPayload,
    (TTypeBuilder<SkeletalMeshPayload>("SnAPI::GameFramework::SkeletalMeshPayload")
        .Field("BaseMesh", &SkeletalMeshPayload::BaseMesh, EFieldFlagBits::Serialized)
        .Field("Bones", &SkeletalMeshPayload::Bones, EFieldFlagBits::Serialized)
        .Field("Skeleton", &SkeletalMeshPayload::Skeleton, EFieldFlagBits::Serialized)
        .Field("Animations", &SkeletalMeshPayload::Animations, EFieldFlagBits::Serialized)
        .Field("SkeletonAnimationBulkIndex", &SkeletalMeshPayload::SkeletonAnimationBulkIndex, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

TExpected<void> SerializeSkeletalMeshPayload(const SkeletalMeshPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return Detail::SerializeBinaryPayload(Payload, OutBytes);
}

TExpected<SkeletalMeshPayload> DeserializeSkeletalMeshPayload(const uint8_t* Bytes, const size_t Size)
{
    return Detail::DeserializeBinaryPayload<SkeletalMeshPayload>(Bytes, Size, "Null payload bytes");
}

} // namespace SnAPI::GameFramework
