#pragma once

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Trait that provides the canonical stable reflection name for a C++ type.
 * @tparam T Type to name.
 *
 * `TTypeName` is the root of the engine's deterministic type-identity scheme. The string exposed by
 * this trait is used to derive `TypeId` values, drive reflection registration, and label types in
 * serialization, scripting, editor UI, and diagnostics.
 *
 * Contract:
 * - The returned name must remain stable once serialized data or network protocols depend on it.
 * - User-defined engine types usually satisfy this by exposing `static constexpr const char* kTypeName`.
 * - External or builtin types should specialize the trait with `SNAPI_DEFINE_TYPE_NAME`.
 */
template<typename T>
struct TTypeName
{
    static constexpr const char* Value = T::kTypeName; /**< @brief Stable fully-qualified type name used for deterministic TypeId generation. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Convenience alias for `TTypeName<T>::Value`.
 */
template<typename T>
inline constexpr const char* TTypeNameV = TTypeName<T>::Value;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Macro that specializes `TTypeName` for a type without a native `kTypeName`.
 * @param Type C++ type to specialize.
 * @param Name Fully qualified name string.
 *
 * Use this for:
 * - builtin primitive wrappers
 * - external library types
 * - engine types that cannot expose `kTypeName` directly
 */
#define SNAPI_DEFINE_TYPE_NAME(Type, Name) \
    template<> \
    struct TTypeName<Type> \
    { \
        static constexpr const char* Value = Name; \
    };

} // namespace SnAPI::GameFramework
