#include "DirectionalLightComponent.h"


#include "BaseNode.h"
#include "IWorld.h"
#include "RendererSystem.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <SnAPI/Math/LinearAlgebra.h>
#include <SnAPI/Math/SemanticTransforms.h>
#include <SnAPI/Math/Types.h>

namespace SnAPI::Renderer
{
using Scalar = SnAPI::Math::Scalar;
using Vec2 = SnAPI::Math::Vec2;
using Vec3 = SnAPI::Math::Vec3;
using Vec4 = SnAPI::Math::Vec4;
using Point3 = SnAPI::Math::Point3D;
using Quat = SnAPI::Math::Quat;
using Matrix4 = SnAPI::Math::Matrix4;
using Transform = SnAPI::Math::Transform;
} // namespace SnAPI::Renderer

#include "Lights/TLightFor.h"

namespace SnAPI::GameFramework
{
namespace
{
bool IsFiniteVec3(const Vec3& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z());
}

float ClampNonNegative(const float Value)
{
    return std::max(0.0f, Value);
}

SnAPI::Renderer::Vec3 ToRendererVector3(const Vec3& Value)
{
    SnAPI::Renderer::Vec3 Result = SnAPI::Renderer::Vec3::Zero();
    Result << static_cast<double>(Value.x()),
        static_cast<double>(Value.y()),
        static_cast<double>(Value.z());
    return Result;
}

SnAPI::Renderer::Vec3 NormalizeDirectionOrDefault(const Vec3& Value)
{
    SnAPI::Renderer::Vec3 Direction = SnAPI::Renderer::Vec3::Zero();
    if (IsFiniteVec3(Value))
    {
        Direction = ToRendererVector3(Value);
    }
    if (Direction.squaredNorm() <= 0.000001)
    {
        Direction << -0.5, -1.0, -0.3;
    }
    return Direction.normalized();
}

std::array<float, 3> ToRendererColor(const Vec3& Value)
{
    const Vec3 Safe = IsFiniteVec3(Value) ? Value : Vec3{1.0, 1.0, 1.0};
    return {
        ClampNonNegative(static_cast<float>(Safe.x())),
        ClampNonNegative(static_cast<float>(Safe.y())),
        ClampNonNegative(static_cast<float>(Safe.z()))};
}

SnAPI::Renderer::DirectionalLightDesc BuildDirectionalLightDesc(
    const DirectionalLightComponent::Settings& Settings)
{
    return SnAPI::Renderer::DirectionalLightDesc{
        .DebugName = "DirectionalLightComponent",
        .DirectionWorld = NormalizeDirectionOrDefault(Settings.Direction),
        .ColorLinear = ToRendererColor(Settings.Color),
        .IlluminanceLux = ClampNonNegative(Settings.Intensity),
        .Mobility = SnAPI::Renderer::ERenderMobility::Dynamic,
        .CastsShadows = Settings.CastShadows};
}

#if defined(WITH_EDITOR) && WITH_EDITOR
bool IsDirectionalLightSettingsField(const std::string_view Name)
{
    return Name == "Settings"
        || Name == "Enabled"
        || Name == "Direction"
        || Name == "Color"
        || Name == "Intensity"
        || Name == "CastShadows"
        || Name == "CascadeCount"
        || Name == "ShadowMapSize"
        || Name == "ShadowBias"
        || Name == "ShadowFarDistance"
        || Name == "SoftnessFactor"
        || Name == "SoftShadows"
        || Name == "ContactHardening"
        || Name == "CascadeBlending";
}
#endif
} // namespace

GameRenderLight* DirectionalLightComponent::Light()
{
    return m_light.Valid() ? &m_light : nullptr;
}

const GameRenderLight* DirectionalLightComponent::Light() const
{
    return m_light.Valid() ? &m_light : nullptr;
}

void DirectionalLightComponent::OnCreate()
{
    UpdateLight(0.0f);
}

void DirectionalLightComponent::OnDestroy()
{
    ReleaseLight();
}

void DirectionalLightComponent::Tick(float DeltaSeconds)
{
    UpdateLight(DeltaSeconds);
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void DirectionalLightComponent::EditorTick(const float DeltaSeconds)
{
    UpdateLight(DeltaSeconds);
}

void DirectionalLightComponent::EditorOnPropertyChanged(const std::string_view Name)
{
    if (IsDirectionalLightSettingsField(Name))
    {
        UpdateLight(0.0f);
    }
}
#endif

void DirectionalLightComponent::UpdateLight(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    EnsureLightRegistered();
    ApplyLightSettings();
}

RendererSystem* DirectionalLightComponent::ResolveRendererSystem() const
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

void DirectionalLightComponent::EnsureLightRegistered()
{
    if (!m_settings.Enabled)
    {
        ReleaseLight();
        return;
    }

    if (m_light.Valid())
    {
        return;
    }

    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return;
    }

    auto Desc = BuildDirectionalLightDesc(m_settings);
    (void)Renderer->CreateDirectionalRenderLight(Desc, m_light, Desc.DebugName);
}

void DirectionalLightComponent::ApplyLightSettings()
{
    if (!m_light.Valid())
    {
        return;
    }

    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return;
    }

    auto Desc = BuildDirectionalLightDesc(m_settings);
    if (!Renderer->SetDirectionalRenderLight(m_light, Desc))
    {
        m_light.Reset();
    }
}

void DirectionalLightComponent::ReleaseLight()
{
    if (!m_light.Valid())
    {
        return;
    }

    auto* Renderer = ResolveRendererSystem();
    if (Renderer && Renderer->IsInitialized())
    {
        (void)Renderer->DestroyRenderLight(m_light);
        return;
    }

    m_light.Reset();
}

} // namespace SnAPI::GameFramework
