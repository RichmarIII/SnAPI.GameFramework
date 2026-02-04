#pragma once

#include <functional>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>

#include "Expected.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

using MethodInvoker = std::function<TExpected<Variant>(void* Instance, std::span<const Variant> Args)>;

namespace detail
{

template<typename Arg>
struct TArgStorage
{
    using Raw = std::remove_reference_t<Arg>;
    using Type = std::conditional_t<std::is_lvalue_reference_v<Arg>, std::reference_wrapper<Raw>, std::remove_cvref_t<Arg>>;
};

template<typename Arg>
using TArgStorageT = typename TArgStorage<Arg>::Type;

template<typename Arg>
TExpected<TArgStorageT<Arg>> ExtractArg(const Variant& Value)
{
    using Raw = std::remove_reference_t<Arg>;
    if constexpr (std::is_lvalue_reference_v<Arg>)
    {
        if constexpr (std::is_const_v<Raw>)
        {
            return Value.AsConstRef<Raw>();
        }
        else
        {
            return Value.AsRef<Raw>();
        }
    }
    else
    {
        auto Ref = Value.AsConstRef<Raw>();
        if (!Ref)
        {
            return std::unexpected(Ref.error());
        }
        return Ref->get();
    }
}

template<typename Arg>
Arg ConvertArg(TArgStorageT<Arg>& Storage)
{
    if constexpr (std::is_lvalue_reference_v<Arg>)
    {
        return Storage.get();
    }
    else
    {
        return Storage;
    }
}

template<typename T, typename R, typename... Args, size_t... I>
TExpected<Variant> InvokeImpl(T* Instance, R(T::*Method)(Args...), std::span<const Variant> ArgsPack, std::index_sequence<I...>)
{
    std::tuple<std::optional<TArgStorageT<Args>>...> Extracted;
    Error ErrorValue;
    bool Ok = true;
    (([&] {
        auto Result = ExtractArg<Args>(ArgsPack[I]);
        if (!Result)
        {
            Ok = false;
            ErrorValue = Result.error();
            return;
        }
        std::get<I>(Extracted) = Result.value();
    }()), ...);

    if (!Ok)
    {
        return std::unexpected(ErrorValue);
    }

    if constexpr (std::is_void_v<R>)
    {
        (Instance->*Method)(ConvertArg<Args>(*std::get<I>(Extracted))...);
        return Variant::Void();
    }
    else
    {
        R Result = (Instance->*Method)(ConvertArg<Args>(*std::get<I>(Extracted))...);
        return Variant::FromValue(std::move(Result));
    }
}

template<typename T, typename R, typename... Args, size_t... I>
TExpected<Variant> InvokeConstImpl(const T* Instance, R(T::*Method)(Args...) const, std::span<const Variant> ArgsPack, std::index_sequence<I...>)
{
    std::tuple<std::optional<TArgStorageT<Args>>...> Extracted;
    Error ErrorValue;
    bool Ok = true;
    (([&] {
        auto Result = ExtractArg<Args>(ArgsPack[I]);
        if (!Result)
        {
            Ok = false;
            ErrorValue = Result.error();
            return;
        }
        std::get<I>(Extracted) = Result.value();
    }()), ...);

    if (!Ok)
    {
        return std::unexpected(ErrorValue);
    }

    if constexpr (std::is_void_v<R>)
    {
        (Instance->*Method)(ConvertArg<Args>(*std::get<I>(Extracted))...);
        return Variant::Void();
    }
    else
    {
        R Result = (Instance->*Method)(ConvertArg<Args>(*std::get<I>(Extracted))...);
        return Variant::FromValue(std::move(Result));
    }
}

} // namespace detail

template<typename T, typename R, typename... Args>
MethodInvoker MakeInvoker(R(T::*Method)(Args...))
{
    return [Method](void* Instance, std::span<const Variant> ArgsPack) -> TExpected<Variant> {
        if (!Instance)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
        }
        if (ArgsPack.size() != sizeof...(Args))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Argument count mismatch"));
        }
        return detail::InvokeImpl(static_cast<T*>(Instance), Method, ArgsPack, std::index_sequence_for<Args...>{});
    };
}

template<typename T, typename R, typename... Args>
MethodInvoker MakeInvoker(R(T::*Method)(Args...) const)
{
    return [Method](void* Instance, std::span<const Variant> ArgsPack) -> TExpected<Variant> {
        if (!Instance)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null instance"));
        }
        if (ArgsPack.size() != sizeof...(Args))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Argument count mismatch"));
        }
        return detail::InvokeConstImpl(static_cast<const T*>(Instance), Method, ArgsPack, std::index_sequence_for<Args...>{});
    };
}

} // namespace SnAPI::GameFramework
