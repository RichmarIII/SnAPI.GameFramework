# SnAPI::GameFramework::ScriptBindings

Reflection-validation entry point for script-binding registration.

`ScriptBindings` currently acts as a minimal front door for script backends that need to ensure a type is reflected before generating or attaching bindings.

Current semantics:
- No backend-specific binding tables are emitted here yet.
- `RegisterType<T>()` succeeds only when `T` is already present in `TypeRegistry`.
- Missing reflection metadata is reported as `EErrorCode::NotFound`.

This keeps the public API stable while the concrete scripting backends evolve.

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `static TExpected< void > SnAPI::GameFramework::ScriptBindings::RegisterType()`

Validate that a type is reflected and therefore eligible for scripting integration.

Backends can extend this pattern to emit VM bindings, native thunks, or ABI glue after the metadata presence check succeeds.

**Returns:** Success when reflection metadata exists for `T`; otherwise an error.
</div>
