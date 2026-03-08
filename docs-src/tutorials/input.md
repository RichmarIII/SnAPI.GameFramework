# Input System

The input model is simple once you stop looking for per-object backends.

- the `World` owns one `InputSystem`
- the input system owns one active `InputContext`
- gameplay code reads normalized frame state from that shared context
- components such as `InputComponent` bridge the raw snapshot into gameplay intent

## 1. Bootstrap Through `GameRuntime`

```cpp
GameRuntime Runtime;
GameRuntimeSettings Settings{};
Settings.WorldName = "InputWorld";

GameRuntimeInputSettings InputSettings{};
InputSettings.CreateDesc.EnableKeyboard = true;
InputSettings.CreateDesc.EnableMouse = true;
InputSettings.CreateDesc.EnableGamepad = true;
InputSettings.CreateDesc.EnableTextInput = true;
Settings.Input = InputSettings;

if (auto InitResult = Runtime.Init(Settings); !InitResult)
{
    return;
}
```

Once initialized, `GameRuntime` will cause `World::Tick()` to pump input before UI and gameplay ECS traversal.

## 2. Read Snapshot Data Directly

```cpp
const auto* Snapshot = Runtime.World().Input().Snapshot();
const auto* Events = Runtime.World().Input().Events();
const auto* Devices = Runtime.World().Input().Devices();
auto* Actions = Runtime.World().Input().Actions();
```

These are borrowed pointers into the active input context.

Do not cache them across:

- input shutdown
- input reinitialization
- world teardown

## 3. Use `InputComponent` For Pawn-Style Input

For normal character control, you usually do not want every node to parse keyboard and gamepad state manually.

That is what `InputComponent` and `InputIntentComponent` are for.

- `InputComponent` samples the world input snapshot
- it resolves local-player routing rules
- it publishes movement, jump, and look intent into a sibling `InputIntentComponent`
- movement and camera systems consume that intent later

Example setup:

```cpp
auto PawnHandle = WorldInstance.CreateNode<PawnBase>("PlayerPawn");
if (!PawnHandle)
{
    return;
}

auto* Pawn = PawnHandle->Borrowed();
if (!Pawn)
{
    return;
}

(void)Pawn->Add<TransformComponent>();
(void)Pawn->Add<InputIntentComponent>();

if (auto Input = Pawn->Add<InputComponent>())
{
    auto& InputSettings = Input->EditSettings();
    InputSettings.RequireInputFocus = true;
    InputSettings.NormalizeMove = true;
    InputSettings.MouseLookSensitivity = 0.12f;
    InputSettings.GamepadLookSensitivity = 180.0f;
}
```

## 4. Understand Local-Player Routing

`InputComponent` is not a raw keyboard reader with no context. It pays attention to `LocalPlayer` ownership.

Important rules:

- if the node is possessed by a local player, the component can use that player's assignment and filtering rules
- non-primary local players do not get keyboard input by default
- assigned-device mode forces input to a chosen gamepad
- if the world has local players and this node is not possessed by one of them, input is suppressed rather than guessed

That design prevents multiple pawns from accidentally reading the same shared keyboard state.

## 5. Work One Layer Lower With `InputIntentComponent`

If you want custom producers or consumers, operate directly on the intent component.

```cpp
auto IntentResult = Pawn->Component<InputIntentComponent>();
if (!IntentResult)
{
    return;
}

IntentResult->SetMoveWorldInput(Vec3(1.0f, 0.0f, 0.0f));
IntentResult->QueueJump();
IntentResult->AddLookInput(5.0f, -2.0f);
```

Consumers such as `CharacterMovementController` and `SprintArmComponent` can then read and consume those values.

## 6. Common Patterns

### Direct snapshot polling for tools and debug shells

Use `Snapshot()` and `Events()` directly.

### Pawn movement and camera control

Use `InputComponent` plus `InputIntentComponent`.

### Split local players

Use `GameplayHost` and `LocalPlayerService` to create local players, then let possession and assigned-device rules drive input ownership.

## 7. Common Mistakes

### Caching event pointers forever

The event vector is owned by the active input context.

### Driving gameplay from input before the world pumps input

Read input after the frame pump, not before.

### Ignoring local-player routing

If you bypass `InputComponent`, you own all the multiplayer and couch-coop ambiguity yourself.

## What To Read Next

- [UI System](ui.md)
- [Physics System and Components](physics.md)
- [Couch Co-op Laser Tag](couch_coop_laser_tag.md)
