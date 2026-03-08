#pragma once

#include "Editor/EditorLayout.h"
#include "Editor/EditorSceneBootstrap.h"
#include "Editor/EditorSelectionModel.h"
#include "Editor/EditorTheme.h"
#include "Editor/EditorViewportBinding.h"
#include "Editor/EditorAssetService.h"
#include "Editor/IEditorService.h"
#include "Serialization.h"
#include "World.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace SnAPI::UI
{
class UIContext;
struct PointerEvent;
struct UIPoint;
struct UIRect;
} // namespace SnAPI::UI

namespace SnAPI::GameFramework
{
class BaseNode;
class CameraComponent;
struct NodeTransform;
class UIRenderViewport;
} // namespace SnAPI::GameFramework

namespace SnAPI::Graphics
{
class ICamera;
class IRenderObject;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Undoable editor command contract.
 *
 * `IEditorCommand` is the unit of work stored by `EditorCommandService`. Implementations are
 * expected to fully describe both the forward mutation and the reverse mutation for one editor
 * action such as selection changes, hierarchy edits, or property adjustments.
 *
 * Core semantics:
 * - `Execute()` applies the command's forward mutation.
 * - `Undo()` applies the reverse mutation.
 * - The same command instance is reused for execute/undo/redo cycles while it remains in history.
 *
 * Ownership and lifetime:
 * - Commands are heap-allocated and owned by `EditorCommandService` after successful execution.
 * - A command may keep internal snapshots required for undo/redo, but should not assume external
 *   object pointers remain valid unless it re-resolves them safely.
 *
 * Threading model:
 * - Main-thread only.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API IEditorCommand
{
public:
    virtual ~IEditorCommand() = default;
    /** @brief Stable command name for diagnostics and UI. @return Borrowed string view. */
    [[nodiscard]] virtual std::string_view Name() const = 0;
    /**
     * @brief Apply the command's forward mutation.
     * @param Context Borrowed editor-service context.
     * @return Success or an error.
     */
    virtual Result Execute(EditorServiceContext& Context) = 0;
    /**
     * @brief Apply the command's reverse mutation.
     * @param Context Borrowed editor-service context.
     * @return Success or an error.
     */
    virtual Result Undo(EditorServiceContext& Context) = 0;
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Central undo and redo service for editor mutations.
 *
 * `EditorCommandService` owns the command-history stacks used by the editor shell.
 * It executes commands immediately, pushes successful commands onto the undo stack,
 * clears redo history on new forward execution, and replays the same command instances
 * when users request undo or redo.
 *
 * Core semantics:
 * - A command enters history only if `Execute()` succeeds.
 * - `Undo()` pops from the undo stack and pushes onto the redo stack only if reversal succeeds.
 * - `Redo()` re-executes the command and moves it back to the undo stack only if replay succeeds.
 * - Oldest undo entries are discarded when history reaches `m_maxHistory`.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see IEditorCommand
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorCommandService final : public IEditorService
{
public:
    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /**
     * @brief Priority hint for service initialization.
     * @return A very low value so command history is ready before higher-level editor services begin using it.
     */
    [[nodiscard]] int Priority() const override;
    /** @brief Reset command history for a fresh editor session. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Clear command history during shutdown. */
    void Shutdown(EditorServiceContext& Context) override;

    /**
     * @brief Execute and record a command.
     * @param Context Borrowed editor-service context.
     * @param Command Owning pointer to the command to execute.
     * @return Success or an error.
     * @warning Ownership transfers to the history only when execution succeeds.
     */
    Result Execute(EditorServiceContext& Context, std::unique_ptr<IEditorCommand> Command);
    /**
     * @brief Undo the most recently executed command.
     * @param Context Borrowed editor-service context.
     * @return Success or an error.
     */
    Result Undo(EditorServiceContext& Context);
    /**
     * @brief Redo the most recently undone command.
     * @param Context Borrowed editor-service context.
     * @return Success or an error.
     */
    Result Redo(EditorServiceContext& Context);

    /** @brief Query whether an undo operation is currently available. */
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
    /** @brief Query whether a redo operation is currently available. */
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }
    /** @brief Current undo-stack depth. */
    [[nodiscard]] std::size_t UndoCount() const { return m_undoStack.size(); }
    /** @brief Current redo-stack depth. */
    [[nodiscard]] std::size_t RedoCount() const { return m_redoStack.size(); }
    /** @brief Drop both undo and redo history stacks. */
    void ClearHistory();

private:
    std::vector<std::unique_ptr<IEditorCommand>> m_undoStack{};
    std::vector<std::unique_ptr<IEditorCommand>> m_redoStack{};
    std::size_t m_maxHistory = 256;
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Picking backend strategy used by selection interaction.
 *
 * `Auto` currently prefers renderer id-buffer picking, then falls back to physics raycast,
 * and finally to selecting the active camera owner when no scene hit can be resolved.
 */
enum class EEditorPickingBackend : std::uint8_t
{
    Auto = 0, /**< @brief Try renderer id-buffer picking first, then physics, then active-camera fallback. */
    PhysicsRaycast, /**< @brief Use a world-space physics raycast against the current editor camera. */
    ActiveCameraOwner, /**< @brief Skip scene picking and select the node that owns the active editor camera. */
    RendererIdBuffer /**< @brief Use the editor id pass and per-pixel render-object ids. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Active transform-gizmo mode for editor interaction.
 */
enum class EEditorTransformMode : std::uint8_t
{
    Translate = 0, /**< @brief Move the selected node in world space, object space, or camera space. */
    Rotate, /**< @brief Rotate the selected node around one axis or free-rotate relative to camera basis. */
    Scale /**< @brief Scale the selected node uniformly or per axis. */
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Service that owns and exposes the active editor theme.
 *
 * This service centralizes theme lifetime so layout code can safely borrow a single `EditorTheme`
 * instance during build and rebuild operations.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorThemeService final : public IEditorService
{
public:
    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /** @brief Initialize the owned `EditorTheme`. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Shutdown hook. The owned theme object remains value-owned by the service. */
    void Shutdown(EditorServiceContext& Context) override;

    /** @brief Access the owned editor theme. @return Borrowed theme reference. */
    [[nodiscard]] EditorTheme& Theme() { return m_theme; }
    /** @brief Access the owned editor theme. @return Borrowed theme reference. */
    [[nodiscard]] const EditorTheme& Theme() const { return m_theme; }

private:
    EditorTheme m_theme{};
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Service that owns bootstrap scene creation and active camera tracking.
 *
 * The service wraps `EditorSceneBootstrap` so other editor services can depend on a stable API for:
 * - ensuring an editor camera exists
 * - querying the active camera component
 * - keeping the renderer's active camera synchronized with the world
 *
 * @see EditorSceneBootstrap
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorSceneService final : public IEditorService
{
public:
    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /** @brief Build or refresh the editor bootstrap scene. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Refresh active-camera tracking each frame. */
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    /** @brief Destroy tracked bootstrap nodes. */
    void Shutdown(EditorServiceContext& Context) override;
    /**
     * @brief Ensure an editor camera exists in the current runtime world.
     * @param Context Borrowed editor-service context.
     * @return Success or an error.
     */
    Result EnsureEditorCamera(EditorServiceContext& Context);

    /** @brief Access the currently tracked active camera component. @return Non-owning pointer or `nullptr`. */
    [[nodiscard]] CameraComponent* ActiveCameraComponent() const;
    /** @brief Access the currently tracked render camera interface. @return Non-owning pointer or `nullptr`. */
    [[nodiscard]] SnAPI::Graphics::ICamera* ActiveRenderCamera() const;

private:
    EditorSceneBootstrap m_scene{};
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Service that owns and resizes the root editor viewport binding.
 *
 * This is the service wrapper around `EditorViewportBinding`. It keeps the explicit root viewport
 * alive and synchronized with the current runtime window for the duration of the editor session.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorRootViewportService final : public IEditorService
{
public:
    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /** @brief Create the root editor viewport binding. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Propagate window-size and UI-binding changes into the explicit root viewport. */
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    /** @brief Destroy the explicit root viewport binding. */
    void Shutdown(EditorServiceContext& Context) override;

private:
    EditorViewportBinding m_binding{};
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Service that owns the shared editor selection model.
 *
 * The service keeps the logical selection valid against runtime world churn. When the current
 * selection no longer resolves, it falls back to the active editor camera owner before clearing.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorSelectionService final : public IEditorService
{
public:
    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /** @brief Hard dependency on `EditorSceneService` so camera fallback is available. */
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    /** @brief Reset selection and validate an initial fallback selection. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Keep the stored selection handle synchronized with the live world. */
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    /** @brief Clear the selection during shutdown. */
    void Shutdown(EditorServiceContext& Context) override;

    /** @brief Access the owned selection model. @return Borrowed reference. */
    [[nodiscard]] EditorSelectionModel& Model() { return m_selection; }
    /** @brief Access the owned selection model. @return Borrowed reference. */
    [[nodiscard]] const EditorSelectionModel& Model() const { return m_selection; }

private:
    void EnsureSelectionValid(EditorServiceContext& Context, CameraComponent* ActiveCamera);

    EditorSelectionModel m_selection{};
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Service that manages Play-In-Editor world session lifecycle.
 *
 * `EditorPieService` snapshots the current editor world, rehydrates that snapshot into a PIE-flavored
 * world instance, optionally starts gameplay, and restores the original editor snapshot when PIE stops.
 *
 * Core semantics:
 * - `Play()` starts a fresh session from the current editor world or resumes a paused one.
 * - `Pause()` swaps the world to a paused execution profile that disables gameplay, physics, audio, and networking pumps.
 * - `Stop()` restores the serialized editor snapshot and original world kind/profile.
 * - PIE loads regenerate object ids so the running play session is isolated from the editor snapshot.
 *
 * Ownership and lifetime:
 * - The service owns only the serialized editor snapshot; the runtime world remains owned by `GameRuntime`.
 *
 * @warning Starting PIE stops any current gameplay host first and may restart it inside the PIE world.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorPieService final : public IEditorService
{
public:
    /**
     * @brief High-level PIE session state.
     */
    enum class EState : std::uint8_t
    {
        Stopped = 0, /**< @brief No PIE session is active and the editor world is live. */
        Playing, /**< @brief PIE world is active and using the normal PIE execution profile. */
        Paused, /**< @brief PIE world is active but running the paused execution profile. */
    };

    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /** @brief Reset PIE state for a fresh editor session. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Stop any active PIE session and drop the stored snapshot. */
    void Shutdown(EditorServiceContext& Context) override;

    /** @brief Start or resume PIE. @return Success or an error. */
    Result Play(EditorServiceContext& Context);
    /** @brief Pause an active PIE session. @return Success or an error. */
    Result Pause(EditorServiceContext& Context);
    /** @brief Stop PIE and restore the editor snapshot. @return Success or an error. */
    Result Stop(EditorServiceContext& Context);

    /** @brief Current PIE state. */
    [[nodiscard]] EState State() const { return m_state; }
    /** @brief Query whether PIE is actively playing. */
    [[nodiscard]] bool IsPlaying() const { return m_state == EState::Playing; }
    /** @brief Query whether PIE is currently paused. */
    [[nodiscard]] bool IsPaused() const { return m_state == EState::Paused; }
    /** @brief Query whether any PIE session is currently active. */
    [[nodiscard]] bool IsSessionActive() const { return m_state != EState::Stopped; }

private:
    Result StartSession(EditorServiceContext& Context);
    Result ResumeSession(EditorServiceContext& Context);
    Result StopSession(EditorServiceContext& Context);
    [[nodiscard]] static WorldExecutionProfile PausedExecutionProfile();

    EState m_state = EState::Stopped;
    std::optional<WorldPayload> m_editorSnapshot{};
    EWorldKind m_editorWorldKind = EWorldKind::Editor;
    WorldExecutionProfile m_editorExecutionProfile{};
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Resolves icon metadata and UI texture bindings for content browser assets.
 *
 * This service converts `EditorAssetService::DiscoveredAsset` records into lightweight icon metadata
 * that `EditorLayoutService` can hand directly to UI widgets. For plain asset kinds it returns
 * fallback icon identifiers. For texture-backed assets it also manages transient external UI texture
 * registrations scoped to one `UIContext`.
 *
 * Core semantics:
 * - Texture thumbnail bindings are created lazily when an asset icon is resolved inside a specific UI context.
 * - Changing the bound UI context invalidates every existing thumbnail binding because external texture ids are
 *   context-local.
 * - `Revision()` increments whenever icon bindings change so higher-level UI code can cheaply decide whether it
 *   needs to refresh rendered content.
 *
 * Ownership and lifetime:
 * - The service owns the external texture bindings it allocates.
 * - Returned `AssetIconMetadata` values are copies; their `TextureId` remains meaningful only while the same
 *   UI context is current and the underlying binding has not been invalidated.
 *
 * Threading model:
 * - Main-thread only.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorAssetIconService final : public IEditorService
{
public:
    ~EditorAssetIconService() override;

    /**
     * @brief Resolved icon payload for one content-browser entry.
     *
     * `IconSource` names the logical fallback icon while the texture fields describe an optional
     * external UI texture binding for thumbnail rendering.
     */
    struct AssetIconMetadata
    {
        std::string IconSource{}; /**< @brief Logical icon identifier used when no texture thumbnail is available. */
        std::uint32_t TextureId = 0; /**< @brief UI-context-local external texture id, or `0` when no thumbnail binding exists. */
        std::uint32_t TextureWidth = 0; /**< @brief Width of the external thumbnail texture in pixels. */
        std::uint32_t TextureHeight = 0; /**< @brief Height of the external thumbnail texture in pixels. */
    };

    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /** @brief Hard dependency on `EditorAssetService` so discovered assets and load helpers are available. */
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    /** @brief Reset cached bindings for a fresh editor session. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Release all external texture bindings. */
    void Shutdown(EditorServiceContext& Context) override;

    /**
     * @brief Synchronize the cached icon-binding set with the currently visible asset list.
     * @param Context Borrowed editor-service context.
     * @param Assets Borrowed view of the asset entries that should remain icon-resolvable.
     * @param UiContext Borrowed UI context that will consume the resulting texture ids, or `nullptr` to force fallback-only behavior.
     *
     * The service drops bindings for assets that are no longer present and fully resets when the
     * UI context changes.
     */
    void Synchronize(EditorServiceContext& Context,
                     const std::vector<EditorAssetService::DiscoveredAsset>& Assets,
                     const SnAPI::UI::UIContext* UiContext);
    /**
     * @brief Invalidate one asset's cached icon data.
     * @param Context Borrowed editor-service context.
     * @param AssetKey Stable discovered-asset key.
     *
     * Use this after saves, reimports, or deletes that can change the asset's preview.
     */
    void InvalidateAsset(EditorServiceContext& Context, std::string_view AssetKey);
    /**
     * @brief Resolve icon metadata for one discovered asset.
     * @param Context Borrowed editor-service context.
     * @param Asset Borrowed discovered-asset description.
     * @param UiContext Borrowed UI context that will render the icon.
     * @return A value payload containing fallback icon source and optional thumbnail texture binding.
     * @warning Returned `TextureId` values are not portable across UI contexts.
     */
    [[nodiscard]] AssetIconMetadata ResolveAssetIcon(EditorServiceContext& Context,
                                                     const EditorAssetService::DiscoveredAsset& Asset,
                                                     const SnAPI::UI::UIContext* UiContext);

    /** @brief Monotonic revision counter for icon-binding invalidation. */
    [[nodiscard]] std::uint64_t Revision() const { return m_revision; }

private:
    struct TextureBinding;

    [[nodiscard]] AssetIconMetadata BuildFallbackIcon(const EditorAssetService::DiscoveredAsset& Asset) const;
    [[nodiscard]] std::uint32_t AllocateTextureId();
    void RemoveBinding(EditorServiceContext& Context, std::string_view AssetKey);
    void ResetAllBindings(EditorServiceContext& Context);

    const SnAPI::UI::UIContext* m_boundContext = nullptr;
    std::unordered_map<std::string, std::shared_ptr<TextureBinding>> m_textureBindingsByAssetKey{};
    std::uint32_t m_nextTextureId = 0x70000000u;
    std::uint64_t m_revision = 1;
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Builds and synchronizes the editor shell UI layout.
 *
 * `EditorLayoutService` is the bridge between service-layer state and the concrete `EditorLayout`
 * widget tree. It constructs the shell once the required editor services exist, translates UI
 * callbacks into queued requests, and applies those requests during its own `Tick()` so the rest of
 * the editor observes deterministic main-thread ordering.
 *
 * Core semantics:
 * - Layout event handlers never mutate editor state directly; they queue requests onto this service.
 * - `Tick()` drains queued project, asset, hierarchy, inspector, and toolbar actions in a stable order.
 * - Asset browser and inspector state are only pushed into the layout when the relevant signatures or
 *   icon/session revisions change, keeping steady-state UI churn low.
 * - Project load/create success forces an editor-camera refresh, clears selection and command history,
 *   and requests a layout rebuild so the shell reflects the new project state.
 *
 * Ownership and lifetime:
 * - The service value-owns the `EditorLayout`.
 * - Returned pointers such as `GameViewportElement()` are borrowed and remain valid only while the
 *   layout is built and has not been rebuilt or shut down.
 *
 * Threading model:
 * - Main-thread only.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorLayoutService final : public IEditorService
{
public:
    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /**
     * @brief Hard dependencies on the theme, scene, selection, PIE, viewport, command, asset, and icon services.
     * @return Exact-type dependency list.
     */
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    /** @brief Build the initial editor shell and install layout delegates. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Drain queued UI requests and synchronize layout state for the current frame. */
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    /** @brief Destroy the layout and clear pending requests. */
    void Shutdown(EditorServiceContext& Context) override;
    /** @brief Access the live game-viewport UI element. @return Non-owning pointer or `nullptr` if no layout is built. */
    [[nodiscard]] UIRenderViewport* GameViewportElement() const;
    /** @brief Index of the active game-view tab container entry, or a negative value if unavailable. */
    [[nodiscard]] int32_t GameViewportTabIndex() const;
    /** @brief Current gizmo-space selection as chosen in the editor tools UI. */
    [[nodiscard]] EditorLayout::EGizmoSpace GizmoSpace() const;
    /** @brief Query whether transform snapping is enabled in the tools UI. */
    [[nodiscard]] bool GizmoSnappingEnabled() const;
    /** @brief Current translation snap step in world units. */
    [[nodiscard]] double MoveSnapStep() const;
    /** @brief Current rotation snap step in degrees. */
    [[nodiscard]] double RotateSnapStepDegrees() const;
    /** @brief Current scale snap step in scalar units. */
    [[nodiscard]] double ScaleSnapStep() const;

private:
    void ApplyAssetBrowserState(EditorServiceContext& Context);
    void QueueLayoutRebuild() { m_layoutRebuildRequested = true; }
    void RebuildLayout(EditorServiceContext& Context);

    EditorLayout m_layout{};
    bool m_hasPendingSelectionRequest = false;
    NodeHandle m_pendingSelectionRequest{};
    bool m_hasPendingHierarchyActionRequest = false;
    EditorLayout::HierarchyActionRequest m_pendingHierarchyActionRequest{};
    bool m_hasPendingToolbarAction = false;
    EditorLayout::EToolbarAction m_pendingToolbarAction = EditorLayout::EToolbarAction::Play;
    bool m_hasPendingProjectActionRequest = false;
    EditorLayout::ProjectActionRequest m_pendingProjectActionRequest{};
    bool m_hasPendingAssetSelection = false;
    bool m_pendingAssetSelectionDoubleClick = false;
    std::string m_pendingAssetSelectionKey{};
    bool m_hasPendingAssetPlaceRequest = false;
    std::string m_pendingAssetPlaceKey{};
    bool m_hasPendingAssetSaveRequest = false;
    std::string m_pendingAssetSaveKey{};
    bool m_hasPendingAssetDeleteRequest = false;
    std::string m_pendingAssetDeleteKey{};
    bool m_hasPendingAssetRenameRequest = false;
    std::string m_pendingAssetRenameKey{};
    std::string m_pendingAssetRenameValue{};
    bool m_hasPendingAssetRefreshRequest = false;
    bool m_hasPendingAssetCreateRequest = false;
    EditorLayout::ContentAssetCreateRequest m_pendingAssetCreateRequest{};
    bool m_hasPendingAssetImportRequest = false;
    EditorLayout::ContentAssetImportRequest m_pendingAssetImportRequest{};
    bool m_hasPendingAssetInspectorSaveRequest = false;
    bool m_hasPendingAssetInspectorReimportRequest = false;
    bool m_hasPendingAssetInspectorCloseRequest = false;
    bool m_hasPendingAssetInspectorNodeSelectionRequest = false;
    NodeHandle m_pendingAssetInspectorNodeSelection{};
    bool m_hasPendingAssetInspectorHierarchyActionRequest = false;
    EditorLayout::HierarchyActionRequest m_pendingAssetInspectorHierarchyActionRequest{};
    bool m_layoutRebuildRequested = false;
    std::size_t m_assetListSignature = 0;
    std::size_t m_assetDetailsSignature = 0;
    std::uint64_t m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t m_assetInspectorIconRevision = std::numeric_limits<std::uint64_t>::max();
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Renders game-viewport overlays inside the viewport-owned UI context.
 *
 * The service creates lightweight HUD elements in the `UIRenderViewport` overlay context rather than
 * in the root editor shell. That keeps overlay rendering spatially scoped to the viewport and allows
 * it to survive layout sync without coupling the overlay widgets to the main shell tree.
 *
 * Current behavior:
 * - The HUD graph is active and samples frame-time / FPS data.
 * - Profiler-panel state exists but is currently kept collapsed unless the active tab changes to the
 *   dedicated profiler view.
 * - Overlay elements are rebuilt when the viewport-owned context id changes.
 *
 * Threading model:
 * - Main-thread only.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorGameViewportOverlayService final : public IEditorService
{
public:
    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /** @brief Depends on `EditorLayoutService` because the viewport UI element is sourced from the layout. */
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    /** @brief Reset overlay bookkeeping for a fresh session. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Ensure overlay widgets exist in the current viewport context and refresh sampled data. */
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    /** @brief Destroy overlay widget handles and forget the bound overlay context. */
    void Shutdown(EditorServiceContext& Context) override;

private:
    void ResetOverlayState();
    bool EnsureOverlayElements(SnAPI::UI::UIContext& OverlayContext);
    void UpdateOverlayVisibility(SnAPI::UI::UIContext& OverlayContext, int32_t ActiveTabIndex);
    void UpdateOverlaySamples(SnAPI::UI::UIContext& OverlayContext, float DeltaSeconds);

    std::uint64_t m_overlayContextId = 0;
    SnAPI::UI::ElementId m_hudPanel{};
    SnAPI::UI::ElementId m_hudGraph{};
    SnAPI::UI::ElementId m_hudFrameLabel{};
    SnAPI::UI::ElementId m_hudFpsLabel{};
    std::uint32_t m_hudFrameSeries = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t m_hudFpsSeries = std::numeric_limits<std::uint32_t>::max();

    SnAPI::UI::ElementId m_profilerPanel{};
    SnAPI::UI::ElementId m_profilerGraph{};
    SnAPI::UI::ElementId m_profilerFrameLabel{};
    SnAPI::UI::ElementId m_profilerFpsLabel{};
    std::uint32_t m_profilerFrameSeries = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t m_profilerFpsSeries = std::numeric_limits<std::uint32_t>::max();
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Handles viewport pointer interaction and updates logical editor selection.
 *
 * This service binds a pointer-event handler to the active editor game viewport and translates mouse
 * clicks into selection changes, placement actions, or PIE mouse-capture transitions.
 *
 * Core semantics:
 * - Outside PIE, click selection is delayed until pointer release so the service can distinguish click
 *   from drag with a small pixel threshold.
 * - If asset placement is armed, placement is attempted before normal selection resolution.
 * - Selection changes are executed through `EditorCommandService`, which makes them undoable.
 * - `Auto` picking currently resolves hits in this order: renderer id buffer, physics raycast, then
 *   active-camera owner fallback.
 * - During PIE, pointer presses inside the viewport enable mouse capture instead of performing editor selection.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning If the renderer id buffer reports a non-zero object id that cannot be mapped back to a node,
 * the current selection is intentionally left unchanged.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorSelectionInteractionService final : public IEditorService
{
public:
    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /** @brief Depends on scene, selection, layout, command, PIE, and asset services. */
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    /** @brief Bind initial viewport interaction hooks. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Keep viewport bindings current and queue selected-node overlay geometry when appropriate. */
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    /** @brief Unbind viewport interaction hooks and clear transient pointer state. */
    void Shutdown(EditorServiceContext& Context) override;
    /** @brief Override the picking backend strategy used for click resolution. */
    void SetPickingBackend(EEditorPickingBackend Backend) { m_backend = Backend; }
    /** @brief Current picking backend strategy. */
    [[nodiscard]] EEditorPickingBackend PickingBackend() const { return m_backend; }

private:
    void RebindViewportHandler(EditorServiceContext& Context);
    void HandleViewportPointerEvent(EditorServiceContext& Context,
                                    const SnAPI::UI::PointerEvent& Event,
                                    std::uint32_t RoutedTypeId,
                                    bool ContainsPointer);
    void UpdatePieMouseCaptureState(EditorServiceContext& Context);
    void SetPieMouseCapture(EditorServiceContext& Context, bool CaptureEnabled);
    void QueueSelectedNodeEditorOverlay(EditorServiceContext& Context) const;
    bool TryResolvePickedNode(EditorServiceContext& Context, const SnAPI::UI::UIPoint& ScreenPoint, NodeHandle& OutNode) const;
    bool TryResolvePickedNodePhysics(EditorServiceContext& Context,
                                     const SnAPI::UI::UIPoint& ScreenPoint,
                                     NodeHandle& OutNode) const;
    bool TryResolvePickedNodeRendererId(EditorServiceContext& Context,
                                        const SnAPI::UI::UIPoint& ScreenPoint,
                                        NodeHandle& OutNode) const;
    bool TryResolvePickedNodeActiveCamera(EditorServiceContext& Context, NodeHandle& OutNode) const;

    IEditorServiceHost* m_host = nullptr;
    EEditorPickingBackend m_backend = EEditorPickingBackend::Auto;

    UIRenderViewport* m_boundViewport = nullptr;
    bool m_pointerPressedInside = false;
    bool m_pointerDragged = false;
    SnAPI::UI::UIPoint m_pointerPressPosition{};
    bool m_pieMouseCaptureEnabled = false;
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Handles transform-gizmo interaction for the current editor selection.
 *
 * The service drives editor translation, rotation, and scaling for the selected node's
 * `TransformComponent`. It consumes layout-configured gizmo space and snap settings, resolves axis
 * picks from the editor overlay/id passes, and writes the resulting transform edits back to the live world.
 *
 * Core semantics:
 * - Disabled while PIE is active.
 * - Requires a selected node, a transform component, an active render camera, a live game viewport, and
 *   a focused window before interaction can begin.
 * - `W`, `E`, and `R` switch translate/rotate/scale modes when the right mouse button is not held.
 * - Translation supports free-plane and axis-constrained movement.
 * - Rotation supports axis-constrained rotation and free yaw/pitch style rotation.
 * - Scale supports axis-constrained scaling and uniform scaling, clamping each component to a minimum of `0.001`.
 * - When snapping is enabled, translation, rotation, and scale deltas are quantized using the configured steps.
 *
 * Threading model:
 * - Main-thread only.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorTransformInteractionService final : public IEditorService
{
public:
    /** @brief Service name used for diagnostics. */
    [[nodiscard]] std::string_view Name() const override;
    /** @brief Depends on scene, selection, PIE, and layout services. */
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    /** @brief Reset interaction state and gizmo bookkeeping. */
    Result Initialize(EditorServiceContext& Context) override;
    /** @brief Poll hotkeys, manage drag interaction, and queue gizmo render objects. */
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    /** @brief Cancel active interaction and release any transient gizmo state. */
    void Shutdown(EditorServiceContext& Context) override;

    /** @brief Set the active transform mode. */
    void SetMode(EEditorTransformMode Mode) { m_mode = Mode; }
    /** @brief Current transform-gizmo mode. */
    [[nodiscard]] EEditorTransformMode Mode() const { return m_mode; }
    /** @brief Set the transform space used for the next interaction. */
    void SetSpace(EditorLayout::EGizmoSpace Space) { m_space = Space; }
    /** @brief Current transform space. */
    [[nodiscard]] EditorLayout::EGizmoSpace Space() const { return m_space; }
    /** @brief Enable or disable transform snapping. */
    void SetSnappingEnabled(bool Enabled) { m_snapEnabled = Enabled; }
    /** @brief Query whether transform snapping is enabled. */
    [[nodiscard]] bool SnappingEnabled() const { return m_snapEnabled; }
    /** @brief Set the translation snap step in world units. */
    void SetMoveSnapStep(SnAPI::Math::Scalar Step) { m_moveSnapStep = Step; }
    /** @brief Translation snap step in world units. */
    [[nodiscard]] SnAPI::Math::Scalar MoveSnapStep() const { return m_moveSnapStep; }
    /** @brief Set the rotation snap increment in degrees. */
    void SetRotateSnapDegrees(SnAPI::Math::Scalar Degrees) { m_rotateSnapDegrees = Degrees; }
    /** @brief Rotation snap increment in degrees. */
    [[nodiscard]] SnAPI::Math::Scalar RotateSnapDegrees() const { return m_rotateSnapDegrees; }
    /** @brief Set the scale snap step in scalar units. */
    void SetScaleSnapStep(SnAPI::Math::Scalar Step) { m_scaleSnapStep = Step; }
    /** @brief Scale snap step in scalar units. */
    [[nodiscard]] SnAPI::Math::Scalar ScaleSnapStep() const { return m_scaleSnapStep; }

private:
    enum class EActiveAxis : std::uint8_t
    {
        None = 0,
        X,
        Y,
        Z
    };

#if defined(SNAPI_GF_ENABLE_RENDERER)
    void EnsureGizmoRenderObjects();
    void ConfigureGizmoGeometryForMode();
    void QueueTransformGizmos(EditorServiceContext& Context,
                              BaseNode* SelectedNode,
                              const NodeTransform& SelectedTransform,
                              SnAPI::Graphics::ICamera& Camera,
                              std::uint64_t ViewportID);
    [[nodiscard]] EActiveAxis PickGizmoAxis(EditorServiceContext& Context,
                                            float ScreenX,
                                            float ScreenY,
                                            const SnAPI::UI::UIRect& ViewRect,
                                            std::uint64_t ViewportID) const;
#endif

    EEditorTransformMode m_mode = EEditorTransformMode::Translate;
    EditorLayout::EGizmoSpace m_space = EditorLayout::EGizmoSpace::World;
    bool m_snapEnabled = false;
    SnAPI::Math::Scalar m_moveSnapStep = static_cast<SnAPI::Math::Scalar>(1.0);
    SnAPI::Math::Scalar m_rotateSnapDegrees = static_cast<SnAPI::Math::Scalar>(15.0);
    SnAPI::Math::Scalar m_scaleSnapStep = static_cast<SnAPI::Math::Scalar>(0.5);
    bool m_dragging = false;
    EActiveAxis m_activeAxis = EActiveAxis::None;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
    SnAPI::Math::Scalar m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
    SnAPI::Math::Scalar m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
    bool m_freeMovePlaneActive = false;
    Vec3 m_freeMovePlaneNormal = Vec3::UnitZ();
    Vec3 m_freeMoveNodeStart = Vec3::Zero();
    Vec3 m_freeMoveHitStart = Vec3::Zero();
    bool m_axisMovePlaneActive = false;
    Vec3 m_axisMovePlaneNormal = Vec3::UnitZ();
    Vec3 m_axisMoveAxisDirection = Vec3::UnitX();
    Vec3 m_axisMoveNodeStart = Vec3::Zero();
    Vec3 m_axisMoveHitStart = Vec3::Zero();
#if defined(SNAPI_GF_ENABLE_RENDERER)
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisX{};
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisY{};
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisZ{};
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisXAux{};
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisYAux{};
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisZAux{};
    std::uint32_t m_gizmoAxisXID = 0;
    std::uint32_t m_gizmoAxisYID = 0;
    std::uint32_t m_gizmoAxisZID = 0;
    std::uint32_t m_gizmoAxisXAuxID = 0;
    std::uint32_t m_gizmoAxisYAuxID = 0;
    std::uint32_t m_gizmoAxisZAuxID = 0;
    EEditorTransformMode m_gizmoGeometryMode = EEditorTransformMode::Translate;
#endif
};

} // namespace SnAPI::GameFramework::Editor
