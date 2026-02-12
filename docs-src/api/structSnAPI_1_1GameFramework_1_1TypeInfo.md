# SnAPI::GameFramework::TypeInfo

Reflection metadata for a type.

## Public Members

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::TypeInfo::Id`

Type id (UUID).
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::TypeInfo::Name`

Fully qualified type name.
</div>
<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::TypeInfo::Size`

sizeof(T).
</div>
<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::TypeInfo::Align`

alignof(T).
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TypeId> SnAPI::GameFramework::TypeInfo::BaseTypes`

Base class TypeIds.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<FieldInfo> SnAPI::GameFramework::TypeInfo::Fields`

Field metadata.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<MethodInfo> SnAPI::GameFramework::TypeInfo::Methods`

Method metadata.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<ConstructorInfo> SnAPI::GameFramework::TypeInfo::Constructors`

Constructor metadata.
</div>
