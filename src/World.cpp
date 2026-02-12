#include "World.h"

namespace SnAPI::GameFramework
{

World::World()
    : NodeGraph("World")
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    , m_networkSystem(*this)
#endif
{
    TypeKey(StaticTypeId<World>());
    BaseNode::World(this);
}

World::World(std::string Name)
    : NodeGraph(std::move(Name))
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    , m_networkSystem(*this)
#endif
{
    TypeKey(StaticTypeId<World>());
    BaseNode::World(this);
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

#if defined(SNAPI_GF_ENABLE_AUDIO)
AudioSystem& World::Audio()
{
    return m_audioSystem;
}

const AudioSystem& World::Audio() const
{
    return m_audioSystem;
}
#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)
NetworkSystem& World::Networking()
{
    return m_networkSystem;
}

const NetworkSystem& World::Networking() const
{
    return m_networkSystem;
}
#endif

} // namespace SnAPI::GameFramework
