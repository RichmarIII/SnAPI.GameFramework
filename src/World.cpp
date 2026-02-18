#include "World.h"
#include "Profiling.h"
#include <algorithm>
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <LinearAlgebra.hpp>
#include <ICamera.hpp>
#include <FontFace.hpp>
#endif
#if defined(SNAPI_GF_ENABLE_UI)
#include <UIPacketWriter.h>
#include <unordered_map>
#endif

namespace SnAPI::GameFramework
{

namespace
{
#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(SNAPI_GF_ENABLE_UI)
class UiFontMetricsAdapter final : public SnAPI::UI::IFontMetrics
{
public:
    void Bind(SnAPI::Graphics::FontFace* Face)
    {
        if (m_face == Face)
        {
            return;
        }
        m_face = Face;
        m_cachedGlyphs.clear();
    }

    const SnAPI::UI::GlyphMetrics* GetGlyph(uint32_t Codepoint) const override
    {
        if (!m_face || !m_face->Valid())
        {
            return nullptr;
        }

        if (const auto Cached = m_cachedGlyphs.find(Codepoint); Cached != m_cachedGlyphs.end())
        {
            return &Cached->second;
        }

        const auto Glyph = m_face->Glyph(Codepoint);
        const auto GlyphUv = m_face->GlyphUV(Codepoint);

        SnAPI::UI::GlyphMetrics Metrics{};
        Metrics.U0 = static_cast<float>(GlyphUv.Min.x());
        Metrics.V0 = static_cast<float>(GlyphUv.Min.y());
        Metrics.U1 = static_cast<float>(GlyphUv.Max.x());
        Metrics.V1 = static_cast<float>(GlyphUv.Max.y());
        Metrics.Width = static_cast<float>(Glyph.Width);
        Metrics.Height = static_cast<float>(Glyph.Height);
        Metrics.BearingX = static_cast<float>(Glyph.BitmapLeft);
        // UIPacketWriter expects stb-style y-offset from baseline (usually negative).
        // FreeType BitmapTop is upward-positive, so convert sign for consistent layout.
        Metrics.BearingY = -static_cast<float>(Glyph.BitmapTop);
        Metrics.Advance = static_cast<float>(Glyph.Advance.x());

        auto [It, Inserted] = m_cachedGlyphs.emplace(Codepoint, Metrics);
        (void)Inserted;
        return &It->second;
    }

    float GetLineHeight() const override
    {
        return (m_face && m_face->Valid()) ? m_face->Height() : 0.0f;
    }

    float GetAscent() const override
    {
        return (m_face && m_face->Valid()) ? m_face->Ascender() : 0.0f;
    }

private:
    SnAPI::Graphics::FontFace* m_face = nullptr;
    mutable std::unordered_map<uint32_t, SnAPI::UI::GlyphMetrics> m_cachedGlyphs{};
};
#endif
} // namespace

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
#if defined(SNAPI_GF_ENABLE_UI)
    if (m_uiSystem.IsInitialized())
    {
        m_uiSystem.Tick(DeltaSeconds);
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
#if defined(SNAPI_GF_ENABLE_UI)
    if (m_rendererSystem.IsInitialized() && m_uiSystem.IsInitialized())
    {
        if (auto* UiContext = m_uiSystem.Context())
        {
            if (auto* FontFace = m_rendererSystem.EnsureDefaultFontFace())
            {
                static UiFontMetricsAdapter FontMetricsAdapter{};
                FontMetricsAdapter.Bind(FontFace);
                UiContext->GetPacketWriter().SetFontMetrics(&FontMetricsAdapter);
            }
            else
            {
                UiContext->GetPacketWriter().SetFontMetrics(nullptr);
            }

            SnAPI::UI::RenderPacketList UiPackets{};
            if (auto BuildPacketsResult = m_uiSystem.BuildRenderPackets(UiPackets); BuildPacketsResult)
            {
                (void)m_rendererSystem.QueueUiRenderPackets(*UiContext, UiPackets);
            }
        }
    }
#endif
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

#if defined(SNAPI_GF_ENABLE_UI)
UISystem& World::UI()
{
    
    return m_uiSystem;
}

const UISystem& World::UI() const
{
    
    return m_uiSystem;
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
