#pragma once

#include <utility>
#include <vector>

namespace SnAPI::GameFramework::Detail
{

template<typename TPayload, auto SerializeFn>
void SerializePayloadObject(const void* Object, std::vector<uint8_t>& OutBytes)
{
    const auto* Payload = static_cast<const TPayload*>(Object);
    if (!Payload)
    {
        OutBytes.clear();
        return;
    }

    auto Result = SerializeFn(*Payload, OutBytes);
    if (!Result)
    {
        OutBytes.clear();
    }
}

template<typename TPayload, auto DeserializeFn>
bool DeserializePayloadObject(void* Object, const uint8_t* Bytes, const std::size_t Size)
{
    auto* Payload = static_cast<TPayload*>(Object);
    if (!Payload)
    {
        return false;
    }

    auto Result = DeserializeFn(Bytes, Size);
    if (!Result)
    {
        return false;
    }

    *Payload = std::move(Result.value());
    return true;
}

} // namespace SnAPI::GameFramework::Detail
