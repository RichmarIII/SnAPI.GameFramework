#pragma once

#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Expected.h"
#include "Invoker.h"
#include "Variant.h"

namespace SnAPI::GameFramework
{

struct FieldInfo
{
    std::string Name;
    TypeId FieldType;
    std::function<TExpected<Variant>(void* Instance)> Getter;
    std::function<Result(void* Instance, const Variant& Value)> Setter;
};

struct MethodInfo
{
    std::string Name;
    TypeId ReturnType;
    std::vector<TypeId> ParamTypes;
    MethodInvoker Invoke;
    bool IsConst = false;
};

struct ConstructorInfo
{
    std::vector<TypeId> ParamTypes;
    std::function<TExpected<std::shared_ptr<void>>(std::span<const Variant> Args)> Construct;
};

struct TypeInfo
{
    TypeId Id;
    std::string Name;
    size_t Size = 0;
    size_t Align = 0;
    std::vector<TypeId> BaseTypes;
    std::vector<FieldInfo> Fields;
    std::vector<MethodInfo> Methods;
    std::vector<ConstructorInfo> Constructors;
};

class TypeRegistry
{
public:
    static TypeRegistry& Instance();

    TExpected<TypeInfo*> Register(TypeInfo Info);
    const TypeInfo* Find(const TypeId& Id) const;
    const TypeInfo* FindByName(std::string_view Name) const;
    bool IsA(const TypeId& Type, const TypeId& Base) const;
    std::vector<const TypeInfo*> Derived(const TypeId& Base) const;

private:
    mutable std::mutex m_mutex{};
    std::unordered_map<TypeId, TypeInfo, UuidHash> m_types{};
    std::unordered_map<std::string, TypeId> m_nameToId{};
};

} // namespace SnAPI::GameFramework
