#include "ScriptABI.h"

#include <limits>
#include <vector>

#include "TypeRegistry.h"
#include "Uuid.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{
namespace
{
/**
 * @brief Convert a TypeId to the C ABI UUID struct.
 * @param Id TypeId to convert.
 * @return C ABI UUID representation.
 */
SnGfUuid ToC(const TypeId& Id)
{
    const auto Parts = ToParts(Id);
    return {Parts.High, Parts.Low};
}

/**
 * @brief Convert a C ABI UUID struct to a TypeId.
 * @param Id C ABI UUID.
 * @return TypeId value.
 */
TypeId FromC(const SnGfUuid& Id)
{
    return FromParts({Id.High, Id.Low});
}

/**
 * @brief Get the sentinel value used for invalid handles.
 * @return Sentinel handle value.
 */
uint64_t InvalidHandle()
{
    return std::numeric_limits<uint64_t>::max();
}

std::vector<ReflectedFieldRef> CollectReflectedFields(const TypeId& Type)
{
    return TypeRegistry::Instance().CollectFields(Type);
}

std::vector<ReflectedMethodRef> CollectReflectedMethods(const TypeId& Type)
{
    return TypeRegistry::Instance().CollectMethods(Type);
}

/**
 * @brief Convert a variant handle to a Variant pointer.
 * @param Handle Variant handle.
 * @return Variant pointer or nullptr.
 */
Variant* FromHandle(SnGfVariantHandle Handle)
{
    return static_cast<Variant*>(Handle.Ptr);
}

/**
 * @brief Convert a Variant pointer to a handle.
 * @param Ptr Variant pointer.
 * @return Variant handle.
 */
SnGfVariantHandle ToHandle(Variant* Ptr)
{
    return {Ptr};
}

} // namespace
} // namespace SnAPI::GameFramework

extern "C" {

SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_type_id_from_name(const char* name)
{
    if (!name)
    {
        return {0, 0};
    }
    auto Id = SnAPI::GameFramework::TypeIdFromName(name);
    return SnAPI::GameFramework::ToC(Id);
}

SNAPI_GAMEFRAMEWORK_API int sn_gf_type_is_registered(SnGfUuid id)
{
    auto Type = SnAPI::GameFramework::FromC(id);
    return SnAPI::GameFramework::TypeRegistry::Instance().Find(Type) != nullptr;
}

SNAPI_GAMEFRAMEWORK_API size_t sn_gf_type_field_count(SnGfUuid id)
{
    auto Type = SnAPI::GameFramework::FromC(id);
    const auto Fields = SnAPI::GameFramework::CollectReflectedFields(Type);
    return Fields.size();
}

SNAPI_GAMEFRAMEWORK_API SnGfFieldHandle sn_gf_type_field_by_name(SnGfUuid id, const char* name)
{
    if (!name)
    {
        return SnAPI::GameFramework::InvalidHandle();
    }
    auto Type = SnAPI::GameFramework::FromC(id);
    const auto Fields = SnAPI::GameFramework::CollectReflectedFields(Type);
    for (size_t Index = 0; Index < Fields.size(); ++Index)
    {
        if (Fields[Index].Field && Fields[Index].Field->Name == name)
        {
            return static_cast<SnGfFieldHandle>(Index);
        }
    }
    return SnAPI::GameFramework::InvalidHandle();
}

SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_field_type(SnGfUuid id, SnGfFieldHandle field)
{
    auto Type = SnAPI::GameFramework::FromC(id);
    const auto Fields = SnAPI::GameFramework::CollectReflectedFields(Type);
    if (field >= Fields.size() || !Fields[field].Field)
    {
        return {0, 0};
    }
    return SnAPI::GameFramework::ToC(Fields[field].Field->FieldType);
}

SNAPI_GAMEFRAMEWORK_API const char* sn_gf_field_name(SnGfUuid id, SnGfFieldHandle field)
{
    auto Type = SnAPI::GameFramework::FromC(id);
    const auto Fields = SnAPI::GameFramework::CollectReflectedFields(Type);
    if (field >= Fields.size() || !Fields[field].Field)
    {
        return nullptr;
    }
    return Fields[field].Field->Name.c_str();
}

SNAPI_GAMEFRAMEWORK_API SnGfMethodHandle sn_gf_type_method_by_name(SnGfUuid id, const char* name)
{
    if (!name)
    {
        return SnAPI::GameFramework::InvalidHandle();
    }
    auto Type = SnAPI::GameFramework::FromC(id);
    const auto Methods = SnAPI::GameFramework::CollectReflectedMethods(Type);
    for (size_t Index = 0; Index < Methods.size(); ++Index)
    {
        if (Methods[Index].Method && Methods[Index].Method->Name == name)
        {
            return static_cast<SnGfMethodHandle>(Index);
        }
    }
    return SnAPI::GameFramework::InvalidHandle();
}

SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_method_return_type(SnGfUuid id, SnGfMethodHandle method)
{
    auto Type = SnAPI::GameFramework::FromC(id);
    const auto Methods = SnAPI::GameFramework::CollectReflectedMethods(Type);
    if (method >= Methods.size() || !Methods[method].Method)
    {
        return {0, 0};
    }
    return SnAPI::GameFramework::ToC(Methods[method].Method->ReturnType);
}

SNAPI_GAMEFRAMEWORK_API size_t sn_gf_method_param_count(SnGfUuid id, SnGfMethodHandle method)
{
    auto Type = SnAPI::GameFramework::FromC(id);
    const auto Methods = SnAPI::GameFramework::CollectReflectedMethods(Type);
    if (method >= Methods.size() || !Methods[method].Method)
    {
        return 0;
    }
    return Methods[method].Method->ParamTypes.size();
}

SNAPI_GAMEFRAMEWORK_API SnGfUuid sn_gf_method_param_type(SnGfUuid id, SnGfMethodHandle method, size_t index)
{
    auto Type = SnAPI::GameFramework::FromC(id);
    const auto Methods = SnAPI::GameFramework::CollectReflectedMethods(Type);
    if (method >= Methods.size() ||
        !Methods[method].Method ||
        index >= Methods[method].Method->ParamTypes.size())
    {
        return {0, 0};
    }
    return SnAPI::GameFramework::ToC(Methods[method].Method->ParamTypes[index]);
}

SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_int(int value)
{
    auto* Var = new SnAPI::GameFramework::Variant(SnAPI::GameFramework::Variant::FromValue(value));
    return SnAPI::GameFramework::ToHandle(Var);
}

SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_float(float value)
{
    auto* Var = new SnAPI::GameFramework::Variant(SnAPI::GameFramework::Variant::FromValue(value));
    return SnAPI::GameFramework::ToHandle(Var);
}

SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_bool(int value)
{
    auto* Var = new SnAPI::GameFramework::Variant(SnAPI::GameFramework::Variant::FromValue(value != 0));
    return SnAPI::GameFramework::ToHandle(Var);
}

SNAPI_GAMEFRAMEWORK_API SnGfVariantHandle sn_gf_variant_from_string(const char* value)
{
    auto* Var = new SnAPI::GameFramework::Variant(SnAPI::GameFramework::Variant::FromValue(std::string(value ? value : "")));
    return SnAPI::GameFramework::ToHandle(Var);
}

SNAPI_GAMEFRAMEWORK_API void sn_gf_variant_destroy(SnGfVariantHandle handle)
{
    delete SnAPI::GameFramework::FromHandle(handle);
}

SNAPI_GAMEFRAMEWORK_API int sn_gf_object_get_field(void* instance, SnGfUuid type, SnGfFieldHandle field, SnGfVariantHandle* outValue)
{
    if (!instance || !outValue)
    {
        return 0;
    }
    auto Type = SnAPI::GameFramework::FromC(type);
    const auto Fields = SnAPI::GameFramework::CollectReflectedFields(Type);
    if (field >= Fields.size() || !Fields[field].Field || !Fields[field].Field->Getter)
    {
        return 0;
    }
    auto Result = Fields[field].Field->Getter(instance);
    if (!Result)
    {
        return 0;
    }
    *outValue = SnAPI::GameFramework::ToHandle(new SnAPI::GameFramework::Variant(std::move(Result.value())));
    return 1;
}

SNAPI_GAMEFRAMEWORK_API int sn_gf_object_set_field(void* instance, SnGfUuid type, SnGfFieldHandle field, SnGfVariantHandle value)
{
    if (!instance)
    {
        return 0;
    }
    auto* Var = SnAPI::GameFramework::FromHandle(value);
    if (!Var)
    {
        return 0;
    }
    auto Type = SnAPI::GameFramework::FromC(type);
    const auto Fields = SnAPI::GameFramework::CollectReflectedFields(Type);
    if (field >= Fields.size() || !Fields[field].Field || !Fields[field].Field->Setter)
    {
        return 0;
    }
    auto Result = Fields[field].Field->Setter(instance, *Var);
    return Result.has_value();
}

SNAPI_GAMEFRAMEWORK_API int sn_gf_object_invoke(void* instance, SnGfUuid type, SnGfMethodHandle method, const SnGfVariantHandle* args, size_t argCount, SnGfVariantHandle* outResult)
{
    if (!instance)
    {
        return 0;
    }
    auto Type = SnAPI::GameFramework::FromC(type);
    const auto Methods = SnAPI::GameFramework::CollectReflectedMethods(Type);
    if (method >= Methods.size() || !Methods[method].Method)
    {
        return 0;
    }
    std::vector<SnAPI::GameFramework::Variant> ArgValues;
    ArgValues.reserve(argCount);
    for (size_t i = 0; i < argCount; ++i)
    {
        auto* Var = SnAPI::GameFramework::FromHandle(args[i]);
        if (!Var)
        {
            return 0;
        }
        ArgValues.push_back(*Var);
    }
    auto Result = Methods[method].Method->Invoke(instance, ArgValues);
    if (!Result)
    {
        return 0;
    }
    if (outResult)
    {
        *outResult = SnAPI::GameFramework::ToHandle(new SnAPI::GameFramework::Variant(std::move(Result.value())));
    }
    return 1;
}

} // extern "C"
