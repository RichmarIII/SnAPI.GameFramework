#include "SSRParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <limits>
#include <vector>

#include "IWorld.h"
#include "RendererSystem.h"

#include <IGraphicsAPI.hpp>
#include <SSRPass.hpp>
#include <VulkanGraphicsAPI.hpp>

namespace SnAPI::GameFramework
{
namespace
{
float ClampNonNegative(const float Value)
{
    return std::max(0.0f, Value);
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
    return std::min<std::uint32_t>(Value, 12u);
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

SSRParamsNode::SSRParamsNode()
{
    TypeKey(StaticTypeId<SSRParamsNode>());
}

SSRParamsNode::SSRParamsNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<SSRParamsNode>());
}

std::int64_t& SSRParamsNode::EditViewportID() { return m_viewportID; }
const std::int64_t& SSRParamsNode::GetViewportID() const { return m_viewportID; }

float& SSRParamsNode::EditMaxDistance() { return m_maxDistance; }
const float& SSRParamsNode::GetMaxDistance() const { return m_maxDistance; }

float& SSRParamsNode::EditThickness() { return m_thickness; }
const float& SSRParamsNode::GetThickness() const { return m_thickness; }

float& SSRParamsNode::EditMaxRoughness() { return m_maxRoughness; }
const float& SSRParamsNode::GetMaxRoughness() const { return m_maxRoughness; }

float& SSRParamsNode::EditRoughnessThreshold() { return m_roughnessThreshold; }
const float& SSRParamsNode::GetRoughnessThreshold() const { return m_roughnessThreshold; }

std::uint32_t& SSRParamsNode::EditMaxSteps() { return m_maxSteps; }
const std::uint32_t& SSRParamsNode::GetMaxSteps() const { return m_maxSteps; }

std::uint32_t& SSRParamsNode::EditMaxBinarySteps() { return m_maxBinarySteps; }
const std::uint32_t& SSRParamsNode::GetMaxBinarySteps() const { return m_maxBinarySteps; }

float& SSRParamsNode::EditScreenEdgeFade() { return m_screenEdgeFade; }
const float& SSRParamsNode::GetScreenEdgeFade() const { return m_screenEdgeFade; }

float& SSRParamsNode::EditReflectionFade() { return m_reflectionFade; }
const float& SSRParamsNode::GetReflectionFade() const { return m_reflectionFade; }

float& SSRParamsNode::EditTemporalBlendFactor() { return m_temporalBlendFactor; }
const float& SSRParamsNode::GetTemporalBlendFactor() const { return m_temporalBlendFactor; }

float& SSRParamsNode::EditDisocclusionThreshold() { return m_disocclusionThreshold; }
const float& SSRParamsNode::GetDisocclusionThreshold() const { return m_disocclusionThreshold; }

float& SSRParamsNode::EditClampStrength() { return m_clampStrength; }
const float& SSRParamsNode::GetClampStrength() const { return m_clampStrength; }

float& SSRParamsNode::EditVelocityWeight() { return m_velocityWeight; }
const float& SSRParamsNode::GetVelocityWeight() const { return m_velocityWeight; }

std::uint32_t& SSRParamsNode::EditTemporalDebugMode() { return m_temporalDebugMode; }
const std::uint32_t& SSRParamsNode::GetTemporalDebugMode() const { return m_temporalDebugMode; }

void SSRParamsNode::OnCreate()
{
    m_applyPending = true;
    ApplyIfNeeded();
}

void SSRParamsNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void SSRParamsNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

void SSRParamsNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_applyPending = true;
    ApplyIfNeeded();
}
#endif

void SSRParamsNode::ApplyIfNeeded()
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

bool SSRParamsNode::ApplyToPass()
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
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::SSR);
        auto* SSR = dynamic_cast<SnAPI::Graphics::SSRPass*>(Pass);
        if (!SSR)
        {
            continue;
        }

        SSR->SetMaxDistance(ClampNonNegative(m_maxDistance));
        SSR->SetThickness(ClampNonNegative(m_thickness));
        SSR->SetMaxRoughness(ClampNonNegative(m_maxRoughness));
        SSR->SetRoughnessThreshold(ClampNonNegative(m_roughnessThreshold));
        SSR->SetMaxSteps(ClampMinOne(m_maxSteps));
        SSR->SetMaxBinarySteps(ClampMinOne(m_maxBinarySteps));
        SSR->SetScreenEdgeFade(ClampNonNegative(m_screenEdgeFade));
        SSR->SetReflectionFade(ClampNonNegative(m_reflectionFade));
        SSR->SetTemporalBlendFactor(ClampUnit(m_temporalBlendFactor));
        SSR->SetDisocclusionThreshold(ClampNonNegative(m_disocclusionThreshold));
        SSR->SetClampStrength(ClampNonNegative(m_clampStrength));
        SSR->SetVelocityWeight(ClampNonNegative(m_velocityWeight));
        SSR->SetTemporalDebugMode(ClampDebugMode(m_temporalDebugMode));
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

void SSRParamsNode::InvalidatePassCache()
{
    m_applyPending = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
