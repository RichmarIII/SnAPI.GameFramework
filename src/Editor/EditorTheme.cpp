#include "Editor/EditorTheme.h"

#if defined(SNAPI_GF_ENABLE_UI)

#include "UIPropertyPanel.h"

#include <UIAccordion.h>
#include <UIBadge.h>
#include <UIBreadcrumbs.h>
#include <UIButton.h>
#include <UICheckbox.h>
#include <UIColorPicker.h>
#include <UIComboBox.h>
#include <UIDatePicker.h>
#include <UIDockZone.h>
#include <UIListView.h>
#include <UIMenuBar.h>
#include <UINumberField.h>
#include <UIPanel.h>
#include <UIPagination.h>
#include <UIRadio.h>
#include <UIScrollContainer.h>
#include <UISlider.h>
#include <UISwitch.h>
#include <UITable.h>
#include <UITabs.h>
#include <UIText.h>
#include <UITextInput.h>
#include <UITokenField.h>
#include <UIToolbar.h>
#include <UITreeView.h>

namespace SnAPI::GameFramework::Editor
{
namespace
{
using SnAPI::UI::Color;

constexpr Color kWindowBg{12, 13, 16, 255};
constexpr Color kSurfaceBg{23, 24, 27, 255};
constexpr Color kSurfaceElevated{29, 31, 35, 255};
constexpr Color kSurfaceSoft{38, 40, 45, 255};
constexpr Color kBorder{74, 77, 84, 255};
constexpr Color kBorderStrong{132, 136, 145, 255};
constexpr Color kAccent{202, 168, 109, 255};
constexpr Color kAccentStrong{230, 206, 162, 255};
constexpr Color kTextPrimary{236, 238, 242, 255};
constexpr Color kTextSecondary{191, 196, 205, 255};
constexpr Color kTextMuted{134, 140, 150, 255};
constexpr Color kSelection{98, 92, 80, 192};
constexpr Color kDanger{214, 86, 86, 255};
constexpr Color kScrollTrack{17, 18, 21, 255};
constexpr Color kScrollThumb{84, 88, 95, 255};
constexpr Color kScrollThumbHover{118, 124, 136, 255};
constexpr Color kShadowSoft{0, 0, 0, 64};
constexpr Color kShadowMedium{0, 0, 0, 128};
constexpr Color kShadowStrong{0, 0, 0, 255};
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
        .Set(SnAPI::UI::UIPanel::UseGradientKey, false)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{39, 42, 49, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{24, 26, 30, 255})
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, kSurfaceBg)
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UIPanel::DropShadowBlurKey, 4.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetYKey, 2.0f);

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
        .Set(SnAPI::UI::UIButton::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UIButton::DropShadowBlurKey, 2.0f)
        .Set(SnAPI::UI::UIButton::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIButton::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIButton::DropShadowOffsetYKey, 1.0f)
        .Hovered()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color{49, 53, 61, 255})
        .Pressed()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color{34, 37, 43, 255})
        .Focused()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color{58, 63, 73, 255});

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
        .Set(SnAPI::UI::UIScrollContainer::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UIScrollContainer::DropShadowBlurKey, 4.0f)
        .Set(SnAPI::UI::UIScrollContainer::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIScrollContainer::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIScrollContainer::DropShadowOffsetYKey, 2.0f)
        .Set(SnAPI::UI::UIScrollContainer::ScrollbarTrackColorKey, kScrollTrack)
        .Set(SnAPI::UI::UIScrollContainer::ScrollbarThumbColorKey, kScrollThumb)
        .Set(SnAPI::UI::UIScrollContainer::ScrollbarThumbHoverColorKey, kScrollThumbHover);

    Define<SnAPI::UI::UITextInput>()
        .Set(SnAPI::UI::UITextInput::BackgroundColorKey, Color{22, 24, 29, 255})
        .Set(SnAPI::UI::UITextInput::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UITextInput::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UITextInput::CornerRadiusKey, 5.0f)
        .Set(SnAPI::UI::UITextInput::PaddingKey, 8.0f)
        .Set(SnAPI::UI::UITextInput::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UITextInput::PlaceholderColorKey, kTextMuted)
        .Set(SnAPI::UI::UITextInput::SelectionColorKey, kSelection)
        .Set(SnAPI::UI::UITextInput::CaretColorKey, Color{232, 234, 239, 255})
        .Set(SnAPI::UI::UITextInput::SpellErrorColorKey, kDanger)
        .Set(SnAPI::UI::UITextInput::TextAlignmentKey, SnAPI::UI::ETextAlignment::Start)
        .Set(SnAPI::UI::UITextInput::TextJustifyKey, SnAPI::UI::ETextJustify::None)
        .Set(SnAPI::UI::UITextInput::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UITextInput::DropShadowBlurKey, 2.0f)
        .Set(SnAPI::UI::UITextInput::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UITextInput::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UITextInput::DropShadowOffsetYKey, 1.0f)
        .Hovered()
            .Set(SnAPI::UI::UITextInput::BorderColorKey, Color{108, 115, 126, 255})
        .Focused()
            .Set(SnAPI::UI::UITextInput::BorderColorKey, kBorderStrong);

    Define<SnAPI::UI::UISlider>()
        .Set(SnAPI::UI::UISlider::TrackColorKey, Color{45, 49, 56, 255})
        .Set(SnAPI::UI::UISlider::FillColorKey, kAccent)
        .Set(SnAPI::UI::UISlider::ThumbColorKey, Color{232, 235, 240, 255})
        .Set(SnAPI::UI::UISlider::LabelColorKey, kTextSecondary)
        .Set(SnAPI::UI::UISlider::ValueTextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UISlider::ValueBackgroundColorKey, Color{22, 24, 29, 255})
        .Set(SnAPI::UI::UISlider::BorderColorKey, kBorder);

    Define<SnAPI::UI::UICheckbox>()
        .Set(SnAPI::UI::UICheckbox::BoxColorKey, Color{36, 40, 47, 255})
        .Set(SnAPI::UI::UICheckbox::CheckColorKey, kAccentStrong)
        .Set(SnAPI::UI::UICheckbox::LabelColorKey, kTextSecondary)
        .Set(SnAPI::UI::UICheckbox::BorderThicknessKey, 1.0f)
        .Hovered()
            .Set(SnAPI::UI::UICheckbox::BoxColorKey, Color{53, 58, 66, 255})
            .Set(SnAPI::UI::UICheckbox::LabelColorKey, kTextPrimary)
        .Pressed()
            .Set(SnAPI::UI::UICheckbox::BoxColorKey, Color{66, 57, 43, 255})
        .Focused()
            .Set(SnAPI::UI::UICheckbox::LabelColorKey, kTextPrimary);

    Define<SnAPI::UI::UIRadio>()
        .Set(SnAPI::UI::UIRadio::OuterColorKey, Color{42, 46, 53, 255})
        .Set(SnAPI::UI::UIRadio::InnerColorKey, kAccent)
        .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextSecondary)
        .Hovered()
            .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextPrimary)
        .Focused()
            .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextPrimary);

    Define<SnAPI::UI::UIComboBox>()
        .Set(SnAPI::UI::UIComboBox::BackgroundColorKey, Color{22, 24, 29, 255})
        .Set(SnAPI::UI::UIComboBox::HoverColorKey, Color{37, 40, 47, 255})
        .Set(SnAPI::UI::UIComboBox::PressedColorKey, Color{30, 33, 39, 255})
        .Set(SnAPI::UI::UIComboBox::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIComboBox::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIComboBox::CornerRadiusKey, 5.0f)
        .Set(SnAPI::UI::UIComboBox::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UIComboBox::PlaceholderColorKey, kTextMuted)
        .Set(SnAPI::UI::UIComboBox::DisabledTextColorKey, kTextMuted)
        .Set(SnAPI::UI::UIComboBox::DropdownBackgroundColorKey, Color{27, 29, 34, 255})
        .Set(SnAPI::UI::UIComboBox::ItemHoverColorKey, Color{43, 47, 55, 255})
        .Set(SnAPI::UI::UIComboBox::ItemSelectedColorKey, Color{58, 63, 73, 255})
        .Set(SnAPI::UI::UIComboBox::ArrowColorKey, kTextSecondary)
        .Set(SnAPI::UI::UIComboBox::PaddingKey, 8.0f)
        .Set(SnAPI::UI::UIComboBox::RowHeightKey, 26.0f)
        .Set(SnAPI::UI::UIComboBox::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UIComboBox::DropShadowBlurKey, 3.0f)
        .Set(SnAPI::UI::UIComboBox::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIComboBox::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIComboBox::DropShadowOffsetYKey, 2.0f);

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
        .Set(SnAPI::UI::UIAccordion::HeaderColorKey, Color{39, 42, 49, 255})
        .Set(SnAPI::UI::UIAccordion::HeaderHoverColorKey, Color{49, 54, 63, 255})
        .Set(SnAPI::UI::UIAccordion::HeaderExpandedColorKey, Color{58, 64, 74, 255})
        .Set(SnAPI::UI::UIAccordion::HeaderTextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UIAccordion::HeaderTextExpandedColorKey, kTextPrimary)
        .Set(SnAPI::UI::UIAccordion::HeaderBorderColorKey, kBorder)
        .Set(SnAPI::UI::UIAccordion::HeaderBorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIAccordion::ArrowSizeKey, 18.0f)
        .Set(SnAPI::UI::UIAccordion::ArrowGapKey, 6.0f)
        .Set(SnAPI::UI::UIAccordion::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UIAccordion::DropShadowBlurKey, 3.0f)
        .Set(SnAPI::UI::UIAccordion::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIAccordion::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIAccordion::DropShadowOffsetYKey, 2.0f);

    Define<SnAPI::UI::UIBadge>()
        .Set(SnAPI::UI::UIBadge::BackgroundKey, Color{56, 47, 33, 255})
        .Set(SnAPI::UI::UIBadge::BorderColorKey, Color{122, 102, 73, 255})
        .Set(SnAPI::UI::UIBadge::TextColorKey, Color{239, 228, 206, 255})
        .Set(SnAPI::UI::UIBadge::CornerRadiusKey, 10.0f)
        .Set(SnAPI::UI::UIBadge::HorizontalPaddingKey, 8.0f)
        .Set(SnAPI::UI::UIBadge::VerticalPaddingKey, 2.0f);

    Define<SnAPI::UI::UIMenuBar>()
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{22, 23, 26, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{68, 72, 79, 255})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{35, 37, 41, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{20, 21, 24, 255});

    Define<SnAPI::UI::UIToolbar>()
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{24, 26, 30, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{70, 74, 82, 255})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{36, 38, 43, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{22, 24, 28, 255});

    Define<SnAPI::UI::UITabs>()
        .Set(SnAPI::UI::UITabs::BackgroundColorKey, Color{17, 18, 22, 255})
        .Set(SnAPI::UI::UITabs::BorderColorKey, Color{76, 80, 88, 255})
        .Set(SnAPI::UI::UITabs::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UITabs::HeaderBackgroundColorKey, Color{27, 29, 34, 255})
        .Set(SnAPI::UI::UITabs::HeaderActiveColorKey, Color{67, 57, 42, 255})
        .Set(SnAPI::UI::UITabs::HeaderHoverColorKey, Color{44, 46, 52, 255})
        .Set(SnAPI::UI::UITabs::HeaderTextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UITabs::HeaderActiveTextColorKey, kAccentStrong)
        .Set(SnAPI::UI::UITabs::ContentBackgroundColorKey, Color{17, 18, 22, 255})
        .Set(SnAPI::UI::UITabs::DropShadowColorKey, kShadowMedium)
        .Set(SnAPI::UI::UITabs::DropShadowBlurKey, 5.0f)
        .Set(SnAPI::UI::UITabs::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UITabs::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UITabs::DropShadowOffsetYKey, 3.0f);

    Define<SnAPI::UI::UIDockZone>()
        .Set(SnAPI::UI::UIDockZone::SplitterThicknessKey, 6.0f)
        .Set(SnAPI::UI::UIDockZone::MinSplitRatioKey, 0.08f)
        .Set(SnAPI::UI::UIDockZone::MaxSplitRatioKey, 0.92f)
        .Set(SnAPI::UI::UIDockZone::SplitterColorKey, Color{54, 59, 67, 255})
        .Set(SnAPI::UI::UIDockZone::SplitterHoverColorKey, Color{126, 136, 150, 255})
        .Set(SnAPI::UI::UIDockZone::TabBgColorKey, Color{18, 20, 24, 255})
        .Set(SnAPI::UI::UIDockZone::TabActiveColorKey, Color{67, 57, 42, 255})
        .Set(SnAPI::UI::UIDockZone::TabHoverColorKey, Color{42, 45, 52, 255})
        .Set(SnAPI::UI::UIDockZone::TabTextColorKey, kTextSecondary);

    Define<SnAPI::UI::UIBreadcrumbs>()
        .Set(SnAPI::UI::UIBreadcrumbs::BackgroundColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIBreadcrumbs::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIBreadcrumbs::BorderThicknessKey, 0.0f)
        .Set(SnAPI::UI::UIBreadcrumbs::TextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UIBreadcrumbs::HoveredTextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UIBreadcrumbs::SeparatorColorKey, Color{120, 123, 130, 255});

    Define<SnAPI::UI::UIPagination>()
        .Set(SnAPI::UI::UIPagination::BackgroundColorKey, Color{20, 22, 26, 255})
        .Set(SnAPI::UI::UIPagination::ButtonColorKey, Color{40, 43, 50, 255})
        .Set(SnAPI::UI::UIPagination::ButtonHoverColorKey, Color{54, 58, 66, 255})
        .Set(SnAPI::UI::UIPagination::ButtonActiveColorKey, Color{74, 63, 46, 255})
        .Set(SnAPI::UI::UIPagination::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UIPagination::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIPagination::BorderThicknessKey, 1.0f);

    Define<SnAPI::UI::UIListView>()
        .Set(SnAPI::UI::UIListView::BackgroundColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIListView::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIListView::BorderThicknessKey, 0.0f)
        .Set(SnAPI::UI::UIListView::CornerRadiusKey, 0.0f);

    Define<SnAPI::UI::UITreeView>()
        .Set(SnAPI::UI::UITreeView::BackgroundColorKey, Color{17, 19, 23, 255})
        .Set(SnAPI::UI::UITreeView::BorderColorKey, Color{72, 76, 84, 255})
        .Set(SnAPI::UI::UITreeView::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UITreeView::CornerRadiusKey, 4.0f)
        .Set(SnAPI::UI::UITreeView::TextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UITreeView::RowHoverColorKey, Color{42, 44, 50, 255})
        .Set(SnAPI::UI::UITreeView::RowSelectedColorKey, Color{71, 60, 43, 255})
        .Set(SnAPI::UI::UITreeView::ExpanderColorKey, Color{164, 168, 176, 255})
        .Set(SnAPI::UI::UITreeView::IconSizeKey, 14.0f)
        .Set(SnAPI::UI::UITreeView::IconGapKey, 6.0f);

    Define<SnAPI::UI::UITable>()
        .Set(SnAPI::UI::UITable::BackgroundColorKey, Color{20, 22, 27, 255})
        .Set(SnAPI::UI::UITable::HeaderBackgroundColorKey, Color{35, 38, 45, 255})
        .Set(SnAPI::UI::UITable::HeaderTextColorKey, kAccentStrong)
        .Set(SnAPI::UI::UITable::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UITable::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UITable::CornerRadiusKey, 4.0f)
        .Set(SnAPI::UI::UITable::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UITable::DropShadowBlurKey, 4.0f)
        .Set(SnAPI::UI::UITable::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UITable::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UITable::DropShadowOffsetYKey, 2.0f);

    Define<SnAPI::UI::UIDatePicker>()
        .Set(SnAPI::UI::UIDatePicker::BackgroundColorKey, Color{22, 24, 29, 255})
        .Set(SnAPI::UI::UIDatePicker::HoverColorKey, Color{40, 44, 51, 255})
        .Set(SnAPI::UI::UIDatePicker::PressedColorKey, Color{64, 55, 41, 255})
        .Set(SnAPI::UI::UIDatePicker::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIDatePicker::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UIDatePicker::ArrowColorKey, kAccentStrong)
        .Set(SnAPI::UI::UIDatePicker::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UIDatePicker::DropShadowBlurKey, 2.0f)
        .Set(SnAPI::UI::UIDatePicker::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIDatePicker::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIDatePicker::DropShadowOffsetYKey, 1.0f);

    Define<SnAPI::UI::UIColorPicker>()
        .Set(SnAPI::UI::UIColorPicker::BackgroundColorKey, Color{20, 22, 27, 255})
        .Set(SnAPI::UI::UIColorPicker::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIColorPicker::PreviewBorderColorKey, Color{132, 136, 144, 255})
        .Set(SnAPI::UI::UIColorPicker::SelectionOutlineColorKey, kAccentStrong)
        .Set(SnAPI::UI::UIColorPicker::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UIColorPicker::DropShadowBlurKey, 2.0f)
        .Set(SnAPI::UI::UIColorPicker::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIColorPicker::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIColorPicker::DropShadowOffsetYKey, 1.0f);

    Define<SnAPI::UI::UINumberField>()
        .Set(SnAPI::UI::UINumberField::BackgroundColorKey, Color{22, 24, 29, 255})
        .Set(SnAPI::UI::UINumberField::HoverColorKey, Color{36, 39, 46, 255})
        .Set(SnAPI::UI::UINumberField::PressedColorKey, Color{58, 50, 38, 255})
        .Set(SnAPI::UI::UINumberField::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UINumberField::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UINumberField::SpinButtonColorKey, Color{39, 42, 49, 255})
        .Set(SnAPI::UI::UINumberField::SpinButtonHoverColorKey, Color{62, 54, 40, 255})
        .Set(SnAPI::UI::UINumberField::CaretColorKey, kAccentStrong)
        .Set(SnAPI::UI::UINumberField::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UINumberField::DropShadowBlurKey, 2.0f)
        .Set(SnAPI::UI::UINumberField::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UINumberField::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UINumberField::DropShadowOffsetYKey, 1.0f);

    Define<SnAPI::UI::UITokenField>()
        .Set(SnAPI::UI::UITokenField::BackgroundColorKey, Color{20, 22, 27, 255})
        .Set(SnAPI::UI::UITokenField::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UITokenField::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UITokenField::PlaceholderColorKey, kTextMuted)
        .Set(SnAPI::UI::UITokenField::TokenBackgroundColorKey, Color{56, 47, 33, 255})
        .Set(SnAPI::UI::UITokenField::TokenBorderColorKey, Color{118, 100, 74, 255})
        .Set(SnAPI::UI::UITokenField::TokenTextColorKey, kAccentStrong)
        .Set(SnAPI::UI::UITokenField::RemoveGlyphColorKey, Color{219, 206, 184, 255})
        .Set(SnAPI::UI::UITokenField::CaretColorKey, kAccentStrong)
        .Set(SnAPI::UI::UITokenField::DropShadowColorKey, kShadowSoft)
        .Set(SnAPI::UI::UITokenField::DropShadowBlurKey, 2.0f)
        .Set(SnAPI::UI::UITokenField::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UITokenField::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UITokenField::DropShadowOffsetYKey, 1.0f);

    DefineClass("editor.root")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, false)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{12, 13, 16, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{12, 13, 16, 255})
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, kWindowBg)
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kWindowBg)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, Color::Transparent());

    DefineClass("editor.menu_bar")
        .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Horizontal)
        .Set(SnAPI::UI::UIPanel::PaddingKey, 7.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 9.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{34, 36, 41, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{20, 21, 25, 255})
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{24, 25, 29, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{72, 76, 84, 255})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, kShadowStrong)
        .Set(SnAPI::UI::UIPanel::DropShadowBlurKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetYKey, 2.0f);

    DefineClass("editor.toolbar")
        .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Horizontal)
        .Set(SnAPI::UI::UIPanel::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 5.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{36, 38, 43, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{22, 24, 29, 255})
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{25, 27, 32, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{70, 74, 82, 255})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, kShadowStrong)
        .Set(SnAPI::UI::UIPanel::DropShadowBlurKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetYKey, 2.0f);

    DefineClass("editor.workspace")
        .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Horizontal)
        .Set(SnAPI::UI::UIPanel::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{17, 18, 22, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{11, 12, 15, 255})
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{13, 14, 17, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, Color::Transparent());

    DefineClass("editor.sidebar")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 8.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{34, 36, 41, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{24, 26, 31, 255})
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, kSurfaceElevated)
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{74, 77, 84, 255})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, kShadowStrong)
        .Set(SnAPI::UI::UIPanel::DropShadowBlurKey, 7.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetYKey, 3.0f);

    DefineClass("editor.center")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, false)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{79, 82, 90, 255})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, Color::Transparent());

    DefineClass("editor.content_browser")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 4.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{35, 37, 42, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{23, 24, 29, 255})
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{31, 34, 40, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{74, 77, 84, 255})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, kShadowMedium)
        .Set(SnAPI::UI::UIPanel::DropShadowBlurKey, 5.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetYKey, 2.0f);

    DefineClass("editor.content_header")
        .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Horizontal)
        .Set(SnAPI::UI::UIPanel::PaddingKey, 3.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 5.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{40, 43, 49, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{30, 32, 37, 255})
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{34, 37, 43, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{79, 82, 90, 255})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, kShadowMedium)
        .Set(SnAPI::UI::UIPanel::DropShadowBlurKey, 4.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetYKey, 2.0f);

    DefineClass("editor.panel_title")
        .Set(SnAPI::UI::UIText::TextColorKey, Color{229, 232, 238, 255})
        .Set(SnAPI::UI::UIText::WrappingKey, SnAPI::UI::ETextWrapping::NoWrap)
        .Set(SnAPI::UI::UIText::TextAlignmentKey, SnAPI::UI::ETextAlignment::Start)
        .Set(SnAPI::UI::UIText::TextJustifyKey, SnAPI::UI::ETextJustify::None);

    DefineClass("editor.panel_subtitle")
        .Set(SnAPI::UI::UIText::TextColorKey, Color{160, 167, 178, 255})
        .Set(SnAPI::UI::UIText::WrappingKey, SnAPI::UI::ETextWrapping::NoWrap);

    DefineClass("editor.menu_item")
        .Set(SnAPI::UI::UIText::TextColorKey, kTextSecondary)
        .Hovered()
            .Set(SnAPI::UI::UIText::TextColorKey, kTextPrimary);

    DefineClass("editor.brand_title")
        .Set(SnAPI::UI::UIText::TextColorKey, kTextPrimary);

    DefineClass("editor.brand_subtitle")
        .Set(SnAPI::UI::UIText::TextColorKey, kTextMuted);

    DefineClass("editor.menu_button")
        .Set(SnAPI::UI::UIButton::BackgroundKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIButton::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIButton::BorderThicknessKey, 0.0f)
        .Set(SnAPI::UI::UIButton::CornerRadiusKey, 4.0f)
        .Hovered()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color{46, 49, 56, 255})
        .Pressed()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color{58, 50, 38, 255});

    DefineClass("editor.menu_button_text")
        .Set(SnAPI::UI::UIText::TextColorKey, Color{224, 228, 235, 255})
        .Hovered()
            .Set(SnAPI::UI::UIText::TextColorKey, Color{245, 247, 251, 255});

    DefineClass("editor.menu_switch")
        .Set(SnAPI::UI::UISwitch::TrackOffColorKey, Color{64, 69, 77, 255})
        .Set(SnAPI::UI::UISwitch::TrackOnColorKey, Color{86, 148, 102, 255})
        .Set(SnAPI::UI::UISwitch::ThumbColorKey, Color{246, 248, 251, 255})
        .Set(SnAPI::UI::UISwitch::BorderColorKey, Color{28, 31, 38, 255})
        .Set(SnAPI::UI::UISwitch::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UISwitch::TrackPaddingKey, 2.0f)
        .Set(SnAPI::UI::UISwitch::CornerRadiusKey, 11.0f);

    DefineClass("editor.toolbar_button")
        .Set(SnAPI::UI::UIButton::BackgroundKey, Color{38, 40, 46, 255})
        .Set(SnAPI::UI::UIButton::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIButton::BorderThicknessKey, 0.0f)
        .Set(SnAPI::UI::UIButton::CornerRadiusKey, 4.0f)
        .Hovered()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color{50, 54, 62, 255})
        .Pressed()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color{68, 58, 42, 255});

    DefineClass("editor.toolbar_button_text")
        .Set(SnAPI::UI::UIText::TextColorKey, Color{218, 223, 232, 255});

    DefineClass("editor.status_badge")
        .Set(SnAPI::UI::UIBadge::BackgroundKey, Color{56, 47, 33, 255})
        .Set(SnAPI::UI::UIBadge::BorderColorKey, Color{122, 102, 73, 255})
        .Set(SnAPI::UI::UIBadge::TextColorKey, Color{239, 228, 206, 255})
        .Set(SnAPI::UI::UIBadge::VerticalPaddingKey, 4.0f);

    DefineClass("editor.modes_breadcrumb")
        .Set(SnAPI::UI::UIBreadcrumbs::BackgroundColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIBreadcrumbs::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIBreadcrumbs::TextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UIBreadcrumbs::HoveredTextColorKey, kTextPrimary);

    DefineClass("editor.toolbar_chip")
        .Set(SnAPI::UI::UIPanel::DirectionKey, SnAPI::UI::ELayoutDirection::Horizontal)
        .Set(SnAPI::UI::UIPanel::PaddingKey, 4.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 4.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{56, 61, 70, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{34, 37, 43, 255})
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{40, 44, 51, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{85, 92, 104, 255})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 4.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, kShadowMedium)
        .Set(SnAPI::UI::UIPanel::DropShadowBlurKey, 4.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetYKey, 2.0f)
        .Hovered()
            .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{51, 56, 64, 255})
            .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{112, 121, 135, 255})
        .Focused()
            .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorderStrong)
        .Pressed()
            .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{31, 34, 40, 255});

    DefineClass("editor.asset_tile_button")
        .Set(SnAPI::UI::UIButton::BackgroundKey, Color::Transparent())
        .Set(SnAPI::UI::UIButton::BorderColorKey, Color{102, 89, 66, 84})
        .Set(SnAPI::UI::UIButton::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIButton::DropShadowColorKey, Color::Transparent())
        .Set(SnAPI::UI::UIButton::DropShadowBlurKey, 0.0f)
        .Set(SnAPI::UI::UIButton::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIButton::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIButton::DropShadowOffsetYKey, 0.0f)
        .Set(SnAPI::UI::UIButton::CornerRadiusKey, 5.0f)
        .Hovered()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color::Transparent())
            .Set(SnAPI::UI::UIButton::BorderColorKey, Color{156, 136, 102, 196})
        .Focused()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color::Transparent())
            .Set(SnAPI::UI::UIButton::BorderColorKey, Color{168, 146, 112, 255})
        .Pressed()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color::Transparent())
            .Set(SnAPI::UI::UIButton::BorderColorKey, Color{185, 163, 126, 228})
        .Disabled()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color::Transparent());

    DefineClass("editor.asset_tile_button_selected")
        .Set(SnAPI::UI::UIButton::BackgroundKey, Color::Transparent())
        .Set(SnAPI::UI::UIButton::BorderColorKey, Color{168, 146, 112, 255})
        .Set(SnAPI::UI::UIButton::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIButton::DropShadowColorKey, Color::Transparent())
        .Set(SnAPI::UI::UIButton::DropShadowBlurKey, 0.0f)
        .Set(SnAPI::UI::UIButton::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIButton::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIButton::DropShadowOffsetYKey, 0.0f)
        .Set(SnAPI::UI::UIButton::CornerRadiusKey, 5.0f)
        .Hovered()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color::Transparent())
            .Set(SnAPI::UI::UIButton::BorderColorKey, Color{205, 182, 144, 255})
        .Focused()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color::Transparent())
            .Set(SnAPI::UI::UIButton::BorderColorKey, Color{205, 182, 144, 255})
        .Pressed()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color::Transparent())
            .Set(SnAPI::UI::UIButton::BorderColorKey, Color{223, 202, 166, 255})
        .Disabled()
            .Set(SnAPI::UI::UIButton::BackgroundKey, Color::Transparent());

    DefineClass("editor.checkbox")
        .Set(SnAPI::UI::UICheckbox::BoxColorKey, Color{20, 24, 31, 252})
        .Set(SnAPI::UI::UICheckbox::CheckColorKey, Color{220, 202, 168, 255})
        .Set(SnAPI::UI::UICheckbox::LabelColorKey, kTextSecondary)
        .Set(SnAPI::UI::UICheckbox::BorderThicknessKey, 1.0f)
        .Hovered()
            .Set(SnAPI::UI::UICheckbox::BoxColorKey, Color{30, 35, 44, 252})
            .Set(SnAPI::UI::UICheckbox::LabelColorKey, kTextPrimary)
        .Pressed()
            .Set(SnAPI::UI::UICheckbox::BoxColorKey, Color{54, 47, 36, 252})
        .Focused()
            .Set(SnAPI::UI::UICheckbox::LabelColorKey, kTextPrimary);

    DefineClass("editor.asset_card")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 3.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{47, 52, 60, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{30, 33, 39, 255})
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{36, 40, 46, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{86, 94, 106, 255})
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 5.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, kShadowStrong)
        .Set(SnAPI::UI::UIPanel::DropShadowBlurKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetYKey, 3.0f)
        .Hovered()
            .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{60, 66, 76, 255})
            .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{39, 43, 50, 255})
            .Set(SnAPI::UI::UIPanel::BorderColorKey, Color{123, 133, 148, 255})
        .Focused()
            .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorderStrong);

    DefineClass("editor.asset_preview")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, false)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color::Transparent())
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color::Transparent())
        .Set(SnAPI::UI::UIPanel::GradientStartXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientStartYKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndXKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::GradientEndYKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color::Transparent())
        .Set(SnAPI::UI::UIPanel::BorderColorKey, Color::Transparent())
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 5.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, Color::Transparent())
        .Set(SnAPI::UI::UIPanel::DropShadowBlurKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetYKey, 0.0f);

    DefineClass("editor.section_card")
        .Set(SnAPI::UI::UIPanel::PaddingKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::GapKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::UseGradientKey, true)
        .Set(SnAPI::UI::UIPanel::GradientStartColorKey, Color{34, 36, 41, 255})
        .Set(SnAPI::UI::UIPanel::GradientEndColorKey, Color{23, 24, 28, 255})
        .Set(SnAPI::UI::UIPanel::BackgroundKey, Color{26, 28, 33, 255})
        .Set(SnAPI::UI::UIPanel::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIPanel::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UIPanel::CornerRadiusKey, 4.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowColorKey, kShadowStrong)
        .Set(SnAPI::UI::UIPanel::DropShadowBlurKey, 6.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowSpreadKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetXKey, 0.0f)
        .Set(SnAPI::UI::UIPanel::DropShadowOffsetYKey, 3.0f);

    DefineClass("editor.browser_path")
        .Set(SnAPI::UI::UIBreadcrumbs::BackgroundColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIBreadcrumbs::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIBreadcrumbs::TextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UIBreadcrumbs::HoveredTextColorKey, kTextPrimary);

    DefineClass("editor.browser_tabs")
        .Set(SnAPI::UI::UITabs::HeaderBackgroundColorKey, Color{30, 32, 37, 255})
        .Set(SnAPI::UI::UITabs::HeaderActiveColorKey, Color{67, 57, 42, 255})
        .Set(SnAPI::UI::UITabs::HeaderHoverColorKey, Color{44, 47, 54, 255})
        .Set(SnAPI::UI::UITabs::HeaderTextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UITabs::HeaderActiveTextColorKey, kAccentStrong)
        .Set(SnAPI::UI::UITabs::ContentBackgroundColorKey, Color{18, 19, 23, 255});

    DefineClass("editor.browser_list")
        .Set(SnAPI::UI::UIListView::BackgroundColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIListView::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIListView::BorderThicknessKey, 0.0f);

    DefineClass("editor.browser_pagination")
        .Set(SnAPI::UI::UIPagination::BackgroundColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIPagination::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIPagination::ButtonActiveColorKey, Color{67, 57, 42, 255})
        .Set(SnAPI::UI::UIPagination::ButtonColorKey, Color{40, 43, 50, 255})
        .Set(SnAPI::UI::UIPagination::ButtonHoverColorKey, Color{56, 60, 69, 255});

    DefineClass("editor.browser_table")
        .Set(SnAPI::UI::UITable::HeaderBackgroundColorKey, Color{36, 39, 46, 255})
        .Set(SnAPI::UI::UITable::HeaderTextColorKey, kAccentStrong)
        .Set(SnAPI::UI::UITable::BackgroundColorKey, Color{22, 24, 29, 255})
        .Set(SnAPI::UI::UITable::BorderColorKey, kBorder);

    DefineClass("editor.tree")
        .Set(SnAPI::UI::UITreeView::BackgroundColorKey, Color{16, 18, 22, 255})
        .Set(SnAPI::UI::UITreeView::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UITreeView::TextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UITreeView::RowHoverColorKey, Color{43, 46, 53, 255})
        .Set(SnAPI::UI::UITreeView::RowSelectedColorKey, Color{67, 57, 42, 255});

    DefineClass("editor.viewport_tabs")
        .Set(SnAPI::UI::UITabs::HeaderBackgroundColorKey, Color{27, 29, 34, 255})
        .Set(SnAPI::UI::UITabs::HeaderActiveColorKey, Color{67, 57, 42, 255})
        .Set(SnAPI::UI::UITabs::HeaderHoverColorKey, Color{43, 45, 52, 255})
        .Set(SnAPI::UI::UITabs::HeaderTextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UITabs::HeaderActiveTextColorKey, kAccentStrong)
        .Set(SnAPI::UI::UITabs::ContentBackgroundColorKey, Color{18, 19, 23, 255});

    DefineClass("editor.viewport_breadcrumb")
        .Set(SnAPI::UI::UIBreadcrumbs::BackgroundColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIBreadcrumbs::BorderColorKey, Color{0, 0, 0, 0})
        .Set(SnAPI::UI::UIBreadcrumbs::TextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UIBreadcrumbs::HoveredTextColorKey, kTextPrimary);

    DefineClass("editor.number_field")
        .Set(SnAPI::UI::UINumberField::BackgroundColorKey, Color{21, 23, 28, 255})
        .Set(SnAPI::UI::UINumberField::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UINumberField::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UINumberField::PaddingKey, 5.0f)
        .Set(SnAPI::UI::UINumberField::SpinButtonColorKey, Color{42, 45, 52, 255})
        .Set(SnAPI::UI::UINumberField::SpinButtonHoverColorKey, Color{68, 58, 42, 255});

    DefineClass("editor.date_picker")
        .Set(SnAPI::UI::UIDatePicker::BackgroundColorKey, Color{21, 23, 28, 255})
        .Set(SnAPI::UI::UIDatePicker::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIDatePicker::TextColorKey, kTextPrimary)
        .Set(SnAPI::UI::UIDatePicker::ArrowColorKey, kAccentStrong);

    DefineClass("editor.color_picker")
        .Set(SnAPI::UI::UIColorPicker::BackgroundColorKey, Color{21, 23, 28, 255})
        .Set(SnAPI::UI::UIColorPicker::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UIColorPicker::SelectionOutlineColorKey, kAccentStrong);

    DefineClass("editor.token_field")
        .Set(SnAPI::UI::UITokenField::BackgroundColorKey, Color{21, 23, 28, 255})
        .Set(SnAPI::UI::UITokenField::BorderColorKey, kBorder)
        .Set(SnAPI::UI::UITokenField::TokenBackgroundColorKey, Color{56, 47, 33, 255})
        .Set(SnAPI::UI::UITokenField::TokenTextColorKey, kAccentStrong);

    DefineClass("editor.tools_table")
        .Set(SnAPI::UI::UITable::BackgroundColorKey, Color{21, 23, 28, 255})
        .Set(SnAPI::UI::UITable::HeaderBackgroundColorKey, Color{35, 37, 43, 255})
        .Set(SnAPI::UI::UITable::HeaderTextColorKey, kAccentStrong)
        .Set(SnAPI::UI::UITable::BorderColorKey, kBorder);

    DefineClass("editor.search")
        .Set(SnAPI::UI::UITextInput::BackgroundColorKey, Color{22, 24, 29, 255})
        .Set(SnAPI::UI::UITextInput::BorderColorKey, Color{78, 82, 90, 255})
        .Set(SnAPI::UI::UITextInput::BorderThicknessKey, 1.0f)
        .Set(SnAPI::UI::UITextInput::CornerRadiusKey, 4.0f)
        .Set(SnAPI::UI::UITextInput::PaddingKey, 5.0f)
        .Set(SnAPI::UI::UITextInput::TextColorKey, kTextSecondary)
        .Set(SnAPI::UI::UITextInput::PlaceholderColorKey, kTextMuted)
        .Focused()
            .Set(SnAPI::UI::UITextInput::BorderColorKey, kBorderStrong);

    DefineClass("editor.hierarchy_item")
        .Set(SnAPI::UI::UIRadio::OuterColorKey, Color{42, 46, 53, 255})
        .Set(SnAPI::UI::UIRadio::InnerColorKey, kAccent)
        .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextSecondary)
        .Hovered()
            .Set(SnAPI::UI::UIRadio::OuterColorKey, Color{53, 58, 66, 255})
            .Set(SnAPI::UI::UIRadio::LabelColorKey, kTextPrimary)
        .Focused()
            .Set(SnAPI::UI::UIRadio::OuterColorKey, Color{62, 68, 78, 255})
            .Set(SnAPI::UI::UIRadio::LabelColorKey, kAccentStrong);
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
