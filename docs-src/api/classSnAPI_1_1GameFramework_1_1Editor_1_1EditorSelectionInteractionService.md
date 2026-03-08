# SnAPI::GameFramework::Editor::EditorSelectionInteractionService

Handles viewport pointer interaction and updates logical editor selection.

This service binds a pointer-event handler to the active editor game viewport and translates mouse clicks into selection changes, placement actions, or PIE mouse-capture transitions.

Core semantics:
- Outside PIE, click selection is delayed until pointer release so the service can distinguish click from drag with a small pixel threshold.
- If asset placement is armed, placement is attempted before normal selection resolution.
- Selection changes are executed through `EditorCommandService`, which makes them undoable.
- `Auto` picking currently resolves hits in this order: renderer id buffer, physics raycast, then active-camera owner fallback.
- During PIE, pointer presses inside the viewport enable mouse capture instead of performing editor selection.

Threading model:
- Main-thread only.

## Private Members

<div class="snapi-api-card" markdown="1">
### `IEditorServiceHost* SnAPI::GameFramework::Editor::EditorSelectionInteractionService::m_host`
</div>
<div class="snapi-api-card" markdown="1">
### `EEditorPickingBackend SnAPI::GameFramework::Editor::EditorSelectionInteractionService::m_backend`
</div>
<div class="snapi-api-card" markdown="1">
### `UIRenderViewport* SnAPI::GameFramework::Editor::EditorSelectionInteractionService::m_boundViewport`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorSelectionInteractionService::m_pointerPressedInside`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorSelectionInteractionService::m_pointerDragged`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::UIPoint SnAPI::GameFramework::Editor::EditorSelectionInteractionService::m_pointerPressPosition`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorSelectionInteractionService::m_pieMouseCaptureEnabled`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorSelectionInteractionService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< std::type_index > SnAPI::GameFramework::Editor::EditorSelectionInteractionService::Dependencies() const override`

Depends on scene, selection, layout, command, PIE, and asset services.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorSelectionInteractionService::Initialize(EditorServiceContext &Context) override`

Bind initial viewport interaction hooks.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionInteractionService::Tick(EditorServiceContext &Context, float DeltaSeconds) override`

Keep viewport bindings current and queue selected-node overlay geometry when appropriate.

**Parameters**

- `Context`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionInteractionService::Shutdown(EditorServiceContext &Context) override`

Unbind viewport interaction hooks and clear transient pointer state.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionInteractionService::SetPickingBackend(EEditorPickingBackend Backend)`

Override the picking backend strategy used for click resolution.

**Parameters**

- `Backend`:
</div>
<div class="snapi-api-card" markdown="1">
### `EEditorPickingBackend SnAPI::GameFramework::Editor::EditorSelectionInteractionService::PickingBackend() const`

Current picking backend strategy.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionInteractionService::RebindViewportHandler(EditorServiceContext &Context)`

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionInteractionService::HandleViewportPointerEvent(EditorServiceContext &Context, const SnAPI::UI::PointerEvent &Event, std::uint32_t RoutedTypeId, bool ContainsPointer)`

**Parameters**

- `Context`: 
- `Event`: 
- `RoutedTypeId`: 
- `ContainsPointer`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionInteractionService::UpdatePieMouseCaptureState(EditorServiceContext &Context)`

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionInteractionService::SetPieMouseCapture(EditorServiceContext &Context, bool CaptureEnabled)`

**Parameters**

- `Context`: 
- `CaptureEnabled`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionInteractionService::QueueSelectedNodeEditorOverlay(EditorServiceContext &Context) const`

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorSelectionInteractionService::TryResolvePickedNode(EditorServiceContext &Context, const SnAPI::UI::UIPoint &ScreenPoint, NodeHandle &OutNode) const`

**Parameters**

- `Context`: 
- `ScreenPoint`: 
- `OutNode`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorSelectionInteractionService::TryResolvePickedNodePhysics(EditorServiceContext &Context, const SnAPI::UI::UIPoint &ScreenPoint, NodeHandle &OutNode) const`

**Parameters**

- `Context`: 
- `ScreenPoint`: 
- `OutNode`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorSelectionInteractionService::TryResolvePickedNodeRendererId(EditorServiceContext &Context, const SnAPI::UI::UIPoint &ScreenPoint, NodeHandle &OutNode) const`

**Parameters**

- `Context`: 
- `ScreenPoint`: 
- `OutNode`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorSelectionInteractionService::TryResolvePickedNodeActiveCamera(EditorServiceContext &Context, NodeHandle &OutNode) const`

**Parameters**

- `Context`: 
- `OutNode`:
</div>
