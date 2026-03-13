#include "TAAParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <limits>
#include <vector>

#include "IWorld.h"
#include "RendererSystem.h"

#include <IGraphicsAPI.hpp>
#include <TAAPass.hpp>
#include <VulkanGraphicsAPI.hpp>

namespace SnAPI::GameFramework
{
namespace
{
float ClampUnit(const float Value)
{
    return std::clamp(Value, 0.0f, 1.0f);
}

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

void ResetTAAPasses(RendererSystem& Renderer, const std::int64_t ViewportID)
{
    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        return;
    }

    const auto TargetViewports = ResolveTargetViewports(*Graphics, ViewportID);
    if (ViewportID < 0)
    {
        Renderer.SetDefaultTaaJitterScale(1.0f);
    }

    for (const auto ViewportId : TargetViewports)
    {
        if (ViewportID >= 0)
        {
            Renderer.SetViewportTaaJitterScale(static_cast<std::uint64_t>(ViewportId), 1.0f);
        }

        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::TAA);
        auto* TAA = dynamic_cast<SnAPI::Graphics::TAAPass*>(Pass);
        if (!TAA)
        {
            continue;
        }

        TAA->SetParams(SnAPI::Graphics::TAAContract::DefaultParams());
    }
}
} // namespace

TAAParamsNode::TAAParamsNode()
{
    TypeKey(StaticTypeId<TAAParamsNode>());
}

TAAParamsNode::TAAParamsNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<TAAParamsNode>());
}

std::int64_t& TAAParamsNode::EditViewportID() { return m_viewportID; }
const std::int64_t& TAAParamsNode::GetViewportID() const { return m_viewportID; }

float& TAAParamsNode::EditBlendFactor() { return m_blendFactor; }
const float& TAAParamsNode::GetBlendFactor() const { return m_blendFactor; }

float& TAAParamsNode::EditMotionBlendFactor() { return m_motionBlendFactor; }
const float& TAAParamsNode::GetMotionBlendFactor() const { return m_motionBlendFactor; }

float& TAAParamsNode::EditClampStrength() { return m_clampStrength; }
const float& TAAParamsNode::GetClampStrength() const { return m_clampStrength; }

float& TAAParamsNode::EditSharpen() { return m_sharpen; }
const float& TAAParamsNode::GetSharpen() const { return m_sharpen; }

float& TAAParamsNode::EditJitterScale() { return m_jitterScale; }
const float& TAAParamsNode::GetJitterScale() const { return m_jitterScale; }

void TAAParamsNode::OnCreate()
{
    m_applyPending = true;
    ApplyIfNeeded();
}

void TAAParamsNode::OnDestroy()
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

    ResetTAAPasses(Renderer, m_viewportID);
    InvalidatePassCache();
}

void TAAParamsNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void TAAParamsNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

void TAAParamsNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_applyPending = true;
    ApplyIfNeeded();
}
#endif

void TAAParamsNode::ApplyIfNeeded()
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

bool TAAParamsNode::ApplyToPass()
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
    const float JitterScale = ClampNonNegative(m_jitterScale);
    if (m_viewportID < 0)
    {
        Renderer.SetDefaultTaaJitterScale(JitterScale);
        AppliedAny = true;
    }

    const auto TargetViewports = ResolveTargetViewports(*Graphics, m_viewportID);
    for (const auto ViewportId : TargetViewports)
    {
        if (m_viewportID >= 0)
        {
            Renderer.SetViewportTaaJitterScale(static_cast<std::uint64_t>(ViewportId), JitterScale);
            AppliedAny = true;
        }

        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::TAA);
        auto* TAA = dynamic_cast<SnAPI::Graphics::TAAPass*>(Pass);
        if (!TAA)
        {
            continue;
        }

        TAA->SetBlendFactor(ClampUnit(m_blendFactor));
        TAA->SetMotionBlendFactor(ClampUnit(m_motionBlendFactor));
        TAA->SetClampStrength(ClampNonNegative(m_clampStrength));
        TAA->SetSharpen(ClampNonNegative(m_sharpen));
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

void TAAParamsNode::InvalidatePassCache()
{
    m_applyPending = true;
}
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
