# SnAPI::GameFramework::ScriptInstanceContext

Non-owning object context injected into a newly created script instance.

The runtime passes this structure to the backend so the script can discover the World, owner Node, and owner Component that caused it to be created.

Ownership and lifetime:
- All pointers are borrowed.
- They remain valid only as long as the corresponding World and gameplay objects exist.
- Backends may copy the pointers into script-side state, but they do not acquire ownership.

## Public Members

<div class="snapi-api-card" markdown="1">
### `IWorld* SnAPI::GameFramework::ScriptInstanceContext::World`

Borrowed World that owns the script runtime and gameplay graph.
</div>
<div class="snapi-api-card" markdown="1">
### `BaseNode* SnAPI::GameFramework::ScriptInstanceContext::OwnerNode`

Borrowed Node that owns the Component requesting the script instance.
</div>
<div class="snapi-api-card" markdown="1">
### `BaseComponent* SnAPI::GameFramework::ScriptInstanceContext::OwnerComponent`

Borrowed Component responsible for the script instance, typically a `ScriptComponent`.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::ScriptInstanceContext::OwnerComponentType`

Reflected concrete component type.

Used when the backend needs a stable type name even if only a base pointer is available.
</div>
