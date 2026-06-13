#include "Editor/EditorLayout.h"

#include "AuthoredAssetRegistry.h"
#include "AssetPipelineIds.h"
#include "BaseNode.h"
#include "CameraComponent.h"
#include "Editor/EditorSelectionModel.h"
#include "Editor/UIHelpTooltip.h"
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
#include <any>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>


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
constexpr std::string_view kSearchIconPath = "editor://Assets/search.svg";
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
constexpr std::array<std::string_view, 3> kKnownBuildPlatforms{
    "Windows", "Linux", "MacOS"};
constexpr std::array<std::string_view, 1> kKnownArchiveFormats{
    "zip"};
constexpr std::array<std::string_view, 4> kKnownBuildExecutionEnvironments{
    "host-local", "docker://snapi/windows-msvc:stable", "docker://snapi/linux-clang:stable",
    "docker://snapi/macos-clang:stable"};
constexpr std::string_view kContentAssetDragPayloadType = "Editor.ContentAsset";

struct ContentAssetDragPayload
{
    std::string AssetKey{};
};

[[nodiscard]] const ContentAssetDragPayload* TryGetContentAssetDragPayload(const SnAPI::UI::DragDropEvent& Event)
{
    if (Event.PayloadType != kContentAssetDragPayloadType || Event.Payload == nullptr)
    {
        return nullptr;
    }

    return std::any_cast<ContentAssetDragPayload>(Event.Payload);
}

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

[[nodiscard]] constexpr int32_t BuildConfigurationToIndex(const EBuildConfiguration Configuration)
{
    switch (Configuration)
    {
    case EBuildConfiguration::Debug:
        return 0;
    case EBuildConfiguration::Development:
        return 1;
    case EBuildConfiguration::Test:
        return 2;
    case EBuildConfiguration::Shipping:
        return 3;
    }

    return 1;
}

[[nodiscard]] constexpr EBuildConfiguration BuildConfigurationFromIndex(const int32_t Index)
{
    switch (Index)
    {
    case 0:
        return EBuildConfiguration::Debug;
    case 2:
        return EBuildConfiguration::Test;
    case 3:
        return EBuildConfiguration::Shipping;
    case 1:
    default:
        return EBuildConfiguration::Development;
    }
}

[[nodiscard]] constexpr std::string_view BuildConfigurationLabel(const EBuildConfiguration Configuration)
{
    switch (Configuration)
    {
    case EBuildConfiguration::Debug:
        return "Debug";
    case EBuildConfiguration::Test:
        return "Test";
    case EBuildConfiguration::Shipping:
        return "Shipping";
    case EBuildConfiguration::Development:
    default:
        return "Development";
    }
}

[[nodiscard]] constexpr int32_t DependencyPolicyToIndex(const EAssetDependencyPolicy Policy)
{
    switch (Policy)
    {
    case EAssetDependencyPolicy::HardOnly:
        return 0;
    case EAssetDependencyPolicy::HardAndSoft:
        return 1;
    case EAssetDependencyPolicy::HardSoftAndEditorPreview:
        return 2;
    case EAssetDependencyPolicy::CustomResolver:
        return 3;
    }

    return 0;
}

[[nodiscard]] constexpr EAssetDependencyPolicy DependencyPolicyFromIndex(const int32_t Index)
{
    switch (Index)
    {
    case 1:
        return EAssetDependencyPolicy::HardAndSoft;
    case 2:
        return EAssetDependencyPolicy::HardSoftAndEditorPreview;
    case 3:
        return EAssetDependencyPolicy::CustomResolver;
    case 0:
    default:
        return EAssetDependencyPolicy::HardOnly;
    }
}

[[nodiscard]] constexpr int32_t ChunkStrategyToIndex(const EAssetChunkStrategy Strategy)
{
    switch (Strategy)
    {
    case EAssetChunkStrategy::Monolithic:
        return 0;
    case EAssetChunkStrategy::SharedPlusPerLevel:
        return 1;
    case EAssetChunkStrategy::PerLabel:
        return 2;
    case EAssetChunkStrategy::CustomGraph:
        return 3;
    }

    return 0;
}

[[nodiscard]] constexpr EAssetChunkStrategy ChunkStrategyFromIndex(const int32_t Index)
{
    switch (Index)
    {
    case 1:
        return EAssetChunkStrategy::SharedPlusPerLevel;
    case 2:
        return EAssetChunkStrategy::PerLabel;
    case 3:
        return EAssetChunkStrategy::CustomGraph;
    case 0:
    default:
        return EAssetChunkStrategy::Monolithic;
    }
}

[[nodiscard]] constexpr int32_t ModuleTypeToIndex(const EProjectModuleType ModuleType)
{
    switch (ModuleType)
    {
    case EProjectModuleType::Runtime:
        return 0;
    case EProjectModuleType::Editor:
        return 1;
    case EProjectModuleType::Shared:
        return 2;
    case EProjectModuleType::Developer:
        return 3;
    case EProjectModuleType::Test:
        return 4;
    case EProjectModuleType::Program:
        return 5;
    }

    return 0;
}

[[nodiscard]] constexpr EProjectModuleType ModuleTypeFromIndex(const int32_t Index)
{
    switch (Index)
    {
    case 1:
        return EProjectModuleType::Editor;
    case 2:
        return EProjectModuleType::Shared;
    case 3:
        return EProjectModuleType::Developer;
    case 4:
        return EProjectModuleType::Test;
    case 5:
        return EProjectModuleType::Program;
    case 0:
    default:
        return EProjectModuleType::Runtime;
    }
}

[[nodiscard]] constexpr bool DefaultLoadInEditorForModule(const EProjectModuleType ModuleType)
{
    switch (ModuleType)
    {
    case EProjectModuleType::Runtime:
    case EProjectModuleType::Editor:
    case EProjectModuleType::Shared:
    case EProjectModuleType::Developer:
        return true;
    case EProjectModuleType::Test:
    case EProjectModuleType::Program:
        return false;
    }

    return false;
}

[[nodiscard]] constexpr bool DefaultLoadInRuntimeForModule(const EProjectModuleType ModuleType)
{
    switch (ModuleType)
    {
    case EProjectModuleType::Runtime:
    case EProjectModuleType::Shared:
        return true;
    case EProjectModuleType::Editor:
    case EProjectModuleType::Developer:
    case EProjectModuleType::Test:
    case EProjectModuleType::Program:
        return false;
    }

    return false;
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
constexpr std::string_view kContextMenuItemFileNewPluginId = "menu.file.new_plugin";
constexpr std::string_view kContextMenuItemFileAddModuleId = "menu.file.add_module";
constexpr std::string_view kContextMenuItemFilePackageProjectId = "menu.file.package_project";
constexpr std::string_view kContextMenuItemFileProjectSettingsId = "menu.file.project_settings";
constexpr std::string_view kContextMenuItemConduitSpawnPrefix = "conduit.spawn.";
constexpr std::string_view kContextMenuItemConduitSpawnNoMatchesId = "conduit.spawn.no_matches";

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
    return PrettyReflectedTypeName(QualifiedName);
}

[[nodiscard]] std::vector<std::string> SplitCategoryPath(std::string_view Path)
{
    std::vector<std::string> Segments{};
    std::size_t SegmentStart = 0;
    while (SegmentStart < Path.size())
    {
        const std::size_t SegmentEnd = Path.find('/', SegmentStart);
        const std::size_t Count = SegmentEnd == std::string_view::npos
            ? Path.size() - SegmentStart
            : SegmentEnd - SegmentStart;
        std::string Segment = std::string(Path.substr(SegmentStart, Count));
        Segment.erase(
            std::remove_if(Segment.begin(), Segment.end(), [](const unsigned char Character) {
                return Character == '\t' || Character == '\n' || Character == '\r';
            }),
            Segment.end());
        if (!Segment.empty())
        {
            Segments.push_back(std::move(Segment));
        }
        if (SegmentEnd == std::string_view::npos)
        {
            break;
        }
        SegmentStart = SegmentEnd + 1;
    }
    return Segments;
}

[[nodiscard]] SnAPI::UI::UIContextMenuItem* FindContextMenuGroup(
    std::vector<SnAPI::UI::UIContextMenuItem>& Items,
    std::string_view Label)
{
    auto It = std::find_if(Items.begin(), Items.end(), [Label](const SnAPI::UI::UIContextMenuItem& Item) {
        return !Item.IsSeparator && Item.Label == Label && !Item.Children.empty();
    });
    return It == Items.end() ? nullptr : &(*It);
}

[[nodiscard]] std::vector<SnAPI::UI::UIContextMenuItem> BuildConduitSpawnMenuItems(
    const Conduit::Editor::GraphSpawnMenuRequest& Request,
    const std::vector<Conduit::Editor::SpawnMenuEntryView>& Entries)
{
    std::vector<SnAPI::UI::UIContextMenuItem> Items{};
    if (Entries.empty())
    {
        Items.push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemConduitSpawnNoMatchesId),
            .Label = Request.FromPinDrag ? "No compatible nodes" : "No nodes available",
            .Enabled = false,
        });
        return Items;
    }

    for (std::size_t Index = 0; Index < Entries.size(); ++Index)
    {
        const auto& Entry = Entries[Index];
        std::vector<SnAPI::UI::UIContextMenuItem>* ParentItems = &Items;
        std::string CategoryPath{};
        for (const std::string& Segment : SplitCategoryPath(Entry.Category))
        {
            CategoryPath = CategoryPath.empty() ? Segment : CategoryPath + "/" + Segment;
            SnAPI::UI::UIContextMenuItem* Group = FindContextMenuGroup(*ParentItems, Segment);
            if (!Group)
            {
                ParentItems->push_back(SnAPI::UI::UIContextMenuItem{
                    .Id = "conduit.spawn.group." + CategoryPath,
                    .Label = Segment,
                });
                Group = &ParentItems->back();
            }
            ParentItems = &Group->Children;
        }

        ParentItems->push_back(SnAPI::UI::UIContextMenuItem{
            .Id = std::string(kContextMenuItemConduitSpawnPrefix) + std::to_string(Index),
            .Label = Entry.DisplayName,
            .Shortcut = Entry.TargetPin.empty() ? std::optional<std::string>{} : std::make_optional(Entry.TargetPin),
        });
    }

    return Items;
}

struct ConduitPaletteCategoryNode
{
    std::string Label{};
    std::vector<const EditorLayout::ConduitWorkspaceState::PaletteEntry*> Entries{};
    std::vector<ConduitPaletteCategoryNode> Children{};
};

[[nodiscard]] ConduitPaletteCategoryNode* FindOrAddPaletteChild(
    std::vector<ConduitPaletteCategoryNode>& Children,
    std::string_view Label)
{
    auto It = std::find_if(Children.begin(), Children.end(), [Label](const ConduitPaletteCategoryNode& Node) {
        return Node.Label == Label;
    });
    if (It != Children.end())
    {
        return &(*It);
    }

    Children.push_back(ConduitPaletteCategoryNode{.Label = std::string(Label)});
    return &Children.back();
}

[[nodiscard]] ConduitPaletteCategoryNode BuildConduitPaletteTree(
    const std::vector<EditorLayout::ConduitWorkspaceState::PaletteEntry>& Entries,
    std::string_view FilterText)
{
    ConduitPaletteCategoryNode Root{};
    std::string FilterLower = std::string(FilterText);
    std::transform(FilterLower.begin(), FilterLower.end(), FilterLower.begin(), [](const unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });

    for (const auto& Entry : Entries)
    {
        std::string SearchText = Entry.DisplayName + " " + Entry.Category + " " + Entry.Tooltip;
        std::transform(SearchText.begin(), SearchText.end(), SearchText.begin(), [](const unsigned char Character) {
            return static_cast<char>(std::tolower(Character));
        });

        if (!FilterLower.empty() && SearchText.find(FilterLower) == std::string::npos)
        {
            continue;
        }

        ConduitPaletteCategoryNode* Current = &Root;
        for (const std::string& Segment : SplitCategoryPath(Entry.Category))
        {
            Current = FindOrAddPaletteChild(Current->Children, Segment);
        }
        Current->Entries.push_back(&Entry);
    }

    const auto SortTree = [](const auto& Self, ConduitPaletteCategoryNode& Node) -> void {
        std::sort(Node.Entries.begin(), Node.Entries.end(), [](const auto* Left, const auto* Right) {
            if (!Left || !Right)
            {
                return Left < Right;
            }
            if (Left->DisplayName != Right->DisplayName)
            {
                return Left->DisplayName < Right->DisplayName;
            }
            return Left->StableId < Right->StableId;
        });
        std::sort(Node.Children.begin(), Node.Children.end(), [](const ConduitPaletteCategoryNode& Left,
                                                                 const ConduitPaletteCategoryNode& Right) {
            return Left.Label < Right.Label;
        });
        for (ConduitPaletteCategoryNode& Child : Node.Children)
        {
            Self(Self, Child);
        }
    };
    SortTree(SortTree, Root);
    return Root;
}

void DestroyDirectChildren(SnAPI::UI::UIContext& Context, const SnAPI::UI::ElementId ParentId)
{
    if (ParentId.Value == 0)
    {
        return;
    }

    std::vector<SnAPI::UI::ElementId> Children{};
    UI::IUIElement& Parent = Context.GetElement(ParentId);
    Children.reserve(Parent.ChildCount());
    for (uint32_t Index = 0; Index < Parent.ChildCount(); ++Index)
    {
        Children.push_back(Parent.ChildAt(Index).GetId());
    }

    for (const SnAPI::UI::ElementId ChildId : Children)
    {
        Context.DestroyElement(ChildId);
    }
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

[[nodiscard]] std::vector<std::string> ParseMultilineEntries(const std::string_view Text)
{
    std::vector<std::string> Entries{};
    std::string Current{};
    for (const char Character : Text)
    {
        if (Character == '\r')
        {
            continue;
        }
        if (Character == '\n')
        {
            Current = TrimCopy(std::move(Current));
            if (!Current.empty())
            {
                Entries.push_back(std::move(Current));
            }
            Current.clear();
            continue;
        }
        Current.push_back(Character);
    }

    Current = TrimCopy(std::move(Current));
    if (!Current.empty())
    {
        Entries.push_back(std::move(Current));
    }

    return Entries;
}

[[nodiscard]] std::string JoinEntries(const std::vector<std::string>& Entries)
{
    std::string Text{};
    for (std::size_t Index = 0; Index < Entries.size(); ++Index)
    {
        if (Index > 0u)
        {
            Text += '\n';
        }
        Text += Entries[Index];
    }
    return Text;
}

[[nodiscard]] bool ContainsEntry(const std::vector<std::string>& Entries, const std::string_view Value)
{
    return std::ranges::any_of(Entries, [Value](const std::string& Entry) { return Entry == Value; });
}

void AppendUniqueEntryText(std::string& TargetText, const std::string_view Value)
{
    const std::string TrimmedValue = TrimCopy(std::string(Value));
    if (TrimmedValue.empty())
    {
        return;
    }

    std::vector<std::string> Entries = ParseMultilineEntries(TargetText);
    if (!ContainsEntry(Entries, TrimmedValue))
    {
        Entries.push_back(TrimmedValue);
        TargetText = JoinEntries(Entries);
    }
}

[[nodiscard]] std::vector<std::string> BuildComboItemsWithCurrent(
    const std::span<const std::string_view> BaseItems, const std::string_view Placeholder, const std::string_view CurrentValue)
{
    std::vector<std::string> Items{};
    Items.reserve(BaseItems.size() + 2u);
    Items.emplace_back(Placeholder);
    for (const std::string_view Item : BaseItems)
    {
        Items.emplace_back(Item);
    }

    const std::string TrimmedCurrent = TrimCopy(std::string(CurrentValue));
    if (!TrimmedCurrent.empty() && !ContainsEntry(Items, TrimmedCurrent))
    {
        Items.push_back(TrimmedCurrent);
    }

    return Items;
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

template<typename TBuilder>
void AddFieldHelpText(TBuilder& Parent, const std::string_view HelpText)
{
    if (HelpText.empty())
    {
        return;
    }

    auto HelpRow = Parent.Add(SnAPI::UI::UIPanel{});
    auto& HelpRowElement = HelpRow.Element();
    ConfigureTransparentLayoutPanel(HelpRowElement);
    HelpRowElement.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HelpRowElement.Width().Set(SnAPI::UI::Sizing::Auto());
    HelpRowElement.Height().Set(SnAPI::UI::Sizing::Auto());
    HelpRowElement.Gap().Set(6.0f);

    auto HelpBadge = HelpRow.Add(UIHelpTooltip{});
    auto& HelpBadgeElement = HelpBadge.Element();
    HelpBadgeElement.TooltipText().Set(std::string(HelpText));
    HelpBadgeElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 2.0f, 0.0f});

    std::string PreviewText{};
    PreviewText.reserve(HelpText.size());
    bool LastWasWhitespace = false;
    for (const unsigned char Ch : HelpText)
    {
        const bool IsWhitespace = std::isspace(Ch) != 0;
        if (IsWhitespace)
        {
            if (!PreviewText.empty() && !LastWasWhitespace)
            {
                PreviewText.push_back(' ');
            }
            LastWasWhitespace = true;
            continue;
        }

        PreviewText.push_back(static_cast<char>(Ch));
        LastWasWhitespace = false;
    }

    constexpr std::size_t kHelpPreviewMaxLength = 120;
    if (PreviewText.size() > kHelpPreviewMaxLength)
    {
        PreviewText.resize(kHelpPreviewMaxLength - 3);
        PreviewText.append("...");
    }

    auto HelpLabel = HelpRow.Add(SnAPI::UI::UIText(std::move(PreviewText)));
    auto& HelpLabelElement = HelpLabel.Element();
    HelpLabelElement.ElementStyle().Apply("editor.panel_subtitle");
    HelpLabelElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    HelpLabelElement.Width().Set(SnAPI::UI::Sizing::Fill());
    HelpLabelElement.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);
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
    DestroyBuildModalOverlay();
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
    m_contentAssetsScroll = {};
    m_contentAssetsGrid = {};
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
    m_conduitCompileButton = {};
    m_conduitVariablesTree = {};
    m_conduitGraphSelfTypeCombo = {};
    m_conduitPaletteSearchInput = {};
    m_conduitPaletteScroll = {};
    m_conduitPaletteContentPanel = {};
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
    m_conduitNodeDefaultInputsPanel = {};
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
    m_buildModalOpen = false;
    m_projectModalAction = EProjectAction::CreateNew;
    m_projectNameText.clear();
    m_projectDirectoryText.clear();
    m_projectFilePathText.clear();
    m_projectSettingsNameText.clear();
    m_projectSettingsStartupAssetText.clear();
    m_projectSettingsDefaultRenderSettingsAssetId.clear();
    m_buildPanelState = {};
    m_buildModalSelectedProfileName.clear();
    m_buildModalSelectedHistoryBuildId.clear();
    m_buildModalProfileKeys.clear();
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
    m_onBuildActionRequested = {};
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
    m_buildModalOverlay = {};
    m_buildProfileCombo = {};
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
    m_contentAssetsScroll = {};
    m_contentAssetsGrid = {};
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

    auto AssetsScroll = AssetsTab.Add(SnAPI::UI::UIScrollContainer{});
    auto& AssetsScrollElement = AssetsScroll.Element();
    AssetsScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    AssetsScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    AssetsScrollElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    AssetsScrollElement.Padding().Set(0.0f);
    AssetsScrollElement.Gap().Set(0.0f);
    AssetsScrollElement.ShowHorizontalScrollbar().Set(false);
    AssetsScrollElement.ShowVerticalScrollbar().Set(true);
    AssetsScrollElement.ElementStyle().Apply("editor.browser_list");
    AssetsScrollElement.OnContextMenuRequested(
        SnAPI::UI::TDelegate<void(const SnAPI::UI::PointerEvent&)>::Bind(
            [this](const SnAPI::UI::PointerEvent& Event) {
                OpenContentBrowserContextMenu(Event);
            }));
    m_contentAssetsScroll = AssetsScroll.Handle();

    auto AssetsGrid = AssetsScroll.Add(SnAPI::UI::UIGrid{});
    auto& AssetsGridElement = AssetsGrid.Element();
    AssetsGridElement.Width().Set(SnAPI::UI::Sizing::Fill());
    AssetsGridElement.Height().Set(SnAPI::UI::Sizing::Auto());
    AssetsGridElement.Padding().Set(0.0f);
    AssetsGridElement.CellWidth().Set(152.0f);
    AssetsGridElement.CellHeight().Set(420.0f);
    AssetsGridElement.ColumnGap().Set(10.0f);
    AssetsGridElement.RowGap().Set(10.0f);
    AssetsGridElement.ClipContent().Set(false);
    AssetsGridElement.BackgroundColor().Set(SnAPI::UI::Color::Transparent());
    AssetsGridElement.BorderColor().Set(SnAPI::UI::Color::Transparent());
    AssetsGridElement.BorderThickness().Set(0.0f);
    AssetsGridElement.CornerRadius().Set(0.0f);
    m_contentAssetsGrid = AssetsGrid.Handle();

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
    TreeElement.SetDragDropHandler(
        [this](const int32_t ItemIndex,
               const SnAPI::UI::UITreeItem*,
               const uint32_t RoutedTypeId,
               const SnAPI::UI::DragDropEvent& Event) -> bool {
            const ContentAssetDragPayload* Payload = TryGetContentAssetDragPayload(Event);
            if (!Payload)
            {
                return false;
            }

            if (RoutedTypeId != SnAPI::UI::RoutedEventTypes::Drop.Id)
            {
                return true;
            }

            if (!m_onContentAssetDropRequested)
            {
                return false;
            }

            ContentAssetDropRequest Request{};
            Request.AssetKey = Payload->AssetKey;
            Request.Target = EContentAssetDropTarget::Hierarchy;
            Request.ScreenPosition = Event.Position;
            if (ItemIndex >= 0 && static_cast<std::size_t>(ItemIndex) < m_hierarchyVisibleNodes.size())
            {
                Request.TargetNode = m_hierarchyVisibleNodes[static_cast<std::size_t>(ItemIndex)];
            }
            m_onContentAssetDropRequested(Request);
            return true;
        });
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

void EditorLayout::SetPluginActionHandler(SnAPI::UI::TDelegate<void(const PluginActionRequest&)> Handler)
{
    m_onPluginActionRequested = std::move(Handler);
}

void EditorLayout::SetModuleActionHandler(SnAPI::UI::TDelegate<void(const ModuleActionRequest&)> Handler)
{
    m_onModuleActionRequested = std::move(Handler);
}

void EditorLayout::SetBuildActionHandler(SnAPI::UI::TDelegate<void(const BuildActionRequest&)> Handler)
{
    m_onBuildActionRequested = std::move(Handler);
}

void EditorLayout::SetProjectState(ProjectState State)
{
    m_projectState = std::move(State);

    if (!m_projectState.IsLoaded && m_projectSettingsModalOpen)
    {
        CloseProjectSettingsModal();
    }

    if (!m_projectState.IsLoaded && m_buildModalOpen)
    {
        CloseBuildModal();
    }

    if (!m_projectSettingsModalOpen)
    {
        m_projectSettingsNameText = m_projectState.Name;
        m_projectSettingsStartupAssetText = m_projectState.StartupLevelAsset;
        m_projectSettingsDefaultRenderSettingsAssetId = m_projectState.DefaultRenderSettingsAssetId;
    }
}

void EditorLayout::SetBuildPanelState(BuildPanelState State)
{
    const BuildPanelState PreviousState = m_buildPanelState;
    m_buildPanelState = std::move(State);

    const auto ProfileStillExists = [this]() {
        return std::ranges::any_of(
            m_buildPanelState.Profiles,
            [this](const BuildProfileEntry& Entry) { return Entry.Name == m_buildModalSelectedProfileName; });
    };

    if (!ProfileStillExists())
    {
        auto DefaultIt = std::find_if(
            m_buildPanelState.Profiles.begin(),
            m_buildPanelState.Profiles.end(),
            [](const BuildProfileEntry& Entry) { return Entry.IsDefault; });
        if (DefaultIt != m_buildPanelState.Profiles.end())
        {
            m_buildModalSelectedProfileName = DefaultIt->Name;
        }
        else if (!m_buildPanelState.Profiles.empty())
        {
            m_buildModalSelectedProfileName = m_buildPanelState.Profiles.front().Name;
        }
        else
        {
            m_buildModalSelectedProfileName.clear();
        }
    }

    const bool DraftSeedStillExists =
        m_buildModalDraftSeedProfileName.empty() ||
        std::ranges::any_of(m_buildPanelState.Profiles,
                            [this](const BuildProfileEntry& Entry) { return Entry.Name == m_buildModalDraftSeedProfileName; });
    if (!DraftSeedStillExists ||
        (m_buildModalDraftSeedProfileName.empty() && !m_buildModalDraftDirty) ||
        (m_buildModalDraftSeedProfileName != m_buildModalSelectedProfileName && !m_buildModalDraftDirty))
    {
        ResetBuildModalDraftFromSelectedProfile();
    }

    const auto HistoryStillExists = [this]() {
        return std::ranges::any_of(
            m_buildPanelState.HistoryEntries,
            [this](const BuildHistoryEntryView& Entry) { return Entry.BuildId == m_buildModalSelectedHistoryBuildId; });
    };

    if (!HistoryStillExists())
    {
        auto LatestIt = std::find_if(
            m_buildPanelState.HistoryEntries.begin(),
            m_buildPanelState.HistoryEntries.end(),
            [](const BuildHistoryEntryView& Entry) { return Entry.IsLatest; });
        if (LatestIt != m_buildPanelState.HistoryEntries.end())
        {
            m_buildModalSelectedHistoryBuildId = LatestIt->BuildId;
        }
        else if (!m_buildPanelState.HistoryEntries.empty())
        {
            m_buildModalSelectedHistoryBuildId = m_buildPanelState.HistoryEntries.front().BuildId;
        }
        else
        {
            m_buildModalSelectedHistoryBuildId.clear();
        }
    }

    if (m_buildModalOpen)
    {
        if (m_buildModalOverlay.Id.Value == 0 || BuildModalRequiresStructuralRebuild(PreviousState, m_buildPanelState))
        {
            RebuildBuildModalOverlay();
        }
        else
        {
            RefreshBuildModalLiveState();
        }
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

void EditorLayout::SetContentAssetDropHandler(SnAPI::UI::TDelegate<void(const ContentAssetDropRequest&)> Handler)
{
    m_onContentAssetDropRequested = std::move(Handler);
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

void EditorLayout::SetContentAssetInspectorRuntimeMutatedHandler(
    SnAPI::UI::TDelegate<void(const TypeId&, void*)> Handler)
{
    m_onContentAssetInspectorRuntimeMutated = std::move(Handler);
}

void EditorLayout::SetContentAssetInspectorImportMutatedHandler(
    SnAPI::UI::TDelegate<void(const TypeId&, void*)> Handler)
{
    m_onContentAssetInspectorImportMutated = std::move(Handler);
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

void EditorLayout::SetConduitCanvasView(SnAPI::GameFramework::Conduit::Editor::GraphCanvasView View)
{
    m_conduitWorkspaceState.CanvasPanX = View.Viewport.PanX;
    m_conduitWorkspaceState.CanvasPanY = View.Viewport.PanY;
    m_conduitWorkspaceState.CanvasZoom = View.Viewport.Zoom;

    m_conduitWorkspaceState.CanvasNodes.clear();
    m_conduitWorkspaceState.CanvasNodes.reserve(View.Nodes.size());
    for (auto& Node : View.Nodes)
    {
        std::vector<ConduitWorkspaceState::CanvasNode::Pin> InputPins{};
        InputPins.reserve(Node.InputPins.size());
        for (auto& Pin : Node.InputPins)
        {
            InputPins.push_back(ConduitWorkspaceState::CanvasNode::Pin{
                .Name = std::move(Pin.Name),
                .TypeLabel = std::move(Pin.TypeLabel),
                .Tooltip = std::move(Pin.Tooltip),
                .Kind = Pin.Kind,
                .IsInput = Pin.IsInput,
                .IsExec = Pin.IsExec,
            });
        }

        std::vector<ConduitWorkspaceState::CanvasNode::Pin> OutputPins{};
        OutputPins.reserve(Node.OutputPins.size());
        for (auto& Pin : Node.OutputPins)
        {
            OutputPins.push_back(ConduitWorkspaceState::CanvasNode::Pin{
                .Name = std::move(Pin.Name),
                .TypeLabel = std::move(Pin.TypeLabel),
                .Tooltip = std::move(Pin.Tooltip),
                .Kind = Pin.Kind,
                .IsInput = Pin.IsInput,
                .IsExec = Pin.IsExec,
            });
        }

        m_conduitWorkspaceState.CanvasNodes.push_back(ConduitWorkspaceState::CanvasNode{
            .Id = Node.Id,
            .Title = std::move(Node.Title),
            .Detail = std::move(Node.Detail),
            .Tooltip = std::move(Node.Tooltip),
            .X = Node.X,
            .Y = Node.Y,
            .Width = Node.Width,
            .IsCollapsed = Node.IsCollapsed,
            .Selected = Node.Selected,
            .InputPins = std::move(InputPins),
            .OutputPins = std::move(OutputPins),
        });
    }

    m_conduitWorkspaceState.CanvasComments.clear();
    m_conduitWorkspaceState.CanvasComments.reserve(View.Comments.size());
    for (auto& Comment : View.Comments)
    {
        m_conduitWorkspaceState.CanvasComments.push_back(ConduitWorkspaceState::CanvasComment{
            .Id = Comment.Id,
            .Title = std::move(Comment.Title),
            .X = Comment.X,
            .Y = Comment.Y,
            .Width = Comment.Width,
            .Height = Comment.Height,
            .ColorRgba = Comment.ColorRgba,
            .Selected = Comment.Selected,
        });
    }

    m_conduitWorkspaceState.CanvasWires.clear();
    m_conduitWorkspaceState.CanvasWires.reserve(View.Wires.size());
    for (auto& Wire : View.Wires)
    {
        m_conduitWorkspaceState.CanvasWires.push_back(ConduitWorkspaceState::CanvasWire{
            .SourceNodeId = Wire.SourceNodeId,
            .SourcePin = std::move(Wire.SourcePin),
            .TargetNodeId = Wire.TargetNodeId,
            .TargetPin = std::move(Wire.TargetPin),
            .Kind = Wire.Kind,
            .IsExec = Wire.IsExec,
        });
    }

    RefreshConduitCanvasView();
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

void EditorLayout::SetConduitGraphSelfTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)> Handler)
{
    m_onConduitGraphSelfTypeRequested = std::move(Handler);
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

void EditorLayout::SetConduitNodeDefaultBoolHandler(SnAPI::UI::TDelegate<void(const std::string&, bool)> Handler)
{
    m_onConduitNodeDefaultBoolRequested = std::move(Handler);
}

void EditorLayout::SetConduitNodeDefaultTextHandler(
    SnAPI::UI::TDelegate<void(const std::string&, const std::string&)> Handler)
{
    m_onConduitNodeDefaultTextRequested = std::move(Handler);
}

void EditorLayout::SetConduitNodeDefaultEnumHandler(
    SnAPI::UI::TDelegate<void(const std::string&, const std::string&)> Handler)
{
    m_onConduitNodeDefaultEnumRequested = std::move(Handler);
}

void EditorLayout::SetConduitNodeDefaultClearHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler)
{
    m_onConduitNodeDefaultClearRequested = std::move(Handler);
}

void EditorLayout::SetConduitNodeMoveHandler(SnAPI::UI::TDelegate<void(const Uuid&, float, float)> Handler)
{
    m_onConduitNodeMoveRequested = std::move(Handler);
}

void EditorLayout::SetConduitPinConnectedHandler(
    SnAPI::UI::TDelegate<void(const Uuid&, const std::string&, const Uuid&, const std::string&)> Handler)
{
    m_onConduitPinConnectedRequested = std::move(Handler);
}

void EditorLayout::SetConduitSpawnMenuRequestHandler(
    SnAPI::UI::TDelegate<void(const SnAPI::GameFramework::Conduit::Editor::GraphSpawnMenuRequest&)> Handler)
{
    m_onConduitSpawnMenuRequest = std::move(Handler);
}

void EditorLayout::SetConduitSpawnMenuSelectionHandler(
    SnAPI::UI::TDelegate<void(const SnAPI::GameFramework::Conduit::Editor::GraphSpawnMenuRequest&,
                              const SnAPI::GameFramework::Conduit::Editor::SpawnMenuEntryView&)> Handler)
{
    m_onConduitSpawnMenuSelectionRequested = std::move(Handler);
}

void EditorLayout::SetConduitCompileHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onConduitCompileRequested = std::move(Handler);
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
    const bool CanCompileActiveConduitGraph = m_conduitWorkspaceState.Open &&
                                              m_conduitWorkspaceState.Kind == ConduitWorkspaceState::EDocumentKind::Graph;
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
    if (m_conduitCompileButton.Id.Value != 0)
    {
        if (auto* Button = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_conduitCompileButton.Id)))
        {
            Button->SetDisabled(!CanCompileActiveConduitGraph);
        }
    }

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

    if (m_conduitGraphSelfTypeCombo.Id.Value != 0)
    {
        if (auto* Combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(m_conduitGraphSelfTypeCombo.Id)))
        {
            std::vector<std::string> Labels{};
            Labels.reserve(m_conduitWorkspaceState.GraphSelfTypeOptions.size());
            int32_t SelectedIndex = -1;
            for (std::size_t Index = 0; Index < m_conduitWorkspaceState.GraphSelfTypeOptions.size(); ++Index)
            {
                Labels.push_back(m_conduitWorkspaceState.GraphSelfTypeOptions[Index].Label);
                if (m_conduitWorkspaceState.GraphSelfTypeOptions[Index].Type == m_conduitWorkspaceState.SelectedSelfType)
                {
                    SelectedIndex = static_cast<int32_t>(Index);
                }
            }
            Combo->SetItems(std::move(Labels));
            (void)Combo->SetSelectedIndex(SelectedIndex, false);
        }
    }

    if (m_conduitPaletteSearchInput.Id.Value != 0)
    {
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_conduitPaletteSearchInput.Id)))
        {
            Input->Text().Set(m_conduitPaletteFilterText);
        }
    }

    if (m_conduitPaletteContentPanel.Id.Value != 0)
    {
        DestroyDirectChildren(*m_context, m_conduitPaletteContentPanel.Id);
        m_conduitVisiblePaletteStableIds.clear();

        const ConduitPaletteCategoryNode PaletteRoot =
            BuildConduitPaletteTree(m_conduitWorkspaceState.PaletteEntries, m_conduitPaletteFilterText);

        if (!PaletteRoot.Entries.empty() || !PaletteRoot.Children.empty())
        {
            SnAPI::UI::TElementBuilder<SnAPI::UI::UIPanel> PaletteRootBuilder(m_context, m_conduitPaletteContentPanel);

            const auto AddPaletteEntryButton = [this](auto& ParentBuilder,
                                                      const ConduitWorkspaceState::PaletteEntry& Entry,
                                                      const float Indent) {
                auto ButtonBuilder = ParentBuilder.Add(SnAPI::UI::UIButton{});
                if (ButtonBuilder.Handle().Id.Value == 0)
                {
                    return;
                }
                auto& Button = ButtonBuilder.Element();
                Button.ElementStyle().Apply("editor.toolbar_button");
                Button.Width().Set(SnAPI::UI::Sizing::Fill());
                Button.ElementPadding().Set(SnAPI::UI::Padding{8.0f + Indent, 4.0f, 8.0f, 4.0f});
                if (Entry.StableId == m_conduitSelectedPaletteStableId)
                {
                    Button.Background().Set(SnAPI::UI::Color::RGBA(74, 93, 126, 255));
                }
                Button.OnClick([this, StableId = Entry.StableId]() {
                    m_conduitSelectedPaletteStableId = StableId;
                    RefreshConduitWorkspaceView();
                });

                std::string Label = Entry.DisplayName;
                if (Entry.RequiresSpecialization)
                {
                    Label += " *";
                }
                auto TextBuilder = ButtonBuilder.Add(SnAPI::UI::UIText(Label));
                TextBuilder.Element().ElementStyle().Apply("editor.menu_item");

                m_conduitVisiblePaletteStableIds.push_back(Entry.StableId);
            };

            const auto BuildPaletteGroup = [&](const auto& Self,
                                               auto& ParentBuilder,
                                               const ConduitPaletteCategoryNode& Node,
                                               const int Depth) -> void {
                for (const auto* Entry : Node.Entries)
                {
                    if (Entry)
                    {
                        AddPaletteEntryButton(ParentBuilder, *Entry, Depth > 0 ? 4.0f : 0.0f);
                    }
                }

                if (Node.Children.empty())
                {
                    return;
                }

                auto AccordionBuilder = ParentBuilder.Add(SnAPI::UI::UIAccordion{});
                auto& Accordion = AccordionBuilder.Element();
                Accordion.Width().Set(SnAPI::UI::Sizing::Fill());
                Accordion.Height().Set(SnAPI::UI::Sizing::Auto());
                Accordion.AllowMultipleExpanded().Set(true);
                Accordion.DefaultExpanded().Set(m_conduitPaletteFilterText.empty() ? Depth == 0 : true);
                Accordion.Padding().Set(0.0f);
                Accordion.Gap().Set(6.0f);
                Accordion.ContentPadding().Set(6.0f);

                for (const ConduitPaletteCategoryNode& Child : Node.Children)
                {
                    auto SectionBuilder = AccordionBuilder.Add(SnAPI::UI::UIPanel("Editor.ConduitPaletteCategory"));
                    if (SectionBuilder.Handle().Id.Value == 0)
                    {
                        continue;
                    }
                    auto& Section = SectionBuilder.Element();
                    Section.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
                    Section.Width().Set(SnAPI::UI::Sizing::Fill());
                    Section.Height().Set(SnAPI::UI::Sizing::Auto());
                    Section.Padding().Set(0.0f);
                    Section.Gap().Set(4.0f);

                    Accordion.SetSectionHeading(SectionBuilder.Handle().Id, Child.Label);
                    Accordion.SetSectionExpanded(SectionBuilder.Handle().Id, m_conduitPaletteFilterText.empty() ? Depth == 0 : true);
                    Self(Self, SectionBuilder, Child, Depth + 1);
                }
            };

            BuildPaletteGroup(BuildPaletteGroup, PaletteRootBuilder, PaletteRoot, 0);
        }
        else
        {
            SnAPI::UI::TElementBuilder<SnAPI::UI::UIPanel> PaletteRootBuilder(m_context, m_conduitPaletteContentPanel);
            auto HintBuilder = PaletteRootBuilder.Add(
                SnAPI::UI::UIText(m_conduitPaletteFilterText.empty() ? "No nodes available." : "No nodes match the current filter."));
            auto& Hint = HintBuilder.Element();
            Hint.ElementStyle().Apply("editor.panel_subtitle");
            Hint.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
        }

        if (m_conduitSelectedPaletteStableId.empty() && !m_conduitVisiblePaletteStableIds.empty())
        {
            m_conduitSelectedPaletteStableId = m_conduitVisiblePaletteStableIds.front();
        }
        else if (!m_conduitSelectedPaletteStableId.empty() &&
                 std::find(
                     m_conduitVisiblePaletteStableIds.begin(),
                     m_conduitVisiblePaletteStableIds.end(),
                     m_conduitSelectedPaletteStableId) == m_conduitVisiblePaletteStableIds.end())
        {
            m_conduitSelectedPaletteStableId = m_conduitVisiblePaletteStableIds.empty()
                ? std::string{}
                : m_conduitVisiblePaletteStableIds.front();
        }
    }

    RefreshConduitCanvasView();

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
    if (m_conduitNodeDefaultInputsPanel.Id.Value != 0)
    {
        DestroyDirectChildren(*m_context, m_conduitNodeDefaultInputsPanel.Id);
        SnAPI::UI::TElementBuilder<SnAPI::UI::UIPanel> DefaultsRoot(m_context, m_conduitNodeDefaultInputsPanel);

        auto TitleBuilder = DefaultsRoot.Add(SnAPI::UI::UIText("Input Defaults"));
        auto& TitleText = TitleBuilder.Element();
        TitleText.ElementStyle().Apply("editor.panel_subtitle");
        TitleText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 6.0f, 0.0f, 0.0f});

        const std::string EmptyHint = !ShowNodeInspector
            ? std::string("Select a node to configure fallback input values.")
            : (NodeInspector.InputDefaults.empty()
                   ? std::string("Selected node has no literal-capable input pins. Unwired compatible inputs default-construct when possible.")
                   : std::string("Fallback values are used only when the input pin is not connected."));
        auto HintBuilder = DefaultsRoot.Add(SnAPI::UI::UIText(EmptyHint));
        auto& HintText = HintBuilder.Element();
        HintText.ElementStyle().Apply("editor.panel_subtitle");
        HintText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

        for (const auto& Entry : NodeInspector.InputDefaults)
        {
            auto CardBuilder = DefaultsRoot.Add(SnAPI::UI::UIPanel("Editor.ConduitNodeInputDefaultCard"));
            auto& Card = CardBuilder.Element();
            Card.ElementStyle().Apply("editor.section_card");
            Card.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
            Card.Width().Set(SnAPI::UI::Sizing::Fill());
            Card.Height().Set(SnAPI::UI::Sizing::Auto());
            Card.Padding().Set(6.0f);
            Card.Gap().Set(4.0f);

            auto NameBuilder = CardBuilder.Add(SnAPI::UI::UIText(Entry.DisplayName + " : " + Entry.TypeLabel));
            NameBuilder.Element().ElementStyle().Apply("editor.menu_item");

            std::string StatusText = Entry.Connected
                ? "Connected. The authored fallback is kept, but the wire currently wins."
                : (Entry.HasDefault ? "Using authored fallback when disconnected."
                                    : "No explicit fallback. The compiler will default-construct this input when possible.");
            if (!Entry.Tooltip.empty())
            {
                StatusText += "\n" + Entry.Tooltip;
            }
            auto StatusBuilder = CardBuilder.Add(SnAPI::UI::UIText(StatusText));
            auto& StatusLabel = StatusBuilder.Element();
            StatusLabel.ElementStyle().Apply("editor.panel_subtitle");
            StatusLabel.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

            if (Entry.DefaultEditorKind == ConduitWorkspaceState::EVariableDefaultEditorKind::Bool)
            {
                auto CheckboxBuilder = CardBuilder.Add(SnAPI::UI::UICheckbox("Use explicit bool fallback"));
                auto& Checkbox = CheckboxBuilder.Element();
                Checkbox.Checked().Set(Entry.HasDefault ? Entry.BoolValue : false);
                Checkbox.OnChanged([this, PinKey = Entry.PinKey](const bool Checked) {
                    if (m_onConduitNodeDefaultBoolRequested)
                    {
                        m_onConduitNodeDefaultBoolRequested(PinKey, Checked);
                    }
                });
            }
            else if (Entry.DefaultEditorKind == ConduitWorkspaceState::EVariableDefaultEditorKind::Text)
            {
                auto TextBuilder = CardBuilder.Add(SnAPI::UI::UITextInput{});
                auto& TextInput = TextBuilder.Element();
                TextInput.Width().Set(SnAPI::UI::Sizing::Fill());
                TextInput.Placeholder().Set(std::string("Press Enter to apply fallback value"));
                TextInput.Text().Set(Entry.TextValue);
                TextInput.OnSubmit(SnAPI::UI::TDelegate<void(const std::string&)>::Bind(
                    [this, PinKey = Entry.PinKey](const std::string& Text) {
                        if (m_onConduitNodeDefaultTextRequested)
                        {
                            m_onConduitNodeDefaultTextRequested(PinKey, Text);
                        }
                    }));
                TextInput.OnFocusStateChanged(SnAPI::UI::TDelegate<void(bool)>::Bind(
                    [this, PinKey = Entry.PinKey, Handle = TextBuilder.Handle()](const bool Focused) {
                        if (Focused || !m_onConduitNodeDefaultTextRequested || !m_context)
                        {
                            return;
                        }
                        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(Handle.Id)))
                        {
                            m_onConduitNodeDefaultTextRequested(PinKey, Input->Text().Get());
                        }
                    }));
            }
            else if (Entry.DefaultEditorKind == ConduitWorkspaceState::EVariableDefaultEditorKind::Enum)
            {
                auto ComboBuilder = CardBuilder.Add(SnAPI::UI::UIComboBox{});
                auto& Combo = ComboBuilder.Element();
                Combo.Width().Set(SnAPI::UI::Sizing::Fill());
                Combo.Placeholder().Set(std::string("Enum fallback"));
                Combo.SetItems(Entry.EnumOptions);
                (void)Combo.SetSelectedIndex(Entry.SelectedEnumIndex, false);
                Combo.OnChanged([this, PinKey = Entry.PinKey](const int32_t Index, const std::string& Text) {
                    if (Index >= 0 && m_onConduitNodeDefaultEnumRequested)
                    {
                        m_onConduitNodeDefaultEnumRequested(PinKey, Text);
                    }
                });
            }
            else
            {
                auto UnsupportedBuilder = CardBuilder.Add(SnAPI::UI::UIText(
                    "No inline editor for this type yet. Leave it unwired to use implicit default construction."));
                auto& UnsupportedText = UnsupportedBuilder.Element();
                UnsupportedText.ElementStyle().Apply("editor.panel_subtitle");
                UnsupportedText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
            }

            auto ClearButtonBuilder = CardBuilder.Add(SnAPI::UI::UIButton{});
            auto& ClearButton = ClearButtonBuilder.Element();
            ClearButton.ElementStyle().Apply("editor.toolbar_button");
            ClearButton.Width().Set(SnAPI::UI::Sizing::Auto());
            ClearButton.ElementPadding().Set(SnAPI::UI::Padding{6.0f, 4.0f, 6.0f, 4.0f});
            ClearButton.OnClick([this, PinKey = Entry.PinKey]() {
                if (m_onConduitNodeDefaultClearRequested)
                {
                    m_onConduitNodeDefaultClearRequested(PinKey);
                }
            });
            auto ClearLabelBuilder = ClearButtonBuilder.Add(SnAPI::UI::UIText("Clear Explicit Fallback"));
            ClearLabelBuilder.Element().ElementStyle().Apply("editor.menu_item");
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

void EditorLayout::RefreshConduitCanvasView()
{
    if (!m_context || m_conduitGraphCanvas.Id.Value == 0)
    {
        return;
    }

    auto* Canvas = dynamic_cast<Conduit::Editor::UIConduitGraphCanvas*>(&m_context->GetElement(m_conduitGraphCanvas.Id));
    if (!Canvas)
    {
        return;
    }

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
                .Tooltip = Pin.Tooltip,
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
                .Tooltip = Pin.Tooltip,
                .Kind = Pin.Kind,
                .IsInput = Pin.IsInput,
                .IsExec = Pin.IsExec,
            });
        }

        CanvasView.Nodes.push_back(Conduit::Editor::CanvasNodeView{
            .Id = Node.Id,
            .Title = Node.Title,
            .Detail = Node.Detail,
            .Tooltip = Node.Tooltip,
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
        .Id = std::string(kContextMenuItemFileNewPluginId),
        .Label = "New Plugin...",
        .Shortcut = std::nullopt,
        .Enabled = true,
        .IsSeparator = false,
        .Checked = false,
    });
    Items.push_back(SnAPI::UI::UIContextMenuItem{
        .Id = std::string(kContextMenuItemFileAddModuleId),
        .Label = "Add Module...",
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
        .Id = std::string(kContextMenuItemFilePackageProjectId),
        .Label = "Package Project...",
        .Shortcut = std::nullopt,
        .Enabled = m_projectState.IsLoaded,
        .IsSeparator = false,
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

void EditorLayout::OpenConduitSpawnMenu(
    const SnAPI::GameFramework::Conduit::Editor::GraphSpawnMenuRequest& Request,
    std::vector<SnAPI::GameFramework::Conduit::Editor::SpawnMenuEntryView> Entries)
{
    CloseContextMenu();
    m_contextMenuScope = EContextMenuScope::ConduitCanvasSpawn;
    m_contextMenuConduitSpawnRequest = Request;
    m_contextMenuConduitSpawnEntries = std::move(Entries);

    OpenContextMenu(
        SnAPI::UI::UIPoint{Request.ScreenX, Request.ScreenY},
        BuildConduitSpawnMenuItems(Request, m_contextMenuConduitSpawnEntries));
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
    m_contextMenuConduitSpawnRequest = {};
    m_contextMenuConduitSpawnEntries.clear();
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
        else if (Item.Id == kContextMenuItemFileNewPluginId)
        {
            OpenPluginCreateModal();
        }
        else if (Item.Id == kContextMenuItemFileAddModuleId)
        {
            OpenProjectModuleModal();
        }
        else if (Item.Id == kContextMenuItemFilePackageProjectId)
        {
            OpenBuildModal();
        }
        else if (Item.Id == kContextMenuItemFileProjectSettingsId)
        {
            OpenProjectSettingsModal();
        }
        return;
    }

    if (m_contextMenuScope == EContextMenuScope::ConduitCanvasSpawn)
    {
        if (const auto SpawnIndex = TryParsePrefixedIndex(Item.Id, kContextMenuItemConduitSpawnPrefix))
        {
            if (m_onConduitSpawnMenuSelectionRequested && *SpawnIndex < m_contextMenuConduitSpawnEntries.size())
            {
                m_onConduitSpawnMenuSelectionRequested(
                    m_contextMenuConduitSpawnRequest,
                    m_contextMenuConduitSpawnEntries[*SpawnIndex]);
            }
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
    if (!m_context || m_contentAssetsGrid.Id.Value == 0)
    {
        return;
    }

    SnAPI::UI::TElementBuilder<SnAPI::UI::UIGrid> AssetsListBuilder(
        m_context,
        SnAPI::UI::ElementHandle<SnAPI::UI::UIGrid>{m_contentAssetsGrid.Id});

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
        CardButtonElement.SetDragPayloadBuilder(
            [this, CardIndex](std::string& OutPayloadType, std::any& OutPayload) -> bool {
                if (CardIndex >= m_contentBrowserEntries.size() || CardIndex >= m_contentAssetCardIndices.size())
                {
                    return false;
                }

                const ContentBrowserEntry& Entry = m_contentBrowserEntries[CardIndex];
                if (Entry.IsFolder || Entry.AssetIndex >= m_contentAssets.size())
                {
                    return false;
                }

                OutPayloadType = std::string(kContentAssetDragPayloadType);
                OutPayload = ContentAssetDragPayload{.AssetKey = m_contentAssets[Entry.AssetIndex].Key};
                return true;
            });

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
    OverlayPanel.Resizable().Set(true);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 22, 30, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(87, 97, 112, 245));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(10.0f);
    OverlayPanel.ContentPadding().Set(14.0f);
    ConfigureModalScreenRatio(OverlayPanel, 0.62f);
    m_projectModalOverlay = Overlay.Handle();

    auto Modal = Overlay.Add(SnAPI::UI::UIPanel("Editor.ProjectModal"));
    auto& ModalPanel = Modal.Element();
    ModalPanel.ElementStyle().Apply("editor.project_modal_root");
    ModalPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ModalPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Padding().Set(12.0f);
    ModalPanel.Gap().Set(10.0f);

    const bool IsCreate = m_projectModalAction == EProjectAction::CreateNew;

    auto HeaderRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.Header"));
    auto& HeaderRowPanel = HeaderRow.Element();
    ConfigureTransparentLayoutPanel(HeaderRowPanel);
    HeaderRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HeaderRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderRowPanel.Gap().Set(10.0f);

    auto HeaderIcon = HeaderRow.Add(SnAPI::UI::UIImage(
        ResolveUIImageSource(IsCreate ? kProjectWelcomeCreateIconPath : kProjectWelcomeOpenIconPath)));
    auto& HeaderIconImage = HeaderIcon.Element();
    ConfigureSvgIcon(HeaderIconImage, 20.0f, SnAPI::UI::Color::RGB(230, 206, 162));
    HeaderIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto HeaderTextHost = HeaderRow.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.HeaderText"));
    auto& HeaderTextHostPanel = HeaderTextHost.Element();
    ConfigureTransparentLayoutPanel(HeaderTextHostPanel);
    HeaderTextHostPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    HeaderTextHostPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    HeaderTextHostPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderTextHostPanel.Gap().Set(2.0f);

    auto Title = HeaderTextHost.Add(SnAPI::UI::UIText(IsCreate ? "Create Project" : "Open Project"));
    auto& TitleText = Title.Element();
    TitleText.ElementStyle().Apply("editor.project_welcome_title");
    TitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    TitleText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    const std::string SubtitleTextValue = m_projectModalRequired
                                              ? std::string("Create or open a project before continuing.")
                                              : std::string(IsCreate
                                                                ? "Shape the workspace, starter code, and startup content before the project is created."
                                                                : "Load an existing project descriptor and restore the editor to that workspace.");
    auto Subtitle = HeaderTextHost.Add(SnAPI::UI::UIText(SubtitleTextValue));
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

    auto CreateModeButton = ModeRow.Add(SnAPI::UI::UIButton{});
    auto& CreateModeButtonElement = CreateModeButton.Element();
    CreateModeButtonElement.ElementStyle().Apply(IsCreate ? "editor.project_modal_mode_button_active"
                                                          : "editor.project_modal_mode_button");
    CreateModeButtonElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    CreateModeButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 6.0f, 10.0f, 6.0f});
    CreateModeButtonElement.OnClick([this, IsCreate]() {
        if (!IsCreate)
        {
            OpenProjectCreateModal();
        }
    });
    auto CreateModeLabel = CreateModeButton.Add(SnAPI::UI::UIText("New Workspace"));
    CreateModeLabel.Element().ElementStyle().Apply("editor.project_modal_mode_button_text");
    CreateModeLabel.Element().TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
    CreateModeLabel.Element().Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto OpenModeButton = ModeRow.Add(SnAPI::UI::UIButton{});
    auto& OpenModeButtonElement = OpenModeButton.Element();
    OpenModeButtonElement.ElementStyle().Apply(IsCreate ? "editor.project_modal_mode_button"
                                                        : "editor.project_modal_mode_button_active");
    OpenModeButtonElement.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    OpenModeButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 6.0f, 10.0f, 6.0f});
    OpenModeButtonElement.OnClick([this, IsCreate]() {
        if (IsCreate)
        {
            OpenProjectOpenModal();
        }
    });
    auto OpenModeLabel = OpenModeButton.Add(SnAPI::UI::UIText("Open Existing"));
    OpenModeLabel.Element().ElementStyle().Apply("editor.project_modal_mode_button_text");
    OpenModeLabel.Element().TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
    OpenModeLabel.Element().Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto BodyScroll = Modal.Add(SnAPI::UI::UIScrollContainer{});
    auto& BodyScrollElement = BodyScroll.Element();
    BodyScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    BodyScrollElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    BodyScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    BodyScrollElement.ShowHorizontalScrollbar().Set(false);
    BodyScrollElement.ShowVerticalScrollbar().Set(true);
    BodyScrollElement.Smooth().Set(true);
    BodyScrollElement.Padding().Set(2.0f);
    BodyScrollElement.Gap().Set(10.0f);

    if (IsCreate)
    {
        auto TopRow = BodyScroll.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.TopRow"));
        auto& TopRowPanel = TopRow.Element();
        ConfigureTransparentLayoutPanel(TopRowPanel);
        TopRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
        TopRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        TopRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        TopRowPanel.Gap().Set(10.0f);

        auto IdentityCard = TopRow.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.IdentityCard"));
        auto& IdentityCardPanel = IdentityCard.Element();
        IdentityCardPanel.ElementStyle().Apply("editor.section_card");
        IdentityCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        IdentityCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        IdentityCardPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        IdentityCardPanel.Padding().Set(12.0f);
        IdentityCardPanel.Gap().Set(8.0f);

        auto IdentityTitle = IdentityCard.Add(SnAPI::UI::UIText("Identity"));
        IdentityTitle.Element().ElementStyle().Apply("editor.panel_title");
        AddFieldHelpText(IdentityCard,
                         "Project identity becomes the canonical descriptor name, root workspace folder, and default "
                         "type/module stem for generated starter code.");

        auto NameLabel = IdentityCard.Add(SnAPI::UI::UIText("Project Name"));
        NameLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(IdentityCard,
                         "Stable project identifier used for the workspace folder, descriptor identity, and default "
                         "starter module name.");
        auto NameInput = IdentityCard.Add(SnAPI::UI::UITextInput{});
        auto& NameInputElement = NameInput.Element();
        NameInputElement.ElementStyle().Apply("editor.text_input");
        NameInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        NameInputElement.Placeholder().Set("MyGame");
        NameInputElement.Text().Set(m_projectNameText);
        NameInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_projectNameText = Value;
            if (TrimCopy(m_projectDisplayNameText).empty())
            {
                m_projectDisplayNameText = Value;
            }
            if (TrimCopy(m_projectRuntimeModuleText).empty())
            {
                m_projectRuntimeModuleText = Value;
            }
            if (TrimCopy(m_projectNamespaceText).empty())
            {
                m_projectNamespaceText = Value;
            }
            RefreshProjectModalOkButtonState();
            if (m_context)
            {
                m_context->MarkLayoutDirty();
            }
        }));
        m_projectNameInput = NameInput.Handle();

        auto DisplayNameLabel = IdentityCard.Add(SnAPI::UI::UIText("Display Name"));
        DisplayNameLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(IdentityCard,
                         "Human-facing project title shown in editor surfaces and package metadata.");
        auto DisplayNameInput = IdentityCard.Add(SnAPI::UI::UITextInput{});
        auto& DisplayNameInputElement = DisplayNameInput.Element();
        DisplayNameInputElement.ElementStyle().Apply("editor.text_input");
        DisplayNameInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        DisplayNameInputElement.Placeholder().Set("My Game");
        DisplayNameInputElement.Text().Set(m_projectDisplayNameText);
        DisplayNameInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_projectDisplayNameText = Value;
        }));
        m_projectDisplayNameInput = DisplayNameInput.Handle();

        auto CompanyLabel = IdentityCard.Add(SnAPI::UI::UIText("Company"));
        CompanyLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(IdentityCard,
                         "Optional studio or organization name written into the project descriptor.");
        auto CompanyInput = IdentityCard.Add(SnAPI::UI::UITextInput{});
        auto& CompanyInputElement = CompanyInput.Element();
        CompanyInputElement.ElementStyle().Apply("editor.text_input");
        CompanyInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        CompanyInputElement.Placeholder().Set("Studio Name");
        CompanyInputElement.Text().Set(m_projectCompanyText);
        CompanyInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_projectCompanyText = Value;
        }));
        m_projectCompanyInput = CompanyInput.Handle();

        auto LocationCard = TopRow.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.LocationCard"));
        auto& LocationCardPanel = LocationCard.Element();
        LocationCardPanel.ElementStyle().Apply("editor.section_card");
        LocationCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        LocationCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        LocationCardPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        LocationCardPanel.Padding().Set(12.0f);
        LocationCardPanel.Gap().Set(8.0f);

        auto LocationTitle = LocationCard.Add(SnAPI::UI::UIText("Location"));
        LocationTitle.Element().ElementStyle().Apply("editor.panel_title");
        AddFieldHelpText(LocationCard,
                         "Location fields control where the workspace root and descriptor are materialized on disk.");

        auto ShapeLabel = LocationCard.Add(SnAPI::UI::UIText("Project Shape"));
        ShapeLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(LocationCard,
                         "Choose whether the new workspace starts as a runtime game, a runtime plus editor game, or a "
                         "content-only project.");
        auto ShapeComboBuilder = LocationCard.Add(SnAPI::UI::UIComboBox{});
        auto& ShapeCombo = ShapeComboBuilder.Element();
        ShapeCombo.Width().Set(SnAPI::UI::Sizing::Fill());
        ShapeCombo.SetItems({"Runtime Game", "Runtime + Editor Game", "Content Only"});
        (void)ShapeCombo.SetSelectedIndex(static_cast<int32_t>(m_projectTemplatePreset), false);
        ShapeCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
            (void)Text;
            ApplyProjectTemplatePreset(Index);
            DestroyProjectModalOverlay();
            RefreshProjectModalVisibility();
            RefreshProjectModalOkButtonState();
            if (m_context)
            {
                m_context->MarkLayoutDirty();
            }
        });
        m_projectTemplateCombo = ShapeComboBuilder.Handle();

        auto DirectoryLabel = LocationCard.Add(SnAPI::UI::UIText("Parent Directory"));
        DirectoryLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(LocationCard,
                         "The project root is created beneath this directory as `<Parent>/<ProjectName>`.");
        auto DirectoryInput = LocationCard.Add(SnAPI::UI::UIFilesystemPicker{});
        auto& DirectoryInputElement = DirectoryInput.Element();
        DirectoryInputElement.ElementStyle().Apply("editor.filesystem_picker");
        DirectoryInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
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
                        if (m_context)
                        {
                            m_context->MarkLayoutDirty();
                        }
                    }
                }));
        m_projectDirectoryInput = DirectoryInput.Handle();

        std::string PreviewPath = TrimCopy(m_projectDirectoryText);
        if (!PreviewPath.empty() && !TrimCopy(m_projectNameText).empty())
        {
            PreviewPath = (std::filesystem::path(PreviewPath) / TrimCopy(m_projectNameText)).lexically_normal().string();
        }
        else
        {
            PreviewPath = "Choose a parent directory to preview the new workspace root.";
        }
        auto PreviewText = LocationCard.Add(SnAPI::UI::UIText(PreviewPath));
        auto& PreviewTextElement = PreviewText.Element();
        PreviewTextElement.ElementStyle().Apply("editor.panel_subtitle");
        PreviewTextElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

        auto CodeRow = BodyScroll.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.CodeRow"));
        auto& CodeRowPanel = CodeRow.Element();
        ConfigureTransparentLayoutPanel(CodeRowPanel);
        CodeRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
        CodeRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        CodeRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        CodeRowPanel.Gap().Set(10.0f);

        auto CodeCard = CodeRow.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.CodeCard"));
        auto& CodeCardPanel = CodeCard.Element();
        CodeCardPanel.ElementStyle().Apply("editor.section_card");
        CodeCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        CodeCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        CodeCardPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        CodeCardPanel.Padding().Set(12.0f);
        CodeCardPanel.Gap().Set(8.0f);

        auto CodeTitle = CodeCard.Add(SnAPI::UI::UIText("Code"));
        CodeTitle.Element().ElementStyle().Apply("editor.panel_title");
        AddFieldHelpText(CodeCard,
                         "Starter code settings decide whether the new project is code-backed immediately and what "
                         "module/bootstrap names the generated files use.");

        auto NamespaceLabel = CodeCard.Add(SnAPI::UI::UIText("Namespace Root"));
        NamespaceLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(CodeCard,
                         "Namespace root used by generated starter classes such as the project game, game mode, and "
                         "module bootstrap types.");
        auto NamespaceInput = CodeCard.Add(SnAPI::UI::UITextInput{});
        auto& NamespaceInputElement = NamespaceInput.Element();
        NamespaceInputElement.ElementStyle().Apply("editor.text_input");
        NamespaceInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        NamespaceInputElement.Placeholder().Set("MyGame");
        NamespaceInputElement.Text().Set(m_projectNamespaceText);
        NamespaceInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_projectNamespaceText = Value;
        }));
        m_projectNamespaceInput = NamespaceInput.Handle();

        auto RuntimeCheckboxBuilder = CodeCard.Add(SnAPI::UI::UICheckbox("Generate starter runtime module"));
        auto& RuntimeCheckbox = RuntimeCheckboxBuilder.Element();
        RuntimeCheckbox.Checked().Set(m_projectCreateRuntimeModule);
        RuntimeCheckbox.OnChanged([this](const bool Checked) {
            m_projectCreateRuntimeModule = Checked;
            RefreshProjectModalOkButtonState();
        });
        m_projectRuntimeModuleCheckbox = RuntimeCheckboxBuilder.Handle();
        AddFieldHelpText(CodeCard,
                         "When enabled, project creation emits a runtime module with CMake wiring plus starter "
                         "`IGame` and `IGameMode` implementations.");

        auto RuntimeModuleLabel = CodeCard.Add(SnAPI::UI::UIText("Runtime Module Name"));
        RuntimeModuleLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(CodeCard,
                         "Primary runtime module target name and source folder stem.");
        auto RuntimeModuleInput = CodeCard.Add(SnAPI::UI::UITextInput{});
        auto& RuntimeModuleInputElement = RuntimeModuleInput.Element();
        RuntimeModuleInputElement.ElementStyle().Apply("editor.text_input");
        RuntimeModuleInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        RuntimeModuleInputElement.Placeholder().Set("MyGame");
        RuntimeModuleInputElement.Text().Set(m_projectRuntimeModuleText);
        RuntimeModuleInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_projectRuntimeModuleText = Value;
            RefreshProjectModalOkButtonState();
        }));
        RuntimeModuleInputElement.SetDisabled(!m_projectCreateRuntimeModule);
        m_projectRuntimeModuleInput = RuntimeModuleInput.Handle();

        auto EditorCheckboxBuilder = CodeCard.Add(SnAPI::UI::UICheckbox("Generate companion editor module"));
        auto& EditorCheckbox = EditorCheckboxBuilder.Element();
        EditorCheckbox.Checked().Set(m_projectCreateEditorModule);
        EditorCheckbox.OnChanged([this](const bool Checked) {
            m_projectCreateEditorModule = Checked;
            RefreshProjectModalOkButtonState();
            if (m_context)
            {
                m_context->MarkLayoutDirty();
            }
        });
        m_projectEditorModuleCheckbox = EditorCheckboxBuilder.Handle();
        AddFieldHelpText(CodeCard,
                         "Emit an editor-only companion module for custom tools, inspectors, or content workflows.");

        auto EditorModuleLabel = CodeCard.Add(SnAPI::UI::UIText("Editor Module Name"));
        EditorModuleLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(CodeCard,
                         "Editor companion target name. This module is linked into editor hosts only.");
        auto EditorModuleInput = CodeCard.Add(SnAPI::UI::UITextInput{});
        auto& EditorModuleInputElement = EditorModuleInput.Element();
        EditorModuleInputElement.ElementStyle().Apply("editor.text_input");
        EditorModuleInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        EditorModuleInputElement.Placeholder().Set("MyGameEditor");
        EditorModuleInputElement.Text().Set(m_projectEditorModuleText);
        EditorModuleInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_projectEditorModuleText = Value;
            RefreshProjectModalOkButtonState();
        }));
        EditorModuleInputElement.SetDisabled(!m_projectCreateEditorModule);
        m_projectEditorModuleInput = EditorModuleInput.Handle();

        auto StartupCard = CodeRow.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.StartupCard"));
        auto& StartupCardPanel = StartupCard.Element();
        StartupCardPanel.ElementStyle().Apply("editor.section_card");
        StartupCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        StartupCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        StartupCardPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        StartupCardPanel.Padding().Set(12.0f);
        StartupCardPanel.Gap().Set(8.0f);

        auto StartupTitle = StartupCard.Add(SnAPI::UI::UIText("Startup"));
        StartupTitle.Element().ElementStyle().Apply("editor.panel_title");
        AddFieldHelpText(StartupCard,
                         "Startup content settings seed the descriptor values used when the project boots in editor or "
                         "runtime mode.");

        auto StartupLevelLabel = StartupCard.Add(SnAPI::UI::UIText("Startup Level Asset"));
        StartupLevelLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(StartupCard,
                         "Relative startup level asset field written into the descriptor. The source asset must exist "
                         "for validation and packaging to succeed.");
        auto StartupLevelInput = StartupCard.Add(SnAPI::UI::UITextInput{});
        auto& StartupLevelInputElement = StartupLevelInput.Element();
        StartupLevelInputElement.ElementStyle().Apply("editor.text_input");
        StartupLevelInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        StartupLevelInputElement.Placeholder().Set("Levels/StarterLevel.level");
        StartupLevelInputElement.Text().Set(m_projectStartupLevelText);
        StartupLevelInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_projectStartupLevelText = Value;
        }));
        m_projectStartupLevelInput = StartupLevelInput.Handle();

        const std::string RuntimeSummary = m_projectCreateRuntimeModule
            ? ("Runtime module `" + TrimCopy(m_projectRuntimeModuleText.empty() ? m_projectNameText : m_projectRuntimeModuleText) +
               "` with starter game/bootstrap code.")
            : std::string("Content-only project. No starter runtime module will be generated.");
        const std::string EditorSummary = m_projectCreateEditorModule
            ? ("\nEditor module `" + TrimCopy(m_projectEditorModuleText.empty()
                                                    ? ((m_projectRuntimeModuleText.empty() ? m_projectNameText : m_projectRuntimeModuleText) + "Editor")
                                                    : m_projectEditorModuleText) + "` will be generated.")
            : std::string("\nNo editor companion module.");
        auto SummaryText = StartupCard.Add(SnAPI::UI::UIText(RuntimeSummary + EditorSummary));
        auto& SummaryTextElement = SummaryText.Element();
        SummaryTextElement.ElementStyle().Apply("editor.panel_subtitle");
        SummaryTextElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    }
    else
    {
        auto OpenCard = BodyScroll.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.OpenCard"));
        auto& OpenCardPanel = OpenCard.Element();
        OpenCardPanel.ElementStyle().Apply("editor.section_card");
        OpenCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        OpenCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        OpenCardPanel.Height().Set(SnAPI::UI::Sizing::Auto());
        OpenCardPanel.Padding().Set(12.0f);
        OpenCardPanel.Gap().Set(8.0f);

        auto FileLabel = OpenCard.Add(SnAPI::UI::UIText("Project File"));
        FileLabel.Element().ElementStyle().Apply("editor.menu_item");
        auto FileInput = OpenCard.Add(SnAPI::UI::UIFilesystemPicker{});
        auto& FileInputElement = FileInput.Element();
        FileInputElement.ElementStyle().Apply("editor.filesystem_picker");
        FileInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        FileInputElement.ReadOnly().Set(false);
        FileInputElement.AllowMultiSelect().Set(false);
        FileInputElement.PickDirectories().Set(false);
        FileInputElement.ShowDirectories().Set(true);
        FileInputElement.ShowFiles().Set(true);
        FileInputElement.RestrictToRoot().Set(false);
        FileInputElement.Placeholder().Set(std::string("Path to project.snproj.json"));
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

        auto OpenHint = OpenCard.Add(SnAPI::UI::UIText(
            "Opening a project reloads the asset manager, loads the startup level, and applies the project's default render settings when configured."));
        auto& OpenHintElement = OpenHint.Element();
        OpenHintElement.ElementStyle().Apply("editor.panel_subtitle");
        OpenHintElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    }

    auto ButtonsRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.Buttons"));
    auto& ButtonsRowPanel = ButtonsRow.Element();
    ConfigureTransparentLayoutPanel(ButtonsRowPanel);
    ButtonsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ButtonsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ButtonsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ButtonsRowPanel.Gap().Set(8.0f);

    auto Spacer = ButtonsRow.Add(SnAPI::UI::UIPanel("Editor.ProjectModal.ButtonSpacer"));
    ConfigureLayoutSpacerPanel(Spacer.Element());
    Spacer.Element().Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto CancelButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& CancelButtonElement = CancelButton.Element();
    CancelButtonElement.ElementStyle().Apply("editor.project_modal_action_button");
    CancelButtonElement.ElementPadding().Set(SnAPI::UI::Padding{12.0f, 6.0f, 12.0f, 6.0f});
    CancelButtonElement.SetDisabled(m_projectModalRequired);
    CancelButtonElement.OnClick([this]() {
        CloseProjectModal();
    });
    auto CancelLabel = CancelButton.Add(SnAPI::UI::UIText("Cancel"));
    CancelLabel.Element().ElementStyle().Apply("editor.project_modal_action_button_text");
    CancelLabel.Element().Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto OkButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    auto& OkButtonElement = OkButton.Element();
    OkButtonElement.ElementStyle().Apply("editor.project_modal_action_button_primary");
    OkButtonElement.ElementPadding().Set(SnAPI::UI::Padding{12.0f, 6.0f, 12.0f, 6.0f});
    OkButtonElement.OnClick([this]() {
        ConfirmProjectModal();
    });
    auto OkLabel = OkButton.Add(SnAPI::UI::UIText(IsCreate ? "Create Project" : "Open Project"));
    OkLabel.Element().ElementStyle().Apply("editor.project_modal_action_button_text");
    OkLabel.Element().Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);
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
    m_projectDisplayNameInput = {};
    m_projectCompanyInput = {};
    m_projectNamespaceInput = {};
    m_projectRuntimeModuleInput = {};
    m_projectEditorModuleInput = {};
    m_projectDirectoryInput = {};
    m_projectFilePathInput = {};
    m_projectStartupLevelInput = {};
    m_projectTemplateCombo = {};
    m_projectRuntimeModuleCheckbox = {};
    m_projectEditorModuleCheckbox = {};
    m_projectModalOkButton = {};
}

void EditorLayout::EnsurePluginModalOverlay()
{
    if (!m_context || m_pluginModalOverlay.Id.Value != 0 || !m_pluginModalOpen)
    {
        return;
    }

    auto Root = m_context->Root();
    auto Overlay = Root.Add(SnAPI::UI::UIModal{});
    auto& OverlayPanel = Overlay.Element();
    OverlayPanel.Movable().Set(true);
    OverlayPanel.Resizable().Set(true);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 22, 30, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(87, 97, 112, 245));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(10.0f);
    OverlayPanel.ContentPadding().Set(14.0f);
    ConfigureModalScreenRatio(OverlayPanel, 0.58f);
    m_pluginModalOverlay = Overlay.Handle();

    auto Modal = Overlay.Add(SnAPI::UI::UIPanel("Editor.PluginModal"));
    auto& ModalPanel = Modal.Element();
    ModalPanel.ElementStyle().Apply("editor.project_modal_root");
    ModalPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ModalPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Padding().Set(12.0f);
    ModalPanel.Gap().Set(10.0f);

    auto Header = Modal.Add(SnAPI::UI::UIPanel("Editor.PluginModal.Header"));
    auto& HeaderPanel = Header.Element();
    ConfigureTransparentLayoutPanel(HeaderPanel);
    HeaderPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HeaderPanel.Gap().Set(10.0f);

    auto HeaderIcon = Header.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kContentBrowserIconPath)));
    auto& HeaderIconImage = HeaderIcon.Element();
    ConfigureSvgIcon(HeaderIconImage, 20.0f, SnAPI::UI::Color::RGB(230, 206, 162));
    HeaderIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto HeaderText = Header.Add(SnAPI::UI::UIPanel("Editor.PluginModal.HeaderText"));
    auto& HeaderTextPanel = HeaderText.Element();
    ConfigureTransparentLayoutPanel(HeaderTextPanel);
    HeaderTextPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    HeaderTextPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    HeaderTextPanel.Gap().Set(2.0f);

    auto Title = HeaderText.Add(SnAPI::UI::UIText("Create Plugin"));
    Title.Element().ElementStyle().Apply("editor.project_welcome_title");
    Title.Element().Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto Subtitle = HeaderText.Add(SnAPI::UI::UIText(
        "Create a runtime, editor, hybrid, or content-only plugin with generated module scaffolding and build wiring."));
    Subtitle.Element().ElementStyle().Apply("editor.project_welcome_subtitle");
    Subtitle.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto Scroll = Modal.Add(SnAPI::UI::UIScrollContainer{});
    auto& ScrollElement = Scroll.Element();
    ScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ScrollElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ScrollElement.ShowHorizontalScrollbar().Set(false);
    ScrollElement.ShowVerticalScrollbar().Set(true);
    ScrollElement.Smooth().Set(true);
    ScrollElement.Padding().Set(2.0f);
    ScrollElement.Gap().Set(10.0f);

    auto TopRow = Scroll.Add(SnAPI::UI::UIPanel("Editor.PluginModal.TopRow"));
    auto& TopRowPanel = TopRow.Element();
    ConfigureTransparentLayoutPanel(TopRowPanel);
    TopRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    TopRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    TopRowPanel.Gap().Set(10.0f);

    auto IdentityCard = TopRow.Add(SnAPI::UI::UIPanel("Editor.PluginModal.IdentityCard"));
    auto& IdentityCardPanel = IdentityCard.Element();
    IdentityCardPanel.ElementStyle().Apply("editor.section_card");
    IdentityCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    IdentityCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    IdentityCardPanel.Padding().Set(12.0f);
    IdentityCardPanel.Gap().Set(8.0f);

    auto IdentityTitle = IdentityCard.Add(SnAPI::UI::UIText("Identity"));
    IdentityTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(IdentityCard,
                     "Plugin identity fields become the canonical plugin descriptor values and seed default starter "
                     "module names.");

    auto PluginNameLabel = IdentityCard.Add(SnAPI::UI::UIText("Plugin Name"));
    PluginNameLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(IdentityCard,
                     "Stable plugin identifier used for the root folder, descriptor identity, and default runtime "
                     "module stem.");
    auto PluginNameInput = IdentityCard.Add(SnAPI::UI::UITextInput{});
    auto& PluginNameInputElement = PluginNameInput.Element();
    PluginNameInputElement.ElementStyle().Apply("editor.text_input");
    PluginNameInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    PluginNameInputElement.Placeholder().Set("MyPlugin");
    PluginNameInputElement.Text().Set(m_pluginNameText);
    PluginNameInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_pluginNameText = Value;
        if (TrimCopy(m_pluginDisplayNameText).empty())
        {
            m_pluginDisplayNameText = Value;
        }
        if (TrimCopy(m_pluginRuntimeModuleText).empty())
        {
            m_pluginRuntimeModuleText = Value;
        }
        if (TrimCopy(m_pluginNamespaceText).empty())
        {
            m_pluginNamespaceText = Value;
        }
        RefreshPluginModalOkButtonState();
    }));
    m_pluginNameInput = PluginNameInput.Handle();

    auto PluginDisplayLabel = IdentityCard.Add(SnAPI::UI::UIText("Display Name"));
    PluginDisplayLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(IdentityCard,
                     "Human-facing plugin title shown in editor-facing plugin lists and metadata.");
    auto PluginDisplayInput = IdentityCard.Add(SnAPI::UI::UITextInput{});
    auto& PluginDisplayInputElement = PluginDisplayInput.Element();
    PluginDisplayInputElement.ElementStyle().Apply("editor.text_input");
    PluginDisplayInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    PluginDisplayInputElement.Placeholder().Set("My Plugin");
    PluginDisplayInputElement.Text().Set(m_pluginDisplayNameText);
    PluginDisplayInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_pluginDisplayNameText = Value;
    }));
    m_pluginDisplayNameInput = PluginDisplayInput.Handle();

    auto PluginCompanyLabel = IdentityCard.Add(SnAPI::UI::UIText("Company"));
    PluginCompanyLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(IdentityCard,
                     "Optional vendor or organization name written into the plugin descriptor.");
    auto PluginCompanyInput = IdentityCard.Add(SnAPI::UI::UITextInput{});
    auto& PluginCompanyInputElement = PluginCompanyInput.Element();
    PluginCompanyInputElement.ElementStyle().Apply("editor.text_input");
    PluginCompanyInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    PluginCompanyInputElement.Placeholder().Set("Studio Name");
    PluginCompanyInputElement.Text().Set(m_pluginCompanyText);
    PluginCompanyInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_pluginCompanyText = Value;
    }));
    m_pluginCompanyInput = PluginCompanyInput.Handle();

    auto VersionLabel = IdentityCard.Add(SnAPI::UI::UIText("Version"));
    VersionLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(IdentityCard,
                     "Semantic or studio-defined plugin version string persisted into the descriptor.");
    auto VersionInput = IdentityCard.Add(SnAPI::UI::UITextInput{});
    auto& VersionInputElement = VersionInput.Element();
    VersionInputElement.ElementStyle().Apply("editor.text_input");
    VersionInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    VersionInputElement.Placeholder().Set("0.1.0");
    VersionInputElement.Text().Set(m_pluginVersionText);
    VersionInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_pluginVersionText = Value;
    }));
    m_pluginVersionInput = VersionInput.Handle();

    auto LocationCard = TopRow.Add(SnAPI::UI::UIPanel("Editor.PluginModal.LocationCard"));
    auto& LocationCardPanel = LocationCard.Element();
    LocationCardPanel.ElementStyle().Apply("editor.section_card");
    LocationCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    LocationCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    LocationCardPanel.Padding().Set(12.0f);
    LocationCardPanel.Gap().Set(8.0f);

    auto LocationTitle = LocationCard.Add(SnAPI::UI::UIText("Shape and Location"));
    LocationTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(LocationCard,
                     "Plugin shape controls which starter modules are emitted, while the location chooses where the "
                     "workspace is materialized on disk.");

    auto ShapeLabel = LocationCard.Add(SnAPI::UI::UIText("Plugin Shape"));
    ShapeLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(LocationCard,
                     "Choose whether the plugin is runtime-only, editor-only, hybrid, or content-focused.");
    auto ShapeComboBuilder = LocationCard.Add(SnAPI::UI::UIComboBox{});
    auto& ShapeCombo = ShapeComboBuilder.Element();
    ShapeCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    ShapeCombo.SetItems({"Runtime Plugin", "Editor Tool", "Hybrid Plugin", "Content Plugin"});
    (void)ShapeCombo.SetSelectedIndex(static_cast<int32_t>(m_pluginTemplatePreset), false);
    ShapeCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        ApplyPluginTemplatePreset(Index);
        DestroyPluginModalOverlay();
        RefreshPluginModalVisibility();
        RefreshPluginModalOkButtonState();
        if (m_context)
        {
            m_context->MarkLayoutDirty();
        }
    });
    m_pluginTemplateCombo = ShapeComboBuilder.Handle();

    auto DirectoryLabel = LocationCard.Add(SnAPI::UI::UIText("Parent Directory"));
    DirectoryLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(LocationCard,
                     "The plugin root is created beneath this directory as `<Parent>/<PluginName>`.");
    auto DirectoryInput = LocationCard.Add(SnAPI::UI::UIFilesystemPicker{});
    auto& DirectoryInputElement = DirectoryInput.Element();
    DirectoryInputElement.ElementStyle().Apply("editor.filesystem_picker");
    DirectoryInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    DirectoryInputElement.ReadOnly().Set(false);
    DirectoryInputElement.AllowMultiSelect().Set(false);
    DirectoryInputElement.PickDirectories().Set(true);
    DirectoryInputElement.ShowDirectories().Set(true);
    DirectoryInputElement.ShowFiles().Set(false);
    DirectoryInputElement.RestrictToRoot().Set(false);
    DirectoryInputElement.Placeholder().Set(std::string("Path to parent folder"));
    DirectoryInputElement.Value().Set(m_pluginDirectoryText);
    DirectoryInputElement.CurrentPath().Set(m_pluginDirectoryText);
    DirectoryInputElement.OnSelectionChanged(
        SnAPI::UI::TDelegate<void(const std::vector<std::string>&)>::Bind([this](const std::vector<std::string>& Values) {
            if (!Values.empty())
            {
                m_pluginDirectoryText = Values.front();
                RefreshPluginModalOkButtonState();
            }
        }));
    m_pluginDirectoryInput = DirectoryInput.Handle();

    auto AssetsCheckboxBuilder = LocationCard.Add(SnAPI::UI::UICheckbox("Plugin can contain assets"));
    auto& AssetsCheckbox = AssetsCheckboxBuilder.Element();
    AssetsCheckbox.Checked().Set(m_pluginCanContainAssets);
    AssetsCheckbox.OnChanged([this](const bool Checked) {
        m_pluginCanContainAssets = Checked;
    });
    m_pluginCanContainAssetsCheckbox = AssetsCheckboxBuilder.Handle();
    AddFieldHelpText(LocationCard,
                     "Enable this when the plugin should own authored assets in addition to code modules.");

    auto DescriptionLabel = Scroll.Add(SnAPI::UI::UIText("Description"));
    DescriptionLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(Scroll,
                     "Short human-readable summary of what the plugin provides. This is persisted into plugin "
                     "metadata and later package/manifests can surface it.");
    auto DescriptionInput = Scroll.Add(SnAPI::UI::UITextInput{});
    auto& DescriptionInputElement = DescriptionInput.Element();
    DescriptionInputElement.ElementStyle().Apply("editor.text_input");
    DescriptionInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    DescriptionInputElement.Multiline().Set(true);
    DescriptionInputElement.Text().Set(m_pluginDescriptionText);
    DescriptionInputElement.Placeholder().Set("What does this plugin provide?");
    DescriptionInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_pluginDescriptionText = Value;
    }));
    m_pluginDescriptionInput = DescriptionInput.Handle();

    auto CodeCard = Scroll.Add(SnAPI::UI::UIPanel("Editor.PluginModal.CodeCard"));
    auto& CodeCardPanel = CodeCard.Element();
    CodeCardPanel.ElementStyle().Apply("editor.section_card");
    CodeCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    CodeCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    CodeCardPanel.Padding().Set(12.0f);
    CodeCardPanel.Gap().Set(8.0f);

    auto CodeTitle = CodeCard.Add(SnAPI::UI::UIText("Code"));
    CodeTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(CodeCard,
                     "Code settings decide which starter plugin modules are emitted and how their generated type names "
                     "are composed.");

    auto NamespaceLabel = CodeCard.Add(SnAPI::UI::UIText("Namespace Root"));
    NamespaceLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(CodeCard,
                     "Namespace root used by generated plugin runtime/editor module starter classes.");
    auto NamespaceInput = CodeCard.Add(SnAPI::UI::UITextInput{});
    auto& NamespaceInputElement = NamespaceInput.Element();
    NamespaceInputElement.ElementStyle().Apply("editor.text_input");
    NamespaceInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    NamespaceInputElement.Placeholder().Set("MyPlugin");
    NamespaceInputElement.Text().Set(m_pluginNamespaceText);
    NamespaceInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_pluginNamespaceText = Value;
    }));
    m_pluginNamespaceInput = NamespaceInput.Handle();

    auto RuntimeCheckboxBuilder = CodeCard.Add(SnAPI::UI::UICheckbox("Generate runtime module"));
    auto& RuntimeCheckbox = RuntimeCheckboxBuilder.Element();
    RuntimeCheckbox.Checked().Set(m_pluginCreateRuntimeModule);
    RuntimeCheckbox.OnChanged([this](const bool Checked) {
        m_pluginCreateRuntimeModule = Checked;
        RefreshPluginModalOkButtonState();
    });
    m_pluginRuntimeModuleCheckbox = RuntimeCheckboxBuilder.Handle();
    AddFieldHelpText(CodeCard,
                     "Emit one starter runtime plugin module plus generated CMake wiring.");

    auto RuntimeModuleLabel = CodeCard.Add(SnAPI::UI::UIText("Runtime Module Name"));
    RuntimeModuleLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(CodeCard,
                     "Primary runtime plugin target name and generated source folder stem.");
    auto RuntimeModuleInput = CodeCard.Add(SnAPI::UI::UITextInput{});
    auto& RuntimeModuleInputElement = RuntimeModuleInput.Element();
    RuntimeModuleInputElement.ElementStyle().Apply("editor.text_input");
    RuntimeModuleInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    RuntimeModuleInputElement.Placeholder().Set("MyPlugin");
    RuntimeModuleInputElement.Text().Set(m_pluginRuntimeModuleText);
    RuntimeModuleInputElement.SetDisabled(!m_pluginCreateRuntimeModule);
    RuntimeModuleInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_pluginRuntimeModuleText = Value;
        RefreshPluginModalOkButtonState();
    }));
    m_pluginRuntimeModuleInput = RuntimeModuleInput.Handle();

    auto EditorCheckboxBuilder = CodeCard.Add(SnAPI::UI::UICheckbox("Generate editor module"));
    auto& EditorCheckbox = EditorCheckboxBuilder.Element();
    EditorCheckbox.Checked().Set(m_pluginCreateEditorModule);
    EditorCheckbox.OnChanged([this](const bool Checked) {
        m_pluginCreateEditorModule = Checked;
        RefreshPluginModalOkButtonState();
        if (m_context)
        {
            m_context->MarkLayoutDirty();
        }
    });
    m_pluginEditorModuleCheckbox = EditorCheckboxBuilder.Handle();
    AddFieldHelpText(CodeCard,
                     "Emit one editor-only companion plugin module for tools and authoring integrations.");

    auto EditorModuleLabel = CodeCard.Add(SnAPI::UI::UIText("Editor Module Name"));
    EditorModuleLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(CodeCard,
                     "Editor companion target name. This module is linked into editor hosts only.");
    auto EditorModuleInput = CodeCard.Add(SnAPI::UI::UITextInput{});
    auto& EditorModuleInputElement = EditorModuleInput.Element();
    EditorModuleInputElement.ElementStyle().Apply("editor.text_input");
    EditorModuleInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    EditorModuleInputElement.Placeholder().Set("MyPluginEditor");
    EditorModuleInputElement.Text().Set(m_pluginEditorModuleText);
    EditorModuleInputElement.SetDisabled(!m_pluginCreateEditorModule);
    EditorModuleInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_pluginEditorModuleText = Value;
        RefreshPluginModalOkButtonState();
    }));
    m_pluginEditorModuleInput = EditorModuleInput.Handle();

    auto ButtonsRow = Modal.Add(SnAPI::UI::UIPanel("Editor.PluginModal.Buttons"));
    auto& ButtonsRowPanel = ButtonsRow.Element();
    ConfigureTransparentLayoutPanel(ButtonsRowPanel);
    ButtonsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ButtonsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ButtonsRowPanel.Gap().Set(8.0f);

    auto Spacer = ButtonsRow.Add(SnAPI::UI::UIPanel("Editor.PluginModal.ButtonSpacer"));
    ConfigureLayoutSpacerPanel(Spacer.Element());
    Spacer.Element().Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto CancelButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    CancelButton.Element().ElementStyle().Apply("editor.project_modal_action_button");
    CancelButton.Element().ElementPadding().Set(SnAPI::UI::Padding{12.0f, 6.0f, 12.0f, 6.0f});
    CancelButton.Element().OnClick([this]() {
        ClosePluginModal();
    });
    auto CancelLabel = CancelButton.Add(SnAPI::UI::UIText("Cancel"));
    CancelLabel.Element().ElementStyle().Apply("editor.project_modal_action_button_text");
    CancelLabel.Element().Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto OkButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    OkButton.Element().ElementStyle().Apply("editor.project_modal_action_button_primary");
    OkButton.Element().ElementPadding().Set(SnAPI::UI::Padding{12.0f, 6.0f, 12.0f, 6.0f});
    OkButton.Element().OnClick([this]() {
        ConfirmPluginModal();
    });
    auto OkLabel = OkButton.Add(SnAPI::UI::UIText("Create Plugin"));
    OkLabel.Element().ElementStyle().Apply("editor.project_modal_action_button_text");
    OkLabel.Element().Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);
    m_pluginModalOkButton = OkButton.Handle();

    RefreshPluginModalOkButtonState();
}

void EditorLayout::DestroyPluginModalOverlay()
{
    if (m_context && m_pluginModalOverlay.Id.Value != 0)
    {
        const SnAPI::UI::ElementId OverlayId = m_pluginModalOverlay.Id;
        const SnAPI::UI::ElementId CapturedElement = m_context->GetCapture();
        if (IsElementWithinSubtree(*m_context, CapturedElement, OverlayId))
        {
            m_context->ReleaseCapture();
        }
        m_context->DestroyElement(OverlayId);
    }

    m_pluginModalOverlay = {};
    m_pluginNameInput = {};
    m_pluginDisplayNameInput = {};
    m_pluginCompanyInput = {};
    m_pluginVersionInput = {};
    m_pluginDescriptionInput = {};
    m_pluginNamespaceInput = {};
    m_pluginRuntimeModuleInput = {};
    m_pluginEditorModuleInput = {};
    m_pluginDirectoryInput = {};
    m_pluginTemplateCombo = {};
    m_pluginRuntimeModuleCheckbox = {};
    m_pluginEditorModuleCheckbox = {};
    m_pluginCanContainAssetsCheckbox = {};
    m_pluginModalOkButton = {};
}

void EditorLayout::EnsureModuleModalOverlay()
{
    if (!m_context || m_moduleModalOverlay.Id.Value != 0 || !m_moduleModalOpen)
    {
        return;
    }

    auto Root = m_context->Root();
    auto Overlay = Root.Add(SnAPI::UI::UIModal{});
    auto& OverlayPanel = Overlay.Element();
    OverlayPanel.Movable().Set(true);
    OverlayPanel.Resizable().Set(true);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 22, 30, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(87, 97, 112, 245));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(10.0f);
    OverlayPanel.ContentPadding().Set(14.0f);
    ConfigureModalScreenRatio(OverlayPanel, 0.60f);
    m_moduleModalOverlay = Overlay.Handle();

    auto Modal = Overlay.Add(SnAPI::UI::UIPanel("Editor.ModuleModal"));
    auto& ModalPanel = Modal.Element();
    ModalPanel.ElementStyle().Apply("editor.project_modal_root");
    ModalPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ModalPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Padding().Set(12.0f);
    ModalPanel.Gap().Set(10.0f);

    const bool IsPluginModule = m_moduleModalAction == EModuleAction::CreatePluginModule;

    auto Header = Modal.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.Header"));
    auto& HeaderPanel = Header.Element();
    ConfigureTransparentLayoutPanel(HeaderPanel);
    HeaderPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HeaderPanel.Gap().Set(10.0f);

    auto HeaderIcon = Header.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kHierarchyNodeIconPath)));
    auto& HeaderIconImage = HeaderIcon.Element();
    ConfigureSvgIcon(HeaderIconImage, 20.0f, SnAPI::UI::Color::RGB(230, 206, 162));
    HeaderIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto HeaderText = Header.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.HeaderText"));
    auto& HeaderTextPanel = HeaderText.Element();
    ConfigureTransparentLayoutPanel(HeaderTextPanel);
    HeaderTextPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    HeaderTextPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    HeaderTextPanel.Gap().Set(2.0f);

    auto Title = HeaderText.Add(SnAPI::UI::UIText(IsPluginModule ? "Add Plugin Module" : "Add Project Module"));
    Title.Element().ElementStyle().Apply("editor.project_welcome_title");
    Title.Element().Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto Subtitle = HeaderText.Add(SnAPI::UI::UIText(
        "Generate module source, CMake fragments, and descriptor entries with type-specific starter hooks."));
    Subtitle.Element().ElementStyle().Apply("editor.project_welcome_subtitle");
    Subtitle.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto Scroll = Modal.Add(SnAPI::UI::UIScrollContainer{});
    auto& ScrollElement = Scroll.Element();
    ScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ScrollElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ScrollElement.ShowHorizontalScrollbar().Set(false);
    ScrollElement.ShowVerticalScrollbar().Set(true);
    ScrollElement.Smooth().Set(true);
    ScrollElement.Padding().Set(2.0f);
    ScrollElement.Gap().Set(10.0f);

    auto TopCard = Scroll.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.TopCard"));
    auto& TopCardPanel = TopCard.Element();
    TopCardPanel.ElementStyle().Apply("editor.section_card");
    TopCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    TopCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    TopCardPanel.Padding().Set(12.0f);
    TopCardPanel.Gap().Set(8.0f);

    auto TargetRow = TopCard.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.TargetRow"));
    auto& TargetRowPanel = TargetRow.Element();
    ConfigureTransparentLayoutPanel(TargetRowPanel);
    TargetRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    TargetRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    TargetRowPanel.Gap().Set(10.0f);

    auto TargetModeCard = TargetRow.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.TargetModeCard"));
    auto& TargetModeCardPanel = TargetModeCard.Element();
    TargetModeCardPanel.ElementStyle().Apply("editor.section_card");
    TargetModeCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    TargetModeCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(0.42f));
    TargetModeCardPanel.Padding().Set(10.0f);
    TargetModeCardPanel.Gap().Set(8.0f);

    auto TargetModeLabel = TargetModeCard.Add(SnAPI::UI::UIText("Target"));
    TargetModeLabel.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(TargetModeCard,
                     "Choose whether the new module will be authored into the loaded project or an existing plugin "
                     "descriptor.");
    auto TargetComboBuilder = TargetModeCard.Add(SnAPI::UI::UIComboBox{});
    auto& TargetCombo = TargetComboBuilder.Element();
    TargetCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    TargetCombo.SetItems({"Project", "Plugin"});
    (void)TargetCombo.SetSelectedIndex(IsPluginModule ? 1 : 0, false);
    TargetCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        m_moduleModalAction = Index == 1 ? EModuleAction::CreatePluginModule : EModuleAction::CreateProjectModule;
        DestroyModuleModalOverlay();
        RefreshModuleModalVisibility();
        RefreshModuleModalOkButtonState();
        if (m_context)
        {
            m_context->MarkLayoutDirty();
        }
    });
    m_moduleTargetCombo = TargetComboBuilder.Handle();

    auto TargetDescriptorCard = TargetRow.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.TargetDescriptorCard"));
    auto& TargetDescriptorCardPanel = TargetDescriptorCard.Element();
    TargetDescriptorCardPanel.ElementStyle().Apply("editor.section_card");
    TargetDescriptorCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    TargetDescriptorCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(0.58f));
    TargetDescriptorCardPanel.Padding().Set(10.0f);
    TargetDescriptorCardPanel.Gap().Set(8.0f);

    auto TargetDescriptorLabel = TargetDescriptorCard.Add(SnAPI::UI::UIText(IsPluginModule ? "Plugin Descriptor" : "Project Descriptor"));
    TargetDescriptorLabel.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(TargetDescriptorCard,
                     "Point this dialog at the descriptor file that should be updated. The service will append the "
                     "module declaration, emit starter source, and regenerate workspace build wiring.");
    auto TargetDescriptorPicker = TargetDescriptorCard.Add(SnAPI::UI::UIFilesystemPicker{});
    auto& TargetDescriptorPickerElement = TargetDescriptorPicker.Element();
    TargetDescriptorPickerElement.ElementStyle().Apply("editor.filesystem_picker");
    TargetDescriptorPickerElement.Width().Set(SnAPI::UI::Sizing::Fill());
    TargetDescriptorPickerElement.ReadOnly().Set(false);
    TargetDescriptorPickerElement.AllowMultiSelect().Set(false);
    TargetDescriptorPickerElement.PickDirectories().Set(false);
    TargetDescriptorPickerElement.ShowDirectories().Set(true);
    TargetDescriptorPickerElement.ShowFiles().Set(true);
    TargetDescriptorPickerElement.RestrictToRoot().Set(false);
    TargetDescriptorPickerElement.Placeholder().Set(IsPluginModule ? std::string("Path to plugin.snplugin.json")
                                                                  : std::string("Path to project.snproj.json"));
    TargetDescriptorPickerElement.Value().Set(m_moduleDescriptorFilePathText);
    TargetDescriptorPickerElement.CurrentPath().Set(std::filesystem::path(m_moduleDescriptorFilePathText).parent_path().string());
    TargetDescriptorPickerElement.SetAllowedExtensions({".json"});
    TargetDescriptorPickerElement.OnSelectionChanged(
        SnAPI::UI::TDelegate<void(const std::vector<std::string>&)>::Bind([this](const std::vector<std::string>& Values) {
            if (!Values.empty())
            {
                m_moduleDescriptorFilePathText = Values.front();
                RefreshModuleModalOkButtonState();
            }
        }));
    m_moduleDescriptorFileInput = TargetDescriptorPicker.Handle();

    auto IdentityRow = Scroll.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.IdentityRow"));
    auto& IdentityRowPanel = IdentityRow.Element();
    ConfigureTransparentLayoutPanel(IdentityRowPanel);
    IdentityRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    IdentityRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    IdentityRowPanel.Gap().Set(10.0f);

    auto IdentityCard = IdentityRow.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.IdentityCard"));
    auto& IdentityCardPanel = IdentityCard.Element();
    IdentityCardPanel.ElementStyle().Apply("editor.section_card");
    IdentityCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    IdentityCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    IdentityCardPanel.Padding().Set(12.0f);
    IdentityCardPanel.Gap().Set(8.0f);

    auto IdentityTitle = IdentityCard.Add(SnAPI::UI::UIText("Module Identity"));
    IdentityTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto NameLabel = IdentityCard.Add(SnAPI::UI::UIText("Module Name"));
    NameLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(IdentityCard,
                     "This becomes the stable module identifier, default namespace stem, generated class names, and "
                     "CMake target name.");
    auto NameInput = IdentityCard.Add(SnAPI::UI::UITextInput{});
    auto& NameInputElement = NameInput.Element();
    NameInputElement.ElementStyle().Apply("editor.text_input");
    NameInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    NameInputElement.Placeholder().Set("GameplaySystems");
    NameInputElement.Text().Set(m_moduleNameText);
    NameInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_moduleNameText = Value;
        if (TrimCopy(m_moduleNamespaceText).empty())
        {
            m_moduleNamespaceText = Value;
        }
        RefreshModuleModalOkButtonState();
    }));
    m_moduleNameInput = NameInput.Handle();

    auto NamespaceLabel = IdentityCard.Add(SnAPI::UI::UIText("Namespace Root"));
    NamespaceLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(IdentityCard,
                     "Override the generated C++ namespace root. Leave it aligned with the module name unless the "
                     "workspace uses a different namespace policy.");
    auto NamespaceInput = IdentityCard.Add(SnAPI::UI::UITextInput{});
    auto& NamespaceInputElement = NamespaceInput.Element();
    NamespaceInputElement.ElementStyle().Apply("editor.text_input");
    NamespaceInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    NamespaceInputElement.Placeholder().Set("GameplaySystems");
    NamespaceInputElement.Text().Set(m_moduleNamespaceText);
    NamespaceInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_moduleNamespaceText = Value;
    }));
    m_moduleNamespaceInput = NamespaceInput.Handle();

    auto RootLabel = IdentityCard.Add(SnAPI::UI::UIText("Module Root Override"));
    RootLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(IdentityCard,
                     "Optional descriptor-relative module root. When left empty the module is written under the host "
                     "workspace code root as `Code/<ModuleName>`.");
    auto RootInput = IdentityCard.Add(SnAPI::UI::UITextInput{});
    auto& RootInputElement = RootInput.Element();
    RootInputElement.ElementStyle().Apply("editor.text_input");
    RootInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    RootInputElement.Placeholder().Set("Code/GameplaySystems");
    RootInputElement.Text().Set(m_moduleRootText);
    RootInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_moduleRootText = Value;
    }));
    m_moduleRootInput = RootInput.Handle();

    auto TypeCard = IdentityRow.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.TypeCard"));
    auto& TypeCardPanel = TypeCard.Element();
    TypeCardPanel.ElementStyle().Apply("editor.section_card");
    TypeCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    TypeCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    TypeCardPanel.Padding().Set(12.0f);
    TypeCardPanel.Gap().Set(8.0f);

    auto TypeTitle = TypeCard.Add(SnAPI::UI::UIText("Module Type"));
    TypeTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(TypeCard,
                     "Module type controls default host linkage, intended usage, and starter scaffolding shape.");

    auto TypeComboBuilder = TypeCard.Add(SnAPI::UI::UIComboBox{});
    auto& TypeCombo = TypeComboBuilder.Element();
    TypeCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    TypeCombo.SetItems({"Runtime", "Editor", "Shared", "Developer", "Test", "Program"});
    (void)TypeCombo.SetSelectedIndex(ModuleTypeToIndex(m_moduleType), false);
    TypeCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        m_moduleType = ModuleTypeFromIndex(Index);
        m_moduleGenerateGameplayBootstrap = m_moduleType == EProjectModuleType::Runtime;
        m_moduleLoadInEditor = DefaultLoadInEditorForModule(m_moduleType);
        m_moduleLoadInRuntime = DefaultLoadInRuntimeForModule(m_moduleType);
        DestroyModuleModalOverlay();
        RefreshModuleModalVisibility();
        RefreshModuleModalOkButtonState();
        if (m_context)
        {
            m_context->MarkLayoutDirty();
        }
    });
    m_moduleTypeCombo = TypeComboBuilder.Handle();

    auto ReflectionCheckbox = TypeCard.Add(SnAPI::UI::UICheckbox("Use reflection generation"));
    ReflectionCheckbox.Element().Checked().Set(m_moduleUseReflectionGen);
    ReflectionCheckbox.Element().OnChanged([this](const bool Checked) {
        m_moduleUseReflectionGen = Checked;
    });
    m_moduleReflectionCheckbox = ReflectionCheckbox.Handle();
    AddFieldHelpText(TypeCard,
                     "Enable the reflection/code-generation pipeline for this module before C++ compilation.");

    auto SwigCheckbox = TypeCard.Add(SnAPI::UI::UICheckbox("Generate SWIG bindings"));
    SwigCheckbox.Element().Checked().Set(m_moduleUseSwig);
    SwigCheckbox.Element().OnChanged([this](const bool Checked) {
        m_moduleUseSwig = Checked;
    });
    m_moduleSwigCheckbox = SwigCheckbox.Handle();
    AddFieldHelpText(TypeCard,
                     "Opt the module into SWIG binding generation when the workspace uses script or interop wrappers.");

    if (m_moduleType == EProjectModuleType::Runtime)
    {
        auto GameplayBootstrapCheckbox = TypeCard.Add(SnAPI::UI::UICheckbox("Generate Game + GameMode starter code"));
        GameplayBootstrapCheckbox.Element().Checked().Set(m_moduleGenerateGameplayBootstrap);
        GameplayBootstrapCheckbox.Element().OnChanged([this](const bool Checked) {
            m_moduleGenerateGameplayBootstrap = Checked;
        });
        m_moduleGameplayBootstrapCheckbox = GameplayBootstrapCheckbox.Handle();
        AddFieldHelpText(TypeCard,
                         "Emit starter `IGame` and `IGameMode` implementations alongside the module class so a new "
                         "runtime module can bootstrap gameplay immediately.");
    }

    auto LoadEditorCheckbox = TypeCard.Add(SnAPI::UI::UICheckbox("Load in editor host"));
    LoadEditorCheckbox.Element().Checked().Set(m_moduleLoadInEditor);
    LoadEditorCheckbox.Element().OnChanged([this](const bool Checked) {
        m_moduleLoadInEditor = Checked;
    });
    m_moduleLoadInEditorCheckbox = LoadEditorCheckbox.Handle();
    AddFieldHelpText(TypeCard,
                     "Control whether the generated module is linked into editor-host builds by default.");

    auto LoadRuntimeCheckbox = TypeCard.Add(SnAPI::UI::UICheckbox("Load in runtime host"));
    LoadRuntimeCheckbox.Element().Checked().Set(m_moduleLoadInRuntime);
    LoadRuntimeCheckbox.Element().OnChanged([this](const bool Checked) {
        m_moduleLoadInRuntime = Checked;
    });
    m_moduleLoadInRuntimeCheckbox = LoadRuntimeCheckbox.Handle();
    AddFieldHelpText(TypeCard,
                     "Control whether the generated module is linked into packaged/runtime builds by default.");

    auto DetailsCard = Scroll.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.DetailsCard"));
    auto& DetailsCardPanel = DetailsCard.Element();
    DetailsCardPanel.ElementStyle().Apply("editor.section_card");
    DetailsCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    DetailsCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    DetailsCardPanel.Padding().Set(12.0f);
    DetailsCardPanel.Gap().Set(8.0f);

    auto DetailsTitle = DetailsCard.Add(SnAPI::UI::UIText("Dependencies and Flags"));
    DetailsTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(DetailsCard,
                     "List-shaped fields are edited as tokens so each dependency, platform filter, or definition stays "
                     "explicit and easy to review.");

    const auto AddTokenInput = [&DetailsCard](const char* Label,
                                              const char* HelpText,
                                              const std::string& Value,
                                              const std::function<void(const std::vector<std::string>&)>& OnChanged) {
        auto FieldLabel = DetailsCard.Add(SnAPI::UI::UIText(Label));
        FieldLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(DetailsCard, HelpText);
        auto FieldInput = DetailsCard.Add(SnAPI::UI::UITokenField{});
        auto& FieldInputElement = FieldInput.Element();
        FieldInputElement.ElementStyle().Apply("editor.token_field");
        FieldInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        FieldInputElement.Placeholder().Set("Type and press Enter");
        FieldInputElement.SetTokens(ParseMultilineEntries(Value), false);
        FieldInputElement.OnTokensChanged(
            SnAPI::UI::TDelegate<void(const std::vector<std::string>&)>::Bind(OnChanged));
        return FieldInput;
    };

    m_modulePublicDependenciesInput = AddTokenInput(
        "Public Dependencies",
        "Dependencies listed here are linked publicly and exposed through the module's public headers.",
        m_modulePublicDependenciesText,
        [this](const std::vector<std::string>& Values) { m_modulePublicDependenciesText = JoinEntries(Values); }).Handle();
    m_modulePrivateDependenciesInput = AddTokenInput(
        "Private Dependencies",
        "Dependencies listed here are linked privately and should not leak through public headers.",
        m_modulePrivateDependenciesText,
        [this](const std::vector<std::string>& Values) { m_modulePrivateDependenciesText = JoinEntries(Values); }).Handle();
    m_modulePlatformsInput = AddTokenInput(
        "Platform Filters",
        "Optional allow/deny platform entries copied into the descriptor. Use these only when the module truly "
        "cannot participate on every platform.",
        m_modulePlatformsText,
        [this](const std::vector<std::string>& Values) { m_modulePlatformsText = JoinEntries(Values); }).Handle();
    m_moduleDefinitionsInput = AddTokenInput(
        "Preprocessor Definitions",
        "Compile definitions are emitted as `target_compile_definitions(... PRIVATE ...)` entries in the generated "
        "module fragment.",
        m_moduleDefinitionsText,
        [this](const std::vector<std::string>& Values) { m_moduleDefinitionsText = JoinEntries(Values); }).Handle();

    auto ButtonsRow = Modal.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.Buttons"));
    auto& ButtonsRowPanel = ButtonsRow.Element();
    ConfigureTransparentLayoutPanel(ButtonsRowPanel);
    ButtonsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ButtonsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ButtonsRowPanel.Gap().Set(8.0f);

    auto Spacer = ButtonsRow.Add(SnAPI::UI::UIPanel("Editor.ModuleModal.ButtonSpacer"));
    ConfigureLayoutSpacerPanel(Spacer.Element());
    Spacer.Element().Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto CancelButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    CancelButton.Element().ElementStyle().Apply("editor.project_modal_action_button");
    CancelButton.Element().ElementPadding().Set(SnAPI::UI::Padding{12.0f, 6.0f, 12.0f, 6.0f});
    CancelButton.Element().OnClick([this]() {
        CloseModuleModal();
    });
    auto CancelLabel = CancelButton.Add(SnAPI::UI::UIText("Cancel"));
    CancelLabel.Element().ElementStyle().Apply("editor.project_modal_action_button_text");
    CancelLabel.Element().Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto OkButton = ButtonsRow.Add(SnAPI::UI::UIButton{});
    OkButton.Element().ElementStyle().Apply("editor.project_modal_action_button_primary");
    OkButton.Element().ElementPadding().Set(SnAPI::UI::Padding{12.0f, 6.0f, 12.0f, 6.0f});
    OkButton.Element().OnClick([this]() {
        ConfirmModuleModal();
    });
    auto OkLabel = OkButton.Add(SnAPI::UI::UIText(IsPluginModule ? "Add Plugin Module" : "Add Project Module"));
    OkLabel.Element().ElementStyle().Apply("editor.project_modal_action_button_text");
    OkLabel.Element().Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);
    m_moduleModalOkButton = OkButton.Handle();

    RefreshModuleModalOkButtonState();
}

void EditorLayout::DestroyModuleModalOverlay()
{
    if (m_context && m_moduleModalOverlay.Id.Value != 0)
    {
        const SnAPI::UI::ElementId OverlayId = m_moduleModalOverlay.Id;
        const SnAPI::UI::ElementId CapturedElement = m_context->GetCapture();
        if (IsElementWithinSubtree(*m_context, CapturedElement, OverlayId))
        {
            m_context->ReleaseCapture();
        }
        m_context->DestroyElement(OverlayId);
    }

    m_moduleModalOverlay = {};
    m_moduleTargetCombo = {};
    m_moduleTypeCombo = {};
    m_moduleNameInput = {};
    m_moduleNamespaceInput = {};
    m_moduleRootInput = {};
    m_moduleDescriptorFileInput = {};
    m_modulePublicDependenciesInput = {};
    m_modulePrivateDependenciesInput = {};
    m_modulePlatformsInput = {};
    m_moduleDefinitionsInput = {};
    m_moduleReflectionCheckbox = {};
    m_moduleSwigCheckbox = {};
    m_moduleGameplayBootstrapCheckbox = {};
    m_moduleLoadInEditorCheckbox = {};
    m_moduleLoadInRuntimeCheckbox = {};
    m_moduleModalOkButton = {};
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

const EditorLayout::BuildProfileEntry* EditorLayout::SelectedBuildProfileEntry() const
{
    auto SelectedIt = std::find_if(
        m_buildPanelState.Profiles.begin(),
        m_buildPanelState.Profiles.end(),
        [this](const BuildProfileEntry& Entry) { return Entry.Name == m_buildModalSelectedProfileName; });
    if (SelectedIt != m_buildPanelState.Profiles.end())
    {
        return std::addressof(*SelectedIt);
    }

    auto DefaultIt = std::find_if(
        m_buildPanelState.Profiles.begin(),
        m_buildPanelState.Profiles.end(),
        [](const BuildProfileEntry& Entry) { return Entry.IsDefault; });
    if (DefaultIt != m_buildPanelState.Profiles.end())
    {
        return std::addressof(*DefaultIt);
    }

    return m_buildPanelState.Profiles.empty() ? nullptr : std::addressof(m_buildPanelState.Profiles.front());
}

const EditorLayout::BuildHistoryEntryView* EditorLayout::SelectedBuildHistoryEntry() const
{
    auto SelectedIt = std::find_if(
        m_buildPanelState.HistoryEntries.begin(),
        m_buildPanelState.HistoryEntries.end(),
        [this](const BuildHistoryEntryView& Entry) { return Entry.BuildId == m_buildModalSelectedHistoryBuildId; });
    if (SelectedIt != m_buildPanelState.HistoryEntries.end())
    {
        return std::addressof(*SelectedIt);
    }

    auto LatestIt = std::find_if(
        m_buildPanelState.HistoryEntries.begin(),
        m_buildPanelState.HistoryEntries.end(),
        [](const BuildHistoryEntryView& Entry) { return Entry.IsLatest; });
    if (LatestIt != m_buildPanelState.HistoryEntries.end())
    {
        return std::addressof(*LatestIt);
    }

    return m_buildPanelState.HistoryEntries.empty() ? nullptr : std::addressof(m_buildPanelState.HistoryEntries.front());
}

bool EditorLayout::BuildModalRequiresStructuralRebuild(const BuildPanelState& PreviousState,
                                                       const BuildPanelState& NextState) const
{
    if (PreviousState.ProjectLoaded != NextState.ProjectLoaded || PreviousState.ProjectName != NextState.ProjectName ||
        PreviousState.ProjectFilePath != NextState.ProjectFilePath ||
        PreviousState.AssetRootDirectory != NextState.AssetRootDirectory)
    {
        return true;
    }

    const auto ProfilesEqual = [](const std::vector<BuildProfileEntry>& Left,
                                  const std::vector<BuildProfileEntry>& Right) -> bool {
        if (Left.size() != Right.size())
        {
            return false;
        }

        for (std::size_t Index = 0; Index < Left.size(); ++Index)
        {
            const BuildProfileEntry& A = Left[Index];
            const BuildProfileEntry& B = Right[Index];
            if (A.Name != B.Name || A.Label != B.Label || A.Summary != B.Summary || A.Platform != B.Platform ||
                A.Configuration != B.Configuration || A.ExecutionEnvironment != B.ExecutionEnvironment ||
                A.SelectedLevels != B.SelectedLevels || A.ExplicitAssets != B.ExplicitAssets ||
                A.IncludeFolders != B.IncludeFolders || A.ExcludeFolders != B.ExcludeFolders ||
                A.IncludeAssetLabels != B.IncludeAssetLabels || A.ExcludeAssetLabels != B.ExcludeAssetLabels ||
                A.IncludeAssetKinds != B.IncludeAssetKinds || A.ExcludeAssetKinds != B.ExcludeAssetKinds ||
                A.DependencyPolicy != B.DependencyPolicy || A.ChunkStrategy != B.ChunkStrategy ||
                A.AllowExplicitOverrideExcludes != B.AllowExplicitOverrideExcludes ||
                A.ArchiveEnabled != B.ArchiveEnabled || A.ArchiveFormat != B.ArchiveFormat ||
                A.IsDefault != B.IsDefault || A.IsAdHoc != B.IsAdHoc)
            {
                return false;
            }
        }

        return true;
    };

    const auto HistoryEqual = [](const std::vector<BuildHistoryEntryView>& Left,
                                 const std::vector<BuildHistoryEntryView>& Right) -> bool {
        if (Left.size() != Right.size())
        {
            return false;
        }

        for (std::size_t Index = 0; Index < Left.size(); ++Index)
        {
            const BuildHistoryEntryView& A = Left[Index];
            const BuildHistoryEntryView& B = Right[Index];
            if (A.BuildId != B.BuildId || A.Label != B.Label || A.Summary != B.Summary ||
                A.RequestHash != B.RequestHash || A.StartedAtUtc != B.StartedAtUtc ||
                A.FinishedAtUtc != B.FinishedAtUtc || A.IsComplete != B.IsComplete || A.IsLatest != B.IsLatest)
            {
                return false;
            }
        }

        return true;
    };

    return !ProfilesEqual(PreviousState.Profiles, NextState.Profiles) ||
           PreviousState.AvailableLevels != NextState.AvailableLevels ||
           PreviousState.AvailableAssets != NextState.AvailableAssets ||
           PreviousState.AvailableAssetKinds != NextState.AvailableAssetKinds ||
           !HistoryEqual(PreviousState.HistoryEntries, NextState.HistoryEntries);
}

void EditorLayout::RefreshBuildModalLiveState()
{
    if (!m_buildModalOpen || m_buildModalOverlay.Id.Value == 0 || !m_context)
    {
        return;
    }

    const auto SetTextHandle = [this](auto& Handle, const std::string& Value) {
        if (Handle.Id.Value == 0)
        {
            return;
        }

        if (auto* Text = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(Handle.Id)))
        {
            Text->Text().Set(Value);
        }
    };
    const auto SyncComboSelection = [this](auto& Handle, const int32_t Index) {
        if (Handle.Id.Value == 0)
        {
            return;
        }

        if (auto* Combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(Handle.Id)))
        {
            (void)Combo->SetSelectedIndex(Index, false);
        }
    };
    const auto SyncComboSelectionByText = [this](auto& Handle, const std::string& Value) {
        if (Handle.Id.Value == 0)
        {
            return;
        }

        auto* Combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(Handle.Id));
        if (Combo == nullptr)
        {
            return;
        }

        if (TrimCopy(Value).empty())
        {
            (void)Combo->SetSelectedIndex(0, false);
            return;
        }

        const std::vector<std::string>& Items = Combo->Items();
        const auto It = std::find(Items.begin(), Items.end(), TrimCopy(Value));
        if (It != Items.end())
        {
            (void)Combo->SetSelectedIndex(static_cast<int32_t>(std::distance(Items.begin(), It)), false);
        }
    };

    std::string SubtitleValue = m_buildPanelState.ProjectLoaded
                                    ? "Plan, package, and inspect build history for the active project."
                                    : "Load a project to access packaging and build history.";
    if (!m_buildPanelState.ProjectFilePath.empty())
    {
        SubtitleValue += " File: " + m_buildPanelState.ProjectFilePath;
    }
    SetTextHandle(m_buildModalSubtitleText, SubtitleValue);

    std::string OverviewSummary = m_buildPanelState.ProjectLoaded ? ("Project: " + m_buildPanelState.ProjectName)
                                                                  : std::string("No project is currently loaded.");
    if (m_buildPanelState.BuildInProgress)
    {
        OverviewSummary += "\nBackground task: build/package work is running on a worker thread.";
    }
    if (!m_buildPanelState.StatusMessage.empty())
    {
        OverviewSummary += "\nStatus: " + m_buildPanelState.StatusMessage;
    }
    SetTextHandle(m_buildModalOverviewSummaryText, OverviewSummary);

    std::string ProfileSummary = "No build profile is available for the active project.";
    if (const BuildProfileEntry* SelectedProfile = SelectedBuildProfileEntry())
    {
        ProfileSummary = SelectedProfile->Summary;
        const std::string EffectivePlatform =
            TrimCopy(m_buildModalPlatformText).empty()
                ? (SelectedProfile->Platform.empty() ? std::string("Inherited") : SelectedProfile->Platform)
                : TrimCopy(m_buildModalPlatformText);
        if (!EffectivePlatform.empty())
        {
            ProfileSummary += "\nResolved target: " + EffectivePlatform + " / " +
                              std::string(BuildConfigurationLabel(m_buildModalConfiguration));
        }
        const std::string EffectiveExecutionEnvironment =
            TrimCopy(m_buildModalExecutionEnvironmentText).empty()
                ? (SelectedProfile->ExecutionEnvironment.empty() ? std::string("Inherited / host-local")
                                                                 : SelectedProfile->ExecutionEnvironment)
                : TrimCopy(m_buildModalExecutionEnvironmentText);
        if (!EffectiveExecutionEnvironment.empty())
        {
            ProfileSummary += "\nExecution: " + EffectiveExecutionEnvironment;
        }
        ProfileSummary += "\nSelected levels: " + std::to_string(ParseMultilineEntries(m_buildModalSelectedLevelsText).size()) +
                          "  Explicit assets: " +
                          std::to_string(ParseMultilineEntries(m_buildModalExplicitAssetsText).size());
    }
    SetTextHandle(m_buildModalProfileSummaryText, ProfileSummary);

    std::string LatestSummary = m_buildPanelState.LastPlanSummary;
    if (!m_buildPanelState.LastBuildSummary.empty())
    {
        LatestSummary = m_buildPanelState.LastBuildSummary;
    }
    if (!m_buildPanelState.LastBuildOutputSummary.empty())
    {
        if (!LatestSummary.empty())
        {
            LatestSummary += "\n";
        }
        LatestSummary += m_buildPanelState.LastBuildOutputSummary;
    }
    if (LatestSummary.empty())
    {
        LatestSummary = "No build has been planned or executed in this editor session yet.";
    }
    SetTextHandle(m_buildModalLatestSummaryText, LatestSummary);

    std::string PlatformSummary = "Platform: " +
                                  (TrimCopy(m_buildModalPlatformText).empty() ? std::string("Inherited")
                                                                             : TrimCopy(m_buildModalPlatformText));
    PlatformSummary += "\nConfiguration: ";
    PlatformSummary += BuildConfigurationLabel(m_buildModalConfiguration);
    PlatformSummary += "\nExecution Environment: " +
                       (TrimCopy(m_buildModalExecutionEnvironmentText).empty() ? std::string("Inherited / host-local")
                                                                              : TrimCopy(m_buildModalExecutionEnvironmentText));
    SetTextHandle(m_buildModalPlatformSummaryText, PlatformSummary);

    std::string OutputSummary =
        "Packages promote the staged tree into a final output directory using the standard naming scheme.";
    if (!TrimCopy(m_buildModalPackageDirectoryText).empty())
    {
        OutputSummary += "\nDirectory override: " + TrimCopy(m_buildModalPackageDirectoryText);
    }
    if (!TrimCopy(m_buildModalOutputRootText).empty())
    {
        OutputSummary += "\nOutput root: " + TrimCopy(m_buildModalOutputRootText);
    }
    if (m_buildModalArchiveEnabled)
    {
        OutputSummary += "\nArchive: " +
                         (TrimCopy(m_buildModalArchiveFormatText).empty() ? std::string("zip")
                                                                          : TrimCopy(m_buildModalArchiveFormatText));
        if (!TrimCopy(m_buildModalArchiveFileText).empty())
        {
            OutputSummary += " -> " + TrimCopy(m_buildModalArchiveFileText);
        }
    }
    SetTextHandle(m_buildModalOutputSummaryText, OutputSummary);

    std::string HistoryDetailSummary = "Select a build from history to inspect, retry, or compare it.";
    if (const BuildHistoryEntryView* SelectedHistory = SelectedBuildHistoryEntry())
    {
        HistoryDetailSummary = SelectedHistory->Summary;
        if (!SelectedHistory->RequestHash.empty())
        {
            HistoryDetailSummary += "\nRequest: " + SelectedHistory->RequestHash;
        }
        if (!SelectedHistory->StartedAtUtc.empty())
        {
            HistoryDetailSummary += "\nStarted: " + SelectedHistory->StartedAtUtc;
        }
        if (!SelectedHistory->FinishedAtUtc.empty())
        {
            HistoryDetailSummary += "\nFinished: " + SelectedHistory->FinishedAtUtc;
        }
    }
    SetTextHandle(m_buildModalHistoryDetailText, HistoryDetailSummary);

    const std::string ComparisonSummary = m_buildPanelState.HistoryComparisonSummary.empty()
                                              ? std::string("No comparison result is currently loaded.")
                                              : m_buildPanelState.HistoryComparisonSummary;
    SetTextHandle(m_buildModalComparisonText, ComparisonSummary);

    const std::string ConsoleSummary =
        m_buildPanelState.ConsoleLogText.empty()
            ? std::string("No packaging output has been captured yet. Plan or package a build to populate this console.")
            : std::string("Captured output from the active project's most recent planning or packaging session.");
    SetTextHandle(m_buildModalConsoleSummaryText, ConsoleSummary);
    SetTextHandle(m_buildModalConsoleText,
                  m_buildPanelState.ConsoleLogText.empty() ? std::string("No packaging output has been captured yet.\n")
                                                           : m_buildPanelState.ConsoleLogText);
    if (m_buildModalConsoleScroll.Id.Value != 0 && m_context)
    {
        if (auto* Scroll =
                dynamic_cast<SnAPI::UI::UIScrollContainer*>(&m_context->GetElement(m_buildModalConsoleScroll.Id)))
        {
            Scroll->ScrollToEnd();
        }
    }

    SyncComboSelection(m_buildConfigurationCombo, BuildConfigurationToIndex(m_buildModalConfiguration));
    SyncComboSelection(m_buildDependencyPolicyCombo, DependencyPolicyToIndex(m_buildModalDependencyPolicy));
    SyncComboSelection(m_buildChunkStrategyCombo, ChunkStrategyToIndex(m_buildModalChunkStrategy));
    SyncComboSelectionByText(m_buildPlatformInput, m_buildModalPlatformText);
    SyncComboSelectionByText(m_buildArchiveFormatInput, m_buildModalArchiveFormatText);
    if (m_buildAllowExplicitOverrideCheckbox.Id.Value != 0)
    {
        if (auto* Checkbox =
                dynamic_cast<SnAPI::UI::UICheckbox*>(&m_context->GetElement(m_buildAllowExplicitOverrideCheckbox.Id)))
        {
            Checkbox->Checked().Set(m_buildModalAllowExplicitOverrideExcludes);
        }
    }
    if (m_buildArchiveEnabledCheckbox.Id.Value != 0)
    {
        if (auto* Checkbox =
                dynamic_cast<SnAPI::UI::UICheckbox*>(&m_context->GetElement(m_buildArchiveEnabledCheckbox.Id)))
        {
            Checkbox->Checked().Set(m_buildModalArchiveEnabled);
        }
    }
    if (m_buildExecutionEnvironmentInput.Id.Value != 0)
    {
        if (auto* Input =
                dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_buildExecutionEnvironmentInput.Id));
            Input != nullptr && Input->Text().Get() != m_buildModalExecutionEnvironmentText)
        {
            Input->Text().Set(m_buildModalExecutionEnvironmentText);
        }
    }
    if (m_buildOutputRootInput.Id.Value != 0)
    {
        if (auto* Picker =
                dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_context->GetElement(m_buildOutputRootInput.Id)))
        {
            Picker->Value().Set(m_buildModalOutputRootText);
            if (!TrimCopy(m_buildModalOutputRootText).empty())
            {
                Picker->CurrentPath().Set(m_buildModalOutputRootText);
            }
        }
    }
    if (m_buildPackageDirectoryInput.Id.Value != 0)
    {
        if (auto* Input =
                dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_buildPackageDirectoryInput.Id));
            Input != nullptr && Input->Text().Get() != m_buildModalPackageDirectoryText)
        {
            Input->Text().Set(m_buildModalPackageDirectoryText);
        }
    }
    if (m_buildArchiveFileInput.Id.Value != 0)
    {
        if (auto* Input = dynamic_cast<SnAPI::UI::UITextInput*>(&m_context->GetElement(m_buildArchiveFileInput.Id));
            Input != nullptr && Input->Text().Get() != m_buildModalArchiveFileText)
        {
            Input->Text().Set(m_buildModalArchiveFileText);
        }
    }

    SyncBuildModalTokenField(m_buildSelectedLevelsInput, m_buildModalSelectedLevelsText);
    SyncBuildModalTokenField(m_buildExplicitAssetsInput, m_buildModalExplicitAssetsText);
    SyncBuildModalTokenField(m_buildIncludeFoldersInput, m_buildModalIncludeFoldersText);
    SyncBuildModalTokenField(m_buildExcludeFoldersInput, m_buildModalExcludeFoldersText);
    SyncBuildModalTokenField(m_buildIncludeLabelsInput, m_buildModalIncludeLabelsText);
    SyncBuildModalTokenField(m_buildExcludeLabelsInput, m_buildModalExcludeLabelsText);
    SyncBuildModalTokenField(m_buildIncludeKindsInput, m_buildModalIncludeKindsText);
    SyncBuildModalTokenField(m_buildExcludeKindsInput, m_buildModalExcludeKindsText);

    if (m_buildModalTabs.Id.Value != 0)
    {
        if (auto* Tabs = dynamic_cast<SnAPI::UI::UITabs*>(&m_context->GetElement(m_buildModalTabs.Id)))
        {
            Tabs->ActiveIndex().Set(m_buildModalActiveTabIndex);
        }
    }
    if (m_buildArchiveFormatInput.Id.Value != 0)
    {
        if (auto* Element = dynamic_cast<SnAPI::UI::UIElementBase*>(&m_context->GetElement(m_buildArchiveFormatInput.Id)))
        {
            Element->SetDisabled(!m_buildModalArchiveEnabled);
        }
    }
    if (m_buildArchiveFileInput.Id.Value != 0)
    {
        if (auto* Element = dynamic_cast<SnAPI::UI::UIElementBase*>(&m_context->GetElement(m_buildArchiveFileInput.Id)))
        {
            Element->SetDisabled(!m_buildModalArchiveEnabled);
        }
    }
}

std::string EditorLayout::BuildHistoryComparisonTargetId() const
{
    if (!m_buildPanelState.LastBuildId.empty() && m_buildPanelState.LastBuildId != m_buildModalSelectedHistoryBuildId)
    {
        return m_buildPanelState.LastBuildId;
    }

    for (const BuildHistoryEntryView& Entry : m_buildPanelState.HistoryEntries)
    {
        if (Entry.BuildId != m_buildModalSelectedHistoryBuildId)
        {
            return Entry.BuildId;
        }
    }

    return {};
}

void EditorLayout::ApplyProjectTemplatePreset(const std::int32_t Index)
{
    switch (Index)
    {
    case 1:
        m_projectTemplatePreset = EProjectTemplatePreset::RuntimeAndEditorGame;
        m_projectCreateRuntimeModule = true;
        m_projectCreateEditorModule = true;
        break;
    case 2:
        m_projectTemplatePreset = EProjectTemplatePreset::ContentOnly;
        m_projectCreateRuntimeModule = false;
        m_projectCreateEditorModule = false;
        break;
    case 0:
    default:
        m_projectTemplatePreset = EProjectTemplatePreset::RuntimeGame;
        m_projectCreateRuntimeModule = true;
        m_projectCreateEditorModule = false;
        break;
    }

    if (TrimCopy(m_projectRuntimeModuleText).empty())
    {
        m_projectRuntimeModuleText = TrimCopy(m_projectNameText);
    }
    if (TrimCopy(m_projectEditorModuleText).empty())
    {
        const std::string RuntimeName = TrimCopy(m_projectRuntimeModuleText.empty() ? m_projectNameText : m_projectRuntimeModuleText);
        m_projectEditorModuleText = RuntimeName.empty() ? std::string("ProjectEditor") : (RuntimeName + "Editor");
    }
}

void EditorLayout::ApplyPluginTemplatePreset(const std::int32_t Index)
{
    switch (Index)
    {
    case 1:
        m_pluginTemplatePreset = EPluginTemplatePreset::EditorTool;
        m_pluginCreateRuntimeModule = false;
        m_pluginCreateEditorModule = true;
        m_pluginCanContainAssets = false;
        break;
    case 2:
        m_pluginTemplatePreset = EPluginTemplatePreset::Hybrid;
        m_pluginCreateRuntimeModule = true;
        m_pluginCreateEditorModule = true;
        m_pluginCanContainAssets = true;
        break;
    case 3:
        m_pluginTemplatePreset = EPluginTemplatePreset::ContentOnly;
        m_pluginCreateRuntimeModule = false;
        m_pluginCreateEditorModule = false;
        m_pluginCanContainAssets = true;
        break;
    case 0:
    default:
        m_pluginTemplatePreset = EPluginTemplatePreset::Runtime;
        m_pluginCreateRuntimeModule = true;
        m_pluginCreateEditorModule = false;
        m_pluginCanContainAssets = true;
        break;
    }

    if (TrimCopy(m_pluginRuntimeModuleText).empty())
    {
        m_pluginRuntimeModuleText = TrimCopy(m_pluginNameText);
    }
    if (TrimCopy(m_pluginEditorModuleText).empty())
    {
        const std::string RuntimeName = TrimCopy(m_pluginRuntimeModuleText.empty() ? m_pluginNameText : m_pluginRuntimeModuleText);
        m_pluginEditorModuleText = RuntimeName.empty() ? std::string("PluginEditor") : (RuntimeName + "Editor");
    }
}

void EditorLayout::ResetBuildModalDraftFromSelectedProfile()
{
    const BuildProfileEntry* Profile = SelectedBuildProfileEntry();
    if (Profile == nullptr)
    {
        m_buildModalPlatformText.clear();
        m_buildModalExecutionEnvironmentText.clear();
        m_buildModalConfiguration = EBuildConfiguration::Development;
        m_buildModalSelectedLevelsText.clear();
        m_buildModalExplicitAssetsText.clear();
        m_buildModalIncludeFoldersText.clear();
        m_buildModalExcludeFoldersText.clear();
        m_buildModalIncludeLabelsText.clear();
        m_buildModalExcludeLabelsText.clear();
        m_buildModalIncludeKindsText.clear();
        m_buildModalExcludeKindsText.clear();
        m_buildModalDependencyPolicy = EAssetDependencyPolicy::HardOnly;
        m_buildModalChunkStrategy = EAssetChunkStrategy::Monolithic;
        m_buildModalAllowExplicitOverrideExcludes = false;
        m_buildModalOutputRootText.clear();
        m_buildModalPackageDirectoryText.clear();
        m_buildModalArchiveEnabled = false;
        m_buildModalArchiveFormatText.clear();
        m_buildModalArchiveFileText.clear();
        m_buildModalDraftSeedProfileName.clear();
        m_buildModalDraftDirty = false;
        return;
    }

    m_buildModalPlatformText = Profile->Platform;
    m_buildModalExecutionEnvironmentText = Profile->ExecutionEnvironment;
    if (Profile->Configuration == "Debug")
    {
        m_buildModalConfiguration = EBuildConfiguration::Debug;
    }
    else if (Profile->Configuration == "Test")
    {
        m_buildModalConfiguration = EBuildConfiguration::Test;
    }
    else if (Profile->Configuration == "Shipping")
    {
        m_buildModalConfiguration = EBuildConfiguration::Shipping;
    }
    else
    {
        m_buildModalConfiguration = EBuildConfiguration::Development;
    }
    m_buildModalSelectedLevelsText = JoinEntries(Profile->SelectedLevels);
    m_buildModalExplicitAssetsText = JoinEntries(Profile->ExplicitAssets);
    m_buildModalIncludeFoldersText = JoinEntries(Profile->IncludeFolders);
    m_buildModalExcludeFoldersText = JoinEntries(Profile->ExcludeFolders);
    m_buildModalIncludeLabelsText = JoinEntries(Profile->IncludeAssetLabels);
    m_buildModalExcludeLabelsText = JoinEntries(Profile->ExcludeAssetLabels);
    m_buildModalIncludeKindsText = JoinEntries(Profile->IncludeAssetKinds);
    m_buildModalExcludeKindsText = JoinEntries(Profile->ExcludeAssetKinds);
    m_buildModalDependencyPolicy = Profile->DependencyPolicy;
    m_buildModalChunkStrategy = Profile->ChunkStrategy;
    m_buildModalAllowExplicitOverrideExcludes = Profile->AllowExplicitOverrideExcludes;
    m_buildModalArchiveEnabled = Profile->ArchiveEnabled;
    m_buildModalArchiveFormatText = Profile->ArchiveFormat;
    m_buildModalOutputRootText.clear();
    m_buildModalPackageDirectoryText.clear();
    m_buildModalArchiveFileText.clear();
    m_buildModalDraftSeedProfileName = Profile->Name;
    m_buildModalDraftDirty = false;
}

void EditorLayout::MarkBuildModalDraftDirty(const bool RefreshLiveState)
{
    m_buildModalDraftDirty = true;
    if (RefreshLiveState)
    {
        RefreshBuildModalLiveState();
    }
}

void EditorLayout::SyncBuildModalTokenField(const SnAPI::UI::ElementHandle<SnAPI::UI::UITokenField>& Handle,
                                            const std::string& Value) const
{
    if (Handle.Id.Value == 0 || !m_context)
    {
        return;
    }

    if (auto* TokenField = dynamic_cast<SnAPI::UI::UITokenField*>(&m_context->GetElement(Handle.Id)))
    {
        TokenField->SetTokens(ParseMultilineEntries(Value), false);
    }
}

BuildRequest EditorLayout::BuildModalRequest() const
{
    BuildRequest Request{};
    Request.ProfileName = m_buildModalSelectedProfileName;
    Request.Overrides.Platform = BuildProfileValue<std::string>{.IsSet = !TrimCopy(m_buildModalPlatformText).empty(),
                                                                .Value = TrimCopy(m_buildModalPlatformText)};
    Request.Overrides.ExecutionEnvironment = BuildProfileValue<std::string>{
        .IsSet = !TrimCopy(m_buildModalExecutionEnvironmentText).empty(),
        .Value = TrimCopy(m_buildModalExecutionEnvironmentText),
    };
    Request.Overrides.Configuration = BuildProfileValue<EBuildConfiguration>{
        .IsSet = true,
        .Value = m_buildModalConfiguration,
    };
    Request.Overrides.SelectedLevels = BuildProfileStringList{
        .IsSet = true,
        .Values = ParseMultilineEntries(m_buildModalSelectedLevelsText),
    };
    Request.Overrides.ExplicitAssets = BuildProfileStringList{
        .IsSet = true,
        .Values = ParseMultilineEntries(m_buildModalExplicitAssetsText),
    };
    Request.Overrides.IncludeFolders = BuildProfileStringList{
        .IsSet = true,
        .Values = ParseMultilineEntries(m_buildModalIncludeFoldersText),
    };
    Request.Overrides.ExcludeFolders = BuildProfileStringList{
        .IsSet = true,
        .Values = ParseMultilineEntries(m_buildModalExcludeFoldersText),
    };
    Request.Overrides.IncludeAssetLabels = BuildProfileStringList{
        .IsSet = true,
        .Values = ParseMultilineEntries(m_buildModalIncludeLabelsText),
    };
    Request.Overrides.ExcludeAssetLabels = BuildProfileStringList{
        .IsSet = true,
        .Values = ParseMultilineEntries(m_buildModalExcludeLabelsText),
    };
    Request.Overrides.IncludeAssetKinds = BuildProfileStringList{
        .IsSet = true,
        .Values = ParseMultilineEntries(m_buildModalIncludeKindsText),
    };
    Request.Overrides.ExcludeAssetKinds = BuildProfileStringList{
        .IsSet = true,
        .Values = ParseMultilineEntries(m_buildModalExcludeKindsText),
    };
    Request.Overrides.DependencyPolicy = BuildProfileValue<EAssetDependencyPolicy>{
        .IsSet = true,
        .Value = m_buildModalDependencyPolicy,
    };
    Request.Overrides.ChunkStrategy = BuildProfileValue<EAssetChunkStrategy>{
        .IsSet = true,
        .Value = m_buildModalChunkStrategy,
    };
    Request.Overrides.AllowExplicitOverrideExcludes = BuildProfileValue<bool>{
        .IsSet = true,
        .Value = m_buildModalAllowExplicitOverrideExcludes,
    };
    Request.Overrides.Archive.Enabled = BuildProfileValue<bool>{.IsSet = true, .Value = m_buildModalArchiveEnabled};
    Request.Overrides.Archive.Format = BuildProfileValue<std::string>{
        .IsSet = !TrimCopy(m_buildModalArchiveFormatText).empty(),
        .Value = TrimCopy(m_buildModalArchiveFormatText),
    };
    return Request;
}

PackageOutputOptions EditorLayout::BuildModalPackageOutput() const
{
    PackageOutputOptions Options{};
    Options.OutputRootDirectory = TrimCopy(m_buildModalOutputRootText);
    Options.PackageDirectoryName = TrimCopy(m_buildModalPackageDirectoryText);
    Options.ArchiveEnabled = m_buildModalArchiveEnabled;
    Options.ArchiveFormat = TrimCopy(m_buildModalArchiveFormatText);
    Options.ArchiveFileName = TrimCopy(m_buildModalArchiveFileText);
    return Options;
}

void EditorLayout::EnsureBuildModalOverlay()
{
    if (!m_context || m_buildModalOverlay.Id.Value != 0 || !m_buildModalOpen)
    {
        return;
    }

    auto Root = m_context->Root();
    auto Overlay = Root.Add(SnAPI::UI::UIModal{});
    auto& OverlayPanel = Overlay.Element();
    OverlayPanel.Movable().Set(true);
    OverlayPanel.Resizable().Set(true);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 22, 30, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(87, 97, 112, 245));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(10.0f);
    OverlayPanel.ContentPadding().Set(12.0f);
    ConfigureModalScreenRatio(OverlayPanel, 0.66f);
    m_buildModalOverlay = Overlay.Handle();

    auto Modal = Overlay.Add(SnAPI::UI::UIPanel("Editor.BuildModal"));
    auto& ModalPanel = Modal.Element();
    ModalPanel.ElementStyle().Apply("editor.project_modal_root");
    ModalPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ModalPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ModalPanel.Padding().Set(10.0f);
    ModalPanel.Gap().Set(10.0f);

    auto HeaderRow = Modal.Add(SnAPI::UI::UIPanel("Editor.BuildModal.Header"));
    auto& HeaderRowPanel = HeaderRow.Element();
    ConfigureTransparentLayoutPanel(HeaderRowPanel);
    HeaderRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HeaderRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HeaderRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HeaderRowPanel.Gap().Set(8.0f);

    auto HeaderIcon = HeaderRow.Add(SnAPI::UI::UIImage(ResolveUIImageSource(kProjectSettingsIconPath)));
    auto& HeaderIconImage = HeaderIcon.Element();
    ConfigureSvgIcon(HeaderIconImage, 18.0f, SnAPI::UI::Color::RGB(230, 206, 162));
    HeaderIconImage.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto Title = HeaderRow.Add(SnAPI::UI::UIText("Package Project"));
    auto& TitleText = Title.Element();
    TitleText.ElementStyle().Apply("editor.project_welcome_title");
    TitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    TitleText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto HeaderSpacer = HeaderRow.Add(SnAPI::UI::UIPanel("Editor.BuildModal.HeaderSpacer"));
    auto& HeaderSpacerPanel = HeaderSpacer.Element();
    ConfigureLayoutSpacerPanel(HeaderSpacerPanel);
    HeaderSpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));

    auto CloseButton = HeaderRow.Add(SnAPI::UI::UIButton{});
    auto& CloseButtonElement = CloseButton.Element();
    CloseButtonElement.ElementStyle().Apply("editor.project_modal_action_button");
    CloseButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    CloseButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    CloseButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    CloseButtonElement.OnClick([this]() {
        CloseBuildModal();
    });
    auto CloseLabel = CloseButton.Add(SnAPI::UI::UIText("Close"));
    auto& CloseLabelText = CloseLabel.Element();
    CloseLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    CloseLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto Subtitle = Modal.Add(SnAPI::UI::UIText(""));
    auto& SubtitleText = Subtitle.Element();
    SubtitleText.ElementStyle().Apply("editor.project_welcome_subtitle");
    SubtitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_buildModalSubtitleText = Subtitle.Handle();

    auto Tabs = Modal.Add(SnAPI::UI::UITabs{});
    auto& TabsElement = Tabs.Element();
    TabsElement.ElementStyle().Apply("editor.viewport_tabs");
    TabsElement.Width().Set(SnAPI::UI::Sizing::Fill());
    TabsElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    TabsElement.HeaderHeight().Set(30.0f);
    TabsElement.ActiveIndex().Set(m_buildModalActiveTabIndex);
    TabsElement.OnSelectionChanged([this](const int32_t Index) {
        m_buildModalActiveTabIndex = std::max(0, Index);
    });
    m_buildModalTabs = Tabs.Handle();

    auto OverviewTab = Tabs.Add(SnAPI::UI::UIPanel("Editor.BuildModal.Overview"));
    auto& OverviewTabPanel = OverviewTab.Element();
    OverviewTabPanel.ElementStyle().Apply("editor.section_card");
    OverviewTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    OverviewTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    OverviewTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    OverviewTabPanel.Padding().Set(8.0f);
    OverviewTabPanel.Gap().Set(8.0f);

    auto OverviewScroll = OverviewTab.Add(SnAPI::UI::UIScrollContainer{});
    auto& OverviewScrollElement = OverviewScroll.Element();
    OverviewScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    OverviewScrollElement.Height().Set(SnAPI::UI::Sizing::Fill());
    OverviewScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    OverviewScrollElement.ShowHorizontalScrollbar().Set(false);
    OverviewScrollElement.ShowVerticalScrollbar().Set(true);
    OverviewScrollElement.Smooth().Set(true);
    OverviewScrollElement.Padding().Set(2.0f);
    OverviewScrollElement.Gap().Set(8.0f);

    auto OverviewCard = OverviewScroll.Add(SnAPI::UI::UIPanel("Editor.BuildModal.OverviewCard"));
    auto& OverviewCardPanel = OverviewCard.Element();
    OverviewCardPanel.ElementStyle().Apply("editor.section_card");
    OverviewCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    OverviewCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    OverviewCardPanel.Gap().Set(4.0f);

    auto OverviewTitle = OverviewCard.Add(SnAPI::UI::UIText("Overview"));
    OverviewTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto OverviewSummaryText = OverviewCard.Add(SnAPI::UI::UIText(""));
    auto& OverviewSummaryElement = OverviewSummaryText.Element();
    OverviewSummaryElement.ElementStyle().Apply("editor.panel_subtitle");
    OverviewSummaryElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_buildModalOverviewSummaryText = OverviewSummaryText.Handle();

    auto ProfileCard = OverviewScroll.Add(SnAPI::UI::UIPanel("Editor.BuildModal.ProfileCard"));
    auto& ProfileCardPanel = ProfileCard.Element();
    ProfileCardPanel.ElementStyle().Apply("editor.section_card");
    ProfileCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ProfileCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ProfileCardPanel.Gap().Set(6.0f);

    auto ProfileTitle = ProfileCard.Add(SnAPI::UI::UIText("Profile"));
    ProfileTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(ProfileCard,
                     "Profiles capture reusable package defaults. The fields in the tabs below override the selected "
                     "profile for this one build request without rewriting the descriptor.");

    auto ProfileComboBuilder = ProfileCard.Add(SnAPI::UI::UIComboBox{});
    auto& ProfileCombo = ProfileComboBuilder.Element();
    ProfileCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    ProfileCombo.Height().Set(SnAPI::UI::Sizing::Auto());
    ProfileCombo.Placeholder().Set(std::string("Select build profile"));
    ProfileCombo.MaxDropdownHeight().Set(240.0f);
    m_buildModalProfileKeys.clear();

    std::vector<std::string> ProfileLabels{};
    ProfileLabels.reserve(m_buildPanelState.Profiles.size());
    int32_t SelectedProfileIndex = -1;
    for (std::size_t Index = 0; Index < m_buildPanelState.Profiles.size(); ++Index)
    {
        const BuildProfileEntry& Entry = m_buildPanelState.Profiles[Index];
        ProfileLabels.push_back(Entry.Label.empty() ? Entry.Name : Entry.Label);
        m_buildModalProfileKeys.push_back(Entry.Name);
        if (Entry.Name == m_buildModalSelectedProfileName)
        {
            SelectedProfileIndex = static_cast<int32_t>(Index);
        }
    }
    if (SelectedProfileIndex < 0 && !m_buildPanelState.Profiles.empty())
    {
        auto DefaultIt = std::find_if(
            m_buildPanelState.Profiles.begin(),
            m_buildPanelState.Profiles.end(),
            [](const BuildProfileEntry& Entry) { return Entry.IsDefault; });
        if (DefaultIt != m_buildPanelState.Profiles.end())
        {
            SelectedProfileIndex = static_cast<int32_t>(std::distance(m_buildPanelState.Profiles.begin(), DefaultIt));
            m_buildModalSelectedProfileName = DefaultIt->Name;
        }
        else
        {
            SelectedProfileIndex = 0;
            m_buildModalSelectedProfileName = m_buildPanelState.Profiles.front().Name;
        }
    }
    ProfileCombo.SetItems(std::move(ProfileLabels));
    (void)ProfileCombo.SetSelectedIndex(SelectedProfileIndex, false);
    ProfileCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        if (Index >= 0 && static_cast<std::size_t>(Index) < m_buildModalProfileKeys.size())
        {
            m_buildModalSelectedProfileName = m_buildModalProfileKeys[static_cast<std::size_t>(Index)];
        }
        else
        {
            m_buildModalSelectedProfileName.clear();
        }
        ResetBuildModalDraftFromSelectedProfile();
        RebuildBuildModalOverlay();
    });
    m_buildProfileCombo = ProfileComboBuilder.Handle();

    auto ProfileSummaryText = ProfileCard.Add(SnAPI::UI::UIText(""));
    auto& ProfileSummaryElement = ProfileSummaryText.Element();
    ProfileSummaryElement.ElementStyle().Apply("editor.panel_subtitle");
    ProfileSummaryElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_buildModalProfileSummaryText = ProfileSummaryText.Handle();

    auto ActionsRow = OverviewScroll.Add(SnAPI::UI::UIPanel("Editor.BuildModal.Actions"));
    auto& ActionsRowPanel = ActionsRow.Element();
    ConfigureTransparentLayoutPanel(ActionsRowPanel);
    ActionsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ActionsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ActionsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ActionsRowPanel.Gap().Set(8.0f);

    const bool CanExecuteBuildActions =
        m_buildPanelState.ProjectLoaded && !m_buildPanelState.Profiles.empty() && !m_buildPanelState.BuildInProgress;

    auto PlanButton = ActionsRow.Add(SnAPI::UI::UIButton{});
    auto& PlanButtonElement = PlanButton.Element();
    PlanButtonElement.ElementStyle().Apply("editor.project_modal_action_button");
    PlanButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    PlanButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    PlanButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    PlanButtonElement.SetDisabled(!CanExecuteBuildActions || !m_onBuildActionRequested);
    PlanButtonElement.OnClick([this]() {
        if (!m_onBuildActionRequested)
        {
            return;
        }
        BuildActionRequest Request{};
        Request.Action = EBuildAction::PlanProject;
        Request.Request = BuildModalRequest();
        m_onBuildActionRequested(Request);
    });
    auto PlanLabel = PlanButton.Add(SnAPI::UI::UIText("Plan Build"));
    auto& PlanLabelText = PlanLabel.Element();
    PlanLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    PlanLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto PackageButton = ActionsRow.Add(SnAPI::UI::UIButton{});
    auto& PackageButtonElement = PackageButton.Element();
    PackageButtonElement.ElementStyle().Apply("editor.project_modal_action_button_primary");
    PackageButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    PackageButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    PackageButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    PackageButtonElement.SetDisabled(!CanExecuteBuildActions || !m_onBuildActionRequested);
    PackageButtonElement.OnClick([this]() {
        if (!m_onBuildActionRequested)
        {
            return;
        }
        BuildActionRequest Request{};
        Request.Action = EBuildAction::PackageProject;
        Request.Request = BuildModalRequest();
        Request.PackageOutput = BuildModalPackageOutput();
        m_onBuildActionRequested(Request);
    });
    auto PackageLabel = PackageButton.Add(SnAPI::UI::UIText("Package"));
    auto& PackageLabelText = PackageLabel.Element();
    PackageLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    PackageLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto LatestCard = OverviewScroll.Add(SnAPI::UI::UIPanel("Editor.BuildModal.LatestCard"));
    auto& LatestCardPanel = LatestCard.Element();
    LatestCardPanel.ElementStyle().Apply("editor.section_card");
    LatestCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    LatestCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    LatestCardPanel.Gap().Set(4.0f);

    auto LatestTitle = LatestCard.Add(SnAPI::UI::UIText("Latest Result"));
    LatestTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto LatestSummaryText = LatestCard.Add(SnAPI::UI::UIText(""));
    auto& LatestSummaryElement = LatestSummaryText.Element();
    LatestSummaryElement.ElementStyle().Apply("editor.panel_subtitle");
    LatestSummaryElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_buildModalLatestSummaryText = LatestSummaryText.Handle();

    const auto AddTokenBuildInput =
        [](SnAPI::UI::TElementBuilder<SnAPI::UI::UIPanel>& Parent,
           const char* Label,
           const char* HelpText,
           const std::string& Value,
           const std::function<void(const std::vector<std::string>&)>& OnChanged)
            -> SnAPI::UI::ElementHandle<SnAPI::UI::UITokenField> {
        auto FieldLabel = Parent.Add(SnAPI::UI::UIText(Label));
        FieldLabel.Element().ElementStyle().Apply("editor.menu_item");
        AddFieldHelpText(Parent, HelpText);
        auto FieldInput = Parent.Add(SnAPI::UI::UITokenField{});
        auto& FieldInputElement = FieldInput.Element();
        FieldInputElement.ElementStyle().Apply("editor.token_field");
        FieldInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
        FieldInputElement.Placeholder().Set("Type and press Enter");
        FieldInputElement.SetTokens(ParseMultilineEntries(Value), false);
        FieldInputElement.OnTokensChanged(
            SnAPI::UI::TDelegate<void(const std::vector<std::string>&)>::Bind(OnChanged));
        return FieldInput.Handle();
    };

    auto ContentTab = Tabs.Add(SnAPI::UI::UIPanel("Editor.BuildModal.Content"));
    auto& ContentTabPanel = ContentTab.Element();
    ContentTabPanel.ElementStyle().Apply("editor.section_card");
    ContentTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ContentTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ContentTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ContentTabPanel.Padding().Set(8.0f);
    ContentTabPanel.Gap().Set(8.0f);

    auto ContentScroll = ContentTab.Add(SnAPI::UI::UIScrollContainer{});
    auto& ContentScrollElement = ContentScroll.Element();
    ContentScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ContentScrollElement.Height().Set(SnAPI::UI::Sizing::Fill());
    ContentScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ContentScrollElement.ShowHorizontalScrollbar().Set(false);
    ContentScrollElement.ShowVerticalScrollbar().Set(true);
    ContentScrollElement.Smooth().Set(true);
    ContentScrollElement.Padding().Set(2.0f);
    ContentScrollElement.Gap().Set(10.0f);

    auto ContentSelectors = ContentScroll.Add(SnAPI::UI::UIPanel("Editor.BuildModal.ContentSelectors"));
    auto& ContentSelectorsPanel = ContentSelectors.Element();
    ConfigureTransparentLayoutPanel(ContentSelectorsPanel);
    ContentSelectorsPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ContentSelectorsPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ContentSelectorsPanel.Gap().Set(10.0f);

    auto SelectionCard = ContentSelectors.Add(SnAPI::UI::UIPanel("Editor.BuildModal.SelectionCard"));
    auto& SelectionCardPanel = SelectionCard.Element();
    SelectionCardPanel.ElementStyle().Apply("editor.section_card");
    SelectionCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    SelectionCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    SelectionCardPanel.Padding().Set(12.0f);
    SelectionCardPanel.Gap().Set(8.0f);

    auto SelectionTitle = SelectionCard.Add(SnAPI::UI::UIText("Content Selection"));
    SelectionTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(SelectionCard,
                     "Choose the primary authored assets that drive the cook set. Selected levels are the normal V1 "
                     "entrypoint because dependency expansion can build a package from them.");
    auto AddLevelComboBuilder = SelectionCard.Add(SnAPI::UI::UIComboBox{});
    const auto AddLevelComboHandle = AddLevelComboBuilder.Handle();
    auto& AddLevelCombo = AddLevelComboBuilder.Element();
    AddLevelCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    AddLevelCombo.Placeholder().Set("Add discovered level");
    {
        std::vector<std::string> Items{"Add discovered level"};
        Items.insert(Items.end(), m_buildPanelState.AvailableLevels.begin(), m_buildPanelState.AvailableLevels.end());
        AddLevelCombo.SetItems(std::move(Items));
    }
    (void)AddLevelCombo.SetSelectedIndex(0, false);
    AddLevelCombo.OnChanged([this, AddLevelComboHandle](const int32_t Index, const std::string& Text) {
        if (Index <= 0)
        {
            return;
        }
        AppendUniqueEntryText(m_buildModalSelectedLevelsText, Text);
        if (AddLevelComboHandle.Id.Value != 0 && m_context)
        {
            if (auto* Combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(AddLevelComboHandle.Id)))
            {
                (void)Combo->SetSelectedIndex(0, false);
            }
        }
        MarkBuildModalDraftDirty();
    });
    m_buildSelectedLevelsInput = AddTokenBuildInput(
        SelectionCard,
        "Selected Levels",
        "Explicitly selected level source assets. These are usually authored as `Levels/*.level` relative asset "
        "paths and drive dependency expansion for level-based packages.",
        m_buildModalSelectedLevelsText,
        [this](const std::vector<std::string>& Values) {
            m_buildModalSelectedLevelsText = JoinEntries(Values);
            MarkBuildModalDraftDirty();
        });
    auto AddAssetComboBuilder = SelectionCard.Add(SnAPI::UI::UIComboBox{});
    const auto AddAssetComboHandle = AddAssetComboBuilder.Handle();
    auto& AddAssetCombo = AddAssetComboBuilder.Element();
    AddAssetCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    AddAssetCombo.Placeholder().Set("Add discovered asset");
    {
        std::vector<std::string> Items{"Add discovered asset"};
        Items.insert(Items.end(), m_buildPanelState.AvailableAssets.begin(), m_buildPanelState.AvailableAssets.end());
        AddAssetCombo.SetItems(std::move(Items));
    }
    (void)AddAssetCombo.SetSelectedIndex(0, false);
    AddAssetCombo.OnChanged([this, AddAssetComboHandle](const int32_t Index, const std::string& Text) {
        if (Index <= 0)
        {
            return;
        }
        AppendUniqueEntryText(m_buildModalExplicitAssetsText, Text);
        if (AddAssetComboHandle.Id.Value != 0 && m_context)
        {
            if (auto* Combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(AddAssetComboHandle.Id)))
            {
                (void)Combo->SetSelectedIndex(0, false);
            }
        }
        MarkBuildModalDraftDirty();
    });
    m_buildExplicitAssetsInput = AddTokenBuildInput(
        SelectionCard,
        "Explicit Assets",
        "Explicit source assets that should be included even when they are not reached from a selected level. Use "
        "this for standalone assets, shared gameplay data, or assets under active investigation.",
        m_buildModalExplicitAssetsText,
        [this](const std::vector<std::string>& Values) {
            m_buildModalExplicitAssetsText = JoinEntries(Values);
            MarkBuildModalDraftDirty();
        });

    auto RulesCard = ContentSelectors.Add(SnAPI::UI::UIPanel("Editor.BuildModal.RulesCard"));
    auto& RulesCardPanel = RulesCard.Element();
    RulesCardPanel.ElementStyle().Apply("editor.section_card");
    RulesCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    RulesCardPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    RulesCardPanel.Padding().Set(12.0f);
    RulesCardPanel.Gap().Set(8.0f);

    auto RulesTitle = RulesCard.Add(SnAPI::UI::UIText("Rules"));
    RulesTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(RulesCard,
                     "Rules let one profile widen or narrow the cook set without forcing whole-project packaging.");
    m_buildIncludeFoldersInput = AddTokenBuildInput(
        RulesCard,
        "Include Folders",
        "Descriptor-relative folder rules whose contents should be included before exclusion and dependency policy are "
        "applied.",
        m_buildModalIncludeFoldersText,
        [this](const std::vector<std::string>& Values) {
            m_buildModalIncludeFoldersText = JoinEntries(Values);
            MarkBuildModalDraftDirty();
        });
    m_buildExcludeFoldersInput = AddTokenBuildInput(
        RulesCard,
        "Exclude Folders",
        "Descriptor-relative folder rules whose contents should be removed from the resolved cook set.",
        m_buildModalExcludeFoldersText,
        [this](const std::vector<std::string>& Values) {
            m_buildModalExcludeFoldersText = JoinEntries(Values);
            MarkBuildModalDraftDirty();
        });
    m_buildIncludeLabelsInput = AddTokenBuildInput(
        RulesCard,
        "Include Labels",
        "Asset labels that should opt matching content into the build when labels are available from the authored "
        "asset metadata set.",
        m_buildModalIncludeLabelsText,
        [this](const std::vector<std::string>& Values) {
            m_buildModalIncludeLabelsText = JoinEntries(Values);
            MarkBuildModalDraftDirty();
        });
    m_buildExcludeLabelsInput = AddTokenBuildInput(
        RulesCard,
        "Exclude Labels",
        "Asset labels that should remove matching content from the resolved cook set.",
        m_buildModalExcludeLabelsText,
        [this](const std::vector<std::string>& Values) {
            m_buildModalExcludeLabelsText = JoinEntries(Values);
            MarkBuildModalDraftDirty();
        });
    auto AddIncludeKindComboBuilder = RulesCard.Add(SnAPI::UI::UIComboBox{});
    const auto AddIncludeKindComboHandle = AddIncludeKindComboBuilder.Handle();
    auto& AddIncludeKindCombo = AddIncludeKindComboBuilder.Element();
    AddIncludeKindCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    AddIncludeKindCombo.Placeholder().Set("Add discovered asset kind");
    {
        std::vector<std::string> Items{"Add discovered asset kind"};
        Items.insert(Items.end(), m_buildPanelState.AvailableAssetKinds.begin(), m_buildPanelState.AvailableAssetKinds.end());
        AddIncludeKindCombo.SetItems(std::move(Items));
    }
    (void)AddIncludeKindCombo.SetSelectedIndex(0, false);
    AddIncludeKindCombo.OnChanged([this, AddIncludeKindComboHandle](const int32_t Index, const std::string& Text) {
        if (Index <= 0)
        {
            return;
        }
        AppendUniqueEntryText(m_buildModalIncludeKindsText, Text);
        if (AddIncludeKindComboHandle.Id.Value != 0 && m_context)
        {
            if (auto* Combo =
                    dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(AddIncludeKindComboHandle.Id)))
            {
                (void)Combo->SetSelectedIndex(0, false);
            }
        }
        MarkBuildModalDraftDirty();
    });
    m_buildIncludeKindsInput = AddTokenBuildInput(
        RulesCard,
        "Include Asset Kinds",
        "Asset-kind filters that opt matching content into the build. Use this sparingly because kind-wide rules can "
        "pull in large content sets.",
        m_buildModalIncludeKindsText,
        [this](const std::vector<std::string>& Values) {
            m_buildModalIncludeKindsText = JoinEntries(Values);
            MarkBuildModalDraftDirty();
        });
    auto AddExcludeKindComboBuilder = RulesCard.Add(SnAPI::UI::UIComboBox{});
    const auto AddExcludeKindComboHandle = AddExcludeKindComboBuilder.Handle();
    auto& AddExcludeKindCombo = AddExcludeKindComboBuilder.Element();
    AddExcludeKindCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    AddExcludeKindCombo.Placeholder().Set("Add discovered asset kind to exclude");
    {
        std::vector<std::string> Items{"Add discovered asset kind to exclude"};
        Items.insert(Items.end(), m_buildPanelState.AvailableAssetKinds.begin(), m_buildPanelState.AvailableAssetKinds.end());
        AddExcludeKindCombo.SetItems(std::move(Items));
    }
    (void)AddExcludeKindCombo.SetSelectedIndex(0, false);
    AddExcludeKindCombo.OnChanged([this, AddExcludeKindComboHandle](const int32_t Index, const std::string& Text) {
        if (Index <= 0)
        {
            return;
        }
        AppendUniqueEntryText(m_buildModalExcludeKindsText, Text);
        if (AddExcludeKindComboHandle.Id.Value != 0 && m_context)
        {
            if (auto* Combo =
                    dynamic_cast<SnAPI::UI::UIComboBox*>(&m_context->GetElement(AddExcludeKindComboHandle.Id)))
            {
                (void)Combo->SetSelectedIndex(0, false);
            }
        }
        MarkBuildModalDraftDirty();
    });
    m_buildExcludeKindsInput = AddTokenBuildInput(
        RulesCard,
        "Exclude Asset Kinds",
        "Asset-kind filters that remove matching content from the resolved cook set.",
        m_buildModalExcludeKindsText,
        [this](const std::vector<std::string>& Values) {
            m_buildModalExcludeKindsText = JoinEntries(Values);
            MarkBuildModalDraftDirty();
        });

    auto PolicyCard = ContentScroll.Add(SnAPI::UI::UIPanel("Editor.BuildModal.PolicyCard"));
    auto& PolicyCardPanel = PolicyCard.Element();
    PolicyCardPanel.ElementStyle().Apply("editor.section_card");
    PolicyCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    PolicyCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PolicyCardPanel.Padding().Set(12.0f);
    PolicyCardPanel.Gap().Set(8.0f);

    auto PolicyTitle = PolicyCard.Add(SnAPI::UI::UIText("Dependency and Chunking"));
    PolicyTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(PolicyCard,
                     "These options control how the selected primary assets expand into the final cook set and how the "
                     "cooked payload is partitioned into `.snpak` outputs.");

    auto ConfigLabel = PolicyCard.Add(SnAPI::UI::UIText("Build Configuration"));
    ConfigLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(PolicyCard,
                     "Configuration affects both C++ compilation and package composition, including optimization level, "
                     "diagnostics, and editor-only content handling.");
    auto ConfigComboBuilder = PolicyCard.Add(SnAPI::UI::UIComboBox{});
    auto& ConfigCombo = ConfigComboBuilder.Element();
    ConfigCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    ConfigCombo.SetItems({"Debug", "Development", "Test", "Shipping"});
    (void)ConfigCombo.SetSelectedIndex(BuildConfigurationToIndex(m_buildModalConfiguration), false);
    ConfigCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        m_buildModalConfiguration = BuildConfigurationFromIndex(Index);
        MarkBuildModalDraftDirty();
    });
    m_buildConfigurationCombo = ConfigComboBuilder.Handle();

    auto DependencyLabel = PolicyCard.Add(SnAPI::UI::UIText("Dependency Policy"));
    DependencyLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(PolicyCard,
                     "Choose how far dependency walking should go beyond the primary selected assets.");
    auto DependencyComboBuilder = PolicyCard.Add(SnAPI::UI::UIComboBox{});
    auto& DependencyCombo = DependencyComboBuilder.Element();
    DependencyCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    DependencyCombo.SetItems({"Hard Only", "Hard + Soft", "Hard + Soft + Editor Preview", "Custom Resolver"});
    (void)DependencyCombo.SetSelectedIndex(DependencyPolicyToIndex(m_buildModalDependencyPolicy), false);
    DependencyCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        m_buildModalDependencyPolicy = DependencyPolicyFromIndex(Index);
        MarkBuildModalDraftDirty();
    });
    m_buildDependencyPolicyCombo = DependencyComboBuilder.Handle();

    auto ChunkLabel = PolicyCard.Add(SnAPI::UI::UIText("Chunk Strategy"));
    ChunkLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(PolicyCard,
                     "Chunk strategy decides whether cooked payloads are emitted as one package or partitioned by level "
                     "or label for future modular delivery.");
    auto ChunkComboBuilder = PolicyCard.Add(SnAPI::UI::UIComboBox{});
    auto& ChunkCombo = ChunkComboBuilder.Element();
    ChunkCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    ChunkCombo.SetItems({"Monolithic", "Shared + Per-Level", "Per Label", "Custom Graph"});
    (void)ChunkCombo.SetSelectedIndex(ChunkStrategyToIndex(m_buildModalChunkStrategy), false);
    ChunkCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        m_buildModalChunkStrategy = ChunkStrategyFromIndex(Index);
        MarkBuildModalDraftDirty();
    });
    m_buildChunkStrategyCombo = ChunkComboBuilder.Handle();

    auto OverrideCheckbox = PolicyCard.Add(SnAPI::UI::UICheckbox("Allow explicit includes to override excludes"));
    OverrideCheckbox.Element().Checked().Set(m_buildModalAllowExplicitOverrideExcludes);
    OverrideCheckbox.Element().OnChanged([this](const bool Checked) {
        m_buildModalAllowExplicitOverrideExcludes = Checked;
        MarkBuildModalDraftDirty();
    });
    m_buildAllowExplicitOverrideCheckbox = OverrideCheckbox.Handle();
    AddFieldHelpText(PolicyCard,
                     "When enabled, an explicit asset or level selection can win against inherited exclusion rules. "
                     "Leave this off when exclusion rules should stay authoritative.");

    const std::string AvailableLevelsSummary = m_buildPanelState.AvailableLevels.empty()
        ? std::string("No discovered level assets yet.")
        : JoinEntries(m_buildPanelState.AvailableLevels);
    auto LevelsHint = PolicyCard.Add(SnAPI::UI::UIText("Known levels:\n" + AvailableLevelsSummary));
    LevelsHint.Element().ElementStyle().Apply("editor.panel_subtitle");
    LevelsHint.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    const std::string AvailableAssetsSummary = m_buildPanelState.AvailableAssets.empty()
        ? std::string("No discovered authored assets yet.")
        : JoinEntries(m_buildPanelState.AvailableAssets);
    auto AssetsHint = PolicyCard.Add(SnAPI::UI::UIText("Known assets:\n" + AvailableAssetsSummary));
    AssetsHint.Element().ElementStyle().Apply("editor.panel_subtitle");
    AssetsHint.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    const std::string AvailableKindsSummary = m_buildPanelState.AvailableAssetKinds.empty()
        ? std::string("No discovered asset kinds yet.")
        : JoinEntries(m_buildPanelState.AvailableAssetKinds);
    auto KindsHint = PolicyCard.Add(SnAPI::UI::UIText("Known asset kinds:\n" + AvailableKindsSummary));
    KindsHint.Element().ElementStyle().Apply("editor.panel_subtitle");
    KindsHint.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto PlatformTab = Tabs.Add(SnAPI::UI::UIPanel("Editor.BuildModal.Platform"));
    auto& PlatformTabPanel = PlatformTab.Element();
    PlatformTabPanel.ElementStyle().Apply("editor.section_card");
    PlatformTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    PlatformTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PlatformTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    PlatformTabPanel.Padding().Set(8.0f);
    PlatformTabPanel.Gap().Set(8.0f);

    auto PlatformScroll = PlatformTab.Add(SnAPI::UI::UIScrollContainer{});
    auto& PlatformScrollElement = PlatformScroll.Element();
    PlatformScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    PlatformScrollElement.Height().Set(SnAPI::UI::Sizing::Fill());
    PlatformScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    PlatformScrollElement.ShowHorizontalScrollbar().Set(false);
    PlatformScrollElement.ShowVerticalScrollbar().Set(true);
    PlatformScrollElement.Smooth().Set(true);
    PlatformScrollElement.Padding().Set(2.0f);
    PlatformScrollElement.Gap().Set(10.0f);

    auto PlatformCard = PlatformScroll.Add(SnAPI::UI::UIPanel("Editor.BuildModal.PlatformCard"));
    auto& PlatformCardPanel = PlatformCard.Element();
    PlatformCardPanel.ElementStyle().Apply("editor.section_card");
    PlatformCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    PlatformCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PlatformCardPanel.Padding().Set(12.0f);
    PlatformCardPanel.Gap().Set(8.0f);

    auto PlatformTitle = PlatformCard.Add(SnAPI::UI::UIText("Platform and Toolchain"));
    PlatformTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(PlatformCard,
                     "Packaging is resolved against an explicit target platform and execution environment so builds can "
                     "be reproduced locally, in CI, or inside pinned container images.");

    auto PlatformLabel = PlatformCard.Add(SnAPI::UI::UIText("Target Platform"));
    PlatformLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(PlatformCard,
                     "Select the runtime platform that should receive binaries, cooked assets, and packaging rules.");
    auto PlatformComboBuilder = PlatformCard.Add(SnAPI::UI::UIComboBox{});
    auto& PlatformCombo = PlatformComboBuilder.Element();
    PlatformCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    std::vector<std::string> PlatformItems = BuildComboItemsWithCurrent(
        std::span<const std::string_view>(kKnownBuildPlatforms.begin(), kKnownBuildPlatforms.end()),
        "Inherited platform",
        m_buildModalPlatformText);
    PlatformCombo.SetItems(PlatformItems);
    int32_t SelectedPlatformIndex = 0;
    const std::string TrimmedPlatform = TrimCopy(m_buildModalPlatformText);
    if (!TrimmedPlatform.empty())
    {
        const auto It = std::find(PlatformItems.begin(), PlatformItems.end(), TrimmedPlatform);
        if (It != PlatformItems.end())
        {
            SelectedPlatformIndex = static_cast<int32_t>(std::distance(PlatformItems.begin(), It));
        }
    }
    (void)PlatformCombo.SetSelectedIndex(SelectedPlatformIndex, false);
    PlatformCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        m_buildModalPlatformText = Index <= 0 ? std::string{} : Text;
        MarkBuildModalDraftDirty();
    });
    m_buildPlatformInput = PlatformComboBuilder.Handle();

    auto EnvironmentLabel = PlatformCard.Add(SnAPI::UI::UIText("Execution Environment"));
    EnvironmentLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(PlatformCard,
                     "Use `host-local` for local toolchains or a `docker://image:tag` environment to pin the build "
                     "against a known-good platform container.");
    auto EnvironmentPresetComboBuilder = PlatformCard.Add(SnAPI::UI::UIComboBox{});
    auto& EnvironmentPresetCombo = EnvironmentPresetComboBuilder.Element();
    EnvironmentPresetCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    std::vector<std::string> EnvironmentItems = BuildComboItemsWithCurrent(
        std::span<const std::string_view>(kKnownBuildExecutionEnvironments.begin(), kKnownBuildExecutionEnvironments.end()),
        "Inherited execution environment",
        m_buildModalExecutionEnvironmentText);
    EnvironmentPresetCombo.SetItems(EnvironmentItems);
    int32_t SelectedEnvironmentIndex = 0;
    const std::string TrimmedEnvironment = TrimCopy(m_buildModalExecutionEnvironmentText);
    if (!TrimmedEnvironment.empty())
    {
        const auto It = std::find(EnvironmentItems.begin(), EnvironmentItems.end(), TrimmedEnvironment);
        if (It != EnvironmentItems.end())
        {
            SelectedEnvironmentIndex = static_cast<int32_t>(std::distance(EnvironmentItems.begin(), It));
        }
    }
    (void)EnvironmentPresetCombo.SetSelectedIndex(SelectedEnvironmentIndex, false);
    EnvironmentPresetCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        m_buildModalExecutionEnvironmentText = Index <= 0 ? std::string{} : Text;
        MarkBuildModalDraftDirty();
    });
    auto EnvironmentInput = PlatformCard.Add(SnAPI::UI::UITextInput{});
    auto& EnvironmentInputElement = EnvironmentInput.Element();
    EnvironmentInputElement.ElementStyle().Apply("editor.text_input");
    EnvironmentInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    EnvironmentInputElement.Placeholder().Set("Optional custom execution environment override");
    EnvironmentInputElement.Text().Set(m_buildModalExecutionEnvironmentText);
    EnvironmentInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_buildModalExecutionEnvironmentText = Value;
        MarkBuildModalDraftDirty();
    }));
    m_buildExecutionEnvironmentInput = EnvironmentInput.Handle();

    auto PlatformHint = PlatformCard.Add(SnAPI::UI::UIText(
        "Use the execution environment field to pin the build to a stable host or Docker image for reproducible packaging."));
    PlatformHint.Element().ElementStyle().Apply("editor.panel_subtitle");
    PlatformHint.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto PlatformSummaryCard = PlatformScroll.Add(SnAPI::UI::UIPanel("Editor.BuildModal.PlatformSummaryCard"));
    auto& PlatformSummaryCardPanel = PlatformSummaryCard.Element();
    PlatformSummaryCardPanel.ElementStyle().Apply("editor.section_card");
    PlatformSummaryCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    PlatformSummaryCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PlatformSummaryCardPanel.Padding().Set(12.0f);
    PlatformSummaryCardPanel.Gap().Set(8.0f);

    auto PlatformSummaryTitle = PlatformSummaryCard.Add(SnAPI::UI::UIText("Resolved Intent"));
    PlatformSummaryTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto PlatformSummaryText = PlatformSummaryCard.Add(SnAPI::UI::UIText(""));
    PlatformSummaryText.Element().ElementStyle().Apply("editor.panel_subtitle");
    PlatformSummaryText.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_buildModalPlatformSummaryText = PlatformSummaryText.Handle();

    auto OutputTab = Tabs.Add(SnAPI::UI::UIPanel("Editor.BuildModal.Output"));
    auto& OutputTabPanel = OutputTab.Element();
    OutputTabPanel.ElementStyle().Apply("editor.section_card");
    OutputTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    OutputTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    OutputTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    OutputTabPanel.Padding().Set(8.0f);
    OutputTabPanel.Gap().Set(8.0f);

    auto OutputScroll = OutputTab.Add(SnAPI::UI::UIScrollContainer{});
    auto& OutputScrollElement = OutputScroll.Element();
    OutputScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    OutputScrollElement.Height().Set(SnAPI::UI::Sizing::Fill());
    OutputScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    OutputScrollElement.ShowHorizontalScrollbar().Set(false);
    OutputScrollElement.ShowVerticalScrollbar().Set(true);
    OutputScrollElement.Smooth().Set(true);
    OutputScrollElement.Padding().Set(2.0f);
    OutputScrollElement.Gap().Set(10.0f);

    auto OutputCard = OutputScroll.Add(SnAPI::UI::UIPanel("Editor.BuildModal.OutputCard"));
    auto& OutputCardPanel = OutputCard.Element();
    OutputCardPanel.ElementStyle().Apply("editor.section_card");
    OutputCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    OutputCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    OutputCardPanel.Padding().Set(12.0f);
    OutputCardPanel.Gap().Set(8.0f);

    auto OutputTitle = OutputCard.Add(SnAPI::UI::UIText("Output and Archive"));
    OutputTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(OutputCard,
                     "Final output settings control where the staged package tree is promoted, how the final package "
                     "directory is named, and whether an archive should be emitted for handoff or CI artifacts.");

    auto OutputRootLabel = OutputCard.Add(SnAPI::UI::UIText("Output Root"));
    OutputRootLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(OutputCard,
                     "Optional user-facing package destination. Leave this empty to promote packages under the "
                     "project's Saved/Packages directory.");
    auto OutputRootInput = OutputCard.Add(SnAPI::UI::UIFilesystemPicker{});
    auto& OutputRootInputElement = OutputRootInput.Element();
    OutputRootInputElement.ElementStyle().Apply("editor.filesystem_picker");
    OutputRootInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    OutputRootInputElement.ReadOnly().Set(false);
    OutputRootInputElement.AllowMultiSelect().Set(false);
    OutputRootInputElement.PickDirectories().Set(true);
    OutputRootInputElement.ShowDirectories().Set(true);
    OutputRootInputElement.ShowFiles().Set(false);
    OutputRootInputElement.RestrictToRoot().Set(false);
    OutputRootInputElement.Placeholder().Set(std::string("Defaults to <Saved>/Packages"));
    OutputRootInputElement.Value().Set(m_buildModalOutputRootText);
    OutputRootInputElement.CurrentPath().Set(m_buildModalOutputRootText.empty() ? m_buildPanelState.AssetRootDirectory
                                                                                : m_buildModalOutputRootText);
    OutputRootInputElement.OnSelectionChanged(
        SnAPI::UI::TDelegate<void(const std::vector<std::string>&)>::Bind([this](const std::vector<std::string>& Values) {
            if (!Values.empty())
            {
                m_buildModalOutputRootText = Values.front();
                MarkBuildModalDraftDirty();
            }
        }));
    m_buildOutputRootInput = OutputRootInput.Handle();

    auto PackageDirLabel = OutputCard.Add(SnAPI::UI::UIText("Package Directory Name"));
    PackageDirLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(OutputCard,
                     "Optional final package directory leaf name. Leave this blank to use the standard "
                     "<Project>_<Profile>_<Platform>_<Configuration>_<BuildId> convention.");
    auto PackageDirInput = OutputCard.Add(SnAPI::UI::UITextInput{});
    auto& PackageDirInputElement = PackageDirInput.Element();
    PackageDirInputElement.ElementStyle().Apply("editor.text_input");
    PackageDirInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    PackageDirInputElement.Placeholder().Set("Leave blank for standard naming");
    PackageDirInputElement.Text().Set(m_buildModalPackageDirectoryText);
    PackageDirInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_buildModalPackageDirectoryText = Value;
        MarkBuildModalDraftDirty();
    }));
    m_buildPackageDirectoryInput = PackageDirInput.Handle();

    auto ArchiveCheckbox = OutputCard.Add(SnAPI::UI::UICheckbox("Create archive"));
    ArchiveCheckbox.Element().Checked().Set(m_buildModalArchiveEnabled);
    ArchiveCheckbox.Element().OnChanged([this](const bool Checked) {
        m_buildModalArchiveEnabled = Checked;
        MarkBuildModalDraftDirty();
    });
    m_buildArchiveEnabledCheckbox = ArchiveCheckbox.Handle();
    AddFieldHelpText(OutputCard,
                     "Enable final archive emission after the package directory is promoted. Archives are built from "
                     "the final copied package tree rather than directly from intermediate staging.");

    auto ArchiveFormatLabel = OutputCard.Add(SnAPI::UI::UIText("Archive Format"));
    ArchiveFormatLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(OutputCard,
                     "Choose the archive container emitted from the final package directory. V1 currently supports "
                     "`zip`, so this is intentionally a constrained dropdown instead of a free-form text field.");
    auto ArchiveFormatComboBuilder = OutputCard.Add(SnAPI::UI::UIComboBox{});
    auto& ArchiveFormatCombo = ArchiveFormatComboBuilder.Element();
    ArchiveFormatCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    std::vector<std::string> ArchiveFormatItems = BuildComboItemsWithCurrent(
        std::span<const std::string_view>(kKnownArchiveFormats.begin(), kKnownArchiveFormats.end()),
        "Use resolved profile format",
        m_buildModalArchiveFormatText);
    ArchiveFormatCombo.SetItems(ArchiveFormatItems);
    int32_t SelectedArchiveFormatIndex = 0;
    const std::string TrimmedArchiveFormat = TrimCopy(m_buildModalArchiveFormatText);
    if (!TrimmedArchiveFormat.empty())
    {
        const auto It = std::find(ArchiveFormatItems.begin(), ArchiveFormatItems.end(), TrimmedArchiveFormat);
        if (It != ArchiveFormatItems.end())
        {
            SelectedArchiveFormatIndex = static_cast<int32_t>(std::distance(ArchiveFormatItems.begin(), It));
        }
    }
    (void)ArchiveFormatCombo.SetSelectedIndex(SelectedArchiveFormatIndex, false);
    ArchiveFormatCombo.SetDisabled(!m_buildModalArchiveEnabled);
    ArchiveFormatCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        m_buildModalArchiveFormatText = Index <= 0 ? std::string{} : Text;
        MarkBuildModalDraftDirty();
    });
    m_buildArchiveFormatInput = ArchiveFormatComboBuilder.Handle();

    auto ArchiveFileLabel = OutputCard.Add(SnAPI::UI::UIText("Archive File Name"));
    ArchiveFileLabel.Element().ElementStyle().Apply("editor.menu_item");
    AddFieldHelpText(OutputCard,
                     "Optional archive file leaf name. Leave this blank to use the standard "
                     "<Project>_<Platform>_<Configuration>.<Format> convention.");
    auto ArchiveFileInput = OutputCard.Add(SnAPI::UI::UITextInput{});
    auto& ArchiveFileInputElement = ArchiveFileInput.Element();
    ArchiveFileInputElement.ElementStyle().Apply("editor.text_input");
    ArchiveFileInputElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ArchiveFileInputElement.Placeholder().Set("Leave blank for standard naming");
    ArchiveFileInputElement.Text().Set(m_buildModalArchiveFileText);
    ArchiveFileInputElement.SetDisabled(!m_buildModalArchiveEnabled);
    ArchiveFileInputElement.OnTextChanged(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
        m_buildModalArchiveFileText = Value;
        MarkBuildModalDraftDirty();
    }));
    m_buildArchiveFileInput = ArchiveFileInput.Handle();

    auto OutputSummaryCard = OutputScroll.Add(SnAPI::UI::UIPanel("Editor.BuildModal.OutputSummaryCard"));
    auto& OutputSummaryCardPanel = OutputSummaryCard.Element();
    OutputSummaryCardPanel.ElementStyle().Apply("editor.section_card");
    OutputSummaryCardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    OutputSummaryCardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    OutputSummaryCardPanel.Padding().Set(12.0f);
    OutputSummaryCardPanel.Gap().Set(8.0f);

    auto OutputSummaryTitle = OutputSummaryCard.Add(SnAPI::UI::UIText("Promotion Preview"));
    OutputSummaryTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(OutputSummaryCard,
                     "This preview reflects the final promoted package outputs after staging succeeds. It does not "
                     "change the build graph itself; it changes only the final copy/archive step.");
    auto OutputSummaryText = OutputSummaryCard.Add(SnAPI::UI::UIText(""));
    OutputSummaryText.Element().ElementStyle().Apply("editor.panel_subtitle");
    OutputSummaryText.Element().Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_buildModalOutputSummaryText = OutputSummaryText.Handle();

    auto HistoryTab = Tabs.Add(SnAPI::UI::UIPanel("Editor.BuildModal.History"));
    auto& HistoryTabPanel = HistoryTab.Element();
    HistoryTabPanel.ElementStyle().Apply("editor.section_card");
    HistoryTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    HistoryTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HistoryTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    HistoryTabPanel.Padding().Set(8.0f);
    HistoryTabPanel.Gap().Set(8.0f);

    auto HistoryControls = HistoryTab.Add(SnAPI::UI::UIPanel("Editor.BuildModal.HistoryControls"));
    auto& HistoryControlsPanel = HistoryControls.Element();
    ConfigureTransparentLayoutPanel(HistoryControlsPanel);
    HistoryControlsPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HistoryControlsPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HistoryControlsPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    HistoryControlsPanel.Gap().Set(8.0f);
    AddFieldHelpText(HistoryTab,
                     "Build history keeps each package invocation's frozen request, plan, report, logs, and manifests "
                     "under Saved/BuildHistory so failed and successful runs stay inspectable.");

    const bool HasSelectedHistory = SelectedBuildHistoryEntry() != nullptr;
    const std::string ComparisonTargetId = BuildHistoryComparisonTargetId();

    auto RefreshButton = HistoryControls.Add(SnAPI::UI::UIButton{});
    auto& RefreshButtonElement = RefreshButton.Element();
    RefreshButtonElement.ElementStyle().Apply("editor.project_modal_action_button");
    RefreshButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    RefreshButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    RefreshButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    RefreshButtonElement.SetDisabled(!m_buildPanelState.ProjectLoaded || !m_onBuildActionRequested);
    RefreshButtonElement.OnClick([this]() {
        if (!m_onBuildActionRequested)
        {
            return;
        }
        BuildActionRequest Request{};
        Request.Action = EBuildAction::RefreshHistory;
        m_onBuildActionRequested(Request);
    });
    auto RefreshLabel = RefreshButton.Add(SnAPI::UI::UIText("Refresh History"));
    auto& RefreshLabelText = RefreshLabel.Element();
    RefreshLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    RefreshLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto RetryButton = HistoryControls.Add(SnAPI::UI::UIButton{});
    auto& RetryButtonElement = RetryButton.Element();
    RetryButtonElement.ElementStyle().Apply("editor.project_modal_action_button");
    RetryButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    RetryButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    RetryButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    RetryButtonElement.SetDisabled(!HasSelectedHistory || !m_onBuildActionRequested || m_buildPanelState.BuildInProgress);
    RetryButtonElement.OnClick([this]() {
        if (!m_onBuildActionRequested)
        {
            return;
        }
        BuildActionRequest Request{};
        Request.Action = EBuildAction::RetryBuild;
        Request.SourceBuildId = m_buildModalSelectedHistoryBuildId;
        m_onBuildActionRequested(Request);
    });
    auto RetryLabel = RetryButton.Add(SnAPI::UI::UIText("Retry Selected"));
    auto& RetryLabelText = RetryLabel.Element();
    RetryLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    RetryLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto RebuildButton = HistoryControls.Add(SnAPI::UI::UIButton{});
    auto& RebuildButtonElement = RebuildButton.Element();
    RebuildButtonElement.ElementStyle().Apply("editor.project_modal_action_button_primary");
    RebuildButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    RebuildButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    RebuildButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    RebuildButtonElement.SetDisabled(!HasSelectedHistory || !m_onBuildActionRequested || m_buildPanelState.BuildInProgress);
    RebuildButtonElement.OnClick([this]() {
        if (!m_onBuildActionRequested)
        {
            return;
        }
        BuildActionRequest Request{};
        Request.Action = EBuildAction::RebuildAll;
        Request.SourceBuildId = m_buildModalSelectedHistoryBuildId;
        m_onBuildActionRequested(Request);
    });
    auto RebuildLabel = RebuildButton.Add(SnAPI::UI::UIText("Rebuild All"));
    auto& RebuildLabelText = RebuildLabel.Element();
    RebuildLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    RebuildLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto CompareButton = HistoryControls.Add(SnAPI::UI::UIButton{});
    auto& CompareButtonElement = CompareButton.Element();
    CompareButtonElement.ElementStyle().Apply("editor.project_modal_action_button");
    CompareButtonElement.Width().Set(SnAPI::UI::Sizing::Auto());
    CompareButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
    CompareButtonElement.ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
    CompareButtonElement.SetDisabled(!HasSelectedHistory || ComparisonTargetId.empty() || !m_onBuildActionRequested);
    CompareButtonElement.OnClick([this, ComparisonTargetId]() {
        if (!m_onBuildActionRequested)
        {
            return;
        }
        BuildActionRequest Request{};
        Request.Action = EBuildAction::CompareHistory;
        Request.SourceBuildId = m_buildModalSelectedHistoryBuildId;
        Request.CompareBuildId = ComparisonTargetId;
        m_onBuildActionRequested(Request);
    });
    auto CompareLabel = CompareButton.Add(SnAPI::UI::UIText("Compare"));
    auto& CompareLabelText = CompareLabel.Element();
    CompareLabelText.ElementStyle().Apply("editor.project_modal_action_button_text");
    CompareLabelText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

    auto HistoryBody = HistoryTab.Add(SnAPI::UI::UIPanel("Editor.BuildModal.HistoryBody"));
    auto& HistoryBodyPanel = HistoryBody.Element();
    ConfigureTransparentLayoutPanel(HistoryBodyPanel);
    HistoryBodyPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HistoryBodyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    HistoryBodyPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    HistoryBodyPanel.Gap().Set(8.0f);

    auto HistoryListScroll = HistoryBody.Add(SnAPI::UI::UIScrollContainer{});
    auto& HistoryListScrollElement = HistoryListScroll.Element();
    HistoryListScrollElement.Width().Set(SnAPI::UI::Sizing::Ratio(0.48f));
    HistoryListScrollElement.Height().Set(SnAPI::UI::Sizing::Fill());
    HistoryListScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    HistoryListScrollElement.ShowHorizontalScrollbar().Set(false);
    HistoryListScrollElement.ShowVerticalScrollbar().Set(true);
    HistoryListScrollElement.Smooth().Set(true);
    HistoryListScrollElement.Padding().Set(2.0f);
    HistoryListScrollElement.Gap().Set(6.0f);

    if (m_buildPanelState.HistoryEntries.empty())
    {
        auto EmptyText = HistoryListScroll.Add(SnAPI::UI::UIText("No build history is available for the active project yet."));
        auto& EmptyTextElement = EmptyText.Element();
        EmptyTextElement.ElementStyle().Apply("editor.panel_subtitle");
        EmptyTextElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    }
    else
    {
        for (const BuildHistoryEntryView& Entry : m_buildPanelState.HistoryEntries)
        {
            const bool IsSelected = Entry.BuildId == m_buildModalSelectedHistoryBuildId;

            auto EntryButton = HistoryListScroll.Add(SnAPI::UI::UIButton{});
            auto& EntryButtonElement = EntryButton.Element();
            EntryButtonElement.ElementStyle().Apply(IsSelected ? "editor.project_modal_mode_button_active"
                                                               : "editor.project_modal_mode_button");
            EntryButtonElement.Width().Set(SnAPI::UI::Sizing::Fill());
            EntryButtonElement.Height().Set(SnAPI::UI::Sizing::Auto());
            EntryButtonElement.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 8.0f, 8.0f, 8.0f});
            EntryButtonElement.OnClick([this, BuildId = Entry.BuildId]() {
                m_buildModalSelectedHistoryBuildId = BuildId;
                RebuildBuildModalOverlay();
            });

            auto EntryPanel = EntryButton.Add(SnAPI::UI::UIPanel("Editor.BuildModal.HistoryEntry"));
            auto& EntryPanelElement = EntryPanel.Element();
            ConfigureTransparentLayoutPanel(EntryPanelElement);
            EntryPanelElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
            EntryPanelElement.Width().Set(SnAPI::UI::Sizing::Fill());
            EntryPanelElement.Height().Set(SnAPI::UI::Sizing::Auto());
            EntryPanelElement.Gap().Set(2.0f);
            EntryPanelElement.Properties().SetProperty(
                SnAPI::UI::UIElementBase::VisibilityKey,
                SnAPI::UI::EVisibility::HitTestInvisible);

            auto EntryTitle = EntryPanel.Add(SnAPI::UI::UIText(Entry.Label));
            auto& EntryTitleText = EntryTitle.Element();
            EntryTitleText.ElementStyle().Apply("editor.panel_title");
            EntryTitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
            EntryTitleText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);

            auto EntrySummary = EntryPanel.Add(SnAPI::UI::UIText(Entry.Summary));
            auto& EntrySummaryText = EntrySummary.Element();
            EntrySummaryText.ElementStyle().Apply("editor.panel_subtitle");
            EntrySummaryText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
            EntrySummaryText.Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);
        }
    }

    auto HistoryDetail = HistoryBody.Add(SnAPI::UI::UIPanel("Editor.BuildModal.HistoryDetail"));
    auto& HistoryDetailPanel = HistoryDetail.Element();
    HistoryDetailPanel.ElementStyle().Apply("editor.section_card");
    HistoryDetailPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    HistoryDetailPanel.Width().Set(SnAPI::UI::Sizing::Ratio(0.52f));
    HistoryDetailPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    HistoryDetailPanel.Padding().Set(8.0f);
    HistoryDetailPanel.Gap().Set(6.0f);

    auto HistoryDetailTitle = HistoryDetail.Add(SnAPI::UI::UIText("Selection"));
    HistoryDetailTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto HistoryDetailText = HistoryDetail.Add(SnAPI::UI::UIText(""));
    auto& HistoryDetailTextElement = HistoryDetailText.Element();
    HistoryDetailTextElement.ElementStyle().Apply("editor.panel_subtitle");
    HistoryDetailTextElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_buildModalHistoryDetailText = HistoryDetailText.Handle();

    auto ComparisonTitle = HistoryDetail.Add(SnAPI::UI::UIText("Comparison"));
    ComparisonTitle.Element().ElementStyle().Apply("editor.panel_title");

    auto ComparisonText = HistoryDetail.Add(SnAPI::UI::UIText(""));
    auto& ComparisonTextElement = ComparisonText.Element();
    ComparisonTextElement.ElementStyle().Apply("editor.panel_subtitle");
    ComparisonTextElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_buildModalComparisonText = ComparisonText.Handle();

    auto ConsoleTab = Tabs.Add(SnAPI::UI::UIPanel("Editor.BuildModal.Console"));
    auto& ConsoleTabPanel = ConsoleTab.Element();
    ConsoleTabPanel.ElementStyle().Apply("editor.section_card");
    ConsoleTabPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConsoleTabPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConsoleTabPanel.Height().Set(SnAPI::UI::Sizing::Fill());
    ConsoleTabPanel.Padding().Set(8.0f);
    ConsoleTabPanel.Gap().Set(8.0f);

    auto ConsoleHeader = ConsoleTab.Add(SnAPI::UI::UIPanel("Editor.BuildModal.ConsoleHeader"));
    auto& ConsoleHeaderPanel = ConsoleHeader.Element();
    ConfigureTransparentLayoutPanel(ConsoleHeaderPanel);
    ConsoleHeaderPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConsoleHeaderPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConsoleHeaderPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ConsoleHeaderPanel.Gap().Set(4.0f);

    auto ConsoleTitle = ConsoleHeader.Add(SnAPI::UI::UIText("Console Output"));
    ConsoleTitle.Element().ElementStyle().Apply("editor.panel_title");
    AddFieldHelpText(ConsoleHeader,
                     "This log captures packaging session stdout, stderr, shared build events, and streamed CMake "
                     "configure/build output. The current execution model is synchronous, so the console is a "
                     "session transcript for the active action rather than a detached live terminal.");

    auto ConsoleSummaryText = ConsoleHeader.Add(SnAPI::UI::UIText(""));
    auto& ConsoleSummaryTextElement = ConsoleSummaryText.Element();
    ConsoleSummaryTextElement.ElementStyle().Apply("editor.panel_subtitle");
    ConsoleSummaryTextElement.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);
    m_buildModalConsoleSummaryText = ConsoleSummaryText.Handle();

    auto ConsoleScroll = ConsoleTab.Add(SnAPI::UI::UIScrollContainer{});
    auto& ConsoleScrollElement = ConsoleScroll.Element();
    ConsoleScrollElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ConsoleScrollElement.Height().Set(SnAPI::UI::Sizing::Fill());
    ConsoleScrollElement.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConsoleScrollElement.ShowHorizontalScrollbar().Set(true);
    ConsoleScrollElement.ShowVerticalScrollbar().Set(true);
    ConsoleScrollElement.Smooth().Set(true);
    ConsoleScrollElement.AutoScrollToEnd().Set(true);
    ConsoleScrollElement.Padding().Set(10.0f);
    ConsoleScrollElement.Gap().Set(0.0f);
    m_buildModalConsoleScroll = ConsoleScroll.Handle();

    auto ConsoleText = ConsoleScroll.Add(SnAPI::UI::UIText(""));
    auto& ConsoleTextElement = ConsoleText.Element();
    ConsoleTextElement.ElementStyle().Apply("editor.console_log_text");
    ConsoleTextElement.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
    m_buildModalConsoleText = ConsoleText.Handle();

    TabsElement.SetTabLabel(0, "Overview");
    TabsElement.SetTabLabel(1, "Content");
    TabsElement.SetTabLabel(2, "Platform");
    TabsElement.SetTabLabel(3, "Output");
    TabsElement.SetTabLabel(4, "History");
    TabsElement.SetTabLabel(5, "Console");
    RefreshBuildModalLiveState();
}

void EditorLayout::DestroyBuildModalOverlay()
{
    if (m_buildModalTabs.Id.Value != 0 && m_context)
    {
        if (auto* Tabs = dynamic_cast<SnAPI::UI::UITabs*>(&m_context->GetElement(m_buildModalTabs.Id)))
        {
            m_buildModalActiveTabIndex = std::max(0, Tabs->ActiveIndex().Get());
        }
    }

    if (m_context && m_buildModalOverlay.Id.Value != 0)
    {
        const SnAPI::UI::ElementId OverlayId = m_buildModalOverlay.Id;
        const SnAPI::UI::ElementId CapturedElement = m_context->GetCapture();
        if (IsElementWithinSubtree(*m_context, CapturedElement, OverlayId))
        {
            m_context->ReleaseCapture();
        }
        m_context->DestroyElement(OverlayId);
    }

    m_buildModalOverlay = {};
    m_buildModalTabs = {};
    m_buildProfileCombo = {};
    m_buildConfigurationCombo = {};
    m_buildDependencyPolicyCombo = {};
    m_buildChunkStrategyCombo = {};
    m_buildPlatformInput = {};
    m_buildExecutionEnvironmentInput = {};
    m_buildSelectedLevelsInput = {};
    m_buildExplicitAssetsInput = {};
    m_buildIncludeFoldersInput = {};
    m_buildExcludeFoldersInput = {};
    m_buildIncludeLabelsInput = {};
    m_buildExcludeLabelsInput = {};
    m_buildIncludeKindsInput = {};
    m_buildExcludeKindsInput = {};
    m_buildAllowExplicitOverrideCheckbox = {};
    m_buildArchiveEnabledCheckbox = {};
    m_buildOutputRootInput = {};
    m_buildPackageDirectoryInput = {};
    m_buildArchiveFormatInput = {};
    m_buildArchiveFileInput = {};
    m_buildModalSubtitleText = {};
    m_buildModalOverviewSummaryText = {};
    m_buildModalProfileSummaryText = {};
    m_buildModalLatestSummaryText = {};
    m_buildModalPlatformSummaryText = {};
    m_buildModalOutputSummaryText = {};
    m_buildModalHistoryDetailText = {};
    m_buildModalComparisonText = {};
    m_buildModalConsoleSummaryText = {};
    m_buildModalConsoleScroll = {};
    m_buildModalConsoleText = {};
    m_buildModalProfileKeys.clear();
}

void EditorLayout::OpenBuildModal()
{
    if (!m_context || !m_projectState.IsLoaded)
    {
        return;
    }

    CloseContextMenu();
    ClosePluginModal();
    CloseModuleModal();
    CloseProjectModal(true);
    CloseProjectSettingsModal();
    ResetBuildModalDraftFromSelectedProfile();
    m_buildModalOpen = true;
    RefreshBuildModalVisibility();
    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::CloseBuildModal()
{
    if (!m_buildModalOpen && m_buildModalOverlay.Id.Value == 0)
    {
        return;
    }

    m_buildModalOpen = false;
    RefreshBuildModalVisibility();
    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::RefreshBuildModalVisibility()
{
    if (!m_context)
    {
        return;
    }

    if (m_buildModalOpen)
    {
        EnsureBuildModalOverlay();
        return;
    }

    DestroyBuildModalOverlay();
}

void EditorLayout::RebuildBuildModalOverlay()
{
    if (!m_buildModalOpen)
    {
        return;
    }

    if (m_buildModalTabs.Id.Value != 0 && m_context)
    {
        if (auto* Tabs = dynamic_cast<SnAPI::UI::UITabs*>(&m_context->GetElement(m_buildModalTabs.Id)))
        {
            m_buildModalActiveTabIndex = std::max(0, Tabs->ActiveIndex().Get());
        }
    }

    DestroyBuildModalOverlay();
    EnsureBuildModalOverlay();
    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
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
            Request.ImportSettings = std::make_shared<AssimpImporterSettings>(m_contentImportAssimpSettings);

            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.GenerateNormals",
                BoolToText(m_contentImportAssimpSettings.Mesh.GenerateNormals));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.GenerateTangents",
                BoolToText(m_contentImportAssimpSettings.Mesh.GenerateTangents));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.FlipUVs",
                BoolToText(m_contentImportAssimpSettings.Mesh.FlipUVs));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.OptimizeMeshes",
                BoolToText(m_contentImportAssimpSettings.Mesh.OptimizeMeshes));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.ForceSkeletal",
                BoolToText(m_contentImportAssimpSettings.Mesh.ForceSkeletal));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.ForceStatic",
                BoolToText(m_contentImportAssimpSettings.Mesh.ForceStatic));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.ImportMaterials",
                BoolToText(m_contentImportAssimpSettings.Mesh.ImportMaterials));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.ImportTextures",
                BoolToText(m_contentImportAssimpSettings.Mesh.ImportTextures));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.ImportAnimations",
                BoolToText(m_contentImportAssimpSettings.Mesh.ImportAnimations));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.ImportSkeleton",
                BoolToText(m_contentImportAssimpSettings.Mesh.ImportSkeleton));
            Request.BuildOptions.emplace(
                "SnAPI.GF.Assimp.MaxBonesPerVertex",
                std::to_string(std::max(1u, m_contentImportAssimpSettings.Mesh.MaxBonesPerVertex)));
            if (!m_contentImportAssimpSettings.LogicalNameOverride.empty())
            {
                Request.BuildOptions.emplace("SnAPI.GF.Assimp.LogicalName", m_contentImportAssimpSettings.LogicalNameOverride);
            }
            if (!m_contentImportAssimpSettings.DefaultShaderModule.empty())
            {
                Request.BuildOptions.emplace("SnAPI.GF.Assimp.DefaultShaderModule", m_contentImportAssimpSettings.DefaultShaderModule);
            }
            if (!m_contentImportAssimpSettings.DefaultShadingModel.empty())
            {
                Request.BuildOptions.emplace(
                    "SnAPI.GF.Assimp.DefaultShadingModel",
                    m_contentImportAssimpSettings.DefaultShadingModel);
            }
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
            Request.ImportSettings = std::make_shared<TextureImporterSettings>(m_contentImportTextureSettings);

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
        (void)Panel->BindObject<AssimpImporterSettings>(&m_contentImportAssimpSettings);
        break;
    case EImportProfile::Texture:
        (void)Panel->BindObject<TextureImporterSettings>(&m_contentImportTextureSettings);
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
    CloseBuildModal();
    CloseProjectSettingsModal();
    m_projectModalAction = EProjectAction::CreateNew;
    m_projectModalOpen = true;
    m_projectModalShowWelcome = false;
    m_projectTemplatePreset = EProjectTemplatePreset::RuntimeGame;
    m_projectNameText = "NewProject";
    m_projectDisplayNameText = "New Project";
    m_projectCompanyText.clear();
    m_projectNamespaceText = "NewProject";
    m_projectRuntimeModuleText = "NewProject";
    m_projectEditorModuleText = "NewProjectEditor";
    m_projectStartupLevelText = std::string(ProjectDescriptorService::kDefaultStartupLevelAsset);
    m_projectCreateRuntimeModule = true;
    m_projectCreateEditorModule = false;
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

void EditorLayout::OpenPluginCreateModal()
{
    if (!m_context)
    {
        return;
    }

    CloseContextMenu();
    CloseBuildModal();
    CloseProjectSettingsModal();
    m_pluginModalOpen = true;
    m_pluginTemplatePreset = EPluginTemplatePreset::Runtime;
    m_pluginNameText = "NewPlugin";
    m_pluginDisplayNameText = "New Plugin";
    m_pluginCompanyText.clear();
    m_pluginVersionText = "0.1.0";
    m_pluginDescriptionText.clear();
    m_pluginNamespaceText = "NewPlugin";
    m_pluginRuntimeModuleText = "NewPlugin";
    m_pluginEditorModuleText = "NewPluginEditor";
    m_pluginCreateRuntimeModule = true;
    m_pluginCreateEditorModule = false;
    m_pluginCanContainAssets = true;
    if (m_pluginDirectoryText.empty())
    {
        std::error_code Error{};
        const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
        if (!Error && !CurrentPath.empty())
        {
            m_pluginDirectoryText = CurrentPath.string();
        }
    }
    DestroyPluginModalOverlay();
    RefreshPluginModalVisibility();
    RefreshPluginModalOkButtonState();
    m_context->MarkLayoutDirty();
}

void EditorLayout::OpenProjectModuleModal()
{
    if (!m_context)
    {
        return;
    }

    CloseContextMenu();
    CloseBuildModal();
    m_moduleModalAction = EModuleAction::CreateProjectModule;
    m_moduleModalOpen = true;
    m_moduleType = EProjectModuleType::Runtime;
    m_moduleNameText = "GameplaySystems";
    m_moduleNamespaceText = "GameplaySystems";
    m_moduleRootText.clear();
    m_modulePublicDependenciesText.clear();
    m_modulePrivateDependenciesText.clear();
    m_modulePlatformsText.clear();
    m_moduleDefinitionsText.clear();
    m_moduleUseReflectionGen = false;
    m_moduleUseSwig = false;
    m_moduleGenerateGameplayBootstrap = true;
    m_moduleLoadInEditor = DefaultLoadInEditorForModule(m_moduleType);
    m_moduleLoadInRuntime = DefaultLoadInRuntimeForModule(m_moduleType);
    m_moduleDescriptorFilePathText = m_projectState.ProjectFilePath;
    DestroyModuleModalOverlay();
    RefreshModuleModalVisibility();
    RefreshModuleModalOkButtonState();
    m_context->MarkLayoutDirty();
}

void EditorLayout::OpenPluginModuleModal()
{
    if (!m_context)
    {
        return;
    }

    CloseContextMenu();
    CloseBuildModal();
    m_moduleModalAction = EModuleAction::CreatePluginModule;
    m_moduleModalOpen = true;
    m_moduleType = EProjectModuleType::Runtime;
    m_moduleNameText = "PluginRuntime";
    m_moduleNamespaceText = "PluginRuntime";
    m_moduleRootText.clear();
    m_modulePublicDependenciesText.clear();
    m_modulePrivateDependenciesText.clear();
    m_modulePlatformsText.clear();
    m_moduleDefinitionsText.clear();
    m_moduleUseReflectionGen = false;
    m_moduleUseSwig = false;
    m_moduleGenerateGameplayBootstrap = true;
    m_moduleLoadInEditor = DefaultLoadInEditorForModule(m_moduleType);
    m_moduleLoadInRuntime = DefaultLoadInRuntimeForModule(m_moduleType);
    if (m_moduleDescriptorFilePathText.empty())
    {
        std::error_code Error{};
        const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
        if (!Error && !CurrentPath.empty())
        {
            m_moduleDescriptorFilePathText = (CurrentPath / std::string(PluginDescriptorService::kDefaultPluginFileName)).string();
        }
    }
    DestroyModuleModalOverlay();
    RefreshModuleModalVisibility();
    RefreshModuleModalOkButtonState();
    m_context->MarkLayoutDirty();
}

void EditorLayout::OpenProjectOpenModal()
{
    if (!m_context)
    {
        return;
    }

    CloseContextMenu();
    CloseBuildModal();
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
    CloseBuildModal();
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

void EditorLayout::ClosePluginModal()
{
    if (!m_pluginModalOpen && m_pluginModalOverlay.Id.Value == 0)
    {
        return;
    }

    m_pluginModalOpen = false;
    RefreshPluginModalVisibility();
    if (m_context)
    {
        m_context->MarkLayoutDirty();
    }
}

void EditorLayout::CloseModuleModal()
{
    if (!m_moduleModalOpen && m_moduleModalOverlay.Id.Value == 0)
    {
        return;
    }

    m_moduleModalOpen = false;
    RefreshModuleModalVisibility();
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
        Request.CreateRequest.ProjectName = Request.ProjectName;
        Request.CreateRequest.ParentDirectory = Request.ProjectDirectory;
        Request.CreateRequest.ProjectFileName = std::string(kDefaultProjectConfigFileName);

        auto DefaultDescriptor = ProjectCreationService::BuildDefaultDescriptor(Request.ProjectName);
        if (!DefaultDescriptor)
        {
            RefreshProjectModalOkButtonState();
            return;
        }

        Request.CreateRequest.Descriptor = std::move(*DefaultDescriptor);
        if (!TrimCopy(m_projectDisplayNameText).empty())
        {
            Request.CreateRequest.Descriptor.Project.DisplayName = TrimCopy(m_projectDisplayNameText);
        }
        if (!TrimCopy(m_projectCompanyText).empty())
        {
            Request.CreateRequest.Descriptor.Project.Company = TrimCopy(m_projectCompanyText);
        }
        if (!TrimCopy(m_projectStartupLevelText).empty())
        {
            Request.CreateRequest.Descriptor.Startup.StartupLevelAsset = TrimCopy(m_projectStartupLevelText);
        }
        Request.CreateRequest.Code.CreateStarterRuntimeModule = m_projectCreateRuntimeModule;
        Request.CreateRequest.Code.RuntimeModuleName = TrimCopy(m_projectRuntimeModuleText);
        Request.CreateRequest.Code.NamespaceRoot = TrimCopy(m_projectNamespaceText);
        Request.CreateRequest.Code.CreateStarterEditorModule = m_projectCreateEditorModule;
        Request.CreateRequest.Code.EditorModuleName = TrimCopy(m_projectEditorModuleText);

        if (Request.ProjectName.empty() || Request.ProjectDirectory.empty() ||
            (m_projectCreateRuntimeModule && Request.CreateRequest.Code.RuntimeModuleName.empty()) ||
            (m_projectCreateEditorModule && Request.CreateRequest.Code.EditorModuleName.empty()))
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

    if (m_onProjectActionRequested)
    {
        m_onProjectActionRequested(Request);
    }
    CloseProjectModal(true);
}

void EditorLayout::ConfirmPluginModal()
{
    if (!m_pluginModalOpen)
    {
        return;
    }

    PluginActionRequest Request{};
    Request.Action = EPluginAction::CreateNew;
    Request.CreateRequest.PluginName = TrimCopy(m_pluginNameText);
    Request.CreateRequest.ParentDirectory = TrimCopy(m_pluginDirectoryText);
    Request.CreateRequest.PluginFileName = std::string(PluginDescriptorService::kDefaultPluginFileName);

    auto DefaultDescriptor = PluginCreationService::BuildDefaultDescriptor(Request.CreateRequest.PluginName);
    if (!DefaultDescriptor)
    {
        RefreshPluginModalOkButtonState();
        return;
    }

    Request.CreateRequest.Descriptor = std::move(*DefaultDescriptor);
    if (!TrimCopy(m_pluginDisplayNameText).empty())
    {
        Request.CreateRequest.Descriptor.Plugin.DisplayName = TrimCopy(m_pluginDisplayNameText);
    }
    if (!TrimCopy(m_pluginCompanyText).empty())
    {
        Request.CreateRequest.Descriptor.Plugin.Company = TrimCopy(m_pluginCompanyText);
    }
    if (!TrimCopy(m_pluginVersionText).empty())
    {
        Request.CreateRequest.Descriptor.Plugin.Version = TrimCopy(m_pluginVersionText);
    }
    if (!TrimCopy(m_pluginDescriptionText).empty())
    {
        Request.CreateRequest.Descriptor.Plugin.Description = TrimCopy(m_pluginDescriptionText);
    }
    Request.CreateRequest.Descriptor.Plugin.CanContainAssets = m_pluginCanContainAssets;
    Request.CreateRequest.Code.CreateStarterRuntimeModule = m_pluginCreateRuntimeModule;
    Request.CreateRequest.Code.RuntimeModuleName = TrimCopy(m_pluginRuntimeModuleText);
    Request.CreateRequest.Code.NamespaceRoot = TrimCopy(m_pluginNamespaceText);
    Request.CreateRequest.Code.CreateStarterEditorModule = m_pluginCreateEditorModule;
    Request.CreateRequest.Code.EditorModuleName = TrimCopy(m_pluginEditorModuleText);

    if (Request.CreateRequest.PluginName.empty() || Request.CreateRequest.ParentDirectory.empty() ||
        (m_pluginCreateRuntimeModule && Request.CreateRequest.Code.RuntimeModuleName.empty()) ||
        (m_pluginCreateEditorModule && Request.CreateRequest.Code.EditorModuleName.empty()) ||
        (!m_pluginCreateRuntimeModule && !m_pluginCreateEditorModule && !m_pluginCanContainAssets))
    {
        RefreshPluginModalOkButtonState();
        return;
    }

    if (m_onPluginActionRequested)
    {
        m_onPluginActionRequested(Request);
    }
    ClosePluginModal();
}

void EditorLayout::ConfirmModuleModal()
{
    if (!m_moduleModalOpen)
    {
        return;
    }

    ModuleActionRequest Request{};
    Request.Action = m_moduleModalAction;
    if (m_moduleModalAction == EModuleAction::CreatePluginModule)
    {
        Request.PluginRequest.PluginFilePath = TrimCopy(m_moduleDescriptorFilePathText);
        Request.PluginRequest.ModuleName = TrimCopy(m_moduleNameText);
        Request.PluginRequest.ModuleType = m_moduleType;
        Request.PluginRequest.ModuleRoot = TrimCopy(m_moduleRootText);
        Request.PluginRequest.NamespaceRoot = TrimCopy(m_moduleNamespaceText);
        Request.PluginRequest.PublicDependencies = ParseMultilineEntries(m_modulePublicDependenciesText);
        Request.PluginRequest.PrivateDependencies = ParseMultilineEntries(m_modulePrivateDependenciesText);
        Request.PluginRequest.Platforms = ParseMultilineEntries(m_modulePlatformsText);
        Request.PluginRequest.PreprocessorDefinitions = ParseMultilineEntries(m_moduleDefinitionsText);
        Request.PluginRequest.UseReflectionGen = m_moduleUseReflectionGen;
        Request.PluginRequest.UseSWIG = m_moduleUseSwig;
        Request.PluginRequest.GenerateGameplayBootstrap = m_moduleGenerateGameplayBootstrap;
        Request.PluginRequest.LoadInEditor = m_moduleLoadInEditor;
        Request.PluginRequest.LoadInRuntime = m_moduleLoadInRuntime;
        if (Request.PluginRequest.PluginFilePath.empty() || Request.PluginRequest.ModuleName.empty())
        {
            RefreshModuleModalOkButtonState();
            return;
        }
    }
    else
    {
        Request.ProjectRequest.ProjectFilePath = TrimCopy(m_moduleDescriptorFilePathText);
        Request.ProjectRequest.ModuleName = TrimCopy(m_moduleNameText);
        Request.ProjectRequest.ModuleType = m_moduleType;
        Request.ProjectRequest.ModuleRoot = TrimCopy(m_moduleRootText);
        Request.ProjectRequest.NamespaceRoot = TrimCopy(m_moduleNamespaceText);
        Request.ProjectRequest.PublicDependencies = ParseMultilineEntries(m_modulePublicDependenciesText);
        Request.ProjectRequest.PrivateDependencies = ParseMultilineEntries(m_modulePrivateDependenciesText);
        Request.ProjectRequest.Platforms = ParseMultilineEntries(m_modulePlatformsText);
        Request.ProjectRequest.PreprocessorDefinitions = ParseMultilineEntries(m_moduleDefinitionsText);
        Request.ProjectRequest.UseReflectionGen = m_moduleUseReflectionGen;
        Request.ProjectRequest.UseSWIG = m_moduleUseSwig;
        Request.ProjectRequest.GenerateGameplayBootstrap = m_moduleGenerateGameplayBootstrap;
        Request.ProjectRequest.LoadInEditor = m_moduleLoadInEditor;
        Request.ProjectRequest.LoadInRuntime = m_moduleLoadInRuntime;
        if (Request.ProjectRequest.ProjectFilePath.empty() || Request.ProjectRequest.ModuleName.empty())
        {
            RefreshModuleModalOkButtonState();
            return;
        }
    }

    if (m_onModuleActionRequested)
    {
        m_onModuleActionRequested(Request);
    }
    CloseModuleModal();
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

void EditorLayout::RefreshPluginModalVisibility()
{
    if (!m_context)
    {
        return;
    }

    if (m_pluginModalOpen)
    {
        EnsurePluginModalOverlay();
        return;
    }

    DestroyPluginModalOverlay();
}

void EditorLayout::RefreshPluginModalOkButtonState()
{
    if (!m_context || m_pluginModalOkButton.Id.Value == 0)
    {
        return;
    }

    const bool CanConfirm = !TrimCopy(m_pluginNameText).empty() &&
        !TrimCopy(m_pluginDirectoryText).empty() &&
        (!m_pluginCreateRuntimeModule || !TrimCopy(m_pluginRuntimeModuleText).empty()) &&
        (!m_pluginCreateEditorModule || !TrimCopy(m_pluginEditorModuleText).empty()) &&
        (m_pluginCreateRuntimeModule || m_pluginCreateEditorModule || m_pluginCanContainAssets);
    if (auto* Button = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_pluginModalOkButton.Id)))
    {
        Button->SetDisabled(!CanConfirm);
    }
}

void EditorLayout::RefreshModuleModalVisibility()
{
    if (!m_context)
    {
        return;
    }

    if (m_moduleModalOpen)
    {
        EnsureModuleModalOverlay();
        return;
    }

    DestroyModuleModalOverlay();
}

void EditorLayout::RefreshModuleModalOkButtonState()
{
    if (!m_context || m_moduleModalOkButton.Id.Value == 0)
    {
        return;
    }

    const bool CanConfirm = !TrimCopy(m_moduleDescriptorFilePathText).empty() && !TrimCopy(m_moduleNameText).empty();
    if (auto* Button = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_moduleModalOkButton.Id)))
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
            PropertyPanel->SetObjectMutatedHandler(m_onContentAssetInspectorRuntimeMutated);
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
            ImportPanel->SetObjectMutatedHandler(m_onContentAssetInspectorImportMutated);
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
    ViewportElement.FeatureProfile().Set(EGameRenderFeatureProfile::EditorWorld);
    ViewportElement.AutoApplyFeatureProfile().Set(true);
    ViewportElement.RenderScale().Set(1.0f);
    ViewportElement.Enabled().Set(true);
    ViewportElement.SetGameRuntime(&Runtime);
    ViewportElement.SetDragDropEventHandler(
        SnAPI::UI::TDelegate<bool(const SnAPI::UI::DragDropEvent&, std::uint32_t, bool)>::Bind(
            [this](const SnAPI::UI::DragDropEvent& Event, const std::uint32_t RoutedTypeId, const bool ContainsPointer) -> bool {
            const ContentAssetDragPayload* Payload = TryGetContentAssetDragPayload(Event);
            if (!Payload || !ContainsPointer)
            {
                return false;
            }

            if (RoutedTypeId != SnAPI::UI::RoutedEventTypes::Drop.Id)
            {
                return true;
            }

            if (!m_onContentAssetDropRequested)
            {
                return false;
            }

            ContentAssetDropRequest Request{};
            Request.AssetKey = Payload->AssetKey;
            Request.Target = EContentAssetDropTarget::Viewport;
            Request.ScreenPosition = Event.Position;
            m_onContentAssetDropRequested(Request);
            return true;
        }));
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

    auto ConduitActionsRow = ConduitTab.Add(SnAPI::UI::UIPanel("Editor.ConduitWorkspaceActions"));
    auto& ConduitActionsRowPanel = ConduitActionsRow.Element();
    ConduitActionsRowPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    ConduitActionsRowPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitActionsRowPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    ConduitActionsRowPanel.Gap().Set(6.0f);
    ConduitActionsRowPanel.Padding().Set(0.0f);
    ConduitActionsRowPanel.Background().Set(SnAPI::UI::Color::Transparent());
    ConduitActionsRowPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    ConduitActionsRowPanel.BorderThickness().Set(0.0f);
    ConduitActionsRowPanel.CornerRadius().Set(0.0f);

    auto ConduitActionsSpacer = ConduitActionsRow.Add(SnAPI::UI::UIPanel("Editor.ConduitWorkspaceActionsSpacer"));
    auto& ConduitActionsSpacerPanel = ConduitActionsSpacer.Element();
    ConfigureTransparentLayoutPanel(ConduitActionsSpacerPanel);
    ConduitActionsSpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ConduitActionsSpacerPanel.Height().Set(SnAPI::UI::Sizing::Auto());

    auto ConduitCompileButtonBuilder = ConduitActionsRow.Add(SnAPI::UI::UIButton{});
    auto& ConduitCompileButton = ConduitCompileButtonBuilder.Element();
    ConduitCompileButton.ElementStyle().Apply("editor.toolbar_button");
    ConduitCompileButton.Width().Set(SnAPI::UI::Sizing::Auto());
    ConduitCompileButton.ElementPadding().Set(SnAPI::UI::Padding{8.0f, 4.0f, 8.0f, 4.0f});
    ConduitCompileButton.OnClick([this]() {
        if (m_onConduitCompileRequested)
        {
            m_onConduitCompileRequested();
        }
    });
    auto ConduitCompileLabel = ConduitCompileButtonBuilder.Add(SnAPI::UI::UIText("Compile Graph"));
    ConduitCompileLabel.Element().ElementStyle().Apply("editor.menu_item");
    m_conduitCompileButton = ConduitCompileButtonBuilder.Handle();

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

    auto GraphSelfTypeLabel = ConduitVariablesCard.Add(SnAPI::UI::UIText("Graph Self Type"));
    auto& GraphSelfTypeLabelText = GraphSelfTypeLabel.Element();
    GraphSelfTypeLabelText.ElementStyle().Apply("editor.panel_subtitle");
    GraphSelfTypeLabelText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 2.0f, 0.0f, 0.0f});

    auto GraphSelfTypeComboBuilder = ConduitVariablesCard.Add(SnAPI::UI::UIComboBox{});
    auto& GraphSelfTypeCombo = GraphSelfTypeComboBuilder.Element();
    GraphSelfTypeCombo.Width().Set(SnAPI::UI::Sizing::Fill());
    GraphSelfTypeCombo.Height().Set(SnAPI::UI::Sizing::Auto());
    GraphSelfTypeCombo.Placeholder().Set(std::string("Select graph self type"));
    GraphSelfTypeCombo.OnChanged([this](const int32_t Index, const std::string& Text) {
        (void)Text;
        if (!m_onConduitGraphSelfTypeRequested)
        {
            return;
        }
        if (Index < 0 || static_cast<std::size_t>(Index) >= m_conduitWorkspaceState.GraphSelfTypeOptions.size())
        {
            m_onConduitGraphSelfTypeRequested(TypeId{});
            return;
        }
        m_onConduitGraphSelfTypeRequested(
            m_conduitWorkspaceState.GraphSelfTypeOptions[static_cast<std::size_t>(Index)].Type);
    });
    m_conduitGraphSelfTypeCombo = GraphSelfTypeComboBuilder.Handle();

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

    auto ConduitPaletteScrollBuilder = ConduitPaletteCard.Add(SnAPI::UI::UIScrollContainer{});
    auto& ConduitPaletteScroll = ConduitPaletteScrollBuilder.Element();
    ConduitPaletteScroll.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitPaletteScroll.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    m_conduitPaletteScroll = ConduitPaletteScrollBuilder.Handle();

    auto ConduitPaletteContentBuilder = ConduitPaletteScrollBuilder.Add(SnAPI::UI::UIPanel("Editor.ConduitPaletteContent"));
    auto& ConduitPaletteContent = ConduitPaletteContentBuilder.Element();
    ConduitPaletteContent.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    ConduitPaletteContent.Width().Set(SnAPI::UI::Sizing::Fill());
    ConduitPaletteContent.Height().Set(SnAPI::UI::Sizing::Auto());
    ConduitPaletteContent.Padding().Set(0.0f);
    ConduitPaletteContent.Gap().Set(6.0f);
    m_conduitPaletteContentPanel = ConduitPaletteContentBuilder.Handle();

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
        SnAPI::UI::UIText("Left drag nodes. Drag from output pins onto input pins to connect data or label flow. Right or middle drag pans. Mouse wheel zooms."));
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
    ConduitCanvas.SetPinConnectedHandler(
        SnAPI::UI::TDelegate<void(const Uuid&, const std::string&, const Uuid&, const std::string&)>::Bind(
            [this](const Uuid& SourceNodeId,
                   const std::string& SourcePin,
                   const Uuid& TargetNodeId,
                   const std::string& TargetPin) {
                if (m_onConduitPinConnectedRequested)
                {
                    m_onConduitPinConnectedRequested(SourceNodeId, SourcePin, TargetNodeId, TargetPin);
                }
            }));
    ConduitCanvas.SetSpawnMenuRequestedHandler(
        SnAPI::UI::TDelegate<void(const SnAPI::GameFramework::Conduit::Editor::GraphSpawnMenuRequest&)>::Bind(
            [this](const SnAPI::GameFramework::Conduit::Editor::GraphSpawnMenuRequest& Request) {
                if (m_onConduitSpawnMenuRequest)
                {
                    m_onConduitSpawnMenuRequest(Request);
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

    auto NodeDefaultInputsPanelBuilder = NodeInspectorPanelBuilder.Add(SnAPI::UI::UIPanel("Editor.ConduitNodeDefaultInputsPanel"));
    auto& NodeDefaultInputsPanel = NodeDefaultInputsPanelBuilder.Element();
    NodeDefaultInputsPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    NodeDefaultInputsPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    NodeDefaultInputsPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    NodeDefaultInputsPanel.Gap().Set(6.0f);
    NodeDefaultInputsPanel.Padding().Set(0.0f);
    NodeDefaultInputsPanel.Background().Set(SnAPI::UI::Color::Transparent());
    NodeDefaultInputsPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
    NodeDefaultInputsPanel.BorderThickness().Set(0.0f);
    NodeDefaultInputsPanel.CornerRadius().Set(0.0f);
    m_conduitNodeDefaultInputsPanel = NodeDefaultInputsPanelBuilder.Handle();

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

    CameraComponent* ActiveCameraComponent = ResolveActiveCameraComponent(Runtime, ActiveCamera);
    std::shared_ptr<GameRenderCamera> RetainedCamera{};
    GameRenderCamera* RenderCamera = nullptr;
    if (ActiveCameraComponent)
    {
        RetainedCamera = ActiveCameraComponent->CameraShared();
        RenderCamera = RetainedCamera.get();
    }

    if (!RenderCamera)
    {
        if (auto* WorldPtr = Runtime.WorldPtr())
        {
            RetainedCamera = WorldPtr->Renderer().ActiveCameraShared();
            RenderCamera = RetainedCamera ? RetainedCamera.get() : WorldPtr->Renderer().ActiveCamera();
        }
    }

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
