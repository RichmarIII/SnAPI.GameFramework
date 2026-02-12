# Reflection and Serialization

This page explains the type system that powers serialization, replication, and reflection-based RPC.

## 1. Startup: Register Builtins

Call this once during startup:

```cpp
RegisterBuiltinTypes();
```

It registers built-in value types and default serialization codecs.

## 2. Register-on-First-Use Model

SnAPI.GameFramework uses lazy reflection registration:

- `SNAPI_REFLECT_TYPE(...)` stores an ensure callback in `TypeAutoRegistry`.
- `TypeRegistry::Find(...)` can trigger that callback on first miss.
- The heavy registration runs once (`std::call_once`).

Why this model helps:

- No giant central type list.
- No cross-translation-unit init ordering dependency for full metadata.
- Pay-for-what-you-use behavior.

### Linker/stripping note

`SNAPI_REFLECT_TYPE` installs static registration glue in its translation unit.
To guarantee that translation unit is linked in static library/plugin builds, ensure something in that unit is referenced by the final binary (or explicitly force-link the object/library according to your build system).

## 3. Defining Reflected Types

```cpp
class EnemyNode final : public BaseNode
{
public:
    static constexpr const char* kTypeName = "MyGame::EnemyNode";

    int Health = 100;

    void ServerSetHealth(int InValue)
    {
        Health = InValue;
    }
};

class InventoryComponent final : public IComponent
{
public:
    static constexpr const char* kTypeName = "MyGame::InventoryComponent";

    int Gold = 0;
};

SNAPI_REFLECT_TYPE(EnemyNode, (TTypeBuilder<EnemyNode>(EnemyNode::kTypeName)
    .Base<BaseNode>()
    .Field("Health", &EnemyNode::Health, EFieldFlagBits::Replication)
    .Method("ServerSetHealth",
            &EnemyNode::ServerSetHealth,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_COMPONENT(InventoryComponent, (TTypeBuilder<InventoryComponent>(InventoryComponent::kTypeName)
    .Field("Gold", &InventoryComponent::Gold, EFieldFlagBits::Replication)
    .Constructor<>()
    .Register()));
```

Important points:

- Always register base classes with `.Base<...>()` to preserve inheritance queries.
- Register a default constructor if the type must be created from type metadata (serialization/network spawn paths).

## 4. `StaticTypeId<T>()` and `StaticType<T>()`

Use cached type identity in hot paths:

```cpp
const TypeId& CachedId = StaticTypeId<EnemyNode>();
auto EnsureResult = StaticType<EnemyNode>(); // returns TExpected<TypeId*>
```

- `StaticTypeId<T>()` is deterministic and cached via function-local static.
- `StaticType<T>()` ensures reflection registration exists before use.

Hot path guidance:

- prefer `StaticTypeId<T>()` over repeated `TypeIdFromName(...)`.
- use `StaticType<T>()` when you need to force ensure-on-first-use behavior.

## 5. Value Serialization (`TValueCodec<T>`)

Most primitive/trivial types already work.
For custom packed formats, specialize `TValueCodec<T>`.

```cpp
struct PackedVec2
{
    static constexpr const char* kTypeName = "MyGame::PackedVec2";
    int32_t X = 0;
    int32_t Y = 0;
};

template<>
struct TValueCodec<PackedVec2>
{
    static TExpected<void> Encode(const PackedVec2& Value,
                                  cereal::BinaryOutputArchive& Archive,
                                  const TSerializationContext&)
    {
        int32_t Packed = (Value.X << 16) | (Value.Y & 0xFFFF);
        Archive(Packed);
        return Ok();
    }

    static TExpected<PackedVec2> Decode(cereal::BinaryInputArchive& Archive,
                                        const TSerializationContext&)
    {
        int32_t Packed = 0;
        Archive(Packed);
        return PackedVec2{(Packed >> 16), (Packed & 0xFFFF)};
    }

    static TExpected<void> DecodeInto(PackedVec2& Value,
                                      cereal::BinaryInputArchive& Archive,
                                      const TSerializationContext&)
    {
        int32_t Packed = 0;
        Archive(Packed);
        Value.X = (Packed >> 16);
        Value.Y = (Packed & 0xFFFF);
        return Ok();
    }
};
```

## 6. Replication + Nested Types + Value Codecs

For a reflected field marked with `EFieldFlagBits::Replication`:

- if a `TValueCodec<FieldType>` is registered, replication uses that codec for the field value.
- if no codec is registered, replication recursively visits nested reflected fields and serializes only nested fields that also have replication flags.

That means field flags are still important for nested structs.
Example: if `Settings` is replicated but only `Settings.SoundPath` is flagged, only `SoundPath` replicates unless you provide a codec that serializes the whole `Settings` value.

## 7. Graph/Level/World Serialization

### Graph to payload bytes

```cpp
NodeGraph Graph("Gameplay");
auto PayloadResult = NodeGraphSerializer::Serialize(Graph);
if (!PayloadResult)
{
    return;
}

std::vector<uint8_t> Bytes;
auto BytesResult = SerializeNodeGraphPayload(PayloadResult.value(), Bytes);
if (!BytesResult)
{
    return;
}
```

### Payload bytes back to graph

```cpp
auto PayloadRoundTrip = DeserializeNodeGraphPayload(Bytes.data(), Bytes.size());
if (!PayloadRoundTrip)
{
    return;
}

NodeGraph Loaded;
auto LoadResult = NodeGraphSerializer::Deserialize(PayloadRoundTrip.value(), Loaded);
if (!LoadResult)
{
    return;
}
```

The same pattern exists for `LevelSerializer` + `WorldSerializer` and their payload byte helpers.

## 8. Component Serialization Registration

Why components are special:

- Components are created dynamically by type id during graph/world deserialization.
- `ComponentSerializationRegistry` provides the creation + field-serialization callbacks.

You normally do **not** need to register this manually for normal reflected components.
`TTypeBuilder<ComponentT>::Register()` does it automatically when `ComponentT` derives from `IComponent`.

Next: [AssetPipeline Integration](assetpipeline.md)
