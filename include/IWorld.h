#pragma once

#include <string>

#include "Expected.h"
#include "INode.h"

namespace SnAPI::GameFramework
{

class Level;
#if defined(SNAPI_GF_ENABLE_AUDIO)
class AudioSystem;
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
class NetworkSystem;
#endif

/**
 * @brief Root runtime container contract for gameplay sessions.
 * @remarks
 * A world is the top-level execution root that owns levels and optional subsystem
 * integrations (audio/networking). Worlds drive frame lifecycle (`Tick`/`EndFrame`)
 * and establish authoritative context for contained node graphs.
 */
class IWorld
{
public:
    /** @brief Virtual destructor. */
    virtual ~IWorld() = default;

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
};

} // namespace SnAPI::GameFramework
