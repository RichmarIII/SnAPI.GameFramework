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
#include "TypeName.h"
#include "ReflectionAnnotations.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Bootstrap settings for world-owned physics.
 *
 * `PhysicsBootstrapSettings` captures how `PhysicsSystem` creates and advances its
 * scene, including backend scene descriptors, tick integration policy, and optional
 * floating-origin behavior used to keep large worlds numerically stable.
 *
 * Core semantics:
 * - `Scene`, `Routing`, and `Couplings` are forwarded into scene creation
 * - `ThreadCount` and `MaxSubStepping` override fields on the effective scene descriptor
 * - fixed/variable tick flags only describe when the world should call `Step(...)`; they do not schedule ticks themselves
 * - floating-origin settings control world/physics coordinate conversion and scene rebasing
 *
 * Units:
 * - `FloatingOriginRebaseDistance` is expressed in world units used by the physics backend
 *
 * @see PhysicsSystem
 */
SnType()
struct PhysicsBootstrapSettings
{
    SnAPI::Physics::PhysicsSceneDesc Scene{}; /**< @brief Scene descriptor used for backend scene creation. */
    SnAPI::Physics::SceneRoutingDesc Routing{}; /**< @brief Backend routing per physics domain. */
    std::vector<SnAPI::Physics::CouplingDesc> Couplings{}; /**< @brief Optional inter-domain coupling descriptors. */

    SnField(SnKey("ThreadCount"))
    std::uint32_t ThreadCount{0}; /**< @brief Optional physics worker-thread override (0 = use scene/default backend behavior). */
    std::optional<std::uint32_t> MaxSubStepping{}; /**< @brief Optional simulation substep count override; when set, maps to `Scene.CollisionSteps`. */

    SnField(SnKey("TickInFixedTick"))
    bool TickInFixedTick{true}; /**< @brief When true, world fixed tick advances the physics scene. */
    SnField(SnKey("TickInVariableTick"))
    bool TickInVariableTick{false}; /**< @brief When true, world variable tick advances the physics scene. */

    SnField(SnKey("EnableFloatingOrigin"))
    bool EnableFloatingOrigin{true}; /**< @brief Use world->physics position offsetting to keep simulation near local origin. */
    SnField(SnKey("AutoRebaseFloatingOrigin"))
    bool AutoRebaseFloatingOrigin{true}; /**< @brief Allow automatic rebasing when anchor point drifts beyond threshold. */
    SnField(SnKey("FloatingOriginRebaseDistance"))
    SnAPI::Physics::Scalar FloatingOriginRebaseDistance = static_cast<SnAPI::Physics::Scalar>(512.0); /**< @brief Rebase distance threshold in world units. */
    SnField(SnKey("InitializeFloatingOriginFromFirstBody"))
    bool InitializeFloatingOriginFromFirstBody{true}; /**< @brief Initialize floating origin from first world-position conversion call. */
    SnField(SnKey("InitialFloatingOrigin"))
    SnAPI::Physics::Vec3 InitialFloatingOrigin{SnAPI::Physics::Vec3::Zero()}; /**< @brief Initial world origin when auto-init is disabled. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief World-owned adapter over SnAPI.Physics runtime and scene.
 *
 * `PhysicsSystem` owns one active physics scene for a `World` and is responsible for
 * stepping simulation, draining physics events, and translating between world space and
 * physics-local space when floating-origin mode is enabled.
 *
 * Why this abstraction exists:
 * - to bind physics lifetime to world lifetime
 * - to present a stable scene/event API to gameplay code without exposing backend setup details
 * - to centralize floating-origin rebasing so components can convert coordinates consistently
 *
 * Core semantics:
 * - `Initialize(...)` replaces any previous scene and resets listener/token state
 * - `Step(...)` simulates, fetches results, drains scene events, stores a pending event queue, and then notifies listeners
 * - general event listeners observe all drained events from each step
 * - body sleep listeners only observe `BodySleep` and `BodyWake` events for their registered body
 * - `DrainEvents(...)` consumes the subsystem-owned pending queue populated by prior `Step(...)` calls
 *
 * Ownership and lifetime:
 * - Owned by `World`.
 * - Owns the backend runtime facade and the active scene instance.
 * - Returned scene pointers are borrowed and invalidated by `Shutdown()` or reinitialization.
 *
 * Threading model:
 * - Main-thread oriented for simulation and listener registration.
 * - Cross-thread work should be marshaled via `EnqueueTask(...)`.
 *
 * Performance notes:
 * - `Step(...)` may dispatch many callbacks proportional to drained event count.
 * - `DrainEvents(...)` erases consumed events from an internal vector.
 *
 * @see World
 * @see PhysicsBootstrapSettings
 */
SnType()
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
     * @param Settings Physics bootstrap settings copied into the subsystem.
     * @return Success or error.
     * @post On success, a backend scene exists and listener state is reset to empty.
     * @warning Replaces any previously initialized scene.
     */
    Result Initialize(const PhysicsBootstrapSettings& Settings);

    /**
     * @brief Shutdown physics scene/runtime resources.
     * @remarks Clears pending events, listeners, and floating-origin state.
     */
    void Shutdown();

    /**
     * @brief Check whether the scene is initialized.
     */
    SnFunction(SnKey("IsInitialized"))
    bool IsInitialized() const;

    /**
     * @brief Step simulation and fetch results.
     * @param DeltaSeconds Simulation step in seconds. Must be greater than zero.
     * @return Success or error.
     * @post On success, pending events and registered listeners reflect the events drained from this step.
     * @warning Listener callbacks run after the simulation lock is released.
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
     * @brief Drain queued physics events captured by prior `Step(...)` calls.
     * @param OutEvents Destination span.
     * @return Number of events copied into @p OutEvents.
     * @remarks This consumes from the subsystem-owned pending-event queue, not directly from the backend scene.
     */
    std::uint32_t DrainEvents(std::span<SnAPI::Physics::PhysicsEvent> OutEvents);

    /**
     * @brief Register a callback invoked for all physics events after each step.
     * @param Listener Callback receiving each drained physics event.
     * @return Listener token used for removal.
     * @remarks Tokens are monotonic within the lifetime of one initialized system instance.
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
     * @return Listener token used for removal, or `0` when registration fails.
     * @remarks Only `BodySleep` and `BodyWake` events participate in this routing path.
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
     * @return Non-owning scene pointer or `nullptr`.
     * @warning The returned pointer is invalidated by `Shutdown()` or successful reinitialization.
     */
    SnAPI::Physics::IPhysicsScene* Scene();
    /**
     * @brief Access active scene (const).
     * @return Non-owning scene pointer or `nullptr`.
     * @warning The returned pointer is invalidated by `Shutdown()` or successful reinitialization.
     */
    const SnAPI::Physics::IPhysicsScene* Scene() const;

    /**
     * @brief Access effective bootstrap settings.
     * @return Borrowed reference to the active settings snapshot.
     */
    SnFunction(SnKey("Settings"))
    const PhysicsBootstrapSettings& Settings() const
    {
        return m_settings;
    }

    /**
     * @brief Check whether fixed-tick world updates should advance physics.
     * @return `true` when fixed tick is the configured stepping path.
     */
    SnFunction(SnKey("TickInFixedTick"))
    bool TickInFixedTick() const
    {
        return m_settings.TickInFixedTick;
    }

    /**
     * @brief Check whether variable-tick world updates should advance physics.
     * @return `true` when variable tick is the configured stepping path.
     */
    SnFunction(SnKey("TickInVariableTick"))
    bool TickInVariableTick() const
    {
        return m_settings.TickInVariableTick;
    }

    /**
     * @brief Convert world-space position to physics-local space.
     * @param WorldPosition Input world position.
     * @param AllowInitializeOrigin When `true`, the first call may initialize the floating origin from @p WorldPosition.
     * @return Physics-local position.
     * @remarks Returns @p WorldPosition unchanged when floating origin is disabled.
     */
    SnFunction(SnKey("WorldToPhysicsPosition"))
    SnAPI::Physics::Vec3 WorldToPhysicsPosition(const SnAPI::Physics::Vec3& WorldPosition, bool AllowInitializeOrigin = true);

    /**
     * @brief Convert physics-local position back to world space.
     * @param PhysicsPosition Input physics-local position.
     * @return World position.
     * @remarks Returns @p PhysicsPosition unchanged when floating origin is disabled.
     */
    SnFunction(SnKey("PhysicsToWorldPosition"))
    SnAPI::Physics::Vec3 PhysicsToWorldPosition(const SnAPI::Physics::Vec3& PhysicsPosition) const;

    /**
     * @brief Ensure floating origin stays near a world-space anchor.
     * @param WorldAnchor Anchor world position.
     * @return `true` when the origin was initialized or rebased.
     * @remarks No-op when floating origin or automatic rebasing is disabled.
     */
    SnFunction(SnKey("EnsureFloatingOriginNear"))
    bool EnsureFloatingOriginNear(const SnAPI::Physics::Vec3& WorldAnchor);

    /**
     * @brief Rebase floating origin to a specific world-space origin.
     * @param NewWorldOrigin New world-space origin.
     * @return `true` when the origin changed.
     * @remarks
     * When a scene exists, this attempts to shift backend bodies by the origin delta.
     * When no scene exists yet, it only updates the stored origin state.
     */
    SnFunction(SnKey("RebaseFloatingOrigin"))
    bool RebaseFloatingOrigin(const SnAPI::Physics::Vec3& NewWorldOrigin);

    /**
     * @brief Get current floating origin in world space.
     * @return World-space origin offset.
     */
    SnFunction(SnKey("FloatingOriginWorld"))
    SnAPI::Physics::Vec3 FloatingOriginWorld() const;

    /**
     * @brief Check whether floating origin has been initialized.
     * @return True when origin is initialized.
     */
    SnFunction(SnKey("HasFloatingOrigin"))
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

SNAPI_DEFINE_TYPE_NAME(PhysicsSystem, "SnAPI::GameFramework::PhysicsSystem")
SNAPI_DEFINE_TYPE_NAME(PhysicsBootstrapSettings, "SnAPI::GameFramework::PhysicsBootstrapSettings")

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_PHYSICS
