#pragma once

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
 */
template<typename T>
struct THandle
{
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

    Uuid Id{}; /**< @brief UUID of the referenced object. */

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
     * @remarks This lookup is O(1) via ObjectRegistry.
     * @note The returned pointer must not be stored.
     */
    T* Borrowed() const
    {
        return ObjectRegistry::Instance().Resolve<T>(Id);
    }

    // Borrowed pointers are valid only for the current frame; do not cache or store them.
    /**
     * @brief Resolve to a borrowed pointer (mutable).
     * @return Pointer to the object, or nullptr if not loaded/registered.
     * @remarks This lookup is O(1) via ObjectRegistry.
     * @note The returned pointer must not be stored.
     */
    T* Borrowed()
    {
        return ObjectRegistry::Instance().Resolve<T>(Id);
    }

    /**
     * @brief Check whether the handle resolves to a live object.
     * @return True when the object is registered.
     * @remarks Useful for cross-asset references that may load later.
     */
    bool IsValid() const
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
