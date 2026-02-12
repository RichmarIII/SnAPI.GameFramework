# SnAPI::GameFramework::FieldInfo

Reflection metadata for a field.

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
