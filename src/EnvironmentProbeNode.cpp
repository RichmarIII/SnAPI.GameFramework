#include "EnvironmentProbeNode.h"


#include "BaseNode.inl"
#include "EnvironmentCaptureComponent.h"
#include "TransformComponent.h"

namespace SnAPI::GameFramework
{
EnvironmentProbeNode::EnvironmentProbeNode()
{
    TypeKey(StaticTypeId<EnvironmentProbeNode>());
}

EnvironmentProbeNode::EnvironmentProbeNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<EnvironmentProbeNode>());
}

void EnvironmentProbeNode::OnCreate()
{
    EnsureDefaultComponents();
}

void EnvironmentProbeNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void EnvironmentProbeNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
}
#endif

void EnvironmentProbeNode::RefreshPreviewMeshState()
{
}

void EnvironmentProbeNode::EnsureDefaultComponents()
{
    if (!Has<TransformComponent>())
    {
        (void)Add<TransformComponent>();
    }

    if (!Has<EnvironmentCaptureComponent>())
    {
        (void)Add<EnvironmentCaptureComponent>();
    }
}
} // namespace SnAPI::GameFramework
