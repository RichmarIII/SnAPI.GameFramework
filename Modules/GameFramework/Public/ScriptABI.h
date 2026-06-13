#pragma once

#include <cstddef>
#include <cstdint>

#include "Export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup SnAPI_GameFramework
 * @brief C ABI representation of a GameFramework UUID.
 *
 * The struct is intentionally POD and language-neutral so foreign runtimes can pass type
 * and object ids across the C boundary without depending on C++ layout rules.
 */
typedef struct SnGfUuid
{
    uint64_t High; /**< @brief High 64 bits of the UUID. */
    uint64_t Low;  /**< @brief Low 64 bits of the UUID. */
} SnGfUuid;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Owning opaque handle to a heap-allocated `Variant`.
 *
 * The handle is a C ABI token for a `Variant` allocated by the runtime. Consumers must
 * treat the pointer as opaque and must release ownership with `sn_gf_variant_destroy()`.
 *
 * Ownership and lifetime:
 * - The handle owns the pointed-to `Variant`.
 * - Passing the handle by value does not duplicate ownership.
 * - `Ptr == NULL` represents an empty handle.
 */
typedef struct SnGfVariantHandle
{
    void* Ptr; /**< @brief Opaque pointer to runtime-owned `Variant` storage. */
} SnGfVariantHandle;

/**
 * @ingroup SnAPI_GameFramework
 * @brief C ABI representation of a 3D vector.
 *
 * The values are plain `float`s. Units and coordinate space are defined by the engine API
 * that consumes the vector; this struct only transports three scalar components.
 */
typedef struct SnGfVec3
{
    float X; /**< @brief X component. */
    float Y; /**< @brief Y component. */
    float Z; /**< @brief Z component. */
} SnGfVec3;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Index-like handle to a reflected field in a collected field list.
 *
 * Handles are not globally stable ids. They are indices into the field list returned for
 * one specific reflected type, including inherited fields.
 */
typedef uint64_t SnGfFieldHandle;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Index-like handle to a reflected method in a collected method list.
 *
 * Handles are not globally stable ids. They are indices into the method list returned for
 * one specific reflected type, including inherited methods subject to name-hiding rules.
 */
typedef uint64_t SnGfMethodHandle;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Compute a deterministic type id from a fully qualified type name.
 *
 * This function hashes the supplied name into the same UUID form used by the reflection
 * system. The result may identify an unregistered type; registration is a separate
 * question handled by `sn_gf_type_is_registered()`.
 *
 * @param name Null-terminated fully qualified type name.
 * @return Deterministic UUID for the name, or `{0, 0}` when @p name is null.
 *
 * @see sn_gf_type_is_registered()
 */
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_type_id_from_name(const char* name);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Check whether reflected metadata is registered for a type id.
 * @param id Type id to query.
 * @return Non-zero when the type exists in the reflection registry, zero otherwise.
 */
SNAPI_GAMEFRAMEWORK_API int sn_gf_type_is_registered(SnGfUuid id);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Count the reflected fields on a type.
 * @param id Type id to query.
 * @return Number of collected reflected fields, including inherited fields.
 *
 * @note Unknown types currently report `0`.
 */
SNAPI_GAMEFRAMEWORK_API size_t sn_gf_type_field_count(SnGfUuid id);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Resolve a reflected field handle by name.
 * @param id Type id to query.
 * @param name Null-terminated field name to search for.
 * @return Field handle on success, or the invalid-handle sentinel (`UINT64_MAX`) when the
 *         type is unknown, @p name is null, or no matching field exists.
 *
 * @note The search includes inherited fields.
 */
SNAPI_GAMEFRAMEWORK_API SnGfFieldHandle sn_gf_type_field_by_name(SnGfUuid id, const char* name);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Query the reflected type of one field.
 * @param id Type id whose field list produced @p field.
 * @param field Field handle previously returned for that type.
 * @return Field type id, or `{0, 0}` when the handle is invalid.
 */
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_field_type(SnGfUuid id, SnGfFieldHandle field);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Return the name of one reflected field.
 * @param id Type id whose field list produced @p field.
 * @param field Field handle previously returned for that type.
 * @return Borrowed null-terminated field name, or `NULL` when the handle is invalid.
 *
 * @note The returned pointer refers to reflection metadata storage. Do not free it.
 */
SNAPI_GAMEFRAMEWORK_API const char* sn_gf_field_name(SnGfUuid id, SnGfFieldHandle field);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Resolve a reflected method handle by name.
 * @param id Type id to query.
 * @param name Null-terminated method name to search for.
 * @return Method handle on success, or the invalid-handle sentinel (`UINT64_MAX`) when
 *         the type is unknown, @p name is null, or no matching method exists.
 *
 * @note The collected method list includes inherited methods and applies C++-style name
 * hiding: derived declarations with the same name hide base declarations.
 */
SNAPI_GAMEFRAMEWORK_API SnGfMethodHandle sn_gf_type_method_by_name(SnGfUuid id, const char* name);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Query the reflected return type of one method.
 * @param id Type id whose method list produced @p method.
 * @param method Method handle previously returned for that type.
 * @return Return type id, or `{0, 0}` when the handle is invalid.
 */
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_method_return_type(SnGfUuid id, SnGfMethodHandle method);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Count the reflected parameters of one method.
 * @param id Type id whose method list produced @p method.
 * @param method Method handle previously returned for that type.
 * @return Parameter count, or `0` when the handle is invalid.
 */
SNAPI_GAMEFRAMEWORK_API size_t sn_gf_method_param_count(SnGfUuid id, SnGfMethodHandle method);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Query the reflected type of one method parameter.
 * @param id Type id whose method list produced @p method.
 * @param method Method handle previously returned for that type.
 * @param index Zero-based parameter index.
 * @return Parameter type id, or `{0, 0}` when the handle or index is invalid.
 */
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_method_param_type(SnGfUuid id, SnGfMethodHandle method, size_t index);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Allocate a new `Variant` containing an `int`.
 * @param value Value to store.
 * @return Owning handle to the allocated `Variant`.
 *
 * @warning The API does not expose allocation failure through the return value. The
 * caller must still destroy the returned handle with `sn_gf_variant_destroy()`.
 */
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_int(int value);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Allocate a new `Variant` containing a `float`.
 * @param value Value to store.
 * @return Owning handle to the allocated `Variant`.
 */
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_float(float value);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Allocate a new `Variant` containing a `bool`.
 * @param value Zero maps to `false`; any non-zero value maps to `true`.
 * @return Owning handle to the allocated `Variant`.
 */
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_bool(int value);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Allocate a new `Variant` containing a string.
 * @param value Null-terminated string. `NULL` is treated as the empty string.
 * @return Owning handle to the allocated `Variant`.
 *
 * @note The string bytes are copied into the `Variant`.
 */
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_string(const char* value);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Allocate a new `Variant` containing a `Vec3`.
 * @param value Vector value to store.
 * @return Owning handle to the allocated `Variant`.
 */
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_vec3(SnGfVec3 value);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Destroy an owning `Variant` handle.
 * @param handle Handle previously returned by this ABI.
 *
 * @note Passing an empty handle (`Ptr == NULL`) is safe and behaves as a no-op.
 */
SNAPI_GAMEFRAMEWORK_API void sn_gf_variant_destroy(SnGfVariantHandle handle);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Extract a `Vec3` payload from a `Variant` handle.
 * @param handle Variant handle to inspect.
 * @param outValue Destination vector. Must not be `NULL`.
 * @return Non-zero on success, zero when @p handle is empty, @p outValue is null, or the
 *         stored `Variant` does not currently hold a `Vec3`.
 */
SNAPI_GAMEFRAMEWORK_API int sn_gf_variant_to_vec3(SnGfVariantHandle handle, SnGfVec3* outValue);

/**
 * @ingroup SnAPI_GameFramework
 * @brief Read a reflected field value from an object instance.
 * @param instance Pointer to the object instance to read.
 * @param type Reflected type that owns the field handle.
 * @param field Field handle previously resolved for @p type.
 * @param outValue Destination for a newly allocated owning `Variant` handle.
 * @return Non-zero on success, zero on invalid arguments, invalid handles, unreadable
 *         fields, or getter failure.
 *
 * @post On success, `*outValue` owns a newly allocated `Variant` that the caller must
 *       destroy with `sn_gf_variant_destroy()`.
 * @warning This is a raw reflection call. The ABI does not validate that @p instance
 * actually points to a live object compatible with @p type.
 */
SNAPI_GAMEFRAMEWORK_API int sn_gf_object_get_field(void* instance, SnGfUuid type, SnGfFieldHandle field, SnGfVariantHandle* outValue);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Write a reflected field value on an object instance.
 * @param instance Pointer to the object instance to mutate.
 * @param type Reflected type that owns the field handle.
 * @param field Field handle previously resolved for @p type.
 * @param value Owning or borrowed `Variant` handle containing the new value.
 * @return Non-zero on success, zero on invalid arguments, invalid handles, type mismatch,
 *         read-only fields, or setter failure.
 *
 * @note Ownership of @p value is not transferred. The caller remains responsible for
 * destroying the handle if it owns one.
 */
SNAPI_GAMEFRAMEWORK_API int sn_gf_object_set_field(void* instance, SnGfUuid type, SnGfFieldHandle field, SnGfVariantHandle value);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Invoke a reflected method on an object instance.
 * @param instance Pointer to the object instance to call.
 * @param type Reflected type that owns the method handle.
 * @param method Method handle previously resolved for @p type.
 * @param args Array of argument handles. Each handle is borrowed for the duration of the
 *        call and is not consumed.
 * @param argCount Number of elements in @p args.
 * @param outResult Optional destination for a newly allocated owning `Variant` handle
 *        containing the return value.
 * @return Non-zero on success, zero on invalid arguments, invalid handles, or method
 *         invocation failure.
 *
 * @post When @p outResult is non-null and the call succeeds, `*outResult` owns a newly
 *       allocated `Variant` that the caller must destroy with `sn_gf_variant_destroy()`.
 * @warning The function reports failure only as `0`; it does not expose detailed error
 * diagnostics across the C boundary.
 */
SNAPI_GAMEFRAMEWORK_API int sn_gf_object_invoke(void* instance, SnGfUuid type, SnGfMethodHandle method, const SnGfVariantHandle* args, size_t argCount, SnGfVariantHandle* outResult);
/**
 * @ingroup SnAPI_GameFramework
 * @brief Resolve a Component pointer from a Node instance and reflected component type.
 * @param nodeInstance Pointer to a live `BaseNode` instance.
 * @param componentType Reflected concrete component type to resolve.
 * @return Borrowed raw Component pointer, or `NULL` when the node has no such component,
 *         the node is not attached to a World, or @p nodeInstance is null.
 *
 * @warning The returned pointer is borrowed from the World. It becomes invalid when the
 * Component is removed, the Node is destroyed, or the World shuts down.
 */
SNAPI_GAMEFRAMEWORK_API void* sn_gf_node_get_component(void* nodeInstance, SnGfUuid componentType);

#ifdef __cplusplus
}
#endif
