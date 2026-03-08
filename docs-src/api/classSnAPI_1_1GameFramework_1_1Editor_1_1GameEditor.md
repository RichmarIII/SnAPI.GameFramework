# SnAPI::GameFramework::Editor::GameEditor

High-level editor host layered on top of `GameRuntime`.

`GameEditor` is the application-facing entry point for the editor module. It owns one `GameRuntime`, registers and orders editor services, and coordinates the special bootstrap work needed to make editor-only scene content safe to initialize.

Core semantics:
- `Initialize()` resets any previous session, initializes the runtime, then initializes editor services.
- `Update()` ticks editor services before forwarding the frame to `GameRuntime::Update()`.
- Service initialization obeys dependency order first and `Priority()` second.
- `UnregisterService()` removes the target service and any transitive dependents.

Bootstrap ordering:
- During editor module startup the host defers node/component `OnCreate` work until the editor viewport and related UI bindings have had a chance to materialize.
- This keeps editor-authored scene bootstrap nodes from running render-dependent setup before viewports and pass graphs are ready.

Ownership and lifetime:
- `GameEditor` owns the runtime and every registered service instance.
- References returned by `Runtime()`, `Settings()`, and `GetService()` are borrowed.
- Borrowed service pointers become invalid after unregistration or `Shutdown()`.

Threading model:
- Main-thread only.

## Contents

- **Type:** SnAPI::GameFramework::Editor::GameEditor::ServiceEntry

## Private Members

<div class="snapi-api-card" markdown="1">
### `GameEditorSettings SnAPI::GameFramework::Editor::GameEditor::m_settings`
</div>
<div class="snapi-api-card" markdown="1">
### `GameRuntime SnAPI::GameFramework::Editor::GameEditor::m_runtime`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<ServiceEntry> SnAPI::GameFramework::Editor::GameEditor::m_services`
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<std::type_index, std::size_t> SnAPI::GameFramework::Editor::GameEditor::m_serviceIndexByType`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::size_t> SnAPI::GameFramework::Editor::GameEditor::m_serviceOrder`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::GameEditor::m_defaultServicesRegistered`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::GameEditor::m_initialized`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::GameEditor::Initialize(const GameEditorSettings &Settings)`

Initialize editor runtime.

**Parameters**

- `Settings`: 

**Returns:** Success or an initialization error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::GameEditor::Shutdown()`

Shutdown editor runtime.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::GameEditor::IsInitialized() const`

Check whether editor runtime is initialized.

**Returns:** `true` when runtime and editor services completed initialization.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::GameEditor::Update(float DeltaSeconds)`

Update one frame.

**Parameters**

- `DeltaSeconds`: Frame delta time in seconds.

**Returns:** `true` to continue running; `false` when runtime requests exit.
</div>
<div class="snapi-api-card" markdown="1">
### `GameRuntime & SnAPI::GameFramework::Editor::GameEditor::Runtime()`

Mutable access to wrapped `GameRuntime`.

**Returns:** Borrowed runtime reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const GameRuntime & SnAPI::GameFramework::Editor::GameEditor::Runtime() const`

Const access to wrapped `GameRuntime`.

**Returns:** Borrowed runtime reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const GameEditorSettings & SnAPI::GameFramework::Editor::GameEditor::Settings() const`

Access the last applied settings snapshot.

**Returns:** Borrowed settings reference.
</div>
<div class="snapi-api-card" markdown="1">
### `TService & SnAPI::GameFramework::Editor::GameEditor::RegisterService(TArgs &&... Args)`

Register a concrete editor service type.

**Parameters**

- `Args`: Constructor arguments forwarded into the new service.

**Returns:** Borrowed reference to the existing or newly created service.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::GameEditor::RegisterService(std::unique_ptr< IEditorService > Service)`

Register a runtime-provided service instance.

**Parameters**

- `Service`: Owning pointer to the service instance to adopt.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::GameEditor::UnregisterService(const std::type_index &ServiceType)`

Unregister a registered service type.

**Parameters**

- `ServiceType`: Exact concrete service type to remove.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::GameEditor::UnregisterService()`

Unregister a registered service type.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `TService * SnAPI::GameFramework::Editor::GameEditor::GetService()`

Query a registered service by type.

**Returns:** Non-owning pointer or `nullptr` when the service is not registered.
</div>
<div class="snapi-api-card" markdown="1">
### `const TService * SnAPI::GameFramework::Editor::GameEditor::GetService() const`

Query a registered service by type (const).

**Returns:** Non-owning pointer or `nullptr` when the service is not registered.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::GameEditor::InitializeRuntime(const GameEditorSettings &Settings)`

**Parameters**

- `Settings`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::GameEditor::EnsureDefaultServicesRegistered()`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::GameEditor::BuildServiceOrder()`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::GameEditor::InitializeServices()`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::GameEditor::FinalizeBootstrapLifecycle()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::GameEditor::TickServices(float DeltaSeconds)`

**Parameters**

- `DeltaSeconds`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::GameEditor::ShutdownServices()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::GameEditor::RebuildServiceIndexByType()`
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::GameEditor::InitializeEditorModules()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::GameEditor::ShutdownEditorModules()`
</div>
<div class="snapi-api-card" markdown="1">
### `GameRuntime & SnAPI::GameFramework::Editor::GameEditor::RuntimeForServices() override`

Access the runtime used for editor service execution.

**Returns:** Borrowed runtime reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const GameRuntime & SnAPI::GameFramework::Editor::GameEditor::RuntimeForServices() const override`

Access the runtime used for editor service execution.

**Returns:** Borrowed runtime reference.
</div>
<div class="snapi-api-card" markdown="1">
### `IEditorService * SnAPI::GameFramework::Editor::GameEditor::ResolveServiceForContext(const std::type_index &Type) override`

Resolve a registered service by exact concrete type.

**Parameters**

- `Type`: 

**Returns:** Non-owning pointer or `nullptr`.
</div>
<div class="snapi-api-card" markdown="1">
### `const IEditorService * SnAPI::GameFramework::Editor::GameEditor::ResolveServiceForContext(const std::type_index &Type) const override`

Resolve a registered service by exact concrete type.

**Parameters**

- `Type`: 

**Returns:** Non-owning pointer or `nullptr`.
</div>
