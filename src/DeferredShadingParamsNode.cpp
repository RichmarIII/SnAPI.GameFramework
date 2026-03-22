#include "DeferredShadingParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <limits>
#include <vector>

#include "IWorld.h"
#include "RendererSystem.h"

#include <DeferredShadingPass.hpp>
#include <IGraphicsAPI.hpp>
#include <VulkanGraphicsAPI.hpp>

namespace SnAPI::GameFramework
{
namespace
{
std::uint64_t ViewportSelectionKey(const std::int64_t ViewportID)
{
    return (ViewportID >= 0)
        ? static_cast<std::uint64_t>(ViewportID)
        : std::numeric_limits<std::uint64_t>::max();
}

std::vector<SnAPI::Graphics::RenderViewportID> ResolveTargetViewports(SnAPI::Graphics::VulkanGraphicsAPI& Graphics,
                                                                      const std::int64_t ViewportID)
{
    if (ViewportID >= 0)
    {
        return {static_cast<SnAPI::Graphics::RenderViewportID>(static_cast<std::uint64_t>(ViewportID))};
    }

    return Graphics.RenderViewportIDs();
}

void ResetDeferredShadingPasses(RendererSystem& Renderer, const std::int64_t ViewportID)
{
    auto* Graphics = Renderer.Graphics();
    if (!Graphics)
    {
        return;
    }

    const auto TargetViewports = ResolveTargetViewports(*Graphics, ViewportID);
    for (const auto ViewportId : TargetViewports)
    {
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::DeferredShading);
        auto* Deferred = dynamic_cast<SnAPI::Graphics::DeferredShadingPass*>(Pass);
        if (!Deferred)
        {
            continue;
        }

        Deferred->SetFeatures(SnAPI::Graphics::DeferredContract::Feature::None);
    }
}
} // namespace

DeferredShadingParamsNode::DeferredShadingParamsNode()
{
    TypeKey(StaticTypeId<DeferredShadingParamsNode>());
}

DeferredShadingParamsNode::DeferredShadingParamsNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<DeferredShadingParamsNode>());
}

std::int64_t& DeferredShadingParamsNode::EditViewportID() { return m_viewportID; }
const std::int64_t& DeferredShadingParamsNode::GetViewportID() const { return m_viewportID; }

bool& DeferredShadingParamsNode::EditDebugMotionVectors() { return m_debugMotionVectors; }
const bool& DeferredShadingParamsNode::GetDebugMotionVectors() const { return m_debugMotionVectors; }

bool& DeferredShadingParamsNode::EditDebugNormals() { return m_debugNormals; }
const bool& DeferredShadingParamsNode::GetDebugNormals() const { return m_debugNormals; }

bool& DeferredShadingParamsNode::EditDebugAlbedo() { return m_debugAlbedo; }
const bool& DeferredShadingParamsNode::GetDebugAlbedo() const { return m_debugAlbedo; }

bool& DeferredShadingParamsNode::EditDebugAO() { return m_debugAO; }
const bool& DeferredShadingParamsNode::GetDebugAO() const { return m_debugAO; }

bool& DeferredShadingParamsNode::EditDebugRoughness() { return m_debugRoughness; }
const bool& DeferredShadingParamsNode::GetDebugRoughness() const { return m_debugRoughness; }

bool& DeferredShadingParamsNode::EditDebugMetallic() { return m_debugMetallic; }
const bool& DeferredShadingParamsNode::GetDebugMetallic() const { return m_debugMetallic; }

bool& DeferredShadingParamsNode::EditDebugDepth() { return m_debugDepth; }
const bool& DeferredShadingParamsNode::GetDebugDepth() const { return m_debugDepth; }

bool& DeferredShadingParamsNode::EditDebugTextureCoords() { return m_debugTextureCoords; }
const bool& DeferredShadingParamsNode::GetDebugTextureCoords() const { return m_debugTextureCoords; }

bool& DeferredShadingParamsNode::EditDebugDirectLighting() { return m_debugDirectLighting; }
const bool& DeferredShadingParamsNode::GetDebugDirectLighting() const { return m_debugDirectLighting; }

bool& DeferredShadingParamsNode::EditDebugGI() { return m_debugGI; }
const bool& DeferredShadingParamsNode::GetDebugGI() const { return m_debugGI; }

bool& DeferredShadingParamsNode::EditDebugSpecular() { return m_debugSpecular; }
const bool& DeferredShadingParamsNode::GetDebugSpecular() const { return m_debugSpecular; }

bool& DeferredShadingParamsNode::EditDebugLighting() { return m_debugLighting; }
const bool& DeferredShadingParamsNode::GetDebugLighting() const { return m_debugLighting; }

void DeferredShadingParamsNode::OnCreate()
{
    m_applyPending = true;
    ApplyIfNeeded();
}

void DeferredShadingParamsNode::OnDestroy()
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

    ResetDeferredShadingPasses(Renderer, m_viewportID);
    InvalidatePassCache();
}

void DeferredShadingParamsNode::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void DeferredShadingParamsNode::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    ApplyIfNeeded();
}

void DeferredShadingParamsNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_applyPending = true;
    ApplyIfNeeded();
}
#endif

void DeferredShadingParamsNode::ApplyIfNeeded()
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

bool DeferredShadingParamsNode::ApplyToPass()
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

    using Feature = SnAPI::Graphics::DeferredContract::Feature;

    std::uint32_t FeatureMask = 0u;
    auto AppendFeature = [&FeatureMask](const bool Enabled, const Feature Flag) {
        if (Enabled)
        {
            FeatureMask |= static_cast<std::uint32_t>(Flag);
        }
    };

    AppendFeature(m_debugMotionVectors, Feature::DebugMotionVectors);
    AppendFeature(m_debugNormals, Feature::DebugNormals);
    AppendFeature(m_debugAlbedo, Feature::DebugAlbedo);
    AppendFeature(m_debugAO, Feature::DebugAO);
    AppendFeature(m_debugRoughness, Feature::DebugRoughness);
    AppendFeature(m_debugMetallic, Feature::DebugMetallic);
    AppendFeature(m_debugDepth, Feature::DebugDepth);
    AppendFeature(m_debugTextureCoords, Feature::DebugTextureCoords);
    AppendFeature(m_debugDirectLighting, Feature::DebugDirectLighting);
    AppendFeature(m_debugGI, Feature::DebugGI);
    AppendFeature(m_debugSpecular, Feature::DebugSpecular);
    AppendFeature(m_debugLighting, Feature::DebugLighting);

    bool AppliedAny = false;
    const auto TargetViewports = ResolveTargetViewports(*Graphics, m_viewportID);
    for (const auto ViewportId : TargetViewports)
    {
        auto* Pass = Graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::DeferredShading);
        auto* Deferred = dynamic_cast<SnAPI::Graphics::DeferredShadingPass*>(Pass);
        if (!Deferred)
        {
            continue;
        }

        Deferred->SetFeatures(static_cast<Feature>(FeatureMask));
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

void DeferredShadingParamsNode::InvalidatePassCache()
{
    m_applyPending = true;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
