# File `TypeAutoRegistration.h`

## Contents

- **Namespace:** SnAPI
- **Namespace:** SnAPI::GameFramework
- **Type:** SnAPI::GameFramework::TTypeRegistrar

## Macros

<div class="snapi-api-card" markdown="1">
### `SNAPI_DETAIL_CONCAT_INNER`

Internal macro helper for concatenation.

**Parameters**

- `a`: 
- `b`:
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_DETAIL_CONCAT`

Internal macro helper for concatenation.

**Parameters**

- `a`: 
- `b`:
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_DETAIL_USED`

Internal macro helper for "used" attribute to survive LTO/GC-sections.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_REFLECT_TYPE_IMPL`

Register a reflected type using a builder expression (lazy).

**Parameters**

- `Type`: 
- `BuilderExpr`: Expression that builds and registers the type.
- `Id`: Unique counter to avoid symbol collisions.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_REFLECT_TYPE`

Register a reflected type using a builder expression (lazy).

**Parameters**

- `Type`: C++ type being registered.
- `BuilderExpr`: Expression that builds and registers the type's TypeInfo.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_REFLECT_COMPONENT`

Register a reflected component type and its serializer.

**Parameters**

- `ComponentType`: Component C++ type.
- `BuilderExpr`: Expression that builds and registers the type's TypeInfo.
</div>
