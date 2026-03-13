#include "SSAOParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <limits>
#include <vector>

#include "IWorld.h"
#include "RendererSystem.h"

#include <IGraphicsAPI.hpp>
#include <SSAOPass.hpp>
#include <VulkanGraphicsAPI.hpp>

namespace SnAPI::GameFramework
{
namespace
{
float ClampNonNegative(const float Value)
{
    return std::max(0.0f, Value);
}

std::uint32_t ClampMinOne(const std::uint32_t Value)
{
    return std::max<std::uint32_t>(1u, Value);
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

void ResetSSAOPasses(RendererSystem& Renderer, const std::int64_t ViewportID)
{
    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        return;
    }

    const auto TargetViewports = ResolveTargetViewports(*Graphics, ViewportID);
    for (const auto ViewportId : TargetViewports)
    {
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::SSAO);
        auto* SSAO = dynamic_cast<SnAPI::Graphics::SSAOPass*>(Pass);
        if (!SSAO)
        {
            continue;
        }

        SSAO->SetParams(SnAPI::Graphics::SSAOContract::DefaultParams());
        SSAO->SetTemporalParams(SnAPI::Graphics::SSAOTemporalContract::DefaultParams());
        SSAO->SetDenoiseBlurBeta(SnAPI::Graphics::SSAODenoiseContract::DefaultPushConstants().DenoiseBlurBeta);
    }
}
} // namespace

SSAOParamsNode::SSAOParamsNode()
{
    TypeKey(StaticTypeId<SSAOParamsNode>());
}

SSAOParamsNode::SSAOParamsNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<SSAOParamsNode>());
}

std::int64_t& SSAOParamsNode::EditViewportID() { return m_viewportID; }
const std::int64_t& SSAOParamsNode::GetViewportID() const { return m_viewportID; }

float& SSAOParamsNode::EditRadius() { return m_radius; }
const float& SSAOParamsNode::GetRadius() const { return m_radius; }

float& SSAOParamsNode::EditBias() { return m_bias; }
const float& SSAOParamsNode::GetBias() const { return m_bias; }

float& SSAOParamsNode::EditIntensity() { return m_intensity; }
const float& SSAOParamsNode::GetIntensity() const { return m_intensity; }

float& SSAOParamsNode::EditMaxDistance() { return m_maxDistance; }
const float& SSAOParamsNode::GetMaxDistance() const { return m_maxDistance; }

std::uint32_t& SSAOParamsNode::EditSliceCount() { return m_sliceCount; }
const std::uint32_t& SSAOParamsNode::GetSliceCount() const { return m_sliceCount; }

std::uint32_t& SSAOParamsNode::EditStepsPerSlice() { return m_stepsPerSlice; }
const std::uint32_t& SSAOParamsNode::GetStepsPerSlice() const { return m_stepsPerSlice; }

float& SSAOParamsNode::EditFalloffStart() { return m_falloffStart; }
const float& SSAOParamsNode::GetFalloffStart() const { return m_falloffStart; }

float& SSAOParamsNode::EditFalloffEnd() { return m_falloffEnd; }
const float& SSAOParamsNode::GetFalloffEnd() const { return m_falloffEnd; }

float& SSAOParamsNode::EditMaxPixelRadius() { return m_maxPixelRadius; }
const float& SSAOParamsNode::GetMaxPixelRadius() const { return m_maxPixelRadius; }

float& SSAOParamsNode::EditThickness() { return m_thickness; }
const float& SSAOParamsNode::GetThickness() const { return m_thickness; }

float& SSAOParamsNode::EditDenoiseBlurBeta() { return m_denoiseBlurBeta; }
const float& SSAOParamsNode::GetDenoiseBlurBeta() const { return m_denoiseBlurBeta; }

float& SSAOParamsNode::EditTemporalBlendFactor() { return m_temporalBlendFactor; }
const float& SSAOParamsNode::GetTemporalBlendFactor() const { return m_temporalBlendFactor; }

float& SSAOParamsNode::EditDisocclusionThreshold() { return m_disocclusionThreshold; }
const float& SSAOParamsNode::GetDisocclusionThreshold() const { return m_disocclusionThreshold; }

float& SSAOParamsNode::EditVelocityWeight() { return m_velocityWeight; }
const float& SSAOParamsNode::GetVelocityWeight() const { return m_velocityWeight; }

void SSAOParamsNode::OnCreate()
{
    m_applyPending = true;
    ApplyIfNeeded();
}

void SSAOParamsNode::OnDestroy()
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

    ResetSSAOPasses(Renderer, m_viewportID);
    InvalidatePassCache();
}

void SSAOParamsNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void SSAOParamsNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

void SSAOParamsNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_applyPending = true;
    ApplyIfNeeded();
}
#endif

void SSAOParamsNode::ApplyIfNeeded()
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

bool SSAOParamsNode::ApplyToPass()
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

    const float FalloffStart = ClampNonNegative(m_falloffStart);
    const float FalloffEnd = std::max(FalloffStart + 1.0e-3f, m_falloffEnd);

    bool AppliedAny = false;
    const auto TargetViewports = ResolveTargetViewports(*Graphics, m_viewportID);
    for (const auto ViewportId : TargetViewports)
    {
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::SSAO);
        auto* SSAO = dynamic_cast<SnAPI::Graphics::SSAOPass*>(Pass);
        if (!SSAO)
        {
            continue;
        }

        SSAO->SetRadius(ClampNonNegative(m_radius));
        SSAO->SetBias(ClampNonNegative(m_bias));
        SSAO->SetIntensity(ClampNonNegative(m_intensity));
        SSAO->SetMaxDistance(ClampNonNegative(m_maxDistance));
        SSAO->SetSliceCount(ClampMinOne(m_sliceCount));
        SSAO->SetStepsPerSlice(ClampMinOne(m_stepsPerSlice));
        SSAO->SetFalloff(FalloffStart, FalloffEnd);
        SSAO->SetMaxPixelRadius(ClampNonNegative(m_maxPixelRadius));
        SSAO->SetThickness(ClampNonNegative(m_thickness));
        SSAO->SetDenoiseBlurBeta(ClampNonNegative(m_denoiseBlurBeta));
        SSAO->SetTemporalBlendFactor(ClampNonNegative(m_temporalBlendFactor));
        SSAO->SetDisocclusionThreshold(ClampNonNegative(m_disocclusionThreshold));
        SSAO->SetVelocityWeight(ClampNonNegative(m_velocityWeight));
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

void SSAOParamsNode::InvalidatePassCache()
{
    m_applyPending = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
