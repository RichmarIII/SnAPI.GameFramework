# SnAPI::GameFramework::Editor::EditorWorld

`World` specialization configured for editor execution.

`EditorWorld` exists so tools can opt into a predictable editor execution profile without repeating the same world-kind and subsystem-policy setup at every call site.

Core semantics:
- `EWorldKind` is forced to `Editor`.
- `WorldExecutionProfile::Editor()` is applied during construction.
- Gameplay orchestration, autonomous physics stepping, audio pumping, and networking simulation are disabled by default, while non-simulating queries remain available.

Ownership and lifetime:
- Same as `World`; this type adds no extra ownership rules.

Threading model:
- Follows `World`: main-thread mutation unless a narrower subsystem contract states otherwise.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Editor::EditorWorld::EditorWorld()`

Construct an editor world with the default name `"EditorWorld"`.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Editor::EditorWorld::EditorWorld(std::string Name)`

Construct an editor world with a caller-provided name.

**Parameters**

- `Name`:
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorWorld::ApplyEditorDefaults()`
</div>
