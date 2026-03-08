# SnAPI::GameFramework::Editor::EditorServiceContext

Lightweight execution context passed to editor services.

`EditorServiceContext` is the narrow bridge between a service and the editor host. It intentionally exposes only:
- the owning `GameRuntime`
- service lookup through the hosting `GameEditor`

The context exists so services can cooperate without directly depending on the concrete `GameEditor` implementation or reaching through global state.

Ownership and lifetime:
- The context stores a non-owning pointer to the active `IEditorServiceHost`.
- It is created transiently by the host during initialize/tick/shutdown calls.
- Pointers returned from `GetService()` remain borrowed and are invalidated if the referenced service is unregistered or the editor shuts down.

Threading model:
- Main-thread only.

## Private Members

<div class="snapi-api-card" markdown="1">
### `IEditorServiceHost* SnAPI::GameFramework::Editor::EditorServiceContext::m_host`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Editor::EditorServiceContext::EditorServiceContext(IEditorServiceHost &Host)`

**Parameters**

- `Host`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::GameRuntime & SnAPI::GameFramework::Editor::EditorServiceContext::Runtime()`

Access the runtime owned by the hosting editor.

**Returns:** Borrowed runtime reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const SnAPI::GameFramework::GameRuntime & SnAPI::GameFramework::Editor::EditorServiceContext::Runtime() const`

Access the runtime owned by the hosting editor.

**Returns:** Borrowed runtime reference.
</div>
<div class="snapi-api-card" markdown="1">
### `IEditorServiceHost & SnAPI::GameFramework::Editor::EditorServiceContext::Host() const`

Access the service host that created this context.

**Returns:** Borrowed host reference.
</div>
<div class="snapi-api-card" markdown="1">
### `TService * SnAPI::GameFramework::Editor::EditorServiceContext::GetService()`

Resolve another registered editor service by exact concrete type.

**Returns:** Non-owning pointer to the matching service, or `nullptr` when no such service is registered.
</div>
<div class="snapi-api-card" markdown="1">
### `const TService * SnAPI::GameFramework::Editor::EditorServiceContext::GetService() const`

Resolve another registered editor service by exact concrete type.

**Returns:** Non-owning pointer to the matching service, or `nullptr` when no such service is registered.
</div>
