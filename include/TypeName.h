#pragma once

namespace SnAPI::GameFramework
{

template<typename T>
struct TTypeName
{
    static constexpr const char* Value = T::kTypeName;
};

template<typename T>
inline constexpr const char* TTypeNameV = TTypeName<T>::Value;

#define SNAPI_DEFINE_TYPE_NAME(Type, Name) \
    template<> \
    struct TTypeName<Type> \
    { \
        static constexpr const char* Value = Name; \
    };

} // namespace SnAPI::GameFramework
