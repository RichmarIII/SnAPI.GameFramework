#include "SkeletalMeshComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "Profiling.h"

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
} // namespace

bool SkeletalMeshComponent::ReloadMesh()
{
    
    ClearMesh();
    return EnsureMeshLoaded();
}

void SkeletalMeshComponent::ClearMesh()
{
    
    m_renderObject.reset();
    m_loadedPath.clear();
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
    ApplyAutoPlay(*m_renderObject);
    m_renderObject->Update(DeltaSeconds);
}

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
    m_lastAutoPlayAnimation.clear();
    m_lastAutoPlayLoop = m_settings.LoopAnimations;
    m_autoPlayApplied = false;
    m_registered = false;

    (void)Renderer->ApplyDefaultMaterials(*m_renderObject);
    ApplyRenderObjectState(*m_renderObject);

    return true;
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
    
    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return;
    }

    const std::uint64_t PassGraphRevision = Renderer->RenderViewportPassGraphRevision();
    const bool PassStateChanged = !m_passStateInitialized
                               || m_lastVisible != m_settings.Visible
                               || m_lastCastShadows != m_settings.CastShadows
                               || m_lastPassGraphRevision != PassGraphRevision;
    if (PassStateChanged)
    {
        if (Renderer->ConfigureRenderObjectPasses(RenderObject, m_settings.Visible, m_settings.CastShadows))
        {
            m_passStateInitialized = true;
            m_lastVisible = m_settings.Visible;
            m_lastCastShadows = m_settings.CastShadows;
            m_lastPassGraphRevision = PassGraphRevision;
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
