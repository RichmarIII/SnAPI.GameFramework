#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include "GameThreading.h"
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Expected.h"
#include "Handle.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Generation-safe UUID object pool that keeps object addresses stable while entries are alive.
 * @tparam T Base type stored in the pool.
 *
 * `TObjectPool` is the low-level backing store used for handle-addressable engine
 * objects whose lifetime is frame-oriented rather than instant-destroy:
 * - each live object has a stable UUID
 * - handles also carry a runtime slot key for fast resolution
 * - slots are generation-checked so stale handles do not alias reused entries
 * - destruction is normally deferred to `EndFrame()`
 *
 * Ownership and lifetime:
 * - `Create*()` stores pool-owned objects through `std::unique_ptr`.
 * - `CreateFromShared*()` stores an externally shared object through `std::shared_ptr`.
 * - Borrowed pointers remain valid until the object is scheduled for destroy and the
 *   pool reaches `EndFrame()`, or until `Clear()` destroys everything immediately.
 *
 * Threading:
 * - Not generally thread-safe.
 * - Internal `GameMutex` use validates thread affinity in debug builds but does not
 *   provide cross-thread mutual exclusion.
 * - Mutate and query the pool from one owner thread or provide external synchronization.
 *
 * Performance:
 * - Runtime-handle lookups are O(1) direct slot checks.
 * - UUID lookups are O(1) average hash-map probes.
 * - Slot reuse avoids vector growth where possible.
 *
 * @warning `DestroyLater()` only marks an object for destruction. Code that needs the
 *          object fully gone must call `EndFrame()`.
 */
template<typename T>
class TObjectPool : public std::enable_shared_from_this<TObjectPool<T>>
{
public:
    /**
     * @brief Handle type for objects in this pool.
     */
    using Handle = THandle<T>;

    /** @brief Construct an empty pool and acquire a unique runtime-pool token for fast handle resolution. */
    TObjectPool()
        : m_runtimePoolToken(ObjectRegistry::Instance().AcquireRuntimePoolToken())
    {
    }

    /**
     * @brief Destroy the pool and release its runtime-pool token.
     * @warning Releasing the token only invalidates future fast-path resolution. It does
     *          not flush UUID registrations on behalf of higher-level systems.
     */
    ~TObjectPool()
    {
        ObjectRegistry::Instance().ReleaseRuntimePoolToken(m_runtimePoolToken);
    }

    TObjectPool(const TObjectPool&) = delete;
    TObjectPool& operator=(const TObjectPool&) = delete;
    TObjectPool(TObjectPool&&) = delete;
    TObjectPool& operator=(TObjectPool&&) = delete;

    /**
     * @brief Construct and insert a new pool-owned object with a generated UUID.
     * @tparam U Derived type to construct.
     * @param args Constructor arguments for U.
     * @return A generation-safe handle to the new object, or an error when the object
     *         cannot be created.
     *
     * The returned handle usually contains both UUID and runtime-slot identity. If the
     * pool grows beyond the 32-bit runtime-index range, creation fails instead of
     * silently producing an unusable fast-path handle.
     */
    template<typename U = T, typename... Args>
    TExpected<Handle> Create(Args&&... args)
    {
        static_assert(std::is_base_of_v<T, U>, "Pool type mismatch");
        return CreateWithId<U>(NewUuid(), std::forward<Args>(args)...);
    }

    /**
     * @brief Construct and insert a new pool-owned object under an explicit UUID.
     * @tparam U Derived type to construct.
     * @param Id UUID to assign to the object.
     * @param args Constructor arguments for U.
     * @return Handle to the created object, or an error when insertion fails.
     *
     * @pre `Id` must be non-nil.
     * @post On success, `Borrowed(Result)` returns the constructed object until it is
     *       destroyed.
     *
     * @warning UUID collisions fail with `EErrorCode::AlreadyExists`; this function does
     *          not overwrite an existing entry.
     */
    template<typename U = T, typename... Args>
    TExpected<Handle> CreateWithId(const Uuid& Id, Args&&... args)
    {
        static_assert(std::is_base_of_v<T, U>, "Pool type mismatch");
        GameLockGuard Lock(m_mutex);
        if (Id.is_nil())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Nil uuid"));
        }
        if (m_index.find(Id) != m_index.end())
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Uuid already in pool"));
        }
        size_t Index = AllocateSlot();
        auto RuntimeIndex = RuntimeIndexFromSlot(Index);
        if (!RuntimeIndex)
        {
            m_freeList.push_back(Index);
            return std::unexpected(RuntimeIndex.error());
        }
        Entry& EntryRef = m_entries[Index];
        EntryRef.Generation = NextGeneration(EntryRef.Generation);
        EntryRef.Id = Id;
        EntryRef.m_uniqueObject = std::make_unique<U>(std::forward<Args>(args)...);
        EntryRef.m_sharedObject.reset();
        EntryRef.m_pendingDestroy = false;
        m_index.emplace(Id, Index);
        return Handle(Id, m_runtimePoolToken, RuntimeIndex.value(), EntryRef.Generation);
    }

    /**
     * @brief Insert an already-allocated shared object under a generated UUID.
     * @param Object Shared pointer to insert.
     * @return Handle to the inserted object or error.
     *
     * Ownership:
     * - The pool shares ownership with the caller by storing the same `shared_ptr`.
     * - The object address stays stable for the lifetime of that shared object.
     */
    TExpected<Handle> CreateFromShared(std::shared_ptr<T> Object)
    {
        if (!Object)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null object"));
        }
        return CreateFromSharedWithId(std::move(Object), NewUuid());
    }

    /**
     * @brief Insert an already-allocated shared object under an explicit UUID.
     * @param Object Shared pointer to insert.
     * @param Id UUID to assign to the object.
     * @return Handle to the inserted object or error.
     *
     * @pre `Object != nullptr`
     * @pre `Id` is non-nil and unused in this pool.
     */
    TExpected<Handle> CreateFromSharedWithId(std::shared_ptr<T> Object, const Uuid& Id)
    {
        GameLockGuard Lock(m_mutex);
        if (!Object)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null object"));
        }
        if (Id.is_nil())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Nil uuid"));
        }
        if (m_index.find(Id) != m_index.end())
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Uuid already in pool"));
        }
        size_t Index = AllocateSlot();
        auto RuntimeIndex = RuntimeIndexFromSlot(Index);
        if (!RuntimeIndex)
        {
            m_freeList.push_back(Index);
            return std::unexpected(RuntimeIndex.error());
        }
        Entry& EntryRef = m_entries[Index];
        EntryRef.Generation = NextGeneration(EntryRef.Generation);
        EntryRef.Id = Id;
        EntryRef.m_sharedObject = std::move(Object);
        EntryRef.m_uniqueObject.reset();
        EntryRef.m_pendingDestroy = false;
        m_index.emplace(Id, Index);
        return Handle(Id, m_runtimePoolToken, RuntimeIndex.value(), EntryRef.Generation);
    }

    /**
     * @brief Check if a handle resolves to a live object.
     * @param HandleRef Handle to validate.
     * @return True if object exists and is not pending destroy.
     */
    bool IsValid(const Handle& HandleRef) const
    {
        GameLockGuard Lock(m_mutex);
        size_t Index = 0;
        if (!ResolveIndexLocked(HandleRef, Index))
        {
            return false;
        }
        const Entry& EntryRef = m_entries[Index];
        if (!EntryRef.m_object || EntryRef.m_pendingDestroy)
        {
            return false;
        }
        return true;
    }

    /**
     * @brief Check if a UUID resolves to a live object.
     * @param Id UUID to validate.
     * @return True if object exists and is not pending destroy.
     */
    bool IsValid(const Uuid& Id) const
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return false;
        }
        const Entry& EntryRef = m_entries[It->second];
        if (!ObjectPtr(EntryRef) || EntryRef.m_pendingDestroy)
        {
            return false;
        }
        return true;
    }

    /**
     * @brief Rebuild a fast runtime handle from a UUID lookup.
     * @param Id UUID to resolve.
     * @return Runtime-key handle or error if missing/pending destroy.
     *
     * Use this when code persisted only the UUID identity and now needs a current,
     * generation-checked runtime handle again. This is intentionally the slow path and
     * performs a hash-map lookup.
     */
    TExpected<Handle> HandleByIdSlow(const Uuid& Id) const
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Handle not found"));
        }
        const Entry& EntryRef = m_entries[It->second];
        if (!ObjectPtr(EntryRef) || EntryRef.m_pendingDestroy)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Object missing"));
        }
        return MakeHandle(It->second, EntryRef);
    }

    /**
     * @brief Resolve a runtime handle to a borrowed mutable pointer.
     * @param HandleRef Handle to resolve.
     * @return Pointer to object or nullptr if not found/pending destroy.
     *
     * Borrowing does not extend lifetime. The returned pointer becomes invalid when the
     * entry is physically removed by `EndFrame()` or when `Clear()` is called.
     */
    T* Borrowed(const Handle& HandleRef)
    {
        GameLockGuard Lock(m_mutex);
        size_t Index = 0;
        if (!ResolveIndexLocked(HandleRef, Index))
        {
            return nullptr;
        }
        Entry& EntryRef = m_entries[Index];
        T* Object = ObjectPtr(EntryRef);
        if (!Object || EntryRef.m_pendingDestroy)
        {
            return nullptr;
        }
        return Object;
    }

    /**
     * @brief Resolve a UUID to a borrowed mutable pointer.
     * @param Id UUID to resolve.
     * @return Pointer to object or nullptr if not found/pending destroy.
     * @note This is slower than handle-based lookup because it hashes the UUID.
     */
    T* Borrowed(const Uuid& Id)
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return nullptr;
        }
        Entry& EntryRef = m_entries[It->second];
        T* Object = ObjectPtr(EntryRef);
        if (!Object || EntryRef.m_pendingDestroy)
        {
            return nullptr;
        }
        return Object;
    }

    /**
     * @brief Resolve a handle to a borrowed pointer (const).
     * @param HandleRef Handle to resolve.
     * @return Pointer to object or nullptr if not found/pending destroy.
     */
    const T* Borrowed(const Handle& HandleRef) const
    {
        GameLockGuard Lock(m_mutex);
        size_t Index = 0;
        if (!ResolveIndexLocked(HandleRef, Index))
        {
            return nullptr;
        }
        const Entry& EntryRef = m_entries[Index];
        T* Object = ObjectPtr(EntryRef);
        if (!Object || EntryRef.m_pendingDestroy)
        {
            return nullptr;
        }
        return Object;
    }

    /**
     * @brief Resolve a UUID to a borrowed pointer (const).
     * @param Id UUID to resolve.
     * @return Pointer to object or nullptr if not found/pending destroy.
     */
    const T* Borrowed(const Uuid& Id) const
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return nullptr;
        }
        const Entry& EntryRef = m_entries[It->second];
        T* Object = ObjectPtr(EntryRef);
        if (!Object || EntryRef.m_pendingDestroy)
        {
            return nullptr;
        }
        return Object;
    }

    /**
     * @brief Mark an object for deferred destruction by runtime handle.
     * @param HandleRef Handle to destroy.
     * @return Success or error if not found.
     *
     * Semantics:
     * - The object remains borrowable until `EndFrame()`.
     * - Repeated calls are idempotent.
     * - `IsValid()` returns `false` immediately once an entry is pending destroy.
     */
    TExpected<void> DestroyLater(const Handle& HandleRef)
    {
        GameLockGuard Lock(m_mutex);
        size_t Index = 0;
        if (!ResolveIndexLocked(HandleRef, Index))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Handle not found"));
        }

        Entry& EntryRef = m_entries[Index];
        if (!ObjectPtr(EntryRef))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Object missing"));
        }
        if (!EntryRef.m_pendingDestroy)
        {
            EntryRef.m_pendingDestroy = true;
            m_pendingDestroy.push_back(Index);
        }
        return Ok();
    }

    /**
     * @brief Mark an object for deferred destruction by UUID.
     * @param Id UUID to destroy.
     * @return Success or error if not found.
     * @remarks Equivalent to the handle overload but uses the UUID map.
     */
    TExpected<void> DestroyLater(const Uuid& Id)
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Handle not found"));
        }
        Entry& EntryRef = m_entries[It->second];
        if (!ObjectPtr(EntryRef))
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Object missing"));
        }
        if (!EntryRef.m_pendingDestroy)
        {
            EntryRef.m_pendingDestroy = true;
            m_pendingDestroy.push_back(It->second);
        }
        return Ok();
    }

    /**
     * @brief Finalize all deferred destroys and recycle the freed slots.
     *
     * `EndFrame()` is the point where pending entries actually disappear:
     * - UUID lookup entries are removed
     * - ownership pointers are released
     * - slot ids are cleared
     * - slot indices return to the free list
     *
     * After this call, all borrowed pointers to destroyed objects are invalid.
     */
    void EndFrame()
    {
        GameLockGuard Lock(m_mutex);
        for (size_t Index : m_pendingDestroy)
        {
            if (Index >= m_entries.size())
            {
                continue;
            }
            Entry& EntryRef = m_entries[Index];
            m_index.erase(EntryRef.Id);
            EntryRef.m_uniqueObject.reset();
            EntryRef.m_sharedObject.reset();
            EntryRef.m_pendingDestroy = false;
            EntryRef.Id = Uuid{};
            m_freeList.push_back(Index);
        }
        m_pendingDestroy.clear();
    }

    /**
     * @brief Destroy all entries immediately and reset the pool to empty.
     * @remarks This bypasses deferred-destroy semantics and invalidates every live handle
     *          and borrowed pointer immediately.
     */
    void Clear()
    {
        GameLockGuard Lock(m_mutex);
        m_entries.clear();
        m_index.clear();
        m_freeList.clear();
        m_pendingDestroy.clear();
    }

    /**
     * @brief Check if a handle is pending destruction.
     * @param HandleRef Handle to check.
     * @return True if the object is marked for deletion.
     */
    bool IsPendingDestroy(const Handle& HandleRef) const
    {
        GameLockGuard Lock(m_mutex);
        size_t Index = 0;
        if (!ResolveIndexLocked(HandleRef, Index))
        {
            return false;
        }
        return m_entries[Index].m_pendingDestroy;
    }

    /**
     * @brief Check if a UUID is pending destruction.
     * @param Id UUID to check.
     * @return True if the object is marked for deletion.
     */
    bool IsPendingDestroy(const Uuid& Id) const
    {
        GameLockGuard Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return false;
        }
        return m_entries[It->second].m_pendingDestroy;
    }

    /**
     * @brief Visit all currently live, non-pending objects in slot order.
     * @tparam Fn Callable type.
     * @param Func Callback invoked with (Handle, Object).
     *
     * @warning The callback runs while the pool is inside its affinity guard. Do not
     *          structurally mutate the same pool from the callback.
     */
    template<typename Fn>
    void ForEach(const Fn& Func) const
    {
        GameLockGuard Lock(m_mutex);
        for (size_t Index = 0; Index < m_entries.size(); ++Index)
        {
            const Entry& EntryRef = m_entries[Index];
            T* Object = ObjectPtr(EntryRef);
            if (!Object || EntryRef.m_pendingDestroy)
            {
                continue;
            }
            Func(MakeHandle(Index, EntryRef), *Object);
        }
    }

    /**
     * @brief Visit all objects including entries already marked for destruction.
     * @tparam Fn Callable type.
     * @param Func Callback invoked with (Handle, Object).
     * @remarks Useful for teardown paths that still need access to pending-destroy entries.
     */
    template<typename Fn>
    void ForEachAll(const Fn& Func) const
    {
        GameLockGuard Lock(m_mutex);
        for (size_t Index = 0; Index < m_entries.size(); ++Index)
        {
            const Entry& EntryRef = m_entries[Index];
            T* Object = ObjectPtr(EntryRef);
            if (!Object)
            {
                continue;
            }
            Func(MakeHandle(Index, EntryRef), *Object);
        }
    }

    /**
     * @brief Mutable overload of `ForEach` for live, non-pending objects.
     * @tparam Fn Callable type.
     * @param Func Callback invoked with (Handle, Object).
     * @remarks Pending-destroy entries are skipped.
     */
    template<typename Fn>
    void ForEach(const Fn& Func)
    {
        GameLockGuard Lock(m_mutex);
        for (size_t Index = 0; Index < m_entries.size(); ++Index)
        {
            Entry& EntryRef = m_entries[Index];
            T* Object = ObjectPtr(EntryRef);
            if (!Object || EntryRef.m_pendingDestroy)
            {
                continue;
            }
            Func(MakeHandle(Index, EntryRef), *Object);
        }
    }

    /**
     * @brief Mutable overload of `ForEachAll`.
     * @tparam Fn Callable type.
     * @param Func Callback invoked with (Handle, Object).
     * @remarks Includes objects marked for deletion but not yet flushed by `EndFrame()`.
     */
    template<typename Fn>
    void ForEachAll(const Fn& Func)
    {
        GameLockGuard Lock(m_mutex);
        for (size_t Index = 0; Index < m_entries.size(); ++Index)
        {
            Entry& EntryRef = m_entries[Index];
            T* Object = ObjectPtr(EntryRef);
            if (!Object)
            {
                continue;
            }
            Func(MakeHandle(Index, EntryRef), *Object);
        }
    }

private:
    /**
     * @brief Internal slot payload for one object entry.
     * @remarks Exactly one of `m_uniqueObject` or `m_sharedObject` is expected to hold
     *          the live instance for a populated slot.
     */
    struct Entry
    {
        Uuid Id{}; /**< @brief UUID key for this entry. */
        uint32_t Generation = 0; /**< @brief Slot generation used for stale-handle rejection. */
        std::unique_ptr<T> m_uniqueObject{}; /**< @brief Standard ownership path for pool-created objects. */
        std::shared_ptr<T> m_sharedObject{}; /**< @brief Shared ownership path for externally-owned inserts. */
        bool m_pendingDestroy = false; /**< @brief True when scheduled for deletion. */
    };

    static T* ObjectPtr(Entry& EntryRef)
    {
        if (EntryRef.m_uniqueObject)
        {
            return EntryRef.m_uniqueObject.get();
        }
        return EntryRef.m_sharedObject.get();
    }

    static T* ObjectPtr(const Entry& EntryRef)
    {
        if (EntryRef.m_uniqueObject)
        {
            return EntryRef.m_uniqueObject.get();
        }
        return EntryRef.m_sharedObject.get();
    }

    /**
     * @brief Return a runtime slot index usable by `THandle`.
     * @param Index Slot index in `m_entries`.
     * @return Runtime index value or error when pool exceeds handle index range.
     */
    TExpected<uint32_t> RuntimeIndexFromSlot(size_t Index) const
    {
        if (Index > static_cast<size_t>(std::numeric_limits<uint32_t>::max()) - 1)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Object pool index exceeded runtime handle range"));
        }
        return static_cast<uint32_t>(Index);
    }

    /**
     * @brief Increment slot generation while reserving zero as invalid.
     * @param Previous Previous generation value.
     * @return Next non-zero generation.
     */
    static uint32_t NextGeneration(uint32_t Previous)
    {
        uint32_t Next = Previous + 1;
        if (Next == 0)
        {
            Next = 1;
        }
        return Next;
    }

    /**
     * @brief Build a handle for a live entry.
     * @param Index Slot index.
     * @param EntryRef Entry payload.
     * @return Handle containing UUID plus runtime key when index fits.
     */
    Handle MakeHandle(size_t Index, const Entry& EntryRef) const
    {
        if (Index > static_cast<size_t>(std::numeric_limits<uint32_t>::max()) - 1)
        {
            return Handle(EntryRef.Id);
        }
        return Handle(EntryRef.Id, m_runtimePoolToken, static_cast<uint32_t>(Index), EntryRef.Generation);
    }

    /**
     * @brief Resolve a handle to an entry index using only the runtime-key fast path.
     * @param HandleRef Handle to resolve.
     * @param OutIndex Resolved index on success.
     * @return True when the handle matches the current slot generation and UUID.
     *
     * This function deliberately does not fall back to UUID lookup. Callers that only
     * have a UUID must first use `HandleByIdSlow()`.
     */
    bool ResolveIndexLocked(const Handle& HandleRef, size_t& OutIndex) const
    {
        if (HandleRef.HasRuntimeKey() && HandleRef.RuntimePoolToken == m_runtimePoolToken)
        {
            const size_t Candidate = static_cast<size_t>(HandleRef.RuntimeIndex);
            if (Candidate < m_entries.size())
            {
                const Entry& EntryRef = m_entries[Candidate];
                if (EntryRef.Generation == HandleRef.RuntimeGeneration && EntryRef.Id == HandleRef.Id)
                {
                    OutIndex = Candidate;
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Allocate a storage slot, reusing free slots if possible.
     * @return Index into m_entries.
     * @remarks Free list is used to avoid vector growth where possible.
     */
    size_t AllocateSlot()
    {
        if (!m_freeList.empty())
        {
            size_t Index = m_freeList.back();
            m_freeList.pop_back();
            return Index;
        }
        size_t Index = m_entries.size();
        m_entries.push_back({});
        return Index;
    }

    mutable GameMutex m_mutex{}; /**< @brief Protects pool state. */
    std::vector<Entry> m_entries{}; /**< @brief Dense storage for entries. */
    std::unordered_map<Uuid, size_t, UuidHash> m_index{}; /**< @brief UUID -> entry index. */
    std::vector<size_t> m_freeList{}; /**< @brief Reusable entry indices. */
    std::vector<size_t> m_pendingDestroy{}; /**< @brief Indices scheduled for deletion. */
    uint32_t m_runtimePoolToken = ObjectRegistry::kInvalidRuntimePoolToken; /**< @brief Runtime token used for direct handle resolution. */
};

} // namespace SnAPI::GameFramework
