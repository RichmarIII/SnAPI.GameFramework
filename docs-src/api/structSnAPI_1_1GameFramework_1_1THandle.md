# SnAPI::GameFramework::THandle

Strongly typed UUID handle for framework objects.

Strongly typed handle that stores a UUID.

## Public Members

<div class="snapi-api-card" markdown="1">
### `Uuid SnAPI::GameFramework::THandle< T >::Id`

UUID of the referenced object.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::THandle< T >::THandle()=default`

Construct a null handle.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::THandle< T >::THandle(Uuid InId)`

Construct a handle from a UUID.

**Parameters**

- `InId`: UUID of the target object.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::THandle< T >::IsNull() const noexcept`

Check if the handle is null.

**Returns:** True when the UUID is nil.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::THandle< T >::operator bool() const noexcept`

Boolean conversion for validity checks.

**Returns:** True when the handle is not null.

**Notes**

- This does not guarantee the object is loaded.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::THandle< T >::operator==(const THandle &Other) const noexcept`

Equality comparison.

**Parameters**

- `Other`: Another handle.

**Returns:** True when UUIDs match.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::THandle< T >::operator!=(const THandle &Other) const noexcept`

Inequality comparison.

**Parameters**

- `Other`: Another handle.

**Returns:** True when UUIDs differ.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::THandle< T >::Borrowed() const`

Resolve to a borrowed pointer (const).

**Returns:** Pointer to the object, or nullptr if not loaded/registered.

**Notes**

- The returned pointer must not be stored.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::THandle< T >::Borrowed()`

Resolve to a borrowed pointer (mutable).

**Returns:** Pointer to the object, or nullptr if not loaded/registered.

**Notes**

- The returned pointer must not be stored.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::THandle< T >::IsValid() const`

Check whether the handle resolves to a live object.

**Returns:** True when the object is registered.
</div>
