#include "Editor/EditorGameViewportOverlayService.h"

#include "Editor/EditorLayoutService.h"
#include "GameRuntime.h"
#include "UIRenderViewport.h"
#include "World.h"

#include <UIContext.h>
#include <UIPanel.h>
#include <UIRealtimeGraph.h>
#include <UISizing.h>
#include <UIText.h>

#include <limits>

namespace SnAPI::GameFramework::Editor
{
std::string_view EditorGameViewportOverlayService::Name() const
{
    return "EditorGameViewportOverlayService";
}

std::vector<std::type_index> EditorGameViewportOverlayService::Dependencies() const
{
    return {std::type_index(typeid(EditorLayoutService))};
}

Result EditorGameViewportOverlayService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    ResetOverlayState();
    return Ok();
}

void EditorGameViewportOverlayService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
#if !defined(SNAPI_GF_ENABLE_UI) || !defined(SNAPI_GF_ENABLE_RENDERER)
    (void)Context;
    (void)DeltaSeconds;
    ResetOverlayState();
    return;
#else
    auto* LayoutService = Context.GetService<EditorLayoutService>();
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!LayoutService || !WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        return;
    }

    auto* Viewport = LayoutService->GameViewportElement();
    if (!Viewport)
    {
        return;
    }

    const std::uint64_t OverlayContextId = Viewport->OwnedContextId();
    if (OverlayContextId == 0)
    {
        return;
    }

    if (m_overlayContextId != OverlayContextId)
    {
        ResetOverlayState();
        m_overlayContextId = OverlayContextId;
    }

    auto* OverlayContext = WorldPtr->UI().Context(m_overlayContextId);
    if (!OverlayContext)
    {
        return;
    }

    if (!EnsureOverlayElements(*OverlayContext))
    {
        return;
    }

    UpdateOverlayVisibility(*OverlayContext, LayoutService->GameViewportTabIndex());
    UpdateOverlaySamples(*OverlayContext, DeltaSeconds);
#endif
}

void EditorGameViewportOverlayService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    ResetOverlayState();
}

void EditorGameViewportOverlayService::ResetOverlayState()
{
    m_overlayContextId = 0;

    m_hudPanel = {};
    m_hudGraph = {};
    m_hudFrameLabel = {};
    m_hudFpsLabel = {};
    m_hudFrameSeries = std::numeric_limits<std::uint32_t>::max();
    m_hudFpsSeries = std::numeric_limits<std::uint32_t>::max();

    m_profilerPanel = {};
    m_profilerGraph = {};
    m_profilerFrameLabel = {};
    m_profilerFpsLabel = {};
    m_profilerFrameSeries = std::numeric_limits<std::uint32_t>::max();
    m_profilerFpsSeries = std::numeric_limits<std::uint32_t>::max();
}

bool EditorGameViewportOverlayService::EnsureOverlayElements(SnAPI::UI::UIContext& OverlayContext)
{
#if !defined(SNAPI_GF_ENABLE_UI)
    (void)OverlayContext;
    return false;
#else
    const auto ExistingHudGraph = dynamic_cast<SnAPI::UI::UIRealtimeGraph*>(&OverlayContext.GetElement(m_hudGraph));
    const auto ExistingHudFrameLabel = dynamic_cast<SnAPI::UI::UIText*>(&OverlayContext.GetElement(m_hudFrameLabel));
    const auto ExistingHudFpsLabel = dynamic_cast<SnAPI::UI::UIText*>(&OverlayContext.GetElement(m_hudFpsLabel));
    if (ExistingHudGraph && ExistingHudFrameLabel && ExistingHudFpsLabel)
    {
        return true;
    }

    m_hudPanel = {};
    m_hudGraph = {};
    m_hudFrameLabel = {};
    m_hudFpsLabel = {};
    m_hudFrameSeries = std::numeric_limits<std::uint32_t>::max();
    m_hudFpsSeries = std::numeric_limits<std::uint32_t>::max();

    m_profilerPanel = {};
    m_profilerGraph = {};
    m_profilerFrameLabel = {};
    m_profilerFpsLabel = {};
    m_profilerFrameSeries = std::numeric_limits<std::uint32_t>::max();
    m_profilerFpsSeries = std::numeric_limits<std::uint32_t>::max();

    auto HudPanelBuilder = OverlayContext.Root().Add(SnAPI::UI::UIPanel("Editor.GameViewportOverlay.HUD"));
    auto& HudPanel = HudPanelBuilder.Element();
    HudPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    HudPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    HudPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HudPanel.HAlign().Set(SnAPI::UI::EAlignment::End);
    HudPanel.VAlign().Set(SnAPI::UI::EAlignment::End);
    HudPanel.ElementMargin().Set(SnAPI::UI::Margin{12.0f, 12.0f, 12.0f, 12.0f});
    HudPanel.Padding().Set(6.0f);
    HudPanel.Gap().Set(3.0f);
    HudPanel.Background().Set(SnAPI::UI::Color{20, 22, 27, 214});
    HudPanel.BorderColor().Set(SnAPI::UI::Color{87, 93, 104, 220});
    HudPanel.BorderThickness().Set(1.0f);
    HudPanel.CornerRadius().Set(6.0f);
    HudPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudStatsBuilder = HudPanelBuilder.Add(SnAPI::UI::UIPanel("Editor.GameViewportOverlay.HUD.Stats"));
    auto& HudStats = HudStatsBuilder.Element();
    HudStats.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HudStats.Width().Set(SnAPI::UI::Sizing::Auto());
    HudStats.Height().Set(SnAPI::UI::Sizing::Auto());
    HudStats.Gap().Set(12.0f);
    HudStats.Background().Set(SnAPI::UI::Color::Transparent());
    HudStats.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudFrameLabelBuilder = HudStatsBuilder.Add(SnAPI::UI::UIText("Frame: -- ms"));
    auto& HudFrameLabel = HudFrameLabelBuilder.Element();
    HudFrameLabel.Width().Set(SnAPI::UI::Sizing::Auto());
    HudFrameLabel.TextColor().Set(SnAPI::UI::Color{206, 212, 221, 255});
    HudFrameLabel.HAlign().Set(SnAPI::UI::EAlignment::Start);
    HudFrameLabel.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    HudFrameLabel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudFpsLabelBuilder = HudStatsBuilder.Add(SnAPI::UI::UIText("FPS: --"));
    auto& HudFpsLabel = HudFpsLabelBuilder.Element();
    HudFpsLabel.Width().Set(SnAPI::UI::Sizing::Auto());
    HudFpsLabel.TextColor().Set(SnAPI::UI::Color{223, 227, 234, 255});
    HudFpsLabel.HAlign().Set(SnAPI::UI::EAlignment::Start);
    HudFpsLabel.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    HudFpsLabel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudGraphBuilder = HudPanelBuilder.Add(SnAPI::UI::UIRealtimeGraph("Frame Time / FPS"));
    auto& HudGraph = HudGraphBuilder.Element();
    HudGraph.Width().Set(SnAPI::UI::Sizing::Auto());
    HudGraph.Height().Set(SnAPI::UI::Sizing::Auto());
    HudGraph.SampleCapacity().Set(220u);
    HudGraph.AutoRange().Set(true);
    HudGraph.ShowLegend().Set(true);
    HudGraph.GridLinesX().Set(8u);
    HudGraph.GridLinesY().Set(4u);
    HudGraph.ContentPadding().Set(6.0f);
    HudGraph.LineThickness().Set(1.6f);
    HudGraph.ValuePrecision().Set(1u);
    HudGraph.BackgroundColor().Set(SnAPI::UI::Color{19, 21, 25, 224});
    HudGraph.PlotBackgroundColor().Set(SnAPI::UI::Color{24, 27, 33, 230});
    HudGraph.BorderColor().Set(SnAPI::UI::Color{84, 90, 101, 216});
    HudGraph.GridColor().Set(SnAPI::UI::Color{92, 99, 110, 76});
    HudGraph.AxisColor().Set(SnAPI::UI::Color{130, 137, 149, 152});
    HudGraph.TitleColor().Set(SnAPI::UI::Color{228, 231, 237, 255});
    HudGraph.LegendTextColor().Set(SnAPI::UI::Color{186, 192, 202, 255});
    HudGraph.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    const std::uint32_t HudFrameSeries = HudGraph.AddSeries("Frame ms", SnAPI::UI::Color{184, 191, 201, 255});
    const std::uint32_t HudFpsSeries = HudGraph.AddSeries("FPS", SnAPI::UI::Color{223, 228, 235, 255});
    if (HudFrameSeries != SnAPI::UI::UIRealtimeGraph::InvalidSeries)
    {
        (void)HudGraph.SetSeriesRange(HudFrameSeries, 0.0f, 33.34f);
    }
    if (HudFpsSeries != SnAPI::UI::UIRealtimeGraph::InvalidSeries)
    {
        (void)HudGraph.SetSeriesRange(HudFpsSeries, 0.0f, 240.0f);
    }

    m_hudPanel = HudPanelBuilder.Handle().Id;
    m_hudGraph = HudGraphBuilder.Handle().Id;
    m_hudFrameLabel = HudFrameLabelBuilder.Handle().Id;
    m_hudFpsLabel = HudFpsLabelBuilder.Handle().Id;
    m_hudFrameSeries = HudFrameSeries;
    m_hudFpsSeries = HudFpsSeries;

    m_profilerPanel = {};
    m_profilerGraph = {};
    m_profilerFrameLabel = {};
    m_profilerFpsLabel = {};
    m_profilerFrameSeries = std::numeric_limits<std::uint32_t>::max();
    m_profilerFpsSeries = std::numeric_limits<std::uint32_t>::max();
    return true;
#endif
}

void EditorGameViewportOverlayService::UpdateOverlayVisibility(SnAPI::UI::UIContext& OverlayContext,
                                                               const int32_t ActiveTabIndex)
{
#if !defined(SNAPI_GF_ENABLE_UI)
    (void)OverlayContext;
    (void)ActiveTabIndex;
#else
    (void)ActiveTabIndex;
    constexpr SnAPI::UI::EVisibility HudVisibility = SnAPI::UI::EVisibility::HitTestInvisible;
    constexpr SnAPI::UI::EVisibility ProfilerVisibility = SnAPI::UI::EVisibility::Collapsed;

    if (m_hudPanel.Value != 0)
    {
        if (auto* HudPanel = dynamic_cast<SnAPI::UI::UIPanel*>(&OverlayContext.GetElement(m_hudPanel)))
        {
            HudPanel->Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, HudVisibility);
        }
    }

    if (m_profilerPanel.Value != 0)
    {
        if (auto* ProfilerPanel = dynamic_cast<SnAPI::UI::UIPanel*>(&OverlayContext.GetElement(m_profilerPanel)))
        {
            ProfilerPanel->Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, ProfilerVisibility);
        }
    }
#endif
}

void EditorGameViewportOverlayService::UpdateOverlaySamples(SnAPI::UI::UIContext& OverlayContext, const float DeltaSeconds)
{
#if !defined(SNAPI_GF_ENABLE_UI)
    (void)OverlayContext;
    (void)DeltaSeconds;
#else
    if (!std::isfinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
    {
        return;
    }

    const float FrameMs = std::clamp(DeltaSeconds * 1000.0f, 0.0f, 500.0f);
    const float FramesPerSecond = std::clamp(1.0f / DeltaSeconds, 0.0f, 2000.0f);

    auto PushGraphSamples = [FrameMs, FramesPerSecond](SnAPI::UI::UIContext& Context,
                                                       const SnAPI::UI::ElementId GraphId,
                                                       const std::uint32_t FrameSeries,
                                                       const std::uint32_t FpsSeries) {
        auto* Graph = dynamic_cast<SnAPI::UI::UIRealtimeGraph*>(&Context.GetElement(GraphId));
        if (!Graph || FrameSeries == std::numeric_limits<std::uint32_t>::max())
        {
            return;
        }
        (void)Graph->PushSample(FrameSeries, FrameMs);
        if (FpsSeries != std::numeric_limits<std::uint32_t>::max())
        {
            (void)Graph->PushSample(FpsSeries, FramesPerSecond);
        }
    };

    auto UpdateLabel = [FrameMs, FramesPerSecond](SnAPI::UI::UIContext& Context,
                                                  const SnAPI::UI::ElementId FrameLabelId,
                                                  const SnAPI::UI::ElementId FpsLabelId) {
        if (auto* FrameLabel = dynamic_cast<SnAPI::UI::UIText*>(&Context.GetElement(FrameLabelId)))
        {
            char Buffer[64]{};
            std::snprintf(Buffer, sizeof(Buffer), "Frame: %.2f ms", FrameMs);
            FrameLabel->Text().Set(std::string(Buffer));
        }
        if (auto* FpsLabel = dynamic_cast<SnAPI::UI::UIText*>(&Context.GetElement(FpsLabelId)))
        {
            char Buffer[64]{};
            std::snprintf(Buffer, sizeof(Buffer), "FPS: %.1f", FramesPerSecond);
            FpsLabel->Text().Set(std::string(Buffer));
        }
    };

    PushGraphSamples(OverlayContext, m_hudGraph, m_hudFrameSeries, m_hudFpsSeries);
    UpdateLabel(OverlayContext, m_hudFrameLabel, m_hudFpsLabel);
#endif
}


} // namespace SnAPI::GameFramework::Editor
