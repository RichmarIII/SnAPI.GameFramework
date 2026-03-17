#include "StaticMeshComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "Profiling.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <string>
#include <string_view>

#include <CapsuleStreamSource.hpp>
#include <IVertexStreamSource.hpp>
#include <LinearAlgebra.hpp>
#include <MaterialInstance.hpp>
#include <MeshRenderObject.hpp>

#include "BaseNode.h"
#include "IWorld.h"
#include "PathResolver.h"
#include "RendererSystem.h"
#include "TransformComponent.h"

namespace SnAPI::GameFramework
{
namespace
{
std::string ToLowerASCII(const std::string_view Value)
{
    std::string Out(Value);
    std::ranges::transform(Out, Out.begin(), [](const unsigned char Ch) {
        return static_cast<char>(std::tolower(Ch));
    });
    return Out;
}

[[nodiscard]] bool IsPrimitiveMeshToken(const std::string_view MeshPath)
{
    const std::string Token = ToLowerASCII(MeshPath);
    return Token == "primitive://box"
        || Token == "__primitive_box__"
        || Token == "primitive://sphere"
        || Token == "__primitive_sphere__"
        || Token == "primitive://capsule"
        || Token == "__primitive_capsule__"
        || Token == "primitive://cone"
        || Token == "__primitive_cone__"
        || Token == "primitive://pyramid"
        || Token == "__primitive_pyramid__";
}

[[nodiscard]] bool ResolveFilesystemMeshPath(const std::string_view MeshPath, std::string& OutResolvedPath)
{
    if (MeshPath.empty() || IsPrimitiveMeshToken(MeshPath))
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

SnAPI::Vector3DF ToVector3DF(const Vec3& Value)
{
    return SnAPI::Vector3DF{
        static_cast<float>(Value.x()),
        static_cast<float>(Value.y()),
        static_cast<float>(Value.z())};
}

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

SnAPI::Graphics::SharedVertexStreamSourcePtr BuildPrimitiveSourceFromMeshPath(const std::string& MeshPath)
{
    const std::string Token = ToLowerASCII(MeshPath);

    if (Token == "primitive://box" || Token == "__primitive_box__")
    {
        auto Source = std::make_shared<SnAPI::Graphics::BoxStreamSource>();
        Source->SetSize(ToVector3DF(Vec3(1.0f, 1.0f, 1.0f)));
        return Source;
    }

    if (Token == "primitive://sphere" || Token == "__primitive_sphere__")
    {
        auto Source = std::make_shared<SnAPI::Graphics::SphereStreamSource>();
        Source->SetRadius(0.5f);
        Source->SetSegments(32u, 16u);
        return Source;
    }

    if (Token == "primitive://capsule" || Token == "__primitive_capsule__")
    {
        auto Source = std::make_shared<SnAPI::Graphics::CapsuleStreamSource>();
        Source->SetRadius(0.35f);
        Source->SetHalfHeight(0.6f);
        Source->SetSegments(24u, 8u);
        return Source;
    }

    if (Token == "primitive://cone" || Token == "__primitive_cone__")
    {
        auto Source = std::make_shared<SnAPI::Graphics::ConeStreamSource>();
        Source->SetRadius(0.5f);
        Source->SetHeight(1.0f);
        Source->SetRadialSegments(24u);
        return Source;
    }

    if (Token == "primitive://pyramid" || Token == "__primitive_pyramid__")
    {
        auto Source = std::make_shared<SnAPI::Graphics::PyramidStreamSource>();
        Source->SetSize(ToVector3DF(Vec3(1.0f, 1.0f, 1.0f)));
        return Source;
    }

    return {};
}

[[nodiscard]] std::string BuildMeshAssetToken(const StaticMeshAssetRef& MeshAssetRef)
{
    const std::string& AssetId = MeshAssetRef.GetAssetId();
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

[[nodiscard]] std::vector<TAssetRef<MaterialInstanceAsset>> BuildMaterialRefsFromPayloads(
    const std::vector<AssetRefPayload>& Payloads)
{
    std::vector<TAssetRef<MaterialInstanceAsset>> Refs{};
    Refs.reserve(Payloads.size());
    for (const AssetRefPayload& Payload : Payloads)
    {
        Refs.emplace_back(Payload.AssetName, Payload.AssetId);
    }
    return Refs;
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
bool IsStaticMeshSettingsField(const std::string_view Name)
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
        || Name == "MaterialInstanceOverrides";
}
#endif
} // namespace

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

    if (m_renderObject)
    {
        ApplySharedMaterialInstances(*m_renderObject);
    }
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
        if (auto* Renderer = ResolveRendererSystem(); Renderer && Renderer->IsInitialized())
        {
            Renderer->RemoveRenderObject(m_renderObject);
        }
    }

    m_renderObject.reset();
    m_loadedPath.clear();
    m_loadedFromAsset = false;
    m_loadedMeshMaterialInstances.clear();
    m_loadedStreamSource.reset();
    m_registered = false;
    m_passStateInitialized = false;
    m_lastPassGraphRevision = 0;
}

void StaticMeshComponent::OnCreate()
{
    (void)EnsureMeshLoaded();
}

void StaticMeshComponent::OnDestroy()
{
    
    ClearMesh();
}

void StaticMeshComponent::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    if (!m_renderObject)
    {
        return;
    }

    if (m_settings.SyncFromTransform)
    {
        SyncRenderObjectTransform(*m_renderObject);
    }
    ApplyRenderObjectState(*m_renderObject);
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void StaticMeshComponent::EditorTick(float DeltaSeconds)
{
    Tick(DeltaSeconds);
}

void StaticMeshComponent::EditorOnPropertyChanged(const std::string_view Name)
{
    if (!IsStaticMeshSettingsField(Name))
    {
        return;
    }

    if (Name == "MeshPath" || Name == "MeshAsset" || Name == "AssetName" || Name == "AssetId")
    {
        m_lastFailedPathLoadKey.clear();
    }

    if (m_settings.MeshPath.empty() && m_settings.MeshAsset.IsNull() && !m_streamSource)
    {
        ClearMesh();
        return;
    }

    if (!m_streamSource && m_loadedFromAsset)
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
        const bool IsPrimitiveMesh = IsPrimitiveMeshToken(m_settings.MeshPath);
        if (!m_streamSource
            && ((HasResolvedMeshPath && m_loadedPath != ResolvedMeshPath)
                || (IsPrimitiveMesh && m_loadedPath != ToLowerASCII(m_settings.MeshPath))
                || (!HasResolvedMeshPath && !IsPrimitiveMesh)))
        {
            ClearMesh();
        }
    }
    if (m_streamSource && m_loadedStreamSource.lock() != m_streamSource)
    {
        ClearMesh();
    }

    if (!EnsureMeshLoaded() || !m_renderObject)
    {
        return;
    }

    // Always refresh material bindings for settings edits in editor mode.
    // Some property panels emit coarse field names (for example "Settings")
    // instead of leaf names, so only checking "MaterialInstanceOverrides"
    // can miss override updates.
    ApplyConfiguredMaterialInstances(*m_renderObject);
    ApplySharedMaterialInstances(*m_renderObject);

    if (m_settings.SyncFromTransform)
    {
        SyncRenderObjectTransform(*m_renderObject);
    }
    ApplyRenderObjectState(*m_renderObject);
}
#endif

RendererSystem* StaticMeshComponent::ResolveRendererSystem() const
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

bool StaticMeshComponent::EnsureMeshLoaded()
{
    
    if (m_settings.MeshPath.empty() && m_settings.MeshAsset.IsNull() && !m_streamSource)
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

    // This means a stream source was manually set on this component
    if (m_streamSource)
    {
        auto RenderObject = std::make_shared<SnAPI::Graphics::MeshRenderObject>();
        if (!RenderObject)
        {
            return false;
        }

        RenderObject->SetVertexStreamSource(m_streamSource);
        m_renderObject = std::move(RenderObject);
        m_loadedPath.clear();
        m_loadedFromAsset = false;
        m_loadedMeshMaterialInstances.clear();
        m_loadedStreamSource = m_streamSource;
        m_registered = false;

        ApplyConfiguredMaterialInstances(*m_renderObject);
        ApplySharedMaterialInstances(*m_renderObject);
        ApplyRenderObjectState(*m_renderObject);

        return true;
    }

    //Mesh asset was specified
    if (!m_settings.MeshAsset.IsNull())
    {
        if (auto* AssetManager = ResolveDefaultAssetManager())
        {
            auto RuntimeSource = m_settings.MeshAsset.GetRuntimeShared<SnAPI::Graphics::IVertexStreamSource>(*AssetManager);
            auto AuthoredMesh = m_settings.MeshAsset.LoadAsset();
            if (RuntimeSource && *RuntimeSource)
            {
                std::string AssetToken = BuildMeshAssetToken(m_settings.MeshAsset);
                if (AssetToken.empty())
                {
                    AssetToken = "asset-ref://" + m_settings.MeshAsset.DisplayLabel();
                }
                if (auto RenderObject = std::make_shared<SnAPI::Graphics::MeshRenderObject>())
                {
                    RenderObject->SetVertexStreamSource(*RuntimeSource);
                    m_renderObject = std::move(RenderObject);
                    m_loadedPath = AssetToken;
                    m_loadedFromAsset = true;
                    if (AuthoredMesh && *AuthoredMesh)
                    {
                        m_loadedMeshMaterialInstances = BuildMaterialRefsFromPayloads((*AuthoredMesh)->Mesh.MaterialInstances);
                    }
                    else
                    {
                        m_loadedMeshMaterialInstances.clear();
                    }
                    m_loadedStreamSource.reset();
                    m_registered = false;
                    m_settings.MeshPath.clear();

                    ApplyConfiguredMaterialInstances(*m_renderObject);
                    ApplySharedMaterialInstances(*m_renderObject);
                    ApplyRenderObjectState(*m_renderObject);

                    return true;
                }
            }
        }
    }

    // Primitive was specified
    if (const auto PrimitiveSource = BuildPrimitiveSourceFromMeshPath(m_settings.MeshPath))
    {
        auto RenderObject = std::make_shared<SnAPI::Graphics::MeshRenderObject>();
        if (!RenderObject)
        {
            return false;
        }

        RenderObject->SetVertexStreamSource(PrimitiveSource);
        m_renderObject = std::move(RenderObject);
        m_loadedPath = ToLowerASCII(m_settings.MeshPath);
        m_loadedFromAsset = false;
        m_loadedMeshMaterialInstances.clear();
        m_loadedStreamSource.reset();
        m_registered = false;
        m_settings.MeshAsset.Clear();

        ApplyConfiguredMaterialInstances(*m_renderObject);
        ApplySharedMaterialInstances(*m_renderObject);
        ApplyRenderObjectState(*m_renderObject);
        m_lastFailedPathLoadKey.clear();

        return true;
    }
    return false;
}

void StaticMeshComponent::ApplyConfiguredMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject)
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

void StaticMeshComponent::SyncRenderObjectTransform(SnAPI::Graphics::IRenderObject& RenderObject) const
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

void StaticMeshComponent::ApplySharedMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject) const
{
    
    if (!m_sharedGBufferInstance && !m_sharedShadowInstance)
    {
        return;
    }

    const auto& Source = RenderObject.VertexStreamSource();
    if (!Source)
    {
        return;
    }

    for (std::size_t SubMeshIndex = 0; SubMeshIndex < Source->SubMeshCount(); ++SubMeshIndex)
    {
        if (m_sharedGBufferInstance)
        {
            RenderObject.SetMaterialInstance(static_cast<std::uint32_t>(SubMeshIndex), m_sharedGBufferInstance);
        }
        if (m_sharedShadowInstance)
        {
            RenderObject.SetShadowMaterialInstance(static_cast<std::uint32_t>(SubMeshIndex), m_sharedShadowInstance);
        }
    }
}

void StaticMeshComponent::ApplyRenderObjectState(SnAPI::Graphics::IRenderObject& RenderObject)
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

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
