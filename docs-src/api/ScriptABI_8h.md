# File `ScriptABI.h`

## Contents

- **Type:** SnGfUuid
- **Type:** SnGfVariantHandle

## Type Aliases

<div class="snapi-api-card" markdown="1">
### `typedef struct SnGfUuid SnGfUuid`

C ABI representation of a UUID.
</div>
<div class="snapi-api-card" markdown="1">
### `typedef struct SnGfVariantHandle SnGfVariantHandle`

Opaque handle to a Variant owned by the runtime.
</div>
<div class="snapi-api-card" markdown="1">
### `typedef uint64_t SnGfFieldHandle`

Opaque handle to a reflected field.
</div>
<div class="snapi-api-card" markdown="1">
### `typedef uint64_t SnGfMethodHandle`

Opaque handle to a reflected method.
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_type_id_from_name(const char *name)`

Get a TypeId from a fully qualified name.

**Parameters**

- `name`: Type name string.

**Returns:** UUID for the type.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API int sn_gf_type_is_registered(SnGfUuid id)`

Check if a type is registered.

**Parameters**

- `id`: TypeId to check.

**Returns:** Non-zero if the type exists.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API size_t sn_gf_type_field_count(SnGfUuid id)`

Get the number of fields on a type.

**Parameters**

- `id`: TypeId to query.

**Returns:** Field count.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API SnGfFieldHandle sn_gf_type_field_by_name(SnGfUuid id, const char *name)`

Find a field by name.

**Parameters**

- `id`: TypeId to query.
- `name`: Field name.

**Returns:** Field handle or 0 if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_field_type(SnGfUuid id, SnGfFieldHandle field)`

Get the type of a field.

**Parameters**

- `id`: TypeId to query.
- `field`: Field handle.

**Returns:** Field type id.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API const char * sn_gf_field_name(SnGfUuid id, SnGfFieldHandle field)`

Get the name of a field.

**Parameters**

- `id`: TypeId to query.
- `field`: Field handle.

**Returns:** Null-terminated field name string.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API SnGfMethodHandle sn_gf_type_method_by_name(SnGfUuid id, const char *name)`

Find a method by name.

**Parameters**

- `id`: TypeId to query.
- `name`: Method name.

**Returns:** Method handle or 0 if not found.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_method_return_type(SnGfUuid id, SnGfMethodHandle method)`

Get the return type of a method.

**Parameters**

- `id`: TypeId to query.
- `method`: Method handle.

**Returns:** Return type id.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API size_t sn_gf_method_param_count(SnGfUuid id, SnGfMethodHandle method)`

Get the number of method parameters.

**Parameters**

- `id`: TypeId to query.
- `method`: Method handle.

**Returns:** Parameter count.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_method_param_type(SnGfUuid id, SnGfMethodHandle method, size_t index)`

Get the type of a method parameter.

**Parameters**

- `id`: TypeId to query.
- `method`: Method handle.
- `index`: Parameter index.

**Returns:** Parameter type id.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_int(int value)`

Create a Variant from an int.

**Parameters**

- `value`: Value to store.

**Returns:** Variant handle.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_float(float value)`

Create a Variant from a float.

**Parameters**

- `value`: Value to store.

**Returns:** Variant handle.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_bool(int value)`

Create a Variant from a bool.

**Parameters**

- `value`: Non-zero for true, zero for false.

**Returns:** Variant handle.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_string(const char *value)`

Create a Variant from a string.

**Parameters**

- `value`: Null-terminated string.

**Returns:** Variant handle.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API void sn_gf_variant_destroy(SnGfVariantHandle handle)`

Destroy a Variant handle.

**Parameters**

- `handle`: Variant handle to destroy.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API int sn_gf_object_get_field(void *instance, SnGfUuid type, SnGfFieldHandle field, SnGfVariantHandle *outValue)`

Read a field value from an object.

**Parameters**

- `instance`: Pointer to the object instance.
- `type`: TypeId of the instance.
- `field`: Field handle.
- `outValue`: Output Variant handle.

**Returns:** Non-zero on success.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API int sn_gf_object_set_field(void *instance, SnGfUuid type, SnGfFieldHandle field, SnGfVariantHandle value)`

Write a field value on an object.

**Parameters**

- `instance`: Pointer to the object instance.
- `type`: TypeId of the instance.
- `field`: Field handle.
- `value`: Variant handle containing the new value.

**Returns:** Non-zero on success.
</div>
<div class="snapi-api-card" markdown="1">
### `SNAPI_GAMEFRAMEWORK_API int sn_gf_object_invoke(void *instance, SnGfUuid type, SnGfMethodHandle method, const SnGfVariantHandle *args, size_t argCount, SnGfVariantHandle *outResult)`

Invoke a reflected method on an object.

**Parameters**

- `instance`: Pointer to the object instance.
- `type`: TypeId of the instance.
- `method`: Method handle.
- `args`: Array of Variant handles for arguments.
- `argCount`: Argument count.
- `outResult`: Output Variant handle for the return value.

**Returns:** Non-zero on success.
</div>
