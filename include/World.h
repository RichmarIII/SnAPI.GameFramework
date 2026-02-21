#pragma once

#include <functional>
#include <string>
#include <vector>

#include "GameThreading.h"
#include "IWorld.h"
#include "JobSystem.h"
#include "Level.h"
#include "NodeGraph.h"
#if defined(SNAPI_GF_ENABLE_INPUT)
#include "InputSystem.h"
#endif
#if defined(SNAPI_GF_ENABLE_UI)
#include "UISystem.h"
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
#include "AudioSystem.h"
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetworkSystem.h"
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "PhysicsSystem.h"
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "RendererSystem.h"
#endif

namespace SnAPI::GameFramework
{

class GameplayHost;

/**
 * @brief World frame-phase execution policy.
 * @remarks
 * This allows editor/runtime/PIE worlds to share one implementation while
 * selectively enabling simulation and subsystem phases.
 */
struct WorldExecutionProfile
{
    bool RunGameplay = true; /**< @brief Run high-level gameplay host tick. */
    bool TickInput = true; /**< @brief Pump world input in variable tick. */
    bool TickUI = true; /**< @brief Tick world UI contexts in variable tick. */
    bool PumpNetworking = true; /**< @brief Pump networking queues/sessions each frame. */
    bool TickNodeGraph = true; /**< @brief Run node/component tick traversal phases. */
    bool TickPhysicsSimulation = true; /**< @brief Advance physics simulation in variable/fixed phases. */
    bool AllowPhysicsQueries = true; /**< @brief Allow query-only physics access even when simulation is disabled. */
    bool TickAudio = true; /**< @brief Update world audio subsystem. */
    bool RunNodeEndFrame = true; /**< @brief Run node/component end-frame flush. */
    bool BuildUiRenderPackets = true; /**< @brief Build UI packets and queue to renderer. */
    bool RenderFrame = true; /**< @brief Submit renderer end-frame. */

    /**
     * @brief Runtime/game defaults.
     */
    [[nodiscard]] static WorldExecutionProfile Runtime();
    /**
     * @brief Editor defaults.
     * @remarks
     * Physics simulation is disabled while collider/query data remains available.
     */
    [[nodiscard]] static WorldExecutionProfile Editor();
    /**
     * @brief PIE defaults.
     * @remarks Equivalent to runtime defaults.
     */
    [[nodiscard]] static WorldExecutionProfile PIE();
};

/**
 * @brief Concrete world root that owns levels and subsystems.
 * @remarks
 * `World` is the top-level runtime orchestration object:
 * - derives from `NodeGraph` for hierarchical node traversal
 * - implements `IWorld` for level/subsystem contracts
 * - owns subsystem instances (job system + optional input/ui/audio/networking/
 *   physics/renderer adapters)
 *
 * Responsibility boundaries:
 * - world controls frame lifecycle and end-of-frame flush
 * - levels are represented as child nodes/graphs under the world
 * - nodes/components can query world context through `Owner()->World()`
 */
class World : public NodeGraph, public IWorld, public ITaskDispatcher
{
public:
    using WorkTask = std::function<void(World&)>;
    using CompletionTask = std::function<void(const TaskHandle&)>;

    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::World";

    /**
     * @brief Construct a world with default name.
     */
    World();
    /**
     * @brief Construct a world with a name.
     * @param Name World name.
     */
    explicit World(std::string Name);
    ~World() override;
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;

    EWorldKind Kind() const override;
    bool ShouldRunGameplay() const override;
    bool ShouldTickInput() const override;
    bool ShouldTickUI() const override;
    bool ShouldPumpNetworking() const override;
    bool ShouldTickNodeGraph() const override;
    bool ShouldSimulatePhysics() const override;
    bool ShouldAllowPhysicsQueries() const override;
    bool ShouldTickAudio() const override;
    bool ShouldRunNodeEndFrame() const override;
    bool ShouldBuildUiRenderPackets() const override;
    bool ShouldRenderFrame() const override;
    void SetWorldKind(EWorldKind Kind);
    const WorldExecutionProfile& ExecutionProfile() const;
    void SetExecutionProfile(const WorldExecutionProfile& Profile);

    /**
     * @brief Enqueue work on the world (game) thread.
     * @param InTask Work callback executed on world-thread affinity.
     * @param OnComplete Optional completion callback marshaled to caller dispatcher.
     * @return Task handle for wait/cancel polling.
     */
    TaskHandle EnqueueTask(WorkTask InTask, CompletionTask OnComplete = {});

    /**
     * @brief Enqueue a generic thread task for dispatcher marshalling.
     * @param InTask Callback to execute on the world thread.
     */
    void EnqueueThreadTask(std::function<void()> InTask) override;

    /**
     * @brief Execute all queued tasks on the world thread.
     */
    void ExecuteQueuedTasks();

    /**
     * @brief Per-frame tick.
     * @param DeltaSeconds Time since last tick.
     * @remarks
     * When input is enabled and initialized, this pumps one input frame before
     * node tick traversal so gameplay code can consume current frame state.
     * When UI is enabled and initialized, this ticks world UI before graph
     * traversal.
     * When networking is enabled and a session is attached, this pumps the
     * session before graph traversal.
     * When audio is enabled, this updates the world audio subsystem after
     * node/component tick traversal.
     */
    void Tick(float DeltaSeconds) override;
    /**
     * @brief Fixed-step tick.
     * @param DeltaSeconds Fixed time step.
     */
    void FixedTick(float DeltaSeconds) override;
    /**
     * @brief Late tick.
     * @param DeltaSeconds Time since last tick.
     */
    void LateTick(float DeltaSeconds) override;
    /**
     * @brief End-of-frame processing.
     * @remarks
     * Executes graph end-frame flush and subsystem post-frame maintenance.
     * When networking is enabled and a session is attached, this pumps the
     * session again to flush post-tick outbound traffic.
     */
    void EndFrame() override;

    /**
     * @brief Check whether fixed-step simulation is enabled for this frame.
     * @return True when fixed-step simulation is active.
     */
    bool FixedTickEnabled() const override;
    /**
     * @brief Get fixed-step delta used by runtime this frame.
     * @return Fixed-step interval in seconds (0 when disabled).
     */
    float FixedTickDeltaSeconds() const override;
    /**
     * @brief Get render interpolation alpha between fixed samples.
     * @return Alpha in [0, 1].
     */
    float FixedTickInterpolationAlpha() const override;

    /**
     * @brief Update runtime fixed-step timing snapshot consumed by components/systems.
     * @param Enabled True when fixed simulation is active.
     * @param FixedDeltaSeconds Active fixed-step interval in seconds.
     * @param InterpolationAlpha Current interpolation alpha between fixed samples.
     * @remarks
     * This is updated by `GameRuntime` before `World::Tick` each frame.
     */
    void SetFixedTickFrameState(bool Enabled, float FixedDeltaSeconds, float InterpolationAlpha);

    /**
     * @brief Create a level as a child node.
     * @param Name Level name.
     * @return Handle to the created level or error.
     * @remarks Created level is inserted into world graph and participates in world tick traversal.
     */
    TExpected<NodeHandle> CreateLevel(std::string Name) override;
    /**
     * @brief Access a level by handle.
     * @param Handle Level handle.
     * @return Reference wrapper or error.
     */
    TExpectedRef<Level> LevelRef(NodeHandle Handle) override;
    /**
     * @brief Get all level handles.
     * @return Vector of level handles.
     * @remarks Snapshot of currently attached level-node handles.
     */
    std::vector<NodeHandle> Levels() const;

    /**
     * @brief Set gameplay host pointer associated with this world runtime.
     * @remarks Non-owning and managed by `GameRuntime`.
     */
    void SetGameplayHost(GameplayHost* Host);

    /**
     * @brief Access gameplay host pointer associated with this world runtime.
     */
    GameplayHost* GameplayHostPtr();

    /**
     * @brief Access gameplay host pointer associated with this world runtime (const).
     */
    const GameplayHost* GameplayHostPtr() const;
    /**
     * @brief Access the job system for parallel internal tasks.
     * @return Reference to JobSystem.
     * @remarks Current implementation is minimal but provides a stable integration point.
     */
    JobSystem& Jobs();

#if defined(SNAPI_GF_ENABLE_INPUT)
    /**
     * @brief Access the input system for this world.
     * @return Reference to InputSystem.
     */
    InputSystem& Input() override;
    /**
     * @brief Access the input system for this world (const).
     * @return Const reference to InputSystem.
     */
    const InputSystem& Input() const override;
#endif

#if defined(SNAPI_GF_ENABLE_UI)
    /**
     * @brief Access the UI system for this world.
     * @return Reference to UISystem.
     */
    UISystem& UI() override;
    /**
     * @brief Access the UI system for this world (const).
     * @return Const reference to UISystem.
     */
    const UISystem& UI() const override;
#endif

#if defined(SNAPI_GF_ENABLE_AUDIO)
    /**
     * @brief Access the audio system for this world.
     * @return Reference to AudioSystem.
     */
    AudioSystem& Audio() override;
    /**
     * @brief Access the audio system for this world (const).
     * @return Const reference to AudioSystem.
     */
    const AudioSystem& Audio() const override;
#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    /**
     * @brief Access world networking subsystem.
     * @return Reference to NetworkSystem.
     */
    NetworkSystem& Networking() override;
    /**
     * @brief Access world networking subsystem (const).
     * @return Const reference to NetworkSystem.
     */
    const NetworkSystem& Networking() const override;
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    /**
     * @brief Access world physics subsystem.
     * @return Reference to PhysicsSystem.
     */
    PhysicsSystem& Physics() override;
    /**
     * @brief Access world physics subsystem (const).
     * @return Const reference to PhysicsSystem.
     */
    const PhysicsSystem& Physics() const override;
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
    /**
     * @brief Access world renderer subsystem.
     * @return Reference to RendererSystem.
     */
    RendererSystem& Renderer() override;
    /**
     * @brief Access world renderer subsystem (const).
     * @return Const reference to RendererSystem.
     */
    const RendererSystem& Renderer() const override;
#endif

private:
    mutable GameMutex m_threadMutex{}; /**< @brief World-thread affinity guard for queued task execution. */
    TSystemTaskQueue<World> m_taskQueue{}; /**< @brief Cross-thread task handoff queue for world-thread callbacks. */
    JobSystem m_jobSystem{}; /**< @brief World-scoped job dispatch facade for framework/runtime tasks. */
    GameplayHost* m_gameplayHost = nullptr; /**< @brief Non-owning gameplay host pointer for runtime bridge access. */
#if defined(SNAPI_GF_ENABLE_INPUT)
    InputSystem m_inputSystem{}; /**< @brief World-scoped input subsystem instance. */
#endif
#if defined(SNAPI_GF_ENABLE_UI)
    UISystem m_uiSystem{}; /**< @brief World-scoped UI subsystem instance. */
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
    AudioSystem m_audioSystem{}; /**< @brief World-scoped audio subsystem instance. */
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    NetworkSystem m_networkSystem; /**< @brief World-scoped networking subsystem with replication/RPC bridges. */
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    PhysicsSystem m_physicsSystem{}; /**< @brief World-scoped physics subsystem. */
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
    RendererSystem m_rendererSystem{}; /**< @brief World-scoped renderer subsystem. */
#endif
    EWorldKind m_worldKind = EWorldKind::Runtime; /**< @brief Role/classification of this world instance. */
    WorldExecutionProfile m_executionProfile{}; /**< @brief Per-world frame-phase execution policy. */
    bool m_fixedTickEnabled = false; /**< @brief Runtime fixed-step enable state for current frame. */
    float m_fixedTickDeltaSeconds = 0.0f; /**< @brief Runtime fixed-step interval snapshot for current frame. */
    float m_fixedTickInterpolationAlpha = 1.0f; /**< @brief Runtime interpolation alpha between fixed samples for current frame. */
};

} // namespace SnAPI::GameFramework
