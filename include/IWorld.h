#pragma once

#include <cstdint>
#include <string>

#include "Expected.h"
#include "INode.h"

namespace SnAPI::GameFramework
{

/**
 * @brief High-level world role used by runtime/editor flows.
 */
enum class EWorldKind : std::uint8_t
{
    Runtime,
    Editor,
    PIE
};

class Level;
#if defined(SNAPI_GF_ENABLE_INPUT)
class InputSystem;
#endif
#if defined(SNAPI_GF_ENABLE_UI)
class UISystem;
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
class AudioSystem;
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
class NetworkSystem;
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
class PhysicsSystem;
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
class RendererSystem;
#endif

/**
 * @brief Root runtime container contract for gameplay sessions.
 * @remarks
 * A world is the top-level execution root that owns levels and optional subsystem
 * integrations (input/ui/audio/networking/physics/renderer). Worlds drive frame
 * lifecycle (`Tick`/`EndFrame`) and establish authoritative context for
 * contained node graphs.
 */
class IWorld
{
public:
    /** @brief Virtual destructor. */
    virtual ~IWorld() = default;

    /**
     * @brief World role classification.
     * @return Active world kind.
     */
    virtual EWorldKind Kind() const = 0;

    /**
     * @brief Whether high-level gameplay orchestration should run for this world.
     * @remarks
     * GameRuntime uses this to gate `GameplayHost::Tick`.
     */
    virtual bool ShouldRunGameplay() const = 0;
    /**
     * @brief Whether input pumping should run during variable tick.
     */
    virtual bool ShouldTickInput() const = 0;
    /**
     * @brief Whether UI context tick should run during variable tick.
     */
    virtual bool ShouldTickUI() const = 0;
    /**
     * @brief Whether networking queues/session pumps should run.
     */
    virtual bool ShouldPumpNetworking() const = 0;
    /**
     * @brief Whether node/component tick traversal should run.
     */
    virtual bool ShouldTickNodeGraph() const = 0;
    /**
     * @brief Whether physics simulation stepping should run.
     * @remarks
     * Physics queries can still be allowed independently via
     * `ShouldAllowPhysicsQueries()`.
     */
    virtual bool ShouldSimulatePhysics() const = 0;
    /**
     * @brief Whether physics query access should be considered valid.
     * @remarks
     * Editor worlds typically return true while `ShouldSimulatePhysics()` is false.
     */
    virtual bool ShouldAllowPhysicsQueries() const = 0;
    /**
     * @brief Whether audio subsystem update should run.
     */
    virtual bool ShouldTickAudio() const = 0;
    /**
     * @brief Whether node/component end-frame flush should run.
     */
    virtual bool ShouldRunNodeEndFrame() const = 0;
    /**
     * @brief Whether UI render packet generation/queueing should run.
     */
    virtual bool ShouldBuildUiRenderPackets() const = 0;
    /**
     * @brief Whether renderer end-frame submission should run.
     */
    virtual bool ShouldRenderFrame() const = 0;

    /**
     * @brief Per-frame tick.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void Tick(float DeltaSeconds) = 0;
    /**
     * @brief Fixed-step tick.
     * @param DeltaSeconds Fixed time step.
     */
    virtual void FixedTick(float DeltaSeconds) = 0;
    /**
     * @brief Late tick.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void LateTick(float DeltaSeconds) = 0;
    /**
     * @brief End-of-frame processing.
     * @remarks Flushes deferred destruction queues and finalizes frame-consistent state transitions.
     */
    virtual void EndFrame() = 0;

    /**
     * @brief Report whether the runtime currently drives a fixed-step simulation loop.
     * @return True when fixed-step simulation is enabled for the current frame.
     * @remarks
     * Components that interpolate fixed-step results for rendering should check this
     * first. When false, interpolation alpha should be treated as 1.
     */
    virtual bool FixedTickEnabled() const = 0;

    /**
     * @brief Get active fixed-step delta seconds.
     * @return Fixed simulation step interval in seconds (0 when fixed tick is disabled).
     * @remarks
     * This value is provided for systems that need deterministic step size metadata.
     */
    virtual float FixedTickDeltaSeconds() const = 0;

    /**
     * @brief Get current render interpolation alpha between fixed simulation samples.
     * @return Alpha in range [0, 1].
     * @remarks
     * Convention:
     * - 0 means "at previous fixed sample"
     * - 1 means "at current fixed sample"
     * When fixed tick is disabled this returns 1.
     */
    virtual float FixedTickInterpolationAlpha() const = 0;

    /**
     * @brief Create a level as a child node.
     * @param Name Level name.
     * @return Handle to the created level or error.
     * @remarks New levels are world-owned and participate in world tick traversal.
     */
    virtual TExpected<NodeHandle> CreateLevel(std::string Name) = 0;
    /**
     * @brief Access a level by handle.
     * @param Handle Level handle.
     * @return Reference wrapper or error.
     * @remarks Returns typed level reference if handle resolves and is level-compatible.
     */
    virtual TExpectedRef<Level> LevelRef(NodeHandle Handle) = 0;

#if defined(SNAPI_GF_ENABLE_INPUT)
    /**
     * @brief Access the input subsystem for this world.
     * @return Reference to InputSystem.
     */
    virtual InputSystem& Input() = 0;
    /**
     * @brief Access the input subsystem for this world (const).
     * @return Const reference to InputSystem.
     */
    virtual const InputSystem& Input() const = 0;
#endif

#if defined(SNAPI_GF_ENABLE_UI)
    /**
     * @brief Access the UI subsystem for this world.
     * @return Reference to UISystem.
     */
    virtual UISystem& UI() = 0;
    /**
     * @brief Access the UI subsystem for this world (const).
     * @return Const reference to UISystem.
     */
    virtual const UISystem& UI() const = 0;
#endif

#if defined(SNAPI_GF_ENABLE_AUDIO)
    /**
     * @brief Access the audio system for this world.
     * @return Reference to AudioSystem.
     */
    virtual AudioSystem& Audio() = 0;
    /**
     * @brief Access the audio system for this world (const).
     * @return Const reference to AudioSystem.
     */
    virtual const AudioSystem& Audio() const = 0;
#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    /**
     * @brief Access the networking subsystem for this world.
     * @return Reference to NetworkSystem.
     * @remarks World networking owns session bridge wiring for replication/RPC.
     */
    virtual NetworkSystem& Networking() = 0;
    /**
     * @brief Access the networking subsystem for this world (const).
     * @return Const reference to NetworkSystem.
     */
    virtual const NetworkSystem& Networking() const = 0;
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    /**
     * @brief Access the physics subsystem for this world.
     * @return Reference to PhysicsSystem.
     */
    virtual PhysicsSystem& Physics() = 0;
    /**
     * @brief Access the physics subsystem for this world (const).
     * @return Const reference to PhysicsSystem.
     */
    virtual const PhysicsSystem& Physics() const = 0;
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
    /**
     * @brief Access the renderer subsystem for this world.
     * @return Reference to RendererSystem.
     */
    virtual RendererSystem& Renderer() = 0;
    /**
     * @brief Access the renderer subsystem for this world (const).
     * @return Const reference to RendererSystem.
     */
    virtual const RendererSystem& Renderer() const = 0;
#endif
};

} // namespace SnAPI::GameFramework
