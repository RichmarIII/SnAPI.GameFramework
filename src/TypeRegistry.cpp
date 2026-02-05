#include "TypeRegistry.h"

namespace SnAPI::GameFramework
{

TypeRegistry& TypeRegistry::Instance()
{
    static TypeRegistry Instance;
    return Instance;
}

TExpected<TypeInfo*> TypeRegistry::Register(TypeInfo Info)
{
    std::lock_guard<std::mutex> Lock(m_mutex);
    if (m_frozen.load(std::memory_order_acquire))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Type registry is frozen"));
    }
    auto It = m_types.find(Info.Id);
    if (It != m_types.end())
    {
        return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Type already registered"));
    }
    auto Inserted = m_types.emplace(Info.Id, std::move(Info));
    m_nameToId.emplace(Inserted.first->second.Name, Inserted.first->first);
    return &Inserted.first->second;
}

const TypeInfo* TypeRegistry::Find(const TypeId& Id) const
{
    if (!m_frozen.load(std::memory_order_acquire))
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_types.find(Id);
        if (It == m_types.end())
        {
            return nullptr;
        }
        return &It->second;
    }
    auto It = m_types.find(Id);
    if (It == m_types.end())
    {
        return nullptr;
    }
    return &It->second;
}

const TypeInfo* TypeRegistry::FindByName(std::string_view Name) const
{
    if (!m_frozen.load(std::memory_order_acquire))
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        auto It = m_nameToId.find(Name);
        if (It == m_nameToId.end())
        {
            return nullptr;
        }
        auto TypeIt = m_types.find(It->second);
        if (TypeIt == m_types.end())
        {
            return nullptr;
        }
        return &TypeIt->second;
    }
    auto It = m_nameToId.find(Name);
    if (It == m_nameToId.end())
    {
        return nullptr;
    }
    auto TypeIt = m_types.find(It->second);
    if (TypeIt == m_types.end())
    {
        return nullptr;
    }
    return &TypeIt->second;
}

namespace
{
bool IsAUnlocked(const std::unordered_map<TypeId, TypeInfo, UuidHash>& Types, const TypeId& Type, const TypeId& Base)
{
    if (Type == Base)
    {
        return true;
    }
    auto It = Types.find(Type);
    if (It == Types.end())
    {
        return false;
    }
    const TypeInfo& Info = It->second;
    for (const auto& Parent : Info.BaseTypes)
    {
        if (Parent == Base)
        {
            return true;
        }
        if (IsAUnlocked(Types, Parent, Base))
        {
            return true;
        }
    }
    return false;
}
} // namespace

bool TypeRegistry::IsA(const TypeId& Type, const TypeId& Base) const
{
    if (!m_frozen.load(std::memory_order_acquire))
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        return IsAUnlocked(m_types, Type, Base);
    }
    return IsAUnlocked(m_types, Type, Base);
}

std::vector<const TypeInfo*> TypeRegistry::Derived(const TypeId& Base) const
{
    std::vector<const TypeInfo*> Result;
    if (!m_frozen.load(std::memory_order_acquire))
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        for (const auto& [Id, Info] : m_types)
        {
            if (Id == Base)
            {
                continue;
            }
            if (IsAUnlocked(m_types, Id, Base))
            {
                Result.push_back(&Info);
            }
        }
        return Result;
    }
    for (const auto& [Id, Info] : m_types)
    {
        if (Id == Base)
        {
            continue;
        }
        if (IsAUnlocked(m_types, Id, Base))
        {
            Result.push_back(&Info);
        }
    }
    return Result;
}

void TypeRegistry::Freeze(bool Enable)
{
    m_frozen.store(Enable, std::memory_order_release);
}

bool TypeRegistry::IsFrozen() const
{
    return m_frozen.load(std::memory_order_acquire);
}

} // namespace SnAPI::GameFramework
