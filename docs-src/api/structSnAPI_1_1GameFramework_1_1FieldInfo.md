# SnAPI::GameFramework::FieldInfo

Reflection metadata for one field-like property.

Field access is intentionally multi-lane:
- `Getter` / `Setter` provide value-semantic access through `Variant`
- `ViewGetter` provides a non-owning `VariantView` fast path
- `ConstPointer` / `MutablePointer` provide raw-address access for hot serialization and replication paths

Not every lane is required to be populated. Read-only and write-only reflected properties are represented by leaving unsupported accessors absent or returning errors.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::FieldInfo::Name`

Field name as registered.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::FieldInfo::FieldType`

TypeId of the field.
</div>
<div class="snapi-api-card" markdown="1">
### `FieldFlags SnAPI::GameFramework::FieldInfo::Flags`

Field flags (replication, etc.).
</div>
<div class="snapi-api-card" markdown="1">
### `std::function<TExpected<Variant>(void* Instance)> SnAPI::GameFramework::FieldInfo::Getter`

Getter callback.
</div>
<div class="snapi-api-card" markdown="1">
### `std::function<Result(void* Instance, const Variant& Value)> SnAPI::GameFramework::FieldInfo::Setter`

Setter callback.
</div>
<div class="snapi-api-card" markdown="1">
### `std::function<TExpected<VariantView>(void* Instance)> SnAPI::GameFramework::FieldInfo::ViewGetter`

Non-owning getter.
</div>
<div class="snapi-api-card" markdown="1">
### `std::function<const void*(const void* Instance)> SnAPI::GameFramework::FieldInfo::ConstPointer`

Direct const pointer accessor.
</div>
<div class="snapi-api-card" markdown="1">
### `std::function<void*(void* Instance)> SnAPI::GameFramework::FieldInfo::MutablePointer`

Direct mutable pointer accessor.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::FieldInfo::IsConst`

True if field is const-qualified.
</div>
