#pragma once

#include <mutex>
#include <optional>

#include "Expected.h"
#include "TypeAutoRegistry.h"
#include "TypeBuilder.h"

namespace SnAPI::GameFramework
{
/**
 * Auto-registration helpers for reflection and serialization.
 *
 * Usage (place in a single .cpp per type to avoid duplicate registration):
 *   SNAPI_REFLECT_TYPE(MyType, (TTypeBuilder<MyType>(MyType::kTypeName)
 *       .Base<BaseNode>()
 *       .Field("Health", &MyType::m_health)
 *       .Constructor<>()
 *       .Register()));
 *
 *   SNAPI_REFLECT_COMPONENT(MyComponent, (TTypeBuilder<MyComponent>(MyComponent::kTypeName)
 *       .Field("Speed", &MyComponent::m_speed)
 *       .Constructor<>()
 *       .Register()));
 *
 * The builder expression should register the type with TypeRegistry. If a node must be
 * created by TypeId (serialization, scripting), register a default constructor.
 * Types are registered lazily: the macro installs an "ensure" callback keyed by
 * deterministic TypeId. The actual TypeRegistry registration is performed on first use
 * (TypeRegistry::Find on miss, or explicit TypeAutoRegistry::Ensure).
 */

using TTypeRegisterFn = void(*)();

/**
 * @brief Helper that executes a registration function at static initialization.
 * @remarks Used by SNAPI_REFLECT_TYPE and SNAPI_REFLECT_COMPONENT macros.
 * @note Static initialization order across translation units is undefined.
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
 * @brief Register a reflected type using a builder expression (lazy).
 * @param BuilderExpr Expression that builds and registers the type.
 * @param Id Unique counter to avoid symbol collisions.
 * @remarks Use SNAPI_REFLECT_TYPE instead of calling this directly.
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
            const ::SnAPI::GameFramework::TypeId TypeKey = ::SnAPI::GameFramework::TypeIdFromName(::SnAPI::GameFramework::TTypeNameV<Type>); \
            ::SnAPI::GameFramework::TypeAutoRegistry::Instance().Register(TypeKey, ::SnAPI::GameFramework::TTypeNameV<Type>, &SNAPI_DETAIL_CONCAT(SnAPI_EnsureType_, Id)); \
        } \
        SNAPI_DETAIL_USED const ::SnAPI::GameFramework::TTypeRegistrar SNAPI_DETAIL_CONCAT(SnAPI_TypeRegistrar_, Id)( \
            &SNAPI_DETAIL_CONCAT(SnAPI_RegisterAutoType_, Id)); \
    }

/**
 * @brief Register a reflected type using a builder expression (lazy).
 * @param Type C++ type being registered.
 * @param BuilderExpr Expression that builds and registers the type's TypeInfo.
 * @remarks Place this in a single .cpp per type to avoid duplicate registration.
 */
#define SNAPI_REFLECT_TYPE(Type, BuilderExpr) SNAPI_REFLECT_TYPE_IMPL(Type, BuilderExpr, __COUNTER__)

/**
 * @brief Register a reflected component type and its serializer.
 * @param ComponentType Component C++ type.
 * @param BuilderExpr Expression that builds and registers the type's TypeInfo.
 * @remarks Components are automatically registered with ComponentSerializationRegistry
 *          by TTypeBuilder<>::Register when they derive from IComponent.
 */
#define SNAPI_REFLECT_COMPONENT(ComponentType, BuilderExpr) \
    SNAPI_REFLECT_TYPE(ComponentType, BuilderExpr)
