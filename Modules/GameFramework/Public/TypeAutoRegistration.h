#pragma once

#include <mutex>
#include <optional>

#include "Expected.h"
#include "TypeAutoRegistry.h"
#include "TypeBuilder.h"

namespace SnAPI::GameFramework
{
/**
 * @ingroup SnAPI_GameFramework
 * @brief Macro-based helpers for lazy reflection auto-registration.
 *
 * Typical usage places one `SNAPI_REFLECT_TYPE(...)` or `SNAPI_REFLECT_COMPONENT(...)` invocation in
 * exactly one `.cpp` file per reflected type:
 *
 * `SNAPI_REFLECT_TYPE(MyType, (TTypeBuilder<MyType>(MyType::kTypeName).Field(...).Register()));`
 *
 * Design intent:
 * - static initialization installs a cheap ensure callback into `TypeAutoRegistry`
 * - heavy `TypeRegistry` mutation is deferred until first use
 * - this reduces dependency on cross-translation-unit static initialization order
 *
 * If a type must be created by reflected constructor metadata, the builder expression should include
 * a matching `Constructor<...>()` registration.
 */

using TTypeRegisterFn = void(*)();

/**
 * @ingroup SnAPI_GameFramework
 * @brief Tiny helper that runs a function during static initialization.
 *
 * `TTypeRegistrar` is intentionally minimal: it simply executes a registration thunk when the static
 * object is constructed. The thunk normally registers an ensure callback with `TypeAutoRegistry`,
 * not the full type metadata itself.
 *
 * @note Static initialization order across translation units is still undefined, which is why the
 * heavy registration work is deferred to lazy ensure callbacks.
 */
class TTypeRegistrar
{
public:
    /**
     * @brief Construct and invoke the registration function.
     * @param Fn Function pointer to call.
     * @remarks If Fn is null, no action is taken.
     */
    explicit TTypeRegistrar(TTypeRegisterFn Fn)
    {
        if (Fn)
        {
            Fn();
        }
    }
};

} // namespace SnAPI::GameFramework

/**
 * @brief Internal macro helper for concatenation.
 */
#define SNAPI_DETAIL_CONCAT_INNER(a, b) a##b
/**
 * @brief Internal macro helper for concatenation.
 */
#define SNAPI_DETAIL_CONCAT(a, b) SNAPI_DETAIL_CONCAT_INNER(a, b)

/**
 * @brief Internal macro helper for "used" attribute to survive LTO/GC-sections.
 */
#if defined(__GNUC__) || defined(__clang__)
    #define SNAPI_DETAIL_USED [[gnu::used]]
#else
    #define SNAPI_DETAIL_USED
#endif

/**
 * @ingroup SnAPI_GameFramework
 * @brief Low-level macro that installs a lazy ensure callback for a reflected type.
 * @param BuilderExpr Expression that builds and registers the type.
 * @param Id Unique counter to avoid symbol collisions.
 *
 * `BuilderExpr` is executed at most once through a `std::once_flag`. Non-`AlreadyExists` errors are
 * cached and replayed on later ensure attempts.
 *
 * @warning This is an implementation macro. Use `SNAPI_REFLECT_TYPE` instead.
 */
#define SNAPI_REFLECT_TYPE_IMPL(Type, BuilderExpr, Id) \
    namespace \
    { \
        ::SnAPI::GameFramework::Result SNAPI_DETAIL_CONCAT(SnAPI_EnsureType_, Id)() \
        { \
            static std::once_flag Once; \
            static std::optional<::SnAPI::GameFramework::Error> ErrorValue; \
            std::call_once(Once, [] { \
                auto ResultValue = (BuilderExpr); \
                if constexpr (requires { ResultValue.has_value(); ResultValue.error(); }) \
                { \
                    if (!ResultValue && ResultValue.error().Code != ::SnAPI::GameFramework::EErrorCode::AlreadyExists) \
                    { \
                        ErrorValue = ResultValue.error(); \
                    } \
                } \
            }); \
            if (ErrorValue) \
            { \
                return std::unexpected(*ErrorValue); \
            } \
            return ::SnAPI::GameFramework::Ok(); \
        } \
        void SNAPI_DETAIL_CONCAT(SnAPI_RegisterAutoType_, Id)() \
        { \
            const ::SnAPI::GameFramework::TypeId TypeKey = ::SnAPI::GameFramework::StaticTypeId<Type>(); \
            ::SnAPI::GameFramework::TypeAutoRegistry::Instance().Register(TypeKey, ::SnAPI::GameFramework::ReflectedTypeName<Type>(), &SNAPI_DETAIL_CONCAT(SnAPI_EnsureType_, Id)); \
            const ::SnAPI::GameFramework::TypeId MutablePointerKey = ::SnAPI::GameFramework::StaticTypeId<Type*>(); \
            ::SnAPI::GameFramework::TypeAutoRegistry::Instance().Register(MutablePointerKey, ::SnAPI::GameFramework::ReflectedTypeName<Type*>(), &SNAPI_DETAIL_CONCAT(SnAPI_EnsureType_, Id)); \
            const ::SnAPI::GameFramework::TypeId ConstPointerKey = ::SnAPI::GameFramework::StaticTypeId<const Type*>(); \
            ::SnAPI::GameFramework::TypeAutoRegistry::Instance().Register(ConstPointerKey, ::SnAPI::GameFramework::ReflectedTypeName<const Type*>(), &SNAPI_DETAIL_CONCAT(SnAPI_EnsureType_, Id)); \
        } \
        SNAPI_DETAIL_USED const ::SnAPI::GameFramework::TTypeRegistrar SNAPI_DETAIL_CONCAT(SnAPI_TypeRegistrar_, Id)( \
            &SNAPI_DETAIL_CONCAT(SnAPI_RegisterAutoType_, Id)); \
    }

/**
 * @ingroup SnAPI_GameFramework
 * @brief Register a reflected type through a lazy `TypeAutoRegistry` ensure callback.
 * @param Type C++ type being registered.
 * @param RegistrationExpr Expression that registers and/or mutates the type metadata, returning `TExpected<TypeInfo*>` or compatible.
 *
 * This is the generic lazy auto-registration entry point used by both handwritten and generated
 * reflection code. Most call sites should prefer `SNAPI_REFLECT_TYPE`, but generators can use this
 * macro directly for enum records or other metadata that does not flow through `TTypeBuilder<T>`.
 */
#define SNAPI_REFLECT_METADATA(Type, RegistrationExpr) SNAPI_REFLECT_TYPE_IMPL(Type, RegistrationExpr, __COUNTER__)

/**
 * @ingroup SnAPI_GameFramework
 * @brief Register a reflected type through a lazy `TypeAutoRegistry` ensure callback.
 * @param Type C++ type being registered.
 * @param BuilderExpr Expression that builds and registers the type's TypeInfo.
 *
 * Place this in exactly one translation unit per type. The first use of the type's `TypeId` or type
 * name can then trigger actual `TypeRegistry` registration on demand.
 */
#define SNAPI_REFLECT_TYPE(Type, BuilderExpr) SNAPI_REFLECT_METADATA(Type, BuilderExpr)

/**
 * @ingroup SnAPI_GameFramework
 * @brief Register a reflected component type.
 * @param ComponentType Component C++ type.
 * @param BuilderExpr Expression that builds and registers the type's TypeInfo.
 *
 * Components still use the same lazy registration path as other reflected types. Their component
 * serialization registration is handled by `TTypeBuilder<T>::Register()` when `T` derives from
 * `BaseComponent`.
 */
#define SNAPI_REFLECT_COMPONENT(ComponentType, BuilderExpr) \
    SNAPI_REFLECT_TYPE(ComponentType, BuilderExpr)
