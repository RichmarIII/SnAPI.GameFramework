# SnAPI::GameFramework::Editor::EditorTransformInteractionService

Handles transform-gizmo interaction for the current editor selection.

The service drives editor translation, rotation, and scaling for the selected node's `TransformComponent`. It consumes layout-configured gizmo space and snap settings, resolves axis picks from the editor overlay/id passes, and writes the resulting transform edits back to the live world.

Core semantics:
- Disabled while PIE is active.
- Requires a selected node, a transform component, an active render camera, a live game viewport, and a focused window before interaction can begin.
- `W`, `E`, and `R` switch translate/rotate/scale modes when the right mouse button is not held.
- Translation supports free-plane and axis-constrained movement.
- Rotation supports axis-constrained rotation and free yaw/pitch style rotation.
- Scale supports axis-constrained scaling and uniform scaling, clamping each component to a minimum of `0.001`.
- When snapping is enabled, translation, rotation, and scale deltas are quantized using the configured steps.

Threading model:
- Main-thread only.

## Private Types

<div class="snapi-api-card" markdown="1">
### `enum EActiveAxis`

**Values**

- `None`
- `X`
- `Y`
- `Z`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `EEditorTransformMode SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_mode`
</div>
<div class="snapi-api-card" markdown="1">
### `EditorLayout::EGizmoSpace SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_space`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_snapEnabled`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Math::Scalar SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_moveSnapStep`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Math::Scalar SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_rotateSnapDegrees`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Math::Scalar SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_scaleSnapStep`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_dragging`
</div>
<div class="snapi-api-card" markdown="1">
### `EActiveAxis SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_activeAxis`
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_lastMouseX`
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_lastMouseY`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Math::Scalar SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_rotateSnapRemainderPrimary`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Math::Scalar SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_rotateSnapRemainderSecondary`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_freeMovePlaneActive`
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_freeMovePlaneNormal`
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_freeMoveNodeStart`
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_freeMoveHitStart`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_axisMovePlaneActive`
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_axisMovePlaneNormal`
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_axisMoveAxisDirection`
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_axisMoveNodeStart`
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::Editor::EditorTransformInteractionService::m_axisMoveHitStart`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::EditorTransformInteractionService::Name() const override`

Service name used for diagnostics.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< std::type_index > SnAPI::GameFramework::Editor::EditorTransformInteractionService::Dependencies() const override`

Depends on scene, selection, PIE, and layout services.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorTransformInteractionService::Initialize(EditorServiceContext &Context) override`

Reset interaction state and gizmo bookkeeping.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorTransformInteractionService::Tick(EditorServiceContext &Context, float DeltaSeconds) override`

Poll hotkeys, manage drag interaction, and queue gizmo render objects.

**Parameters**

- `Context`: 
- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorTransformInteractionService::Shutdown(EditorServiceContext &Context) override`

Cancel active interaction and release any transient gizmo state.

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorTransformInteractionService::SetMode(EEditorTransformMode Mode)`

Set the active transform mode.

**Parameters**

- `Mode`:
</div>
<div class="snapi-api-card" markdown="1">
### `EEditorTransformMode SnAPI::GameFramework::Editor::EditorTransformInteractionService::Mode() const`

Current transform-gizmo mode.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorTransformInteractionService::SetSpace(EditorLayout::EGizmoSpace Space)`

Set the transform space used for the next interaction.

**Parameters**

- `Space`:
</div>
<div class="snapi-api-card" markdown="1">
### `EditorLayout::EGizmoSpace SnAPI::GameFramework::Editor::EditorTransformInteractionService::Space() const`

Current transform space.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorTransformInteractionService::SetSnappingEnabled(bool Enabled)`

Enable or disable transform snapping.

**Parameters**

- `Enabled`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorTransformInteractionService::SnappingEnabled() const`

Query whether transform snapping is enabled.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorTransformInteractionService::SetMoveSnapStep(SnAPI::Math::Scalar Step)`

Set the translation snap step in world units.

**Parameters**

- `Step`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Math::Scalar SnAPI::GameFramework::Editor::EditorTransformInteractionService::MoveSnapStep() const`

Translation snap step in world units.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorTransformInteractionService::SetRotateSnapDegrees(SnAPI::Math::Scalar Degrees)`

Set the rotation snap increment in degrees.

**Parameters**

- `Degrees`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Math::Scalar SnAPI::GameFramework::Editor::EditorTransformInteractionService::RotateSnapDegrees() const`

Rotation snap increment in degrees.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorTransformInteractionService::SetScaleSnapStep(SnAPI::Math::Scalar Step)`

Set the scale snap step in scalar units.

**Parameters**

- `Step`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Math::Scalar SnAPI::GameFramework::Editor::EditorTransformInteractionService::ScaleSnapStep() const`

Scale snap step in scalar units.
</div>
