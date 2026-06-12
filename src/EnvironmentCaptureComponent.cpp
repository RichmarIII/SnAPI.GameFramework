#include "EnvironmentCaptureComponent.h"

#if defined(SNAPI_GF_ENABLE_LEGACY_RENDERER)

#include <algorithm>
#include <optional>

#include "BaseNode.h"
#include "IWorld.h"
#include "RendererSystem.h"
#include "TransformComponent.h"

#include <EnvironmentProbe.hpp>
#include <IGraphicsAPI.hpp>
#include <VulkanGraphicsAPI.hpp>

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] constexpr unsigned int ClampFaceSize(const unsigned int Value)
{
    return std::max(1u, Value);
}

[[nodiscard]] constexpr uint8_t ClampFacesPerFrame(const unsigned int Value)
{
    return static_cast<uint8_t>(std::clamp(Value, 1u, 6u));
}

[[nodiscard]] const char* CaptureStateLabel(const SnAPI::Graphics::EEnvironmentProbeCaptureState State)
{
    using SnAPI::Graphics::EEnvironmentProbeCaptureState;
    switch (State)
    {
    case EEnvironmentProbeCaptureState::Idle:
        return "Idle";
    case EEnvironmentProbeCaptureState::Queued:
        return "Queued";
    case EEnvironmentProbeCaptureState::Capturing:
        return "Capturing";
    case EEnvironmentProbeCaptureState::Ready:
        return "Ready";
    case EEnvironmentProbeCaptureState::Failed:
        return "Failed";
    case EEnvironmentProbeCaptureState::Canceled:
        return "Canceled";
    default:
        return "Unknown";
    }
}
} // namespace

EnvironmentCaptureComponent::Settings& EnvironmentCaptureComponent::EditSettings() { return m_settings; }
const EnvironmentCaptureComponent::Settings& EnvironmentCaptureComponent::GetSettings() const { return m_settings; }

std::string EnvironmentCaptureComponent::GetCaptureStateText() const
{
    auto* Renderer = ResolveRendererSystem();
    auto* Graphics = Renderer ? Renderer->Graphics() : nullptr;
    if (!Graphics || !m_probe || !ProbeRegisteredWithGraphics(*Graphics))
    {
        return "Unregistered";
    }

    return std::string(CaptureStateLabel(m_probe->CaptureState()));
}

unsigned int EnvironmentCaptureComponent::GetCapturedFaces() const
{
    auto* Renderer = ResolveRendererSystem();
    auto* Graphics = Renderer ? Renderer->Graphics() : nullptr;
    if (!Graphics || !m_probe || !ProbeRegisteredWithGraphics(*Graphics))
    {
        return 0u;
    }

    return static_cast<unsigned int>(m_probe->CapturedFaces());
}

void EnvironmentCaptureComponent::Bake()
{
    m_bBakeRequested = true;
    if (RequestCapture(true))
    {
        m_bBakeRequested = false;
    }
}

void EnvironmentCaptureComponent::CancelCapture()
{
    m_bBakeRequested = false;
    auto* Renderer = ResolveRendererSystem();
    auto* Graphics = Renderer ? Renderer->Graphics() : nullptr;
    if (!Graphics || !m_probe || !ProbeRegisteredWithGraphics(*Graphics))
    {
        return;
    }

    m_probe->CancelCapture();
}

void EnvironmentCaptureComponent::OnCreate()
{
    (void)EnsureProbeRegistered();
    SyncProbePosition();
    SyncProbeSettings();
}

void EnvironmentCaptureComponent::OnDestroy()
{
    UnregisterProbe();
}

void EnvironmentCaptureComponent::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;

    if (!EnsureProbeRegistered())
    {
        return;
    }

    SyncProbePosition();
    SyncProbeSettings();

    if (m_bBakeRequested)
    {
        if (RequestCapture(true))
        {
            m_bBakeRequested = false;
        }
        return;
    }

    if (!m_settings.Realtime || !m_probe)
    {
        return;
    }

    switch (m_probe->CaptureState())
    {
    case SnAPI::Graphics::EEnvironmentProbeCaptureState::Queued:
    case SnAPI::Graphics::EEnvironmentProbeCaptureState::Capturing:
        return;
    default:
        break;
    }

    (void)RequestCapture(false);
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void EnvironmentCaptureComponent::EditorTick(const float DeltaSeconds)
{
    Tick(DeltaSeconds);
}

void EnvironmentCaptureComponent::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    if (!EnsureProbeRegistered())
    {
        return;
    }

    SyncProbePosition();
    if (m_settings.Realtime)
    {
        (void)RequestCapture(true);
    }
}
#endif

RendererSystem* EnvironmentCaptureComponent::ResolveRendererSystem() const
{
    auto* Owner = OwnerNode();
    if (!Owner)
    {
        return nullptr;
    }

    auto* WorldPtr = Owner->World();
    if (!WorldPtr)
    {
        return nullptr;
    }

    return &WorldPtr->Renderer();
}

SnAPI::Graphics::VulkanGraphicsAPI* EnvironmentCaptureComponent::ResolveGraphics() const
{
    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return nullptr;
    }

    return Renderer->Graphics();
}

bool EnvironmentCaptureComponent::EnsureProbeRegistered()
{
    auto* Graphics = ResolveGraphics();
    if (!Graphics)
    {
        return false;
    }

    if (m_probe && ProbeRegisteredWithGraphics(*Graphics))
    {
        return true;
    }

    m_probe = nullptr;

    auto Probe = std::make_unique<SnAPI::Graphics::EnvironmentProbe>();
    if (!Probe)
    {
        return false;
    }

    m_probe = Probe.get();
    SyncProbePosition();
    SyncProbeSettings();
    Graphics->RegisterEnvironmentProbe(std::move(Probe));
    return ProbeRegisteredWithGraphics(*Graphics);
}

void EnvironmentCaptureComponent::UnregisterProbe()
{
    auto* Graphics = ResolveGraphics();
    if (Graphics && m_probe)
    {
        static_cast<void>(Graphics->UnregisterEnvironmentProbe(m_probe));
    }

    m_probe = nullptr;
    m_bBakeRequested = false;
}

void EnvironmentCaptureComponent::SyncProbePosition()
{
    auto* Owner = OwnerNode();
    if (!Owner || !m_probe)
    {
        return;
    }

    NodeTransform WorldTransform{};
    if (!TransformComponent::TryGetNodeWorldTransform(*Owner, WorldTransform))
    {
        return;
    }

    m_probe->Position(SnAPI::Vector3D{
        static_cast<SnAPI::Vector3D::Scalar>(WorldTransform.Position.x()),
        static_cast<SnAPI::Vector3D::Scalar>(WorldTransform.Position.y()),
        static_cast<SnAPI::Vector3D::Scalar>(WorldTransform.Position.z())});
}

void EnvironmentCaptureComponent::SyncProbeSettings()
{
    if (!m_probe)
    {
        return;
    }

    m_probe->ProjectionExtents(m_settings.ProjectionExtents);
    m_probe->InfluenceExtents(m_settings.InfluenceExtents);
    m_probe->Priority(m_settings.Priority);
}

bool EnvironmentCaptureComponent::RequestCapture(const bool Force)
{
    auto* Graphics = ResolveGraphics();
    if (!Graphics || !EnsureProbeRegistered() || !m_probe)
    {
        return false;
    }

    const auto SourceViewportID = ResolveSourceViewportID(*Graphics);
    if (!SourceViewportID.has_value())
    {
        return false;
    }

    SnAPI::Graphics::EnvironmentProbeCaptureRequest Request{};
    Request.SourceViewportID = *SourceViewportID;
    Request.FaceExtent = {ClampFaceSize(m_settings.FaceSize), ClampFaceSize(m_settings.FaceSize)};
    Request.FacesPerFrame = ClampFacesPerFrame(m_settings.FacesPerFrame);
    if (!m_settings.CaptureResourceNameOverride.empty())
    {
        Request.CaptureResourceNameOverride = m_settings.CaptureResourceNameOverride;
    }
    Request.Force = Force;
    m_probe->RequestCapture(Request);
    return true;
}

std::optional<std::uint64_t> EnvironmentCaptureComponent::ResolveSourceViewportID(
    const SnAPI::Graphics::VulkanGraphicsAPI& Graphics) const
{
    if (m_settings.ViewportID >= 0)
    {
        return static_cast<std::uint64_t>(m_settings.ViewportID);
    }

    if (Graphics.IsUsingDefaultViewport())
    {
        return Graphics.DefaultRenderViewportID();
    }

    const auto ViewportIDs = Graphics.RenderViewportIDs();
    if (!ViewportIDs.empty())
    {
        return ViewportIDs.front();
    }

    return std::nullopt;
}

bool EnvironmentCaptureComponent::ProbeRegisteredWithGraphics(const SnAPI::Graphics::VulkanGraphicsAPI& Graphics) const
{
    if (!m_probe)
    {
        return false;
    }

    return std::ranges::any_of(Graphics.EnvironmentProbes(), [this](const SnAPI::Graphics::EnvironmentProbePtr& Probe) {
        return Probe.get() == m_probe;
    });
}
} // namespace SnAPI::GameFramework

#elif defined(SNAPI_GF_ENABLE_RENDERER_NEW)

#include "BaseNode.h"
#include "IWorld.h"

namespace SnAPI::GameFramework
{

EnvironmentCaptureComponent::Settings& EnvironmentCaptureComponent::EditSettings() { return m_settings; }
const EnvironmentCaptureComponent::Settings& EnvironmentCaptureComponent::GetSettings() const { return m_settings; }

std::string EnvironmentCaptureComponent::GetCaptureStateText() const
{
    return "Disabled";
}

unsigned int EnvironmentCaptureComponent::GetCapturedFaces() const
{
    return 0u;
}

void EnvironmentCaptureComponent::Bake()
{
    m_bBakeRequested = false;
}

void EnvironmentCaptureComponent::CancelCapture()
{
    m_bBakeRequested = false;
}

void EnvironmentCaptureComponent::OnCreate()
{
}

void EnvironmentCaptureComponent::OnDestroy()
{
    UnregisterProbe();
}

void EnvironmentCaptureComponent::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void EnvironmentCaptureComponent::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
}

void EnvironmentCaptureComponent::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
}
#endif

RendererSystem* EnvironmentCaptureComponent::ResolveRendererSystem() const
{
    auto* Owner = OwnerNode();
    if (!Owner)
    {
        return nullptr;
    }

    auto* WorldPtr = Owner->World();
    if (!WorldPtr)
    {
        return nullptr;
    }

    return &WorldPtr->Renderer();
}

SnAPI::Graphics::VulkanGraphicsAPI* EnvironmentCaptureComponent::ResolveGraphics() const
{
    return nullptr;
}

bool EnvironmentCaptureComponent::EnsureProbeRegistered()
{
    return false;
}

void EnvironmentCaptureComponent::UnregisterProbe()
{
    m_probe = nullptr;
    m_bBakeRequested = false;
}

void EnvironmentCaptureComponent::SyncProbePosition()
{
}

void EnvironmentCaptureComponent::SyncProbeSettings()
{
}

bool EnvironmentCaptureComponent::RequestCapture(const bool Force)
{
    (void)Force;
    return false;
}

std::optional<std::uint64_t> EnvironmentCaptureComponent::ResolveSourceViewportID(
    const SnAPI::Graphics::VulkanGraphicsAPI& Graphics) const
{
    (void)Graphics;
    return std::nullopt;
}

bool EnvironmentCaptureComponent::ProbeRegisteredWithGraphics(const SnAPI::Graphics::VulkanGraphicsAPI& Graphics) const
{
    (void)Graphics;
    return false;
}
} // namespace SnAPI::GameFramework

#endif // renderer integration
