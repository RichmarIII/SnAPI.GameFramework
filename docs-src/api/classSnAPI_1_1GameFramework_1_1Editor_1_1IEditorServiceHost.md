# SnAPI::GameFramework::Editor::IEditorServiceHost

Internal host contract consumed by `EditorServiceContext`.

`IEditorServiceHost` abstracts the minimum services the editor runtime must provide so `EditorServiceContext` can remain decoupled from `GameEditor`. External code typically should not implement or consume this interface directly unless it is providing an alternate editor host.

Ownership and lifetime:
- Implementations own the runtime and registered services they expose.
- Returned pointers are non-owning and follow the host's shutdown/unregister lifetime.

Threading model:
- Main-thread only.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::Editor::IEditorServiceHost::~IEditorServiceHost()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::GameRuntime & SnAPI::GameFramework::Editor::IEditorServiceHost::RuntimeForServices()=0`

Access the runtime used for editor service execution.

**Returns:** Borrowed runtime reference.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const SnAPI::GameFramework::GameRuntime & SnAPI::GameFramework::Editor::IEditorServiceHost::RuntimeForServices() const =0`

Access the runtime used for editor service execution.

**Returns:** Borrowed runtime reference.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual IEditorService * SnAPI::GameFramework::Editor::IEditorServiceHost::ResolveServiceForContext(const std::type_index &Type)=0`

Resolve a registered service by exact concrete type.

**Parameters**

- `Type`: 

**Returns:** Non-owning pointer or `nullptr`.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual const IEditorService * SnAPI::GameFramework::Editor::IEditorServiceHost::ResolveServiceForContext(const std::type_index &Type) const =0`

Resolve a registered service by exact concrete type.

**Parameters**

- `Type`: 

**Returns:** Non-owning pointer or `nullptr`.
</div>
