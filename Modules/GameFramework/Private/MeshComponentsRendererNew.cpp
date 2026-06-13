#include "StaticMeshComponent.h"
#include "SkeletalMeshComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "IWorld.h"
#include "RenderAssets/MeshRuntimeAssets.h"
#include "RendererSystem.h"
#include "TransformComponent.h"

#include "Scene/PrimitiveGeometry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <SnAPI/Math/LinearAlgebra.h>

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

[[nodiscard]] std::uint64_t StableTextKey(const std::string_view Value) noexcept
{
    std::uint64_t Hash = 1469598103934665603ull;
    for (const unsigned char Ch : Value)
    {
        Hash ^= static_cast<std::uint64_t>(Ch);
        Hash *= 1099511628211ull;
    }
    return Hash == 0u ? 1u : Hash;
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

[[nodiscard]] std::optional<SnAPI::Renderer::PrimitiveMeshData> BuildPrimitiveMeshDataFromToken(
    const std::string_view MeshPath)
{
    const std::string Token = ToLowerASCII(MeshPath);

    SnAPI::Renderer::PrimitiveMeshData MeshData{};
    if (Token == "primitive://box" || Token == "__primitive_box__")
    {
        MeshData = SnAPI::Renderer::PrimitiveGeometry::MakeBox(1.0, 1.0, 1.0);
    }
    else if (Token == "primitive://sphere" || Token == "__primitive_sphere__")
    {
        MeshData = SnAPI::Renderer::PrimitiveGeometry::MakeUvSphere(0.5, 32u, 16u);
    }
    else if (Token == "primitive://capsule" || Token == "__primitive_capsule__")
    {
        MeshData = SnAPI::Renderer::PrimitiveGeometry::MakeCapsule(0.35, 0.6, 24u, 8u);
    }
    else if (Token == "primitive://cone" || Token == "__primitive_cone__")
    {
        MeshData = SnAPI::Renderer::PrimitiveGeometry::MakeCone(0.5, 1.0, 24u);
    }
    else if (Token == "primitive://pyramid" || Token == "__primitive_pyramid__")
    {
        MeshData = SnAPI::Renderer::PrimitiveGeometry::MakeCone(0.70710678118, 1.0, 4u);
    }
    else
    {
        return std::nullopt;
    }

    MeshData.DebugName = Token;
    MeshData.GeometrySignature = StableTextKey(Token);
    return MeshData;
}

template <typename TAssetRef>
[[nodiscard]] std::string BuildMeshAssetToken(const TAssetRef& MeshAssetRef)
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

    const std::string Label = MeshAssetRef.DisplayLabel();
    return Label.empty() ? std::string{} : "asset-ref://" + Label;
}

bool IsFiniteVec3(const Vec3& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z());
}

bool IsFiniteQuat(const Quat& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z()) && std::isfinite(Value.w());
}

SnAPI::Math::Matrix4 ComposeNativeWorldTransform(const NodeTransform& Transform)
{
    const SnAPI::Math::Vec3 Position{
        static_cast<SnAPI::Math::Scalar>(Transform.Position.x()),
        static_cast<SnAPI::Math::Scalar>(Transform.Position.y()),
        static_cast<SnAPI::Math::Scalar>(Transform.Position.z())};
    const SnAPI::Math::Vec3 Scale{
        static_cast<SnAPI::Math::Scalar>(Transform.Scale.x()),
        static_cast<SnAPI::Math::Scalar>(Transform.Scale.y()),
        static_cast<SnAPI::Math::Scalar>(Transform.Scale.z())};

    SnAPI::Math::Quat Rotation = SnAPI::Math::Quat::Identity();
    Rotation.x() = static_cast<SnAPI::Math::Scalar>(Transform.Rotation.x());
    Rotation.y() = static_cast<SnAPI::Math::Scalar>(Transform.Rotation.y());
    Rotation.z() = static_cast<SnAPI::Math::Scalar>(Transform.Rotation.z());
    Rotation.w() = static_cast<SnAPI::Math::Scalar>(Transform.Rotation.w());
    if (Rotation.squaredNorm() > static_cast<SnAPI::Math::Scalar>(0))
    {
        Rotation.normalize();
    }
    else
    {
        Rotation = SnAPI::Math::Quat::Identity();
    }

    auto WorldTransform = SnAPI::Math::Transform::Identity();
    WorldTransform.translate(Position);
    WorldTransform.rotate(Rotation);
    WorldTransform.scale(Scale);
    return WorldTransform.matrix();
}

bool TryBuildNativeWorldTransform(const BaseComponent& Component, SnAPI::Math::Matrix4& OutWorldFromLocal)
{
    auto* Owner = Component.OwnerNode();
    if (!Owner)
    {
        OutWorldFromLocal = SnAPI::Math::Matrix4::Identity();
        return true;
    }

    NodeTransform WorldTransform{};
    if (!TransformComponent::TryGetNodeWorldTransform(*Owner, WorldTransform))
    {
        OutWorldFromLocal = SnAPI::Math::Matrix4::Identity();
        return true;
    }

    if (!IsFiniteVec3(WorldTransform.Position)
        || !IsFiniteQuat(WorldTransform.Rotation)
        || !IsFiniteVec3(WorldTransform.Scale))
    {
        return false;
    }

    OutWorldFromLocal = ComposeNativeWorldTransform(WorldTransform);
    return true;
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
        || Name == "RetainInScene"
        || Name == "MaterialInstanceOverrides";
}

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
        || Name == "RetainInScene"
        || Name == "AutoPlayAnimations"
        || Name == "LoopAnimations"
        || Name == "AnimationName"
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

void StaticMeshComponent::ClearMesh()
{
    if (auto* Renderer = ResolveRendererSystem(); Renderer && Renderer->IsInitialized())
    {
        if (m_renderObject.Valid())
        {
            (void)Renderer->DestroyRenderObject(m_renderObject);
        }
        if (m_renderMesh.Valid())
        {
            (void)Renderer->DestroyRenderMesh(m_renderMesh);
        }
    }
    else
    {
        m_renderObject.Reset();
        m_renderMesh.Reset();
    }

    m_loadedPath.clear();
    m_loadedFromAsset = false;
    m_loadedMeshMaterialInstances.clear();
    m_retainedSceneObjectStateInitialized = false;
}

void StaticMeshComponent::OnCreate()
{
    (void)EnsureMeshLoaded();
    UpdateRetainedSceneObject();
}

void StaticMeshComponent::OnDestroy()
{
    ClearMesh();
}

void StaticMeshComponent::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;

    if (m_settings.MeshPath.empty() && m_settings.MeshAsset.IsNull())
    {
        ClearMesh();
        return;
    }

    if (!m_settings.MeshAsset.IsNull())
    {
        const std::string AssetToken = BuildMeshAssetToken(m_settings.MeshAsset);
        if (!m_loadedFromAsset || (!AssetToken.empty() && m_loadedPath != AssetToken))
        {
            ClearMesh();
        }
    }
    else if (IsPrimitiveMeshToken(m_settings.MeshPath))
    {
        const std::string PrimitiveToken = ToLowerASCII(m_settings.MeshPath);
        if (m_loadedPath != PrimitiveToken)
        {
            ClearMesh();
        }
    }
    else
    {
        ClearMesh();
        return;
    }

    if (!EnsureMeshLoaded())
    {
        return;
    }

    if (m_settings.SyncFromTransform)
    {
        SyncRenderObjectTransform();
    }
    UpdateRetainedSceneObject();
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
        ClearMesh();
    }

    if (!EnsureMeshLoaded())
    {
        return;
    }

    if (m_settings.SyncFromTransform)
    {
        SyncRenderObjectTransform();
    }
    UpdateRetainedSceneObject();
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
    if (m_renderMesh.Valid())
    {
        return true;
    }

    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return false;
    }

    if (!m_settings.MeshAsset.IsNull())
    {
        auto* AssetManager = ResolveDefaultAssetManager();
        if (!AssetManager)
        {
            return false;
        }

        auto RuntimeMesh = m_settings.MeshAsset.GetRuntimeShared<StaticMeshRuntime>(*AssetManager);
        if (!RuntimeMesh || !*RuntimeMesh || !(*RuntimeMesh)->MeshData)
        {
            return false;
        }

        std::string AssetToken = BuildMeshAssetToken(m_settings.MeshAsset);
        if (AssetToken.empty())
        {
            AssetToken = "asset-ref://" + m_settings.MeshAsset.DisplayLabel();
        }
        if (!Renderer->CreateStaticRenderMesh(*(*RuntimeMesh)->MeshData, m_renderMesh, AssetToken))
        {
            return false;
        }

        m_loadedPath = AssetToken;
        m_loadedFromAsset = true;
        m_loadedMeshMaterialInstances = (*RuntimeMesh)->MaterialRefs;
        m_settings.MeshPath.clear();
    }
    else if (auto PrimitiveMesh = BuildPrimitiveMeshDataFromToken(m_settings.MeshPath))
    {
        const std::string PrimitiveToken = ToLowerASCII(m_settings.MeshPath);
        if (!Renderer->CreateStaticRenderMesh(std::move(*PrimitiveMesh), m_renderMesh, PrimitiveToken))
        {
            return false;
        }

        m_loadedPath = PrimitiveToken;
        m_loadedFromAsset = false;
        m_loadedMeshMaterialInstances.clear();
        m_settings.MeshAsset.Clear();
        m_lastFailedPathLoadKey.clear();
    }
    else
    {
        return false;
    }

    m_retainedSceneObjectStateInitialized = false;
    UpdateRetainedSceneObject();
    return m_renderMesh.Valid();
}

void StaticMeshComponent::SyncRenderObjectTransform()
{
    if (!m_renderObject.Valid())
    {
        return;
    }

    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return;
    }

    SnAPI::Math::Matrix4 WorldFromLocal = SnAPI::Math::Matrix4::Identity();
    if (!TryBuildNativeWorldTransform(*this, WorldFromLocal))
    {
        return;
    }

    (void)Renderer->SetRenderObjectTransform(m_renderObject, WorldFromLocal);
}

void StaticMeshComponent::UpdateRetainedSceneObject()
{
    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized() || !m_renderMesh.Valid())
    {
        return;
    }

    const bool EffectiveVisible = m_settings.RetainInScene && m_settings.Visible;
    if (!EffectiveVisible)
    {
        if (m_renderObject.Valid())
        {
            (void)Renderer->DestroyRenderObject(m_renderObject);
        }
        m_retainedSceneObjectStateInitialized = true;
        m_lastVisible = false;
        m_lastCastShadows = m_settings.CastShadows;
        return;
    }

    const bool NeedsObjectRebuild = !m_renderObject.Valid()
                                 || !m_retainedSceneObjectStateInitialized
                                 || m_lastCastShadows != m_settings.CastShadows;
    if (NeedsObjectRebuild)
    {
        if (m_renderObject.Valid())
        {
            (void)Renderer->DestroyRenderObject(m_renderObject);
        }

        SnAPI::Math::Matrix4 WorldFromLocal = SnAPI::Math::Matrix4::Identity();
        if (!TryBuildNativeWorldTransform(*this, WorldFromLocal))
        {
            return;
        }

        const std::string_view DebugName = m_loadedPath.empty()
            ? std::string_view{m_renderMesh.DebugName()}
            : std::string_view{m_loadedPath};
        if (!Renderer->CreateStaticRenderObject(m_renderMesh, m_renderObject, WorldFromLocal, m_settings.CastShadows, DebugName))
        {
            return;
        }
        if (auto* Owner = OwnerNode())
        {
            m_renderObject.SetOwnerNode(Owner->Handle());
        }
    }

    m_retainedSceneObjectStateInitialized = true;
    m_lastVisible = true;
    m_lastCastShadows = m_settings.CastShadows;
}

bool SkeletalMeshComponent::ReloadMesh()
{
    m_lastFailedPathLoadKey.clear();
    ClearMesh();
    return EnsureMeshLoaded();
}

void SkeletalMeshComponent::ClearMesh()
{
    if (auto* Renderer = ResolveRendererSystem(); Renderer && Renderer->IsInitialized())
    {
        if (m_renderObject.Valid())
        {
            (void)Renderer->DestroyRenderObject(m_renderObject);
        }
        if (m_renderMesh.Valid())
        {
            (void)Renderer->DestroyRenderMesh(m_renderMesh);
        }
    }
    else
    {
        m_renderObject.Reset();
        m_renderMesh.Reset();
    }

    m_loadedPath.clear();
    m_loadedFromAsset = false;
    m_loadedMeshMaterialInstances.clear();
    m_lastAutoPlayAnimation.clear();
    m_lastAutoPlayLoop = true;
    m_autoPlayApplied = false;
    m_retainedSceneObjectStateInitialized = false;
}

bool SkeletalMeshComponent::PlayAnimation(const std::string& Name, const bool Loop, const float StartTime)
{
    (void)StartTime;
    if (!EnsureMeshLoaded())
    {
        return false;
    }

    m_lastAutoPlayAnimation = Name;
    m_lastAutoPlayLoop = Loop;
    m_autoPlayApplied = true;
    return true;
}

bool SkeletalMeshComponent::PlayAllAnimations(const bool Loop, const float StartTime)
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
    UpdateRetainedSceneObject();
}

void SkeletalMeshComponent::OnDestroy()
{
    ClearMesh();
}

void SkeletalMeshComponent::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;

    if (m_settings.MeshPath.empty() && m_settings.MeshAsset.IsNull())
    {
        ClearMesh();
        return;
    }

    if (!m_settings.MeshAsset.IsNull())
    {
        const std::string AssetToken = BuildMeshAssetToken(m_settings.MeshAsset);
        if (!m_loadedFromAsset || (!AssetToken.empty() && m_loadedPath != AssetToken))
        {
            ClearMesh();
        }
    }
    else
    {
        ClearMesh();
        return;
    }

    if (!EnsureMeshLoaded())
    {
        return;
    }

    if (m_settings.SyncFromTransform)
    {
        SyncRenderObjectTransform();
    }
    UpdateRetainedSceneObject();
    if (!m_settings.AutoPlayAnimations)
    {
        m_autoPlayApplied = false;
        m_lastAutoPlayAnimation = m_settings.AnimationName;
        m_lastAutoPlayLoop = m_settings.LoopAnimations;
    }
    else
    {
        if (m_lastAutoPlayAnimation != m_settings.AnimationName
            || m_lastAutoPlayLoop != m_settings.LoopAnimations)
        {
            m_autoPlayApplied = false;
        }

        m_lastAutoPlayAnimation = m_settings.AnimationName;
        m_lastAutoPlayLoop = m_settings.LoopAnimations;
        m_autoPlayApplied = true;
    }
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
        ClearMesh();
    }

    if (!EnsureMeshLoaded())
    {
        return;
    }

    if (m_settings.SyncFromTransform)
    {
        SyncRenderObjectTransform();
    }
    UpdateRetainedSceneObject();
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
    if (m_renderMesh.Valid())
    {
        return true;
    }

    if (m_settings.MeshAsset.IsNull())
    {
        return false;
    }

    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return false;
    }

    auto* AssetManager = ResolveDefaultAssetManager();
    if (!AssetManager)
    {
        return false;
    }

    auto RuntimeMesh = m_settings.MeshAsset.GetRuntimeShared<SkeletalMeshRuntime>(*AssetManager);
    if (!RuntimeMesh || !*RuntimeMesh || !(*RuntimeMesh)->MeshData)
    {
        return false;
    }

    std::string AssetToken = BuildMeshAssetToken(m_settings.MeshAsset);
    if (AssetToken.empty())
    {
        AssetToken = "asset-ref://" + m_settings.MeshAsset.DisplayLabel();
    }
    if (!Renderer->CreateStaticRenderMesh(*(*RuntimeMesh)->MeshData, m_renderMesh, AssetToken))
    {
        return false;
    }

    m_loadedPath = AssetToken;
    m_loadedFromAsset = true;
    m_loadedMeshMaterialInstances = (*RuntimeMesh)->MaterialRefs;
    m_lastAutoPlayAnimation.clear();
    m_lastAutoPlayLoop = m_settings.LoopAnimations;
    m_autoPlayApplied = false;
    m_retainedSceneObjectStateInitialized = false;
    UpdateRetainedSceneObject();
    return m_renderMesh.Valid();
}

void SkeletalMeshComponent::SyncRenderObjectTransform()
{
    if (!m_renderObject.Valid())
    {
        return;
    }

    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return;
    }

    SnAPI::Math::Matrix4 WorldFromLocal = SnAPI::Math::Matrix4::Identity();
    if (!TryBuildNativeWorldTransform(*this, WorldFromLocal))
    {
        return;
    }

    (void)Renderer->SetRenderObjectTransform(m_renderObject, WorldFromLocal);
}

void SkeletalMeshComponent::UpdateRetainedSceneObject()
{
    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized() || !m_renderMesh.Valid())
    {
        return;
    }

    const bool EffectiveVisible = m_settings.RetainInScene && m_settings.Visible;
    if (!EffectiveVisible)
    {
        if (m_renderObject.Valid())
        {
            (void)Renderer->DestroyRenderObject(m_renderObject);
        }
        m_retainedSceneObjectStateInitialized = true;
        m_lastVisible = false;
        m_lastCastShadows = m_settings.CastShadows;
        return;
    }

    const bool NeedsObjectRebuild = !m_renderObject.Valid()
                                 || !m_retainedSceneObjectStateInitialized
                                 || m_lastCastShadows != m_settings.CastShadows;
    if (NeedsObjectRebuild)
    {
        if (m_renderObject.Valid())
        {
            (void)Renderer->DestroyRenderObject(m_renderObject);
        }

        SnAPI::Math::Matrix4 WorldFromLocal = SnAPI::Math::Matrix4::Identity();
        if (!TryBuildNativeWorldTransform(*this, WorldFromLocal))
        {
            return;
        }

        const std::string_view DebugName = m_loadedPath.empty()
            ? std::string_view{m_renderMesh.DebugName()}
            : std::string_view{m_loadedPath};
        if (!Renderer->CreateStaticRenderObject(m_renderMesh, m_renderObject, WorldFromLocal, m_settings.CastShadows, DebugName))
        {
            return;
        }
        if (auto* Owner = OwnerNode())
        {
            m_renderObject.SetOwnerNode(Owner->Handle());
        }
    }

    m_retainedSceneObjectStateInitialized = true;
    m_lastVisible = true;
    m_lastCastShadows = m_settings.CastShadows;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
