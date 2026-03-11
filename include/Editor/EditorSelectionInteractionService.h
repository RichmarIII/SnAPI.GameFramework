#pragma once

#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"
#include "Handles.h"

#include <UILayout.h>

#include <cstdint>

namespace SnAPI::UI
{
struct PointerEvent;
}

namespace SnAPI::GameFramework
{
class UIRenderViewport;
}

namespace SnAPI::GameFramework::Editor
{

enum class EEditorPickingBackend : std::uint8_t
{
    Auto = 0,
    PhysicsRaycast,
    ActiveCameraOwner,
    RendererIdBuffer
};

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

} // namespace SnAPI::GameFramework::Editor
