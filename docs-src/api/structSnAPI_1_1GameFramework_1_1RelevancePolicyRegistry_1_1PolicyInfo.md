# SnAPI::GameFramework::RelevancePolicyRegistry::PolicyInfo

Dispatch metadata recorded for a registered policy type.

The registry is intentionally minimal today: evaluation is the only required behavior. Additional policy-side metadata can be added here later without changing the component storage format.

## Public Members

<div class="snapi-api-card" markdown="1">
### `EvaluateFn SnAPI::GameFramework::RelevancePolicyRegistry::PolicyInfo::Evaluate`

Type-erased evaluation entry point for the policy type.
</div>
