#pragma once

#include <exception>
#include <sstream>
#include <vector>

#include "AuthoredAssetCereal.h"
#include "Expected.h"
#include <cereal/archives/binary.hpp>

namespace SnAPI::GameFramework::Detail
{

template<typename TPayload>
TExpected<void> SerializeBinaryPayload(const TPayload& Payload, std::vector<uint8_t>& OutBytes)
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
TExpected<TPayload> DeserializeBinaryPayload(const uint8_t* Bytes, const size_t Size, const char* NullErrorMessage)
{
    if (!Bytes && Size > 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, NullErrorMessage));
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

template<typename TAsset>
Result SaveAuthoredAssetJson(const TAsset& Asset, std::ostream& Output)
{
    return SaveAuthoredAssetViaCerealJsonStream(Asset, Output);
}

} // namespace SnAPI::GameFramework::Detail
