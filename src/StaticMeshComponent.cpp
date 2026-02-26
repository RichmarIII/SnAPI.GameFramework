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

#include <BoxStreamSource.hpp>
#include <CapsuleStreamSource.hpp>
#include <ConeStreamSource.hpp>
#include <LinearAlgebra.hpp>
#include <MeshManager.hpp>
#include <MeshRenderObject.hpp>
#include <PyramidStreamSource.hpp>
#include <SphereStreamSource.hpp>

#include "BaseNode.h"
#include "IWorld.h"
#include "RendererSystem.h"
#include "TransformComponent.h"

namespace SnAPI::GameFramework
{
namespace
{
std::string ToLowerASCII(const std::string_view Value)
{
    std::string Out(Value);
    std::transform(Out.begin(), Out.end(), Out.begin(), [](const unsigned char Ch) {
        return static_cast<char>(std::tolower(Ch));
    });
    return Out;
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
} // namespace

bool StaticMeshComponent::ReloadMesh()
{
    
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
    
    m_renderObject.reset();
    m_loadedPath.clear();
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

void StaticMeshComponent::Tick(float DeltaSeconds)
{
    RuntimeTick(DeltaSeconds);
}

void StaticMeshComponent::RuntimeTick(float DeltaSeconds)
{
    (void)DeltaSeconds;

    if (m_settings.MeshPath.empty() && !m_streamSource)
    {
        ClearMesh();
        return;
    }

    if (!m_streamSource && m_loadedPath != m_settings.MeshPath)
    {
        ClearMesh();
    }
    if (m_streamSource && m_loadedStreamSource.lock() != m_streamSource)
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
    
    if (m_settings.MeshPath.empty() && !m_streamSource)
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
        m_loadedStreamSource = m_streamSource;
        m_registered = false;

        (void)Renderer->ApplyDefaultMaterials(*m_renderObject);
        ApplySharedMaterialInstances(*m_renderObject);
        ApplyRenderObjectState(*m_renderObject);

        return true;
    }

    if (const auto PrimitiveSource = BuildPrimitiveSourceFromMeshPath(m_settings.MeshPath))
    {
        auto RenderObject = std::make_shared<SnAPI::Graphics::MeshRenderObject>();
        if (!RenderObject)
        {
            return false;
        }

        RenderObject->SetVertexStreamSource(PrimitiveSource);
        m_renderObject = std::move(RenderObject);
        m_loadedPath = m_settings.MeshPath;
        m_loadedStreamSource.reset();
        m_registered = false;

        (void)Renderer->ApplyDefaultMaterials(*m_renderObject);
        ApplySharedMaterialInstances(*m_renderObject);
        ApplyRenderObjectState(*m_renderObject);

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
    m_loadedStreamSource.reset();
    m_registered = false;

    (void)Renderer->ApplyDefaultMaterials(*m_renderObject);
    ApplySharedMaterialInstances(*m_renderObject);
    ApplyRenderObjectState(*m_renderObject);

    return true;
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

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
