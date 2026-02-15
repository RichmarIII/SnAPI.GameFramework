#pragma once

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <cstdint>

#include <Physics.h>

#include "IComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

class PhysicsSystem;

/**
 * @brief Physics rigid-body component bound to an owning node.
 */
class RigidBodyComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::RigidBodyComponent";

    /**
     * @brief Runtime body configuration used for body creation.
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::RigidBodyComponent::Settings";

        SnAPI::Physics::EBodyType BodyType = SnAPI::Physics::EBodyType::Dynamic; /**< @brief Physics body type. */
        float Mass = 1.0f; /**< @brief Body mass used for dynamic bodies. */
        float LinearDamping = 0.05f; /**< @brief Linear damping factor. */
        float AngularDamping = 0.05f; /**< @brief Angular damping factor. */
        bool EnableCcd = true; /**< @brief Continuous collision detection toggle. */
        bool StartActive = true; /**< @brief Initial activation state. */

        Vec3 InitialLinearVelocity{}; /**< @brief Initial linear velocity at creation time. */
        Vec3 InitialAngularVelocity{}; /**< @brief Initial angular velocity at creation time. */

        bool SyncFromPhysics = true; /**< @brief Pull transform from physics for dynamic bodies. */
        bool SyncToPhysics = true; /**< @brief Push transform to physics for static/kinematic bodies. */
        bool AutoDeactivateWhenSleeping = true; /**< @brief Toggle component tick activity from physics sleep/wake events (dynamic bodies only). */
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

    void OnCreate() override;
    void OnDestroy() override;
    void FixedTick(float DeltaSeconds) override;

    /** @brief Ensure the physics body exists for this component. */
    bool CreateBody();
    /** @brief Destroy and release the physics body for this component. */
    void DestroyBody();
    /** @brief Rebuild the physics body from current settings/collider data. */
    bool RecreateBody();

    /** @brief Check whether a valid body handle exists. */
    bool HasBody() const
    {
        return m_bodyHandle.IsValid();
    }

    /** @brief Return raw body handle value (0 when invalid). */
    std::uint64_t BodyHandleValue() const
    {
        return m_bodyHandle.Value();
    }

    /** @brief Check last known backend sleep state for this body. */
    bool IsSleeping() const
    {
        return m_isSleeping;
    }

    /** @brief Get current backend body handle. */
    SnAPI::Physics::BodyHandle PhysicsBodyHandle() const
    {
        return m_bodyHandle;
    }

    /** @brief Apply force or impulse to body. */
    bool ApplyForce(const Vec3& Force, bool AsImpulse = false);
    /** @brief Apply force using an explicit physics force mode. */
    bool ApplyForce(const Vec3& Force, SnAPI::Physics::EForceMode Mode);
    /** @brief Set current linear/angular velocity. */
    bool SetVelocity(const Vec3& Linear, const Vec3& Angular = Vec3{});
    /** @brief Teleport body and owner transform without rebuilding body state. */
    bool Teleport(const Vec3& Position, const Quat& Rotation = Quat::Identity(), bool ResetVelocity = false);
private:
    PhysicsSystem* ResolvePhysicsSystem() const;

    void BindPhysicsEvents();
    void UnbindPhysicsEvents();
    void HandlePhysicsEvent(const SnAPI::Physics::PhysicsEvent& Event);
    void UpdateSleepDrivenActivity(bool Sleeping);

    bool SyncFromPhysics() const;
    bool SyncToPhysics() const;

    Settings m_settings{}; /**< @brief Body configuration settings. */
    SnAPI::Physics::BodyHandle m_bodyHandle{}; /**< @brief Active backend body handle. */
    std::uint64_t m_sleepListenerToken = 0; /**< @brief PhysicsSystem listener token for body sleep/wake routing. */
    bool m_isSleeping = false; /**< @brief Last known backend sleep state for the bound body. */
    mutable bool m_hasLastSyncedTransform = false; /**< @brief Whether transform delta cache is initialized for SyncFromPhysics. */
    mutable SnAPI::Physics::Vec3 m_lastSyncedPhysicsPosition = SnAPI::Physics::Vec3::Zero(); /**< @brief Last synced physics-space position. */
    mutable SnAPI::Physics::Quat m_lastSyncedPhysicsRotation = SnAPI::Physics::Quat::Identity(); /**< @brief Last synced physics-space rotation. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
