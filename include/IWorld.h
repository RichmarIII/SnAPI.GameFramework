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

/**
 * @brief Interface for world containers.
 * @remarks Worlds are the root tick objects and own levels.
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
     * @remarks Handles deferred destruction.
     */
    virtual void EndFrame() = 0;

    /**
     * @brief Create a level as a child node.
     * @param Name Level name.
     * @return Handle to the created level or error.
     */
    virtual TExpected<NodeHandle> CreateLevel(std::string Name) = 0;
    /**
     * @brief Access a level by handle.
     * @param Handle Level handle.
     * @return Reference wrapper or error.
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
};

} // namespace SnAPI::GameFramework
