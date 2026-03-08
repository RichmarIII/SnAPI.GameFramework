# SnGfUuid

C ABI representation of a GameFramework UUID.

The struct is intentionally POD and language-neutral so foreign runtimes can pass type and object ids across the C boundary without depending on C++ layout rules.

## Public Members

<div class="snapi-api-card" markdown="1">
### `uint64_t SnGfUuid::High`

High 64 bits of the UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `uint64_t SnGfUuid::Low`

Low 64 bits of the UUID.
</div>
