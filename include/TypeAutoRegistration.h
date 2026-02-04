#pragma once

#include "Serialization.h"
#include "TypeBuilder.h"

namespace SnAPI::GameFramework
{
/**
 * Auto-registration helpers for reflection and serialization.
 *
 * Usage (place in a single .cpp per type to avoid duplicate registration):
 *   SNAPI_REFLECT_TYPE((TTypeBuilder<MyType>(MyType::kTypeName)
 *       .Base<BaseNode>()
 *       .Field("Health", &MyType::m_health)
 *       .Constructor<>()
 *       .Register()));
 *
 *   SNAPI_REFLECT_COMPONENT((TTypeBuilder<MyComponent>(MyComponent::kTypeName)
 *       .Field("Speed", &MyComponent::m_speed)
 *       .Constructor<>()
 *       .Register()), MyComponent);
 *
 * The builder expression should register the type with TypeRegistry. If a node must be
 * created by TypeId (serialization, scripting), register a default constructor.
 * Components registered with SNAPI_REFLECT_COMPONENT are also added to
 * ComponentSerializationRegistry and use reflection-based serialization by default.
 * Field types must be known to ValueCodecRegistry (builtins) or themselves be
 * registered for reflection serialization.
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
 * @brief Register a reflected type using a builder expression.
 * @param BuilderExpr Expression that builds and registers the type.
 * @param Id Unique counter to avoid symbol collisions.
 * @remarks Use SNAPI_REFLECT_TYPE instead of calling this directly.
 */
#define SNAPI_REFLECT_TYPE_IMPL(BuilderExpr, Id) \
    namespace \
    { \
        void SNAPI_DETAIL_CONCAT(SnAPI_RegisterType_, Id)() \
        { \
            (void)(BuilderExpr); \
        } \
        const ::SnAPI::GameFramework::TTypeRegistrar SNAPI_DETAIL_CONCAT(SnAPI_TypeRegistrar_, Id)( \
            &SNAPI_DETAIL_CONCAT(SnAPI_RegisterType_, Id)); \
    }

/**
 * @brief Register a reflected type using a builder expression.
 * @param BuilderExpr Expression that builds and registers the type.
 * @remarks Place this in a single .cpp per type to avoid duplicate registration.
 */
#define SNAPI_REFLECT_TYPE(BuilderExpr) SNAPI_REFLECT_TYPE_IMPL(BuilderExpr, __COUNTER__)

/**
 * @brief Register a reflected component type and its serializer.
 * @param BuilderExpr Expression that builds and registers the type.
 * @param ComponentType Component C++ type.
 * @param Id Unique counter to avoid symbol collisions.
 * @remarks Also registers the component with ComponentSerializationRegistry.
 */
#define SNAPI_REFLECT_COMPONENT_IMPL(BuilderExpr, ComponentType, Id) \
    namespace \
    { \
        void SNAPI_DETAIL_CONCAT(SnAPI_RegisterComponent_, Id)() \
        { \
            (void)(BuilderExpr); \
            ::SnAPI::GameFramework::ComponentSerializationRegistry::Instance().Register<ComponentType>(); \
        } \
        const ::SnAPI::GameFramework::TTypeRegistrar SNAPI_DETAIL_CONCAT(SnAPI_ComponentRegistrar_, Id)( \
            &SNAPI_DETAIL_CONCAT(SnAPI_RegisterComponent_, Id)); \
    }

/**
 * @brief Register a reflected component type and its serializer.
 * @param BuilderExpr Expression that builds and registers the type.
 * @param ComponentType Component C++ type.
 * @remarks Place this in a single .cpp per type to avoid duplicate registration.
 */
#define SNAPI_REFLECT_COMPONENT(BuilderExpr, ComponentType) \
    SNAPI_REFLECT_COMPONENT_IMPL(BuilderExpr, ComponentType, __COUNTER__)
