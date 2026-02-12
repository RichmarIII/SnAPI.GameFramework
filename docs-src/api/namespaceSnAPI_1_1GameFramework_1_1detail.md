# SnAPI::GameFramework::detail

## Contents

- **Type:** SnAPI::GameFramework::detail::TArgStorage

## Type Aliases

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::detail::TArgStorageT = typename TArgStorage<Arg>::Type`

Helper alias for argument storage type.
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::detail::DebugAssertFail(const char *File, int Line, const char *Condition, const std::string &Message)`

Internal handler for failed debug assertions.

**Parameters**

- `File`: Source file where the assertion failed.
- `Line`: Line number where the assertion failed.
- `Condition`: Stringified assertion condition.
- `Message`: Formatted diagnostic message.

**Notes**

- This function always terminates the process via std::abort().
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< TArgStorageT< Arg > > SnAPI::GameFramework::detail::ExtractArg(const Variant &Value)`

Extract a typed argument from a Variant.

**Parameters**

- `Value`: Variant to extract from.

**Returns:** Storage wrapper containing the argument or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Arg SnAPI::GameFramework::detail::ConvertArg(TArgStorageT< Arg > &Storage)`

Convert storage wrapper to the actual argument type.

**Parameters**

- `Storage`: Storage wrapper.

**Returns:** Argument value or reference.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Variant > SnAPI::GameFramework::detail::InvokeImpl(T *Instance, R(T::*Method)(Args...), std::span< const Variant > ArgsPack, std::index_sequence< I... >)`

Invoke a non-const member function with reflected args.

**Parameters**

- `Instance`: Pointer to instance.
- `Method`: Member function pointer.
- `ArgsPack`: Packed arguments.

**Returns:** Variant containing the result or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Variant > SnAPI::GameFramework::detail::InvokeConstImpl(const T *Instance, R(T::*Method)(Args...) const, std::span< const Variant > ArgsPack, std::index_sequence< I... >)`

Invoke a const member function with reflected args.

**Parameters**

- `Instance`: Pointer to const instance.
- `Method`: Const member function pointer.
- `ArgsPack`: Packed arguments.

**Returns:** Variant containing the result or error.
</div>
