#include "StaticMeshComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "Profiling.h"

#include <cstddef>
#include <cstdint>
#include <cmath>

#include <LinearAlgebra.hpp>
#include <MeshManager.hpp>
#include <MeshRenderObject.hpp>

#include "BaseNode.h"
#include "IWorld.h"
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

SnAPI::Matrix4 ComposeRendererWorldTransform(const TransformComponent& Transform)
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
} // namespace

bool StaticMeshComponent::ReloadMesh()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    ClearMesh();
    return EnsureMeshLoaded();
}

void StaticMeshComponent::SetSharedMaterialInstances(std::shared_ptr<SnAPI::Graphics::MaterialInstance> GBufferInstance,
                                                     std::shared_ptr<SnAPI::Graphics::MaterialInstance> ShadowInstance)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    m_sharedGBufferInstance = std::move(GBufferInstance);
    m_sharedShadowInstance = std::move(ShadowInstance);

    if (m_renderObject)
    {
        ApplySharedMaterialInstances(*m_renderObject);
    }
}

void StaticMeshComponent::ClearMesh()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    m_renderObject.reset();
    m_loadedPath.clear();
    m_registered = false;
    m_passStateInitialized = false;
}

void StaticMeshComponent::OnCreate()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    (void)EnsureMeshLoaded();
}

void StaticMeshComponent::OnDestroy()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    ClearMesh();
}

void StaticMeshComponent::Tick(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    (void)DeltaSeconds;

    if (m_settings.MeshPath.empty())
    {
        ClearMesh();
        return;
    }

    if (m_loadedPath != m_settings.MeshPath)
    {
        ClearMesh();
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

    if (m_settings.SyncFromTransform)
    {
        SyncRenderObjectTransform(*m_renderObject);
    }
    ApplyRenderObjectState(*m_renderObject);
}

RendererSystem* StaticMeshComponent::ResolveRendererSystem() const
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
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
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_settings.MeshPath.empty())
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

    auto* Meshes = SnAPI::Graphics::MeshManager::Instance();
    if (!Meshes)
    {
        return false;
    }

    const auto LoadedMesh = Meshes->Load(m_settings.MeshPath);
    const auto SourceMesh = LoadedMesh.lock();
    if (!SourceMesh)
    {
        return false;
    }

    auto RenderObject = std::make_shared<SnAPI::Graphics::MeshRenderObject>(SourceMesh);
    if (!RenderObject)
    {
        return false;
    }

    m_renderObject = std::move(RenderObject);
    m_loadedPath = m_settings.MeshPath;
    m_registered = false;

    (void)Renderer->ApplyDefaultMaterials(*m_renderObject);
    ApplySharedMaterialInstances(*m_renderObject);
    ApplyRenderObjectState(*m_renderObject);

    return true;
}

void StaticMeshComponent::SyncRenderObjectTransform(SnAPI::Graphics::IRenderObject& RenderObject) const
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    auto* Owner = OwnerNode();
    if (!Owner)
    {
        return;
    }

    auto TransformResult = Owner->Component<TransformComponent>();
    if (!TransformResult)
    {
        return;
    }

    if (!IsFiniteVec3(TransformResult->Position)
        || !IsFiniteQuat(TransformResult->Rotation)
        || !IsFiniteVec3(TransformResult->Scale))
    {
        return;
    }

    RenderObject.SetWorldTransform(ComposeRendererWorldTransform(*TransformResult));
}

void StaticMeshComponent::ApplySharedMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject) const
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
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
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return;
    }

    const bool PassStateChanged = !m_passStateInitialized
                               || m_lastVisible != m_settings.Visible
                               || m_lastCastShadows != m_settings.CastShadows;
    if (PassStateChanged)
    {
        if (Renderer->ConfigureRenderObjectPasses(RenderObject, m_settings.Visible, m_settings.CastShadows))
        {
            m_passStateInitialized = true;
            m_lastVisible = m_settings.Visible;
            m_lastCastShadows = m_settings.CastShadows;
        }
    }

    if (!m_settings.RegisterWithRenderer)
    {
        m_registered = false;
        return;
    }

    if (!m_registered)
    {
        if (Renderer->RegisterRenderObject(m_renderObject))
        {
            m_registered = true;
        }
    }
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
