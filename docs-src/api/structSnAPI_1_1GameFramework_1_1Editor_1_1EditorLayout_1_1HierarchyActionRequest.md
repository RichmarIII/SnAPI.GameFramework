# SnAPI::GameFramework::Editor::EditorLayout::HierarchyActionRequest

Payload describing one hierarchy action request.

## Public Members

<div class="snapi-api-card" markdown="1">
### `EHierarchyAction SnAPI::GameFramework::Editor::EditorLayout::HierarchyActionRequest::Action`

Requested hierarchy action.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorLayout::HierarchyActionRequest::TargetNode`

Target node for the action, when applicable.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::HierarchyActionRequest::TargetIsWorldRoot`

`true` when the action conceptually targets the world root rather than a concrete node.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorLayout::HierarchyActionRequest::Type`

Reflected node or component type associated with add/remove requests.
</div>
