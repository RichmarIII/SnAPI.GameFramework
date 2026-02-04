#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>

#include <uuid.h>

namespace SnAPI::GameFramework
{

using Uuid = uuids::uuid;
using TypeId = Uuid;

struct UuidParts
{
    uint64_t High = 0;
    uint64_t Low = 0;
};

inline const Uuid& TypeIdNamespace()
{
    static const Uuid Namespace = [] {
        auto Parsed = uuids::uuid::from_string("8b76c145-755f-4bda-b3a7-593eb5c9129d");
        return Parsed.value_or(Uuid{});
    }();
    return Namespace;
}

inline TypeId TypeIdFromName(std::string_view Name)
{
    uuids::uuid_name_generator Generator(TypeIdNamespace());
    return Generator(std::string(Name));
}

inline Uuid NewUuid()
{
    static thread_local std::random_device Device;
    static thread_local std::mt19937 Engine(Device());
    static thread_local uuids::uuid_random_generator Generator(Engine);
    return Generator();
}

inline std::string ToString(const Uuid& Id)
{
    return uuids::to_string(Id);
}

inline UuidParts ToParts(const Uuid& Id)
{
    const auto& Bytes = Id.as_bytes();
    uint64_t High = 0;
    uint64_t Low = 0;
    for (int i = 0; i < 8; ++i)
    {
        High = (High << 8) | static_cast<uint64_t>(std::to_integer<uint8_t>(Bytes[i]));
    }
    for (int i = 8; i < 16; ++i)
    {
        Low = (Low << 8) | static_cast<uint64_t>(std::to_integer<uint8_t>(Bytes[i]));
    }
    return {High, Low};
}

inline Uuid FromParts(UuidParts Parts)
{
    std::array<uint8_t, 16> Bytes{};
    uint64_t High = Parts.High;
    uint64_t Low = Parts.Low;
    for (int i = 7; i >= 0; --i)
    {
        Bytes[i] = static_cast<uint8_t>(High & 0xFFu);
        High >>= 8;
    }
    for (int i = 15; i >= 8; --i)
    {
        Bytes[i] = static_cast<uint8_t>(Low & 0xFFu);
        Low >>= 8;
    }
    return Uuid(Bytes);
}

struct UuidHash
{
    std::size_t operator()(const Uuid& Id) const noexcept
    {
        const auto Parts = ToParts(Id);
        return static_cast<std::size_t>(Parts.High ^ (Parts.Low + 0x9e3779b97f4a7c15ULL + (Parts.High << 6) + (Parts.High >> 2)));
    }
};

} // namespace SnAPI::GameFramework
