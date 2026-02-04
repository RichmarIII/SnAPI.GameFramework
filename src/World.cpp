#include "World.h"

namespace SnAPI::GameFramework
{

World::World()
    : NodeGraph("World")
{
    TypeKey(TypeIdFromName(kTypeName));
}

World::World(std::string Name)
    : NodeGraph(std::move(Name))
{
    TypeKey(TypeIdFromName(kTypeName));
}

void World::Tick(float DeltaSeconds)
{
    NodeGraph::Tick(DeltaSeconds);
}

void World::FixedTick(float DeltaSeconds)
{
    NodeGraph::FixedTick(DeltaSeconds);
}

void World::LateTick(float DeltaSeconds)
{
    NodeGraph::LateTick(DeltaSeconds);
}

void World::EndFrame()
{
    NodeGraph::EndFrame();
}

TExpected<NodeHandle> World::CreateLevel(std::string Name)
{
    return CreateNode<Level>(std::move(Name));
}

TExpectedRef<Level> World::LevelRef(NodeHandle Handle)
{
    if (auto* Node = Handle.Borrowed())
    {
        if (auto* LevelPtr = dynamic_cast<class Level*>(Node))
        {
            return *LevelPtr;
        }
    }
    return std::unexpected(MakeError(EErrorCode::NotFound, "Level not found"));
}

std::vector<NodeHandle> World::Levels() const
{
    std::vector<NodeHandle> Result;
    NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (dynamic_cast<Level*>(&Node))
        {
            Result.push_back(Handle);
        }
    });
    return Result;
}

JobSystem& World::Jobs()
{
    return m_jobSystem;
}

} // namespace SnAPI::GameFramework
