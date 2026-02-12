# SnAPI::GameFramework::anonymous_namespace{ScriptABI.cpp}

## Functions

<div class="snapi-api-card" markdown="1">
### `SnGfUuid SnAPI::GameFramework::anonymous_namespace{ScriptABI.cpp}::ToC(const TypeId &Id)`

Convert a TypeId to the C ABI UUID struct.

**Parameters**

- `Id`: TypeId to convert.

**Returns:** C ABI UUID representation.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::anonymous_namespace{ScriptABI.cpp}::FromC(const SnGfUuid &Id)`

Convert a C ABI UUID struct to a TypeId.

**Parameters**

- `Id`: C ABI UUID.

**Returns:** TypeId value.
</div>
<div class="snapi-api-card" markdown="1">
### `uint64_t SnAPI::GameFramework::anonymous_namespace{ScriptABI.cpp}::InvalidHandle()`

Get the sentinel value used for invalid handles.

**Returns:** Sentinel handle value.
</div>
<div class="snapi-api-card" markdown="1">
### `Variant * SnAPI::GameFramework::anonymous_namespace{ScriptABI.cpp}::FromHandle(SnGfVariantHandle Handle)`

Convert a variant handle to a Variant pointer.

**Parameters**

- `Handle`: Variant handle.

**Returns:** Variant pointer or nullptr.
</div>
<div class="snapi-api-card" markdown="1">
### `SnGfVariantHandle SnAPI::GameFramework::anonymous_namespace{ScriptABI.cpp}::ToHandle(Variant *Ptr)`

Convert a Variant pointer to a handle.

**Parameters**

- `Ptr`: Variant pointer.

**Returns:** Variant handle.
</div>
