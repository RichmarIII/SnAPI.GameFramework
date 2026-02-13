# SnAPI::GameFramework

## Contents

- **Namespace:** SnAPI::GameFramework::detail
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{AssetPipelineFactories.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{AssetPipelineSerializers.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{IComponent.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{INode.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{ScriptABI.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}
- **Namespace:** SnAPI::GameFramework::anonymous_namespace{TypeRegistry.cpp}
- **Type:** SnAPI::GameFramework::BaseNode
- **Type:** SnAPI::GameFramework::ComponentTypeRegistry
- **Type:** SnAPI::GameFramework::IComponentStorage
- **Type:** SnAPI::GameFramework::TComponentStorage
- **Type:** SnAPI::GameFramework::Error
- **Type:** SnAPI::GameFramework::TExpectedRef
- **Type:** SnAPI::GameFramework::TFlags
- **Type:** SnAPI::GameFramework::EnableFlags
- **Type:** SnAPI::GameFramework::GameRuntimeTickSettings
- **Type:** SnAPI::GameFramework::GameRuntimeSettings
- **Type:** SnAPI::GameFramework::GameRuntime
- **Type:** SnAPI::GameFramework::THandle
- **Type:** SnAPI::GameFramework::HandleHash
- **Type:** SnAPI::GameFramework::IComponent
- **Type:** SnAPI::GameFramework::ILevel
- **Type:** SnAPI::GameFramework::INode
- **Type:** SnAPI::GameFramework::IWorld
- **Type:** SnAPI::GameFramework::JobSystem
- **Type:** SnAPI::GameFramework::Level
- **Type:** SnAPI::GameFramework::Vec3
- **Type:** SnAPI::GameFramework::NodeGraph
- **Type:** SnAPI::GameFramework::TObjectPool
- **Type:** SnAPI::GameFramework::ObjectRegistry
- **Type:** SnAPI::GameFramework::RelevanceContext
- **Type:** SnAPI::GameFramework::RelevancePolicyRegistry
- **Type:** SnAPI::GameFramework::RelevanceComponent
- **Type:** SnAPI::GameFramework::ScriptBindings
- **Type:** SnAPI::GameFramework::ScriptComponent
- **Type:** SnAPI::GameFramework::IScriptEngine
- **Type:** SnAPI::GameFramework::ScriptRuntime
- **Type:** SnAPI::GameFramework::TSerializationContext
- **Type:** SnAPI::GameFramework::TValueCodec
- **Type:** SnAPI::GameFramework::ValueCodecRegistry
- **Type:** SnAPI::GameFramework::ComponentSerializationRegistry
- **Type:** SnAPI::GameFramework::NodeComponentPayload
- **Type:** SnAPI::GameFramework::NodePayload
- **Type:** SnAPI::GameFramework::NodeGraphPayload
- **Type:** SnAPI::GameFramework::LevelPayload
- **Type:** SnAPI::GameFramework::WorldPayload
- **Type:** SnAPI::GameFramework::NodeGraphSerializer
- **Type:** SnAPI::GameFramework::LevelSerializer
- **Type:** SnAPI::GameFramework::WorldSerializer
- **Type:** SnAPI::GameFramework::TransformComponent
- **Type:** SnAPI::GameFramework::TTypeRegistrar
- **Type:** SnAPI::GameFramework::TypeAutoRegistry
- **Type:** SnAPI::GameFramework::TTypeBuilder
- **Type:** SnAPI::GameFramework::TTypeName
- **Type:** SnAPI::GameFramework::TransparentStringHash
- **Type:** SnAPI::GameFramework::TransparentStringEqual
- **Type:** SnAPI::GameFramework::EnableFlags< EFieldFlagBits >
- **Type:** SnAPI::GameFramework::EnableFlags< EMethodFlagBits >
- **Type:** SnAPI::GameFramework::FieldInfo
- **Type:** SnAPI::GameFramework::MethodInfo
- **Type:** SnAPI::GameFramework::ConstructorInfo
- **Type:** SnAPI::GameFramework::TypeInfo
- **Type:** SnAPI::GameFramework::TypeRegistry
- **Type:** SnAPI::GameFramework::UuidParts
- **Type:** SnAPI::GameFramework::UuidHash
- **Type:** SnAPI::GameFramework::Variant
- **Type:** SnAPI::GameFramework::VariantView
- **Type:** SnAPI::GameFramework::World

## Enumerations

<div class="snapi-api-card" markdown="1">
### `enum EErrorCode`

Canonical error codes used by the framework.

**Values**

- `None`: No error.
- `NotFound`: Requested item was not found.
- `InvalidArgument`: One or more arguments are invalid.
- `TypeMismatch`: Type mismatch or unsafe conversion.
- `OutOfRange`: Index or value is out of range.
- `AlreadyExists`: Attempted to create an object that already exists.
- `NotReady`: Subsystem or object is not ready.
- `InternalError`: Unexpected internal failure.
</div>
<div class="snapi-api-card" markdown="1">
### `enum EObjectKind`

Kind of object stored in the registry.

**Values**

- `Node`: BaseNode-derived object.
- `Component`: IComponent-derived object.
- `Other`: Arbitrary registered type.
</div>
<div class="snapi-api-card" markdown="1">
### `enum EFieldFlagBits`

Field-level flags for reflection metadata.

**Values**

- `None`: No special field behavior flags.
- `Replication`: Field is eligible for replication payload traversal.
</div>
<div class="snapi-api-card" markdown="1">
### `enum EMethodFlagBits`

Method-level flags for reflection metadata.

**Values**

- `None`: No special method behavior flags.
- `RpcReliable`: Prefer reliable transport channel for RPC dispatch.
- `RpcUnreliable`: Prefer unreliable transport channel for RPC dispatch.
- `RpcNetServer`: Method is intended as server-target endpoint.
- `RpcNetClient`: Method is intended as client-target endpoint.
- `RpcNetMulticast`: Method is intended for server-initiated multicast dispatch.
</div>

## Type Aliases

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TExpected = std::expected<T, Error>`

Convenience alias for std::expected with framework Error.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::Result = TExpected<void>`

Convenience alias for operations returning only success/failure.
</div>
<div class="snapi-api-card" markdown="1">
### `typedef THandle< BaseNode > SnAPI::GameFramework::NodeHandle = THandle<BaseNode>`

Handle type for nodes.

Handle type for nodes (local alias).
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ComponentHandle = THandle<IComponent>`

Handle type for components.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::MethodInvoker = std::function<TExpected<Variant>(void* Instance, std::span<const Variant> Args)>`

Function type for reflected method invocation.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::ScriptInstanceId = uint64_t`

Unique identifier for a script instance.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TTypeRegisterFn = void(*)()`

Auto-registration helpers for reflection and serialization.

Usage (place in a single .cpp per type to avoid duplicate registration): SNAPI_REFLECT_TYPE(MyType, (TTypeBuilder<MyType>(MyType::kTypeName) .Base<BaseNode>() .Field("Health", &MyType::m_health) .Constructor<>() .Register()));

SNAPI_REFLECT_COMPONENT(MyComponent, (TTypeBuilder<MyComponent>(MyComponent::kTypeName) .Field("Speed", &MyComponent::m_speed) .Constructor<>() .Register()));

The builder expression should register the type with TypeRegistry. If a node must be created by TypeId (serialization, scripting), register a default constructor. Types are registered lazily: the macro installs an "ensure" callback keyed by deterministic TypeId. The actual TypeRegistry registration is performed on first use (TypeRegistry::Find on miss, or explicit TypeAutoRegistry::Ensure).
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::FieldFlags = TFlags<EFieldFlagBits>`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::MethodFlags = TFlags<EMethodFlagBits>`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::Uuid = uuids::uuid`

UUID type used throughout the framework.
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TypeId = Uuid`

Strong alias for TypeId values.
</div>

## Variables

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindNodeGraphName`

Asset kind name for NodeGraph assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindLevelName`

Asset kind name for Level assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kAssetKindWorldName`

Asset kind name for World assets.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadNodeGraphName`

Payload type name for NodeGraph cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadLevelName`

Payload type name for Level cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::kPayloadWorldName`

Payload type name for World cooked data.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::EnableFlagsV`
</div>
<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::TTypeNameV`

Convenience alias for TTypeName<T>::Value.
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::RegisterAssetPipelinePayloads(::SnAPI::AssetPipeline::PayloadRegistry &Registry)`

Register GameFramework payload serializers with the AssetPipeline registry.

**Parameters**

- `Registry`: Payload registry.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::RegisterAssetPipelineFactories(::SnAPI::AssetPipeline::AssetManager &Manager)`

Register GameFramework runtime factories with the AssetManager.

**Parameters**

- `Manager`: Asset manager.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::Uuid SnAPI::GameFramework::AssetPipelineNamespace()`

Namespace UUID for AssetPipeline ids.

**Returns:** Namespace UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetPipelineTypeIdFromName(std::string_view Name)`

Generate a deterministic TypeId from a name.

**Parameters**

- `Name`: Name string.

**Returns:** UUIDv5-based TypeId.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::AssetId SnAPI::GameFramework::AssetPipelineAssetIdFromName(std::string_view Name)`

Generate a deterministic AssetId from a name.

**Parameters**

- `Name`: Name string.

**Returns:** UUIDv5-based AssetId.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindNodeGraph()`

Get the AssetPipeline TypeId for NodeGraph assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindLevel()`

Get the AssetPipeline TypeId for Level assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::AssetKindWorld()`

Get the AssetPipeline TypeId for World assets.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadNodeGraph()`

Get the payload TypeId for NodeGraph payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadLevel()`

Get the payload TypeId for Level payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `inline ::SnAPI::AssetPipeline::TypeId SnAPI::GameFramework::PayloadWorld()`

Get the payload TypeId for World payloads.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer > SnAPI::GameFramework::CreateNodeGraphPayloadSerializer()`

Create the payload serializer for NodeGraph cooked data.

Create the NodeGraph payload serializer.

**Returns:** Serializer instance.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer > SnAPI::GameFramework::CreateLevelPayloadSerializer()`

Create the payload serializer for Level cooked data.

Create the Level payload serializer.

**Returns:** Serializer instance.
</div>
<div class="snapi-api-card" markdown="1">
### `std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer > SnAPI::GameFramework::CreateWorldPayloadSerializer()`

Create the payload serializer for World cooked data.

Create the World payload serializer.

**Returns:** Serializer instance.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Ok()`

Construct a success Result.

**Returns:** Result with no error.

**Notes**

- Use for functions returning Result.
</div>
<div class="snapi-api-card" markdown="1">
### `Error SnAPI::GameFramework::MakeError(EErrorCode Code, std::string Message)`

Construct an Error value.

**Parameters**

- `Code`: Error category.
- `Message`: Diagnostic message.

**Returns:** Error instance with the provided data.
</div>
<div class="snapi-api-card" markdown="1">
### `std::enable_if_t< EnableFlagsV< Enum >, TFlags< Enum > > SnAPI::GameFramework::operator|(Enum Left, Enum Right)`

Combine two enum flag bits into a TFlags value.

**Parameters**

- `Left`: 
- `Right`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::enable_if_t< EnableFlagsV< Enum >, TFlags< Enum > > SnAPI::GameFramework::operator&(Enum Left, Enum Right)`

Intersect two enum flag bits into a TFlags value.

**Parameters**

- `Left`: 
- `Right`:
</div>
<div class="snapi-api-card" markdown="1">
### `MethodInvoker SnAPI::GameFramework::MakeInvoker(R(T::*Method)(Args...))`

Create a MethodInvoker for a non-const member function.

Create a MethodInvoker for a const member function.

**Parameters**

- `Method`: Const member function pointer.

**Returns:** Callable MethodInvoker.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::operator+(Vec3 Left, const Vec3 &Right)`

Vector addition.

**Parameters**

- `Left`: Left-hand vector.
- `Right`: Right-hand vector.

**Returns:** Sum of the two vectors.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::operator-(Vec3 Left, const Vec3 &Right)`

Vector subtraction.

**Parameters**

- `Left`: Left-hand vector.
- `Right`: Right-hand vector.

**Returns:** Difference of the two vectors.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::operator*(Vec3 Left, float Scalar)`

Scalar multiplication (vector * scalar).

**Parameters**

- `Left`: Vector.
- `Scalar`: Scalar multiplier.

**Returns:** Scaled vector.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::operator*(float Scalar, Vec3 Right)`

Scalar multiplication (scalar * vector).

**Parameters**

- `Scalar`: Scalar multiplier.
- `Right`: Vector.

**Returns:** Scaled vector.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::SerializeNodeGraphPayload(const NodeGraphPayload &Payload, std::vector< uint8_t > &OutBytes)`

Serialize a NodeGraphPayload to bytes.

**Parameters**

- `Payload`: Payload to serialize.
- `OutBytes`: Output byte vector.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< NodeGraphPayload > SnAPI::GameFramework::DeserializeNodeGraphPayload(const uint8_t *Bytes, size_t Size)`

Deserialize a NodeGraphPayload from bytes.

**Parameters**

- `Bytes`: Byte buffer.
- `Size`: Byte count.

**Returns:** Payload or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::SerializeLevelPayload(const LevelPayload &Payload, std::vector< uint8_t > &OutBytes)`

Serialize a LevelPayload to bytes.

**Parameters**

- `Payload`: Payload to serialize.
- `OutBytes`: Output byte vector.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< LevelPayload > SnAPI::GameFramework::DeserializeLevelPayload(const uint8_t *Bytes, size_t Size)`

Deserialize a LevelPayload from bytes.

**Parameters**

- `Bytes`: Byte buffer.
- `Size`: Byte count.

**Returns:** Payload or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< void > SnAPI::GameFramework::SerializeWorldPayload(const WorldPayload &Payload, std::vector< uint8_t > &OutBytes)`

Serialize a WorldPayload to bytes.

**Parameters**

- `Payload`: Payload to serialize.
- `OutBytes`: Output byte vector.

**Returns:** Success or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< WorldPayload > SnAPI::GameFramework::DeserializeWorldPayload(const uint8_t *Bytes, size_t Size)`

Deserialize a WorldPayload from bytes.

**Parameters**

- `Bytes`: Byte buffer.
- `Size`: Byte count.

**Returns:** Payload or error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::RegisterSerializationDefaults()`

Register default serialization codecs and component serializers.
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::StaticTypeId()`

Get the deterministic TypeId for a type, cached in a function-local static.

**Returns:** Stable TypeId reference.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< TypeId * > SnAPI::GameFramework::StaticType()`

Ensure a type is registered in TypeRegistry and return its TypeId pointer.

**Returns:** Pointer to a stable TypeId on success, or error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::EnsureReflectionRegistered()`

Ensure reflection registration for a type.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::RegisterBuiltinTypes()`

Register built-in types and default serializers.

**Notes**

- Safe to call multiple times; duplicate registrations are ignored or fail gracefully.
</div>
<div class="snapi-api-card" markdown="1">
### `const Uuid & SnAPI::GameFramework::TypeIdNamespace()`

Namespace UUID for deterministic type id generation.

**Returns:** Stable namespace UUID.

**Notes**

- Keep this stable across versions for serialized compatibility.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::TypeIdFromName(std::string_view Name)`

Generate a stable TypeId from a fully qualified name.

**Parameters**

- `Name`: Fully qualified type name.

**Returns:** UUIDv5 derived from the name and TypeIdNamespace.
</div>
<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::NewUuid()`

Generate a new random UUID (UUIDv4).

**Returns:** Newly generated UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::ToString(const Uuid &Id)`

Convert a UUID to its canonical string form.

**Parameters**

- `Id`: UUID to convert.

**Returns:** Lowercase UUID string.
</div>
<div class="snapi-api-card" markdown="1">
### `UuidParts SnAPI::GameFramework::ToParts(const Uuid &Id)`

Convert a UUID to a split High/Low representation.

**Parameters**

- `Id`: UUID to split.

**Returns:** UuidParts containing the high/low 64-bit values.
</div>
<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::FromParts(UuidParts Parts)`

Reconstruct a UUID from split High/Low parts.

**Parameters**

- `Parts`: High/Low representation.

**Returns:** Reconstructed UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::RegisterAssetPipelinePlugin(::SnAPI::AssetPipeline::IPluginRegistrar &Registrar)`

Register the GameFramework AssetPipeline plugin.

**Parameters**

- `Registrar`: AssetPipeline plugin registrar.
</div>
<div class="snapi-api-card" markdown="1">
### `&PerfComponentB::m_value Constructor() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Base< BaseNode >() .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Base< NodeGraph >() .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Field("Position", &TransformComponent::Position) .Field("Rotation"`
</div>
<div class="snapi-api-card" markdown="1">
### `&TransformComponent::Rotation SnAPI::GameFramework::Field("Scale", &TransformComponent::Scale) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Field("ScriptModule", &ScriptComponent::ScriptModule) .Field("ScriptType"`
</div>
<div class="snapi-api-card" markdown="1">
### `&ScriptComponent::ScriptType SnAPI::GameFramework::Field("Instance", &ScriptComponent::Instance) .Constructor<>() .Register()))`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &ArchiveRef, NodeComponentPayload &Value)`

cereal serialize for NodeComponentPayload.

**Parameters**

- `ArchiveRef`: Archive.
- `Value`: Payload to serialize.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &ArchiveRef, NodePayload &Value)`

cereal serialize for NodePayload.

**Parameters**

- `ArchiveRef`: Archive.
- `Value`: Payload to serialize.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &ArchiveRef, NodeGraphPayload &Value)`

cereal serialize for NodeGraphPayload.

**Parameters**

- `ArchiveRef`: Archive.
- `Value`: Payload to serialize.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &ArchiveRef, LevelPayload &Value)`

cereal serialize for LevelPayload.

**Parameters**

- `ArchiveRef`: Archive.
- `Value`: Payload to serialize.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::serialize(Archive &ArchiveRef, WorldPayload &Value)`

cereal serialize for WorldPayload.

**Parameters**

- `ArchiveRef`: Archive.
- `Value`: Payload to serialize.
</div>
