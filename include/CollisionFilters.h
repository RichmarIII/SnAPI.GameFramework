#pragma once

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <cstdint>

#include "Flags.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Game-level collision channel bit flags used for physics layers and masks.
 *
 * Physics backends usually consume plain integer layer and mask fields. `ECollisionFilterBits`
 * provides the strongly typed gameplay-facing vocabulary used throughout GameFramework when
 * selecting collider layers, query masks, and broad filtering behavior.
 *
 * Semantics:
 * - A single set bit represents one logical collision layer.
 * - A mask may contain any number of bits.
 * - `All` is a convenience value with all 32 bits set.
 *
 * @note Although the same flag type is used for layers and masks, downstream code typically expects
 * layers to contain exactly one effective bit.
 */
enum class ECollisionFilterBits : std::uint32_t
{
    None = 0u, /**< @brief No collision bits set. */
    WorldStatic = 1u << 0u, /**< @brief Static level geometry and other immovable world surfaces. */
    WorldDynamic = 1u << 1u, /**< @brief Dynamic world props and general simulated objects. */
    Character = 1u << 2u, /**< @brief Generic character bodies. */
    Player = 1u << 3u, /**< @brief Player-controlled actors. */
    Npc = 1u << 4u, /**< @brief Non-player characters. */
    Vehicle = 1u << 5u, /**< @brief Vehicles and rideable movers. */
    Projectile = 1u << 6u, /**< @brief Projectiles such as bullets or rockets. */
    TriggerVolume = 1u << 7u, /**< @brief Trigger volumes and overlap-only gameplay regions. */
    Pickup = 1u << 8u, /**< @brief Pickups and collectible gameplay items. */
    Debris = 1u << 9u, /**< @brief Debris and lightweight breakable fragments. */
    Sensor = 1u << 10u, /**< @brief Query-only sensor objects. */
    Cloth = 1u << 11u, /**< @brief Cloth-like simulated objects. */
    Terrain = 1u << 12u, /**< @brief Terrain surfaces when distinguished from other world geometry. */
    Water = 1u << 13u, /**< @brief Water volumes or water-surface interaction bodies. */
    Foliage = 1u << 14u, /**< @brief Foliage and vegetation collision. */
    Effect = 1u << 15u, /**< @brief Non-gameplay effects with optional collision presence. */
    Weapon = 1u << 16u, /**< @brief Weapon actors or weapon traces. */
    Hitbox = 1u << 17u, /**< @brief Offensive hit-detection volumes. */
    Hurtbox = 1u << 18u, /**< @brief Defensive damage-receiving volumes. */
    Ragdoll = 1u << 19u, /**< @brief Ragdoll bodies. */
    Interactable = 1u << 20u, /**< @brief Interactable world objects. */
    Door = 1u << 21u, /**< @brief Doors and door-like blockers. */
    Buildable = 1u << 22u, /**< @brief Player-placed or build-system objects. */
    Destructible = 1u << 23u, /**< @brief Destructible world objects. */
    PhysicsProxy = 1u << 24u, /**< @brief Simplified proxy collision used by higher-level objects. */
    Ghost = 1u << 25u, /**< @brief Ghosted objects with special or reduced collision semantics. */
    SpawnPoint = 1u << 26u, /**< @brief Spawn markers and spawn-area queries. */
    Camera = 1u << 27u, /**< @brief Camera blockers or camera-specific query geometry. */
    TeamA = 1u << 28u, /**< @brief Team A collision grouping bit. */
    TeamB = 1u << 29u, /**< @brief Team B collision grouping bit. */
    TeamC = 1u << 30u, /**< @brief Team C collision grouping bit. */
    TeamD = 1u << 31u, /**< @brief Team D collision grouping bit. */
    All = 0xFFFFFFFFu /**< @brief All collision bits enabled. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Generic collision-flag set over `ECollisionFilterBits`.
 */
using CollisionFilterFlags = TFlags<ECollisionFilterBits>;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Flag set used where one effective collision layer is expected.
 */
using CollisionLayerFlags = CollisionFilterFlags;
/**
 * @ingroup SnAPI_GameFramework
 * @brief Flag set used for collision/query masks spanning multiple layers.
 */
using CollisionMaskFlags = CollisionFilterFlags;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Enables `TFlags` bitwise helpers for `ECollisionFilterBits`.
 */
template<>
struct EnableFlags<ECollisionFilterBits> : std::true_type
{
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Build a single-bit collision layer from a numeric layer index.
 * @param LayerIndex Collision layer index in the range `[0, 31]`.
 * @return A single-bit `CollisionLayerFlags` value, or an empty flag set when @p LayerIndex is out of range.
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
 * @ingroup SnAPI_GameFramework
 * @brief Convert a collision layer flag set into its first set bit index.
 * @param Layer Collision layer flag set to inspect.
 * @return The index of the least-significant set bit, or `0` when no bits are set.
 *
 * @warning An empty layer set also returns `0`, so callers that need to distinguish "no layer" from
 * "layer 0" must inspect `Layer.Value()` separately before calling this helper.
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

/**
 * @ingroup SnAPI_GameFramework
 * @brief Convenience mask with all collision bits enabled.
 */
inline constexpr CollisionMaskFlags kCollisionMaskAll =
    CollisionMaskFlags::FromRaw(static_cast<std::uint32_t>(ECollisionFilterBits::All));

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
