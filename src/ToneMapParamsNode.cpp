#include "ToneMapParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <limits>
#include <vector>

#include "IWorld.h"
#include "RendererSystem.h"

#include <IGraphicsAPI.hpp>
#include <ToneMapPass.hpp>
#include <VulkanGraphicsAPI.hpp>

namespace SnAPI::GameFramework
{
namespace
{
float ClampNonNegative(const float Value)
{
    return std::max(0.0f, Value);
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
} // namespace

ToneMapParamsNode::ToneMapParamsNode()
{
    TypeKey(StaticTypeId<ToneMapParamsNode>());
}

ToneMapParamsNode::ToneMapParamsNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<ToneMapParamsNode>());
}

std::int64_t& ToneMapParamsNode::EditViewportID() { return m_viewportID; }
const std::int64_t& ToneMapParamsNode::GetViewportID() const { return m_viewportID; }

float& ToneMapParamsNode::EditExposure() { return m_exposure; }
const float& ToneMapParamsNode::GetExposure() const { return m_exposure; }

float& ToneMapParamsNode::EditGamma() { return m_gamma; }
const float& ToneMapParamsNode::GetGamma() const { return m_gamma; }

float& ToneMapParamsNode::EditDitherStrength() { return m_ditherStrength; }
const float& ToneMapParamsNode::GetDitherStrength() const { return m_ditherStrength; }

float& ToneMapParamsNode::EditAgXExposureBiasStops() { return m_agXExposureBiasStops; }
const float& ToneMapParamsNode::GetAgXExposureBiasStops() const { return m_agXExposureBiasStops; }

float& ToneMapParamsNode::EditAgXSaturation() { return m_agXSaturation; }
const float& ToneMapParamsNode::GetAgXSaturation() const { return m_agXSaturation; }

float& ToneMapParamsNode::EditAgXContrast() { return m_agXContrast; }
const float& ToneMapParamsNode::GetAgXContrast() const { return m_agXContrast; }

float& ToneMapParamsNode::EditAgXPivot() { return m_agXPivot; }
const float& ToneMapParamsNode::GetAgXPivot() const { return m_agXPivot; }

float& ToneMapParamsNode::EditAgXGamutThreshold() { return m_agXGamutThreshold; }
const float& ToneMapParamsNode::GetAgXGamutThreshold() const { return m_agXGamutThreshold; }

float& ToneMapParamsNode::EditAgXGamutKnee() { return m_agXGamutKnee; }
const float& ToneMapParamsNode::GetAgXGamutKnee() const { return m_agXGamutKnee; }

float& ToneMapParamsNode::EditAcesSaturation() { return m_acesSaturation; }
const float& ToneMapParamsNode::GetAcesSaturation() const { return m_acesSaturation; }

float& ToneMapParamsNode::EditAcesWhitePoint() { return m_acesWhitePoint; }
const float& ToneMapParamsNode::GetAcesWhitePoint() const { return m_acesWhitePoint; }

bool& ToneMapParamsNode::EditEnableACES() { return m_enableACES; }
const bool& ToneMapParamsNode::GetEnableACES() const { return m_enableACES; }

bool& ToneMapParamsNode::EditEnableAgX() { return m_enableAgX; }
const bool& ToneMapParamsNode::GetEnableAgX() const { return m_enableAgX; }

bool& ToneMapParamsNode::EditEnableCompare() { return m_enableCompare; }
const bool& ToneMapParamsNode::GetEnableCompare() const { return m_enableCompare; }

void ToneMapParamsNode::OnCreate()
{
    m_applyPending = true;
    ApplyIfNeeded();
}

void ToneMapParamsNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void ToneMapParamsNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

void ToneMapParamsNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_applyPending = true;
    ApplyIfNeeded();
}
#endif

void ToneMapParamsNode::ApplyIfNeeded()
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

bool ToneMapParamsNode::ApplyToPass()
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

    std::uint32_t FeatureMask = 0u;
    if (m_enableACES)
    {
        FeatureMask |= static_cast<uint32_t>(SnAPI::Graphics::ToneMapPass::Feature::ACES);
    }
    if (m_enableAgX)
    {
        FeatureMask |= static_cast<uint32_t>(SnAPI::Graphics::ToneMapPass::Feature::AgX);
    }
    if (m_enableCompare)
    {
        FeatureMask |= static_cast<uint32_t>(SnAPI::Graphics::ToneMapPass::Feature::Compare);
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
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::ToneMap);
        auto* ToneMap = dynamic_cast<SnAPI::Graphics::ToneMapPass*>(Pass);
        if (!ToneMap)
        {
            continue;
        }

        ToneMap->SetExposure(ClampNonNegative(m_exposure));
        ToneMap->SetGamma(std::max(m_gamma, 0.001f));
        ToneMap->SetDitherStrength(ClampNonNegative(m_ditherStrength));
        ToneMap->SetAgXExposureBiasStops(m_agXExposureBiasStops);
        ToneMap->SetAgXSaturation(ClampNonNegative(m_agXSaturation));
        ToneMap->SetAgXContrast(ClampNonNegative(m_agXContrast));
        ToneMap->SetAgXPivot(std::clamp(m_agXPivot, 0.0f, 1.0f));
        ToneMap->SetAgXGamutThreshold(std::clamp(m_agXGamutThreshold, 0.0f, 2.0f));
        ToneMap->SetAgXGamutKnee(ClampNonNegative(m_agXGamutKnee));
        ToneMap->SetAcesSaturation(ClampNonNegative(m_acesSaturation));
        ToneMap->SetAcesWhitePoint(ClampNonNegative(m_acesWhitePoint));
        ToneMap->SetFeatures(static_cast<SnAPI::Graphics::ToneMapPass::Feature>(FeatureMask));
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

void ToneMapParamsNode::InvalidatePassCache()
{
    m_applyPending = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
