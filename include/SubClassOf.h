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

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reflected handle that stores a subclass selection constrained to a reflected base type.
 * @tparam TBase Required reflected base type.
 *
 * `TSubClassOf<TBase>` is the type-selection counterpart to `TAssetRef`: instead of pointing to an
 * asset instance, it points to reflected type metadata that must satisfy `TypeRegistry::IsA(Type, TBase)`.
 *
 * Core semantics:
 * - The stored `TypeId` is authoritative when valid.
 * - `TypeName` is a fallback/display string and is refreshed from `TypeRegistry` when a valid type is set.
 * - `SetTypeByName()` matches either the fully qualified reflected name or the final `::ShortName`.
 * - `EnumerateTypes()` includes the base type itself and every currently registered derived type.
 *
 * Threading model:
 * - Value operations are thread-safe in isolation.
 * - Validity and enumeration depend on the global `TypeRegistry`.
 */
template<typename TBase>
class TSubClassOf
{
public:
    /**
     * @brief One compatible reflected type entry.
     */
    struct TEntry
    {
        std::string Name{}; /**< @brief Reflected fully qualified type name. */
        TypeId Type{}; /**< @brief Stable reflected type id. */
    };

    /** @brief Construct an empty subclass selection. */
    TSubClassOf() = default;

    /**
     * @brief Construct a subclass selection from a type id.
     * @param Type Candidate reflected type id.
     *
     * Invalid ids leave the object empty.
     */
    explicit TSubClassOf(const TypeId& Type)
    {
        (void)SetType(Type);
    }

    /** @brief Access the stored fallback/display type name. */
    const std::string& GetTypeName() const
    {
        return m_typeName;
    }

    /**
     * @brief Mutably access the stored fallback/display type name.
     * @warning If edited directly, callers are responsible for keeping it consistent with `TypeId`.
     */
    std::string& EditTypeName()
    {
        return m_typeName;
    }

    /** @brief Access the stored type id. */
    const TypeId& GetTypeId() const
    {
        return m_typeId;
    }

    /**
     * @brief Mutably access the stored type id.
     * @warning If edited directly, callers are responsible for keeping it consistent with `TypeName`.
     */
    TypeId& EditTypeId()
    {
        return m_typeId;
    }

    /** @brief Query whether no subclass is currently selected. */
    [[nodiscard]] bool IsNull() const
    {
        return m_typeId.is_nil();
    }

    /** @brief Clear both the stored type id and fallback/display name. */
    void Clear()
    {
        m_typeName.clear();
        m_typeId = {};
    }

    /**
     * @brief Check whether the stored type id currently resolves to a compatible reflected type.
     * @return `true` when the stored id is non-null and `IsA(id, StaticTypeId<TBase>())`.
     */
    [[nodiscard]] bool IsValid() const
    {
        return IsTypeCompatible(m_typeId);
    }

    /**
     * @brief Resolve the best current type name for display.
     * @return Reflected name from `TypeRegistry` when the stored id is valid, otherwise the stored fallback name.
     */
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

    /**
     * @brief Set the subclass selection from a reflected type id.
     * @param Type Candidate reflected type id.
     * @return `true` when the id resolves to a compatible reflected type or when clearing with a nil id.
     */
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

    /**
     * @brief Set the subclass selection by reflected name.
     * @param Name Fully qualified type name or short unqualified type name.
     * @return `true` when a compatible reflected type is found.
     */
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

    /**
     * @brief Set the subclass selection from a compile-time derived type.
     * @tparam TDerived Type that must derive from `TBase`.
     * @return `true` when the reflected type is compatible and available.
     */
    template<typename TDerived>
    bool SetType()
    {
        static_assert(std::is_base_of_v<TBase, TDerived>, "TDerived must derive from TBase");
        return SetType(StaticTypeId<TDerived>());
    }

    /**
     * @brief Resolve the stored type id or fall back to a caller-supplied default.
     * @param FallbackType Type id returned when the current selection is invalid.
     * @return Compatible stored type id or `FallbackType`.
     */
    [[nodiscard]] TypeId ResolveTypeOr(const TypeId& FallbackType) const
    {
        if (IsTypeCompatible(m_typeId))
        {
            return m_typeId;
        }
        return FallbackType;
    }

    /**
     * @brief Enumerate the currently known compatible reflected types.
     * @return Sorted list containing the base type and all currently registered derived types.
     *
     * Enumeration reflects the current `TypeRegistry` snapshot and therefore grows as more lazy
     * auto-registration callbacks are executed.
     */
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
