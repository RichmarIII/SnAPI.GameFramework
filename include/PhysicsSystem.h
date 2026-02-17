#pragma once

#if defined(SNAPI_GF_ENABLE_PHYSICS)

#include <memory>
#include "GameThreading.h"
#include <mutex>
#include <functional>
#include <span>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <optional>

#include <Physics.h>

#include "Expected.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Bootstrap settings for world-owned physics.
 */
struct PhysicsBootstrapSettings
{
    SnAPI::Physics::PhysicsSceneDesc Scene{}; /**< @brief Scene descriptor used for backend scene creation. */
    SnAPI::Physics::SceneRoutingDesc Routing{}; /**< @brief Backend routing per physics domain. */
    std::vector<SnAPI::Physics::CouplingDesc> Couplings{}; /**< @brief Optional inter-domain coupling descriptors. */

    std::uint32_t ThreadCount = 0; /**< @brief Optional physics worker-thread override (0 = use scene/default backend behavior). */
    std::optional<std::uint32_t> MaxSubStepping{}; /**< @brief Optional simulation substep count override; when set, maps to `Scene.CollisionSteps`. */

    bool TickInFixedTick = true; /**< @brief When true, world fixed tick advances the physics scene. */
    bool TickInVariableTick = false; /**< @brief When true, world variable tick advances the physics scene. */

    bool EnableFloatingOrigin = true; /**< @brief Use world->physics position offsetting to keep simulation near local origin. */
    bool AutoRebaseFloatingOrigin = false; /**< @brief Allow automatic rebasing when anchor point drifts beyond threshold. */
    SnAPI::Physics::Scalar FloatingOriginRebaseDistance = static_cast<SnAPI::Physics::Scalar>(512.0); /**< @brief Rebase distance threshold in world units. */
    bool InitializeFloatingOriginFromFirstBody = true; /**< @brief Initialize floating origin from first world-position conversion call. */
    SnAPI::Physics::Vec3 InitialFloatingOrigin = SnAPI::Physics::Vec3::Zero(); /**< @brief Initial world origin when auto-init is disabled. */
};

/**
 * @brief World-owned adapter over SnAPI.Physics runtime/scene.
 */
class PhysicsSystem final : public ITaskDispatcher
{
public:
    using WorkTask = std::function<void(PhysicsSystem&)>;
    using CompletionTask = std::function<void(const TaskHandle&)>;
    using PhysicsEventListener = std::function<void(const SnAPI::Physics::PhysicsEvent&)>;
    using PhysicsEventListenerToken = std::uint64_t;
    using BodySleepListener = std::function<void(const SnAPI::Physics::PhysicsEvent&)>;
    using BodySleepListenerToken = std::uint64_t;

    /** @brief Construct an uninitialized physics system. */
    PhysicsSystem() = default;
    /** @brief Destructor; releases owned scene/runtime state. */
    ~PhysicsSystem() = default;

    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    PhysicsSystem(PhysicsSystem&& Other) noexcept;
    PhysicsSystem& operator=(PhysicsSystem&& Other) noexcept;

    /**
     * @brief Initialize physics runtime and world scene.
     * @param Settings Physics bootstrap settings.
     * @return Success or error.
     */
    Result Initialize(const PhysicsBootstrapSettings& Settings);

    /**
     * @brief Shutdown physics scene/runtime resources.
     */
    void Shutdown();

    /**
     * @brief Check whether the scene is initialized.
     */
    bool IsInitialized() const;

    /**
     * @brief Step simulation and fetch results.
     * @param DeltaSeconds Simulation step.
     * @return Success or error.
     */
    Result Step(float DeltaSeconds);

    /**
     * @brief Enqueue work on the physics system thread.
     * @param InTask Work callback executed on physics-thread affinity.
     * @param OnComplete Optional completion callback marshaled to caller dispatcher.
     * @return Task handle for wait/cancel polling.
     */
    TaskHandle EnqueueTask(WorkTask InTask, CompletionTask OnComplete = {});

    /**
     * @brief Enqueue a generic thread task for dispatcher marshalling.
     * @param InTask Callback to execute on this system thread.
     */
    void EnqueueThreadTask(std::function<void()> InTask) override;

    /**
     * @brief Execute all queued tasks on the physics thread.
     */
    void ExecuteQueuedTasks();

    /**
     * @brief Drain physics events from the active scene.
     * @param OutEvents Destination span.
     * @return Number of drained events.
     */
    std::uint32_t DrainEvents(std::span<SnAPI::Physics::PhysicsEvent> OutEvents);

    /**
     * @brief Register a callback invoked for physics events after each step.
     * @param Listener Callback receiving each drained physics event.
     * @return Listener token used for removal.
     */
    PhysicsEventListenerToken AddEventListener(PhysicsEventListener Listener);

    /**
     * @brief Remove a previously registered physics event listener.
     * @param Token Listener token from `AddEventListener`.
     * @return True when a listener was removed.
     */
    bool RemoveEventListener(PhysicsEventListenerToken Token);

    /**
     * @brief Register a callback for sleep/wake events affecting a specific body.
     * @param BodyHandle Body to route sleep/wake events for.
     * @param Listener Callback invoked for matching body sleep/wake events.
     * @return Listener token used for removal, or 0 when registration fails.
     */
    BodySleepListenerToken AddBodySleepListener(SnAPI::Physics::BodyHandle BodyHandle, BodySleepListener Listener);

    /**
     * @brief Remove a previously registered body sleep listener.
     * @param Token Listener token from `AddBodySleepListener`.
     * @return True when a listener was removed.
     */
    bool RemoveBodySleepListener(BodySleepListenerToken Token);

    /**
     * @brief Access active scene.
     * @return Scene pointer or nullptr.
     */
    SnAPI::Physics::IPhysicsScene* Scene();
    const SnAPI::Physics::IPhysicsScene* Scene() const;

    /**
     * @brief Access effective bootstrap settings.
     */
    const PhysicsBootstrapSettings& Settings() const
    {
        return m_settings;
    }

    /**
     * @brief Check if fixed tick should step physics.
     */
    bool TickInFixedTick() const
    {
        return m_settings.TickInFixedTick;
    }

    /**
     * @brief Check if variable tick should step physics.
     */
    bool TickInVariableTick() const
    {
        return m_settings.TickInVariableTick;
    }

    /**
     * @brief Convert world-space position to physics-local space.
     * @param WorldPosition Input world position.
     * @param AllowInitializeOrigin When true, may initialize floating origin from this point.
     * @return Physics-local position.
     */
    SnAPI::Physics::Vec3 WorldToPhysicsPosition(const SnAPI::Physics::Vec3& WorldPosition, bool AllowInitializeOrigin = true);

    /**
     * @brief Convert physics-local position back to world space.
     * @param PhysicsPosition Input physics-local position.
     * @return World position.
     */
    SnAPI::Physics::Vec3 PhysicsToWorldPosition(const SnAPI::Physics::Vec3& PhysicsPosition) const;

    /**
     * @brief Ensure floating origin stays near a world-space anchor.
     * @param WorldAnchor Anchor world position.
     * @return True when origin was initialized or rebased.
     */
    bool EnsureFloatingOriginNear(const SnAPI::Physics::Vec3& WorldAnchor);

    /**
     * @brief Rebase floating origin to a specific world-space origin.
     * @param NewWorldOrigin New world-space origin.
     * @return True when origin changed and bodies were rebased.
     */
    bool RebaseFloatingOrigin(const SnAPI::Physics::Vec3& NewWorldOrigin);

    /**
     * @brief Get current floating origin in world space.
     * @return World-space origin offset.
     */
    SnAPI::Physics::Vec3 FloatingOriginWorld() const;

    /**
     * @brief Check whether floating origin has been initialized.
     * @return True when origin is initialized.
     */
    bool HasFloatingOrigin() const;

private:
    struct BodySleepListenerEntry
    {
        std::uint64_t BodyHandleValue = 0; /**< @brief Raw physics body handle value used for routing. */
        BodySleepListener Listener{}; /**< @brief Callback for matching body sleep/wake events. */
    };

    static Error MapPhysicsError(const SnAPI::Physics::Error& ErrorValue);
    bool RebaseFloatingOriginUnlocked(const SnAPI::Physics::Vec3& NewWorldOrigin);

    mutable GameMutex m_mutex{}; /**< @brief Physics-system thread affinity guard. */
    TSystemTaskQueue<PhysicsSystem> m_taskQueue{}; /**< @brief Cross-thread task handoff queue (real lock only on enqueue). */
    SnAPI::Physics::PhysicsRuntime m_runtime{}; /**< @brief Owned backend registry/runtime facade. */
    std::unique_ptr<SnAPI::Physics::IPhysicsScene> m_scene{}; /**< @brief Active world scene instance. */
    PhysicsBootstrapSettings m_settings{}; /**< @brief Active settings snapshot. */
    std::vector<SnAPI::Physics::PhysicsEvent> m_pendingEvents{}; /**< @brief Pending drained events not yet consumed by callers. */
    std::unordered_map<PhysicsEventListenerToken, PhysicsEventListener> m_eventListeners{}; /**< @brief Registered post-step event listeners. */
    PhysicsEventListenerToken m_nextEventListenerToken = 1; /**< @brief Monotonic listener token generator. */
    std::unordered_map<BodySleepListenerToken, BodySleepListenerEntry> m_bodySleepListeners{}; /**< @brief Body-scoped sleep listener entries keyed by token. */
    std::unordered_map<std::uint64_t, std::vector<BodySleepListenerToken>> m_bodySleepListenerTokensByBody{}; /**< @brief Listener-token lists per body handle. */
    BodySleepListenerToken m_nextBodySleepListenerToken = 1; /**< @brief Monotonic body sleep listener token generator. */
    SnAPI::Physics::Vec3 m_floatingOriginWorld = SnAPI::Physics::Vec3::Zero(); /**< @brief Current floating-origin world offset. */
    bool m_hasFloatingOrigin = false; /**< @brief True when floating origin has been initialized. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
