#pragma once

#include <type_traits>

namespace SnAPI::GameFramework
{

/**
 * @brief Bit-flag helper for strongly-typed enums.
 * @tparam Enum Enum type that defines bit values.
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

    static constexpr TFlags FromRaw(Underlying Value)
    {
        return TFlags(Value);
    }

    constexpr Underlying Value() const
    {
        return m_value;
    }

    constexpr bool Empty() const
    {
        return m_value == 0;
    }

    constexpr bool Has(Enum Bits) const
    {
        return (m_value & static_cast<Underlying>(Bits)) != 0;
    }

    constexpr void Add(Enum Bits)
    {
        m_value |= static_cast<Underlying>(Bits);
    }

    constexpr void Remove(Enum Bits)
    {
        m_value &= ~static_cast<Underlying>(Bits);
    }

    constexpr void Clear()
    {
        m_value = 0;
    }

    constexpr TFlags operator|(Enum Bits) const
    {
        return TFlags(m_value | static_cast<Underlying>(Bits));
    }

    constexpr TFlags operator|(TFlags Other) const
    {
        return TFlags(m_value | Other.m_value);
    }

    constexpr TFlags operator&(Enum Bits) const
    {
        return TFlags(m_value & static_cast<Underlying>(Bits));
    }

    constexpr TFlags operator&(TFlags Other) const
    {
        return TFlags(m_value & Other.m_value);
    }

    constexpr TFlags& operator|=(Enum Bits)
    {
        m_value |= static_cast<Underlying>(Bits);
        return *this;
    }

    constexpr TFlags& operator|=(TFlags Other)
    {
        m_value |= Other.m_value;
        return *this;
    }

    constexpr bool operator==(TFlags Other) const
    {
        return m_value == Other.m_value;
    }

    constexpr bool operator!=(TFlags Other) const
    {
        return m_value != Other.m_value;
    }

private:
    Underlying m_value = 0;
};

/**
 * @brief Combine two enum flag bits into a TFlags value.
 */
template<typename Enum>
constexpr TFlags<Enum> operator|(Enum Left, Enum Right)
{
    return TFlags<Enum>(Left) | Right;
}

/**
 * @brief Intersect two enum flag bits into a TFlags value.
 */
template<typename Enum>
constexpr TFlags<Enum> operator&(Enum Left, Enum Right)
{
    return TFlags<Enum>(Left) & Right;
}

} // namespace SnAPI::GameFramework
