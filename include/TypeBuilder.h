#pragma once

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "TypeName.h"
#include "TypeRegistry.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

template<typename T>
class TTypeBuilder
{
public:
    static_assert(std::is_class_v<T> || std::is_union_v<T>, "TTypeBuilder requires class/struct types");
    explicit TTypeBuilder(const char* Name)
    {
        m_info.Name = Name;
        m_info.Id = TypeIdFromName(Name);
        m_info.Size = sizeof(T);
        m_info.Align = alignof(T);
    }

    template<typename BaseT>
    TTypeBuilder& Base()
    {
        m_info.BaseTypes.push_back(TypeIdFromName(TTypeNameV<BaseT>));
        return *this;
    }

    template<typename FieldT>
    TTypeBuilder& Field(const char* Name, FieldT T::*Member)
    {
        using Raw = std::remove_cv_t<FieldT>;
        FieldInfo Info;
        Info.Name = Name;
        Info.FieldType = TypeIdFromName(TTypeNameV<Raw>);
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
        m_info.Fields.push_back(std::move(Info));
        return *this;
    }

    template<typename R, typename... Args>
    TTypeBuilder& Method(const char* Name, R(T::*Method)(Args...))
    {
        MethodInfo Info;
        Info.Name = Name;
        if constexpr (std::is_void_v<R>)
        {
            Info.ReturnType = TypeIdFromName("void");
        }
        else
        {
            Info.ReturnType = TypeIdFromName(TTypeNameV<std::remove_cvref_t<R>>);
        }
        Info.ParamTypes = {TypeIdFromName(TTypeNameV<std::remove_cvref_t<Args>>) ...};
        Info.Invoke = MakeInvoker(Method);
        Info.IsConst = false;
        m_info.Methods.push_back(std::move(Info));
        return *this;
    }

    template<typename R, typename... Args>
    TTypeBuilder& Method(const char* Name, R(T::*Method)(Args...) const)
    {
        MethodInfo Info;
        Info.Name = Name;
        if constexpr (std::is_void_v<R>)
        {
            Info.ReturnType = TypeIdFromName("void");
        }
        else
        {
            Info.ReturnType = TypeIdFromName(TTypeNameV<std::remove_cvref_t<R>>);
        }
        Info.ParamTypes = {TypeIdFromName(TTypeNameV<std::remove_cvref_t<Args>>) ...};
        Info.Invoke = MakeInvoker(Method);
        Info.IsConst = true;
        m_info.Methods.push_back(std::move(Info));
        return *this;
    }

    template<typename... Args>
    TTypeBuilder& Constructor()
    {
        ConstructorInfo Info;
        Info.ParamTypes = {TypeIdFromName(TTypeNameV<std::remove_cvref_t<Args>>) ...};
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

    TExpected<TypeInfo*> Register()
    {
        return TypeRegistry::Instance().Register(std::move(m_info));
    }

private:
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

    TypeInfo m_info{};
};

} // namespace SnAPI::GameFramework
