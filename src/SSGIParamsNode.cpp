#include "SSGIParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <limits>
#include <vector>

#include "IWorld.h"
#include "RendererSystem.h"

#include <IGraphicsAPI.hpp>
#include <SSGIPass.hpp>
#include <VulkanGraphicsAPI.hpp>

namespace SnAPI::GameFramework
{
namespace
{
float ClampNonNegative(const float Value)
{
    return std::max(0.0f, Value);
}

float ClampMinFloat(const float Value, const float Minimum)
{
    return std::max(Minimum, Value);
}

float ClampUnit(const float Value)
{
    return std::clamp(Value, 0.0f, 1.0f);
}

std::uint32_t ClampMinOne(const std::uint32_t Value)
{
    return std::max<std::uint32_t>(1u, Value);
}

std::uint32_t ClampDebugMode(const std::uint32_t Value)
{
    return std::min<std::uint32_t>(Value, 5u);
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

void ResetSSGIPasses(RendererSystem& Renderer, const std::int64_t ViewportID)
{
    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        return;
    }

    const auto TargetViewports = ResolveTargetViewports(*Graphics, ViewportID);
    for (const auto ViewportId : TargetViewports)
    {
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::SSGI);
        auto* SSGI = dynamic_cast<SnAPI::Graphics::SSGIPass*>(Pass);
        if (!SSGI)
        {
            continue;
        }

        SSGI->SetParams(SnAPI::Graphics::SSGIContract::DefaultParams());
        SSGI->SetTemporalParams(SnAPI::Graphics::SSGITemporalContract::DefaultParams());
    }
}
} // namespace

SSGIParamsNode::SSGIParamsNode()
{
    TypeKey(StaticTypeId<SSGIParamsNode>());
}

SSGIParamsNode::SSGIParamsNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<SSGIParamsNode>());
}

std::int64_t& SSGIParamsNode::EditViewportID() { return m_viewportID; }
const std::int64_t& SSGIParamsNode::GetViewportID() const { return m_viewportID; }

float& SSGIParamsNode::EditIntensity() { return m_intensity; }
const float& SSGIParamsNode::GetIntensity() const { return m_intensity; }

float& SSGIParamsNode::EditMaxDistance() { return m_maxDistance; }
const float& SSGIParamsNode::GetMaxDistance() const { return m_maxDistance; }

float& SSGIParamsNode::EditThickness() { return m_thickness; }
const float& SSGIParamsNode::GetThickness() const { return m_thickness; }

float& SSGIParamsNode::EditSurfaceBias() { return m_surfaceBias; }
const float& SSGIParamsNode::GetSurfaceBias() const { return m_surfaceBias; }

std::uint32_t& SSGIParamsNode::EditMaxSteps() { return m_maxSteps; }
const std::uint32_t& SSGIParamsNode::GetMaxSteps() const { return m_maxSteps; }

std::uint32_t& SSGIParamsNode::EditRayCount() { return m_rayCount; }
const std::uint32_t& SSGIParamsNode::GetRayCount() const { return m_rayCount; }

float& SSGIParamsNode::EditDepthSigma() { return m_depthSigma; }
const float& SSGIParamsNode::GetDepthSigma() const { return m_depthSigma; }

float& SSGIParamsNode::EditNormalSigma() { return m_normalSigma; }
const float& SSGIParamsNode::GetNormalSigma() const { return m_normalSigma; }

float& SSGIParamsNode::EditRadianceClamp() { return m_radianceClamp; }
const float& SSGIParamsNode::GetRadianceClamp() const { return m_radianceClamp; }

float& SSGIParamsNode::EditMaxPixelRadius() { return m_maxPixelRadius; }
const float& SSGIParamsNode::GetMaxPixelRadius() const { return m_maxPixelRadius; }

float& SSGIParamsNode::EditStepExponent() { return m_stepExponent; }
const float& SSGIParamsNode::GetStepExponent() const { return m_stepExponent; }

float& SSGIParamsNode::EditTemporalBlendFactor() { return m_temporalBlendFactor; }
const float& SSGIParamsNode::GetTemporalBlendFactor() const { return m_temporalBlendFactor; }

float& SSGIParamsNode::EditDisocclusionThreshold() { return m_disocclusionThreshold; }
const float& SSGIParamsNode::GetDisocclusionThreshold() const { return m_disocclusionThreshold; }

float& SSGIParamsNode::EditClampStrength() { return m_clampStrength; }
const float& SSGIParamsNode::GetClampStrength() const { return m_clampStrength; }

float& SSGIParamsNode::EditVelocityWeight() { return m_velocityWeight; }
const float& SSGIParamsNode::GetVelocityWeight() const { return m_velocityWeight; }

float& SSGIParamsNode::EditLowLumaBoost() { return m_lowLumaBoost; }
const float& SSGIParamsNode::GetLowLumaBoost() const { return m_lowLumaBoost; }

std::uint32_t& SSGIParamsNode::EditTemporalDebugMode() { return m_temporalDebugMode; }
const std::uint32_t& SSGIParamsNode::GetTemporalDebugMode() const { return m_temporalDebugMode; }

void SSGIParamsNode::OnCreate()
{
    m_applyPending = true;
    ApplyIfNeeded();
}

void SSGIParamsNode::OnDestroy()
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

    ResetSSGIPasses(Renderer, m_viewportID);
    InvalidatePassCache();
}

void SSGIParamsNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void SSGIParamsNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

void SSGIParamsNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_applyPending = true;
    ApplyIfNeeded();
}
#endif

void SSGIParamsNode::ApplyIfNeeded()
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

bool SSGIParamsNode::ApplyToPass()
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
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::SSGI);
        auto* SSGI = dynamic_cast<SnAPI::Graphics::SSGIPass*>(Pass);
        if (!SSGI)
        {
            continue;
        }

        SSGI->SetIntensity(ClampNonNegative(m_intensity));
        SSGI->SetMaxDistance(ClampNonNegative(m_maxDistance));
        SSGI->SetThickness(ClampNonNegative(m_thickness));
        SSGI->SetSurfaceBias(ClampNonNegative(m_surfaceBias));
        SSGI->SetMaxSteps(ClampMinOne(m_maxSteps));
        SSGI->SetRayCount(ClampMinOne(m_rayCount));
        SSGI->SetDepthSigma(ClampNonNegative(m_depthSigma));
        SSGI->SetNormalSigma(ClampNonNegative(m_normalSigma));
        SSGI->SetRadianceClamp(ClampNonNegative(m_radianceClamp));
        SSGI->SetMaxPixelRadius(ClampMinFloat(m_maxPixelRadius, 1.0f));
        SSGI->SetStepExponent(ClampMinFloat(m_stepExponent, 0.25f));
        SSGI->SetTemporalBlendFactor(ClampUnit(m_temporalBlendFactor));
        SSGI->SetDisocclusionThreshold(ClampNonNegative(m_disocclusionThreshold));
        SSGI->SetClampStrength(ClampNonNegative(m_clampStrength));
        SSGI->SetVelocityWeight(ClampNonNegative(m_velocityWeight));
        SSGI->SetLowLumaBoost(ClampNonNegative(m_lowLumaBoost));
        SSGI->SetTemporalDebugMode(ClampDebugMode(m_temporalDebugMode));
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

void SSGIParamsNode::InvalidatePassCache()
{
    m_applyPending = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
