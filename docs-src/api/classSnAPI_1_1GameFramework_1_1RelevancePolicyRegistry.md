# SnAPI::GameFramework::RelevancePolicyRegistry

Process-wide registry that binds reflected relevance-policy types to evaluation callbacks.

`RelevanceComponent` stores policy state in type-erased form. The registry supplies the code path that turns that erased payload back into "call `PolicyT::Evaluate(...)`". This keeps runtime storage compact while still allowing arbitrary policy structs to be registered lazily on first use.

Core semantics:
- Registration is keyed by reflected `TypeId`.
- Duplicate registration of the same type is ignored.
- The registry does not own policy instances; it only owns dispatch metadata.
- `Find()` returns metadata only when the policy type has already been registered.

Threading:
- Not generally thread-safe.
- Internal `GameMutex` use provides affinity validation, not real mutual exclusion.
- Register and lookup on the game thread or provide external synchronization.

## Contents

- **Type:** SnAPI::GameFramework::RelevancePolicyRegistry::PolicyInfo

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::RelevancePolicyRegistry::EvaluateFn = bool(*)(const void* PolicyData, const RelevanceContext& Context)`

Type-erased function signature used to evaluate one policy instance.

**Returns:** `true` when the node should be treated as relevant/active.
</div>

## Private Static Attrib

<div class="snapi-api-card" markdown="1">
### `GameMutex SnAPI::GameFramework::RelevancePolicyRegistry::m_mutex`

Protects policy map.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, PolicyInfo, UuidHash> SnAPI::GameFramework::RelevancePolicyRegistry::m_policies`

Policy map by TypeId.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::RelevancePolicyRegistry::Register()`

Register a policy type and its type-erased evaluation trampoline.

`PolicyT` is expected to provide `bool Evaluate(const RelevanceContext&) const` or another compatible callable member used by `EvaluateImpl`.

Registration is idempotent. Re-registering an already-known type leaves the original metadata in place.

**Notes**

- This function does not create or own policy instances.
</div>
<div class="snapi-api-card" markdown="1">
### `static const PolicyInfo * SnAPI::GameFramework::RelevancePolicyRegistry::Find(const TypeId &PolicyId)`

Look up dispatch metadata for a previously registered policy type.

The returned pointer is borrowed and remains valid until static shutdown.

**Parameters**

- `PolicyId`: Reflected policy type id.

**Returns:** Pointer to registry-owned metadata, or `nullptr` when the type has not been registered.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static bool SnAPI::GameFramework::RelevancePolicyRegistry::EvaluateImpl(const void *PolicyData, const RelevanceContext &Context)`

Type-specific trampoline used by the registry to erase policy storage.

**Parameters**

- `PolicyData`: Borrowed pointer to a stored `PolicyT` instance.
- `Context`: Borrowed evaluation context.

**Returns:** `true` when `PolicyT::Evaluate(Context)` reports the node as relevant.
</div>
