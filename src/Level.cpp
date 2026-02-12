#include "Level.h"

namespace SnAPI::GameFramework
{

Level::Level()
    : NodeGraph("Level")
{
    TypeKey(StaticTypeId<Level>());
}

Level::Level(std::string Name)
    : NodeGraph(std::move(Name))
{
    TypeKey(StaticTypeId<Level>());
}

void Level::Tick(float DeltaSeconds)
{
    NodeGraph::Tick(DeltaSeconds);
}

void Level::FixedTick(float DeltaSeconds)
{
    NodeGraph::FixedTick(DeltaSeconds);
}

void Level::LateTick(float DeltaSeconds)
{
    NodeGraph::LateTick(DeltaSeconds);
}

void Level::EndFrame()
{
    NodeGraph::EndFrame();
}

TExpected<NodeHandle> Level::CreateGraph(std::string Name)
{
    return CreateNode<NodeGraph>(std::move(Name));
}

TExpectedRef<NodeGraph> Level::Graph(NodeHandle Handle)
{
    if (auto* Node = Handle.Borrowed())
    {
        if (auto* Graph = dynamic_cast<NodeGraph*>(Node))
        {
            return *Graph;
        }
    }
    return std::unexpected(MakeError(EErrorCode::NotFound, "Graph not found"));
}

NodeGraph& Level::RootGraph()
{
    return *this;
}

const NodeGraph& Level::RootGraph() const
{
    return *this;
}

} // namespace SnAPI::GameFramework
