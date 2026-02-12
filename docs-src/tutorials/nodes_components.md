# Nodes and Components

This page shows how to design gameplay types and attach reusable behavior.

## 1. Node vs Component

Use a **node** when the thing needs identity in the hierarchy (name, children, transform owner, etc.).

Use a **component** for modular data/behavior you can attach to many node types.

## 2. Define a Custom Node and Component

Header (`MyGameplayTypes.h`):

```cpp
#pragma once

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

class PlayerNode final : public BaseNode
{
public:
    static constexpr const char* kTypeName = "MyGame::PlayerNode";

    int& EditHealth() { return m_health; }
    const int& GetHealth() const { return m_health; }

    void Tick(float DeltaSeconds) override
    {
        (void)DeltaSeconds;
        // Player-specific logic
    }

private:
    int m_health = 100;
};

class MovementComponent final : public IComponent
{
public:
    static constexpr const char* kTypeName = "MyGame::MovementComponent";

    float& EditMaxSpeed() { return m_maxSpeed; }
    const float& GetMaxSpeed() const { return m_maxSpeed; }

    void Tick(float DeltaSeconds) override
    {
        (void)DeltaSeconds;
        // Movement logic using Owner().Borrowed()
    }

private:
    float m_maxSpeed = 5.0f;
};
```

## 3. Register Reflection Once in a `.cpp`

Source (`MyGameplayTypes.cpp`):

```cpp
#include "MyGameplayTypes.h"

SNAPI_REFLECT_TYPE(PlayerNode, (TTypeBuilder<PlayerNode>(PlayerNode::kTypeName)
    .Base<BaseNode>()
    .Field("Health", &PlayerNode::EditHealth, &PlayerNode::GetHealth)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_COMPONENT(MovementComponent, (TTypeBuilder<MovementComponent>(MovementComponent::kTypeName)
    .Field("MaxSpeed", &MovementComponent::EditMaxSpeed, &MovementComponent::GetMaxSpeed)
    .Constructor<>()
    .Register()));
```

Notes:

- `SNAPI_REFLECT_COMPONENT` is an alias of `SNAPI_REFLECT_TYPE`.
- For components, `TTypeBuilder<>::Register()` auto-registers component serialization support.

## 4. Spawn Nodes and Add Components

```cpp
World WorldInstance("GameWorld");
auto LevelHandle = WorldInstance.CreateLevel("MainLevel");
auto LevelRef = WorldInstance.LevelRef(LevelHandle.value());

auto PlayerHandle = LevelRef->CreateNode<PlayerNode>("Player");
auto* Player = static_cast<PlayerNode*>(PlayerHandle->Borrowed());
if (!Player)
{
    return;
}

Player->EditHealth() = 150;

auto MoveResult = Player->Add<MovementComponent>();
if (MoveResult)
{
    MoveResult->EditMaxSpeed() = 8.0f;
}
```

## 5. Query and Remove Components

```cpp
if (Player->Has<MovementComponent>())
{
    auto Move = Player->Component<MovementComponent>();
    if (Move)
    {
        float CurrentSpeed = Move->GetMaxSpeed();
        (void)CurrentSpeed;
    }

    Player->Remove<MovementComponent>();
}

// Actual destruction happens at end-of-frame.
WorldInstance.EndFrame();
```

## 6. Tick Order for Node + Components

For a node in an active tree:

1. `Node::Tick(...)`
2. all attached component `Tick(...)`
3. child nodes recursively

Same pattern exists for `FixedTick` and `LateTick`.

## 7. Replication Gate (Important)

Component/field replication is not automatic just because a field has replication flags.

You also need:

- `Node->Replicated(true)`
- `Component->Replicated(true)`

Without those runtime flags, replication payloads for that object are skipped.

Next: [Reflection and Serialization](reflection_serialization.md)
