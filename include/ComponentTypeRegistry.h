#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "GameThreading.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Global allocator for compact component-type bit indices.
 *
 * `ComponentTypeRegistry` turns arbitrary reflected component `TypeId`s into dense bit
 * positions used by masks and query acceleration structures. The assigned index for a
 * type remains stable for the lifetime of the process.
 *
 * Threading:
 * - Not generally thread-safe.
 * - Internal `GameMutex` use validates affinity only.
 */
class ComponentTypeRegistry
{
public:
    /**
     * @brief Get the existing bit index for a component type, or assign a new one.
     * @param Id Component type id.
     * @return Bit index for the type.
     * @remarks `Version()` is incremented only when a previously unseen type is added.
     */
    static uint32_t TypeIndex(const TypeId& Id)
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_typeToIndex.find(Id);
        if (It != m_typeToIndex.end())
        {
            return It->second;
        }
        uint32_t Index = static_cast<uint32_t>(m_typeToIndex.size());
        m_typeToIndex.emplace(Id, Index);
        ++m_version;
        return Index;
    }

    /**
     * @brief Look up an existing bit index for a component type without mutating the registry.
     * @param Id Component type id.
     * @return Existing bit index, or `std::nullopt` when the type has not been seen yet.
     */
    static std::optional<uint32_t> TryGetTypeIndex(const TypeId& Id)
    {
        GameLockGuard Lock(m_mutex);
        if (const auto It = m_typeToIndex.find(Id); It != m_typeToIndex.end())
        {
            return It->second;
        }
        return std::nullopt;
    }

    /**
     * @brief Get the current mutation version of the registry.
     * @return Version counter.
     * @remarks Useful for invalidating cached masks sized from `WordCount()`.
     */
    static uint32_t Version()
    {
        GameLockGuard Lock(m_mutex);
        return m_version;
    }

    /** @brief Get the number of 64-bit words needed to represent the current type set. */
    static std::size_t WordCount()
    {
        GameLockGuard Lock(m_mutex);
        const std::size_t BitCount = m_typeToIndex.size();
        return (BitCount + 63u) / 64u;
    }

private:
    static inline GameMutex m_mutex{};
    static inline std::unordered_map<TypeId, uint32_t, UuidHash> m_typeToIndex{};
    static inline uint32_t m_version = 0;
};

} // namespace SnAPI::GameFramework
