# SnAPI::GameFramework::detail

## Contents

- **Type:** SnAPI::GameFramework::detail::TaskState
- **Type:** SnAPI::GameFramework::detail::TArgStorage

## Type Aliases

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::detail::TArgStorageT = typename TArgStorage<Arg>::Type`

Helper alias for the extracted storage type of one reflected argument.
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< TArgStorageT< Arg > > SnAPI::GameFramework::detail::ExtractArg(const Variant &Value)`

Extract one typed argument from a `Variant`.

Mutable lvalue references require a mutable reference payload in `Value`.

**Parameters**

- `Value`: Variant to extract from.

**Returns:** Storage wrapper containing the argument or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `Arg SnAPI::GameFramework::detail::ConvertArg(TArgStorageT< Arg > &Storage)`

Convert extracted storage to the actual invocation argument type.

**Parameters**

- `Storage`: Extracted storage wrapper.

**Returns:** Value copy or lvalue reference as required by `Arg`.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Variant > SnAPI::GameFramework::detail::InvokeImpl(T *Instance, R(T::*Method)(Args...), std::span< const Variant > ArgsPack, std::index_sequence< I... >)`

Invoke a non-const member function with reflected args.

**Parameters**

- `Instance`: Pointer to instance.
- `Method`: 
- `ArgsPack`: Packed arguments.

**Returns:** Variant containing the result or error.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Variant > SnAPI::GameFramework::detail::InvokeConstImpl(const T *Instance, R(T::*Method)(Args...) const, std::span< const Variant > ArgsPack, std::index_sequence< I... >)`

Invoke a const member function with reflected args.

**Parameters**

- `Instance`: Pointer to const instance.
- `Method`: 
- `ArgsPack`: Packed arguments.

**Returns:** Variant containing the result or error.
</div>
