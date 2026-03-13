#include "AtmosphereParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "IWorld.h"
#include "RendererSystem.h"

#include <AtmospherePass.hpp>
#include <IGraphicsAPI.hpp>
#include <VulkanGraphicsAPI.hpp>

namespace SnAPI::GameFramework
{
namespace
{
float ClampNonNegative(const float Value)
{
    return std::max(0.0f, Value);
}

float SanitizeFinitePositive(const float Value, const float Fallback)
{
    if (!std::isfinite(Value))
    {
        return Fallback;
    }
    return std::max(Value, 1.0f);
}

float ToFloat(const Scalar Value)
{
    return static_cast<float>(Value);
}

Vec3 NormalizeOrFallback(const Vec3& Value, const Vec3& Fallback)
{
    if (!std::isfinite(Value.x()) || !std::isfinite(Value.y()) || !std::isfinite(Value.z()))
    {
        return Fallback;
    }

    Vec3 Direction = Value;
    const Scalar LenSq = Direction.squaredNorm();
    if (LenSq <= static_cast<Scalar>(1.0e-12))
    {
        return Fallback;
    }

    Direction.normalize();
    return Direction;
}

std::uint64_t ViewportSelectionKey(const std::int64_t ViewportID)
{
    return (ViewportID >= 0)
        ? static_cast<std::uint64_t>(ViewportID)
        : std::numeric_limits<std::uint64_t>::max();
}

std::vector<SnAPI::Graphics::RenderViewportID> ResolveTargetViewports(SnAPI::Graphics::IGraphicsAPI& Graphics,
                                                                       const std::int64_t ViewportID)
{
    if (ViewportID >= 0)
    {
        return {static_cast<SnAPI::Graphics::RenderViewportID>(static_cast<std::uint64_t>(ViewportID))};
    }

    return Graphics.RenderViewportIDs();
}

void ResetAtmospherePasses(RendererSystem& Renderer, const std::int64_t ViewportID)
{
    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        return;
    }

    const auto DefaultParams = SnAPI::Graphics::AtmosphereContract::DefaultParams();
    const auto TargetViewports = ResolveTargetViewports(*Graphics, ViewportID);
    for (const auto ViewportId : TargetViewports)
    {
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::Atmosphere);
        auto* Atmosphere = dynamic_cast<SnAPI::Graphics::AtmospherePass*>(Pass);
        if (!Atmosphere)
        {
            continue;
        }

        Atmosphere->SetParams(DefaultParams);
        Atmosphere->SetFeatures(SnAPI::Graphics::AtmospherePass::Feature::None);
    }
}
} // namespace

AtmosphereParamsNode::AtmosphereParamsNode()
{
    TypeKey(StaticTypeId<AtmosphereParamsNode>());
}

AtmosphereParamsNode::AtmosphereParamsNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<AtmosphereParamsNode>());
}

std::int64_t& AtmosphereParamsNode::EditViewportID() { return m_viewportID; }
const std::int64_t& AtmosphereParamsNode::GetViewportID() const { return m_viewportID; }

bool& AtmosphereParamsNode::EditWorldMode() { return m_worldMode; }
const bool& AtmosphereParamsNode::GetWorldMode() const { return m_worldMode; }

Vec3& AtmosphereParamsNode::EditSunDirection() { return m_sunDirection; }
const Vec3& AtmosphereParamsNode::GetSunDirection() const { return m_sunDirection; }

Vec3& AtmosphereParamsNode::EditSunColor() { return m_sunColor; }
const Vec3& AtmosphereParamsNode::GetSunColor() const { return m_sunColor; }

float& AtmosphereParamsNode::EditExposure() { return m_exposure; }
const float& AtmosphereParamsNode::GetExposure() const { return m_exposure; }

float& AtmosphereParamsNode::EditSunIntensity() { return m_sunIntensity; }
const float& AtmosphereParamsNode::GetSunIntensity() const { return m_sunIntensity; }

Vec3& AtmosphereParamsNode::EditRayleighScattering() { return m_rayleighScattering; }
const Vec3& AtmosphereParamsNode::GetRayleighScattering() const { return m_rayleighScattering; }

float& AtmosphereParamsNode::EditRayleighScaleHeight() { return m_rayleighScaleHeight; }
const float& AtmosphereParamsNode::GetRayleighScaleHeight() const { return m_rayleighScaleHeight; }

Vec3& AtmosphereParamsNode::EditMieScattering() { return m_mieScattering; }
const Vec3& AtmosphereParamsNode::GetMieScattering() const { return m_mieScattering; }

float& AtmosphereParamsNode::EditMieScaleHeight() { return m_mieScaleHeight; }
const float& AtmosphereParamsNode::GetMieScaleHeight() const { return m_mieScaleHeight; }

Vec3& AtmosphereParamsNode::EditMieAbsorption() { return m_mieAbsorption; }
const Vec3& AtmosphereParamsNode::GetMieAbsorption() const { return m_mieAbsorption; }

float& AtmosphereParamsNode::EditMieAnisotropyG() { return m_mieAnisotropyG; }
const float& AtmosphereParamsNode::GetMieAnisotropyG() const { return m_mieAnisotropyG; }

float& AtmosphereParamsNode::EditPlanetRadiusMeters() { return m_planetRadiusMeters; }
const float& AtmosphereParamsNode::GetPlanetRadiusMeters() const { return m_planetRadiusMeters; }

float& AtmosphereParamsNode::EditAtmosphereRadiusMeters() { return m_atmosphereRadiusMeters; }
const float& AtmosphereParamsNode::GetAtmosphereRadiusMeters() const { return m_atmosphereRadiusMeters; }

float& AtmosphereParamsNode::EditCameraGroundOffsetMeters() { return m_cameraGroundOffsetMeters; }
const float& AtmosphereParamsNode::GetCameraGroundOffsetMeters() const { return m_cameraGroundOffsetMeters; }

float& AtmosphereParamsNode::EditMaxSunDistanceMeters() { return m_maxSunDistanceMeters; }
const float& AtmosphereParamsNode::GetMaxSunDistanceMeters() const { return m_maxSunDistanceMeters; }

std::uint32_t& AtmosphereParamsNode::EditViewSampleCount() { return m_viewSampleCount; }
const std::uint32_t& AtmosphereParamsNode::GetViewSampleCount() const { return m_viewSampleCount; }

std::uint32_t& AtmosphereParamsNode::EditSunSampleCount() { return m_sunSampleCount; }
const std::uint32_t& AtmosphereParamsNode::GetSunSampleCount() const { return m_sunSampleCount; }

float& AtmosphereParamsNode::EditMultiScatterStrength() { return m_multiScatterStrength; }
const float& AtmosphereParamsNode::GetMultiScatterStrength() const { return m_multiScatterStrength; }

void AtmosphereParamsNode::OnCreate()
{
    m_applyPending = true;
    ApplyIfNeeded();
}

void AtmosphereParamsNode::OnDestroy()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidatePassCache();
        return;
    }

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.IsInitialized())
    {
        InvalidatePassCache();
        return;
    }

    ResetAtmospherePasses(Renderer, m_viewportID);
    InvalidatePassCache();
}

void AtmosphereParamsNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void AtmosphereParamsNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

void AtmosphereParamsNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_applyPending = true;
    ApplyIfNeeded();
}
#endif

void AtmosphereParamsNode::ApplyIfNeeded()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidatePassCache();
        return;
    }

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.IsInitialized())
    {
        InvalidatePassCache();
        return;
    }

    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        InvalidatePassCache();
        return;
    }

    const std::uint64_t TargetViewportID = ViewportSelectionKey(m_viewportID);
    const std::uint64_t PassGraphRevision = Renderer.RenderViewportPassGraphRevision();

    if (!m_applyPending && m_lastAppliedViewportID == TargetViewportID
        && m_lastAppliedPassGraphRevision == PassGraphRevision)
    {
        return;
    }

    (void)ApplyToPass();
}

bool AtmosphereParamsNode::ApplyToPass()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidatePassCache();
        return false;
    }

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.IsInitialized())
    {
        InvalidatePassCache();
        return false;
    }

    const Vec3 SunDirection = NormalizeOrFallback(
        m_sunDirection,
        Vec3{static_cast<Scalar>(0.70710677), static_cast<Scalar>(0.70710677), static_cast<Scalar>(0.0)});
    Vec3 SunColor = m_sunColor.cwiseMax(static_cast<Scalar>(0.0));
    if (!std::isfinite(SunColor.x()) || !std::isfinite(SunColor.y()) || !std::isfinite(SunColor.z()))
    {
        SunColor = Vec3{static_cast<Scalar>(1.0), static_cast<Scalar>(1.0), static_cast<Scalar>(1.0)};
    }
    Vec3 RayleighScattering = m_rayleighScattering.cwiseMax(static_cast<Scalar>(0.0));
    if (!std::isfinite(RayleighScattering.x()) || !std::isfinite(RayleighScattering.y()) || !std::isfinite(RayleighScattering.z()))
    {
        RayleighScattering = Vec3{static_cast<Scalar>(5.8e-6), static_cast<Scalar>(13.5e-6), static_cast<Scalar>(33.1e-6)};
    }
    Vec3 MieScattering = m_mieScattering.cwiseMax(static_cast<Scalar>(0.0));
    if (!std::isfinite(MieScattering.x()) || !std::isfinite(MieScattering.y()) || !std::isfinite(MieScattering.z()))
    {
        MieScattering = Vec3{static_cast<Scalar>(21.0e-6), static_cast<Scalar>(21.0e-6), static_cast<Scalar>(21.0e-6)};
    }

    Vec3 MieAbsorption = m_mieAbsorption.cwiseMax(static_cast<Scalar>(0.0));
    if (!std::isfinite(MieAbsorption.x()) || !std::isfinite(MieAbsorption.y()) || !std::isfinite(MieAbsorption.z()))
    {
        MieAbsorption = Vec3{static_cast<Scalar>(0.0), static_cast<Scalar>(0.0), static_cast<Scalar>(0.0)};
    }

    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        InvalidatePassCache();
        return false;
    }

    const float PlanetRadius = SanitizeFinitePositive(m_planetRadiusMeters, 6360.0e3f);
    const float AtmosphereRadius = std::max(SanitizeFinitePositive(m_atmosphereRadiusMeters, 6420.0e3f), PlanetRadius + 1.0f);

    bool AppliedAny = false;
    const auto TargetViewports = ResolveTargetViewports(*Graphics, m_viewportID);
    for (const auto ViewportId : TargetViewports)
    {
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::Atmosphere);
        auto* Atmosphere = dynamic_cast<SnAPI::Graphics::AtmospherePass*>(Pass);
        if (!Atmosphere)
        {
            continue;
        }

        Atmosphere->SetFeature(SnAPI::Graphics::AtmospherePass::Feature::World, m_worldMode);
        Atmosphere->SetSunDirection(ToFloat(SunDirection.x()), ToFloat(SunDirection.y()), ToFloat(SunDirection.z()));
        Atmosphere->SetSunColor(ToFloat(SunColor.x()), ToFloat(SunColor.y()), ToFloat(SunColor.z()));
        Atmosphere->SetExposure(ClampNonNegative(m_exposure));
        Atmosphere->SetSunIntensity(ClampNonNegative(m_sunIntensity));
        Atmosphere->SetRayleighScattering(
            ToFloat(RayleighScattering.x()),
            ToFloat(RayleighScattering.y()),
            ToFloat(RayleighScattering.z()));
        Atmosphere->SetRayleighScaleHeight(std::max(m_rayleighScaleHeight, 1.0f));
        Atmosphere->SetMieScattering(
            ToFloat(MieScattering.x()),
            ToFloat(MieScattering.y()),
            ToFloat(MieScattering.z()));
        Atmosphere->SetMieScaleHeight(std::max(m_mieScaleHeight, 1.0f));
        Atmosphere->SetMieAbsorption(
            ToFloat(MieAbsorption.x()),
            ToFloat(MieAbsorption.y()),
            ToFloat(MieAbsorption.z()));
        Atmosphere->SetMieAnisotropy(std::clamp(m_mieAnisotropyG, -0.999f, 0.999f));
        Atmosphere->SetPlanetRadius(PlanetRadius);
        Atmosphere->SetAtmosphereRadius(AtmosphereRadius);
        Atmosphere->SetCameraGroundOffset(ClampNonNegative(std::isfinite(m_cameraGroundOffsetMeters) ? m_cameraGroundOffsetMeters : 100.0f));
        Atmosphere->SetMaxSunDistance(SanitizeFinitePositive(m_maxSunDistanceMeters, 120.0e3f));
        Atmosphere->SetViewSampleCount(std::max<std::uint32_t>(1u, m_viewSampleCount));
        Atmosphere->SetSunSampleCount(std::max<std::uint32_t>(1u, m_sunSampleCount));
        Atmosphere->SetMultiScatterStrength(ClampNonNegative(m_multiScatterStrength));
        AppliedAny = true;
    }

    if (!AppliedAny)
    {
        return false;
    }

    m_applyPending = false;
    m_lastAppliedPassGraphRevision = Renderer.RenderViewportPassGraphRevision();
    m_lastAppliedViewportID = ViewportSelectionKey(m_viewportID);
    return true;
}

void AtmosphereParamsNode::InvalidatePassCache()
{
    m_applyPending = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
