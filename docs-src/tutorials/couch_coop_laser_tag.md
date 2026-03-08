# Couch Co-op Laser Tag

This tutorial focuses on local multiplayer flow.

The goal is to help a newcomer understand that split/local players are not a UI trick. They are modeled explicitly in gameplay through `GameplayHost` and `LocalPlayer` nodes.

## What You Will Build

- one runtime with gameplay enabled
- multiple local players in one session
- possession-aware input ownership
- explicit player indices and device assignment rules

## 1. Start With Gameplay Enabled

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "LaserTagWorld";

GameRuntimeGameplaySettings Gameplay{};
Gameplay.AutoCreateLocalPlayer = false;
Gameplay.RegisterDefaultLocalPlayerService = true;
Settings.Gameplay = Gameplay;

GameRuntimeInputSettings Input{};
Input.CreateDesc.EnableKeyboard = true;
Input.CreateDesc.EnableGamepad = true;
Settings.Input = Input;

if (auto InitResult = Runtime.Init(Settings); !InitResult)
{
    return;
}
```

Why disable auto-create here:

- you want to practice joining players deliberately
- you want stable control over player index and naming

## 2. Request Two Local Players

```cpp
auto* Host = Runtime.Gameplay();
if (!Host)
{
    return;
}

(void)Host->RequestJoinPlayer("Red", std::optional<unsigned int>{0}, true);
(void)Host->RequestJoinPlayer("Blue", std::optional<unsigned int>{1}, true);
```

On a local or listen-server runtime, those requests execute through the authority path directly.

## 3. Inspect The Created `LocalPlayer` Nodes

```cpp
for (const NodeHandle Handle : Host->LocalPlayers())
{
    auto* Player = NodeCast<LocalPlayer>(Handle.Borrowed());
    if (!Player)
    {
        continue;
    }

    // Player->GetPlayerIndex()
    // Player->GetOwnerConnectionId()
    // Player->GetPossessedNode()
}
```

The important lesson is that a local player is a real node in the world, not just a platform-side controller slot.

## 4. Assigned Device Rules Matter

`LocalPlayer` can carry device assignment state.

```cpp
auto Players = Host->LocalPlayers();
if (Players.size() >= 2)
{
    if (auto* Blue = NodeCast<LocalPlayer>(Players[1].Borrowed()))
    {
        Blue->EditUseAssignedInputDevice() = true;
        Blue->EditAssignedInputDevice() = DesiredGamepadId;
    }
}
```

Why this exists:

- keyboard input should not leak into every player
- secondary local players usually need explicit gamepad ownership
- `InputComponent` uses this data when deciding whether a pawn should consume input

## 5. Possession Is A First-Class Concept

Local players do not become useful until they possess something.

You can drive possession through gameplay host spawn flow, `PlayerStart`, and `PawnBase`, or directly request possession on `LocalPlayer`.

```cpp
LocalPlayer* Player = /* resolved from a handle */;
NodeHandle PawnHandle = /* your pawn */;
Player->RequestPossess(PawnHandle);
```

Clients route that through server RPC when needed. Local authority applies it directly.

## 6. Why This Tutorial Matters

This is the point where many engine codebases become messy. They let arbitrary pawn components poll global keyboard state and call it multiplayer.

GameFramework has a better shape:

- `GameplayHost` owns player join/leave flow
- `LocalPlayer` owns player identity and possession state
- `InputComponent` respects that state when mapping input to gameplay intent

## 7. Fun Rules For The Actual Laser Tag Game

Try these constraints for practice:

1. Player 0 can use keyboard and mouse.
2. Player 1 must use a gamepad.
3. Each player possesses a different pawn.
4. Each pawn owns its own `InputComponent`, `InputIntentComponent`, and `CharacterMovementController`.

## 8. Extensions

1. Add `AudioSourceComponent` hit sounds per player.
2. Add replicated score nodes so this tutorial can graduate into online play.
3. Add camera auto-activation per possessed pawn.

Continue with [Net Arena](net_arena.md) if you want to move from local to networked play.
