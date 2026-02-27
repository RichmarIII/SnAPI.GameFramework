#pragma once

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "BaseComponent.h"
#include "Serialization.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "TypeRegistry.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Fluent builder for registering reflection metadata.
 * @tparam T Type to register.
 * @remarks
 * Builder collects full reflected metadata for a type and commits into `TypeRegistry`.
 *
 * Best-practice lifecycle:
 * 1. define fields/methods/constructors/base types
 * 2. call `Register()` once in one translation unit (typically through `SNAPI_REFLECT_TYPE`)
 * 3. let `TypeAutoRegistry` ensure-on-first-use resolve registration at runtime
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

public:
    /**
     * @brief Construct a builder for a type name.
     * @param Name Fully qualified type name.
     * @remarks The TypeId is derived from Name via TypeIdFromName.
     */
    explicit TTypeBuilder(const char* Name)
    {
        m_info.Name = Name;
        m_info.Id = TypeIdFromName(Name);
        m_info.Size = sizeof(T);
        m_info.Align = alignof(T);
    }

    /**
     * @brief Register a base type.
     * @tparam BaseT Base class type.
     * @return Reference to the builder for chaining.
     * @remarks
     * Base metadata is used for:
     * - `TypeRegistry::IsA`/`Derived`
     * - inherited field/method traversal in serialization/replication/RPC lookup.
     */
    template<typename BaseT>
    TTypeBuilder& Base()
    {
        const TypeId BaseId = StaticTypeId<BaseT>();
        (void)TypeRegistry::Instance().Find(BaseId); // triggers lazy ensure on miss
        m_info.BaseTypes.push_back(BaseId);
        return *this;
    }

    /**
     * @brief Register a field with getter/setter support.
     * @tparam FieldT Field type.
     * @param Name Field name.
     * @param Member Pointer-to-member field.
     * @return Reference to the builder for chaining.
     * @remarks
     * Member-pointer registration emits:
     * - Variant getter/setter
     * - direct pointer accessors
     * - optional field flags (e.g., replication)
     *
     * Const member fields are reflected as read-only; setter returns error at runtime.
     */
    template<typename FieldT>
    TTypeBuilder& Field(const char* Name, FieldT T::*Member, FieldFlags Flags = {})
    {
        using Raw = std::remove_cv_t<FieldT>;
        FieldInfo Info;
        Info.Name = Name;
        const TypeId FieldType = StaticTypeId<Raw>();
        Info.FieldType = FieldType;
        Info.Flags = Flags;
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
        Info.Setter = [Member](void* Instance, const Variant& Value) -> Result {
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
                Typed->*Member = Ref->get();
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
     * @brief Register a read-only field from a getter method.
     * @tparam GetterMethod Getter member-function pointer type.
     * @param Name Field name.
     * @param Getter Getter method (`T::GetX()` or `T::GetX() const`).
     * @return Reference to the builder for chaining.
     * @remarks
     * Getter return can be by value, reference, or const reference.
     */
    template<typename GetterMethod>
    requires (IsGetterMethodV<GetterMethod>)
    TTypeBuilder& Field(const char* Name, GetterMethod Getter, FieldFlags Flags = {})
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
     * @brief Register a write-only field from a setter method.
     * @tparam SetterMethod Setter member-function pointer type.
     * @param Name Field name.
     * @param Setter Setter method (`T::SetX(value)`).
     * @return Reference to the builder for chaining.
     * @remarks
     * Setter parameter can be value/reference/const-reference.
     * Setter return can be void, bool, or Result.
     */
    template<typename SetterMethod>
    requires (IsSetterMethodV<SetterMethod>)
    TTypeBuilder& Field(const char* Name, SetterMethod Setter, FieldFlags Flags = {})
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
        Info.IsConst = false;
        m_info.Fields.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Register a readable/writable field using getter + setter methods.
     * @tparam GetterMethod Getter member-function pointer type.
     * @tparam SetterMethod Setter member-function pointer type.
     * @param Name Field name.
     * @param Getter Getter method (`T::GetX()` or `T::GetX() const`).
     * @param Setter Setter method (`T::SetX(value)`).
     * @return Reference to the builder for chaining.
     * @remarks
     * Getter return can be value/reference/const-reference.
     * Setter parameter can be value/reference/const-reference.
     */
    template<typename GetterMethod, typename SetterMethod>
    requires (IsGetterMethodV<GetterMethod> && IsSetterMethodV<SetterMethod>)
    TTypeBuilder& Field(const char* Name, GetterMethod Getter, SetterMethod Setter, FieldFlags Flags = {})
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
        Info.Getter = [Getter](void* Instance) -> TExpected<Variant> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            return BuildGetterVariant(Typed, Getter);
        };
        Info.Setter = [Setter](void* Instance, const Variant& Value) -> Result {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            return ApplySetter<SetterMethod, Raw>(Typed, Setter, Value);
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
     * @brief Typed bridge for overloaded method names in getter+setter registration.
     * @remarks
     * Enables calls such as:
     * `Field("Name", &Type::Name, &Type::Name)` when one overload is a const getter
     * and another overload is a single-parameter setter.
     */
    template<typename GetterReturn, typename SetterArg, typename SetterReturn>
    TTypeBuilder& Field(
        const char* Name,
        GetterReturn (T::*Getter)(),
        SetterReturn (T::*Setter)(SetterArg),
        FieldFlags Flags = {})
    {
        return Field<decltype(Getter), decltype(Setter)>(Name, Getter, Setter, Flags);
    }

    template<typename GetterReturn, typename SetterArg, typename SetterReturn>
    TTypeBuilder& Field(
        const char* Name,
        GetterReturn (T::*Getter)() const,
        SetterReturn (T::*Setter)(SetterArg),
        FieldFlags Flags = {})
    {
        return Field<decltype(Getter), decltype(Setter)>(Name, Getter, Setter, Flags);
    }

    /**
     * @brief Legacy accessor registration overload.
     * @deprecated Use Field(Name, Getter), Field(Name, Setter), or Field(Name, Getter, Setter).
     */
    template<typename FieldT>
    [[deprecated("Use Field(Name, Getter), Field(Name, Setter), or Field(Name, Getter, Setter).")]]
    TTypeBuilder& Field(
        const char* Name,
        FieldT& (T::*Getter)(),
        const FieldT& (T::*GetterConst)() const,
        FieldFlags Flags = {})
    {
        using Raw = std::remove_cv_t<FieldT>;
        FieldInfo Info;
        Info.Name = Name;
        const TypeId FieldType = StaticTypeId<Raw>();
        Info.FieldType = FieldType;
        Info.Flags = Flags;
        Info.Getter = [Getter](void* Instance) -> TExpected<Variant> {
            if (!Instance)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
            }
            auto* Typed = static_cast<T*>(Instance);
            return Variant::FromRef((Typed->*Getter)());
        };
        Info.Setter = [Getter](void* Instance, const Variant& Value) -> Result {
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
            (Typed->*Getter)() = Ref->get();
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
     * @brief Register a non-const method for reflection.
     * @tparam R Return type.
     * @tparam Args Parameter pack.
     * @param Name Method name.
     * @param Method Pointer to member function.
     * @return Reference to the builder for chaining.
     * @remarks Method flags can mark RPC intent and reliability semantics.
     */
    template<typename R, typename... Args>
    TTypeBuilder& Method(const char* Name, R(T::*Method)(Args...), MethodFlags Flags = {})
    {
        MethodInfo Info;
        Info.Name = Name;
        if constexpr (std::is_void_v<R>)
        {
            Info.ReturnType = StaticTypeId<void>();
        }
        else
        {
            Info.ReturnType = StaticTypeId<std::remove_cvref_t<R>>();
        }
        Info.ParamTypes = {StaticTypeId<std::remove_cvref_t<Args>>() ...};
        Info.Invoke = MakeInvoker(Method);
        Info.IsConst = false;
        Info.Flags = Flags;
        m_info.Methods.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Register a const method for reflection.
     * @tparam R Return type.
     * @tparam Args Parameter pack.
     * @param Name Method name.
     * @param Method Pointer to const member function.
     * @return Reference to the builder for chaining.
     * @remarks Constness is encoded in metadata and enforced through invoker binding.
     */
    template<typename R, typename... Args>
    TTypeBuilder& Method(const char* Name, R(T::*Method)(Args...) const, MethodFlags Flags = {})
    {
        MethodInfo Info;
        Info.Name = Name;
        if constexpr (std::is_void_v<R>)
        {
            Info.ReturnType = StaticTypeId<void>();
        }
        else
        {
            Info.ReturnType = StaticTypeId<std::remove_cvref_t<R>>();
        }
        Info.ParamTypes = {StaticTypeId<std::remove_cvref_t<Args>>() ...};
        Info.Invoke = MakeInvoker(Method);
        Info.IsConst = true;
        Info.Flags = Flags;
        m_info.Methods.push_back(std::move(Info));
        return *this;
    }

    /**
     * @brief Register a constructor signature.
     * @tparam Args Constructor argument types.
     * @return Reference to the builder for chaining.
     * @remarks
     * Constructor metadata powers runtime creation by type id (serialization spawn paths,
     * script/runtime factories, replication instantiation).
     */
    template<typename... Args>
    TTypeBuilder& Constructor()
    {
        ConstructorInfo Info;
        Info.ParamTypes = {StaticTypeId<std::remove_cvref_t<Args>>() ...};
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
     * @brief Register the built TypeInfo into the global TypeRegistry.
     * @return Pointer to stored TypeInfo or error.
     * @remarks
     * If `T` derives from `BaseComponent`, this also auto-registers component serialization
     * in `ComponentSerializationRegistry`.
     */
    TExpected<TypeInfo*> Register()
    {
        auto Result = TypeRegistry::Instance().Register(std::move(m_info));
        if constexpr (std::is_base_of_v<BaseComponent, T>)
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
