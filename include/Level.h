#pragma once

#include <memory>
#include <string>

#include "ILevel.h"
#include "NodeGraph.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Level implementation that is also a NodeGraph.
 * @remarks Levels can be serialized and nested like any graph.
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
     */
    void EndFrame() override;

    /**
     * @brief Create a child node graph in this level.
     * @param Name Graph name.
     * @return Handle to the created graph or error.
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
