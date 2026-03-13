#include "BloomParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <limits>
#include <vector>

#include "IWorld.h"
#include "RendererSystem.h"

#include <BloomPass.hpp>
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

void ResetBloomPasses(RendererSystem& Renderer, const std::int64_t ViewportID)
{
    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        return;
    }

    const auto DefaultParams = SnAPI::Graphics::BloomContract::DefaultParams();
    const auto TargetViewports = ResolveTargetViewports(*Graphics, ViewportID);
    for (const auto ViewportId : TargetViewports)
    {
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::Bloom);
        auto* Bloom = dynamic_cast<SnAPI::Graphics::BloomPass*>(Pass);
        if (!Bloom)
        {
            continue;
        }

        Bloom->SetParams(DefaultParams);
        Bloom->SetMipCount(5u);
    }
}
} // namespace

BloomParamsNode::BloomParamsNode()
{
    TypeKey(StaticTypeId<BloomParamsNode>());
}

BloomParamsNode::BloomParamsNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<BloomParamsNode>());
}

std::int64_t& BloomParamsNode::EditViewportID() { return m_viewportID; }
const std::int64_t& BloomParamsNode::GetViewportID() const { return m_viewportID; }

float& BloomParamsNode::EditThreshold() { return m_threshold; }
const float& BloomParamsNode::GetThreshold() const { return m_threshold; }

float& BloomParamsNode::EditKnee() { return m_knee; }
const float& BloomParamsNode::GetKnee() const { return m_knee; }

float& BloomParamsNode::EditIntensity() { return m_intensity; }
const float& BloomParamsNode::GetIntensity() const { return m_intensity; }

float& BloomParamsNode::EditScatter() { return m_scatter; }
const float& BloomParamsNode::GetScatter() const { return m_scatter; }

float& BloomParamsNode::EditClamp() { return m_clamp; }
const float& BloomParamsNode::GetClamp() const { return m_clamp; }

std::uint32_t& BloomParamsNode::EditMipCount() { return m_mipCount; }
const std::uint32_t& BloomParamsNode::GetMipCount() const { return m_mipCount; }

void BloomParamsNode::OnCreate()
{
    m_applyPending = true;
    ApplyIfNeeded();
}

void BloomParamsNode::OnDestroy()
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

    ResetBloomPasses(Renderer, m_viewportID);
    InvalidatePassCache();
}

void BloomParamsNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void BloomParamsNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

void BloomParamsNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_applyPending = true;
    ApplyIfNeeded();
}
#endif

void BloomParamsNode::ApplyIfNeeded()
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

bool BloomParamsNode::ApplyToPass()
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
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::Bloom);
        auto* Bloom = dynamic_cast<SnAPI::Graphics::BloomPass*>(Pass);
        if (!Bloom)
        {
            continue;
        }

        Bloom->SetThreshold(ClampNonNegative(m_threshold));
        Bloom->SetKnee(ClampNonNegative(m_knee));
        Bloom->SetIntensity(ClampNonNegative(m_intensity));
        Bloom->SetScatter(ClampNonNegative(m_scatter));
        Bloom->SetClamp(ClampNonNegative(m_clamp));
        Bloom->SetMipCount(ClampMinOne(m_mipCount));
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

void BloomParamsNode::InvalidatePassCache()
{
    m_applyPending = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
