#include "EnvironmentCaptureComponent.h"


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

bool EnvironmentCaptureComponent::EnsureProbeRegistered()
{
    return false;
}

void EnvironmentCaptureComponent::UnregisterProbe()
{
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
} // namespace SnAPI::GameFramework
