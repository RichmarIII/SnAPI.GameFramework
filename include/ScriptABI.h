#pragma once

#include <cstddef>
#include <cstdint>

#include "Export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SnGfUuid
{
    uint64_t High;
    uint64_t Low;
} SnGfUuid;

typedef struct SnGfVariantHandle
{
    void* Ptr;
} SnGfVariantHandle;

typedef uint64_t SnGfFieldHandle;
typedef uint64_t SnGfMethodHandle;

SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_type_id_from_name(const char* name);
SNAPI_GAMEFRAMEWORK_API int sn_gf_type_is_registered(SnGfUuid id);
SNAPI_GAMEFRAMEWORK_API size_t sn_gf_type_field_count(SnGfUuid id);
SNAPI_GAMEFRAMEWORK_API SnGfFieldHandle sn_gf_type_field_by_name(SnGfUuid id, const char* name);
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_field_type(SnGfUuid id, SnGfFieldHandle field);
SNAPI_GAMEFRAMEWORK_API const char* sn_gf_field_name(SnGfUuid id, SnGfFieldHandle field);

SNAPI_GAMEFRAMEWORK_API SnGfMethodHandle sn_gf_type_method_by_name(SnGfUuid id, const char* name);
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_method_return_type(SnGfUuid id, SnGfMethodHandle method);
SNAPI_GAMEFRAMEWORK_API size_t sn_gf_method_param_count(SnGfUuid id, SnGfMethodHandle method);
SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_method_param_type(SnGfUuid id, SnGfMethodHandle method, size_t index);

SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_int(int value);
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_float(float value);
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_bool(int value);
SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_string(const char* value);
SNAPI_GAMEFRAMEWORK_API void sn_gf_variant_destroy(SnGfVariantHandle handle);

SNAPI_GAMEFRAMEWORK_API int sn_gf_object_get_field(void* instance, SnGfUuid type, SnGfFieldHandle field, SnGfVariantHandle* outValue);
SNAPI_GAMEFRAMEWORK_API int sn_gf_object_set_field(void* instance, SnGfUuid type, SnGfFieldHandle field, SnGfVariantHandle value);
SNAPI_GAMEFRAMEWORK_API int sn_gf_object_invoke(void* instance, SnGfUuid type, SnGfMethodHandle method, const SnGfVariantHandle* args, size_t argCount, SnGfVariantHandle* outResult);

#ifdef __cplusplus
}
#endif
