#include "Conduit/Editor/GraphCanvas.h"

#include <algorithm>
#include <cmath>
#include <string_view>

#include <UIContext.h>
#include <UIEvents.h>
#include <UIPacketWriter.h>

namespace SnAPI::GameFramework::Conduit::Editor
{
namespace
{
constexpr float kFallbackAdvance = 8.0f;
constexpr float kFallbackLineHeight = 16.0f;
constexpr float kDefaultCanvasWidth = 960.0f;
constexpr float kDefaultCanvasHeight = 640.0f;
constexpr float kMinZoom = 0.35f;
constexpr float kMaxZoom = 2.75f;
constexpr float kMinorGridUnits = 32.0f;
constexpr float kMajorGridUnits = 128.0f;
constexpr float kNodeCollapsedHeight = 72.0f;
constexpr float kNodeCornerRadius = 10.0f;
constexpr float kCommentHeaderHeight = 28.0f;
constexpr float kCommentCornerRadius = 12.0f;
constexpr float kNodeTitleInsetX = 14.0f;
constexpr float kNodeTitleInsetY = 12.0f;
constexpr float kNodeDetailGapY = 7.0f;
constexpr float kNodeDetailBaselineY = 18.0f;
constexpr float kNodePinRowHeight = 18.0f;
constexpr float kNodePinRowGap = 7.0f;
constexpr float kNodePinInsetX = 12.0f;
constexpr float kNodePinLabelInset = 12.0f;
constexpr float kNodePinTopY = 60.0f;
constexpr float kCollapsedPinTopY = 46.0f;
constexpr float kNodeBottomPadding = 12.0f;
constexpr float kPinRadius = 5.0f;
constexpr float kExecPinSize = 8.0f;
constexpr float kWireThickness = 2.5f;
constexpr float kWireArrowSize = 7.0f;
constexpr float kNodeFooterBarHeight = 8.0f;
constexpr float kGridLineThickness = 1.0f;
constexpr float kMajorGridLineThickness = 1.35f;
constexpr float kBorderThickness = 1.0f;
constexpr float kSelectedBorderThickness = 2.0f;
constexpr float kPanStrokeFactor = 1.0f / 120.0f;
constexpr float kZoomWheelMagnitude = 100.0f;
constexpr float kRightClickPanThreshold = 6.0f;

[[nodiscard]] SnAPI::UI::ScissorRect ToScissorRect(const SnAPI::UI::UIRect& Rect)
{
    if (Rect.W <= 0.0f || Rect.H <= 0.0f)
    {
        return {};
    }

    return SnAPI::UI::ScissorRect{
        static_cast<std::int32_t>(std::floor(Rect.X)),
        static_cast<std::int32_t>(std::floor(Rect.Y)),
        static_cast<std::int32_t>(std::ceil(Rect.W)),
        static_cast<std::int32_t>(std::ceil(Rect.H)),
    };
}

[[nodiscard]] float ClampZoom(const float Zoom)
{
    if (!std::isfinite(Zoom))
    {
        return 1.0f;
    }
    return std::clamp(Zoom, kMinZoom, kMaxZoom);
}

[[nodiscard]] float ClampWheelStep(const float Delta)
{
    return std::clamp(Delta * kPanStrokeFactor, -4.0f, 4.0f);
}

[[nodiscard]] float EstimateAdvance(const char Character,
                                    const SnAPI::UI::IFontMetrics* Metrics,
                                    const float FallbackAdvance)
{
    if (Metrics)
    {
        if (const auto* Glyph = Metrics->GetGlyph(static_cast<uint32_t>(static_cast<unsigned char>(Character))))
        {
            return Glyph->Advance;
        }
    }
    return FallbackAdvance;
}

[[nodiscard]] float MeasureTextWidth(const std::string_view Text,
                                     const SnAPI::UI::IFontMetrics* Metrics,
                                     const float Scale,
                                     const float FallbackAdvance = kFallbackAdvance)
{
    float Width = 0.0f;
    for (const char Character : Text)
    {
        if (Character == '\n')
        {
            continue;
        }
        Width += EstimateAdvance(Character, Metrics, FallbackAdvance) * Scale;
    }
    return Width;
}

[[nodiscard]] float ResolveLineHeight(const SnAPI::UI::IFontMetrics* Metrics, const float Scale)
{
    return (Metrics ? Metrics->GetLineHeight() : kFallbackLineHeight) * Scale;
}

[[nodiscard]] size_t FitCharsToWidth(const std::string_view Text,
                                     const float MaxWidth,
                                     const SnAPI::UI::IFontMetrics* Metrics,
                                     const float Scale,
                                     const float FallbackAdvance = kFallbackAdvance)
{
    if (MaxWidth <= 0.0f || Text.empty())
    {
        return 0;
    }

    float Width = 0.0f;
    size_t Count = 0;
    for (const char Character : Text)
    {
        if (Character == '\n')
        {
            break;
        }

        const float Advance = EstimateAdvance(Character, Metrics, FallbackAdvance) * Scale;
        if (Width + Advance > MaxWidth)
        {
            break;
        }

        Width += Advance;
        ++Count;
    }

    return Count;
}

[[nodiscard]] std::string EllipsizeToWidth(const std::string_view Text,
                                           const float MaxWidth,
                                           const SnAPI::UI::IFontMetrics* Metrics,
                                           const float Scale)
{
    if (Text.empty())
    {
        return {};
    }

    if (MeasureTextWidth(Text, Metrics, Scale) <= MaxWidth)
    {
        return std::string(Text);
    }

    constexpr std::string_view Ellipsis = "...";
    const float EllipsisWidth = MeasureTextWidth(Ellipsis, Metrics, Scale);
    if (MaxWidth <= EllipsisWidth)
    {
        return std::string(Ellipsis.substr(0, FitCharsToWidth(Ellipsis, MaxWidth, Metrics, Scale)));
    }

    const size_t VisibleCount = FitCharsToWidth(Text, MaxWidth - EllipsisWidth, Metrics, Scale);
    return std::string(Text.substr(0, VisibleCount)) + std::string(Ellipsis);
}

[[nodiscard]] SnAPI::UI::Color PinColor(const CanvasPinView& Pin)
{
    if (Pin.IsExec)
    {
        return SnAPI::UI::Color{233, 167, 72, 255};
    }
    if (Pin.Kind == ESlotKind::Handle)
    {
        return SnAPI::UI::Color{110, 215, 173, 255};
    }
    return SnAPI::UI::Color{110, 180, 255, 255};
}

void DrawWireSegment(SnAPI::UI::PacketWriter& Packets,
                     const SnAPI::UI::UIPoint Start,
                     const SnAPI::UI::UIPoint End,
                     const float Thickness,
                     const SnAPI::UI::Color Color)
{
    if (std::abs(Start.X - End.X) <= 0.25f)
    {
        const float X = Start.X - (Thickness * 0.5f);
        const float Y = std::min(Start.Y, End.Y);
        const float Height = std::max(Thickness, std::abs(End.Y - Start.Y));
        Packets.DrawRect(SnAPI::UI::UIRect{X, Y, Thickness, Height},
                         Color,
                         Thickness * 0.5f,
                         Color,
                         0.0f,
                         SnAPI::UI::MaterialHandle{});
        return;
    }

    const float X = std::min(Start.X, End.X);
    const float Y = Start.Y - (Thickness * 0.5f);
    const float Width = std::max(Thickness, std::abs(End.X - Start.X));
    Packets.DrawRect(SnAPI::UI::UIRect{X, Y, Width, Thickness},
                     Color,
                     Thickness * 0.5f,
                     Color,
                     0.0f,
                     SnAPI::UI::MaterialHandle{});
}

void DrawWireArrow(SnAPI::UI::PacketWriter& Packets,
                   const SnAPI::UI::UIPoint Tip,
                   const bool PointsLeft,
                   const float Size,
                   const SnAPI::UI::Color Color)
{
    if (PointsLeft)
    {
        Packets.DrawTriangle(Tip,
                             SnAPI::UI::UIPoint{Tip.X + Size, Tip.Y - (Size * 0.6f)},
                             SnAPI::UI::UIPoint{Tip.X + Size, Tip.Y + (Size * 0.6f)},
                             Color,
                             SnAPI::UI::MaterialHandle{});
        return;
    }

    Packets.DrawTriangle(Tip,
                         SnAPI::UI::UIPoint{Tip.X - Size, Tip.Y - (Size * 0.6f)},
                         SnAPI::UI::UIPoint{Tip.X - Size, Tip.Y + (Size * 0.6f)},
                         Color,
                         SnAPI::UI::MaterialHandle{});
}

void ReleaseCanvasCaptureIfOwned(SnAPI::UI::UIContext* const Context, const SnAPI::UI::ElementId Id)
{
    if (Context && Context->GetCapture() == Id)
    {
        Context->ReleaseCapture();
    }
}
} // namespace

UIConduitGraphCanvas::UIConduitGraphCanvas()
{
    Width().Set(SnAPI::UI::Sizing::Fill());
    Height().Set(SnAPI::UI::Sizing::Fill());
}

void UIConduitGraphCanvas::Initialize(SnAPI::UI::UIContext* Context, const SnAPI::UI::ElementId Id)
{
    InitializeBase(Context, Id);
}

void UIConduitGraphCanvas::SetViewState(GraphCanvasView View)
{
    m_view = std::move(View);
    m_view.Viewport.Zoom = EffectiveZoom();

    const bool DragNodeStillExists = std::find_if(m_view.Nodes.begin(), m_view.Nodes.end(), [this](const CanvasNodeView& Node) {
        return Node.Id == m_dragNodeId;
    }) != m_view.Nodes.end();
    if (!DragNodeStillExists)
    {
        m_isDraggingNode = false;
        m_dragNodeId = {};
    }

    const bool DragWireNodeStillExists = std::find_if(m_view.Nodes.begin(), m_view.Nodes.end(), [this](const CanvasNodeView& Node) {
        return Node.Id == m_dragWireNodeId;
    }) != m_view.Nodes.end();
    if (!DragWireNodeStillExists)
    {
        m_isDraggingWire = false;
        m_dragWireNodeId = {};
        m_dragWirePinName.clear();
    }

    Invalidate(SnAPI::UI::EInvalidation::Paint);
}

void UIConduitGraphCanvas::SetNodeSelectionHandler(NodeSelectionHandler Handler)
{
    m_onNodeSelected = std::move(Handler);
}

void UIConduitGraphCanvas::SetNodeMovedHandler(NodeMovedHandler Handler)
{
    m_onNodeMoved = std::move(Handler);
}

void UIConduitGraphCanvas::SetPinConnectedHandler(PinConnectedHandler Handler)
{
    m_onPinConnected = std::move(Handler);
}

void UIConduitGraphCanvas::SetSpawnMenuRequestedHandler(SpawnMenuRequestedHandler Handler)
{
    m_onSpawnMenuRequested = std::move(Handler);
}

void UIConduitGraphCanvas::SetViewportChangedHandler(ViewportChangedHandler Handler)
{
    m_onViewportChanged = std::move(Handler);
}

void UIConduitGraphCanvas::Measure(const SnAPI::UI::UIConstraints& Constraints, SnAPI::UI::UISize& OutDesired)
{
    if (IsCollapsed())
    {
        OutDesired = {};
        return;
    }

    const float Dpi = DpiScale();
    OutDesired.W = kDefaultCanvasWidth * Dpi;
    OutDesired.H = kDefaultCanvasHeight * Dpi;
    ApplyConstraints(OutDesired, Constraints);
}

void UIConduitGraphCanvas::Paint(SnAPI::UI::UIPaintContext& Context) const
{
    if (!IsVisible() || m_Rect.W <= 0.0f || m_Rect.H <= 0.0f)
    {
        return;
    }

    const float Dpi = DpiScale();
    const float Zoom = EffectiveZoom();
    const float Scale = Dpi * Zoom;
    const float CornerRadius = kNodeCornerRadius * Scale;
    const float CommentRadius = kCommentCornerRadius * Scale;
    const float GridMinor = kMinorGridUnits * Scale;
    const float GridMajor = kMajorGridUnits * Scale;
    const SnAPI::UI::Color BackgroundColor{14, 18, 24, 255};
    const SnAPI::UI::Color BorderColor{54, 69, 88, 255};
    const SnAPI::UI::Color MinorGridColor{28, 35, 45, 255};
    const SnAPI::UI::Color MajorGridColor{42, 54, 70, 255};
    const SnAPI::UI::Color NodeColor{31, 37, 48, 245};
    const SnAPI::UI::Color NodeBorderColor{91, 109, 132, 255};
    const SnAPI::UI::Color NodeSelectedColor{54, 71, 92, 250};
    const SnAPI::UI::Color NodeSelectedBorder{196, 226, 255, 255};
    const SnAPI::UI::Color NodeDetailColor{185, 197, 214, 255};
    const SnAPI::UI::Color NodeTitleColor{242, 247, 255, 255};
    const SnAPI::UI::Color NodePinTextColor{216, 227, 241, 255};
    const SnAPI::UI::Color NodeFooterColor{94, 132, 171, 255};
    const SnAPI::UI::Color EmptyTextColor{184, 192, 204, 255};
    const auto* Metrics = Context.Packets.GetFontMetrics();

    Context.Packets.DrawRect(m_Rect,
                             BackgroundColor,
                             8.0f * Dpi,
                             BorderColor,
                             kBorderThickness,
                             SnAPI::UI::MaterialHandle{});

    Context.Packets.PushScissor(ToScissorRect(m_Rect));

    if (GridMinor >= 12.0f)
    {
        const float MinorOffsetX = std::fmod((m_view.Viewport.PanX * Scale), GridMinor);
        const float MinorOffsetY = std::fmod((m_view.Viewport.PanY * Scale), GridMinor);
        for (float X = m_Rect.X - MinorOffsetX; X <= (m_Rect.X + m_Rect.W); X += GridMinor)
        {
            Context.Packets.DrawRect(
                SnAPI::UI::UIRect{X, m_Rect.Y, kGridLineThickness, m_Rect.H},
                MinorGridColor,
                0.0f,
                MinorGridColor,
                0.0f,
                SnAPI::UI::MaterialHandle{});
        }
        for (float Y = m_Rect.Y - MinorOffsetY; Y <= (m_Rect.Y + m_Rect.H); Y += GridMinor)
        {
            Context.Packets.DrawRect(
                SnAPI::UI::UIRect{m_Rect.X, Y, m_Rect.W, kGridLineThickness},
                MinorGridColor,
                0.0f,
                MinorGridColor,
                0.0f,
                SnAPI::UI::MaterialHandle{});
        }
    }

    const float MajorOffsetX = std::fmod((m_view.Viewport.PanX * Scale), GridMajor);
    const float MajorOffsetY = std::fmod((m_view.Viewport.PanY * Scale), GridMajor);
    for (float X = m_Rect.X - MajorOffsetX; X <= (m_Rect.X + m_Rect.W); X += GridMajor)
    {
        Context.Packets.DrawRect(
            SnAPI::UI::UIRect{X, m_Rect.Y, kMajorGridLineThickness, m_Rect.H},
            MajorGridColor,
            0.0f,
            MajorGridColor,
            0.0f,
            SnAPI::UI::MaterialHandle{});
    }
    for (float Y = m_Rect.Y - MajorOffsetY; Y <= (m_Rect.Y + m_Rect.H); Y += GridMajor)
    {
        Context.Packets.DrawRect(
            SnAPI::UI::UIRect{m_Rect.X, Y, m_Rect.W, kMajorGridLineThickness},
            MajorGridColor,
            0.0f,
            MajorGridColor,
            0.0f,
            SnAPI::UI::MaterialHandle{});
    }

    for (const CanvasCommentView& Comment : m_view.Comments)
    {
        const CommentVisual Visual = ComputeCommentVisual(Comment);
        const SnAPI::UI::Color FillColor = DecodeColor(Comment.ColorRgba, 52);
        const SnAPI::UI::Color OutlineColor = DecodeColor(Comment.ColorRgba, 170);
        Context.Packets.DrawRect(Visual.Rect,
                                 FillColor,
                                 CommentRadius,
                                 OutlineColor,
                                 Comment.Selected ? kSelectedBorderThickness : kBorderThickness,
                                 SnAPI::UI::MaterialHandle{});

        if (!Comment.Title.empty())
        {
            const float TextScale = Scale;
            const float TextInsetX = 12.0f * Scale;
            const float TextInsetY = 8.0f * Scale;
            const float TextMaxWidth = std::max(0.0f, Visual.Rect.W - (TextInsetX * 2.0f));
            const std::string Title = EllipsizeToWidth(Comment.Title, TextMaxWidth, Metrics, TextScale);
            Context.Packets.DrawText(Visual.Rect.X + TextInsetX,
                                     Visual.Rect.Y + TextInsetY,
                                     Title.c_str(),
                                     Title.length(),
                                     DecodeColor(Comment.ColorRgba, 255),
                                     SnAPI::UI::FontId{},
                                     SnAPI::UI::MaterialHandle{},
                                     TextScale);
        }
    }

    for (const CanvasWireView& Wire : m_view.Wires)
    {
        const auto SourceNodeIt = std::find_if(m_view.Nodes.begin(), m_view.Nodes.end(), [&Wire](const CanvasNodeView& Node) {
            return Node.Id == Wire.SourceNodeId;
        });
        const auto TargetNodeIt = std::find_if(m_view.Nodes.begin(), m_view.Nodes.end(), [&Wire](const CanvasNodeView& Node) {
            return Node.Id == Wire.TargetNodeId;
        });
        if (SourceNodeIt == m_view.Nodes.end() || TargetNodeIt == m_view.Nodes.end())
        {
            continue;
        }

        const auto SourcePinIt = std::find_if(SourceNodeIt->OutputPins.begin(),
                                              SourceNodeIt->OutputPins.end(),
                                              [&Wire](const CanvasPinView& Pin) {
                                                  return Pin.Name == Wire.SourcePin;
                                              });
        const auto TargetPinIt = std::find_if(TargetNodeIt->InputPins.begin(),
                                              TargetNodeIt->InputPins.end(),
                                              [&Wire](const CanvasPinView& Pin) {
                                                  return Pin.Name == Wire.TargetPin;
                                              });
        if (SourcePinIt == SourceNodeIt->OutputPins.end() || TargetPinIt == TargetNodeIt->InputPins.end())
        {
            continue;
        }

        const std::size_t SourceIndex = static_cast<std::size_t>(std::distance(SourceNodeIt->OutputPins.begin(), SourcePinIt));
        const std::size_t TargetIndex = static_cast<std::size_t>(std::distance(TargetNodeIt->InputPins.begin(), TargetPinIt));
        const NodeVisual SourceVisual = ComputeNodeVisual(*SourceNodeIt);
        const NodeVisual TargetVisual = ComputeNodeVisual(*TargetNodeIt);
        const PinVisual SourcePin = ComputePinVisual(*SourceNodeIt, SourceVisual, false, SourceIndex);
        const PinVisual TargetPin = ComputePinVisual(*TargetNodeIt, TargetVisual, true, TargetIndex);

        const SnAPI::UI::Color WireColor = PinColor(*SourcePinIt);
        const float Thickness = std::max(1.5f, kWireThickness * Dpi * std::clamp(Zoom, 0.75f, 1.25f));
        const float HorizontalLead = std::max(28.0f * Dpi, 42.0f * Dpi * std::clamp(Zoom, 0.8f, 1.2f));
        const float MidX = std::max(SourcePin.Center.X + HorizontalLead,
                                    std::min(TargetPin.Center.X - HorizontalLead,
                                             (SourcePin.Center.X + TargetPin.Center.X) * 0.5f));

        const SnAPI::UI::UIPoint P0 = SourcePin.Center;
        const SnAPI::UI::UIPoint P1{MidX, SourcePin.Center.Y};
        const SnAPI::UI::UIPoint P2{MidX, TargetPin.Center.Y};
        const SnAPI::UI::UIPoint P3 = TargetPin.Center;

        DrawWireSegment(Context.Packets, P0, P1, Thickness, WireColor);
        DrawWireSegment(Context.Packets, P1, P2, Thickness, WireColor);
        DrawWireSegment(Context.Packets, P2, P3, Thickness, WireColor);
        DrawWireArrow(Context.Packets,
                      TargetPin.Center,
                      TargetPin.Center.X <= P2.X,
                      std::max(5.0f, kWireArrowSize * Dpi),
                      WireColor);
    }

    if (m_isDraggingWire && !m_dragWirePinName.empty())
    {
        const auto SourceNodeIt = std::find_if(m_view.Nodes.begin(), m_view.Nodes.end(), [this](const CanvasNodeView& Node) {
            return Node.Id == m_dragWireNodeId;
        });
        if (SourceNodeIt != m_view.Nodes.end())
        {
            const auto SourcePinIt = std::find_if(SourceNodeIt->OutputPins.begin(),
                                                  SourceNodeIt->OutputPins.end(),
                                                  [this](const CanvasPinView& Pin) {
                                                      return Pin.Name == m_dragWirePinName;
                                                  });
            if (SourcePinIt != SourceNodeIt->OutputPins.end())
            {
                const std::size_t SourceIndex =
                    static_cast<std::size_t>(std::distance(SourceNodeIt->OutputPins.begin(), SourcePinIt));
                const NodeVisual SourceVisual = ComputeNodeVisual(*SourceNodeIt);
                const PinVisual SourcePin = ComputePinVisual(*SourceNodeIt, SourceVisual, false, SourceIndex);
                const SnAPI::UI::Color WireColor = PinColor(*SourcePinIt);
                const float Thickness = std::max(1.5f, kWireThickness * Dpi * std::clamp(Zoom, 0.75f, 1.25f));
                const float HorizontalLead = std::max(28.0f * Dpi, 42.0f * Dpi * std::clamp(Zoom, 0.8f, 1.2f));
                const float MidX = std::max(SourcePin.Center.X + HorizontalLead,
                                            std::min(m_dragWirePointer.X - HorizontalLead,
                                                     (SourcePin.Center.X + m_dragWirePointer.X) * 0.5f));

                const SnAPI::UI::UIPoint P0 = SourcePin.Center;
                const SnAPI::UI::UIPoint P1{MidX, SourcePin.Center.Y};
                const SnAPI::UI::UIPoint P2{MidX, m_dragWirePointer.Y};
                const SnAPI::UI::UIPoint P3 = m_dragWirePointer;
                DrawWireSegment(Context.Packets, P0, P1, Thickness, WireColor);
                DrawWireSegment(Context.Packets, P1, P2, Thickness, WireColor);
                DrawWireSegment(Context.Packets, P2, P3, Thickness, WireColor);
                DrawWireArrow(Context.Packets,
                              m_dragWirePointer,
                              m_dragWirePointer.X <= P2.X,
                              std::max(5.0f, kWireArrowSize * Dpi),
                              WireColor);
            }
        }
    }

    if (m_view.Nodes.empty())
    {
        static constexpr std::string_view kEmptyText =
            "Use the palette on the left to add entry nodes, logic nodes, and reflected fields or methods.";
        Context.Packets.DrawText(m_Rect.X + (18.0f * Dpi),
                                 m_Rect.Y + (18.0f * Dpi),
                                 kEmptyText.data(),
                                 kEmptyText.size(),
                                 EmptyTextColor,
                                 SnAPI::UI::FontId{},
                                 SnAPI::UI::MaterialHandle{},
                                 Dpi);
        Context.Packets.PopScissor();
        return;
    }

    for (const CanvasNodeView& Node : m_view.Nodes)
    {
        const NodeVisual Visual = ComputeNodeVisual(Node);
        const SnAPI::UI::Color FillColor = Node.Selected ? NodeSelectedColor : NodeColor;
        const SnAPI::UI::Color StrokeColor = Node.Selected ? NodeSelectedBorder : NodeBorderColor;
        const float StrokeThickness = Node.Selected ? kSelectedBorderThickness : kBorderThickness;

        Context.Packets.DrawRect(Visual.Rect,
                                 FillColor,
                                 CornerRadius,
                                 StrokeColor,
                                 StrokeThickness,
                                 SnAPI::UI::MaterialHandle{});

        Context.Packets.DrawRect(
            SnAPI::UI::UIRect{
                Visual.Rect.X,
                Visual.Rect.Y + Visual.Rect.H - (kNodeFooterBarHeight * Scale),
                Visual.Rect.W,
                kNodeFooterBarHeight * Scale,
            },
            NodeFooterColor,
            CornerRadius * 0.5f,
            NodeFooterColor,
            0.0f,
            SnAPI::UI::MaterialHandle{});

        const float TextScale = Scale;
        const float PinTextScale = Scale * 0.9f;
        const float PinLineHeight = ResolveLineHeight(Metrics, PinTextScale);
        const float TitleMaxWidth = std::max(0.0f, Visual.Rect.W - (kNodeTitleInsetX * 2.0f * Scale));
        const std::string Title = EllipsizeToWidth(Node.Title, TitleMaxWidth, Metrics, TextScale);
        const float TitleX = Visual.Rect.X + (kNodeTitleInsetX * Scale);
        const float TitleY = Visual.Rect.Y + (kNodeTitleInsetY * Scale);
        Context.Packets.DrawText(TitleX,
                                 TitleY,
                                 Title.c_str(),
                                 Title.length(),
                                 NodeTitleColor,
                                 SnAPI::UI::FontId{},
                                 SnAPI::UI::MaterialHandle{},
                                 TextScale);

        if (!Node.IsCollapsed)
        {
            for (std::size_t PinIndex = 0; PinIndex < Node.InputPins.size(); ++PinIndex)
            {
                const CanvasPinView& Pin = Node.InputPins[PinIndex];
                const PinVisual PinVisual = ComputePinVisual(Node, Visual, true, PinIndex);
                const SnAPI::UI::Color PinFill = PinColor(Pin);
                const float Radius = std::max(3.0f, kPinRadius * Scale);
                const float ExecSize = std::max(5.0f, kExecPinSize * Scale);

                if (Pin.IsExec)
                {
                    Context.Packets.DrawRect(SnAPI::UI::UIRect{
                                                 PinVisual.Center.X - (ExecSize * 0.5f),
                                                 PinVisual.Center.Y - (ExecSize * 0.5f),
                                                 ExecSize,
                                                 ExecSize,
                                             },
                                             PinFill,
                                             2.0f,
                                             NodeBorderColor,
                                             1.0f,
                                             SnAPI::UI::MaterialHandle{});
                }
                else
                {
                    Context.Packets.DrawCircle(PinVisual.Center,
                                               Radius,
                                               PinFill,
                                               NodeBorderColor,
                                               1.0f,
                                               SnAPI::UI::MaterialHandle{});
                }

                const float LabelX = PinVisual.Center.X + (kNodePinLabelInset * Scale);
                const float LabelMaxWidth = std::max(0.0f,
                                                     (Visual.Rect.X + Visual.Rect.W) - LabelX - (10.0f * Scale));
                const std::string PinLabel = EllipsizeToWidth(Pin.Name, LabelMaxWidth, Metrics, PinTextScale);
                Context.Packets.DrawText(LabelX,
                                         PinVisual.Center.Y - (PinLineHeight * 0.5f),
                                         PinLabel.c_str(),
                                         PinLabel.length(),
                                         NodePinTextColor,
                                         SnAPI::UI::FontId{},
                                         SnAPI::UI::MaterialHandle{},
                                         PinTextScale);
            }

            for (std::size_t PinIndex = 0; PinIndex < Node.OutputPins.size(); ++PinIndex)
            {
                const CanvasPinView& Pin = Node.OutputPins[PinIndex];
                const PinVisual PinVisual = ComputePinVisual(Node, Visual, false, PinIndex);
                const SnAPI::UI::Color PinFill = PinColor(Pin);
                const float Radius = std::max(3.0f, kPinRadius * Scale);
                const float ExecSize = std::max(5.0f, kExecPinSize * Scale);

                if (Pin.IsExec)
                {
                    Context.Packets.DrawRect(SnAPI::UI::UIRect{
                                                 PinVisual.Center.X - (ExecSize * 0.5f),
                                                 PinVisual.Center.Y - (ExecSize * 0.5f),
                                                 ExecSize,
                                                 ExecSize,
                                             },
                                             PinFill,
                                             2.0f,
                                             NodeBorderColor,
                                             1.0f,
                                             SnAPI::UI::MaterialHandle{});
                }
                else
                {
                    Context.Packets.DrawCircle(PinVisual.Center,
                                               Radius,
                                               PinFill,
                                               NodeBorderColor,
                                               1.0f,
                                               SnAPI::UI::MaterialHandle{});
                }

                const float LabelMaxWidth = std::max(0.0f,
                                                     PinVisual.Center.X - (Visual.Rect.X + (10.0f * Scale)) -
                                                         (kNodePinLabelInset * Scale));
                const std::string PinLabel = EllipsizeToWidth(Pin.Name, LabelMaxWidth, Metrics, PinTextScale);
                const float LabelWidth = MeasureTextWidth(PinLabel, Metrics, PinTextScale);
                Context.Packets.DrawText(std::max(Visual.Rect.X + (10.0f * Scale),
                                                  PinVisual.Center.X - LabelWidth - (kNodePinLabelInset * Scale)),
                                         PinVisual.Center.Y - (PinLineHeight * 0.5f),
                                         PinLabel.c_str(),
                                         PinLabel.length(),
                                         NodePinTextColor,
                                         SnAPI::UI::FontId{},
                                         SnAPI::UI::MaterialHandle{},
                                         PinTextScale);
            }
        }
    }

    Context.Packets.PopScissor();
}

void UIConduitGraphCanvas::OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context)
{
    const std::uint32_t TypeId = Context.TypeId();
    if (TypeId == SnAPI::UI::RoutedEventTypes::PointerEnter.Id)
    {
        SetHovered(true);
        return;
    }

    if (TypeId == SnAPI::UI::RoutedEventTypes::PointerLeave.Id)
    {
        SetHovered(false);
        if (!m_isDraggingNode && !m_isDraggingWire && !m_isPanning && !m_isPendingContextMenu)
        {
            SetPressed(false);
            ClearInteractionState();
        }
        return;
    }

    if (TypeId == SnAPI::UI::RoutedEventTypes::PointerWheel.Id)
    {
        auto* Wheel = static_cast<SnAPI::UI::WheelEvent*>(Context.Payload());
        if (!Wheel || !m_Rect.Contains(Wheel->Position))
        {
            return;
        }

        const float OldZoom = EffectiveZoom();
        const SnAPI::UI::UIPoint GraphPoint = ScreenToGraph(Wheel->Position);
        const float ZoomFactor = std::pow(1.15f, ClampWheelStep(Wheel->DeltaY) * kZoomWheelMagnitude);
        const float NewZoom = ClampZoom(OldZoom * ZoomFactor);
        if (std::abs(NewZoom - OldZoom) <= 0.0001f)
        {
            return;
        }

        const float Dpi = DpiScale();
        const float LocalX = Wheel->Position.X - m_Rect.X;
        const float LocalY = Wheel->Position.Y - m_Rect.Y;
        const float NewPanX = GraphPoint.X - (LocalX / (Dpi * NewZoom));
        const float NewPanY = GraphPoint.Y - (LocalY / (Dpi * NewZoom));

        m_view.Viewport.PanX = NewPanX;
        m_view.Viewport.PanY = NewPanY;
        m_view.Viewport.Zoom = NewZoom;
        Invalidate(SnAPI::UI::EInvalidation::Paint);
        if (m_onViewportChanged)
        {
            m_onViewportChanged(NewPanX, NewPanY, NewZoom);
        }
        Context.SetHandled(true);
        return;
    }

    const auto* Pointer = static_cast<SnAPI::UI::PointerEvent*>(Context.Payload());
    if (!Pointer)
    {
        return;
    }

    if (TypeId == SnAPI::UI::RoutedEventTypes::PointerMove.Id)
    {
        SetHovered(m_Rect.Contains(Pointer->Position));

        if (m_isPendingContextMenu)
        {
            const float DeltaX = Pointer->Position.X - m_dragStartPointer.X;
            const float DeltaY = Pointer->Position.Y - m_dragStartPointer.Y;
            const float DistanceSq = (DeltaX * DeltaX) + (DeltaY * DeltaY);
            if (!Pointer->RightDown)
            {
                RequestSpawnMenu(Pointer->Position, false);
                SetPressed(false);
                ClearInteractionState();
                Context.SetHandled(true);
                return;
            }
            if (DistanceSq >= (kRightClickPanThreshold * kRightClickPanThreshold))
            {
                m_isPendingContextMenu = false;
                m_isPanning = true;
                SetPressed(true);
            }
        }

        if (!m_isDraggingNode &&
            !m_isPendingContextMenu &&
            !m_isPanning &&
            m_Rect.Contains(Pointer->Position) &&
            Pointer->MiddleDown)
        {
            m_isPanning = true;
            m_dragStartPointer = Pointer->Position;
            m_dragStartPanX = m_view.Viewport.PanX;
            m_dragStartPanY = m_view.Viewport.PanY;
            SetPressed(true);
        }

        if (m_isDraggingNode)
        {
            if (!Pointer->LeftDown)
            {
                UpdateDraggedNodePosition(Pointer->Position);
                SetPressed(false);
                ClearInteractionState();
                Context.SetHandled(true);
                return;
            }

            UpdateDraggedNodePosition(Pointer->Position);
            Context.SetHandled(true);
            return;
        }

        if (m_isDraggingWire)
        {
            if (!Pointer->LeftDown)
            {
                CompleteWireDrag(Pointer->Position);
                SetPressed(false);
                ClearInteractionState();
                Context.SetHandled(true);
                return;
            }

            m_dragWirePointer = Pointer->Position;
            Invalidate(SnAPI::UI::EInvalidation::Paint);
            Context.SetHandled(true);
            return;
        }

        if (m_isPanning)
        {
            if (!Pointer->RightDown && !Pointer->MiddleDown)
            {
                UpdatePanPosition(Pointer->Position);
                SetPressed(false);
                ClearInteractionState();
                Context.SetHandled(true);
                return;
            }

            UpdatePanPosition(Pointer->Position);
            Context.SetHandled(true);
            return;
        }

        return;
    }

    if (TypeId == SnAPI::UI::RoutedEventTypes::PointerDown.Id)
    {
        if (!m_Rect.Contains(Pointer->Position))
        {
            return;
        }

        if (Pointer->RightDown)
        {
            m_isPendingContextMenu = true;
            m_dragStartPointer = Pointer->Position;
            m_dragStartPanX = m_view.Viewport.PanX;
            m_dragStartPanY = m_view.Viewport.PanY;
            if (m_Context)
            {
                m_Context->SetCapture(m_Id);
            }
            SetPressed(true);
            Context.SetHandled(true);
            return;
        }

        if (Pointer->MiddleDown)
        {
            m_isPanning = true;
            m_dragStartPointer = Pointer->Position;
            m_dragStartPanX = m_view.Viewport.PanX;
            m_dragStartPanY = m_view.Viewport.PanY;
            if (m_Context)
            {
                m_Context->SetCapture(m_Id);
            }
            SetPressed(true);
            Context.SetHandled(true);
            return;
        }

        if (!Pointer->LeftDown)
        {
            return;
        }

        if (const auto HitOutputPin = HitTestPin(Pointer->Position, true); HitOutputPin.has_value())
        {
            const CanvasNodeView& HitNode = m_view.Nodes[HitOutputPin->NodeIndex];
            const CanvasPinView& HitPin = HitNode.OutputPins[HitOutputPin->PinIndex];
            SetSelectedNodeLocal(HitNode.Id);
            if (m_onNodeSelected)
            {
                m_onNodeSelected(HitNode.Id);
            }

            m_isDraggingWire = true;
            m_dragWireNodeId = HitNode.Id;
            m_dragWirePinName = HitPin.Name;
            m_dragWireKind = HitPin.Kind;
            m_dragWireIsExec = HitPin.IsExec;
            m_dragWirePointer = Pointer->Position;
            if (m_Context)
            {
                m_Context->SetCapture(m_Id);
            }
            SetPressed(true);
            Context.SetHandled(true);
            return;
        }

        if (const auto HitNodeIndex = HitTestNode(Pointer->Position); HitNodeIndex.has_value())
        {
            const CanvasNodeView& HitNode = m_view.Nodes[*HitNodeIndex];
            SetSelectedNodeLocal(HitNode.Id);
            if (m_onNodeSelected)
            {
                m_onNodeSelected(HitNode.Id);
            }

            m_isDraggingNode = true;
            m_dragNodeId = HitNode.Id;
            const SnAPI::UI::UIPoint GraphPoint = ScreenToGraph(Pointer->Position);
            m_dragNodeOffsetGraph = SnAPI::UI::UIPoint{GraphPoint.X - HitNode.X, GraphPoint.Y - HitNode.Y};
            if (m_Context)
            {
                m_Context->SetCapture(m_Id);
            }
            SetPressed(true);
            Context.SetHandled(true);
            return;
        }

        SetPressed(true);
        Context.SetHandled(true);
        return;
    }

    if (TypeId == SnAPI::UI::RoutedEventTypes::PointerUp.Id)
    {
        const bool WasDraggingWire = m_isDraggingWire;
        const bool WasPendingContextMenu = m_isPendingContextMenu;
        const bool WasInteracting =
            m_isDraggingNode || m_isDraggingWire || m_isPanning || m_isPendingContextMenu || IsPressed();
        SetPressed(false);
        if (m_isDraggingNode)
        {
            UpdateDraggedNodePosition(Pointer->Position);
        }
        if (m_isPanning)
        {
            UpdatePanPosition(Pointer->Position);
        }
        if (WasDraggingWire)
        {
            CompleteWireDrag(Pointer->Position);
        }
        if (WasPendingContextMenu)
        {
            RequestSpawnMenu(Pointer->Position, false);
        }
        ClearInteractionState();
        if (WasInteracting)
        {
            Context.SetHandled(true);
        }
    }
}

void UIConduitGraphCanvas::Invalidate(const SnAPI::UI::EInvalidation Flags) const
{
    if (m_Context)
    {
        if (Flags == SnAPI::UI::EInvalidation::Paint)
        {
            m_Context->InvalidateElementLocal(m_Id, Flags);
        }
        else
        {
            m_Context->InvalidateElement(m_Id, Flags);
        }
    }
}

float UIConduitGraphCanvas::EffectiveZoom() const
{
    return ClampZoom(m_view.Viewport.Zoom <= 0.0f ? 1.0f : m_view.Viewport.Zoom);
}

float UIConduitGraphCanvas::DpiScale() const
{
    return GetDpiScale();
}

float UIConduitGraphCanvas::NodeHeightGraphUnits(const CanvasNodeView& Node) const
{
    if (Node.IsCollapsed)
    {
        return kNodeCollapsedHeight;
    }

    const std::size_t PinRows = std::max(Node.InputPins.size(), Node.OutputPins.size());
    if (PinRows == 0)
    {
        return 96.0f;
    }

    return kNodePinTopY + (static_cast<float>(PinRows) * kNodePinRowHeight) +
        (static_cast<float>(PinRows > 0 ? PinRows - 1 : 0) * kNodePinRowGap) +
        kNodeBottomPadding + kNodeFooterBarHeight;
}

UIConduitGraphCanvas::NodeVisual UIConduitGraphCanvas::ComputeNodeVisual(const CanvasNodeView& Node) const
{
    const float Scale = DpiScale() * EffectiveZoom();
    return NodeVisual{
        .Rect = SnAPI::UI::UIRect{
            m_Rect.X + ((Node.X - m_view.Viewport.PanX) * Scale),
            m_Rect.Y + ((Node.Y - m_view.Viewport.PanY) * Scale),
            std::max(120.0f, Node.Width) * Scale,
            NodeHeightGraphUnits(Node) * Scale,
        },
        .GraphX = Node.X,
        .GraphY = Node.Y,
    };
}

UIConduitGraphCanvas::PinVisual UIConduitGraphCanvas::ComputePinVisual(const CanvasNodeView& Node,
                                                                       const NodeVisual& Visual,
                                                                       const bool IsInput,
                                                                       const std::size_t PinIndex) const
{
    const float Scale = DpiScale() * EffectiveZoom();
    const float PinYBase = (Node.IsCollapsed ? kCollapsedPinTopY : kNodePinTopY) * Scale;
    const float PinStep = (kNodePinRowHeight + kNodePinRowGap) * Scale;
    const float PinX = IsInput
        ? (Visual.Rect.X + (kNodePinInsetX * Scale))
        : (Visual.Rect.X + Visual.Rect.W - (kNodePinInsetX * Scale));

    return PinVisual{
        .Center = SnAPI::UI::UIPoint{
            PinX,
            Visual.Rect.Y + PinYBase + (static_cast<float>(PinIndex) * PinStep),
        },
    };
}

UIConduitGraphCanvas::CommentVisual UIConduitGraphCanvas::ComputeCommentVisual(const CanvasCommentView& Comment) const
{
    const float Scale = DpiScale() * EffectiveZoom();
    return CommentVisual{
        .Rect = SnAPI::UI::UIRect{
            m_Rect.X + ((Comment.X - m_view.Viewport.PanX) * Scale),
            m_Rect.Y + ((Comment.Y - m_view.Viewport.PanY) * Scale),
            Comment.Width * Scale,
            std::max(Comment.Height, kCommentHeaderHeight) * Scale,
        },
    };
}

SnAPI::UI::UIPoint UIConduitGraphCanvas::ScreenToGraph(const SnAPI::UI::UIPoint& ScreenPosition) const
{
    const float Scale = DpiScale() * EffectiveZoom();
    return SnAPI::UI::UIPoint{
        m_view.Viewport.PanX + ((ScreenPosition.X - m_Rect.X) / Scale),
        m_view.Viewport.PanY + ((ScreenPosition.Y - m_Rect.Y) / Scale),
    };
}

std::optional<std::size_t> UIConduitGraphCanvas::HitTestNode(const SnAPI::UI::UIPoint& ScreenPosition) const
{
    for (std::size_t Index = m_view.Nodes.size(); Index > 0; --Index)
    {
        const std::size_t NodeIndex = Index - 1;
        if (ComputeNodeVisual(m_view.Nodes[NodeIndex]).Rect.Contains(ScreenPosition))
        {
            return NodeIndex;
        }
    }
    return std::nullopt;
}

std::optional<UIConduitGraphCanvas::HitPinResult> UIConduitGraphCanvas::HitTestPin(const SnAPI::UI::UIPoint& ScreenPosition,
                                                                                   const bool OutputsOnly) const
{
    const float Scale = DpiScale() * EffectiveZoom();
    const float HitRadius = std::max(10.0f, kNodePinRowHeight * Scale * 0.75f);
    const float HitRadiusSq = HitRadius * HitRadius;
    const float InputRowHalfHeight = std::max(HitRadius, ((kNodePinRowHeight + kNodePinRowGap) * Scale) * 0.5f);

    for (std::size_t Index = m_view.Nodes.size(); Index > 0; --Index)
    {
        const std::size_t NodeIndex = Index - 1;
        const CanvasNodeView& Node = m_view.Nodes[NodeIndex];
        const NodeVisual Visual = ComputeNodeVisual(Node);

        if (!OutputsOnly)
        {
            for (std::size_t PinIndex = 0; PinIndex < Node.InputPins.size(); ++PinIndex)
            {
                const PinVisual Pin = ComputePinVisual(Node, Visual, true, PinIndex);
                const float InputHitWidth = std::max(HitRadius * 2.0f, Visual.Rect.W);
                const SnAPI::UI::UIRect HitRect{
                    Visual.Rect.X,
                    Pin.Center.Y - InputRowHalfHeight,
                    InputHitWidth,
                    InputRowHalfHeight * 2.0f,
                };
                if (HitRect.Contains(ScreenPosition))
                {
                    return HitPinResult{
                        .NodeIndex = NodeIndex,
                        .IsInput = true,
                        .PinIndex = PinIndex,
                    };
                }
            }
        }

        if (OutputsOnly)
        {
            for (std::size_t PinIndex = 0; PinIndex < Node.OutputPins.size(); ++PinIndex)
            {
                const PinVisual Pin = ComputePinVisual(Node, Visual, false, PinIndex);
                const float DeltaX = ScreenPosition.X - Pin.Center.X;
                const float DeltaY = ScreenPosition.Y - Pin.Center.Y;
                if ((DeltaX * DeltaX) + (DeltaY * DeltaY) <= HitRadiusSq)
                {
                    return HitPinResult{
                        .NodeIndex = NodeIndex,
                        .IsInput = false,
                        .PinIndex = PinIndex,
                    };
                }
            }
        }
    }

    return std::nullopt;
}

SnAPI::UI::Color UIConduitGraphCanvas::DecodeColor(const std::uint32_t Rgba, const std::uint8_t DefaultAlpha)
{
    const std::uint8_t R = static_cast<std::uint8_t>((Rgba >> 24u) & 0xFFu);
    const std::uint8_t G = static_cast<std::uint8_t>((Rgba >> 16u) & 0xFFu);
    const std::uint8_t B = static_cast<std::uint8_t>((Rgba >> 8u) & 0xFFu);
    const std::uint8_t A = static_cast<std::uint8_t>(Rgba & 0xFFu);
    return SnAPI::UI::Color{R, G, B, A == 0 ? DefaultAlpha : A};
}

void UIConduitGraphCanvas::UpdateDraggedNodePosition(const SnAPI::UI::UIPoint& ScreenPosition)
{
    const SnAPI::UI::UIPoint GraphPoint = ScreenToGraph(ScreenPosition);
    const float NewX = GraphPoint.X - m_dragNodeOffsetGraph.X;
    const float NewY = GraphPoint.Y - m_dragNodeOffsetGraph.Y;
    for (CanvasNodeView& Node : m_view.Nodes)
    {
        if (Node.Id == m_dragNodeId)
        {
            Node.X = NewX;
            Node.Y = NewY;
            break;
        }
    }
    Invalidate(SnAPI::UI::EInvalidation::Paint);
    if (m_onNodeMoved)
    {
        m_onNodeMoved(m_dragNodeId, NewX, NewY);
    }
}

void UIConduitGraphCanvas::UpdatePanPosition(const SnAPI::UI::UIPoint& ScreenPosition)
{
    const float Scale = DpiScale() * EffectiveZoom();
    const float DeltaX = (ScreenPosition.X - m_dragStartPointer.X) / Scale;
    const float DeltaY = (ScreenPosition.Y - m_dragStartPointer.Y) / Scale;
    m_view.Viewport.PanX = m_dragStartPanX - DeltaX;
    m_view.Viewport.PanY = m_dragStartPanY - DeltaY;
    Invalidate(SnAPI::UI::EInvalidation::Paint);
    if (m_onViewportChanged)
    {
        m_onViewportChanged(m_view.Viewport.PanX, m_view.Viewport.PanY, EffectiveZoom());
    }
}

void UIConduitGraphCanvas::CompleteWireDrag(const SnAPI::UI::UIPoint& ScreenPosition)
{
    const Uuid SourceNodeId = m_dragWireNodeId;
    const std::string SourcePinName = m_dragWirePinName;
    if (const auto HitInputPin = HitTestPin(ScreenPosition, false); HitInputPin.has_value())
    {
        const CanvasNodeView& TargetNode = m_view.Nodes[HitInputPin->NodeIndex];
        const CanvasPinView& TargetPin = TargetNode.InputPins[HitInputPin->PinIndex];
        if (m_onPinConnected && !SourcePinName.empty())
        {
            m_onPinConnected(SourceNodeId, SourcePinName, TargetNode.Id, TargetPin.Name);
        }
        return;
    }

    RequestSpawnMenu(ScreenPosition, true);
}

void UIConduitGraphCanvas::RequestSpawnMenu(const SnAPI::UI::UIPoint& ScreenPosition, const bool FromPinDrag)
{
    if (!m_onSpawnMenuRequested || !m_Rect.Contains(ScreenPosition))
    {
        return;
    }

    GraphSpawnMenuRequest Request{};
    Request.ScreenX = ScreenPosition.X;
    Request.ScreenY = ScreenPosition.Y;
    const SnAPI::UI::UIPoint GraphPoint = ScreenToGraph(ScreenPosition);
    Request.GraphX = GraphPoint.X;
    Request.GraphY = GraphPoint.Y;
    Request.FromPinDrag = FromPinDrag;
    if (FromPinDrag)
    {
        Request.SourceNodeId = m_dragWireNodeId;
        Request.SourcePin = m_dragWirePinName;
    }

    m_onSpawnMenuRequested(Request);
}

void UIConduitGraphCanvas::SetSelectedNodeLocal(const Uuid& NodeId)
{
    for (CanvasNodeView& Node : m_view.Nodes)
    {
        Node.Selected = (Node.Id == NodeId);
    }
    Invalidate(SnAPI::UI::EInvalidation::Paint);
}

void UIConduitGraphCanvas::ClearInteractionState()
{
    m_isPendingContextMenu = false;
    m_isPanning = false;
    m_isDraggingNode = false;
    m_isDraggingWire = false;
    m_dragNodeId = {};
    m_dragWireNodeId = {};
    m_dragWirePinName.clear();
    m_dragWireKind = ESlotKind::Value;
    m_dragWireIsExec = false;
    ReleaseCanvasCaptureIfOwned(m_Context, m_Id);
}

} // namespace SnAPI::GameFramework::Conduit::Editor
