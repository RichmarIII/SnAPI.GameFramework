#include "TypeRegistry.h"
#include "GameThreading.h"

#include <algorithm>
#include <unordered_set>

#include "TypeAutoRegistry.h"

namespace SnAPI::GameFramework
{

TypeRegistry& TypeRegistry::Instance()
{
    static TypeRegistry Instance;
    return Instance;
}

TExpected<TypeInfo*> TypeRegistry::Register(TypeInfo Info)
{
    GameLockGuard Lock(m_mutex);
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
        {
            GameLockGuard Lock(m_mutex);
            auto It = m_types.find(Id);
            if (It != m_types.end())
            {
                return &It->second;
            }
        }

        // Not found: attempt lazy registration without holding the registry lock.
        (void)TypeAutoRegistry::Instance().Ensure(Id);

        GameLockGuard Lock(m_mutex);
        auto It = m_types.find(Id);
        return It != m_types.end() ? &It->second : nullptr;
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
        {
            GameLockGuard Lock(m_mutex);
            auto It = m_nameToId.find(Name);
            if (It != m_nameToId.end())
            {
                auto TypeIt = m_types.find(It->second);
                if (TypeIt != m_types.end())
                {
                    return &TypeIt->second;
                }
            }
        }

        // Name not found: derive TypeId deterministically and attempt lazy registration.
        (void)TypeAutoRegistry::Instance().Ensure(TypeIdFromName(Name));

        GameLockGuard Lock(m_mutex);
        auto It = m_nameToId.find(Name);
        if (It == m_nameToId.end())
        {
            return nullptr;
        }
        auto TypeIt = m_types.find(It->second);
        return TypeIt != m_types.end() ? &TypeIt->second : nullptr;
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

void BuildLineageUnlocked(const std::unordered_map<TypeId, TypeInfo, UuidHash>& Types,
                          const TypeId& Type,
                          std::unordered_set<TypeId, UuidHash>& Visited,
                          std::vector<const TypeInfo*>& OutLineage)
{
    const auto It = Types.find(Type);
    if (It == Types.end())
    {
        return;
    }
    if (!Visited.insert(Type).second)
    {
        return;
    }

    for (const TypeId& BaseType : It->second.BaseTypes)
    {
        BuildLineageUnlocked(Types, BaseType, Visited, OutLineage);
    }

    OutLineage.push_back(&It->second);
}

std::vector<ReflectedFieldRef> CollectFieldsUnlocked(const std::unordered_map<TypeId, TypeInfo, UuidHash>& Types,
                                                     const TypeId& Type,
                                                     const bool IncludeBaseTypes)
{
    std::vector<ReflectedFieldRef> Result{};
    const auto It = Types.find(Type);
    if (It == Types.end())
    {
        return Result;
    }

    std::vector<const TypeInfo*> Lineage{};
    if (IncludeBaseTypes)
    {
        std::unordered_set<TypeId, UuidHash> Visited{};
        BuildLineageUnlocked(Types, Type, Visited, Lineage);
    }
    else
    {
        Lineage.push_back(&It->second);
    }

    for (const TypeInfo* Owner : Lineage)
    {
        for (const FieldInfo& Field : Owner->Fields)
        {
            Result.push_back(ReflectedFieldRef{
                .OwnerType = Owner->Id,
                .Field = &Field,
            });
        }
    }

    return Result;
}

std::vector<ReflectedMethodRef> CollectMethodsUnlocked(const std::unordered_map<TypeId, TypeInfo, UuidHash>& Types,
                                                       const TypeId& Type,
                                                       const bool IncludeBaseTypes)
{
    std::vector<ReflectedMethodRef> Result{};
    const auto It = Types.find(Type);
    if (It == Types.end())
    {
        return Result;
    }

    std::vector<const TypeInfo*> Lineage{};
    if (IncludeBaseTypes)
    {
        std::unordered_set<TypeId, UuidHash> Visited{};
        BuildLineageUnlocked(Types, Type, Visited, Lineage);
    }
    else
    {
        Lineage.push_back(&It->second);
    }

    for (const TypeInfo* Owner : Lineage)
    {
        if (Owner->Methods.empty())
        {
            continue;
        }

        if (IncludeBaseTypes)
        {
            std::unordered_set<std::string_view> DeclaredNames{};
            DeclaredNames.reserve(Owner->Methods.size());
            for (const MethodInfo& Method : Owner->Methods)
            {
                DeclaredNames.insert(Method.Name);
            }

            std::erase_if(Result, [&Types, &DeclaredNames, &Owner](const ReflectedMethodRef& Entry) {
                return Entry.Method &&
                       DeclaredNames.contains(Entry.Method->Name) &&
                       IsAUnlocked(Types, Owner->Id, Entry.OwnerType);
            });
        }

        for (const MethodInfo& Method : Owner->Methods)
        {
            Result.push_back(ReflectedMethodRef{
                .OwnerType = Owner->Id,
                .Method = &Method,
            });
        }
    }

    return Result;
}
} // namespace

bool TypeRegistry::IsA(const TypeId& Type, const TypeId& Base) const
{
    if (!m_frozen.load(std::memory_order_acquire))
    {
        GameLockGuard Lock(m_mutex);
        return IsAUnlocked(m_types, Type, Base);
    }
    return IsAUnlocked(m_types, Type, Base);
}

std::vector<const TypeInfo*> TypeRegistry::Derived(const TypeId& Base) const
{
    std::vector<const TypeInfo*> Result;
    if (!m_frozen.load(std::memory_order_acquire))
    {
        GameLockGuard Lock(m_mutex);
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

std::vector<ReflectedFieldRef> TypeRegistry::CollectFields(const TypeId& Type, const bool IncludeBaseTypes) const
{
    if (!Find(Type))
    {
        return {};
    }

    if (!m_frozen.load(std::memory_order_acquire))
    {
        GameLockGuard Lock(m_mutex);
        return CollectFieldsUnlocked(m_types, Type, IncludeBaseTypes);
    }

    return CollectFieldsUnlocked(m_types, Type, IncludeBaseTypes);
}

std::vector<ReflectedMethodRef> TypeRegistry::CollectMethods(const TypeId& Type, const bool IncludeBaseTypes) const
{
    if (!Find(Type))
    {
        return {};
    }

    if (!m_frozen.load(std::memory_order_acquire))
    {
        GameLockGuard Lock(m_mutex);
        return CollectMethodsUnlocked(m_types, Type, IncludeBaseTypes);
    }

    return CollectMethodsUnlocked(m_types, Type, IncludeBaseTypes);
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
