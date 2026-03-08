# SnAPI::GameFramework::RelevanceContext

Inputs provided to a relevance-policy evaluation.

Relevance policies are intentionally evaluated against a narrow context rather than against the full world API. This keeps the policy contract cheap to pass around and makes the decision inputs explicit.

Lifetime and ownership:
- `Node` is a value handle copy.
- `Graph` is a borrowed reference to the owning level and must not be retained past the evaluation call.

Threading:
- Main-thread only unless the owning level explicitly guarantees otherwise.

## Public Members

<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::RelevanceContext::Node`

Handle of the node currently being tested for relevance.
</div>
<div class="snapi-api-card" markdown="1">
### `std::reference_wrapper<Level> SnAPI::GameFramework::RelevanceContext::Graph`

Borrowed owning level used for neighborhood or graph-aware decisions.
</div>
