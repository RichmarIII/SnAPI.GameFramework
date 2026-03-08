# SnAPI::GameFramework::TFlags

Lightweight bitflag wrapper for strongly typed enums.

`TFlags` provides a small value-type wrapper around the enum's underlying integer bits while keeping explicit control over which enums are allowed to participate in free `operator|` / `operator&`.

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::TFlags< Enum >::Underlying = std::underlying_type_t<Enum>`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `Underlying SnAPI::GameFramework::TFlags< Enum >::m_value`

Raw underlying-bit storage for the wrapped enum flags.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TFlags< Enum >::TFlags()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TFlags< Enum >::TFlags(Enum Bits)`

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TFlags< Enum >::TFlags(Underlying Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `Underlying SnAPI::GameFramework::TFlags< Enum >::Value() const`

Get the raw underlying-bit value.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TFlags< Enum >::Empty() const`

Check whether no bits are set.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TFlags< Enum >::Has(Enum Bits) const`

Check whether any bit from `Bits` is set.

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TFlags< Enum >::Add(Enum Bits)`

Set the supplied bits.

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TFlags< Enum >::Remove(Enum Bits)`

Clear the supplied bits.

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TFlags< Enum >::Clear()`

Clear all bits.
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags SnAPI::GameFramework::TFlags< Enum >::operator|(Enum Bits) const`

Return a new flag set with `Bits` added.

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags SnAPI::GameFramework::TFlags< Enum >::operator|(TFlags Other) const`

Return the union of two flag sets.

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags SnAPI::GameFramework::TFlags< Enum >::operator&(Enum Bits) const`

Return the intersection between this set and `Bits`.

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags SnAPI::GameFramework::TFlags< Enum >::operator&(TFlags Other) const`

Return the intersection of two flag sets.

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags & SnAPI::GameFramework::TFlags< Enum >::operator|=(Enum Bits)`

In-place union with `Bits`.

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags & SnAPI::GameFramework::TFlags< Enum >::operator|=(TFlags Other)`

In-place union with another flag set.

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TFlags< Enum >::operator==(TFlags Other) const`

Equality comparison on raw bits.

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TFlags< Enum >::operator!=(TFlags Other) const`

Inequality comparison on raw bits.

**Parameters**

- `Other`:
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static constexpr TFlags SnAPI::GameFramework::TFlags< Enum >::FromRaw(Underlying Value)`

Construct flags directly from raw underlying bits.

**Parameters**

- `Value`:
</div>
