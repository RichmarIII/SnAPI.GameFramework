# SnAPI::GameFramework::Variant

Type-erased value container used by reflection and scripting.

## Private Members

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Variant::m_type`

Type id of the stored value.
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<void> SnAPI::GameFramework::Variant::m_storage`

Owned value or referenced pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::m_isRef`

True if the variant holds a reference.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::m_isConst`

True if the reference is const.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Variant::Variant()=default`

Construct an empty (void) variant.
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::Variant::Type() const`

Get the stored type id.

**Returns:** TypeId for the stored value.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::IsVoid() const`

Check whether this is a void variant.

**Returns:** True if the variant represents void.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::IsRef() const`

Check whether this variant stores a reference.

**Returns:** True if it stores a reference; false if it owns the value.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::IsConst() const`

Check whether a referenced value is const.

**Returns:** True if reference is const.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::Variant::Borrowed()`

Borrow the underlying pointer (mutable).

**Returns:** Pointer to stored value or reference.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::Variant::Borrowed() const`

Borrow the underlying pointer (const).

**Returns:** Pointer to stored value or reference.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Variant::Is() const`

Type check helper.

**Returns:** True if the stored type matches T.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< std::reference_wrapper< T > > SnAPI::GameFramework::Variant::AsRef()`

Get a mutable reference to the stored value.

**Returns:** Reference wrapper on success; error otherwise.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< std::reference_wrapper< const T > > SnAPI::GameFramework::Variant::AsConstRef() const`

Get a const reference to the stored value.

**Returns:** Const reference wrapper on success; error otherwise.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static Variant SnAPI::GameFramework::Variant::Void()`

Create a void variant.

**Returns:** Variant representing void.
</div>
<div class="snapi-api-card" markdown="1">
### `static Variant SnAPI::GameFramework::Variant::FromValue(T Value)`

Create a variant that owns a value.

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

**Notes**

- Caller must ensure the referenced object outlives the Variant.
</div>
<div class="snapi-api-card" markdown="1">
### `static Variant SnAPI::GameFramework::Variant::FromConstRef(const T &Value)`

Create a variant that references a const object.

**Parameters**

- `Value`: Const reference to the object.

**Returns:** Variant referencing the object as const.

**Notes**

- Caller must ensure the referenced object outlives the Variant.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static const TypeId & SnAPI::GameFramework::Variant::VoidTypeId()`
</div>
<div class="snapi-api-card" markdown="1">
### `static const TypeId & SnAPI::GameFramework::Variant::CachedTypeId()`
</div>
