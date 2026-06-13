#include "Editor/UIHelpTooltip.h"

#include <UIContext.h>
#include <UIEvents.h>

#include <algorithm>
#include <string>

namespace SnAPI::GameFramework::Editor
{
UIHelpTooltip::UIHelpTooltip()
    : SnAPI::UI::UIBadge("?")
{
    Width().SetDefault(SnAPI::UI::Sizing::Auto());
    Height().SetDefault(SnAPI::UI::Sizing::Auto());
    Visibility().SetDefault(SnAPI::UI::EVisibility::Visible);

    HorizontalPadding().SetDefault(6.0f);
    VerticalPadding().SetDefault(1.5f);
    CornerRadius().SetDefault(999.0f);
    Background().SetDefault(SnAPI::UI::Color{64, 88, 118, 220});
    BorderColor().SetDefault(SnAPI::UI::Color{108, 140, 176, 235});
    BorderThickness().SetDefault(1.0f);
    TextColor().SetDefault(SnAPI::UI::Color{236, 243, 250, 255});
    TooltipText().SetDefault(std::string{});

    ElementStyle()
        .Hovered()
        .Set(SnAPI::UI::UIBadge::BackgroundKey, SnAPI::UI::Color{90, 122, 158, 236})
        .Set(SnAPI::UI::UIBadge::BorderColorKey, SnAPI::UI::Color{150, 186, 226, 255});

    m_tooltip.UseAnchor().SetDefault(true);
    m_tooltip.ClampToViewport().SetDefault(true);
    m_tooltip.Offset().SetDefault(10.0f);
    m_tooltip.MaxBubbleWidth().SetDefault(420.0f);
    m_tooltip.MaxBubbleHeight().SetDefault(260.0f);
}

void UIHelpTooltip::Initialize(SnAPI::UI::UIContext* const Context, const SnAPI::UI::ElementId Id)
{
    SnAPI::UI::UIBadge::Initialize(Context, Id);
    m_tooltip.Initialize(Context, Id);
}

void UIHelpTooltip::Paint(SnAPI::UI::UIPaintContext& Context) const
{
    SnAPI::UI::UIBadge::Paint(Context);

    if (!IsHovered() || GetStyledProperty(TooltipTextKey, std::string{}).empty())
    {
        return;
    }

    SyncTooltipState();
    m_tooltip.Arrange(ResolveViewportRect());
    m_tooltip.Paint(Context);
}

void UIHelpTooltip::OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context)
{
    const auto InvalidatePaint = [this]() {
        if (m_Context)
        {
            m_Context->InvalidateElement(GetId(), SnAPI::UI::EInvalidation::Paint);
        }
    };

    if (Context.TypeId() == SnAPI::UI::RoutedEventTypes::PointerEnter.Id)
    {
        SetHovered(true);
        InvalidatePaint();
        return;
    }

    if (Context.TypeId() == SnAPI::UI::RoutedEventTypes::PointerLeave.Id)
    {
        SetHovered(false);
        m_tooltip.ResetScroll();
        InvalidatePaint();
        return;
    }

    if (Context.TypeId() == SnAPI::UI::RoutedEventTypes::PointerMove.Id)
    {
        if (const auto* Payload = static_cast<const SnAPI::UI::PointerEvent*>(Context.Payload()))
        {
            const bool Hovered = LayoutRect().Contains(Payload->Position);
            if (Hovered != IsHovered())
            {
                SetHovered(Hovered);
                if (!Hovered)
                {
                    m_tooltip.ResetScroll();
                }
                InvalidatePaint();
            }
        }
        return;
    }

    if (Context.TypeId() == SnAPI::UI::RoutedEventTypes::PointerWheel.Id && IsHovered() &&
        !GetStyledProperty(TooltipTextKey, std::string{}).empty())
    {
        if (const auto* Wheel = static_cast<const SnAPI::UI::WheelEvent*>(Context.Payload()))
        {
            SyncTooltipState();
            m_tooltip.Arrange(ResolveViewportRect());
            if (m_tooltip.BubbleRect().Contains(Wheel->Position))
            {
                m_tooltip.OnRoutedEvent(Context);
                if (Context.Handled())
                {
                    InvalidatePaint();
                }
            }
        }
        return;
    }

    if ((Context.TypeId() == SnAPI::UI::RoutedEventTypes::PointerDown.Id ||
         Context.TypeId() == SnAPI::UI::RoutedEventTypes::PointerUp.Id ||
         Context.TypeId() == SnAPI::UI::RoutedEventTypes::ContextMenuRequest.Id) &&
        Context.Payload() != nullptr)
    {
        const auto* Pointer = static_cast<const SnAPI::UI::PointerEvent*>(Context.Payload());
        if (!LayoutRect().Contains(Pointer->Position) && IsHovered())
        {
            SetHovered(false);
            m_tooltip.ResetScroll();
            InvalidatePaint();
        }
    }
}

SnAPI::UI::UIRect UIHelpTooltip::ResolveViewportRect() const
{
    if (!m_Context)
    {
        return LayoutRect();
    }

    const SnAPI::UI::UISize Viewport = m_Context->GetViewportSize();
    return SnAPI::UI::UIRect{0.0f, 0.0f, Viewport.W, Viewport.H};
}

void UIHelpTooltip::SyncTooltipState() const
{
    m_tooltip.Text().Set(GetStyledProperty(TooltipTextKey, std::string{}));
    m_tooltip.AnchorRect().Set(LayoutRect());
    m_tooltip.Placement().Set(SnAPI::UI::ETooltipPlacement::Right);
}

} // namespace SnAPI::GameFramework::Editor
