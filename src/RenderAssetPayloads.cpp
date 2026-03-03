#include "RenderAssetPayloads.h"

#include <exception>
#include <sstream>

#include <cereal/archives/binary.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace SnAPI::GameFramework
{
template<class Archive>
void serialize(Archive& Ar, AssetRefPayload& Value)
{
    Ar(Value.AssetName, Value.AssetId);
}

template<class Archive>
void serialize(Archive& Ar, MeshStreamChunkRef& Value)
{
    Ar(Value.Semantic, Value.BulkIndex, Value.ElementCount, Value.StrideBytes);
}

template<class Archive>
void serialize(Archive& Ar, StaticSubMeshPayload& Value)
{
    Ar(Value.IndexOffset, Value.IndexCount, Value.MaterialSlot, Value.BoundsMin, Value.BoundsMax);
}

template<class Archive>
void serialize(Archive& Ar, StaticMeshPayload& Value)
{
    Ar(Value.Name, Value.BoundsMin, Value.BoundsMax, Value.SubMeshes, Value.MaterialInstances, Value.Streams);
}

template<class Archive>
void serialize(Archive& Ar, SkeletalBonePayload& Value)
{
    Ar(Value.Name, Value.ParentIndex, Value.BindPose);
}

template<class Archive>
void serialize(Archive& Ar, SkeletonPayload& Value)
{
    Ar(Value.Name, Value.Bones);
}

template<class Archive>
void serialize(Archive& Ar, AnimationKeyFramePayload& Value)
{
    Ar(Value.Time, Value.Translation, Value.Rotation, Value.Scale);
}

template<class Archive>
void serialize(Archive& Ar, AnimationTrackPayload& Value)
{
    Ar(Value.BoneName, Value.KeyFrames);
}

template<class Archive>
void serialize(Archive& Ar, AnimationPayload& Value)
{
    Ar(Value.Name, Value.DurationSeconds, Value.TicksPerSecond, Value.Tracks);
}

template<class Archive>
void serialize(Archive& Ar, SkeletalMeshPayload& Value)
{
    Ar(Value.BaseMesh, Value.Bones, Value.Skeleton, Value.Animations, Value.SkeletonAnimationBulkIndex);
}

template<class Archive>
void serialize(Archive& Ar, MaterialPayload& Value)
{
    Ar(Value.ShaderModule, Value.ShadingModel);
}

template<class Archive>
void serialize(Archive& Ar, MaterialScalarParamPayload& Value)
{
    Ar(Value.Name, Value.Value);
}

template<class Archive>
void serialize(Archive& Ar, MaterialVectorParamPayload& Value)
{
    Ar(Value.Name, Value.Value);
}

template<class Archive>
void serialize(Archive& Ar, MaterialTextureParamPayload& Value)
{
    Ar(Value.SlotName, Value.Texture, Value.SRGB);
}

template<class Archive>
void serialize(Archive& Ar, MaterialInstancePayload& Value)
{
    Ar(Value.ParentMaterial, Value.Scalars, Value.Vectors, Value.Textures);
}

namespace
{

template<typename TPayload>
TExpected<void> SerializePayloadBinary(const TPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    try
    {
        std::ostringstream Stream(std::ios::binary);
        cereal::BinaryOutputArchive Archive(Stream);
        Archive(Payload);
        const std::string Bytes = Stream.str();
        OutBytes.assign(Bytes.begin(), Bytes.end());
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
    catch (...)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Unknown exception while serializing payload"));
    }
}

template<typename TPayload>
TExpected<TPayload> DeserializePayloadBinary(const uint8_t* Bytes, const size_t Size)
{
    if (!Bytes && Size > 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null payload bytes"));
    }

    try
    {
        const std::string Data(reinterpret_cast<const char*>(Bytes), Size);
        std::istringstream Stream(Data, std::ios::binary);
        cereal::BinaryInputArchive Archive(Stream);
        TPayload Payload{};
        Archive(Payload);
        return Payload;
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
    }
    catch (...)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown exception while deserializing payload"));
    }
}
} // namespace

TExpected<void> SerializeStaticMeshPayload(const StaticMeshPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return SerializePayloadBinary(Payload, OutBytes);
}

TExpected<StaticMeshPayload> DeserializeStaticMeshPayload(const uint8_t* Bytes, const size_t Size)
{
    return DeserializePayloadBinary<StaticMeshPayload>(Bytes, Size);
}

TExpected<void> SerializeSkeletalMeshPayload(const SkeletalMeshPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return SerializePayloadBinary(Payload, OutBytes);
}

TExpected<SkeletalMeshPayload> DeserializeSkeletalMeshPayload(const uint8_t* Bytes, const size_t Size)
{
    return DeserializePayloadBinary<SkeletalMeshPayload>(Bytes, Size);
}

TExpected<void> SerializeSkeletonPayload(const SkeletonPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return SerializePayloadBinary(Payload, OutBytes);
}

TExpected<SkeletonPayload> DeserializeSkeletonPayload(const uint8_t* Bytes, const size_t Size)
{
    return DeserializePayloadBinary<SkeletonPayload>(Bytes, Size);
}

TExpected<void> SerializeAnimationPayload(const AnimationPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return SerializePayloadBinary(Payload, OutBytes);
}

TExpected<AnimationPayload> DeserializeAnimationPayload(const uint8_t* Bytes, const size_t Size)
{
    return DeserializePayloadBinary<AnimationPayload>(Bytes, Size);
}

TExpected<void> SerializeMaterialPayload(const MaterialPayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return SerializePayloadBinary(Payload, OutBytes);
}

TExpected<MaterialPayload> DeserializeMaterialPayload(const uint8_t* Bytes, const size_t Size)
{
    return DeserializePayloadBinary<MaterialPayload>(Bytes, Size);
}

TExpected<void> SerializeMaterialInstancePayload(const MaterialInstancePayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return SerializePayloadBinary(Payload, OutBytes);
}

TExpected<MaterialInstancePayload> DeserializeMaterialInstancePayload(const uint8_t* Bytes, const size_t Size)
{
    return DeserializePayloadBinary<MaterialInstancePayload>(Bytes, Size);
}

} // namespace SnAPI::GameFramework
