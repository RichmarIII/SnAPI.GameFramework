#pragma once

#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"
#include "Editor/EditorImportSettings.h"
#include "ModuleCreationService.h"
#include "PluginCreationService.h"
#include "ProjectCreationService.h"

#include "Handles.h"
#include "Math.h"
#include "TypeRegistration.h"
#include "AssetManager.h"
#include "RenderAssetPayloads.h"
#include <TextureCompressorPayloads.h>

#include <filesystem>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SnAPI::GameFramework
{
class BaseNode;
class World;
}

namespace SnAPI::GameFramework::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief High-level import profile recorded for editor-managed asset imports.
 *
 * The profile is editor metadata, not the runtime asset kind. It tells the editor which
 * reflected import-settings object should be exposed for reimport workflows.
 */
enum class EAssetImportProfile : std::uint8_t
{
    Unknown = 0, /**< @brief No recognized import profile is recorded. The asset may be non-imported or metadata may be incomplete. */
    AssimpModel, /**< @brief Asset originated from the Assimp-based model import pipeline. */
    Texture, /**< @brief Asset originated from the texture-compression import pipeline. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Asset-discovery, import, editing, and instantiation backend for the editor.
 *
 * `EditorAssetService` is the editor module's central asset workflow service. It owns the
 * editor-facing `AssetManager`, maintains the live discovery index shown to asset browsers,
 * tracks selection and placement intent, manages project-level asset roots, and hosts the
 * temporary state used by the asset inspector.
 *
 * Core responsibilities:
 * - discover source assets and mounted runtime assets and expose a stable editor-facing list
 * - create, rename, delete, save, and import assets
 * - manage project files, startup level packs, and default render settings assets
 * - host a temporary asset-editor session for node, level, world, texture, mesh, and material assets
 * - instantiate placeable assets into the active editor world
 *
 * Core semantics:
 * - The service owns one `AssetManager` instance and rebuilds it when project roots change.
 * - Discovery results are snapshots stored in internal vectors; references and pointers returned
 *   by query functions are invalidated by `RefreshDiscovery()` and many mutating operations.
 * - Packed-asset rename edits are staged in editor-only override maps until saved.
 * - Asset-inspector dirty tracking is revision-based. Runtime and import-setting mutations mark the
 *   active document dirty immediately instead of polling serialized payload diffs every frame.
 * - Import settings edits are persisted onto authored assets and mirrored into reimport metadata;
 *   they still typically require `ReimportActiveAsset()` to affect cooked output.
 *
 * Ownership and lifetime:
 * - Owned by `GameEditor` through the `IEditorService` contract.
 * - The service owns the asset manager and any temporary asset-editor world it creates.
 * - Raw pointers exposed through `AssetEditorSessionView` are borrowed pointers into service-owned
 *   state and become invalid when the session closes, the asset editor switches targets, discovery
 *   rebuilds affected state, or the service shuts down.
 *
 * Threading model:
 * - Main-thread only.
 * - The service does not synchronize public API access.
 * - Some operations may perform file I/O and may block.
 *
 * @see IEditorService
 * @see EditorImportSettings.h
 * @see AssetEditorSessionView
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorAssetService final : public IEditorService
{
public:
    /**
     * @brief One discovered asset entry exposed to editor UI.
     *
     * Instances are rebuilt from the asset manager plus editor override state during
     * `RefreshDiscovery()`. They should be treated as snapshot records rather than long-lived handles.
     */
    struct DiscoveredAsset
    {
        std::string Key{}; /**< @brief Editor lookup key, currently derived from the logical source asset name. Stable until discovery state is rebuilt. */
        std::string Name{}; /**< @brief Logical source asset name used by source discovery and runtime JIT lookup. */
        std::string TypeLabel{}; /**< @brief Human-readable asset-kind label for UI display. */
        std::string Variant{}; /**< @brief Variant key within the owning asset pack entry. Empty means the default variant. */
        ::SnAPI::AssetPipeline::AssetId AssetId{}; /**< @brief Stable asset identifier used by the asset pipeline and runtime references. */
        TypeId AssetType{}; /**< @brief Reflected authored source asset type, or empty when unresolved. */
        ::SnAPI::AssetPipeline::TypeId AssetKind{}; /**< @brief Runtime asset kind describing how the asset loads or instantiates. */
        ::SnAPI::AssetPipeline::TypeId CookedPayloadType{}; /**< @brief Cooked payload serializer type currently associated with the asset. */
        uint32_t SchemaVersion = 0; /**< @brief Schema version of the currently discovered cooked payload. */
        bool IsRuntime = false; /**< @brief `true` when the asset currently lives in the runtime-asset store rather than only in a mounted pack. */
        bool IsDirty = false; /**< @brief `true` when the editor currently tracks unsaved runtime payload or metadata overrides for the asset. */
        bool CanSave = true; /**< @brief `true` when the current asset state is considered saveable through the editor workflow. */
        std::string OwningPackPath{}; /**< @brief Best-known source or cooked backing path for UI display. */
        std::string SourceFilePath{}; /**< @brief Absolute source file path when the editor discovered this asset from project content. */ 
    };

    /**
     * @brief Read-only snapshot of the active asset-editor session.
     *
     * The view is intentionally value-based except for `TargetObject` and `ImportSettingsObject`,
     * which are borrowed pointers to the mutable objects currently edited by inspector UI.
     */
    struct AssetEditorSessionView
    {
        /**
         * @brief Flattened hierarchy entry for node and level asset editing.
         */
        struct NodeEntry
        {
            NodeHandle Handle{}; /**< @brief Handle of the node represented by this row. Valid only while the asset-editor world still exists. */
            int Depth = 0; /**< @brief Zero-based hierarchy depth used for tree indentation. */
            std::string Label{}; /**< @brief Display label generated from node name and reflected type. */
        };

        bool IsOpen = false; /**< @brief `true` when the service currently has an active asset-editor session. */
        std::string AssetKey{}; /**< @brief Discovery key of the asset currently being edited. */
        std::string Title{}; /**< @brief UI-facing session title, typically `<TypeLabel> - <Name>`. */
        TypeId TargetType{}; /**< @brief Reflected type of `TargetObject`, or empty when no runtime-editable object is exposed. */
        void* TargetObject = nullptr; /**< @brief Borrowed pointer to the mutable runtime-editable object currently exposed in the inspector. */
        TypeId ImportSettingsType{}; /**< @brief Reflected type of `ImportSettingsObject`, or empty when no import settings are available. */
        void* ImportSettingsObject = nullptr; /**< @brief Borrowed pointer to the mutable import-settings object currently exposed in the inspector. */
        std::vector<NodeEntry> Nodes{}; /**< @brief Flattened hierarchy snapshot for node or level assets. Empty for non-hierarchical assets. */
        NodeHandle SelectedNode{}; /**< @brief Current selection within the asset-editor hierarchy. */
        bool CanEditHierarchy = false; /**< @brief `true` when add/remove node and component operations are supported for the active asset. */
        bool HasImportSettings = false; /**< @brief `true` when import metadata resolved to an editable reflected settings object. */
        bool RuntimeDirty = false; /**< @brief `true` when the runtime-editable payload differs from the session baseline. */
        bool ImportSettingsDirty = false; /**< @brief `true` when editable import settings differ from the stored import metadata baseline. */
        bool IsDirty = false; /**< @brief Aggregate dirty flag equal to `RuntimeDirty || ImportSettingsDirty`. */
        bool CanSave = false; /**< @brief `true` when the active session can currently be saved through the editor. */
        bool CanReimport = false; /**< @brief `true` when a valid reimport source path and import profile are available. */
        bool HasTexturePreviewStats = false; /**< @brief `true` when the texture preview statistics below are populated. */
        std::uint32_t TexturePreviewWidth = 0; /**< @brief Base-level texture width in texels for the previewed cooked texture. */
        std::uint32_t TexturePreviewHeight = 0; /**< @brief Base-level texture height in texels for the previewed cooked texture. */
        std::uint32_t TexturePreviewMipCount = 0; /**< @brief Number of mip levels present in the previewed cooked texture. */
        std::string TexturePreviewTarget{}; /**< @brief Human-readable compression target label for the previewed texture. */
        std::string TexturePreviewFormat{}; /**< @brief Human-readable compression format label for the previewed texture. */
        std::uint64_t TexturePreviewGpuSizeBytes = 0; /**< @brief Estimated GPU memory footprint in bytes for the previewed texture. */
    };

    /**
     * @brief Snapshot of the currently loaded editor project.
     *
     * Paths are stored as strings exactly as the editor currently tracks them. Some values are
     * logical project-relative fields, while the `*Directory` fields are resolved filesystem paths.
     */
    struct ProjectInfo
    {
        bool IsLoaded = false; /**< @brief `true` when a project file has been loaded successfully. */
        std::string Name{}; /**< @brief Project display name from the project file. */
        std::string ProjectFilePath{}; /**< @brief Absolute or normalized path to the loaded project file. */
        std::string ProjectRootDirectory{}; /**< @brief Resolved filesystem directory containing the project file. */
        std::string AssetRoot{}; /**< @brief Asset-root field stored in project settings, potentially project-relative or URI-based. */
        std::string AssetRootDirectory{}; /**< @brief Resolved filesystem directory used as the live asset root. */
        std::string StartupLevelAsset{}; /**< @brief Startup level source-asset field stored in project settings. */
        std::string DefaultRenderSettingsAssetId{}; /**< @brief Asset id string for the project's default `WorldRenderSettings` node, if any. */
    };

    /** @brief Stable service name for diagnostics. @return Borrowed static string view. */
    [[nodiscard]] std::string_view Name() const override;
    /**
     * @brief Initialize asset-service state for the current editor session.
     * @param Context Borrowed editor-service context.
     * @return Success or an initialization error.
     * @remarks
     * Initialization clears previous state, ensures template assets exist, configures the active
     * asset root when a project is already loaded, rebuilds the asset manager, and loads project
     * default render settings when applicable.
     */
    Result Initialize(EditorServiceContext& Context) override;
    /**
     * @brief Per-frame asset-service maintenance tick.
     * @param Context Borrowed editor-service context.
     * @param DeltaSeconds Variable-step frame delta in seconds.
     * @remarks
     * This tick advances deferred default-render-settings application and refreshes active
     * asset-editor dirty state, hierarchy state, and reimport readiness.
     */
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    /**
     * @brief Shutdown the asset service and release owned temporary state.
     * @param Context Borrowed editor-service context.
     * @remarks Safe to call repeatedly.
     */
    void Shutdown(EditorServiceContext& Context) override;

    /**
     * @brief Access the current discovered-asset snapshot.
     * @return Borrowed vector reference.
     * @warning The returned reference is invalidated by `RefreshDiscovery()` and most mutating asset operations.
     */
    [[nodiscard]] const std::vector<DiscoveredAsset>& Assets() const { return m_assets; }
    /**
     * @brief Access the currently selected asset snapshot.
     * @return Non-owning pointer into the discovery array, or `nullptr` when no selection exists.
     * @warning Invalidated by discovery rebuilds and selection changes.
     */
    [[nodiscard]] const DiscoveredAsset* SelectedAsset() const;
    /**
     * @brief Query whether placement mode is currently armed.
     * @return `true` when `InstantiateArmedAsset()` would attempt to place an asset.
     */
    [[nodiscard]] bool IsPlacementArmed() const { return !m_placementAssetKey.empty(); }
    /**
     * @brief Access the key of the currently placement-armed asset.
     * @return Borrowed string reference. Empty when placement is not armed.
     */
    [[nodiscard]] const std::string& PlacementAssetKey() const { return m_placementAssetKey; }

    struct AssetPlacementRequest
    {
        NodeHandle Parent{}; /**< @brief Optional explicit parent for hierarchy-style placement. Null falls back to the default scene root/level. */
        bool UseScreenPoint = false; /**< @brief When true, `ScreenPositionX/Y` should be resolved into a viewport/world placement point if possible. */
        float ScreenPositionX = 0.0f; /**< @brief Screen-space X captured at drop/click time. */
        float ScreenPositionY = 0.0f; /**< @brief Screen-space Y captured at drop/click time. */
        bool UseWorldPosition = false; /**< @brief When true, spawn recipes should place the created root at `WorldPosition`. */
        Vec3 WorldPosition{0.0f, 0.0f, 0.0f}; /**< @brief Optional resolved world-space placement position for the created root node. */
        NodeHandle* OutCreatedRoot = nullptr; /**< @brief Optional out-pointer receiving the created root node when one exists. */
    };

    /**
     * @brief Select one discovered asset by key.
     * @param Key Discovery key of the asset to select.
     * @return `true` when the asset exists and selection was updated, otherwise `false`.
     * @remarks This does not open the asset editor or validate saveability.
     */
    bool SelectAssetByKey(std::string_view Key);
    /**
     * @brief Arm one asset for scene placement.
     * @param Key Discovery key of the asset to place.
     * @return Success or an error.
     * @remarks Node, level, world, static-mesh, and texture assets are placeable through this path.
     */
    Result ArmPlacementByKey(std::string_view Key);
    /**
     * @brief Clear placement mode.
     */
    void ClearPlacement();

    /**
     * @brief Rebuild the discovered-asset list from the current asset manager and editor override state.
     * @return Success or an error.
     * @remarks
     * This may clear invalid selection, placement, and asset-editor references when their targets no longer exist.
     */
    Result RefreshDiscovery();
    /**
     * @brief Load a temporary preview of the currently selected asset.
     * @return Success or an error.
     * @remarks
     * Node, level, and world assets are loaded into temporary preview worlds and summarized.
     * Other asset kinds currently report that preview is not implemented.
     */
    Result OpenSelectedAssetPreview();
    /**
     * @brief Save the currently selected asset.
     * @return Success or an error.
     * @remarks Equivalent to `SaveAssetByKey(SelectedAsset()->Key)` when a selection exists.
     */
    Result SaveSelectedAssetUpdate();
    /**
     * @brief Save the currently selected asset using the full editor-service context.
     * @param Context Borrowed editor-service context.
     * @return Success or an error.
     * @remarks
     * This overload allows service-coordinated save behavior for asset kinds such as Conduit graphs
     * that are edited through dedicated document services rather than the legacy asset-inspector lane.
     */
    Result SaveSelectedAssetUpdate(EditorServiceContext& Context);
    /**
     * @brief Persist one asset's current editor-visible state.
     * @param Key Discovery key of the asset to save.
     * @return Success or an error.
     * @remarks
     * Runtime assets are upserted back into the runtime store and then written to disk.
     * Packed assets are rewritten into their owning pack, incorporating staged rename or payload overrides.
     */
    Result SaveAssetByKey(std::string_view Key);
    /**
     * @brief Persist one asset's current editor-visible state using the full editor-service context.
     * @param Context Borrowed editor-service context.
     * @param Key Discovery key of the asset to save.
     * @return Success or an error.
     * @remarks
     * This overload is used when the edited representation lives in another editor service such as
     * `ConduitEditorService`.
     */
    Result SaveAssetByKey(EditorServiceContext& Context, std::string_view Key);
    /**
     * @brief Delete one asset.
     * @param Key Discovery key of the asset to delete.
     * @return Success or an error.
     * @remarks
     * Deleting the last remaining asset in a packed `.snpak` removes the pack file itself.
     */
    Result DeleteAssetByKey(std::string_view Key);
    /**
     * @brief Delete the currently selected asset.
     * @return Success or an error.
     */
    Result DeleteSelectedAsset();
    /**
     * @brief Rename one asset.
     * @param Key Discovery key of the asset to rename.
     * @param NewName New logical asset name.
     * @return Success or an error.
     * @remarks
     * Runtime assets rename immediately. Packed assets stage an in-editor rename override that is not persisted
     * until the asset is saved.
     */
    Result RenameAssetByKey(std::string_view Key, std::string_view NewName);
    /**
     * @brief Rename the currently selected asset.
     * @param NewName New logical asset name.
     * @return Success or an error.
     */
    Result RenameSelectedAsset(std::string_view NewName);
    /**
     * @brief Create a runtime prefab or level asset from an existing world node.
     * @param Context Borrowed editor-service context.
     * @param SourceHandle Source node handle.
     * @return Success or an error.
     * @remarks
     * World nodes are rejected. Level nodes are serialized as level assets; all other node types
     * are serialized as prefab-style node assets.
     */
    Result CreateRuntimePrefabFromNode(EditorServiceContext& Context, const NodeHandle& SourceHandle);
    /**
     * @brief Create a new authored source asset by reflected asset type.
     * @param Context Borrowed editor-service context.
     * @param AssetType Reflected authored asset type to default-construct and serialize.
     * @param AssetName Preferred logical asset name.
     * @param FolderPath Logical destination folder inside the asset root.
     * @return Success or an error.
     * @remarks Material and material-instance pseudo-types are redirected to dedicated creation helpers.
     */
    Result CreateSourceAssetByType(EditorServiceContext& Context,
                                   const TypeId& AssetType,
                                   std::string_view AssetName,
                                   std::string_view FolderPath);
    /**
     * @brief Create a prefab source asset initialized from one concrete node type.
     * @param Context Borrowed editor-service context.
     * @param NodeType Reflected root-node type to instantiate into a scratch world.
     * @param AssetName Preferred logical asset name.
     * @param FolderPath Logical destination folder inside the asset root.
     * @return Success or an error.
     */
    Result CreatePrefabSourceAssetByNodeType(EditorServiceContext& Context,
                                             const TypeId& NodeType,
                                             std::string_view AssetName,
                                             std::string_view FolderPath);
    /**
     * @brief Create a runtime material asset with default payload values.
     * @param Context Borrowed editor-service context.
     * @param AssetName Preferred logical asset name.
     * @param FolderPath Logical destination folder inside the asset root.
     * @return Success or an error.
     */
    Result CreateRuntimeMaterialAsset(EditorServiceContext& Context,
                                      std::string_view AssetName,
                                      std::string_view FolderPath);
    /**
     * @brief Create a runtime material-instance asset with default payload values.
     * @param Context Borrowed editor-service context.
     * @param AssetName Preferred logical asset name.
     * @param FolderPath Logical destination folder inside the asset root.
     * @return Success or an error.
     */
    Result CreateRuntimeMaterialInstanceAsset(EditorServiceContext& Context,
                                              std::string_view AssetName,
                                              std::string_view FolderPath);
    /**
     * @brief Import one source file into the current asset root.
     * @param Context Borrowed editor-service context.
     * @param SourcePath Source file path or resolvable URI.
     * @param DestinationFolderPath Logical destination folder inside the asset root.
     * @param BuildOptions Additional pipeline build options. Managed options may be normalized or overridden.
     * @param ImportSettings Optional typed import-settings object. When empty, importer-specific defaults are created.
     * @return Success or an error.
     * @remarks
     * Import may create or append to a `.snpak`, remount the resulting pack, clear asset caches,
     * and persist import metadata for later reimport.
     */
    Result ImportSourceAsset(EditorServiceContext& Context,
                             std::string_view SourcePath,
                             std::string_view DestinationFolderPath,
                             const std::unordered_map<std::string, std::string>& BuildOptions,
                             ::SnAPI::AssetPipeline::AssetImportSettingsPtr ImportSettings = {});
    /**
     * @brief Open the asset inspector for one asset.
     * @param Key Discovery key of the asset to inspect.
     * @return Success or an error.
     * @remarks
     * Opening a new asset editor closes any previous session first. Hierarchical assets are loaded
     * into a temporary `World`; material and texture-style assets use temporary payload objects instead.
     */
    Result OpenAssetEditorByKey(std::string_view Key);
    /**
     * @brief Open the editor surface for one asset using the full editor-service context.
     * @param Context Borrowed editor-service context.
     * @param Key Discovery key of the asset to inspect or open.
     * @return Success or an error.
     * @remarks
     * This overload routes asset kinds that use dedicated document services, such as Conduit graph
     * assets, into those editor systems instead of the legacy inspector-only path.
     */
    Result OpenAssetEditorByKey(EditorServiceContext& Context, std::string_view Key);
    /**
     * @brief Close the active asset-editor session.
     */
    void CloseAssetEditor();
    /**
     * @brief Close the active asset editor or Conduit document session.
     * @param Context Borrowed editor-service context.
     */
    void CloseAssetEditor(EditorServiceContext& Context);
    /**
     * @brief Change the selected node inside the active asset-editor hierarchy.
     * @param Node Requested node handle. A null handle selects the root.
     * @return Success or an error.
     */
    Result SelectAssetEditorNode(const NodeHandle& Node);
    /**
     * @brief Add a child node inside the active hierarchical asset editor.
     * @param Parent Parent node handle. A null handle means the current asset-editor root.
     * @param NodeType Reflected node type to create.
     * @return Success or an error.
     * @warning World and Level node types are rejected in node-asset hierarchy editing.
     */
    Result AddAssetEditorNode(const NodeHandle& Parent, const TypeId& NodeType);
    /**
     * @brief Delete one node from the active hierarchical asset editor.
     * @param Node Node handle to delete.
     * @return Success or an error.
     * @warning The root node of the asset-editor session cannot be deleted.
     */
    Result DeleteAssetEditorNode(const NodeHandle& Node);
    /**
     * @brief Add a component to a node in the active hierarchical asset editor.
     * @param Owner Owner node handle.
     * @param ComponentType Reflected component type to create.
     * @return Success or an error.
     */
    Result AddAssetEditorComponent(const NodeHandle& Owner, const TypeId& ComponentType);
    /**
     * @brief Remove a component from a node in the active hierarchical asset editor.
     * @param Owner Owner node handle.
     * @param ComponentType Reflected component type to remove by type.
     * @return Success or an error.
     */
    Result RemoveAssetEditorComponent(const NodeHandle& Owner, const TypeId& ComponentType);
    /**
     * @brief Advance active asset-editor session maintenance.
     * @param DeltaSeconds Variable-step frame delta in seconds.
     * @remarks
     * This keeps the active session title/bindings current and applies runtime-side synchronization
     * that cannot be expressed directly through reflected field writes.
     */
    void TickAssetEditorSession(float DeltaSeconds = 0.0f);
    /**
     * @brief Save the active asset-editor session.
     * @return Success or an error.
     * @remarks
     * This persists runtime payload overrides first, then import metadata changes, and finally updates
     * the session baseline used for future dirty-state comparisons.
     */
    Result SaveActiveAssetEditor();
    /**
     * @brief Save the active asset editor or Conduit document session.
     * @param Context Borrowed editor-service context.
     * @return Success or an error.
     */
    Result SaveActiveAssetEditor(EditorServiceContext& Context);
    /**
     * @brief Reimport the asset currently open in the asset editor.
     * @param Context Borrowed editor-service context.
     * @return Success or an error.
     * @warning Reimport is rejected while unsaved runtime payload overrides are present.
     */
    Result ReimportActiveAsset(EditorServiceContext& Context);
    /**
     * @brief Snapshot the active asset-editor session.
     * @return Value snapshot of the current session state.
     * @warning `TargetObject` and `ImportSettingsObject` inside the returned view are borrowed pointers.
     */
    [[nodiscard]] AssetEditorSessionView AssetEditorSession() const;
    /**
     * @brief Monotonic revision counter for asset-editor UI invalidation.
     * @return Revision number incremented when session-visible state changes.
     */
    [[nodiscard]] std::uint64_t AssetEditorSessionRevision() const { return m_assetEditorSessionRevision; }
    /**
     * @brief Mark the active asset-editor runtime document dirty after one reflected-object mutation.
     * @param RootType Reflected type of the mutated root object.
     * @param RootInstance Borrowed pointer to the mutated root object.
     * @remarks
     * The current implementation uses the active inspector context rather than strict pointer matching.
     * The parameters are preserved so callers can pass the actual mutated root and future validation
     * can be tightened without changing the UI callback contract.
     */
    void NotifyActiveAssetEditorRuntimeMutated(const TypeId& RootType, void* RootInstance);
    /**
     * @brief Mark the active asset-editor import-settings document dirty after one mutation.
     * @param RootType Reflected type of the mutated import-settings root object.
     * @param RootInstance Borrowed pointer to the mutated import-settings root object.
     */
    void NotifyActiveAssetEditorImportSettingsMutated(const TypeId& RootType, void* RootInstance);

    /**
     * @brief Instantiate the currently placement-armed asset into the active runtime world.
     * @param Context Borrowed editor-service context.
     * @return Success or an error.
     * @post On success, placement mode is cleared.
     */
    Result InstantiateArmedAsset(EditorServiceContext& Context, const AssetPlacementRequest& Request);
    /**
     * @brief Instantiate one asset into the active runtime world.
     * @param Context Borrowed editor-service context.
     * @param Key Discovery key of the asset to instantiate.
     * @return Success or an error.
     * @remarks Node, level, world, static-mesh, and texture assets are supported by this path.
     */
    Result InstantiateAssetByKey(EditorServiceContext& Context,
                                 std::string_view Key,
                                 const AssetPlacementRequest& Request);
    /**
     * @brief Create a new project on disk and load it immediately.
     * @param Context Borrowed editor-service context.
     * @param ProjectName New project name.
     * @param ParentDirectory Parent directory that will contain the project folder.
     * @return Success or an error.
     * @remarks
     * This creates the project directory, asset root, starter level source asset, template assets,
     * starter script, and project configuration file before forwarding to `LoadProject()`.
     */
    Result CreateProject(EditorServiceContext& Context, std::string_view ProjectName, std::string_view ParentDirectory);
    /**
     * @brief Create a new project workspace from a fully authored creation request.
     * @param Context Borrowed editor-service context.
     * @param Request Concrete project-creation request.
     * @param LoadAfterCreate `true` to load the created project immediately after scaffolding succeeds.
     * @param OutResult Optional filesystem/descriptor snapshot populated on success.
     * @return Success or an error.
     * @remarks
     * When the request leaves template-resource fields empty, editor-managed starter templates are
     * injected automatically so the wizard and the legacy simple-create flow stay aligned.
     */
    Result CreateProject(EditorServiceContext& Context,
                         const ProjectCreationRequest& Request,
                         bool LoadAfterCreate = true,
                         ProjectCreationResult* OutResult = nullptr);
    /**
     * @brief Create a new plugin workspace from a fully authored creation request.
     * @param Context Borrowed editor-service context.
     * @param Request Concrete plugin-creation request.
     * @param OutResult Optional filesystem/descriptor snapshot populated on success.
     * @return Success or an error.
     */
    Result CreatePlugin(EditorServiceContext& Context,
                        const PluginCreationRequest& Request,
                        PluginCreationResult* OutResult = nullptr);
    /**
     * @brief Add one new code module to the active or explicitly selected project workspace.
     * @param Context Borrowed editor-service context.
     * @param Request Concrete project-module-creation request.
     * @param OutResult Optional filesystem/descriptor snapshot populated on success.
     * @return Success or an error.
     * @remarks
     * When `Request.ProjectFilePath` is empty, the currently loaded project descriptor path is used.
     */
    Result CreateProjectModule(EditorServiceContext& Context,
                               const ModuleCreationRequest& Request,
                               ModuleCreationResult* OutResult = nullptr);
    /**
     * @brief Add one new code module to the explicitly selected plugin workspace.
     * @param Context Borrowed editor-service context.
     * @param Request Concrete plugin-module-creation request.
     * @param OutResult Optional filesystem/descriptor snapshot populated on success.
     * @return Success or an error.
     */
    Result CreatePluginModule(EditorServiceContext& Context,
                              const PluginModuleCreationRequest& Request,
                              PluginModuleCreationResult* OutResult = nullptr);
    /**
     * @brief Load an existing project file.
     * @param Context Borrowed editor-service context.
     * @param ProjectFilePath Project file path or resolvable URI.
     * @return Success or an error.
     * @remarks
     * Loading updates the active asset root, rebuilds the asset manager, loads the startup level,
     * and applies the project's default render settings asset when configured.
     */
    Result LoadProject(EditorServiceContext& Context, std::string_view ProjectFilePath);
    /**
     * @brief Persist editable project settings to the loaded project file.
     * @param Context Borrowed editor-service context.
     * @param ProjectName Updated project name. Empty keeps the current name.
     * @param StartupLevelAsset Updated startup level source-asset field. Empty keeps the current field.
     * @param DefaultRenderSettingsAssetId Updated default render settings asset id. Empty keeps the current value.
     * @return Success or an error.
     * @remarks Saving also reloads the project's default render settings asset.
     */
    Result SaveProjectSettings(EditorServiceContext& Context,
                               std::string_view ProjectName,
                               std::string_view StartupLevelAsset,
                               std::string_view DefaultRenderSettingsAssetId);
    /**
     * @brief Access the current project snapshot.
     * @return Borrowed project-info reference.
     */
    [[nodiscard]] const ProjectInfo& CurrentProject() const { return m_currentProject; }

    /**
     * @brief Access the last preview summary string.
     * @return Borrowed string reference.
     * @warning The returned reference is invalidated by later preview, selection, and status updates.
     */
    [[nodiscard]] const std::string& PreviewSummary() const { return m_previewSummary; }
    /**
     * @brief Access the latest human-readable status message.
     * @return Borrowed string reference.
     * @warning The returned reference is invalidated by later service operations.
     */
    [[nodiscard]] const std::string& StatusMessage() const;

private:
    [[nodiscard]] std::vector<std::string> BuildPackSearchPaths() const;
    [[nodiscard]] static std::vector<std::string> ParsePackSearchPathEnv(std::string_view Raw);
    [[nodiscard]] static std::string AssetKindToLabel(const ::SnAPI::AssetPipeline::TypeId& AssetKind);
    [[nodiscard]] const DiscoveredAsset* FindAssetByKey(std::string_view Key) const;
    [[nodiscard]] std::expected<std::string, std::string> ResolveOwningPackPath(
        const DiscoveredAsset& Asset) const;
    [[nodiscard]] std::expected<std::string, std::string> ResolveRuntimeSavePath(
        const DiscoveredAsset& Asset) const;
    [[nodiscard]] std::expected<::SnAPI::AssetPipeline::TypedPayload, std::string> BuildCookedPayloadForAsset(
        const DiscoveredAsset& Asset);
    Result InstantiateNodeAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset, const AssetPlacementRequest& Request);
    Result InstantiateLevelAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset, const AssetPlacementRequest& Request);
    Result InstantiateWorldAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset, const AssetPlacementRequest& Request);
    Result InstantiateStaticMeshAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset, const AssetPlacementRequest& Request);
    Result InstantiateTextureAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset, const AssetPlacementRequest& Request);
    Result RebuildAssetManager();
    Result EnsureEditorTemplateAssets(EditorServiceContext& Context);
    Result EnsureProjectShaderDirectory(const std::filesystem::path& ProjectAssetRoot);
    Result EnsureProjectStarterLevelAsset(const std::filesystem::path& ProjectAssetRoot,
                                         const std::filesystem::path& StartupAssetPath);
    Result LoadProjectStartupLevelAsset(EditorServiceContext& Context, const std::filesystem::path& StartupAssetPath);
    Result LoadProjectDefaultRenderSettings(EditorServiceContext& Context);
    [[nodiscard]] std::expected<::SnAPI::AssetPipeline::TypedPayload, std::string> SerializeAssetEditorPayload() const;
    [[nodiscard]] std::expected<std::string, std::string> SerializeAssetEditorSourceJson() const;
    struct AssetEditorDocumentState
    {
        std::uint64_t RuntimeRevision = 0;
        std::uint64_t SavedRuntimeRevision = 0;
        std::uint64_t ImportRevision = 0;
        std::uint64_t SavedImportRevision = 0;

        void Reset()
        {
            RuntimeRevision = 0;
            SavedRuntimeRevision = 0;
            ImportRevision = 0;
            SavedImportRevision = 0;
        }
    };
    [[nodiscard]] bool IsAssetEditorRuntimeDirty() const;
    [[nodiscard]] bool IsAssetEditorImportDirty() const;
    void RefreshAssetEditorDirtyFlags(bool RefreshDiscoveryState = false);
    [[nodiscard]] std::expected<void, std::string> SyncAssetEditorRuntimeOverrideFromCurrentState();
    void MarkAssetEditorRuntimeChanged(bool RefreshDiscoveryState = false);
    void MarkAssetEditorImportSettingsChanged(bool RefreshDiscoveryState = false);
    void MarkAssetEditorRuntimeSaved();
    void MarkAssetEditorImportSettingsSaved();
    [[nodiscard]] MaterialInstanceAsset* ResolveActiveMaterialInstanceEditorPayload();
    [[nodiscard]] const MaterialInstanceAsset* ResolveActiveMaterialInstanceEditorPayload() const;
    Result SyncMaterialInstanceEditorPayloadFromDescriptor();
    struct AssetImportMetadataEntry
    {
        EAssetImportProfile Profile = EAssetImportProfile::Unknown;
        std::string SourcePath{};
        std::string DestinationFolder{};
        std::string ImporterName{};
        std::unordered_map<std::string, std::string> BuildOptions{};
        AssimpImporterSettings Assimp{};
        TextureImporterSettings Texture{};
    };
    [[nodiscard]] std::filesystem::path ResolveImportMetadataPath() const;
    [[nodiscard]] std::expected<void, std::string> LoadAssetImportMetadataDatabase();
    [[nodiscard]] std::expected<void, std::string> SaveAssetImportMetadataDatabase() const;
    void InvalidateAssetRuntimeCache(const ::SnAPI::AssetPipeline::AssetId& AssetId);
    void InvalidateAssetRuntimeCaches(
        const std::vector<::SnAPI::AssetPipeline::AssetId>& AssetIds);
    [[nodiscard]] bool RefreshAssetEditorImportSettingsBinding(const DiscoveredAsset& Asset);
    static void ApplyImportedAssetProvenanceToMetadata(
        const ImportedAssetProvenancePayload& Provenance,
        AssetImportMetadataEntry& Metadata);
    [[nodiscard]] bool PopulateImportSettingsBindingFromActiveSourceAsset(AssetImportMetadataEntry& InOutMetadata);
    void ApplyAssetEditorImportSettingsToActiveSourceAsset();
    Result SyncImportedAssetGroupProvenance(const DiscoveredAsset& ActiveAsset, const AssetImportMetadataEntry& Metadata);
    [[nodiscard]] std::optional<AssetImportMetadataEntry> BuildAssetEditorImportMetadataFromCurrentState() const;
    [[nodiscard]] bool ImportMetadataRecordsEqual(const AssetImportMetadataEntry& Left, const AssetImportMetadataEntry& Right) const;
    [[nodiscard]] ::SnAPI::AssetPipeline::AssetImportSettingsPtr BuildTypedImportSettingsForRecord(
        const AssetImportMetadataEntry& Record) const;
    void ClearAssetEditorImportSettingsBinding();
    [[nodiscard]] BaseNode* ResolveAssetEditorNode(NodeHandle& InOutNode);
    [[nodiscard]] const BaseNode* ResolveAssetEditorNode(const NodeHandle& Node) const;
    [[nodiscard]] void* ResolveAssetEditorRuntimeTargetObject() const;
    [[nodiscard]] bool HasAssetEditorRuntimeTarget() const;
    void RefreshAssetEditorHierarchy();
    void ClearAssetEditorState();
    void MaybeReportStatusMessageToStdout() const;

    std::unique_ptr<::SnAPI::AssetPipeline::AssetManager> m_assetManager{};
    std::vector<DiscoveredAsset> m_assets{};
    std::unordered_map<std::string, std::size_t> m_assetIndexByKey{};
    std::unordered_map<::SnAPI::AssetPipeline::AssetId, std::string, ::SnAPI::AssetPipeline::UuidHash> m_assetRenameOverrides{};
    std::unordered_map<::SnAPI::AssetPipeline::AssetId, ::SnAPI::AssetPipeline::TypedPayload, ::SnAPI::AssetPipeline::UuidHash> m_assetPayloadOverrides{};
    std::string m_selectedAssetKey{};
    std::string m_placementAssetKey{};
    std::string m_previewSummary{};
    std::string m_statusMessage{};
    mutable std::string m_lastReportedStatusMessage{};
    std::filesystem::path m_editorTemplateAssetDirectory{};
    std::filesystem::path m_editorStarterLevelTemplateAssetPath{};
    std::filesystem::path m_editorStarterScriptTemplatePath{};
    ProjectInfo m_currentProject{};
    NodeHandle m_loadedDefaultRenderSettingsNode{};
    bool m_defaultRenderSettingsApplyPending = false;
    std::uint64_t m_defaultRenderSettingsLastFeatureRevision = 0;

    std::unique_ptr<::SnAPI::GameFramework::World> m_assetEditorWorld{};
    NodeHandle m_assetEditorRootHandle{};
    std::string m_assetEditorAssetKey{};
    ::SnAPI::AssetPipeline::AssetId m_assetEditorAssetId{};
    ::SnAPI::AssetPipeline::TypeId m_assetEditorAssetKind{};
    TypeId m_assetEditorTargetType{};
    void* m_assetEditorTargetObject = nullptr;
    TypeId m_assetEditorSourceAssetType{};
    bool m_assetEditorDirty = false;
    bool m_assetEditorRuntimeDirty = false;
    bool m_assetEditorCanSave = false;
    bool m_assetEditorCanEditHierarchy = false;
    std::optional<MaterialAsset> m_assetEditorMaterialPayload{};
    std::optional<MaterialInstanceAsset> m_assetEditorMaterialInstancePayload{};
    std::optional<TextureCompressorPlugin::TextureCompressorCookedInfo> m_assetEditorTextureCookedInfo{};
    std::optional<Editor::TextureAssetEditorPayload> m_assetEditorTexturePayload{};
    std::optional<StaticMeshPayload> m_assetEditorStaticMeshPayload{};
    std::optional<Editor::StaticMeshAssetEditorPayload> m_assetEditorStaticMeshEditorPayload{};
    std::optional<AssimpImporterSettings> m_assetEditorAssimpImportSettings{};
    std::optional<TextureImporterSettings> m_assetEditorTextureImportSettings{};
    TypeId m_assetEditorImportSettingsType{};
    void* m_assetEditorImportSettingsObject = nullptr;
    bool m_assetEditorImportSettingsDirty = false;
    bool m_assetEditorCanReimport = false;
    std::optional<AssetImportMetadataEntry> m_assetEditorImportMetadataBaseline{};
    std::string m_assetEditorMaterialInstanceDescriptorParentKey{};
    std::string m_assetEditorTitle{};
    NodeHandle m_assetEditorSelectedNode{};
    std::vector<AssetEditorSessionView::NodeEntry> m_assetEditorHierarchy{};
    bool m_assetEditorHierarchyDirty = false;
    AssetEditorDocumentState m_assetEditorDocumentState{};
    std::uint64_t m_assetEditorSessionRevision = 0;
    std::unordered_map<::SnAPI::AssetPipeline::AssetId, AssetImportMetadataEntry, ::SnAPI::AssetPipeline::UuidHash> m_assetImportMetadata{};
    std::filesystem::path m_assetImportMetadataPath{};
    bool m_assetImportMetadataDirty = false;
    std::unique_ptr<void, std::function<void(void*)>> m_assetEditorGenericSourceObject{nullptr, [](void*) {}};
};

} // namespace SnAPI::GameFramework::Editor
