# SnAPI::GameFramework::PlayerStart

Spawn marker node used by gameplay-host pawn spawning.

`PlayerStart` marks a candidate spawn location for newly joined players. It may also carry an optional pawn asset reference that overrides the default spawn path for players using this start. The gameplay host validates that a selected player start belongs to the same world, is active, and is not pending destruction before using it.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::PlayerStart::kTypeName`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `TAssetRef<PawnBase> SnAPI::GameFramework::PlayerStart::m_spawnPawnAsset`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::PlayerStart::PlayerStart()`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::PlayerStart::PlayerStart(std::string Name)`

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::PlayerStart::OnCreate()`

Lifecycle hook used to ensure required PlayerStart components exist.
</div>
<div class="snapi-api-card" markdown="1">
### `TAssetRef< PawnBase > & SnAPI::GameFramework::PlayerStart::EditSpawnPawnAsset()`

Access the optional pawn asset instantiated when players spawn from this start.

**Returns:** Mutable asset reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const TAssetRef< PawnBase > & SnAPI::GameFramework::PlayerStart::GetSpawnPawnAsset() const`

Access the optional pawn asset instantiated when players spawn from this start.

**Returns:** Const asset reference.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::PlayerStart::EnsureDefaultComponents()`
</div>
