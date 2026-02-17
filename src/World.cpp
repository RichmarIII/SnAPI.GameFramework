#include "World.h"
#include "Profiling.h"
#include <algorithm>
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

World::~World()
{
    
    // Ensure component OnDestroy paths run while world subsystems still exist.
    NodeGraph::Clear();
    BaseNode::World(nullptr);
}

TaskHandle World::EnqueueTask(WorkTask InTask, CompletionTask OnComplete)
{
    return m_taskQueue.EnqueueTask(std::move(InTask), std::move(OnComplete));
}

void World::EnqueueThreadTask(std::function<void()> InTask)
{
    
    m_taskQueue.EnqueueThreadTask(std::move(InTask));
}

void World::ExecuteQueuedTasks()
{
    m_taskQueue.ExecuteQueuedTasks(*this, m_threadMutex);
}

void World::Tick(const float DeltaSeconds)
{
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
#if defined(SNAPI_GF_ENABLE_INPUT)
    if (m_inputSystem.IsInitialized())
    {
        (void)m_inputSystem.Pump();
    }
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    m_networkSystem.ExecuteQueuedTasks();
    if (auto* Session = m_networkSystem.Session())
    {
        
        Session->Pump(Networking::Clock::now());
    }
#endif
    {
        
        NodeGraph::Tick(DeltaSeconds);
    }
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    if (m_physicsSystem.IsInitialized() && m_physicsSystem.TickInVariableTick())
    {
        
        (void)m_physicsSystem.Step(DeltaSeconds);
    }
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
    {
        
        m_audioSystem.Update(DeltaSeconds);
    }
#endif
}

void World::FixedTick(float DeltaSeconds)
{

    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    {
        
        NodeGraph::FixedTick(DeltaSeconds);
    }
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    const bool RunPhysicsFixedStep = [&]() {
        
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
        
        (void)m_physicsSystem.Step(DeltaSeconds);
    }
#endif
}

void World::LateTick(const float DeltaSeconds)
{

    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    {
        
        NodeGraph::LateTick(DeltaSeconds);
    }
}

void World::EndFrame()
{

    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    m_networkSystem.ExecuteQueuedTasks();
#endif
    {
        
        NodeGraph::EndFrame();
    }
#if defined(SNAPI_GF_ENABLE_RENDERER)
    {
        
        m_rendererSystem.EndFrame();
    }
#endif
}

bool World::FixedTickEnabled() const
{
    return m_fixedTickEnabled;
}

float World::FixedTickDeltaSeconds() const
{
    return m_fixedTickDeltaSeconds;
}

float World::FixedTickInterpolationAlpha() const
{
    return m_fixedTickInterpolationAlpha;
}

void World::SetFixedTickFrameState(const bool Enabled, const float FixedDeltaSeconds, const float InterpolationAlpha)
{
    m_fixedTickEnabled = Enabled;
    m_fixedTickDeltaSeconds = Enabled ? std::max(0.0f, FixedDeltaSeconds) : 0.0f;
    m_fixedTickInterpolationAlpha = Enabled ? std::clamp(InterpolationAlpha, 0.0f, 1.0f) : 1.0f;
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

#if defined(SNAPI_GF_ENABLE_INPUT)
InputSystem& World::Input()
{
    
    return m_inputSystem;
}

const InputSystem& World::Input() const
{
    
    return m_inputSystem;
}
#endif

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

#if defined(SNAPI_GF_ENABLE_PHYSICS)
PhysicsSystem& World::Physics()
{
    
    return m_physicsSystem;
}

const PhysicsSystem& World::Physics() const
{
    
    return m_physicsSystem;
}
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
RendererSystem& World::Renderer()
{
    
    return m_rendererSystem;
}

const RendererSystem& World::Renderer() const
{
    
    return m_rendererSystem;
}
#endif

} // namespace SnAPI::GameFramework
