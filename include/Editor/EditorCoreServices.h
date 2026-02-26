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

namespace SnAPI::UI
{
class UIContext;
struct PointerEvent;
struct UIPoint;
} // namespace SnAPI::UI

namespace SnAPI::GameFramework
{
class CameraComponent;
class UIRenderViewport;
} // namespace SnAPI::GameFramework

namespace SnAPI::Graphics
{
class ICamera;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework::Editor
{

/**
 * @brief Command contract used by `EditorCommandService`.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API IEditorCommand
{
public:
    virtual ~IEditorCommand() = default;
    [[nodiscard]] virtual std::string_view Name() const = 0;
    virtual Result Execute(EditorServiceContext& Context) = 0;
    virtual Result Undo(EditorServiceContext& Context) = 0;
};

/**
 * @brief Central undo/redo service for editor mutations.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorCommandService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] int Priority() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Shutdown(EditorServiceContext& Context) override;

    Result Execute(EditorServiceContext& Context, std::unique_ptr<IEditorCommand> Command);
    Result Undo(EditorServiceContext& Context);
    Result Redo(EditorServiceContext& Context);

    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }
    [[nodiscard]] std::size_t UndoCount() const { return m_undoStack.size(); }
    [[nodiscard]] std::size_t RedoCount() const { return m_redoStack.size(); }
    void ClearHistory();

private:
    std::vector<std::unique_ptr<IEditorCommand>> m_undoStack{};
    std::vector<std::unique_ptr<IEditorCommand>> m_redoStack{};
    std::size_t m_maxHistory = 256;
};

/**
 * @brief Picking backend strategy used by selection interaction.
 */
enum class EEditorPickingBackend : std::uint8_t
{
    Auto = 0,
    PhysicsRaycast,
    ActiveCameraOwner,
    RendererIdBuffer
};

enum class EEditorTransformMode : std::uint8_t
{
    Translate = 0,
    Rotate,
    Scale
};

/**
 * @brief Provides the active editor UI theme.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorThemeService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Shutdown(EditorServiceContext& Context) override;

    [[nodiscard]] EditorTheme& Theme() { return m_theme; }
    [[nodiscard]] const EditorTheme& Theme() const { return m_theme; }

private:
    EditorTheme m_theme{};
};

/**
 * @brief Owns the bootstrap editor camera and tracks active runtime camera component.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorSceneService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;

    [[nodiscard]] CameraComponent* ActiveCameraComponent() const;
    [[nodiscard]] SnAPI::Graphics::ICamera* ActiveRenderCamera() const;

private:
    EditorSceneBootstrap m_scene{};
};

/**
 * @brief Owns and resizes the root editor render viewport binding.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorRootViewportService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;

private:
    EditorViewportBinding m_binding{};
};

/**
 * @brief Owns selected-node editor state.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorSelectionService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;

    [[nodiscard]] EditorSelectionModel& Model() { return m_selection; }
    [[nodiscard]] const EditorSelectionModel& Model() const { return m_selection; }

private:
    void EnsureSelectionValid(EditorServiceContext& Context, CameraComponent* ActiveCamera);

    EditorSelectionModel m_selection{};
};

/**
 * @brief Manages Play-In-Editor world session lifecycle.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorPieService final : public IEditorService
{
public:
    enum class EState : std::uint8_t
    {
        Stopped = 0,
        Playing,
        Paused,
    };

    [[nodiscard]] std::string_view Name() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Shutdown(EditorServiceContext& Context) override;

    Result Play(EditorServiceContext& Context);
    Result Pause(EditorServiceContext& Context);
    Result Stop(EditorServiceContext& Context);

    [[nodiscard]] EState State() const { return m_state; }
    [[nodiscard]] bool IsPlaying() const { return m_state == EState::Playing; }
    [[nodiscard]] bool IsPaused() const { return m_state == EState::Paused; }
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
 * @brief Builds and synchronizes the editor shell UI layout.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorLayoutService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;
    [[nodiscard]] UIRenderViewport* GameViewportElement() const;
    [[nodiscard]] int32_t GameViewportTabIndex() const;

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
    bool m_hasPendingAssetInspectorSaveRequest = false;
    bool m_hasPendingAssetInspectorCloseRequest = false;
    bool m_hasPendingAssetInspectorNodeSelectionRequest = false;
    NodeHandle m_pendingAssetInspectorNodeSelection{};
    bool m_hasPendingAssetInspectorHierarchyActionRequest = false;
    EditorLayout::HierarchyActionRequest m_pendingAssetInspectorHierarchyActionRequest{};
    bool m_layoutRebuildRequested = false;
    std::size_t m_assetListSignature = 0;
    std::size_t m_assetDetailsSignature = 0;
    std::uint64_t m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
};

/**
 * @brief Renders game-viewport overlays (HUD stats + profiler panel) inside the viewport-owned UI context.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorGameViewportOverlayService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
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
 * @brief Handles viewport click interaction and updates editor selection.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorSelectionInteractionService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;
    void SetPickingBackend(EEditorPickingBackend Backend) { m_backend = Backend; }
    [[nodiscard]] EEditorPickingBackend PickingBackend() const { return m_backend; }

private:
    void RebindViewportHandler(EditorServiceContext& Context);
    void HandleViewportPointerEvent(EditorServiceContext& Context,
                                    const SnAPI::UI::PointerEvent& Event,
                                    std::uint32_t RoutedTypeId,
                                    bool ContainsPointer);
    void UpdatePieMouseCaptureState(EditorServiceContext& Context);
    void SetPieMouseCapture(EditorServiceContext& Context, bool CaptureEnabled);
    bool TryResolvePickedNode(EditorServiceContext& Context, const SnAPI::UI::UIPoint& ScreenPoint, NodeHandle& OutNode) const;
    bool TryResolvePickedNodePhysics(EditorServiceContext& Context,
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

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorTransformInteractionService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;

    void SetMode(EEditorTransformMode Mode) { m_mode = Mode; }
    [[nodiscard]] EEditorTransformMode Mode() const { return m_mode; }

private:
    EEditorTransformMode m_mode = EEditorTransformMode::Translate;
    bool m_dragging = false;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
};

} // namespace SnAPI::GameFramework::Editor
