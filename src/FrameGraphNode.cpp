#include "FrameGraphNode.h"

#if defined(SNAPI_GF_ENABLE_UI)

#include <algorithm>
#include <cmath>
#include <utility>

#include "IWorld.h"
#include "StaticTypeId.h"
#include "UISystem.h"

#include <UIContext.h>
#include <UIRealtimeGraph.h>

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] float SanitizePositiveRange(const float Value, const float Fallback)
{
    return (std::isfinite(Value) && Value > 0.0f) ? Value : Fallback;
}

[[nodiscard]] std::uint32_t ClampSampleCapacity(const std::uint32_t Value)
{
    return std::max<std::uint32_t>(2u, Value);
}

[[nodiscard]] std::uint32_t ClampPrecision(const std::uint32_t Value)
{
    return std::min<std::uint32_t>(Value, 5u);
}
} // namespace

FrameGraphNode::FrameGraphNode()
{
    TypeKey(StaticTypeId<FrameGraphNode>());
}

FrameGraphNode::FrameGraphNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<FrameGraphNode>());
}

std::string& FrameGraphNode::EditTitle() { return m_title; }
const std::string& FrameGraphNode::GetTitle() const { return m_title; }

bool& FrameGraphNode::EditShowLegend() { return m_showLegend; }
const bool& FrameGraphNode::GetShowLegend() const { return m_showLegend; }

std::uint32_t& FrameGraphNode::EditValuePrecision() { return m_valuePrecision; }
const std::uint32_t& FrameGraphNode::GetValuePrecision() const { return m_valuePrecision; }

std::uint32_t& FrameGraphNode::EditSampleCapacity() { return m_sampleCapacity; }
const std::uint32_t& FrameGraphNode::GetSampleCapacity() const { return m_sampleCapacity; }

float& FrameGraphNode::EditFrameTimeMaxSeconds() { return m_frameTimeMaxSeconds; }
const float& FrameGraphNode::GetFrameTimeMaxSeconds() const { return m_frameTimeMaxSeconds; }

float& FrameGraphNode::EditFpsMax() { return m_fpsMax; }
const float& FrameGraphNode::GetFpsMax() const { return m_fpsMax; }

void FrameGraphNode::OnCreate()
{
    m_graphNeedsRebuild = true;
    EnsureGraph();
}

void FrameGraphNode::OnDestroy()
{
    DestroyGraph();
}

void FrameGraphNode::Tick(const float DeltaSeconds)
{
    PushSamples(DeltaSeconds);
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void FrameGraphNode::EditorTick(const float DeltaSeconds)
{
    PushSamples(DeltaSeconds);
}

void FrameGraphNode::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    m_graphNeedsRebuild = true;
    EnsureGraph();
}
#endif

void FrameGraphNode::ResetGraphState()
{
    m_graphElementId = {};
    m_graphContextId = 0;
    m_frameTimeSeries = kInvalidSeries;
    m_fpsSeries = kInvalidSeries;
}

void FrameGraphNode::DestroyGraph()
{
    auto* WorldPtr = World();
    if (WorldPtr && WorldPtr->UI().IsInitialized() && m_graphContextId != 0 && m_graphElementId.Value != 0)
    {
        if (auto* Context = WorldPtr->UI().Context(m_graphContextId))
        {
            Context->DestroyElement(m_graphElementId);
        }
    }

    ResetGraphState();
    m_graphNeedsRebuild = true;
}

void FrameGraphNode::EnsureGraph()
{
    auto* WorldPtr = World();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        ResetGraphState();
        return;
    }

    auto& UI = WorldPtr->UI();
    const auto RootContextId = UI.RootContextId();
    if (RootContextId == 0)
    {
        ResetGraphState();
        return;
    }

    auto* Context = UI.Context(RootContextId);
    if (!Context)
    {
        ResetGraphState();
        return;
    }

    if (!m_graphNeedsRebuild && m_graphContextId == RootContextId && m_graphElementId.Value != 0)
    {
        if (dynamic_cast<SnAPI::UI::UIRealtimeGraph*>(&Context->GetElement(m_graphElementId)) != nullptr)
        {
            return;
        }
        ResetGraphState();
    }

    if (m_graphElementId.Value != 0)
    {
        if (auto* PreviousContext = UI.Context(m_graphContextId))
        {
            PreviousContext->DestroyElement(m_graphElementId);
        }
        ResetGraphState();
    }

    auto GraphBuilder = m_title.empty()
        ? Context->Root().Add(SnAPI::UI::UIRealtimeGraph{})
        : Context->Root().Add(SnAPI::UI::UIRealtimeGraph(m_title));
    auto& Graph = GraphBuilder.Element();
    Graph.AutoRangeVal(false)
        .AxisColorVal(SnAPI::UI::Color::WhiteSmoke())
        .ShowLegendVal(m_showLegend)
        .ValuePrecisionVal(ClampPrecision(m_valuePrecision))
        .SampleCapacityVal(ClampSampleCapacity(m_sampleCapacity))
        .HAlignVal(SnAPI::UI::EAlignment::End)
        .VAlignVal(SnAPI::UI::EAlignment::Start)
        .WidthVal(SnAPI::UI::Sizing::Auto())
        .HeightVal(SnAPI::UI::Sizing::Auto());

    const auto FrameSeries = Graph.AddSeries("FrameTime", SnAPI::UI::Color::Blue());
    const auto FpsSeries = Graph.AddSeries("FPS", SnAPI::UI::Color::Orange());
    if (FrameSeries != SnAPI::UI::UIRealtimeGraph::InvalidSeries)
    {
        (void)Graph.SetSeriesRange(FrameSeries, 0.0f, SanitizePositiveRange(m_frameTimeMaxSeconds, 0.1f));
    }
    if (FpsSeries != SnAPI::UI::UIRealtimeGraph::InvalidSeries)
    {
        (void)Graph.SetSeriesRange(FpsSeries, 0.0f, SanitizePositiveRange(m_fpsMax, 120.0f));
    }

    m_graphElementId = Graph.GetId();
    m_graphContextId = RootContextId;
    m_frameTimeSeries = FrameSeries;
    m_fpsSeries = FpsSeries;
    m_graphNeedsRebuild = false;
}

void FrameGraphNode::PushSamples(const float DeltaSeconds)
{
    if (!std::isfinite(DeltaSeconds) || DeltaSeconds < 0.0f)
    {
        return;
    }

    EnsureGraph();

    auto* WorldPtr = World();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized() || m_graphContextId == 0 || m_graphElementId.Value == 0)
    {
        return;
    }

    auto* Context = WorldPtr->UI().Context(m_graphContextId);
    auto* Graph = Context ? dynamic_cast<SnAPI::UI::UIRealtimeGraph*>(&Context->GetElement(m_graphElementId)) : nullptr;
    if (!Graph)
    {
        ResetGraphState();
        m_graphNeedsRebuild = true;
        return;
    }

    if (m_frameTimeSeries != kInvalidSeries)
    {
        (void)Graph->PushSample(m_frameTimeSeries, DeltaSeconds);
    }

    if (m_fpsSeries != kInvalidSeries)
    {
        const float Fps = DeltaSeconds > 0.000001f ? (1.0f / DeltaSeconds) : 0.0f;
        (void)Graph->PushSample(m_fpsSeries, Fps);
    }
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
