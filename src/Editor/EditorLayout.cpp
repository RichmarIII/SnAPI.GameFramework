#include "Editor/EditorLayout.h"

#include "BaseNode.h"
#include "CameraComponent.h"
#include "Editor/EditorSelectionModel.h"
#include "GameRuntime.h"
#include "BaseComponent.h"
#include "Level.h"
#include "NodeCast.h"
#include "PawnBase.h"
#include "PlayerStart.h"
#include "RendererSystem.h"
#include "Serialization.h"
#include "StaticTypeId.h"
#include "TypeAutoRegistry.h"
#include "TypeRegistry.h"
#include "UIPropertyPanel.h"
#include "UIRenderViewport.h"
#include "UISystem.h"
#include "World.h"

#include <UIContext.h>
#include <UIColorPicker.h>
#include <UIDatePicker.h>
#include <UIDockZone.h>
#include <UIElementBase.h>
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

#include <array>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
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

constexpr std::string_view kBrandIconPath = "Editor/Assets/component.svg";
constexpr std::string_view kHierarchyIconPath = "Editor/Assets/hierarchy-circle.svg";
constexpr std::string_view kHierarchyWorldIconPath = "Editor/Assets/world.svg";
constexpr std::string_view kHierarchyLevelIconPath = "Editor/Assets/level.svg";
constexpr std::string_view kHierarchyNodeIconPath = "Editor/Assets/component.svg";
constexpr std::string_view kSearchIconPath = "Editor/Assets/options-vertical.svg";
constexpr std::string_view kGameViewIconPath = "Editor/Assets/box.svg";
constexpr std::string_view kInspectorIconPath = "Editor/Assets/settings.svg";
constexpr std::string_view kContentBrowserIconPath = "Editor/Assets/folder-open.svg";
constexpr std::string_view kRescanIconPath = "Editor/Assets/folder.svg";
constexpr std::string_view kFolderCardIconPath = "Editor/Assets/folder.svg";
constexpr std::string_view kPlaceIconPath = "Editor/Assets/sphere.svg";
constexpr std::string_view kSaveIconPath = "Editor/Assets/cylinder.svg";
constexpr int kDefaultSvgRasterSize = 256;
constexpr float kEditorIconScale = 2.0f;
constexpr SnAPI::UI::Color kIconWhite = SnAPI::UI::Color::RGB(255, 255, 255);
constexpr SnAPI::UI::Color kIconPlayGreen = SnAPI::UI::Color::RGB(73, 199, 112);

struct ToolbarActionSpec
{
    EditorLayout::EToolbarAction Action = EditorLayout::EToolbarAction::Play;
    std::string_view IconPath;
    SnAPI::UI::Color Tint = kIconWhite;
};

constexpr std::array<std::string_view, 6> kMenuItems{
    "File", "Edit", "Assets", "Tools", "Window", "Help"};
constexpr std::array<ToolbarActionSpec, 4> kToolbarActions{{
    {EditorLayout::EToolbarAction::Play, "Editor/Assets/play.svg", kIconPlayGreen},
    {EditorLayout::EToolbarAction::Pause, "Editor/Assets/pause.svg", kIconWhite},
    {EditorLayout::EToolbarAction::Stop, "Editor/Assets/stop.svg", kIconWhite},
    {EditorLayout::EToolbarAction::JoinLocalPlayer2, "Editor/Assets/world.svg", SnAPI::UI::Color::RGB(112, 169, 255)},
}};
constexpr std::array<std::string_view, 3> kViewportModes{
    "Perspective", "Lit", "Shaded"};

constexpr float kMainAreaSplitRatio = 0.68f;
constexpr float kWorkspaceLeftSplitRatio = 0.23f;
constexpr float kWorkspaceCenterSplitRatio = 0.74f;
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
constexpr std::string_view kContextMenuItemContentInspectorSelectId = "asset_inspector.select";
constexpr std::string_view kContextMenuItemContentInspectorDeleteNodeId = "asset_inspector.delete_node";
constexpr std::string_view kContextMenuItemContentInspectorDeleteComponentId = "asset_inspector.delete_component";
constexpr std::string_view kContextMenuItemContentInspectorAddNodeTypePrefix = "asset_inspector.add_node.type.";
constexpr std::string_view kContextMenuItemContentInspectorAddComponentTypePrefix = "asset_inspector.add_component.type.";

void ApplyHierarchyRowIcon(SnAPI::UI::UIImage& Icon,
                           const std::string& Source,
                           const SnAPI::UI::Color Tint)
{
    if (Icon.Source().Get() != Source)
    {
        Icon.Source().Set(Source);
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

void ConfigureSvgIcon(SnAPI::UI::UIImage& Image,
                      const float SizePx,
                      const SnAPI::UI::Color Tint,
                      const SnAPI::UI::Margin Margin = {})
{
    const float ScaledSizePx = SizePx * kEditorIconScale;
    Image.SetSvgRasterSize(kDefaultSvgRasterSize, kDefaultSvgRasterSize, true)
         .ClearSvgColorReplacements()
         .ClearSvgGlobalFill()
         .ClearSvgGlobalStroke()
         .ClearSvgGlobalStrokeWidth()
         .ReplaceSvgColor(SnAPI::UI::Color::RGB(0, 0, 0), Tint, 40);
    Image.Width().Set(SnAPI::UI::Sizing::Fixed(ScaledSizePx));
    Image.Height().Set(SnAPI::UI::Sizing::Fixed(ScaledSizePx));
    Image.Mode().Set(SnAPI::UI::EImageMode::Aspect);
    Image.LazyLoad().Set(true);
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

    if (Image.Source().Get() != kFolderCardIconPath)
    {
        Image.Source().Set(std::string(kFolderCardIconPath));
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
    std::transform(Out.begin(), Out.end(), Out.begin(), [](const unsigned char Ch) {
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

[[nodiscard]] std::vector<CreateNodeTypeEntry> BuildCreateNodeTypeEntries(const std::string& FilterLower)
{
    (void)TypeAutoRegistry::Instance().EnsureAll();

    const TypeId BaseNodeType = StaticTypeId<BaseNode>();
    const TypeInfo* BaseNodeInfo = TypeRegistry::Instance().Find(BaseNodeType);
    if (!BaseNodeInfo)
    {
        return {};
    }

    std::vector<const TypeInfo*> CandidateTypes = TypeRegistry::Instance().Derived(BaseNodeType);
    const bool HasBaseNode = std::ranges::any_of(CandidateTypes, [BaseNodeInfo](const TypeInfo* Type) {
        return Type && Type->Id == BaseNodeInfo->Id;
    });
    if (!HasBaseNode)
    {
        CandidateTypes.push_back(BaseNodeInfo);
    }

    CandidateTypes.erase(
        std::remove_if(CandidateTypes.begin(), CandidateTypes.end(), [](const TypeInfo* Type) {
            return !Type || !HasDefaultConstructor(*Type) || !TypeRegistry::Instance().IsA(Type->Id, StaticTypeId<BaseNode>());
        }),
        CandidateTypes.end());

    std::unordered_map<TypeId, const TypeInfo*, UuidHash> TypeById{};
    TypeById.reserve(CandidateTypes.size());
    for (const TypeInfo* Type : CandidateTypes)
    {
        TypeById[Type->Id] = Type;
    }

    std::unordered_map<TypeId, std::vector<const TypeInfo*>, UuidHash> ChildrenByType{};
    ChildrenByType.reserve(TypeById.size());
    for (const TypeInfo* Type : CandidateTypes)
    {
        if (!Type || Type->Id == BaseNodeType)
        {
            continue;
        }

        TypeId ParentType = BaseNodeType;
        for (const TypeId& BaseType : Type->BaseTypes)
        {
            if (TypeById.contains(BaseType) && TypeRegistry::Instance().IsA(BaseType, BaseNodeType))
            {
                ParentType = BaseType;
                break;
            }
        }

        ChildrenByType[ParentType].push_back(Type);
    }

    for (auto& [ParentType, Children] : ChildrenByType)
    {
        (void)ParentType;
        std::sort(Children.begin(), Children.end(), [](const TypeInfo* Left, const TypeInfo* Right) {
            if (!Left || !Right)
            {
                return Left < Right;
            }
            const std::string LeftLabel = ShortTypeLabel(Left->Name);
            const std::string RightLabel = ShortTypeLabel(Right->Name);
            return LeftLabel < RightLabel;
        });
    }

    std::unordered_map<TypeId, bool, UuidHash> VisibleCache{};
    const std::function<bool(const TypeInfo*)> IsVisible = [&](const TypeInfo* Type) -> bool {
        if (!Type)
        {
            return false;
        }

        if (const auto Cached = VisibleCache.find(Type->Id); Cached != VisibleCache.end())
        {
            return Cached->second;
        }

        const bool LabelMatch = FilterLower.empty() ||
                                LabelMatchesFilter(ShortTypeLabel(Type->Name), FilterLower) ||
                                LabelMatchesFilter(Type->Name, FilterLower);

        bool ChildVisible = false;
        if (const auto ChildrenIt = ChildrenByType.find(Type->Id); ChildrenIt != ChildrenByType.end())
        {
            for (const TypeInfo* Child : ChildrenIt->second)
            {
                if (IsVisible(Child))
                {
                    ChildVisible = true;
                    break;
                }
            }
        }

        const bool Visible = LabelMatch || ChildVisible;
        VisibleCache[Type->Id] = Visible;
        return Visible;
    };

    std::vector<CreateNodeTypeEntry> Entries{};
    const std::function<void(const TypeInfo*, int)> Append = [&](const TypeInfo* Type, const int Depth) {
        if (!Type || !IsVisible(Type))
        {
            return;
        }

        std::vector<const TypeInfo*> VisibleChildren{};
        if (const auto ChildrenIt = ChildrenByType.find(Type->Id); ChildrenIt != ChildrenByType.end())
        {
            for (const TypeInfo* Child : ChildrenIt->second)
            {
                if (IsVisible(Child))
                {
                    VisibleChildren.push_back(Child);
                }
            }
        }

        Entries.push_back(CreateNodeTypeEntry{
            .Type = Type->Id,
            .Label = ShortTypeLabel(Type->Name),
            .QualifiedName = Type->Name,
            .Depth = Depth,
            .HasChildren = !VisibleChildren.empty(),
        });

        for (const TypeInfo* Child : VisibleChildren)
        {
            Append(Child, Depth + 1);
        }
    };

    Append(BaseNodeInfo, 0);
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
} // namespace

Result EditorLayout::Build(GameRuntime& Runtime,
                           SnAPI::UI::Theme& Theme,
                           CameraComponent* ActiveCamera,
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
    BindInspectorTarget(ResolveSelectedNode(Runtime, ActiveCamera), ActiveCamera);
    SyncGameViewportCamera(Runtime, ActiveCamera);

    m_built = true;
    return Ok();
#endif
}

void EditorLayout::Shutdown(GameRuntime* Runtime)
{
    (void)Runtime;
    CloseContextMenu();
    m_context = nullptr;
    m_runtime = nullptr;
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
    m_contentInspectorModalOverlay = {};
    m_contentInspectorTitleText = {};
    m_contentInspectorStatusText = {};
    m_contentInspectorHierarchyTree = {};
    m_contentInspectorPropertyPanel = {};
    m_contentInspectorSaveButton = {};
    m_contentAssetCards.clear();
    m_contentAssetCardButtons.clear();
    m_contentAssetCardIndices.clear();
    m_contentBrowserEntries.clear();
    m_contentAssets.clear();
    m_contentAssetFilterText.clear();
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
    m_contentAssetInspectorState = {};
    m_contentInspectorVisibleNodes.clear();
    m_contentInspectorHierarchySource.reset();
    m_contentInspectorTargetBound = false;
    m_contentInspectorBoundObject = nullptr;
    m_contentInspectorBoundType = {};
    m_contentAssetDetails = {};
    m_onContentAssetSelected = {};
    m_onContentAssetPlaceRequested = {};
    m_onContentAssetSaveRequested = {};
    m_onContentAssetDeleteRequested = {};
    m_onContentAssetRenameRequested = {};
    m_onContentAssetRefreshRequested = {};
    m_onContentAssetCreateRequested = {};
    m_onContentAssetInspectorSaveRequested = {};
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
    m_boundInspectorObject = nullptr;
    m_boundInspectorType = {};
    m_boundInspectorComponentSignature = 0;
    m_invalidationDebugOverlayEnabled = false;
    m_viewModel = SnAPI::UI::PropertyMap{};
    m_built = false;
}

void EditorLayout::Sync(GameRuntime& Runtime,
                        CameraComponent* ActiveCamera,
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
    BindInspectorTarget(ResolveSelectedNode(Runtime, ActiveCamera), ActiveCamera);
    SyncGameViewportCamera(Runtime, ActiveCamera);
    RefreshContentAssetInspectorModalState();
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
    return static_cast<bool>(RegisterViewport) && static_cast<bool>(RegisterPropertyPanel);
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
                              CameraComponent* ActiveCamera,
                              EditorSelectionModel* SelectionModel)
{
    auto Root = Context.Root();
    ConfigureRoot(Context);
    m_selection = SelectionModel;

    BuildMenuBar(Root);
    BuildToolbar(Root);

    auto MainAreaSplit = Root.Add(SnAPI::UI::UIDockZone{});
    auto& MainAreaSplitElement = MainAreaSplit.Element();
    ConfigureSplitZone(MainAreaSplitElement, SnAPI::UI::EDockSplit::Vertical, kMainAreaSplitRatio, 220.0f, 140.0f);

    auto WorkspaceHost = MainAreaSplit.Add(SnAPI::UI::UIPanel("Editor.WorkspaceHost"));
    ConfigureHostPanel(WorkspaceHost.Element());
    BuildWorkspace(WorkspaceHost, Runtime, ActiveCamera, SelectionModel);

    auto BrowserHost = MainAreaSplit.Add(SnAPI::UI::UIPanel("Editor.ContentBrowserHost"));
    ConfigureHostPanel(BrowserHost.Element());
    BuildContentBrowser(BrowserHost);
    BuildContextMenuOverlay(Root);
    BuildCreateAssetModalOverlay(Root);
    BuildAssetInspectorModalOverlay(Root);
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

    auto BrandIcon = MenuBar.Add(SnAPI::UI::UIImage(kBrandIconPath));
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
        auto Item = MenuBar.Add(SnAPI::UI::UIText(kMenuItems[Index]));
        auto& ItemText = Item.Element();
        ItemText.ElementStyle().Apply("editor.menu_item");
        ItemText.TextColor().Set(SnAPI::UI::Color{224, 228, 235, 255});
        ItemText.Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
        ItemText.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 10.0f, 0.0f});
    }

    auto Spacer = MenuBar.Add(SnAPI::UI::UIPanel("Editor.MenuSpacer"));
    auto& SpacerPanel = Spacer.Element();
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    SpacerPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    SpacerPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});

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

        auto Icon = Button.Add(SnAPI::UI::UIImage(kToolbarActions[Index].IconPath));
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
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    SpacerPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    SpacerPanel.Background().Set(SnAPI::UI::Color{0, 0, 0, 0});

    auto ModeBreadcrumbs = Toolbar.Add(SnAPI::UI::UIBreadcrumbs{});
    auto& ModeBreadcrumbsElement = ModeBreadcrumbs.Element();
    ModeBreadcrumbsElement.ElementStyle().Apply("editor.modes_breadcrumb");
    ModeBreadcrumbsElement.SetCrumbs({std::string(kViewportModes[0]), std::string(kViewportModes[1])});
}

void EditorLayout::BuildWorkspace(PanelBuilder& Root,
                                  GameRuntime& Runtime,
                                  CameraComponent* ActiveCamera,
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
    BuildInspectorPane(InspectorHost, ResolveSelectedNode(Runtime, ActiveCamera), ActiveCamera);
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

    auto BrowserIcon = HeaderRow.Add(SnAPI::UI::UIImage(kContentBrowserIconPath));
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

    auto RefreshIcon = RefreshContent.Add(SnAPI::UI::UIImage(kRescanIconPath));
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

    auto EmptyHint = AssetsTab.Add(SnAPI::UI::UIText("No assets discovered. Click Rescan to search for .snpak packs."));
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

void EditorLayout::BuildContextMenuOverlay(PanelBuilder& Root)
{
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

void EditorLayout::BuildCreateAssetModalOverlay(PanelBuilder& Root)
{
    m_contentCreateModalOverlay = {};
    m_contentCreateTypeTree = {};
    m_contentCreateSearchInput = {};
    m_contentCreateNameInput = {};
    m_contentCreateOkButton = {};
    m_contentCreateVisibleTypes.clear();
    m_contentCreateSelectedType = {};
    m_contentCreateModalOpen = false;
    if (!m_contentCreateTypeSource)
    {
        m_contentCreateTypeSource = std::make_shared<VectorTreeItemSource>();
    }

    auto Overlay = Root.Add(SnAPI::UI::UIModal{});
    auto& OverlayPanel = Overlay.Element();
    OverlayPanel.IsOpen().Set(false);
    OverlayPanel.CloseOnBackdropClick().Set(false);
    OverlayPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    OverlayPanel.Movable().Set(true);
    OverlayPanel.Resizable().Set(true);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.ResizeBorderThickness().Set(12.0f);
    OverlayPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::Collapsed);
    OverlayPanel.BackdropColor().Set(SnAPI::UI::Color::RGBA(6, 8, 12, 218));
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 22, 30, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(87, 97, 112, 245));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(8.0f);
    OverlayPanel.ContentPadding().Set(10.0f);
    OverlayPanel.DialogMaxWidthRatio().Set(0.76f);
    OverlayPanel.DialogMaxHeightRatio().Set(0.84f);
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

    auto Subtitle = Modal.Add(SnAPI::UI::UIText("Select a BaseNode-derived class, set the asset name, then click Create."));
    auto& SubtitleText = Subtitle.Element();
    SubtitleText.ElementStyle().Apply("editor.panel_subtitle");
    SubtitleText.Wrapping().Set(SnAPI::UI::ETextWrapping::Wrap);

    auto Search = Modal.Add(SnAPI::UI::UITextInput{});
    auto& SearchInput = Search.Element();
    SearchInput.ElementStyle().Apply("editor.search");
    SearchInput.Width().Set(SnAPI::UI::Sizing::Fill());
    SearchInput.Resizable().Set(false);
    SearchInput.Placeholder().Set(std::string("Filter classes..."));
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
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    SpacerPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    SpacerPanel.Background().Set(SnAPI::UI::Color::Transparent());

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

    RebuildContentAssetCreateTypeTree();
    RefreshContentAssetCreateOkButtonState();
    RefreshContentAssetCreateModalVisibility();
}

void EditorLayout::BuildAssetInspectorModalOverlay(PanelBuilder& Root)
{
    m_contentInspectorModalOverlay = {};
    m_contentInspectorTitleText = {};
    m_contentInspectorStatusText = {};
    m_contentInspectorHierarchyTree = {};
    m_contentInspectorPropertyPanel = {};
    m_contentInspectorSaveButton = {};
    m_contentInspectorVisibleNodes.clear();
    m_contentInspectorTargetBound = false;
    m_contentInspectorBoundObject = nullptr;
    m_contentInspectorBoundType = {};
    if (!m_contentInspectorHierarchySource)
    {
        m_contentInspectorHierarchySource = std::make_shared<VectorTreeItemSource>();
    }

    auto Overlay = Root.Add(SnAPI::UI::UIModal{});
    auto& OverlayPanel = Overlay.Element();
    OverlayPanel.IsOpen().Set(false);
    OverlayPanel.CloseOnBackdropClick().Set(false);
    OverlayPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    OverlayPanel.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    OverlayPanel.Movable().Set(true);
    OverlayPanel.Resizable().Set(true);
    OverlayPanel.DragRegionHeight().Set(30.0f);
    OverlayPanel.ResizeBorderThickness().Set(12.0f);
    OverlayPanel.DialogWidth().Set(1060.0f);
    OverlayPanel.DialogHeight().Set(700.0f);
    OverlayPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::Collapsed);
    OverlayPanel.BackdropColor().Set(SnAPI::UI::Color::RGBA(7, 10, 15, 214));
    OverlayPanel.ContentBackgroundColor().Set(SnAPI::UI::Color::RGBA(18, 23, 32, 252));
    OverlayPanel.ContentBorderColor().Set(SnAPI::UI::Color::RGBA(84, 97, 117, 242));
    OverlayPanel.ContentBorderThickness().Set(1.0f);
    OverlayPanel.ContentCornerRadius().Set(8.0f);
    OverlayPanel.ContentPadding().Set(10.0f);
    OverlayPanel.DialogMaxWidthRatio().Set(0.92f);
    OverlayPanel.DialogMaxHeightRatio().Set(0.92f);
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

    auto PropertyPanelBuilder = InspectorHost.Add(UIPropertyPanel{});
    auto& PropertyPanel = PropertyPanelBuilder.Element();
    PropertyPanel.ElementStyle().Apply("editor.inspector_properties");
    PropertyPanel.Width().Set(SnAPI::UI::Sizing::Fill());
    PropertyPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    PropertyPanel.SetComponentContextMenuHandler(
        SnAPI::UI::TDelegate<void(NodeHandle, const TypeId&, const SnAPI::UI::PointerEvent&)>::Bind(
            [this](const NodeHandle OwnerNode, const TypeId& ComponentType, const SnAPI::UI::PointerEvent& Event) {
                OpenContentAssetInspectorComponentContextMenu(OwnerNode, ComponentType, Event);
            }));
    m_contentInspectorPropertyPanel = PropertyPanelBuilder.Handle();

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
    SpacerPanel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    SpacerPanel.Height().Set(SnAPI::UI::Sizing::Auto());
    SpacerPanel.Background().Set(SnAPI::UI::Color::Transparent());

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

    RebuildContentAssetInspectorHierarchyTree();
    RefreshContentAssetInspectorModalState();
    RefreshContentAssetInspectorModalVisibility();
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

    auto PlaceIcon = PlaceContent.Add(SnAPI::UI::UIImage(kPlaceIconPath));
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

    auto SaveIcon = SaveContent.Add(SnAPI::UI::UIImage(kSaveIconPath));
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
                                      CameraComponent* ActiveCamera,
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

    auto TitleIcon = TitleRow.Add(SnAPI::UI::UIImage(kHierarchyIconPath));
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

    auto SearchIcon = SearchRow.Add(SnAPI::UI::UIImage(kSearchIconPath));
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
    EnsureDefaultSelection(ActiveCamera);
    SyncHierarchy(Runtime, ActiveCamera);

    if (m_selection)
    {
        std::size_t NodeCount = 1; // Include synthetic World root row.
        if (auto* WorldPtr = Runtime.WorldPtr())
        {
            WorldPtr->NodePool().ForEach([&](const NodeHandle&, BaseNode& Node) {
                if (!Node.EditorTransient())
                {
                    ++NodeCount;
                }
            });
        }
        ViewModelProperty<std::string>(kVmHierarchyCountTextKey).Set(std::to_string(NodeCount));
    }
}

void EditorLayout::EnsureDefaultSelection(CameraComponent* ActiveCamera)
{
    if (!m_selection || !m_selection->SelectedNode().IsNull())
    {
        return;
    }

    if (!ActiveCamera || ActiveCamera->Owner().IsNull())
    {
        return;
    }

    (void)m_selection->SelectNode(ActiveCamera->Owner());
}

void EditorLayout::SyncHierarchy(GameRuntime& Runtime, CameraComponent* ActiveCamera)
{
    if (!m_context || m_hierarchyTree.Id.Value == 0)
    {
        return;
    }

    EnsureDefaultSelection(ActiveCamera);

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
        WorldContext.NodePool().ForEach([Depth, &OutNodes](const NodeHandle& Handle, BaseNode& Node) {
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
        const TraversalNode Current = Stack.back();
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

        for (const NodeHandle ChildHandle : Node->Children())
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
            IconSource = std::string(kHierarchyWorldIconPath);
            break;
        case EHierarchyEntryKind::Level:
            IconSource = std::string(kHierarchyLevelIconPath);
            break;
        case EHierarchyEntryKind::Node:
        default:
            IconSource = std::string(kHierarchyNodeIconPath);
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

void EditorLayout::SetContentAssetInspectorSaveHandler(SnAPI::UI::TDelegate<void()> Handler)
{
    m_onContentAssetInspectorSaveRequested = std::move(Handler);
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
        (m_contentAssetInspectorState.SelectedNode != State.SelectedNode) ||
        (m_contentAssetInspectorState.CanEditHierarchy != State.CanEditHierarchy) ||
        (m_contentAssetInspectorState.IsDirty != State.IsDirty) ||
        (m_contentAssetInspectorState.CanSave != State.CanSave) ||
        NodesChanged;

    m_contentAssetInspectorState = std::move(State);
    if (!m_contentAssetInspectorState.Open)
    {
        m_contentAssetInspectorState.TargetObject = nullptr;
        m_contentAssetInspectorState.TargetType = {};
        m_contentAssetInspectorState.SelectedNode = {};
        m_contentAssetInspectorState.Nodes.clear();
    }

    RebuildContentAssetInspectorHierarchyTree();
    RefreshContentAssetInspectorModalState();
    RefreshContentAssetInspectorModalVisibility();
    if (m_context && Changed)
    {
        m_context->MarkLayoutDirty();
    }
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
    if (!m_context || m_contextMenu.Id.Value == 0 || Items.empty())
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
    if (m_context && m_contextMenu.Id.Value != 0)
    {
        if (auto* Menu = dynamic_cast<SnAPI::UI::UIContextMenu*>(&m_context->GetElement(m_contextMenu.Id)))
        {
            if (Menu->IsOpen().Get())
            {
                Menu->Close(false);
            }
        }

        if (m_context->GetCapture() == m_contextMenu.Id)
        {
            m_context->ReleaseCapture();
        }
    }

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
        CardButtonElement.ElementPadding().Set(SnAPI::UI::Padding{0.0f, 0.0f, 0.0f, 0.0f});
        CardButtonElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
        CardButtonElement.OnClick([this, CardIndex]() { HandleContentAssetCardClicked(CardIndex); });
        CardButtonElement.OnContextMenuRequested(
            SnAPI::UI::TDelegate<void(const SnAPI::UI::PointerEvent&)>::Bind(
                [this, CardIndex](const SnAPI::UI::PointerEvent& Event) {
                    OpenContentAssetContextMenu(CardIndex, Event);
                }));

        auto Card = CardButton.Add(SnAPI::UI::UIPanel("Editor.AssetCard"));
        auto& CardPanel = Card.Element();
        CardPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        CardPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        CardPanel.Height().Set(SnAPI::UI::Sizing::Fill());
        CardPanel.Padding().Set(6.0f);
        CardPanel.Gap().Set(4.0f);
        CardPanel.UseGradient().Set(false);
        CardPanel.Background().Set(SnAPI::UI::Color::Transparent());
        CardPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
        CardPanel.BorderThickness().Set(0.0f);
        CardPanel.DropShadowColor().Set(SnAPI::UI::Color::Transparent());
        CardPanel.DropShadowBlur().Set(0.0f);
        CardPanel.DropShadowSpread().Set(0.0f);
        CardPanel.DropShadowOffsetX().Set(0.0f);
        CardPanel.DropShadowOffsetY().Set(0.0f);
        CardPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

        auto Preview = Card.Add(SnAPI::UI::UIPanel("Editor.AssetPreview"));
        auto& PreviewPanel = Preview.Element();
        PreviewPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        PreviewPanel.Width().Set(SnAPI::UI::Sizing::Fill());
        PreviewPanel.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        PreviewPanel.Padding().Set(4.0f);
        PreviewPanel.Gap().Set(4.0f);
        PreviewPanel.UseGradient().Set(false);
        PreviewPanel.Background().Set(SnAPI::UI::Color::Transparent());
        PreviewPanel.BorderColor().Set(SnAPI::UI::Color::Transparent());
        PreviewPanel.BorderThickness().Set(0.0f);
        PreviewPanel.DropShadowColor().Set(SnAPI::UI::Color::Transparent());
        PreviewPanel.DropShadowBlur().Set(0.0f);
        PreviewPanel.DropShadowSpread().Set(0.0f);
        PreviewPanel.DropShadowOffsetX().Set(0.0f);
        PreviewPanel.DropShadowOffsetY().Set(0.0f);
        PreviewPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

        auto FolderIcon = Preview.Add(SnAPI::UI::UIImage{});
        auto& FolderIconImage = FolderIcon.Element();
        FolderIconImage.Width().Set(SnAPI::UI::Sizing::Fill());
        FolderIconImage.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        FolderIconImage.Mode().Set(SnAPI::UI::EImageMode::Aspect);
        FolderIconImage.LazyLoad().Set(true);
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
            CardIcon->Visibility().Set(SnAPI::UI::EVisibility::Collapsed);
        }
        Button->Visibility().Set(SnAPI::UI::EVisibility::Visible);
    }

    if (m_contentAssetsEmptyHint.Id.Value != 0)
    {
        if (auto* EmptyHint = dynamic_cast<SnAPI::UI::UIText*>(&m_context->GetElement(m_contentAssetsEmptyHint.Id)))
        {
            if (m_contentAssets.empty())
            {
                EmptyHint->Text().Set("No assets discovered. Click Rescan to search for .snpak packs.");
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
    RebuildContentAssetCreateTypeTree();
    RefreshContentAssetCreateOkButtonState();
    RefreshContentAssetCreateModalVisibility();
    m_context->MarkLayoutDirty();
}

void EditorLayout::CloseContentAssetCreateModal()
{
    if (!m_contentCreateModalOpen)
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
    if (!m_context || m_contentCreateModalOverlay.Id.Value == 0)
    {
        return;
    }

    if (auto* Overlay = dynamic_cast<SnAPI::UI::UIModal*>(&m_context->GetElement(m_contentCreateModalOverlay.Id)))
    {
        Overlay->IsOpen().Set(m_contentCreateModalOpen);
        Overlay->Properties().SetProperty(
            SnAPI::UI::UIElementBase::VisibilityKey,
            m_contentCreateModalOpen ? SnAPI::UI::EVisibility::Visible : SnAPI::UI::EVisibility::Collapsed);
    }

    if (!m_contentCreateModalOpen)
    {
        const SnAPI::UI::ElementId CapturedElement = m_context->GetCapture();
        if (IsElementWithinSubtree(*m_context, CapturedElement, m_contentCreateModalOverlay.Id))
        {
            m_context->ReleaseCapture();
        }
    }
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
            .IconSource = std::string(kHierarchyNodeIconPath),
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

void EditorLayout::CloseContentAssetInspectorModal(const bool NotifyHandler)
{
    const bool WasOpen = m_contentAssetInspectorState.Open;
    m_contentAssetInspectorState.Open = false;
    m_contentAssetInspectorState.TargetObject = nullptr;
    m_contentAssetInspectorState.TargetType = {};
    m_contentAssetInspectorState.SelectedNode = {};
    m_contentAssetInspectorState.Nodes.clear();
    m_contentAssetInspectorState.CanEditHierarchy = false;
    m_contentAssetInspectorState.IsDirty = false;
    m_contentAssetInspectorState.CanSave = false;

    RebuildContentAssetInspectorHierarchyTree();
    RefreshContentAssetInspectorModalState();
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
    if (!m_context || m_contentInspectorModalOverlay.Id.Value == 0)
    {
        return;
    }

    if (auto* Overlay = dynamic_cast<SnAPI::UI::UIModal*>(&m_context->GetElement(m_contentInspectorModalOverlay.Id)))
    {
        Overlay->IsOpen().Set(m_contentAssetInspectorState.Open);
        Overlay->Properties().SetProperty(
            SnAPI::UI::UIElementBase::VisibilityKey,
            m_contentAssetInspectorState.Open ? SnAPI::UI::EVisibility::Visible : SnAPI::UI::EVisibility::Collapsed);
    }

    if (!m_contentAssetInspectorState.Open)
    {
        const SnAPI::UI::ElementId CapturedElement = m_context->GetCapture();
        if (IsElementWithinSubtree(*m_context, CapturedElement, m_contentInspectorModalOverlay.Id))
        {
            m_context->ReleaseCapture();
        }
    }
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
                .IconSource = std::string(kHierarchyNodeIconPath),
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

    bool HasTarget = m_contentAssetInspectorState.TargetObject != nullptr &&
                     m_contentAssetInspectorState.TargetType != TypeId{};
    void* BindingTargetObject = m_contentAssetInspectorState.TargetObject;
    TypeId BindingTargetType = m_contentAssetInspectorState.TargetType;
    BaseNode* SelectedHierarchyNode = nullptr;
    if (m_contentAssetInspectorState.CanEditHierarchy && !m_contentAssetInspectorState.SelectedNode.IsNull())
    {
        SelectedHierarchyNode = m_contentAssetInspectorState.SelectedNode.Borrowed();
        if (!SelectedHierarchyNode)
        {
            SelectedHierarchyNode = m_contentAssetInspectorState.SelectedNode.BorrowedSlowByUuid();
        }
        if (SelectedHierarchyNode)
        {
            BindingTargetObject = SelectedHierarchyNode;
            BindingTargetType = SelectedHierarchyNode->TypeKey();
            HasTarget = true;
        }
    }

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
                else if (!HasTarget)
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
                        StatusText = m_contentAssetInspectorState.IsDirty
                                         ? "Unsaved changes. Click Save to persist."
                                         : "No pending edits.";
                    }
                }
            }
            Status->Text().Set(std::move(StatusText));
        }
    }

    if (m_contentInspectorPropertyPanel.Id.Value != 0)
    {
        if (auto* PropertyPanel = dynamic_cast<UIPropertyPanel*>(&m_context->GetElement(m_contentInspectorPropertyPanel.Id)))
        {
            if (m_contentAssetInspectorState.Open && HasTarget)
            {
                if (!m_contentInspectorTargetBound ||
                    m_contentInspectorBoundObject != BindingTargetObject ||
                    m_contentInspectorBoundType != BindingTargetType)
                {
                    PropertyPanel->ClearObject();
                    if (SelectedHierarchyNode && m_contentAssetInspectorState.CanEditHierarchy)
                    {
                        m_contentInspectorTargetBound = PropertyPanel->BindNode(SelectedHierarchyNode);
                    }
                    else
                    {
                        m_contentInspectorTargetBound = PropertyPanel->BindObject(BindingTargetType, BindingTargetObject);
                    }
                    if (m_contentInspectorTargetBound)
                    {
                        m_contentInspectorBoundObject = BindingTargetObject;
                        m_contentInspectorBoundType = BindingTargetType;
                    }
                    else
                    {
                        m_contentInspectorBoundObject = nullptr;
                        m_contentInspectorBoundType = {};
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
                m_contentInspectorBoundObject = nullptr;
                m_contentInspectorBoundType = {};
            }
        }
    }

    if (m_contentInspectorSaveButton.Id.Value != 0)
    {
        if (auto* SaveButton = dynamic_cast<SnAPI::UI::UIButton*>(&m_context->GetElement(m_contentInspectorSaveButton.Id)))
        {
            const bool CanSave = m_contentAssetInspectorState.Open &&
                                 m_contentInspectorTargetBound &&
                                 m_contentAssetInspectorState.CanSave &&
                                 m_contentAssetInspectorState.IsDirty;
            SaveButton->SetDisabled(!CanSave);
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

BaseNode* EditorLayout::ResolveSelectedNode(GameRuntime& Runtime, CameraComponent* ActiveCamera) const
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

    if (ActiveCamera && !ActiveCamera->Owner().IsNull())
    {
        if (auto* CameraNode = ActiveCamera->Owner().Borrowed())
        {
            return CameraNode;
        }
        return ActiveCamera->Owner().BorrowedSlowByUuid();
    }

    return nullptr;
}

void EditorLayout::BuildGamePane(PanelBuilder& Workspace, GameRuntime& Runtime, CameraComponent* ActiveCamera)
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

    auto HeaderIcon = Header.Add(SnAPI::UI::UIImage(kGameViewIconPath));
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

    auto Viewport = GameViewTab.Add(UIRenderViewport{});
    auto& ViewportElement = Viewport.Element();
    ViewportElement.Width().Set(SnAPI::UI::Sizing::Fill());
    ViewportElement.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    ViewportElement.ElementMargin().Set(SnAPI::UI::Margin{0.0f, 0.0f, 0.0f, 0.0f});
    ViewportElement.ViewportName().Set(std::string("Editor.GameViewport"));
    ViewportElement.PassGraphPreset().Set(ERenderViewportPassGraphPreset::DefaultWorld);
    ViewportElement.AutoRegisterPassGraph().Set(true);
    ViewportElement.RenderScale().Set(1.0f);
    ViewportElement.Enabled().Set(true);
    ViewportElement.SetGameRuntime(&Runtime);
    if (ActiveCamera && ActiveCamera->Camera())
    {
        ViewportElement.SetViewportCamera(ActiveCamera->Camera());
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

    ViewTabsElement.SetTabLabel(0, "Game View");
    ViewTabsElement.SetTabLabel(1, "Profiler");

    m_gameViewport = Viewport.Handle();
}

void EditorLayout::BuildInspectorPane(PanelBuilder& Workspace, BaseNode* SelectedNode, CameraComponent* ActiveCamera)
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

    auto TitleIcon = TitleRow.Add(SnAPI::UI::UIImage(kInspectorIconPath));
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

    auto MoveSnap = SnapCard.Add(SnAPI::UI::UINumberField{});
    auto& MoveSnapField = MoveSnap.Element();
    MoveSnapField.ElementStyle().Apply("editor.number_field");
    MoveSnapField.Step().Set(0.1);
    MoveSnapField.Value().Set(1.0);
    MoveSnapField.Precision().Set(2u);
    MoveSnapField.Width().Set(SnAPI::UI::Sizing::Fill());
    MoveSnapField.Height().Set(SnAPI::UI::Sizing::Auto());
    MoveSnapField.Padding().Set(5.0f);

    auto RotateSnap = SnapCard.Add(SnAPI::UI::UINumberField{});
    auto& RotateSnapField = RotateSnap.Element();
    RotateSnapField.ElementStyle().Apply("editor.number_field");
    RotateSnapField.Step().Set(1.0);
    RotateSnapField.Value().Set(15.0);
    RotateSnapField.Precision().Set(1u);
    RotateSnapField.Width().Set(SnAPI::UI::Sizing::Fill());
    RotateSnapField.Height().Set(SnAPI::UI::Sizing::Auto());
    RotateSnapField.Padding().Set(5.0f);

    auto ScaleSnap = SnapCard.Add(SnAPI::UI::UINumberField{});
    auto& ScaleSnapField = ScaleSnap.Element();
    ScaleSnapField.ElementStyle().Apply("editor.number_field");
    ScaleSnapField.Step().Set(0.05);
    ScaleSnapField.Value().Set(0.5);
    ScaleSnapField.Precision().Set(2u);
    ScaleSnapField.Width().Set(SnAPI::UI::Sizing::Fill());
    ScaleSnapField.Height().Set(SnAPI::UI::Sizing::Auto());
    ScaleSnapField.Padding().Set(5.0f);

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
    BindInspectorTarget(SelectedNode, ActiveCamera);
}

void EditorLayout::BindInspectorTarget(BaseNode* SelectedNode, CameraComponent* ActiveCamera)
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
        if (m_boundInspectorObject != SelectedNode
            || m_boundInspectorType != SelectedNode->TypeKey()
            || m_boundInspectorComponentSignature != ComponentSignature)
        {
            PropertyPanel->ClearObject();
            if (PropertyPanel->BindNode(SelectedNode))
            {
                m_boundInspectorObject = SelectedNode;
                m_boundInspectorType = SelectedNode->TypeKey();
                m_boundInspectorComponentSignature = ComponentSignature;
            }
            else
            {
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

    if (ActiveCamera)
    {
        TargetObject = ActiveCamera;
        TargetType = StaticTypeId<CameraComponent>();
    }

    if (!TargetObject)
    {
        PropertyPanel->ClearObject();
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
        m_boundInspectorObject = TargetObject;
        m_boundInspectorType = TargetType;
        m_boundInspectorComponentSignature = 0;
    }
    else
    {
        m_boundInspectorObject = nullptr;
        m_boundInspectorType = {};
        m_boundInspectorComponentSignature = 0;
    }
}

void EditorLayout::SyncGameViewportCamera(GameRuntime& Runtime, CameraComponent* ActiveCamera)
{
    auto* Viewport = ResolveGameViewport();
    if (!Viewport)
    {
        return;
    }

    Viewport->SetGameRuntime(&Runtime);

    SnAPI::Graphics::ICamera* RenderCamera = nullptr;
    if (ActiveCamera)
    {
        RenderCamera = ActiveCamera->Camera();
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    if (!RenderCamera)
    {
        if (auto* WorldPtr = Runtime.WorldPtr())
        {
            RenderCamera = WorldPtr->Renderer().ActiveCamera();
        }
    }
#endif

    if (ActiveCamera && Viewport)
    {
        const SnAPI::UI::UIRect ViewRect = Viewport->LayoutRect();
        if (ViewRect.W > 0.0f && ViewRect.H > 0.0f)
        {
            auto& CameraSettings = ActiveCamera->EditSettings();
            CameraSettings.Aspect = ViewRect.W / ViewRect.H;
        }
    }

    Viewport->SetViewportCamera(RenderCamera);
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
