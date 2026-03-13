#include "AtmosphereCompositeParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <limits>
#include <vector>

#include "IWorld.h"
#include "RendererSystem.h"

#include <AtmosphereCompositePass.hpp>
#include <IGraphicsAPI.hpp>
#include <VulkanGraphicsAPI.hpp>

namespace SnAPI::GameFramework
{
namespace
{
float ClampUnit(const float Value)
{
    return std::clamp(Value, 0.0f, 1.0f);
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

void ResetAtmosphereCompositePasses(RendererSystem& Renderer, const std::int64_t ViewportID)
{
    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        return;
    }

    const auto DefaultParams = SnAPI::Graphics::AtmosphereCompositePass::ParamBlock{};
    const auto TargetViewports = ResolveTargetViewports(*Graphics, ViewportID);
    for (const auto ViewportId : TargetViewports)
    {
        for (std::uint32_t Index = 0;; ++Index)
        {
            auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::Composite, Index);
            if (!Pass)
            {
                break;
            }

            auto* AtmosComposite = dynamic_cast<SnAPI::Graphics::AtmosphereCompositePass*>(Pass);
            if (!AtmosComposite)
            {
                continue;
            }

            AtmosComposite->SetParams(DefaultParams);
        }
    }
}
} // namespace

AtmosphereCompositeParamsNode::AtmosphereCompositeParamsNode()
{
    TypeKey(StaticTypeId<AtmosphereCompositeParamsNode>());
}

AtmosphereCompositeParamsNode::AtmosphereCompositeParamsNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<AtmosphereCompositeParamsNode>());
}

std::int64_t& AtmosphereCompositeParamsNode::EditViewportID() { return m_viewportID; }
const std::int64_t& AtmosphereCompositeParamsNode::GetViewportID() const { return m_viewportID; }

float& AtmosphereCompositeParamsNode::EditDepthThreshold() { return m_depthThreshold; }
const float& AtmosphereCompositeParamsNode::GetDepthThreshold() const { return m_depthThreshold; }

float& AtmosphereCompositeParamsNode::EditBlendWhenGeometry() { return m_blendWhenGeometry; }
const float& AtmosphereCompositeParamsNode::GetBlendWhenGeometry() const { return m_blendWhenGeometry; }

float& AtmosphereCompositeParamsNode::EditBlendWhenSky() { return m_blendWhenSky; }
const float& AtmosphereCompositeParamsNode::GetBlendWhenSky() const { return m_blendWhenSky; }

void AtmosphereCompositeParamsNode::OnCreate()
{
    m_applyPending = true;
    ApplyIfNeeded();
}

void AtmosphereCompositeParamsNode::OnDestroy()
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

    ResetAtmosphereCompositePasses(Renderer, m_viewportID);
    InvalidatePassCache();
}

void AtmosphereCompositeParamsNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void AtmosphereCompositeParamsNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

void AtmosphereCompositeParamsNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_applyPending = true;
    ApplyIfNeeded();
}
#endif

void AtmosphereCompositeParamsNode::ApplyIfNeeded()
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

bool AtmosphereCompositeParamsNode::ApplyToPass()
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
        for (std::uint32_t Index = 0;; ++Index)
        {
            auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::Composite, Index);
            if (!Pass)
            {
                break;
            }

            auto* AtmosComposite = dynamic_cast<SnAPI::Graphics::AtmosphereCompositePass*>(Pass);
            if (!AtmosComposite)
            {
                continue;
            }

            AtmosComposite->SetDepthThreshold(ClampUnit(m_depthThreshold));
            AtmosComposite->SetBlendWhenGeometry(ClampUnit(m_blendWhenGeometry));
            AtmosComposite->SetBlendWhenSky(ClampUnit(m_blendWhenSky));
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

void AtmosphereCompositeParamsNode::InvalidatePassCache()
{
    m_applyPending = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
