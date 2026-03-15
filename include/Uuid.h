#pragma once

#include <algorithm>
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

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical UUID type used throughout the framework.
 *
 * Backed by the `stduuid` library.
 */
using Uuid = uuids::uuid;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Distinct wrapper used for reflected type ids.
 *
 * `TypeId` stays UUID-backed, but it is intentionally a separate reflected type so tooling can
 * distinguish generic UUID values from "pick one reflected type" values.
 */
struct TypeId
{
    Uuid Value{};

    constexpr TypeId() = default;
    constexpr TypeId(const Uuid& InValue)
        : Value(InValue)
    {
    }
    constexpr TypeId(Uuid&& InValue)
        : Value(std::move(InValue))
    {
    }

    [[nodiscard]] bool is_nil() const noexcept
    {
        return Value.is_nil();
    }

    [[nodiscard]] auto as_bytes() const noexcept
    {
        return Value.as_bytes();
    }

    [[nodiscard]] std::string ToString() const
    {
        return uuids::to_string(Value);
    }

    constexpr operator const Uuid&() const noexcept
    {
        return Value;
    }

    constexpr operator Uuid&() noexcept
    {
        return Value;
    }

    friend bool operator==(const TypeId&, const TypeId&) = default;
    friend bool operator<(const TypeId& Left, const TypeId& Right) noexcept
    {
        const auto LeftBytes = Left.Value.as_bytes();
        const auto RightBytes = Right.Value.as_bytes();
        return std::lexicographical_compare(
            LeftBytes.begin(),
            LeftBytes.end(),
            RightBytes.begin(),
            RightBytes.end(),
            [] (const auto LeftByte, const auto RightByte) {
                return std::to_integer<std::uint8_t>(LeftByte) < std::to_integer<std::uint8_t>(RightByte);
            });
    }
    friend bool operator==(const TypeId& Left, const Uuid& Right) noexcept
    {
        return Left.Value == Right;
    }
    friend bool operator==(const Uuid& Left, const TypeId& Right) noexcept
    {
        return Left == Right.Value;
    }
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Split UUID representation for hashing, scripting ABI transport, or interop.
 *
 * `High` and `Low` store the UUID bytes in big-endian order.
 */
struct UuidParts
{
    uint64_t High = 0; /**< @brief High 64 bits. */
    uint64_t Low = 0;  /**< @brief Low 64 bits. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Namespace UUID used for deterministic reflected type-id generation.
 * @return Stable namespace UUID.
 *
 * The value returned here is part of the serialization and reflection ABI. Changing it would change
 * every derived `TypeId`.
 */
inline const Uuid& TypeIdNamespace()
{
    static const Uuid Namespace = [] {
        auto Parsed = uuids::uuid::from_string("8b76c145-755f-4bda-b3a7-593eb5c9129d");
        return Parsed.value_or(Uuid{});
    }();
    return Namespace;
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Generate a deterministic reflected `TypeId` from a stable name.
 * @param Name Fully qualified reflected type name.
 * @return UUIDv5 derived from `Name` within `TypeIdNamespace()`.
 *
 * The name string is part of the serialization contract and must remain stable once data is persisted.
 */
inline TypeId TypeIdFromName(std::string_view Name)
{
    uuids::uuid_name_generator Generator(TypeIdNamespace());
    return TypeId{Generator(std::string(Name))};
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Generate a new random UUID.
 * @return Newly generated UUID.
 *
 * Uses a thread-local random generator so repeated calls avoid global locking.
 */
inline Uuid NewUuid()
{
    static thread_local std::random_device Device;
    static thread_local std::mt19937 Engine(Device());
    static thread_local uuids::uuid_random_generator Generator(Engine);
    return Generator();
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Convert a UUID to its canonical lowercase string form.
 */
inline std::string ToString(const Uuid& Id)
{
    return uuids::to_string(Id);
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Convert a reflected type id to its canonical lowercase string form.
 */
inline std::string ToString(const TypeId& Id)
{
    return uuids::to_string(Id.Value);
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Convert a UUID to its split high/low representation.
 * @param Id UUID to split.
 * @return `UuidParts` containing the high and low 64-bit values.
 */
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

/**
 * @ingroup SnAPI_GameFramework
 * @brief Convert a reflected type id to its split high/low representation.
 */
inline UuidParts ToParts(const TypeId& Id)
{
    return ToParts(Id.Value);
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reconstruct a UUID from `UuidParts`.
 * @param Parts Split representation.
 * @return Reconstructed UUID.
 *
 * This is the inverse of `ToParts()`.
 */
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

/**
 * @ingroup SnAPI_GameFramework
 * @brief Hash functor for `Uuid`.
 *
 * Enables `Uuid` and `TypeId` use in unordered containers.
 */
struct UuidHash
{
    /**
     * @brief Compute a hash value for a UUID.
     * @param Id UUID to hash.
     * @return Hash value.
     * @note Combines High/Low with a 64-bit mix.
     */
    std::size_t operator()(const Uuid& Id) const noexcept
    {
        const auto Parts = ToParts(Id);
        return static_cast<std::size_t>(Parts.High ^ (Parts.Low + 0x9e3779b97f4a7c15ULL + (Parts.High << 6) + (Parts.High >> 2)));
    }

    std::size_t operator()(const TypeId& Id) const noexcept
    {
        return (*this)(Id.Value);
    }
};

} // namespace SnAPI::GameFramework
