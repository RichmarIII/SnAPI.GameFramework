# SnAPI::GameFramework::TransparentStringHash

Heterogeneous hash functor for reflected type-name lookup tables.

Supports `std::string` and `std::string_view` without transient allocations.

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TransparentStringHash::is_transparent = void`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::TransparentStringHash::operator()(std::string_view Value) const noexcept`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::TransparentStringHash::operator()(const std::string &Value) const noexcept`

**Parameters**

- `Value`:
</div>
