# SnAPI::GameFramework::RelevancePolicyRegistry

Registry for relevance policy types.

## Contents

- **Type:** SnAPI::GameFramework::RelevancePolicyRegistry::PolicyInfo

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::RelevancePolicyRegistry::EvaluateFn = bool(*)(const void* PolicyData, const RelevanceContext& Context)`

Signature for relevance evaluation callbacks.

**Returns:** True if the node is relevant/active.
</div>

## Private Static Attrib

<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::RelevancePolicyRegistry::m_mutex`

Protects policy map.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, PolicyInfo, UuidHash> SnAPI::GameFramework::RelevancePolicyRegistry::m_policies`

Policy map by TypeId.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::RelevancePolicyRegistry::Register()`

Register a policy type.
</div>
<div class="snapi-api-card" markdown="1">
### `static const PolicyInfo * SnAPI::GameFramework::RelevancePolicyRegistry::Find(const TypeId &PolicyId)`

Find policy metadata by TypeId.

**Parameters**

- `PolicyId`: Policy type id.

**Returns:** Pointer to PolicyInfo or nullptr.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static bool SnAPI::GameFramework::RelevancePolicyRegistry::EvaluateImpl(const void *PolicyData, const RelevanceContext &Context)`

Internal evaluation wrapper for PolicyT.

**Parameters**

- `PolicyData`: Pointer to policy instance.
- `Context`: Evaluation context.

**Returns:** True if relevant.
</div>
