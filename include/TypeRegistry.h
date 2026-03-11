#pragma once

#include <atomic>
#include "GameThreading.h"
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Expected.h"
#include "Flags.h"
#include "Invoker.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

class IWorld;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Heterogeneous hash functor for reflected type-name lookup tables.
 *
 * Supports `std::string` and `std::string_view` without transient allocations.
 */
struct TransparentStringHash
{
    using is_transparent = void;

    size_t operator()(std::string_view Value) const noexcept
    {
        return std::hash<std::string_view>{}(Value);
    }

    size_t operator()(const std::string& Value) const noexcept
    {
        return std::hash<std::string_view>{}(Value);
    }
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Heterogeneous equality functor paired with `TransparentStringHash`.
 */
struct TransparentStringEqual
{
    using is_transparent = void;

    bool operator()(std::string_view Left, std::string_view Right) const noexcept
    {
        return Left == Right;
    }

    bool operator()(std::string_view Left, const std::string& Right) const noexcept
    {
        return Left == std::string_view(Right);
    }

    bool operator()(const std::string& Left, std::string_view Right) const noexcept
    {
        return std::string_view(Left) == Right;
    }

    bool operator()(const std::string& Left, const std::string& Right) const noexcept
    {
        return Left == Right;
    }
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Field-level behavior flags carried by reflection metadata.
 */
enum class EFieldFlagBits : uint32_t
{
    None = 0, /**< @brief No special field behavior flags. */
    Replication = 1u << 0, /**< @brief Field is eligible for replication payload traversal. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Bitflag wrapper for `EFieldFlagBits`.
 */
using FieldFlags = TFlags<EFieldFlagBits>;
template<>
struct EnableFlags<EFieldFlagBits> : std::true_type
{
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Method-level behavior flags carried by reflection metadata.
 *
 * These flags are primarily consumed by RPC and networking bridges rather than generic invocation.
 */
enum class EMethodFlagBits : uint32_t
{
    None = 0, /**< @brief No special method behavior flags. */
    RpcReliable = 1u << 0, /**< @brief Prefer reliable transport channel for RPC dispatch. */
    RpcUnreliable = 1u << 1, /**< @brief Prefer unreliable transport channel for RPC dispatch. */
    RpcNetServer = 1u << 2, /**< @brief Method is intended as server-target endpoint. */
    RpcNetClient = 1u << 3, /**< @brief Method is intended as client-target endpoint. */
    RpcNetMulticast = 1u << 4, /**< @brief Method is intended for server-initiated multicast dispatch. */
    EditorAction = 1u << 5, /**< @brief Expose the method as an editor action button when inspector tooling supports it. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Bitflag wrapper for `EMethodFlagBits`.
 */
using MethodFlags = TFlags<EMethodFlagBits>;
template<>
struct EnableFlags<EMethodFlagBits> : std::true_type
{
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Erased runtime lifecycle and copy hooks for one reflected type.
 *
 * These operations are used by Conduit frame storage and other low-level systems that need to own
 * typed values without going through `Variant`.
 */
struct TypeRuntimeOps
{
    void (*DefaultConstruct)(void* Storage) = nullptr; /**< @brief Placement-default-construct into storage. */
    void (*Destroy)(void* Storage) noexcept = nullptr; /**< @brief Destroy a live object in storage. */
    void (*CopyConstruct)(const void* Source, void* Dest) = nullptr; /**< @brief Placement-copy-construct into destination storage. */
    void (*MoveConstruct)(void* Source, void* Dest) noexcept = nullptr; /**< @brief Placement-move-construct into destination storage. */
    void (*CopyAssign)(const void* Source, void* Dest) = nullptr; /**< @brief Assign from source into an existing destination object. */
    void (*MoveAssign)(void* Source, void* Dest) noexcept = nullptr; /**< @brief Move-assign from source into an existing destination object. */
    bool (*Equals)(const void* Left, const void* Right) = nullptr; /**< @brief Optional equality predicate. */
};

template<typename T>
const TypeRuntimeOps& GetTypeRuntimeOps()
{
    static const TypeRuntimeOps Ops{
        .DefaultConstruct = []() -> void (*)(void*) {
            if constexpr (std::is_default_constructible_v<T>)
            {
                return [] (void* Storage) {
                    std::construct_at(static_cast<T*>(Storage));
                };
            }
            else
            {
                return nullptr;
            }
        }(),
        .Destroy = []() -> void (*)(void*) noexcept {
            if constexpr (std::is_trivially_destructible_v<T>)
            {
                return [] (void*) noexcept {};
            }
            else
            {
                return [] (void* Storage) noexcept {
                    std::destroy_at(static_cast<T*>(Storage));
                };
            }
        }(),
        .CopyConstruct = []() -> void (*)(const void*, void*) {
            if constexpr (std::is_copy_constructible_v<T>)
            {
                return [] (const void* Source, void* Dest) {
                    std::construct_at(static_cast<T*>(Dest), *static_cast<const T*>(Source));
                };
            }
            else
            {
                return nullptr;
            }
        }(),
        .MoveConstruct = []() -> void (*)(void*, void*) noexcept {
            if constexpr (std::is_move_constructible_v<T>)
            {
                return [] (void* Source, void* Dest) noexcept {
                    std::construct_at(static_cast<T*>(Dest), std::move(*static_cast<T*>(Source)));
                };
            }
            else
            {
                return nullptr;
            }
        }(),
        .CopyAssign = []() -> void (*)(const void*, void*) {
            if constexpr (std::is_copy_assignable_v<T>)
            {
                return [] (const void* Source, void* Dest) {
                    *static_cast<T*>(Dest) = *static_cast<const T*>(Source);
                };
            }
            else
            {
                return nullptr;
            }
        }(),
        .MoveAssign = []() -> void (*)(void*, void*) noexcept {
            if constexpr (std::is_move_assignable_v<T>)
            {
                return [] (void* Source, void* Dest) noexcept {
                    *static_cast<T*>(Dest) = std::move(*static_cast<T*>(Source));
                };
            }
            else
            {
                return nullptr;
            }
        }(),
        .Equals = [] (const void* Left, const void* Right) -> bool {
            if constexpr (requires (const T& A, const T& B) { { A == B } -> std::convertible_to<bool>; })
            {
                return *static_cast<const T*>(Left) == *static_cast<const T*>(Right);
            }
            else
            {
                return false;
            }
        }
    };
    return Ops;
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reflection metadata for one field-like property.
 *
 * Field access is intentionally multi-lane:
 * - `Getter` / `Setter` provide value-semantic access through `Variant`
 * - `ViewGetter` provides a non-owning `VariantView` fast path
 * - `RawSetter` provides a typed-erased write fast path for systems like Conduit
 * - `ConstPointer` / `MutablePointer` provide raw-address access for hot serialization and replication paths
 *
 * Not every lane is required to be populated. Read-only and write-only reflected properties are
 * represented by leaving unsupported accessors absent or returning errors.
 */
struct FieldInfo
{
    std::string Name; /**< @brief Field name as registered. */
    TypeId FieldType; /**< @brief TypeId of the field. */
    FieldFlags Flags{}; /**< @brief Field flags (replication, etc.). */
    std::function<TExpected<Variant>(void* Instance)> Getter; /**< @brief Getter callback. */
    std::function<Result(void* Instance, const Variant& Value)> Setter; /**< @brief Setter callback. */
    std::function<TExpected<VariantView>(void* Instance)> ViewGetter; /**< @brief Non-owning getter. */
    std::function<Result(void* Instance, const void* Value)> RawSetter; /**< @brief Typed-erased direct setter callback. */
    std::function<const void*(const void* Instance)> ConstPointer; /**< @brief Direct const pointer accessor. */
    std::function<void*(void* Instance)> MutablePointer; /**< @brief Direct mutable pointer accessor. */
    bool IsConst = false; /**< @brief True if field is const-qualified. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reflection metadata for one invokable method.
 *
 * Invocation is expressed in terms of variant-packed arguments and a variant return value so the same
 * metadata can serve scripting, editor tooling, and RPC dispatch.
 */
struct MethodInfo
{
    std::string Name; /**< @brief Method name as registered. */
    TypeId ReturnType; /**< @brief Return type id. */
    std::vector<TypeId> ParamTypes; /**< @brief Parameter type ids. */
    std::vector<EMethodParamPassKind> ParamPassKinds; /**< @brief Raw fast-path argument passing modes. */
    MethodInvoker Invoke; /**< @brief Invocation callback. */
    RawMethodInvoker RawInvoke = nullptr; /**< @brief Optional fast-path raw invoker used by Conduit. */
    std::shared_ptr<const void> RawInvokeUserData{}; /**< @brief Opaque payload captured for `RawInvoke`. */
    MethodFlags Flags{}; /**< @brief Method flags (rpc, etc.). */
    bool IsConst = false; /**< @brief True if method is const-qualified. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reflection metadata for one constructor signature.
 *
 * Constructor callbacks return an owning `shared_ptr<void>` so type-erased creation can be routed
 * through a common interface.
 */
struct ConstructorInfo
{
    std::vector<TypeId> ParamTypes; /**< @brief Parameter type ids. */
    std::function<TExpected<std::shared_ptr<void>>(std::span<const Variant> Args)> Construct; /**< @brief Construction callback. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reflection metadata for one enum entry.
 */
struct EnumValueInfo
{
    std::string Name; /**< @brief Symbolic enum entry name (e.g. "Dynamic"). */
    std::uint64_t Value = 0; /**< @brief Raw underlying-value bits (zero-extended to 64-bit). */
};

struct TypeCastInfo
{
    TypeId TargetType{}; /**< @brief Direct reflected base/interface target type. */
    void* (*CastMutable)(void* Instance) = nullptr; /**< @brief Mutable pointer-adjust cast for this direct relationship. */
    const void* (*CastConst)(const void* Instance) = nullptr; /**< @brief Const pointer-adjust cast for this direct relationship. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Central reflection metadata record for one type.
 *
 * `TypeInfo` is the contract object consumed by serialization, replication, RPC, editor tooling, and
 * type-erased construction. It intentionally stores only runtime-usable metadata and callback entry points.
 *
 * Ownership and lifetime:
 * - Once registered, the stored `TypeInfo` lives inside `TypeRegistry` for the process lifetime.
 * - Callback pointers and lambdas must therefore remain valid for the life of the process.
 */
struct TypeInfo
{
#if defined(WITH_EDITOR) && WITH_EDITOR
    using EditorPropertyChangedInvoker = void(*)(void* Instance, std::string_view Name);
#endif
    using NodeOnCreateInvoker = void(*)(void* Instance, IWorld* WorldRef);

    TypeId Id; /**< @brief Deterministic type id. */
    std::string Name; /**< @brief Fully qualified stable reflected type name. */
    size_t Size = 0; /**< @brief `sizeof(T)` for plain reflected types, or `0` for synthetic marker types like `void`. */
    size_t Align = 0; /**< @brief `alignof(T)` for plain reflected types, or `0` for synthetic marker types like `void`. */
    std::vector<TypeId> BaseTypes; /**< @brief Direct reflected base types. Transitive relationships are derived by traversal at query time. */
    std::vector<TypeId> InterfaceTypes; /**< @brief Direct reflected interface/abstract relationships. These participate in `IsA()`/`Derived()` but exist separately from concrete base inheritance. */
    std::vector<TypeCastInfo> DirectCasts; /**< @brief Direct pointer-adjust cast shims for reflected base/interface relationships. */
    std::vector<FieldInfo> Fields; /**< @brief Field metadata declared directly on this type. Inherited fields are discovered through `TypeRegistry::CollectFields()`. */
    std::vector<MethodInfo> Methods; /**< @brief Method metadata declared directly on this type. */
    std::vector<ConstructorInfo> Constructors; /**< @brief Reflected constructors available for type-erased creation. */
    const TypeRuntimeOps* RuntimeOps = nullptr; /**< @brief Runtime lifecycle hooks for owning values of this type. */
    bool IsAbstract = false; /**< @brief `true` when the reflected C++ type is abstract/non-constructible. */
    bool IsInterface = false; /**< @brief `true` when the reflected type is intended to act as an interface contract. */
    bool IsEnum = false; /**< @brief `true` when this type record represents an enum. */
    bool EnumIsSigned = false; /**< @brief `true` when the enum underlying type is signed. */
    std::vector<EnumValueInfo> EnumValues; /**< @brief Enum entries for tooling, serialization, and UI. */
    NodeOnCreateInvoker NodeOnCreate = nullptr; /**< @brief Optional node `OnCreate` callback installed by `TTypeBuilder` for `BaseNode`-derived types. */
#if defined(WITH_EDITOR) && WITH_EDITOR
    EditorPropertyChangedInvoker EditorPropertyChanged = nullptr; /**< @brief Optional editor-only property-changed callback used by reflected field setters. */
#endif
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief View record for one reflected field discovered during collection.
 *
 * `OwnerType` identifies where the field was declared within the inheritance chain.
 */
struct ReflectedFieldRef
{
    TypeId OwnerType{};
    const FieldInfo* Field = nullptr;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief View record for one reflected method discovered during collection.
 *
 * `OwnerType` identifies where the method was declared within the inheritance chain.
 */
struct ReflectedMethodRef
{
    TypeId OwnerType{};
    const MethodInfo* Method = nullptr;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Global runtime registry of reflected type metadata.
 *
 * `TypeRegistry` is the canonical metadata index keyed by deterministic `TypeId`.
 *
 * Read/write model:
 * - Unfrozen mode: registration and lookup use mutex protection, and `Find*()` may trigger lazy
 *   auto-registration on misses.
 * - Frozen mode: registration is rejected, no lazy auto-registration is attempted, and lookup uses a
 *   lock-free fast path over the already-populated maps.
 *
 * This allows startup/bootstrap code to remain flexible while hot lookup paths in replication,
 * serialization, and tooling avoid lock contention once bootstrap is complete.
 *
 * Ownership and lifetime:
 * - Stored `TypeInfo` records live for the process lifetime.
 * - Returned pointers remain valid for the lifetime of the process once registration succeeds.
 */
class TypeRegistry
{
public:
    /**
     * @brief Access the singleton TypeRegistry instance.
     * @return Reference to the registry.
     */
    static TypeRegistry& Instance();

    /**
     * @brief Register a new type record.
     * @param Info Owning metadata payload to store.
     * @return Pointer to the stored `TypeInfo` or an error.
     *
     * Registration fails when:
     * - the registry is frozen
     * - the `TypeId` already exists
     *
     * The implementation currently only checks duplicate ids, not duplicate names, so callers should
     * still treat reflected names as globally unique.
     */
    TExpected<TypeInfo*> Register(TypeInfo Info);
    /**
     * @brief Find a reflected type by `TypeId`.
     * @param Id Type id to look up.
     * @return Pointer to the stored metadata or `nullptr`.
     *
     * In unfrozen mode, a miss triggers `TypeAutoRegistry::Ensure(Id)` before the lookup is retried.
     */
    const TypeInfo* Find(const TypeId& Id) const;
    /**
     * @brief Find a reflected type by stable name.
     * @param Name Fully qualified type name.
     * @return Pointer to the stored metadata or `nullptr`.
     *
     * In unfrozen mode, a miss deterministically derives `TypeIdFromName(Name)` and tries lazy
     * auto-registration before retrying the lookup.
     */
    const TypeInfo* FindByName(std::string_view Name) const;
    /**
     * @brief Check the reflected inheritance relationship between two types.
     * @param Type Candidate derived type id.
     * @param Base Candidate base type id.
     * @return `true` when `Type == Base` or the reflected base graph reaches `Base`.
     */
    bool IsA(const TypeId& Type, const TypeId& Base) const;
    /**
     * @brief Cast an erased mutable instance pointer between reflected related types.
     * @param SourceType Concrete/source reflected type id for @p Instance.
     * @param TargetType Reflected target base/interface type id.
     * @param Instance Pointer to a live object of type @p SourceType.
     * @return Pointer adjusted to @p TargetType or `nullptr` when no reflected cast path exists.
     */
    void* Cast(const TypeId& SourceType, const TypeId& TargetType, void* Instance) const;
    /**
     * @brief Cast an erased const instance pointer between reflected related types.
     * @param SourceType Concrete/source reflected type id for @p Instance.
     * @param TargetType Reflected target base/interface type id.
     * @param Instance Pointer to a live object of type @p SourceType.
     * @return Pointer adjusted to @p TargetType or `nullptr` when no reflected cast path exists.
     */
    const void* Cast(const TypeId& SourceType, const TypeId& TargetType, const void* Instance) const;
    /**
     * @brief Enumerate all currently registered reflected types.
     * @return Vector of pointers into the current registry snapshot.
     */
    std::vector<const TypeInfo*> All() const;
    /**
     * @brief Enumerate all currently registered types derived from a base type.
     * @param Base Base type id.
     * @return Vector of pointers into the current registry snapshot.
     *
     * The result excludes the base type itself and includes transitive descendants.
     */
    std::vector<const TypeInfo*> Derived(const TypeId& Base) const;
    /**
     * @brief Collect reflected fields for a type.
     * @param Type Type to inspect.
     * @param IncludeBaseTypes `true` to include inherited fields in base-to-derived order.
     * @return Field view entries with declaring owner type.
     *
     * The function first ensures the type exists through `Find(Type)` before walking the registry snapshot.
     */
    std::vector<ReflectedFieldRef> CollectFields(const TypeId& Type, bool IncludeBaseTypes = true) const;
    /**
     * @brief Collect reflected methods for a type.
     * @param Type Type to inspect.
     * @param IncludeBaseTypes `true` to include inherited methods in base-to-derived order.
     * @return Method view entries with declaring owner type.
     *
     * When inherited methods are included, derived declarations hide base declarations with the same
     * method name, matching C++ name-hiding behavior.
     */
    std::vector<ReflectedMethodRef> CollectMethods(const TypeId& Type, bool IncludeBaseTypes = true) const;
    /**
     * @brief Enable or disable frozen lookup mode.
     * @param Enable `true` to freeze the registry.
     *
     * Freezing prevents future registration and disables lazy auto-registration side effects during
     * lookup. Unfreezing re-enables mutation and lock-based lazy lookup behavior.
     */
    void Freeze(bool Enable);
    /**
     * @brief Check whether the registry is currently frozen.
     * @return `true` when frozen.
     */
    bool IsFrozen() const;

private:
    mutable GameMutex m_mutex{}; /**< @brief Guards registry mutation and non-frozen lookups. */
    std::atomic<bool> m_frozen{false}; /**< @brief Frozen state flag controlling read/write mode behavior. */
    std::unordered_map<TypeId, TypeInfo, UuidHash> m_types{}; /**< @brief Primary metadata store keyed by TypeId. */
    std::unordered_map<std::string, TypeId, TransparentStringHash, TransparentStringEqual> m_nameToId{}; /**< @brief Secondary name index for lookup by stable type name. */
};

} // namespace SnAPI::GameFramework
