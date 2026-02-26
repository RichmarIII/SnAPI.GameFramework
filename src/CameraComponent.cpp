#include "CameraComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "Profiling.h"

#include <algorithm>
#include <cmath>

#include <SnAPI/Math/LinearAlgebra.h>
#include <LinearAlgebra.hpp>
#include <CameraBase.hpp>

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

SnAPI::Vector3D ToRendererVector3(const Vec3& Value)
{
    return SnAPI::Vector3D{
        static_cast<SnAPI::Vector3D::Scalar>(Value.x()),
        static_cast<SnAPI::Vector3D::Scalar>(Value.y()),
        static_cast<SnAPI::Vector3D::Scalar>(Value.z())};
}

SnAPI::QuaternionF ToRendererRotation(const Quat& Rotation)
{
    SnAPI::QuaternionF Out = SnAPI::QuaternionF::Identity();
    Out.x() = static_cast<SnAPI::QuaternionF::Scalar>(Rotation.x());
    Out.y() = static_cast<SnAPI::QuaternionF::Scalar>(Rotation.y());
    Out.z() = static_cast<SnAPI::QuaternionF::Scalar>(Rotation.z());
    Out.w() = static_cast<SnAPI::QuaternionF::Scalar>(Rotation.w());
    if (Out.squaredNorm() > 0.0f)
    {
        Out.normalize();
    }
    else
    {
        Out = SnAPI::QuaternionF::Identity();
    }
    return Out;
}

Quat EulerToQuat(const Vec3& Euler)
{
    const SnAPI::Math::Quaternion Rotation = SnAPI::Math::AngleAxis3D(Euler.z(), SnAPI::Math::Vector3::UnitZ())
                                           * SnAPI::Math::AngleAxis3D(Euler.y(), SnAPI::Math::Vector3::UnitY())
                                           * SnAPI::Math::AngleAxis3D(Euler.x(), SnAPI::Math::Vector3::UnitX());
    Quat Out = Quat::Identity();
    Out.x() = static_cast<Quat::Scalar>(Rotation.x());
    Out.y() = static_cast<Quat::Scalar>(Rotation.y());
    Out.z() = static_cast<Quat::Scalar>(Rotation.z());
    Out.w() = static_cast<Quat::Scalar>(Rotation.w());
    if (Out.squaredNorm() > static_cast<Quat::Scalar>(0))
    {
        Out.normalize();
    }
    else
    {
        Out = Quat::Identity();
    }
    return Out;
}
} // namespace

void CameraComponent::CameraDeleter::operator()(SnAPI::Graphics::CameraBase* Camera) const
{
    delete Camera;
}

SnAPI::Graphics::CameraBase* CameraComponent::Camera()
{
    return m_camera.get();
}

const SnAPI::Graphics::CameraBase* CameraComponent::Camera() const
{
    return m_camera.get();
}

CameraComponent::~CameraComponent() = default;

void CameraComponent::SetActive(const bool Active)
{
    m_settings.Active = Active;
    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !m_camera || !Renderer->IsInitialized())
    {
        return;
    }

    if (m_settings.Active)
    {
        (void)Renderer->SetActiveCamera(m_camera.get());
    }
    else if (Renderer->ActiveCamera() == m_camera.get())
    {
        (void)Renderer->SetActiveCamera(nullptr);
    }
}

void CameraComponent::OnCreate()
{
    EnsureCamera();
    ApplyCameraSettings();
    SyncFromTransform();

    if (auto* Renderer = ResolveRendererSystem(); Renderer && Renderer->IsInitialized() && m_settings.Active)
    {
        (void)Renderer->SetActiveCamera(m_camera.get());
    }
}

void CameraComponent::OnDestroy()
{
    if (auto* Renderer = ResolveRendererSystem(); Renderer && Renderer->IsInitialized() && Renderer->ActiveCamera() == m_camera.get())
    {
        (void)Renderer->SetActiveCamera(nullptr);
    }
    m_camera.reset();
}

void CameraComponent::Tick(float DeltaSeconds)
{
    RuntimeTick(DeltaSeconds);
}

void CameraComponent::RuntimeTick(const float DeltaSeconds)
{
    UpdateCamera(DeltaSeconds);
}

void CameraComponent::LateTick(float DeltaSeconds)
{
    RuntimeLateTick(DeltaSeconds);
}

void CameraComponent::RuntimeLateTick(const float DeltaSeconds)
{
    UpdateCamera(DeltaSeconds);
}

void CameraComponent::UpdateCamera(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    EnsureCamera();
    ApplyCameraSettings();
    SyncFromTransform();

    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return;
    }

    if (m_settings.Active)
    {
        if (Renderer->ActiveCamera() != m_camera.get())
        {
            (void)Renderer->SetActiveCamera(m_camera.get());
        }
    }
    else if (Renderer->ActiveCamera() == m_camera.get())
    {
        (void)Renderer->SetActiveCamera(nullptr);
    }
}

RendererSystem* CameraComponent::ResolveRendererSystem() const
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

void CameraComponent::EnsureCamera()
{
    if (!m_camera)
    {
        m_camera.reset(new SnAPI::Graphics::CameraBase());
    }
}

void CameraComponent::ApplyCameraSettings()
{
    if (!m_camera)
    {
        return;
    }

    const float NearClip = std::max(0.0001f, m_settings.NearClip);
    const float FarClip = std::max(NearClip + 0.001f, m_settings.FarClip);
    const float Fov = std::clamp(m_settings.FovDegrees, 1.0f, 179.0f);
    const float Aspect = std::max(0.001f, m_settings.Aspect);

    m_camera->Near(NearClip);
    m_camera->Far(FarClip);
    m_camera->Fov(Fov);
    m_camera->Aspect(Aspect);
}

void CameraComponent::SyncFromTransform()
{
    if (!m_camera || !m_settings.SyncFromTransform)
    {
        return;
    }

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

    if (!IsFiniteVec3(WorldTransform.Position) || !IsFiniteQuat(WorldTransform.Rotation))
    {
        return;
    }

    Vec3 CameraPosition = WorldTransform.Position;
    Quat CameraRotation = WorldTransform.Rotation;

    if (IsFiniteVec3(m_settings.LocalPositionOffset))
    {
        CameraPosition += CameraRotation * m_settings.LocalPositionOffset;
    }

    if (IsFiniteVec3(m_settings.LocalRotationOffsetEuler))
    {
        CameraRotation *= EulerToQuat(m_settings.LocalRotationOffsetEuler);
        if (CameraRotation.squaredNorm() > static_cast<Quat::Scalar>(0))
        {
            CameraRotation.normalize();
        }
        else
        {
            CameraRotation = Quat::Identity();
        }
    }

    if (!IsFiniteVec3(CameraPosition) || !IsFiniteQuat(CameraRotation))
    {
        return;
    }

    m_camera->Position(ToRendererVector3(CameraPosition));
    m_camera->Rotation(ToRendererRotation(CameraRotation));
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
