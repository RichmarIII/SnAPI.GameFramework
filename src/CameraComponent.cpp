#include "CameraComponent.h"


#include <algorithm>
#include <cmath>

#include <SnAPI/Math/LinearAlgebra.h>

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

#if defined(WITH_EDITOR) && WITH_EDITOR
bool IsCameraSettingsField(const std::string_view Name)
{
    return Name == "Settings"
        || Name == "NearClip"
        || Name == "FarClip"
        || Name == "FovDegrees"
        || Name == "Aspect"
        || Name == "Active"
        || Name == "SyncFromTransform"
        || Name == "LocalPositionOffset"
        || Name == "LocalRotationOffsetEuler"
        || Name == "AutoActivateForPlayer";
}
#endif
} // namespace

GameRenderCamera* CameraComponent::Camera()
{
    return m_camera.get();
}

const GameRenderCamera* CameraComponent::Camera() const
{
    return m_camera.get();
}

std::shared_ptr<GameRenderCamera> CameraComponent::CameraShared() const
{
    return m_camera;
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
        (void)Renderer->SetActiveCamera(m_camera);
    }
    else if (Renderer->ActiveCamera() == m_camera.get())
    {
        (void)Renderer->SetActiveCamera(nullptr);
    }
}

void CameraComponent::OnCreate()
{
    UpdateCamera(0.0f);
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
    UpdateCamera(DeltaSeconds);
}

void CameraComponent::LateTick(float DeltaSeconds)
{
    UpdateCamera(DeltaSeconds);
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void CameraComponent::EditorTick(const float DeltaSeconds)
{
    UpdateCamera(DeltaSeconds);
}

void CameraComponent::EditorOnPropertyChanged(const std::string_view Name)
{
    if (IsCameraSettingsField(Name))
    {
        UpdateCamera(0.0f);
    }
}
#endif

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
            (void)Renderer->SetActiveCamera(m_camera);
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
        m_camera = std::make_shared<GameRenderCamera>();
    }
}

void CameraComponent::ApplyCameraSettings() const
{
    if (!m_camera)
    {
        return;
    }

    m_camera->Configure(
        m_settings.NearClip,
        m_settings.FarClip,
        m_settings.FovDegrees,
        m_settings.Aspect,
        m_camera->Position(),
        m_camera->Rotation());
}

void CameraComponent::SyncFromTransform() const
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

    m_camera->Configure(
        m_settings.NearClip,
        m_settings.FarClip,
        m_settings.FovDegrees,
        m_settings.Aspect,
        CameraPosition,
        CameraRotation);
}

} // namespace SnAPI::GameFramework
