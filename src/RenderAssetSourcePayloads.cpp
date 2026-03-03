#include "RenderAssetSourcePayloads.h"

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
void serialize(Archive& Ar, MeshStreamSourcePayload& Value)
{
    Ar(Value.Semantic, Value.SubIndex, Value.Uri, Value.Bytes, Value.ElementCount, Value.StrideBytes, Value.Compress);
}

template<class Archive>
void serialize(Archive& Ar, MeshImportSettingsPayload& Value)
{
    Ar(Value.GenerateNormals,
       Value.GenerateTangents,
       Value.FlipUVs,
       Value.OptimizeMeshes,
       Value.ForceSkeletal,
       Value.ForceStatic,
       Value.MaxBonesPerVertex);
}

template<class Archive>
void serialize(Archive& Ar, StaticMeshSourcePayload& Value)
{
    Ar(Value.Mesh, Value.Streams, Value.ImportSettings);
}

template<class Archive>
void serialize(Archive& Ar, SkeletalMeshSourcePayload& Value)
{
    Ar(Value.BaseMesh,
       Value.Bones,
       Value.Skeleton,
       Value.Animations,
       Value.SkeletonAnimationUri,
       Value.SkeletonAnimationBytes,
       Value.SkeletonAnimationSubIndex,
       Value.CompressSkeletonAnimation);
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
        return std::unexpected(MakeError(EErrorCode::InternalError, "Unknown exception while serializing source payload"));
    }
}

template<typename TPayload>
TExpected<TPayload> DeserializePayloadBinary(const uint8_t* Bytes, const size_t Size)
{
    if (!Bytes && Size > 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null source payload bytes"));
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
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown exception while deserializing source payload"));
    }
}

} // namespace

TExpected<void> SerializeStaticMeshSourcePayload(const StaticMeshSourcePayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return SerializePayloadBinary(Payload, OutBytes);
}

TExpected<StaticMeshSourcePayload> DeserializeStaticMeshSourcePayload(const uint8_t* Bytes, const size_t Size)
{
    return DeserializePayloadBinary<StaticMeshSourcePayload>(Bytes, Size);
}

TExpected<void> SerializeSkeletalMeshSourcePayload(const SkeletalMeshSourcePayload& Payload, std::vector<uint8_t>& OutBytes)
{
    return SerializePayloadBinary(Payload, OutBytes);
}

TExpected<SkeletalMeshSourcePayload> DeserializeSkeletalMeshSourcePayload(const uint8_t* Bytes, const size_t Size)
{
    return DeserializePayloadBinary<SkeletalMeshSourcePayload>(Bytes, Size);
}

} // namespace SnAPI::GameFramework
