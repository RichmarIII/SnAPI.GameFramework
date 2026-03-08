# SnAPI::GameFramework::TTypeBuilder

Fluent builder for registering reflection metadata for one type.

`TTypeBuilder<T>` accumulates a `TypeInfo` record and then commits it into `TypeRegistry`.

Design responsibilities:
- declare direct base-type relationships
- describe fields as readable, writable, or read-write reflected properties
- describe reflected methods and constructors
- automatically bridge supported node `OnCreate` and editor property-change callbacks
- automatically register component serialization when `T` derives from `BaseComponent`

Best-practice lifecycle:
1. add base types, fields, methods, and constructors
2. call `Register()` exactly once in one translation unit, usually through `SNAPI_REFLECT_TYPE`
3. let `TypeAutoRegistry` ensure the metadata on first use

Threading model:
- Building is single-threaded, local-value work.
- Registration delegates to `TypeRegistry`, which is process-global and synchronized.

## Contents

- **Type:** SnAPI::GameFramework::TTypeBuilder::TGetterMethodTraits
- **Type:** SnAPI::GameFramework::TTypeBuilder::TGetterMethodTraits< R(T::*)()>
- **Type:** SnAPI::GameFramework::TTypeBuilder::TGetterMethodTraits< R(T::*)() noexcept >
- **Type:** SnAPI::GameFramework::TTypeBuilder::TGetterMethodTraits< R(T::*)() const >
- **Type:** SnAPI::GameFramework::TTypeBuilder::TGetterMethodTraits< R(T::*)() const noexcept >
- **Type:** SnAPI::GameFramework::TTypeBuilder::TSetterMethodTraits
- **Type:** SnAPI::GameFramework::TTypeBuilder::TSetterMethodTraits< R(T::*)(Arg)>
- **Type:** SnAPI::GameFramework::TTypeBuilder::TSetterMethodTraits< R(T::*)(Arg) noexcept >
- **Type:** SnAPI::GameFramework::TTypeBuilder::TSetterMethodTraits< R(T::*)(Arg) const >
- **Type:** SnAPI::GameFramework::TTypeBuilder::TSetterMethodTraits< R(T::*)(Arg) const noexcept >

## Private Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TTypeBuilder< T >::TGetterTraits = TGetterMethodTraits<std::remove_cvref_t<GetterMethod>>`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TTypeBuilder< T >::TSetterTraits = TSetterMethodTraits<std::remove_cvref_t<SetterMethod>>`
</div>

## Private Static Attrib

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TTypeBuilder< T >::IsGetterMethodV`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TTypeBuilder< T >::IsSetterMethodV`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TTypeBuilder< T >::IsSupportedSetterReturnV`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TTypeBuilder< T >::IsEqualityComparableV`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TTypeBuilder< T >::SupportsNodeOnCreateWithWorldV`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TTypeBuilder< T >::SupportsNodeOnCreateNoWorldV`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TTypeBuilder< T >::HasDeclaredNodeOnCreateV`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `TypeInfo SnAPI::GameFramework::TTypeBuilder< T >::m_info`

Accumulated type metadata.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static bool SnAPI::GameFramework::TTypeBuilder< T >::ValuesEqual(const TValue &Left, const TValue &Right)`

**Parameters**

- `Left`: 
- `Right`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TTypeBuilder< T >::InvokeDeclaredNodeOnCreate(T &Typed, IWorld *const WorldRef)`

**Parameters**

- `Typed`: 
- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TTypeBuilder< T >::InvokeNodeOnCreateCallback(void *const Instance, IWorld *const WorldRef)`

**Parameters**

- `Instance`: 
- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `static TypeInfo::NodeOnCreateInvoker SnAPI::GameFramework::TTypeBuilder< T >::NodeOnCreateInvokerForType()`
</div>
<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::TTypeBuilder< T >::NotifyEditorPropertyChangedIfNeeded(T &Typed, const std::string_view FieldName, const bool Changed)`

**Parameters**

- `Typed`: 
- `FieldName`: 
- `Changed`:
</div>
<div class="snapi-api-card" markdown="1">
### `static TExpected< Variant > SnAPI::GameFramework::TTypeBuilder< T >::BuildGetterVariant(T *Typed, GetterMethod Getter)`

**Parameters**

- `Typed`: 
- `Getter`:
</div>
<div class="snapi-api-card" markdown="1">
### `static Result SnAPI::GameFramework::TTypeBuilder< T >::ApplySetter(T *Typed, SetterMethod Setter, const Variant &Value)`

**Parameters**

- `Typed`: 
- `Setter`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `static TExpected< std::shared_ptr< void > > SnAPI::GameFramework::TTypeBuilder< T >::ConstructImpl(std::span< const Variant > ArgsPack, std::index_sequence< I... >)`

Construct an instance from a Variant argument pack.

**Parameters**

- `ArgsPack`: Argument span.

**Returns:** Shared pointer to the constructed object.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TTypeBuilder< T >::TTypeBuilder(const char *Name)`

Construct a builder for a reflected type name.

The builder derives `TypeId` from `Name`, captures `sizeof(T)` / `alignof(T)`, and wires any supported node/editor callback shims into the pending `TypeInfo`.

**Parameters**

- `Name`: Fully qualified stable reflected type name.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Base()`

Register one direct reflected base type.

The base type is lazily ensured in `TypeRegistry` as a side effect. Base relationships are used by:
- `TypeRegistry::IsA()` and `Derived()`
- inherited field and method collection
- reflected type compatibility checks in systems like assets and subclass selection

**Returns:** Builder reference for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Field(const char *Name, FieldT T::*Member, FieldFlags Flags={})`

Reflect a data member through a pointer-to-member.

Generated metadata includes:
- variant getter/setter access
- non-owning `VariantView` access
- direct const/mutable pointer accessors for hot paths

For non-const fields, reflected writes compare old vs new values when possible and notify the editor property-changed hook only when the value actually changed.

**Parameters**

- `Name`: Stable reflected field name.
- `Member`: Pointer-to-member field.
- `Flags`: Optional field flags.

**Returns:** Builder reference for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Field(const char *Name, GetterMethod Getter, FieldFlags Flags={})`

Reflect a read-only field through a getter method.

Getter return may be by value or by reference. Reference-returning getters additionally expose `VariantView` and raw-pointer read access where possible.

**Parameters**

- `Name`: Stable reflected field name.
- `Getter`: Getter method.
- `Flags`: Optional field flags.

**Returns:** Builder reference for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Field(const char *Name, SetterMethod Setter, FieldFlags Flags={})`

Reflect a write-only field through a setter method.

Supported setter return contracts:
- `void`: assignment always succeeds
- `bool`: `false` is treated as a rejected value
- `Result`: full error propagation

**Parameters**

- `Name`: Stable reflected field name.
- `Setter`: Setter method.
- `Flags`: Optional field flags.

**Returns:** Builder reference for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Field(const char *Name, GetterMethod Getter, SetterMethod Setter, FieldFlags Flags={})`

Reflect a read-write field through getter and setter methods.

When the getter return type is copy-constructible and equality comparable, reflected writes compare pre- and post-set values so editor property-change notifications fire only on real changes.

**Parameters**

- `Name`: Stable reflected field name.
- `Getter`: Getter method.
- `Setter`: Setter method.
- `Flags`: Optional field flags.

**Returns:** Builder reference for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Field(const char *Name, GetterReturn(T::*Getter)(), SetterReturn(T::*Setter)(SetterArg), FieldFlags Flags={})`

Overload bridge for getter/setter pairs that share the same member name.

This allows declarations such as `Field("Name", &Type::Name, &Type::Name)` where overload resolution would otherwise be ambiguous.

**Parameters**

- `Name`: 
- `Getter`: 
- `Setter`: 
- `Flags`:
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Field(const char *Name, GetterReturn(T::*Getter)() const, SetterReturn(T::*Setter)(SetterArg), FieldFlags Flags={})`

**Parameters**

- `Name`: 
- `Getter`: 
- `Setter`: 
- `Flags`:
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Field(const char *Name, FieldT &(T::*Getter)(), const FieldT &(T::*GetterConst)() const, FieldFlags Flags={})`

Reflect a read-write field through an editable-reference accessor and a const getter.

This bridge exists for the engine's common `EditX()/GetX()` pattern, where a type exposes a mutable reference for in-place editor/gameplay mutation and a separate const accessor for read-only reflection and serialization.

Semantics:
- Reflected reads use `Getter`.
- Reflected writes assign through the reference returned by `Getter`.
- Const pointer/view access uses `GetterConst`.
- Editor property-change notifications are emitted only when the assigned value actually changes.

**Parameters**

- `Name`: Stable reflected field name.
- `Getter`: Mutable accessor that returns the stored field by non-const reference.
- `GetterConst`: Read-only accessor that returns the same field by const reference.
- `Flags`: Optional field flags.

**Returns:** Builder reference for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Method(const char *Name, R(T::*Method)(Args...), MethodFlags Flags={})`

Reflect a non-const method.

Invocation is bridged through `MakeInvoker()` so callers can use type-erased `Variant` argument packs.

**Parameters**

- `Name`: Stable reflected method name.
- `Method`: 
- `Flags`: Optional method flags.

**Returns:** Builder reference for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Method(const char *Name, R(T::*Method)(Args...) const, MethodFlags Flags={})`

Reflect a const method.

Constness is stored in metadata and exposed to callers through `MethodInfo::IsConst`.

**Parameters**

- `Name`: Stable reflected method name.
- `Method`: 
- `Flags`: Optional method flags.

**Returns:** Builder reference for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TTypeBuilder & SnAPI::GameFramework::TTypeBuilder< T >::Constructor()`

Reflect a constructor signature.

Constructor metadata powers runtime creation by `TypeId` in systems such as serialization, script binding, editor creation flows, and component registration helpers.

**Returns:** Builder reference for chaining.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< TypeInfo * > SnAPI::GameFramework::TTypeBuilder< T >::Register()`

Commit the accumulated `TypeInfo` into the global `TypeRegistry`.

Additional side effects:
- if `T` derives from `BaseComponent`, `ComponentSerializationRegistry::Register<T>()` is attempted
- node `OnCreate` and editor property-changed callback shims are already embedded in the metadata

**Returns:** Pointer to the stored `TypeInfo` or an error.
</div>
