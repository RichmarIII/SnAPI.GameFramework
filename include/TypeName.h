#pragma once

namespace SnAPI::GameFramework
{

/**
 * @brief Type trait that provides a stable type name string.
 * @tparam T Type to name.
 * @remarks Types must provide a static kTypeName or specialize this trait.
 */
template<typename T>
struct TTypeName
{
    static constexpr const char* Value = T::kTypeName;
};

/**
 * @brief Convenience alias for TTypeName<T>::Value.
 */
template<typename T>
inline constexpr const char* TTypeNameV = TTypeName<T>::Value;

/**
 * @brief Macro to specialize TTypeName for a type without kTypeName.
 * @param Type C++ type to specialize.
 * @param Name Fully qualified name string.
 * @remarks Use this for external or builtin types.
 */
#define SNAPI_DEFINE_TYPE_NAME(Type, Name) \
    template<> \
    struct TTypeName<Type> \
    { \
        static constexpr const char* Value = Name; \
    };

} // namespace SnAPI::GameFramework
