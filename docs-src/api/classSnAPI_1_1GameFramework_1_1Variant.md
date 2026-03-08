# SnAPI::GameFramework::Variant

Type-erased value container used by reflection, scripting, and generic invocation.

`Variant` stores either:
- an owned heap-allocated value
- a borrowed mutable reference
- a borrowed const reference
- a distinguished `void` marker

Core semantics:
- Type identity is tracked by deterministic reflected `TypeId`.
- Owned values use shared heap storage so `Variant` remains cheap to copy.
- Reference variants do not own the referenced object; callers must guarantee lifetime.
- Reference constness is enforced by `AsRef()`.

Threading model:
- Copying and moving the `Variant` object is thread-safe in isolation.
- Access to referenced payloads follows the thread-safety rules of the referenced object.

## Private Members

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Variant::m_type`

Reflected type id of stored payload.
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<void> SnAPI::GameFramework::Variant::m_storage`

Owned object storage or non-owning reference wrapper pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::m_isRef`

Reference mode flag (`true` for non-owning reference payload).
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::m_isConst`

Const-reference qualifier for reference mode payloads.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Variant::Variant()=default`

Construct an empty variant.

The default state behaves like a void variant with no payload storage.
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::Variant::Type() const`

Get the stored reflected type id.

**Returns:** Type id for the stored value or the void marker type.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::IsVoid() const`

Check whether this variant represents `void`.

**Returns:** `true` when the stored type id is the void marker type.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::IsRef() const`

Check whether this variant stores a borrowed reference.

**Returns:** `true` for borrowed reference payloads, `false` for owned values and void.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::IsConst() const`

Check whether the stored reference payload is const-qualified.

**Returns:** `true` only for const-reference payloads.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::Variant::Borrowed()`

Borrow the underlying payload pointer as mutable.

**Returns:** Raw payload pointer, or `nullptr` when no payload exists.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::Variant::Borrowed() const`

Borrow the underlying payload pointer as const.

**Returns:** Raw payload pointer, or `nullptr` when no payload exists.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::Is() const`

Check whether the stored payload type matches `T`.

**Returns:** `true` when the stored reflected type id equals `T`.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< std::reference_wrapper< T > > SnAPI::GameFramework::Variant::AsRef()`

Extract a mutable reference to the stored payload.

Fails when:
- the stored type does not match `T`
- the payload is a const reference
- no payload storage exists

**Returns:** Reference wrapper on success; error otherwise.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< std::reference_wrapper< const T > > SnAPI::GameFramework::Variant::AsConstRef() const`

Extract a const reference to the stored payload.

**Returns:** Const reference wrapper on success; error otherwise.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static Variant SnAPI::GameFramework::Variant::Void()`

Create an explicit void variant.

**Returns:** Variant representing void.
</div>
<div class="snapi-api-card" markdown="1">
### `static Variant SnAPI::GameFramework::Variant::FromValue(T Value)`

Create a variant that owns a value.

The value is copied or moved into heap storage owned by the variant.

**Parameters**

- `Value`: Value to store (moved or copied).

**Returns:** Variant owning the value.
</div>
<div class="snapi-api-card" markdown="1">
### `static Variant SnAPI::GameFramework::Variant::FromRef(T &Value)`

Create a variant that references a mutable object.

**Parameters**

- `Value`: Reference to the object.

**Returns:** Variant referencing the object.
</div>
<div class="snapi-api-card" markdown="1">
### `static Variant SnAPI::GameFramework::Variant::FromConstRef(const T &Value)`

Create a variant that references a const object.

**Parameters**

- `Value`: Const reference to the object.

**Returns:** Variant referencing the object as const.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static const TypeId & SnAPI::GameFramework::Variant::VoidTypeId()`
</div>
<div class="snapi-api-card" markdown="1">
### `static const TypeId & SnAPI::GameFramework::Variant::CachedTypeId()`
</div>
