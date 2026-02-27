#pragma once

#include <cstddef>
#include <cstdint>

#include "Export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief C ABI representation of a UUID.
 * @remarks Split into high/low 64-bit parts for language interoperability.
 */
typedef struct SnGfUuid
{
    uint64_t High; /**< @brief High 64 bits. */
    uint64_t Low;  /**< @brief Low 64 bits. */
} SnGfUuid;

/**
 * @brief Opaque handle to a Variant owned by the runtime.
 * @remarks
 * ABI consumers must treat this as move-by-value opaque token and release with
 * `sn_gf_variant_destroy` when done.
 */
typedef struct SnGfVariantHandle
{
    void* Ptr; /**< @brief Opaque pointer to internal Variant storage. */
} SnGfVariantHandle;

/** @brief Opaque handle to a reflected field. */
typedef uint64_t SnGfFieldHandle;
/** @brief Opaque handle to a reflected method. */
typedef uint64_t SnGfMethodHandle;

/**
 * @brief Get a TypeId from a fully qualified name.
 * @param name Type name string.
 * @return UUID for the type.
 * @remarks Deterministic UUIDv5 based on the name.
 */
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_type_id_from_name(const char* name);
/**
 * @brief Check if a type is registered.
 * @param id TypeId to check.
 * @return Non-zero if the type exists.
 */
SNAPI_GAMEFRAMEWORK_API int sn_gf_type_is_registered(SnGfUuid id);
/**
 * @brief Get the number of fields on a type.
 * @param id TypeId to query.
 * @return Field count.
 * @remarks Includes inherited fields.
 */
SNAPI_GAMEFRAMEWORK_API size_t sn_gf_type_field_count(SnGfUuid id);
/**
 * @brief Find a field by name.
 * @param id TypeId to query.
 * @param name Field name.
 * @return Field handle or 0 if not found.
 * @remarks Searches inherited fields as well.
 */
SNAPI_GAMEFRAMEWORK_API SnGfFieldHandle sn_gf_type_field_by_name(SnGfUuid id, const char* name);
/**
 * @brief Get the type of a field.
 * @param id TypeId to query.
 * @param field Field handle.
 * @return Field type id.
 */
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_field_type(SnGfUuid id, SnGfFieldHandle field);
/**
 * @brief Get the name of a field.
 * @param id TypeId to query.
 * @param field Field handle.
 * @return Null-terminated field name string.
 */
SNAPI_GAMEFRAMEWORK_API const char* sn_gf_field_name(SnGfUuid id, SnGfFieldHandle field);

/**
 * @brief Find a method by name.
 * @param id TypeId to query.
 * @param name Method name.
 * @return Method handle or 0 if not found.
 * @remarks
 * Searches inherited methods and applies C++-style name hiding
 * (derived declarations hide base declarations with the same name).
 */
SNAPI_GAMEFRAMEWORK_API SnGfMethodHandle sn_gf_type_method_by_name(SnGfUuid id, const char* name);
/**
 * @brief Get the return type of a method.
 * @param id TypeId to query.
 * @param method Method handle.
 * @return Return type id.
 */
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_method_return_type(SnGfUuid id, SnGfMethodHandle method);
/**
 * @brief Get the number of method parameters.
 * @param id TypeId to query.
 * @param method Method handle.
 * @return Parameter count.
 */
SNAPI_GAMEFRAMEWORK_API size_t sn_gf_method_param_count(SnGfUuid id, SnGfMethodHandle method);
/**
 * @brief Get the type of a method parameter.
 * @param id TypeId to query.
 * @param method Method handle.
 * @param index Parameter index.
 * @return Parameter type id.
 */
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_method_param_type(SnGfUuid id, SnGfMethodHandle method, size_t index);

/**
 * @brief Create a Variant from an int.
 * @param value Value to store.
 * @return Variant handle.
 */
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_int(int value);
/**
 * @brief Create a Variant from a float.
 * @param value Value to store.
 * @return Variant handle.
 */
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_float(float value);
/**
 * @brief Create a Variant from a bool.
 * @param value Non-zero for true, zero for false.
 * @return Variant handle.
 */
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_bool(int value);
/**
 * @brief Create a Variant from a string.
 * @param value Null-terminated string.
 * @return Variant handle.
 * @remarks The string is copied into the Variant.
 */
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_string(const char* value);
/**
 * @brief Destroy a Variant handle.
 * @param handle Variant handle to destroy.
 */
SNAPI_GAMEFRAMEWORK_API void sn_gf_variant_destroy(SnGfVariantHandle handle);

/**
 * @brief Read a field value from an object.
 * @param instance Pointer to the object instance.
 * @param type TypeId of the instance.
 * @param field Field handle.
 * @param outValue Output Variant handle.
 * @return Non-zero on success.
 * @remarks On success, `outValue` receives ownership of an allocated variant handle.
 */
SNAPI_GAMEFRAMEWORK_API int sn_gf_object_get_field(void* instance, SnGfUuid type, SnGfFieldHandle field, SnGfVariantHandle* outValue);
/**
 * @brief Write a field value on an object.
 * @param instance Pointer to the object instance.
 * @param type TypeId of the instance.
 * @param field Field handle.
 * @param value Variant handle containing the new value.
 * @return Non-zero on success.
 * @remarks Respects const fields; will fail if the field is read-only.
 */
SNAPI_GAMEFRAMEWORK_API int sn_gf_object_set_field(void* instance, SnGfUuid type, SnGfFieldHandle field, SnGfVariantHandle value);
/**
 * @brief Invoke a reflected method on an object.
 * @param instance Pointer to the object instance.
 * @param type TypeId of the instance.
 * @param method Method handle.
 * @param args Array of Variant handles for arguments.
 * @param argCount Argument count.
 * @param outResult Output Variant handle for the return value.
 * @return Non-zero on success.
 * @remarks On success, `outResult` owns a variant handle that must be destroyed by caller.
 */
SNAPI_GAMEFRAMEWORK_API int sn_gf_object_invoke(void* instance, SnGfUuid type, SnGfMethodHandle method, const SnGfVariantHandle* args, size_t argCount, SnGfVariantHandle* outResult);

#ifdef __cplusplus
}
#endif
