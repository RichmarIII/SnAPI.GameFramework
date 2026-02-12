# SnAPI::GameFramework::RelevanceComponent

Component that drives relevance evaluation for a node.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::RelevanceComponent::kTypeName`

Stable type name for reflection.
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::RelevanceComponent::m_policyId`

Policy type id.
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<void> SnAPI::GameFramework::RelevanceComponent::m_policyData`

Type-erased policy instance.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::RelevanceComponent::m_active`

Last computed active state.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::RelevanceComponent::m_lastScore`

Last computed score.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::RelevanceComponent::Policy(PolicyT Policy)`

Set the relevance policy for this component.

**Parameters**

- `Policy`:
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::RelevanceComponent::PolicyId() const`

Get the policy type id.

**Returns:** TypeId of the policy.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::shared_ptr< void > & SnAPI::GameFramework::RelevanceComponent::PolicyData() const`

Get the stored policy instance.

**Returns:** Shared pointer to the policy data.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::RelevanceComponent::Active() const`

Get the active state computed by relevance.

**Returns:** True if relevant.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::RelevanceComponent::Active(bool Active)`

Set the active state computed by relevance.

**Parameters**

- `Active`:
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::RelevanceComponent::LastScore() const`

Get the last computed relevance score.

**Returns:** Score value.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::RelevanceComponent::LastScore(float Score)`

Set the last computed relevance score.

**Parameters**

- `Score`: Score value.
</div>
