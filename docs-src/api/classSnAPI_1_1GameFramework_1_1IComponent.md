# SnAPI::GameFramework::IComponent

Base interface for components attached to nodes.

## Private Members

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::IComponent::m_owner`

Owning node handle.
</div>
<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::IComponent::m_id`

Component UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::IComponent::m_replicated`

Replication flag.
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
### `ComponentHandle SnAPI::GameFramework::IComponent::Handle() const`

Get a handle for this component.

**Returns:** ComponentHandle wrapping the UUID.
</div>
