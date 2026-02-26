#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "StaticTypeId.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework
{

template<typename TBase>
class TSubClassOf
{
public:
    struct TEntry
    {
        std::string Name{};
        TypeId Type{};
    };

    TSubClassOf() = default;

    explicit TSubClassOf(const TypeId& Type)
    {
        (void)SetType(Type);
    }

    const std::string& GetTypeName() const
    {
        return m_typeName;
    }

    std::string& EditTypeName()
    {
        return m_typeName;
    }

    const TypeId& GetTypeId() const
    {
        return m_typeId;
    }

    TypeId& EditTypeId()
    {
        return m_typeId;
    }

    [[nodiscard]] bool IsNull() const
    {
        return m_typeId.is_nil();
    }

    void Clear()
    {
        m_typeName.clear();
        m_typeId = {};
    }

    [[nodiscard]] bool IsValid() const
    {
        return IsTypeCompatible(m_typeId);
    }

    [[nodiscard]] std::string ResolvedTypeName() const
    {
        if (!m_typeId.is_nil())
        {
            if (const TypeInfo* TypeInfo = TypeRegistry::Instance().Find(m_typeId))
            {
                return TypeInfo->Name;
            }
        }

        return m_typeName;
    }

    bool SetType(const TypeId& Type)
    {
        if (Type.is_nil())
        {
            Clear();
            return true;
        }

        const TypeInfo* TypeInfo = TypeRegistry::Instance().Find(Type);
        if (!TypeInfo || !IsTypeCompatible(Type))
        {
            return false;
        }

        m_typeId = Type;
        m_typeName = TypeInfo->Name;
        return true;
    }

    bool SetTypeByName(std::string_view Name)
    {
        if (Name.empty())
        {
            Clear();
            return true;
        }

        for (const TEntry& Entry : EnumerateTypes())
        {
            if (!NameMatches(Entry.Name, Name))
            {
                continue;
            }

            return SetType(Entry.Type);
        }

        return false;
    }

    template<typename TDerived>
    bool SetType()
    {
        static_assert(std::is_base_of_v<TBase, TDerived>, "TDerived must derive from TBase");
        return SetType(StaticTypeId<TDerived>());
    }

    [[nodiscard]] TypeId ResolveTypeOr(const TypeId& FallbackType) const
    {
        if (IsTypeCompatible(m_typeId))
        {
            return m_typeId;
        }
        return FallbackType;
    }

    static std::vector<TEntry> EnumerateTypes()
    {
        const TypeId BaseType = StaticTypeId<TBase>();

        std::vector<TEntry> Entries{};
        std::unordered_set<TypeId, UuidHash> SeenTypes{};

        if (const TypeInfo* BaseInfo = TypeRegistry::Instance().Find(BaseType))
        {
            Entries.push_back(TEntry{BaseInfo->Name, BaseInfo->Id});
            SeenTypes.insert(BaseInfo->Id);
        }
        else
        {
            Entries.push_back(TEntry{std::string(TTypeNameV<TBase>), BaseType});
            SeenTypes.insert(BaseType);
        }

        for (const TypeInfo* Derived : TypeRegistry::Instance().Derived(BaseType))
        {
            if (!Derived || SeenTypes.contains(Derived->Id))
            {
                continue;
            }

            Entries.push_back(TEntry{Derived->Name, Derived->Id});
            SeenTypes.insert(Derived->Id);
        }

        std::sort(Entries.begin(), Entries.end(), [](const TEntry& Left, const TEntry& Right) {
            return Left.Name < Right.Name;
        });

        return Entries;
    }

private:
    static bool IsTypeCompatible(const TypeId& Type)
    {
        if (Type.is_nil())
        {
            return false;
        }

        return TypeRegistry::Instance().IsA(Type, StaticTypeId<TBase>());
    }

    static bool NameMatches(const std::string& CandidateName, const std::string_view Query)
    {
        if (CandidateName == Query)
        {
            return true;
        }

        const size_t Separator = CandidateName.rfind("::");
        if (Separator == std::string::npos)
        {
            return false;
        }

        const std::string_view ShortName(CandidateName.c_str() + Separator + 2,
                                         CandidateName.size() - (Separator + 2));
        return ShortName == Query;
    }

    std::string m_typeName{};
    TypeId m_typeId{};
};

} // namespace SnAPI::GameFramework
