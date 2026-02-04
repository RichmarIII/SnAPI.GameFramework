#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Expected.h"
#include "Handle.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class NodeGraph;

/**
 * @brief Handle type for nodes (local alias).
 * @remarks This is equivalent to SnAPI::GameFramework::NodeHandle.
 */
using NodeHandle = THandle<BaseNode>;

/**
 * @brief Interface for level containers.
 * @remarks Levels are graphs nested under a world.
 */
class ILevel
{
public:
    /** @brief Virtual destructor. */
    virtual ~ILevel() = default;

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
     * @brief Create a child node graph.
     * @param Name Graph name.
     * @return Handle to the created graph or error.
     */
    virtual TExpected<NodeHandle> CreateGraph(std::string Name) = 0;
    /**
     * @brief Access a child graph by handle.
     * @param Handle Graph handle.
     * @return Reference wrapper or error.
     */
    virtual TExpectedRef<NodeGraph> Graph(NodeHandle Handle) = 0;
};

} // namespace SnAPI::GameFramework
