#include "Editor/EditorLayout.h"

#include "AuthoredAssetRegistry.h"
#include "AssetPipelineIds.h"
#include "BaseNode.h"
#include "CameraComponent.h"
#include "Editor/EditorSelectionModel.h"
#include "Conduit/Editor/GraphCanvas.h"
#include "GameRuntime.h"
#include "BaseComponent.h"
#include "Level.h"
#include "NodeCast.h"
#include "PathResolver.h"
#include "PawnBase.h"
#include "PlayerStart.h"
#include "RenderAssetImportSettings.h"
#include "RendererSystem.h"
#include "Serialization.h"
#include "StaticTypeId.h"
#include "TypeAutoRegistry.h"
#include "TypeRegistry.h"
#include "UIPropertyPanel.h"
#include "UIRenderViewport.h"
#include "UISystem.h"
#include "WorldRenderSettings.h"
#include "World.h"

#include <UIContext.h>
#include <UICheckbox.h>
#include <UIComboBox.h>
#include <UIColorPicker.h>
#include <UIDatePicker.h>
#include <UIDockZone.h>
#include <UIElementBase.h>
#include <UIFilesystemPicker.h>
#include <UIImage.h>
#include <UIListView.h>
#include <UIMenuBar.h>
#include <UIModal.h>
#include <UINumberField.h>
#include <UIPanel.h>
#include <UIPagination.h>
#include <UIScrollContainer.h>
#include <UISizing.h>
#include <UISwitch.h>
#include <UITable.h>
#include <UITabs.h>
#include <UITokenField.h>
#include <UIToolbar.h>
#include <UIText.h>
#include <UITextInput.h>
#include <UITreeView.h>
#include <UIBadge.h>
#include <UIBreadcrumbs.h>
#include <UIButton.h>
#include <UIContextMenu.h>
#include <TextureCompressorImportSettings.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "CameraBase.hpp"

namespace SnAPI::GameFramework::Editor
{
namespace
{
class VectorTreeItemSource final : public SnAPI::UI::ITreeItemSource
{
public:
    void SetItems(std::vector<SnAPI::UI::UITreeItem> Items)
    {
        m_items = std::move(Items);
    }

    [[nodiscard]] uint32_t ItemCount() const override
    {
        return static_cast<uint32_t>(m_items.size());
    }

    [[nodiscard]] bool TryGetItem(const uint32_t Index, SnAPI::UI::UITreeItem& OutItem) const override
    {
        if (Index >= m_items.size())
        {
            return false;
        }

        OutItem = m_items[Index];
        return true;
    }

private:
    std::vector<SnAPI::UI::UITreeItem> m_items{};
};

constexpr std::string_view kBrandIconPath = "editor://Assets/component.svg";
constexpr std::string_view kHierarchyIconPath = "editor://Assets/hierarchy-circle.svg";
constexpr std::string_view kHierarchyWorldIconPath = "editor://Assets/world.svg";
constexpr std::string_view kHierarchyLevelIconPath = "editor://Assets/level.svg";
constexpr std::string_view kHierarchyNodeIconPath = "editor://Assets/component.svg";
constexpr std::string_view kSearchIconPath = "editor://Assets/options-vertical.svg";
constexpr std::string_view kGameViewIconPath = "editor://Assets/box.svg";
constexpr std::string_view kInspectorIconPath = "editor://Assets/settings.svg";
constexpr std::string_view kContentBrowserIconPath = "editor://Assets/folder-open.svg";
constexpr std::string_view kRescanIconPath = "editor://Assets/folder.svg";
constexpr std::string_view kFolderCardIconPath = "editor://Assets/folder.svg";
constexpr std::string_view kPlaceIconPath = "editor://Assets/sphere.svg";
constexpr std::string_view kSaveIconPath = "editor://Assets/cylinder.svg";
constexpr std::string_view kProjectWelcomeOpenIconPath = "editor://Assets/folder-open.svg";
constexpr std::string_view kProjectWelcomeCreateIconPath = "editor://Assets/level.svg";
constexpr std::string_view kProjectWelcomeRecentIconPath = "editor://Assets/folder.svg";
constexpr std::string_view kProjectWelcomeFooterIconPath = "editor://Assets/world.svg";
constexpr std::string_view kProjectSettingsIconPath = "editor://Assets/settings.svg";
constexpr std::string_view kDefaultProjectConfigFileName = "project.snproj.json";
constexpr int kDefaultSvgRasterSize = 256;
constexpr float kEditorIconScale = 2.0f;
constexpr SnAPI::UI::Color kIconWhite = SnAPI::UI::Color::RGB(255, 255, 255);
constexpr SnAPI::UI::Color kIconPlayGreen = SnAPI::UI::Color::RGB(73, 199, 112);
constexpr std::size_t kMaxRecentProjects = 8;
constexpr float kConduitSidebarSplitRatio = 0.29f;
constexpr float kConduitSidebarUpperSplitRatio = 0.48f;
constexpr float kConduitCanvasSplitRatio = 0.68f;
constexpr float kConduitClassSplitRatio = 0.46f;

struct ToolbarActionSpec
{
    EditorLayout::EToolbarAction Action = EditorLayout::EToolbarAction::Play;
    std::string_view IconPath;
    SnAPI::UI::Color Tint = kIconWhite;
};

constexpr std::array<std::string_view, 6> kMenuItems{
    "File", "Edit", "Assets", "Tools", "Window", "Help"};
constexpr std::array<ToolbarActionSpec, 4> kToolbarActions{{
    {EditorLayout::EToolbarAction::Play, "editor://Assets/play.svg", kIconPlayGreen},
    {EditorLayout::EToolbarAction::Pause, "editor://Assets/pause.svg", kIconWhite},
    {EditorLayout::EToolbarAction::Stop, "editor://Assets/stop.svg", kIconWhite},
    {EditorLayout::EToolbarAction::JoinLocalPlayer2, "editor://Assets/world.svg", SnAPI::UI::Color::RGB(112, 169, 255)},
}};
constexpr std::array<std::string_view, 3> kViewportModes{
    "Perspective", "Lit", "Shaded"};

[[nodiscard]] constexpr int32_t GizmoSpaceToIndex(const EditorLayout::EGizmoSpace Space)
{
    switch (Space)
    {
    case EditorLayout::EGizmoSpace::World:
        return 0;
    case EditorLayout::EGizmoSpace::Object:
        return 1;
    case EditorLayout::EGizmoSpace::Camera:
        return 2;
    default:
        return 0;
    }
}

[[nodiscard]] constexpr EditorLayout::EGizmoSpace GizmoSpaceFromIndex(const int32_t Index)
{
    switch (Index)
    {
    case 1:
        return EditorLayout::EGizmoSpace::Object;
    case 2:
        return EditorLayout::EGizmoSpace::Camera;
    case 0:
    default:
        return EditorLayout::EGizmoSpace::World;
    }
}

[[nodiscard]] constexpr int32_t SnapModeToIndex(const EditorLayout::ESnapMode Mode)
{
    switch (Mode)
    {
    case EditorLayout::ESnapMode::On:
        return 1;
    case EditorLayout::ESnapMode::Off:
    default:
        return 0;
    }
}

[[nodiscard]] constexpr EditorLayout::ESnapMode SnapModeFromIndex(const int32_t Index)
{
    return Index == 1 ? EditorLayout::ESnapMode::On : EditorLayout::ESnapMode::Off;
}

[[nodiscard]] double SanitizePositiveStep(const double Value, const double Fallback)
{
    if (!std::isfinite(Value) || Value <= 0.0)
    {
        return std::max(0.0001, Fallback);
    }
    return std::max(0.0001, Value);
}

[[nodiscard]] std::string ResolveUIImageSource(std::string_view Source)
{
    if (Source.empty())
    {
        return {};
    }

    if (auto Resolved = SPathResolver::Instance().Resolve(Source); Resolved)
    {
        return Resolved->generic_string();
    }

    return std::string(Source);
}

constexpr float kMainAreaSplitRatio = 0.68f;
constexpr float kWorkspaceLeftSplitRatio = 0.23f;
constexpr float kWorkspaceCenterSplitRatio = 0.74f;
constexpr float kDefaultModalScreenRatio = 0.50f;
constexpr float kModalRequestedSizePixels = 10000.0f;
constexpr float kToolbarActionIconDisplaySize = 24.0f;
constexpr float kToolbarActionButtonSize = 80.0f;

constexpr auto kVmInvalidationDebugEnabledKey =
    SnAPI::UI::MakePropertyKey<bool>("EditorLayout.InvalidationDebugEnabled");
constexpr auto kVmInvalidationDebugLabelTextKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.InvalidationDebugLabelText");
constexpr auto kVmInvalidationDebugLabelColorKey =
    SnAPI::UI::MakePropertyKey<SnAPI::UI::Color>("EditorLayout.InvalidationDebugLabelColor");
constexpr auto kVmHierarchyFilterTextKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.HierarchyFilterText");
constexpr auto kVmHierarchyCountTextKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.HierarchyCountText");
constexpr auto kVmContentFilterTextKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.ContentFilterText");
constexpr auto kVmSelectedContentAssetKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.SelectedContentAssetKey");
constexpr auto kVmContentAssetNameKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.ContentAssetName");
constexpr auto kVmContentAssetTypeKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.ContentAssetType");
constexpr auto kVmContentAssetVariantKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.ContentAssetVariant");
constexpr auto kVmContentAssetIdKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.ContentAssetId");
constexpr auto kVmContentAssetStatusKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.ContentAssetStatus");
constexpr auto kVmContentAssetCanPlaceKey =
    SnAPI::UI::MakePropertyKey<bool>("EditorLayout.ContentAssetCanPlace");
constexpr auto kVmContentAssetCanSaveKey =
    SnAPI::UI::MakePropertyKey<bool>("EditorLayout.ContentAssetCanSave");
constexpr auto kVmContentCreateTypeFilterKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.ContentCreateTypeFilter");
constexpr auto kVmContentCreateAssetNameKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.ContentCreateAssetName");
constexpr auto kHierarchyRowIconIdKey =
    SnAPI::UI::MakePropertyKey<SnAPI::UI::ElementId>("EditorLayout.HierarchyRow.IconId");
constexpr auto kHierarchyRowTextIdKey =
    SnAPI::UI::MakePropertyKey<SnAPI::UI::ElementId>("EditorLayout.HierarchyRow.TextId");
constexpr auto kHierarchyRowLastIconSourceKey =
    SnAPI::UI::MakePropertyKey<std::string>("EditorLayout.HierarchyRow.LastIconSource");
constexpr auto kHierarchyRowLastIconTintKey =
    SnAPI::UI::MakePropertyKey<SnAPI::UI::Color>("EditorLayout.HierarchyRow.LastIconTint");
constexpr std::string_view kContextMenuItemHierarchySelectId = "hierarchy.select";
constexpr std::string_view kContextMenuItemHierarchyAddNodeId = "hierarchy.add_node";
constexpr std::string_view kContextMenuItemHierarchyAddComponentId = "hierarchy.add_component";
constexpr std::string_view kContextMenuItemHierarchyDeleteId = "hierarchy.delete";
constexpr std::string_view kContextMenuItemHierarchyCreatePrefabId = "hierarchy.create_prefab";
constexpr std::string_view kContextMenuItemHierarchyBackId = "hierarchy.back";
constexpr std::string_view kContextMenuItemHierarchyAddNodeTypePrefix = "hierarchy.add_node.type.";
constexpr std::string_view kContextMenuItemHierarchyAddComponentTypePrefix = "hierarchy.add_component.type.";
constexpr std::string_view kContextMenuItemInspectorDeleteComponentId = "inspector.delete_component";
constexpr std::string_view kContextMenuItemAssetSelectId = "asset.select";
constexpr std::string_view kContextMenuItemAssetPreviewId = "asset.preview";
constexpr std::string_view kContextMenuItemAssetPlaceId = "asset.place";
constexpr std::string_view kContextMenuItemAssetSaveId = "asset.save";
constexpr std::string_view kContextMenuItemAssetDeleteId = "asset.delete";
constexpr std::string_view kContextMenuItemAssetRenameId = "asset.rename";
constexpr std::string_view kContextMenuItemAssetRescanId = "asset.rescan";
constexpr std::string_view kContextMenuItemAssetCreateId = "asset.create";
constexpr std::string_view kContextMenuItemAssetImportId = "asset.import";
constexpr std::string_view kContextMenuItemContentInspectorSelectId = "asset_inspector.select";
constexpr std::string_view kContextMenuItemContentInspectorDeleteNodeId = "asset_inspector.delete_node";
constexpr std::string_view kContextMenuItemContentInspectorDeleteComponentId = "asset_inspector.delete_component";
constexpr std::string_view kContextMenuItemContentInspectorAddNodeTypePrefix = "asset_inspector.add_node.type.";
constexpr std::string_view kContextMenuItemContentInspectorAddComponentTypePrefix = "asset_inspector.add_component.type.";
constexpr std::string_view kContextMenuItemFileNewProjectId = "menu.file.new_project";
constexpr std::string_view kContextMenuItemFileOpenProjectId = "menu.file.open_project";
constexpr std::string_view kContextMenuItemFileProjectSettingsId = "menu.file.project_settings";

constexpr std::array<std::string_view, 14> kImportModelExtensions{
    ".fbx", ".gltf", ".glb", ".obj", ".dae", ".blend", ".3ds", ".ply",
    ".stl", ".x", ".x3d", ".usd", ".usdz", ".abc"};
constexpr std::array<std::string_view, 15> kImportTextureExtensions{
    ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".gif", ".tiff", ".tif",
    ".exr", ".hdr", ".psd", ".dds", ".pbm", ".pgm", ".ppm"};

void ApplyHierarchyRowIcon(SnAPI::UI::UIImage& Icon,
                           const std::string& Source,
                           const SnAPI::UI::Color Tint)
{
    const std::string ResolvedSource = ResolveUIImageSource(Source);
    if (Icon.Source().Get() != ResolvedSource)
    {
        Icon.Source().Set(ResolvedSource);
    }

    SnAPI::UI::SVGImageOptions SvgOptions{};
    SvgOptions.SetRasterSize(128, 128, true).ReplaceColor(SnAPI::UI::Color::RGB(0, 0, 0), Tint, 40);
    if (!(Icon.SvgOptions().Get() == SvgOptions))
    {
        Icon.SvgOptions().Set(SvgOptions);
    }
}

[[nodiscard]] bool HasDefaultConstructor(const TypeInfo& Info)
{
    return std::ranges::any_of(Info.Constructors, [](const ConstructorInfo& Constructor) {
        return Constructor.ParamTypes.empty();
    });
}

[[nodiscard]] std::string ShortTypeLabel(std::string_view QualifiedName)
{
    const std::size_t Delimiter = QualifiedName.rfind("::");
    if (Delimiter == std::string_view::npos)
    {
        return std::string(QualifiedName);
    }
    return std::string(QualifiedName.substr(Delimiter + 2));
}

[[nodiscard]] std::optional<std::size_t> TryParsePrefixedIndex(std::string_view Value, std::string_view Prefix)
{
    if (!Value.starts_with(Prefix))
    {
        return std::nullopt;
    }

    const std::string_view IndexText = Value.substr(Prefix.size());
    if (IndexText.empty())
    {
        return std::nullopt;
    }

    std::size_t Index = 0;
    for (const char Character : IndexText)
    {
        if (Character < '0' || Character > '9')
        {
            return std::nullopt;
        }
        Index = Index * 10u + static_cast<std::size_t>(Character - '0');
    }

    return Index;
}

[[nodiscard]] std::string TrimCopy(std::string Value)
{
    while (!Value.empty() && std::isspace(static_cast<unsigned char>(Value.front())) != 0)
    {
        Value.erase(Value.begin());
    }
    while (!Value.empty() && std::isspace(static_cast<unsigned char>(Value.back())) != 0)
    {
        Value.pop_back();
    }
    return Value;
}

[[nodiscard]] std::string ToLowerCopy(std::string_view Value)
{
    std::string Lower(Value);
    std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](const unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });
    return Lower;
}

template<std::size_t ExtCount>
[[nodiscard]] bool HasPathExtension(const std::string_view Path, const std::array<std::string_view, ExtCount>& Extensions)
{
    if (Path.empty())
    {
        return false;
    }

    std::filesystem::path FsPath{std::string(Path)};
    const std::string ExtensionLower = ToLowerCopy(FsPath.extension().string());
    if (ExtensionLower.empty())
    {
        return false;
    }

    return std::ranges::any_of(Extensions, [&ExtensionLower](const std::string_view Candidate) {
        return ExtensionLower == Candidate;
    });
}

void ConfigureSplitZone(SnAPI::UI::UIDockZone& Zone,
                        const SnAPI::UI::EDockSplit Direction,
                        const float SplitRatio,
                        const float MinPrimarySize,
                        const float MinSecondarySize)
{
    Zone.SplitDirection().Set(Direction);
    Zone.SplitRatio().Set(SplitRatio);
    Zone.MinPrimarySize().Set(MinPrimarySize);
    Zone.MinSecondarySize().Set(MinSecondarySize);
    Zone.Width().Set(SnAPI::UI::Sizing::Fill());
    Zone.Height().Set(SnAPI::UI::Sizing::Fill());
    Zone.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
}

void ConfigureConduitSplitZone(SnAPI::UI::UIDockZone& Zone,
                               const SnAPI::UI::EDockSplit Direction,
                               const float SplitRatio,
                               const float MinPrimarySize,
                               const float MinSecondarySize)
{
    ConfigureSplitZone(Zone, Direction, SplitRatio, MinPrimarySize, MinSecondarySize);
    Zone.SplitterThickness().Set(10.0f);
    Zone.SplitterColor().Set(SnAPI::UI::Color{60, 76, 96, 224});
    Zone.SplitterHoverColor().Set(SnAPI::UI::Color{118, 150, 186, 255});
}

void ConfigureHostPanel(SnAPI::UI::UIPanel& Panel)
{
    Panel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    Panel.Width().Set(SnAPI::UI::Sizing::Fill());
    Panel.Height().Set(SnAPI::UI::Sizing::Fill());
    Panel.Padding().Set(0.0f);
    Panel.Gap().Set(0.0f);
    Panel.UseGradient().Set(false);
    Panel.Background().Set(SnAPI::UI::Color::Transparent());
    Panel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    Panel.BorderThickness().Set(0.0f);
    Panel.CornerRadius().Set(0.0f);
    Panel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
}

void ConfigureLayoutSpacerPanel(SnAPI::UI::UIPanel& Panel)
{
    Panel.Height().Set(SnAPI::UI::Sizing::Auto());
    Panel.Padding().Set(0.0f);
    Panel.Gap().Set(0.0f);
    Panel.UseGradient().Set(false);
    Panel.Background().Set(SnAPI::UI::Color::Transparent());
    Panel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    Panel.BorderThickness().Set(0.0f);
    Panel.CornerRadius().Set(0.0f);
    Panel.DropShadowColor().Set(SnAPI::UI::Color::Transparent());
    Panel.DropShadowBlur().Set(0.0f);
    Panel.DropShadowSpread().Set(0.0f);
    Panel.DropShadowOffsetX().Set(0.0f);
    Panel.DropShadowOffsetY().Set(0.0f);
    Panel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey,
                                   SnAPI::UI::EVisibility::HitTestInvisible);
}

void ConfigureTransparentLayoutPanel(SnAPI::UI::UIPanel& Panel)
{
    Panel.UseGradient().Set(false);
    Panel.Background().Set(SnAPI::UI::Color::Transparent());
    Panel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    Panel.BorderThickness().Set(0.0f);
    Panel.CornerRadius().Set(0.0f);
    Panel.DropShadowColor().Set(SnAPI::UI::Color::Transparent());
    Panel.DropShadowBlur().Set(0.0f);
    Panel.DropShadowSpread().Set(0.0f);
    Panel.DropShadowOffsetX().Set(0.0f);
    Panel.DropShadowOffsetY().Set(0.0f);
}

void ConfigureModalScreenRatio(SnAPI::UI::UIModal& Modal, const float Ratio)
{
    const float ClampedRatio = std::clamp(Ratio, 0.1f, 1.0f);
    Modal.MinDialogWidth().Set(1.0f);
    Modal.MinDialogHeight().Set(1.0f);
    Modal.DialogWidth().Set(kModalRequestedSizePixels);
    Modal.DialogHeight().Set(kModalRequestedSizePixels);
    Modal.DialogMaxWidthRatio().Set(ClampedRatio);
    Modal.DialogMaxHeightRatio().Set(ClampedRatio);
}

void ConfigureSvgIcon(SnAPI::UI::UIImage& Image,
                      const float SizePx,
                      const SnAPI::UI::Color Tint,
                      const SnAPI::UI::Margin Margin = {})
{
    const float ScaledSizePx = SizePx * kEditorIconScale;
    const int IconRasterSizePx = std::max(1, static_cast<int>(std::round(ScaledSizePx)));
    SnAPI::UI::SVGImageOptions SvgOptions{};
    SvgOptions.SetRasterSize(IconRasterSizePx, IconRasterSizePx, true)
        .ReplaceColor(SnAPI::UI::Color::RGB(0, 0, 0), Tint, 40);
    if (!(Image.SvgOptions().Get() == SvgOptions))
    {
        Image.SvgOptions().Set(SvgOptions);
    }
    Image.Width().Set(SnAPI::UI::Sizing::Auto());
    Image.Height().Set(SnAPI::UI::Sizing::Auto());
    Image.Mode().Set(SnAPI::UI::EImageMode::Aspect);
    Image.LazyLoad().Set(false);
    Image.HAlign().Set(SnAPI::UI::EAlignment::Center);
    Image.VAlign().Set(SnAPI::UI::EAlignment::Center);
    Image.ElementMargin().Set(Margin);
}

void ConfigureFolderCardIcon(SnAPI::UI::UIImage& Image)
{
    SnAPI::UI::SVGImageOptions SvgOptions{};
    SvgOptions.SetRasterSize(512, 512, true)
        .ReplaceColor(SnAPI::UI::Color::RGB(0, 0, 0), kIconWhite, 40)
        .TreatBlackAsTransparent(6);

    const std::string FolderCardSource = ResolveUIImageSource(kFolderCardIconPath);
    if (Image.Source().Get() != FolderCardSource)
    {
        Image.Source().Set(FolderCardSource);
    }
    if (!(Image.SvgOptions().Get() == SvgOptions))
    {
        Image.SvgOptions().Set(SvgOptions);
    }

    Image.Width().Set(SnAPI::UI::Sizing::Fill());
    Image.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    Image.Mode().Set(SnAPI::UI::EImageMode::Aspect);
    Image.LazyLoad().Set(true);
    Image.HAlign().Set(SnAPI::UI::EAlignment::Center);
    Image.VAlign().Set(SnAPI::UI::EAlignment::Center);
    Image.Visibility().Set(SnAPI::UI::EVisibility::Visible);
}

[[nodiscard]] std::string ToLower(const std::string_view Text)
{
    std::string Out(Text);
    std::ranges::transform(Out, Out.begin(), [](const unsigned char Ch) {
        return static_cast<char>(std::tolower(Ch));
    });
    return Out;
}

[[nodiscard]] bool LabelMatchesFilter(const std::string_view Label, const std::string& FilterLower)
{
    if (FilterLower.empty())
    {
        return true;
    }

    const std::string LabelLower = ToLower(Label);
    return LabelLower.find(FilterLower) != std::string::npos;
}

[[nodiscard]] bool IsElementWithinSubtree(SnAPI::UI::UIContext& Context,
                                          SnAPI::UI::ElementId Element,
                                          const SnAPI::UI::ElementId SubtreeRoot)
{
    while (Element.Value != 0)
    {
        if (Element == SubtreeRoot)
        {
            return true;
        }
        Element = Context.GetParent(Element);
    }
    return false;
}

[[nodiscard]] std::string NormalizeBrowserPath(std::string_view Value)
{
    std::string Path(Value);
    std::replace(Path.begin(), Path.end(), '\\', '/');
    while (!Path.empty() && Path.front() == '/')
    {
        Path.erase(Path.begin());
    }
    while (!Path.empty() && Path.back() == '/')
    {
        Path.pop_back();
    }
    while (Path.find("//") != std::string::npos)
    {
        Path.replace(Path.find("//"), 2u, "/");
    }
    return Path;
}

[[nodiscard]] std::string ProjectNameFromFilePath(const std::string_view ProjectFilePath, const std::string_view FallbackName = {})
{
    std::string DerivedName = TrimCopy(std::string(FallbackName));
    if (!DerivedName.empty())
    {
        return DerivedName;
    }

    std::filesystem::path FilePath(ProjectFilePath);
    if (FilePath.empty())
    {
        return std::string("Project");
    }

    DerivedName = FilePath.stem().string();
    if (DerivedName.ends_with(".snproj"))
    {
        DerivedName.erase(DerivedName.size() - std::string(".snproj").size());
    }
    if (DerivedName.empty())
    {
        DerivedName = FilePath.parent_path().filename().string();
    }
    if (DerivedName.empty())
    {
        DerivedName = "Project";
    }

    return DerivedName;
}

[[nodiscard]] std::vector<std::string> SplitBrowserPath(const std::string& Path)
{
    std::vector<std::string> Parts{};
    std::string Normalized = NormalizeBrowserPath(Path);
    std::size_t Start = 0;
    while (Start < Normalized.size())
    {
        const std::size_t Delimiter = Normalized.find('/', Start);
        const std::string Part = (Delimiter == std::string::npos)
                                     ? Normalized.substr(Start)
                                     : Normalized.substr(Start, Delimiter - Start);
        if (!Part.empty())
        {
            Parts.push_back(Part);
        }
        if (Delimiter == std::string::npos)
        {
            break;
        }
        Start = Delimiter + 1u;
    }
    return Parts;
}

[[nodiscard]] std::string ParentBrowserPath(const std::string& Path)
{
    const std::string Normalized = NormalizeBrowserPath(Path);
    const std::size_t Delimiter = Normalized.rfind('/');
    if (Delimiter == std::string::npos)
    {
        return std::string{};
    }
    return Normalized.substr(0, Delimiter);
}

[[nodiscard]] std::string LeafBrowserName(const std::string& Path)
{
    const std::string Normalized = NormalizeBrowserPath(Path);
    const std::size_t Delimiter = Normalized.rfind('/');
    if (Delimiter == std::string::npos)
    {
        return Normalized;
    }
    return Normalized.substr(Delimiter + 1u);
}

[[nodiscard]] bool FolderContainsAsset(const std::string& FolderPath, const std::string& AssetFolderPath)
{
    if (FolderPath.empty())
    {
        return true;
    }
    if (AssetFolderPath == FolderPath)
    {
        return true;
    }
    const std::string Prefix = FolderPath + "/";
    return AssetFolderPath.rfind(Prefix, 0) == 0;
}

[[nodiscard]] std::size_t ComputeNodeComponentSignature(const BaseNode& Node)
{
    std::size_t Seed = Node.ComponentTypes().size();
    const auto HashCombine = [&Seed](const std::size_t Value) {
        Seed ^= Value + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
    };

    for (const TypeId& Type : Node.ComponentTypes())
    {
        HashCombine(UuidHash{}(Type));

        NodeHandle OwnerHandle = Node.Handle();
        if (auto* WorldRef = Node.World())
        {
            const void* ComponentPtr = WorldRef->BorrowedComponent(OwnerHandle, Type);
            HashCombine(std::hash<const void*>{}(ComponentPtr));
        }
    }

    return Seed;
}

struct CreateNodeTypeEntry
{
    TypeId Type{};
    std::string Label{};
    std::string QualifiedName{};
    int Depth = 0;
    bool HasChildren = false;
};

[[nodiscard]] std::vector<const TypeInfo*> CollectContentInspectorCreatableNodeTypes();

[[nodiscard]] std::vector<CreateNodeTypeEntry> BuildCreateNodeTypeEntries(const std::string& FilterLower)
{
    AuthoredAssetRegistry::Instance().EnsureBuilt();
    std::vector<CreateNodeTypeEntry> Entries{};
    for (const AuthoredAssetDescriptor& Descriptor : AuthoredAssetRegistry::Instance().All())
    {
        if (!Descriptor.CanCreate || Descriptor.AssetType == TypeId{})
        {
            continue;
        }

        const std::string QualifiedName = Descriptor.Type ? Descriptor.Type->Name : Descriptor.DisplayName;
        if (!FilterLower.empty() &&
            !LabelMatchesFilter(Descriptor.DisplayName, FilterLower) &&
            !LabelMatchesFilter(Descriptor.Category, FilterLower) &&
            !LabelMatchesFilter(QualifiedName, FilterLower))
        {
            continue;
        }

        Entries.push_back(CreateNodeTypeEntry{
            .Type = Descriptor.AssetType,
            .Label = Descriptor.DisplayName,
            .QualifiedName = QualifiedName,
            .Depth = 0,
            .HasChildren = false,
        });
    }

    for (const TypeInfo* Type : CollectContentInspectorCreatableNodeTypes())
    {
        if (!Type)
        {
            continue;
        }

        const std::string TypeLabel = ShortTypeLabel(Type->Name);
        const std::string QualifiedName = Type->Name;
        if (!FilterLower.empty() &&
            !LabelMatchesFilter(TypeLabel, FilterLower) &&
            !LabelMatchesFilter(QualifiedName, FilterLower) &&
            !LabelMatchesFilter("Prefab", FilterLower))
        {
            continue;
        }

        Entries.push_back(CreateNodeTypeEntry{
            .Type = Type->Id,
            .Label = TypeLabel.empty() ? std::string("Node") : (TypeLabel + " (Prefab)"),
            .QualifiedName = QualifiedName,
            .Depth = 0,
            .HasChildren = false,
        });
    }

    std::sort(Entries.begin(), Entries.end(), [](const CreateNodeTypeEntry& Left, const CreateNodeTypeEntry& Right) {
        if (Left.Label != Right.Label)
        {
            return Left.Label < Right.Label;
        }
        return Left.QualifiedName < Right.QualifiedName;
    });
    return Entries;
}

[[nodiscard]] std::vector<const TypeInfo*> CollectContentInspectorCreatableNodeTypes()
{
    (void)TypeAutoRegistry::Instance().EnsureAll();

    std::vector<const TypeInfo*> CandidateTypes = TypeRegistry::Instance().Derived(StaticTypeId<BaseNode>());
    if (const TypeInfo* BaseNodeInfo = TypeRegistry::Instance().Find(StaticTypeId<BaseNode>()))
    {
        const bool AlreadyPresent = std::ranges::any_of(CandidateTypes, [BaseNodeInfo](const TypeInfo* Type) {
            return Type && Type->Id == BaseNodeInfo->Id;
        });
        if (!AlreadyPresent)
        {
            CandidateTypes.push_back(BaseNodeInfo);
        }
    }

    CandidateTypes.erase(
        std::remove_if(CandidateTypes.begin(), CandidateTypes.end(), [](const TypeInfo* Type) {
            if (!Type || !HasDefaultConstructor(*Type))
            {
                return true;
            }
            if (!TypeRegistry::Instance().IsA(Type->Id, StaticTypeId<BaseNode>()))
            {
                return true;
            }
            if (TypeRegistry::Instance().IsA(Type->Id, StaticTypeId<World>()))
            {
                return true;
            }
            if (TypeRegistry::Instance().IsA(Type->Id, StaticTypeId<Level>()))
            {
                return true;
            }
            return false;
        }),
        CandidateTypes.end());

    std::sort(CandidateTypes.begin(), CandidateTypes.end(), [](const TypeInfo* Left, const TypeInfo* Right) {
        if (!Left || !Right)
        {
            return Left < Right;
        }
        const std::string LeftName = ShortTypeLabel(Left->Name);
        const std::string RightName = ShortTypeLabel(Right->Name);
        return LeftName < RightName;
    });
    return CandidateTypes;
}

[[nodiscard]] std::vector<const TypeInfo*> CollectContentInspectorCreatableComponentTypes()
{
    (void)TypeAutoRegistry::Instance().EnsureAll();

    std::vector<const TypeInfo*> CandidateTypes{};
    const auto RegisteredComponentTypes = ComponentSerializationRegistry::Instance().Types();
    CandidateTypes.reserve(RegisteredComponentTypes.size());
    for (const TypeId& ComponentType : RegisteredComponentTypes)
    {
        const TypeInfo* Info = TypeRegistry::Instance().Find(ComponentType);
        if (!Info || !HasDefaultConstructor(*Info))
        {
            continue;
        }
        CandidateTypes.push_back(Info);
    }

    std::sort(CandidateTypes.begin(), CandidateTypes.end(), [](const TypeInfo* Left, const TypeInfo* Right) {
        if (!Left || !Right)
        {
            return Left < Right;
        }
        const std::string LeftName = ShortTypeLabel(Left->Name);
        const std::string RightName = ShortTypeLabel(Right->Name);
        return LeftName < RightName;
    });
    return CandidateTypes;
}

[[nodiscard]] CameraComponent* ResolveActiveCameraComponent(GameRuntime& Runtime, ComponentHandle& InOutHandle)
{
    if (InOutHandle.IsNull())
    {
        return nullptr;
    }

    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return nullptr;
    }

    return static_cast<CameraComponent*>(WorldPtr->BorrowedComponent(InOutHandle));
}
} // namespace

Result EditorLayout::Build(GameRuntime& Runtime,
                           SnAPI::UI::Theme& Theme,
                           ComponentHandle ActiveCamera,
                           EditorSelectionModel* SelectionModel)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    (void)Theme;
    (void)ActiveCamera;
    (void)SelectionModel;
    return std::unexpected(MakeError(EErrorCode::NotSupported, "Editor layout requires renderer and UI support"));
#else
    Shutdown(&Runtime);

    if (!RegisterExternalElements(Runtime))
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to register external editor UI elements"));
    }

    m_context = RootContext(Runtime);
    if (!m_context)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Root UI context is not available"));
    }

    m_runtime = &Runtime;
    m_invalidationDebugOverlayEnabled = QueryInvalidationDebugOverlayEnabled();
    InitializeViewModel();
    m_context->SetActiveTheme(&Theme);
    BuildShell(*m_context, Runtime, ActiveCamera, SelectionModel);
    SyncInvalidationDebugOverlay();
    BindInspectorTarget(ResolveSelectedNode(Runtime, ActiveCamera), Runtime, ActiveCamera);
    SyncGameViewportCamera(Runtime, ActiveCamera);

    m_built = true;
    return Ok();
#endif
}

void EditorLayout::Shutdown(GameRuntime* Runtime)
{
    (void)Runtime;
    CloseContextMenu();
    DestroyContentAssetCreateModalOverlay();
    DestroyContentAssetImportModalOverlay();
    DestroyProjectModalOverlay();
    DestroyProjectSettingsModalOverlay();
    DestroyContentAssetInspectorModalOverlay();
    if (m_context && m_shellRoot.Id.Value != 0)
    {
        m_context->DestroyElement(m_shellRoot.Id);
    }
    m_context = nullptr;
    m_runtime = nullptr;
    m_shellRoot = {};
    m_gameViewTabs = {};
    m_gameViewport = {};
    m_inspectorPropertyPanel = {};
    m_hierarchyTree = {};
    m_contextMenu = {};
    m_hierarchyCountBadge = {};
    m_invalidationDebugToggleSwitch = {};
    m_invalidationDebugToggleLabel = {};
    m_contentSearchInput = {};
    m_contentPathBreadcrumbs = {};
    m_contentAssetNameValue = {};
    m_contentAssetTypeValue = {};
    m_contentAssetVariantValue = {};
    m_contentAssetIdValue = {};
    m_contentAssetStatusValue = {};
    m_contentPlaceButton = {};
    m_contentSaveButton = {};
    m_contentAssetsList = {};
    m_contentAssetsEmptyHint = {};
    m_contentCreateModalOverlay = {};
    m_contentCreateTypeTree = {};
    m_contentCreateSearchInput = {};
    m_contentCreateNameInput = {};
    m_contentCreateOkButton = {};
    m_contentImportModalOverlay = {};
    m_contentImportSourcePicker = {};
    m_contentImportSummaryText = {};
    m_contentImportSettingsPanel = {};
    m_contentImportOkButton = {};
    m_contentInspectorModalOverlay = {};
    m_contentInspectorTitleText = {};
    m_contentInspectorStatusText = {};
    m_contentInspectorHierarchyTitleText = {};
    m_contentInspectorPreviewStatsText = {};
    m_contentInspectorPreviewImage = {};
    m_contentInspectorHierarchyTree = {};
    m_contentInspectorPropertyPanel = {};
    m_contentInspectorImportSettingsTitleText = {};
    m_contentInspectorImportSettingsPanel = {};
    m_contentInspectorSaveButton = {};
    m_contentInspectorReimportButton = {};
    m_conduitWorkspaceTitleText = {};
    m_conduitWorkspaceStatusText = {};
    m_conduitWorkspaceSummaryText = {};
    m_conduitVariablesTree = {};
    m_conduitPaletteSearchInput = {};
    m_conduitPaletteTree = {};
    m_conduitPaletteAddNodeButton = {};
    m_conduitGraphCanvas = {};
    m_conduitNodesTree = {};
    m_conduitNodeRemoveButton = {};
    m_conduitVariableInspectorPanel = {};
    m_conduitNodeInspectorPanel = {};
    m_conduitGraphWorkspaceHost = {};
    m_conduitClassWorkspaceHost = {};
    m_conduitInspectorTitleText = {};
    m_conduitVariableCreateNameInput = {};
    m_conduitVariableCreateTypeCombo = {};
    m_conduitVariableCreateButton = {};
    m_conduitVariableNameInput = {};
    m_conduitVariableTypeCombo = {};
    m_conduitVariableRemoveButton = {};
    m_conduitVariableDefaultHintText = {};
    m_conduitVariableDefaultBoolCheckbox = {};
    m_conduitVariableDefaultTextInput = {};
    m_conduitVariableDefaultEnumCombo = {};
    m_conduitVariableDefaultPropertyPanel = {};
    m_conduitVariableDefaultClearButton = {};
    m_conduitVariableDefaultApplyButton = {};
    m_conduitVariableDefaultResetButton = {};
    m_conduitNodeSummaryText = {};
    m_conduitNodePrimaryLabelText = {};
    m_conduitNodePrimaryTextInput = {};
    m_conduitNodeSecondaryLabelText = {};
    m_conduitNodeSecondaryTextInput = {};
    m_conduitClassOverviewSummaryText = {};
    m_conduitClassOverviewHostText = {};
    m_conduitClassOverviewGraphText = {};
    m_contentAssetCards.clear();
    m_contentAssetCardButtons.clear();
    m_contentAssetCardIndices.clear();
    m_contentBrowserEntries.clear();
    m_contentAssets.clear();
    m_contentAssetFilterText.clear();
    m_conduitVisibleNodeIds.clear();
    m_conduitVisiblePaletteStableIds.clear();
    m_conduitPaletteFilterText.clear();
    m_conduitSelectedPaletteStableId.clear();
    m_contentCurrentFolder.clear();
    m_selectedContentAssetKey.clear();
    m_selectedContentFolderPath.clear();
    m_lastContentAssetClickKey.clear();
    m_lastContentAssetClickTime = {};
    m_contentCreateModalOpen = false;
    m_contentCreateTypeFilterText.clear();
    m_contentCreateNameText.clear();
    m_contentCreateSelectedType = {};
    m_contentCreateVisibleTypes.clear();
    m_contentCreateTypeSource.reset();
    m_contentImportModalOpen = false;
    m_contentImportSourcePath.clear();
    m_contentImportProfile = EImportProfile::Unknown;
    m_contentImportAssimpSettings = {};
    m_contentImportTextureSettings = {};
    m_conduitWorkspaceState = {};
    m_conduitVisibleVariableIds.clear();
    m_conduitCreateVariableNameText.clear();
    m_conduitCreateSelectedVariableType = {};
    m_conduitVariableDefaultPanelBound = false;
    m_conduitVariableDefaultBoundObject = nullptr;
    m_conduitVariableDefaultBoundType = {};
    m_projectModalOpen = false;
    m_projectModalRequired = false;
    m_projectModalShowWelcome = false;
    m_projectSettingsModalOpen = false;
    m_projectModalAction = EProjectAction::CreateNew;
    m_projectNameText.clear();
    m_projectDirectoryText.clear();
    m_projectFilePathText.clear();
    m_projectSettingsNameText.clear();
    m_projectSettingsStartupAssetText.clear();
    m_projectSettingsDefaultRenderSettingsAssetId.clear();
    m_projectSettingsRenderSettingsOptions.clear();
    m_projectState = {};
    m_recentProjects.clear();
    m_contentAssetInspectorState = {};
    m_contentInspectorVisibleNodes.clear();
    m_contentInspectorHierarchySource.reset();
    m_contentInspectorTargetBound = false;
    m_contentInspectorBoundNode = {};
    m_contentInspectorBoundObject = nullptr;
    m_contentInspectorBoundType = {};
    m_contentInspectorBoundComponentSignature = 0;
    m_contentInspectorImportTargetBound = false;
    m_contentInspectorImportBoundObject = nullptr;
    m_contentInspectorImportBoundType = {};
    m_contentAssetDetails = {};
    m_onContentAssetSelected = {};
    m_onContentAssetPlaceRequested = {};
    m_onContentAssetSaveRequested = {};
    m_onContentAssetDeleteRequested = {};
    m_onContentAssetRenameRequested = {};
    m_onContentAssetRefreshRequested = {};
    m_onContentAssetCreateRequested = {};
    m_onContentAssetImportRequested = {};
    m_onContentAssetInspectorSaveRequested = {};
    m_onContentAssetInspectorReimportRequested = {};
    m_onContentAssetInspectorCloseRequested = {};
    m_onContentAssetInspectorNodeSelected = {};
    m_onContentAssetInspectorHierarchyActionRequested = {};
    m_hierarchyItemSource.reset();
    m_contextMenuScope = EContextMenuScope::None;
    m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    m_pendingHierarchyMenuIndex.reset();
    m_pendingHierarchyMenuOpenPosition = {};
    m_contextMenuHierarchyIndex.reset();
    m_contextMenuAssetIndex.reset();
    m_contextMenuContentInspectorNode = {};
    m_contextMenuComponentOwner.reset();
    m_contextMenuComponentType = {};
    m_contextMenuNodeTypes.clear();
    m_contextMenuComponentTypes.clear();
    m_contextMenuOpenPosition = {};
    m_hierarchyVisibleNodes.clear();
    m_hierarchySignature = 0;
    m_hierarchyNodeCount = 0;
    m_hierarchyVisualSelection = {};
    m_hierarchyFilterText.clear();
    m_selection = nullptr;
    m_onHierarchyNodeChosen.Reset();
    m_onHierarchyActionRequested = {};
    m_onToolbarActionRequested = {};
    m_onProjectActionRequested = {};
    m_boundInspectorNode = {};
    m_boundInspectorObject = nullptr;
    m_boundInspectorType = {};
    m_boundInspectorComponentSignature = 0;
    m_invalidationDebugOverlayEnabled = false;
    m_menuFileButton = {};
    m_projectModalOverlay = {};
    m_projectNameInput = {};
    m_projectDirectoryInput = {};
    m_projectFilePathInput = {};
    m_projectModalOkButton = {};
    m_projectSettingsModalOverlay = {};
    m_projectSettingsNameInput = {};
    m_projectSettingsStartupAssetInput = {};
    m_projectSettingsDefaultRenderSettingsCombo = {};
    m_projectSettingsSaveButton = {};
    m_viewModel = SnAPI::UI::PropertyMap{};
    m_built = false;
}

void EditorLayout::Sync(GameRuntime& Runtime,
                        ComponentHandle ActiveCamera,
                        EditorSelectionModel* SelectionModel,
                        const float DeltaSeconds)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    (void)ActiveCamera;
    (void)SelectionModel;
    (void)DeltaSeconds;
#else
    if (!m_built)
    {
        return;
    }

    if (!m_context)
    {
        m_context = RootContext(Runtime);
        if (!m_context)
        {
            m_built = false;
            return;
        }
    }

    m_selection = SelectionModel;
    SyncInvalidationDebugOverlay();
    SyncHierarchy(Runtime, ActiveCamera);
    BindInspectorTarget(ResolveSelectedNode(Runtime, ActiveCamera), Runtime, ActiveCamera);
    SyncGameViewportCamera(Runtime, ActiveCamera);
    (void)DeltaSeconds;
#endif
}

bool EditorLayout::RegisterExternalElements(GameRuntime& Runtime)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    return false;
#else
    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        return false;
    }

    auto& UI = WorldPtr->UI();
    const Result RegisterViewport = UI.RegisterElementType<UIRenderViewport>(SnAPI::UI::TypeHash<SnAPI::UI::UIPanel>());
    const Result RegisterPropertyPanel =
        UI.RegisterElementType<UIPropertyPanel>(SnAPI::UI::TypeHash<SnAPI::UI::UIScrollContainer>());
    const Result RegisterConduitCanvas =
        UI.RegisterElementType<Conduit::Editor::UIConduitGraphCanvas>(SnAPI::UI::TypeHash<SnAPI::UI::UIPanel>());
    return static_cast<bool>(RegisterViewport) &&
           static_cast<bool>(RegisterPropertyPanel) &&
           static_cast<bool>(RegisterConduitCanvas);
#endif
}

SnAPI::UI::UIContext* EditorLayout::RootContext(GameRuntime& Runtime) const
{
#if !defined(SNAPI_GF_ENABLE_UI)
    (void)Runtime;
    return nullptr;
#else
    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        return nullptr;
    }

    const std::uint64_t RootContextId = WorldPtr->UI().RootContextId();
    if (RootContextId == 0)
    {
        return nullptr;
    }

    return WorldPtr->UI().Context(RootContextId);
#endif
}

void EditorLayout::BuildShell(SnAPI::UI::UIContext& Context,
                              GameRuntime& Runtime,
                              ComponentHandle& ActiveCamera,
                              EditorSelectionModel* SelectionModel)
{
    auto Root = Context.Root();
    ConfigureRoot(Context);
    m_selection = SelectionModel;

    auto ShellRoot = Root.Add(SnAPI::UI::UIPanel("Editor.ShellRoot"));
    auto& ShellRootPanel = ShellRoot.Element();
    ShellRootPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ShellRootPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ShellRootPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ShellRootPanel.Padding().Set(0.0f);
    ShellRootPanel.Gap().Set(0.0f);
    ShellRootPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ShellRootPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ShellRootPanel.BorderThickness().Set(0.0f);
    ShellRootPanel.CornerRadius().Set(0.0f);
    ShellRootPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    m_shellRoot = ShellRoot.Handle();

    BuildMenuBar(ShellRoot);
    BuildToolbar(ShellRoot);

    auto MainAreaSplit = ShellRoot.Add(SnAPI::UI::UIDockZone{});
    auto& MainAreaSplitElement = MainAreaSplit.Element();
    ConfigureSplitZone(MainAreaSplitElement, SnAPI::UI::EDockSplit::Vertical, kMainAreaSplitRatio, 220.0f, 140.0f);

    auto WorkspaceHost = MainAreaSplit.Add(SnAPI::UI::UIPanel("Editor.WorkspaceHost"));
    ConfigureHostPanel(WorkspaceHost.Element());
    BuildWorkspace(WorkspaceHost, Runtime, ActiveCamera, SelectionModel);

    auto BrowserHost = MainAreaSplit.Add(SnAPI::UI::UIPanel("Editor.ContentBrowserHost"));
    ConfigureHostPanel(BrowserHost.Element());
    BuildContentBrowser(BrowserHost);
}

void EditorLayout::ConfigureRoot(SnAPI::UI::UIContext& Context)
{
    auto Root = Context.Root();
    auto& RootPanel = Root.Element();
    RootPanel.ElementStyle().Apply("editor.root");
    // Force an opaque fullscreen root so uncovered regions never reveal desktop composition.
    RootPanel.UseGradient().Set(false);
    RootPanel.Background().Set(SnAPI::UI::Color{12, 13, 16, 255});
    RootPanel.BorderColor().Set(SnAPI::UI::Color{12, 13, 16, 255});
    RootPanel.BorderThickness().Set(0.0f);
    RootPanel.CornerRadius().Set(0.0f);
    RootPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    RootPanel.Padding().Set(0.0f);
    RootPanel.Gap().Set(0.0f);
    RootPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    RootPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    RootPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
}

void EditorLayout::InitializeViewModel()
{
    m_viewModel = SnAPI::UI::PropertyMap{};

    ViewModelProperty<bool>(kVmInvalidationDebugEnabledKey).AddSetHook([this](const bool Enabled) {
        m_invalidationDebugOverlayEnabled = Enabled;

#if defined(SNAPI_GF_ENABLE_UI)
        if (m_runtime)
        {
            if (auto* WorldPtr = m_runtime->WorldPtr(); WorldPtr && WorldPtr->UI().IsInitialized())
            {
                auto& UI = WorldPtr->UI();
                const auto ContextIds = UI.ContextIds();
                for (const std::uint64_t ContextId : ContextIds)
                {
                    if (auto* Context = UI.Context(ContextId))
                    {
                        Context->SetInvalidationDebugOverlayEnabled(Enabled);
                    }
                }
            }
        }
#endif

        ViewModelProperty<std::string>(kVmInvalidationDebugLabelTextKey)
            .Set(Enabled ? std::string("ON") : std::string("OFF"));
        ViewModelProperty<SnAPI::UI::Color>(kVmInvalidationDebugLabelColorKey)
            .Set(Enabled ? SnAPI::UI::Color{184, 238, 198, 255} : SnAPI::UI::Color{224, 228, 235, 255});
    });

    ViewModelProperty<std::string>(kVmHierarchyFilterTextKey).AddSetHook([this](const std::string& Value) {
        m_hierarchyFilterText = ToLower(Value);
        m_hierarchySignature = 0;
        m_hierarchyNodeCount = 0;
    });

    ViewModelProperty<std::string>(kVmContentFilterTextKey).AddSetHook([this](const std::string& Value) {
        m_contentAssetFilterText = ToLower(Value);
        ApplyContentAssetFilter();
    });

    ViewModelProperty<std::string>(kVmContentCreateTypeFilterKey).AddSetHook([this](const std::string& Value) {
        m_contentCreateTypeFilterText = ToLower(Value);
        RebuildContentAssetCreateTypeTree();
    });

    ViewModelProperty<std::string>(kVmContentCreateAssetNameKey).AddSetHook([this](const std::string& Value) {
        m_contentCreateNameText = Value;
        RefreshContentAssetCreateOkButtonState();
    });

    ViewModelProperty<std::string>(kVmSelectedContentAssetKey).AddSetHook([this](const std::string& Value) {
        m_selectedContentAssetKey = Value;
        if (!Value.empty())
        {
            m_selectedContentFolderPath.clear();
        }
        RefreshContentAssetCardSelectionStyles();
        RefreshContentAssetDetailsViewModel();
    });

    ViewModelProperty<bool>(kVmContentAssetCanPlaceKey).AddSetHook([this](const bool CanPlace) {
        if (!m_context || m_contentPlaceButton.Id.Value == 0)
        {
            return;
        }

        if (auto* PlaceButton = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_contentPlaceButton.Id)))
        {
            PlaceButton->SetDisabled(!CanPlace);
        }
    });

    ViewModelProperty<bool>(kVmContentAssetCanSaveKey).AddSetHook([this](const bool CanSave) {
        if (!m_context || m_contentSaveButton.Id.Value == 0)
        {
            return;
        }

        if (auto* SaveButton = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_contentSaveButton.Id)))
        {
            SaveButton->SetDisabled(!CanSave);
        }
    });

    ViewModelProperty<std::string>(kVmHierarchyCountTextKey).Set(std::string("0"));
    ViewModelProperty<std::string>(kVmHierarchyFilterTextKey).Set(m_hierarchyFilterText);
    ViewModelProperty<std::string>(kVmContentFilterTextKey).Set(m_contentAssetFilterText);
    ViewModelProperty<std::string>(kVmSelectedContentAssetKey).Set(m_selectedContentAssetKey);
    ViewModelProperty<std::string>(kVmContentAssetNameKey).Set(std::string("--"));
    ViewModelProperty<std::string>(kVmContentAssetTypeKey).Set(std::string("--"));
    ViewModelProperty<std::string>(kVmContentAssetVariantKey).Set(std::string("--"));
    ViewModelProperty<std::string>(kVmContentAssetIdKey).Set(std::string("--"));
    ViewModelProperty<std::string>(kVmContentAssetStatusKey).Set(std::string("Ready"));
    ViewModelProperty<std::string>(kVmContentCreateTypeFilterKey).Set(std::string{});
    ViewModelProperty<std::string>(kVmContentCreateAssetNameKey).Set(std::string{});
    ViewModelProperty<bool>(kVmContentAssetCanPlaceKey).Set(false);
    ViewModelProperty<bool>(kVmContentAssetCanSaveKey).Set(false);
    ViewModelProperty<bool>(kVmInvalidationDebugEnabledKey).Set(m_invalidationDebugOverlayEnabled);
    RefreshContentAssetDetailsViewModel();
}

void EditorLayout::BuildMenuBar(PanelBuilder& Root)
{
    auto MenuBar = Root.Add(SnAPI::UI::UIMenuBar{});
    auto& MenuBarElement = MenuBar.Element();
    MenuBarElement.ElementStyle().Apply("editor.menu_bar");
    MenuBarElement.Height().Set(SnAPI::UI::Sizing::Auto());
    MenuBarElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto BrandIcon = MenuBar.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kBrandIconPath)));
    auto& BrandIconImage = BrandIcon.Element();
    ConfigureSvgIcon(BrandIconImage, 16.0f, kIconWhite, SnAPI::UI::Margin{0.0f, 0.0f, 6.0f, 0.0f});

    auto Brand = MenuBar.Add(SnAPI::UI::UIText("SnAPI"));
    Brand.Element().ElementStyle().Apply("editor.brand_title");
    Brand.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 3.0f, 0.0f});

    auto Product = MenuBar.Add(SnAPI::UI::UIText("GameFramework"));
    Product.Element().ElementStyle().Apply("editor.brand_subtitle");
    Product.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 14.0f, 0.0f});

    for (std::size_t Index = 0; Index < kMenuItems.size(); ++Index)
    {
        if (kMenuItems[Index] == std::string_view("File"))
        {
            auto MenuButton = MenuBar.Add(SnAPI::UI::UIButton{});
            auto& MenuButtonElement = MenuButton.Element();
            MenuButtonElement.ElementStyle().Apply("editor.menu_button");
            MenuButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
            MenuButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
            MenuButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
            MenuButtonElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 10.0f, 0.0f});
            MenuButtonElement.OnClick([this]() {
                OpenFileMenu();
            });

            auto Label = MenuButton.Add(SnAPI::UI::UIText(std::string(kMenuItems[Index])));
            auto& LabelText = Label.Element();
            LabelText.ElementStyle().Apply("editor.menu_button_text");
            LabelText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);

            m_menuFileButton = MenuButton.Handle();
            continue;
        }

        auto Item = MenuBar.Add(SnAPI::UI::UIText(kMenuItems[Index]));
        auto& ItemText = Item.Element();
        ItemText.ElementStyle().Apply("editor.menu_item");
        ItemText.TextColor().Set(SnAPI::UI::Color{224, 228, 235, 255});
        ItemText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
        ItemText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 10.0f, 0.0f});
    }

    auto Spacer = MenuBar.Add(SnAPI::UI::UIPanel("Editor.MenuSpacer"));
    auto& SpacerPanel = Spacer.Element();
    ConfigureLayoutSpacerPanel(SpacerPanel);
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto InvalidationTogglePanel = MenuBar.Add(SnAPI::UI::UIPanel("Editor.InvalidationDebugToggle"));
    auto& InvalidationTogglePanelElement = InvalidationTogglePanel.Element();
    InvalidationTogglePanelElement.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    InvalidationTogglePanelElement.Width().Set(SnAPI::UI::Sizing::Auto());
    InvalidationTogglePanelElement.Height().Set(SnAPI::UI::Sizing::Auto());
    InvalidationTogglePanelElement.Padding().Set(0.0f);
    InvalidationTogglePanelElement.Gap().Set(6.0f);
    InvalidationTogglePanelElement.Background().Set(SnAPI::UI::Color::Transparent());
    InvalidationTogglePanelElement.BorderColor().Set(SnAPI::UI::Color::Transparent());
    InvalidationTogglePanelElement.BorderThickness().Set(0.0f);
    InvalidationTogglePanelElement.ElementMargin().Set(SnAPI::UI::Margin{8.0f, 0.0f, 0.0f, 0.0f});

    auto InvalidationTitle = InvalidationTogglePanel.Add(SnAPI::UI::UIText("InvDbg"));
    auto& InvalidationTitleText = InvalidationTitle.Element();
    InvalidationTitleText.ElementStyle().Apply("editor.menu_button_text");
    InvalidationTitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);

    auto InvalidationToggleLabel = InvalidationTogglePanel.Add(SnAPI::UI::UIText{});
    auto& InvalidationToggleLabelText = InvalidationToggleLabel.Element();
    InvalidationToggleLabelText.ElementStyle().Apply("editor.menu_button_text");
    InvalidationToggleLabelText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
    auto vmInvalidationLabelText = ViewModelProperty<std::string>(kVmInvalidationDebugLabelTextKey);
    auto vmInvalidationLabelColor = ViewModelProperty<SnAPI::UI::Color>(kVmInvalidationDebugLabelColorKey);
    InvalidationToggleLabelText.Text().BindTo(vmInvalidationLabelText);
    InvalidationToggleLabelText.TextColor().BindTo(vmInvalidationLabelColor);
    m_invalidationDebugToggleLabel = InvalidationToggleLabel.Handle();

    auto InvalidationToggleSwitch = InvalidationTogglePanel.Add(SnAPI::UI::UISwitch{});
    auto& InvalidationToggleSwitchElement = InvalidationToggleSwitch.Element();
    InvalidationToggleSwitchElement.ElementStyle().Apply("editor.menu_switch");
    InvalidationToggleSwitchElement.Width().Set(SnAPI::UI::Sizing::Fixed(42.0f));
    InvalidationToggleSwitchElement.Height().Set(SnAPI::UI::Sizing::Fixed(22.0f));
    auto vmInvalidationEnabled = ViewModelProperty<bool>(kVmInvalidationDebugEnabledKey);
    InvalidationToggleSwitchElement.Value().BindTo(vmInvalidationEnabled, SnAPI::UI::EBindMode::TwoWay);
    m_invalidationDebugToggleSwitch = InvalidationToggleSwitch.Handle();

    PublishInvalidationDebugState();
}

void EditorLayout::BuildToolbar(PanelBuilder& Root)
{
    auto Toolbar = Root.Add(SnAPI::UI::UIToolbar{});
    auto& ToolbarElement = Toolbar.Element();
    ToolbarElement.ElementStyle().Apply("editor.toolbar");
    ToolbarElement.Height().Set(SnAPI::UI::Sizing::Auto());
    ToolbarElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    for (std::size_t Index = 0; Index < kToolbarActions.size(); ++Index)
    {
        auto Button = Toolbar.Add(SnAPI::UI::UIButton{});
        auto& ButtonElement = Button.Element();
        ButtonElement.ElementStyle().Apply("editor.toolbar_button");
        ButtonElement.Width().Set(SnAPI::UI::Sizing::Fixed(kToolbarActionButtonSize));
        ButtonElement.Height().Set(SnAPI::UI::Sizing::Fixed(kToolbarActionButtonSize));
        ButtonElement.ElementPadding().Set(SnAPI::UI::Padding{12.0f, 12.0f, 12.0f, 12.0f});
        ButtonElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 6.0f, 0.0f});

        auto Icon = Button.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kToolbarActions[Index].IconPath)));
        auto& IconImage = Icon.Element();
        ConfigureSvgIcon(IconImage, kToolbarActionIconDisplaySize, kToolbarActions[Index].Tint);
        ButtonElement.OnClick([this, Index]() {
            if (!m_onToolbarActionRequested)
            {
                return;
            }

            m_onToolbarActionRequested(kToolbarActions[Index].Action);
        });
    }

    auto Spacer = Toolbar.Add(SnAPI::UI::UIPanel("Editor.ToolbarSpacer"));
    auto& SpacerPanel = Spacer.Element();
    ConfigureLayoutSpacerPanel(SpacerPanel);
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto ModeBreadcrumbs = Toolbar.Add(SnAPI::UI::UIBreadcrumbs{});
    auto& ModeBreadcrumbsElement = ModeBreadcrumbs.Element();
    ModeBreadcrumbsElement.ElementStyle().Apply("editor.modes_breadcrumb");
    ModeBreadcrumbsElement.SetCrumbs({std::string(kViewportModes[0]), std::string(kViewportModes[1])});
}

void EditorLayout::BuildWorkspace(PanelBuilder& Root,
                                  GameRuntime& Runtime,
                                  ComponentHandle& ActiveCamera,
                                  EditorSelectionModel* SelectionModel)
{
    auto Workspace = Root.Add(SnAPI::UI::UIPanel("Editor.Workspace"));
    auto& WorkspacePanel = Workspace.Element();
    WorkspacePanel.ElementStyle().Apply("editor.workspace");
    WorkspacePanel.Width().Set(SnAPI::UI::Sizing::Fill());
    WorkspacePanel.Height().Set(SnAPI::UI::Sizing::Fill());
    WorkspacePanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto WorkspaceSplit = Workspace.Add(SnAPI::UI::UIDockZone{});
    auto& WorkspaceSplitElement = WorkspaceSplit.Element();
    ConfigureSplitZone(WorkspaceSplitElement, SnAPI::UI::EDockSplit::Horizontal, kWorkspaceLeftSplitRatio, 210.0f, 360.0f);

    auto HierarchyHost = WorkspaceSplit.Add(SnAPI::UI::UIPanel("Editor.Workspace.HierarchyHost"));
    ConfigureHostPanel(HierarchyHost.Element());
    BuildHierarchyPane(HierarchyHost, Runtime, ActiveCamera, SelectionModel);

    auto CenterRightHost = WorkspaceSplit.Add(SnAPI::UI::UIPanel("Editor.Workspace.CenterRightHost"));
    ConfigureHostPanel(CenterRightHost.Element());

    auto CenterRightSplit = CenterRightHost.Add(SnAPI::UI::UIDockZone{});
    auto& CenterRightSplitElement = CenterRightSplit.Element();
    ConfigureSplitZone(CenterRightSplitElement, SnAPI::UI::EDockSplit::Horizontal, kWorkspaceCenterSplitRatio, 340.0f, 220.0f);

    auto GameHost = CenterRightSplit.Add(SnAPI::UI::UIPanel("Editor.Workspace.GameHost"));
    ConfigureHostPanel(GameHost.Element());
    BuildGamePane(GameHost, Runtime, ActiveCamera);

    auto InspectorHost = CenterRightSplit.Add(SnAPI::UI::UIPanel("Editor.Workspace.InspectorHost"));
    ConfigureHostPanel(InspectorHost.Element());
    BuildInspectorPane(InspectorHost, ResolveSelectedNode(Runtime, ActiveCamera), Runtime, ActiveCamera);
}

void EditorLayout::BuildContentBrowser(PanelBuilder& Root)
{
    m_contentAssetCardButtons.clear();
    m_contentAssetCardIndices.clear();
    m_contentBrowserEntries.clear();
    m_contentSearchInput = {};
    m_contentPathBreadcrumbs = {};
    m_contentAssetNameValue = {};
    m_contentAssetTypeValue = {};
    m_contentAssetVariantValue = {};
    m_contentAssetIdValue = {};
    m_contentAssetStatusValue = {};
    m_contentPlaceButton = {};
    m_contentSaveButton = {};
    m_contentAssetsList = {};
    m_contentAssetsEmptyHint = {};
    m_contentAssetCards.clear();

    auto ContentBrowser = Root.Add(SnAPI::UI::UIPanel("Editor.ContentBrowser"));
    auto& ContentPanel = ContentBrowser.Element();
    ContentPanel.ElementStyle().Apply("editor.content_browser");
    ContentPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ContentPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ContentPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto HeaderRow = ContentBrowser.Add(SnAPI::UI::UIPanel("Editor.ContentHeader"));
    auto& HeaderPanel = HeaderRow.Element();
    HeaderPanel.ElementStyle().Apply("editor.content_header");
    HeaderPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HeaderPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto BrowserIcon = HeaderRow.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kContentBrowserIconPath)));
    auto& BrowserIconImage = BrowserIcon.Element();
    ConfigureSvgIcon(
        BrowserIconImage,
        14.0f,
        kIconWhite,
        SnAPI::UI::Margin{1.0f, 0.0f, 4.0f, 0.0f});

    auto Path = HeaderRow.Add(SnAPI::UI::UIBreadcrumbs{});
    auto& PathElement = Path.Element();
    PathElement.ElementStyle().Apply("editor.browser_path");
    PathElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    PathElement.OnCrumbClicked(SnAPI::UI::TDelegate<void(uint32_t, const std::string&)>::Bind(
        [this](const uint32_t Index, const std::string&) {
            const auto Segments = SplitBrowserPath(m_contentCurrentFolder);
            if (Index <= 1u)
            {
                m_contentCurrentFolder.clear();
            }
            else
            {
                const std::size_t SegmentCount = std::min<std::size_t>(Segments.size(), static_cast<std::size_t>(Index - 1u));
                std::string NextPath{};
                for (std::size_t SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
                {
                    if (!NextPath.empty())
                    {
                        NextPath += "/";
                    }
                    NextPath += Segments[SegmentIndex];
                }
                m_contentCurrentFolder = std::move(NextPath);
            }

            ApplyContentAssetFilter();
            RefreshContentAssetCardSelectionStyles();
            RefreshContentAssetDetailsViewModel();
        }));
    m_contentPathBreadcrumbs = Path.Handle();
    RefreshContentBrowserPath();

    auto HeaderSearch = HeaderRow.Add(SnAPI::UI::UITextInput{});
    auto& HeaderSearchInput = HeaderSearch.Element();
    HeaderSearchInput.ElementStyle().Apply("editor.search");
    HeaderSearchInput.Width().Set(SnAPI::UI::Sizing::Ratio(0.45f));
    HeaderSearchInput.Placeholder().Set(std::string("Search assets..."));
    auto vmContentFilterText = ViewModelProperty<std::string>(kVmContentFilterTextKey);
    HeaderSearchInput.Text().BindTo(vmContentFilterText, SnAPI::UI::EBindMode::TwoWay);
    m_contentSearchInput = HeaderSearch.Handle();

    auto RefreshButton = HeaderRow.Add(SnAPI::UI::UIButton{});
    auto& RefreshButtonElement = RefreshButton.Element();
    RefreshButtonElement.ElementStyle().Apply("editor.toolbar_button");
    RefreshButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    RefreshButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    RefreshButtonElement.ElementPadding().Set(SnAPI::UI::Padding{6.0f, 3.0f, 6.0f, 3.0f});
    RefreshButtonElement.OnClick([this]() {
        if (m_onContentAssetRefreshRequested)
        {
            m_onContentAssetRefreshRequested();
        }
    });

    auto RefreshContent = RefreshButton.Add(SnAPI::UI::UIPanel("Editor.ContentRefreshContent"));
    auto& RefreshContentPanel = RefreshContent.Element();
    RefreshContentPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    RefreshContentPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    RefreshContentPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    RefreshContentPanel.Gap().Set(4.0f);
    RefreshContentPanel.Padding().Set(0.0f);
    RefreshContentPanel.Background().Set(SnAPI::UI::Color::Transparent());
    RefreshContentPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    RefreshContentPanel.BorderThickness().Set(0.0f);
    RefreshContentPanel.CornerRadius().Set(0.0f);
    RefreshContentPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto RefreshIcon = RefreshContent.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kRescanIconPath)));
    auto& RefreshIconImage = RefreshIcon.Element();
    ConfigureSvgIcon(RefreshIconImage, 12.0f, kIconWhite);

    auto RefreshLabel = RefreshContent.Add(SnAPI::UI::UIText("Rescan"));
    RefreshLabel.Element().ElementStyle().Apply("editor.toolbar_button_text");
    RefreshLabel.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);

    auto BrowserTabs = ContentBrowser.Add(SnAPI::UI::UITabs{});
    auto& BrowserTabsElement = BrowserTabs.Element();
    BrowserTabsElement.ElementStyle().Apply("editor.browser_tabs");
    BrowserTabsElement.Width().Set(SnAPI::UI::Sizing::Fill());
    BrowserTabsElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    BrowserTabsElement.HeaderHeight().Set(28.0f);

    auto AssetsTab = BrowserTabs.Add(SnAPI::UI::UIPanel("Editor.ContentTab.Assets"));
    auto& AssetsTabPanel = AssetsTab.Element();
    AssetsTabPanel.ElementStyle().Apply("editor.section_card");
    AssetsTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    AssetsTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    AssetsTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    AssetsTabPanel.Padding().Set(6.0f);
    AssetsTabPanel.Gap().Set(6.0f);

    auto AssetsList = AssetsTab.Add(SnAPI::UI::UIListView{});
    auto& AssetsListElement = AssetsList.Element();
    AssetsListElement.Orientation().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    AssetsListElement.ItemExtent().Set(152.0f);
    AssetsListElement.ItemGap().Set(10.0f);
    AssetsListElement.Virtualized().Set(false);
    AssetsListElement.Width().Set(SnAPI::UI::Sizing::Fill());
    AssetsListElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    AssetsListElement.ElementStyle().Apply("editor.browser_list");
    AssetsListElement.OnContextMenuRequested(
        SnAPI::UI::TDelegate<void(int32_t, const SnAPI::UI::PointerEvent&)>::Bind(
            [this](const int32_t ItemIndex, const SnAPI::UI::PointerEvent& Event) {
                if (ItemIndex >= 0 && static_cast<std::size_t>(ItemIndex) < m_contentAssetCards.size())
                {
                    if (const auto CardIndex = static_cast<std::size_t>(ItemIndex);
                        m_context && m_contentAssetCards[CardIndex].Button.Id.Value != 0)
                    {
                        auto* CardButton = dynamic_cast<SnAPI::UI::UIButton*>(
                            &m_context->GetElement(m_contentAssetCards[CardIndex].Button.Id));
                        if (CardButton && !CardButton->IsCollapsed())
                        {
                            OpenContentAssetContextMenu(CardIndex, Event);
                            return;
                        }
                    }
                }

                OpenContentBrowserContextMenu(Event);
            }));
    m_contentAssetsList = AssetsList.Handle();

    auto EmptyHint = AssetsTab.Add(SnAPI::UI::UIText("No assets discovered. Click Rescan to search for source assets."));
    EmptyHint.Element().ElementStyle().Apply("editor.panel_subtitle");
    EmptyHint.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_contentAssetsEmptyHint = EmptyHint.Handle();

    EnsureContentAssetCardCapacity();
    UpdateContentAssetCardWidgets();

    auto BrowserPagination = AssetsTab.Add(SnAPI::UI::UIPagination{});
    auto& BrowserPaginationElement = BrowserPagination.Element();
    BrowserPaginationElement.ElementStyle().Apply("editor.browser_pagination");
    BrowserPaginationElement.PageCount().Set(1);
    BrowserPaginationElement.VisibleButtonCount().Set(1);
    BrowserPaginationElement.Width().Set(SnAPI::UI::Sizing::Fill());

    auto DetailsTab = BrowserTabs.Add(SnAPI::UI::UIPanel("Editor.ContentTab.Details"));
    auto& DetailsTabPanel = DetailsTab.Element();
    DetailsTabPanel.ElementStyle().Apply("editor.section_card");
    DetailsTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    DetailsTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    DetailsTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    DetailsTabPanel.Padding().Set(6.0f);
    DetailsTabPanel.Gap().Set(6.0f);

    BuildContentDetailsPane(DetailsTab);

    auto CollectionsTab = BrowserTabs.Add(SnAPI::UI::UIPanel("Editor.ContentTab.Collections"));
    auto& CollectionsTabPanel = CollectionsTab.Element();
    CollectionsTabPanel.ElementStyle().Apply("editor.section_card");
    CollectionsTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    CollectionsTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    CollectionsTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    CollectionsTabPanel.Padding().Set(6.0f);
    CollectionsTabPanel.Gap().Set(6.0f);

    auto Tags = CollectionsTab.Add(SnAPI::UI::UITokenField{});
    auto& TagsElement = Tags.Element();
    TagsElement.ElementStyle().Apply("editor.token_field");
    TagsElement.Width().Set(SnAPI::UI::Sizing::Fill());
    TagsElement.AddToken("Environment", false);
    TagsElement.AddToken("Gameplay", false);
    TagsElement.AddToken("Favorites", false);

    auto Palette = CollectionsTab.Add(SnAPI::UI::UIColorPicker{});
    auto& PaletteElement = Palette.Element();
    PaletteElement.ElementStyle().Apply("editor.color_picker");
    PaletteElement.Width().Set(SnAPI::UI::Sizing::Fill());
    PaletteElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    BrowserTabsElement.SetTabLabel(0, "Assets");
    BrowserTabsElement.SetTabLabel(1, "Details");
    BrowserTabsElement.SetTabLabel(2, "Collections");

    ApplyContentAssetFilter();
    RefreshContentAssetCardSelectionStyles();
    RefreshContentAssetDetailsViewModel();
}

void EditorLayout::EnsureContextMenuOverlay()
{
    if (!m_context || m_contextMenu.Id.Value != 0)
    {
        return;
    }

    auto Root = m_context->Root();
    auto ContextMenu = Root.Add(SnAPI::UI::UIContextMenu{});
    auto& ContextMenuElement = ContextMenu.Element();
    ContextMenuElement.Width().Set(SnAPI::UI::Sizing::Fixed(0.0f));
    ContextMenuElement.Height().Set(SnAPI::UI::Sizing::Fixed(0.0f));
    ContextMenuElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    ContextMenuElement.ClampToViewport().Set(false);
    ContextMenuElement.MinMenuWidth().Set(196.0f);
    ContextMenuElement.MaxMenuWidth().Set(360.0f);
    ContextMenuElement.ItemHeight().Set(26.0f);
    ContextMenuElement.PaddingX().Set(10.0f);
    ContextMenuElement.PaddingY().Set(6.0f);
    ContextMenuElement.RowGap().Set(2.0f);
    ContextMenuElement.BackgroundColor().Set(SnAPI::UI::Color::RGBA(20, 24, 31, 248));
    ContextMenuElement.BorderColor().Set(SnAPI::UI::Color::RGBA(74, 82, 94, 236));
    ContextMenuElement.BorderThickness().Set(1.0f);
    ContextMenuElement.CornerRadius().Set(6.0f);
    ContextMenuElement.ItemHoverColor().Set(SnAPI::UI::Color::RGBA(56, 64, 77, 240));
    ContextMenuElement.ItemSelectedColor().Set(SnAPI::UI::Color::RGBA(67, 57, 42, 236));
    ContextMenuElement.ItemPressedColor().Set(SnAPI::UI::Color::RGBA(80, 71, 53, 244));
    ContextMenuElement.TextColor().Set(SnAPI::UI::Color::RGB(228, 234, 245));
    ContextMenuElement.DisabledTextColor().Set(SnAPI::UI::Color::RGB(122, 129, 140));
    ContextMenuElement.ShortcutColor().Set(SnAPI::UI::Color::RGB(164, 173, 188));
    ContextMenuElement.SeparatorColor().Set(SnAPI::UI::Color::RGBA(96, 104, 117, 224));
    ContextMenuElement.CheckColor().Set(SnAPI::UI::Color::RGB(218, 203, 162));
    ContextMenuElement.OnItemInvoked(
        SnAPI::UI::TDelegate<void(int32_t, const SnAPI::UI::UIContextMenuItem&)>::Bind(
            [this](const int32_t, const SnAPI::UI::UIContextMenuItem& Item) { OnContextMenuItemInvoked(Item); }));
    ContextMenuElement.OnClosed(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        const EPendingHierarchyMenu PendingMenu = m_pendingHierarchyMenu;
        const std::optional<std::size_t> PendingHierarchyIndex = m_pendingHierarchyMenuIndex;
        const SnAPI::UI::UIPoint PendingPosition = m_pendingHierarchyMenuOpenPosition;
        m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
        m_pendingHierarchyMenuIndex.reset();
        m_pendingHierarchyMenuOpenPosition = {};

        CloseContextMenu();

        if (PendingMenu == EPendingHierarchyMenu::None || !PendingHierarchyIndex.has_value())
        {
            return;
        }

        const std::size_t ItemIndex = *PendingHierarchyIndex;
        if (ItemIndex >= m_hierarchyVisibleNodes.size())
        {
            return;
        }

        m_contextMenuScope = EContextMenuScope::HierarchyItem;
        m_contextMenuHierarchyIndex = ItemIndex;
        m_contextMenuOpenPosition = PendingPosition;

        if (PendingMenu == EPendingHierarchyMenu::Root)
        {
            SnAPI::UI::PointerEvent Event{};
            Event.Position = PendingPosition;
            OpenHierarchyContextMenu(ItemIndex, Event);
            return;
        }

        OpenHierarchyAddTypeMenu(PendingMenu == EPendingHierarchyMenu::AddComponentTypes);
    }));
    m_contextMenu = ContextMenu.Handle();
}

void EditorLayout::EnsureContentAssetCreateModalOverlay()
{
    if (!m_context || m_contentCreateModalOverlay.Id.Value != 0)
    {
        return;
    }

    if (!m_contentCreateTypeSource)
    {
        m_contentCreateTypeSource = std::make_shared<VectorTreeItemSource>();
    }

    auto Root = m_context->Root();
    auto Overlay = Root.Add(SnAPI::UI::UIModal{});
    auto& OverlayPanel = Overlay.Element();
    OverlayPanel.CloseOnBackdropClick().Set(false);
    OverlayPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    OverlayPanel.Movable().Set(true);
    OverlayPanel.Resizable().Set(true);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.ResizeBorderThickness().Set(12.0f);
    OverlayPanel.BackdropColor().Set(SnAPI::UI::Color::RGBA(6, 8, 12, 218));
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 22, 30, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(87, 97, 112, 245));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(8.0f);
    OverlayPanel.ContentPadding().Set(10.0f);
    ConfigureModalScreenRatio(OverlayPanel, kDefaultModalScreenRatio);
    m_contentCreateModalOverlay = Overlay.Handle();

    auto Modal = Overlay.Add(SnAPI::UI::UIPanel("Editor.ContentCreateModal"));
    auto& ModalPanel = Modal.Element();
    ModalPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ModalPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Padding().Set(0.0f);
    ModalPanel.Gap().Set(10.0f);
    ModalPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ModalPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ModalPanel.BorderThickness().Set(0.0f);
    ModalPanel.CornerRadius().Set(0.0f);
    ModalPanel.DropShadowColor().Set(SnAPI::UI::Color::Transparent());
    ModalPanel.DropShadowBlur().Set(0.0f);
    ModalPanel.DropShadowSpread().Set(0.0f);
    ModalPanel.DropShadowOffsetX().Set(0.0f);
    ModalPanel.DropShadowOffsetY().Set(0.0f);

    auto Title = Modal.Add(SnAPI::UI::UIText("Create Asset"));
    auto& TitleText = Title.Element();
    TitleText.ElementStyle().Apply("editor.panel_title");
    TitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);

    auto Subtitle = Modal.Add(SnAPI::UI::UIText("Select a node class or built-in asset type, set the asset name, then click Create."));
    auto& SubtitleText = Subtitle.Element();
    SubtitleText.ElementStyle().Apply("editor.panel_subtitle");
    SubtitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto Search = Modal.Add(SnAPI::UI::UITextInput{});
    auto& SearchInput = Search.Element();
    SearchInput.ElementStyle().Apply("editor.search");
    SearchInput.Width().Set(SnAPI::UI::Sizing::Fill());
    SearchInput.Resizable().Set(false);
    SearchInput.Placeholder().Set(std::string("Filter asset types..."));
    auto vmContentCreateTypeFilter = ViewModelProperty<std::string>(kVmContentCreateTypeFilterKey);
    SearchInput.Text().BindTo(vmContentCreateTypeFilter, SnAPI::UI::EBindMode::TwoWay);
    m_contentCreateSearchInput = Search.Handle();

    auto Tree = Modal.Add(SnAPI::UI::UITreeView{});
    auto& TreeElement = Tree.Element();
    TreeElement.ElementStyle().Apply("editor.tree");
    TreeElement.Width().Set(SnAPI::UI::Sizing::Fill());
    TreeElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    TreeElement.RowHeight().Set(38.0f);
    TreeElement.IndentWidth().Set(20.0f);
    TreeElement.PaddingX().Set(6.0f);
    TreeElement.PaddingY().Set(4.0f);
    TreeElement.SetItemSource(m_contentCreateTypeSource.get());
    TreeElement.OnSelectionChanged(SnAPI::UI::TDelegate<void(int32_t)>::Bind([this](const int32_t ItemIndex) {
        if (ItemIndex < 0 || static_cast<std::size_t>(ItemIndex) >= m_contentCreateVisibleTypes.size())
        {
            m_contentCreateSelectedType = {};
            RefreshContentAssetCreateOkButtonState();
            return;
        }

        m_contentCreateSelectedType = m_contentCreateVisibleTypes[static_cast<std::size_t>(ItemIndex)];
        RefreshContentAssetCreateOkButtonState();
    }));
    m_contentCreateTypeTree = Tree.Handle();

    auto NameRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ContentCreate.NameRow"));
    auto& NameRowPanel = NameRow.Element();
    NameRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    NameRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    NameRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    NameRowPanel.Gap().Set(4.0f);
    NameRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
    NameRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    NameRowPanel.BorderThickness().Set(0.0f);
    NameRowPanel.CornerRadius().Set(0.0f);

    auto NameLabel = NameRow.Add(SnAPI::UI::UIText("Asset Name"));
    auto& NameLabelText = NameLabel.Element();
    NameLabelText.ElementStyle().Apply("editor.menu_item");
    NameLabelText.Width().Set(SnAPI::UI::Sizing::Auto());
    NameLabelText.HAlign().Set(SnAPI::UI::EAlignment::Start);

    auto NameInputBuilder = NameRow.Add(SnAPI::UI::UITextInput{});
    auto& NameInput = NameInputBuilder.Element();
    NameInput.ElementStyle().Apply("editor.text_input");
    NameInput.Width().Set(SnAPI::UI::Sizing::Fill());
    NameInput.Resizable().Set(false);
    NameInput.Multiline().Set(false);
    NameInput.AcceptTab().Set(false);
    NameInput.Placeholder().Set("NewAsset");
    auto vmContentCreateAssetName = ViewModelProperty<std::string>(kVmContentCreateAssetNameKey);
    NameInput.Text().BindTo(vmContentCreateAssetName, SnAPI::UI::EBindMode::TwoWay);
    NameInput.OnSubmit(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string&) {
        ConfirmContentAssetCreate();
    }));
    m_contentCreateNameInput = NameInputBuilder.Handle();

    auto ButtonsRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ContentCreate.Buttons"));
    auto& ButtonsRowPanel = ButtonsRow.Element();
    ButtonsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ButtonsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ButtonsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ButtonsRowPanel.Gap().Set(8.0f);
    ButtonsRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ButtonsRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ButtonsRowPanel.BorderThickness().Set(0.0f);
    ButtonsRowPanel.CornerRadius().Set(0.0f);

    auto Spacer = ButtonsRow.Add(SnAPI::UI::UIPanel("Editor.ContentCreate.ButtonSpacer"));
    auto& SpacerPanel = Spacer.Element();
    ConfigureLayoutSpacerPanel(SpacerPanel);
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto CancelButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& CancelButtonElement = CancelButton.Element();
    CancelButtonElement.ElementStyle().Apply("editor.toolbar_button");
    CancelButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    CancelButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    CancelButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    CancelButtonElement.OnClick([this]() {
        CloseContentAssetCreateModal();
    });
    auto CancelLabel = CancelButton.Add(SnAPI::UI::UIText("Cancel"));
    CancelLabel.Element().ElementStyle().Apply("editor.toolbar_button_text");

    auto CreateButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& CreateButtonElement = CreateButton.Element();
    CreateButtonElement.ElementStyle().Apply("editor.toolbar_button");
    CreateButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    CreateButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    CreateButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    CreateButtonElement.OnClick([this]() {
        ConfirmContentAssetCreate();
    });
    auto CreateLabel = CreateButton.Add(SnAPI::UI::UIText("Create"));
    CreateLabel.Element().ElementStyle().Apply("editor.toolbar_button_text");
    m_contentCreateOkButton = CreateButton.Handle();
}

void EditorLayout::EnsureContentAssetImportModalOverlay()
{
    if (!m_context || m_contentImportModalOverlay.Id.Value != 0)
    {
        return;
    }

    auto Root = m_context->Root();
    auto Overlay = Root.Add(SnAPI::UI::UIModal{});
    auto& OverlayPanel = Overlay.Element();
    OverlayPanel.CloseOnBackdropClick().Set(false);
    OverlayPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    OverlayPanel.Movable().Set(true);
    OverlayPanel.Resizable().Set(true);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.ResizeBorderThickness().Set(12.0f);
    OverlayPanel.BackdropColor().Set(SnAPI::UI::Color::RGBA(8, 10, 14, 216));
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(20, 25, 35, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(90, 102, 124, 245));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(8.0f);
    OverlayPanel.ContentPadding().Set(10.0f);
    ConfigureModalScreenRatio(OverlayPanel, kDefaultModalScreenRatio);
    m_contentImportModalOverlay = Overlay.Handle();

    auto Modal = Overlay.Add(SnAPI::UI::UIPanel("Editor.ContentImportModal"));
    auto& ModalPanel = Modal.Element();
    ModalPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ModalPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Padding().Set(0.0f);
    ModalPanel.Gap().Set(8.0f);
    ModalPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ModalPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ModalPanel.BorderThickness().Set(0.0f);
    ModalPanel.CornerRadius().Set(0.0f);
    ModalPanel.DropShadowColor().Set(SnAPI::UI::Color::Transparent());
    ModalPanel.DropShadowBlur().Set(0.0f);
    ModalPanel.DropShadowSpread().Set(0.0f);
    ModalPanel.DropShadowOffsetX().Set(0.0f);
    ModalPanel.DropShadowOffsetY().Set(0.0f);

    auto Title = Modal.Add(SnAPI::UI::UIText("Import Source Asset"));
    auto& TitleText = Title.Element();
    TitleText.ElementStyle().Apply("editor.panel_title");
    TitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);

    auto Summary = Modal.Add(SnAPI::UI::UIText("Select a source file to import into the current content folder."));
    auto& SummaryText = Summary.Element();
    SummaryText.ElementStyle().Apply("editor.panel_subtitle");
    SummaryText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_contentImportSummaryText = Summary.Handle();

    auto SourceLabel = Modal.Add(SnAPI::UI::UIText("Source File"));
    SourceLabel.Element().ElementStyle().Apply("editor.menu_item");

    auto SourcePicker = Modal.Add(SnAPI::UI::UIFilesystemPicker{});
    auto& SourcePickerElement = SourcePicker.Element();
    SourcePickerElement.ElementStyle().Apply("editor.filesystem_picker");
    SourcePickerElement.Width().Set(SnAPI::UI::Sizing::Fill());
    SourcePickerElement.Height().Set(SnAPI::UI::Sizing::Auto());
    SourcePickerElement.ReadOnly().Set(false);
    SourcePickerElement.AllowMultiSelect().Set(false);
    SourcePickerElement.PickDirectories().Set(false);
    SourcePickerElement.ShowDirectories().Set(true);
    SourcePickerElement.ShowFiles().Set(true);
    SourcePickerElement.RestrictToRoot().Set(false);
    SourcePickerElement.Placeholder().Set("Path to source asset (fbx, glb, png, tiff, ...)");
    SourcePickerElement.Value().Set(m_contentImportSourcePath);
    if (!m_contentImportSourcePath.empty())
    {
        SourcePickerElement.CurrentPath().Set(std::filesystem::path(m_contentImportSourcePath).parent_path().string());
    }
    else
    {
        std::error_code Error{};
        const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
        if (!Error)
        {
            SourcePickerElement.CurrentPath().Set(CurrentPath.string());
        }
    }

    std::vector<std::string> AllowedExtensions{};
    AllowedExtensions.reserve(kImportModelExtensions.size() + kImportTextureExtensions.size());
    for (const std::string_view Ext : kImportModelExtensions)
    {
        AllowedExtensions.emplace_back(Ext);
    }
    for (const std::string_view Ext : kImportTextureExtensions)
    {
        AllowedExtensions.emplace_back(Ext);
    }
    SourcePickerElement.SetAllowedExtensions(std::move(AllowedExtensions));
    SourcePickerElement.OnSelectionChanged(
        SnAPI::UI::TDelegate<void(const std::vector<std::string>&)>::Bind(
            [this](const std::vector<std::string>& Values) {
                if (!Values.empty())
                {
                    m_contentImportSourcePath = Values.front();
                }
                RefreshContentAssetImportProfile();
                RefreshContentAssetImportSettingsPanel();
                RefreshContentAssetImportSummary();
                RefreshContentAssetImportOkButtonState();
            }));
    m_contentImportSourcePicker = SourcePicker.Handle();

    auto SettingsTitle = Modal.Add(SnAPI::UI::UIText("Import Settings"));
    SettingsTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto SettingsHost = Modal.Add(SnAPI::UI::UIPanel("Editor.ContentImport.SettingsHost"));
    auto& SettingsHostPanel = SettingsHost.Element();
    ConfigureHostPanel(SettingsHostPanel);
    SettingsHostPanel.ElementStyle().Apply("editor.section_card");
    SettingsHostPanel.Padding().Set(6.0f);
    SettingsHostPanel.Gap().Set(6.0f);
    SettingsHostPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    SettingsHostPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto PropertyPanelBuilder = SettingsHost.Add(UIPropertyPanel{});
    auto& PropertyPanel = PropertyPanelBuilder.Element();
    PropertyPanel.ElementStyle().Apply("editor.inspector_properties");
    PropertyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PropertyPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    PropertyPanel.ShowHorizontalScrollbar().Set(false);
    PropertyPanel.ShowVerticalScrollbar().Set(true);
    PropertyPanel.Smooth().Set(true);
    m_contentImportSettingsPanel = PropertyPanelBuilder.Handle();

    auto ButtonsRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ContentImport.Buttons"));
    auto& ButtonsRowPanel = ButtonsRow.Element();
    ButtonsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ButtonsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ButtonsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ButtonsRowPanel.Gap().Set(8.0f);
    ButtonsRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ButtonsRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ButtonsRowPanel.BorderThickness().Set(0.0f);
    ButtonsRowPanel.CornerRadius().Set(0.0f);

    auto Spacer = ButtonsRow.Add(SnAPI::UI::UIPanel("Editor.ContentImport.ButtonSpacer"));
    auto& SpacerPanel = Spacer.Element();
    ConfigureLayoutSpacerPanel(SpacerPanel);
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto CancelButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& CancelButtonElement = CancelButton.Element();
    CancelButtonElement.ElementStyle().Apply("editor.toolbar_button");
    CancelButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    CancelButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    CancelButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    CancelButtonElement.OnClick([this]() {
        CloseContentAssetImportModal();
    });
    auto CancelLabel = CancelButton.Add(SnAPI::UI::UIText("Cancel"));
    CancelLabel.Element().ElementStyle().Apply("editor.toolbar_button_text");

    auto ImportButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& ImportButtonElement = ImportButton.Element();
    ImportButtonElement.ElementStyle().Apply("editor.toolbar_button");
    ImportButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    ImportButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    ImportButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    ImportButtonElement.OnClick([this]() {
        ConfirmContentAssetImport();
    });
    auto ImportLabel = ImportButton.Add(SnAPI::UI::UIText("Import"));
    ImportLabel.Element().ElementStyle().Apply("editor.toolbar_button_text");
    m_contentImportOkButton = ImportButton.Handle();

    RefreshContentAssetImportProfile();
    RefreshContentAssetImportSettingsPanel();
    RefreshContentAssetImportSummary();
    RefreshContentAssetImportOkButtonState();
}

void EditorLayout::DestroyContentAssetImportModalOverlay()
{
    if (m_context && m_contentImportModalOverlay.Id.Value != 0)
    {
        const SnAPI::UI::ElementId OverlayId = m_contentImportModalOverlay.Id;
        const SnAPI::UI::ElementId CapturedElement = m_context->GetCapture();
        if (IsElementWithinSubtree(*m_context, CapturedElement, OverlayId))
        {
            m_context->ReleaseCapture();
        }

        m_context->DestroyElement(OverlayId);
    }

    m_contentImportModalOverlay = {};
    m_contentImportSourcePicker = {};
    m_contentImportSummaryText = {};
    m_contentImportSettingsPanel = {};
    m_contentImportOkButton = {};
}

void EditorLayout::EnsureContentAssetInspectorModalOverlay()
{
    if (!m_context || m_contentInspectorModalOverlay.Id.Value != 0)
    {
        return;
    }

    if (!m_contentInspectorHierarchySource)
    {
        m_contentInspectorHierarchySource = std::make_shared<VectorTreeItemSource>();
    }

    auto Root = m_context->Root();
    auto Overlay = Root.Add(SnAPI::UI::UIModal{});
    auto& OverlayPanel = Overlay.Element();
    OverlayPanel.CloseOnBackdropClick().Set(false);
    OverlayPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    OverlayPanel.Movable().Set(true);
    OverlayPanel.Resizable().Set(true);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.ResizeBorderThickness().Set(12.0f);
    OverlayPanel.BackdropColor().Set(SnAPI::UI::Color::RGBA(7, 10, 15, 214));
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 23, 32, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(84, 97, 117, 242));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(8.0f);
    OverlayPanel.ContentPadding().Set(10.0f);
    ConfigureModalScreenRatio(OverlayPanel, kDefaultModalScreenRatio);
    m_contentInspectorModalOverlay = Overlay.Handle();

    auto Modal = Overlay.Add(SnAPI::UI::UIPanel("Editor.ContentInspectorModal"));
    auto& ModalPanel = Modal.Element();
    ModalPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ModalPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Padding().Set(0.0f);
    ModalPanel.Gap().Set(8.0f);
    ModalPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ModalPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ModalPanel.BorderThickness().Set(0.0f);
    ModalPanel.CornerRadius().Set(0.0f);
    ModalPanel.DropShadowColor().Set(SnAPI::UI::Color::Transparent());
    ModalPanel.DropShadowBlur().Set(0.0f);
    ModalPanel.DropShadowSpread().Set(0.0f);
    ModalPanel.DropShadowOffsetX().Set(0.0f);
    ModalPanel.DropShadowOffsetY().Set(0.0f);

    auto Title = Modal.Add(SnAPI::UI::UIText("Asset Inspector"));
    auto& TitleText = Title.Element();
    TitleText.ElementStyle().Apply("editor.panel_title");
    TitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
    m_contentInspectorTitleText = Title.Handle();

    auto Status = Modal.Add(SnAPI::UI::UIText("Double-click an asset to inspect and edit properties."));
    auto& StatusText = Status.Element();
    StatusText.ElementStyle().Apply("editor.panel_subtitle");
    StatusText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_contentInspectorStatusText = Status.Handle();

    auto BodySplit = Modal.Add(SnAPI::UI::UIDockZone{});
    auto& BodySplitElement = BodySplit.Element();
    ConfigureSplitZone(BodySplitElement, SnAPI::UI::EDockSplit::Horizontal, 0.32f, 240.0f, 300.0f);
    BodySplitElement.Width().Set(SnAPI::UI::Sizing::Fill());
    BodySplitElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto HierarchyHost = BodySplit.Add(SnAPI::UI::UIPanel("Editor.ContentInspector.HierarchyHost"));
    auto& HierarchyHostPanel = HierarchyHost.Element();
    ConfigureHostPanel(HierarchyHostPanel);
    HierarchyHostPanel.ElementStyle().Apply("editor.sidebar");
    HierarchyHostPanel.Padding().Set(6.0f);
    HierarchyHostPanel.Gap().Set(6.0f);

    auto HierarchyTitle = HierarchyHost.Add(SnAPI::UI::UIText("Asset Hierarchy"));
    HierarchyTitle.Element().ElementStyle().Apply("editor.panel_title");
    m_contentInspectorHierarchyTitleText = HierarchyTitle.Handle();

    auto PreviewStats = HierarchyHost.Add(SnAPI::UI::UIText{});
    auto& PreviewStatsText = PreviewStats.Element();
    PreviewStatsText.ElementStyle().Apply("editor.panel_subtitle");
    PreviewStatsText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    PreviewStatsText.Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
    m_contentInspectorPreviewStatsText = PreviewStats.Handle();

    auto PreviewImageBuilder = HierarchyHost.Add(SnAPI::UI::UIImage{});
    auto& PreviewImage = PreviewImageBuilder.Element();
    PreviewImage.Width().Set(SnAPI::UI::Sizing::Fill());
    PreviewImage.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    PreviewImage.Mode().Set(SnAPI::UI::EImageMode::Aspect);
    PreviewImage.LazyLoad().Set(false);
    PreviewImage.HAlign().Set(SnAPI::UI::EAlignment::Center);
    PreviewImage.VAlign().Set(SnAPI::UI::EAlignment::Center);
    PreviewImage.Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
    m_contentInspectorPreviewImage = PreviewImageBuilder.Handle();

    auto HierarchyTree = HierarchyHost.Add(SnAPI::UI::UITreeView{});
    auto& HierarchyTreeElement = HierarchyTree.Element();
    HierarchyTreeElement.ElementStyle().Apply("editor.tree");
    HierarchyTreeElement.Width().Set(SnAPI::UI::Sizing::Fill());
    HierarchyTreeElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    HierarchyTreeElement.RowHeight().Set(32.0f);
    HierarchyTreeElement.IndentWidth().Set(20.0f);
    HierarchyTreeElement.PaddingX().Set(6.0f);
    HierarchyTreeElement.PaddingY().Set(4.0f);
    HierarchyTreeElement.SetItemSource(m_contentInspectorHierarchySource.get());
    HierarchyTreeElement.OnSelectionChanged(SnAPI::UI::TDelegate<void(int32_t)>::Bind([this](const int32_t ItemIndex) {
        if (ItemIndex < 0 || static_cast<std::size_t>(ItemIndex) >= m_contentInspectorVisibleNodes.size())
        {
            return;
        }

        m_contentAssetInspectorState.SelectedNode = m_contentInspectorVisibleNodes[static_cast<std::size_t>(ItemIndex)];
        if (m_onContentAssetInspectorNodeSelected)
        {
            m_onContentAssetInspectorNodeSelected(m_contentAssetInspectorState.SelectedNode);
        }
        RefreshContentAssetInspectorModalState();
        if (m_context)
        {
            m_context->MarkLayoutDirty();
        }
    }));
    HierarchyTreeElement.OnContextMenuRequested(
        SnAPI::UI::TDelegate<void(int32_t, const SnAPI::UI::UITreeItem&, const SnAPI::UI::PointerEvent&)>::Bind(
            [this](const int32_t ItemIndex, const SnAPI::UI::UITreeItem&, const SnAPI::UI::PointerEvent& Event) {
                if (ItemIndex < 0)
                {
                    return;
                }
                OpenContentAssetInspectorHierarchyContextMenu(static_cast<std::size_t>(ItemIndex), Event);
            }));
    m_contentInspectorHierarchyTree = HierarchyTree.Handle();

    auto InspectorHost = BodySplit.Add(SnAPI::UI::UIPanel("Editor.ContentInspector.PropertyHost"));
    auto& InspectorHostPanel = InspectorHost.Element();
    ConfigureHostPanel(InspectorHostPanel);
    InspectorHostPanel.ElementStyle().Apply("editor.section_card");
    InspectorHostPanel.Padding().Set(6.0f);
    InspectorHostPanel.Gap().Set(6.0f);

    auto RuntimeSettingsTitle = InspectorHost.Add(SnAPI::UI::UIText("Runtime Settings"));
    RuntimeSettingsTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto RuntimePropertyPanelBuilder = InspectorHost.Add(UIPropertyPanel{});
    auto& RuntimePropertyPanel = RuntimePropertyPanelBuilder.Element();
    RuntimePropertyPanel.ElementStyle().Apply("editor.inspector_properties");
    RuntimePropertyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    RuntimePropertyPanel.Height().Set(SnAPI::UI::Sizing::Ratio(0.62f));
    RuntimePropertyPanel.ShowHorizontalScrollbar().Set(false);
    RuntimePropertyPanel.ShowVerticalScrollbar().Set(true);
    RuntimePropertyPanel.Smooth().Set(true);
    RuntimePropertyPanel.SetComponentContextMenuHandler(
        SnAPI::UI::TDelegate<void(NodeHandle, const TypeId&, const SnAPI::UI::PointerEvent&)>::Bind(
            [this](const NodeHandle OwnerNode, const TypeId& ComponentType, const SnAPI::UI::PointerEvent& Event) {
                OpenContentAssetInspectorComponentContextMenu(OwnerNode, ComponentType, Event);
            }));
    m_contentInspectorPropertyPanel = RuntimePropertyPanelBuilder.Handle();

    auto ImportSettingsTitle = InspectorHost.Add(SnAPI::UI::UIText("Import Settings"));
    ImportSettingsTitle.Element().ElementStyle().Apply("editor.panel_title");
    m_contentInspectorImportSettingsTitleText = ImportSettingsTitle.Handle();

    auto ImportPropertyPanelBuilder = InspectorHost.Add(UIPropertyPanel{});
    auto& ImportPropertyPanel = ImportPropertyPanelBuilder.Element();
    ImportPropertyPanel.ElementStyle().Apply("editor.inspector_properties");
    ImportPropertyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ImportPropertyPanel.Height().Set(SnAPI::UI::Sizing::Ratio(0.38f));
    ImportPropertyPanel.ShowHorizontalScrollbar().Set(false);
    ImportPropertyPanel.ShowVerticalScrollbar().Set(true);
    ImportPropertyPanel.Smooth().Set(true);
    m_contentInspectorImportSettingsPanel = ImportPropertyPanelBuilder.Handle();

    auto ButtonsRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ContentInspector.Buttons"));
    auto& ButtonsRowPanel = ButtonsRow.Element();
    ButtonsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ButtonsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ButtonsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ButtonsRowPanel.Gap().Set(8.0f);
    ButtonsRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ButtonsRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ButtonsRowPanel.BorderThickness().Set(0.0f);
    ButtonsRowPanel.CornerRadius().Set(0.0f);

    auto Spacer = ButtonsRow.Add(SnAPI::UI::UIPanel("Editor.ContentInspector.ButtonSpacer"));
    auto& SpacerPanel = Spacer.Element();
    ConfigureLayoutSpacerPanel(SpacerPanel);
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto CloseButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& CloseButtonElement = CloseButton.Element();
    CloseButtonElement.ElementStyle().Apply("editor.toolbar_button");
    CloseButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    CloseButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    CloseButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    CloseButtonElement.OnClick([this]() {
        CloseContentAssetInspectorModal(true);
    });
    auto CloseLabel = CloseButton.Add(SnAPI::UI::UIText("Close"));
    CloseLabel.Element().ElementStyle().Apply("editor.toolbar_button_text");

    auto SaveButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& SaveButtonElement = SaveButton.Element();
    SaveButtonElement.ElementStyle().Apply("editor.toolbar_button");
    SaveButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    SaveButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    SaveButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    SaveButtonElement.OnClick([this]() {
        if (m_onContentAssetInspectorSaveRequested)
        {
            m_onContentAssetInspectorSaveRequested();
        }
    });
    auto SaveLabel = SaveButton.Add(SnAPI::UI::UIText("Save"));
    SaveLabel.Element().ElementStyle().Apply("editor.toolbar_button_text");
    m_contentInspectorSaveButton = SaveButton.Handle();

    auto ReimportButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& ReimportButtonElement = ReimportButton.Element();
    ReimportButtonElement.ElementStyle().Apply("editor.toolbar_button");
    ReimportButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    ReimportButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    ReimportButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    ReimportButtonElement.OnClick([this]() {
        if (m_onContentAssetInspectorReimportRequested)
        {
            m_onContentAssetInspectorReimportRequested();
        }
    });
    auto ReimportLabel = ReimportButton.Add(SnAPI::UI::UIText("Reimport"));
    ReimportLabel.Element().ElementStyle().Apply("editor.toolbar_button_text");
    m_contentInspectorReimportButton = ReimportButton.Handle();
}

void EditorLayout::BuildContentDetailsPane(PanelBuilder& DetailsTab)
{
    auto Instructions = DetailsTab.Add(SnAPI::UI::UIText("Double-click an asset to open the inspector. Edit Name + press Enter to rename. Click Place then click the viewport to instantiate."));
    Instructions.Element().ElementStyle().Apply("editor.panel_subtitle");
    Instructions.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto DetailsTable = DetailsTab.Add(SnAPI::UI::UITable{});
    auto& DetailsTableElement = DetailsTable.Element();
    DetailsTableElement.ElementStyle().Apply("editor.browser_table");
    DetailsTableElement.ColumnCount().Set(2u);
    DetailsTableElement.RowHeight().Set(28.0f);
    DetailsTableElement.HeaderHeight().Set(28.0f);
    DetailsTableElement.Width().Set(SnAPI::UI::Sizing::Fill());
    DetailsTableElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    DetailsTableElement.SetColumnHeaders({"Field", "Value"});

    auto NameFieldCell = DetailsTable.Add(SnAPI::UI::UIText("Name"));
    NameFieldCell.Element().ElementStyle().Apply("editor.menu_item");

    auto NameValueCell = DetailsTable.Add(SnAPI::UI::UITextInput{});
    auto& NameEditor = NameValueCell.Element();
    NameEditor.ElementStyle().Apply("editor.text_input");
    NameEditor.Multiline().Set(false);
    NameEditor.AcceptTab().Set(false);
    NameEditor.Placeholder().Set("Asset name");
    auto vmNameValue = ViewModelProperty<std::string>(kVmContentAssetNameKey);
    NameEditor.Text().BindTo(vmNameValue, SnAPI::UI::EBindMode::TwoWay);
    NameEditor.OnSubmit(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& NewName) {
        if (m_onContentAssetRenameRequested && !m_selectedContentAssetKey.empty())
        {
            m_onContentAssetRenameRequested(m_selectedContentAssetKey, NewName);
        }
    }));
    m_contentAssetNameValue = NameValueCell.Handle();

    const auto AddField = [&](const std::string_view Label,
                              const SnAPI::UI::PropertyKey ValueKey,
                              SnAPI::UI::ElementHandle<SnAPI::UI::UIText>& OutValueHandle) {
        auto FieldCell = DetailsTable.Add(SnAPI::UI::UIText(Label));
        FieldCell.Element().ElementStyle().Apply("editor.menu_item");

        auto ValueCell = DetailsTable.Add(SnAPI::UI::UIText("--"));
        ValueCell.Element().ElementStyle().Apply("editor.panel_title");
        ValueCell.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
        auto vmFieldValue = ViewModelProperty<std::string>(ValueKey);
        ValueCell.Element().Text().BindTo(vmFieldValue);
        OutValueHandle = ValueCell.Handle();
    };

    AddField("Type", kVmContentAssetTypeKey, m_contentAssetTypeValue);
    AddField("Variant", kVmContentAssetVariantKey, m_contentAssetVariantValue);
    AddField("Asset Id", kVmContentAssetIdKey, m_contentAssetIdValue);
    AddField("Status", kVmContentAssetStatusKey, m_contentAssetStatusValue);

    auto ActionsRow = DetailsTab.Add(SnAPI::UI::UIPanel("Editor.ContentActions"));
    auto& ActionsRowPanel = ActionsRow.Element();
    ActionsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ActionsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ActionsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ActionsRowPanel.Padding().Set(0.0f);
    ActionsRowPanel.Gap().Set(6.0f);
    ActionsRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ActionsRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ActionsRowPanel.BorderThickness().Set(0.0f);
    ActionsRowPanel.CornerRadius().Set(0.0f);

    auto PlaceButton = ActionsRow.Add(SnAPI::UI::UIButton{});
    auto& PlaceButtonElement = PlaceButton.Element();
    PlaceButtonElement.ElementStyle().Apply("editor.toolbar_button");
    PlaceButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    PlaceButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    PlaceButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    PlaceButtonElement.OnClick([this]() {
        if (m_onContentAssetPlaceRequested && !m_selectedContentAssetKey.empty())
        {
            m_onContentAssetPlaceRequested(m_selectedContentAssetKey);
        }
    });
    auto PlaceContent = PlaceButton.Add(SnAPI::UI::UIPanel("Editor.ContentAction.Place"));
    auto& PlaceContentPanel = PlaceContent.Element();
    PlaceContentPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    PlaceContentPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    PlaceContentPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    PlaceContentPanel.Gap().Set(4.0f);
    PlaceContentPanel.Padding().Set(0.0f);
    PlaceContentPanel.Background().Set(SnAPI::UI::Color::Transparent());
    PlaceContentPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    PlaceContentPanel.BorderThickness().Set(0.0f);
    PlaceContentPanel.CornerRadius().Set(0.0f);
    PlaceContentPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto PlaceIcon = PlaceContent.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kPlaceIconPath)));
    auto& PlaceIconImage = PlaceIcon.Element();
    ConfigureSvgIcon(PlaceIconImage, 12.0f, kIconWhite);

    auto PlaceLabel = PlaceContent.Add(SnAPI::UI::UIText("Place In Scene"));
    PlaceLabel.Element().ElementStyle().Apply("editor.toolbar_button_text");
    PlaceLabel.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
    m_contentPlaceButton = PlaceButton.Handle();

    auto SaveButton = ActionsRow.Add(SnAPI::UI::UIButton{});
    auto& SaveButtonElement = SaveButton.Element();
    SaveButtonElement.ElementStyle().Apply("editor.toolbar_button");
    SaveButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    SaveButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    SaveButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    SaveButtonElement.OnClick([this]() {
        if (m_onContentAssetSaveRequested && !m_selectedContentAssetKey.empty())
        {
            m_onContentAssetSaveRequested(m_selectedContentAssetKey);
        }
    });
    auto SaveContent = SaveButton.Add(SnAPI::UI::UIPanel("Editor.ContentAction.Save"));
    auto& SaveContentPanel = SaveContent.Element();
    SaveContentPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    SaveContentPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    SaveContentPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    SaveContentPanel.Gap().Set(4.0f);
    SaveContentPanel.Padding().Set(0.0f);
    SaveContentPanel.Background().Set(SnAPI::UI::Color::Transparent());
    SaveContentPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    SaveContentPanel.BorderThickness().Set(0.0f);
    SaveContentPanel.CornerRadius().Set(0.0f);
    SaveContentPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto SaveIcon = SaveContent.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kSaveIconPath)));
    auto& SaveIconImage = SaveIcon.Element();
    ConfigureSvgIcon(SaveIconImage, 12.0f, kIconWhite);

    auto SaveLabel = SaveContent.Add(SnAPI::UI::UIText("Save Update"));
    SaveLabel.Element().ElementStyle().Apply("editor.toolbar_button_text");
    SaveLabel.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
    m_contentSaveButton = SaveButton.Handle();

    PlaceButtonElement.SetDisabled(!ViewModelProperty<bool>(kVmContentAssetCanPlaceKey).Get());
    SaveButtonElement.SetDisabled(!ViewModelProperty<bool>(kVmContentAssetCanSaveKey).Get());
}

void EditorLayout::BuildHierarchyPane(PanelBuilder& Workspace,
                                      GameRuntime& Runtime,
                                      ComponentHandle& ActiveCamera,
                                      EditorSelectionModel* SelectionModel)
{
    auto Hierarchy = Workspace.Add(SnAPI::UI::UIPanel("Editor.Hierarchy"));
    auto& HierarchyPanel = Hierarchy.Element();
    HierarchyPanel.ElementStyle().Apply("editor.sidebar");
    HierarchyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HierarchyPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    HierarchyPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    HierarchyPanel.Padding().Set(6.0f);
    HierarchyPanel.Gap().Set(6.0f);

    auto TitleRow = Hierarchy.Add(SnAPI::UI::UIPanel("Editor.HierarchyTitleRow"));
    auto& TitleRowPanel = TitleRow.Element();
    TitleRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    TitleRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    TitleRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    TitleRowPanel.Gap().Set(4.0f);
    TitleRowPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});
    TitleRowPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto TitleIcon = TitleRow.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kHierarchyIconPath)));
    auto& TitleIconImage = TitleIcon.Element();
    ConfigureSvgIcon(TitleIconImage, 14.0f, kIconWhite);

    auto Title = TitleRow.Add(SnAPI::UI::UIText("Scene Hierarchy"));
    auto& TitleText = Title.Element();
    TitleText.ElementStyle().Apply("editor.panel_title");
    TitleText.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    TitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    TitleText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 4.0f, 0.0f});

    auto CountBadge = TitleRow.Add(SnAPI::UI::UIBadge("0"));
    CountBadge.Element().ElementStyle().Apply("editor.status_badge");
    CountBadge.Element().HorizontalPadding().Set(5.0f);
    CountBadge.Element().VerticalPadding().Set(2.0f);
    auto vmHierarchyCountText = ViewModelProperty<std::string>(kVmHierarchyCountTextKey);
    CountBadge.Element().Text().BindTo(vmHierarchyCountText);
    m_hierarchyCountBadge = CountBadge.Handle();

    auto SearchRow = Hierarchy.Add(SnAPI::UI::UIPanel("Editor.HierarchySearchRow"));
    auto& SearchRowPanel = SearchRow.Element();
    SearchRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    SearchRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    SearchRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    SearchRowPanel.Gap().Set(6.0f);
    SearchRowPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});
    SearchRowPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto SearchIcon = SearchRow.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kSearchIconPath)));
    auto& SearchIconImage = SearchIcon.Element();
    ConfigureSvgIcon(
        SearchIconImage,
        14.0f,
        kIconWhite,
        SnAPI::UI::Margin{2.0f, 0.0f, 0.0f, 0.0f});

    auto Search = SearchRow.Add(SnAPI::UI::UITextInput{});
    auto& SearchInput = Search.Element();
    SearchInput.ElementStyle().Apply("editor.search");
    SearchInput.Width().Set(SnAPI::UI::Sizing::Fill());
    SearchInput.Height().Set(SnAPI::UI::Sizing::Auto());
    SearchInput.Placeholder().Set(std::string("Search..."));
    SearchInput.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    auto vmHierarchyFilterText = ViewModelProperty<std::string>(kVmHierarchyFilterTextKey);
    SearchInput.Text().BindTo(vmHierarchyFilterText, SnAPI::UI::EBindMode::TwoWay);

    auto Tree = Hierarchy.Add(SnAPI::UI::UITreeView{});
    auto& TreeElement = Tree.Element();
    TreeElement.ElementStyle().Apply("editor.tree");
    TreeElement.Width().Set(SnAPI::UI::Sizing::Fill());
    TreeElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    TreeElement.RowHeight().Set(48.0f);
    TreeElement.IndentWidth().Set(28.0f);
    TreeElement.PaddingX().Set(6.0f);
    TreeElement.PaddingY().Set(4.0f);
    TreeElement.IconSize().Set(28.0f);
    TreeElement.IconGap().Set(6.0f);
    if (!m_hierarchyItemSource)
    {
        m_hierarchyItemSource = std::make_shared<VectorTreeItemSource>();
    }
    TreeElement.SetItemSource(m_hierarchyItemSource.get());
    TreeElement.SetItemPresenter(
        SnAPI::UI::TDelegate<SnAPI::UI::ElementId(SnAPI::UI::UIContext&)>::Bind([](SnAPI::UI::UIContext& Context)
                                                                                     -> SnAPI::UI::ElementId {
            const auto RowHandle = Context.CreateElement<SnAPI::UI::UIPanel>("Editor.Hierarchy.TreeItemRow");
            if (RowHandle.Id.Value == 0)
            {
                return {};
            }

            const auto IconHandle = Context.CreateElement<SnAPI::UI::UIImage>();
            const auto TextHandle = Context.CreateElement<SnAPI::UI::UIText>();
            if (IconHandle.Id.Value == 0 || TextHandle.Id.Value == 0)
            {
                return {};
            }

            Context.AddChild(RowHandle.Id, IconHandle.Id);
            Context.AddChild(RowHandle.Id, TextHandle.Id);

            if (auto* Row = dynamic_cast<SnAPI::UI::UIPanel*>(&Context.GetElement(RowHandle.Id)))
            {
                Row->Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
                Row->Width().Set(SnAPI::UI::Sizing::Fill());
                Row->Height().Set(SnAPI::UI::Sizing::Fill());
                Row->Gap().Set(6.0f);
                Row->Padding().Set(0.0f);
                Row->Background().Set(SnAPI::UI::Color::Transparent());
                Row->BorderColor().Set(SnAPI::UI::Color::Transparent());
                Row->BorderThickness().Set(0.0f);
                Row->Properties().SetProperty(kHierarchyRowIconIdKey, IconHandle.Id);
                Row->Properties().SetProperty(kHierarchyRowTextIdKey, TextHandle.Id);
            }

            if (auto* Icon = dynamic_cast<SnAPI::UI::UIImage*>(&Context.GetElement(IconHandle.Id)))
            {
                Icon->Width().Set(SnAPI::UI::Sizing::Fixed(28.0f));
                Icon->Height().Set(SnAPI::UI::Sizing::Fixed(28.0f));
                Icon->Mode().Set(SnAPI::UI::EImageMode::Aspect);
                Icon->LazyLoad().Set(true);
                Icon->ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
                ApplyHierarchyRowIcon(*Icon, std::string{}, kIconWhite);
            }

            if (auto* Text = dynamic_cast<SnAPI::UI::UIText*>(&Context.GetElement(TextHandle.Id)))
            {
                Text->Width().Set(SnAPI::UI::Sizing::Fill());
                Text->Height().Set(SnAPI::UI::Sizing::Fill());
                Text->VAlign().Set(SnAPI::UI::EAlignment::Center);
                Text->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
                Text->TextColor().Set(SnAPI::UI::Color{198, 204, 216, 255});
                Text->ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
            }

            return RowHandle.Id;
        }),
        SnAPI::UI::TDelegate<void(SnAPI::UI::UIContext&,
                                  SnAPI::UI::ElementId,
                                  const SnAPI::UI::UITreeItem&,
                                  bool,
                                  bool)>::Bind([](SnAPI::UI::UIContext& Context,
                                                  const SnAPI::UI::ElementId RowId,
                                                  const SnAPI::UI::UITreeItem& Item,
                                                  const bool IsSelected,
                                                  const bool IsHovered) {
            auto* Row = dynamic_cast<SnAPI::UI::UIPanel*>(&Context.GetElement(RowId));
            if (!Row)
            {
                return;
            }

            const SnAPI::UI::ElementId IconId = Row->Properties().GetPropertyOr(kHierarchyRowIconIdKey, SnAPI::UI::ElementId{});
            if (IconId.Value != 0)
            {
                if (auto* Icon = dynamic_cast<SnAPI::UI::UIImage*>(&Context.GetElement(IconId)))
                {
                    const std::string LastSource = Row->Properties().GetPropertyOr(kHierarchyRowLastIconSourceKey, std::string{});
                    const SnAPI::UI::Color LastTint =
                        Row->Properties().GetPropertyOr(kHierarchyRowLastIconTintKey, SnAPI::UI::Color::Transparent());
                    if (LastSource != Item.IconSource || LastTint != Item.IconTint)
                    {
                        ApplyHierarchyRowIcon(*Icon, Item.IconSource, Item.IconTint);
                        Row->Properties().SetProperty(kHierarchyRowLastIconSourceKey, Item.IconSource);
                        Row->Properties().SetProperty(kHierarchyRowLastIconTintKey, Item.IconTint);
                    }
                }
            }

            const SnAPI::UI::ElementId TextId = Row->Properties().GetPropertyOr(kHierarchyRowTextIdKey, SnAPI::UI::ElementId{});
            if (TextId.Value == 0)
            {
                return;
            }

            auto* Text = dynamic_cast<SnAPI::UI::UIText*>(&Context.GetElement(TextId));
            if (!Text)
            {
                return;
            }

            Text->Text().Set(Item.Label);
            const SnAPI::UI::Color TextColor = IsSelected
                                                   ? SnAPI::UI::Color{236, 224, 196, 255}
                                                   : (IsHovered ? SnAPI::UI::Color{224, 230, 240, 255}
                                                                : SnAPI::UI::Color{198, 204, 216, 255});
            Text->TextColor().Set(TextColor);
        }));
    TreeElement.OnSelectionChanged(SnAPI::UI::TDelegate<void(int32_t)>::Bind([this](const int32_t ItemIndex) {
        if (ItemIndex < 0 || static_cast<std::size_t>(ItemIndex) >= m_hierarchyVisibleNodes.size())
        {
            return;
        }

        const NodeHandle SelectedHandle = m_hierarchyVisibleNodes[static_cast<std::size_t>(ItemIndex)];
        if (SelectedHandle.IsNull())
        {
            const NodeHandle CurrentSelection = (m_selection != nullptr) ? m_selection->SelectedNode() : NodeHandle{};
            SyncHierarchySelection(CurrentSelection);
            return;
        }

        OnHierarchyNodeChosen(SelectedHandle);
    }));
    TreeElement.OnContextMenuRequested(
        SnAPI::UI::TDelegate<void(int32_t, const SnAPI::UI::UITreeItem&, const SnAPI::UI::PointerEvent&)>::Bind(
            [this](const int32_t ItemIndex, const SnAPI::UI::UITreeItem&, const SnAPI::UI::PointerEvent& Event) {
                if (ItemIndex < 0 || static_cast<std::size_t>(ItemIndex) >= m_hierarchyVisibleNodes.size())
                {
                    return;
                }

                OpenHierarchyContextMenu(static_cast<std::size_t>(ItemIndex), Event);
            }));
    m_hierarchyTree = Tree.Handle();

    auto HierarchyPager = Hierarchy.Add(SnAPI::UI::UIPagination{});
    auto& HierarchyPagerElement = HierarchyPager.Element();
    HierarchyPagerElement.ElementStyle().Apply("editor.browser_pagination");
    HierarchyPagerElement.PageCount().Set(1);
    HierarchyPagerElement.VisibleButtonCount().Set(1);
    HierarchyPagerElement.ButtonWidth().Set(26.0f);
    HierarchyPagerElement.Width().Set(SnAPI::UI::Sizing::Fill());

    m_selection = SelectionModel;
    EnsureDefaultSelection(Runtime, ActiveCamera);
    SyncHierarchy(Runtime, ActiveCamera);

    if (m_selection)
    {
        std::size_t NodeCount = 1; // Include synthetic World root row.
        if (auto* WorldPtr = Runtime.WorldPtr())
        {
            WorldPtr->ForEachNode([&](const NodeHandle&, BaseNode& Node) {
                if (!Node.EditorTransient())
                {
                    ++NodeCount;
                }
            });
        }
        ViewModelProperty<std::string>(kVmHierarchyCountTextKey).Set(std::to_string(NodeCount));
    }
}

void EditorLayout::EnsureDefaultSelection(GameRuntime& Runtime, ComponentHandle& ActiveCamera)
{
    if (!m_selection || !m_selection->SelectedNode().IsNull())
    {
        return;
    }

    auto* ActiveCameraComponent = ResolveActiveCameraComponent(Runtime, ActiveCamera);
    if (!ActiveCameraComponent || ActiveCameraComponent->Owner().IsNull())
    {
        return;
    }

    (void)m_selection->SelectNode(ActiveCameraComponent->Owner());
}

void EditorLayout::SyncHierarchy(GameRuntime& Runtime, ComponentHandle& ActiveCamera)
{
    if (!m_context || m_hierarchyTree.Id.Value == 0)
    {
        return;
    }

    EnsureDefaultSelection(Runtime, ActiveCamera);

    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return;
    }

    std::vector<HierarchyEntry> Entries{};
    if (!CollectHierarchyEntries(*WorldPtr, Entries))
    {
        return;
    }

    if (!m_hierarchyFilterText.empty())
    {
        Entries.erase(
            std::remove_if(Entries.begin(), Entries.end(), [this](const HierarchyEntry& Entry) {
                if (Entry.Kind == EHierarchyEntryKind::World)
                {
                    return false;
                }
                return !LabelMatchesFilter(Entry.Label, m_hierarchyFilterText);
            }),
            Entries.end());
    }

    ViewModelProperty<std::string>(kVmHierarchyCountTextKey).Set(std::to_string(Entries.size()));

    const NodeHandle SelectedNode = (m_selection != nullptr) ? m_selection->SelectedNode() : NodeHandle{};
    const std::uint64_t Signature = ComputeHierarchySignature(Entries);
    const bool StructureChanged = (Signature != m_hierarchySignature) || (Entries.size() != m_hierarchyNodeCount);
    if (StructureChanged)
    {
        RebuildHierarchyTree(Entries, SelectedNode);
        m_hierarchySignature = Signature;
        m_hierarchyNodeCount = Entries.size();
    }
    else if (SelectedNode != m_hierarchyVisualSelection)
    {
        SyncHierarchySelection(SelectedNode);
    }

    m_hierarchyVisualSelection = SelectedNode;
}

bool EditorLayout::CollectHierarchyEntries(World& WorldRef, std::vector<HierarchyEntry>& OutEntries) const
{
    OutEntries.clear();
    const std::string WorldLabel = WorldRef.Name().empty() ? std::string("World") : WorldRef.Name();
    OutEntries.push_back(HierarchyEntry{
        .Handle = {},
        .Depth = 0,
        .Label = WorldLabel,
        .Kind = EHierarchyEntryKind::World,
    });

    struct TraversalNode
    {
        NodeHandle Handle{};
        BaseNode* Node = nullptr;
        int Depth = 0;
    };

    const auto CollectWorldRoots = [](World& WorldContext, const int Depth, std::vector<TraversalNode>& OutNodes) {
        WorldContext.ForEachNode([Depth, &OutNodes](const NodeHandle& Handle, BaseNode& Node) {
            if (Node.EditorTransient())
            {
                return;
            }

            if (Node.Parent().IsNull())
            {
                OutNodes.push_back(TraversalNode{Handle, &Node, Depth});
            }
        });
    };

    std::vector<TraversalNode> RootNodes{};
    CollectWorldRoots(WorldRef, 1, RootNodes);

    std::vector<TraversalNode> Stack{};
    Stack.reserve(RootNodes.size());
    for (auto It = RootNodes.rbegin(); It != RootNodes.rend(); ++It)
    {
        Stack.push_back(*It);
    }

    std::unordered_set<Uuid, UuidHash> VisitedNodes{};
    VisitedNodes.reserve(RootNodes.size() * 2u);

    while (!Stack.empty())
    {
        TraversalNode Current = Stack.back();
        Stack.pop_back();

        BaseNode* Node = Current.Node;
        if (!Node)
        {
            Node = Current.Handle.Borrowed();
        }
        if (!Node)
        {
            Node = Current.Handle.BorrowedSlowByUuid();
        }
        if (!Node)
        {
            continue;
        }
        if (Node->EditorTransient())
        {
            continue;
        }

        if (!VisitedNodes.insert(Node->Id()).second)
        {
            continue;
        }

        NodeHandle EntryHandle = Current.Handle;
        if (EntryHandle.IsNull() || EntryHandle.Borrowed() == nullptr)
        {
            EntryHandle = Node->Handle();
        }
        if (EntryHandle.IsNull())
        {
            continue;
        }

        EHierarchyEntryKind EntryKind = EHierarchyEntryKind::Node;
        if (TypeRegistry::Instance().IsA(Node->TypeKey(), StaticTypeId<Level>()))
        {
            EntryKind = EHierarchyEntryKind::Level;
        }

        OutEntries.push_back(HierarchyEntry{
            .Handle = EntryHandle,
            .Depth = Current.Depth,
            .Label = Node->Name(),
            .Kind = EntryKind,
        });

        std::vector<TraversalNode> ChildNodes{};
        ChildNodes.reserve(Node->Children().size() + 8u);

        for (NodeHandle ChildHandle : Node->Children())
        {
            if (ChildHandle.IsNull())
            {
                continue;
            }

            BaseNode* ChildNode = ChildHandle.Borrowed();
            if (!ChildNode)
            {
                ChildNode = ChildHandle.BorrowedSlowByUuid();
            }
            if (!ChildNode)
            {
                continue;
            }
            if (ChildNode->EditorTransient())
            {
                continue;
            }

            ChildNodes.push_back(TraversalNode{ChildNode->Handle(), ChildNode, Current.Depth + 1});
        }

        for (auto ChildIt = ChildNodes.rbegin(); ChildIt != ChildNodes.rend(); ++ChildIt)
        {
            Stack.push_back(*ChildIt);
        }
    }

    return true;
}

std::uint64_t EditorLayout::ComputeHierarchySignature(const std::vector<HierarchyEntry>& Entries) const
{
    constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;

    std::uint64_t Hash = kFnvOffset;
    const auto Mix = [&](const std::uint64_t Value) {
        Hash ^= Value;
        Hash *= kFnvPrime;
    };

    Mix(static_cast<std::uint64_t>(Entries.size()));
    for (const HierarchyEntry& Entry : Entries)
    {
        Mix(static_cast<std::uint64_t>(UuidHash{}(Entry.Handle.Id)));
        Mix(static_cast<std::uint64_t>(Entry.Depth));
        Mix(static_cast<std::uint64_t>(std::hash<std::string>{}(Entry.Label)));
        Mix(static_cast<std::uint64_t>(Entry.Kind));
    }

    return Hash;
}

void EditorLayout::RebuildHierarchyTree(const std::vector<HierarchyEntry>& Entries, const NodeHandle& SelectedNode)
{
    if (!m_context || m_hierarchyTree.Id.Value == 0)
    {
        return;
    }

    auto* Tree = dynamic_cast<SnAPI::UI::UITreeView*>(&m_context->GetElement(m_hierarchyTree.Id));
    if (!Tree)
    {
        return;
    }

    std::vector<SnAPI::UI::UITreeItem> TreeItems{};
    TreeItems.reserve(Entries.size());
    m_hierarchyVisibleNodes.clear();
    m_hierarchyVisibleNodes.reserve(Entries.size());

    for (std::size_t Index = 0; Index < Entries.size(); ++Index)
    {
        const HierarchyEntry& Entry = Entries[Index];
        const std::string Label = Entry.Label.empty() ? std::string("<unnamed>") : Entry.Label;
        std::string IconSource{};
        switch (Entry.Kind)
        {
        case EHierarchyEntryKind::World:
            IconSource = ResolveUIImageSource(kHierarchyWorldIconPath);
            break;
        case EHierarchyEntryKind::Level:
            IconSource = ResolveUIImageSource(kHierarchyLevelIconPath);
            break;
        case EHierarchyEntryKind::Node:
        default:
            IconSource = ResolveUIImageSource(kHierarchyNodeIconPath);
            break;
        }

        bool HasChildren = false;
        if ((Index + 1u) < Entries.size())
        {
            HasChildren = Entries[Index + 1u].Depth > Entry.Depth;
        }

        TreeItems.push_back(SnAPI::UI::UITreeItem{
            .Label = std::move(Label),
            .IconSource = std::move(IconSource),
            .IconTint = kIconWhite,
            .Depth = static_cast<uint32_t>(std::max(0, Entry.Depth)),
            .HasChildren = HasChildren,
            .Expanded = true,
        });
        m_hierarchyVisibleNodes.push_back(Entry.Handle);
    }

    auto* Source = dynamic_cast<VectorTreeItemSource*>(m_hierarchyItemSource.get());
    if (!Source)
    {
        m_hierarchyItemSource = std::make_shared<VectorTreeItemSource>();
        Source = static_cast<VectorTreeItemSource*>(m_hierarchyItemSource.get());
    }

    if (Tree->ItemSource() != m_hierarchyItemSource.get())
    {
        Tree->SetItemSource(m_hierarchyItemSource.get());
    }

    Source->SetItems(std::move(TreeItems));
    Tree->RefreshItemsFromSource();
    SyncHierarchySelection(SelectedNode);
    m_context->MarkLayoutDirty();
}

void EditorLayout::SyncHierarchySelection(const NodeHandle& SelectedNode)
{
    if (!m_context || m_hierarchyTree.Id.Value == 0)
    {
        return;
    }

    auto* Tree = dynamic_cast<SnAPI::UI::UITreeView*>(&m_context->GetElement(m_hierarchyTree.Id));
    if (!Tree)
    {
        return;
    }

    int32_t SelectedIndex = -1;
    if (!SelectedNode.IsNull())
    {
        const auto SelectedIt = std::find(m_hierarchyVisibleNodes.begin(), m_hierarchyVisibleNodes.end(), SelectedNode);
        if (SelectedIt != m_hierarchyVisibleNodes.end())
        {
            SelectedIndex = static_cast<int32_t>(std::distance(m_hierarchyVisibleNodes.begin(), SelectedIt));
        }
    }

    Tree->SetSelectedIndex(SelectedIndex, false);
}

void EditorLayout::OnHierarchyNodeChosen(const NodeHandle& Handle)
{
    if (m_onHierarchyNodeChosen)
    {
        m_onHierarchyNodeChosen(Handle);
        return;
    }

    if (!m_selection)
    {
        return;
    }

    (void)m_selection->SelectNode(Handle);
}

void EditorLayout::SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)> Handler)
{
    m_onHierarchyNodeChosen = std::move(Handler);
}

void EditorLayout::SetHierarchyActionHandler(SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> Handler)
{
    m_onHierarchyActionRequested = std::move(Handler);
}

void EditorLayout::SetToolbarActionHandler(SnAPI::UI::TDelegate<void(EToolbarAction)> Handler)
{
    m_onToolbarActionRequested = std::move(Handler);
}

void EditorLayout::SetProjectActionHandler(SnAPI::UI::TDelegate<void(const ProjectActionRequest&)> Handler)
{
    m_onProjectActionRequested = std::move(Handler);
}

void EditorLayout::SetProjectState(ProjectState State)
{
    m_projectState = std::move(State);

    if (!m_projectState.IsLoaded && m_projectSettingsModalOpen)
    {
        CloseProjectSettingsModal();
    }

    if (!m_projectSettingsModalOpen)
    {
        m_projectSettingsNameText = m_projectState.Name;
        m_projectSettingsStartupAssetText = m_projectState.StartupLevelAsset;
        m_projectSettingsDefaultRenderSettingsAssetId = m_projectState.DefaultRenderSettingsAssetId;
    }
}

void EditorLayout::SetProjectSelectionRequired(const bool Required)
{
    const bool Changed = (m_projectModalRequired != Required);
    m_projectModalRequired = Required;
    if (!m_context)
    {
        return;
    }

    if (m_projectModalRequired)
    {
        if (m_projectSettingsModalOpen)
        {
            CloseProjectSettingsModal();
        }

        if (!m_projectModalOpen)
        {
            OpenProjectWelcomeModal();
            return;
        }

        if (Changed)
        {
            m_projectModalShowWelcome = true;
            DestroyProjectModalOverlay();
            RefreshProjectModalVisibility();
            RefreshProjectModalOkButtonState();
            m_context->MarkLayoutDirty();
        }
        return;
    }

    if (Changed)
    {
        if (m_projectModalOpen)
        {
            CloseProjectModal();
            return;
        }

        RefreshProjectModalOkButtonState();
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::SetContentAssets(std::vector<ContentAssetEntry> Assets)
{
    m_contentAssets = std::move(Assets);

    const auto SelectedIt = std::find_if(
        m_contentAssets.begin(),
        m_contentAssets.end(),
        [this](const ContentAssetEntry& Entry) { return Entry.Key == m_selectedContentAssetKey; });
    if (SelectedIt == m_contentAssets.end())
    {
        m_selectedContentAssetKey.clear();
        ViewModelProperty<std::string>(kVmSelectedContentAssetKey).Set(std::string{});
    }

    if (m_built)
    {
        ApplyContentAssetFilter();
        RefreshContentAssetCardSelectionStyles();
        RefreshContentAssetDetailsViewModel();
    }
    else
    {
        RebuildContentBrowserEntries();
    }
}

void EditorLayout::SetContentAssetSelectionHandler(SnAPI::UI::TDelegate<void(const std::string&, bool)> Handler)
{
    m_onContentAssetSelected = std::move(Handler);
}

void EditorLayout::SetContentAssetPlaceHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onContentAssetPlaceRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetSaveHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onContentAssetSaveRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetDeleteHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onContentAssetDeleteRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetRenameHandler(SnAPI::UI::TDelegate<void(const std::string&, const std::string&)> Handler)
{
    m_onContentAssetRenameRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetRefreshHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onContentAssetRefreshRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetCreateHandler(SnAPI::UI::TDelegate<void(const ContentAssetCreateRequest&)> Handler)
{
    m_onContentAssetCreateRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetImportHandler(SnAPI::UI::TDelegate<void(const ContentAssetImportRequest&)> Handler)
{
    m_onContentAssetImportRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetInspectorSaveHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onContentAssetInspectorSaveRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetInspectorReimportHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onContentAssetInspectorReimportRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetInspectorCloseHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onContentAssetInspectorCloseRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetInspectorNodeSelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)> Handler)
{
    m_onContentAssetInspectorNodeSelected = std::move(Handler);
}

void EditorLayout::SetContentAssetInspectorHierarchyActionHandler(
    SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> Handler)
{
    m_onContentAssetInspectorHierarchyActionRequested = std::move(Handler);
}

void EditorLayout::SetContentAssetDetails(ContentAssetDetails Details)
{
    m_contentAssetDetails = std::move(Details);
    RefreshContentAssetDetailsViewModel();
}

void EditorLayout::SetContentAssetInspectorState(ContentAssetInspectorState State)
{
    const bool SessionRevisionChanged = (m_contentAssetInspectorState.SessionRevision != State.SessionRevision);
    const bool NodesChanged = [&]() -> bool {
        if (m_contentAssetInspectorState.Nodes.size() != State.Nodes.size())
        {
            return true;
        }
        for (std::size_t Index = 0; Index < State.Nodes.size(); ++Index)
        {
            const auto& Left = m_contentAssetInspectorState.Nodes[Index];
            const auto& Right = State.Nodes[Index];
            if (Left.Handle != Right.Handle || Left.Depth != Right.Depth || Left.Label != Right.Label)
            {
                return true;
            }
        }
        return false;
    }();

    const bool Changed =
        (m_contentAssetInspectorState.Open != State.Open) ||
        (m_contentAssetInspectorState.AssetKey != State.AssetKey) ||
        (m_contentAssetInspectorState.Title != State.Title) ||
        (m_contentAssetInspectorState.Status != State.Status) ||
        (m_contentAssetInspectorState.TargetType != State.TargetType) ||
        (m_contentAssetInspectorState.TargetObject != State.TargetObject) ||
        (m_contentAssetInspectorState.ImportSettingsType != State.ImportSettingsType) ||
        (m_contentAssetInspectorState.ImportSettingsObject != State.ImportSettingsObject) ||
        (m_contentAssetInspectorState.SelectedNode != State.SelectedNode) ||
        (m_contentAssetInspectorState.CanEditHierarchy != State.CanEditHierarchy) ||
        (m_contentAssetInspectorState.HasImportSettings != State.HasImportSettings) ||
        (m_contentAssetInspectorState.RuntimeDirty != State.RuntimeDirty) ||
        (m_contentAssetInspectorState.ImportSettingsDirty != State.ImportSettingsDirty) ||
        (m_contentAssetInspectorState.IsDirty != State.IsDirty) ||
        (m_contentAssetInspectorState.CanSave != State.CanSave) ||
        (m_contentAssetInspectorState.CanReimport != State.CanReimport) ||
        (m_contentAssetInspectorState.PreviewIconSource != State.PreviewIconSource) ||
        (m_contentAssetInspectorState.PreviewTextureId != State.PreviewTextureId) ||
        (m_contentAssetInspectorState.PreviewWidth != State.PreviewWidth) ||
        (m_contentAssetInspectorState.PreviewHeight != State.PreviewHeight) ||
        (m_contentAssetInspectorState.PreviewStatsPrimary != State.PreviewStatsPrimary) ||
        (m_contentAssetInspectorState.PreviewStatsSecondary != State.PreviewStatsSecondary) ||
        (m_contentAssetInspectorState.SessionRevision != State.SessionRevision) ||
        NodesChanged;

    if (!Changed)
    {
        return;
    }

    m_contentAssetInspectorState = std::move(State);
    if (SessionRevisionChanged)
    {
        m_contentInspectorTargetBound = false;
        m_contentInspectorBoundNode = {};
        m_contentInspectorBoundObject = nullptr;
        m_contentInspectorBoundType = {};
        m_contentInspectorBoundComponentSignature = 0;
        m_contentInspectorImportTargetBound = false;
        m_contentInspectorImportBoundObject = nullptr;
        m_contentInspectorImportBoundType = {};
    }

    if (!m_contentAssetInspectorState.Open)
    {
        m_contentAssetInspectorState.TargetObject = nullptr;
        m_contentAssetInspectorState.TargetType = {};
        m_contentAssetInspectorState.ImportSettingsObject = nullptr;
        m_contentAssetInspectorState.ImportSettingsType = {};
        m_contentAssetInspectorState.SelectedNode = {};
        m_contentAssetInspectorState.Nodes.clear();
        m_contentAssetInspectorState.PreviewIconSource.clear();
        m_contentAssetInspectorState.PreviewTextureId = 0;
        m_contentAssetInspectorState.PreviewWidth = 0;
        m_contentAssetInspectorState.PreviewHeight = 0;
        m_contentAssetInspectorState.PreviewStatsPrimary.clear();
        m_contentAssetInspectorState.PreviewStatsSecondary.clear();

        RefreshContentAssetInspectorModalVisibility();
        if (m_context)
        {
            m_context->MarkLayoutDirty();
        }
        return;
    }

    EnsureContentAssetInspectorModalOverlay();

    RebuildContentAssetInspectorHierarchyTree();
    RefreshContentAssetInspectorModalState();
    RefreshContentAssetInspectorModalVisibility();
    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::SetConduitWorkspaceState(ConduitWorkspaceState State)
{
    const bool Changed =
        (m_conduitWorkspaceState.Open != State.Open) ||
        (m_conduitWorkspaceState.AssetKey != State.AssetKey) ||
        (m_conduitWorkspaceState.Title != State.Title) ||
        (m_conduitWorkspaceState.Status != State.Status) ||
        (m_conduitWorkspaceState.SelfTypeLabel != State.SelfTypeLabel) ||
        (m_conduitWorkspaceState.SlotCount != State.SlotCount) ||
        (m_conduitWorkspaceState.VariableCount != State.VariableCount) ||
        (m_conduitWorkspaceState.NodeCount != State.NodeCount) ||
        (m_conduitWorkspaceState.IsDirty != State.IsDirty) ||
        (m_conduitWorkspaceState.CompileSucceeded != State.CompileSucceeded) ||
        (m_conduitWorkspaceState.WarningCount != State.WarningCount) ||
        (m_conduitWorkspaceState.ErrorCount != State.ErrorCount) ||
        (m_conduitWorkspaceState.Revision != State.Revision);
    if (!Changed)
    {
        return;
    }

    m_conduitWorkspaceState = std::move(State);
    RefreshConduitWorkspaceView();
}

void EditorLayout::SetConduitVariableSelectionHandler(SnAPI::UI::TDelegate<void(const Uuid&)> Handler)
{
    m_onConduitVariableSelected = std::move(Handler);
}

void EditorLayout::SetConduitVariableCreateHandler(SnAPI::UI::TDelegate<void(const std::string&, const TypeId&)> Handler)
{
    m_onConduitVariableCreateRequested = std::move(Handler);
}

void EditorLayout::SetConduitVariableRemoveHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onConduitVariableRemoveRequested = std::move(Handler);
}

void EditorLayout::SetConduitVariableRenameHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onConduitVariableRenameRequested = std::move(Handler);
}

void EditorLayout::SetConduitVariableTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)> Handler)
{
    m_onConduitVariableTypeRequested = std::move(Handler);
}

void EditorLayout::SetConduitVariableDefaultBoolHandler(SnAPI::UI::TDelegate<void(bool)> Handler)
{
    m_onConduitVariableDefaultBoolRequested = std::move(Handler);
}

void EditorLayout::SetConduitVariableDefaultTextHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onConduitVariableDefaultTextRequested = std::move(Handler);
}

void EditorLayout::SetConduitVariableDefaultEnumHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onConduitVariableDefaultEnumRequested = std::move(Handler);
}

void EditorLayout::SetConduitVariableClearDefaultHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onConduitVariableClearDefaultRequested = std::move(Handler);
}

void EditorLayout::SetConduitVariableCommitDefaultHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onConduitVariableCommitDefaultRequested = std::move(Handler);
}

void EditorLayout::SetConduitVariableResetDefaultHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onConduitVariableResetDefaultRequested = std::move(Handler);
}

void EditorLayout::SetConduitNodeSelectionHandler(SnAPI::UI::TDelegate<void(const Uuid&)> Handler)
{
    m_onConduitNodeSelected = std::move(Handler);
}

void EditorLayout::SetConduitNodeCreateHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onConduitNodeCreateRequested = std::move(Handler);
}

void EditorLayout::SetConduitNodeRemoveHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onConduitNodeRemoveRequested = std::move(Handler);
}

void EditorLayout::SetConduitNodeMoveHandler(SnAPI::UI::TDelegate<void(const Uuid&, float, float)> Handler)
{
    m_onConduitNodeMoveRequested = std::move(Handler);
}

void EditorLayout::SetConduitNodePrimaryTextHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onConduitNodePrimaryTextRequested = std::move(Handler);
}

void EditorLayout::SetConduitNodeSecondaryTextHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onConduitNodeSecondaryTextRequested = std::move(Handler);
}

void EditorLayout::SetConduitViewportHandler(SnAPI::UI::TDelegate<void(float, float, float)> Handler)
{
    m_onConduitViewportRequested = std::move(Handler);
}

void EditorLayout::SetConduitClassNameHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onConduitClassNameRequested = std::move(Handler);
}

void EditorLayout::SetConduitClassHostTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)> Handler)
{
    m_onConduitClassHostTypeRequested = std::move(Handler);
}

void EditorLayout::SetConduitClassGraphHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onConduitClassGraphRequested = std::move(Handler);
}

void EditorLayout::RefreshConduitWorkspaceView()
{
    if (auto* Tabs = ResolveGameViewTabs())
    {
        Tabs->SetTabLabel(2, m_conduitWorkspaceState.Open && m_conduitWorkspaceState.IsDirty
                                 ? std::string("Conduit*")
                                 : std::string("Conduit"));

        if (m_conduitWorkspaceState.Open)
        {
            Tabs->ActiveIndex().Set(2);
        }
        else if (Tabs->ActiveIndex().Get() == 2)
        {
            Tabs->ActiveIndex().Set(0);
        }
    }

    if (!m_context)
    {
        return;
    }

    const auto SetText = [this](const SnAPI::UI::ElementHandle<SnAPI::UI::UIText>& Handle, const std::string& Value) {
        if (Handle.Id.Value == 0)
        {
            return;
        }
        if (auto* Text = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(Handle.Id)))
        {
            Text->Text().Set(Value);
        }
    };
    const auto SetVisibility = [this](const auto& Handle, const SnAPI::UI::EVisibility Visibility) {
        if (Handle.Id.Value == 0)
        {
            return;
        }
        m_context->GetElement(Handle.Id).Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, Visibility);
    };
    const auto SetSizeForVisibility = [this](const auto& Handle,
                                             const bool Visible,
                                             const SnAPI::UI::Sizing& VisibleWidth,
                                             const SnAPI::UI::Sizing& VisibleHeight) {
        if (Handle.Id.Value == 0)
        {
            return;
        }
        auto& Properties = m_context->GetElement(Handle.Id).Properties();
        Properties.SetProperty(SnAPI::UI::UIElementBase::WidthKey,
                               Visible ? VisibleWidth : SnAPI::UI::Sizing::Fixed(0.0f));
        Properties.SetProperty(SnAPI::UI::UIElementBase::HeightKey,
                               Visible ? VisibleHeight : SnAPI::UI::Sizing::Fixed(0.0f));
    };

    if (!m_conduitWorkspaceState.Open)
    {
        SetText(m_conduitWorkspaceTitleText, "Conduit Workspace");
        SetText(m_conduitWorkspaceStatusText, "No Conduit document open.");
        SetText(m_conduitWorkspaceSummaryText,
                "Double-click a Conduit Graph or Conduit Class asset in the Content Browser to open it here.");
        SetText(m_conduitClassOverviewSummaryText,
                "Conduit classes resolve one concrete host node type and one graph asset. Built-in and custom entrypoints run against that host instance as self.");
        SetText(m_conduitClassOverviewHostText, "Host: None");
        SetText(m_conduitClassOverviewGraphText, "Graph: None");
        m_conduitVisibleVariableIds.clear();
        m_conduitVisibleNodeIds.clear();
        m_conduitVisiblePaletteStableIds.clear();
        m_conduitSelectedPaletteStableId.clear();
    }
    else if (m_conduitWorkspaceState.Kind == ConduitWorkspaceState::EDocumentKind::Class)
    {
        std::string Title = m_conduitWorkspaceState.Title.empty()
            ? std::string("Conduit Class")
            : m_conduitWorkspaceState.Title;
        if (m_conduitWorkspaceState.IsDirty)
        {
            Title += " *";
        }

        std::ostringstream Summary{};
        Summary << "Host: "
                << (m_conduitWorkspaceState.HostTypeLabel.empty()
                    ? std::string("None")
                    : m_conduitWorkspaceState.HostTypeLabel)
                << " | Graph: "
                << (m_conduitWorkspaceState.GraphAssetLabel.empty()
                    ? std::string("None")
                    : m_conduitWorkspaceState.GraphAssetLabel);

        SetText(m_conduitWorkspaceTitleText, Title);
        SetText(m_conduitWorkspaceStatusText,
                m_conduitWorkspaceState.Status.empty() ? std::string("Conduit class ready.") : m_conduitWorkspaceState.Status);
        SetText(m_conduitWorkspaceSummaryText, Summary.str());
        SetText(m_conduitClassOverviewSummaryText,
                "Conduit classes resolve one concrete host node type and one graph asset. Built-in and custom entrypoints run against that host instance as self.");
        SetText(m_conduitClassOverviewHostText,
                "Host: " + (m_conduitWorkspaceState.HostTypeLabel.empty() ? std::string("None") : m_conduitWorkspaceState.HostTypeLabel));
        SetText(m_conduitClassOverviewGraphText,
                "Graph: " + (m_conduitWorkspaceState.GraphAssetLabel.empty() ? std::string("None") : m_conduitWorkspaceState.GraphAssetLabel));
    }
    else
    {
        std::string Title = m_conduitWorkspaceState.Title.empty()
            ? std::string("Conduit Graph")
            : m_conduitWorkspaceState.Title;
        if (m_conduitWorkspaceState.IsDirty)
        {
            Title += " *";
        }

        std::ostringstream Summary{};
        Summary << "Self: "
                << (m_conduitWorkspaceState.SelfTypeLabel.empty()
                    ? std::string("None")
                    : m_conduitWorkspaceState.SelfTypeLabel)
                << " | Slots: " << m_conduitWorkspaceState.SlotCount
                << " | Variables: " << m_conduitWorkspaceState.VariableCount
                << " | Nodes: " << m_conduitWorkspaceState.NodeCount
                << " | Warnings: " << m_conduitWorkspaceState.WarningCount
                << " | Errors: " << m_conduitWorkspaceState.ErrorCount;

        SetText(m_conduitWorkspaceTitleText, Title);
        SetText(m_conduitWorkspaceStatusText,
                m_conduitWorkspaceState.Status.empty() ? std::string("Ready") : m_conduitWorkspaceState.Status);
        SetText(m_conduitWorkspaceSummaryText, Summary.str());
        SetText(m_conduitClassOverviewSummaryText,
                "Conduit classes resolve one concrete host node type and one graph asset. Built-in and custom entrypoints run against that host instance as self.");
        SetText(m_conduitClassOverviewHostText, "Host: None");
        SetText(m_conduitClassOverviewGraphText, "Graph: None");
    }

    const bool IsClassMode = m_conduitWorkspaceState.Open &&
                             m_conduitWorkspaceState.Kind == ConduitWorkspaceState::EDocumentKind::Class;
    const bool ShowNodeInspector = m_conduitWorkspaceState.SelectedNode.HasSelection;
    const bool ShowVariableInspector = !ShowNodeInspector && m_conduitWorkspaceState.SelectedVariable.HasSelection;
    SetVisibility(m_conduitGraphWorkspaceHost,
                  IsClassMode ? SnAPI::UI::EVisibility::Collapsed : SnAPI::UI::EVisibility::Visible);
    SetVisibility(m_conduitClassWorkspaceHost,
                  IsClassMode ? SnAPI::UI::EVisibility::Visible : SnAPI::UI::EVisibility::Collapsed);
    SetSizeForVisibility(m_conduitGraphWorkspaceHost,
                         !IsClassMode,
                         SnAPI::UI::Sizing::Fill(),
                         SnAPI::UI::Sizing::Ratio(1.0f));
    SetSizeForVisibility(m_conduitClassWorkspaceHost,
                         IsClassMode,
                         SnAPI::UI::Sizing::Fill(),
                         SnAPI::UI::Sizing::Ratio(1.0f));
    SetVisibility(m_conduitVariablesCard, IsClassMode ? SnAPI::UI::EVisibility::Collapsed : SnAPI::UI::EVisibility::Visible);
    SetVisibility(m_conduitNodesCard, IsClassMode ? SnAPI::UI::EVisibility::Collapsed : SnAPI::UI::EVisibility::Visible);
    SetVisibility(m_conduitInspectorCard, IsClassMode ? SnAPI::UI::EVisibility::Collapsed : SnAPI::UI::EVisibility::Visible);
    SetVisibility(m_conduitClassCard, IsClassMode ? SnAPI::UI::EVisibility::Visible : SnAPI::UI::EVisibility::Collapsed);
    SetVisibility(m_conduitVariableInspectorPanel,
                  ShowVariableInspector ? SnAPI::UI::EVisibility::Visible : SnAPI::UI::EVisibility::Collapsed);
    SetVisibility(m_conduitNodeInspectorPanel,
                  ShowNodeInspector ? SnAPI::UI::EVisibility::Visible : SnAPI::UI::EVisibility::Collapsed);
    SetSizeForVisibility(m_conduitVariableInspectorPanel,
                         ShowVariableInspector,
                         SnAPI::UI::Sizing::Fill(),
                         SnAPI::UI::Sizing::Auto());
    SetSizeForVisibility(m_conduitNodeInspectorPanel,
                         ShowNodeInspector,
                         SnAPI::UI::Sizing::Fill(),
                         SnAPI::UI::Sizing::Auto());
    SetText(m_conduitInspectorTitleText,
            ShowVariableInspector ? std::string("Selected Variable")
                                  : (ShowNodeInspector ? std::string("Selected Node")
                                                       : std::string("Selection Inspector")));

    if (m_conduitVariablesTree.Id.Value != 0)
    {
        if (auto* Tree = dynamic_cast<SnAPI::UI::UITreeView*>(&m_context->GetElement(m_conduitVariablesTree.Id)))
        {
            std::vector<SnAPI::UI::UITreeItem> Items{};
            Items.reserve(m_conduitWorkspaceState.VariableEntries.size());
            m_conduitVisibleVariableIds.clear();

            int32_t SelectedIndex = -1;
            for (std::size_t Index = 0; Index < m_conduitWorkspaceState.VariableEntries.size(); ++Index)
            {
                const auto& Entry = m_conduitWorkspaceState.VariableEntries[Index];
                SnAPI::UI::UITreeItem Item{};
                Item.Label = Entry.Name + " : " + Entry.TypeLabel + (Entry.HasDefault ? " [default]" : "");
                Items.push_back(std::move(Item));
                m_conduitVisibleVariableIds.push_back(Entry.Id);
                if (Entry.Selected)
                {
                    SelectedIndex = static_cast<int32_t>(Index);
                }
            }

            Tree->SetItems(std::move(Items));
            Tree->SetSelectedIndex(SelectedIndex, false);
        }
    }

    if (m_conduitPaletteSearchInput.Id.Value != 0)
    {
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_conduitPaletteSearchInput.Id)))
        {
            Input->Text().Set(m_conduitPaletteFilterText);
        }
    }

    if (m_conduitPaletteTree.Id.Value != 0)
    {
        if (auto* Tree = dynamic_cast<SnAPI::UI::UITreeView*>(&m_context->GetElement(m_conduitPaletteTree.Id)))
        {
            std::vector<SnAPI::UI::UITreeItem> Items{};
            m_conduitVisiblePaletteStableIds.clear();

            std::string FilterLower = m_conduitPaletteFilterText;
            std::transform(FilterLower.begin(), FilterLower.end(), FilterLower.begin(), [](const unsigned char Character) {
                return static_cast<char>(std::tolower(Character));
            });

            int32_t SelectedIndex = -1;
            for (const auto& Entry : m_conduitWorkspaceState.PaletteEntries)
            {
                std::string SearchText = Entry.DisplayName + " " + Entry.Category + " " + Entry.Tooltip;
                std::transform(SearchText.begin(), SearchText.end(), SearchText.begin(), [](const unsigned char Character) {
                    return static_cast<char>(std::tolower(Character));
                });

                if (!FilterLower.empty() && SearchText.find(FilterLower) == std::string::npos)
                {
                    continue;
                }

                SnAPI::UI::UITreeItem Item{};
                Item.Label = Entry.Category.empty()
                    ? Entry.DisplayName
                    : Entry.Category + " :: " + Entry.DisplayName + (Entry.RequiresSpecialization ? " *" : "");
                Items.push_back(std::move(Item));
                m_conduitVisiblePaletteStableIds.push_back(Entry.StableId);
                if (Entry.StableId == m_conduitSelectedPaletteStableId)
                {
                    SelectedIndex = static_cast<int32_t>(m_conduitVisiblePaletteStableIds.size() - 1);
                }
            }

            if (SelectedIndex < 0 && !m_conduitVisiblePaletteStableIds.empty())
            {
                SelectedIndex = 0;
                m_conduitSelectedPaletteStableId = m_conduitVisiblePaletteStableIds.front();
            }

            Tree->SetItems(std::move(Items));
            Tree->SetSelectedIndex(SelectedIndex, false);
        }
    }

    if (m_conduitGraphCanvas.Id.Value != 0)
    {
        if (auto* Canvas =
                dynamic_cast<Conduit::Editor::UIConduitGraphCanvas*>(&m_context->GetElement(m_conduitGraphCanvas.Id)))
        {
            Conduit::Editor::GraphCanvasView CanvasView{};
            CanvasView.Viewport.PanX = m_conduitWorkspaceState.CanvasPanX;
            CanvasView.Viewport.PanY = m_conduitWorkspaceState.CanvasPanY;
            CanvasView.Viewport.Zoom = m_conduitWorkspaceState.CanvasZoom;
            CanvasView.Nodes.reserve(m_conduitWorkspaceState.CanvasNodes.size());
            for (const auto& Node : m_conduitWorkspaceState.CanvasNodes)
            {
                std::vector<Conduit::Editor::CanvasPinView> InputPins{};
                InputPins.reserve(Node.InputPins.size());
                for (const auto& Pin : Node.InputPins)
                {
                    InputPins.push_back(Conduit::Editor::CanvasPinView{
                        .Name = Pin.Name,
                        .TypeLabel = Pin.TypeLabel,
                        .Kind = Pin.Kind,
                        .IsInput = Pin.IsInput,
                        .IsExec = Pin.IsExec,
                    });
                }

                std::vector<Conduit::Editor::CanvasPinView> OutputPins{};
                OutputPins.reserve(Node.OutputPins.size());
                for (const auto& Pin : Node.OutputPins)
                {
                    OutputPins.push_back(Conduit::Editor::CanvasPinView{
                        .Name = Pin.Name,
                        .TypeLabel = Pin.TypeLabel,
                        .Kind = Pin.Kind,
                        .IsInput = Pin.IsInput,
                        .IsExec = Pin.IsExec,
                    });
                }

                CanvasView.Nodes.push_back(Conduit::Editor::CanvasNodeView{
                    .Id = Node.Id,
                    .Title = Node.Title,
                    .Detail = Node.Detail,
                    .X = Node.X,
                    .Y = Node.Y,
                    .Width = Node.Width,
                    .IsCollapsed = Node.IsCollapsed,
                    .Selected = Node.Selected,
                    .InputPins = std::move(InputPins),
                    .OutputPins = std::move(OutputPins),
                });
            }
            CanvasView.Comments.reserve(m_conduitWorkspaceState.CanvasComments.size());
            for (const auto& Comment : m_conduitWorkspaceState.CanvasComments)
            {
                CanvasView.Comments.push_back(Conduit::Editor::CanvasCommentView{
                    .Id = Comment.Id,
                    .Title = Comment.Title,
                    .X = Comment.X,
                    .Y = Comment.Y,
                    .Width = Comment.Width,
                    .Height = Comment.Height,
                    .ColorRgba = Comment.ColorRgba,
                    .Selected = Comment.Selected,
                });
            }
            CanvasView.Wires.reserve(m_conduitWorkspaceState.CanvasWires.size());
            for (const auto& Wire : m_conduitWorkspaceState.CanvasWires)
            {
                CanvasView.Wires.push_back(Conduit::Editor::CanvasWireView{
                    .SourceNodeId = Wire.SourceNodeId,
                    .SourcePin = Wire.SourcePin,
                    .TargetNodeId = Wire.TargetNodeId,
                    .TargetPin = Wire.TargetPin,
                    .Kind = Wire.Kind,
                    .IsExec = Wire.IsExec,
                });
            }
            Canvas->SetViewState(std::move(CanvasView));
        }
    }

    if (m_conduitNodesTree.Id.Value != 0)
    {
        if (auto* Tree = dynamic_cast<SnAPI::UI::UITreeView*>(&m_context->GetElement(m_conduitNodesTree.Id)))
        {
            std::vector<SnAPI::UI::UITreeItem> Items{};
            Items.reserve(m_conduitWorkspaceState.NodeEntries.size());
            m_conduitVisibleNodeIds.clear();

            int32_t SelectedIndex = -1;
            for (std::size_t Index = 0; Index < m_conduitWorkspaceState.NodeEntries.size(); ++Index)
            {
                const auto& Entry = m_conduitWorkspaceState.NodeEntries[Index];
                SnAPI::UI::UITreeItem Item{};
                Item.Label = Entry.Detail.empty() ? Entry.Title : Entry.Title + " :: " + Entry.Detail;
                Items.push_back(std::move(Item));
                m_conduitVisibleNodeIds.push_back(Entry.Id);
                if (Entry.Selected)
                {
                    SelectedIndex = static_cast<int32_t>(Index);
                }
            }

            Tree->SetItems(std::move(Items));
            Tree->SetSelectedIndex(SelectedIndex, false);
        }
    }

    if (m_conduitVariableCreateTypeCombo.Id.Value != 0)
    {
        if (auto* Combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(m_conduitVariableCreateTypeCombo.Id)))
        {
            std::vector<std::string> Labels{};
            Labels.reserve(m_conduitWorkspaceState.VariableTypeOptions.size());
            int32_t SelectedIndex = -1;
            for (std::size_t Index = 0; Index < m_conduitWorkspaceState.VariableTypeOptions.size(); ++Index)
            {
                Labels.push_back(m_conduitWorkspaceState.VariableTypeOptions[Index].Label);
                if (m_conduitWorkspaceState.VariableTypeOptions[Index].Type == m_conduitCreateSelectedVariableType)
                {
                    SelectedIndex = static_cast<int32_t>(Index);
                }
            }
            if (SelectedIndex < 0 && !m_conduitWorkspaceState.VariableTypeOptions.empty())
            {
                m_conduitCreateSelectedVariableType = m_conduitWorkspaceState.VariableTypeOptions.front().Type;
                SelectedIndex = 0;
            }
            Combo->SetItems(std::move(Labels));
            (void)Combo->SetSelectedIndex(SelectedIndex, false);
        }
    }

    if (m_conduitVariableCreateNameInput.Id.Value != 0)
    {
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_conduitVariableCreateNameInput.Id)))
        {
            Input->Text().Set(m_conduitCreateVariableNameText);
        }
    }

    const auto& Inspector = m_conduitWorkspaceState.SelectedVariable;
    const auto& NodeInspector = m_conduitWorkspaceState.SelectedNode;
    if (m_conduitVariableNameInput.Id.Value != 0)
    {
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_conduitVariableNameInput.Id)))
        {
            if (!Input->IsFocused())
            {
                Input->Text().Set(Inspector.HasSelection ? Inspector.Name : std::string{});
            }
        }
    }
    if (m_conduitVariableTypeCombo.Id.Value != 0)
    {
        if (auto* Combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(m_conduitVariableTypeCombo.Id)))
        {
            std::vector<std::string> Labels{};
            Labels.reserve(m_conduitWorkspaceState.VariableTypeOptions.size());
            int32_t SelectedIndex = -1;
            for (std::size_t Index = 0; Index < m_conduitWorkspaceState.VariableTypeOptions.size(); ++Index)
            {
                Labels.push_back(m_conduitWorkspaceState.VariableTypeOptions[Index].Label);
                if (Inspector.HasSelection && m_conduitWorkspaceState.VariableTypeOptions[Index].Type == Inspector.Type)
                {
                    SelectedIndex = static_cast<int32_t>(Index);
                }
            }
            Combo->SetItems(std::move(Labels));
            (void)Combo->SetSelectedIndex(SelectedIndex, false);
        }
    }

    std::string DefaultHint = "Select a graph variable to edit its default value.";
    if (Inspector.HasSelection)
    {
        switch (Inspector.DefaultEditorKind)
        {
        case ConduitWorkspaceState::EVariableDefaultEditorKind::Bool:
            DefaultHint = "Bool default applies immediately.";
            break;
        case ConduitWorkspaceState::EVariableDefaultEditorKind::Text:
            DefaultHint = "Press Enter in the text field to apply the default.";
            break;
        case ConduitWorkspaceState::EVariableDefaultEditorKind::Enum:
            DefaultHint = "Selecting an enum entry applies the default immediately.";
            break;
        case ConduitWorkspaceState::EVariableDefaultEditorKind::Complex:
            DefaultHint = "Edit reflected fields below, then click Apply Default.";
            break;
        case ConduitWorkspaceState::EVariableDefaultEditorKind::None:
        default:
            DefaultHint = "Selected type has no inline default editor yet.";
            break;
        }
    }
    SetText(m_conduitVariableDefaultHintText, DefaultHint);
    SetText(m_conduitNodeSummaryText,
            ShowNodeInspector ? (NodeInspector.Detail.empty() ? NodeInspector.Title : NodeInspector.Detail)
                              : std::string("Select an authored Conduit node to edit entry names and control-flow labels."));
    SetText(m_conduitNodePrimaryLabelText, NodeInspector.PrimaryTextLabel);
    SetText(m_conduitNodeSecondaryLabelText, NodeInspector.SecondaryTextLabel);
    SetVisibility(m_conduitNodePrimaryLabelText,
                  ShowNodeInspector && NodeInspector.CanEditPrimaryText
                      ? SnAPI::UI::EVisibility::Visible
                      : SnAPI::UI::EVisibility::Collapsed);
    SetVisibility(m_conduitNodePrimaryTextInput,
                  ShowNodeInspector && NodeInspector.CanEditPrimaryText
                      ? SnAPI::UI::EVisibility::Visible
                      : SnAPI::UI::EVisibility::Collapsed);
    SetVisibility(m_conduitNodeSecondaryLabelText,
                  ShowNodeInspector && NodeInspector.CanEditSecondaryText
                      ? SnAPI::UI::EVisibility::Visible
                      : SnAPI::UI::EVisibility::Collapsed);
    SetVisibility(m_conduitNodeSecondaryTextInput,
                  ShowNodeInspector && NodeInspector.CanEditSecondaryText
                      ? SnAPI::UI::EVisibility::Visible
                      : SnAPI::UI::EVisibility::Collapsed);
    if (m_conduitNodePrimaryTextInput.Id.Value != 0)
    {
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_conduitNodePrimaryTextInput.Id)))
        {
            if (!Input->IsFocused())
            {
                Input->Text().Set(NodeInspector.CanEditPrimaryText ? NodeInspector.PrimaryTextValue : std::string{});
            }
        }
    }
    if (m_conduitNodeSecondaryTextInput.Id.Value != 0)
    {
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_conduitNodeSecondaryTextInput.Id)))
        {
            if (!Input->IsFocused())
            {
                Input->Text().Set(NodeInspector.CanEditSecondaryText ? NodeInspector.SecondaryTextValue : std::string{});
            }
        }
    }

    if (m_conduitVariableDefaultBoolCheckbox.Id.Value != 0)
    {
        if (auto* Checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_context->GetElement(m_conduitVariableDefaultBoolCheckbox.Id)))
        {
            Checkbox->Checked().Set(Inspector.BoolValue);
        }
    }
    if (m_conduitVariableDefaultTextInput.Id.Value != 0)
    {
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_conduitVariableDefaultTextInput.Id)))
        {
            if (!Input->IsFocused())
            {
                Input->Text().Set(Inspector.TextValue);
            }
        }
    }
    if (m_conduitVariableDefaultEnumCombo.Id.Value != 0)
    {
        if (auto* Combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(m_conduitVariableDefaultEnumCombo.Id)))
        {
            Combo->SetItems(Inspector.EnumOptions);
            (void)Combo->SetSelectedIndex(Inspector.SelectedEnumIndex, false);
        }
    }

    const auto& ClassInspector = m_conduitWorkspaceState.SelectedClass;
    if (m_conduitClassNameInput.Id.Value != 0)
    {
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_conduitClassNameInput.Id)))
        {
            if (!Input->IsFocused())
            {
                Input->Text().Set(ClassInspector.HasSelection ? ClassInspector.Name : std::string{});
            }
        }
    }
    if (m_conduitClassHostTypeCombo.Id.Value != 0)
    {
        if (auto* Combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(m_conduitClassHostTypeCombo.Id)))
        {
            std::vector<std::string> Labels{};
            Labels.reserve(m_conduitWorkspaceState.ClassHostTypeOptions.size());
            int32_t SelectedIndex = -1;
            for (std::size_t Index = 0; Index < m_conduitWorkspaceState.ClassHostTypeOptions.size(); ++Index)
            {
                Labels.push_back(m_conduitWorkspaceState.ClassHostTypeOptions[Index].Label);
                if (ClassInspector.HasSelection &&
                    m_conduitWorkspaceState.ClassHostTypeOptions[Index].Type == ClassInspector.HostType)
                {
                    SelectedIndex = static_cast<int32_t>(Index);
                }
            }
            Combo->SetItems(std::move(Labels));
            (void)Combo->SetSelectedIndex(SelectedIndex, false);
        }
    }
    if (m_conduitClassGraphCombo.Id.Value != 0)
    {
        if (auto* Combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(m_conduitClassGraphCombo.Id)))
        {
            std::vector<std::string> Labels{};
            Labels.reserve(m_conduitWorkspaceState.ClassGraphOptions.size());
            int32_t SelectedIndex = -1;
            for (std::size_t Index = 0; Index < m_conduitWorkspaceState.ClassGraphOptions.size(); ++Index)
            {
                Labels.push_back(m_conduitWorkspaceState.ClassGraphOptions[Index].Label);
                if (ClassInspector.HasSelection &&
                    m_conduitWorkspaceState.ClassGraphOptions[Index].AssetKey == ClassInspector.GraphAssetKey)
                {
                    SelectedIndex = static_cast<int32_t>(Index);
                }
            }
            Combo->SetItems(std::move(Labels));
            (void)Combo->SetSelectedIndex(SelectedIndex, false);
        }
    }

    if (UIPropertyPanel* Panel = ResolveConduitVariableDefaultPanel())
    {
        if (Inspector.DefaultEditorKind == ConduitWorkspaceState::EVariableDefaultEditorKind::Complex &&
            Inspector.ComplexObject != nullptr &&
            Inspector.ComplexType != TypeId{})
        {
            m_conduitVariableDefaultPanelBound = Panel->BindObject(Inspector.ComplexType, Inspector.ComplexObject);
            m_conduitVariableDefaultBoundObject = Inspector.ComplexObject;
            m_conduitVariableDefaultBoundType = Inspector.ComplexType;
        }
        else
        {
            Panel->ClearObject();
            m_conduitVariableDefaultPanelBound = false;
            m_conduitVariableDefaultBoundObject = nullptr;
            m_conduitVariableDefaultBoundType = {};
        }
    }

    m_context->MarkLayoutDirty();
}

UIPropertyPanel* EditorLayout::ResolveConduitVariableDefaultPanel() const
{
    if (!m_context || m_conduitVariableDefaultPropertyPanel.Id.Value == 0)
    {
        return nullptr;
    }

    return dynamic_cast<UIPropertyPanel*>(&m_context->GetElement(m_conduitVariableDefaultPropertyPanel.Id));
}

void EditorLayout::HandleContentAssetCardClicked(const std::size_t CardIndex)
{
    if (CardIndex >= m_contentBrowserEntries.size())
    {
        return;
    }

    const ContentBrowserEntry& Entry = m_contentBrowserEntries[CardIndex];
    const auto Now = std::chrono::steady_clock::now();
    const std::string ClickKey = Entry.IsFolder ? std::string("folder:") + Entry.FolderPath
                                                : (Entry.AssetIndex < m_contentAssets.size()
                                                       ? m_contentAssets[Entry.AssetIndex].Key
                                                       : std::string{});
    const bool IsDoubleClick = !ClickKey.empty() &&
                               (m_lastContentAssetClickKey == ClickKey) &&
                               (std::chrono::duration_cast<std::chrono::milliseconds>(Now - m_lastContentAssetClickTime).count() <= 350);
    m_lastContentAssetClickKey = ClickKey;
    m_lastContentAssetClickTime = Now;

    if (Entry.IsFolder)
    {
        m_selectedContentFolderPath = NormalizeBrowserPath(Entry.FolderPath);
        m_selectedContentAssetKey.clear();
        ViewModelProperty<std::string>(kVmSelectedContentAssetKey).Set(std::string{});
        RefreshContentAssetCardSelectionStyles();

        if (!IsDoubleClick)
        {
            m_contentAssetDetails.Name.clear();
            m_contentAssetDetails.Type.clear();
            m_contentAssetDetails.Variant.clear();
            m_contentAssetDetails.AssetId.clear();
            m_contentAssetDetails.Status =
                "Folder selected: " + (Entry.DisplayName.empty() ? Entry.FolderPath : Entry.DisplayName) +
                " (double-click to open)";
            RefreshContentAssetDetailsViewModel();
            return;
        }

        m_contentCurrentFolder = m_selectedContentFolderPath;
        m_selectedContentAssetKey.clear();
        ViewModelProperty<std::string>(kVmSelectedContentAssetKey).Set(std::string{});
        m_contentAssetDetails.Status = "Opened folder: " + (Entry.DisplayName.empty() ? Entry.FolderPath : Entry.DisplayName);
        ApplyContentAssetFilter();
        return;
    }

    const std::size_t AssetIndex = Entry.AssetIndex;
    if (AssetIndex >= m_contentAssets.size())
    {
        return;
    }

    SelectContentAsset(AssetIndex, true, IsDoubleClick);
}

void EditorLayout::SelectContentAsset(const std::size_t AssetIndex,
                                      const bool NotifySelection,
                                      const bool IsDoubleClick)
{
    if (AssetIndex >= m_contentAssets.size())
    {
        return;
    }

    const ContentAssetEntry& Asset = m_contentAssets[AssetIndex];
    m_selectedContentFolderPath.clear();
    m_selectedContentAssetKey = Asset.Key;
    ViewModelProperty<std::string>(kVmSelectedContentAssetKey).Set(m_selectedContentAssetKey);

    m_contentAssetDetails.Name.clear();
    m_contentAssetDetails.Type.clear();
    m_contentAssetDetails.Variant.clear();
    m_contentAssetDetails.AssetId.clear();
    m_contentAssetDetails.Status = IsDoubleClick ? std::string("Loading preview...") : std::string("Selected");

    if (NotifySelection && m_onContentAssetSelected)
    {
        m_onContentAssetSelected(Asset.Key, IsDoubleClick);
    }

    RefreshContentAssetDetailsViewModel();
}

void EditorLayout::OpenHierarchyContextMenu(const std::size_t ItemIndex, const SnAPI::UI::PointerEvent& Event)
{
    if (ItemIndex >= m_hierarchyVisibleNodes.size())
    {
        return;
    }

    CloseContextMenu();
    m_contextMenuScope = EContextMenuScope::HierarchyItem;
    m_contextMenuHierarchyIndex = ItemIndex;
    m_contextMenuAssetIndex.reset();
    m_contextMenuContentInspectorNode = {};
    m_contextMenuComponentOwner.reset();
    m_contextMenuComponentType = {};
    m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    m_pendingHierarchyMenuIndex.reset();
    m_pendingHierarchyMenuOpenPosition = {};
    m_contextMenuNodeTypes.clear();
    m_contextMenuComponentTypes.clear();
    m_contextMenuOpenPosition = Event.Position;

    const NodeHandle Handle = m_hierarchyVisibleNodes[ItemIndex];
    const bool HasNodeHandle = !Handle.IsNull();
    const bool IsSelected = HasNodeHandle && m_selection && (m_selection->SelectedNode() == Handle);

    std::vector<SnAPI::UI::UIContextMenuItem> Items{};
    if (HasNodeHandle)
    {
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemHierarchySelectId),
            .Label = "Select",
            .Shortcut = std::string("Enter"),
            .Enabled = true,
            .IsSeparator = false,
            .Checked = IsSelected,
        });
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemHierarchyAddNodeId),
            .Label = "Add Child Node...",
            .Shortcut = std::nullopt,
            .Enabled = true,
            .IsSeparator = false,
            .Checked = false,
        });
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemHierarchyAddComponentId),
            .Label = "Add Component...",
            .Shortcut = std::nullopt,
            .Enabled = true,
            .IsSeparator = false,
            .Checked = false,
        });
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = "hierarchy.sep.prefab",
            .Label = {},
            .Shortcut = std::nullopt,
            .Enabled = false,
            .IsSeparator = true,
            .Checked = false,
        });
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemHierarchyCreatePrefabId),
            .Label = "Create Prefab",
            .Shortcut = std::nullopt,
            .Enabled = true,
            .IsSeparator = false,
            .Checked = false,
        });
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = "hierarchy.sep.delete",
            .Label = {},
            .Shortcut = std::nullopt,
            .Enabled = false,
            .IsSeparator = true,
            .Checked = false,
        });
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemHierarchyDeleteId),
            .Label = "Delete",
            .Shortcut = std::string("Del"),
            .Enabled = true,
            .IsSeparator = false,
            .Checked = false,
        });
    }
    else
    {
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemHierarchySelectId),
            .Label = "World Root",
            .Shortcut = std::nullopt,
            .Enabled = false,
            .IsSeparator = false,
            .Checked = false,
        });
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemHierarchyAddNodeId),
            .Label = "Add Node...",
            .Shortcut = std::nullopt,
            .Enabled = true,
            .IsSeparator = false,
            .Checked = false,
        });
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemHierarchyAddComponentId),
            .Label = "Add Component...",
            .Shortcut = std::nullopt,
            .Enabled = false,
            .IsSeparator = false,
            .Checked = false,
        });
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemHierarchyCreatePrefabId),
            .Label = "Create Prefab",
            .Shortcut = std::nullopt,
            .Enabled = false,
            .IsSeparator = false,
            .Checked = false,
        });
    }

    OpenContextMenu(Event.Position, std::move(Items));
}

void EditorLayout::OpenHierarchyAddTypeMenu(const bool AddComponents)
{
    if (!m_contextMenuHierarchyIndex.has_value())
    {
        return;
    }

    const std::size_t ItemIndex = *m_contextMenuHierarchyIndex;
    if (ItemIndex >= m_hierarchyVisibleNodes.size())
    {
        return;
    }

    const NodeHandle TargetHandle = m_hierarchyVisibleNodes[ItemIndex];
    const bool TargetIsWorldRoot = TargetHandle.IsNull();

    std::vector<const TypeInfo*> CandidateTypes{};
    if (AddComponents)
    {
        (void)TypeAutoRegistry::Instance().EnsureAll();

        const auto RegisteredComponentTypes = ComponentSerializationRegistry::Instance().Types();
        CandidateTypes.reserve(RegisteredComponentTypes.size());
        for (const TypeId& ComponentType : RegisteredComponentTypes)
        {
            if (const TypeInfo* Info = TypeRegistry::Instance().Find(ComponentType))
            {
                CandidateTypes.push_back(Info);
            }
        }
    }
    else
    {
        EnsureReflectionRegistered<PawnBase>();
        EnsureReflectionRegistered<PlayerStart>();

        CandidateTypes = TypeRegistry::Instance().Derived(StaticTypeId<BaseNode>());
        if (const TypeInfo* BaseNodeInfo = TypeRegistry::Instance().Find(StaticTypeId<BaseNode>()))
        {
            const bool AlreadyPresent = std::ranges::any_of(CandidateTypes, [BaseNodeInfo](const TypeInfo* Type) {
                return Type && Type->Id == BaseNodeInfo->Id;
            });
            if (!AlreadyPresent)
            {
                CandidateTypes.push_back(BaseNodeInfo);
            }
        }
    }
    std::sort(CandidateTypes.begin(), CandidateTypes.end(), [](const TypeInfo* Left, const TypeInfo* Right) {
        if (!Left || !Right)
        {
            return Left < Right;
        }
        const std::string LeftName = ShortTypeLabel(Left->Name);
        const std::string RightName = ShortTypeLabel(Right->Name);
        return LeftName < RightName;
    });

    auto& TypeList = AddComponents ? m_contextMenuComponentTypes : m_contextMenuNodeTypes;
    TypeList.clear();

    std::vector<SnAPI::UI::UIContextMenuItem> Items{};
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemHierarchyBackId),
        .Label = "Back",
        .Shortcut = std::nullopt,
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = "hierarchy.sep.types",
        .Label = {},
        .Shortcut = std::nullopt,
        .Enabled = false,
        .IsSeparator = true,
        .Checked = false,
    });

    for (const TypeInfo* Type : CandidateTypes)
    {
        if (!Type || !HasDefaultConstructor(*Type))
        {
            continue;
        }

        if (AddComponents)
        {
            if (!ComponentSerializationRegistry::Instance().Has(Type->Id))
            {
                continue;
            }
        }
        else
        {
            if (!TypeRegistry::Instance().IsA(Type->Id, StaticTypeId<BaseNode>()))
            {
                continue;
            }
            if (TypeRegistry::Instance().IsA(Type->Id, StaticTypeId<World>()))
            {
                continue;
            }
            if (TypeRegistry::Instance().IsA(Type->Id, StaticTypeId<Level>()) && !TargetIsWorldRoot)
            {
                continue;
            }
        }

        const std::size_t TypeIndex = TypeList.size();
        TypeList.push_back(Type->Id);
        const std::string ItemId = std::string(
            AddComponents ? kContextMenuItemHierarchyAddComponentTypePrefix : kContextMenuItemHierarchyAddNodeTypePrefix) + std::to_string(TypeIndex);

        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::move(ItemId),
            .Label = ShortTypeLabel(Type->Name),
            .Shortcut = std::nullopt,
            .Enabled = true,
            .IsSeparator = false,
            .Checked = false,
        });
    }

    if (TypeList.empty())
    {
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = "hierarchy.none",
            .Label = AddComponents ? "No creatable component types" : "No creatable node types",
            .Shortcut = std::nullopt,
            .Enabled = false,
            .IsSeparator = false,
            .Checked = false,
        });
    }

    OpenContextMenu(m_contextMenuOpenPosition, std::move(Items));
}

void EditorLayout::OpenContentAssetContextMenu(const std::size_t CardIndex, const SnAPI::UI::PointerEvent& Event)
{
    if (CardIndex >= m_contentBrowserEntries.size())
    {
        return;
    }

    const ContentBrowserEntry& Entry = m_contentBrowserEntries[CardIndex];
    if (Entry.IsFolder)
    {
        OpenContentBrowserContextMenu(Event);
        return;
    }

    const std::size_t AssetIndex = Entry.AssetIndex;
    if (AssetIndex >= m_contentAssets.size())
    {
        return;
    }

    SelectContentAsset(AssetIndex, true, false);
    CloseContextMenu();
    m_contextMenuScope = EContextMenuScope::ContentAssetItem;
    m_contextMenuAssetIndex = AssetIndex;
    m_contextMenuHierarchyIndex.reset();
    m_contextMenuContentInspectorNode = {};
    m_contextMenuComponentOwner.reset();
    m_contextMenuComponentType = {};
    m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    m_pendingHierarchyMenuIndex.reset();
    m_pendingHierarchyMenuOpenPosition = {};

    const bool CanPlace = ViewModelProperty<bool>(kVmContentAssetCanPlaceKey).Get();
    const bool CanSave = ViewModelProperty<bool>(kVmContentAssetCanSaveKey).Get();
    const bool IsRuntime = m_contentAssets[AssetIndex].IsRuntime;
    const bool IsDirty = m_contentAssets[AssetIndex].IsDirty;
    std::vector<SnAPI::UI::UIContextMenuItem> Items{};
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemAssetSelectId),
        .Label = "Select",
        .Shortcut = std::string("Enter"),
        .Enabled = true,
        .IsSeparator = false,
        .Checked = true,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemAssetPreviewId),
        .Label = "Open Preview",
        .Shortcut = std::string("Double-click"),
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemAssetRenameId),
        .Label = "Rename",
        .Shortcut = std::string("F2"),
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = "asset.sep.actions",
        .Label = {},
        .Shortcut = std::nullopt,
        .Enabled = false,
        .IsSeparator = true,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemAssetPlaceId),
        .Label = "Place In Scene",
        .Shortcut = std::string("P"),
        .Enabled = CanPlace,
        .IsSeparator = false,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemAssetSaveId),
        .Label = IsRuntime ? "Save Asset" : "Save Update",
        .Shortcut = std::string("S"),
        .Enabled = CanSave && (!IsRuntime || IsDirty),
        .IsSeparator = false,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = "asset.sep.delete",
        .Label = {},
        .Shortcut = std::nullopt,
        .Enabled = false,
        .IsSeparator = true,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemAssetDeleteId),
        .Label = "Delete",
        .Shortcut = std::string("Del"),
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });

    OpenContextMenu(Event.Position, std::move(Items));
}

void EditorLayout::OpenFileMenu()
{
    if (!m_context)
    {
        return;
    }

    CloseContextMenu();
    m_contextMenuScope = EContextMenuScope::MenuBar;
    m_contextMenuHierarchyIndex.reset();
    m_contextMenuAssetIndex.reset();
    m_contextMenuContentInspectorNode = {};
    m_contextMenuComponentOwner.reset();
    m_contextMenuComponentType = {};
    m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    m_pendingHierarchyMenuIndex.reset();
    m_pendingHierarchyMenuOpenPosition = {};

    SnAPI::UI::UIPoint OpenPosition{12.0f, 28.0f};
    if (m_menuFileButton.Id.Value != 0)
    {
        if (auto* MenuButton = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_menuFileButton.Id)))
        {
            const SnAPI::UI::UIRect Rect = MenuButton->LayoutRect();
            OpenPosition.X = Rect.X;
            OpenPosition.Y = Rect.Y + Rect.H + 2.0f;
        }
    }
    m_contextMenuOpenPosition = OpenPosition;

    std::vector<SnAPI::UI::UIContextMenuItem> Items{};
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemFileNewProjectId),
        .Label = "New Project...",
        .Shortcut = std::nullopt,
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemFileOpenProjectId),
        .Label = "Open Project...",
        .Shortcut = std::nullopt,
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = "menu.file.sep.project",
        .Label = {},
        .Shortcut = std::nullopt,
        .Enabled = false,
        .IsSeparator = true,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemFileProjectSettingsId),
        .Label = "Project Settings...",
        .Shortcut = std::nullopt,
        .Enabled = m_projectState.IsLoaded,
        .IsSeparator = false,
        .Checked = false,
    });

    OpenContextMenu(OpenPosition, std::move(Items));
}

void EditorLayout::OpenInspectorComponentContextMenu(const NodeHandle& OwnerNode,
                                                     const TypeId& ComponentType,
                                                     const SnAPI::UI::PointerEvent& Event)
{
    if (OwnerNode.IsNull() || ComponentType == TypeId{})
    {
        return;
    }

    CloseContextMenu();
    m_contextMenuScope = EContextMenuScope::InspectorComponent;
    m_contextMenuHierarchyIndex.reset();
    m_contextMenuAssetIndex.reset();
    m_contextMenuContentInspectorNode = {};
    m_contextMenuComponentOwner = OwnerNode;
    m_contextMenuComponentType = ComponentType;
    m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    m_pendingHierarchyMenuIndex.reset();
    m_pendingHierarchyMenuOpenPosition = {};
    m_contextMenuOpenPosition = Event.Position;

    std::string ComponentLabel = std::string("Delete Component");
    if (const TypeInfo* Type = TypeRegistry::Instance().Find(ComponentType))
    {
        ComponentLabel = "Delete " + ShortTypeLabel(Type->Name);
    }

    std::vector<SnAPI::UI::UIContextMenuItem> Items{};
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemInspectorDeleteComponentId),
        .Label = std::move(ComponentLabel),
        .Shortcut = std::string("Del"),
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });

    OpenContextMenu(Event.Position, std::move(Items));
}

void EditorLayout::OpenContentAssetInspectorHierarchyContextMenu(const std::size_t ItemIndex,
                                                                 const SnAPI::UI::PointerEvent& Event)
{
    if (ItemIndex >= m_contentInspectorVisibleNodes.size())
    {
        return;
    }

    const NodeHandle TargetNode = m_contentInspectorVisibleNodes[ItemIndex];
    if (TargetNode.IsNull())
    {
        return;
    }

    CloseContextMenu();
    m_contextMenuScope = EContextMenuScope::ContentInspectorHierarchyItem;
    m_contextMenuHierarchyIndex.reset();
    m_contextMenuAssetIndex.reset();
    m_contextMenuContentInspectorNode = TargetNode;
    m_contextMenuComponentOwner.reset();
    m_contextMenuComponentType = {};
    m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    m_pendingHierarchyMenuIndex.reset();
    m_pendingHierarchyMenuOpenPosition = {};
    m_contextMenuNodeTypes.clear();
    m_contextMenuComponentTypes.clear();
    m_contextMenuOpenPosition = Event.Position;

    const bool IsRootNode = !m_contentInspectorVisibleNodes.empty() && (TargetNode == m_contentInspectorVisibleNodes.front());
    const bool IsSelected = (m_contentAssetInspectorState.SelectedNode == TargetNode);

    std::vector<SnAPI::UI::UIContextMenuItem> Items{};
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemContentInspectorSelectId),
        .Label = "Select",
        .Shortcut = std::string("Enter"),
        .Enabled = true,
        .IsSeparator = false,
        .Checked = IsSelected,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = "asset_inspector.sep.add_node",
        .Label = {},
        .Shortcut = std::nullopt,
        .Enabled = false,
        .IsSeparator = true,
        .Checked = false,
    });

    for (const TypeInfo* Type : CollectContentInspectorCreatableNodeTypes())
    {
        if (!Type)
        {
            continue;
        }
        const std::size_t TypeIndex = m_contextMenuNodeTypes.size();
        m_contextMenuNodeTypes.push_back(Type->Id);
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemContentInspectorAddNodeTypePrefix) + std::to_string(TypeIndex),
            .Label = "Add Child Node: " + ShortTypeLabel(Type->Name),
            .Shortcut = std::nullopt,
            .Enabled = true,
            .IsSeparator = false,
            .Checked = false,
        });
    }

    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = "asset_inspector.sep.add_component",
        .Label = {},
        .Shortcut = std::nullopt,
        .Enabled = false,
        .IsSeparator = true,
        .Checked = false,
    });

    for (const TypeInfo* Type : CollectContentInspectorCreatableComponentTypes())
    {
        if (!Type)
        {
            continue;
        }
        const std::size_t TypeIndex = m_contextMenuComponentTypes.size();
        m_contextMenuComponentTypes.push_back(Type->Id);
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemContentInspectorAddComponentTypePrefix) + std::to_string(TypeIndex),
            .Label = "Add Component: " + ShortTypeLabel(Type->Name),
            .Shortcut = std::nullopt,
            .Enabled = true,
            .IsSeparator = false,
            .Checked = false,
        });
    }

    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = "asset_inspector.sep.delete",
        .Label = {},
        .Shortcut = std::nullopt,
        .Enabled = false,
        .IsSeparator = true,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemContentInspectorDeleteNodeId),
        .Label = "Delete Node",
        .Shortcut = std::string("Del"),
        .Enabled = !IsRootNode,
        .IsSeparator = false,
        .Checked = false,
    });

    OpenContextMenu(Event.Position, std::move(Items));
}

void EditorLayout::OpenContentAssetInspectorComponentContextMenu(const NodeHandle& OwnerNode,
                                                                 const TypeId& ComponentType,
                                                                 const SnAPI::UI::PointerEvent& Event)
{
    if (OwnerNode.IsNull() || ComponentType == TypeId{})
    {
        return;
    }

    CloseContextMenu();
    m_contextMenuScope = EContextMenuScope::ContentInspectorComponent;
    m_contextMenuHierarchyIndex.reset();
    m_contextMenuAssetIndex.reset();
    m_contextMenuContentInspectorNode = OwnerNode;
    m_contextMenuComponentOwner = OwnerNode;
    m_contextMenuComponentType = ComponentType;
    m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    m_pendingHierarchyMenuIndex.reset();
    m_pendingHierarchyMenuOpenPosition = {};
    m_contextMenuNodeTypes.clear();
    m_contextMenuComponentTypes.clear();
    m_contextMenuOpenPosition = Event.Position;

    std::string ComponentLabel = std::string("Delete Component");
    if (const TypeInfo* Type = TypeRegistry::Instance().Find(ComponentType))
    {
        ComponentLabel = "Delete " + ShortTypeLabel(Type->Name);
    }

    std::vector<SnAPI::UI::UIContextMenuItem> Items{};
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemContentInspectorDeleteComponentId),
        .Label = std::move(ComponentLabel),
        .Shortcut = std::string("Del"),
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });

    OpenContextMenu(Event.Position, std::move(Items));
}

void EditorLayout::OpenContentBrowserContextMenu(const SnAPI::UI::PointerEvent& Event)
{
    CloseContextMenu();
    m_contextMenuScope = EContextMenuScope::ContentBrowser;
    m_contextMenuHierarchyIndex.reset();
    m_contextMenuAssetIndex.reset();
    m_contextMenuContentInspectorNode = {};
    m_contextMenuComponentOwner.reset();
    m_contextMenuComponentType = {};
    m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    m_pendingHierarchyMenuIndex.reset();
    m_pendingHierarchyMenuOpenPosition = {};

    std::vector<SnAPI::UI::UIContextMenuItem> Items{};
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemAssetCreateId),
        .Label = "Create...",
        .Shortcut = std::string("N"),
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemAssetImportId),
        .Label = "Import...",
        .Shortcut = std::string("I"),
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = "asset.sep.browser",
        .Label = {},
        .Shortcut = std::nullopt,
        .Enabled = false,
        .IsSeparator = true,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemAssetRescanId),
        .Label = "Rescan Assets",
        .Shortcut = std::string("R"),
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });

    OpenContextMenu(Event.Position, std::move(Items));
}

void EditorLayout::OpenContextMenu(const SnAPI::UI::UIPoint& ScreenPosition,
                                   std::vector<SnAPI::UI::UIContextMenuItem> Items)
{
    if (!m_context || Items.empty())
    {
        return;
    }

    EnsureContextMenuOverlay();
    if (m_contextMenu.Id.Value == 0)
    {
        return;
    }

    auto* Menu = dynamic_cast<SnAPI::UI::UIContextMenu*>(&m_context->GetElement(m_contextMenu.Id));
    if (!Menu)
    {
        return;
    }

    const float Dpi = m_context->GetDpiScale();
    const float ItemHeight = std::max(1.0f, Menu->ItemHeight().Get() * Dpi);
    const float RowGap = std::max(0.0f, Menu->RowGap().Get() * Dpi);
    const float PaddingY = std::max(0.0f, Menu->PaddingY().Get() * Dpi);
    const float EstimatedHeight = PaddingY * 2.0f +
                                  static_cast<float>(Items.size()) * ItemHeight +
                                  static_cast<float>(std::max<std::size_t>(0, Items.size() - 1)) * RowGap;
    const float EstimatedWidth = std::max(
        Menu->MinMenuWidth().Get() * Dpi,
        Menu->MaxMenuWidth().Get() > 0.0f ? Menu->MaxMenuWidth().Get() * Dpi : Menu->MinMenuWidth().Get() * Dpi);

    const SnAPI::UI::UISize ViewportSize = m_context->GetViewportSize();
    SnAPI::UI::UIPoint ClampedPosition = ScreenPosition;
    ClampedPosition.X = std::clamp(ClampedPosition.X, 0.0f, std::max(0.0f, ViewportSize.W - EstimatedWidth));
    ClampedPosition.Y = std::clamp(ClampedPosition.Y, 0.0f, std::max(0.0f, ViewportSize.H - EstimatedHeight));

    Menu->SetItems(std::move(Items));
    Menu->OpenAt(ClampedPosition);
    m_context->SetCapture(m_contextMenu.Id);
}

void EditorLayout::CloseContextMenu()
{
    const SnAPI::UI::ElementId MenuId = m_contextMenu.Id;
    if (m_context && MenuId.Value != 0)
    {
        if (m_context->GetCapture() == MenuId)
        {
            m_context->ReleaseCapture();
        }

        m_context->DestroyElement(MenuId);
    }

    m_contextMenu = {};
    m_contextMenuScope = EContextMenuScope::None;
    m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    m_pendingHierarchyMenuIndex.reset();
    m_pendingHierarchyMenuOpenPosition = {};
    m_contextMenuHierarchyIndex.reset();
    m_contextMenuAssetIndex.reset();
    m_contextMenuContentInspectorNode = {};
    m_contextMenuComponentOwner.reset();
    m_contextMenuComponentType = {};
}

void EditorLayout::OnContextMenuItemInvoked(const SnAPI::UI::UIContextMenuItem& Item)
{
    if (Item.Id == kContextMenuItemAssetRescanId)
    {
        if (m_onContentAssetRefreshRequested)
        {
            m_onContentAssetRefreshRequested();
        }
        return;
    }

    if (m_contextMenuScope == EContextMenuScope::MenuBar)
    {
        if (Item.Id == kContextMenuItemFileNewProjectId)
        {
            OpenProjectCreateModal();
        }
        else if (Item.Id == kContextMenuItemFileOpenProjectId)
        {
            OpenProjectOpenModal();
        }
        else if (Item.Id == kContextMenuItemFileProjectSettingsId)
        {
            OpenProjectSettingsModal();
        }
        return;
    }

    if (m_contextMenuScope == EContextMenuScope::HierarchyItem)
    {
        if (!m_contextMenuHierarchyIndex.has_value())
        {
            return;
        }

        const std::size_t ItemIndex = *m_contextMenuHierarchyIndex;
        if (ItemIndex >= m_hierarchyVisibleNodes.size())
        {
            return;
        }

        const NodeHandle Handle = m_hierarchyVisibleNodes[ItemIndex];
        const bool TargetIsWorldRoot = Handle.IsNull();

        if (Item.Id == kContextMenuItemHierarchySelectId)
        {
            if (!Handle.IsNull())
            {
                OnHierarchyNodeChosen(Handle);
            }
            return;
        }

        if (Item.Id == kContextMenuItemHierarchyAddNodeId)
        {
            m_pendingHierarchyMenu = EPendingHierarchyMenu::AddNodeTypes;
            m_pendingHierarchyMenuIndex = ItemIndex;
            m_pendingHierarchyMenuOpenPosition = m_contextMenuOpenPosition;
            return;
        }

        if (Item.Id == kContextMenuItemHierarchyAddComponentId)
        {
            m_pendingHierarchyMenu = EPendingHierarchyMenu::AddComponentTypes;
            m_pendingHierarchyMenuIndex = ItemIndex;
            m_pendingHierarchyMenuOpenPosition = m_contextMenuOpenPosition;
            return;
        }

        if (Item.Id == kContextMenuItemHierarchyBackId)
        {
            m_pendingHierarchyMenu = EPendingHierarchyMenu::Root;
            m_pendingHierarchyMenuIndex = ItemIndex;
            m_pendingHierarchyMenuOpenPosition = m_contextMenuOpenPosition;
            return;
        }

        if (Item.Id == kContextMenuItemHierarchyCreatePrefabId)
        {
            if (m_onHierarchyActionRequested && !Handle.IsNull())
            {
                HierarchyActionRequest Request{};
                Request.Action = EHierarchyAction::CreatePrefab;
                Request.TargetNode = Handle;
                Request.TargetIsWorldRoot = TargetIsWorldRoot;
                m_onHierarchyActionRequested(Request);
            }
            return;
        }

        if (Item.Id == kContextMenuItemHierarchyDeleteId)
        {
            if (m_onHierarchyActionRequested && !Handle.IsNull())
            {
                HierarchyActionRequest Request{};
                Request.Action = EHierarchyAction::DeleteNode;
                Request.TargetNode = Handle;
                Request.TargetIsWorldRoot = false;
                m_onHierarchyActionRequested(Request);
            }
            return;
        }

        if (const auto NodeTypeIndex = TryParsePrefixedIndex(Item.Id, kContextMenuItemHierarchyAddNodeTypePrefix))
        {
            if (m_onHierarchyActionRequested && *NodeTypeIndex < m_contextMenuNodeTypes.size())
            {
                HierarchyActionRequest Request{};
                Request.Action = EHierarchyAction::AddNodeType;
                Request.TargetNode = Handle;
                Request.TargetIsWorldRoot = TargetIsWorldRoot;
                Request.Type = m_contextMenuNodeTypes[*NodeTypeIndex];
                m_onHierarchyActionRequested(Request);
            }
            return;
        }

        if (const auto ComponentTypeIndex =
                TryParsePrefixedIndex(Item.Id, kContextMenuItemHierarchyAddComponentTypePrefix))
        {
            if (m_onHierarchyActionRequested && *ComponentTypeIndex < m_contextMenuComponentTypes.size())
            {
                HierarchyActionRequest Request{};
                Request.Action = EHierarchyAction::AddComponentType;
                Request.TargetNode = Handle;
                Request.TargetIsWorldRoot = TargetIsWorldRoot;
                Request.Type = m_contextMenuComponentTypes[*ComponentTypeIndex];
                m_onHierarchyActionRequested(Request);
            }
        }
        return;
    }

    if (m_contextMenuScope == EContextMenuScope::InspectorComponent)
    {
        if (Item.Id == kContextMenuItemInspectorDeleteComponentId &&
            m_onHierarchyActionRequested &&
            m_contextMenuComponentOwner.has_value() &&
            !m_contextMenuComponentOwner->IsNull() &&
            m_contextMenuComponentType != TypeId{})
        {
            HierarchyActionRequest Request{};
            Request.Action = EHierarchyAction::RemoveComponentType;
            Request.TargetNode = *m_contextMenuComponentOwner;
            Request.TargetIsWorldRoot = false;
            Request.Type = m_contextMenuComponentType;
            m_onHierarchyActionRequested(Request);
        }
        return;
    }

    if (m_contextMenuScope == EContextMenuScope::ContentInspectorHierarchyItem)
    {
        if (m_contextMenuContentInspectorNode.IsNull())
        {
            return;
        }

        const NodeHandle TargetNode = m_contextMenuContentInspectorNode;
        if (Item.Id == kContextMenuItemContentInspectorSelectId)
        {
            m_contentAssetInspectorState.SelectedNode = TargetNode;
            if (m_onContentAssetInspectorNodeSelected)
            {
                m_onContentAssetInspectorNodeSelected(TargetNode);
            }
            RebuildContentAssetInspectorHierarchyTree();
            RefreshContentAssetInspectorModalState();
            return;
        }

        if (Item.Id == kContextMenuItemContentInspectorDeleteNodeId)
        {
            if (m_onContentAssetInspectorHierarchyActionRequested)
            {
                HierarchyActionRequest Request{};
                Request.Action = EHierarchyAction::DeleteNode;
                Request.TargetNode = TargetNode;
                Request.TargetIsWorldRoot = false;
                m_onContentAssetInspectorHierarchyActionRequested(Request);
            }
            return;
        }

        if (const auto NodeTypeIndex =
                TryParsePrefixedIndex(Item.Id, kContextMenuItemContentInspectorAddNodeTypePrefix))
        {
            if (m_onContentAssetInspectorHierarchyActionRequested && *NodeTypeIndex < m_contextMenuNodeTypes.size())
            {
                HierarchyActionRequest Request{};
                Request.Action = EHierarchyAction::AddNodeType;
                Request.TargetNode = TargetNode;
                Request.TargetIsWorldRoot = false;
                Request.Type = m_contextMenuNodeTypes[*NodeTypeIndex];
                m_onContentAssetInspectorHierarchyActionRequested(Request);
            }
            return;
        }

        if (const auto ComponentTypeIndex =
                TryParsePrefixedIndex(Item.Id, kContextMenuItemContentInspectorAddComponentTypePrefix))
        {
            if (m_onContentAssetInspectorHierarchyActionRequested && *ComponentTypeIndex < m_contextMenuComponentTypes.size())
            {
                HierarchyActionRequest Request{};
                Request.Action = EHierarchyAction::AddComponentType;
                Request.TargetNode = TargetNode;
                Request.TargetIsWorldRoot = false;
                Request.Type = m_contextMenuComponentTypes[*ComponentTypeIndex];
                m_onContentAssetInspectorHierarchyActionRequested(Request);
            }
            return;
        }

        return;
    }

    if (m_contextMenuScope == EContextMenuScope::ContentInspectorComponent)
    {
        if (Item.Id == kContextMenuItemContentInspectorDeleteComponentId &&
            m_onContentAssetInspectorHierarchyActionRequested &&
            m_contextMenuComponentOwner.has_value() &&
            !m_contextMenuComponentOwner->IsNull() &&
            m_contextMenuComponentType != TypeId{})
        {
            HierarchyActionRequest Request{};
            Request.Action = EHierarchyAction::RemoveComponentType;
            Request.TargetNode = *m_contextMenuComponentOwner;
            Request.TargetIsWorldRoot = false;
            Request.Type = m_contextMenuComponentType;
            m_onContentAssetInspectorHierarchyActionRequested(Request);
        }
        return;
    }

    if (m_contextMenuScope == EContextMenuScope::ContentAssetItem)
    {
        if (!m_contextMenuAssetIndex.has_value())
        {
            return;
        }

        const std::size_t AssetIndex = *m_contextMenuAssetIndex;
        if (AssetIndex >= m_contentAssets.size())
        {
            return;
        }

        const std::string AssetKey = m_contentAssets[AssetIndex].Key;
        if (AssetKey.empty())
        {
            return;
        }

        if (Item.Id == kContextMenuItemAssetSelectId)
        {
            SelectContentAsset(AssetIndex, true, false);
            return;
        }

        if (Item.Id == kContextMenuItemAssetPreviewId)
        {
            SelectContentAsset(AssetIndex, true, true);
            return;
        }

        if (Item.Id == kContextMenuItemAssetRenameId)
        {
            SelectContentAsset(AssetIndex, true, false);
            m_contentAssetDetails.Status = "Edit Name in details and press Enter to rename.";
            RefreshContentAssetDetailsViewModel();
            return;
        }

        if (Item.Id == kContextMenuItemAssetPlaceId)
        {
            SelectContentAsset(AssetIndex, true, false);
            if (m_onContentAssetPlaceRequested && ViewModelProperty<bool>(kVmContentAssetCanPlaceKey).Get())
            {
                m_onContentAssetPlaceRequested(AssetKey);
            }
            return;
        }

        if (Item.Id == kContextMenuItemAssetSaveId)
        {
            SelectContentAsset(AssetIndex, true, false);
            if (m_onContentAssetSaveRequested && ViewModelProperty<bool>(kVmContentAssetCanSaveKey).Get())
            {
                m_onContentAssetSaveRequested(AssetKey);
            }
            return;
        }

        if (Item.Id == kContextMenuItemAssetDeleteId)
        {
            SelectContentAsset(AssetIndex, true, false);
            if (m_onContentAssetDeleteRequested)
            {
                m_onContentAssetDeleteRequested(AssetKey);
            }
            return;
        }
    }

    if (m_contextMenuScope == EContextMenuScope::ContentBrowser)
    {
        if (Item.Id == kContextMenuItemAssetCreateId)
        {
            OpenContentAssetCreateModal();
            return;
        }

        if (Item.Id == kContextMenuItemAssetImportId)
        {
            OpenContentAssetImportModal();
        }
    }
}

void EditorLayout::EnsureContentAssetCardCapacity()
{
    if (!m_context || m_contentAssetsList.Id.Value == 0)
    {
        return;
    }

    SnAPI::UI::TElementBuilder<SnAPI::UI::UIListView> AssetsListBuilder(
        m_context,
        SnAPI::UI::ElementHandle<SnAPI::UI::UIListView>{m_contentAssetsList.Id});

    while (m_contentAssetCards.size() < m_contentBrowserEntries.size())
    {
        const std::size_t CardIndex = m_contentAssetCards.size();
        auto CardButton = AssetsListBuilder.Add(SnAPI::UI::UIButton{});
        auto& CardButtonElement = CardButton.Element();
        CardButtonElement.ElementStyle().Apply("editor.asset_tile_button");
        CardButtonElement.Width().Set(SnAPI::UI::Sizing::Fill());
        CardButtonElement.Height().Set(SnAPI::UI::Sizing::Fill());
        CardButtonElement.ElementPadding().Set(SnAPI::UI::Padding{1.0f, 1.0f, 1.0f, 1.0f});
        CardButtonElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
        CardButtonElement.OnClick([this, CardIndex]() { HandleContentAssetCardClicked(CardIndex); });
        CardButtonElement.OnContextMenuRequested(
            SnAPI::UI::TDelegate<void(const SnAPI::UI::PointerEvent&)>::Bind(
                [this, CardIndex](const SnAPI::UI::PointerEvent& Event) {
                    OpenContentAssetContextMenu(CardIndex, Event);
                }));

        auto Card = CardButton.Add(SnAPI::UI::UIPanel("Editor.AssetCard"));
        auto& CardPanel = Card.Element();
        CardPanel.ElementStyle().Apply("editor.asset_card");
        CardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        CardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        CardPanel.Height().Set(SnAPI::UI::Sizing::Fill());
        CardPanel.Padding().Set(6.0f);
        CardPanel.Gap().Set(4.0f);
        CardPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

        auto Preview = Card.Add(SnAPI::UI::UIPanel("Editor.AssetPreview"));
        auto& PreviewPanel = Preview.Element();
        PreviewPanel.ElementStyle().Apply("editor.asset_preview");
        PreviewPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        PreviewPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        PreviewPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        PreviewPanel.Padding().Set(4.0f);
        PreviewPanel.Gap().Set(4.0f);
        PreviewPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

        auto FolderIcon = Preview.Add(SnAPI::UI::UIImage{});
        auto& FolderIconImage = FolderIcon.Element();
        FolderIconImage.Width().Set(SnAPI::UI::Sizing::Fill());
        FolderIconImage.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        FolderIconImage.Mode().Set(SnAPI::UI::EImageMode::Aspect);
        // Asset-card icons are frequently rebound as list cells are recycled.
        // Keep eager loading to avoid transient missing SVGs during source swaps.
        FolderIconImage.LazyLoad().Set(false);
        FolderIconImage.HAlign().Set(SnAPI::UI::EAlignment::Center);
        FolderIconImage.VAlign().Set(SnAPI::UI::EAlignment::Center);
        FolderIconImage.Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
        FolderIconImage.Properties().SetProperty(
            SnAPI::UI::UIElementBase::VisibilityKey,
            SnAPI::UI::EVisibility::HitTestInvisible);

        auto TypeLabel = Preview.Add(SnAPI::UI::UIText("--"));
        auto& TypeLabelText = TypeLabel.Element();
        TypeLabelText.ElementStyle().Apply("editor.panel_subtitle");
        TypeLabelText.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
        TypeLabelText.HAlign().Set(SnAPI::UI::EAlignment::Center);
        TypeLabelText.VAlign().Set(SnAPI::UI::EAlignment::Center);
        TypeLabelText.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
        TypeLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto NameLabel = Card.Add(SnAPI::UI::UIText("--"));
        auto& NameLabelText = NameLabel.Element();
        NameLabelText.ElementStyle().Apply("editor.panel_title");
        NameLabelText.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
        NameLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto VariantLabel = Card.Add(SnAPI::UI::UIText("--"));
        auto& VariantLabelText = VariantLabel.Element();
        VariantLabelText.ElementStyle().Apply("editor.panel_subtitle");
        VariantLabelText.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
        VariantLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        ContentAssetCardWidgets Widgets{};
        Widgets.Button = CardButton.Handle();
        Widgets.Card = Card.Handle();
        Widgets.Icon = FolderIcon.Handle();
        Widgets.Type = TypeLabel.Handle();
        Widgets.Name = NameLabel.Handle();
        Widgets.Variant = VariantLabel.Handle();
        m_contentAssetCards.push_back(Widgets);
        m_contentAssetCardButtons.push_back(CardButton.Handle());
        m_contentAssetCardIndices.push_back(0);
    }
}

void EditorLayout::UpdateContentAssetCardWidgets()
{
    if (!m_context)
    {
        return;
    }

    const auto SetText = [this](const SnAPI::UI::ElementHandle<SnAPI::UI::UIText>& Handle, const std::string& Value) {
        if (Handle.Id.Value == 0 || !m_context)
        {
            return;
        }

        auto* Text = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(Handle.Id));
        if (Text)
        {
            Text->Text().Set(Value);
        }
    };
    const auto SetTextVisibility = [this](const SnAPI::UI::ElementHandle<SnAPI::UI::UIText>& Handle,
                                          const SnAPI::UI::EVisibility Visibility) {
        if (Handle.Id.Value == 0 || !m_context)
        {
            return;
        }

        if (auto* Text = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(Handle.Id)))
        {
            Text->Visibility().Set(Visibility);
        }
    };
    const auto ApplyCardPanelClass = [this](const SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel>& Handle,
                                            const char* ClassName) {
        if (Handle.Id.Value == 0 || !m_context)
        {
            return;
        }

        if (auto* Panel = dynamic_cast<SnAPI::UI::UIPanel*>(&m_context->GetElement(Handle.Id)))
        {
            Panel->ElementStyle().InitFrom<SnAPI::UI::UIPanel>();
            Panel->ElementStyle().Apply(ClassName);
        }
    };
    const auto ApplyAssetCardIcon = [](SnAPI::UI::UIImage& Icon, const ContentAssetEntry& Asset) {
        if (Asset.IconTextureId != 0)
        {
            Icon.Width().Set(SnAPI::UI::Sizing::Fill());
            Icon.Height().Set(SnAPI::UI::Sizing::Auto());
            Icon.Mode().Set(SnAPI::UI::EImageMode::Aspect);
            if (!Icon.Source().Get().empty())
            {
                Icon.Source().Set(std::string{});
            }
            if (Icon.Texture().Get().Value != Asset.IconTextureId)
            {
                Icon.Texture().Set(SnAPI::UI::TextureId{Asset.IconTextureId});
            }
            if (!(Icon.SvgOptions().Get() == SnAPI::UI::SVGImageOptions{}))
            {
                Icon.SvgOptions().Set(SnAPI::UI::SVGImageOptions{});
            }
            if (Asset.IconWidth > 0u && Asset.IconHeight > 0u)
            {
                Icon.SetIntrinsicSize(
                    static_cast<float>(Asset.IconWidth),
                    static_cast<float>(Asset.IconHeight));
            }
            Icon.Visibility().Set(SnAPI::UI::EVisibility::Visible);
            return;
        }

        Icon.Width().Set(SnAPI::UI::Sizing::Fill());
        Icon.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        Icon.Mode().Set(SnAPI::UI::EImageMode::Aspect);
        std::string IconSource = ResolveUIImageSource(Asset.IconSource);
        if (IconSource.empty())
        {
            IconSource = ResolveUIImageSource(kHierarchyNodeIconPath);
        }
        if (IconSource.empty())
        {
            Icon.Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
            return;
        }

        if (Icon.Source().Get() != IconSource)
        {
            Icon.Source().Set(IconSource);
        }
        SnAPI::UI::SVGImageOptions SvgOptions{};
        SvgOptions.SetRasterSize(kDefaultSvgRasterSize, kDefaultSvgRasterSize, true);
        if (!(Icon.SvgOptions().Get() == SvgOptions))
        {
            Icon.SvgOptions().Set(SvgOptions);
        }
        Icon.Visibility().Set(SnAPI::UI::EVisibility::Visible);
    };

    for (std::size_t CardIndex = 0; CardIndex < m_contentAssetCards.size(); ++CardIndex)
    {
        const ContentAssetCardWidgets& Widgets = m_contentAssetCards[CardIndex];
        if (Widgets.Button.Id.Value == 0)
        {
            continue;
        }

        auto* Button = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(Widgets.Button.Id));
        if (!Button)
        {
            continue;
        }

        SnAPI::UI::UIImage* CardIcon = nullptr;
        if (Widgets.Icon.Id.Value != 0)
        {
            CardIcon = dynamic_cast<SnAPI::UI::UIImage*>(&m_context->GetElement(Widgets.Icon.Id));
        }

        if (CardIndex >= m_contentBrowserEntries.size())
        {
            Button->Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
            if (CardIcon)
            {
                CardIcon->Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
            }
            continue;
        }

        const ContentBrowserEntry& Entry = m_contentBrowserEntries[CardIndex];
        if (Entry.IsFolder)
        {
            m_contentAssetCardIndices[CardIndex] = std::numeric_limits<std::size_t>::max();
            ApplyCardPanelClass(Widgets.Card, "editor.asset_card_folder");
            SetText(Widgets.Type, Entry.DisplayName.empty() ? std::string("Folder") : Entry.DisplayName);
            SetTextVisibility(Widgets.Type, SnAPI::UI::EVisibility::Visible);
            SetTextVisibility(Widgets.Name, SnAPI::UI::EVisibility::Collapsed);
            SetTextVisibility(Widgets.Variant, SnAPI::UI::EVisibility::Collapsed);
            if (CardIcon)
            {
                ConfigureFolderCardIcon(*CardIcon);
            }
            Button->Visibility().Set(SnAPI::UI::EVisibility::Visible);
            continue;
        }

        if (Entry.AssetIndex >= m_contentAssets.size())
        {
            Button->Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
            if (CardIcon)
            {
                CardIcon->Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
            }
            continue;
        }

        const ContentAssetEntry& Asset = m_contentAssets[Entry.AssetIndex];
        m_contentAssetCardIndices[CardIndex] = Entry.AssetIndex;
        ApplyCardPanelClass(Widgets.Card, "editor.asset_card");

        std::string VariantText = Asset.Variant.empty() ? std::string("default") : Asset.Variant;
        if (Asset.IsRuntime)
        {
            VariantText = std::string("runtime | ") + VariantText;
        }
        if (Asset.IsDirty)
        {
            VariantText += " | unsaved";
        }
        SetText(Widgets.Type, Asset.Type);
        SetText(Widgets.Name, Asset.IsDirty ? std::string("* ") + Entry.DisplayName : Entry.DisplayName);
        SetText(Widgets.Variant, VariantText);
        SetTextVisibility(Widgets.Type, SnAPI::UI::EVisibility::Visible);
        SetTextVisibility(Widgets.Name, SnAPI::UI::EVisibility::Visible);
        SetTextVisibility(Widgets.Variant, SnAPI::UI::EVisibility::Visible);
        if (CardIcon)
        {
            ApplyAssetCardIcon(*CardIcon, Asset);
        }
        Button->Visibility().Set(SnAPI::UI::EVisibility::Visible);
    }

    if (m_contentAssetsEmptyHint.Id.Value != 0)
    {
        if (auto* EmptyHint = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(m_contentAssetsEmptyHint.Id)))
        {
            if (m_contentAssets.empty())
            {
                EmptyHint->Text().Set("No assets discovered. Click Rescan to search for source assets.");
            }
            else
            {
                EmptyHint->Text().Set("This folder is empty.");
            }
            EmptyHint->Visibility().Set(
                m_contentBrowserEntries.empty() ? SnAPI::UI::EVisibility::Visible : SnAPI::UI::EVisibility::Collapsed);
        }
    }
}

void EditorLayout::RefreshContentBrowserPath()
{
    if (!m_context || m_contentPathBreadcrumbs.Id.Value == 0)
    {
        return;
    }

    auto* Breadcrumbs = dynamic_cast<SnAPI::UI::UIBreadcrumbs*>(&m_context->GetElement(m_contentPathBreadcrumbs.Id));
    if (!Breadcrumbs)
    {
        return;
    }

    std::vector<std::string> Crumbs{};
    Crumbs.emplace_back("Content");
    Crumbs.emplace_back("Assets");
    const auto Segments = SplitBrowserPath(m_contentCurrentFolder);
    Crumbs.insert(Crumbs.end(), Segments.begin(), Segments.end());
    Breadcrumbs->SetCrumbs(std::move(Crumbs));
}

void EditorLayout::RebuildContentBrowserEntries()
{
    m_contentCurrentFolder = NormalizeBrowserPath(m_contentCurrentFolder);
    m_contentBrowserEntries.clear();
    std::unordered_set<std::string> AddedFolders{};
    AddedFolders.reserve(m_contentAssets.size());

    const bool HasFilter = !m_contentAssetFilterText.empty();
    for (std::size_t AssetIndex = 0; AssetIndex < m_contentAssets.size(); ++AssetIndex)
    {
        const ContentAssetEntry& Asset = m_contentAssets[AssetIndex];
        const std::string AssetPath = NormalizeBrowserPath(Asset.Name);
        const std::string AssetFolder = ParentBrowserPath(AssetPath);
        const std::string AssetLeaf = LeafBrowserName(AssetPath);
        if (!FolderContainsAsset(m_contentCurrentFolder, AssetFolder))
        {
            continue;
        }
        if (AssetFolder == m_contentCurrentFolder)
        {
            const bool VisibleByName = LabelMatchesFilter(AssetLeaf, m_contentAssetFilterText);
            const bool VisibleByType = LabelMatchesFilter(Asset.Type, m_contentAssetFilterText);
            const bool VisibleByVariant = LabelMatchesFilter(Asset.Variant, m_contentAssetFilterText);
            if (HasFilter && !(VisibleByName || VisibleByType || VisibleByVariant))
            {
                continue;
            }

            ContentBrowserEntry Entry{};
            Entry.IsFolder = false;
            Entry.AssetIndex = AssetIndex;
            Entry.FolderPath = AssetFolder;
            Entry.DisplayName = AssetLeaf.empty() ? Asset.Name : AssetLeaf;
            m_contentBrowserEntries.push_back(std::move(Entry));
            continue;
        }

        std::string RemainingPath = AssetFolder;
        if (!m_contentCurrentFolder.empty())
        {
            RemainingPath = AssetFolder.substr(m_contentCurrentFolder.size() + 1u);
        }

        const std::size_t Delimiter = RemainingPath.find('/');
        const std::string ChildFolderName = (Delimiter == std::string::npos)
                                                ? RemainingPath
                                                : RemainingPath.substr(0, Delimiter);
        if (ChildFolderName.empty())
        {
            continue;
        }

        const std::string ChildFolderPath = m_contentCurrentFolder.empty()
                                                ? ChildFolderName
                                                : (m_contentCurrentFolder + "/" + ChildFolderName);
        if (!AddedFolders.insert(ChildFolderPath).second)
        {
            continue;
        }

        if (HasFilter && !LabelMatchesFilter(ChildFolderName, m_contentAssetFilterText))
        {
            continue;
        }

        ContentBrowserEntry Entry{};
        Entry.IsFolder = true;
        Entry.AssetIndex = std::numeric_limits<std::size_t>::max();
        Entry.FolderPath = ChildFolderPath;
        Entry.DisplayName = ChildFolderName;
        m_contentBrowserEntries.push_back(std::move(Entry));
    }

    std::sort(m_contentBrowserEntries.begin(), m_contentBrowserEntries.end(), [](const ContentBrowserEntry& Left, const ContentBrowserEntry& Right) {
        if (Left.IsFolder != Right.IsFolder)
        {
            return Left.IsFolder && !Right.IsFolder;
        }

        const std::string LeftName = ToLower(Left.DisplayName);
        const std::string RightName = ToLower(Right.DisplayName);
        if (LeftName != RightName)
        {
            return LeftName < RightName;
        }

        return Left.FolderPath < Right.FolderPath;
    });
}

void EditorLayout::ApplyContentAssetFilter()
{
    if (!m_context)
    {
        return;
    }

    RebuildContentBrowserEntries();
    RefreshContentBrowserPath();
    EnsureContentAssetCardCapacity();
    UpdateContentAssetCardWidgets();
    RefreshContentAssetCardSelectionStyles();
    RefreshContentAssetDetailsViewModel();
    m_context->MarkLayoutDirty();
}

void EditorLayout::DestroyContentAssetCreateModalOverlay()
{
    if (m_context && m_contentCreateModalOverlay.Id.Value != 0)
    {
        const SnAPI::UI::ElementId OverlayId = m_contentCreateModalOverlay.Id;
        const SnAPI::UI::ElementId CapturedElement = m_context->GetCapture();
        if (IsElementWithinSubtree(*m_context, CapturedElement, OverlayId))
        {
            m_context->ReleaseCapture();
        }

        m_context->DestroyElement(OverlayId);
    }

    m_contentCreateModalOverlay = {};
    m_contentCreateTypeTree = {};
    m_contentCreateSearchInput = {};
    m_contentCreateNameInput = {};
    m_contentCreateOkButton = {};
    m_contentCreateVisibleTypes.clear();
    m_contentCreateSelectedType = {};
}

void EditorLayout::EnsureProjectModalOverlay()
{
    if (!m_context || m_projectModalOverlay.Id.Value != 0)
    {
        return;
    }

    auto Root = m_context->Root();
    auto Overlay = Root.Add(SnAPI::UI::UIModal{});
    auto& OverlayPanel = Overlay.Element();
    OverlayPanel.CloseOnBackdropClick().Set(false);
    OverlayPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    if (m_projectModalShowWelcome)
    {
        OverlayPanel.Movable().Set(false);
        OverlayPanel.Resizable().Set(false);
        OverlayPanel.DragRegionHeight().Set(0.0f);
        OverlayPanel.BackdropColor().Set(SnAPI::UI::Color::RGBA(6, 8, 12, 218));
        OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 22, 30, 252));
        OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(87, 97, 112, 245));
        OverlayPanel.ContentBorderThickness().Set(1.0f);
        OverlayPanel.ContentCornerRadius().Set(10.0f);
        OverlayPanel.ContentPadding().Set(18.0f);
        ConfigureModalScreenRatio(OverlayPanel, kDefaultModalScreenRatio);
        m_projectModalOverlay = Overlay.Handle();
        m_projectNameInput = {};
        m_projectDirectoryInput = {};
        m_projectFilePathInput = {};
        m_projectModalOkButton = {};

        auto Modal = Overlay.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcomeModal"));
        auto& ModalPanel = Modal.Element();
        ModalPanel.ElementStyle().Apply("editor.project_welcome_root");
        ModalPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        ModalPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        ModalPanel.Height().Set(SnAPI::UI::Sizing::Fill());
        ModalPanel.Padding().Set(10.0f);
        ModalPanel.Gap().Set(8.0f);

        auto HeaderRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.HeaderRow"));
        auto& HeaderRowPanel = HeaderRow.Element();
        ConfigureTransparentLayoutPanel(HeaderRowPanel);
        HeaderRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
        HeaderRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        HeaderRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        HeaderRowPanel.Padding().Set(0.0f);
        HeaderRowPanel.Gap().Set(10.0f);
        HeaderRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
        HeaderRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
        HeaderRowPanel.BorderThickness().Set(0.0f);

        auto HeaderLeftSpacer = HeaderRow.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.HeaderLeftSpacer"));
        auto& HeaderLeftSpacerPanel = HeaderLeftSpacer.Element();
        ConfigureLayoutSpacerPanel(HeaderLeftSpacerPanel);
        HeaderLeftSpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

        auto HeaderBrand = HeaderRow.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.HeaderBrand"));
        auto& HeaderBrandPanel = HeaderBrand.Element();
        ConfigureTransparentLayoutPanel(HeaderBrandPanel);
        HeaderBrandPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
        HeaderBrandPanel.Width().Set(SnAPI::UI::Sizing::Auto());
        HeaderBrandPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        HeaderBrandPanel.Padding().Set(0.0f);
        HeaderBrandPanel.Gap().Set(10.0f);
        HeaderBrandPanel.Background().Set(SnAPI::UI::Color::Transparent());
        HeaderBrandPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
        HeaderBrandPanel.BorderThickness().Set(0.0f);
        HeaderBrandPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey,
                                                  SnAPI::UI::EVisibility::HitTestInvisible);

        auto HeaderIcon = HeaderBrand.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kBrandIconPath)));
        auto& HeaderIconImage = HeaderIcon.Element();
        ConfigureSvgIcon(
            HeaderIconImage,
            22.0f,
            SnAPI::UI::Color::RGB(230, 206, 162),
            SnAPI::UI::Margin{2.0f, 0.0f, 2.0f, 0.0f});
        HeaderIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto HeaderTitle = HeaderBrand.Add(SnAPI::UI::UIText("Welcome to SnAPI GameFramework Editor"));
        auto& HeaderTitleText = HeaderTitle.Element();
        HeaderTitleText.ElementStyle().Apply("editor.project_welcome_title");
        HeaderTitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
        HeaderTitleText.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
        HeaderTitleText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto HeaderRightSpacer = HeaderRow.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.HeaderRightSpacer"));
        auto& HeaderRightSpacerPanel = HeaderRightSpacer.Element();
        ConfigureLayoutSpacerPanel(HeaderRightSpacerPanel);
        HeaderRightSpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

        auto Subtitle = Modal.Add(SnAPI::UI::UIText("Create and manage your game projects"));
        auto& SubtitleText = Subtitle.Element();
        SubtitleText.ElementStyle().Apply("editor.project_welcome_subtitle");
        SubtitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
        SubtitleText.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);

        auto CardsRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.CardsRow"));
        auto& CardsRowPanel = CardsRow.Element();
        ConfigureTransparentLayoutPanel(CardsRowPanel);
        CardsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
        CardsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        CardsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        CardsRowPanel.Padding().Set(0.0f);
        CardsRowPanel.Gap().Set(10.0f);
        CardsRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
        CardsRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
        CardsRowPanel.BorderThickness().Set(0.0f);

        auto CardsLeftSpacer = CardsRow.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.CardsLeftSpacer"));
        auto& CardsLeftSpacerPanel = CardsLeftSpacer.Element();
        ConfigureLayoutSpacerPanel(CardsLeftSpacerPanel);
        CardsLeftSpacerPanel.Width().Set(SnAPI::UI::Sizing::Auto());

        auto OpenCard = CardsRow.Add(SnAPI::UI::UIButton{});
        auto& OpenCardElement = OpenCard.Element();
        OpenCardElement.ElementStyle().Apply("editor.project_welcome_option_card");
        OpenCardElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        OpenCardElement.Height().Set(SnAPI::UI::Sizing::Auto());
        OpenCardElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 10.0f, 10.0f, 10.0f});
        OpenCardElement.OnClick([this]() {
            OpenProjectOpenModal();
        });

        auto OpenCardPanel = OpenCard.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.OpenCard"));
        auto& OpenCardPanelElement = OpenCardPanel.Element();
        ConfigureTransparentLayoutPanel(OpenCardPanelElement);
        OpenCardPanelElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        OpenCardPanelElement.Width().Set(SnAPI::UI::Sizing::Fill());
        OpenCardPanelElement.Height().Set(SnAPI::UI::Sizing::Auto());
        OpenCardPanelElement.Padding().Set(0.0f);
        OpenCardPanelElement.Gap().Set(6.0f);
        OpenCardPanelElement.Background().Set(SnAPI::UI::Color::Transparent());
        OpenCardPanelElement.BorderColor().Set(SnAPI::UI::Color::Transparent());
        OpenCardPanelElement.BorderThickness().Set(0.0f);
        OpenCardPanelElement.Properties().SetProperty(
            SnAPI::UI::UIElementBase::VisibilityKey,
            SnAPI::UI::EVisibility::HitTestInvisible);

        auto OpenIcon = OpenCardPanel.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kProjectWelcomeOpenIconPath)));
        auto& OpenIconImage = OpenIcon.Element();
        ConfigureSvgIcon(OpenIconImage, 46.0f, SnAPI::UI::Color::RGB(229, 233, 240));
        OpenIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto OpenTitle = OpenCardPanel.Add(SnAPI::UI::UIText("Open Project"));
        auto& OpenTitleText = OpenTitle.Element();
        OpenTitleText.ElementStyle().Apply("editor.project_welcome_option_title");
        OpenTitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
        OpenTitleText.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
        OpenTitleText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto OpenDescription = OpenCardPanel.Add(SnAPI::UI::UIText("Open an existing project to start editing"));
        auto& OpenDescriptionText = OpenDescription.Element();
        OpenDescriptionText.ElementStyle().Apply("editor.project_welcome_option_subtitle");
        OpenDescriptionText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
        OpenDescriptionText.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
        OpenDescriptionText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto OpenActionShell = OpenCardPanel.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.OpenCardAction"));
        auto& OpenActionShellPanel = OpenActionShell.Element();
        OpenActionShellPanel.ElementStyle().Apply("editor.project_welcome_option_action");
        OpenActionShellPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        OpenActionShellPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        OpenActionShellPanel.Properties().SetProperty(
            SnAPI::UI::UIElementBase::VisibilityKey,
            SnAPI::UI::EVisibility::HitTestInvisible);

        auto OpenActionText = OpenActionShell.Add(SnAPI::UI::UIText("Open Project"));
        auto& OpenActionLabel = OpenActionText.Element();
        OpenActionLabel.ElementStyle().Apply("editor.project_welcome_option_action_text");
        OpenActionLabel.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
        OpenActionLabel.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto CreateCard = CardsRow.Add(SnAPI::UI::UIButton{});
        auto& CreateCardElement = CreateCard.Element();
        CreateCardElement.ElementStyle().Apply("editor.project_welcome_option_card");
        CreateCardElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        CreateCardElement.Height().Set(SnAPI::UI::Sizing::Auto());
        CreateCardElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 10.0f, 10.0f, 10.0f});
        CreateCardElement.OnClick([this]() {
            OpenProjectCreateModal();
        });

        auto CreateCardPanel = CreateCard.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.CreateCard"));
        auto& CreateCardPanelElement = CreateCardPanel.Element();
        ConfigureTransparentLayoutPanel(CreateCardPanelElement);
        CreateCardPanelElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        CreateCardPanelElement.Width().Set(SnAPI::UI::Sizing::Fill());
        CreateCardPanelElement.Height().Set(SnAPI::UI::Sizing::Auto());
        CreateCardPanelElement.Padding().Set(0.0f);
        CreateCardPanelElement.Gap().Set(6.0f);
        CreateCardPanelElement.Background().Set(SnAPI::UI::Color::Transparent());
        CreateCardPanelElement.BorderColor().Set(SnAPI::UI::Color::Transparent());
        CreateCardPanelElement.BorderThickness().Set(0.0f);
        CreateCardPanelElement.Properties().SetProperty(
            SnAPI::UI::UIElementBase::VisibilityKey,
            SnAPI::UI::EVisibility::HitTestInvisible);

        auto CreateIcon = CreateCardPanel.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kProjectWelcomeCreateIconPath)));
        auto& CreateIconImage = CreateIcon.Element();
        ConfigureSvgIcon(CreateIconImage, 46.0f, SnAPI::UI::Color::RGB(229, 233, 240));
        CreateIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto CreateTitle = CreateCardPanel.Add(SnAPI::UI::UIText("Create New Project"));
        auto& CreateTitleText = CreateTitle.Element();
        CreateTitleText.ElementStyle().Apply("editor.project_welcome_option_title");
        CreateTitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
        CreateTitleText.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
        CreateTitleText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto CreateDescription = CreateCardPanel.Add(SnAPI::UI::UIText("Start a new project from scratch"));
        auto& CreateDescriptionText = CreateDescription.Element();
        CreateDescriptionText.ElementStyle().Apply("editor.project_welcome_option_subtitle");
        CreateDescriptionText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
        CreateDescriptionText.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
        CreateDescriptionText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto CreateActionShell = CreateCardPanel.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.CreateCardAction"));
        auto& CreateActionShellPanel = CreateActionShell.Element();
        CreateActionShellPanel.ElementStyle().Apply("editor.project_welcome_option_action_primary");
        CreateActionShellPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        CreateActionShellPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        CreateActionShellPanel.Properties().SetProperty(
            SnAPI::UI::UIElementBase::VisibilityKey,
            SnAPI::UI::EVisibility::HitTestInvisible);

        auto CreateActionText = CreateActionShell.Add(SnAPI::UI::UIText("New Project"));
        auto& CreateActionLabel = CreateActionText.Element();
        CreateActionLabel.ElementStyle().Apply("editor.project_welcome_option_action_text");
        CreateActionLabel.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
        CreateActionLabel.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto CardsRightSpacer = CardsRow.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.CardsRightSpacer"));
        auto& CardsRightSpacerPanel = CardsRightSpacer.Element();
        ConfigureLayoutSpacerPanel(CardsRightSpacerPanel);
        CardsRightSpacerPanel.Width().Set(SnAPI::UI::Sizing::Auto());

        auto RecentSection = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.RecentSection"));
        auto& RecentSectionPanel = RecentSection.Element();
        RecentSectionPanel.ElementStyle().Apply("editor.project_welcome_recent_panel");
        RecentSectionPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        RecentSectionPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        RecentSectionPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        RecentSectionPanel.Padding().Set(6.0f);
        RecentSectionPanel.Gap().Set(2.0f);

        auto RecentTitle = RecentSection.Add(SnAPI::UI::UIText("Recent Projects"));
        auto& RecentTitleText = RecentTitle.Element();
        RecentTitleText.ElementStyle().Apply("editor.project_welcome_recent_title");
        RecentTitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);

        auto RecentList = RecentSection.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.RecentList"));
        auto& RecentListPanel = RecentList.Element();
        ConfigureTransparentLayoutPanel(RecentListPanel);
        RecentListPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        RecentListPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        RecentListPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        RecentListPanel.Padding().Set(0.0f);
        RecentListPanel.Gap().Set(0.0f);
        RecentListPanel.Background().Set(SnAPI::UI::Color::Transparent());
        RecentListPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
        RecentListPanel.BorderThickness().Set(0.0f);

        if (m_recentProjects.empty())
        {
            auto EmptyHint = RecentList.Add(SnAPI::UI::UIText("No recent projects yet."));
            auto& EmptyHintText = EmptyHint.Element();
            EmptyHintText.ElementStyle().Apply("editor.project_welcome_recent_empty");
            EmptyHintText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
        }
        else
        {
            const std::size_t VisibleRecentCount = std::min<std::size_t>(m_recentProjects.size(), 2u);
            for (std::size_t Index = 0; Index < VisibleRecentCount; ++Index)
            {
                const RecentProjectEntry Entry = m_recentProjects[Index];

                auto RecentButton = RecentList.Add(SnAPI::UI::UIButton{});
                auto& RecentButtonElement = RecentButton.Element();
                RecentButtonElement.ElementStyle().Apply("editor.project_welcome_recent_button");
                RecentButtonElement.Width().Set(SnAPI::UI::Sizing::Fill());
                RecentButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
                RecentButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 6.0f, 8.0f, 6.0f});
                RecentButtonElement.OnClick([this, Entry]() {
                    m_projectModalAction = EProjectAction::OpenExisting;
                    m_projectFilePathText = Entry.ProjectFilePath;
                    ConfirmProjectModal();
                });

                auto RecentRow = RecentButton.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.RecentRow"));
                auto& RecentRowPanel = RecentRow.Element();
                ConfigureTransparentLayoutPanel(RecentRowPanel);
                RecentRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
                RecentRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
                RecentRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
                RecentRowPanel.Padding().Set(0.0f);
                RecentRowPanel.Gap().Set(8.0f);
                RecentRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
                RecentRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
                RecentRowPanel.BorderThickness().Set(0.0f);
                RecentRowPanel.Properties().SetProperty(
                    SnAPI::UI::UIElementBase::VisibilityKey,
                    SnAPI::UI::EVisibility::HitTestInvisible);

                auto RecentIcon = RecentRow.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kProjectWelcomeRecentIconPath)));
                auto& RecentIconImage = RecentIcon.Element();
                ConfigureSvgIcon(RecentIconImage, 13.0f, SnAPI::UI::Color::RGB(214, 198, 164));
                RecentIconImage.ElementMargin().Set(SnAPI::UI::Margin{1.0f, 0.0f, 2.0f, 0.0f});
                RecentIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

                auto RecentText = RecentRow.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.RecentText"));
                auto& RecentTextPanel = RecentText.Element();
                ConfigureTransparentLayoutPanel(RecentTextPanel);
                RecentTextPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
                RecentTextPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
                RecentTextPanel.Height().Set(SnAPI::UI::Sizing::Auto());
                RecentTextPanel.Padding().Set(0.0f);
                RecentTextPanel.Gap().Set(1.0f);
                RecentTextPanel.Background().Set(SnAPI::UI::Color::Transparent());
                RecentTextPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
                RecentTextPanel.BorderThickness().Set(0.0f);
                RecentTextPanel.Properties().SetProperty(
                    SnAPI::UI::UIElementBase::VisibilityKey,
                    SnAPI::UI::EVisibility::HitTestInvisible);

                auto RecentName = RecentText.Add(SnAPI::UI::UIText(Entry.Name));
                auto& RecentNameText = RecentName.Element();
                RecentNameText.ElementStyle().Apply("editor.project_welcome_recent_name");
                RecentNameText.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
                RecentNameText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

                auto RecentPath = RecentText.Add(SnAPI::UI::UIText(Entry.ProjectFilePath));
                auto& RecentPathText = RecentPath.Element();
                RecentPathText.ElementStyle().Apply("editor.project_welcome_recent_path");
                RecentPathText.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
                RecentPathText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);
            }
        }

        auto FooterRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.FooterRow"));
        auto& FooterRowPanel = FooterRow.Element();
        ConfigureTransparentLayoutPanel(FooterRowPanel);
        FooterRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
        FooterRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        FooterRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        FooterRowPanel.Padding().Set(0.0f);
        FooterRowPanel.Gap().Set(8.0f);
        FooterRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
        FooterRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
        FooterRowPanel.BorderThickness().Set(0.0f);

        auto DocsButton = FooterRow.Add(SnAPI::UI::UIButton{});
        auto& DocsButtonElement = DocsButton.Element();
        DocsButtonElement.ElementStyle().Apply("editor.project_welcome_footer_button");
        DocsButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
        DocsButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
        DocsButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 3.0f, 8.0f, 3.0f});
        DocsButtonElement.OnClick([this]() {
            OpenProjectOpenModal();
        });

        auto DocsContent = DocsButton.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.DocsButtonContent"));
        auto& DocsContentPanel = DocsContent.Element();
        ConfigureTransparentLayoutPanel(DocsContentPanel);
        DocsContentPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
        DocsContentPanel.Width().Set(SnAPI::UI::Sizing::Auto());
        DocsContentPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        DocsContentPanel.Padding().Set(0.0f);
        DocsContentPanel.Gap().Set(4.0f);
        DocsContentPanel.Background().Set(SnAPI::UI::Color::Transparent());
        DocsContentPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
        DocsContentPanel.BorderThickness().Set(0.0f);
        DocsContentPanel.Properties().SetProperty(
            SnAPI::UI::UIElementBase::VisibilityKey,
            SnAPI::UI::EVisibility::HitTestInvisible);

        auto DocsIcon = DocsContent.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kProjectWelcomeFooterIconPath)));
        auto& DocsIconImage = DocsIcon.Element();
        ConfigureSvgIcon(DocsIconImage, 12.0f, SnAPI::UI::Color::RGB(170, 178, 194));
        DocsIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto DocsText = DocsContent.Add(SnAPI::UI::UIText("Documentation"));
        auto& DocsTextElement = DocsText.Element();
        DocsTextElement.ElementStyle().Apply("editor.project_welcome_footer_button_text");
        DocsTextElement.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
        DocsTextElement.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        auto FooterSpacer = FooterRow.Add(SnAPI::UI::UIPanel("Editor.ProjectWelcome.FooterSpacer"));
        auto& FooterSpacerPanel = FooterSpacer.Element();
        ConfigureLayoutSpacerPanel(FooterSpacerPanel);
        FooterSpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

        auto ExitButton = FooterRow.Add(SnAPI::UI::UIButton{});
        auto& ExitButtonElement = ExitButton.Element();
        ExitButtonElement.ElementStyle().Apply("editor.project_welcome_footer_button");
        ExitButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
        ExitButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
        ExitButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 3.0f, 8.0f, 3.0f});
        ExitButtonElement.SetDisabled(m_projectModalRequired);
        ExitButtonElement.OnClick([this]() {
            CloseProjectModal(true);
        });

        auto ExitText = ExitButton.Add(SnAPI::UI::UIText("Exit"));
        auto& ExitTextElement = ExitText.Element();
        ExitTextElement.ElementStyle().Apply("editor.project_welcome_footer_button_text");
        ExitTextElement.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
        ExitTextElement.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

        return;
    }

    OverlayPanel.Movable().Set(!m_projectModalRequired);
    OverlayPanel.Resizable().Set(false);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.BackdropColor().Set(SnAPI::UI::Color::RGBA(6, 8, 12, 218));
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 22, 30, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(87, 97, 112, 245));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(10.0f);
    OverlayPanel.ContentPadding().Set(12.0f);
    ConfigureModalScreenRatio(OverlayPanel, kDefaultModalScreenRatio);
    m_projectModalOverlay = Overlay.Handle();

    auto Modal = Overlay.Add(SnAPI::UI::UIPanel("Editor.ProjectModal"));
    auto& ModalPanel = Modal.Element();
    ModalPanel.ElementStyle().Apply("editor.project_modal_root");
    ModalPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ModalPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Padding().Set(10.0f);
    ModalPanel.Gap().Set(10.0f);

    const bool IsCreate = m_projectModalAction == EProjectAction::CreateNew;

    auto HeaderRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.Header"));
    auto& HeaderRowPanel = HeaderRow.Element();
    ConfigureTransparentLayoutPanel(HeaderRowPanel);
    HeaderRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HeaderRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderRowPanel.Gap().Set(8.0f);
    HeaderRowPanel.Padding().Set(0.0f);

    auto HeaderIcon = HeaderRow.Add(SnAPI::UI::UIImage(
        ResolveUIImageSource(IsCreate ? kProjectWelcomeCreateIconPath : kProjectWelcomeOpenIconPath)));
    auto& HeaderIconImage = HeaderIcon.Element();
    ConfigureSvgIcon(HeaderIconImage, 18.0f, SnAPI::UI::Color::RGB(230, 206, 162));
    HeaderIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto Title = HeaderRow.Add(SnAPI::UI::UIText(IsCreate ? "Create New Project" : "Open Existing Project"));
    auto& TitleText = Title.Element();
    TitleText.ElementStyle().Apply("editor.project_welcome_title");
    TitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    TitleText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    const std::string SubtitleTextValue = m_projectModalRequired
                                              ? std::string("Create or open a project before continuing.")
                                              : std::string(IsCreate
                                                                ? "Create a project file, configure its asset root, and initialize starter content."
                                                                : "Load a project file and switch the editor asset root to that project.");
    auto Subtitle = Modal.Add(SnAPI::UI::UIText(SubtitleTextValue));
    auto& SubtitleText = Subtitle.Element();
    SubtitleText.ElementStyle().Apply("editor.project_welcome_subtitle");
    SubtitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto ModeRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.ModeRow"));
    auto& ModeRowPanel = ModeRow.Element();
    ConfigureTransparentLayoutPanel(ModeRowPanel);
    ModeRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ModeRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ModeRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ModeRowPanel.Gap().Set(8.0f);
    ModeRowPanel.Padding().Set(0.0f);

    auto CreateModeButton = ModeRow.Add(SnAPI::UI::UIButton{});
    auto& CreateModeButtonElement = CreateModeButton.Element();
    CreateModeButtonElement.ElementStyle().Apply(IsCreate ? "editor.project_modal_mode_button_active"
                                                          : "editor.project_modal_mode_button");
    CreateModeButtonElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    CreateModeButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    CreateModeButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 6.0f, 8.0f, 6.0f});
    CreateModeButtonElement.OnClick([this, IsCreate]() {
        if (!IsCreate)
        {
            OpenProjectCreateModal();
        }
    });
    auto CreateModeLabel = CreateModeButton.Add(SnAPI::UI::UIText("Create New"));
    auto& CreateModeLabelText = CreateModeLabel.Element();
    CreateModeLabelText.ElementStyle().Apply("editor.project_modal_mode_button_text");
    CreateModeLabelText.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
    CreateModeLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto OpenModeButton = ModeRow.Add(SnAPI::UI::UIButton{});
    auto& OpenModeButtonElement = OpenModeButton.Element();
    OpenModeButtonElement.ElementStyle().Apply(IsCreate ? "editor.project_modal_mode_button"
                                                        : "editor.project_modal_mode_button_active");
    OpenModeButtonElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    OpenModeButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    OpenModeButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 6.0f, 8.0f, 6.0f});
    OpenModeButtonElement.OnClick([this, IsCreate]() {
        if (IsCreate)
        {
            OpenProjectOpenModal();
        }
    });
    auto OpenModeLabel = OpenModeButton.Add(SnAPI::UI::UIText("Open Existing"));
    auto& OpenModeLabelText = OpenModeLabel.Element();
    OpenModeLabelText.ElementStyle().Apply("editor.project_modal_mode_button_text");
    OpenModeLabelText.TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
    OpenModeLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto FormPanel = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.FormPanel"));
    auto& FormPanelElement = FormPanel.Element();
    FormPanelElement.ElementStyle().Apply("editor.project_modal_form_panel");
    FormPanelElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    FormPanelElement.Width().Set(SnAPI::UI::Sizing::Fill());
    FormPanelElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    FormPanelElement.Padding().Set(10.0f);
    FormPanelElement.Gap().Set(8.0f);

    if (IsCreate)
    {
        auto NameLabel = FormPanel.Add(SnAPI::UI::UIText("Project Name"));
        NameLabel.Element().ElementStyle().Apply("editor.menu_item");

        auto NameInput = FormPanel.Add(SnAPI::UI::UITextInput{});
        auto& NameInputElement = NameInput.Element();
        NameInputElement.ElementStyle().Apply("editor.text_input");
        NameInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        NameInputElement.Resizable().Set(false);
        NameInputElement.Multiline().Set(false);
        NameInputElement.AcceptTab().Set(false);
        NameInputElement.Placeholder().Set("NewProject");
        NameInputElement.Text().Set(m_projectNameText);
        NameInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_projectNameText = Value;
            RefreshProjectModalOkButtonState();
        }));
        m_projectNameInput = NameInput.Handle();
        m_projectFilePathInput = {};

        auto DirectoryLabel = FormPanel.Add(SnAPI::UI::UIText("Project Directory"));
        DirectoryLabel.Element().ElementStyle().Apply("editor.menu_item");

        auto DirectoryInput = FormPanel.Add(SnAPI::UI::UIFilesystemPicker{});
        auto& DirectoryInputElement = DirectoryInput.Element();
        DirectoryInputElement.ElementStyle().Apply("editor.filesystem_picker");
        DirectoryInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        DirectoryInputElement.Height().Set(SnAPI::UI::Sizing::Auto());
        DirectoryInputElement.ReadOnly().Set(false);
        DirectoryInputElement.AllowMultiSelect().Set(false);
        DirectoryInputElement.PickDirectories().Set(true);
        DirectoryInputElement.ShowDirectories().Set(true);
        DirectoryInputElement.ShowFiles().Set(false);
        DirectoryInputElement.RestrictToRoot().Set(false);
        DirectoryInputElement.Placeholder().Set(std::string("Path to parent folder"));
        DirectoryInputElement.Value().Set(m_projectDirectoryText);
        DirectoryInputElement.CurrentPath().Set(m_projectDirectoryText);
        DirectoryInputElement.OnSelectionChanged(
            SnAPI::UI::TDelegate<void(const std::vector<std::string>&)>::Bind(
                [this](const std::vector<std::string>& Values) {
                    if (!Values.empty())
                    {
                        m_projectDirectoryText = Values.front();
                        RefreshProjectModalOkButtonState();
                    }
                }));
        m_projectDirectoryInput = DirectoryInput.Handle();
    }
    else
    {
        auto FileLabel = FormPanel.Add(SnAPI::UI::UIText("Project File (.json)"));
        FileLabel.Element().ElementStyle().Apply("editor.menu_item");

        auto FileInput = FormPanel.Add(SnAPI::UI::UIFilesystemPicker{});
        auto& FileInputElement = FileInput.Element();
        FileInputElement.ElementStyle().Apply("editor.filesystem_picker");
        FileInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        FileInputElement.Height().Set(SnAPI::UI::Sizing::Auto());
        FileInputElement.ReadOnly().Set(false);
        FileInputElement.AllowMultiSelect().Set(false);
        FileInputElement.PickDirectories().Set(false);
        FileInputElement.ShowDirectories().Set(true);
        FileInputElement.ShowFiles().Set(true);
        FileInputElement.RestrictToRoot().Set(false);
        FileInputElement.Placeholder().Set(std::string("Path to project.json"));
        FileInputElement.Value().Set(m_projectFilePathText);
        FileInputElement.CurrentPath().Set(std::filesystem::path(m_projectFilePathText).parent_path().string());
        FileInputElement.SetAllowedExtensions({".json"});
        FileInputElement.OnSelectionChanged(
            SnAPI::UI::TDelegate<void(const std::vector<std::string>&)>::Bind(
                [this](const std::vector<std::string>& Values) {
                    if (!Values.empty())
                    {
                        m_projectFilePathText = Values.front();
                        RefreshProjectModalOkButtonState();
                    }
                }));
        m_projectFilePathInput = FileInput.Handle();
        m_projectNameInput = {};
        m_projectDirectoryInput = {};
    }

    auto ButtonsRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.Buttons"));
    auto& ButtonsRowPanel = ButtonsRow.Element();
    ConfigureTransparentLayoutPanel(ButtonsRowPanel);
    ButtonsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ButtonsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ButtonsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ButtonsRowPanel.Gap().Set(8.0f);

    auto Spacer = ButtonsRow.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.ButtonSpacer"));
    auto& SpacerPanel = Spacer.Element();
    ConfigureLayoutSpacerPanel(SpacerPanel);
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto CancelButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& CancelButtonElement = CancelButton.Element();
    CancelButtonElement.ElementStyle().Apply("editor.project_modal_action_button");
    CancelButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    CancelButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    CancelButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    CancelButtonElement.SetDisabled(m_projectModalRequired);
    CancelButtonElement.OnClick([this]() {
        CloseProjectModal();
    });
    auto CancelLabel = CancelButton.Add(SnAPI::UI::UIText("Cancel"));
    auto& CancelLabelText = CancelLabel.Element();
    CancelLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    CancelLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto OkButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& OkButtonElement = OkButton.Element();
    OkButtonElement.ElementStyle().Apply("editor.project_modal_action_button_primary");
    OkButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    OkButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    OkButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    OkButtonElement.OnClick([this]() {
        ConfirmProjectModal();
    });
    auto OkLabel = OkButton.Add(SnAPI::UI::UIText(IsCreate ? "Create Project" : "Open Project"));
    auto& OkLabelText = OkLabel.Element();
    OkLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    OkLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);
    m_projectModalOkButton = OkButton.Handle();

    RefreshProjectModalOkButtonState();
}

void EditorLayout::DestroyProjectModalOverlay()
{
    if (m_context && m_projectModalOverlay.Id.Value != 0)
    {
        const SnAPI::UI::ElementId OverlayId = m_projectModalOverlay.Id;
        const SnAPI::UI::ElementId CapturedElement = m_context->GetCapture();
        if (IsElementWithinSubtree(*m_context, CapturedElement, OverlayId))
        {
            m_context->ReleaseCapture();
        }
        m_context->DestroyElement(OverlayId);
    }

    m_projectModalOverlay = {};
    m_projectNameInput = {};
    m_projectDirectoryInput = {};
    m_projectFilePathInput = {};
    m_projectModalOkButton = {};
}

void EditorLayout::EnsureProjectSettingsModalOverlay()
{
    if (!m_context || m_projectSettingsModalOverlay.Id.Value != 0 || !m_projectSettingsModalOpen)
    {
        return;
    }

    auto Root = m_context->Root();
    auto Overlay = Root.Add(SnAPI::UI::UIModal{});
    auto& OverlayPanel = Overlay.Element();
    OverlayPanel.Movable().Set(true);
    OverlayPanel.Resizable().Set(false);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.BackdropColor().Set(SnAPI::UI::Color::RGBA(6, 8, 12, 214));
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 22, 30, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(87, 97, 112, 245));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(10.0f);
    OverlayPanel.ContentPadding().Set(12.0f);
    ConfigureModalScreenRatio(OverlayPanel, kDefaultModalScreenRatio);
    m_projectSettingsModalOverlay = Overlay.Handle();

    auto Modal = Overlay.Add(SnAPI::UI::UIPanel("Editor.ProjectSettingsModal"));
    auto& ModalPanel = Modal.Element();
    ModalPanel.ElementStyle().Apply("editor.project_modal_root");
    ModalPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ModalPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Padding().Set(10.0f);
    ModalPanel.Gap().Set(10.0f);

    auto HeaderRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectSettingsModal.Header"));
    auto& HeaderRowPanel = HeaderRow.Element();
    ConfigureTransparentLayoutPanel(HeaderRowPanel);
    HeaderRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HeaderRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderRowPanel.Gap().Set(8.0f);
    HeaderRowPanel.Padding().Set(0.0f);

    auto HeaderIcon = HeaderRow.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kProjectSettingsIconPath)));
    auto& HeaderIconImage = HeaderIcon.Element();
    ConfigureSvgIcon(HeaderIconImage, 18.0f, SnAPI::UI::Color::RGB(230, 206, 162));
    HeaderIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto Title = HeaderRow.Add(SnAPI::UI::UIText("Project Settings"));
    auto& TitleText = Title.Element();
    TitleText.ElementStyle().Apply("editor.project_welcome_title");
    TitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    TitleText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    std::string SubtitleValue = "Configure project defaults and save to project.snproj.json.";
    if (!m_projectState.ProjectFilePath.empty())
    {
        SubtitleValue += " File: " + m_projectState.ProjectFilePath;
    }
    auto Subtitle = Modal.Add(SnAPI::UI::UIText(SubtitleValue));
    auto& SubtitleText = Subtitle.Element();
    SubtitleText.ElementStyle().Apply("editor.project_welcome_subtitle");
    SubtitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto FormPanel = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectSettingsModal.FormPanel"));
    auto& FormPanelElement = FormPanel.Element();
    FormPanelElement.ElementStyle().Apply("editor.project_modal_form_panel");
    FormPanelElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    FormPanelElement.Width().Set(SnAPI::UI::Sizing::Fill());
    FormPanelElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    FormPanelElement.Padding().Set(10.0f);
    FormPanelElement.Gap().Set(8.0f);

    auto NameLabel = FormPanel.Add(SnAPI::UI::UIText("Project Name"));
    NameLabel.Element().ElementStyle().Apply("editor.menu_item");

    auto NameInput = FormPanel.Add(SnAPI::UI::UITextInput{});
    auto& NameInputElement = NameInput.Element();
    NameInputElement.ElementStyle().Apply("editor.text_input");
    NameInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    NameInputElement.Resizable().Set(false);
    NameInputElement.Multiline().Set(false);
    NameInputElement.AcceptTab().Set(false);
    NameInputElement.Placeholder().Set("Project");
    NameInputElement.Text().Set(m_projectSettingsNameText);
    NameInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_projectSettingsNameText = Value;
        RefreshProjectSettingsModalSaveButtonState();
    }));
    m_projectSettingsNameInput = NameInput.Handle();

    auto StartupLabel = FormPanel.Add(SnAPI::UI::UIText("Startup Level Asset"));
    StartupLabel.Element().ElementStyle().Apply("editor.menu_item");

    auto StartupInput = FormPanel.Add(SnAPI::UI::UIFilesystemPicker{});
    auto& StartupInputElement = StartupInput.Element();
    StartupInputElement.ElementStyle().Apply("editor.filesystem_picker");
    StartupInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    StartupInputElement.Height().Set(SnAPI::UI::Sizing::Auto());
    StartupInputElement.ReadOnly().Set(false);
    StartupInputElement.AllowMultiSelect().Set(false);
    StartupInputElement.PickDirectories().Set(false);
    StartupInputElement.ShowDirectories().Set(true);
    StartupInputElement.ShowFiles().Set(true);
    StartupInputElement.RestrictToRoot().Set(false);
    StartupInputElement.Placeholder().Set(std::string("Path to startup level asset"));
    StartupInputElement.SetAllowedExtensions({".level"});

    std::string StartupPickerValue = m_projectSettingsStartupAssetText;
    if (!StartupPickerValue.empty() && StartupPickerValue.find("://") == std::string::npos)
    {
        std::filesystem::path StartupPath = std::filesystem::path(StartupPickerValue);
        if (!StartupPath.is_absolute() && !m_projectState.AssetRootDirectory.empty())
        {
            StartupPath = std::filesystem::path(m_projectState.AssetRootDirectory) / StartupPath;
        }
        StartupPickerValue = StartupPath.lexically_normal().string();
    }
    StartupInputElement.Value().Set(StartupPickerValue);
    if (!m_projectState.AssetRootDirectory.empty())
    {
        StartupInputElement.CurrentPath().Set(m_projectState.AssetRootDirectory);
    }
    else if (!StartupPickerValue.empty())
    {
        StartupInputElement.CurrentPath().Set(std::filesystem::path(StartupPickerValue).parent_path().string());
    }
    StartupInputElement.OnSelectionChanged(
        SnAPI::UI::TDelegate<void(const std::vector<std::string>&)>::Bind([this](const std::vector<std::string>& Values) {
            if (!Values.empty())
            {
                m_projectSettingsStartupAssetText = Values.front();
                RefreshProjectSettingsModalSaveButtonState();
            }
        }));
    m_projectSettingsStartupAssetInput = StartupInput.Handle();

    auto DefaultRenderSettingsLabel = FormPanel.Add(SnAPI::UI::UIText("Default Render Settings"));
    DefaultRenderSettingsLabel.Element().ElementStyle().Apply("editor.menu_item");

    auto DefaultRenderSettingsCombo = FormPanel.Add(SnAPI::UI::UIComboBox{});
    auto& DefaultRenderSettingsComboElement = DefaultRenderSettingsCombo.Element();
    DefaultRenderSettingsComboElement.Width().Set(SnAPI::UI::Sizing::Fill());
    DefaultRenderSettingsComboElement.Height().Set(SnAPI::UI::Sizing::Auto());
    DefaultRenderSettingsComboElement.Placeholder().Set(std::string("Select WorldRenderSettings asset"));
    DefaultRenderSettingsComboElement.MaxDropdownHeight().Set(230.0f);

    m_projectSettingsRenderSettingsOptions.clear();
    m_projectSettingsRenderSettingsOptions.emplace_back("<None>", std::string{});
#if defined(SNAPI_GF_ENABLE_RENDERER)
    const auto Entries = TAssetRef<WorldRenderSettings>::EnumerateCompatibleAssets();
    for (const auto& Entry : Entries)
    {
        m_projectSettingsRenderSettingsOptions.emplace_back(Entry.Label, Entry.AssetId);
    }
#endif

    std::vector<std::string> OptionLabels{};
    OptionLabels.reserve(m_projectSettingsRenderSettingsOptions.size());
    int32_t SelectedIndex = 0;
    for (std::size_t Index = 0; Index < m_projectSettingsRenderSettingsOptions.size(); ++Index)
    {
        OptionLabels.push_back(m_projectSettingsRenderSettingsOptions[Index].first);
        if (!m_projectSettingsDefaultRenderSettingsAssetId.empty() &&
            m_projectSettingsRenderSettingsOptions[Index].second == m_projectSettingsDefaultRenderSettingsAssetId)
        {
            SelectedIndex = static_cast<int32_t>(Index);
        }
    }
    if (!m_projectSettingsDefaultRenderSettingsAssetId.empty() && SelectedIndex == 0)
    {
        std::string MissingLabel = "<Missing> [" + m_projectSettingsDefaultRenderSettingsAssetId + "]";
        m_projectSettingsRenderSettingsOptions.emplace_back(MissingLabel, m_projectSettingsDefaultRenderSettingsAssetId);
        OptionLabels.push_back(std::move(MissingLabel));
        SelectedIndex = static_cast<int32_t>(m_projectSettingsRenderSettingsOptions.size() - 1);
    }

    DefaultRenderSettingsComboElement.SetItems(std::move(OptionLabels));
    (void)DefaultRenderSettingsComboElement.SetSelectedIndex(SelectedIndex, false);
    DefaultRenderSettingsComboElement.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        if (Index >= 0 && static_cast<std::size_t>(Index) < m_projectSettingsRenderSettingsOptions.size())
        {
            m_projectSettingsDefaultRenderSettingsAssetId = m_projectSettingsRenderSettingsOptions[static_cast<std::size_t>(Index)].second;
        }
        else
        {
            m_projectSettingsDefaultRenderSettingsAssetId.clear();
        }
    });
    m_projectSettingsDefaultRenderSettingsCombo = DefaultRenderSettingsCombo.Handle();

    auto ButtonsRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectSettingsModal.Buttons"));
    auto& ButtonsRowPanel = ButtonsRow.Element();
    ConfigureTransparentLayoutPanel(ButtonsRowPanel);
    ButtonsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ButtonsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ButtonsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ButtonsRowPanel.Gap().Set(8.0f);

    auto Spacer = ButtonsRow.Add(SnAPI::UI::UIPanel("Editor.ProjectSettingsModal.ButtonSpacer"));
    auto& SpacerPanel = Spacer.Element();
    ConfigureLayoutSpacerPanel(SpacerPanel);
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto CancelButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& CancelButtonElement = CancelButton.Element();
    CancelButtonElement.ElementStyle().Apply("editor.project_modal_action_button");
    CancelButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    CancelButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    CancelButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    CancelButtonElement.OnClick([this]() {
        CloseProjectSettingsModal();
    });
    auto CancelLabel = CancelButton.Add(SnAPI::UI::UIText("Cancel"));
    auto& CancelLabelText = CancelLabel.Element();
    CancelLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    CancelLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto SaveButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& SaveButtonElement = SaveButton.Element();
    SaveButtonElement.ElementStyle().Apply("editor.project_modal_action_button_primary");
    SaveButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    SaveButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    SaveButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    SaveButtonElement.OnClick([this]() {
        ConfirmProjectSettingsModal();
    });
    auto SaveLabel = SaveButton.Add(SnAPI::UI::UIText("Save Settings"));
    auto& SaveLabelText = SaveLabel.Element();
    SaveLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    SaveLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);
    m_projectSettingsSaveButton = SaveButton.Handle();

    RefreshProjectSettingsModalSaveButtonState();
}

void EditorLayout::DestroyProjectSettingsModalOverlay()
{
    if (m_context && m_projectSettingsModalOverlay.Id.Value != 0)
    {
        const SnAPI::UI::ElementId OverlayId = m_projectSettingsModalOverlay.Id;
        const SnAPI::UI::ElementId CapturedElement = m_context->GetCapture();
        if (IsElementWithinSubtree(*m_context, CapturedElement, OverlayId))
        {
            m_context->ReleaseCapture();
        }
        m_context->DestroyElement(OverlayId);
    }

    m_projectSettingsModalOverlay = {};
    m_projectSettingsNameInput = {};
    m_projectSettingsStartupAssetInput = {};
    m_projectSettingsDefaultRenderSettingsCombo = {};
    m_projectSettingsSaveButton = {};
}

void EditorLayout::OpenContentAssetCreateModal()
{
    if (!m_context)
    {
        return;
    }

    CloseContextMenu();
    m_contentCreateModalOpen = true;
    m_contentCreateSelectedType = {};
    ViewModelProperty<std::string>(kVmContentCreateTypeFilterKey).Set(std::string{});
    ViewModelProperty<std::string>(kVmContentCreateAssetNameKey).Set(std::string("NewAsset"));
    RefreshContentAssetCreateModalVisibility();
    RebuildContentAssetCreateTypeTree();
    RefreshContentAssetCreateOkButtonState();
    m_context->MarkLayoutDirty();
}

void EditorLayout::CloseContentAssetCreateModal()
{
    if (!m_contentCreateModalOpen && m_contentCreateModalOverlay.Id.Value == 0)
    {
        return;
    }

    m_contentCreateModalOpen = false;
    RefreshContentAssetCreateModalVisibility();
    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::ConfirmContentAssetCreate()
{
    if (!m_contentCreateModalOpen || m_contentCreateSelectedType == TypeId{})
    {
        return;
    }

    std::string RequestedName = LeafBrowserName(NormalizeBrowserPath(m_contentCreateNameText));
    while (!RequestedName.empty() && std::isspace(static_cast<unsigned char>(RequestedName.front())))
    {
        RequestedName.erase(RequestedName.begin());
    }
    while (!RequestedName.empty() && std::isspace(static_cast<unsigned char>(RequestedName.back())))
    {
        RequestedName.pop_back();
    }
    if (RequestedName.empty())
    {
        RefreshContentAssetCreateOkButtonState();
        return;
    }

    if (m_onContentAssetCreateRequested)
    {
        ContentAssetCreateRequest Request{};
        Request.Type = m_contentCreateSelectedType;
        Request.Name = RequestedName;
        Request.FolderPath = NormalizeBrowserPath(m_contentCurrentFolder);
        m_onContentAssetCreateRequested(Request);
    }

    CloseContentAssetCreateModal();
}

void EditorLayout::RefreshContentAssetCreateModalVisibility()
{
    if (!m_context)
    {
        return;
    }

    if (m_contentCreateModalOpen)
    {
        EnsureContentAssetCreateModalOverlay();
        return;
    }

    DestroyContentAssetCreateModalOverlay();
}

void EditorLayout::RebuildContentAssetCreateTypeTree()
{
    if (!m_context || m_contentCreateTypeTree.Id.Value == 0)
    {
        return;
    }

    auto* Tree = dynamic_cast<SnAPI::UI::UITreeView*>(&m_context->GetElement(m_contentCreateTypeTree.Id));
    if (!Tree)
    {
        return;
    }

    auto* Source = dynamic_cast<VectorTreeItemSource*>(m_contentCreateTypeSource.get());
    if (!Source)
    {
        m_contentCreateTypeSource = std::make_shared<VectorTreeItemSource>();
        Source = static_cast<VectorTreeItemSource*>(m_contentCreateTypeSource.get());
    }

    if (Tree->ItemSource() != m_contentCreateTypeSource.get())
    {
        Tree->SetItemSource(m_contentCreateTypeSource.get());
    }

    const std::vector<CreateNodeTypeEntry> TypeEntries = BuildCreateNodeTypeEntries(m_contentCreateTypeFilterText);
    std::vector<SnAPI::UI::UITreeItem> TreeItems{};
    TreeItems.reserve(TypeEntries.size());
    m_contentCreateVisibleTypes.clear();
    m_contentCreateVisibleTypes.reserve(TypeEntries.size());

    for (const CreateNodeTypeEntry& Entry : TypeEntries)
    {
        std::string Label = Entry.Label.empty() ? Entry.QualifiedName : Entry.Label;
        if (Label.empty())
        {
            Label = "<unnamed>";
        }

        TreeItems.push_back(SnAPI::UI::UITreeItem{
            .Label = std::move(Label),
            .IconSource = ResolveUIImageSource(kHierarchyNodeIconPath),
            .IconTint = kIconWhite,
            .Depth = static_cast<uint32_t>(std::max(0, Entry.Depth)),
            .HasChildren = Entry.HasChildren,
            .Expanded = true,
        });
        m_contentCreateVisibleTypes.push_back(Entry.Type);
    }

    Source->SetItems(std::move(TreeItems));
    Tree->RefreshItemsFromSource();

    int32_t SelectedIndex = -1;
    if (m_contentCreateSelectedType != TypeId{})
    {
        const auto SelectedIt = std::find(m_contentCreateVisibleTypes.begin(),
                                          m_contentCreateVisibleTypes.end(),
                                          m_contentCreateSelectedType);
        if (SelectedIt != m_contentCreateVisibleTypes.end())
        {
            SelectedIndex = static_cast<int32_t>(std::distance(m_contentCreateVisibleTypes.begin(), SelectedIt));
        }
    }

    if (SelectedIndex < 0 && !m_contentCreateVisibleTypes.empty())
    {
        SelectedIndex = 0;
        m_contentCreateSelectedType = m_contentCreateVisibleTypes.front();
    }
    else if (SelectedIndex < 0)
    {
        m_contentCreateSelectedType = {};
    }

    Tree->SetSelectedIndex(SelectedIndex, false);
    RefreshContentAssetCreateOkButtonState();
    m_context->MarkLayoutDirty();
}

void EditorLayout::RefreshContentAssetCreateOkButtonState()
{
    if (!m_context || m_contentCreateOkButton.Id.Value == 0)
    {
        return;
    }

    std::string RequestedName = LeafBrowserName(NormalizeBrowserPath(m_contentCreateNameText));
    while (!RequestedName.empty() && std::isspace(static_cast<unsigned char>(RequestedName.front())))
    {
        RequestedName.erase(RequestedName.begin());
    }
    while (!RequestedName.empty() && std::isspace(static_cast<unsigned char>(RequestedName.back())))
    {
        RequestedName.pop_back();
    }

    const bool CanCreate = m_contentCreateSelectedType != TypeId{} && !RequestedName.empty();
    if (auto* Button = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_contentCreateOkButton.Id)))
    {
        Button->SetDisabled(!CanCreate);
    }
}

void EditorLayout::OpenContentAssetImportModal()
{
    if (!m_context)
    {
        return;
    }

    CloseContextMenu();
    m_contentImportModalOpen = true;
    m_contentImportAssimpSettings = {};
    m_contentImportTextureSettings = {};
    RefreshContentAssetImportProfile();
    RefreshContentAssetImportModalVisibility();
    RefreshContentAssetImportSettingsPanel();
    RefreshContentAssetImportSummary();
    RefreshContentAssetImportOkButtonState();
    m_context->MarkLayoutDirty();
}

void EditorLayout::CloseContentAssetImportModal()
{
    if (!m_contentImportModalOpen && m_contentImportModalOverlay.Id.Value == 0)
    {
        return;
    }

    m_contentImportModalOpen = false;
    RefreshContentAssetImportModalVisibility();
    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::ConfirmContentAssetImport()
{
    if (!m_contentImportModalOpen)
    {
        return;
    }

    if (m_context && m_contentImportSourcePicker.Id.Value != 0)
    {
        if (auto* Picker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_context->GetElement(m_contentImportSourcePicker.Id)))
        {
            m_contentImportSourcePath = TrimCopy(
                Picker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::ValueKey, std::string{}));
            if (m_contentImportSourcePath.empty())
            {
                const auto Selected = Picker->SelectedFilesystemPaths();
                if (!Selected.empty())
                {
                    m_contentImportSourcePath = Selected.front().string();
                }
            }
        }
    }

    m_contentImportSourcePath = TrimCopy(m_contentImportSourcePath);
    RefreshContentAssetImportProfile();
    if (m_contentImportSourcePath.empty() || m_contentImportProfile == EImportProfile::Unknown)
    {
        RefreshContentAssetImportSummary();
        RefreshContentAssetImportOkButtonState();
        return;
    }

    if (m_onContentAssetImportRequested)
    {
        ContentAssetImportRequest Request{};
        Request.SourcePath = m_contentImportSourcePath;
        Request.FolderPath = NormalizeBrowserPath(m_contentCurrentFolder);

        const auto BoolToText = [](const bool Value) {
            return Value ? std::string("true") : std::string("false");
        };

        if (m_contentImportProfile == EImportProfile::AssimpModel)
        {
            auto TypedSettings = std::make_shared<AssimpImporterSettings>();
            TypedSettings->Mesh.GenerateNormals = m_contentImportAssimpSettings.GenerateNormals;
            TypedSettings->Mesh.GenerateTangents = m_contentImportAssimpSettings.GenerateTangents;
            TypedSettings->Mesh.FlipUVs = m_contentImportAssimpSettings.FlipUVs;
            TypedSettings->Mesh.OptimizeMeshes = m_contentImportAssimpSettings.OptimizeMeshes;
            TypedSettings->Mesh.ForceSkeletal = m_contentImportAssimpSettings.ForceSkeletal;
            TypedSettings->Mesh.ForceStatic = m_contentImportAssimpSettings.ForceStatic;
            TypedSettings->Mesh.ImportMaterials = m_contentImportAssimpSettings.ImportMaterials;
            TypedSettings->Mesh.ImportTextures = m_contentImportAssimpSettings.ImportTextures;
            TypedSettings->Mesh.ImportAnimations = m_contentImportAssimpSettings.ImportAnimations;
            TypedSettings->Mesh.ImportSkeleton = m_contentImportAssimpSettings.ImportSkeleton;
            TypedSettings->Mesh.MaxBonesPerVertex = std::max(1u, m_contentImportAssimpSettings.MaxBonesPerVertex);
            Request.ImportSettings = TypedSettings;

            Request.BuildOptions.emplace("SnAPI.GF.Assimp.GenerateNormals", BoolToText(m_contentImportAssimpSettings.GenerateNormals));
            Request.BuildOptions.emplace("SnAPI.GF.Assimp.GenerateTangents", BoolToText(m_contentImportAssimpSettings.GenerateTangents));
            Request.BuildOptions.emplace("SnAPI.GF.Assimp.FlipUVs", BoolToText(m_contentImportAssimpSettings.FlipUVs));
            Request.BuildOptions.emplace("SnAPI.GF.Assimp.OptimizeMeshes", BoolToText(m_contentImportAssimpSettings.OptimizeMeshes));
            Request.BuildOptions.emplace("SnAPI.GF.Assimp.ForceSkeletal", BoolToText(m_contentImportAssimpSettings.ForceSkeletal));
            Request.BuildOptions.emplace("SnAPI.GF.Assimp.ForceStatic", BoolToText(m_contentImportAssimpSettings.ForceStatic));
            Request.BuildOptions.emplace("SnAPI.GF.Assimp.ImportMaterials", BoolToText(m_contentImportAssimpSettings.ImportMaterials));
            Request.BuildOptions.emplace("SnAPI.GF.Assimp.ImportTextures", BoolToText(m_contentImportAssimpSettings.ImportTextures));
            Request.BuildOptions.emplace("SnAPI.GF.Assimp.ImportAnimations", BoolToText(m_contentImportAssimpSettings.ImportAnimations));
            Request.BuildOptions.emplace("SnAPI.GF.Assimp.ImportSkeleton", BoolToText(m_contentImportAssimpSettings.ImportSkeleton));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.MaxBonesPerVertex",
                std::to_string(std::max(1u, m_contentImportAssimpSettings.MaxBonesPerVertex)));
        }
        else if (m_contentImportProfile == EImportProfile::Texture)
        {
            const auto TargetToOption = [](const ETextureCompressionTarget Target) {
                return Target == ETextureCompressionTarget::ASTC ? std::string("astc") : std::string("bcn");
            };
            const auto FormatToOption = [](const ETextureCompressionFormat Format) -> std::string {
                switch (Format)
                {
                case ETextureCompressionFormat::Auto: return {};
                case ETextureCompressionFormat::BC1: return "bc1";
                case ETextureCompressionFormat::BC3: return "bc3";
                case ETextureCompressionFormat::BC4: return "bc4";
                case ETextureCompressionFormat::BC5: return "bc5";
                case ETextureCompressionFormat::BC6H: return "bc6h";
                case ETextureCompressionFormat::BC7: return "bc7";
                case ETextureCompressionFormat::ASTC_4x4: return "astc_4x4";
                case ETextureCompressionFormat::ASTC_5x5: return "astc_5x5";
                case ETextureCompressionFormat::ASTC_6x6: return "astc_6x6";
                case ETextureCompressionFormat::ASTC_8x8: return "astc_8x8";
                case ETextureCompressionFormat::ASTC_10x10: return "astc_10x10";
                case ETextureCompressionFormat::ASTC_12x12: return "astc_12x12";
                case ETextureCompressionFormat::ASTC_4x4_HDR: return "astc_4x4_hdr";
                case ETextureCompressionFormat::ASTC_6x6_HDR: return "astc_6x6_hdr";
                case ETextureCompressionFormat::ASTC_8x8_HDR: return "astc_8x8_hdr";
                default: return {};
                }
            };
            const auto FormatToTyped = [](const ETextureCompressionFormat Format) -> TextureCompressorPlugin::ECompressedFormat {
                using TextureCompressorPlugin::ECompressedFormat;
                switch (Format)
                {
                case ETextureCompressionFormat::BC1: return ECompressedFormat::BC1;
                case ETextureCompressionFormat::BC3: return ECompressedFormat::BC3;
                case ETextureCompressionFormat::BC4: return ECompressedFormat::BC4;
                case ETextureCompressionFormat::BC5: return ECompressedFormat::BC5;
                case ETextureCompressionFormat::BC6H: return ECompressedFormat::BC6H;
                case ETextureCompressionFormat::BC7: return ECompressedFormat::BC7;
                case ETextureCompressionFormat::ASTC_4x4: return ECompressedFormat::ASTC_4x4;
                case ETextureCompressionFormat::ASTC_5x5: return ECompressedFormat::ASTC_5x5;
                case ETextureCompressionFormat::ASTC_6x6: return ECompressedFormat::ASTC_6x6;
                case ETextureCompressionFormat::ASTC_8x8: return ECompressedFormat::ASTC_8x8;
                case ETextureCompressionFormat::ASTC_10x10: return ECompressedFormat::ASTC_10x10;
                case ETextureCompressionFormat::ASTC_12x12: return ECompressedFormat::ASTC_12x12;
                case ETextureCompressionFormat::ASTC_4x4_HDR: return ECompressedFormat::ASTC_4x4_HDR;
                case ETextureCompressionFormat::ASTC_6x6_HDR: return ECompressedFormat::ASTC_6x6_HDR;
                case ETextureCompressionFormat::ASTC_8x8_HDR: return ECompressedFormat::ASTC_8x8_HDR;
                case ETextureCompressionFormat::Auto:
                default: return ECompressedFormat::Unknown;
                }
            };

            auto TypedSettings = std::make_shared<TextureCompressorPlugin::TextureCompressorImportSettings>();
            TypedSettings->Target = m_contentImportTextureSettings.Target == ETextureCompressionTarget::ASTC
                ? TextureCompressorPlugin::ECompressionTarget::ASTC
                : TextureCompressorPlugin::ECompressionTarget::BCn;
            TypedSettings->Format = FormatToTyped(m_contentImportTextureSettings.Format);
            TypedSettings->Quality = std::clamp(m_contentImportTextureSettings.Quality, 0.0f, 1.0f);
            TypedSettings->ForceNormalMap = m_contentImportTextureSettings.ForceNormalMap;
            TypedSettings->MaxMipCount = m_contentImportTextureSettings.MaxMips > 0u
                ? static_cast<int32_t>(m_contentImportTextureSettings.MaxMips)
                : -1;
            if (m_contentImportTextureSettings.ForceLinear)
            {
                TypedSettings->ColorSpacePolicy = TextureCompressorPlugin::ETextureColorSpacePolicy::ForceLinear;
            }
            else if (m_contentImportTextureSettings.ForceSrgb)
            {
                TypedSettings->ColorSpacePolicy = TextureCompressorPlugin::ETextureColorSpacePolicy::ForceSrgb;
            }
            Request.ImportSettings = TypedSettings;

            Request.BuildOptions.emplace("texture.target", TargetToOption(m_contentImportTextureSettings.Target));

            const std::string FormatOption = FormatToOption(m_contentImportTextureSettings.Format);
            if (!FormatOption.empty())
            {
                Request.BuildOptions.emplace("texture.format", FormatOption);
            }

            const float ClampedQuality = std::clamp(m_contentImportTextureSettings.Quality, 0.0f, 1.0f);
            Request.BuildOptions.emplace("texture.quality", std::to_string(ClampedQuality));

            if (m_contentImportTextureSettings.ForceLinear)
            {
                Request.BuildOptions.emplace("texture.srgb", "false");
            }
            else if (m_contentImportTextureSettings.ForceSrgb)
            {
                Request.BuildOptions.emplace("texture.srgb", "true");
            }

            if (m_contentImportTextureSettings.ForceNormalMap)
            {
                Request.BuildOptions.emplace("texture.normal_map", "true");
            }

            if (m_contentImportTextureSettings.MaxMips > 0u)
            {
                Request.BuildOptions.emplace("texture.max_mips", std::to_string(m_contentImportTextureSettings.MaxMips));
            }
        }

        m_onContentAssetImportRequested(Request);
    }

    CloseContentAssetImportModal();
}

void EditorLayout::RefreshContentAssetImportModalVisibility()
{
    if (!m_context)
    {
        return;
    }

    if (m_contentImportModalOpen)
    {
        EnsureContentAssetImportModalOverlay();
        return;
    }

    DestroyContentAssetImportModalOverlay();
}

void EditorLayout::RefreshContentAssetImportProfile()
{
    const std::string SourcePath = TrimCopy(m_contentImportSourcePath);
    if (HasPathExtension(SourcePath, kImportModelExtensions))
    {
        m_contentImportProfile = EImportProfile::AssimpModel;
        return;
    }

    if (HasPathExtension(SourcePath, kImportTextureExtensions))
    {
        m_contentImportProfile = EImportProfile::Texture;
        return;
    }

    m_contentImportProfile = EImportProfile::Unknown;
}

void EditorLayout::RefreshContentAssetImportSettingsPanel()
{
    if (!m_context || m_contentImportSettingsPanel.Id.Value == 0)
    {
        return;
    }

    auto* Panel = dynamic_cast<UIPropertyPanel*>(&m_context->GetElement(m_contentImportSettingsPanel.Id));
    if (!Panel)
    {
        return;
    }

    switch (m_contentImportProfile)
    {
    case EImportProfile::AssimpModel:
        (void)Panel->BindObject<AssimpImportSettings>(&m_contentImportAssimpSettings);
        break;
    case EImportProfile::Texture:
        (void)Panel->BindObject<TextureImportSettings>(&m_contentImportTextureSettings);
        break;
    default:
        Panel->ClearObject();
        break;
    }
}

void EditorLayout::RefreshContentAssetImportSummary()
{
    if (!m_context || m_contentImportSummaryText.Id.Value == 0)
    {
        return;
    }

    auto* SummaryText = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(m_contentImportSummaryText.Id));
    if (!SummaryText)
    {
        return;
    }

    const std::string FolderPath = NormalizeBrowserPath(m_contentCurrentFolder);
    const std::string Destination =
        FolderPath.empty() ? std::string("Content/Assets") : (std::string("Content/Assets/") + FolderPath);
    const std::string SourcePath = TrimCopy(m_contentImportSourcePath);

    std::string Message = "Destination: " + Destination + ". ";
    if (SourcePath.empty())
    {
        Message += "Select a source file to detect the importer.";
    }
    else if (m_contentImportProfile == EImportProfile::AssimpModel)
    {
        Message += "Importer: Assimp (model pipeline with configurable materials/textures/animations/skeleton).";
    }
    else if (m_contentImportProfile == EImportProfile::Texture)
    {
        Message += "Importer: TextureCompressor (raw texture source formats).";
    }
    else
    {
        Message += "No importer matched this file extension.";
    }

    SummaryText->Text().Set(std::move(Message));
}

void EditorLayout::RefreshContentAssetImportOkButtonState()
{
    if (!m_context || m_contentImportOkButton.Id.Value == 0)
    {
        return;
    }

    const bool HasSource = !TrimCopy(m_contentImportSourcePath).empty();
    const bool CanImport = HasSource && m_contentImportProfile != EImportProfile::Unknown;
    if (auto* Button = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_contentImportOkButton.Id)))
    {
        Button->SetDisabled(!CanImport);
    }
}

void EditorLayout::OpenProjectWelcomeModal()
{
    if (!m_context)
    {
        return;
    }

    CloseContextMenu();
    CloseProjectSettingsModal();
    m_projectModalOpen = true;
    m_projectModalShowWelcome = true;
    m_projectModalAction = EProjectAction::CreateNew;
    if (m_projectNameText.empty())
    {
        m_projectNameText = "NewProject";
    }
    if (m_projectDirectoryText.empty())
    {
        std::error_code Error{};
        const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
        if (!Error && !CurrentPath.empty())
        {
            m_projectDirectoryText = CurrentPath.string();
        }
    }
    if (m_projectFilePathText.empty())
    {
        std::error_code Error{};
        const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
        if (!Error && !CurrentPath.empty())
        {
            m_projectFilePathText = (CurrentPath / std::string(kDefaultProjectConfigFileName)).string();
        }
    }
    DestroyProjectModalOverlay();
    RefreshProjectModalVisibility();
    RefreshProjectModalOkButtonState();
    m_context->MarkLayoutDirty();
}

void EditorLayout::OpenProjectCreateModal()
{
    if (!m_context)
    {
        return;
    }

    CloseContextMenu();
    CloseProjectSettingsModal();
    m_projectModalAction = EProjectAction::CreateNew;
    m_projectModalOpen = true;
    m_projectModalShowWelcome = false;
    m_projectNameText = "NewProject";
    if (m_projectDirectoryText.empty())
    {
        std::error_code Error{};
        const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
        if (!Error && !CurrentPath.empty())
        {
            m_projectDirectoryText = CurrentPath.string();
        }
    }
    m_projectFilePathText.clear();
    DestroyProjectModalOverlay();
    RefreshProjectModalVisibility();
    RefreshProjectModalOkButtonState();
    m_context->MarkLayoutDirty();
}

void EditorLayout::OpenProjectOpenModal()
{
    if (!m_context)
    {
        return;
    }

    CloseContextMenu();
    CloseProjectSettingsModal();
    m_projectModalAction = EProjectAction::OpenExisting;
    m_projectModalOpen = true;
    m_projectModalShowWelcome = false;
    m_projectNameText.clear();
    if (m_projectFilePathText.empty())
    {
        std::error_code Error{};
        const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
        if (!Error && !CurrentPath.empty())
        {
            m_projectFilePathText = (CurrentPath / "project.snproj.json").string();
        }
    }
    DestroyProjectModalOverlay();
    RefreshProjectModalVisibility();
    RefreshProjectModalOkButtonState();
    m_context->MarkLayoutDirty();
}

void EditorLayout::OpenProjectSettingsModal()
{
    if (!m_context || !m_projectState.IsLoaded)
    {
        return;
    }

    CloseContextMenu();
    m_projectSettingsModalOpen = true;
    m_projectSettingsNameText = m_projectState.Name;
    m_projectSettingsStartupAssetText = m_projectState.StartupLevelAsset;
    m_projectSettingsDefaultRenderSettingsAssetId = m_projectState.DefaultRenderSettingsAssetId;
    DestroyProjectSettingsModalOverlay();
    RefreshProjectSettingsModalVisibility();
    RefreshProjectSettingsModalSaveButtonState();
    m_context->MarkLayoutDirty();
}

void EditorLayout::CloseProjectSettingsModal()
{
    if (!m_projectSettingsModalOpen && m_projectSettingsModalOverlay.Id.Value == 0)
    {
        return;
    }

    m_projectSettingsModalOpen = false;
    RefreshProjectSettingsModalVisibility();
    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::ConfirmProjectSettingsModal()
{
    if (!m_projectSettingsModalOpen)
    {
        return;
    }

    if (m_context && m_projectSettingsNameInput.Id.Value != 0)
    {
        if (auto* NameInput = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_projectSettingsNameInput.Id)))
        {
            m_projectSettingsNameText = NameInput->Text().Get();
        }
    }

    if (m_context && m_projectSettingsStartupAssetInput.Id.Value != 0)
    {
        if (auto* Picker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_context->GetElement(m_projectSettingsStartupAssetInput.Id)))
        {
            m_projectSettingsStartupAssetText = TrimCopy(
                Picker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::ValueKey, std::string{}));
            if (m_projectSettingsStartupAssetText.empty())
            {
                const auto Selected = Picker->SelectedFilesystemPaths();
                if (!Selected.empty())
                {
                    m_projectSettingsStartupAssetText = Selected.front().string();
                }
            }
        }
    }

    ProjectActionRequest Request{};
    Request.Action = EProjectAction::SaveSettings;
    Request.ProjectName = TrimCopy(m_projectSettingsNameText);
    Request.ProjectFilePath = m_projectState.ProjectFilePath;
    Request.StartupLevelAsset = TrimCopy(m_projectSettingsStartupAssetText);
    Request.DefaultRenderSettingsAssetId = TrimCopy(m_projectSettingsDefaultRenderSettingsAssetId);
    if (Request.ProjectName.empty())
    {
        RefreshProjectSettingsModalSaveButtonState();
        return;
    }

    if (m_onProjectActionRequested)
    {
        m_onProjectActionRequested(Request);
    }
    CloseProjectSettingsModal();
}

void EditorLayout::CloseProjectModal(const bool ForceClose)
{
    if (!m_projectModalOpen && m_projectModalOverlay.Id.Value == 0)
    {
        return;
    }

    if (m_projectModalRequired && !ForceClose)
    {
        RefreshProjectModalVisibility();
        return;
    }

    m_projectModalOpen = false;
    m_projectModalShowWelcome = false;
    RefreshProjectModalVisibility();
    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::ConfirmProjectModal()
{
    if (!m_projectModalOpen)
    {
        return;
    }

    if (m_context)
    {
        if (m_projectModalAction == EProjectAction::CreateNew && m_projectDirectoryInput.Id.Value != 0)
        {
            if (auto* Picker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_context->GetElement(m_projectDirectoryInput.Id)))
            {
                m_projectDirectoryText = TrimCopy(
                    Picker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::ValueKey, std::string{}));
                if (m_projectDirectoryText.empty())
                {
                    const auto Selected = Picker->SelectedFilesystemPaths();
                    if (!Selected.empty())
                    {
                        m_projectDirectoryText = Selected.front().string();
                    }
                }
                if (m_projectDirectoryText.empty())
                {
                    m_projectDirectoryText = TrimCopy(
                        Picker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::CurrentPathKey, std::string{}));
                }
            }
        }
        else if (m_projectModalAction == EProjectAction::OpenExisting && m_projectFilePathInput.Id.Value != 0)
        {
            if (auto* Picker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_context->GetElement(m_projectFilePathInput.Id)))
            {
                m_projectFilePathText = TrimCopy(
                    Picker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::ValueKey, std::string{}));
                if (m_projectFilePathText.empty())
                {
                    const auto Selected = Picker->SelectedFilesystemPaths();
                    if (!Selected.empty())
                    {
                        m_projectFilePathText = Selected.front().string();
                    }
                }
                if (m_projectFilePathText.empty())
                {
                    const std::string CurrentPathText = TrimCopy(
                        Picker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::CurrentPathKey, std::string{}));
                    if (!CurrentPathText.empty())
                    {
                        const std::filesystem::path Candidate = std::filesystem::path(CurrentPathText) / "project.snproj.json";
                        std::error_code Error{};
                        if (std::filesystem::exists(Candidate, Error) && !Error)
                        {
                            m_projectFilePathText = Candidate.string();
                        }
                    }
                }
            }
        }
    }

    ProjectActionRequest Request{};
    Request.Action = m_projectModalAction;
    Request.ProjectName = TrimCopy(m_projectNameText);
    Request.ProjectDirectory = TrimCopy(m_projectDirectoryText);
    Request.ProjectFilePath = TrimCopy(m_projectFilePathText);

    if (Request.Action == EProjectAction::CreateNew)
    {
        if (Request.ProjectName.empty() || Request.ProjectDirectory.empty())
        {
            RefreshProjectModalOkButtonState();
            return;
        }
    }
    else if (Request.ProjectFilePath.empty())
    {
        RefreshProjectModalOkButtonState();
        return;
    }

    RememberRecentProject(Request);

    if (m_onProjectActionRequested)
    {
        m_onProjectActionRequested(Request);
    }
    CloseProjectModal(true);
}

void EditorLayout::RememberRecentProject(const ProjectActionRequest& Request)
{
    if (Request.Action == EProjectAction::SaveSettings)
    {
        return;
    }

    if (Request.Action == EProjectAction::CreateNew)
    {
        const std::string ProjectName = TrimCopy(Request.ProjectName);
        const std::string ProjectDirectory = TrimCopy(Request.ProjectDirectory);
        if (ProjectName.empty() || ProjectDirectory.empty())
        {
            return;
        }

        const std::filesystem::path ProjectFilePath =
            std::filesystem::path(ProjectDirectory) / ProjectName / std::string(kDefaultProjectConfigFileName);
        RememberRecentProjectFile(ProjectFilePath.string(), ProjectName);
        return;
    }

    RememberRecentProjectFile(Request.ProjectFilePath, Request.ProjectName);
}

void EditorLayout::RememberRecentProjectFile(std::string ProjectFilePath, std::string ProjectName)
{
    ProjectFilePath = TrimCopy(std::move(ProjectFilePath));
    if (ProjectFilePath.empty())
    {
        return;
    }

    std::filesystem::path FilePath(ProjectFilePath);
    std::error_code Error{};
    if (!FilePath.is_absolute())
    {
        const std::filesystem::path AbsolutePath = std::filesystem::absolute(FilePath, Error);
        if (!Error && !AbsolutePath.empty())
        {
            FilePath = AbsolutePath;
        }
    }

    const std::string NormalizedPath = FilePath.lexically_normal().string();
    if (NormalizedPath.empty())
    {
        return;
    }

    ProjectName = ProjectNameFromFilePath(NormalizedPath, ProjectName);

    const std::string NormalizedPathLower = ToLower(NormalizedPath);
    const auto ExistingIt = std::find_if(m_recentProjects.begin(),
                                         m_recentProjects.end(),
                                         [&NormalizedPathLower](const RecentProjectEntry& Entry) {
                                             return ToLower(Entry.ProjectFilePath) == NormalizedPathLower;
                                         });
    if (ExistingIt != m_recentProjects.end())
    {
        ExistingIt->Name = std::move(ProjectName);
        ExistingIt->ProjectFilePath = NormalizedPath;
        RecentProjectEntry Existing = std::move(*ExistingIt);
        m_recentProjects.erase(ExistingIt);
        m_recentProjects.insert(m_recentProjects.begin(), std::move(Existing));
    }
    else
    {
        m_recentProjects.insert(
            m_recentProjects.begin(),
            RecentProjectEntry{
                .Name = std::move(ProjectName),
                .ProjectFilePath = NormalizedPath,
            });
    }

    if (m_recentProjects.size() > kMaxRecentProjects)
    {
        m_recentProjects.resize(kMaxRecentProjects);
    }
}

void EditorLayout::RefreshProjectModalVisibility()
{
    if (!m_context)
    {
        return;
    }

    if (m_projectModalOpen)
    {
        EnsureProjectModalOverlay();
        return;
    }

    DestroyProjectModalOverlay();
}

void EditorLayout::RefreshProjectModalOkButtonState()
{
    if (!m_context || m_projectModalOkButton.Id.Value == 0)
    {
        return;
    }

    bool CanConfirm = false;
    if (m_projectModalAction == EProjectAction::CreateNew)
    {
        std::string DirectoryText = TrimCopy(m_projectDirectoryText);
        if (DirectoryText.empty() && m_projectDirectoryInput.Id.Value != 0)
        {
            if (auto* Picker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_context->GetElement(m_projectDirectoryInput.Id)))
            {
                DirectoryText = TrimCopy(
                    Picker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::ValueKey, std::string{}));
                if (DirectoryText.empty())
                {
                    const auto Selected = Picker->SelectedFilesystemPaths();
                    if (!Selected.empty())
                    {
                        DirectoryText = Selected.front().string();
                    }
                }
                if (DirectoryText.empty())
                {
                    DirectoryText = TrimCopy(
                        Picker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::CurrentPathKey, std::string{}));
                }
            }
        }

        CanConfirm = !TrimCopy(m_projectNameText).empty() && !DirectoryText.empty();
    }
    else
    {
        std::string FilePathText = TrimCopy(m_projectFilePathText);
        if (FilePathText.empty() && m_projectFilePathInput.Id.Value != 0)
        {
            if (auto* Picker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_context->GetElement(m_projectFilePathInput.Id)))
            {
                FilePathText = TrimCopy(
                    Picker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::ValueKey, std::string{}));
                if (FilePathText.empty())
                {
                    const auto Selected = Picker->SelectedFilesystemPaths();
                    if (!Selected.empty())
                    {
                        FilePathText = Selected.front().string();
                    }
                }
                if (FilePathText.empty())
                {
                    const std::string CurrentPathText = TrimCopy(
                        Picker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::CurrentPathKey, std::string{}));
                    if (!CurrentPathText.empty())
                    {
                        const std::filesystem::path Candidate = std::filesystem::path(CurrentPathText) / "project.snproj.json";
                        std::error_code Error{};
                        if (std::filesystem::exists(Candidate, Error) && !Error)
                        {
                            FilePathText = Candidate.string();
                        }
                    }
                }
            }
        }

        CanConfirm = !FilePathText.empty();
    }

    if (auto* Button = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_projectModalOkButton.Id)))
    {
        Button->SetDisabled(!CanConfirm);
    }
}

void EditorLayout::RefreshProjectSettingsModalVisibility()
{
    if (!m_context)
    {
        return;
    }

    if (m_projectSettingsModalOpen)
    {
        EnsureProjectSettingsModalOverlay();
        return;
    }

    DestroyProjectSettingsModalOverlay();
}

void EditorLayout::RefreshProjectSettingsModalSaveButtonState()
{
    if (!m_context || m_projectSettingsSaveButton.Id.Value == 0)
    {
        return;
    }

    std::string NameText = TrimCopy(m_projectSettingsNameText);
    if (NameText.empty() && m_projectSettingsNameInput.Id.Value != 0)
    {
        if (auto* NameInput = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_projectSettingsNameInput.Id)))
        {
            NameText = TrimCopy(NameInput->Text().Get());
        }
    }

    const bool CanSave = m_projectState.IsLoaded && !NameText.empty();
    if (auto* Button = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_projectSettingsSaveButton.Id)))
    {
        Button->SetDisabled(!CanSave);
    }
}

void EditorLayout::DestroyContentAssetInspectorModalOverlay()
{
    if (m_context && m_contentInspectorPropertyPanel.Id.Value != 0)
    {
        if (auto* PropertyPanel = dynamic_cast<UIPropertyPanel*>(&m_context->GetElement(m_contentInspectorPropertyPanel.Id)))
        {
            PropertyPanel->ClearObject();
        }
    }
    if (m_context && m_contentInspectorImportSettingsPanel.Id.Value != 0)
    {
        if (auto* PropertyPanel = dynamic_cast<UIPropertyPanel*>(&m_context->GetElement(m_contentInspectorImportSettingsPanel.Id)))
        {
            PropertyPanel->ClearObject();
        }
    }
    if (m_context && m_contentInspectorPreviewImage.Id.Value != 0)
    {
        if (auto* Preview = dynamic_cast<SnAPI::UI::UIImage*>(&m_context->GetElement(m_contentInspectorPreviewImage.Id)))
        {
            Preview->Texture().Set(SnAPI::UI::TextureId{});
            Preview->Source().Set(std::string{});
        }
    }

    m_contentInspectorTargetBound = false;
    m_contentInspectorBoundNode = {};
    m_contentInspectorBoundObject = nullptr;
    m_contentInspectorBoundType = {};
    m_contentInspectorBoundComponentSignature = 0;
    m_contentInspectorImportTargetBound = false;
    m_contentInspectorImportBoundObject = nullptr;
    m_contentInspectorImportBoundType = {};

    if (m_context && m_contentInspectorModalOverlay.Id.Value != 0)
    {
        const SnAPI::UI::ElementId OverlayId = m_contentInspectorModalOverlay.Id;
        const SnAPI::UI::ElementId CapturedElement = m_context->GetCapture();
        if (IsElementWithinSubtree(*m_context, CapturedElement, OverlayId))
        {
            m_context->ReleaseCapture();
        }
        m_context->DestroyElement(OverlayId);
    }

    m_contentInspectorModalOverlay = {};
    m_contentInspectorTitleText = {};
    m_contentInspectorStatusText = {};
    m_contentInspectorHierarchyTitleText = {};
    m_contentInspectorPreviewStatsText = {};
    m_contentInspectorPreviewImage = {};
    m_contentInspectorHierarchyTree = {};
    m_contentInspectorPropertyPanel = {};
    m_contentInspectorImportSettingsTitleText = {};
    m_contentInspectorImportSettingsPanel = {};
    m_contentInspectorSaveButton = {};
    m_contentInspectorReimportButton = {};
    m_contentInspectorVisibleNodes.clear();
}

void EditorLayout::CloseContentAssetInspectorModal(const bool NotifyHandler)
{
    const bool WasOpen = m_contentAssetInspectorState.Open;
    m_contentAssetInspectorState.Open = false;
    m_contentAssetInspectorState.TargetObject = nullptr;
    m_contentAssetInspectorState.TargetType = {};
    m_contentAssetInspectorState.ImportSettingsObject = nullptr;
    m_contentAssetInspectorState.ImportSettingsType = {};
    m_contentAssetInspectorState.SelectedNode = {};
    m_contentAssetInspectorState.Nodes.clear();
    m_contentAssetInspectorState.CanEditHierarchy = false;
    m_contentAssetInspectorState.HasImportSettings = false;
    m_contentAssetInspectorState.RuntimeDirty = false;
    m_contentAssetInspectorState.ImportSettingsDirty = false;
    m_contentAssetInspectorState.IsDirty = false;
    m_contentAssetInspectorState.CanSave = false;
    m_contentAssetInspectorState.CanReimport = false;
    m_contentAssetInspectorState.PreviewIconSource.clear();
    m_contentAssetInspectorState.PreviewTextureId = 0;
    m_contentAssetInspectorState.PreviewWidth = 0;
    m_contentAssetInspectorState.PreviewHeight = 0;
    m_contentAssetInspectorState.PreviewStatsPrimary.clear();
    m_contentAssetInspectorState.PreviewStatsSecondary.clear();

    RefreshContentAssetInspectorModalVisibility();

    if (NotifyHandler && WasOpen && m_onContentAssetInspectorCloseRequested)
    {
        m_onContentAssetInspectorCloseRequested();
    }

    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::RefreshContentAssetInspectorModalVisibility()
{
    if (!m_context)
    {
        return;
    }

    if (m_contentAssetInspectorState.Open)
    {
        EnsureContentAssetInspectorModalOverlay();
        return;
    }

    DestroyContentAssetInspectorModalOverlay();
}

void EditorLayout::RebuildContentAssetInspectorHierarchyTree()
{
    if (!m_context || m_contentInspectorHierarchyTree.Id.Value == 0)
    {
        return;
    }

    auto* Tree = dynamic_cast<SnAPI::UI::UITreeView*>(&m_context->GetElement(m_contentInspectorHierarchyTree.Id));
    if (!Tree)
    {
        return;
    }

    auto* Source = dynamic_cast<VectorTreeItemSource*>(m_contentInspectorHierarchySource.get());
    if (!Source)
    {
        m_contentInspectorHierarchySource = std::make_shared<VectorTreeItemSource>();
        Source = static_cast<VectorTreeItemSource*>(m_contentInspectorHierarchySource.get());
    }

    if (Tree->ItemSource() != m_contentInspectorHierarchySource.get())
    {
        Tree->SetItemSource(m_contentInspectorHierarchySource.get());
    }

    std::vector<SnAPI::UI::UITreeItem> TreeItems{};
    m_contentInspectorVisibleNodes.clear();
    TreeItems.reserve(m_contentAssetInspectorState.Nodes.size());
    m_contentInspectorVisibleNodes.reserve(m_contentAssetInspectorState.Nodes.size());

    if (m_contentAssetInspectorState.Open && m_contentAssetInspectorState.CanEditHierarchy)
    {
        for (const auto& Entry : m_contentAssetInspectorState.Nodes)
        {
            std::string Label = Entry.Label.empty() ? std::string("<unnamed>") : Entry.Label;
            TreeItems.push_back(SnAPI::UI::UITreeItem{
                .Label = std::move(Label),
                .IconSource = ResolveUIImageSource(kHierarchyNodeIconPath),
                .IconTint = kIconWhite,
                .Depth = static_cast<uint32_t>(std::max(0, Entry.Depth)),
                .HasChildren = false,
                .Expanded = true,
            });
            m_contentInspectorVisibleNodes.push_back(Entry.Handle);
        }
    }

    Source->SetItems(std::move(TreeItems));
    Tree->RefreshItemsFromSource();

    int32_t SelectedIndex = -1;
    if (!m_contentAssetInspectorState.SelectedNode.IsNull())
    {
        const auto SelectedIt = std::find(
            m_contentInspectorVisibleNodes.begin(),
            m_contentInspectorVisibleNodes.end(),
            m_contentAssetInspectorState.SelectedNode);
        if (SelectedIt != m_contentInspectorVisibleNodes.end())
        {
            SelectedIndex = static_cast<int32_t>(std::distance(m_contentInspectorVisibleNodes.begin(), SelectedIt));
        }
    }
    if (SelectedIndex < 0 && !m_contentInspectorVisibleNodes.empty())
    {
        SelectedIndex = 0;
        m_contentAssetInspectorState.SelectedNode = m_contentInspectorVisibleNodes.front();
    }
    else if (SelectedIndex < 0)
    {
        m_contentAssetInspectorState.SelectedNode = {};
    }

    Tree->SetSelectedIndex(SelectedIndex, false);
}

void EditorLayout::RefreshContentAssetInspectorModalState()
{
    if (!m_context)
    {
        return;
    }

    bool HasRuntimeTarget = m_contentAssetInspectorState.TargetObject != nullptr &&
                            m_contentAssetInspectorState.TargetType != TypeId{};
    void* RuntimeBindingTargetObject = m_contentAssetInspectorState.TargetObject;
    TypeId RuntimeBindingTargetType = m_contentAssetInspectorState.TargetType;
    BaseNode* SelectedHierarchyNode = nullptr;
    NodeHandle RuntimeBindingTargetNode{};
    std::size_t RuntimeBindingComponentSignature = 0;
    if (m_contentAssetInspectorState.CanEditHierarchy && !m_contentAssetInspectorState.SelectedNode.IsNull())
    {
        SelectedHierarchyNode = m_contentAssetInspectorState.SelectedNode.Borrowed();
        if (!SelectedHierarchyNode)
        {
            SelectedHierarchyNode = m_contentAssetInspectorState.SelectedNode.BorrowedSlowByUuid();
        }
        if (SelectedHierarchyNode)
        {
            RuntimeBindingTargetObject = SelectedHierarchyNode;
            RuntimeBindingTargetType = SelectedHierarchyNode->TypeKey();
            RuntimeBindingTargetNode = m_contentAssetInspectorState.SelectedNode;
            RuntimeBindingComponentSignature = ComputeNodeComponentSignature(*SelectedHierarchyNode);
            HasRuntimeTarget = true;
        }
    }
    const bool HasImportTarget =
        m_contentAssetInspectorState.HasImportSettings &&
        m_contentAssetInspectorState.ImportSettingsObject != nullptr &&
        m_contentAssetInspectorState.ImportSettingsType != TypeId{};
    const bool HasHierarchy = m_contentAssetInspectorState.CanEditHierarchy &&
                              !m_contentAssetInspectorState.Nodes.empty();
    const bool HasPreview = m_contentAssetInspectorState.PreviewTextureId != 0 ||
                            !m_contentAssetInspectorState.PreviewIconSource.empty();
    const bool HasPreviewStats = !m_contentAssetInspectorState.PreviewStatsPrimary.empty() ||
                                 !m_contentAssetInspectorState.PreviewStatsSecondary.empty();

    if (m_contentInspectorTitleText.Id.Value != 0)
    {
        if (auto* Title = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(m_contentInspectorTitleText.Id)))
        {
            std::string TitleText = m_contentAssetInspectorState.Title.empty()
                                        ? std::string("Asset Inspector")
                                        : m_contentAssetInspectorState.Title;
            if (m_contentAssetInspectorState.IsDirty)
            {
                TitleText = "* " + TitleText;
            }
            Title->Text().Set(std::move(TitleText));
        }
    }

    if (m_contentInspectorStatusText.Id.Value != 0)
    {
        if (auto* Status = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(m_contentInspectorStatusText.Id)))
        {
            std::string StatusText = m_contentAssetInspectorState.Status;
            if (StatusText.empty())
            {
                if (!m_contentAssetInspectorState.Open)
                {
                    StatusText = "Double-click an asset to inspect and edit properties.";
                }
                else if (!HasRuntimeTarget && !HasImportTarget)
                {
                    StatusText = "No editable payload is available for this asset.";
                }
                else
                {
                    if (m_contentAssetInspectorState.CanEditHierarchy)
                    {
                        const bool HasSelection = !m_contentAssetInspectorState.SelectedNode.IsNull();
                        StatusText = m_contentAssetInspectorState.IsDirty
                                         ? "Unsaved changes. Right-click hierarchy rows to add/remove nodes/components."
                                         : (HasSelection ? "Right-click hierarchy rows to edit structure."
                                                         : "Select a node in the hierarchy to edit.");
                    }
                    else
                    {
                        if (m_contentAssetInspectorState.RuntimeDirty && m_contentAssetInspectorState.ImportSettingsDirty)
                        {
                            StatusText = "Runtime and import settings changed. Save settings, then Reimport to apply import changes.";
                        }
                        else if (m_contentAssetInspectorState.RuntimeDirty)
                        {
                            StatusText = "Runtime settings changed. Click Save to persist.";
                        }
                        else if (m_contentAssetInspectorState.ImportSettingsDirty)
                        {
                            StatusText = "Import settings changed. Save to persist and Reimport to apply.";
                        }
                        else
                        {
                            StatusText = "No pending edits.";
                        }
                    }
                }
            }
            Status->Text().Set(std::move(StatusText));
        }
    }

    if (m_contentInspectorHierarchyTitleText.Id.Value != 0)
    {
        if (auto* HierarchyTitle = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(m_contentInspectorHierarchyTitleText.Id)))
        {
            HierarchyTitle->Text().Set(HasHierarchy ? std::string("Asset Hierarchy") : std::string("Asset Preview"));
        }
    }

    if (m_contentInspectorHierarchyTree.Id.Value != 0)
    {
        if (auto* HierarchyTree = dynamic_cast<SnAPI::UI::UITreeView*>(&m_context->GetElement(m_contentInspectorHierarchyTree.Id)))
        {
            HierarchyTree->Visibility().Set(
                (m_contentAssetInspectorState.Open && HasHierarchy)
                    ? SnAPI::UI::EVisibility::Visible
                    : SnAPI::UI::EVisibility::Collapsed);
        }
    }

    if (m_contentInspectorPreviewImage.Id.Value != 0)
    {
        if (auto* Preview = dynamic_cast<SnAPI::UI::UIImage*>(&m_context->GetElement(m_contentInspectorPreviewImage.Id)))
        {
            const bool ShowPreview = m_contentAssetInspectorState.Open && !HasHierarchy && HasPreview;
            if (ShowPreview)
            {
                if (m_contentAssetInspectorState.PreviewTextureId != 0)
                {
                    Preview->Width().Set(SnAPI::UI::Sizing::Fill());
                    Preview->Height().Set(SnAPI::UI::Sizing::Auto());
                    Preview->Mode().Set(SnAPI::UI::EImageMode::Aspect);
                    if (!Preview->Source().Get().empty())
                    {
                        Preview->Source().Set(std::string{});
                    }
                    if (Preview->Texture().Get().Value != m_contentAssetInspectorState.PreviewTextureId)
                    {
                        Preview->Texture().Set(SnAPI::UI::TextureId{m_contentAssetInspectorState.PreviewTextureId});
                    }
                    if (m_contentAssetInspectorState.PreviewWidth > 0u && m_contentAssetInspectorState.PreviewHeight > 0u)
                    {
                        Preview->SetIntrinsicSize(
                            static_cast<float>(m_contentAssetInspectorState.PreviewWidth),
                            static_cast<float>(m_contentAssetInspectorState.PreviewHeight));
                    }
                }
                else
                {
                    Preview->Width().Set(SnAPI::UI::Sizing::Fill());
                    Preview->Height().Set(SnAPI::UI::Sizing::Auto());
                    Preview->Mode().Set(SnAPI::UI::EImageMode::Aspect);
                    const std::string PreviewSource = ResolveUIImageSource(m_contentAssetInspectorState.PreviewIconSource);
                    if (Preview->Source().Get() != PreviewSource)
                    {
                        Preview->Source().Set(PreviewSource);
                    }
                    if (!(Preview->SvgOptions().Get() == SnAPI::UI::SVGImageOptions{}))
                    {
                        Preview->SvgOptions().Set(SnAPI::UI::SVGImageOptions{});
                    }
                }
                Preview->Visibility().Set(SnAPI::UI::EVisibility::Visible);
            }
            else
            {
                Preview->Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
            }
        }
    }

    if (m_contentInspectorPreviewStatsText.Id.Value != 0)
    {
        if (auto* PreviewStats = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(m_contentInspectorPreviewStatsText.Id)))
        {
            const bool ShowPreviewStats = m_contentAssetInspectorState.Open && !HasHierarchy && HasPreview && HasPreviewStats;
            if (ShowPreviewStats)
            {
                std::string StatsText = m_contentAssetInspectorState.PreviewStatsPrimary;
                if (!m_contentAssetInspectorState.PreviewStatsSecondary.empty())
                {
                    if (!StatsText.empty())
                    {
                        StatsText += '\n';
                    }
                    StatsText += m_contentAssetInspectorState.PreviewStatsSecondary;
                }
                PreviewStats->Text().Set(std::move(StatsText));
                PreviewStats->Visibility().Set(SnAPI::UI::EVisibility::Visible);
            }
            else
            {
                PreviewStats->Text().Set(std::string{});
                PreviewStats->Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
            }
        }
    }

    if (m_contentInspectorPropertyPanel.Id.Value != 0)
    {
        if (auto* PropertyPanel = dynamic_cast<UIPropertyPanel*>(&m_context->GetElement(m_contentInspectorPropertyPanel.Id)))
        {
            if (m_contentAssetInspectorState.Open && HasRuntimeTarget)
            {
                const bool BindingNodeTarget = !RuntimeBindingTargetNode.IsNull() && SelectedHierarchyNode != nullptr;
                const bool NeedsRebind =
                    !m_contentInspectorTargetBound ||
                    (BindingNodeTarget
                        ? (m_contentInspectorBoundNode != RuntimeBindingTargetNode ||
                           m_contentInspectorBoundObject != RuntimeBindingTargetObject ||
                           m_contentInspectorBoundType != RuntimeBindingTargetType ||
                           m_contentInspectorBoundComponentSignature != RuntimeBindingComponentSignature)
                        : (m_contentInspectorBoundObject != RuntimeBindingTargetObject ||
                           m_contentInspectorBoundType != RuntimeBindingTargetType));
                if (NeedsRebind)
                {
                    PropertyPanel->ClearObject();
                    if (SelectedHierarchyNode && m_contentAssetInspectorState.CanEditHierarchy)
                    {
                        m_contentInspectorTargetBound = PropertyPanel->BindNode(SelectedHierarchyNode);
                    }
                    else
                    {
                        m_contentInspectorTargetBound = PropertyPanel->BindObject(RuntimeBindingTargetType, RuntimeBindingTargetObject);
                    }
                    if (m_contentInspectorTargetBound)
                    {
                        m_contentInspectorBoundNode = RuntimeBindingTargetNode;
                        m_contentInspectorBoundObject = RuntimeBindingTargetObject;
                        m_contentInspectorBoundType = RuntimeBindingTargetType;
                        m_contentInspectorBoundComponentSignature = RuntimeBindingComponentSignature;
                    }
                    else
                    {
                        m_contentInspectorBoundNode = {};
                        m_contentInspectorBoundObject = nullptr;
                        m_contentInspectorBoundType = {};
                        m_contentInspectorBoundComponentSignature = 0;
                    }
                }
                else
                {
                    PropertyPanel->RefreshFromModel();
                }
            }
            else if (m_contentInspectorTargetBound)
            {
                PropertyPanel->ClearObject();
                m_contentInspectorTargetBound = false;
                m_contentInspectorBoundNode = {};
                m_contentInspectorBoundObject = nullptr;
                m_contentInspectorBoundType = {};
                m_contentInspectorBoundComponentSignature = 0;
            }
        }
    }

    if (m_contentInspectorImportSettingsTitleText.Id.Value != 0)
    {
        if (auto* ImportTitle = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(m_contentInspectorImportSettingsTitleText.Id)))
        {
            ImportTitle->Text().Set(HasImportTarget
                                        ? std::string("Import Settings (Requires Reimport)")
                                        : std::string("Import Settings (Unavailable)"));
        }
    }

    if (m_contentInspectorImportSettingsPanel.Id.Value != 0)
    {
        if (auto* ImportPanel = dynamic_cast<UIPropertyPanel*>(&m_context->GetElement(m_contentInspectorImportSettingsPanel.Id)))
        {
            if (m_contentAssetInspectorState.Open && HasImportTarget)
            {
                if (!m_contentInspectorImportTargetBound ||
                    m_contentInspectorImportBoundObject != m_contentAssetInspectorState.ImportSettingsObject ||
                    m_contentInspectorImportBoundType != m_contentAssetInspectorState.ImportSettingsType)
                {
                    ImportPanel->ClearObject();
                    m_contentInspectorImportTargetBound = ImportPanel->BindObject(
                        m_contentAssetInspectorState.ImportSettingsType,
                        m_contentAssetInspectorState.ImportSettingsObject);
                    if (m_contentInspectorImportTargetBound)
                    {
                        m_contentInspectorImportBoundObject = m_contentAssetInspectorState.ImportSettingsObject;
                        m_contentInspectorImportBoundType = m_contentAssetInspectorState.ImportSettingsType;
                    }
                    else
                    {
                        m_contentInspectorImportBoundObject = nullptr;
                        m_contentInspectorImportBoundType = {};
                    }
                }
                else
                {
                    ImportPanel->RefreshFromModel();
                }
            }
            else if (m_contentInspectorImportTargetBound)
            {
                ImportPanel->ClearObject();
                m_contentInspectorImportTargetBound = false;
                m_contentInspectorImportBoundObject = nullptr;
                m_contentInspectorImportBoundType = {};
            }
        }
    }

    if (m_contentInspectorSaveButton.Id.Value != 0)
    {
        if (auto* SaveButton = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_contentInspectorSaveButton.Id)))
        {
            const bool CanSave = m_contentAssetInspectorState.Open &&
                                 (m_contentInspectorTargetBound || m_contentInspectorImportTargetBound) &&
                                 m_contentAssetInspectorState.CanSave &&
                                 m_contentAssetInspectorState.IsDirty;
            SaveButton->SetDisabled(!CanSave);
        }
    }

    if (m_contentInspectorReimportButton.Id.Value != 0)
    {
        if (auto* ReimportButton = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_contentInspectorReimportButton.Id)))
        {
            const bool CanReimport = m_contentAssetInspectorState.Open &&
                                     m_contentInspectorImportTargetBound &&
                                     m_contentAssetInspectorState.CanReimport;
            ReimportButton->SetDisabled(!CanReimport);
        }
    }
}

void EditorLayout::RefreshContentAssetCardSelectionStyles()
{
    if (!m_context)
    {
        return;
    }

    const std::size_t SelectedIndex = ResolveSelectedContentAssetIndex();
    for (std::size_t CardIndex = 0; CardIndex < m_contentAssetCardButtons.size(); ++CardIndex)
    {
        const auto& CardHandle = m_contentAssetCardButtons[CardIndex];
        if (CardHandle.Id.Value == 0 || CardIndex >= m_contentBrowserEntries.size())
        {
            continue;
        }

        auto* Button = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(CardHandle.Id));
        if (!Button)
        {
            continue;
        }

        const ContentBrowserEntry& Entry = m_contentBrowserEntries[CardIndex];
        const bool IsFolderSelected = Entry.IsFolder &&
                                      !m_selectedContentFolderPath.empty() &&
                                      Entry.FolderPath == m_selectedContentFolderPath;
        const bool IsAssetSelected = !Entry.IsFolder &&
                                     SelectedIndex != std::numeric_limits<std::size_t>::max() &&
                                     Entry.AssetIndex == SelectedIndex;
        const bool IsSelected = IsFolderSelected || IsAssetSelected;
        Button->ElementStyle().InitFrom<SnAPI::UI::UIButton>();
        Button->ElementStyle().Apply("editor.asset_tile_button");
        if (IsSelected)
        {
            Button->ElementStyle().Apply("editor.asset_tile_button_selected");
        }
    }
}

void EditorLayout::RefreshContentAssetDetailsViewModel()
{
    const std::size_t SelectedIndex = ResolveSelectedContentAssetIndex();
    const ContentAssetEntry* Selected = (SelectedIndex < m_contentAssets.size()) ? &m_contentAssets[SelectedIndex] : nullptr;

    const std::string NameText = !m_contentAssetDetails.Name.empty()
                                     ? m_contentAssetDetails.Name
                                     : (Selected ? Selected->Name : std::string("--"));
    const std::string TypeText = !m_contentAssetDetails.Type.empty()
                                     ? m_contentAssetDetails.Type
                                     : (Selected ? Selected->Type : std::string("--"));
    const std::string VariantText = !m_contentAssetDetails.Variant.empty()
                                        ? m_contentAssetDetails.Variant
                                        : (Selected ? (Selected->Variant.empty() ? std::string("default") : Selected->Variant)
                                                    : std::string("--"));
    const std::string IdText = !m_contentAssetDetails.AssetId.empty()
                                   ? m_contentAssetDetails.AssetId
                                   : (Selected ? Selected->Key : std::string("--"));
    const std::string StatusText = !m_contentAssetDetails.Status.empty()
                                       ? m_contentAssetDetails.Status
                                       : (Selected ? std::string("Selected") : std::string("No asset selected"));

    ViewModelProperty<std::string>(kVmContentAssetNameKey).Set(NameText);
    ViewModelProperty<std::string>(kVmContentAssetTypeKey).Set(TypeText);
    ViewModelProperty<std::string>(kVmContentAssetVariantKey).Set(VariantText);
    ViewModelProperty<std::string>(kVmContentAssetIdKey).Set(IdText);
    ViewModelProperty<std::string>(kVmContentAssetStatusKey).Set(StatusText);

    const bool HasSelection = (Selected != nullptr);
    ViewModelProperty<bool>(kVmContentAssetCanPlaceKey).Set(HasSelection && m_contentAssetDetails.CanPlace);
    ViewModelProperty<bool>(kVmContentAssetCanSaveKey).Set(HasSelection && m_contentAssetDetails.CanSave);

    if (HasSelection && m_contentAssetDetails.Status.empty())
    {
        std::string DerivedStatus = "Selected";
        if (m_contentAssetDetails.IsRuntime)
        {
            DerivedStatus = "Runtime asset";
        }
        if (m_contentAssetDetails.IsDirty)
        {
            DerivedStatus += m_contentAssetDetails.IsRuntime ? " (unsaved)" : " (modified, save to persist)";
        }
        ViewModelProperty<std::string>(kVmContentAssetStatusKey).Set(DerivedStatus);
    }
}

std::size_t EditorLayout::ResolveSelectedContentAssetIndex() const
{
    if (m_selectedContentAssetKey.empty())
    {
        return std::numeric_limits<std::size_t>::max();
    }

    for (std::size_t AssetIndex = 0; AssetIndex < m_contentAssets.size(); ++AssetIndex)
    {
        if (m_contentAssets[AssetIndex].Key == m_selectedContentAssetKey)
        {
            return AssetIndex;
        }
    }

    return std::numeric_limits<std::size_t>::max();
}

bool EditorLayout::QueryInvalidationDebugOverlayEnabled() const
{
#if !defined(SNAPI_GF_ENABLE_UI)
    return false;
#else
    if (!m_runtime)
    {
        return false;
    }

    auto* WorldPtr = m_runtime->WorldPtr();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        return false;
    }

    auto& UI = WorldPtr->UI();
    const std::uint64_t RootContextId = UI.RootContextId();
    if (RootContextId == 0)
    {
        return false;
    }

    const auto* RootContext = UI.Context(RootContextId);
    return RootContext != nullptr && RootContext->IsInvalidationDebugOverlayEnabled();
#endif
}

void EditorLayout::SetInvalidationDebugOverlayEnabled(const bool Enabled)
{
    ViewModelProperty<bool>(kVmInvalidationDebugEnabledKey).Set(Enabled);
}

void EditorLayout::ToggleInvalidationDebugOverlay()
{
    const bool Enabled = ViewModelProperty<bool>(kVmInvalidationDebugEnabledKey).Get();
    SetInvalidationDebugOverlayEnabled(!Enabled);
}

void EditorLayout::SyncInvalidationDebugOverlay()
{
#if !defined(SNAPI_GF_ENABLE_UI)
    return;
#else
    const bool RuntimeState = QueryInvalidationDebugOverlayEnabled();
    if (RuntimeState != m_invalidationDebugOverlayEnabled)
    {
        m_invalidationDebugOverlayEnabled = RuntimeState;
        PublishInvalidationDebugState();
    }

    if (!m_runtime)
    {
        return;
    }

    auto* WorldPtr = m_runtime->WorldPtr();
    if (!WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        return;
    }

    auto& UI = WorldPtr->UI();
    const auto ContextIds = UI.ContextIds();
    for (const std::uint64_t ContextId : ContextIds)
    {
        if (auto* Context = UI.Context(ContextId))
        {
            if (Context->IsInvalidationDebugOverlayEnabled() != m_invalidationDebugOverlayEnabled)
            {
                Context->SetInvalidationDebugOverlayEnabled(m_invalidationDebugOverlayEnabled);
            }
        }
    }
#endif
}

void EditorLayout::PublishInvalidationDebugState()
{
    ViewModelProperty<bool>(kVmInvalidationDebugEnabledKey).Set(m_invalidationDebugOverlayEnabled);
}

BaseNode* EditorLayout::ResolveSelectedNode(GameRuntime& Runtime, ComponentHandle& ActiveCamera) const
{
    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return nullptr;
    }

    if (m_selection)
    {
        if (auto* SelectedNode = m_selection->ResolveSelectedNode(*WorldPtr))
        {
            return SelectedNode;
        }
    }

    if (auto* ActiveCameraComponent = ResolveActiveCameraComponent(Runtime, ActiveCamera);
        ActiveCameraComponent && !ActiveCameraComponent->Owner().IsNull())
    {
        if (auto* CameraNode = ActiveCameraComponent->OwnerNode())
        {
            return CameraNode;
        }
    }

    return nullptr;
}

void EditorLayout::BuildGamePane(PanelBuilder& Workspace, GameRuntime& Runtime, ComponentHandle& ActiveCamera)
{
    auto GamePane = Workspace.Add(SnAPI::UI::UIPanel("Editor.GamePane"));
    auto& GamePaneElement = GamePane.Element();
    GamePaneElement.ElementStyle().Apply("editor.center");
    GamePaneElement.Width().Set(SnAPI::UI::Sizing::Fill());
    GamePaneElement.Height().Set(SnAPI::UI::Sizing::Fill());
    GamePaneElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto ViewTabs = GamePane.Add(SnAPI::UI::UITabs{});
    auto& ViewTabsElement = ViewTabs.Element();
    ViewTabsElement.ElementStyle().Apply("editor.viewport_tabs");
    ViewTabsElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ViewTabsElement.Height().Set(SnAPI::UI::Sizing::Fill());
    ViewTabsElement.HeaderHeight().Set(30.0f);
    m_gameViewTabs = ViewTabs.Handle();

    auto GameViewTab = ViewTabs.Add(SnAPI::UI::UIPanel("Editor.GameViewTab"));
    auto& GameViewTabPanel = GameViewTab.Element();
    GameViewTabPanel.ElementStyle().Apply("editor.section_card");
    GameViewTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    GameViewTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    GameViewTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    GameViewTabPanel.Padding().Set(4.0f);
    GameViewTabPanel.Gap().Set(4.0f);

    auto Header = GameViewTab.Add(SnAPI::UI::UIPanel("Editor.GameHeader"));
    auto& HeaderPanel = Header.Element();
    HeaderPanel.ElementStyle().Apply("editor.toolbar");
    HeaderPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto HeaderIcon = Header.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kGameViewIconPath)));
    auto& HeaderIconImage = HeaderIcon.Element();
    ConfigureSvgIcon(
        HeaderIconImage,
        14.0f,
        kIconWhite,
        SnAPI::UI::Margin{0.0f, 0.0f, 6.0f, 0.0f});

    auto Breadcrumbs = Header.Add(SnAPI::UI::UIBreadcrumbs{});
    auto& BreadcrumbsElement = Breadcrumbs.Element();
    BreadcrumbsElement.ElementStyle().Apply("editor.viewport_breadcrumb");
    BreadcrumbsElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    BreadcrumbsElement.SetCrumbs({"Game View", "Perspective", "Lit"});

    auto GizmoControls = Header.Add(SnAPI::UI::UIPanel("Editor.GameHeader.GizmoControls"));
    auto& GizmoControlsPanel = GizmoControls.Element();
    ConfigureTransparentLayoutPanel(GizmoControlsPanel);
    GizmoControlsPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    GizmoControlsPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    GizmoControlsPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    GizmoControlsPanel.Gap().Set(6.0f);

    auto SpaceLabel = GizmoControls.Add(SnAPI::UI::UIText("Space"));
    auto& SpaceLabelText = SpaceLabel.Element();
    SpaceLabelText.ElementStyle().Apply("editor.menu_item");
    SpaceLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto SpaceCombo = GizmoControls.Add(SnAPI::UI::UIComboBox{});
    auto& SpaceComboElement = SpaceCombo.Element();
    SpaceComboElement.Width().Set(SnAPI::UI::Sizing::Fixed(120.0f));
    SpaceComboElement.Height().Set(SnAPI::UI::Sizing::Auto());
    SpaceComboElement.Placeholder().Set(std::string("Space"));
    SpaceComboElement.SetItems({"World", "Object", "Camera"});
    (void)SpaceComboElement.SetSelectedIndex(GizmoSpaceToIndex(m_gizmoSpace), false);
    SpaceComboElement.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        m_gizmoSpace = GizmoSpaceFromIndex(Index);
    });

    auto SnapLabel = GizmoControls.Add(SnAPI::UI::UIText("Snap"));
    auto& SnapLabelText = SnapLabel.Element();
    SnapLabelText.ElementStyle().Apply("editor.menu_item");
    SnapLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto SnapCombo = GizmoControls.Add(SnAPI::UI::UIComboBox{});
    auto& SnapComboElement = SnapCombo.Element();
    SnapComboElement.Width().Set(SnAPI::UI::Sizing::Fixed(118.0f));
    SnapComboElement.Height().Set(SnAPI::UI::Sizing::Auto());
    SnapComboElement.Placeholder().Set(std::string("Snap"));
    SnapComboElement.SetItems({"Off", "On"});
    (void)SnapComboElement.SetSelectedIndex(SnapModeToIndex(m_snapMode), false);
    SnapComboElement.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        m_snapMode = SnapModeFromIndex(Index);
    });

    auto Viewport = GameViewTab.Add(UIRenderViewport{});
    auto& ViewportElement = Viewport.Element();
    ViewportElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ViewportElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ViewportElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    ViewportElement.ViewportName().Set(std::string("Editor.GameViewport"));
    ViewportElement.PassGraphPreset().Set(ERenderViewportPassGraphPreset::EditorWorld);
    ViewportElement.AutoRegisterPassGraph().Set(true);
    ViewportElement.RenderScale().Set(1.0f);
    ViewportElement.Enabled().Set(true);
    ViewportElement.SetGameRuntime(&Runtime);
    if (auto* ActiveCameraComponent = ResolveActiveCameraComponent(Runtime, ActiveCamera);
        ActiveCameraComponent && ActiveCameraComponent->Camera())
    {
        ViewportElement.SetViewportCamera(ActiveCameraComponent->CameraShared());
    }

    auto ProfilerTab = ViewTabs.Add(SnAPI::UI::UIPanel("Editor.GameProfilerTab"));
    auto& ProfilerTabPanel = ProfilerTab.Element();
    ProfilerTabPanel.ElementStyle().Apply("editor.section_card");
    ProfilerTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ProfilerTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ProfilerTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ProfilerTabPanel.Padding().Set(8.0f);
    ProfilerTabPanel.Gap().Set(6.0f);

    auto ProfilerHint = ProfilerTab.Add(SnAPI::UI::UIText("Profiler is rendered as a game-viewport overlay."));
    ProfilerHint.Element().ElementStyle().Apply("editor.menu_item");

    auto ConduitTab = ViewTabs.Add(SnAPI::UI::UIPanel("Editor.GameConduitTab"));
    auto& ConduitTabPanel = ConduitTab.Element();
    ConduitTabPanel.ElementStyle().Apply("editor.section_card");
    ConduitTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConduitTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ConduitTabPanel.Padding().Set(12.0f);
    ConduitTabPanel.Gap().Set(8.0f);

    auto ConduitTitle = ConduitTab.Add(SnAPI::UI::UIText("Conduit Workspace"));
    auto& ConduitTitleText = ConduitTitle.Element();
    ConduitTitleText.ElementStyle().Apply("editor.panel_title");
    ConduitTitleText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    m_conduitWorkspaceTitleText = ConduitTitle.Handle();

    auto ConduitStatus = ConduitTab.Add(SnAPI::UI::UIText("No Conduit graph open."));
    auto& ConduitStatusText = ConduitStatus.Element();
    ConduitStatusText.ElementStyle().Apply("editor.panel_subtitle");
    ConduitStatusText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    m_conduitWorkspaceStatusText = ConduitStatus.Handle();

    auto ConduitSummary = ConduitTab.Add(
        SnAPI::UI::UIText("Double-click a Conduit Graph asset in the Content Browser to open it here."));
    auto& ConduitSummaryText = ConduitSummary.Element();
    ConduitSummaryText.ElementStyle().Apply("editor.menu_item");
    ConduitSummaryText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    ConduitSummaryText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    m_conduitWorkspaceSummaryText = ConduitSummary.Handle();

    auto ConduitBody = ConduitTab.Add(SnAPI::UI::UIPanel("Editor.ConduitWorkspaceBody"));
    auto& ConduitBodyPanel = ConduitBody.Element();
    ConduitBodyPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConduitBodyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitBodyPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ConduitBodyPanel.Gap().Set(0.0f);
    ConduitBodyPanel.Padding().Set(0.0f);
    ConduitBodyPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ConduitBodyPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ConduitBodyPanel.BorderThickness().Set(0.0f);
    ConduitBodyPanel.CornerRadius().Set(0.0f);

    auto ConduitGraphWorkspaceHost = ConduitBody.Add(SnAPI::UI::UIPanel("Editor.ConduitGraphWorkspaceHost"));
    ConfigureHostPanel(ConduitGraphWorkspaceHost.Element());
    m_conduitGraphWorkspaceHost = ConduitGraphWorkspaceHost.Handle();

    auto ConduitGraphSplit = ConduitGraphWorkspaceHost.Add(SnAPI::UI::UIDockZone{});
    auto& ConduitGraphSplitElement = ConduitGraphSplit.Element();
    ConfigureConduitSplitZone(ConduitGraphSplitElement,
                              SnAPI::UI::EDockSplit::Horizontal,
                              kConduitSidebarSplitRatio,
                              250.0f,
                              420.0f);

    auto ConduitSidebarHost = ConduitGraphSplit.Add(SnAPI::UI::UIPanel("Editor.ConduitSidebarHost"));
    ConfigureHostPanel(ConduitSidebarHost.Element());

    auto ConduitSidebarSplit = ConduitSidebarHost.Add(SnAPI::UI::UIDockZone{});
    auto& ConduitSidebarSplitElement = ConduitSidebarSplit.Element();
    ConfigureConduitSplitZone(ConduitSidebarSplitElement,
                              SnAPI::UI::EDockSplit::Vertical,
                              kConduitSidebarUpperSplitRatio,
                              220.0f,
                              220.0f);

    auto ConduitVariablesHost = ConduitSidebarSplit.Add(SnAPI::UI::UIPanel("Editor.ConduitVariablesHost"));
    ConfigureHostPanel(ConduitVariablesHost.Element());

    auto ConduitVariablesCard = ConduitVariablesHost.Add(SnAPI::UI::UIPanel("Editor.ConduitVariablesCard"));
    auto& ConduitVariablesCardPanel = ConduitVariablesCard.Element();
    ConduitVariablesCardPanel.ElementStyle().Apply("editor.section_card");
    ConduitVariablesCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConduitVariablesCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitVariablesCardPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ConduitVariablesCardPanel.Padding().Set(8.0f);
    ConduitVariablesCardPanel.Gap().Set(6.0f);
    m_conduitVariablesCard = ConduitVariablesCard.Handle();

    auto ConduitVariablesTitle = ConduitVariablesCard.Add(SnAPI::UI::UIText("Variables"));
    ConduitVariablesTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto ConduitVariablesTreeBuilder = ConduitVariablesCard.Add(SnAPI::UI::UITreeView{});
    auto& ConduitVariablesTree = ConduitVariablesTreeBuilder.Element();
    ConduitVariablesTree.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitVariablesTree.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ConduitVariablesTree.OnSelectionChanged([this](const int32_t Index) {
        if (!m_onConduitVariableSelected || Index < 0 || static_cast<std::size_t>(Index) >= m_conduitVisibleVariableIds.size())
        {
            return;
        }
        m_onConduitVariableSelected(m_conduitVisibleVariableIds[static_cast<std::size_t>(Index)]);
    });
    m_conduitVariablesTree = ConduitVariablesTreeBuilder.Handle();

    auto CreateNameInputBuilder = ConduitVariablesCard.Add(SnAPI::UI::UITextInput{});
    auto& CreateNameInput = CreateNameInputBuilder.Element();
    CreateNameInput.Width().Set(SnAPI::UI::Sizing::Fill());
    CreateNameInput.Placeholder().Set(std::string("New variable name"));
    CreateNameInput.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Text) {
        m_conduitCreateVariableNameText = Text;
    }));
    CreateNameInput.OnSubmit(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Text) {
        m_conduitCreateVariableNameText = Text;
        if (m_onConduitVariableCreateRequested && !Text.empty() && m_conduitCreateSelectedVariableType != TypeId{})
        {
            m_onConduitVariableCreateRequested(Text, m_conduitCreateSelectedVariableType);
            m_conduitCreateVariableNameText.clear();
            RefreshConduitWorkspaceView();
        }
    }));
    m_conduitVariableCreateNameInput = CreateNameInputBuilder.Handle();

    auto CreateTypeComboBuilder = ConduitVariablesCard.Add(SnAPI::UI::UIComboBox{});
    auto& CreateTypeCombo = CreateTypeComboBuilder.Element();
    CreateTypeCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    CreateTypeCombo.Placeholder().Set(std::string("Variable type"));
    CreateTypeCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        if (Index < 0 || static_cast<std::size_t>(Index) >= m_conduitWorkspaceState.VariableTypeOptions.size())
        {
            m_conduitCreateSelectedVariableType = {};
            return;
        }
        m_conduitCreateSelectedVariableType = m_conduitWorkspaceState.VariableTypeOptions[static_cast<std::size_t>(Index)].Type;
    });
    m_conduitVariableCreateTypeCombo = CreateTypeComboBuilder.Handle();

    auto CreateButtonBuilder = ConduitVariablesCard.Add(SnAPI::UI::UIButton{});
    auto& CreateButton = CreateButtonBuilder.Element();
    CreateButton.ElementStyle().Apply("editor.toolbar_button");
    CreateButton.Width().Set(SnAPI::UI::Sizing::Fill());
    CreateButton.ElementPadding().Set(SnAPI::UI::Padding{6.0f, 4.0f, 6.0f, 4.0f});
    CreateButton.OnClick([this]() {
        if (m_onConduitVariableCreateRequested &&
            !m_conduitCreateVariableNameText.empty() &&
            m_conduitCreateSelectedVariableType != TypeId{})
        {
            m_onConduitVariableCreateRequested(m_conduitCreateVariableNameText, m_conduitCreateSelectedVariableType);
            m_conduitCreateVariableNameText.clear();
            RefreshConduitWorkspaceView();
        }
    });
    auto CreateButtonText = CreateButtonBuilder.Add(SnAPI::UI::UIText("Add Variable"));
    CreateButtonText.Element().ElementStyle().Apply("editor.menu_item");
    m_conduitVariableCreateButton = CreateButtonBuilder.Handle();

    auto ConduitPaletteHost = ConduitSidebarSplit.Add(SnAPI::UI::UIPanel("Editor.ConduitPaletteHost"));
    ConfigureHostPanel(ConduitPaletteHost.Element());

    auto ConduitPaletteCard = ConduitPaletteHost.Add(SnAPI::UI::UIPanel("Editor.ConduitPaletteCard"));
    auto& ConduitPaletteCardPanel = ConduitPaletteCard.Element();
    ConduitPaletteCardPanel.ElementStyle().Apply("editor.section_card");
    ConduitPaletteCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConduitPaletteCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitPaletteCardPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ConduitPaletteCardPanel.Padding().Set(8.0f);
    ConduitPaletteCardPanel.Gap().Set(6.0f);

    auto ConduitPaletteTitle = ConduitPaletteCard.Add(SnAPI::UI::UIText("Node Palette"));
    ConduitPaletteTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto ConduitPaletteSearchBuilder = ConduitPaletteCard.Add(SnAPI::UI::UITextInput{});
    auto& ConduitPaletteSearch = ConduitPaletteSearchBuilder.Element();
    ConduitPaletteSearch.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitPaletteSearch.Placeholder().Set(std::string("Search nodes"));
    ConduitPaletteSearch.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Text) {
        m_conduitPaletteFilterText = Text;
        RefreshConduitWorkspaceView();
    }));
    m_conduitPaletteSearchInput = ConduitPaletteSearchBuilder.Handle();

    auto ConduitPaletteTreeBuilder = ConduitPaletteCard.Add(SnAPI::UI::UITreeView{});
    auto& ConduitPaletteTree = ConduitPaletteTreeBuilder.Element();
    ConduitPaletteTree.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitPaletteTree.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ConduitPaletteTree.OnSelectionChanged([this](const int32_t Index) {
        if (Index < 0 || static_cast<std::size_t>(Index) >= m_conduitVisiblePaletteStableIds.size())
        {
            m_conduitSelectedPaletteStableId.clear();
            return;
        }
        m_conduitSelectedPaletteStableId = m_conduitVisiblePaletteStableIds[static_cast<std::size_t>(Index)];
    });
    m_conduitPaletteTree = ConduitPaletteTreeBuilder.Handle();

    auto PaletteAddButtonBuilder = ConduitPaletteCard.Add(SnAPI::UI::UIButton{});
    auto& PaletteAddButton = PaletteAddButtonBuilder.Element();
    PaletteAddButton.ElementStyle().Apply("editor.toolbar_button");
    PaletteAddButton.Width().Set(SnAPI::UI::Sizing::Fill());
    PaletteAddButton.ElementPadding().Set(SnAPI::UI::Padding{6.0f, 4.0f, 6.0f, 4.0f});
    PaletteAddButton.OnClick([this]() {
        if (m_onConduitNodeCreateRequested && !m_conduitSelectedPaletteStableId.empty())
        {
            m_onConduitNodeCreateRequested(m_conduitSelectedPaletteStableId);
        }
    });
    auto PaletteAddButtonText = PaletteAddButtonBuilder.Add(SnAPI::UI::UIText("Add Selected Node"));
    PaletteAddButtonText.Element().ElementStyle().Apply("editor.menu_item");
    m_conduitPaletteAddNodeButton = PaletteAddButtonBuilder.Handle();

    auto ConduitGraphMainHost = ConduitGraphSplit.Add(SnAPI::UI::UIPanel("Editor.ConduitGraphMainHost"));
    ConfigureHostPanel(ConduitGraphMainHost.Element());

    auto ConduitGraphContentSplit = ConduitGraphMainHost.Add(SnAPI::UI::UIDockZone{});
    auto& ConduitGraphContentSplitElement = ConduitGraphContentSplit.Element();
    ConfigureConduitSplitZone(ConduitGraphContentSplitElement,
                              SnAPI::UI::EDockSplit::Horizontal,
                              kConduitCanvasSplitRatio,
                              380.0f,
                              280.0f);

    auto ConduitCanvasHost = ConduitGraphContentSplit.Add(SnAPI::UI::UIPanel("Editor.ConduitCanvasHost"));
    ConfigureHostPanel(ConduitCanvasHost.Element());

    auto ConduitNodesCard = ConduitCanvasHost.Add(SnAPI::UI::UIPanel("Editor.ConduitNodesCard"));
    auto& ConduitNodesCardPanel = ConduitNodesCard.Element();
    ConduitNodesCardPanel.ElementStyle().Apply("editor.section_card");
    ConduitNodesCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConduitNodesCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitNodesCardPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ConduitNodesCardPanel.Padding().Set(8.0f);
    ConduitNodesCardPanel.Gap().Set(6.0f);
    m_conduitNodesCard = ConduitNodesCard.Handle();

    auto ConduitNodesTitle = ConduitNodesCard.Add(SnAPI::UI::UIText("Graph Canvas"));
    ConduitNodesTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto ConduitNodesSummary = ConduitNodesCard.Add(
        SnAPI::UI::UIText("Left drag nodes. Right or middle drag pans. Mouse wheel zooms. Palette nodes spawn into this authored graph."));
    auto& ConduitNodesSummaryText = ConduitNodesSummary.Element();
    ConduitNodesSummaryText.ElementStyle().Apply("editor.panel_subtitle");
    ConduitNodesSummaryText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto ConduitCanvasBuilder = ConduitNodesCard.Add(::SnAPI::GameFramework::Conduit::Editor::UIConduitGraphCanvas{});
    auto& ConduitCanvas = ConduitCanvasBuilder.Element();
    ConduitCanvas.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitCanvas.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ConduitCanvas.SetNodeSelectionHandler(SnAPI::UI::TDelegate<void(const Uuid&)>::Bind([this](const Uuid& NodeId) {
        if (m_onConduitNodeSelected)
        {
            m_onConduitNodeSelected(NodeId);
        }
    }));
    ConduitCanvas.SetNodeMovedHandler(SnAPI::UI::TDelegate<void(const Uuid&, float, float)>::Bind(
        [this](const Uuid& NodeId, const float X, const float Y) {
            if (m_onConduitNodeMoveRequested)
            {
                m_onConduitNodeMoveRequested(NodeId, X, Y);
            }
        }));
    ConduitCanvas.SetViewportChangedHandler(SnAPI::UI::TDelegate<void(float, float, float)>::Bind(
        [this](const float PanX, const float PanY, const float Zoom) {
            if (m_onConduitViewportRequested)
            {
                m_onConduitViewportRequested(PanX, PanY, Zoom);
            }
        }));
    m_conduitGraphCanvas = ConduitCanvasBuilder.Handle();

    auto ConduitNodeFooter = ConduitNodesCard.Add(SnAPI::UI::UIPanel("Editor.ConduitCanvasFooter"));
    auto& ConduitNodeFooterPanel = ConduitNodeFooter.Element();
    ConduitNodeFooterPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ConduitNodeFooterPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitNodeFooterPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ConduitNodeFooterPanel.Gap().Set(6.0f);
    ConduitNodeFooterPanel.Padding().Set(0.0f);
    ConduitNodeFooterPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ConduitNodeFooterPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ConduitNodeFooterPanel.BorderThickness().Set(0.0f);
    ConduitNodeFooterPanel.CornerRadius().Set(0.0f);

    auto ConduitNodeHint = ConduitNodeFooter.Add(SnAPI::UI::UIText("Node removal currently acts on the selected canvas node."));
    auto& ConduitNodeHintText = ConduitNodeHint.Element();
    ConduitNodeHintText.ElementStyle().Apply("editor.menu_item");
    ConduitNodeHintText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    ConduitNodeHintText.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto ConduitNodeRemoveButtonBuilder = ConduitNodeFooter.Add(SnAPI::UI::UIButton{});
    auto& ConduitNodeRemoveButton = ConduitNodeRemoveButtonBuilder.Element();
    ConduitNodeRemoveButton.ElementStyle().Apply("editor.toolbar_button");
    ConduitNodeRemoveButton.Width().Set(SnAPI::UI::Sizing::Auto());
    ConduitNodeRemoveButton.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    ConduitNodeRemoveButton.OnClick([this]() {
        if (m_onConduitNodeRemoveRequested)
        {
            m_onConduitNodeRemoveRequested();
        }
    });
    auto ConduitNodeRemoveLabel = ConduitNodeRemoveButtonBuilder.Add(SnAPI::UI::UIText("Remove Selected Node"));
    ConduitNodeRemoveLabel.Element().ElementStyle().Apply("editor.menu_item");
    m_conduitNodeRemoveButton = ConduitNodeRemoveButtonBuilder.Handle();

    auto ConduitDetailsHost = ConduitGraphContentSplit.Add(SnAPI::UI::UIPanel("Editor.ConduitDetailsHost"));
    ConfigureHostPanel(ConduitDetailsHost.Element());

    auto ConduitInspectorCard = ConduitDetailsHost.Add(SnAPI::UI::UIPanel("Editor.ConduitVariableInspectorCard"));
    auto& ConduitInspectorCardPanel = ConduitInspectorCard.Element();
    ConduitInspectorCardPanel.ElementStyle().Apply("editor.section_card");
    ConduitInspectorCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConduitInspectorCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitInspectorCardPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ConduitInspectorCardPanel.Padding().Set(8.0f);
    ConduitInspectorCardPanel.Gap().Set(6.0f);
    m_conduitInspectorCard = ConduitInspectorCard.Handle();

    auto ConduitInspectorTitle = ConduitInspectorCard.Add(SnAPI::UI::UIText("Selection Inspector"));
    ConduitInspectorTitle.Element().ElementStyle().Apply("editor.panel_title");
    m_conduitInspectorTitleText = ConduitInspectorTitle.Handle();

    auto ConduitInspectorScroll = ConduitInspectorCard.Add(SnAPI::UI::UIScrollContainer{});
    auto& ConduitInspectorScrollElement = ConduitInspectorScroll.Element();
    ConduitInspectorScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitInspectorScrollElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ConduitInspectorScrollElement.Padding().Set(0.0f);
    ConduitInspectorScrollElement.Gap().Set(10.0f);
    ConduitInspectorScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);

    auto VariableInspectorPanelBuilder = ConduitInspectorScroll.Add(SnAPI::UI::UIPanel("Editor.ConduitVariableInspectorPanel"));
    auto& VariableInspectorPanel = VariableInspectorPanelBuilder.Element();
    VariableInspectorPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    VariableInspectorPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    VariableInspectorPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    VariableInspectorPanel.Gap().Set(6.0f);
    VariableInspectorPanel.Background().Set(SnAPI::UI::Color::Transparent());
    VariableInspectorPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    VariableInspectorPanel.BorderThickness().Set(0.0f);
    VariableInspectorPanel.CornerRadius().Set(0.0f);
    m_conduitVariableInspectorPanel = VariableInspectorPanelBuilder.Handle();

    auto VariableNameInputBuilder = VariableInspectorPanelBuilder.Add(SnAPI::UI::UITextInput{});
    auto& VariableNameInput = VariableNameInputBuilder.Element();
    VariableNameInput.Width().Set(SnAPI::UI::Sizing::Fill());
    VariableNameInput.Placeholder().Set(std::string("Variable name"));
    VariableNameInput.OnSubmit(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Text) {
        if (m_onConduitVariableRenameRequested)
        {
            m_onConduitVariableRenameRequested(Text);
        }
    }));
    VariableNameInput.OnFocusStateChanged(SnAPI::UI::TDelegate<void(bool)>::Bind([this, Handle = VariableNameInputBuilder.Handle()](const bool Focused) {
        if (Focused || !m_onConduitVariableRenameRequested || !m_context)
        {
            return;
        }
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(Handle.Id)))
        {
            m_onConduitVariableRenameRequested(Input->Text().Get());
        }
    }));
    m_conduitVariableNameInput = VariableNameInputBuilder.Handle();

    auto VariableTypeComboBuilder = VariableInspectorPanelBuilder.Add(SnAPI::UI::UIComboBox{});
    auto& VariableTypeCombo = VariableTypeComboBuilder.Element();
    VariableTypeCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    VariableTypeCombo.Placeholder().Set(std::string("Selected variable type"));
    VariableTypeCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        if (!m_onConduitVariableTypeRequested ||
            Index < 0 ||
            static_cast<std::size_t>(Index) >= m_conduitWorkspaceState.VariableTypeOptions.size())
        {
            return;
        }
        m_onConduitVariableTypeRequested(m_conduitWorkspaceState.VariableTypeOptions[static_cast<std::size_t>(Index)].Type);
    });
    m_conduitVariableTypeCombo = VariableTypeComboBuilder.Handle();

    auto RemoveButtonBuilder = VariableInspectorPanelBuilder.Add(SnAPI::UI::UIButton{});
    auto& RemoveButton = RemoveButtonBuilder.Element();
    RemoveButton.ElementStyle().Apply("editor.toolbar_button");
    RemoveButton.Width().Set(SnAPI::UI::Sizing::Auto());
    RemoveButton.ElementPadding().Set(SnAPI::UI::Padding{6.0f, 4.0f, 6.0f, 4.0f});
    RemoveButton.OnClick([this]() {
        if (m_onConduitVariableRemoveRequested)
        {
            m_onConduitVariableRemoveRequested();
        }
    });
    auto RemoveButtonText = RemoveButtonBuilder.Add(SnAPI::UI::UIText("Remove Variable"));
    RemoveButtonText.Element().ElementStyle().Apply("editor.menu_item");
    m_conduitVariableRemoveButton = RemoveButtonBuilder.Handle();

    auto DefaultHint = VariableInspectorPanelBuilder.Add(SnAPI::UI::UIText("Select a graph variable to edit its default value."));
    auto& DefaultHintText = DefaultHint.Element();
    DefaultHintText.ElementStyle().Apply("editor.panel_subtitle");
    DefaultHintText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_conduitVariableDefaultHintText = DefaultHint.Handle();

    auto DefaultBoolBuilder = VariableInspectorPanelBuilder.Add(SnAPI::UI::UICheckbox("Bool Default"));
    auto& DefaultBool = DefaultBoolBuilder.Element();
    DefaultBool.OnChanged([this](const bool Checked) {
        if (m_onConduitVariableDefaultBoolRequested)
        {
            m_onConduitVariableDefaultBoolRequested(Checked);
        }
    });
    m_conduitVariableDefaultBoolCheckbox = DefaultBoolBuilder.Handle();

    auto DefaultTextBuilder = VariableInspectorPanelBuilder.Add(SnAPI::UI::UITextInput{});
    auto& DefaultText = DefaultTextBuilder.Element();
    DefaultText.Width().Set(SnAPI::UI::Sizing::Fill());
    DefaultText.Placeholder().Set(std::string("Press Enter to apply text/numeric default"));
    DefaultText.OnSubmit(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Text) {
        if (m_onConduitVariableDefaultTextRequested)
        {
            m_onConduitVariableDefaultTextRequested(Text);
        }
    }));
    DefaultText.OnFocusStateChanged(SnAPI::UI::TDelegate<void(bool)>::Bind([this, Handle = DefaultTextBuilder.Handle()](const bool Focused) {
        if (Focused || !m_onConduitVariableDefaultTextRequested || !m_context)
        {
            return;
        }
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(Handle.Id)))
        {
            m_onConduitVariableDefaultTextRequested(Input->Text().Get());
        }
    }));
    m_conduitVariableDefaultTextInput = DefaultTextBuilder.Handle();

    auto DefaultEnumComboBuilder = VariableInspectorPanelBuilder.Add(SnAPI::UI::UIComboBox{});
    auto& DefaultEnumCombo = DefaultEnumComboBuilder.Element();
    DefaultEnumCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    DefaultEnumCombo.Placeholder().Set(std::string("Enum default"));
    DefaultEnumCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        if (m_onConduitVariableDefaultEnumRequested && Index >= 0)
        {
            m_onConduitVariableDefaultEnumRequested(Text);
        }
    });
    m_conduitVariableDefaultEnumCombo = DefaultEnumComboBuilder.Handle();

    auto DefaultButtonsRow = VariableInspectorPanelBuilder.Add(SnAPI::UI::UIPanel("Editor.ConduitDefaultButtonsRow"));
    auto& DefaultButtonsRowPanel = DefaultButtonsRow.Element();
    DefaultButtonsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    DefaultButtonsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    DefaultButtonsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    DefaultButtonsRowPanel.Gap().Set(6.0f);
    DefaultButtonsRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
    DefaultButtonsRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    DefaultButtonsRowPanel.BorderThickness().Set(0.0f);
    DefaultButtonsRowPanel.CornerRadius().Set(0.0f);

    auto ClearDefaultButtonBuilder = DefaultButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& ClearDefaultButton = ClearDefaultButtonBuilder.Element();
    ClearDefaultButton.ElementStyle().Apply("editor.toolbar_button");
    ClearDefaultButton.Width().Set(SnAPI::UI::Sizing::Auto());
    ClearDefaultButton.ElementPadding().Set(SnAPI::UI::Padding{6.0f, 4.0f, 6.0f, 4.0f});
    ClearDefaultButton.OnClick([this]() {
        if (m_onConduitVariableClearDefaultRequested)
        {
            m_onConduitVariableClearDefaultRequested();
        }
    });
    auto ClearDefaultLabel = ClearDefaultButtonBuilder.Add(SnAPI::UI::UIText("Clear Default"));
    ClearDefaultLabel.Element().ElementStyle().Apply("editor.menu_item");
    m_conduitVariableDefaultClearButton = ClearDefaultButtonBuilder.Handle();

    auto ApplyDefaultButtonBuilder = DefaultButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& ApplyDefaultButton = ApplyDefaultButtonBuilder.Element();
    ApplyDefaultButton.ElementStyle().Apply("editor.toolbar_button");
    ApplyDefaultButton.Width().Set(SnAPI::UI::Sizing::Auto());
    ApplyDefaultButton.ElementPadding().Set(SnAPI::UI::Padding{6.0f, 4.0f, 6.0f, 4.0f});
    ApplyDefaultButton.OnClick([this]() {
        if (m_onConduitVariableCommitDefaultRequested)
        {
            m_onConduitVariableCommitDefaultRequested();
        }
    });
    auto ApplyDefaultLabel = ApplyDefaultButtonBuilder.Add(SnAPI::UI::UIText("Apply Default"));
    ApplyDefaultLabel.Element().ElementStyle().Apply("editor.menu_item");
    m_conduitVariableDefaultApplyButton = ApplyDefaultButtonBuilder.Handle();

    auto ResetDefaultButtonBuilder = DefaultButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& ResetDefaultButton = ResetDefaultButtonBuilder.Element();
    ResetDefaultButton.ElementStyle().Apply("editor.toolbar_button");
    ResetDefaultButton.Width().Set(SnAPI::UI::Sizing::Auto());
    ResetDefaultButton.ElementPadding().Set(SnAPI::UI::Padding{6.0f, 4.0f, 6.0f, 4.0f});
    ResetDefaultButton.OnClick([this]() {
        if (m_onConduitVariableResetDefaultRequested)
        {
            m_onConduitVariableResetDefaultRequested();
        }
    });
    auto ResetDefaultLabel = ResetDefaultButtonBuilder.Add(SnAPI::UI::UIText("Reset Scratch"));
    ResetDefaultLabel.Element().ElementStyle().Apply("editor.menu_item");
    m_conduitVariableDefaultResetButton = ResetDefaultButtonBuilder.Handle();

    auto DefaultPropertyPanelBuilder = VariableInspectorPanelBuilder.Add(UIPropertyPanel{});
    auto& DefaultPropertyPanel = DefaultPropertyPanelBuilder.Element();
    DefaultPropertyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    DefaultPropertyPanel.Height().Set(SnAPI::UI::Sizing::Fixed(320.0f));
    m_conduitVariableDefaultPropertyPanel = DefaultPropertyPanelBuilder.Handle();

    auto NodeInspectorPanelBuilder = ConduitInspectorScroll.Add(SnAPI::UI::UIPanel("Editor.ConduitNodeInspectorPanel"));
    auto& NodeInspectorPanel = NodeInspectorPanelBuilder.Element();
    NodeInspectorPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    NodeInspectorPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    NodeInspectorPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    NodeInspectorPanel.Gap().Set(6.0f);
    NodeInspectorPanel.Background().Set(SnAPI::UI::Color::Transparent());
    NodeInspectorPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    NodeInspectorPanel.BorderThickness().Set(0.0f);
    NodeInspectorPanel.CornerRadius().Set(0.0f);
    m_conduitNodeInspectorPanel = NodeInspectorPanelBuilder.Handle();

    auto NodeSummaryBuilder = NodeInspectorPanelBuilder.Add(
        SnAPI::UI::UIText("Select an authored Conduit node to edit entry names and control-flow labels."));
    auto& NodeSummaryText = NodeSummaryBuilder.Element();
    NodeSummaryText.ElementStyle().Apply("editor.panel_subtitle");
    NodeSummaryText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_conduitNodeSummaryText = NodeSummaryBuilder.Handle();

    auto NodePrimaryLabelBuilder = NodeInspectorPanelBuilder.Add(SnAPI::UI::UIText("Primary"));
    NodePrimaryLabelBuilder.Element().ElementStyle().Apply("editor.panel_subtitle");
    m_conduitNodePrimaryLabelText = NodePrimaryLabelBuilder.Handle();

    auto NodePrimaryInputBuilder = NodeInspectorPanelBuilder.Add(SnAPI::UI::UITextInput{});
    auto& NodePrimaryInput = NodePrimaryInputBuilder.Element();
    NodePrimaryInput.Width().Set(SnAPI::UI::Sizing::Fill());
    NodePrimaryInput.Placeholder().Set(std::string("Press Enter to apply"));
    NodePrimaryInput.OnSubmit(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Text) {
        if (m_onConduitNodePrimaryTextRequested)
        {
            m_onConduitNodePrimaryTextRequested(Text);
        }
    }));
    NodePrimaryInput.OnFocusStateChanged(SnAPI::UI::TDelegate<void(bool)>::Bind([this, Handle = NodePrimaryInputBuilder.Handle()](const bool Focused) {
        if (Focused || !m_onConduitNodePrimaryTextRequested || !m_context)
        {
            return;
        }
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(Handle.Id)))
        {
            m_onConduitNodePrimaryTextRequested(Input->Text().Get());
        }
    }));
    m_conduitNodePrimaryTextInput = NodePrimaryInputBuilder.Handle();

    auto NodeSecondaryLabelBuilder = NodeInspectorPanelBuilder.Add(SnAPI::UI::UIText("Secondary"));
    NodeSecondaryLabelBuilder.Element().ElementStyle().Apply("editor.panel_subtitle");
    m_conduitNodeSecondaryLabelText = NodeSecondaryLabelBuilder.Handle();

    auto NodeSecondaryInputBuilder = NodeInspectorPanelBuilder.Add(SnAPI::UI::UITextInput{});
    auto& NodeSecondaryInput = NodeSecondaryInputBuilder.Element();
    NodeSecondaryInput.Width().Set(SnAPI::UI::Sizing::Fill());
    NodeSecondaryInput.Placeholder().Set(std::string("Press Enter to apply"));
    NodeSecondaryInput.OnSubmit(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Text) {
        if (m_onConduitNodeSecondaryTextRequested)
        {
            m_onConduitNodeSecondaryTextRequested(Text);
        }
    }));
    NodeSecondaryInput.OnFocusStateChanged(SnAPI::UI::TDelegate<void(bool)>::Bind([this, Handle = NodeSecondaryInputBuilder.Handle()](const bool Focused) {
        if (Focused || !m_onConduitNodeSecondaryTextRequested || !m_context)
        {
            return;
        }
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(Handle.Id)))
        {
            m_onConduitNodeSecondaryTextRequested(Input->Text().Get());
        }
    }));
    m_conduitNodeSecondaryTextInput = NodeSecondaryInputBuilder.Handle();

    auto ConduitClassWorkspaceHost = ConduitBody.Add(SnAPI::UI::UIPanel("Editor.ConduitClassWorkspaceHost"));
    ConfigureHostPanel(ConduitClassWorkspaceHost.Element());
    m_conduitClassWorkspaceHost = ConduitClassWorkspaceHost.Handle();

    auto ConduitClassSplit = ConduitClassWorkspaceHost.Add(SnAPI::UI::UIDockZone{});
    auto& ConduitClassSplitElement = ConduitClassSplit.Element();
    ConfigureConduitSplitZone(ConduitClassSplitElement,
                              SnAPI::UI::EDockSplit::Horizontal,
                              kConduitClassSplitRatio,
                              320.0f,
                              260.0f);

    auto ConduitClassSettingsHost = ConduitClassSplit.Add(SnAPI::UI::UIPanel("Editor.ConduitClassSettingsHost"));
    ConfigureHostPanel(ConduitClassSettingsHost.Element());

    auto ConduitClassCard = ConduitClassSettingsHost.Add(SnAPI::UI::UIPanel("Editor.ConduitClassCard"));
    auto& ConduitClassCardPanel = ConduitClassCard.Element();
    ConduitClassCardPanel.ElementStyle().Apply("editor.section_card");
    ConduitClassCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConduitClassCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitClassCardPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ConduitClassCardPanel.Padding().Set(8.0f);
    ConduitClassCardPanel.Gap().Set(6.0f);
    m_conduitClassCard = ConduitClassCard.Handle();

    auto ConduitClassTitle = ConduitClassCard.Add(SnAPI::UI::UIText("Class Settings"));
    ConduitClassTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto ConduitClassScroll = ConduitClassCard.Add(SnAPI::UI::UIScrollContainer{});
    auto& ConduitClassScrollElement = ConduitClassScroll.Element();
    ConduitClassScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitClassScrollElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ConduitClassScrollElement.Padding().Set(0.0f);
    ConduitClassScrollElement.Gap().Set(8.0f);
    ConduitClassScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);

    auto ConduitClassSummary = ConduitClassScroll.Add(
        SnAPI::UI::UIText("Bind one concrete host node type to one Conduit graph asset. The referenced graph's entry nodes drive runtime execution."));
    auto& ConduitClassSummaryText = ConduitClassSummary.Element();
    ConduitClassSummaryText.ElementStyle().Apply("editor.panel_subtitle");
    ConduitClassSummaryText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto ConduitClassNameInputBuilder = ConduitClassScroll.Add(SnAPI::UI::UITextInput{});
    auto& ConduitClassNameInput = ConduitClassNameInputBuilder.Element();
    ConduitClassNameInput.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitClassNameInput.Placeholder().Set(std::string("Class name"));
    ConduitClassNameInput.OnSubmit(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Text) {
        if (m_onConduitClassNameRequested)
        {
            m_onConduitClassNameRequested(Text);
        }
    }));
    ConduitClassNameInput.OnFocusStateChanged(SnAPI::UI::TDelegate<void(bool)>::Bind([this, Handle = ConduitClassNameInputBuilder.Handle()](const bool Focused) {
        if (Focused || !m_onConduitClassNameRequested || !m_context)
        {
            return;
        }
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(Handle.Id)))
        {
            m_onConduitClassNameRequested(Input->Text().Get());
        }
    }));
    m_conduitClassNameInput = ConduitClassNameInputBuilder.Handle();

    auto ConduitClassHostComboBuilder = ConduitClassScroll.Add(SnAPI::UI::UIComboBox{});
    auto& ConduitClassHostCombo = ConduitClassHostComboBuilder.Element();
    ConduitClassHostCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitClassHostCombo.Placeholder().Set(std::string("Host node type"));
    ConduitClassHostCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        if (!m_onConduitClassHostTypeRequested ||
            Index < 0 ||
            static_cast<std::size_t>(Index) >= m_conduitWorkspaceState.ClassHostTypeOptions.size())
        {
            return;
        }
        m_onConduitClassHostTypeRequested(
            m_conduitWorkspaceState.ClassHostTypeOptions[static_cast<std::size_t>(Index)].Type);
    });
    m_conduitClassHostTypeCombo = ConduitClassHostComboBuilder.Handle();

    auto ConduitClassGraphComboBuilder = ConduitClassScroll.Add(SnAPI::UI::UIComboBox{});
    auto& ConduitClassGraphCombo = ConduitClassGraphComboBuilder.Element();
    ConduitClassGraphCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitClassGraphCombo.Placeholder().Set(std::string("Referenced Conduit graph"));
    ConduitClassGraphCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        if (!m_onConduitClassGraphRequested)
        {
            return;
        }
        if (Index < 0 || static_cast<std::size_t>(Index) >= m_conduitWorkspaceState.ClassGraphOptions.size())
        {
            m_onConduitClassGraphRequested(std::string{});
            return;
        }
        m_onConduitClassGraphRequested(
            m_conduitWorkspaceState.ClassGraphOptions[static_cast<std::size_t>(Index)].AssetKey);
    });
    m_conduitClassGraphCombo = ConduitClassGraphComboBuilder.Handle();

    auto ConduitClassOverviewHost = ConduitClassSplit.Add(SnAPI::UI::UIPanel("Editor.ConduitClassOverviewHost"));
    ConfigureHostPanel(ConduitClassOverviewHost.Element());

    auto ConduitClassOverviewCard = ConduitClassOverviewHost.Add(SnAPI::UI::UIPanel("Editor.ConduitClassOverviewCard"));
    auto& ConduitClassOverviewCardPanel = ConduitClassOverviewCard.Element();
    ConduitClassOverviewCardPanel.ElementStyle().Apply("editor.section_card");
    ConduitClassOverviewCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConduitClassOverviewCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitClassOverviewCardPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ConduitClassOverviewCardPanel.Padding().Set(8.0f);
    ConduitClassOverviewCardPanel.Gap().Set(8.0f);

    auto ConduitClassOverviewTitle = ConduitClassOverviewCard.Add(SnAPI::UI::UIText("Overview"));
    ConduitClassOverviewTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto ConduitClassOverviewSummary = ConduitClassOverviewCard.Add(
        SnAPI::UI::UIText("Conduit classes bind a concrete host node type to one authored graph. Resize this split to give the settings pane or overview pane more room."));
    auto& ConduitClassOverviewSummaryElement = ConduitClassOverviewSummary.Element();
    ConduitClassOverviewSummaryElement.ElementStyle().Apply("editor.panel_subtitle");
    ConduitClassOverviewSummaryElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_conduitClassOverviewSummaryText = ConduitClassOverviewSummary.Handle();

    auto ConduitClassOverviewHostText = ConduitClassOverviewCard.Add(SnAPI::UI::UIText("Host: None"));
    auto& ConduitClassOverviewHostElement = ConduitClassOverviewHostText.Element();
    ConduitClassOverviewHostElement.ElementStyle().Apply("editor.menu_item");
    ConduitClassOverviewHostElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_conduitClassOverviewHostText = ConduitClassOverviewHostText.Handle();

    auto ConduitClassOverviewGraphText = ConduitClassOverviewCard.Add(SnAPI::UI::UIText("Graph: None"));
    auto& ConduitClassOverviewGraphElement = ConduitClassOverviewGraphText.Element();
    ConduitClassOverviewGraphElement.ElementStyle().Apply("editor.menu_item");
    ConduitClassOverviewGraphElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_conduitClassOverviewGraphText = ConduitClassOverviewGraphText.Handle();

    ViewTabsElement.SetTabLabel(0, "Game View");
    ViewTabsElement.SetTabLabel(1, "Profiler");
    ViewTabsElement.SetTabLabel(2, "Conduit");

    m_gameViewport = Viewport.Handle();
}

void EditorLayout::BuildInspectorPane(PanelBuilder& Workspace,
                                      BaseNode* SelectedNode,
                                      GameRuntime& Runtime,
                                      ComponentHandle& ActiveCamera)
{
    auto Inspector = Workspace.Add(SnAPI::UI::UIPanel("Editor.Inspector"));
    auto& InspectorPanel = Inspector.Element();
    InspectorPanel.ElementStyle().Apply("editor.sidebar");
    InspectorPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    InspectorPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    InspectorPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    InspectorPanel.Padding().Set(6.0f);
    InspectorPanel.Gap().Set(6.0f);

    auto TitleRow = Inspector.Add(SnAPI::UI::UIPanel("Editor.InspectorTitleRow"));
    auto& TitleRowPanel = TitleRow.Element();
    TitleRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    TitleRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    TitleRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    TitleRowPanel.Gap().Set(4.0f);
    TitleRowPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});
    TitleRowPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto TitleIcon = TitleRow.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kInspectorIconPath)));
    auto& TitleIconImage = TitleIcon.Element();
    ConfigureSvgIcon(TitleIconImage, 14.0f, kIconWhite);

    auto Title = TitleRow.Add(SnAPI::UI::UIText("Inspector"));
    Title.Element().ElementStyle().Apply("editor.panel_title");
    Title.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto InspectorTabs = Inspector.Add(SnAPI::UI::UITabs{});
    auto& InspectorTabsElement = InspectorTabs.Element();
    InspectorTabsElement.ElementStyle().Apply("editor.viewport_tabs");
    InspectorTabsElement.Width().Set(SnAPI::UI::Sizing::Fill());
    InspectorTabsElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    InspectorTabsElement.HeaderHeight().Set(30.0f);

    auto PropertiesTab = InspectorTabs.Add(SnAPI::UI::UIPanel("Editor.Inspector.Properties"));
    auto& PropertiesTabPanel = PropertiesTab.Element();
    PropertiesTabPanel.ElementStyle().Apply("editor.section_card");
    PropertiesTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    PropertiesTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PropertiesTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    PropertiesTabPanel.Padding().Set(4.0f);
    PropertiesTabPanel.Gap().Set(4.0f);

    auto Subtitle = PropertiesTab.Add(SnAPI::UI::UIText("Selection"));
    Subtitle.Element().ElementStyle().Apply("editor.panel_subtitle");
    Subtitle.Element().ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});

    auto PropertyPanelBuilder = PropertiesTab.Add(UIPropertyPanel{});
    auto& PropertyPanel = PropertyPanelBuilder.Element();
    PropertyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PropertyPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    PropertyPanel.Padding().Set(3.0f);
    PropertyPanel.Gap().Set(3.0f);
    PropertyPanel.ShowHorizontalScrollbar().Set(false);
    PropertyPanel.ShowVerticalScrollbar().Set(true);
    PropertyPanel.Smooth().Set(true);
    PropertyPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    PropertyPanel.SetComponentContextMenuHandler(
        SnAPI::UI::TDelegate<void(NodeHandle, const TypeId&, const SnAPI::UI::PointerEvent&)>::Bind(
            [this](const NodeHandle OwnerNode, const TypeId& ComponentType, const SnAPI::UI::PointerEvent& Event) {
                OpenInspectorComponentContextMenu(OwnerNode, ComponentType, Event);
            }));

    auto ToolsTab = InspectorTabs.Add(SnAPI::UI::UIPanel("Editor.Inspector.Tools"));
    auto& ToolsTabPanel = ToolsTab.Element();
    ToolsTabPanel.ElementStyle().Apply("editor.section_card");
    ToolsTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ToolsTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ToolsTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ToolsTabPanel.Padding().Set(4.0f);
    ToolsTabPanel.Gap().Set(6.0f);

    auto ToolsScroll = ToolsTab.Add(SnAPI::UI::UIScrollContainer{});
    auto& ToolsScrollElement = ToolsScroll.Element();
    ToolsScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ToolsScrollElement.Height().Set(SnAPI::UI::Sizing::Fill());
    ToolsScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ToolsScrollElement.ShowHorizontalScrollbar().Set(false);
    ToolsScrollElement.ShowVerticalScrollbar().Set(true);
    ToolsScrollElement.Smooth().Set(true);
    ToolsScrollElement.Padding().Set(2.0f);
    ToolsScrollElement.Gap().Set(6.0f);

    auto SnapCard = ToolsScroll.Add(SnAPI::UI::UIPanel("Editor.Tools.Snap"));
    auto& SnapCardPanel = SnapCard.Element();
    SnapCardPanel.ElementStyle().Apply("editor.section_card");
    SnapCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    SnapCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    SnapCardPanel.Gap().Set(4.0f);

    auto SnapTitle = SnapCard.Add(SnAPI::UI::UIText("Snapping"));
    SnapTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto MoveSnapLabel = SnapCard.Add(SnAPI::UI::UIText("Move Step"));
    MoveSnapLabel.Element().ElementStyle().Apply("editor.menu_item");

    auto MoveSnap = SnapCard.Add(SnAPI::UI::UINumberField{});
    auto& MoveSnapField = MoveSnap.Element();
    MoveSnapField.ElementStyle().Apply("editor.number_field");
    MoveSnapField.Step().Set(0.1);
    MoveSnapField.Value().Set(m_moveSnapStep);
    MoveSnapField.Precision().Set(2u);
    MoveSnapField.Width().Set(SnAPI::UI::Sizing::Fill());
    MoveSnapField.Height().Set(SnAPI::UI::Sizing::Auto());
    MoveSnapField.Padding().Set(5.0f);
    MoveSnapField.OnValueChanged([this](const double Value) {
        m_moveSnapStep = SanitizePositiveStep(Value, m_moveSnapStep);
    });

    auto RotateSnapLabel = SnapCard.Add(SnAPI::UI::UIText("Rotate Step (deg)"));
    RotateSnapLabel.Element().ElementStyle().Apply("editor.menu_item");

    auto RotateSnap = SnapCard.Add(SnAPI::UI::UINumberField{});
    auto& RotateSnapField = RotateSnap.Element();
    RotateSnapField.ElementStyle().Apply("editor.number_field");
    RotateSnapField.Step().Set(1.0);
    RotateSnapField.Value().Set(m_rotateSnapStepDegrees);
    RotateSnapField.Precision().Set(1u);
    RotateSnapField.Width().Set(SnAPI::UI::Sizing::Fill());
    RotateSnapField.Height().Set(SnAPI::UI::Sizing::Auto());
    RotateSnapField.Padding().Set(5.0f);
    RotateSnapField.OnValueChanged([this](const double Value) {
        m_rotateSnapStepDegrees = SanitizePositiveStep(Value, m_rotateSnapStepDegrees);
    });

    auto ScaleSnapLabel = SnapCard.Add(SnAPI::UI::UIText("Scale Step"));
    ScaleSnapLabel.Element().ElementStyle().Apply("editor.menu_item");

    auto ScaleSnap = SnapCard.Add(SnAPI::UI::UINumberField{});
    auto& ScaleSnapField = ScaleSnap.Element();
    ScaleSnapField.ElementStyle().Apply("editor.number_field");
    ScaleSnapField.Step().Set(0.05);
    ScaleSnapField.Value().Set(m_scaleSnapStep);
    ScaleSnapField.Precision().Set(2u);
    ScaleSnapField.Width().Set(SnAPI::UI::Sizing::Fill());
    ScaleSnapField.Height().Set(SnAPI::UI::Sizing::Auto());
    ScaleSnapField.Padding().Set(5.0f);
    ScaleSnapField.OnValueChanged([this](const double Value) {
        m_scaleSnapStep = SanitizePositiveStep(Value, m_scaleSnapStep);
    });

    auto DateCard = ToolsScroll.Add(SnAPI::UI::UIPanel("Editor.Tools.Date"));
    auto& DateCardPanel = DateCard.Element();
    DateCardPanel.ElementStyle().Apply("editor.section_card");
    DateCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    DateCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    DateCardPanel.Gap().Set(4.0f);

    auto DateTitle = DateCard.Add(SnAPI::UI::UIText("Build Date"));
    DateTitle.Element().ElementStyle().Apply("editor.panel_title");
    auto DatePicker = DateCard.Add(SnAPI::UI::UIDatePicker{});
    DatePicker.Element().ElementStyle().Apply("editor.date_picker");
    DatePicker.Element().Width().Set(SnAPI::UI::Sizing::Fill());
    DatePicker.Element().ShowWeekday().Set(false);

    auto ColorCard = ToolsScroll.Add(SnAPI::UI::UIPanel("Editor.Tools.Color"));
    auto& ColorCardPanel = ColorCard.Element();
    ColorCardPanel.ElementStyle().Apply("editor.section_card");
    ColorCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ColorCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ColorCardPanel.Gap().Set(4.0f);

    auto ColorTitle = ColorCard.Add(SnAPI::UI::UIText("Gizmo Palette"));
    ColorTitle.Element().ElementStyle().Apply("editor.panel_title");
    auto ColorPicker = ColorCard.Add(SnAPI::UI::UIColorPicker{});
    ColorPicker.Element().ElementStyle().Apply("editor.color_picker");
    ColorPicker.Element().Width().Set(SnAPI::UI::Sizing::Fill());
    ColorPicker.Element().Height().Set(SnAPI::UI::Sizing::Fixed(136.0f));

    auto TagsCard = ToolsScroll.Add(SnAPI::UI::UIPanel("Editor.Tools.Tags"));
    auto& TagsCardPanel = TagsCard.Element();
    TagsCardPanel.ElementStyle().Apply("editor.section_card");
    TagsCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    TagsCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    TagsCardPanel.Gap().Set(4.0f);

    auto TagsTitle = TagsCard.Add(SnAPI::UI::UIText("Selection Tags"));
    TagsTitle.Element().ElementStyle().Apply("editor.panel_title");
    auto TagsField = TagsCard.Add(SnAPI::UI::UITokenField{});
    TagsField.Element().ElementStyle().Apply("editor.token_field");
    TagsField.Element().Width().Set(SnAPI::UI::Sizing::Fill());
    TagsField.Element().AddToken("Gameplay", false);
    TagsField.Element().AddToken("Dynamic", false);

    auto ShortcutsTable = ToolsScroll.Add(SnAPI::UI::UITable{});
    auto& ShortcutsTableElement = ShortcutsTable.Element();
    ShortcutsTableElement.ElementStyle().Apply("editor.tools_table");
    ShortcutsTableElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ShortcutsTableElement.ColumnCount().Set(2u);
    ShortcutsTableElement.RowHeight().Set(26.0f);
    ShortcutsTableElement.HeaderHeight().Set(26.0f);
    ShortcutsTableElement.SetColumnHeaders({"Action", "Hotkey"});
    constexpr std::array<std::pair<std::string_view, std::string_view>, 4> kShortcuts{{
        {"Focus Selection", "F"},
        {"Duplicate", "Ctrl+D"},
        {"Frame All", "Shift+F"},
        {"Delete", "Del"},
    }};
    for (const auto& [Action, Hotkey] : kShortcuts)
    {
        auto ActionCell = ShortcutsTable.Add(SnAPI::UI::UIText(Action));
        ActionCell.Element().ElementStyle().Apply("editor.menu_item");
        auto KeyCell = ShortcutsTable.Add(SnAPI::UI::UIText(Hotkey));
        KeyCell.Element().ElementStyle().Apply("editor.panel_title");
    }

    InspectorTabsElement.SetTabLabel(0, "Selection");
    InspectorTabsElement.SetTabLabel(1, "Tools");

    m_inspectorPropertyPanel = PropertyPanelBuilder.Handle();
    BindInspectorTarget(SelectedNode, Runtime, ActiveCamera);
}

void EditorLayout::BindInspectorTarget(BaseNode* SelectedNode, GameRuntime& Runtime, ComponentHandle& ActiveCamera)
{
    if (!m_context)
    {
        return;
    }

    auto* PropertyPanel = ResolveInspectorPanel();
    if (!PropertyPanel)
    {
        return;
    }

    void* TargetObject = nullptr;
    TypeId TargetType{};

    if (SelectedNode)
    {
        const std::size_t ComponentSignature = ComputeNodeComponentSignature(*SelectedNode);
        if (m_boundInspectorNode != SelectedNode->Handle()
            || m_boundInspectorObject != SelectedNode
            || m_boundInspectorType != SelectedNode->TypeKey()
            || m_boundInspectorComponentSignature != ComponentSignature)
        {
            PropertyPanel->ClearObject();
            if (PropertyPanel->BindNode(SelectedNode))
            {
                m_boundInspectorNode = SelectedNode->Handle();
                m_boundInspectorObject = SelectedNode;
                m_boundInspectorType = SelectedNode->TypeKey();
                m_boundInspectorComponentSignature = ComponentSignature;
            }
            else
            {
                m_boundInspectorNode = {};
                m_boundInspectorObject = nullptr;
                m_boundInspectorType = {};
                m_boundInspectorComponentSignature = 0;
            }
        }
        else
        {
            PropertyPanel->RefreshFromModel();
        }
        return;
    }

    if (auto* ActiveCameraComponent = ResolveActiveCameraComponent(Runtime, ActiveCamera))
    {
        TargetObject = ActiveCameraComponent;
        TargetType = StaticTypeId<CameraComponent>();
    }

    if (!TargetObject)
    {
        PropertyPanel->ClearObject();
        m_boundInspectorNode = {};
        m_boundInspectorObject = nullptr;
        m_boundInspectorType = {};
        m_boundInspectorComponentSignature = 0;
        return;
    }

    if (m_boundInspectorObject == TargetObject && m_boundInspectorType == TargetType)
    {
        PropertyPanel->RefreshFromModel();
        return;
    }

    PropertyPanel->ClearObject();
    if (PropertyPanel->BindObject(TargetType, TargetObject))
    {
        m_boundInspectorNode = {};
        m_boundInspectorObject = TargetObject;
        m_boundInspectorType = TargetType;
        m_boundInspectorComponentSignature = 0;
    }
    else
    {
        m_boundInspectorNode = {};
        m_boundInspectorObject = nullptr;
        m_boundInspectorType = {};
        m_boundInspectorComponentSignature = 0;
    }
}

void EditorLayout::SyncGameViewportCamera(GameRuntime& Runtime, ComponentHandle& ActiveCamera)
{
    auto* Viewport = ResolveGameViewport();
    if (!Viewport)
    {
        return;
    }

    Viewport->SetGameRuntime(&Runtime);

    std::shared_ptr<SnAPI::Graphics::ICamera> RetainedCamera{};
    SnAPI::Graphics::ICamera* RenderCamera = nullptr;
    CameraComponent* ActiveCameraComponent = ResolveActiveCameraComponent(Runtime, ActiveCamera);
    if (ActiveCameraComponent)
    {
        RetainedCamera = ActiveCameraComponent->CameraShared();
        RenderCamera = RetainedCamera.get();
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (!RenderCamera)
    {
        if (auto* WorldPtr = Runtime.WorldPtr())
        {
            RetainedCamera = WorldPtr->Renderer().ActiveCameraShared();
            RenderCamera = RetainedCamera ? RetainedCamera.get() : WorldPtr->Renderer().ActiveCamera();
        }
    }
#endif

    if (ActiveCameraComponent && Viewport)
    {
        const SnAPI::UI::UIRect ViewRect = Viewport->LayoutRect();
        if (ViewRect.W > 0.0f && ViewRect.H > 0.0f)
        {
            auto& CameraSettings = ActiveCameraComponent->EditSettings();
            CameraSettings.Aspect = ViewRect.W / ViewRect.H;
        }
    }

    if (RetainedCamera)
    {
        Viewport->SetViewportCamera(RetainedCamera);
    }
    else
    {
        Viewport->SetViewportCamera(RenderCamera);
    }
}

UIRenderViewport* EditorLayout::GameViewport() const
{
    return ResolveGameViewport();
}

int32_t EditorLayout::GameViewportTabIndex() const
{
    if (auto* Tabs = ResolveGameViewTabs())
    {
        return Tabs->ActiveIndex().Get();
    }
    return 0;
}

UIRenderViewport* EditorLayout::ResolveGameViewport() const
{
    if (!m_context || m_gameViewport.Id.Value == 0)
    {
        return nullptr;
    }

    return dynamic_cast<UIRenderViewport*>(&m_context->GetElement(m_gameViewport.Id));
}

SnAPI::UI::UITabs* EditorLayout::ResolveGameViewTabs() const
{
    if (!m_context || m_gameViewTabs.Id.Value == 0)
    {
        return nullptr;
    }
    return dynamic_cast<SnAPI::UI::UITabs*>(&m_context->GetElement(m_gameViewTabs.Id));
}

UIPropertyPanel* EditorLayout::ResolveInspectorPanel() const
{
    if (!m_context || m_inspectorPropertyPanel.Id.Value == 0)
    {
        return nullptr;
    }

    return dynamic_cast<UIPropertyPanel*>(&m_context->GetElement(m_inspectorPropertyPanel.Id));
}

} // namespace SnAPI::GameFramework::Editor
