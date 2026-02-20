#include "Editor/EditorTheme.h"

#if defined(SNAPI_GF_ENABLE_UI)

#include "UIPropertyPanel.h"

#include <UIAccordion.h>
#include <UIButton.h>
#include <UICheckbox.h>
#include <UIComboBox.h>
#include <UIPanel.h>
#include <UIRadio.h>
#include <UIScrollContainer.h>
#include <UISlider.h>
#include <UIText.h>
#include <UITextInput.h>

namespace SnAPI::GameFramework::Editor
{
namespace
{
using SnAPI::UI::Color;

constexpr Color kWindowBg{7, 11, 18, 255};
constexpr Color kSurfaceBg{13, 18, 27, 248};
constexpr Color kSurfaceElevated{18, 26, 38, 252};
constexpr Color kSurfaceSoft{22, 33, 47, 244};
constexpr Color kBorder{54, 73, 102, 220};
constexpr Color kBorderStrong{83, 118, 168, 224};
constexpr Color kAccent{92, 186, 255, 255};
constexpr Color kAccentStrong{66, 161, 236, 255};
constexpr Color kTextPrimary{230, 238, 252, 255};
constexpr Color kTextSecondary{174, 190, 214, 255};
constexpr Color kTextMuted{124, 141, 166, 255};
constexpr Color kSelection{51, 110, 178, 196};
constexpr Color kDanger{214, 86, 86, 255};
constexpr Color kScrollTrack{12, 20, 31, 216};
constexpr Color kScrollThumb{58, 84, 122, 224};
constexpr Color kScrollThumbHover{76, 111, 156, 236};
} // namespace

EditorTheme::EditorTheme()
    : Theme("SnAPI.Editor")
{
}

void EditorTheme::Initialize()
{
    Define<SnAPI::UI::UIPanel>()
        .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Vertical)
        .Set(SnAPI::UI::UIPanel::PaddingKey, 8.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, kSurfaceBg)
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 6.0f);

    Define<SnAPI::UI::UIText>()
        .Set(SnAPI::UI::UIText::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UIText::WrappingKey, SnAPI::UI::ETextWrapping::NoWrap)
        .Set(SnAPI::UI::UIText::TextAlignmentKey, SnAPI::UI::ETextAlignment::Start)
        .Set(SnAPI::UI::UIText::TextJustifyKey, SnAPI::UI::ETextJustify::None)
        .Disabled()
            .Set(SnAPI::UI::UIText::TextColorKey, kTextMuted);

    Define<SnAPI::UI::UIButton>()
        .Set(SnAPI::UI::UIButton::BackgroundKey, kSurfaceSoft)
        .Set(SnAPI::UI::UIButton::CornerRadiusKey, 5.0f)
        .Hovered()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color{30, 46, 64, 252})
        .Pressed()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color{25, 38, 54, 252})
        .Focused()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color{33, 53, 74, 252});

    Define<SnAPI::UI::UIScrollContainer>()
        .Set(SnAPI::UI::UIScrollContainer::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIScrollContainer::GapKey, 4.0f)
        .Set(SnAPI::UI::UIScrollContainer::DirectionKey, SnAPI::UI::ELayoutDirection::Vertical)
        .Set(SnAPI::UI::UIScrollContainer::ShowHorizontalScrollbarKey, true)
        .Set(SnAPI::UI::UIScrollContainer::ShowVerticalScrollbarKey, true)
        .Set(SnAPI::UI::UIScrollContainer::SmoothKey, true)
        .Set(SnAPI::UI::UIScrollContainer::ScrollbarThicknessKey, 8.0f)
        .Set(SnAPI::UI::UIScrollContainer::BackgroundColorKey, kSurfaceBg)
        .Set(SnAPI::UI::UIScrollContainer::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIScrollContainer::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIScrollContainer::CornerRadiusKey, 6.0f)
        .Set(SnAPI::UI::UIScrollContainer::ScrollbarTrackColorKey, kScrollTrack)
        .Set(SnAPI::UI::UIScrollContainer::ScrollbarThumbColorKey, kScrollThumb)
        .Set(SnAPI::UI::UIScrollContainer::ScrollbarThumbHoverColorKey, kScrollThumbHover);

    Define<SnAPI::UI::UITextInput>()
        .Set(SnAPI::UI::UITextInput::BackgroundColorKey, Color{9, 16, 26, 244})
        .Set(SnAPI::UI::UITextInput::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UITextInput::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UITextInput::CornerRadiusKey, 5.0f)
        .Set(SnAPI::UI::UITextInput::PaddingKey, 8.0f)
        .Set(SnAPI::UI::UITextInput::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UITextInput::PlaceholderColorKey, kTextMuted)
        .Set(SnAPI::UI::UITextInput::SelectionColorKey, kSelection)
        .Set(SnAPI::UI::UITextInput::CaretColorKey, Color{237, 245, 255, 255})
        .Set(SnAPI::UI::UITextInput::SpellErrorColorKey, kDanger)
        .Set(SnAPI::UI::UITextInput::TextAlignmentKey, SnAPI::UI::ETextAlignment::Start)
        .Set(SnAPI::UI::UITextInput::TextJustifyKey, SnAPI::UI::ETextJustify::None)
        .Hovered()
            .Set(SnAPI::UI::UITextInput::BorderColorKey, Color{90, 122, 170, 224})
        .Focused()
            .Set(SnAPI::UI::UITextInput::BorderColorKey, kBorderStrong);

    Define<SnAPI::UI::UISlider>()
        .Set(SnAPI::UI::UISlider::TrackColorKey, Color{24, 37, 55, 255})
        .Set(SnAPI::UI::UISlider::FillColorKey, kAccent)
        .Set(SnAPI::UI::UISlider::ThumbColorKey, Color{228, 239, 255, 255})
        .Set(SnAPI::UI::UISlider::LabelColorKey, kTextSecondary)
        .Set(SnAPI::UI::UISlider::ValueTextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UISlider::ValueBackgroundColorKey, Color{8, 15, 25, 246})
        .Set(SnAPI::UI::UISlider::BorderColorKey, kBorder);

    Define<SnAPI::UI::UICheckbox>()
        .Set(SnAPI::UI::UICheckbox::BoxColorKey, Color{17, 31, 45, 255})
        .Set(SnAPI::UI::UICheckbox::CheckColorKey, Color{132, 224, 151, 255})
        .Set(SnAPI::UI::UICheckbox::LabelColorKey, kTextSecondary)
        .Hovered()
            .Set(SnAPI::UI::UICheckbox::BoxColorKey, Color{27, 45, 62, 255})
        .Focused()
            .Set(SnAPI::UI::UICheckbox::LabelColorKey, kTextPrimary);

    Define<SnAPI::UI::UIRadio>()
        .Set(SnAPI::UI::UIRadio::OuterColorKey, Color{24, 37, 55, 255})
        .Set(SnAPI::UI::UIRadio::InnerColorKey, kAccent)
        .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextSecondary)
        .Hovered()
            .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextPrimary)
        .Focused()
            .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextPrimary);

    Define<SnAPI::UI::UIComboBox>()
        .Set(SnAPI::UI::UIComboBox::BackgroundColorKey, Color{9, 16, 26, 244})
        .Set(SnAPI::UI::UIComboBox::HoverColorKey, Color{18, 31, 46, 248})
        .Set(SnAPI::UI::UIComboBox::PressedColorKey, Color{13, 25, 37, 248})
        .Set(SnAPI::UI::UIComboBox::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIComboBox::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIComboBox::CornerRadiusKey, 5.0f)
        .Set(SnAPI::UI::UIComboBox::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UIComboBox::PlaceholderColorKey, kTextMuted)
        .Set(SnAPI::UI::UIComboBox::DisabledTextColorKey, kTextMuted)
        .Set(SnAPI::UI::UIComboBox::DropdownBackgroundColorKey, Color{8, 15, 24, 250})
        .Set(SnAPI::UI::UIComboBox::ItemHoverColorKey, Color{26, 41, 59, 248})
        .Set(SnAPI::UI::UIComboBox::ItemSelectedColorKey, Color{31, 57, 84, 248})
        .Set(SnAPI::UI::UIComboBox::ArrowColorKey, kTextSecondary)
        .Set(SnAPI::UI::UIComboBox::PaddingKey, 8.0f)
        .Set(SnAPI::UI::UIComboBox::RowHeightKey, 26.0f);

    Define<SnAPI::UI::UIAccordion>()
        .Set(SnAPI::UI::UIAccordion::PaddingKey, 0.0f)
        .Set(SnAPI::UI::UIAccordion::GapKey, 4.0f)
        .Set(SnAPI::UI::UIAccordion::HeaderHeightKey, 26.0f)
        .Set(SnAPI::UI::UIAccordion::HeaderPaddingKey, 8.0f)
        .Set(SnAPI::UI::UIAccordion::ContentPaddingKey, 6.0f)
        .Set(SnAPI::UI::UIAccordion::BackgroundColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIAccordion::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIAccordion::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIAccordion::CornerRadiusKey, 6.0f)
        .Set(SnAPI::UI::UIAccordion::HeaderColorKey, Color{20, 33, 48, 248})
        .Set(SnAPI::UI::UIAccordion::HeaderHoverColorKey, Color{28, 44, 63, 248})
        .Set(SnAPI::UI::UIAccordion::HeaderExpandedColorKey, Color{34, 56, 82, 248})
        .Set(SnAPI::UI::UIAccordion::HeaderTextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UIAccordion::HeaderTextExpandedColorKey, kTextPrimary)
        .Set(SnAPI::UI::UIAccordion::HeaderBorderColorKey, kBorder)
        .Set(SnAPI::UI::UIAccordion::HeaderBorderThicknessKey, 1.0f);

    DefineClass("editor.root")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, kWindowBg)
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 0.0f);

    DefineClass("editor.menu_bar")
        .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Horizontal)
        .Set(SnAPI::UI::UIPanel::PaddingKey, 8.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 14.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{11, 18, 30, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f);

    DefineClass("editor.toolbar")
        .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Horizontal)
        .Set(SnAPI::UI::UIPanel::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 8.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{12, 20, 33, 250})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f);

    DefineClass("editor.workspace")
        .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Horizontal)
        .Set(SnAPI::UI::UIPanel::PaddingKey, 8.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 8.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{8, 13, 22, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f);

    DefineClass("editor.sidebar")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 8.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, kSurfaceElevated)
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 6.0f);

    DefineClass("editor.center")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{11, 18, 29, 252})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorderStrong)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 8.0f);

    DefineClass("editor.content_browser")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 8.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{12, 20, 32, 252})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f);

    DefineClass("editor.panel_title")
        .Set(SnAPI::UI::UIText::TextColorKey, kAccent)
        .Set(SnAPI::UI::UIText::WrappingKey, SnAPI::UI::ETextWrapping::NoWrap)
        .Set(SnAPI::UI::UIText::TextAlignmentKey, SnAPI::UI::ETextAlignment::Start)
        .Set(SnAPI::UI::UIText::TextJustifyKey, SnAPI::UI::ETextJustify::None);

    DefineClass("editor.panel_subtitle")
        .Set(SnAPI::UI::UIText::TextColorKey, kTextMuted)
        .Set(SnAPI::UI::UIText::WrappingKey, SnAPI::UI::ETextWrapping::NoWrap);

    DefineClass("editor.menu_item")
        .Set(SnAPI::UI::UIText::TextColorKey, kTextSecondary)
        .Hovered()
            .Set(SnAPI::UI::UIText::TextColorKey, kTextPrimary);

    DefineClass("editor.toolbar_chip")
        .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Horizontal)
        .Set(SnAPI::UI::UIPanel::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 4.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{22, 32, 46, 250})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 5.0f)
        .Focused()
            .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorderStrong);

    DefineClass("editor.asset_card")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 3.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{18, 29, 43, 248})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 6.0f)
        .Hovered()
            .Set(SnAPI::UI::UIPanel::BorderColorKey, kAccentStrong);

    DefineClass("editor.asset_preview")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{32, 56, 82, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{78, 114, 158, 220})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 5.0f);

    DefineClass("editor.search")
        .Set(SnAPI::UI::UITextInput::BackgroundColorKey, Color{7, 13, 22, 246})
        .Set(SnAPI::UI::UITextInput::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UITextInput::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UITextInput::CornerRadiusKey, 4.0f)
        .Set(SnAPI::UI::UITextInput::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UITextInput::TextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UITextInput::PlaceholderColorKey, kTextMuted);

    DefineClass("editor.hierarchy_item")
        .Set(SnAPI::UI::UIRadio::OuterColorKey, Color{24, 37, 55, 255})
        .Set(SnAPI::UI::UIRadio::InnerColorKey, kAccent)
        .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextSecondary)
        .Hovered()
            .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextPrimary)
        .Focused()
            .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextPrimary);
}

} // namespace SnAPI::GameFramework::Editor

#else

namespace SnAPI::GameFramework::Editor
{

EditorTheme::EditorTheme()
    : Theme("SnAPI.Editor")
{
}

void EditorTheme::Initialize()
{
}

} // namespace SnAPI::GameFramework::Editor

#endif
