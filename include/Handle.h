#pragma once

#include <cstdint>
#include <limits>
#include <utility>

#include "HandleFwd.h"
#include "ObjectRegistry.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Strongly typed UUID handle for framework objects.
 * @tparam T Resolved object type (e.g., BaseNode, IComponent).
 * @remarks Handles do not own objects; they resolve via ObjectRegistry.
 * @note Borrowed pointers must not be cached.
 * @note Pass handles by reference in hot/runtime APIs. `Borrowed()` refreshes
 * runtime-key fields on the handle instance; passing by value can cause repeated
 * UUID fallback lookups.
 */
template<typename T>
struct THandle
{
    /** @brief Sentinel runtime pool token representing "no runtime key". */
    static constexpr uint32_t kInvalidRuntimePoolToken = 0;
    /** @brief Sentinel runtime slot index representing "no runtime key". */
    static constexpr uint32_t kInvalidRuntimeIndex = std::numeric_limits<uint32_t>::max();

    /**
     * @brief Construct a null handle.
     */
    THandle() = default;

    /**
     * @brief Construct a handle from a UUID.
     * @param InId UUID of the target object.
     * @remarks Use NewUuid when creating new objects.
     */
    explicit THandle(Uuid InId)
        : Id(std::move(InId))
    {
    }

    /**
     * @brief Construct a handle from UUID plus runtime slot identity.
     * @param InId UUID of the target object.
     * @param InRuntimePoolToken Pool token.
     * @param InRuntimeIndex Pool slot index.
     * @param InRuntimeGeneration Pool slot generation.
     * @remarks
     * Runtime key fields are an optimization used by object pools to avoid UUID/hash
     * lookup in hot paths. UUID remains the canonical external identity.
     */
    THandle(Uuid InId,
        uint32_t InRuntimePoolToken,
        uint32_t InRuntimeIndex,
        uint32_t InRuntimeGeneration)
        : Id(std::move(InId))
        , RuntimePoolToken(InRuntimePoolToken)
        , RuntimeIndex(InRuntimeIndex)
        , RuntimeGeneration(InRuntimeGeneration)
    {
    }

    Uuid Id{}; /**< @brief UUID of the referenced object. */
    mutable uint32_t RuntimePoolToken = kInvalidRuntimePoolToken; /**< @brief Runtime pool token (optional fast-path identity). */
    mutable uint32_t RuntimeIndex = kInvalidRuntimeIndex; /**< @brief Runtime pool slot index (optional fast-path identity). */
    mutable uint32_t RuntimeGeneration = 0; /**< @brief Runtime pool slot generation for stale-handle rejection. */

    /**
     * @brief Check if the handle is null.
     * @return True when the UUID is nil.
     */
    bool IsNull() const noexcept
    {
        return Id.is_nil();
    }

    /**
     * @brief Boolean conversion for validity checks.
     * @return True when the handle is not null.
     * @note This does not guarantee the object is loaded.
     */
    explicit operator bool() const noexcept
    {
        return !IsNull();
    }

    /**
     * @brief Check whether runtime slot identity is present.
     * @return True when `RuntimeIndex` contains a valid slot id.
     */
    bool HasRuntimeKey() const noexcept
    {
        return RuntimePoolToken != kInvalidRuntimePoolToken && RuntimeIndex != kInvalidRuntimeIndex;
    }

    /**
     * @brief Equality comparison.
     * @param Other Another handle.
     * @return True when UUIDs match.
     */
    bool operator==(const THandle& Other) const noexcept
    {
        return Id == Other.Id;
    }

    /**
     * @brief Inequality comparison.
     * @param Other Another handle.
     * @return True when UUIDs differ.
     */
    bool operator!=(const THandle& Other) const noexcept
    {
        return !(*this == Other);
    }

    // Borrowed pointers are valid only for the current frame; do not cache or store them.
    /**
     * @brief Resolve to a borrowed pointer (const).
     * @return Pointer to the object, or nullptr if not loaded/registered.
     * @remarks
     * Fast path uses runtime pool token/index/generation only (no UUID hash lookup).
     * On success, runtime identity is refreshed on this handle instance.
     * Returns nullptr when runtime identity is unavailable.
     * @note The returned pointer must not be stored.
     */
    T* Borrowed() const
    {
        ObjectRegistry::RuntimeIdentity Identity{};
        T* Resolved = ObjectRegistry::Instance().ResolveFastOrFallback<T>(
            Id,
            RuntimePoolToken,
            RuntimeIndex,
            RuntimeGeneration,
            &Identity);
        if (Resolved)
        {
            RuntimePoolToken = Identity.RuntimePoolToken;
            RuntimeIndex = Identity.RuntimeIndex;
            RuntimeGeneration = Identity.RuntimeGeneration;
        }
        return Resolved;
    }

    // Borrowed pointers are valid only for the current frame; do not cache or store them.
    /**
     * @brief Resolve to a borrowed pointer (mutable).
     * @return Pointer to the object, or nullptr if not loaded/registered.
     * @remarks
     * Fast path uses runtime pool token/index/generation only (no UUID hash lookup).
     * On success, runtime identity is refreshed on this handle instance.
     * Returns nullptr when runtime identity is unavailable.
     * @note The returned pointer must not be stored.
     */
    T* Borrowed()
    {
        ObjectRegistry::RuntimeIdentity Identity{};
        T* Resolved = ObjectRegistry::Instance().ResolveFastOrFallback<T>(
            Id,
            RuntimePoolToken,
            RuntimeIndex,
            RuntimeGeneration,
            &Identity);
        if (Resolved)
        {
            RuntimePoolToken = Identity.RuntimePoolToken;
            RuntimeIndex = Identity.RuntimeIndex;
            RuntimeGeneration = Identity.RuntimeGeneration;
        }
        return Resolved;
    }

    /**
     * @brief Resolve by UUID using registry hash lookup (slow path).
     * @return Pointer to object or nullptr if missing/type mismatch.
     * @remarks
     * This path is intended for explicit persistence/replication bridging when runtime
     * slot identity is unavailable. Avoid in hot loops.
     */
    T* BorrowedSlowByUuid() const
    {
        return ObjectRegistry::Instance().Resolve<T>(Id);
    }

    /**
     * @brief Resolve by UUID using registry hash lookup (slow path).
     * @return Pointer to object or nullptr if missing/type mismatch.
     */
    T* BorrowedSlowByUuid()
    {
        return ObjectRegistry::Instance().Resolve<T>(Id);
    }

    /**
     * @brief Check whether the handle resolves to a live object.
     * @return True when the object is registered.
     * @remarks
     * Fast path uses runtime slot identity only. For UUID-only persistence handles,
     * use `IsValidSlowByUuid()`.
     */
    bool IsValid() const
    {
        return Borrowed() != nullptr;
    }

    /**
     * @brief Validate by UUID using registry hash lookup (slow path).
     * @return True when object resolves by UUID.
     */
    bool IsValidSlowByUuid() const
    {
        return ObjectRegistry::Instance().IsValid<T>(Id);
    }
};

/**
 * @brief Hash functor for THandle.
 * @remarks Uses UUID hash for stable bucket distribution.
 */
struct HandleHash
{
    /**
     * @brief Compute hash for a handle.
     * @tparam T Handle target type.
     * @param Handle Handle to hash.
     * @return Hash value.
     */
    template<typename T>
    std::size_t operator()(const THandle<T>& Handle) const noexcept
    {
        return UuidHash{}(Handle.Id);
    }
};

} // namespace SnAPI::GameFramework
