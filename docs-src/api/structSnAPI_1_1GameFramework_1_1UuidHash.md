# SnAPI::GameFramework::UuidHash

Hash functor for `Uuid`.

Enables `Uuid` and `TypeId` use in unordered containers.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::UuidHash::operator()(const Uuid &Id) const noexcept`

Compute a hash value for a UUID.

**Parameters**

- `Id`: UUID to hash.

**Returns:** Hash value.

**Notes**

- Combines High/Low with a 64-bit mix.
</div>
