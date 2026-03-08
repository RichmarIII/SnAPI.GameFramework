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
 * @ingroup SnAPI_GameFramework
 * @brief Strongly typed, non-owning identity token for framework objects.
 *
 * `THandle<T>` is the public identity boundary for world-owned objects such as nodes and
 * components. A handle stores the stable UUID that survives serialization, replication, and
 * deferred-destroy windows, plus optional runtime slot metadata used as a fast-path for hot
 * resolution through `ObjectRegistry`.
 *
 * Why this exists:
 * - raw pointers are fast but unsafe to persist across frames, loads, or destroy queues
 * - UUIDs are stable but expensive to hash/resolve repeatedly in hot paths
 * - `THandle` combines both: a stable external identity plus an internal cached runtime key
 *
 * Core semantics:
 * - Handles never own the target object.
 * - Equality compares stable UUID identity, not pointer identity.
 * - A non-null handle may still fail to resolve if the object has been destroyed or is not loaded.
 * - Successful `Borrowed()` resolution may refresh the cached runtime key on the handle instance.
 *
 * Ownership and lifetime:
 * - The caller owns only the handle value, never the resolved object.
 * - Borrowed pointers returned from `Borrowed()` are transient views and must not be cached.
 * - The handle may outlive the target object; resolution then returns `nullptr`.
 *
 * Threading:
 * - Copying and comparing handles is thread-safe.
 * - Calling `Borrowed()` on the same handle instance from multiple threads is not thread-safe,
 *   because the runtime cache fields are updated lazily.
 * - External synchronization is required if one handle instance is shared across threads.
 *
 * Performance:
 * - Fast path is O(1) when runtime key fields are valid.
 * - Slow UUID fallback requires registry lookup and is more expensive; avoid it in hot loops.
 *
 * @tparam T Resolved object type (for example `BaseNode` or `BaseComponent`).
 * @see NodeHandle
 * @see ComponentHandle
 * @see ObjectRegistry
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

    Uuid Id{}; /**< @brief Stable UUID of the referenced object; this is the canonical identity used for equality and persistence. */
    mutable uint32_t RuntimePoolToken = kInvalidRuntimePoolToken; /**< @brief Optional cached pool token used to bypass UUID lookup during hot resolution. */
    mutable uint32_t RuntimeIndex = kInvalidRuntimeIndex; /**< @brief Optional cached slot index paired with `RuntimePoolToken` for fast lookup. */
    mutable uint32_t RuntimeGeneration = 0; /**< @brief Cached generation used to reject stale slot reuse after object destruction. */

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
     * @brief Resolve to a non-owning pointer using cached runtime identity when possible.
     * @return Non-owning pointer to the object, or `nullptr` if the object is not currently registered.
     * @remarks
     * Fast path uses runtime pool token/index/generation only (no UUID hash lookup).
     * On success, runtime identity is refreshed on this handle instance.
     * Returns `nullptr` when the object cannot be resolved.
     * @note The returned pointer must not be stored across frames or destroy boundaries.
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
     * @brief Resolve to a non-owning pointer using cached runtime identity when possible.
     * @return Non-owning pointer to the object, or `nullptr` if the object is not currently registered.
     * @remarks
     * Fast path uses runtime pool token/index/generation only (no UUID hash lookup).
     * On success, runtime identity is refreshed on this handle instance.
     * Returns `nullptr` when the object cannot be resolved.
     * @note The returned pointer must not be stored across frames or destroy boundaries.
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
     * @brief Check whether the handle resolves to a live object through the fast path.
     * @return `true` when the object is currently registered and reachable through `Borrowed()`.
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
 * @ingroup SnAPI_GameFramework
 * @brief Hash functor for `THandle`.
 * @remarks Uses UUID hash so associative containers follow stable object identity.
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
