# Reflection and Serialization

Reflection is one of the core services in the framework. It is not just for editor tooling.

It drives:

- serialization
- replication metadata
- reflected RPC dispatch
- property panels
- dynamic component creation during load

## 1. Register Types Deliberately

A reflected type needs:

- a stable `kTypeName`
- a `TTypeBuilder<...>` registration block
- a default constructor when the engine needs to create it dynamically

```cpp
class CargoNode final : public BaseNode
{
public:
    static constexpr const char* kTypeName = "MyGame::CargoNode";

    int Value = 100;
    NodeHandle DockTarget{};
};

SNAPI_REFLECT_TYPE(CargoNode, (TTypeBuilder<CargoNode>(CargoNode::kTypeName)
    .Base<BaseNode>()
    .Field("Value", &CargoNode::Value)
    .Field("DockTarget", &CargoNode::DockTarget)
    .Constructor<>()
    .Register()));
```

## 2. Reflection And Replication Flags Share The Same Metadata

Fields can carry flags such as replication.
Methods can carry flags such as RPC routing.

```cpp
struct DoorState
{
    static constexpr const char* kTypeName = "MyGame::DoorState";
    bool Open = false;
};

class DoorNode final : public BaseNode
{
public:
    static constexpr const char* kTypeName = "MyGame::DoorNode";

    DoorState State{};

    void ServerToggle()
    {
        State.Open = !State.Open;
    }
};

SNAPI_REFLECT_TYPE(DoorState, (TTypeBuilder<DoorState>(DoorState::kTypeName)
    .Field("Open", &DoorState::Open, EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(DoorNode, (TTypeBuilder<DoorNode>(DoorNode::kTypeName)
    .Base<BaseNode>()
    .Field("State", &DoorNode::State)
    .Method("ServerToggle", &DoorNode::ServerToggle,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Constructor<>()
    .Register()));
```

## 3. Serialize A Node, Level, Or World

The current serializer surface is:

- `NodeSerializer`
- `LevelSerializer`
- `WorldSerializer`

### Node example

```cpp
auto* Node = Handle.Borrowed();
if (!Node)
{
    return;
}

auto PayloadResult = NodeSerializer::Serialize(*Node);
if (!PayloadResult)
{
    return;
}

std::vector<uint8_t> Bytes;
if (!SerializeNodePayload(*PayloadResult, Bytes))
{
    return;
}
```

### Node deserialize example

```cpp
World LoadedWorld("LoadedWorld");
auto PayloadResult = DeserializeNodePayload(Bytes.data(), Bytes.size());
if (!PayloadResult)
{
    return;
}

auto NodeResult = NodeSerializer::Deserialize(*PayloadResult, LoadedWorld);
if (!NodeResult)
{
    return;
}
```

The same pattern exists for levels and whole worlds.

## 4. `TDeserializeOptions` Matters

The most important option beginners should know is `RegenerateObjectIds`.

Use it when you are instantiating the same payload multiple times into one world and need fresh identities.

That is the correct path for prefab-like repeated instantiation.

## 5. `OnCreate` During Deserialize

Deserialization is intentionally two-phase enough to avoid partial-state callbacks.

The important rule is:

- fields are populated first
- `OnCreate` is not allowed to observe half-deserialized state

This is covered by tests such as the serialization test that verifies component `OnCreate` runs once after deserialized fields are populated.

## 6. Value Codecs

When a field type has a `TValueCodec<T>`, serialization and replication can treat it as a packed value.

Without a value codec, nested reflection metadata is used instead.

This is the rule that explains a lot of replication behavior for nested structs.

## 7. Common Mistakes

### Using old serializer names

Use `NodeSerializer`, `LevelSerializer`, and `WorldSerializer`.

### Forgetting default constructors

Dynamic load paths need a constructor they can invoke.

### Forgetting reflection on handle-carrying fields

If a field stores a `NodeHandle`, `ComponentHandle`, `TAssetRef<T>`, or custom struct and you expect serialization to see it, reflect it.

## What To Read Next

- [AssetPipeline Integration](assetpipeline.md)
- [Shipyard Save/Load](shipyard_save_load.md)
