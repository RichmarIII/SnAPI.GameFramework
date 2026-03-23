#include "SkeletalMeshComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "Profiling.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <string_view>

#include <LinearAlgebra.hpp>
#include <MaterialInstance.hpp>
#include <MeshRenderObject.hpp>

#include "BaseNode.h"
#include "IWorld.h"
#include "PathResolver.h"
#include "RenderAssets/MeshRuntimeAssets.h"
#include "RendererSystem.h"
#include "TransformComponent.h"

namespace SnAPI::GameFramework
{
namespace
{
bool IsFiniteVec3(const Vec3& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z());
}

bool IsFiniteQuat(const Quat& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z()) && std::isfinite(Value.w());
}

SnAPI::Matrix4 ComposeRendererWorldTransform(const NodeTransform& Transform)
{
    const SnAPI::Vector3D Position{
        static_cast<SnAPI::Vector3D::Scalar>(Transform.Position.x()),
        static_cast<SnAPI::Vector3D::Scalar>(Transform.Position.y()),
        static_cast<SnAPI::Vector3D::Scalar>(Transform.Position.z())};
    const SnAPI::Vector3D Scale{
        static_cast<SnAPI::Vector3D::Scalar>(Transform.Scale.x()),
        static_cast<SnAPI::Vector3D::Scalar>(Transform.Scale.y()),
        static_cast<SnAPI::Vector3D::Scalar>(Transform.Scale.z())};

    SnAPI::Quaternion Rotation = SnAPI::Quaternion::Identity();
    Rotation.x() = static_cast<SnAPI::Quaternion::Scalar>(Transform.Rotation.x());
    Rotation.y() = static_cast<SnAPI::Quaternion::Scalar>(Transform.Rotation.y());
    Rotation.z() = static_cast<SnAPI::Quaternion::Scalar>(Transform.Rotation.z());
    Rotation.w() = static_cast<SnAPI::Quaternion::Scalar>(Transform.Rotation.w());
    if (Rotation.squaredNorm() > 0.0)
    {
        Rotation.normalize();
    }
    else
    {
        Rotation = SnAPI::Quaternion::Identity();
    }

    auto WorldTransform = SnAPI::Transform3D::Identity();
    WorldTransform.translate(Position);
    WorldTransform.rotate(Rotation);
    WorldTransform.scale(Scale);
    return WorldTransform.matrix();
}

[[nodiscard]] bool ResolveFilesystemMeshPath(const std::string_view MeshPath, std::string& OutResolvedPath)
{
    if (MeshPath.empty())
    {
        OutResolvedPath.clear();
        return false;
    }

    auto ResolvedPath = SPathResolver::Instance().ResolveToString(MeshPath);
    if (!ResolvedPath || ResolvedPath->empty())
    {
        // Compatibility: allow bare relative content paths by treating them as asset:// URIs.
        if (MeshPath.find("://") == std::string_view::npos)
        {
            std::string AssetUri = "asset://";
            AssetUri.append(MeshPath.begin(), MeshPath.end());
            ResolvedPath = SPathResolver::Instance().ResolveToString(AssetUri);
        }
        if (!ResolvedPath || ResolvedPath->empty())
        {
            OutResolvedPath.clear();
            return false;
        }
    }

    OutResolvedPath = *ResolvedPath;
    return true;
}

[[nodiscard]] std::string BuildMeshAssetToken(const SkeletalMeshAssetRef& MeshAssetRef)
{
    const std::string AssetId = MeshAssetRef.GetAssetId();
    if (!AssetId.empty())
    {
        return "asset-id://" + AssetId;
    }

    const std::string AssetName = MeshAssetRef.ResolvedAssetName();
    if (!AssetName.empty())
    {
        return "asset://" + AssetName;
    }

    return {};
}

[[nodiscard]] std::vector<TAssetRef<MaterialInstanceAsset>> BuildEffectiveMaterialRefs(
    const std::vector<TAssetRef<MaterialInstanceAsset>>& BaseRefs,
    const std::vector<TAssetRef<MaterialInstanceAsset>>& OverrideRefs)
{
    if (OverrideRefs.empty())
    {
        return BaseRefs;
    }

    std::vector<TAssetRef<MaterialInstanceAsset>> EffectiveRefs = BaseRefs;
    if (EffectiveRefs.size() < OverrideRefs.size())
    {
        EffectiveRefs.resize(OverrideRefs.size());
    }

    for (std::size_t Index = 0; Index < OverrideRefs.size(); ++Index)
    {
        if (!OverrideRefs[Index].IsNull())
        {
            EffectiveRefs[Index] = OverrideRefs[Index];
        }
    }

    return EffectiveRefs;
}

void ApplyRuntimeOrDefaultMaterialInstances(
    SnAPI::Graphics::IRenderObject& RenderObject,
    RendererSystem& Renderer,
    const std::vector<TAssetRef<MaterialInstanceAsset>>& MaterialRefs,
    ::SnAPI::AssetPipeline::AssetManager* AssetManager)
{
    Renderer.ApplyDefaultMaterials(RenderObject);

    const auto& Source = RenderObject.VertexStreamSource();
    if (!Source || !AssetManager)
    {
        return;
    }

    for (uint32_t SubMeshIndex = 0; SubMeshIndex < Source->SubMeshCount(); ++SubMeshIndex)
    {
        SnAPI::Graphics::VertexSourceSubMesh SubMesh{};
        const bool HasSubMesh = Source->SubMesh(SubMeshIndex, SubMesh);
        const uint32_t MaterialSlot = HasSubMesh ? SubMesh.MaterialSlot : SubMeshIndex;
        if (MaterialSlot >= MaterialRefs.size())
        {
            continue;
        }

        auto RuntimeInstance = MaterialRefs[MaterialSlot].GetRuntimeShared<SnAPI::Graphics::MaterialInstance>(*AssetManager);
        if (RuntimeInstance && *RuntimeInstance && (*RuntimeInstance)->Material())
        {
            RenderObject.SetMaterialInstance(SubMeshIndex, *RuntimeInstance);
        }
    }
}

#if defined(WITH_EDITOR) && WITH_EDITOR
bool IsSkeletalMeshSettingsField(const std::string_view Name)
{
    return Name == "Settings"
        || Name == "MeshAsset"
        || Name == "MeshPath"
        || Name == "AssetName"
        || Name == "AssetId"
        || Name == "Visible"
        || Name == "CastShadows"
        || Name == "SyncFromTransform"
        || Name == "RegisterWithRenderer"
        || Name == "AutoPlayAnimations"
        || Name == "LoopAnimations"
        || Name == "AnimationName"
        || Name == "MaterialInstanceOverrides";
}
#endif
} // namespace

bool SkeletalMeshComponent::ReloadMesh()
{
    
    m_lastFailedPathLoadKey.clear();
    ClearMesh();
    return EnsureMeshLoaded();
}

void SkeletalMeshComponent::ClearMesh()
{
    if (m_renderObject)
    {
        if (auto* Renderer = ResolveRendererSystem(); Renderer && Renderer->IsInitialized())
        {
            Renderer->RemoveRenderObject(m_renderObject);
        }
    }

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

bool SkeletalMeshComponent::PlayAnimation(const std::string& Name, const bool Loop, const float StartTime)
{
    
    if (!m_renderObject)
    {
        if (!EnsureMeshLoaded())
        {
            return false;
        }
        if (!m_renderObject)
        {
            return false;
        }
    }

    m_renderObject->PlayRigidAnimation(Name, StartTime, Loop);
    return true;
}

bool SkeletalMeshComponent::PlayAllAnimations(const bool Loop, const float StartTime)
{
    
    if (!m_renderObject)
    {
        if (!EnsureMeshLoaded())
        {
            return false;
        }
        if (!m_renderObject)
        {
            return false;
        }
    }

    m_renderObject->PlayRigidAnimations(StartTime, Loop);
    return true;
}

void SkeletalMeshComponent::StopAnimations()
{
    
    if (m_renderObject)
    {
        m_renderObject->StopRigidAnimations();
    }
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

void SkeletalMeshComponent::Tick(const float DeltaSeconds)
{
    
    if (m_settings.MeshPath.empty() && m_settings.MeshAsset.IsNull())
    {
        ClearMesh();
        return;
    }

    if (m_loadedFromAsset)
    {
        const std::string AssetToken = BuildMeshAssetToken(m_settings.MeshAsset);
        if (!AssetToken.empty() && m_loadedPath != AssetToken)
        {
            ClearMesh();
        }
    }
    else
    {
        std::string ResolvedMeshPath{};
        const bool HasResolvedMeshPath = ResolveFilesystemMeshPath(m_settings.MeshPath, ResolvedMeshPath);
        if (!HasResolvedMeshPath || m_loadedPath != ResolvedMeshPath)
        {
            ClearMesh();
        }
    }

    if (!EnsureMeshLoaded())
    {
        return;
    }

    if (!m_renderObject)
    {
        ClearMesh();
        return;
    }

    ApplyConfiguredMaterialInstances(*m_renderObject);
    if (m_settings.SyncFromTransform)
    {
        SyncRenderObjectTransform(*m_renderObject);
    }
    ApplyRenderObjectState(*m_renderObject);
    ApplyAutoPlay(*m_renderObject);
    m_renderObject->Update(DeltaSeconds);
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void SkeletalMeshComponent::EditorTick(float DeltaSeconds)
{
    Tick(DeltaSeconds);
}

void SkeletalMeshComponent::EditorOnPropertyChanged(const std::string_view Name)
{
    if (!IsSkeletalMeshSettingsField(Name))
    {
        return;
    }

    if (Name == "MeshPath" || Name == "MeshAsset" || Name == "AssetName" || Name == "AssetId")
    {
        m_lastFailedPathLoadKey.clear();
    }

    if (m_settings.MeshPath.empty() && m_settings.MeshAsset.IsNull())
    {
        ClearMesh();
        return;
    }

    if (m_loadedFromAsset)
    {
        const std::string AssetToken = BuildMeshAssetToken(m_settings.MeshAsset);
        if (AssetToken.empty() || m_loadedPath != AssetToken)
        {
            ClearMesh();
        }
    }
    else
    {
        std::string ResolvedMeshPath{};
        const bool HasResolvedMeshPath = ResolveFilesystemMeshPath(m_settings.MeshPath, ResolvedMeshPath);
        if (!HasResolvedMeshPath || m_loadedPath != ResolvedMeshPath)
        {
            ClearMesh();
        }
    }

    if (!EnsureMeshLoaded() || !m_renderObject)
    {
        return;
    }

    ApplyConfiguredMaterialInstances(*m_renderObject);
    if (m_settings.SyncFromTransform)
    {
        SyncRenderObjectTransform(*m_renderObject);
    }
    ApplyRenderObjectState(*m_renderObject);
    ApplyAutoPlay(*m_renderObject);
}
#endif

RendererSystem* SkeletalMeshComponent::ResolveRendererSystem() const
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

bool SkeletalMeshComponent::EnsureMeshLoaded()
{
    
    if (m_settings.MeshPath.empty() && m_settings.MeshAsset.IsNull())
    {
        return false;
    }

    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return false;
    }

    if (m_renderObject)
    {
        return true;
    }

    if (!m_settings.MeshAsset.IsNull())
    {
        if (auto* AssetManager = ResolveDefaultAssetManager())
        {
            auto RuntimeMesh = m_settings.MeshAsset.GetRuntimeShared<SkeletalMeshRuntime>(*AssetManager);
            if (RuntimeMesh && *RuntimeMesh && (*RuntimeMesh)->StreamSource)
            {
                std::string AssetToken = BuildMeshAssetToken(m_settings.MeshAsset);
                if (AssetToken.empty())
                {
                    AssetToken = "asset-ref://" + m_settings.MeshAsset.DisplayLabel();
                }

                if (auto RenderObject = std::make_shared<SnAPI::Graphics::MeshRenderObject>())
                {
                    RenderObject->SetVertexStreamSource((*RuntimeMesh)->StreamSource);
                    m_renderObject = std::move(RenderObject);
                    m_loadedPath = AssetToken;
                    m_loadedFromAsset = true;
                    m_loadedMeshMaterialInstances = (*RuntimeMesh)->MaterialRefs;
                    m_lastAutoPlayAnimation.clear();
                    m_lastAutoPlayLoop = m_settings.LoopAnimations;
                    m_autoPlayApplied = false;
                    m_registered = false;

                    ApplyConfiguredMaterialInstances(*m_renderObject);
                    ApplyRenderObjectState(*m_renderObject);

                    return true;
                }
            }
        }
    }

    return false;
}

void SkeletalMeshComponent::ApplyConfiguredMaterialInstances(SnAPI::Graphics::MeshRenderObject& RenderObject)
{
    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return;
    }

    const std::vector<TAssetRef<MaterialInstanceAsset>> EffectiveRefs =
        BuildEffectiveMaterialRefs(m_loadedMeshMaterialInstances, m_settings.MaterialInstanceOverrides);
    ApplyRuntimeOrDefaultMaterialInstances(RenderObject, *Renderer, EffectiveRefs, ResolveDefaultAssetManager());
}

void SkeletalMeshComponent::SyncRenderObjectTransform(SnAPI::Graphics::MeshRenderObject& RenderObject) const
{
    
    auto* Owner = OwnerNode();
    if (!Owner)
    {
        return;
    }

    NodeTransform WorldTransform{};
    if (!TransformComponent::TryGetNodeWorldTransform(*Owner, WorldTransform))
    {
        return;
    }

    if (!IsFiniteVec3(WorldTransform.Position)
        || !IsFiniteQuat(WorldTransform.Rotation)
        || !IsFiniteVec3(WorldTransform.Scale))
    {
        return;
    }

    RenderObject.SetWorldTransform(ComposeRendererWorldTransform(WorldTransform));
}

void SkeletalMeshComponent::ApplyRenderObjectState(SnAPI::Graphics::MeshRenderObject& RenderObject)
{
    static_cast<void>(RenderObject);

    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return;
    }

    const bool EffectiveVisible = m_settings.RegisterWithRenderer && m_settings.Visible;
    const std::uint64_t PassGraphRevision = Renderer->RenderViewportPassGraphRevision();
    const bool PassStateChanged = !m_passStateInitialized
                               || m_lastVisible != EffectiveVisible
                               || m_lastCastShadows != m_settings.CastShadows
                               || m_lastPassGraphRevision != PassGraphRevision
                               || m_registered != m_settings.RegisterWithRenderer;
    if (PassStateChanged)
    {
        if (Renderer->ConfigureRenderObjectPasses(m_renderObject, EffectiveVisible, m_settings.CastShadows))
        {
            m_passStateInitialized = true;
            m_lastVisible = EffectiveVisible;
            m_lastCastShadows = m_settings.CastShadows;
            m_lastPassGraphRevision = PassGraphRevision;
            m_registered = m_settings.RegisterWithRenderer;
        }
    }
}

void SkeletalMeshComponent::ApplyAutoPlay(SnAPI::Graphics::MeshRenderObject& RenderObject)
{
    
    if (!m_settings.AutoPlayAnimations)
    {
        m_autoPlayApplied = false;
        m_lastAutoPlayAnimation = m_settings.AnimationName;
        m_lastAutoPlayLoop = m_settings.LoopAnimations;
        return;
    }

    if (m_lastAutoPlayAnimation != m_settings.AnimationName
        || m_lastAutoPlayLoop != m_settings.LoopAnimations)
    {
        m_autoPlayApplied = false;
    }

    m_lastAutoPlayAnimation = m_settings.AnimationName;
    m_lastAutoPlayLoop = m_settings.LoopAnimations;

    if (m_autoPlayApplied)
    {
        return;
    }

    if (m_settings.AnimationName.empty())
    {
        RenderObject.PlayRigidAnimations(0.0f, m_settings.LoopAnimations);
    }
    else
    {
        RenderObject.PlayRigidAnimation(m_settings.AnimationName, 0.0f, m_settings.LoopAnimations);
    }
    m_autoPlayApplied = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
