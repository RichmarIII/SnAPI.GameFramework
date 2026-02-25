#include "DirectionalLightComponent.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "Profiling.h"

#include <algorithm>
#include <cmath>

#include <LightManager.hpp>
#include <TLightFor.hpp>

#include "BaseNode.h"
#include "IWorld.h"
#include "RendererSystem.h"

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

SnAPI::Vector3DF ToRendererVector3(const Vec3& Value)
{
    return SnAPI::Vector3DF{
        static_cast<float>(Value.x()),
        static_cast<float>(Value.y()),
        static_cast<float>(Value.z())};
}

SnAPI::Vector3DF NormalizeDirectionOrDefault(const Vec3& Value)
{
    if (!IsFiniteVec3(Value))
    {
        return SnAPI::Vector3DF(-0.5f, -1.0f, -0.3f).normalized();
    }

    SnAPI::Vector3DF Direction = ToRendererVector3(Value);
    if (Direction.squaredNorm() <= 0.000001f)
    {
        Direction = SnAPI::Vector3DF(-0.5f, -1.0f, -0.3f);
    }
    return Direction.normalized();
}

SnAPI::ColorF ToRendererColor(const Vec3& Value)
{
    const Vec3 Safe = IsFiniteVec3(Value) ? Value : Vec3{1.0, 1.0, 1.0};
    return SnAPI::ColorF(
        ClampNonNegative(static_cast<float>(Safe.x())),
        ClampNonNegative(static_cast<float>(Safe.y())),
        ClampNonNegative(static_cast<float>(Safe.z())),
        1.0f);
}
} // namespace

SnAPI::Graphics::DirectionalLight* DirectionalLightComponent::Light()
{
    return m_light.get();
}

const SnAPI::Graphics::DirectionalLight* DirectionalLightComponent::Light() const
{
    return m_light.get();
}

void DirectionalLightComponent::OnCreate()
{
    EnsureLightRegistered();
    ApplyLightSettings();
}

void DirectionalLightComponent::OnDestroy()
{
    ReleaseLight();
}

void DirectionalLightComponent::Tick(float DeltaSeconds)
{
    RuntimeTick(DeltaSeconds);
}

void DirectionalLightComponent::RuntimeTick(const float DeltaSeconds)
{
    UpdateLight(DeltaSeconds);
}

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

    auto* Renderer = ResolveRendererSystem();
    if (!Renderer || !Renderer->IsInitialized())
    {
        return;
    }

    auto* Manager = Renderer->EnsureLightManager();
    if (!Manager)
    {
        return;
    }

    if (!m_light)
    {
        m_light = Manager->CreateDirectionalLight();
        return;
    }

    const auto& RegisteredLights = Manager->GetAllLights();
    const bool AlreadyRegistered = std::any_of(
        RegisteredLights.begin(), RegisteredLights.end(), [this](const auto& Existing) {
            return Existing.get() == m_light.get();
        });
    if (!AlreadyRegistered)
    {
        Manager->AddLight(m_light);
    }
}

void DirectionalLightComponent::ApplyLightSettings()
{
    if (!m_light)
    {
        return;
    }

    m_light->SetDirection(NormalizeDirectionOrDefault(m_settings.Direction));
    m_light->SetColor(ToRendererColor(m_settings.Color));
    m_light->SetIntensity(ClampNonNegative(m_settings.Intensity));
    m_light->SetCastsShadows(m_settings.CastShadows);
    m_light->SetCascadeCount(m_settings.CascadeCount);
    m_light->SetShadowMapSize(std::max(1u, m_settings.ShadowMapSize));
    m_light->SetShadowBias(ClampNonNegative(m_settings.ShadowBias));
    m_light->SetShadowFarDistance(std::max(1.0f, m_settings.ShadowFarDistance));
    m_light->SetSoftnessFactor(ClampNonNegative(m_settings.SoftnessFactor));
    m_light->SetFeature(SnAPI::Graphics::DirectionalLightContract::Feature::SoftShadows, m_settings.SoftShadows);
    m_light->SetFeature(SnAPI::Graphics::DirectionalLightContract::Feature::ContactHardening, m_settings.ContactHardening);
    m_light->SetFeature(SnAPI::Graphics::DirectionalLightContract::Feature::CascadeBlending, m_settings.CascadeBlending);
}

void DirectionalLightComponent::ReleaseLight()
{
    if (!m_light)
    {
        return;
    }

    if (auto* Renderer = ResolveRendererSystem(); Renderer && Renderer->IsInitialized())
    {
        if (auto* Manager = Renderer->LightManager())
        {
            Manager->RemoveLight(m_light.get());
        }
    }

    m_light.reset();
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
