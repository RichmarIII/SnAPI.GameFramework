# SnAPI::GameFramework::TAssetRef

Typed reference to an asset that can be resolved by asset id, asset name, or a tagged default name.

`TAssetRef` is the engine-facing handle type used in reflected properties, runtime settings, and serialized payloads to point at assets managed by the AssetPipeline.

Core semantics:
- References may be populated by asset id, asset name, or both.
- Resolution prefers asset id first and falls back to the resolved asset name when id lookup fails.
- For `BaseNode`-derived types, the reference enforces runtime type compatibility using reflection.
- Node asset references can either load detached objects or instantiate directly into a world.
- Overloads without an explicit manager use the process-wide default asset-manager resolver.

Ownership and lifetime:
- `Load()` returns an owning `std::unique_ptr` to a detached runtime object unless the supplied load params instruct the asset factory to instantiate into a world.
- `GetShared()` returns an AssetPipeline handle that shares ownership with the asset manager.
- `Instantiate()` returns a non-owning `NodeHandle` into the destination world.

Threading model:
- `TAssetRef` value operations are thread-safe in isolation.
- Actual asset loading and default-manager resolution obey the thread-safety contract of the underlying `AssetManager`.

Error semantics:
- Fails by returning `std::unexpected<std::string>` or an async result with the `Error` field populated.
- Empty references fail with `"AssetRef is empty"`.

## Contents

- **Type:** SnAPI::GameFramework::TAssetRef::TEntry

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::BaseType = TBase`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::LoadedObjectType = std::conditional_t<std::is_base_of_v<BaseNode, TBase>, BaseNode, TBase>`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::TLoadResult = std::expected<std::unique_ptr<LoadedObjectType>, std::string>`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::TAsyncResult = ::SnAPI::AssetPipeline::AsyncLoadResult<LoadedObjectType>`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::TAsyncCallback = std::function<void(TAsyncResult)>`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::m_assetName`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::m_assetId`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::TAssetRef()`

Construct an empty asset reference or a tag-default reference when `TNameTag` supplies one.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::TAssetRef(std::string AssetName)`

Construct a reference from an asset name.

**Parameters**

- `AssetName`: Asset catalog name. Leading and trailing ASCII whitespace is trimmed.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::TAssetRef(std::string AssetName, std::string AssetId)`

Construct a reference from both name and id text.

Supplying both values allows id-first lookup with a name fallback.

**Parameters**

- `AssetName`: Asset catalog name.
- `AssetId`: Canonical asset-id string.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::string & SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::GetAssetName() const`

Access the stored asset name text.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string & SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::EditAssetName()`

Mutably access the stored asset name text.

**Returns:** Borrowed mutable string reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::string & SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::GetAssetId() const`

Access the stored asset-id text.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string & SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::EditAssetId()`

Mutably access the stored asset-id text.

**Returns:** Borrowed mutable string reference.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::SetAsset(std::string AssetName, std::string AssetId)`

Replace both stored reference fields.

Both inputs are trimmed before storage.

**Parameters**

- `AssetName`: Asset catalog name.
- `AssetId`: Asset-id string.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::Clear()`

Clear the stored asset name and id text.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::IsNull() const`

Query whether this reference carries no resolvable asset identity.

**Returns:** `true` when no explicit name or id is stored and no tag-default name exists.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::operator==(const TAssetRef &Other) const`

Compare stored name and id text for exact equality.

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::operator!=(const TAssetRef &Other) const`

Negated equality comparison.

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::ResolvedAssetName() const`

Resolve the effective asset name.

**Returns:** Explicit asset name when present, otherwise the tag-default name, otherwise an empty string.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::DisplayLabel() const`

Build a user-facing display label for editors and diagnostics.

**Returns:** Name plus short id suffix when both are available, otherwise the best available identifier.
</div>
<div class="snapi-api-card" markdown="1">
### `TLoadResult SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::Load(::SnAPI::AssetPipeline::AssetManager &Manager, const std::any &Params={}) const`

Load the referenced asset through an explicit asset manager.

Resolution order is asset id first, then resolved asset name.

**Parameters**

- `Manager`: Borrowed asset manager.
- `Params`: Optional type-erased load parameters forwarded to the asset factory.

**Returns:** Owning detached runtime object on success, or an error string.
</div>
<div class="snapi-api-card" markdown="1">
### `TLoadResult SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::Load(const std::any &Params={}) const`

Load the referenced asset through the default asset manager.

**Parameters**

- `Params`: Optional type-erased load parameters forwarded to the asset factory.

**Returns:** Owning detached runtime object on success, or an error string.
</div>
<div class="snapi-api-card" markdown="1">
### `std::enable_if_t<!std::is_base_of_v< BaseNode, U >, std::expected<::SnAPI::AssetPipeline::AssetHandle< U >, std::string > > SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::GetShared(::SnAPI::AssetPipeline::AssetManager &Manager, const std::any &Params={}) const`

Acquire a shared asset-manager handle for non-node asset types.

This does not clone the asset. Lifetime is tied to the manager's shared asset storage.

**Parameters**

- `Manager`: Borrowed asset manager.
- `Params`: Optional type-erased load parameters.

**Returns:** Shared asset-manager handle on success, or an error string.
</div>
<div class="snapi-api-card" markdown="1">
### `std::enable_if_t<!std::is_base_of_v< BaseNode, U >, std::expected<::SnAPI::AssetPipeline::AssetHandle< U >, std::string > > SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::GetShared(const std::any &Params={}) const`

Acquire a shared asset-manager handle for non-node asset types using the default manager.

**Parameters**

- `Params`: Optional type-erased load parameters.

**Returns:** Shared asset-manager handle on success, or an error string.
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::AsyncLoadHandle SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::LoadAsync(::SnAPI::AssetPipeline::AssetManager &Manager, ::SnAPI::AssetPipeline::ELoadPriority Priority=::SnAPI::AssetPipeline::ELoadPriority::Normal, const std::any &Params={}, TAsyncCallback Callback={}, ::SnAPI::AssetPipeline::CancellationToken Token={}) const`

Begin asynchronous asset loading through an explicit asset manager.

For node-derived asset references, the completion path validates the loaded runtime type before invoking the callback.

**Parameters**

- `Manager`: Borrowed asset manager.
- `Priority`: Asset-pipeline load priority.
- `Params`: Optional type-erased load parameters.
- `Callback`: Optional completion callback.
- `Token`: Optional cancellation token.

**Returns:** Async load handle that can be used to track or cancel the request.
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::AsyncLoadHandle SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::LoadAsync(::SnAPI::AssetPipeline::ELoadPriority Priority=::SnAPI::AssetPipeline::ELoadPriority::Normal, const std::any &Params={}, TAsyncCallback Callback={}, ::SnAPI::AssetPipeline::CancellationToken Token={}) const`

Begin asynchronous asset loading through the default asset manager.

When no default manager exists, the callback is invoked immediately with an error result.

**Parameters**

- `Priority`: Asset-pipeline load priority.
- `Params`: Optional type-erased load parameters.
- `Callback`: Optional completion callback.
- `Token`: Optional cancellation token.

**Returns:** Async load handle, or an empty handle when no default manager is configured.
</div>
<div class="snapi-api-card" markdown="1">
### `std::enable_if_t< std::is_base_of_v< BaseNode, U >, std::expected< NodeHandle, std::string > > SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::Instantiate(::SnAPI::AssetPipeline::AssetManager &Manager, IWorld &WorldRef, const NodeHandle &Parent={}, bool InstantiateAsCopy=true) const`

Instantiate a node-derived asset directly into a world through an explicit asset manager.

The created node is validated against `TBase`. On mismatch, the newly created node is destroyed before the error is returned.

**Parameters**

- `Manager`: Borrowed asset manager.
- `WorldRef`: Borrowed destination world.
- `Parent`: Optional parent node. A null handle means the world root.
- `InstantiateAsCopy`: When `true`, regenerate object ids during deserialization.

**Returns:** Handle to the created root node on success, or an error string.
</div>
<div class="snapi-api-card" markdown="1">
### `std::enable_if_t< std::is_base_of_v< BaseNode, U >, std::expected< NodeHandle, std::string > > SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::Instantiate(IWorld &WorldRef, const NodeHandle &Parent={}, bool InstantiateAsCopy=true) const`

Instantiate a node-derived asset directly into a world through the default asset manager.

**Parameters**

- `WorldRef`: Borrowed destination world.
- `Parent`: Optional parent node.
- `InstantiateAsCopy`: When `true`, regenerate object ids during deserialization.

**Returns:** Handle to the created root node on success, or an error string.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static std::vector< TEntry > SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::EnumerateCompatibleAssets(::SnAPI::AssetPipeline::AssetManager &Manager)`

Enumerate catalog assets that can be resolved as `TBase`.

For node-derived bases, this performs preview loads and reflection-based type compatibility checks. For non-node assets, it performs preview loads of the requested asset type.

**Parameters**

- `Manager`: Borrowed asset manager.

**Returns:** Sorted list of compatible entries.
</div>
<div class="snapi-api-card" markdown="1">
### `static std::vector< TEntry > SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::EnumerateCompatibleAssets()`

Enumerate compatible assets using the default asset manager.

**Returns:** Sorted list of compatible entries, or an empty list when no default manager is configured.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static std::string SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::TrimCopy(std::string_view Text)`

**Parameters**

- `Text`:
</div>
<div class="snapi-api-card" markdown="1">
### `static std::string SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::ShortAssetId(const std::string &AssetId)`

**Parameters**

- `AssetId`:
</div>
<div class="snapi-api-card" markdown="1">
### `static std::optional<::SnAPI::AssetPipeline::AssetId > SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::ParseAssetId(std::string_view AssetIdText)`

**Parameters**

- `AssetIdText`:
</div>
<div class="snapi-api-card" markdown="1">
### `static std::string SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::DefaultAssetName()`
</div>
<div class="snapi-api-card" markdown="1">
### `static std::string SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::BuildTypeMismatchMessage()`
</div>
<div class="snapi-api-card" markdown="1">
### `static bool SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::IsNodeCompatible(const TypeId &RuntimeNodeType)`

**Parameters**

- `RuntimeNodeType`:
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `std::optional<::SnAPI::AssetPipeline::AssetId > SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::ParsedAssetId() const`
</div>
<div class="snapi-api-card" markdown="1">
### `TLoadResult SnAPI::GameFramework::TAssetRef< TBase, TNameTag >::LoadInternal(::SnAPI::AssetPipeline::AssetManager &Manager, const std::any &Params) const`

**Parameters**

- `Manager`: 
- `Params`:
</div>
