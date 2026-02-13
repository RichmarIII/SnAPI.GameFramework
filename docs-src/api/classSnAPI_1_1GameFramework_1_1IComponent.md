# SnAPI::GameFramework::IComponent

Runtime contract for attachable node behavior/data units.

Ownership and lifetime:
- Stored/owned by typed component storages (`TComponentStorage<T>`).
- Addressable by UUID (`ComponentHandle`) through `ObjectRegistry`.
- Destruction is deferred to end-of-frame to keep handles stable within a frame.

Execution context:
- `OwnerNode()` and `World()` are resolved dynamically through handle/graph links.
- Role helpers (`IsServer`/`IsClient`/`IsListenServer`) proxy world networking state.

## Private Members

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::IComponent::m_owner`

Owning node identity; resolved via ObjectRegistry when needed.
</div>
<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::IComponent::m_id`

Stable component identity used for handles/replication/serialization.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::IComponent::m_typeId`

Reflected concrete component type id used by RPC/serialization paths.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::IComponent::m_replicated`

Runtime replication gate for this component instance.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `virtual SnAPI::GameFramework::IComponent::~IComponent()=default`

Virtual destructor for interface.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponent::OnCreate()`

Called immediately after component creation.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponent::OnDestroy()`

Called just before component destruction.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponent::Tick(float DeltaSeconds)`

Per-frame update hook.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponent::FixedTick(float DeltaSeconds)`

Fixed-step update hook.

**Parameters**

- `DeltaSeconds`: Fixed time step.
</div>
<div class="snapi-api-card" markdown="1">
### `virtual void SnAPI::GameFramework::IComponent::LateTick(float DeltaSeconds)`

Late update hook.

**Parameters**

- `DeltaSeconds`: Time since last tick.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::IComponent::Owner(NodeHandle InOwner)`

Set the owning node handle.

**Parameters**

- `InOwner`: Owner node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::IComponent::Owner() const`

Get the owning node handle.

**Returns:** Owner node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::IComponent::Replicated() const`

Check if the component is replicated over the network.

**Returns:** True if replicated.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::IComponent::Replicated(bool Replicated)`

Set whether the component is replicated over the network.

**Parameters**

- `Replicated`:
</div>
<div class="snapi-api-card" markdown="1">
### `const Uuid & SnAPI::GameFramework::IComponent::Id() const`

Get the component UUID.

**Returns:** UUID of this component.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::IComponent::Id(Uuid Id)`

Set the component UUID.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::IComponent::TypeKey() const`

Get the reflected type id for this component.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::IComponent::TypeKey(const TypeId &Id)`

Set the reflected type id for this component.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `ComponentHandle SnAPI::GameFramework::IComponent::Handle() const`

Get a handle for this component.

**Returns:** ComponentHandle wrapping the UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `BaseNode * SnAPI::GameFramework::IComponent::OwnerNode() const`

Resolve the owning node pointer.

**Returns:** Owning BaseNode pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `IWorld * SnAPI::GameFramework::IComponent::World() const`

Resolve the owning world pointer.

**Returns:** Owning world or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::IComponent::IsServer() const`

Check whether this component executes with server authority.

**Returns:** True when server-authoritative.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::IComponent::IsClient() const`

Check whether this component executes in a client context.

**Returns:** True when client-side.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::IComponent::IsListenServer() const`

Check whether this component executes as listen-server.

**Returns:** True when both server and client role are active.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::IComponent::CallRPC(std::string_view MethodName, std::span< const Variant > Args={})`

Dispatch a reflected RPC method for this component.

**Parameters**

- `MethodName`: Reflected method name.
- `Args`: Variant-packed arguments.

**Returns:** True when dispatch succeeded (local invoke or queued network call).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::IComponent::CallRPC(std::string_view MethodName, std::initializer_list< Variant > Args)`

Initializer-list convenience overload for `CallRPC`.

**Parameters**

- `MethodName`: 
- `Args`:
</div>
