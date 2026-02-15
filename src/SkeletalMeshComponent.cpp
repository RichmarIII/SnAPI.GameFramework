#include "SkeletalMeshComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "Profiling.h"

#include <cmath>

#include <LinearAlgebra.hpp>
#include <Mesh.hpp>
#include <MeshManager.hpp>

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

SnAPI::Matrix4 ComposeRendererLocalTransform(const TransformComponent& Transform)
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

    auto LocalTransform = SnAPI::Transform3D::Identity();
    LocalTransform.translate(Position);
    LocalTransform.rotate(Rotation);
    LocalTransform.scale(Scale);
    return LocalTransform.matrix();
}
} // namespace

bool SkeletalMeshComponent::ReloadMesh()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    ClearMesh();
    return EnsureMeshLoaded();
}

void SkeletalMeshComponent::ClearMesh()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    m_mesh.reset();
    m_loadedPath.clear();
    m_lastAutoPlayAnimation.clear();
    m_lastAutoPlayLoop = true;
    m_autoPlayApplied = false;
    m_registered = false;
    m_passStateInitialized = false;
}

bool SkeletalMeshComponent::PlayAnimation(const std::string& Name, const bool Loop, const float StartTime)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!m_mesh)
    {
        if (!EnsureMeshLoaded())
        {
            return false;
        }
        if (!m_mesh)
        {
            return false;
        }
    }

    m_mesh->PlayRigidAnimation(Name, StartTime, Loop);
    return true;
}

bool SkeletalMeshComponent::PlayAllAnimations(const bool Loop, const float StartTime)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!m_mesh)
    {
        if (!EnsureMeshLoaded())
        {
            return false;
        }
        if (!m_mesh)
        {
            return false;
        }
    }

    m_mesh->PlayRigidAnimations(StartTime, Loop);
    return true;
}

void SkeletalMeshComponent::StopAnimations()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_mesh)
    {
        for (auto& Animation : m_mesh->RigidAnimations)
        {
            Animation.Stop();
        }
    }
    m_autoPlayApplied = false;
}

void SkeletalMeshComponent::OnCreate()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    (void)EnsureMeshLoaded();
}

void SkeletalMeshComponent::OnDestroy()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    ClearMesh();
}

void SkeletalMeshComponent::Tick(const float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
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

    if (!m_mesh)
    {
        ClearMesh();
        return;
    }

    if (m_settings.SyncFromTransform)
    {
        SyncMeshTransform(*m_mesh);
    }
    ApplyMeshRenderingState(*m_mesh);
    ApplyAutoPlay(*m_mesh);
    m_mesh->Update(DeltaSeconds);
}

RendererSystem* SkeletalMeshComponent::ResolveRendererSystem() const
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

bool SkeletalMeshComponent::EnsureMeshLoaded()
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

    if (m_mesh)
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

    auto Mesh = std::make_shared<SnAPI::Graphics::Mesh>(*SourceMesh);
    if (!Mesh)
    {
        return false;
    }

    m_mesh = std::move(Mesh);
    m_loadedPath = m_settings.MeshPath;
    m_lastAutoPlayAnimation.clear();
    m_lastAutoPlayLoop = m_settings.LoopAnimations;
    m_autoPlayApplied = false;
    m_registered = false;

    (void)Renderer->ApplyDefaultMaterials(*m_mesh);
    ApplyMeshRenderingState(*m_mesh);

    return true;
}

void SkeletalMeshComponent::SyncMeshTransform(SnAPI::Graphics::Mesh& Mesh) const
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

    Mesh.LocalTransform = ComposeRendererLocalTransform(*TransformResult);
}

void SkeletalMeshComponent::ApplyMeshRenderingState(SnAPI::Graphics::Mesh& Mesh)
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
        if (Renderer->ConfigureMeshPasses(Mesh, m_settings.Visible, m_settings.CastShadows))
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
        if (Renderer->RegisterMesh(m_mesh))
        {
            m_registered = true;
        }
    }
}

void SkeletalMeshComponent::ApplyAutoPlay(SnAPI::Graphics::Mesh& Mesh)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
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
        Mesh.PlayRigidAnimations(0.0f, m_settings.LoopAnimations);
    }
    else
    {
        Mesh.PlayRigidAnimation(m_settings.AnimationName, 0.0f, m_settings.LoopAnimations);
    }
    m_autoPlayApplied = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
