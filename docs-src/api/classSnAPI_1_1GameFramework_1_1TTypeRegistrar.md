# SnAPI::GameFramework::TTypeRegistrar

Tiny helper that runs a function during static initialization.

`TTypeRegistrar` is intentionally minimal: it simply executes a registration thunk when the static object is constructed. The thunk normally registers an ensure callback with `TypeAutoRegistry`, not the full type metadata itself.

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::TTypeRegistrar::TTypeRegistrar(TTypeRegisterFn Fn)`

Construct and invoke the registration function.

**Parameters**

- `Fn`: Function pointer to call.
</div>
