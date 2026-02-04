#pragma once

#include <memory>
#include <string>

#include "ILevel.h"
#include "NodeGraph.h"

namespace SnAPI::GameFramework
{

class Level : public NodeGraph, public ILevel
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Level";

    Level();
    explicit Level(std::string Name);

    void Tick(float DeltaSeconds) override;
    void FixedTick(float DeltaSeconds) override;
    void LateTick(float DeltaSeconds) override;
    void EndFrame() override;

    TExpected<NodeHandle> CreateGraph(std::string Name) override;
    TExpectedRef<NodeGraph> Graph(NodeHandle Handle) override;

    NodeGraph& RootGraph();
    const NodeGraph& RootGraph() const;
};

} // namespace SnAPI::GameFramework
