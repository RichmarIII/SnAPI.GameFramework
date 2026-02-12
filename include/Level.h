#pragma once

#include <memory>
#include <string>

#include "ILevel.h"
#include "NodeGraph.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Concrete level implementation backed by NodeGraph.
 * @remarks
 * `Level` is a thin semantic layer over `NodeGraph` used by worlds to represent
 * gameplay partitions. It preserves full graph capabilities (hierarchy, components,
 * serialization) while satisfying `ILevel`.
 */
class Level : public NodeGraph, public ILevel
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Level";

    /**
     * @brief Construct a level with default name.
     */
    Level();
    /**
     * @brief Construct a level with a name.
     * @param Name Level name.
     */
    explicit Level(std::string Name);

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
     * @remarks Flushes deferred destruction for all nodes/components in this level graph.
     */
    void EndFrame() override;

    /**
     * @brief Create a child node graph in this level.
     * @param Name Graph name.
     * @return Handle to the created graph or error.
     * @remarks Child graph node is owned by this level and inherits its world context.
     */
    TExpected<NodeHandle> CreateGraph(std::string Name) override;
    /**
     * @brief Access a child graph by handle.
     * @param Handle Graph handle.
     * @return Reference wrapper or error.
     */
    TExpectedRef<NodeGraph> Graph(NodeHandle Handle) override;

    /**
     * @brief Access the root graph.
     * @return Reference to the root graph.
     * @remarks Levels are themselves graphs; this returns *this.
     */
    NodeGraph& RootGraph();
    /**
     * @brief Access the root graph (const).
     * @return Const reference to the root graph.
     */
    const NodeGraph& RootGraph() const;
};

} // namespace SnAPI::GameFramework
