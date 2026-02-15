#include "World.h"
#include "Profiling.h"
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <LinearAlgebra.hpp>
#include <ICamera.hpp>
#endif

namespace SnAPI::GameFramework
{

World::World()
    : NodeGraph("World")
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    , m_networkSystem(*this)
#endif
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    TypeKey(StaticTypeId<World>());
    BaseNode::World(this);
}

World::World(std::string Name)
    : NodeGraph(std::move(Name))
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    , m_networkSystem(*this)
#endif
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    TypeKey(StaticTypeId<World>());
    BaseNode::World(this);
}

World::~World()
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    // Ensure component OnDestroy paths run while world subsystems still exist.
    NodeGraph::Clear();
    BaseNode::World(nullptr);
}

void World::Tick(const float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("World");
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (auto* Session = m_networkSystem.Session())
    {
        SNAPI_GF_PROFILE_SCOPE("World.NetworkPump", "Networking");
        Session->Pump(Networking::Clock::now());
    }
#endif
    {
        SNAPI_GF_PROFILE_SCOPE("World.NodeGraphTick", "SceneGraph");
        NodeGraph::Tick(DeltaSeconds);
    }
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    if (m_physicsSystem.IsInitialized() && m_physicsSystem.TickInVariableTick())
    {
        SNAPI_GF_PROFILE_SCOPE("World.PhysicsVariableStep", "Physics");
        //(void)m_physicsSystem.Step(DeltaSeconds);
    }
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
    {
        SNAPI_GF_PROFILE_SCOPE("World.AudioUpdate", "Audio");
        m_audioSystem.Update(DeltaSeconds);
    }
#endif
}

void World::FixedTick(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    {
        SNAPI_GF_PROFILE_SCOPE("World.NodeGraphFixedTick", "SceneGraph");
        NodeGraph::FixedTick(DeltaSeconds);
    }
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    const bool RunPhysicsFixedStep = [&]() {
        SNAPI_GF_PROFILE_SCOPE("World.PhysicsFixedStepGate", "Physics");
        return m_physicsSystem.IsInitialized() && m_physicsSystem.TickInFixedTick();
    }();
    if (RunPhysicsFixedStep)
    {
        if (m_physicsSystem.Settings().AutoRebaseFloatingOrigin)
        {
#if defined(SNAPI_GF_ENABLE_RENDERER)
            if (const auto* ActiveCamera = m_rendererSystem.ActiveCamera())
            {
                {
                    SNAPI_GF_PROFILE_SCOPE("World.PhysicsFloatingOriginCheck", "Physics");
                    const auto CameraPos = ActiveCamera->Position();
                    const SnAPI::Physics::Vec3 AnchorWorld{
                        static_cast<SnAPI::Physics::Vec3::Scalar>(CameraPos.x()),
                        static_cast<SnAPI::Physics::Vec3::Scalar>(CameraPos.y()),
                        static_cast<SnAPI::Physics::Vec3::Scalar>(CameraPos.z())};
                    (void)m_physicsSystem.EnsureFloatingOriginNear(AnchorWorld);
                }
            }
#endif
        }
        SNAPI_GF_PROFILE_SCOPE("World.PhysicsFixedStep", "Physics");
        (void)m_physicsSystem.Step(DeltaSeconds);
    }
#endif
}

void World::LateTick(const float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    {
        SNAPI_GF_PROFILE_SCOPE("World.NodeGraphLateTick", "SceneGraph");
        NodeGraph::LateTick(DeltaSeconds);
    }
}

void World::EndFrame()
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    {
        SNAPI_GF_PROFILE_SCOPE("World.NodeGraphEndFrame", "SceneGraph");
        NodeGraph::EndFrame();
    }
#if defined(SNAPI_GF_ENABLE_RENDERER)
    {
        SNAPI_GF_PROFILE_SCOPE("World.RenderEndFrame", "Rendering");
        m_rendererSystem.EndFrame();
    }
#endif
}

TExpected<NodeHandle> World::CreateLevel(std::string Name)
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    return CreateNode<Level>(std::move(Name));
}

TExpectedRef<Level> World::LevelRef(NodeHandle Handle)
{
    SNAPI_GF_PROFILE_FUNCTION("World");
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
    SNAPI_GF_PROFILE_FUNCTION("World");
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
    SNAPI_GF_PROFILE_FUNCTION("World");
    return m_jobSystem;
}

#if defined(SNAPI_GF_ENABLE_AUDIO)
AudioSystem& World::Audio()
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    return m_audioSystem;
}

const AudioSystem& World::Audio() const
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    return m_audioSystem;
}
#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)
NetworkSystem& World::Networking()
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    return m_networkSystem;
}

const NetworkSystem& World::Networking() const
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    return m_networkSystem;
}
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
PhysicsSystem& World::Physics()
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    return m_physicsSystem;
}

const PhysicsSystem& World::Physics() const
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    return m_physicsSystem;
}
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
RendererSystem& World::Renderer()
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    return m_rendererSystem;
}

const RendererSystem& World::Renderer() const
{
    SNAPI_GF_PROFILE_FUNCTION("World");
    return m_rendererSystem;
}
#endif

} // namespace SnAPI::GameFramework
