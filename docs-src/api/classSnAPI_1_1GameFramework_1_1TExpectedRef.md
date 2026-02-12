# SnAPI::GameFramework::TExpectedRef

Lightweight expected wrapper that stores a reference.

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::expected<std::reference_wrapper<T>, Error> SnAPI::GameFramework::TExpectedRef< T >::m_expected`

Stored expected reference.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TExpectedRef< T >::TExpectedRef()=default`

Construct an empty (invalid) expected reference.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TExpectedRef< T >::TExpectedRef(T &Value)`

Construct a success result from a reference.

**Parameters**

- `Value`: Referenced object.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TExpectedRef< T >::TExpectedRef(std::reference_wrapper< T > Value)`

Construct a success result from a reference wrapper.

**Parameters**

- `Value`: Reference wrapper.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TExpectedRef< T >::TExpectedRef(std::unexpected< Error > ErrorValue)`

Construct a failure result.

**Parameters**

- `ErrorValue`: Error payload.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TExpectedRef< T >::operator bool() const`

Boolean conversion for success checks.

**Returns:** True when a valid reference is present.
</div>
<div class="snapi-api-card" markdown="1">
### `T & SnAPI::GameFramework::TExpectedRef< T >::operator*()`

Dereference to the underlying object.

**Returns:** Reference to the contained object.

**Notes**

- Behavior is undefined if this is in an error state.
</div>
<div class="snapi-api-card" markdown="1">
### `const T & SnAPI::GameFramework::TExpectedRef< T >::operator*() const`

Const dereference to the underlying object.

**Returns:** Const reference to the contained object.

**Notes**

- Behavior is undefined if this is in an error state.
</div>
<div class="snapi-api-card" markdown="1">
### `T * SnAPI::GameFramework::TExpectedRef< T >::operator->()`

Arrow operator access.

**Returns:** Pointer to the contained object.

**Notes**

- Behavior is undefined if this is in an error state.
</div>
<div class="snapi-api-card" markdown="1">
### `const T * SnAPI::GameFramework::TExpectedRef< T >::operator->() const`

Const arrow operator access.

**Returns:** Pointer to the contained object.

**Notes**

- Behavior is undefined if this is in an error state.
</div>
<div class="snapi-api-card" markdown="1">
### `T & SnAPI::GameFramework::TExpectedRef< T >::Get()`

Get the contained reference.

**Returns:** Reference to the object.

**Notes**

- Throws if no value is present.
</div>
<div class="snapi-api-card" markdown="1">
### `const T & SnAPI::GameFramework::TExpectedRef< T >::Get() const`

Get the contained reference (const).

**Returns:** Const reference to the object.

**Notes**

- Throws if no value is present.
</div>
<div class="snapi-api-card" markdown="1">
### `Error & SnAPI::GameFramework::TExpectedRef< T >::error()`

Access the error payload.

**Returns:** Mutable reference to the Error.
</div>
<div class="snapi-api-card" markdown="1">
### `const Error & SnAPI::GameFramework::TExpectedRef< T >::error() const`

Access the error payload (const).

**Returns:** Const reference to the Error.
</div>
<div class="snapi-api-card" markdown="1">
### `T & SnAPI::GameFramework::TExpectedRef< T >::value()`

Access the value, throwing on error.

**Returns:** Reference to the object.
</div>
<div class="snapi-api-card" markdown="1">
### `const T & SnAPI::GameFramework::TExpectedRef< T >::value() const`

Access the value (const), throwing on error.

**Returns:** Const reference to the object.
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< std::reference_wrapper< T >, Error > & SnAPI::GameFramework::TExpectedRef< T >::Raw()`

Access the underlying std::expected.

**Returns:** Mutable reference to the internal std::expected wrapper.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::expected< std::reference_wrapper< T >, Error > & SnAPI::GameFramework::TExpectedRef< T >::Raw() const`

Access the underlying std::expected (const).

**Returns:** Const reference to the internal std::expected wrapper.
</div>
