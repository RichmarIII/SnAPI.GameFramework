#pragma once

#include <string>
#include <vector>

#include "IWorld.h"
#include "JobSystem.h"
#include "Level.h"
#include "NodeGraph.h"

namespace SnAPI::GameFramework
{

/**
 * @brief World implementation that is also a NodeGraph.
 * @remarks Worlds are root tick objects and own levels.
 */
class World : public NodeGraph, public IWorld
{
public:
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

    /**
     * @brief Per-frame tick.
     * @param DeltaSeconds Time since last tick.
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
     */
    void EndFrame() override;

    /**
     * @brief Create a level as a child node.
     * @param Name Level name.
     * @return Handle to the created level or error.
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
     */
    std::vector<NodeHandle> Levels() const;
    /**
     * @brief Access the job system for parallel internal tasks.
     * @return Reference to JobSystem.
     */
    JobSystem& Jobs();

private:
    JobSystem m_jobSystem{}; /**< @brief Internal job system. */
};

} // namespace SnAPI::GameFramework
