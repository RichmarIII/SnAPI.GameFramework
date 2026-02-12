#pragma once

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "IComponent.h"
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
 * @remarks Use in a single translation unit per type.
 */
template<typename T>
class TTypeBuilder
{
public:
    static_assert(std::is_class_v<T> || std::is_union_v<T>, "TTypeBuilder requires class/struct types");
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
     * @remarks Base types are used for inheritance queries and serialization.
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
     * @remarks Const fields are treated as read-only (setter fails).
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
     * @brief Register a field via accessors that return references.
     * @tparam FieldT Field type.
     * @param Name Field name.
     * @param Getter Mutable accessor returning FieldT&.
     * @param GetterConst Const accessor returning const FieldT&.
     * @return Reference to the builder for chaining.
     * @remarks Use this to reflect private/protected storage via public accessors.
     */
    template<typename FieldT>
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
     * @remarks Constructors are used by serialization and scripting.
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
     * @remarks Fails if the type is already registered.
     */
    TExpected<TypeInfo*> Register()
    {
        auto Result = TypeRegistry::Instance().Register(std::move(m_info));
        if constexpr (std::is_base_of_v<IComponent, T>)
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
