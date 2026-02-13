#pragma once

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <cstdint>

#include "Flags.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Game-level collision channel bit flags.
 * @remarks
 * Physics backends consume integer layer/mask values, while gameplay code can use
 * strong typed flags for intent and readability.
 */
enum class ECollisionFilterBits : std::uint32_t
{
    None = 0u,
    WorldStatic = 1u << 0u,
    WorldDynamic = 1u << 1u,
    Character = 1u << 2u,
    Player = 1u << 3u,
    Npc = 1u << 4u,
    Vehicle = 1u << 5u,
    Projectile = 1u << 6u,
    TriggerVolume = 1u << 7u,
    Pickup = 1u << 8u,
    Debris = 1u << 9u,
    Sensor = 1u << 10u,
    Cloth = 1u << 11u,
    Terrain = 1u << 12u,
    Water = 1u << 13u,
    Foliage = 1u << 14u,
    Effect = 1u << 15u,
    Weapon = 1u << 16u,
    Hitbox = 1u << 17u,
    Hurtbox = 1u << 18u,
    Ragdoll = 1u << 19u,
    Interactable = 1u << 20u,
    Door = 1u << 21u,
    Buildable = 1u << 22u,
    Destructible = 1u << 23u,
    PhysicsProxy = 1u << 24u,
    Ghost = 1u << 25u,
    SpawnPoint = 1u << 26u,
    Camera = 1u << 27u,
    TeamA = 1u << 28u,
    TeamB = 1u << 29u,
    TeamC = 1u << 30u,
    TeamD = 1u << 31u,
    All = 0xFFFFFFFFu
};

using CollisionFilterFlags = TFlags<ECollisionFilterBits>;
using CollisionLayerFlags = CollisionFilterFlags;
using CollisionMaskFlags = CollisionFilterFlags;

template<>
struct EnableFlags<ECollisionFilterBits> : std::true_type
{
};

/**
 * @brief Build a single-layer flag from a 0..31 layer index.
 */
constexpr CollisionLayerFlags CollisionLayerFromIndex(const std::uint32_t LayerIndex)
{
    if (LayerIndex >= 32u)
    {
        return CollisionLayerFlags{};
    }
    return CollisionLayerFlags::FromRaw(1u << LayerIndex);
}

/**
 * @brief Convert a layer flag into the first set layer index.
 * @remarks If multiple bits are set, the least-significant bit is used.
 */
constexpr std::uint32_t CollisionLayerToIndex(CollisionLayerFlags Layer)
{
    std::uint32_t Bits = Layer.Value();
    if (Bits == 0u)
    {
        return 0u;
    }

    std::uint32_t Index = 0u;
    while ((Bits & 1u) == 0u && Index < 31u)
    {
        Bits >>= 1u;
        ++Index;
    }
    return Index;
}

inline constexpr CollisionMaskFlags kCollisionMaskAll =
    CollisionMaskFlags::FromRaw(static_cast<std::uint32_t>(ECollisionFilterBits::All));

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
