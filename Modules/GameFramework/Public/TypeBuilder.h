#pragma once

#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "BaseComponent.h"
#include "BaseNode.h"
#include "NodeStorageFactoryRegistry.h"
#include "Serialization.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "TypeRegistry.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Fluent builder for registering reflection metadata for one type.
 * @tparam T Type being registered.
 *
 * `TTypeBuilder<T>` accumulates a `TypeInfo` record and then commits it into `TypeRegistry`.
 *
 * Design responsibilities:
 * - declare direct base-type relationships
 * - describe fields as readable, writable, or read-write reflected properties
 * - describe reflected methods and constructors
 * - automatically bridge supported node `OnCreate` and editor property-change callbacks
 * - automatically register component serialization when `T` derives from `BaseComponent`
 *
 * Best-practice lifecycle:
 * 1. add base types, fields, methods, and constructors
 * 2. call `Register()` exactly once in one translation unit, usually through `SNAPI_REFLECT_TYPE`
 * 3. let `TypeAutoRegistry` ensure the metadata on first use
 *
 * Threading model:
 * - Building is single-threaded, local-value work.
 * - Registration delegates to `TypeRegistry`, which is process-global and synchronized.
 */
template<typename T>
class TTypeBuilder
{
public:
    static_assert(std::is_class_v<T> || std::is_union_v<T>, "TTypeBuilder requires class/struct types");

private:
    template<typename Method>
    struct TGetterMethodTraits
    {
        static constexpr bool Valid = false;
    };

    template<typename R>
    struct TGetterMethodTraits<R (T::*)()>
    {
        static constexpr bool Valid = true;
        static constexpr bool IsConstMethod = false;
        using ReturnType = R;
    };

    template<typename R>
    struct TGetterMethodTraits<R (T::*)() noexcept>
    {
        static constexpr bool Valid = true;
        static constexpr bool IsConstMethod = false;
        using ReturnType = R;
    };

    template<typename R>
    struct TGetterMethodTraits<R (T::*)() const>
    {
        static constexpr bool Valid = true;
        static constexpr bool IsConstMethod = true;
        using ReturnType = R;
    };

    template<typename R>
    struct TGetterMethodTraits<R (T::*)() const noexcept>
    {
        static constexpr bool Valid = true;
        static constexpr bool IsConstMethod = true;
        using ReturnType = R;
    };

    template<typename Method>
    struct TSetterMethodTraits
    {
        static constexpr bool Valid = false;
    };

    template<typename R, typename Arg>
    struct TSetterMethodTraits<R (T::*)(Arg)>
    {
        static constexpr bool Valid = true;
        using ReturnType = R;
        using ArgType = Arg;
    };

    template<typename R, typename Arg>
    struct TSetterMethodTraits<R (T::*)(Arg) noexcept>
    {
        static constexpr bool Valid = true;
        using ReturnType = R;
        using ArgType = Arg;
    };

    template<typename R, typename Arg>
    struct TSetterMethodTraits<R (T::*)(Arg) const>
    {
        static constexpr bool Valid = true;
        using ReturnType = R;
        using ArgType = Arg;
    };

    template<typename R, typename Arg>
    struct TSetterMethodTraits<R (T::*)(Arg) const noexcept>
    {
        static constexpr bool Valid = true;
        using ReturnType = R;
        using ArgType = Arg;
    };

    template<typename GetterMethod>
    using TGetterTraits = TGetterMethodTraits<std::remove_cvref_t<GetterMethod>>;

    template<typename SetterMethod>
    using TSetterTraits = TSetterMethodTraits<std::remove_cvref_t<SetterMethod>>;

    template<typename GetterMethod>
    static constexpr bool IsGetterMethodV = TGetterTraits<GetterMethod>::Valid;

    template<typename SetterMethod>
    static constexpr bool IsSetterMethodV = TSetterTraits<SetterMethod>::Valid;

    template<typename SetterReturn>
    static constexpr bool IsSupportedSetterReturnV = std::is_same_v<std::remove_cvref_t<SetterReturn>, void> ||
                                                     std::is_same_v<std::remove_cvref_t<SetterReturn>, bool> ||
                                                     std::is_same_v<std::remove_cvref_t<SetterReturn>, Result>;

    template<typename TValue>
    static constexpr bool IsEqualityComparableV = requires(const TValue& Left, const TValue& Right) {
        { Left == Right } -> std::convertible_to<bool>;
    };

    template<typename TValue>
    static bool ValuesEqual(const TValue& Left, const TValue& Right)
    {
        if constexpr (IsEqualityComparableV<TValue>)
        {
            return static_cast<bool>(Left == Right);
        }
        else
        {
            (void)Left;
            (void)Right;
            return false;
        }
    }

    template<typename TObject>
    static constexpr bool SupportsNodeOnCreateWithWorldV =
        requires(TObject& Value, IWorld& WorldRef) {
            { Value.OnCreate(WorldRef) } -> std::same_as<void>;
        };

    template<typename TObject>
    static constexpr bool SupportsNodeOnCreateNoWorldV =
        requires(TObject& Value) {
            { Value.OnCreate() } -> std::same_as<void>;
        };

    static constexpr bool HasDeclaredNodeOnCreateV =
        std::is_base_of_v<BaseNode, T> &&
        (SupportsNodeOnCreateWithWorldV<T> || SupportsNodeOnCreateNoWorldV<T>);

    template<typename TReturn>
    static const TypeId& ReflectedMethodReturnTypeId()
    {
        if constexpr (std::is_void_v<TReturn>)
        {
            return StaticTypeId<void>();
        }
        else if constexpr (std::is_lvalue_reference_v<TReturn>)
        {
            using ReturnPointer = std::add_pointer_t<std::remove_reference_t<TReturn>>;
            return StaticTypeId<ReturnPointer>();
        }
        else
        {
            return StaticTypeId<std::remove_cvref_t<TReturn>>();
        }
    }

    static void InvokeDeclaredNodeOnCreate(T& Typed, IWorld* const WorldRef)
    {
        if constexpr (SupportsNodeOnCreateWithWorldV<T>)
        {
            if (WorldRef)
            {
                Typed.OnCreate(*WorldRef);
                return;
            }
        }

        if constexpr (SupportsNodeOnCreateNoWorldV<T>)
        {
            Typed.OnCreate();
        }
    }

    static void InvokeNodeOnCreateCallback(void* const Instance, IWorld* const WorldRef)
    {
        if (!Instance)
        {
            return;
        }
        auto* Typed = static_cast<T*>(Instance);
        InvokeDeclaredNodeOnCreate(*Typed, WorldRef);
    }

    static TypeInfo::NodeOnCreateInvoker NodeOnCreateInvokerForType()
    {
        if constexpr (HasDeclaredNodeOnCreateV)
        {
            return &InvokeNodeOnCreateCallback;
        }
        return nullptr;
    }

#if defined(WITH_EDITOR) && WITH_EDITOR
    template<typename TObject>
    static constexpr bool DeclaresEditorOnPropertyChangedStringViewV =
        requires {
            { &TObject::EditorOnPropertyChanged } -> std::same_as<void (TObject::*)(std::string_view)>;
        };

    template<typename TObject>
    static constexpr bool DeclaresEditorOnPropertyChangedStringRefV =
        requires {
            { &TObject::EditorOnPropertyChanged } -> std::same_as<void (TObject::*)(const std::string&)>;
        };

    static constexpr bool HasDeclaredEditorOnPropertyChangedV =
        DeclaresEditorOnPropertyChangedStringViewV<T> ||
        DeclaresEditorOnPropertyChangedStringRefV<T>;

    static void InvokeDeclaredEditorOnPropertyChanged(T& Typed, const std::string_view FieldName)
    {
        if constexpr (DeclaresEditorOnPropertyChangedStringViewV<T>)
        {
            Typed.EditorOnPropertyChanged(FieldName);
        }
        else if constexpr (DeclaresEditorOnPropertyChangedStringRefV<T>)
        {
            Typed.EditorOnPropertyChanged(std::string(FieldName));
        }
    }

    static void InvokeEditorOnPropertyChangedCallback(void* const Instance, const std::string_view FieldName)
    {
        if (!Instance)
        {
            return;
        }
        auto* Typed = static_cast<T*>(Instance);
        InvokeDeclaredEditorOnPropertyChanged(*Typed, FieldName);
    }

    static TypeInfo::EditorPropertyChangedInvoker EditorOnPropertyChangedInvokerForType()
    {
        if constexpr (HasDeclaredEditorOnPropertyChangedV)
        {
            return &InvokeEditorOnPropertyChangedCallback;
        }
        return nullptr;
    }

    static void NotifyEditorPropertyChangedIfNeeded(T& Typed, const std::string_view FieldName, const bool Changed)
    {
        if (!Changed)
        {
            return;
        }

        if constexpr (std::is_base_of_v<BaseNode, T> || std::is_base_of_v<BaseComponent, T>)
        {
            const TypeId DynamicType = Typed.TypeKey();
            if (const TypeInfo* DynamicInfo = TypeRegistry::Instance().Find(DynamicType))
            {
                if (DynamicInfo->EditorPropertyChanged)
                {
                    DynamicInfo->EditorPropertyChanged(&Typed, FieldName);
                    return;
                }
            }
            return;
        }

        InvokeDeclaredEditorOnPropertyChanged(Typed, FieldName);
    }
#else
    static void NotifyEditorPropertyChangedIfNeeded(T& Typed, const std::string_view FieldName, const bool Changed)
    {
        (void)Typed;
        (void)FieldName;
        (void)Changed;
    }
#endif

    template<typename GetterMethod>
    static TExpected<Variant> BuildGetterVariant(T* Typed, GetterMethod Getter)
    {
        using GetterReturn = typename TGetterTraits<GetterMethod>::ReturnType;
        if constexpr (std::is_reference_v<GetterReturn>)
        {
            if constexpr (std::is_const_v<std::remove_reference_t<GetterReturn>>)
            {
                return Variant::FromConstRef((Typed->*Getter)());
            }
            else
            {
                return Variant::FromRef((Typed->*Getter)());
            }
        }
        else
        {
            return Variant::FromValue((Typed->*Getter)());
        }
    }

    template<typename SetterMethod, typename Raw>
    static Result ApplySetter(T* Typed, SetterMethod Setter, const Variant& Value)
    {
        using SetterArg = typename TSetterTraits<SetterMethod>::ArgType;
        using SetterReturn = typename TSetterTraits<SetterMethod>::ReturnType;
        using SetterRaw = std::remove_cvref_t<SetterArg>;
        using SetterRet = std::remove_cvref_t<SetterReturn>;

        static_assert(std::is_same_v<SetterRaw, Raw>, "Setter parameter type must match reflected field type");
        static_assert(IsSupportedSetterReturnV<SetterReturn>, "Setter must return void, bool, or Result");

        auto Ref = Value.AsConstRef<Raw>();
        if (!Ref)
        {
            return std::unexpected(Ref.error());
        }

        SetterRaw SetterValue = Ref->get();
        if constexpr (std::is_same_v<SetterRet, void>)
        {
            (Typed->*Setter)(static_cast<SetterArg>(SetterValue));
            return Ok();
        }
        else if constexpr (std::is_same_v<SetterRet, bool>)
        {
            if (!(Typed->*Setter)(static_cast<SetterArg>(SetterValue)))
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Setter rejected value"));
            }
            return Ok();
        }
        else
        {
            return (Typed->*Setter)(static_cast<SetterArg>(SetterValue));
        }
    }

    template<typename SetterMethod, typename Raw>
    static Result ApplyRawSetter(T* Typed, SetterMethod Setter, const void* ValuePtr)
    {
        using SetterArg = typename TSetterTraits<SetterMethod>::ArgType;
        using SetterReturn = typename TSetterTraits<SetterMethod>::ReturnType;
        using SetterRaw = std::remove_cvref_t<SetterArg>;
        using SetterRet = std::remove_cvref_t<SetterReturn>;

        static_assert(std::is_same_v<SetterRaw, Raw>, "Setter parameter type must match reflected field type");
        static_assert(IsSupportedSetterReturnV<SetterReturn>, "Setter must return void, bool, or Result");

        if (!ValuePtr)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null setter value"));
        }

        const SetterRaw& TypedValue = *static_cast<const SetterRaw*>(ValuePtr);
        SetterRaw SetterValue = TypedValue;
        if constexpr (std::is_same_v<SetterRet, void>)
        {
            (Typed->*Setter)(static_cast<SetterArg>(SetterValue));
            return Ok();
        }
        else if constexpr (std::is_same_v<SetterRet, bool>)
        {
            if (!(Typed->*Setter)(static_cast<SetterArg>(SetterValue)))
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Setter rejected value"));
            }
            return Ok();
        }
        else
        {
            return (Typed->*Setter)(static_cast<SetterArg>(SetterValue));
        }
    }

public:
    /**
     * @brief Construct a builder for a reflected type name.
     * @param Name Fully qualified stable reflected type name.
     *
     * The builder derives `TypeId` from `Name`, captures `sizeof(T)` / `alignof(T)`, and wires any
     * supported node/editor callback shims into the pending `TypeInfo`.
     */
    explicit TTypeBuilder(const char* Name)
    {
        m_info.Name = Name;
        m_info.Id = TypeIdFromName(Name);
        m_info.Size = sizeof(T);
        m_info.Align = alignof(T);
        m_info.RuntimeOps = &GetTypeRuntimeOps<T>();
        ApplyEditorValueFamilyMetadata<T>(m_info);
        m_info.IsAbstract = std::is_abstract_v<T>;
        m_info.NodeOnCreate = NodeOnCreateInvokerForType();
#if defined(WITH_EDITOR) && WITH_EDITOR
        m_info.EditorPropertyChanged = EditorOnPropertyChangedInvokerForType();
#endif
    }

    /**
     * @brief Register one direct reflected base type.
     * @tparam BaseT Reflected base class type.
     * @return Builder reference for chaining.
     *
     * The base type is lazily ensured in `TypeRegistry` as a side effect. Base relationships are used
     * by:
     * - `TypeRegistry::IsA()` and `Derived()`
     * - inherited field and method collection
     * - reflected type compatibility checks in systems like assets and subclass selection
     */
    template<typename BaseT>
    TTypeBuilder& Base()
    {
        const TypeId BaseId = StaticTypeId<BaseT>();
        (void)TypeRegistry::Instance().Find(BaseId); // triggers lazy ensure on miss
        m_info.BaseTypes.push_back(BaseId);
        m_info.DirectCasts.push_back(TypeCastInfo{
            .TargetType = BaseId,
            .CastMutable = [] (void* Instance) -> void* {
                return static_cast<BaseT*>(static_cast<T*>(Instance));
            },
            .CastConst = [] (const void* Instance) -> const void* {
                return static_cast<const BaseT*>(static_cast<const T*>(Instance));
            },
        });
        return *this;
    }

    /**
     * @brief Register one direct reflected interface/abstract relationship.
     * @tparam InterfaceT Reflected interface type.
     * @return Builder reference for chaining.
     *
     * Interfaces participate in `TypeRegistry::IsA()` and `Derived()` but are stored separately
     * from concrete base inheritance so tooling can distinguish them when needed.
     */
    template<typename InterfaceT>
    TTypeBuilder& Interface()
    {
        static_assert(std::is_abstract_v<InterfaceT>,
                      "Interface<> expects an abstract/interface reflected type");

        const TypeId InterfaceId = StaticTypeId<InterfaceT>();
        (void)TypeRegistry::Instance().Find(InterfaceId); // triggers lazy ensure on miss
        m_info.InterfaceTypes.push_back(InterfaceId);
        m_info.DirectCasts.push_back(TypeCastInfo{
            .TargetType = InterfaceId,
            .CastMutable = [] (void* Instance) -> void* {
                return static_cast<InterfaceT*>(static_cast<T*>(Instance));
            },
            .CastConst = [] (const void* Instance) -> const void* {
                return static_cast<const InterfaceT*>(static_cast<const T*>(Instance));
            },
        });
        return *this;
    }

    /**
     * @brief Mark the reflected type itself as an interface contract.
     * @return Builder reference for chaining.
     */
    TTypeBuilder& AsInterface()
    {
        m_info.IsInterface = true;
        m_info.IsAbstract = true;
        return *this;
    }

    /**
     * @brief Reflect a data member through a pointer-to-member.
     * @tparam FieldT Data-member type.
     * @param Name Stable reflected field name.
     * @param Member Pointer-to-member field.
     * @param Flags Optional field flags.
     * @return Builder reference for chaining.
     *
     * Generated metadata includes:
     * - variant getter/setter access
     * - non-owning `VariantView` access
     * - direct const/mutable pointer accessors for hot paths
     *
     * For non-const fields, reflected writes compare old vs new values when possible and notify the
     * editor property-changed hook only when the value actually changed.
     *
     * @warning Const member fields are reflected as read-only. Attempts to assign through the setter
     * fail at runtime.
     */
    template<typename FieldT>
    requires (!std::is_function_v<FieldT>)
    TTypeBuilder& Field(
        const char* Name,
        FieldT T::*Member,
        FieldFlags Flags = {},
        FieldEditorFlags EditorFlags = {})
    {
        using Raw = std::remove_cv_t<FieldT>;
        FieldInfo Info;
        Info.Name = Name;
        const TypeId FieldType = StaticTypeId<Raw>();
        Info.FieldType = FieldType;
        Info.Flags = Flags;
        Info.EditorFlags = EditorFlags;
        Info.Getter = [Member](void* Instance) -> TExpected<Variant> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            if constexpr (std::is_const_v<FieldT>)
            {
                return Variant::FromConstRef(Typed->*Member);
            }
            else
            {
                return Variant::FromRef(Typed->*Member);
            }
        };
        Info.Setter = [Member, Name](void* Instance, const Variant& Value) -> Result {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            auto Ref = Value.AsConstRef<Raw>();
            if (!Ref)
            {
                return std::unexpected(Ref.error());
            }
            if constexpr (std::is_const_v<FieldT>)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Cannot assign to const field"));
            }
            else
            {
                const Raw& NewValue = Ref->get();
                const bool Changed = !ValuesEqual<Raw>(Typed->*Member, NewValue);
                if (Changed)
                {
                    Typed->*Member = NewValue;
                }
                NotifyEditorPropertyChangedIfNeeded(*Typed, Name, Changed);
                return Ok();
            }
        };
        Info.RawSetter = [Member, Name](void* Instance, const void* ValuePtr) -> Result {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            if (!ValuePtr)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null value"));
            }

            auto* Typed = static_cast<T*>(Instance);
            if constexpr (std::is_const_v<FieldT>)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Cannot assign to const field"));
            }
            else
            {
                const Raw& NewValue = *static_cast<const Raw*>(ValuePtr);
                const bool Changed = !ValuesEqual<Raw>(Typed->*Member, NewValue);
                if (Changed)
                {
                    Typed->*Member = NewValue;
                }
                NotifyEditorPropertyChangedIfNeeded(*Typed, Name, Changed);
                return Ok();
            }
        };
        Info.ViewGetter = [Member, FieldType](void* Instance) -> TExpected<VariantView> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            const auto* Ptr = &(Typed->*Member);
            return VariantView(FieldType, Ptr, std::is_const_v<FieldT>);
        };
        Info.ConstPointer = [Member](const void* Instance) -> const void* {
            if (!Instance)
            {
                return nullptr;
            }
            auto* Typed = static_cast<const T*>(Instance);
            return &(Typed->*Member);
        };
        Info.MutablePointer = [Member](void* Instance) -> void* {
            if (!Instance)
            {
                return nullptr;
            }
            if constexpr (std::is_const_v<FieldT>)
            {
                return nullptr;
            }
            else
            {
                auto* Typed = static_cast<T*>(Instance);
                return &(Typed->*Member);
            }
        };
        Info.IsConst = std::is_const_v<FieldT>;
        m_info.Fields.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Reflect a read-only field through a getter method.
     * @tparam GetterMethod Getter member-function pointer type.
     * @param Name Stable reflected field name.
     * @param Getter Getter method.
     * @param Flags Optional field flags.
     * @return Builder reference for chaining.
     *
     * Getter return may be by value or by reference. Reference-returning getters additionally expose
     * `VariantView` and raw-pointer read access where possible.
     */
    template<typename GetterMethod>
    requires (IsGetterMethodV<GetterMethod>)
    TTypeBuilder& Field(
        const char* Name,
        GetterMethod Getter,
        FieldFlags Flags = {},
        FieldEditorFlags EditorFlags = {})
    {
        using GetterTraits = TGetterTraits<GetterMethod>;
        using GetterReturn = typename GetterTraits::ReturnType;
        using Raw = std::remove_cvref_t<GetterReturn>;

        static_assert(!std::is_void_v<Raw>, "Getter cannot return void");

        FieldInfo Info;
        Info.Name = Name;
        const TypeId FieldType = StaticTypeId<Raw>();
        Info.FieldType = FieldType;
        Info.Flags = Flags;
        Info.EditorFlags = EditorFlags;
        Info.Getter = [Getter](void* Instance) -> TExpected<Variant> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            return BuildGetterVariant(Typed, Getter);
        };
        if constexpr (std::is_reference_v<GetterReturn>)
        {
            Info.ViewGetter = [Getter, FieldType](void* Instance) -> TExpected<VariantView> {
                if (!Instance)
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
                }
                auto* Typed = static_cast<T*>(Instance);
                const auto* Ptr = &((Typed->*Getter)());
                constexpr bool IsConstRef = std::is_const_v<std::remove_reference_t<GetterReturn>>;
                return VariantView(FieldType, Ptr, IsConstRef);
            };
        }
        Info.ConstPointer = [Getter](const void* Instance) -> const void* {
            if (!Instance)
            {
                return nullptr;
            }
            if constexpr (std::is_reference_v<GetterReturn> && GetterTraits::IsConstMethod)
            {
                auto* Typed = static_cast<const T*>(Instance);
                return &((Typed->*Getter)());
            }
            return nullptr;
        };
        Info.IsConst = true;
        m_info.Fields.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Reflect a write-only field through a setter method.
     * @tparam SetterMethod Setter member-function pointer type.
     * @param Name Stable reflected field name.
     * @param Setter Setter method.
     * @param Flags Optional field flags.
     * @return Builder reference for chaining.
     *
     * Supported setter return contracts:
     * - `void`: assignment always succeeds
     * - `bool`: `false` is treated as a rejected value
     * - `Result`: full error propagation
     */
    template<typename SetterMethod>
    requires (IsSetterMethodV<SetterMethod>)
    TTypeBuilder& Field(
        const char* Name,
        SetterMethod Setter,
        FieldFlags Flags = {},
        FieldEditorFlags EditorFlags = {})
    {
        using SetterTraits = TSetterTraits<SetterMethod>;
        using SetterArg = typename SetterTraits::ArgType;
        using SetterReturn = typename SetterTraits::ReturnType;
        using Raw = std::remove_cvref_t<SetterArg>;

        static_assert(!std::is_void_v<Raw>, "Setter argument cannot be void");
        static_assert(IsSupportedSetterReturnV<SetterReturn>, "Setter must return void, bool, or Result");

        FieldInfo Info;
        Info.Name = Name;
        Info.FieldType = StaticTypeId<Raw>();
        Info.Flags = Flags;
        Info.EditorFlags = EditorFlags;
        Info.Getter = [](void*) -> TExpected<Variant> {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Field has no getter"));
        };
        Info.Setter = [Setter](void* Instance, const Variant& Value) -> Result {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            return ApplySetter<SetterMethod, Raw>(Typed, Setter, Value);
        };
        Info.RawSetter = [Setter](void* Instance, const void* ValuePtr) -> Result {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            return ApplyRawSetter<SetterMethod, Raw>(Typed, Setter, ValuePtr);
        };
        Info.IsConst = false;
        m_info.Fields.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Reflect a read-write field through getter and setter methods.
     * @tparam GetterMethod Getter member-function pointer type.
     * @tparam SetterMethod Setter member-function pointer type.
     * @param Name Stable reflected field name.
     * @param Getter Getter method.
     * @param Setter Setter method.
     * @param Flags Optional field flags.
     * @return Builder reference for chaining.
     *
     * When the getter return type is copy-constructible and equality comparable, reflected writes
     * compare pre- and post-set values so editor property-change notifications fire only on real changes.
     */
    template<typename GetterMethod, typename SetterMethod>
    requires (IsGetterMethodV<GetterMethod> && IsSetterMethodV<SetterMethod>)
    TTypeBuilder& Field(
        const char* Name,
        GetterMethod Getter,
        SetterMethod Setter,
        FieldFlags Flags = {},
        FieldEditorFlags EditorFlags = {})
    {
        using GetterTraits = TGetterTraits<GetterMethod>;
        using GetterReturn = typename GetterTraits::ReturnType;
        using SetterTraits = TSetterTraits<SetterMethod>;
        using SetterArg = typename SetterTraits::ArgType;
        using SetterReturn = typename SetterTraits::ReturnType;
        using Raw = std::remove_cvref_t<GetterReturn>;
        using SetterRaw = std::remove_cvref_t<SetterArg>;

        static_assert(!std::is_void_v<Raw>, "Getter cannot return void");
        static_assert(std::is_same_v<SetterRaw, Raw>, "Setter parameter type must match getter field type");
        static_assert(IsSupportedSetterReturnV<SetterReturn>, "Setter must return void, bool, or Result");

        FieldInfo Info;
        Info.Name = Name;
        const TypeId FieldType = StaticTypeId<Raw>();
        Info.FieldType = FieldType;
        Info.Flags = Flags;
        Info.EditorFlags = EditorFlags;
        Info.Getter = [Getter](void* Instance) -> TExpected<Variant> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            return BuildGetterVariant(Typed, Getter);
        };
        Info.Setter = [Getter, Setter, Name](void* Instance, const Variant& Value) -> Result {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            std::optional<Raw> BeforeValue{};
            if constexpr (std::copy_constructible<Raw>)
            {
                BeforeValue.emplace(static_cast<Raw>((Typed->*Getter)()));
            }

            auto SetResult = ApplySetter<SetterMethod, Raw>(Typed, Setter, Value);
            if (!SetResult)
            {
                return std::unexpected(SetResult.error());
            }

            bool Changed = true;
            if constexpr (std::copy_constructible<Raw>)
            {
                if constexpr (IsEqualityComparableV<Raw>)
                {
                    const Raw AfterValue = static_cast<Raw>((Typed->*Getter)());
                    Changed = !BeforeValue.has_value() || !ValuesEqual(*BeforeValue, AfterValue);
                }
            }

            NotifyEditorPropertyChangedIfNeeded(*Typed, Name, Changed);
            return Ok();
        };
        Info.RawSetter = [Getter, Setter, Name](void* Instance, const void* ValuePtr) -> Result {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            if (!ValuePtr)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null value"));
            }

            auto* Typed = static_cast<T*>(Instance);
            std::optional<Raw> BeforeValue{};
            if constexpr (std::copy_constructible<Raw>)
            {
                BeforeValue.emplace(static_cast<Raw>((Typed->*Getter)()));
            }

            auto SetResult = ApplyRawSetter<SetterMethod, Raw>(Typed, Setter, ValuePtr);
            if (!SetResult)
            {
                return std::unexpected(SetResult.error());
            }

            bool Changed = true;
            if constexpr (std::copy_constructible<Raw>)
            {
                if constexpr (IsEqualityComparableV<Raw>)
                {
                    const Raw AfterValue = static_cast<Raw>((Typed->*Getter)());
                    Changed = !BeforeValue.has_value() || !ValuesEqual(*BeforeValue, AfterValue);
                }
            }

            NotifyEditorPropertyChangedIfNeeded(*Typed, Name, Changed);
            return Ok();
        };
        if constexpr (std::is_reference_v<GetterReturn>)
        {
            Info.ViewGetter = [Getter, FieldType](void* Instance) -> TExpected<VariantView> {
                if (!Instance)
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
                }
                auto* Typed = static_cast<T*>(Instance);
                const auto* Ptr = &((Typed->*Getter)());
                constexpr bool IsConstRef = std::is_const_v<std::remove_reference_t<GetterReturn>>;
                return VariantView(FieldType, Ptr, IsConstRef);
            };
        }
        Info.ConstPointer = [Getter](const void* Instance) -> const void* {
            if (!Instance)
            {
                return nullptr;
            }
            if constexpr (std::is_reference_v<GetterReturn> && GetterTraits::IsConstMethod)
            {
                auto* Typed = static_cast<const T*>(Instance);
                return &((Typed->*Getter)());
            }
            return nullptr;
        };
        Info.MutablePointer = [Getter](void* Instance) -> void* {
            if (!Instance)
            {
                return nullptr;
            }
            if constexpr (std::is_reference_v<GetterReturn>)
            {
                auto* Typed = static_cast<T*>(Instance);
                using RefBase = std::remove_reference_t<GetterReturn>;
                if constexpr (std::is_const_v<RefBase>)
                {
                    const auto* Ptr = &((Typed->*Getter)());
                    return const_cast<std::remove_const_t<RefBase>*>(Ptr);
                }
                else
                {
                    return &((Typed->*Getter)());
                }
            }
            return nullptr;
        };
        Info.IsConst = false;
        m_info.Fields.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Overload bridge for getter/setter pairs that share the same member name.
     *
     * This allows declarations such as `Field("Name", &Type::Name, &Type::Name)` where overload
     * resolution would otherwise be ambiguous.
     */
    template<typename GetterReturn, typename SetterArg, typename SetterReturn>
    TTypeBuilder& Field(
        const char* Name,
        GetterReturn (T::*Getter)(),
        SetterReturn (T::*Setter)(SetterArg),
        FieldFlags Flags = {},
        FieldEditorFlags EditorFlags = {})
    {
        return Field<decltype(Getter), decltype(Setter)>(Name, Getter, Setter, Flags, EditorFlags);
    }

    template<typename GetterReturn, typename SetterArg, typename SetterReturn>
    TTypeBuilder& Field(
        const char* Name,
        GetterReturn (T::*Getter)() const,
        SetterReturn (T::*Setter)(SetterArg),
        FieldFlags Flags = {},
        FieldEditorFlags EditorFlags = {})
    {
        return Field<decltype(Getter), decltype(Setter)>(Name, Getter, Setter, Flags, EditorFlags);
    }

    /**
     * @brief Reflect a read-write field through an editable-reference accessor and a const getter.
     * @param Name Stable reflected field name.
     * @param Getter Mutable accessor that returns the stored field by non-const reference.
     * @param GetterConst Read-only accessor that returns the same field by const reference.
     * @param Flags Optional field flags.
     * @return Builder reference for chaining.
     *
     * This bridge exists for the engine's common `EditX()/GetX()` pattern, where a type exposes a
     * mutable reference for in-place editor/gameplay mutation and a separate const accessor for
     * read-only reflection and serialization.
     *
     * Semantics:
     * - Reflected reads use `Getter`.
     * - Reflected writes assign through the reference returned by `Getter`.
     * - Const pointer/view access uses `GetterConst`.
     * - Editor property-change notifications are emitted only when the assigned value actually changes.
     */
    template<typename FieldT>
    TTypeBuilder& Field(
        const char* Name,
        FieldT& (T::*Getter)(),
        const FieldT& (T::*GetterConst)() const,
        FieldFlags Flags = {},
        FieldEditorFlags EditorFlags = {})
    {
        using Raw = std::remove_cv_t<FieldT>;
        FieldInfo Info;
        Info.Name = Name;
        const TypeId FieldType = StaticTypeId<Raw>();
        Info.FieldType = FieldType;
        Info.Flags = Flags;
        Info.EditorFlags = EditorFlags;
        Info.Getter = [Getter](void* Instance) -> TExpected<Variant> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            return Variant::FromRef((Typed->*Getter)());
        };
        Info.Setter = [Getter, Name](void* Instance, const Variant& Value) -> Result {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            auto Ref = Value.AsConstRef<Raw>();
            if (!Ref)
            {
                return std::unexpected(Ref.error());
            }
            Raw& FieldRef = (Typed->*Getter)();
            const Raw& NewValue = Ref->get();
            const bool Changed = !ValuesEqual<Raw>(FieldRef, NewValue);
            if (Changed)
            {
                FieldRef = NewValue;
            }
            NotifyEditorPropertyChangedIfNeeded(*Typed, Name, Changed);
            return Ok();
        };
        Info.RawSetter = [Getter, Name](void* Instance, const void* ValuePtr) -> Result {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            if (!ValuePtr)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null value"));
            }

            auto* Typed = static_cast<T*>(Instance);
            Raw& FieldRef = (Typed->*Getter)();
            const Raw& NewValue = *static_cast<const Raw*>(ValuePtr);
            const bool Changed = !ValuesEqual<Raw>(FieldRef, NewValue);
            if (Changed)
            {
                FieldRef = NewValue;
            }
            NotifyEditorPropertyChangedIfNeeded(*Typed, Name, Changed);
            return Ok();
        };
        Info.ViewGetter = [Getter, FieldType](void* Instance) -> TExpected<VariantView> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            const auto* Ptr = &((Typed->*Getter)());
            return VariantView(FieldType, Ptr, false);
        };
        Info.ConstPointer = [GetterConst](const void* Instance) -> const void* {
            if (!Instance)
            {
                return nullptr;
            }
            auto* Typed = static_cast<const T*>(Instance);
            return &((Typed->*GetterConst)());
        };
        Info.MutablePointer = [Getter](void* Instance) -> void* {
            if (!Instance)
            {
                return nullptr;
            }
            auto* Typed = static_cast<T*>(Instance);
            return &((Typed->*Getter)());
        };
        Info.IsConst = false;
        m_info.Fields.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Reflect a non-const method.
     * @tparam R Return type.
     * @tparam Args Parameter pack.
     * @param Name Stable reflected method name.
     * @param Method Pointer to member function.
     * @param Flags Optional method flags.
     * @return Builder reference for chaining.
     *
     * Invocation is bridged through `MakeInvoker()` so callers can use type-erased `Variant` argument packs.
     */
    template<typename R, typename... Args>
    TTypeBuilder& Method(const char* Name, R(T::*Method)(Args...), MethodFlags Flags = {})
    {
        MethodInfo Info;
        Info.Name = Name;
        Info.ReturnType = ReflectedMethodReturnTypeId<R>();
        Info.ParamTypes = {StaticTypeId<std::remove_cvref_t<Args>>() ...};
        Info.ParamPassKinds = {detail::MethodParamPassKindV<Args> ...};
        Info.Params = {CallableParamInfo{StaticTypeId<std::remove_cvref_t<Args>>(), detail::MethodParamPassKindV<Args>, {}, {}} ...};
        Info.Invoke = MakeInvoker(Method);
        const auto RawBinding = MakeRawInvoker(Method);
        Info.RawInvoke = RawBinding.Invoke;
        Info.RawInvokeUserData = RawBinding.UserData;
        Info.IsConst = false;
        Info.Flags = Flags;
        m_info.Methods.push_back(std::move(Info));
        return *this;
    }

    template<typename R, typename... Args>
    TTypeBuilder& Method(const char* Name, R(T::*Method)(Args...) noexcept, MethodFlags Flags = {})
    {
        MethodInfo Info;
        Info.Name = Name;
        Info.ReturnType = ReflectedMethodReturnTypeId<R>();
        Info.ParamTypes = {StaticTypeId<std::remove_cvref_t<Args>>() ...};
        Info.ParamPassKinds = {detail::MethodParamPassKindV<Args> ...};
        Info.Params = {CallableParamInfo{StaticTypeId<std::remove_cvref_t<Args>>(), detail::MethodParamPassKindV<Args>, {}, {}} ...};
        Info.Invoke = MakeInvoker(Method);
        const auto RawBinding = MakeRawInvoker(Method);
        Info.RawInvoke = RawBinding.Invoke;
        Info.RawInvokeUserData = RawBinding.UserData;
        Info.IsConst = false;
        Info.Flags = Flags;
        m_info.Methods.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Reflect a const method.
     * @tparam R Return type.
     * @tparam Args Parameter pack.
     * @param Name Stable reflected method name.
     * @param Method Pointer to const member function.
     * @param Flags Optional method flags.
     * @return Builder reference for chaining.
     *
     * Constness is stored in metadata and exposed to callers through `MethodInfo::IsConst`.
     */
    template<typename R, typename... Args>
    TTypeBuilder& Method(const char* Name, R(T::*Method)(Args...) const, MethodFlags Flags = {})
    {
        MethodInfo Info;
        Info.Name = Name;
        Info.ReturnType = ReflectedMethodReturnTypeId<R>();
        Info.ParamTypes = {StaticTypeId<std::remove_cvref_t<Args>>() ...};
        Info.ParamPassKinds = {detail::MethodParamPassKindV<Args> ...};
        Info.Params = {CallableParamInfo{StaticTypeId<std::remove_cvref_t<Args>>(), detail::MethodParamPassKindV<Args>, {}, {}} ...};
        Info.Invoke = MakeInvoker(Method);
        const auto RawBinding = MakeRawInvoker(Method);
        Info.RawInvoke = RawBinding.Invoke;
        Info.RawInvokeUserData = RawBinding.UserData;
        Info.IsConst = true;
        Info.Flags = Flags;
        m_info.Methods.push_back(std::move(Info));
        return *this;
    }

    template<typename R, typename... Args>
    TTypeBuilder& Method(const char* Name, R(T::*Method)(Args...) const noexcept, MethodFlags Flags = {})
    {
        MethodInfo Info;
        Info.Name = Name;
        Info.ReturnType = ReflectedMethodReturnTypeId<R>();
        Info.ParamTypes = {StaticTypeId<std::remove_cvref_t<Args>>() ...};
        Info.ParamPassKinds = {detail::MethodParamPassKindV<Args> ...};
        Info.Params = {CallableParamInfo{StaticTypeId<std::remove_cvref_t<Args>>(), detail::MethodParamPassKindV<Args>, {}, {}} ...};
        Info.Invoke = MakeInvoker(Method);
        const auto RawBinding = MakeRawInvoker(Method);
        Info.RawInvoke = RawBinding.Invoke;
        Info.RawInvokeUserData = RawBinding.UserData;
        Info.IsConst = true;
        Info.Flags = Flags;
        m_info.Methods.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Reflect a constructor signature.
     * @tparam Args Constructor argument types.
     * @return Builder reference for chaining.
     *
     * Constructor metadata powers runtime creation by `TypeId` in systems such as serialization,
     * script binding, editor creation flows, and component registration helpers.
     */
    template<typename... Args>
    TTypeBuilder& Constructor()
    {
        static_assert(!std::is_abstract_v<T>, "Cannot register constructors for abstract/interface reflected types");
        static_assert(std::is_constructible_v<T, Args...>, "Requested reflected constructor is not constructible");

        ConstructorInfo Info;
        Info.ParamTypes = {StaticTypeId<std::remove_cvref_t<Args>>() ...};
        Info.Params = {CallableParamInfo{StaticTypeId<std::remove_cvref_t<Args>>(), detail::MethodParamPassKindV<Args>, {}, {}} ...};
        Info.Construct = [](std::span<const Variant> ArgsPack) -> TExpected<std::shared_ptr<void>> {
            if (ArgsPack.size() != sizeof...(Args))
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Argument count mismatch"));
            }
            return ConstructImpl<Args...>(ArgsPack, std::index_sequence_for<Args...>{});
        };
        m_info.Constructors.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Commit the accumulated `TypeInfo` into the global `TypeRegistry`.
     * @return Pointer to the stored `TypeInfo` or an error.
     *
     * Additional side effects:
     * - if `T` derives from `BaseComponent`, `ComponentSerializationRegistry::Register<T>()` is attempted
     * - node `OnCreate` and editor property-changed callback shims are already embedded in the metadata
     *
     * @warning The builder should be consumed only once. `Register()` moves out of the accumulated metadata.
     */
    TExpected<TypeInfo*> Register()
    {
        if constexpr (std::is_base_of_v<BaseNode, T>)
        {
            static_assert(DenseRuntimeNodeType<T>,
                          "Reflected ECS node types must inherit NodeCRTP<Derived>, be move-only, and be noexcept movable");
        }
        if constexpr (std::is_base_of_v<BaseComponent, T> && !std::is_same_v<T, BaseComponent>)
        {
            static_assert(DenseRuntimeComponentType<T>,
                          "Reflected ECS component types must inherit ComponentCRTP<Derived>, be move-only, and be noexcept movable");
        }

        auto Result = TypeRegistry::Instance().Register(std::move(m_info));
        if constexpr (std::is_base_of_v<BaseNode, T>)
        {
            NodeStorageFactoryRegistry::Instance().Register<T>();
        }
        if constexpr (std::is_base_of_v<BaseComponent, T> && !std::is_same_v<T, BaseComponent>)
        {
            ComponentSerializationRegistry::Instance().Register<T>();
        }
        return Result;
    }

private:
    /**
     * @brief Construct an instance from a Variant argument pack.
     * @tparam Args Constructor argument types.
     * @param ArgsPack Argument span.
     * @return Shared pointer to the constructed object.
     * @remarks Performs runtime type extraction and conversion.
     */
    template<typename... Args, size_t... I>
    static TExpected<std::shared_ptr<void>> ConstructImpl(std::span<const Variant> ArgsPack, std::index_sequence<I...>)
    {
        std::tuple<std::optional<typename detail::TArgStorage<Args>::Type>...> Extracted;
        Error ErrorValue;
        bool OkFlag = true;
        (([&] {
            auto Result = detail::ExtractArg<Args>(ArgsPack[I]);
            if (!Result)
            {
                OkFlag = false;
                ErrorValue = Result.error();
                return;
            }
            std::get<I>(Extracted) = Result.value();
        }()), ...);

        if (!OkFlag)
        {
            return std::unexpected(ErrorValue);
        }

        auto Ptr = std::make_shared<T>(detail::ConvertArg<Args>(*std::get<I>(Extracted))...);
        return std::static_pointer_cast<void>(Ptr);
    }

    TypeInfo m_info{}; /**< @brief Accumulated type metadata. */
};

} // namespace SnAPI::GameFramework
