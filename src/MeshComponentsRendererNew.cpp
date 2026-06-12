#include "StaticMeshComponent.h"
#include "SkeletalMeshComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER_NEW)

#include "IWorld.h"
#include "RendererSystem.h"

#include <utility>

namespace SnAPI::GameFramework
{
bool StaticMeshComponent::ReloadMesh()
{
    m_lastFailedPathLoadKey.clear();
    ClearMesh();
    return EnsureMeshLoaded();
}

void StaticMeshComponent::SetSharedMaterialInstances(std::shared_ptr<SnAPI::Graphics::MaterialInstance> GBufferInstance,
                                                    std::shared_ptr<SnAPI::Graphics::MaterialInstance> ShadowInstance)
{
    m_sharedGBufferInstance = std::move(GBufferInstance);
    m_sharedShadowInstance = std::move(ShadowInstance);
}

void StaticMeshComponent::SetVertexStreamSource(std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> StreamSource)
{
    if (m_streamSource == StreamSource)
    {
        return;
    }
    m_streamSource = std::move(StreamSource);
    ClearMesh();
}

void StaticMeshComponent::ClearMesh()
{
    if (m_renderObject)
    {
        if (auto* Renderer = ResolveRendererSystem(); Renderer)
        {
            Renderer->RemoveRenderObject(m_renderObject);
        }
    }

    m_renderObject.reset();
    m_loadedPath.clear();
    m_loadedFromAsset = false;
    m_loadedMeshMaterialInstances.clear();
    m_registered = false;
    m_passStateInitialized = false;
    m_lastPassGraphRevision = 0;
    m_loadedStreamSource.reset();
}

void StaticMeshComponent::OnCreate()
{
    (void)EnsureMeshLoaded();
}

void StaticMeshComponent::OnDestroy()
{
    ClearMesh();
}

void StaticMeshComponent::Tick(float DeltaSeconds)
{
    (void)DeltaSeconds;
    (void)EnsureMeshLoaded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void StaticMeshComponent::EditorTick(float DeltaSeconds)
{
    Tick(DeltaSeconds);
}

void StaticMeshComponent::EditorOnPropertyChanged(std::string_view Name)
{
    (void)Name;
    m_lastFailedPathLoadKey.clear();
    ClearMesh();
}
#endif

RendererSystem* StaticMeshComponent::ResolveRendererSystem() const
{
    auto* Owner = OwnerNode();
    auto* OwnerWorld = Owner ? Owner->World() : nullptr;
    return OwnerWorld ? &OwnerWorld->Renderer() : nullptr;
}

bool StaticMeshComponent::EnsureMeshLoaded()
{
    if (m_renderObject)
    {
        return true;
    }

    const bool HasMeshSource = m_streamSource != nullptr || !m_settings.MeshAsset.IsNull() || !m_settings.MeshPath.empty();
    if (!HasMeshSource)
    {
        return false;
    }

    m_loadedFromAsset = !m_settings.MeshAsset.IsNull();
    m_loadedPath = m_loadedFromAsset ? m_settings.MeshAsset.GetAssetId() : m_settings.MeshPath;
    m_loadedMeshMaterialInstances = m_settings.MaterialInstanceOverrides;
    m_registered = false;
    m_passStateInitialized = false;
    return true;
}

void StaticMeshComponent::SyncRenderObjectTransform(SnAPI::Graphics::IRenderObject& RenderObject) const
{
    (void)RenderObject;
}

void StaticMeshComponent::ApplyConfiguredMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject)
{
    (void)RenderObject;
}

void StaticMeshComponent::ApplySharedMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject) const
{
    (void)RenderObject;
}

void StaticMeshComponent::ApplyRenderObjectState(SnAPI::Graphics::IRenderObject& RenderObject)
{
    (void)RenderObject;
}

bool SkeletalMeshComponent::ReloadMesh()
{
    m_lastFailedPathLoadKey.clear();
    ClearMesh();
    return EnsureMeshLoaded();
}

void SkeletalMeshComponent::ClearMesh()
{
    m_renderObject.reset();
    m_loadedPath.clear();
    m_loadedFromAsset = false;
    m_loadedMeshMaterialInstances.clear();
    m_lastAutoPlayAnimation.clear();
    m_lastAutoPlayLoop = true;
    m_autoPlayApplied = false;
    m_registered = false;
    m_passStateInitialized = false;
    m_lastPassGraphRevision = 0;
}

bool SkeletalMeshComponent::PlayAnimation(const std::string& Name, bool Loop, float StartTime)
{
    (void)Name;
    (void)StartTime;
    if (!EnsureMeshLoaded())
    {
        return false;
    }
    m_lastAutoPlayLoop = Loop;
    m_autoPlayApplied = true;
    return true;
}

bool SkeletalMeshComponent::PlayAllAnimations(bool Loop, float StartTime)
{
    (void)StartTime;
    if (!EnsureMeshLoaded())
    {
        return false;
    }
    m_lastAutoPlayAnimation.clear();
    m_lastAutoPlayLoop = Loop;
    m_autoPlayApplied = true;
    return true;
}

void SkeletalMeshComponent::StopAnimations()
{
    m_autoPlayApplied = false;
}

void SkeletalMeshComponent::OnCreate()
{
    (void)EnsureMeshLoaded();
}

void SkeletalMeshComponent::OnDestroy()
{
    ClearMesh();
}

void SkeletalMeshComponent::Tick(float DeltaSeconds)
{
    (void)DeltaSeconds;
    (void)EnsureMeshLoaded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void SkeletalMeshComponent::EditorTick(float DeltaSeconds)
{
    Tick(DeltaSeconds);
}

void SkeletalMeshComponent::EditorOnPropertyChanged(std::string_view Name)
{
    (void)Name;
    m_lastFailedPathLoadKey.clear();
    ClearMesh();
}
#endif

RendererSystem* SkeletalMeshComponent::ResolveRendererSystem() const
{
    auto* Owner = OwnerNode();
    auto* OwnerWorld = Owner ? Owner->World() : nullptr;
    return OwnerWorld ? &OwnerWorld->Renderer() : nullptr;
}

bool SkeletalMeshComponent::EnsureMeshLoaded()
{
    if (m_renderObject)
    {
        return true;
    }

    const bool HasMeshSource = !m_settings.MeshAsset.IsNull() || !m_settings.MeshPath.empty();
    if (!HasMeshSource)
    {
        return false;
    }

    m_loadedFromAsset = !m_settings.MeshAsset.IsNull();
    m_loadedPath = m_loadedFromAsset ? m_settings.MeshAsset.GetAssetId() : m_settings.MeshPath;
    m_loadedMeshMaterialInstances = m_settings.MaterialInstanceOverrides;
    m_registered = false;
    m_passStateInitialized = false;
    return true;
}

void SkeletalMeshComponent::SyncRenderObjectTransform(SnAPI::Graphics::MeshRenderObject& RenderObject) const
{
    (void)RenderObject;
}

void SkeletalMeshComponent::ApplyConfiguredMaterialInstances(SnAPI::Graphics::MeshRenderObject& RenderObject)
{
    (void)RenderObject;
}

void SkeletalMeshComponent::ApplyRenderObjectState(SnAPI::Graphics::MeshRenderObject& RenderObject)
{
    (void)RenderObject;
}

void SkeletalMeshComponent::ApplyAutoPlay(SnAPI::Graphics::MeshRenderObject& RenderObject)
{
    (void)RenderObject;
}
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER_NEW
