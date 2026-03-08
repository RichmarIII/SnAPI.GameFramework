# SnAPI::GameFramework::BaseComponent

Canonical base type for runtime components attached to nodes.

A `BaseComponent` models attachable gameplay data or behavior that participates in the lifecycle of an owning node. Components are identified independently from their node so they can be serialized, replicated, and targeted by reflection/RPC systems without using raw pointers as the public contract.

Why this type exists:
- to keep reusable behavior/data separate from hierarchy objects
- to give each attachment a stable identity and reflected type
- to let the world own lifecycle, replication, and tick dispatch uniformly

Ownership and lifetime:
- Components are owned by world-managed runtime storage, not by callers.
- `Owner()` and `OwnerNode()` are non-owning links back to the attaching node.
- Destruction is typically deferred until end-of-frame so handles remain stable during the active frame.
- `OnCreate()` delivery may be temporarily suppressed during bootstrap and replayed once the world is ready.

Threading model:
- Main-thread only for attachment, mutation, and lifecycle callbacks unless a derived type documents otherwise.
- Borrowed owner/world pointers are not synchronized for concurrent use.

Invariants:
- `TypeKey()` must identify the concrete reflected component type.
- `Id()` is the stable public identity used by handles, replication, and serialization.
- `Owner()` is null only when the component is detached or not yet fully initialized.

## Private Members

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::BaseComponent::m_owner`

Owning node identity; resolved via ObjectRegistry when needed.
</div>
<div class="snapi-api-card" markdown="1">
### `BaseNode* SnAPI::GameFramework::BaseComponent::m_ownerNode`

Cached owner node pointer to avoid repeated registry resolution.
</div>
<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::BaseComponent::m_id`

Stable component identity used for handles/replication/serialization.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::BaseComponent::m_runtimePoolToken`

Runtime pool token for fast handle resolution.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::BaseComponent::m_runtimeIndex`

Runtime pool slot index for fast handle resolution.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::BaseComponent::m_runtimeGeneration`

Runtime pool slot generation for stale-handle rejection.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::BaseComponent::m_typeId`

Reflected concrete component type id used by RPC/serialization paths.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseComponent::m_active`

Runtime tick gate for this component instance.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseComponent::m_replicated`

Runtime replication gate for this component instance.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::BaseComponent::~BaseComponent()=default`

Destructor.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::OnCreate()`

Component construction lifecycle hook.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::OnDestroy()`

Component destruction lifecycle hook.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::PreTick(float DeltaSeconds)`

Early variable-step update hook.

**Parameters**

- `DeltaSeconds`: Time since last tick in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::Tick(float DeltaSeconds)`

Primary variable-step update hook.

**Parameters**

- `DeltaSeconds`: Time since last tick in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::FixedTick(float DeltaSeconds)`

Fixed-step update hook.

**Parameters**

- `DeltaSeconds`: Fixed time step in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::LateTick(float DeltaSeconds)`

Late update hook.

**Parameters**

- `DeltaSeconds`: Time since last tick in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::PostTick(float DeltaSeconds)`

Post update hook.

**Parameters**

- `DeltaSeconds`: Time since last tick in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::Owner(const NodeHandle &InOwner)`

Set the owning node handle.

**Parameters**

- `InOwner`: Owner node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::BaseComponent::Owner() const`

Get the owning node handle.

**Returns:** Owner node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseComponent::Active() const`

Check if this component is active for tick execution.

**Returns:** True when tick hooks are enabled.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::Active(bool ActiveValue)`

Set component active state for tick execution.

**Parameters**

- `ActiveValue`: New active state.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseComponent::Replicated() const`

Check if the component is replicated over the network.

**Returns:** True if replicated.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::Replicated(bool Replicated)`

Set whether the component is replicated over the network.

**Parameters**

- `Replicated`:
</div>
<div class="snapi-api-card" markdown="1">
### `const Uuid & SnAPI::GameFramework::BaseComponent::Id() const`

Get the component UUID.

**Returns:** UUID of this component.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::Id(Uuid Id)`

Set the component UUID.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::BaseComponent::TypeKey() const`

Get the reflected type id for this component.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::TypeKey(const TypeId &Id)`

Set the reflected type id for this component.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `ComponentHandle SnAPI::GameFramework::BaseComponent::Handle() const`

Get a handle for this component.

**Returns:** ComponentHandle wrapping the UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::BaseComponent::RuntimeIdentity(uint32_t RuntimePoolToken, uint32_t RuntimeIndex, uint32_t RuntimeGeneration)`

Set runtime slot identity for fast handle resolution.

**Parameters**

- `RuntimePoolToken`: Runtime pool token.
- `RuntimeIndex`: Runtime slot index.
- `RuntimeGeneration`: Runtime slot generation.
</div>
<div class="snapi-api-card" markdown="1">
### `BaseNode * SnAPI::GameFramework::BaseComponent::OwnerNode() const`

Resolve the owning node pointer.

**Returns:** Owning BaseNode pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `IWorld * SnAPI::GameFramework::BaseComponent::World() const`

Resolve the owning world pointer.

**Returns:** Owning world or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseComponent::IsServer() const`

Check whether this component executes with server authority.

**Returns:** True when server-authoritative.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseComponent::IsClient() const`

Check whether this component executes in a client context.

**Returns:** True when client-side.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseComponent::IsListenServer() const`

Check whether this component executes as listen-server.

**Returns:** True when both server and client role are active.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseComponent::CallRPC(std::string_view MethodName, std::span< const Variant > Args={})`

Dispatch a reflected RPC method for this component.

**Parameters**

- `MethodName`: Reflected method name.
- `Args`: Variant-packed arguments.

**Returns:** True when dispatch succeeded (local invoke or queued network call).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::BaseComponent::CallRPC(std::string_view MethodName, std::initializer_list< Variant > Args)`

Initializer-list convenience overload for `CallRPC`.

**Parameters**

- `MethodName`: 
- `Args`:
</div>
