#include "Level.h"
#include "Profiling.h"

namespace SnAPI::GameFramework
{

Level::Level()
    : NodeGraph("Level")
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    TypeKey(StaticTypeId<Level>());
}

Level::Level(std::string Name)
    : NodeGraph(std::move(Name))
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    TypeKey(StaticTypeId<Level>());
}

void Level::Tick(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    NodeGraph::Tick(DeltaSeconds);
}

void Level::FixedTick(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    NodeGraph::FixedTick(DeltaSeconds);
}

void Level::LateTick(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    NodeGraph::LateTick(DeltaSeconds);
}

void Level::EndFrame()
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    NodeGraph::EndFrame();
}

TExpected<NodeHandle> Level::CreateGraph(std::string Name)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    return CreateNode<NodeGraph>(std::move(Name));
}

TExpectedRef<NodeGraph> Level::Graph(NodeHandle Handle)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
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
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    return *this;
}

const NodeGraph& Level::RootGraph() const
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    return *this;
}

} // namespace SnAPI::GameFramework
