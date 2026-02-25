#pragma once

#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Component that follows another node's transform.
 * @remarks
 * Typical use: attach to a camera node and configure `Target` + `PositionOffset`
 * to keep camera-follow behavior out of application loops.
 *
 * Ownership and replication notes:
 * - `Target` is a regular `NodeHandle`; fast runtime-key resolution is used when
 *   available.
 * - Optional UUID fallback can be enabled for replication/serialization restore
 *   paths where runtime slot keys are not yet populated.
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

    /** @brief Variable-step follow update. */
    void Tick(float DeltaSeconds);

    /**
     * @brief Non-virtual follow update entry used by ECS runtime bridge.
     * @param DeltaSeconds Variable-step delta used for smoothing filters.
     */
    void RuntimeTick(float DeltaSeconds);
    void TickImpl(IWorld&, float DeltaSeconds) { RuntimeTick(DeltaSeconds); }

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
