# SnAPI::GameFramework::TypeInfo

Central reflection metadata record for one type.

`TypeInfo` is the contract object consumed by serialization, replication, RPC, editor tooling, and type-erased construction. It intentionally stores only runtime-usable metadata and callback entry points.

Ownership and lifetime:
- Once registered, the stored `TypeInfo` lives inside `TypeRegistry` for the process lifetime.
- Callback pointers and lambdas must therefore remain valid for the life of the process.

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TypeInfo::NodeOnCreateInvoker = void(*)(void* Instance, IWorld* WorldRef)`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::TypeInfo::Id`

Deterministic type id.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::TypeInfo::Name`

Fully qualified stable reflected type name.
</div>
<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::TypeInfo::Size`

`sizeof(T)` for plain reflected types, or `0` for synthetic marker types like `void`.
</div>
<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::TypeInfo::Align`

`alignof(T)` for plain reflected types, or `0` for synthetic marker types like `void`.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TypeId> SnAPI::GameFramework::TypeInfo::BaseTypes`

Direct reflected base types.

Transitive relationships are derived by traversal at query time.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<FieldInfo> SnAPI::GameFramework::TypeInfo::Fields`

Field metadata declared directly on this type.

Inherited fields are discovered through `TypeRegistry::CollectFields()`.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<MethodInfo> SnAPI::GameFramework::TypeInfo::Methods`

Method metadata declared directly on this type.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<ConstructorInfo> SnAPI::GameFramework::TypeInfo::Constructors`

Reflected constructors available for type-erased creation.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TypeInfo::IsEnum`

`true` when this type record represents an enum.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TypeInfo::EnumIsSigned`

`true` when the enum underlying type is signed.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<EnumValueInfo> SnAPI::GameFramework::TypeInfo::EnumValues`

Enum entries for tooling, serialization, and UI.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeOnCreateInvoker SnAPI::GameFramework::TypeInfo::NodeOnCreate`

Optional node `OnCreate` callback installed by `TTypeBuilder` for `BaseNode`-derived types.
</div>
