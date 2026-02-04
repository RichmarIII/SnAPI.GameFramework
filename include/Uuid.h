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

/**
 * @brief UUID type used throughout the framework.
 * @remarks Provided by the stduuid library.
 */
using Uuid = uuids::uuid;
/**
 * @brief Strong alias for TypeId values.
 * @remarks TypeId is a UUID derived from a stable type name.
 */
using TypeId = Uuid;

/**
 * @brief Split UUID representation for hashing or ABI transport.
 * @remarks High and Low are big-endian parts of the UUID bytes.
 */
struct UuidParts
{
    uint64_t High = 0; /**< @brief High 64 bits. */
    uint64_t Low = 0;  /**< @brief Low 64 bits. */
};

/**
 * @brief Namespace UUID for deterministic type id generation.
 * @return Stable namespace UUID.
 * @remarks Used by TypeIdFromName for UUIDv5 generation.
 * @note Keep this stable across versions for serialized compatibility.
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
 * @brief Generate a stable TypeId from a fully qualified name.
 * @param Name Fully qualified type name.
 * @return UUIDv5 derived from the name and TypeIdNamespace.
 * @remarks The name must remain stable to preserve serialization.
 */
inline TypeId TypeIdFromName(std::string_view Name)
{
    uuids::uuid_name_generator Generator(TypeIdNamespace());
    return Generator(std::string(Name));
}

/**
 * @brief Generate a new random UUID (UUIDv4).
 * @return Newly generated UUID.
 * @remarks Uses a thread-local generator for performance.
 */
inline Uuid NewUuid()
{
    static thread_local std::random_device Device;
    static thread_local std::mt19937 Engine(Device());
    static thread_local uuids::uuid_random_generator Generator(Engine);
    return Generator();
}

/**
 * @brief Convert a UUID to its canonical string form.
 * @param Id UUID to convert.
 * @return Lowercase UUID string.
 */
inline std::string ToString(const Uuid& Id)
{
    return uuids::to_string(Id);
}

/**
 * @brief Convert a UUID to a split High/Low representation.
 * @param Id UUID to split.
 * @return UuidParts containing the high/low 64-bit values.
 * @remarks Useful for hashing or C ABI bindings.
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
 * @brief Reconstruct a UUID from split High/Low parts.
 * @param Parts High/Low representation.
 * @return Reconstructed UUID.
 * @remarks Inverse of ToParts.
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
 * @brief Hash functor for UUID.
 * @remarks Enables UUID use in unordered_map/set.
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
};

} // namespace SnAPI::GameFramework
