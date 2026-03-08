# SnAPI::GameFramework::PawnBase

Default pawn node used by gameplay-host spawning and auto-possession flow.

`PawnBase` is the stock spawnable pawn type used by `GameplayHost` when no more specific pawn type is selected by the game, game mode, or `PlayerStart` asset reference. Its `OnCreate()` path is intentionally idempotent and ensures a baseline set of transform, movement, input, and rendering-related components exist when the relevant subsystems are enabled.

Semantics:
- the node is replicated by default
- `OnCreate()` ensures default components rather than assuming a pre-authored prefab
- possession toggles the pawn camera active state when a camera component is present

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::PawnBase::kTypeName`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::PawnBase::PawnBase()`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::PawnBase::PawnBase(std::string Name)`

**Parameters**

- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::PawnBase::OnCreate()`

Lifecycle hook used by gameplay spawn flow to ensure default pawn components exist.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::PawnBase::OnPossess(const NodeHandle &PlayerHandle)`

Notification that a player began possessing this pawn.

**Parameters**

- `PlayerHandle`: Possessing player handle.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::PawnBase::OnUnpossess(const NodeHandle &PlayerHandle)`

Notification that a player stopped possessing this pawn.

**Parameters**

- `PlayerHandle`: Player handle that released possession.
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::PawnBase::EnsureDefaultComponents()`
</div>
