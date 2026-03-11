#include "RenderAssetPayloads.h"

#include <exception>
#include <sstream>

#include "AuthoredAssetCereal.h"
#include <cereal/archives/binary.hpp>

namespace SnAPI::GameFramework
{
namespace
{

struct LegacyMaterialPayloadV1
{
    std::string ShaderModule{};
    std::string ShadingModel{};
};

template<class Archive>
void serialize(Archive& Ar, LegacyMaterialPayloadV1& Value)
{
    Ar(Value.ShaderModule, Value.ShadingModel);
}

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

Result MaterialPayload::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetViaCerealJsonStream(*this, Output);
}

Result MaterialInstancePayload::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetViaCerealJsonStream(*this, Output);
}

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
    if (auto Result = DeserializePayloadBinary<MaterialPayload>(Bytes, Size))
    {
        return Result;
    }

    auto Legacy = DeserializePayloadBinary<LegacyMaterialPayloadV1>(Bytes, Size);
    if (!Legacy)
    {
        return std::unexpected(Legacy.error());
    }

    MaterialPayload Upgraded{};
    Upgraded.ShaderModule = std::move(Legacy->ShaderModule);
    Upgraded.ShadingModel = std::move(Legacy->ShadingModel);
    return Upgraded;
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
