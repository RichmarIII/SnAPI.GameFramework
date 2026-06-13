#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <type_traits>

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

template<typename T>
struct THasNativeReflectedTypeName : std::false_type
{
};

template<typename T>
    requires requires {
        { std::remove_cvref_t<T>::kTypeName } -> std::convertible_to<const char*>;
    }
struct THasNativeReflectedTypeName<T> : std::true_type
{
};

template<typename T>
struct THasDeclaredReflectedTypeName : THasNativeReflectedTypeName<T>
{
};

template<typename T>
struct THasReflectedTypeName : THasDeclaredReflectedTypeName<std::remove_cvref_t<T>>
{
};

template<typename T>
struct THasReflectedTypeName<T*> : THasReflectedTypeName<std::remove_cvref_t<T>>
{
};

template<typename T>
struct THasReflectedTypeName<T&> : THasReflectedTypeName<std::remove_cvref_t<T>>
{
};

template<typename T>
struct THasReflectedTypeName<T&&> : THasReflectedTypeName<std::remove_cvref_t<T>>
{
};

template<typename T, std::size_t Extent>
struct THasReflectedTypeName<T[Extent]> : THasReflectedTypeName<std::remove_cvref_t<T>>
{
};

template<typename T>
struct THasReflectedTypeName<T[]> : THasReflectedTypeName<std::remove_cvref_t<T>>
{
};

template<typename T>
concept CHasReflectedTypeName = THasReflectedTypeName<T>::value;

template<typename T>
inline const std::string& ReflectedTypeName()
{
    using Bare = std::remove_reference_t<T>;
    using Normalized = std::remove_cv_t<Bare>;
    static const std::string Name = []() {
        if constexpr (std::is_pointer_v<Bare>)
        {
            using PointeeWithCv = std::remove_pointer_t<Bare>;
            using Pointee = std::remove_cv_t<PointeeWithCv>;

            std::string Result{};
            if constexpr (std::is_const_v<PointeeWithCv>)
            {
                Result += "const ";
            }
            Result += ReflectedTypeName<Pointee>();
            Result += '*';
            return Result;
        }
        else
        {
            return std::string(TTypeNameV<Normalized>);
        }
    }();
    return Name;
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Convert a fully qualified reflected type spelling into a UI-friendly label.
 *
 * Examples:
 * - `SnAPI::GameFramework::Vec3` -> `Vec3`
 * - `SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::Vec3>` -> `TAssetRef<Vec3>`
 * - `const SnAPI::GameFramework::IWorld*` -> `const IWorld*`
 *
 * The transformation is purely cosmetic and preserves template/pointer/reference punctuation while
 * stripping namespace qualifiers from each reflected identifier token independently.
 */
inline std::string PrettyReflectedTypeName(std::string_view QualifiedName)
{
    auto StripQualifier = [] (std::string_view Token) -> std::string_view {
        const std::size_t CppSeparator = Token.rfind("::");
        const std::size_t DotSeparator = Token.rfind('.');
        std::size_t Start = 0;
        if (CppSeparator != std::string_view::npos)
        {
            Start = CppSeparator + 2;
        }
        if (DotSeparator != std::string_view::npos)
        {
            Start = std::max(Start, DotSeparator + 1);
        }
        return Token.substr(Start);
    };

    auto FlushToken = [&StripQualifier](std::string& Out, std::string& Token) {
        if (Token.empty())
        {
            return;
        }

        Out.append(StripQualifier(Token));
        Token.clear();
    };

    std::string Result{};
    Result.reserve(QualifiedName.size());

    std::string Token{};
    Token.reserve(QualifiedName.size());

    for (const char Ch : QualifiedName)
    {
        const unsigned char Byte = static_cast<unsigned char>(Ch);
        if (std::isalnum(Byte) != 0 || Ch == '_' || Ch == ':' || Ch == '.')
        {
            Token.push_back(Ch);
            continue;
        }

        FlushToken(Result, Token);
        Result.push_back(Ch);
    }

    FlushToken(Result, Token);
    return Result;
}

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
    }; \
    template<> \
    struct THasDeclaredReflectedTypeName<Type> : std::true_type \
    { \
    };

} // namespace SnAPI::GameFramework
