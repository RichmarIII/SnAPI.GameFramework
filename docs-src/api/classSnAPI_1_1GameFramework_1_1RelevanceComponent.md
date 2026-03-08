# SnAPI::GameFramework::RelevanceComponent

Component that stores per-node relevance policy state and the latest evaluation result.

A `RelevanceComponent` turns arbitrary policy data into something the level can evaluate uniformly. The component owns an erased policy payload plus two cached outputs:
- whether the node is currently considered active/relevant
- the last score produced by the broader relevance pass

Why it exists:
- policy structs stay plain data types instead of polymorphic heap hierarchies
- node storage can keep one uniform component type
- evaluation code can dispatch through `RelevancePolicyRegistry`

Ownership and lifetime:
- The component owns the current policy payload through `std::shared_ptr<void>`.
- Replacing the policy releases the previous payload when no longer referenced.
- Returned policy data from `PolicyData()` is borrowed and type-erased.

Threading:
- Main-thread only.
- Mutating the policy while a relevance pass is in progress is not supported.

Invariants:
- `m_policyId` is meaningful only when `m_policyData` holds a matching payload.
- `Active()` and `LastScore()` are cache fields; they do not trigger evaluation.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::RelevanceComponent::kTypeName`

Stable type name for reflection.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::RelevanceComponent::m_policyId`

Reflected type id of current policy object.
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<void> SnAPI::GameFramework::RelevanceComponent::m_policyData`

Owned type-erased policy instance payload.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::RelevanceComponent::m_active`

Last computed relevance active state applied to node gating.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::RelevanceComponent::m_lastScore`

Last computed score used for diagnostics/future prioritization.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::RelevanceComponent::Policy(PolicyT Policy)`

Replace the stored relevance policy payload with a new concrete policy value.

Semantics:
- Ensures `PolicyT` is registered in `RelevancePolicyRegistry`.
- Replaces any previously stored policy object.
- Updates `PolicyId()` to the reflected id for `PolicyT`.

Ownership:
- Ownership of the stored instance transfers into the component's internal shared payload.

**Parameters**

- `Policy`:
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::RelevanceComponent::PolicyId() const`

Get the reflected type id of the currently stored policy payload.

Returns the nil/default `TypeId` when no policy has been configured yet.

**Returns:** Borrowed reference to the stored policy type id.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::shared_ptr< void > & SnAPI::GameFramework::RelevanceComponent::PolicyData() const`

Access the owned, type-erased policy payload.

The pointer is intentionally type-erased. Callers are expected to pair this with `PolicyId()` and `RelevancePolicyRegistry::Find()` rather than static-casting it blindly.

**Returns:** Borrowed reference to the internal shared payload.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::RelevanceComponent::Active() const`

Read the most recently applied relevance-active flag.

**Returns:** `true` when the last relevance pass marked this node active.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::RelevanceComponent::Active(bool Active)`

Store the most recently computed relevance-active flag.

This is a passive cache write. It does not itself evaluate the policy.

**Parameters**

- `Active`:
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::RelevanceComponent::LastScore() const`

Read the last score written by the relevance system.

**Returns:** Cached score value.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::RelevanceComponent::LastScore(float Score)`

Store the score produced by the latest relevance evaluation.

**Parameters**

- `Score`: Cached score value.
</div>
