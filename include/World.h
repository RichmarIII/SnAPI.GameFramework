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

/**
 * @brief Concrete world root that owns levels and subsystems.
 * @remarks
 * `World` is the top-level runtime orchestration object:
 * - derives from `NodeGraph` for hierarchical node traversal
 * - implements `IWorld` for level/subsystem contracts
 * - owns subsystem instances (job system + optional input/audio/networking/
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
#if defined(SNAPI_GF_ENABLE_INPUT)
    InputSystem m_inputSystem{}; /**< @brief World-scoped input subsystem instance. */
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
};

} // namespace SnAPI::GameFramework
