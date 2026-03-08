# SnAPI::GameFramework::TDeserializeOptions

Policy flags that control how payload identity is materialized during decode.

These options alter how serializers treat UUIDs embedded in payloads. The main use case is the difference between:
- loading a save file and preserving object identity, and
- instantiating a template/prefab-like payload where every object must receive a fresh runtime id.

## Public Members

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TDeserializeOptions::RegenerateObjectIds`

Regenerate Node and Component UUIDs while loading.

When enabled, the payload's object ids are treated as source/template ids rather than runtime-stable ids. Deserialization builds remap tables up front, assigns fresh ids to newly created objects, and rewrites internal `NodeHandle` and `ComponentHandle` references through those remaps while decoding fields.

**Notes**

- References that point outside the payload are not magically re-bound; only ids present in the generated remap tables are rewritten.
</div>
