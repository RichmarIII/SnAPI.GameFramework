#include "EnvironmentProbeNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstring>
#include <memory>

#include "BaseNode.inl"
#include "EnvironmentCaptureComponent.h"
#include "IWorld.h"
#include "RendererSystem.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"

#include <MaterialContracts.hpp>
#include <MaterialInstance.hpp>
#include <SphereStreamSource.hpp>
#include <VulkanGraphicsAPI.hpp>
#include <VulkanPipeline.hpp>

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] std::shared_ptr<SnAPI::Graphics::MaterialInstance> BuildPreviewMaterialInstance(RendererSystem& Renderer)
{
    auto* Graphics = Renderer.Graphics();
    const auto BaseMaterial = Renderer.DefaultGBufferMaterial();
    if (!Graphics || !BaseMaterial)
    {
        return {};
    }

    auto Instance = BaseMaterial->CreateMaterialInstance();
    if (!Instance)
    {
        return {};
    }

    SnAPI::Graphics::GBufferContract::ParamBlock MaterialData{};
    MaterialData.Color[0] = 1.0f;
    MaterialData.Color[1] = 1.0f;
    MaterialData.Color[2] = 1.0f;
    MaterialData.Color[3] = 1.0f;
    MaterialData.Roughness = 0.02f;
    MaterialData.Metallic = 1.0f;
    MaterialData.Occlusion = 1.0f;

    SnAPI::Graphics::BufferCreateInfo BufferCI{};
    BufferCI.Size = sizeof(SnAPI::Graphics::GBufferContract::ParamBlock);
    BufferCI.Usage = vk::BufferUsageFlagBits::eUniformBuffer;
    BufferCI.MemoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

    auto MaterialBuffer = Graphics->CreateBuffer(BufferCI);
    if (!MaterialBuffer)
    {
        return {};
    }

    void* pMapped = MaterialBuffer->Map(0, sizeof(SnAPI::Graphics::GBufferContract::ParamBlock));
    if (!pMapped)
    {
        return {};
    }

    std::memcpy(pMapped, &MaterialData, sizeof(SnAPI::Graphics::GBufferContract::ParamBlock));
    MaterialBuffer->UnMap();
    Instance->Buffer("Material_MaterialData", std::move(MaterialBuffer));
    return Instance;
}
} // namespace

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
    RefreshPreviewMeshState();
}

void EnvironmentProbeNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    RefreshPreviewMeshState();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void EnvironmentProbeNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    RefreshPreviewMeshState();
}
#endif

void EnvironmentProbeNode::RefreshPreviewMeshState()
{
    auto MeshResult = Component<StaticMeshComponent>();
    if (!MeshResult)
    {
        return;
    }

    if (const auto& RenderObject = MeshResult->RenderObject(); RenderObject)
    {
        RenderObject->SetExcludeFromEnvironmentCapture(true);
    }
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

    bool PreviewMeshNeedsDefaults = false;
    auto MeshResult = Component<StaticMeshComponent>();
    if (!MeshResult)
    {
        MeshResult = Add<StaticMeshComponent>();
        PreviewMeshNeedsDefaults = static_cast<bool>(MeshResult);
    }

    if (!MeshResult)
    {
        return;
    }

    auto& Mesh = *MeshResult;
    const auto& ExistingSettings = Mesh.GetSettings();
    PreviewMeshNeedsDefaults = PreviewMeshNeedsDefaults
        || (!Mesh.GetVertexStreamSource()
            && ExistingSettings.MeshPath.empty()
            && ExistingSettings.MeshAsset.IsNull());

    if (!PreviewMeshNeedsDefaults)
    {
        return;
    }

    auto SphereSource = std::make_shared<SnAPI::Graphics::SphereStreamSource>();
    SphereSource->SetRadius(0.5f);
    SphereSource->SetSegments(32u, 16u);
    Mesh.SetVertexStreamSource(std::move(SphereSource));

    auto& Settings = Mesh.EditSettings();
    Settings.Visible = true;
    Settings.CastShadows = false;
    Settings.SyncFromTransform = true;
    Settings.RegisterWithRenderer = true;
    Settings.MeshPath.clear();
    Settings.MeshAsset.Clear();

    if (auto* WorldPtr = World())
    {
    if (auto PreviewInstance = BuildPreviewMaterialInstance(WorldPtr->Renderer()))
    {
        Mesh.SetSharedMaterialInstances(std::move(PreviewInstance));
    }
    }

    (void)Mesh.ReloadMesh();
    RefreshPreviewMeshState();
}
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
