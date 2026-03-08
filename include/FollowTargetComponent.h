#pragma once

#include "BaseComponent.h"
#include "Math.h"
#include <string_view>

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Component that follows another node's world transform.
 *
 * `FollowTargetComponent` is a lightweight spatial constraint. It samples another node's
 * world transform, blends toward the desired position and/or rotation, and then writes the
 * owning node's world transform back through `TransformComponent` helpers.
 *
 * Typical use:
 * - attach it to a camera or helper node
 * - point `Settings::Target` at a player or anchor node
 * - use smoothing to keep follow behavior out of custom gameplay loops
 *
 * Core semantics:
 * - follow operates in world space
 * - position and rotation synchronization can be enabled independently
 * - if the owner lacks a `TransformComponent`, the world-transform write path may create one
 * - UUID fallback can be enabled for replication/serialization restore paths where runtime slot keys are not yet populated
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see TransformComponent
 */
class FollowTargetComponent : public BaseComponent, public ComponentCRTP<FollowTargetComponent>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::FollowTargetComponent";
    /** @brief Tick ordering hint: follow runs before camera/render consumers. */
    static constexpr int kTickPriority = -50;

    /**
     * @brief Follow behavior settings.
     *
     * Units:
     * - `PositionOffset` is in world units
     * - smoothing values are exponential frequencies in hertz
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::FollowTargetComponent::Settings";

        NodeHandle Target{}; /**< @brief Target node to follow. */
        Vec3 PositionOffset = Vec3::Zero(); /**< @brief World-space offset added to target position when syncing position. */
        bool SyncPosition = true; /**< @brief Enable position follow. */
        bool SyncRotation = false; /**< @brief Enable rotation follow from target rotation. */
        Quat RotationOffset = Quat::Identity(); /**< @brief Extra rotation applied after followed target rotation when SyncRotation is true. */
        float PositionSmoothingHz = 14.0f; /**< @brief Exponential smoothing frequency for position (0 = instant snap). */
        float RotationSmoothingHz = 14.0f; /**< @brief Exponential smoothing frequency for rotation (0 = instant snap). */
        bool ResolveTargetByUuidFallback = true; /**< @brief Resolve target through UUID fallback when runtime key path is unavailable. */
    };

    /** @brief Access settings (const). */
    const Settings& GetSettings() const
    {
        return m_settings;
    }

    /** @brief Access settings for mutation. */
    Settings& EditSettings()
    {
        return m_settings;
    }

    /** @brief Variable-step follow update. @param DeltaSeconds Frame delta in seconds used for smoothing. */
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /** @brief Editor-only property change hook. @param Name Changed reflected field name. */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    /**
     * @brief Execute one follow update using current settings.
     * @param DeltaSeconds Variable-step delta used for smoothing filters.
     * @return True when owner transform was updated.
     */
    bool ApplyFollow(float DeltaSeconds);

    Settings m_settings{}; /**< @brief Follow behavior configuration. */
};

} // namespace SnAPI::GameFramework
