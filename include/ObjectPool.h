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

template<typename T>
class TObjectPool : public std::enable_shared_from_this<TObjectPool<T>>
{
public:
    using Handle = THandle<T>;

    TObjectPool() = default;

    template<typename U = T, typename... Args>
    TExpected<Handle> Create(Args&&... args)
    {
        static_assert(std::is_base_of_v<T, U>, "Pool type mismatch");
        return CreateWithId<U>(NewUuid(), std::forward<Args>(args)...);
    }

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

    TExpected<Handle> CreateFromShared(std::shared_ptr<T> Object)
    {
        if (!Object)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null object"));
        }
        return CreateFromSharedWithId(std::move(Object), NewUuid());
    }

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

    bool IsValid(const Handle& HandleRef) const
    {
        return IsValid(HandleRef.Id);
    }

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

    T* Borrowed(const Handle& HandleRef)
    {
        return Borrowed(HandleRef.Id);
    }

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

    const T* Borrowed(const Handle& HandleRef) const
    {
        return Borrowed(HandleRef.Id);
    }

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

    TExpected<void> DestroyLater(const Handle& HandleRef)
    {
        return DestroyLater(HandleRef.Id);
    }

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

    void Clear()
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        m_entries.clear();
        m_index.clear();
        m_freeList.clear();
        m_pendingDestroy.clear();
    }

    bool IsPendingDestroy(const Handle& HandleRef) const
    {
        return IsPendingDestroy(HandleRef.Id);
    }

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
    struct Entry
    {
        Uuid Id{};
        std::shared_ptr<T> m_object{};
        bool m_pendingDestroy = false;
    };

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

    mutable std::mutex m_mutex{};
    std::vector<Entry> m_entries{};
    std::unordered_map<Uuid, size_t, UuidHash> m_index{};
    std::vector<size_t> m_freeList{};
    std::vector<size_t> m_pendingDestroy{};
};

} // namespace SnAPI::GameFramework
