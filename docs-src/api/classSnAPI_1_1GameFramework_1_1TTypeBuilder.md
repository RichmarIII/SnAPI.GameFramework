# SnAPI::GameFramework::TTypeBuilder

Fluent builder for registering reflection metadata.

Best-practice lifecycle:
1. define fields/methods/constructors/base types
2. call `Register()` once in one translation unit (typically through `SNAPI_REFLECT_TYPE`)
3. let `TypeAutoRegistry` ensure-on-first-use resolve registration at runtime

## Private Members

<div class="snapi-api-card" markdown="1">
### `TypeInfo SnAPI::GameFramework::TTypeBuilder< T >::m_info`

Accumulated type metadata.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TTypeBuilder< T >::TTypeBuilder(const char *Name)`

Construct a builder for a type name.

**Parameters**

- `Name`: Fully qualified type name.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Base()`

Register a base type.

**Returns:** Reference to the builder for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Field(const char *Name, FieldT T::*Member, FieldFlags Flags={})`

Register a field with getter/setter support.

Const member fields are reflected as read-only; setter returns error at runtime.

**Parameters**

- `Name`: Field name.
- `Member`: Pointer-to-member field.
- `Flags`: 

**Returns:** Reference to the builder for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Field(const char *Name, FieldT &(T::*Getter)(), const FieldT &(T::*GetterConst)() const, FieldFlags Flags={})`

Register a field via accessors that return references.

**Parameters**

- `Name`: Field name.
- `Getter`: Mutable accessor returning FieldT&.
- `GetterConst`: Const accessor returning const FieldT&.
- `Flags`: 

**Returns:** Reference to the builder for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Method(const char *Name, R(T::*Method)(Args...), MethodFlags Flags={})`

Register a non-const method for reflection.

**Parameters**

- `Name`: Method name.
- `Method`: 
- `Flags`: 

**Returns:** Reference to the builder for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Method(const char *Name, R(T::*Method)(Args...) const, MethodFlags Flags={})`

Register a const method for reflection.

**Parameters**

- `Name`: Method name.
- `Method`: 
- `Flags`: 

**Returns:** Reference to the builder for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Constructor()`

Register a constructor signature.

**Returns:** Reference to the builder for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< TypeInfo * > SnAPI::GameFramework::TTypeBuilder< T >::Register()`

Register the built TypeInfo into the global TypeRegistry.

**Returns:** Pointer to stored TypeInfo or error.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static TExpected< std::shared_ptr< void > > SnAPI::GameFramework::TTypeBuilder< T >::ConstructImpl(std::span< const Variant > ArgsPack, std::index_sequence< I... >)`

Construct an instance from a Variant argument pack.

**Parameters**

- `ArgsPack`: Argument span.

**Returns:** Shared pointer to the constructed object.
</div>
