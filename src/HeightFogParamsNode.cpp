#include "HeightFogParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "IWorld.h"
#include "RendererSystem.h"

#include <HeightFogPass.hpp>
#include <ICamera.hpp>
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

void ResetHeightFogPasses(RendererSystem& Renderer, const std::int64_t ViewportID)
{
    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        return;
    }

    const auto DefaultParams = SnAPI::Graphics::HeightFogContract::DefaultParams();
    const auto TargetViewports = ResolveTargetViewports(*Graphics, ViewportID);
    for (const auto ViewportId : TargetViewports)
    {
        for (std::uint32_t Index = 0;; ++Index)
        {
            auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::FullScreen, Index);
            if (!Pass)
            {
                break;
            }

            auto* HeightFogPass = dynamic_cast<SnAPI::Graphics::HeightFogPass*>(Pass);
            if (!HeightFogPass)
            {
                continue;
            }

            HeightFogPass->SetParams(DefaultParams);
        }
    }
}
} // namespace

HeightFogParamsNode::HeightFogParamsNode()
{
    TypeKey(StaticTypeId<HeightFogParamsNode>());
}

HeightFogParamsNode::HeightFogParamsNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<HeightFogParamsNode>());
}

std::int64_t& HeightFogParamsNode::EditViewportID() { return m_viewportID; }
const std::int64_t& HeightFogParamsNode::GetViewportID() const { return m_viewportID; }

float& HeightFogParamsNode::EditDensity() { return m_density; }
const float& HeightFogParamsNode::GetDensity() const { return m_density; }

float& HeightFogParamsNode::EditHeightFalloff() { return m_heightFalloff; }
const float& HeightFogParamsNode::GetHeightFalloff() const { return m_heightFalloff; }

bool& HeightFogParamsNode::EditUseAbsoluteHeight() { return m_useAbsoluteHeight; }
const bool& HeightFogParamsNode::GetUseAbsoluteHeight() const { return m_useAbsoluteHeight; }

double& HeightFogParamsNode::EditHeightOffsetAbsoluteY() { return m_heightOffsetAbsoluteY; }
const double& HeightFogParamsNode::GetHeightOffsetAbsoluteY() const { return m_heightOffsetAbsoluteY; }

bool& HeightFogParamsNode::EditUseActiveCameraYAsRebaseOrigin() { return m_useActiveCameraYAsRebaseOrigin; }
const bool& HeightFogParamsNode::GetUseActiveCameraYAsRebaseOrigin() const { return m_useActiveCameraYAsRebaseOrigin; }

double& HeightFogParamsNode::EditRebaseOriginAbsoluteY() { return m_rebaseOriginAbsoluteY; }
const double& HeightFogParamsNode::GetRebaseOriginAbsoluteY() const { return m_rebaseOriginAbsoluteY; }

float& HeightFogParamsNode::EditHeightOffsetRebased() { return m_heightOffsetRebased; }
const float& HeightFogParamsNode::GetHeightOffsetRebased() const { return m_heightOffsetRebased; }

float& HeightFogParamsNode::EditStartDistance() { return m_startDistance; }
const float& HeightFogParamsNode::GetStartDistance() const { return m_startDistance; }

Vec3& HeightFogParamsNode::EditFogColor() { return m_fogColor; }
const Vec3& HeightFogParamsNode::GetFogColor() const { return m_fogColor; }

Vec3& HeightFogParamsNode::EditHorizonColor() { return m_horizonColor; }
const Vec3& HeightFogParamsNode::GetHorizonColor() const { return m_horizonColor; }

Vec3& HeightFogParamsNode::EditZenithColor() { return m_zenithColor; }
const Vec3& HeightFogParamsNode::GetZenithColor() const { return m_zenithColor; }

float& HeightFogParamsNode::EditSkyBlendStartDistance() { return m_skyBlendStartDistance; }
const float& HeightFogParamsNode::GetSkyBlendStartDistance() const { return m_skyBlendStartDistance; }

float& HeightFogParamsNode::EditSkyBlendEndDistance() { return m_skyBlendEndDistance; }
const float& HeightFogParamsNode::GetSkyBlendEndDistance() const { return m_skyBlendEndDistance; }

float& HeightFogParamsNode::EditSkyBlendStrength() { return m_skyBlendStrength; }
const float& HeightFogParamsNode::GetSkyBlendStrength() const { return m_skyBlendStrength; }

float& HeightFogParamsNode::EditTauDitherAmplitude() { return m_tauDitherAmplitude; }
const float& HeightFogParamsNode::GetTauDitherAmplitude() const { return m_tauDitherAmplitude; }

Vec3& HeightFogParamsNode::EditSunDirection() { return m_sunDirection; }
const Vec3& HeightFogParamsNode::GetSunDirection() const { return m_sunDirection; }

float& HeightFogParamsNode::EditSunAnisotropyG() { return m_sunAnisotropyG; }
const float& HeightFogParamsNode::GetSunAnisotropyG() const { return m_sunAnisotropyG; }

Vec3& HeightFogParamsNode::EditSunColor() { return m_sunColor; }
const Vec3& HeightFogParamsNode::GetSunColor() const { return m_sunColor; }

float& HeightFogParamsNode::EditSunInscatterIntensity() { return m_sunInscatterIntensity; }
const float& HeightFogParamsNode::GetSunInscatterIntensity() const { return m_sunInscatterIntensity; }

void HeightFogParamsNode::OnCreate()
{
    m_applyPending = true;
    ApplyIfNeeded();
}

void HeightFogParamsNode::OnDestroy()
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

    ResetHeightFogPasses(Renderer, m_viewportID);
    InvalidatePassCache();
}

void HeightFogParamsNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void HeightFogParamsNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

void HeightFogParamsNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_applyPending = true;
    ApplyIfNeeded();
}
#endif

void HeightFogParamsNode::ApplyIfNeeded()
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

bool HeightFogParamsNode::ApplyToPass()
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

    const float BlendStart = ClampNonNegative(m_skyBlendStartDistance);
    const float BlendEnd = std::max(BlendStart + 1.0e-3f, m_skyBlendEndDistance);
    const Vec3 SunDirection = NormalizeOrFallback(
        m_sunDirection,
        Vec3{static_cast<Scalar>(0.0), static_cast<Scalar>(-1.0), static_cast<Scalar>(0.0)});

    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        InvalidatePassCache();
        return false;
    }

    bool AppliedAny = false;
    const auto TargetViewports = ResolveTargetViewports(*Graphics, m_viewportID);
    for (const auto ViewportId : TargetViewports)
    {
        for (std::uint32_t Index = 0;; ++Index)
        {
            auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::FullScreen, Index);
            if (!Pass)
            {
                break;
            }

            auto* HeightFogPass = dynamic_cast<SnAPI::Graphics::HeightFogPass*>(Pass);
            if (!HeightFogPass)
            {
                continue;
            }

            HeightFogPass->SetDensity(ClampNonNegative(m_density));
            HeightFogPass->SetHeightFalloff(ClampNonNegative(m_heightFalloff));
            HeightFogPass->SetStartDistance(ClampNonNegative(m_startDistance));

            if (m_useAbsoluteHeight)
            {
                double RebaseOrigin = m_rebaseOriginAbsoluteY;
                if (m_useActiveCameraYAsRebaseOrigin)
                {
                    if (const auto* ActiveCamera = Renderer.ActiveCamera())
                    {
                        RebaseOrigin = ActiveCamera->Position().y();
                    }
                }

                HeightFogPass->SetHeightOffsetAbsolute(m_heightOffsetAbsoluteY, RebaseOrigin);
            }
            else
            {
                HeightFogPass->SetHeightOffsetRebased(m_heightOffsetRebased);
            }

            HeightFogPass->SetSkyBlendRange(BlendStart, BlendEnd);
            HeightFogPass->SetSkyBlendStrength(ClampNonNegative(m_skyBlendStrength));
            HeightFogPass->SetTauDitherAmplitude(ClampNonNegative(m_tauDitherAmplitude));
            HeightFogPass->SetSunAnisotropy(m_sunAnisotropyG);
            HeightFogPass->SetSunIntensity(ClampNonNegative(m_sunInscatterIntensity));
            HeightFogPass->SetSunDirection(ToFloat(SunDirection.x()), ToFloat(SunDirection.y()), ToFloat(SunDirection.z()));
            HeightFogPass->SetFogColor(ToFloat(m_fogColor.x()), ToFloat(m_fogColor.y()), ToFloat(m_fogColor.z()));
            HeightFogPass->SetHorizonColor(ToFloat(m_horizonColor.x()), ToFloat(m_horizonColor.y()), ToFloat(m_horizonColor.z()));
            HeightFogPass->SetZenithColor(ToFloat(m_zenithColor.x()), ToFloat(m_zenithColor.y()), ToFloat(m_zenithColor.z()));
            HeightFogPass->SetSunColor(ToFloat(m_sunColor.x()), ToFloat(m_sunColor.y()), ToFloat(m_sunColor.z()));
            AppliedAny = true;
        }
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

void HeightFogParamsNode::InvalidatePassCache()
{
    m_applyPending = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
