# SnAPI::GameFramework::TFlags

Bit-flag helper for strongly-typed enums.

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
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TFlags< Enum >::Empty() const`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TFlags< Enum >::Has(Enum Bits) const`

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TFlags< Enum >::Add(Enum Bits)`

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TFlags< Enum >::Remove(Enum Bits)`

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::TFlags< Enum >::Clear()`
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags SnAPI::GameFramework::TFlags< Enum >::operator|(Enum Bits) const`

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags SnAPI::GameFramework::TFlags< Enum >::operator|(TFlags Other) const`

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags SnAPI::GameFramework::TFlags< Enum >::operator&(Enum Bits) const`

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags SnAPI::GameFramework::TFlags< Enum >::operator&(TFlags Other) const`

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags & SnAPI::GameFramework::TFlags< Enum >::operator|=(Enum Bits)`

**Parameters**

- `Bits`:
</div>
<div class="snapi-api-card" markdown="1">
### `TFlags & SnAPI::GameFramework::TFlags< Enum >::operator|=(TFlags Other)`

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TFlags< Enum >::operator==(TFlags Other) const`

**Parameters**

- `Other`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TFlags< Enum >::operator!=(TFlags Other) const`

**Parameters**

- `Other`:
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static constexpr TFlags SnAPI::GameFramework::TFlags< Enum >::FromRaw(Underlying Value)`

**Parameters**

- `Value`:
</div>
