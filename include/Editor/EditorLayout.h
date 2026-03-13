#pragma once

#include "Expected.h"
#include "Conduit/Types.h"
#include "Editor/EditorImportSettings.h"
#include "Handles.h"
#include "IAssetImportSettings.h"
#include "TypeRegistration.h"

#include <UIHandles.h>
#include <UIDelegates.h>
#include <UIContextMenu.h>
#include <UIProperties.h>

#include "UIBuilder.h"

#include <cstdint>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SnAPI::UI
{
class Theme;
class UIContext;
class UIPanel;
class UIModal;
class UITabs;
class UIText;
class UITextInput;
class UIFilesystemPicker;
class UIImage;
class UIBadge;
class UIBreadcrumbs;
class UIListView;
class UISwitch;
class UITreeView;
class ITreeItemSource;
class UIContextMenu;
class UIButton;
class UIComboBox;
class UICheckbox;
template<typename TElement>
class TElementBuilder;
} // namespace SnAPI::UI

namespace SnAPI::GameFramework
{
class BaseNode;
class CameraComponent;
class GameRuntime;
class UIPropertyPanel;
class UIRenderViewport;
class World;
} // namespace SnAPI::GameFramework

namespace SnAPI::GameFramework::Conduit::Editor
{
class UIConduitGraphCanvas;
}

namespace SnAPI::GameFramework::Editor
{
class EditorSelectionModel;

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Builds and owns the editor shell widget tree inside the root UI context.
 *
 * `EditorLayout` is the concrete UI composition object for the editor shell. It creates the menu bar,
 * toolbar, hierarchy, game viewport tabs, inspector panes, content browser, and project/asset modal
 * overlays inside the root `UIContext` exposed by the running `GameRuntime`.
 *
 * Core semantics:
 * - `Build()` tears down any previous shell, registers required external elements, and rebuilds the
 *   entire widget tree for the current runtime and theme.
 * - `Sync()` is the steady-state update path. It refreshes the hierarchy, current inspector target,
 *   invalidation overlay state, and game viewport camera binding without rebuilding the shell.
 * - The class stores UI callbacks as delegates. Higher-level services provide those delegates and are
 *   responsible for translating them into editor actions.
 * - Content-browser and inspector payload setters copy lightweight view-model data but may contain
 *   borrowed raw object pointers for property panels; those pointers must remain valid until replaced
 *   by a subsequent state push or until the layout is shut down.
 *
 * Ownership and lifetime:
 * - `EditorLayout` value-owns its UI element handles and modal/view-model state.
 * - Returned pointers such as `GameViewport()` and `Context()` are non-owning and become invalid when
 *   the layout is shut down or rebuilt.
 *
 * Threading model:
 * - Main-thread only.
 */
class EditorLayout final
{
public:
    /**
     * @brief One content-browser card entry.
     *
     * This structure carries the normalized display data needed to render a single asset entry in the
     * content browser grid or list.
     */
    struct ContentAssetEntry
    {
        std::string Key{}; /**< @brief Stable asset key used to route selection, placement, save, delete, and rename callbacks. */
        std::string Name{}; /**< @brief User-facing asset display name. */
        std::string Type{}; /**< @brief Short reflected type/category label shown on the asset card. */
        std::string Variant{}; /**< @brief Additional subtype or variant label for disambiguation. */
        std::string IconSource{}; /**< @brief Logical fallback icon identifier used when no thumbnail texture is available. */
        std::uint32_t IconTextureId = 0; /**< @brief External UI texture id for thumbnail rendering, or `0` when no texture preview exists. */
        std::uint32_t IconWidth = 0; /**< @brief Thumbnail width in pixels. */
        std::uint32_t IconHeight = 0; /**< @brief Thumbnail height in pixels. */
        bool IsRuntime = false; /**< @brief `true` when the asset currently lives only in runtime/editor memory rather than persisted project content. */
        bool IsDirty = false; /**< @brief `true` when the asset has unsaved runtime or import-setting changes. */
    };

    /**
     * @brief Detail-pane payload for the currently selected content asset.
     */
    struct ContentAssetDetails
    {
        std::string Name{}; /**< @brief User-facing asset name shown in the details pane. */
        std::string Type{}; /**< @brief Reflected type/category label. */
        std::string Variant{}; /**< @brief Variant or subtype label. */
        std::string AssetId{}; /**< @brief Stable serialized asset identifier, if the asset has one. */
        std::string Status{}; /**< @brief Human-readable status text such as load/save/import state. */
        bool IsRuntime = false; /**< @brief `true` when the asset is runtime-only and not yet persisted into project content. */
        bool IsDirty = false; /**< @brief `true` when the selected asset has unsaved changes. */
        bool CanPlace = true; /**< @brief `true` when the asset may be instantiated or placed into the world from the browser. */
        bool CanSave = true; /**< @brief `true` when a save action should be enabled for the selected asset. */
    };

    /**
     * @brief Request payload emitted when the create-asset modal is confirmed.
     */
    struct ContentAssetCreateRequest
    {
        TypeId Type{}; /**< @brief Reflected asset type selected in the create modal. */
        std::string Name{}; /**< @brief User-entered asset name after layout-side trimming and validation. */
        std::string FolderPath{}; /**< @brief Content-browser folder path that should receive the new asset. */
    };

    /**
     * @brief Request payload emitted when the import-asset modal is confirmed.
     */
    struct ContentAssetImportRequest
    {
        std::string SourcePath{}; /**< @brief Source file path chosen by the user for import. */
        std::string FolderPath{}; /**< @brief Destination content folder path inside the current project. */
        std::unordered_map<std::string, std::string> BuildOptions{}; /**< @brief Normalized string build options derived from the selected import profile. */
        ::SnAPI::AssetPipeline::AssetImportSettingsPtr ImportSettings{}; /**< @brief Owning pointer to the typed import-settings object selected in the modal. */
    };

    /**
     * @brief State payload for the asset-inspector modal.
     *
     * The inspector modal can edit a runtime asset object, optional import settings, and an optional
     * node hierarchy view for hierarchical assets. `TargetObject` and `ImportSettingsObject` are
     * borrowed raw pointers; they must remain valid until a new state payload is pushed or the modal
     * is closed.
     */
    struct ContentAssetInspectorState
    {
        /**
         * @brief One visible node entry in the inspector hierarchy tree.
         */
        struct NodeEntry
        {
            NodeHandle Handle{}; /**< @brief Stable handle for the represented node. */
            int Depth = 0; /**< @brief Tree depth used to rebuild a flat hierarchical view. */
            std::string Label{}; /**< @brief User-facing node label. */
        };

        bool Open = false; /**< @brief `true` when the asset-inspector modal should be visible. */
        std::string AssetKey{}; /**< @brief Stable asset key of the asset being inspected. */
        std::string Title{}; /**< @brief Modal title text. */
        std::string Status{}; /**< @brief Human-readable status line for save/import/runtime state. */
        TypeId TargetType{}; /**< @brief Reflected type of `TargetObject`. */
        void* TargetObject = nullptr; /**< @brief Borrowed pointer to the runtime asset object currently bound into the property panel. */
        TypeId ImportSettingsType{}; /**< @brief Reflected type of `ImportSettingsObject`, if any. */
        void* ImportSettingsObject = nullptr; /**< @brief Borrowed pointer to the editable import-settings object, if any. */
        std::vector<NodeEntry> Nodes{}; /**< @brief Flattened hierarchy view shown for hierarchical assets. */
        NodeHandle SelectedNode{}; /**< @brief Currently selected hierarchy node within the inspected asset. */
        bool CanEditHierarchy = false; /**< @brief `true` when hierarchy add/remove actions should be enabled. */
        bool HasImportSettings = false; /**< @brief `true` when the asset exposes import settings for editing. */
        bool RuntimeDirty = false; /**< @brief `true` when the runtime asset object has unsaved edits. */
        bool ImportSettingsDirty = false; /**< @brief `true` when import settings have unsaved edits. */
        bool IsDirty = false; /**< @brief Aggregate dirty flag used by the modal save affordances. */
        bool CanSave = false; /**< @brief `true` when the save button should be enabled. */
        bool CanReimport = false; /**< @brief `true` when the reimport action is available. */
        std::string PreviewIconSource{}; /**< @brief Logical fallback icon for the preview panel. */
        std::uint32_t PreviewTextureId = 0; /**< @brief External UI texture id for the preview image, or `0` when no preview texture exists. */
        std::uint32_t PreviewWidth = 0; /**< @brief Preview texture width in pixels. */
        std::uint32_t PreviewHeight = 0; /**< @brief Preview texture height in pixels. */
        std::string PreviewStatsPrimary{}; /**< @brief Primary preview statistics line. */
        std::string PreviewStatsSecondary{}; /**< @brief Secondary preview statistics line. */
        std::uint64_t SessionRevision = 0; /**< @brief Revision token used to avoid rebinding unchanged asset-editor sessions. */
    };

    /**
     * @brief Summary payload for the docked Conduit workspace surface.
     *
     * This is intentionally lightweight in the first slice. It gives the shell enough
     * information to render a document-style Conduit tab without coupling the shell to
     * full graph-canvas implementation details.
     */
    struct ConduitWorkspaceState
    {
        enum class EDocumentKind : std::uint8_t
        {
            None = 0,
            Graph,
            Class,
        };

        enum class EVariableDefaultEditorKind : std::uint8_t
        {
            None = 0,
            Bool,
            Text,
            Enum,
            Complex,
        };

        struct VariableEntry
        {
            Uuid Id{};
            std::string Name{};
            std::string TypeLabel{};
            bool HasDefault = false;
            bool Selected = false;
        };

        struct VariableTypeOption
        {
            TypeId Type{};
            std::string Label{};
        };

        struct ClassHostTypeOption
        {
            TypeId Type{};
            std::string Label{};
        };

        struct ClassGraphOption
        {
            std::string AssetKey{};
            std::string Label{};
        };

        struct PaletteEntry
        {
            std::string StableId{};
            std::string DisplayName{};
            std::string Category{};
            std::string Tooltip{};
            bool RequiresSpecialization = false;
        };

        struct NodeEntry
        {
            Uuid Id{};
            std::string Title{};
            std::string Detail{};
            bool Selected = false;
        };

        struct CanvasNode
        {
            Uuid Id{};
            std::string Title{};
            std::string Detail{};
            float X = 0.0f;
            float Y = 0.0f;
            float Width = 240.0f;
            bool IsCollapsed = false;
            bool Selected = false;
            struct Pin
            {
                std::string Name{};
                std::string TypeLabel{};
                SnAPI::GameFramework::Conduit::ESlotKind Kind = SnAPI::GameFramework::Conduit::ESlotKind::Value;
                bool IsInput = true;
                bool IsExec = false;
            };
            std::vector<Pin> InputPins{};
            std::vector<Pin> OutputPins{};
        };

        struct CanvasComment
        {
            Uuid Id{};
            std::string Title{};
            float X = 0.0f;
            float Y = 0.0f;
            float Width = 480.0f;
            float Height = 320.0f;
            std::uint32_t ColorRgba = 0x334455FFu;
            bool Selected = false;
        };

        struct CanvasWire
        {
            Uuid SourceNodeId{};
            std::string SourcePin{};
            Uuid TargetNodeId{};
            std::string TargetPin{};
            SnAPI::GameFramework::Conduit::ESlotKind Kind = SnAPI::GameFramework::Conduit::ESlotKind::Value;
            bool IsExec = false;
        };

        struct VariableInspector
        {
            bool HasSelection = false;
            Uuid VariableId{};
            std::string Name{};
            TypeId Type{};
            std::string TypeLabel{};
            bool HasDefault = false;
            EVariableDefaultEditorKind DefaultEditorKind = EVariableDefaultEditorKind::None;
            bool BoolValue = false;
            std::string TextValue{};
            std::vector<std::string> EnumOptions{};
            int32_t SelectedEnumIndex = -1;
            void* ComplexObject = nullptr;
            TypeId ComplexType{};
        };

        struct ClassInspector
        {
            bool HasSelection = false;
            std::string Name{};
            TypeId HostType{};
            std::string HostTypeLabel{};
            std::string GraphAssetKey{};
            std::string GraphAssetLabel{};
        };

        struct NodeInspector
        {
            bool HasSelection = false;
            Uuid NodeId{};
            std::string Title{};
            std::string Detail{};
            bool CanEditPrimaryText = false;
            std::string PrimaryTextLabel{};
            std::string PrimaryTextValue{};
            bool CanEditSecondaryText = false;
            std::string SecondaryTextLabel{};
            std::string SecondaryTextValue{};
        };

        EDocumentKind Kind = EDocumentKind::None; /**< @brief Active Conduit document category. */
        bool Open = false; /**< @brief `true` when a Conduit document is currently active. */
        std::string AssetKey{}; /**< @brief Stable asset key of the active Conduit document. */
        std::string Title{}; /**< @brief UI-facing title of the active document. */
        std::string Status{}; /**< @brief Human-readable compile/save status text. */
        std::string SelfTypeLabel{}; /**< @brief Human-readable reflected self type label. */
        std::string HostTypeLabel{}; /**< @brief Human-readable reflected host type label for class documents. */
        std::string GraphAssetLabel{}; /**< @brief Human-readable referenced graph asset label for class documents. */
        std::size_t SlotCount = 0; /**< @brief Authored slot count. */
        std::size_t VariableCount = 0; /**< @brief Authored graph-variable count. */
        std::size_t NodeCount = 0; /**< @brief Authored node count. */
        bool IsDirty = false; /**< @brief `true` when the active document has unsaved edits. */
        bool CompileSucceeded = false; /**< @brief `true` when the last compile produced no errors. */
        std::size_t WarningCount = 0; /**< @brief Warning count from the last compile. */
        std::size_t ErrorCount = 0; /**< @brief Error count from the last compile. */
        std::vector<VariableEntry> VariableEntries{};
        std::vector<PaletteEntry> PaletteEntries{};
        std::vector<NodeEntry> NodeEntries{};
        std::vector<CanvasNode> CanvasNodes{};
        std::vector<CanvasComment> CanvasComments{};
        std::vector<CanvasWire> CanvasWires{};
        float CanvasPanX = 0.0f;
        float CanvasPanY = 0.0f;
        float CanvasZoom = 1.0f;
        std::vector<VariableTypeOption> VariableTypeOptions{};
        VariableInspector SelectedVariable{};
        NodeInspector SelectedNode{};
        std::vector<ClassHostTypeOption> ClassHostTypeOptions{};
        std::vector<ClassGraphOption> ClassGraphOptions{};
        ClassInspector SelectedClass{};
        std::uint64_t Revision = 0; /**< @brief Revision token used to avoid unnecessary UI updates. */
    };

    /**
     * @brief Hierarchy action kinds emitted by hierarchy context menus.
     */
    enum class EHierarchyAction : std::uint8_t
    {
        AddNodeType, /**< @brief Create a new child node of the chosen reflected type. */
        AddComponentType, /**< @brief Attach a component of the chosen reflected type to the target node. */
        RemoveComponentType, /**< @brief Remove the specified component type from the target node. */
        DeleteNode, /**< @brief Delete the target node. */
        CreatePrefab, /**< @brief Create a prefab asset from the target node subtree. */
    };

    /**
     * @brief Payload describing one hierarchy action request.
     */
    struct HierarchyActionRequest
    {
        EHierarchyAction Action = EHierarchyAction::AddNodeType; /**< @brief Requested hierarchy action. */
        NodeHandle TargetNode{}; /**< @brief Target node for the action, when applicable. */
        bool TargetIsWorldRoot = false; /**< @brief `true` when the action conceptually targets the world root rather than a concrete node. */
        TypeId Type{}; /**< @brief Reflected node or component type associated with add/remove requests. */
    };

    /**
     * @brief Toolbar actions emitted by the editor shell.
     */
    enum class EToolbarAction : std::uint8_t
    {
        Play, /**< @brief Start or resume Play-In-Editor. */
        Pause, /**< @brief Pause the active PIE session. */
        Stop, /**< @brief Stop PIE and restore the editor world snapshot. */
        JoinLocalPlayer2, /**< @brief Request that a second local player join the current session. */
    };

    /**
     * @brief Transform-gizmo space selection exposed by the tools pane.
     */
    enum class EGizmoSpace : std::uint8_t
    {
        World = 0, /**< @brief Interpret gizmo axes in world space. */
        Object, /**< @brief Interpret gizmo axes in the selected object's local basis. */
        Camera /**< @brief Interpret gizmo axes relative to the active editor camera. */
    };

    /**
     * @brief Binary snapping toggle used by the tools pane.
     */
    enum class ESnapMode : std::uint8_t
    {
        Off = 0, /**< @brief Do not quantize transform interaction deltas. */
        On /**< @brief Quantize transform interaction deltas using the configured snap steps. */
    };

    /**
     * @brief Project-management actions emitted by project modals.
     */
    enum class EProjectAction : std::uint8_t
    {
        CreateNew, /**< @brief Create a new project from the values entered in the create-project flow. */
        OpenExisting, /**< @brief Open an existing project file selected by the user. */
        SaveSettings, /**< @brief Persist project settings edits for the currently loaded project. */
    };

    /**
     * @brief Payload describing one project action request.
     */
    struct ProjectActionRequest
    {
        EProjectAction Action = EProjectAction::CreateNew; /**< @brief Requested project action. */
        std::string ProjectName{}; /**< @brief User-facing project name. */
        std::string ProjectDirectory{}; /**< @brief Directory that should contain the project when creating a new one. */
        std::string ProjectFilePath{}; /**< @brief Absolute project file path used for open/save workflows. */
        std::string StartupLevelAsset{}; /**< @brief Asset id or pack path for the project's startup level. */
        std::string DefaultRenderSettingsAssetId{}; /**< @brief Default render-settings asset id chosen in project settings. */
    };

    /**
     * @brief Current loaded-project state shown by the shell.
     */
    struct ProjectState
    {
        bool IsLoaded = false; /**< @brief `true` when an editor project is currently loaded. */
        std::string Name{}; /**< @brief Project display name. */
        std::string ProjectFilePath{}; /**< @brief Absolute path to the active project file. */
        std::string ProjectRootDirectory{}; /**< @brief Root directory that contains the project file and related metadata. */
        std::string AssetRootDirectory{}; /**< @brief Root content directory shown by the content browser. */
        std::string StartupLevelAsset{}; /**< @brief Configured startup level asset identifier or pack path. */
        std::string DefaultRenderSettingsAssetId{}; /**< @brief Configured default render-settings asset identifier. */
    };

    /**
     * @brief Build or rebuild the full editor shell for the supplied runtime.
     * @param Runtime Borrowed runtime that exposes the root UI context and world services.
     * @param Theme Borrowed theme applied to the shell.
     * @param ActiveCamera Non-owning active editor camera pointer used for initial hierarchy/inspector/game-view setup.
     * @param SelectionModel Borrowed selection model used for hierarchy and inspector binding.
     * @return Success or an error.
     *
     * `Build()` first shuts down any previously built shell, then registers external elements such as
     * `UIRenderViewport` and `UIPropertyPanel`, resolves the root UI context, and composes the full shell.
     */
    Result Build(GameRuntime& Runtime,
                 SnAPI::UI::Theme& Theme,
                 ComponentHandle ActiveCamera,
                 EditorSelectionModel* SelectionModel);
    /**
     * @brief Destroy the current shell and clear all element handles, modal state, and callback bindings.
     * @param Runtime Borrowed runtime associated with the current layout, or `nullptr` when tearing down detached state.
     */
    void Shutdown(GameRuntime* Runtime);

    /**
     * @brief Synchronize the built shell with current runtime, selection, and camera state.
     * @param Runtime Borrowed runtime.
     * @param ActiveCamera Active editor camera handle.
     * @param SelectionModel Borrowed selection model.
     * @param DeltaSeconds Frame delta time in seconds.
     *
     * This is the steady-state update path and does not rebuild the shell.
     */
    void Sync(GameRuntime& Runtime, ComponentHandle ActiveCamera, EditorSelectionModel* SelectionModel, float DeltaSeconds);
    /** @brief Query whether the shell is currently built. */
    [[nodiscard]] bool IsBuilt() const { return m_built; }
    /** @brief Access the embedded game-viewport UI element. @return Non-owning pointer or `nullptr`. */
    [[nodiscard]] UIRenderViewport* GameViewport() const;
    /** @brief Index of the active game-viewport tab within the game-view tab control, or a negative value if unavailable. */
    [[nodiscard]] int32_t GameViewportTabIndex() const;
    /** @brief Access the root UI context currently hosting the shell. @return Non-owning pointer or `nullptr`. */
    [[nodiscard]] SnAPI::UI::UIContext* Context() const { return m_context; }
    /** @brief Current gizmo-space selection from the tools UI. */
    [[nodiscard]] EGizmoSpace GizmoSpace() const { return m_gizmoSpace; }
    /** @brief Query whether transform snapping is currently enabled in the tools UI. */
    [[nodiscard]] bool GizmoSnappingEnabled() const { return m_snapMode == ESnapMode::On; }
    /** @brief Translation snap step in world units. */
    [[nodiscard]] double MoveSnapStep() const { return m_moveSnapStep; }
    /** @brief Rotation snap increment in degrees. */
    [[nodiscard]] double RotateSnapStepDegrees() const { return m_rotateSnapStepDegrees; }
    /** @brief Scale snap increment in scalar units. */
    [[nodiscard]] double ScaleSnapStep() const { return m_scaleSnapStep; }
    /** @brief Install the callback invoked when the user chooses a hierarchy node. */
    void SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)> Handler);
    /** @brief Install the callback invoked when the user requests a hierarchy mutation. */
    void SetHierarchyActionHandler(SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> Handler);
    /** @brief Install the callback invoked when the user presses a toolbar action. */
    void SetToolbarActionHandler(SnAPI::UI::TDelegate<void(EToolbarAction)> Handler);
    /** @brief Install the callback invoked for project create/open/save-settings flows. */
    void SetProjectActionHandler(SnAPI::UI::TDelegate<void(const ProjectActionRequest&)> Handler);
    /** @brief Replace the loaded-project view state shown across project-sensitive UI. */
    void SetProjectState(ProjectState State);
    /** @brief Control whether the shell should force the user through project selection before normal editing. */
    void SetProjectSelectionRequired(bool Required);
    /** @brief Replace the content-browser asset list. */
    void SetContentAssets(std::vector<ContentAssetEntry> Assets);
    /** @brief Install the callback invoked when the user selects or double-clicks a content asset. */
    void SetContentAssetSelectionHandler(SnAPI::UI::TDelegate<void(const std::string&, bool)> Handler);
    /** @brief Install the callback invoked when the user requests asset placement into the world. */
    void SetContentAssetPlaceHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    /** @brief Install the callback invoked when the user requests asset save. */
    void SetContentAssetSaveHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    /** @brief Install the callback invoked when the user requests asset deletion. */
    void SetContentAssetDeleteHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    /** @brief Install the callback invoked when the user confirms an asset rename. */
    void SetContentAssetRenameHandler(SnAPI::UI::TDelegate<void(const std::string&, const std::string&)> Handler);
    /** @brief Install the callback invoked when the user requests a content refresh. */
    void SetContentAssetRefreshHandler(SnAPI::UI::TDelegate<void()> Handler);
    /** @brief Install the callback invoked when the create-asset modal is confirmed. */
    void SetContentAssetCreateHandler(SnAPI::UI::TDelegate<void(const ContentAssetCreateRequest&)> Handler);
    /** @brief Install the callback invoked when the import-asset modal is confirmed. */
    void SetContentAssetImportHandler(SnAPI::UI::TDelegate<void(const ContentAssetImportRequest&)> Handler);
    /** @brief Install the callback invoked when the asset-inspector save action is chosen. */
    void SetContentAssetInspectorSaveHandler(SnAPI::UI::TDelegate<void()> Handler);
    /** @brief Install the callback invoked when the asset-inspector reimport action is chosen. */
    void SetContentAssetInspectorReimportHandler(SnAPI::UI::TDelegate<void()> Handler);
    /** @brief Install the callback invoked when the asset-inspector modal is closed by the user. */
    void SetContentAssetInspectorCloseHandler(SnAPI::UI::TDelegate<void()> Handler);
    /** @brief Install the callback invoked when the user selects a node inside the asset-inspector hierarchy. */
    void SetContentAssetInspectorNodeSelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)> Handler);
    /** @brief Install the callback invoked when the user requests a hierarchy edit inside the asset-inspector modal. */
    void SetContentAssetInspectorHierarchyActionHandler(SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> Handler);
    /** @brief Replace the detail-pane payload for the currently selected content asset. */
    void SetContentAssetDetails(ContentAssetDetails Details);
    /** @brief Replace the asset-inspector modal state. */
    void SetContentAssetInspectorState(ContentAssetInspectorState State);
    /** @brief Replace the docked Conduit workspace state. */
    void SetConduitWorkspaceState(ConduitWorkspaceState State);
    /** @brief Install the callback invoked when the user selects one Conduit graph variable. */
    void SetConduitVariableSelectionHandler(SnAPI::UI::TDelegate<void(const Uuid&)> Handler);
    /** @brief Install the callback invoked when the user creates one Conduit graph variable. */
    void SetConduitVariableCreateHandler(SnAPI::UI::TDelegate<void(const std::string&, const TypeId&)> Handler);
    /** @brief Install the callback invoked when the user removes the selected Conduit graph variable. */
    void SetConduitVariableRemoveHandler(SnAPI::UI::TDelegate<void()> Handler);
    /** @brief Install the callback invoked when the user renames the selected Conduit graph variable. */
    void SetConduitVariableRenameHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    /** @brief Install the callback invoked when the user changes the selected Conduit graph variable type. */
    void SetConduitVariableTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)> Handler);
    /** @brief Install the callback invoked when the user changes the selected bool default. */
    void SetConduitVariableDefaultBoolHandler(SnAPI::UI::TDelegate<void(bool)> Handler);
    /** @brief Install the callback invoked when the user submits a text-encoded selected-variable default. */
    void SetConduitVariableDefaultTextHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    /** @brief Install the callback invoked when the user selects one enum default value. */
    void SetConduitVariableDefaultEnumHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    /** @brief Install the callback invoked when the user clears the selected-variable default. */
    void SetConduitVariableClearDefaultHandler(SnAPI::UI::TDelegate<void()> Handler);
    /** @brief Install the callback invoked when the user applies complex-default edits. */
    void SetConduitVariableCommitDefaultHandler(SnAPI::UI::TDelegate<void()> Handler);
    /** @brief Install the callback invoked when the user resets complex-default scratch state. */
    void SetConduitVariableResetDefaultHandler(SnAPI::UI::TDelegate<void()> Handler);
    /** @brief Install the callback invoked when the user selects one authored Conduit node. */
    void SetConduitNodeSelectionHandler(SnAPI::UI::TDelegate<void(const Uuid&)> Handler);
    /** @brief Install the callback invoked when the user spawns one schema-backed Conduit node. */
    void SetConduitNodeCreateHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    /** @brief Install the callback invoked when the user removes the selected authored Conduit node. */
    void SetConduitNodeRemoveHandler(SnAPI::UI::TDelegate<void()> Handler);
    /** @brief Install the callback invoked when the user drags one authored Conduit node on the canvas. */
    void SetConduitNodeMoveHandler(SnAPI::UI::TDelegate<void(const Uuid&, float, float)> Handler);
    /** @brief Install the callback invoked when the user edits the primary text field for the selected authored Conduit node. */
    void SetConduitNodePrimaryTextHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    /** @brief Install the callback invoked when the user edits the secondary text field for the selected authored Conduit node. */
    void SetConduitNodeSecondaryTextHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    /** @brief Install the callback invoked when the user pans or zooms the Conduit canvas viewport. */
    void SetConduitViewportHandler(SnAPI::UI::TDelegate<void(float, float, float)> Handler);
    /** @brief Install the callback invoked when the user renames the active Conduit class. */
    void SetConduitClassNameHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);
    /** @brief Install the callback invoked when the user changes the active Conduit class host type. */
    void SetConduitClassHostTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)> Handler);
    /** @brief Install the callback invoked when the user changes the active Conduit class graph reference. */
    void SetConduitClassGraphHandler(SnAPI::UI::TDelegate<void(const std::string&)> Handler);

private:
    using PanelBuilder = SnAPI::UI::TElementBuilder<SnAPI::UI::UIPanel>;

    enum class EHierarchyEntryKind : std::uint8_t
    {
        World,
        Level,
        Node,
    };

    enum class EImportProfile : std::uint8_t
    {
        Unknown = 0,
        AssimpModel,
        Texture,
    };

    struct HierarchyEntry
    {
        NodeHandle Handle{};
        int Depth = 0;
        std::string Label{};
        EHierarchyEntryKind Kind = EHierarchyEntryKind::Node;
    };

    [[nodiscard]] bool RegisterExternalElements(GameRuntime& Runtime);
    [[nodiscard]] SnAPI::UI::UIContext* RootContext(GameRuntime& Runtime) const;

    void BuildShell(SnAPI::UI::UIContext& Context,
                    GameRuntime& Runtime,
                    ComponentHandle& ActiveCamera,
                    EditorSelectionModel* SelectionModel);
    void ConfigureRoot(SnAPI::UI::UIContext& Context);

    void BuildMenuBar(PanelBuilder& Root);
    void BuildToolbar(PanelBuilder& Root);
    void BuildWorkspace(PanelBuilder& Root,
                        GameRuntime& Runtime,
                        ComponentHandle& ActiveCamera,
                        EditorSelectionModel* SelectionModel);
    void BuildContentBrowser(PanelBuilder& Root);
    void EnsureContextMenuOverlay();
    void EnsureContentAssetCreateModalOverlay();
    void DestroyContentAssetCreateModalOverlay();
    void EnsureContentAssetImportModalOverlay();
    void DestroyContentAssetImportModalOverlay();
    void EnsureContentAssetInspectorModalOverlay();
    void DestroyContentAssetInspectorModalOverlay();
    void EnsureProjectModalOverlay();
    void DestroyProjectModalOverlay();
    void EnsureProjectSettingsModalOverlay();
    void DestroyProjectSettingsModalOverlay();

    void BuildHierarchyPane(PanelBuilder& Workspace,
                            GameRuntime& Runtime,
                            ComponentHandle& ActiveCamera,
                            EditorSelectionModel* SelectionModel);
    void BuildGamePane(PanelBuilder& Workspace, GameRuntime& Runtime, ComponentHandle& ActiveCamera);
    void BuildInspectorPane(PanelBuilder& Workspace, BaseNode* SelectedNode, GameRuntime& Runtime, ComponentHandle& ActiveCamera);
    void BuildContentDetailsPane(PanelBuilder& DetailsTab);

    void EnsureDefaultSelection(GameRuntime& Runtime, ComponentHandle& ActiveCamera);
    void SyncHierarchy(GameRuntime& Runtime, ComponentHandle& ActiveCamera);
    void RebuildHierarchyTree(const std::vector<HierarchyEntry>& Entries, const NodeHandle& SelectedNode);
    void SyncHierarchySelection(const NodeHandle& SelectedNode);
    [[nodiscard]] bool CollectHierarchyEntries(World& WorldRef, std::vector<HierarchyEntry>& OutEntries) const;
    [[nodiscard]] std::uint64_t ComputeHierarchySignature(const std::vector<HierarchyEntry>& Entries) const;
    void OnHierarchyNodeChosen(const NodeHandle& Handle);
    [[nodiscard]] BaseNode* ResolveSelectedNode(GameRuntime& Runtime, ComponentHandle& ActiveCamera) const;
    [[nodiscard]] bool QueryInvalidationDebugOverlayEnabled() const;
    void SetInvalidationDebugOverlayEnabled(bool Enabled);
    void ToggleInvalidationDebugOverlay();
    void SyncInvalidationDebugOverlay();
    void PublishInvalidationDebugState();
    void HandleContentAssetCardClicked(std::size_t AssetIndex);
    void SelectContentAsset(std::size_t AssetIndex, bool NotifySelection, bool IsDoubleClick);
    void OpenHierarchyContextMenu(std::size_t ItemIndex, const SnAPI::UI::PointerEvent& Event);
    void OpenHierarchyAddTypeMenu(bool AddComponents);
    void OpenContentAssetContextMenu(std::size_t AssetIndex, const SnAPI::UI::PointerEvent& Event);
    void OpenFileMenu();
    void OpenInspectorComponentContextMenu(const NodeHandle& OwnerNode,
                                           const TypeId& ComponentType,
                                           const SnAPI::UI::PointerEvent& Event);
    void OpenContentBrowserContextMenu(const SnAPI::UI::PointerEvent& Event);
    void OpenContextMenu(const SnAPI::UI::UIPoint& ScreenPosition, std::vector<SnAPI::UI::UIContextMenuItem> Items);
    void CloseContextMenu();
    void OnContextMenuItemInvoked(const SnAPI::UI::UIContextMenuItem& Item);
    void EnsureContentAssetCardCapacity();
    void UpdateContentAssetCardWidgets();
    void ApplyContentAssetFilter();
    void OpenContentAssetCreateModal();
    void CloseContentAssetCreateModal();
    void ConfirmContentAssetCreate();
    void RefreshContentAssetCreateModalVisibility();
    void RebuildContentAssetCreateTypeTree();
    void RefreshContentAssetCreateOkButtonState();
    void OpenContentAssetImportModal();
    void CloseContentAssetImportModal();
    void ConfirmContentAssetImport();
    void RefreshContentAssetImportModalVisibility();
    void RefreshContentAssetImportProfile();
    void RefreshContentAssetImportSettingsPanel();
    void RefreshContentAssetImportSummary();
    void RefreshContentAssetImportOkButtonState();
    void OpenProjectWelcomeModal();
    void OpenProjectCreateModal();
    void OpenProjectOpenModal();
    void OpenProjectSettingsModal();
    void CloseProjectSettingsModal();
    void ConfirmProjectSettingsModal();
    void CloseProjectModal(bool ForceClose = false);
    void ConfirmProjectModal();
    void RefreshProjectModalVisibility();
    void RefreshProjectModalOkButtonState();
    void RefreshProjectSettingsModalVisibility();
    void RefreshProjectSettingsModalSaveButtonState();
    void RememberRecentProject(const ProjectActionRequest& Request);
    void RememberRecentProjectFile(std::string ProjectFilePath, std::string ProjectName = {});
    void CloseContentAssetInspectorModal(bool NotifyHandler);
    void RefreshContentAssetInspectorModalVisibility();
    void RefreshContentAssetInspectorModalState();
    void RebuildContentAssetInspectorHierarchyTree();
    void OpenContentAssetInspectorHierarchyContextMenu(std::size_t ItemIndex, const SnAPI::UI::PointerEvent& Event);
    void OpenContentAssetInspectorComponentContextMenu(const NodeHandle& OwnerNode,
                                                       const TypeId& ComponentType,
                                                       const SnAPI::UI::PointerEvent& Event);
    void RebuildContentBrowserEntries();
    void RefreshContentBrowserPath();
    void RefreshContentAssetCardSelectionStyles();
    void RefreshContentAssetDetailsViewModel();
    void RefreshConduitWorkspaceView();
    [[nodiscard]] std::size_t ResolveSelectedContentAssetIndex() const;
    [[nodiscard]] UIPropertyPanel* ResolveConduitVariableDefaultPanel() const;
    void InitializeViewModel();

    template<typename TValue>
    SnAPI::UI::TPropertyRef<TValue> ViewModelProperty(const SnAPI::UI::PropertyKey Key)
    {
        return SnAPI::UI::TPropertyRef<TValue>(&m_viewModel, Key);
    }

    void BindInspectorTarget(BaseNode* SelectedNode, GameRuntime& Runtime, ComponentHandle& ActiveCamera);
    void SyncGameViewportCamera(GameRuntime& Runtime, ComponentHandle& ActiveCamera);

    [[nodiscard]] UIRenderViewport* ResolveGameViewport() const;
    [[nodiscard]] SnAPI::UI::UITabs* ResolveGameViewTabs() const;
    [[nodiscard]] UIPropertyPanel* ResolveInspectorPanel() const;

    SnAPI::UI::UIContext* m_context = nullptr;
    GameRuntime* m_runtime = nullptr;
    SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> m_shellRoot{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITabs> m_gameViewTabs{};
    SnAPI::UI::ElementHandle<UIRenderViewport> m_gameViewport{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_inspectorPropertyPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> m_hierarchyTree{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIContextMenu> m_contextMenu{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIBadge> m_hierarchyCountBadge{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UISwitch> m_invalidationDebugToggleSwitch{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_invalidationDebugToggleLabel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_contentSearchInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIBreadcrumbs> m_contentPathBreadcrumbs{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_contentAssetNameValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetTypeValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetVariantValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetIdValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetStatusValue{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentPlaceButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentSaveButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIListView> m_contentAssetsList{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentAssetsEmptyHint{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_contentCreateModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> m_contentCreateTypeTree{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_contentCreateSearchInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_contentCreateNameInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentCreateOkButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_contentImportModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> m_contentImportSourcePicker{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentImportSummaryText{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_contentImportSettingsPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentImportOkButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_contentInspectorModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorTitleText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorStatusText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorHierarchyTitleText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorPreviewStatsText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIImage> m_contentInspectorPreviewImage{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> m_contentInspectorHierarchyTree{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_contentInspectorPropertyPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_contentInspectorImportSettingsTitleText{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_contentInspectorImportSettingsPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentInspectorSaveButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_contentInspectorReimportButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitWorkspaceTitleText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitWorkspaceStatusText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitWorkspaceSummaryText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> m_conduitVariablesTree{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_conduitPaletteSearchInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> m_conduitPaletteTree{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_conduitPaletteAddNodeButton{};
    SnAPI::UI::ElementHandle<Conduit::Editor::UIConduitGraphCanvas> m_conduitGraphCanvas{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> m_conduitNodesTree{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_conduitNodeRemoveButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> m_conduitVariablesCard{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> m_conduitNodesCard{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> m_conduitInspectorCard{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> m_conduitVariableInspectorPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> m_conduitNodeInspectorPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> m_conduitClassCard{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> m_conduitGraphWorkspaceHost{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> m_conduitClassWorkspaceHost{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitInspectorTitleText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_conduitVariableCreateNameInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIComboBox> m_conduitVariableCreateTypeCombo{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_conduitVariableCreateButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_conduitVariableNameInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIComboBox> m_conduitVariableTypeCombo{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_conduitVariableRemoveButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitVariableDefaultHintText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UICheckbox> m_conduitVariableDefaultBoolCheckbox{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_conduitVariableDefaultTextInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIComboBox> m_conduitVariableDefaultEnumCombo{};
    SnAPI::UI::ElementHandle<UIPropertyPanel> m_conduitVariableDefaultPropertyPanel{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_conduitVariableDefaultClearButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_conduitVariableDefaultApplyButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_conduitVariableDefaultResetButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitNodeSummaryText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitNodePrimaryLabelText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_conduitNodePrimaryTextInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitNodeSecondaryLabelText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_conduitNodeSecondaryTextInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_conduitClassNameInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIComboBox> m_conduitClassHostTypeCombo{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIComboBox> m_conduitClassGraphCombo{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitClassOverviewSummaryText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitClassOverviewHostText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIText> m_conduitClassOverviewGraphText{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_menuFileButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_projectModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_projectNameInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> m_projectDirectoryInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> m_projectFilePathInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_projectModalOkButton{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> m_projectSettingsModalOverlay{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> m_projectSettingsNameInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> m_projectSettingsStartupAssetInput{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIComboBox> m_projectSettingsDefaultRenderSettingsCombo{};
    SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> m_projectSettingsSaveButton{};

    struct ContentAssetCardWidgets
    {
        SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> Button{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> Card{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIImage> Icon{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIText> Type{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIText> Name{};
        SnAPI::UI::ElementHandle<SnAPI::UI::UIText> Variant{};
    };
    struct ContentBrowserEntry
    {
        bool IsFolder = false;
        std::size_t AssetIndex = 0;
        std::string FolderPath{};
        std::string DisplayName{};
    };
    struct RecentProjectEntry
    {
        std::string Name{};
        std::string ProjectFilePath{};
    };

    std::vector<ContentAssetCardWidgets> m_contentAssetCards{};
    std::vector<SnAPI::UI::ElementHandle<SnAPI::UI::UIButton>> m_contentAssetCardButtons{};
    std::vector<std::size_t> m_contentAssetCardIndices{};
    std::vector<ContentBrowserEntry> m_contentBrowserEntries{};
    std::vector<ContentAssetEntry> m_contentAssets{};
    ContentAssetDetails m_contentAssetDetails{};
    std::string m_contentAssetFilterText{};
    std::string m_contentCurrentFolder{};
    std::string m_selectedContentAssetKey{};
    std::string m_selectedContentFolderPath{};
    std::string m_lastContentAssetClickKey{};
    std::chrono::steady_clock::time_point m_lastContentAssetClickTime{};
    bool m_contentCreateModalOpen = false;
    std::string m_contentCreateTypeFilterText{};
    std::string m_contentCreateNameText{};
    TypeId m_contentCreateSelectedType{};
    std::vector<TypeId> m_contentCreateVisibleTypes{};
    std::shared_ptr<SnAPI::UI::ITreeItemSource> m_contentCreateTypeSource{};
    bool m_contentImportModalOpen = false;
    std::string m_contentImportSourcePath{};
    EImportProfile m_contentImportProfile = EImportProfile::Unknown;
    AssimpImportSettings m_contentImportAssimpSettings{};
    TextureImportSettings m_contentImportTextureSettings{};
    ContentAssetInspectorState m_contentAssetInspectorState{};
    ConduitWorkspaceState m_conduitWorkspaceState{};
    std::vector<Uuid> m_conduitVisibleVariableIds{};
    std::vector<Uuid> m_conduitVisibleNodeIds{};
    std::vector<std::string> m_conduitVisiblePaletteStableIds{};
    std::string m_conduitPaletteFilterText{};
    std::string m_conduitSelectedPaletteStableId{};
    std::string m_conduitCreateVariableNameText{};
    TypeId m_conduitCreateSelectedVariableType{};
    bool m_conduitVariableDefaultPanelBound = false;
    void* m_conduitVariableDefaultBoundObject = nullptr;
    TypeId m_conduitVariableDefaultBoundType{};
    bool m_projectModalOpen = false;
    bool m_projectModalRequired = false;
    bool m_projectModalShowWelcome = false;
    bool m_projectSettingsModalOpen = false;
    EProjectAction m_projectModalAction = EProjectAction::CreateNew;
    std::string m_projectNameText{};
    std::string m_projectDirectoryText{};
    std::string m_projectFilePathText{};
    std::string m_projectSettingsNameText{};
    std::string m_projectSettingsStartupAssetText{};
    std::string m_projectSettingsDefaultRenderSettingsAssetId{};
    std::vector<std::pair<std::string, std::string>> m_projectSettingsRenderSettingsOptions{};
    ProjectState m_projectState{};
    std::vector<RecentProjectEntry> m_recentProjects{};
    std::vector<NodeHandle> m_contentInspectorVisibleNodes{};
    std::shared_ptr<SnAPI::UI::ITreeItemSource> m_contentInspectorHierarchySource{};
    bool m_contentInspectorTargetBound = false;
    NodeHandle m_contentInspectorBoundNode{};
    void* m_contentInspectorBoundObject = nullptr;
    TypeId m_contentInspectorBoundType{};
    std::size_t m_contentInspectorBoundComponentSignature = 0;
    bool m_contentInspectorImportTargetBound = false;
    void* m_contentInspectorImportBoundObject = nullptr;
    TypeId m_contentInspectorImportBoundType{};
    SnAPI::UI::TDelegate<void(const std::string&, bool)> m_onContentAssetSelected{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetPlaceRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetSaveRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onContentAssetDeleteRequested{};
    SnAPI::UI::TDelegate<void(const std::string&, const std::string&)> m_onContentAssetRenameRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetRefreshRequested{};
    SnAPI::UI::TDelegate<void(const ContentAssetCreateRequest&)> m_onContentAssetCreateRequested{};
    SnAPI::UI::TDelegate<void(const ContentAssetImportRequest&)> m_onContentAssetImportRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetInspectorSaveRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetInspectorReimportRequested{};
    SnAPI::UI::TDelegate<void()> m_onContentAssetInspectorCloseRequested{};
    SnAPI::UI::TDelegate<void(const NodeHandle&)> m_onContentAssetInspectorNodeSelected{};
    SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> m_onContentAssetInspectorHierarchyActionRequested{};
    SnAPI::UI::TDelegate<void(const Uuid&)> m_onConduitVariableSelected{};
    SnAPI::UI::TDelegate<void(const std::string&, const TypeId&)> m_onConduitVariableCreateRequested{};
    SnAPI::UI::TDelegate<void()> m_onConduitVariableRemoveRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onConduitVariableRenameRequested{};
    SnAPI::UI::TDelegate<void(const TypeId&)> m_onConduitVariableTypeRequested{};
    SnAPI::UI::TDelegate<void(bool)> m_onConduitVariableDefaultBoolRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onConduitVariableDefaultTextRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onConduitVariableDefaultEnumRequested{};
    SnAPI::UI::TDelegate<void()> m_onConduitVariableClearDefaultRequested{};
    SnAPI::UI::TDelegate<void()> m_onConduitVariableCommitDefaultRequested{};
    SnAPI::UI::TDelegate<void()> m_onConduitVariableResetDefaultRequested{};
    SnAPI::UI::TDelegate<void(const Uuid&)> m_onConduitNodeSelected{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onConduitNodeCreateRequested{};
    SnAPI::UI::TDelegate<void()> m_onConduitNodeRemoveRequested{};
    SnAPI::UI::TDelegate<void(const Uuid&, float, float)> m_onConduitNodeMoveRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onConduitNodePrimaryTextRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onConduitNodeSecondaryTextRequested{};
    SnAPI::UI::TDelegate<void(float, float, float)> m_onConduitViewportRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onConduitClassNameRequested{};
    SnAPI::UI::TDelegate<void(const TypeId&)> m_onConduitClassHostTypeRequested{};
    SnAPI::UI::TDelegate<void(const std::string&)> m_onConduitClassGraphRequested{};
    std::shared_ptr<SnAPI::UI::ITreeItemSource> m_hierarchyItemSource{};
    enum class EContextMenuScope : std::uint8_t
    {
        None,
        MenuBar,
        HierarchyItem,
        InspectorComponent,
        ContentAssetItem,
        ContentBrowser,
        ContentInspectorHierarchyItem,
        ContentInspectorComponent,
    };
    enum class EPendingHierarchyMenu : std::uint8_t
    {
        None,
        Root,
        AddNodeTypes,
        AddComponentTypes,
    };
    EContextMenuScope m_contextMenuScope = EContextMenuScope::None;
    EPendingHierarchyMenu m_pendingHierarchyMenu = EPendingHierarchyMenu::None;
    std::optional<std::size_t> m_pendingHierarchyMenuIndex{};
    SnAPI::UI::UIPoint m_pendingHierarchyMenuOpenPosition{};
    std::optional<std::size_t> m_contextMenuHierarchyIndex{};
    std::optional<std::size_t> m_contextMenuAssetIndex{};
    NodeHandle m_contextMenuContentInspectorNode{};
    std::optional<NodeHandle> m_contextMenuComponentOwner{};
    TypeId m_contextMenuComponentType{};
    std::vector<TypeId> m_contextMenuNodeTypes{};
    std::vector<TypeId> m_contextMenuComponentTypes{};
    SnAPI::UI::UIPoint m_contextMenuOpenPosition{};
    std::vector<NodeHandle> m_hierarchyVisibleNodes{};
    std::uint64_t m_hierarchySignature = 0;
    std::size_t m_hierarchyNodeCount = 0;
    NodeHandle m_hierarchyVisualSelection{};
    std::string m_hierarchyFilterText{};
    EditorSelectionModel* m_selection = nullptr;
    SnAPI::UI::TDelegate<void(const NodeHandle&)> m_onHierarchyNodeChosen{};
    SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> m_onHierarchyActionRequested{};
    SnAPI::UI::TDelegate<void(EToolbarAction)> m_onToolbarActionRequested{};
    SnAPI::UI::TDelegate<void(const ProjectActionRequest&)> m_onProjectActionRequested{};
    NodeHandle m_boundInspectorNode{};
    void* m_boundInspectorObject = nullptr;
    TypeId m_boundInspectorType{};
    std::size_t m_boundInspectorComponentSignature = 0;
    EGizmoSpace m_gizmoSpace = EGizmoSpace::World;
    ESnapMode m_snapMode = ESnapMode::Off;
    double m_moveSnapStep = 1.0;
    double m_rotateSnapStepDegrees = 15.0;
    double m_scaleSnapStep = 0.5;
    bool m_invalidationDebugOverlayEnabled = false;
    SnAPI::UI::PropertyMap m_viewModel{};
    bool m_built = false;
};

} // namespace SnAPI::GameFramework::Editor
