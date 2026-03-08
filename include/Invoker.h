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

/**
 * @ingroup SnAPI_GameFramework
 * @brief Type-erased reflected method invocation function.
 *
 * The callable accepts a raw instance pointer plus a packed `Variant` argument span and returns either
 * a `Variant` result or an error.
 */
using MethodInvoker = std::function<TExpected<Variant>(void* Instance, std::span<const Variant> Args)>;

namespace detail
{

/**
 * @brief Internal storage selection for extracted reflected arguments.
 * @tparam Arg Requested argument type.
 *
 * Lvalue references are stored as `std::reference_wrapper`, while by-value arguments are copied.
 */
template<typename Arg>
struct TArgStorage
{
    using Raw = std::remove_reference_t<Arg>;
    using Type = std::conditional_t<std::is_lvalue_reference_v<Arg>, std::reference_wrapper<Raw>, std::remove_cvref_t<Arg>>;
};

/** @brief Helper alias for the extracted storage type of one reflected argument. */
template<typename Arg>
using TArgStorageT = typename TArgStorage<Arg>::Type;

/**
 * @brief Extract one typed argument from a `Variant`.
 * @tparam Arg Expected argument type.
 * @param Value Variant to extract from.
 * @return Storage wrapper containing the argument or an error.
 *
 * Mutable lvalue references require a mutable reference payload in `Value`.
 */
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

/**
 * @brief Convert extracted storage to the actual invocation argument type.
 * @tparam Arg Requested argument type.
 * @param Storage Extracted storage wrapper.
 * @return Value copy or lvalue reference as required by `Arg`.
 */
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

/**
 * @brief Invoke a non-const member function with reflected args.
 * @tparam T Instance type.
 * @tparam R Return type.
 * @tparam Args Argument pack.
 * @param Instance Pointer to instance.
 * @param Method Member function pointer.
 * @param ArgsPack Packed arguments.
 * @return Variant containing the result or error.
 * @remarks Argument extraction is validated before invocation.
 */
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

/**
 * @brief Invoke a const member function with reflected args.
 * @tparam T Instance type.
 * @tparam R Return type.
 * @tparam Args Argument pack.
 * @param Instance Pointer to const instance.
 * @param Method Const member function pointer.
 * @param ArgsPack Packed arguments.
 * @return Variant containing the result or error.
 */
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

/**
 * @ingroup SnAPI_GameFramework
 * @brief Create a `MethodInvoker` for a non-const member function.
 * @tparam T Instance type.
 * @tparam R Return type.
 * @tparam Args Argument pack.
 * @param Method Member function pointer.
 * @return Callable invoker.
 *
 * Runtime checks performed by the invoker:
 * - null instance detection
 * - exact argument-count validation
 * - per-argument type and constness validation through `Variant`
 */
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

/**
 * @ingroup SnAPI_GameFramework
 * @overload
 *
 * Create a `MethodInvoker` for a const member function.
 *
 * The returned invoker applies the same runtime validation contract as the non-const overload
 * while invoking the member through a const-qualified instance view.
 */
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
