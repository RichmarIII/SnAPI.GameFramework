# SnAPI::GameFramework::Editor::IEditorService

Contract for modular editor subsystems.

`IEditorService` is the extension point used by `GameEditor` to assemble editor behavior from independent modules. A service typically owns one focused concern such as selection, scene bootstrapping, layout, asset management, or viewport binding.

Core semantics:
- Services are registered by concrete type.
- `Dependencies()` declares hard initialization requirements on other service types.
- `Priority()` breaks ties only among services whose dependencies are already satisfied.
- `Initialize()` is called at most once per registration lifetime.
- `Shutdown()` is called before removal or editor shutdown if initialization succeeded.

Ownership and lifetime:
- Services are owned by `GameEditor`.
- Services may keep borrowed references to runtime/world state only while initialized.

Threading model:
- Main-thread only.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::Editor::IEditorService::~IEditorService()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::string_view SnAPI::GameFramework::Editor::IEditorService::Name() const =0`

Stable service name for diagnostics and error reporting.

**Returns:** Borrowed string view. Implementations typically return static storage.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual std::vector< std::type_index > SnAPI::GameFramework::Editor::IEditorService::Dependencies() const`

Hard dependencies required before this service may initialize.

**Returns:** List of exact concrete service types.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual int SnAPI::GameFramework::Editor::IEditorService::Priority() const`

Ordering hint among services whose dependencies are already satisfied.

**Returns:** Signed priority value; lower values initialize earlier.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual Result SnAPI::GameFramework::Editor::IEditorService::Initialize(EditorServiceContext &Context)=0`

Initialize service state.

**Parameters**

- `Context`: Borrowed execution context for runtime and peer-service access.

**Returns:** Success or an initialization error.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::Editor::IEditorService::Tick(EditorServiceContext &Context, float DeltaSeconds)`

Per-frame update hook.

**Parameters**

- `Context`: Borrowed execution context.
- `DeltaSeconds`: Variable-step frame delta in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::Editor::IEditorService::Shutdown(EditorServiceContext &Context)=0`

Shutdown and release service state.

**Parameters**

- `Context`: Borrowed execution context.
</div>
