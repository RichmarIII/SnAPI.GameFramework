#pragma once

#include "Editor/EditorLayout.h"
#include "Editor/EditorSceneBootstrap.h"
#include "Editor/EditorSelectionModel.h"
#include "Editor/EditorTheme.h"
#include "Editor/EditorViewportBinding.h"
#include "Editor/IEditorService.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace SnAPI::UI
{
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

private:
    EditorLayout m_layout{};
    bool m_hasPendingSelectionRequest = false;
    NodeHandle m_pendingSelectionRequest{};
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
