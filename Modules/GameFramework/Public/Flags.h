#pragma once

#include <type_traits>

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Lightweight bitflag wrapper for strongly typed enums.
 * @tparam Enum Enum type whose underlying values represent bit flags.
 *
 * `TFlags` provides a small value-type wrapper around the enum's underlying integer bits while
 * keeping explicit control over which enums are allowed to participate in free `operator|` / `operator&`.
 */
template<typename Enum>
class TFlags
{
public:
    static_assert(std::is_enum_v<Enum>, "TFlags requires an enum type");
    using Underlying = std::underlying_type_t<Enum>;

    constexpr TFlags() = default;
    constexpr TFlags(Enum Bits)
        : m_value(static_cast<Underlying>(Bits))
    {
    }
    constexpr explicit TFlags(Underlying Value)
        : m_value(Value)
    {
    }

    /** @brief Construct flags directly from raw underlying bits. */
    static constexpr TFlags FromRaw(Underlying Value)
    {
        return TFlags(Value);
    }

    /** @brief Get the raw underlying-bit value. */
    constexpr Underlying Value() const
    {
        return m_value;
    }

    /** @brief Check whether no bits are set. */
    constexpr bool Empty() const
    {
        return m_value == 0;
    }

    /** @brief Check whether any bit from `Bits` is set. */
    constexpr bool Has(Enum Bits) const
    {
        return (m_value & static_cast<Underlying>(Bits)) != 0;
    }

    /** @brief Set the supplied bits. */
    constexpr void Add(Enum Bits)
    {
        m_value |= static_cast<Underlying>(Bits);
    }

    /** @brief Clear the supplied bits. */
    constexpr void Remove(Enum Bits)
    {
        m_value &= ~static_cast<Underlying>(Bits);
    }

    /** @brief Clear all bits. */
    constexpr void Clear()
    {
        m_value = 0;
    }

    /** @brief Return a new flag set with `Bits` added. */
    constexpr TFlags operator|(Enum Bits) const
    {
        return TFlags(m_value | static_cast<Underlying>(Bits));
    }

    /** @brief Return the union of two flag sets. */
    constexpr TFlags operator|(TFlags Other) const
    {
        return TFlags(m_value | Other.m_value);
    }

    /** @brief Return the intersection between this set and `Bits`. */
    constexpr TFlags operator&(Enum Bits) const
    {
        return TFlags(m_value & static_cast<Underlying>(Bits));
    }

    /** @brief Return the intersection of two flag sets. */
    constexpr TFlags operator&(TFlags Other) const
    {
        return TFlags(m_value & Other.m_value);
    }

    /** @brief In-place union with `Bits`. */
    constexpr TFlags& operator|=(Enum Bits)
    {
        m_value |= static_cast<Underlying>(Bits);
        return *this;
    }

    /** @brief In-place union with another flag set. */
    constexpr TFlags& operator|=(TFlags Other)
    {
        m_value |= Other.m_value;
        return *this;
    }

    /** @brief Equality comparison on raw bits. */
    constexpr bool operator==(TFlags Other) const
    {
        return m_value == Other.m_value;
    }

    /** @brief Inequality comparison on raw bits. */
    constexpr bool operator!=(TFlags Other) const
    {
        return m_value != Other.m_value;
    }

private:
    Underlying m_value = 0; /**< @brief Raw underlying-bit storage for the wrapped enum flags. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Trait used to opt an enum into free bitwise flag operators.
 * @tparam Enum Enum type.
 */
template<typename Enum>
struct EnableFlags : std::false_type
{
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Convenience variable template for `EnableFlags<Enum>::value`.
 */
template<typename Enum>
inline constexpr bool EnableFlagsV = EnableFlags<Enum>::value;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Combine two enum values into a `TFlags<Enum>` result.
 */
template<typename Enum>
constexpr std::enable_if_t<EnableFlagsV<Enum>, TFlags<Enum>> operator|(Enum Left, Enum Right)
{
    return TFlags<Enum>(Left) | Right;
}

/**
 * @ingroup SnAPI_GameFramework
 * @brief Intersect two enum values into a `TFlags<Enum>` result.
 */
template<typename Enum>
constexpr std::enable_if_t<EnableFlagsV<Enum>, TFlags<Enum>> operator&(Enum Left, Enum Right)
{
    return TFlags<Enum>(Left) & Right;
}

} // namespace SnAPI::GameFramework
