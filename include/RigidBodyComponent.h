#pragma once

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <cstdint>
#include <string_view>

#include <Physics.h>

#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

class PhysicsSystem;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Physics rigid-body component that binds a backend body to an owning node transform.
 *
 * `RigidBodyComponent` is the bridge between the world graph and the physics scene. It creates
 * and owns one backend rigid body, keeps that body synchronized with the owning node, and exposes
 * a small imperative API for forces, velocities, and teleports.
 *
 * Sync semantics:
 * - Dynamic bodies pull transforms from physics into the owning node during variable tick.
 * - Static and kinematic bodies push the owning node transform into physics.
 * - When fixed ticking is enabled, static/kinematic push happens in `FixedTick()`. Otherwise it
 *   happens in normal `Tick()`.
 * - Dynamic bodies can interpolate between fixed-step samples using the world's interpolation alpha.
 *
 * Collider semantics:
 * - Body creation reads a sibling `ColliderComponent` when present.
 * - If no collider component exists, body creation falls back to a default box collider with
 *   half-extents `(0.5, 0.5, 0.5)`.
 *
 * Ownership and lifetime:
 * - The component owns the backend body handle and its sleep-listener registration.
 * - The actual body resource is owned by the physics scene.
 * - Editing settings marks the body dirty and causes recreation on the next sync point.
 *
 * Threading model:
 * - Main-thread only.
 *
 * Error semantics:
 * - Fails softly by returning `false` when the physics scene is unavailable or backend operations fail.
 * - Never throws from the public API.
 *
 * @note World-space positions are converted through `PhysicsSystem` so floating-origin or rebasing
 * policies remain consistent with the physics scene.
 *
 * @see PhysicsSystem
 * @see ColliderComponent
 * @see TransformComponent
 */
class RigidBodyComponent : public BaseComponent, public ComponentCRTP<RigidBodyComponent>
{
public:
    /** @brief Stable reflected type name used for serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RigidBodyComponent";
    /** @brief Tick ordering hint: rigid bodies run early so later systems see synchronized transforms. */
    static constexpr int kTickPriority = -100;

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Runtime body configuration used for body creation and recreation.
     *
     * These values are forwarded into the backend body description each time the body is created.
     * Mutating them does not live-edit the backend body directly; instead, the component marks the
     * body dirty and rebuilds it on the next sync point.
     */
    struct Settings
    {
        /** @brief Stable reflected type name used for serialization registration. */
        static constexpr const char* kTypeName = "SnAPI::GameFramework::RigidBodyComponent::Settings";

        SnAPI::Physics::EBodyType BodyType = SnAPI::Physics::EBodyType::Dynamic; /**< @brief Backend body type controlling simulation authority. */
        float Mass = 1.0f; /**< @brief Body mass forwarded to the backend during creation. Meaningful primarily for dynamic bodies. */
        float LinearDamping = 0.05f; /**< @brief Linear damping scalar forwarded to the backend body description. */
        float AngularDamping = 0.05f; /**< @brief Angular damping scalar forwarded to the backend body description. */
        bool EnableCcd = true; /**< @brief Enable continuous collision detection when supported by the backend. */
        bool StartActive = true; /**< @brief Request an initially active backend body on creation. */

        Vec3 InitialLinearVelocity{}; /**< @brief Initial world-space linear velocity forwarded directly to the backend. */
        Vec3 InitialAngularVelocity{}; /**< @brief Initial angular velocity vector forwarded directly to the backend. */

        bool SyncFromPhysics = true; /**< @brief Allow dynamic-body transforms to be published back onto the owning node. */
        bool SyncToPhysics = true; /**< @brief Allow static/kinematic owner transforms to be pushed into physics. */
        bool EnableRenderInterpolation = true; /**< @brief Blend dynamic-body transforms between fixed-step samples when the world uses fixed tick. */
        bool AutoDeactivateWhenSleeping = true; /**< @brief Toggle component activity from body sleep/wake events for dynamic bodies. */
    };

    /** @brief Construct the component with default body settings. */
    RigidBodyComponent() = default;
    RigidBodyComponent(const RigidBodyComponent&) = delete;
    RigidBodyComponent& operator=(const RigidBodyComponent&) = delete;
    RigidBodyComponent(RigidBodyComponent&&) noexcept = default;
    RigidBodyComponent& operator=(RigidBodyComponent&&) noexcept = default;
    /** @brief Default destructor. Backend teardown happens through `OnDestroy()`. */
    ~RigidBodyComponent() = default;
    /**
     * @brief Construct the component with explicit initial settings.
     * @param Settings Initial body settings copied into the component.
     * @post The body is marked dirty and will be created from @p Settings on the next lifecycle/sync point.
     */
    explicit RigidBodyComponent(const Settings& Settings)
    {
        m_settings = Settings;
        m_settingsDirty = true;
    }

    /**
     * @brief Read the current body settings.
     * @return Borrowed reference to the stored settings object.
     */
    const Settings& GetSettings() const
    {
        return m_settings;
    }

    /**
     * @brief Mutate the current body settings.
     * @return Borrowed reference to the stored settings object.
     * @post The body is marked dirty and will be recreated on the next sync point.
     */
    Settings& EditSettings()
    {
        m_settingsDirty = true;
        return m_settings;
    }

    /** @brief Create the backend body if possible during component initialization. */
    void OnCreate();
    /** @brief Destroy the backend body and unregister physics callbacks during component teardown. */
    void OnDestroy();
    /**
     * @brief Variable-step sync phase.
     * @param DeltaSeconds Variable-step frame delta in seconds. Currently unused.
     *
     * Semantics:
     * - Recreates the body first when settings changed.
     * - Dynamic bodies publish physics transforms back to the node.
     * - Static/kinematic bodies push node transforms only when the world is not using fixed tick.
     */
    void Tick(float DeltaSeconds);
    /**
     * @brief Fixed-step sync phase for static and kinematic bodies.
     * @param DeltaSeconds Fixed-step delta in seconds. Currently unused.
     * @remarks Recreates the body first when settings changed, then pushes the owner transform into physics.
     */
    void FixedTick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /**
     * @brief React to editor-side body-setting edits.
     * @param Name Name of the changed reflected property.
     * @remarks Relevant setting edits cause immediate body recreation or creation.
     */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

    /**
     * @brief Ensure the backend physics body exists.
     * @return `true` when the component already has a valid body or body creation succeeded.
     *
     * Body creation reads the owning node transform, converts it into physics space, gathers collider
     * data from a sibling `ColliderComponent` when present, and binds sleep/wake listeners.
     */
    bool CreateBody();
    /**
     * @brief Destroy the backend physics body if it exists.
     * @post The body handle becomes invalid and physics-event subscriptions are removed.
     */
    void DestroyBody();
    /**
     * @brief Rebuild the backend body from the current settings and collider state.
     * @return `true` when recreation succeeds.
     */
    bool RecreateBody();

    /** @brief Check whether the component currently owns a valid backend body handle. */
    bool HasBody() const
    {
        return m_bodyHandle.IsValid();
    }

    /** @brief Return the raw backend body-handle value, or `0` when no body exists. */
    std::uint64_t BodyHandleValue() const
    {
        return m_bodyHandle.Value();
    }

    /** @brief Read the last known backend sleep state for the current body. */
    bool IsSleeping() const
    {
        return m_isSleeping;
    }

    /**
     * @brief Read the current backend body handle.
     * @return Copy of the backend handle. The handle remains valid until body destruction or recreation.
     */
    SnAPI::Physics::BodyHandle PhysicsBodyHandle() const
    {
        return m_bodyHandle;
    }

    /**
     * @brief Apply a force or impulse to the body.
     * @param Force World-space force vector forwarded directly to the backend.
     * @param AsImpulse When `true`, interpret @p Force as an impulse instead of a continuous force.
     * @return `true` on success.
     * @remarks Creates the body first if needed.
     */
    bool ApplyForce(const Vec3& Force, bool AsImpulse = false);
    /**
     * @brief Apply a force using an explicit backend force mode.
     * @param Force World-space force vector forwarded directly to the backend.
     * @param Mode Backend-defined force application mode.
     * @return `true` on success.
     * @remarks Creates the body first if needed.
     */
    bool ApplyForce(const Vec3& Force, SnAPI::Physics::EForceMode Mode);
    /**
     * @brief Set the backend body's current velocity state.
     * @param Linear World-space linear velocity.
     * @param Angular Angular velocity vector forwarded directly to the backend.
     * @return `true` on success.
     * @remarks Creates the body first if needed.
     */
    bool SetVelocity(const Vec3& Linear, const Vec3& Angular = Vec3{});
    /**
     * @brief Teleport both the owning node and backend body to a new pose.
     * @param Position Target world-space position.
     * @param Rotation Target world-space orientation.
     * @param ResetVelocity When `true`, also zero linear and angular velocity after the teleport.
     * @return `true` on success.
     *
     * Semantics:
     * - Updates the owning node transform first.
     * - Writes the new transform into the backend body.
     * - Resets interpolation/published-transform caches to the teleported state.
     */
    bool Teleport(const Vec3& Position, const Quat& Rotation = Quat::Identity(), bool ResetVelocity = false);
private:
    PhysicsSystem* ResolvePhysicsSystem() const;

    void BindPhysicsEvents();
    void UnbindPhysicsEvents();
    void HandlePhysicsEvent(const SnAPI::Physics::PhysicsEvent& Event);
    void UpdateSleepDrivenActivity(bool Sleeping);

    bool SyncFromPhysics() const;
    bool SyncToPhysics() const;
    /** @brief Resolve world-provided interpolation alpha for dynamic transform blending. */
    float ResolveInterpolationAlpha() const;

    Settings m_settings{}; /**< @brief Body configuration settings. */
    SnAPI::Physics::BodyHandle m_bodyHandle{}; /**< @brief Active backend body handle. */
    std::uint64_t m_sleepListenerToken = 0; /**< @brief PhysicsSystem listener token for body sleep/wake routing. */
    bool m_isSleeping = false; /**< @brief Last known backend sleep state for the bound body. */
    mutable bool m_hasPoseSamples = false; /**< @brief Whether previous/current dynamic pose samples are initialized. */
    mutable SnAPI::Physics::Vec3 m_previousPhysicsPosition = SnAPI::Physics::Vec3::Zero(); /**< @brief Previous fixed/sample position used for interpolation. */
    mutable SnAPI::Physics::Quat m_previousPhysicsRotation = SnAPI::Physics::Quat::Identity(); /**< @brief Previous fixed/sample rotation used for interpolation. */
    mutable SnAPI::Physics::Vec3 m_currentPhysicsPosition = SnAPI::Physics::Vec3::Zero(); /**< @brief Latest sampled physics position. */
    mutable SnAPI::Physics::Quat m_currentPhysicsRotation = SnAPI::Physics::Quat::Identity(); /**< @brief Latest sampled physics rotation. */
    mutable bool m_hasLastPublishedTransform = false; /**< @brief Whether last published transform cache is initialized. */
    mutable SnAPI::Physics::Vec3 m_lastPublishedPhysicsPosition = SnAPI::Physics::Vec3::Zero(); /**< @brief Last transform position written to owner. */
    mutable SnAPI::Physics::Quat m_lastPublishedPhysicsRotation = SnAPI::Physics::Quat::Identity(); /**< @brief Last transform rotation written to owner. */
    bool m_settingsDirty = true; /**< @brief True when mutable settings were edited and body recreation is required. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
