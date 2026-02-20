#include "UIRenderViewport.h"

#if defined(SNAPI_GF_ENABLE_UI) && defined(SNAPI_GF_ENABLE_RENDERER)

#include "GameRuntime.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include <ICamera.hpp>
#include <UIContext.h>
#include <UIEvents.h>
#include <UIPacketWriter.h>
#include <UISizing.h>

namespace SnAPI::GameFramework
{
namespace
{
constexpr float kDefaultWidth = 640.0f;
constexpr float kDefaultHeight = 360.0f;
constexpr float kMinRenderScale = 0.05f;
constexpr float kMaxRenderScale = 8.0f;
constexpr float kSmallNumber = 0.0001f;
} // namespace

UIRenderViewport::UIRenderViewport()
{
    m_Properties.SetDefaultProperty(ViewportNameKey, std::string{"RenderViewport"});
    m_Properties.SetDefaultProperty(EnabledKey, true);
    m_Properties.SetDefaultProperty(RenderScaleKey, 1.0f);
    m_Properties.SetDefaultProperty(PassGraphPresetKey, ERenderViewportPassGraphPreset::DefaultWorld);
    m_Properties.SetDefaultProperty(AutoRegisterPassGraphKey, true);

    m_Properties.SetDefaultProperty(BackgroundColorKey, SnAPI::UI::Color{9, 14, 24, 0});
    m_Properties.SetDefaultProperty(BorderColorKey, SnAPI::UI::Color{72, 96, 128, 220});
    m_Properties.SetDefaultProperty(BorderThicknessKey, 1.0f);
    m_Properties.SetDefaultProperty(CornerRadiusKey, 4.0f);
}

void UIRenderViewport::Initialize(SnAPI::UI::UIContext* Context, const SnAPI::UI::ElementId Id)
{
    InitializeBase(Context, Id);
}

void UIRenderViewport::SetGameRuntime(GameRuntime* Runtime)
{
    if (m_runtime == Runtime)
    {
        return;
    }

    ReleaseOwnedResources();
    m_runtime = Runtime;
    SyncViewport();
}

void UIRenderViewport::SetViewportCamera(SnAPI::Graphics::ICamera* Camera)
{
    if (m_camera == Camera)
    {
        return;
    }

    m_camera = Camera;
    SyncViewport();
}

void UIRenderViewport::SetPointerEventHandler(PointerEventHandler Handler)
{
    m_pointerEventHandler = std::move(Handler);
}

void UIRenderViewport::ClearPointerEventHandler()
{
    m_pointerEventHandler = {};
}

void UIRenderViewport::Measure(const SnAPI::UI::UIConstraints& Constraints, SnAPI::UI::UISize& OutDesired)
{
    if (IsCollapsed())
    {
        OutDesired = {};
        return;
    }

    const float Dpi = GetDpiScale();
    const auto WidthSizing = GetStyledProperty(WidthKey, SnAPI::UI::Sizing::Auto());
    const auto HeightSizing = GetStyledProperty(HeightKey, SnAPI::UI::Sizing::Auto());

    switch (WidthSizing.Mode)
    {
    case SnAPI::UI::ESizingMode::Fixed:
        OutDesired.W = WidthSizing.Value * Dpi;
        break;
    case SnAPI::UI::ESizingMode::Ratio:
        OutDesired.W = Constraints.Max.W > 0.0f ? Constraints.Max.W : Constraints.Min.W;
        break;
    case SnAPI::UI::ESizingMode::Auto:
    default:
        OutDesired.W = kDefaultWidth * Dpi;
        break;
    }

    switch (HeightSizing.Mode)
    {
    case SnAPI::UI::ESizingMode::Fixed:
        OutDesired.H = HeightSizing.Value * Dpi;
        break;
    case SnAPI::UI::ESizingMode::Ratio:
        OutDesired.H = Constraints.Max.H > 0.0f ? Constraints.Max.H : Constraints.Min.H;
        break;
    case SnAPI::UI::ESizingMode::Auto:
    default:
        OutDesired.H = kDefaultHeight * Dpi;
        break;
    }

    ApplyConstraints(OutDesired, Constraints);
}

void UIRenderViewport::Arrange(const SnAPI::UI::UIRect& FinalRect)
{
    UIElementBase::Arrange(FinalRect);
    SyncViewport();
}

void UIRenderViewport::Paint(SnAPI::UI::UIPaintContext& Context) const
{
    const_cast<UIRenderViewport*>(this)->SyncViewport();

    if (m_Rect.W <= 0.0f || m_Rect.H <= 0.0f)
    {
        return;
    }

    const float Dpi = GetDpiScale();
    const float BorderThickness = std::max(0.0f, GetStyledProperty(BorderThicknessKey, 1.0f));
    const float CornerRadius = std::max(0.0f, GetStyledProperty(CornerRadiusKey, 4.0f) * Dpi);
    const auto BackgroundColor = GetStyledProperty(BackgroundColorKey, SnAPI::UI::Color{9, 14, 24, 0});
    const auto BorderColor = GetStyledProperty(BorderColorKey, SnAPI::UI::Color{72, 96, 128, 220});

    // Context.Packets.DrawRect(
    //     m_Rect,
    //     BackgroundColor,
    //     CornerRadius,
    //     BorderColor,
    //     BorderThickness,
    //     SnAPI::UI::MaterialHandle{});

    //PaintContent(Context);
}

void UIRenderViewport::OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context)
{
    const uint32_t TypeId = Context.TypeId();
    if (TypeId == SnAPI::UI::RoutedEventTypes::PointerEnter.Id)
    {
        SetHovered(true);
        return;
    }

    if (TypeId == SnAPI::UI::RoutedEventTypes::PointerLeave.Id)
    {
        SetHovered(false);
        SetPressed(false);
        return;
    }

    if (TypeId == SnAPI::UI::RoutedEventTypes::PointerMove.Id ||
        TypeId == SnAPI::UI::RoutedEventTypes::PointerDown.Id ||
        TypeId == SnAPI::UI::RoutedEventTypes::PointerUp.Id)
    {
        auto* PointerPayload = static_cast<SnAPI::UI::PointerEvent*>(Context.Payload());
        if (!PointerPayload)
        {
            return;
        }

        const bool ContainsPointer = m_Rect.Contains(PointerPayload->Position);
        if (m_pointerEventHandler)
        {
            m_pointerEventHandler(*PointerPayload, TypeId, ContainsPointer);
        }

        if (TypeId == SnAPI::UI::RoutedEventTypes::PointerMove.Id)
        {
            SetHovered(ContainsPointer);
            if (!PointerPayload->LeftDown)
            {
                SetPressed(false);
            }
            return;
        }

        if (TypeId == SnAPI::UI::RoutedEventTypes::PointerDown.Id)
        {
            if (PointerPayload->LeftDown && ContainsPointer)
            {
                SetPressed(true);
                Context.SetHandled(true);
            }
            return;
        }

        if (TypeId == SnAPI::UI::RoutedEventTypes::PointerUp.Id)
        {
            if (IsPressed())
            {
                SetPressed(false);
                if (ContainsPointer)
                {
                    Context.SetHandled(true);
                }
            }
        }
    }
}

void UIRenderViewport::OnFocusChanged(const bool Focused)
{
    SetFocused(Focused);
}

void UIRenderViewport::ReleaseOwnedResources()
{
    if (!m_runtime)
    {
        m_ownedViewportId = 0;
        m_ownedContextId = 0;
        m_bindingEstablished = false;
        m_registeredPassGraphPreset.reset();
        return;
    }

    auto* World = m_runtime->WorldPtr();
    if (!World)
    {
        m_ownedViewportId = 0;
        m_ownedContextId = 0;
        m_bindingEstablished = false;
        m_registeredPassGraphPreset.reset();
        return;
    }

    if (m_bindingEstablished && m_ownedViewportId != 0)
    {
        (void)m_runtime->UnbindViewportFromUI(m_ownedViewportId);
        m_bindingEstablished = false;
    }

    if (m_ownedContextId != 0 && World->UI().IsInitialized())
    {
        (void)World->UI().DestroyContext(m_ownedContextId);
    }

    if (m_ownedViewportId != 0 && World->Renderer().IsInitialized())
    {
        (void)World->Renderer().DestroyRenderViewport(m_ownedViewportId);
    }

    m_ownedViewportId = 0;
    m_ownedContextId = 0;
    m_bindingEstablished = false;
    m_registeredPassGraphPreset.reset();
}

void UIRenderViewport::SyncViewport()
{
    if (!m_runtime)
    {
        return;
    }

    auto* World = m_runtime->WorldPtr();
    if (!World || !World->Renderer().IsInitialized() || !World->UI().IsInitialized())
    {
        return;
    }

    auto& Renderer = World->Renderer();
    auto& UI = World->UI();

    if (m_Rect.W <= kSmallNumber || m_Rect.H <= kSmallNumber)
    {
        return;
    }

    if (m_ownedContextId != 0 && !UI.Context(m_ownedContextId))
    {
        m_ownedContextId = 0;
        m_bindingEstablished = false;
    }

    if (m_ownedViewportId != 0 && !Renderer.HasRenderViewport(m_ownedViewportId))
    {
        m_ownedViewportId = 0;
        m_bindingEstablished = false;
        m_registeredPassGraphPreset.reset();
    }

    if (m_ownedContextId == 0)
    {
        const std::uint64_t ParentContextId = UI.ContextIdFor(m_Context);
        if (ParentContextId == 0)
        {
            return;
        }

        std::uint64_t NewContextId = 0;
        if (auto CreateContextResult = UI.CreateContext(ParentContextId, NewContextId); !CreateContextResult)
        {
            return;
        }

        m_ownedContextId = NewContextId;

        if (auto* ChildContext = UI.Context(m_ownedContextId))
        {
            auto Root = ChildContext->Root();
            auto& RootPanel = Root.Element();
            RootPanel.Padding().Set(0.0f);
            RootPanel.Gap().Set(0.0f);
            RootPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});
            RootPanel.BorderColor().Set(SnAPI::UI::Color{0, 0, 0, 0});
            RootPanel.BorderThickness().Set(0.0f);
            RootPanel.CornerRadius().Set(0.0f);
            RootPanel.Width().Set(SnAPI::UI::Sizing::Fill());
            RootPanel.Height().Set(SnAPI::UI::Sizing::Fill());
        }
    }

    const std::string StyledName = GetStyledProperty(ViewportNameKey, std::string{"RenderViewport"});
    const std::string Name = StyledName.empty() ? std::string{"RenderViewport"} : StyledName;

    float RenderScaleValue = GetStyledProperty(RenderScaleKey, 1.0f);
    if (!std::isfinite(RenderScaleValue))
    {
        RenderScaleValue = 1.0f;
    }
    RenderScaleValue = std::clamp(RenderScaleValue, kMinRenderScale, kMaxRenderScale);

    const bool EnabledValue = GetStyledProperty(EnabledKey, true) && IsVisible();
    const std::uint32_t RenderWidth = ComputeRenderExtent(m_Rect.W, RenderScaleValue);
    const std::uint32_t RenderHeight = ComputeRenderExtent(m_Rect.H, RenderScaleValue);
    if (m_camera && RenderHeight > 0u)
    {
        m_camera->Aspect(static_cast<float>(RenderWidth) / static_cast<float>(RenderHeight));
    }

    if (m_ownedViewportId == 0)
    {
        std::uint64_t NewViewportId = 0;
        if (!Renderer.CreateRenderViewport(
                Name, m_Rect.X, m_Rect.Y, m_Rect.W, m_Rect.H, RenderWidth, RenderHeight, m_camera, EnabledValue, NewViewportId))
        {
            return;
        }

        m_ownedViewportId = NewViewportId;
        m_registeredPassGraphPreset.reset();
    }

    if (m_ownedViewportId != 0)
    {
        const bool AutoRegisterPassGraph = GetStyledProperty(AutoRegisterPassGraphKey, true);
        const auto Preset = GetStyledProperty(PassGraphPresetKey, ERenderViewportPassGraphPreset::DefaultWorld);
        if (!AutoRegisterPassGraph || Preset == ERenderViewportPassGraphPreset::None)
        {
            m_registeredPassGraphPreset.reset();
        }
        else if (!m_registeredPassGraphPreset.has_value() || *m_registeredPassGraphPreset != Preset)
        {
            if (Renderer.RegisterRenderViewportPassGraph(m_ownedViewportId, Preset))
            {
                m_registeredPassGraphPreset = Preset;
            }
        }
    }

    if (m_bindingEstablished)
    {
        const auto BoundContext = m_runtime->BoundUIContext(m_ownedViewportId);
        if (!BoundContext || *BoundContext != m_ownedContextId)
        {
            m_bindingEstablished = false;
        }
    }

    if (!m_bindingEstablished)
    {
        if (auto BindResult = m_runtime->BindViewportWithUI(m_ownedViewportId, m_ownedContextId); !BindResult)
        {
            return;
        }

        m_bindingEstablished = true;
    }

    (void)UI.SetContextScreenRect(m_ownedContextId, m_Rect.X, m_Rect.Y, m_Rect.W, m_Rect.H);
    if (auto* ChildContext = UI.Context(m_ownedContextId))
    {
        ChildContext->SetViewportSize(std::max(m_Rect.W, 1.0f), std::max(m_Rect.H, 1.0f));
    }

    (void)Renderer.UpdateRenderViewport(
        m_ownedViewportId, Name, m_Rect.X, m_Rect.Y, m_Rect.W, m_Rect.H, RenderWidth, RenderHeight, m_camera, EnabledValue);
}

std::uint32_t UIRenderViewport::ComputeRenderExtent(const float LogicalSize, const float RenderScale)
{
    if (!std::isfinite(LogicalSize) || !std::isfinite(RenderScale))
    {
        return 1u;
    }

    const float Scaled = std::max(1.0f, LogicalSize * RenderScale);
    constexpr auto MaxValue = std::numeric_limits<std::uint32_t>::max();
    if (Scaled >= static_cast<float>(MaxValue))
    {
        return MaxValue;
    }

    const long long Rounded = std::llround(Scaled);
    if (Rounded <= 1)
    {
        return 1u;
    }

    if (Rounded >= static_cast<long long>(MaxValue))
    {
        return MaxValue;
    }

    return static_cast<std::uint32_t>(Rounded);
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI && SNAPI_GF_ENABLE_RENDERER
