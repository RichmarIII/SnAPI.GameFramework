#pragma once

#include <memory>
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
 * @brief Thread-safe object pool keyed by UUID handles.
 * @tparam T Base type stored in the pool.
 * @remarks
 * Slot-based pool with UUID index and free-list reuse.
 * Objects are stored as `shared_ptr` to preserve pointer stability while alive.
 * @note Destruction is deferred until EndFrame to keep handles valid within a frame.
 */
template<typename T>
class TObjectPool : public std::enable_shared_from_this<TObjectPool<T>>
{
public:
    /**
     * @brief Handle type for objects in this pool.
     */
    using Handle = THandle<T>;

    /**
     * @brief Construct an empty pool.
     */
    TObjectPool() = default;

    /**
     * @brief Create a new object with a generated UUID.
     * @tparam U Derived type to construct.
     * @param args Constructor arguments for U.
     * @return Handle to the created object or error.
     * @remarks Uses NewUuid for the handle identity.
     */
    template<typename U = T, typename... Args>
    TExpected<Handle> Create(Args&&... args)
    {
        static_assert(std::is_base_of_v<T, U>, "Pool type mismatch");
        return CreateWithId<U>(NewUuid(), std::forward<Args>(args)...);
    }

    /**
     * @brief Create a new object with an explicit UUID.
     * @tparam U Derived type to construct.
     * @param Id UUID to assign to the object.
     * @param args Constructor arguments for U.
     * @return Handle to the created object or error.
     * @remarks Fails if Id is nil or already present in the pool.
     */
    template<typename U = T, typename... Args>
    TExpected<Handle> CreateWithId(const Uuid& Id, Args&&... args)
    {
        static_assert(std::is_base_of_v<T, U>, "Pool type mismatch");
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (Id.is_nil())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Nil uuid"));
        }
        if (m_index.find(Id) != m_index.end())
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Uuid already in pool"));
        }
        size_t Index = AllocateSlot();
        Entry& EntryRef = m_entries[Index];
        EntryRef.Id = Id;
        EntryRef.m_object = std::make_shared<U>(std::forward<Args>(args)...);
        EntryRef.m_pendingDestroy = false;
        m_index.emplace(Id, Index);
        return Handle(Id);
    }

    /**
     * @brief Insert an existing shared object with a generated UUID.
     * @param Object Shared pointer to insert.
     * @return Handle to the inserted object or error.
     * @remarks Fails if Object is null.
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
     * @brief Insert an existing shared object with an explicit UUID.
     * @param Object Shared pointer to insert.
     * @param Id UUID to assign to the object.
     * @return Handle to the inserted object or error.
     * @remarks Fails if Id is nil or already present in the pool.
     */
    TExpected<Handle> CreateFromSharedWithId(std::shared_ptr<T> Object, const Uuid& Id)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
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
        Entry& EntryRef = m_entries[Index];
        EntryRef.Id = Id;
        EntryRef.m_object = std::move(Object);
        EntryRef.m_pendingDestroy = false;
        m_index.emplace(Id, Index);
        return Handle(Id);
    }

    /**
     * @brief Check if a handle resolves to a live object.
     * @param HandleRef Handle to validate.
     * @return True if object exists and is not pending destroy.
     */
    bool IsValid(const Handle& HandleRef) const
    {
        return IsValid(HandleRef.Id);
    }

    /**
     * @brief Check if a UUID resolves to a live object.
     * @param Id UUID to validate.
     * @return True if object exists and is not pending destroy.
     */
    bool IsValid(const Uuid& Id) const
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return false;
        }
        const Entry& EntryRef = m_entries[It->second];
        if (!EntryRef.m_object || EntryRef.m_pendingDestroy)
        {
            return false;
        }
        return true;
    }

    /**
     * @brief Resolve a handle to a borrowed pointer.
     * @param HandleRef Handle to resolve.
     * @return Pointer to object or nullptr if not found/pending destroy.
     * @note Borrowed pointers must not be cached.
     */
    T* Borrowed(const Handle& HandleRef)
    {
        return Borrowed(HandleRef.Id);
    }

    /**
     * @brief Resolve a UUID to a borrowed pointer.
     * @param Id UUID to resolve.
     * @return Pointer to object or nullptr if not found/pending destroy.
     * @note Borrowed pointers must not be cached.
     */
    T* Borrowed(const Uuid& Id)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return nullptr;
        }
        Entry& EntryRef = m_entries[It->second];
        if (!EntryRef.m_object || EntryRef.m_pendingDestroy)
        {
            return nullptr;
        }
        return EntryRef.m_object.get();
    }

    /**
     * @brief Resolve a handle to a borrowed pointer (const).
     * @param HandleRef Handle to resolve.
     * @return Pointer to object or nullptr if not found/pending destroy.
     */
    const T* Borrowed(const Handle& HandleRef) const
    {
        return Borrowed(HandleRef.Id);
    }

    /**
     * @brief Resolve a UUID to a borrowed pointer (const).
     * @param Id UUID to resolve.
     * @return Pointer to object or nullptr if not found/pending destroy.
     */
    const T* Borrowed(const Uuid& Id) const
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return nullptr;
        }
        const Entry& EntryRef = m_entries[It->second];
        if (!EntryRef.m_object || EntryRef.m_pendingDestroy)
        {
            return nullptr;
        }
        return EntryRef.m_object.get();
    }

    /**
     * @brief Mark an object for end-of-frame destruction by handle.
     * @param HandleRef Handle to destroy.
     * @return Success or error if not found.
     * @remarks Object remains valid until EndFrame.
     */
    TExpected<void> DestroyLater(const Handle& HandleRef)
    {
        return DestroyLater(HandleRef.Id);
    }

    /**
     * @brief Mark an object for end-of-frame destruction by UUID.
     * @param Id UUID to destroy.
     * @return Success or error if not found.
     * @remarks Object remains valid until EndFrame.
     */
    TExpected<void> DestroyLater(const Uuid& Id)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Handle not found"));
        }
        Entry& EntryRef = m_entries[It->second];
        if (!EntryRef.m_object)
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
     * @brief Destroy all objects that were marked for deletion.
     * @remarks Frees slots and clears pending lists.
     * @note Should be called at end of frame to keep handles stable.
     * @note Destroyed UUID keys are removed from index and may be reused on future creates.
     */
    void EndFrame()
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        for (size_t Index : m_pendingDestroy)
        {
            if (Index >= m_entries.size())
            {
                continue;
            }
            Entry& EntryRef = m_entries[Index];
            m_index.erase(EntryRef.Id);
            EntryRef.m_object.reset();
            EntryRef.m_pendingDestroy = false;
            EntryRef.Id = Uuid{};
            m_freeList.push_back(Index);
        }
        m_pendingDestroy.clear();
    }

    /**
     * @brief Remove all objects immediately.
     * @remarks Clears the pool and all indices.
     * @note Use cautiously; invalidates all handles immediately.
     */
    void Clear()
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
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
        return IsPendingDestroy(HandleRef.Id);
    }

    /**
     * @brief Check if a UUID is pending destruction.
     * @param Id UUID to check.
     * @return True if the object is marked for deletion.
     */
    bool IsPendingDestroy(const Uuid& Id) const
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_index.find(Id);
        if (It == m_index.end())
        {
            return false;
        }
        return m_entries[It->second].m_pendingDestroy;
    }

    /**
     * @brief Iterate over all live (non-pending) objects (const).
     * @tparam Fn Callable type.
     * @param Func Callback invoked with (Handle, Object).
     * @remarks Skips pending-destroy entries so "already removed this frame" objects are excluded.
     */
    template<typename Fn>
    void ForEach(const Fn& Func) const
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        for (size_t Index = 0; Index < m_entries.size(); ++Index)
        {
            const Entry& EntryRef = m_entries[Index];
            if (!EntryRef.m_object || EntryRef.m_pendingDestroy)
            {
                continue;
            }
            Func(Handle(EntryRef.Id), *EntryRef.m_object);
        }
    }

    /**
     * @brief Iterate over all objects including pending destroy (const).
     * @tparam Fn Callable type.
     * @param Func Callback invoked with (Handle, Object).
     * @remarks Includes objects marked for deletion but not yet flushed by EndFrame.
     */
    template<typename Fn>
    void ForEachAll(const Fn& Func) const
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        for (size_t Index = 0; Index < m_entries.size(); ++Index)
        {
            const Entry& EntryRef = m_entries[Index];
            if (!EntryRef.m_object)
            {
                continue;
            }
            Func(Handle(EntryRef.Id), *EntryRef.m_object);
        }
    }

    /**
     * @brief Iterate over all live (non-pending) objects (mutable).
     * @tparam Fn Callable type.
     * @param Func Callback invoked with (Handle, Object).
     * @remarks Skips pending-destroy entries so mutation ignores soon-to-be-destroyed objects.
     */
    template<typename Fn>
    void ForEach(const Fn& Func)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        for (size_t Index = 0; Index < m_entries.size(); ++Index)
        {
            Entry& EntryRef = m_entries[Index];
            if (!EntryRef.m_object || EntryRef.m_pendingDestroy)
            {
                continue;
            }
            Func(Handle(EntryRef.Id), *EntryRef.m_object);
        }
    }

    /**
     * @brief Iterate over all objects including pending destroy (mutable).
     * @tparam Fn Callable type.
     * @param Func Callback invoked with (Handle, Object).
     * @remarks Includes objects marked for deletion but not yet flushed by EndFrame.
     */
    template<typename Fn>
    void ForEachAll(const Fn& Func)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        for (size_t Index = 0; Index < m_entries.size(); ++Index)
        {
            Entry& EntryRef = m_entries[Index];
            if (!EntryRef.m_object)
            {
                continue;
            }
            Func(Handle(EntryRef.Id), *EntryRef.m_object);
        }
    }

private:
    /**
     * @brief Internal storage entry.
     * @remarks Shared ownership keeps object addresses stable for borrowed pointer usage.
     */
    struct Entry
    {
        Uuid Id{}; /**< @brief UUID key for this entry. */
        std::shared_ptr<T> m_object{}; /**< @brief Stored object pointer. */
        bool m_pendingDestroy = false; /**< @brief True when scheduled for deletion. */
    };

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

    mutable std::mutex m_mutex{}; /**< @brief Protects pool state. */
    std::vector<Entry> m_entries{}; /**< @brief Dense storage for entries. */
    std::unordered_map<Uuid, size_t, UuidHash> m_index{}; /**< @brief UUID -> entry index. */
    std::vector<size_t> m_freeList{}; /**< @brief Reusable entry indices. */
    std::vector<size_t> m_pendingDestroy{}; /**< @brief Indices scheduled for deletion. */
};

} // namespace SnAPI::GameFramework
