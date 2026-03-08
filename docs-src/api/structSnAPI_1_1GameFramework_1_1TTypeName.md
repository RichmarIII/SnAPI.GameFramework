# SnAPI::GameFramework::TTypeName

Trait that provides the canonical stable reflection name for a C++ type.

`TTypeName` is the root of the engine's deterministic type-identity scheme. The string exposed by this trait is used to derive `TypeId` values, drive reflection registration, and label types in serialization, scripting, editor UI, and diagnostics.

Contract:
- The returned name must remain stable once serialized data or network protocols depend on it.
- User-defined engine types usually satisfy this by exposing `static constexpr const char* kTypeName`.
- External or builtin types should specialize the trait with `SNAPI_DEFINE_TYPE_NAME`.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::TTypeName< T >::Value`

Stable fully-qualified type name used for deterministic TypeId generation.
</div>
