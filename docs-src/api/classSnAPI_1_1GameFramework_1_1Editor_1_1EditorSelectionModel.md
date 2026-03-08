# SnAPI::GameFramework::Editor::EditorSelectionModel

Lightweight node-selection state shared across editor views.

`EditorSelectionModel` stores one selected `NodeHandle` and resolves it lazily against a world. It exists so inspectors, hierarchy panels, and viewport tools can share the same logical selection without requiring a hard reference to a live `BaseNode`.

Core semantics:
- Selection is stored as a handle, not a raw pointer.
- `ResolveSelectedNode()` first tries the embedded handle, then world lookup by object id, and finally a slower UUID-based fallback.
- The model does not own the selected node and does not keep it alive.

Threading model:
- Main-thread only.

## Private Members

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorSelectionModel::m_selectedNode`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorSelectionModel::SelectedNode() const`

Access the currently stored selection handle.

**Returns:** Copy of the stored handle. May be null.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorSelectionModel::HasSelection() const`

Query whether any selection is currently stored.

**Returns:** `true` when `SelectedNode()` is non-null.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorSelectionModel::SelectNode(const NodeHandle &Node)`

Replace the stored selection handle.

**Parameters**

- `Node`: New selection handle. May be null to represent "no selection".

**Returns:** `true` when the stored selection changed, `false` when the same handle was already selected.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorSelectionModel::Clear()`

Clear the stored selection.
</div>
<div class="snapi-api-card" markdown="1">
### `BaseNode * SnAPI::GameFramework::Editor::EditorSelectionModel::ResolveSelectedNode(World &WorldRef) const`

Resolve the selected node against a mutable world.

**Parameters**

- `WorldRef`: World used for id-based fallback resolution.

**Returns:** Non-owning pointer to the selected node, or `nullptr` when the selection no longer resolves.
</div>
<div class="snapi-api-card" markdown="1">
### `const BaseNode * SnAPI::GameFramework::Editor::EditorSelectionModel::ResolveSelectedNode(const World &WorldRef) const`

Resolve the selected node against a const world.

**Parameters**

- `WorldRef`: World used for id-based fallback resolution.

**Returns:** Non-owning pointer to the selected node, or `nullptr` when the selection no longer resolves.
</div>
