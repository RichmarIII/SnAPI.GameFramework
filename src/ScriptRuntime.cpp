#include "ScriptRuntime.h"

#include "BaseComponent.h"
#include "BaseNode.h"
#include "Handle.h"
#include "Handles.h"
#include "IWorld.h"
#include "PathResolver.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "TypeRegistry.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(SNAPI_GF_ENABLE_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#if defined(SNAPI_GF_ENABLE_SWIG) && (SNAPI_GF_ENABLE_SWIG == 1)
extern "C" int luaopen_snapi_gf(lua_State* L);
#endif
#endif

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] std::optional<Uuid> ParseUuid(const std::string_view Text)
{
    if (Text.empty())
    {
        return std::nullopt;
    }

    auto Parsed = Uuid::from_string(std::string(Text));
    if (!Parsed)
    {
        return std::nullopt;
    }

    return *Parsed;
}

[[nodiscard]] std::optional<TypeId> ParseTypeId(const std::string_view Text)
{
#if defined(SNAPI_GF_ENABLE_SWIG) && (SNAPI_GF_ENABLE_SWIG == 1)
    (void)Text;
    return std::nullopt;
#else
    const auto Parsed = ParseUuid(Text);
    if (!Parsed)
    {
        return std::nullopt;
    }

    return *Parsed;
#endif
}

#if defined(SNAPI_GF_ENABLE_LUA)
[[nodiscard]] const TypeId& BoolTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<bool>);
    return Id;
}

[[nodiscard]] const TypeId& IntTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<int>);
    return Id;
}

[[nodiscard]] const TypeId& UnsignedIntTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<unsigned int>);
    return Id;
}

[[nodiscard]] const TypeId& UInt64TypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<std::uint64_t>);
    return Id;
}

[[nodiscard]] const TypeId& FloatTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<float>);
    return Id;
}

[[nodiscard]] const TypeId& DoubleTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<double>);
    return Id;
}

[[nodiscard]] const TypeId& StringTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<std::string>);
    return Id;
}

[[nodiscard]] const TypeId& UuidTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<Uuid>);
    return Id;
}

[[nodiscard]] const TypeId& TypeIdTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<TypeId>);
    return Id;
}

[[nodiscard]] const TypeId& NodeHandleTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<NodeHandle>);
    return Id;
}

[[nodiscard]] const TypeId& ComponentHandleTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<ComponentHandle>);
    return Id;
}

[[nodiscard]] const TypeId& Vec2TypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<Vec2>);
    return Id;
}

[[nodiscard]] const TypeId& Vec3TypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<Vec3>);
    return Id;
}

[[nodiscard]] const TypeId& Vec4TypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<Vec4>);
    return Id;
}

[[nodiscard]] const TypeId& QuatTypeId()
{
    static const TypeId Id = TypeIdFromName(TTypeNameV<Quat>);
    return Id;
}

#if !defined(SNAPI_GF_ENABLE_SWIG) || (SNAPI_GF_ENABLE_SWIG != 1)
constexpr const char* kLuaReflectedObjectMetatable = "snapi.reflected_object";

struct LuaReflectedObject
{
    void* Instance = nullptr;
    std::uint64_t TypeHigh = 0;
    std::uint64_t TypeLow = 0;
};

[[nodiscard]] TypeId TypeIdFromLuaObject(const LuaReflectedObject& Object)
{
    return FromParts({Object.TypeHigh, Object.TypeLow});
}
#endif

[[nodiscard]] std::string NormalizePath(std::string_view PathText)
{
    if (PathText.empty())
    {
        return {};
    }

    auto ResolvedPath = SPathResolver::Instance().Resolve(PathText);
    if (!ResolvedPath)
    {
        return {};
    }
    return ResolvedPath->string();
}

[[nodiscard]] Result RegisterLuaScriptingApi(lua_State* L)
{
#if !defined(SNAPI_GF_ENABLE_SWIG) || (SNAPI_GF_ENABLE_SWIG != 1)
    (void)L;
    return std::unexpected(MakeError(EErrorCode::NotReady, "Lua scripting requires SWIG bindings"));
#else
    if (!L)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Lua state is null"));
    }

    const int OpenResult = luaopen_snapi_gf(L);
    if (OpenResult != 1 || !lua_istable(L, -1))
    {
        if (lua_gettop(L) > 0)
        {
            lua_pop(L, 1);
        }
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "luaopen_snapi_gf did not return module table"));
    }

    lua_pushvalue(L, -1);
    lua_setglobal(L, "snapi_gf");
    lua_setglobal(L, "snapi");
    return Ok();
#endif
}

[[nodiscard]] bool IsExpectedInteger(lua_State* L, int Index)
{
    return lua_isinteger(L, Index) != 0;
}

void PushVec2Table(lua_State* L, const Vec2& Value)
{
    lua_createtable(L, 2, 2);
    lua_pushnumber(L, static_cast<lua_Number>(Value.x()));
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, static_cast<lua_Number>(Value.y()));
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, static_cast<lua_Number>(Value.x()));
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, static_cast<lua_Number>(Value.y()));
    lua_setfield(L, -2, "y");
}

void PushVec3Table(lua_State* L, const Vec3& Value)
{
    lua_createtable(L, 3, 3);
    lua_pushnumber(L, static_cast<lua_Number>(Value.x()));
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, static_cast<lua_Number>(Value.y()));
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, static_cast<lua_Number>(Value.z()));
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, static_cast<lua_Number>(Value.x()));
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, static_cast<lua_Number>(Value.y()));
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, static_cast<lua_Number>(Value.z()));
    lua_setfield(L, -2, "z");
}

void PushVec4Table(lua_State* L, const Vec4& Value)
{
    lua_createtable(L, 4, 4);
    lua_pushnumber(L, static_cast<lua_Number>(Value.x()));
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, static_cast<lua_Number>(Value.y()));
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, static_cast<lua_Number>(Value.z()));
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, static_cast<lua_Number>(Value.w()));
    lua_rawseti(L, -2, 4);
    lua_pushnumber(L, static_cast<lua_Number>(Value.x()));
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, static_cast<lua_Number>(Value.y()));
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, static_cast<lua_Number>(Value.z()));
    lua_setfield(L, -2, "z");
    lua_pushnumber(L, static_cast<lua_Number>(Value.w()));
    lua_setfield(L, -2, "w");
}

void PushQuatTable(lua_State* L, const Quat& Value)
{
    lua_createtable(L, 4, 4);
    lua_pushnumber(L, static_cast<lua_Number>(Value.x()));
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, static_cast<lua_Number>(Value.y()));
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, static_cast<lua_Number>(Value.z()));
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, static_cast<lua_Number>(Value.w()));
    lua_rawseti(L, -2, 4);
    lua_pushnumber(L, static_cast<lua_Number>(Value.x()));
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, static_cast<lua_Number>(Value.y()));
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, static_cast<lua_Number>(Value.z()));
    lua_setfield(L, -2, "z");
    lua_pushnumber(L, static_cast<lua_Number>(Value.w()));
    lua_setfield(L, -2, "w");
}

[[nodiscard]] bool ReadNumericFieldOrIndex(lua_State* L,
                                           const int TableIndex,
                                           const char* FieldName,
                                           const int ArrayIndex,
                                           lua_Number& OutValue)
{
    const int AbsIndex = lua_absindex(L, TableIndex);

    lua_getfield(L, AbsIndex, FieldName);
    if (lua_isnumber(L, -1))
    {
        OutValue = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return true;
    }
    lua_pop(L, 1);

    lua_rawgeti(L, AbsIndex, ArrayIndex);
    if (lua_isnumber(L, -1))
    {
        OutValue = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return true;
    }
    lua_pop(L, 1);
    return false;
}

[[nodiscard]] bool PushVariantToLua(lua_State* L, const Variant& Value, std::string* OutError = nullptr)
{
    if (Value.IsVoid())
    {
        lua_pushnil(L);
        return true;
    }

    const TypeId& Type = Value.Type();
    if (Type == BoolTypeId())
    {
        auto BoolResult = Value.AsConstRef<bool>();
        if (!BoolResult)
        {
            if (OutError)
            {
                *OutError = BoolResult.error().Message;
            }
            return false;
        }
        lua_pushboolean(L, BoolResult->get() ? 1 : 0);
        return true;
    }

    if (Type == IntTypeId())
    {
        auto IntResult = Value.AsConstRef<int>();
        if (!IntResult)
        {
            if (OutError)
            {
                *OutError = IntResult.error().Message;
            }
            return false;
        }
        lua_pushinteger(L, static_cast<lua_Integer>(IntResult->get()));
        return true;
    }

    if (Type == UnsignedIntTypeId())
    {
        auto UIntResult = Value.AsConstRef<unsigned int>();
        if (!UIntResult)
        {
            if (OutError)
            {
                *OutError = UIntResult.error().Message;
            }
            return false;
        }
        lua_pushinteger(L, static_cast<lua_Integer>(UIntResult->get()));
        return true;
    }

    if (Type == UInt64TypeId())
    {
        auto UInt64Result = Value.AsConstRef<std::uint64_t>();
        if (!UInt64Result)
        {
            if (OutError)
            {
                *OutError = UInt64Result.error().Message;
            }
            return false;
        }

        const std::uint64_t Raw = UInt64Result->get();
        if (Raw > static_cast<std::uint64_t>(std::numeric_limits<lua_Integer>::max()))
        {
            if (OutError)
            {
                *OutError = "Value is too large for lua integer";
            }
            return false;
        }

        lua_pushinteger(L, static_cast<lua_Integer>(Raw));
        return true;
    }

    if (Type == FloatTypeId())
    {
        auto FloatResult = Value.AsConstRef<float>();
        if (!FloatResult)
        {
            if (OutError)
            {
                *OutError = FloatResult.error().Message;
            }
            return false;
        }
        lua_pushnumber(L, static_cast<lua_Number>(FloatResult->get()));
        return true;
    }

    if (Type == DoubleTypeId())
    {
        auto DoubleResult = Value.AsConstRef<double>();
        if (!DoubleResult)
        {
            if (OutError)
            {
                *OutError = DoubleResult.error().Message;
            }
            return false;
        }
        lua_pushnumber(L, static_cast<lua_Number>(DoubleResult->get()));
        return true;
    }

    if (Type == StringTypeId())
    {
        auto StringResult = Value.AsConstRef<std::string>();
        if (!StringResult)
        {
            if (OutError)
            {
                *OutError = StringResult.error().Message;
            }
            return false;
        }
        lua_pushlstring(L, StringResult->get().data(), StringResult->get().size());
        return true;
    }

    if (Type == Vec2TypeId())
    {
        auto Vec2Result = Value.AsConstRef<Vec2>();
        if (!Vec2Result)
        {
            if (OutError)
            {
                *OutError = Vec2Result.error().Message;
            }
            return false;
        }

        PushVec2Table(L, Vec2Result->get());
        return true;
    }

    if (Type == Vec3TypeId())
    {
        auto Vec3Result = Value.AsConstRef<Vec3>();
        if (!Vec3Result)
        {
            if (OutError)
            {
                *OutError = Vec3Result.error().Message;
            }
            return false;
        }

        PushVec3Table(L, Vec3Result->get());
        return true;
    }

    if (Type == Vec4TypeId())
    {
        auto Vec4Result = Value.AsConstRef<Vec4>();
        if (!Vec4Result)
        {
            if (OutError)
            {
                *OutError = Vec4Result.error().Message;
            }
            return false;
        }

        PushVec4Table(L, Vec4Result->get());
        return true;
    }

    if (Type == QuatTypeId())
    {
        auto QuatResult = Value.AsConstRef<Quat>();
        if (!QuatResult)
        {
            if (OutError)
            {
                *OutError = QuatResult.error().Message;
            }
            return false;
        }

        PushQuatTable(L, QuatResult->get());
        return true;
    }

    if (Type == UuidTypeId() || Type == TypeIdTypeId())
    {
        auto UuidResult = Value.AsConstRef<Uuid>();
        if (!UuidResult)
        {
            if (OutError)
            {
                *OutError = UuidResult.error().Message;
            }
            return false;
        }

        const std::string Text = ToString(UuidResult->get());
        lua_pushlstring(L, Text.data(), Text.size());
        return true;
    }

    if (Type == NodeHandleTypeId())
    {
        auto HandleResult = Value.AsConstRef<NodeHandle>();
        if (!HandleResult)
        {
            if (OutError)
            {
                *OutError = HandleResult.error().Message;
            }
            return false;
        }

        const std::string Text = ToString(HandleResult->get().Id);
        lua_pushlstring(L, Text.data(), Text.size());
        return true;
    }

    if (Type == ComponentHandleTypeId())
    {
        auto HandleResult = Value.AsConstRef<ComponentHandle>();
        if (!HandleResult)
        {
            if (OutError)
            {
                *OutError = HandleResult.error().Message;
            }
            return false;
        }

        const std::string Text = ToString(HandleResult->get().Id);
        lua_pushlstring(L, Text.data(), Text.size());
        return true;
    }

    if (OutError)
    {
        *OutError = "Unsupported Variant type for Lua conversion";
    }
    return false;
}

[[nodiscard]] TExpected<Variant> LuaValueToVariant(lua_State* L, int Index)
{
    const int LuaType = lua_type(L, Index);
    switch (LuaType)
    {
    case LUA_TNIL:
        return Variant::Void();
    case LUA_TBOOLEAN:
        return Variant::FromValue(lua_toboolean(L, Index) != 0);
    case LUA_TNUMBER:
        if (IsExpectedInteger(L, Index))
        {
            const lua_Integer Value = lua_tointeger(L, Index);
            if (Value >= static_cast<lua_Integer>(std::numeric_limits<int>::min())
                && Value <= static_cast<lua_Integer>(std::numeric_limits<int>::max()))
            {
                return Variant::FromValue(static_cast<int>(Value));
            }
            if (Value >= 0)
            {
                return Variant::FromValue(static_cast<std::uint64_t>(Value));
            }
            return Variant::FromValue(static_cast<double>(Value));
        }
        return Variant::FromValue(static_cast<double>(lua_tonumber(L, Index)));
    case LUA_TSTRING:
    {
        std::size_t Size = 0;
        const char* Text = lua_tolstring(L, Index, &Size);
        if (!Text)
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Failed to read lua string"));
        }
        return Variant::FromValue(std::string(Text, Size));
    }
    default:
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Unsupported lua value type"));
    }
}

[[nodiscard]] bool LuaValueToExpectedType(lua_State* L,
                                          int Index,
                                          const TypeId& ExpectedType,
                                          Variant& OutValue,
                                          std::string& OutError)
{
    if (ExpectedType == BoolTypeId())
    {
        OutValue = Variant::FromValue(lua_toboolean(L, Index) != 0);
        return true;
    }

    if (ExpectedType == IntTypeId())
    {
        if (!lua_isinteger(L, Index))
        {
            OutError = "Expected integer";
            return false;
        }

        const lua_Integer Value = lua_tointeger(L, Index);
        if (Value < static_cast<lua_Integer>(std::numeric_limits<int>::min())
            || Value > static_cast<lua_Integer>(std::numeric_limits<int>::max()))
        {
            OutError = "Integer is out of range for int";
            return false;
        }

        OutValue = Variant::FromValue(static_cast<int>(Value));
        return true;
    }

    if (ExpectedType == UnsignedIntTypeId())
    {
        if (!lua_isinteger(L, Index))
        {
            OutError = "Expected integer";
            return false;
        }

        const lua_Integer Value = lua_tointeger(L, Index);
        if (Value < 0 || Value > static_cast<lua_Integer>(std::numeric_limits<unsigned int>::max()))
        {
            OutError = "Integer is out of range for unsigned int";
            return false;
        }

        OutValue = Variant::FromValue(static_cast<unsigned int>(Value));
        return true;
    }

    if (ExpectedType == UInt64TypeId())
    {
        if (!lua_isinteger(L, Index))
        {
            OutError = "Expected integer";
            return false;
        }

        const lua_Integer Value = lua_tointeger(L, Index);
        if (Value < 0)
        {
            OutError = "Integer is out of range for uint64";
            return false;
        }

        OutValue = Variant::FromValue(static_cast<std::uint64_t>(Value));
        return true;
    }

    if (ExpectedType == FloatTypeId())
    {
        if (!lua_isnumber(L, Index))
        {
            OutError = "Expected numeric value";
            return false;
        }

        OutValue = Variant::FromValue(static_cast<float>(lua_tonumber(L, Index)));
        return true;
    }

    if (ExpectedType == DoubleTypeId())
    {
        if (!lua_isnumber(L, Index))
        {
            OutError = "Expected numeric value";
            return false;
        }

        OutValue = Variant::FromValue(static_cast<double>(lua_tonumber(L, Index)));
        return true;
    }

    if (ExpectedType == StringTypeId())
    {
        if (!lua_isstring(L, Index))
        {
            OutError = "Expected string";
            return false;
        }

        std::size_t Size = 0;
        const char* Text = lua_tolstring(L, Index, &Size);
        if (!Text)
        {
            OutError = "Failed to read lua string";
            return false;
        }

        OutValue = Variant::FromValue(std::string(Text, Size));
        return true;
    }

    if (ExpectedType == Vec2TypeId())
    {
        if (!lua_istable(L, Index))
        {
            OutError = "Expected vec2 table";
            return false;
        }

        lua_Number X = 0.0;
        lua_Number Y = 0.0;
        if (!ReadNumericFieldOrIndex(L, Index, "x", 1, X)
            || !ReadNumericFieldOrIndex(L, Index, "y", 2, Y))
        {
            OutError = "Expected vec2 with numeric x/y (or [1]/[2])";
            return false;
        }

        OutValue = Variant::FromValue(Vec2(static_cast<Vec2::Scalar>(X), static_cast<Vec2::Scalar>(Y)));
        return true;
    }

    if (ExpectedType == Vec3TypeId())
    {
        if (!lua_istable(L, Index))
        {
            OutError = "Expected vec3 table";
            return false;
        }

        lua_Number X = 0.0;
        lua_Number Y = 0.0;
        lua_Number Z = 0.0;
        if (!ReadNumericFieldOrIndex(L, Index, "x", 1, X)
            || !ReadNumericFieldOrIndex(L, Index, "y", 2, Y)
            || !ReadNumericFieldOrIndex(L, Index, "z", 3, Z))
        {
            OutError = "Expected vec3 with numeric x/y/z (or [1]/[2]/[3])";
            return false;
        }

        OutValue = Variant::FromValue(Vec3(static_cast<Vec3::Scalar>(X),
                                           static_cast<Vec3::Scalar>(Y),
                                           static_cast<Vec3::Scalar>(Z)));
        return true;
    }

    if (ExpectedType == Vec4TypeId())
    {
        if (!lua_istable(L, Index))
        {
            OutError = "Expected vec4 table";
            return false;
        }

        lua_Number X = 0.0;
        lua_Number Y = 0.0;
        lua_Number Z = 0.0;
        lua_Number W = 0.0;
        if (!ReadNumericFieldOrIndex(L, Index, "x", 1, X)
            || !ReadNumericFieldOrIndex(L, Index, "y", 2, Y)
            || !ReadNumericFieldOrIndex(L, Index, "z", 3, Z)
            || !ReadNumericFieldOrIndex(L, Index, "w", 4, W))
        {
            OutError = "Expected vec4 with numeric x/y/z/w (or [1]/[2]/[3]/[4])";
            return false;
        }

        OutValue = Variant::FromValue(Vec4(static_cast<Vec4::Scalar>(X),
                                           static_cast<Vec4::Scalar>(Y),
                                           static_cast<Vec4::Scalar>(Z),
                                           static_cast<Vec4::Scalar>(W)));
        return true;
    }

    if (ExpectedType == QuatTypeId())
    {
        if (!lua_istable(L, Index))
        {
            OutError = "Expected quaternion table";
            return false;
        }

        lua_Number X = 0.0;
        lua_Number Y = 0.0;
        lua_Number Z = 0.0;
        lua_Number W = 0.0;
        if (!ReadNumericFieldOrIndex(L, Index, "x", 1, X)
            || !ReadNumericFieldOrIndex(L, Index, "y", 2, Y)
            || !ReadNumericFieldOrIndex(L, Index, "z", 3, Z)
            || !ReadNumericFieldOrIndex(L, Index, "w", 4, W))
        {
            OutError = "Expected quaternion with numeric x/y/z/w (or [1]/[2]/[3]/[4])";
            return false;
        }

        OutValue = Variant::FromValue(Quat(static_cast<Quat::Scalar>(W),
                                           static_cast<Quat::Scalar>(X),
                                           static_cast<Quat::Scalar>(Y),
                                           static_cast<Quat::Scalar>(Z)));
        return true;
    }

    if (ExpectedType == UuidTypeId() || ExpectedType == TypeIdTypeId())
    {
        if (!lua_isstring(L, Index))
        {
            OutError = "Expected UUID string";
            return false;
        }

        std::size_t Size = 0;
        const char* Text = lua_tolstring(L, Index, &Size);
        if (!Text)
        {
            OutError = "Failed to read UUID string";
            return false;
        }

        const std::optional<Uuid> Parsed = ParseUuid(std::string_view(Text, Size));
        if (!Parsed)
        {
            OutError = "Invalid UUID string";
            return false;
        }

        OutValue = Variant::FromValue(*Parsed);
        return true;
    }

    if (ExpectedType == NodeHandleTypeId())
    {
        if (!lua_isstring(L, Index))
        {
            OutError = "Expected node-handle UUID string";
            return false;
        }

        std::size_t Size = 0;
        const char* Text = lua_tolstring(L, Index, &Size);
        if (!Text)
        {
            OutError = "Failed to read node-handle UUID string";
            return false;
        }

        const std::optional<Uuid> Parsed = ParseUuid(std::string_view(Text, Size));
        if (!Parsed)
        {
            OutError = "Invalid node-handle UUID";
            return false;
        }

        OutValue = Variant::FromValue(NodeHandle(*Parsed));
        return true;
    }

    if (ExpectedType == ComponentHandleTypeId())
    {
        if (!lua_isstring(L, Index))
        {
            OutError = "Expected component-handle UUID string";
            return false;
        }

        std::size_t Size = 0;
        const char* Text = lua_tolstring(L, Index, &Size);
        if (!Text)
        {
            OutError = "Failed to read component-handle UUID string";
            return false;
        }

        const std::optional<Uuid> Parsed = ParseUuid(std::string_view(Text, Size));
        if (!Parsed)
        {
            OutError = "Invalid component-handle UUID";
            return false;
        }

        OutValue = Variant::FromValue(ComponentHandle(*Parsed));
        return true;
    }

    OutError = "Unsupported target reflected type for Lua conversion";
    return false;
}

#if !defined(SNAPI_GF_ENABLE_SWIG) || (SNAPI_GF_ENABLE_SWIG != 1)
[[nodiscard]] const FieldInfo* FindFieldByName(const TypeId& Type, std::string_view Name)
{
    const auto Fields = TypeRegistry::Instance().CollectFields(Type, true);
    for (const auto& FieldRef : Fields)
    {
        if (!FieldRef.Field)
        {
            continue;
        }

        if (FieldRef.Field->Name == Name)
        {
            return FieldRef.Field;
        }
    }

    return nullptr;
}

[[nodiscard]] std::vector<const MethodInfo*> FindMethodsByName(const TypeId& Type, std::string_view Name)
{
    std::vector<const MethodInfo*> Result{};
    const auto Methods = TypeRegistry::Instance().CollectMethods(Type, true);
    Result.reserve(Methods.size());

    for (const auto& MethodRef : Methods)
    {
        if (!MethodRef.Method)
        {
            continue;
        }

        if (MethodRef.Method->Name == Name)
        {
            Result.push_back(MethodRef.Method);
        }
    }

    return Result;
}

[[nodiscard]] std::optional<TypeId> GetTypeIdArg(lua_State* L, int Index)
{
    std::size_t Size = 0;
    const char* Text = luaL_checklstring(L, Index, &Size);
    if (!Text)
    {
        return std::nullopt;
    }

    const std::string_view Raw(Text, Size);
    if (const auto Parsed = ParseTypeId(Raw))
    {
        return Parsed;
    }

    return TypeIdFromName(Raw);
}

[[nodiscard]] int LuaReflectedObjectIndex(lua_State* L);
[[nodiscard]] int LuaReflectedObjectNewIndex(lua_State* L);
[[nodiscard]] int LuaReflectedObjectToString(lua_State* L);
[[nodiscard]] int LuaReflectedObjectInvokeBound(lua_State* L);

void EnsureReflectedObjectMetatable(lua_State* L)
{
    if (luaL_newmetatable(L, kLuaReflectedObjectMetatable) != 0)
    {
        lua_pushcfunction(L, &LuaReflectedObjectIndex);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, &LuaReflectedObjectNewIndex);
        lua_setfield(L, -2, "__newindex");

        lua_pushcfunction(L, &LuaReflectedObjectToString);
        lua_setfield(L, -2, "__tostring");
    }

    lua_pop(L, 1);
}

[[nodiscard]] LuaReflectedObject* TryGetReflectedObject(lua_State* L, int Index)
{
    return static_cast<LuaReflectedObject*>(luaL_testudata(L, Index, kLuaReflectedObjectMetatable));
}

[[nodiscard]] LuaReflectedObject* CheckReflectedObject(lua_State* L, int Index)
{
    return static_cast<LuaReflectedObject*>(luaL_checkudata(L, Index, kLuaReflectedObjectMetatable));
}

void PushReflectedObject(lua_State* L, void* Instance, const TypeId& Type)
{
    if (!Instance)
    {
        lua_pushnil(L);
        return;
    }

    EnsureReflectedObjectMetatable(L);
    auto* Object = static_cast<LuaReflectedObject*>(lua_newuserdatauv(L, sizeof(LuaReflectedObject), 0));
    const auto Parts = ToParts(Type);
    Object->Instance = Instance;
    Object->TypeHigh = Parts.High;
    Object->TypeLow = Parts.Low;

    luaL_getmetatable(L, kLuaReflectedObjectMetatable);
    lua_setmetatable(L, -2);
}

[[nodiscard]] int InvokeReflectedMethodFromLua(lua_State* L,
                                               void* Instance,
                                               const TypeId& Type,
                                               const std::string_view MethodName,
                                               const int FirstArgIndex)
{
    const int ArgCount = lua_gettop(L) - FirstArgIndex + 1;
    if (ArgCount < 0)
    {
        return luaL_error(L, "invalid reflected invoke argument range");
    }

    const std::vector<const MethodInfo*> Methods = FindMethodsByName(Type, MethodName);
    if (Methods.empty())
    {
        return luaL_error(L, "Method not found");
    }

    std::string LastConversionError{};
    for (const MethodInfo* Method : Methods)
    {
        if (!Method)
        {
            continue;
        }

        if (static_cast<int>(Method->ParamTypes.size()) != ArgCount)
        {
            continue;
        }

        std::vector<Variant> ConvertedArgs{};
        ConvertedArgs.resize(Method->ParamTypes.size());

        bool ConversionOk = true;
        for (int Index = 0; Index < ArgCount; ++Index)
        {
            std::string ConvertError{};
            if (!LuaValueToExpectedType(L,
                                        FirstArgIndex + Index,
                                        Method->ParamTypes[static_cast<std::size_t>(Index)],
                                        ConvertedArgs[static_cast<std::size_t>(Index)],
                                        ConvertError))
            {
                ConversionOk = false;
                LastConversionError = std::move(ConvertError);
                break;
            }
        }

        if (!ConversionOk)
        {
            continue;
        }

        auto InvokeResult = Method->Invoke(Instance, std::span<const Variant>(ConvertedArgs));
        if (!InvokeResult)
        {
            return luaL_error(L, "Method invoke failed: %s", InvokeResult.error().Message.c_str());
        }

        std::string PushError{};
        if (!PushVariantToLua(L, *InvokeResult, &PushError))
        {
            return luaL_error(L, "Method return conversion failed: %s", PushError.c_str());
        }

        return 1;
    }

    return luaL_error(L,
                      "No overload matched invoke arguments%s%s",
                      LastConversionError.empty() ? "" : ": ",
                      LastConversionError.empty() ? "" : LastConversionError.c_str());
}

[[nodiscard]] int LuaReflectedObjectInvokeBound(lua_State* L)
{
    std::size_t NameSize = 0;
    const char* MethodName = luaL_checklstring(L, lua_upvalueindex(1), &NameSize);
    if (!MethodName)
    {
        return luaL_error(L, "bound method missing method name");
    }

    auto* Object = CheckReflectedObject(L, 1);
    if (!Object->Instance)
    {
        return luaL_error(L, "cannot invoke method on null reflected object");
    }

    return InvokeReflectedMethodFromLua(
        L, Object->Instance, TypeIdFromLuaObject(*Object), std::string_view(MethodName, NameSize), 2);
}

[[nodiscard]] int LuaReflectedObjectIndex(lua_State* L)
{
    auto* Object = CheckReflectedObject(L, 1);
    if (!Object->Instance)
    {
        lua_pushnil(L);
        return 1;
    }

    std::size_t NameSize = 0;
    const char* Name = luaL_checklstring(L, 2, &NameSize);
    if (!Name)
    {
        return luaL_error(L, "reflected object index expects a key");
    }

    const std::string_view Key(Name, NameSize);
    const TypeId Type = TypeIdFromLuaObject(*Object);
    if (Key == "__ptr")
    {
        lua_pushlightuserdata(L, Object->Instance);
        return 1;
    }
    if (Key == "__type")
    {
        const std::string TypeText = ToString(Type);
        lua_pushlstring(L, TypeText.data(), TypeText.size());
        return 1;
    }

    if (const FieldInfo* Field = FindFieldByName(Type, Key))
    {
        if (!Field->Getter)
        {
            return luaL_error(L, "field '%s' is write-only", Name);
        }

        auto ValueResult = Field->Getter(Object->Instance);
        if (!ValueResult)
        {
            return luaL_error(L, "field '%s' getter failed: %s", Name, ValueResult.error().Message.c_str());
        }

        std::string ConversionError{};
        if (!PushVariantToLua(L, *ValueResult, &ConversionError))
        {
            return luaL_error(L, "field '%s' conversion failed: %s", Name, ConversionError.c_str());
        }
        return 1;
    }

    const auto Methods = FindMethodsByName(Type, Key);
    if (!Methods.empty())
    {
        lua_pushvalue(L, 2); // method name
        lua_pushcclosure(L, &LuaReflectedObjectInvokeBound, 1);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

[[nodiscard]] int LuaReflectedObjectNewIndex(lua_State* L)
{
    auto* Object = CheckReflectedObject(L, 1);
    if (!Object->Instance)
    {
        return luaL_error(L, "cannot assign field on null reflected object");
    }

    std::size_t NameSize = 0;
    const char* Name = luaL_checklstring(L, 2, &NameSize);
    if (!Name)
    {
        return luaL_error(L, "reflected object assignment expects a field name");
    }

    const TypeId Type = TypeIdFromLuaObject(*Object);
    const FieldInfo* Field = FindFieldByName(Type, std::string_view(Name, NameSize));
    if (!Field)
    {
        return luaL_error(L, "field '%s' not found", Name);
    }

    if (!Field->Setter)
    {
        return luaL_error(L, "field '%s' is read-only", Name);
    }

    Variant Converted{};
    std::string ConvertError{};
    if (!LuaValueToExpectedType(L, 3, Field->FieldType, Converted, ConvertError))
    {
        return luaL_error(L, "field '%s' conversion failed: %s", Name, ConvertError.c_str());
    }

    auto SetResult = Field->Setter(Object->Instance, Converted);
    if (!SetResult)
    {
        return luaL_error(L, "field '%s' setter failed: %s", Name, SetResult.error().Message.c_str());
    }

    return 0;
}

[[nodiscard]] int LuaReflectedObjectToString(lua_State* L)
{
    auto* Object = CheckReflectedObject(L, 1);
    const TypeId Type = TypeIdFromLuaObject(*Object);
    const std::string TypeText = ToString(Type);
    const std::string Text = "snapi.object<" + TypeText + ">(" +
        (Object->Instance ? std::to_string(reinterpret_cast<std::uintptr_t>(Object->Instance)) : std::string("null")) + ")";
    lua_pushlstring(L, Text.data(), Text.size());
    return 1;
}

[[nodiscard]] bool ResolveReflectionTarget(lua_State* L,
                                           void*& OutInstance,
                                           TypeId& OutType,
                                           int& OutNameIndex,
                                           int& OutFirstArgIndex)
{
    if (auto* Object = TryGetReflectedObject(L, 1))
    {
        if (!Object->Instance)
        {
            return false;
        }

        OutInstance = Object->Instance;
        OutType = TypeIdFromLuaObject(*Object);
        OutNameIndex = 2;
        OutFirstArgIndex = 3;
        return true;
    }

    OutInstance = lua_touserdata(L, 1);
    if (!OutInstance)
    {
        return false;
    }

    const std::optional<TypeId> Type = GetTypeIdArg(L, 2);
    if (!Type)
    {
        return false;
    }

    OutType = *Type;
    OutNameIndex = 3;
    OutFirstArgIndex = 4;
    return true;
}

[[nodiscard]] BaseNode* ResolveNodePointerArg(lua_State* L, int Index)
{
    if (auto* Object = TryGetReflectedObject(L, Index))
    {
        if (!Object->Instance)
        {
            return nullptr;
        }

        const TypeId Type = TypeIdFromLuaObject(*Object);
        if (!TypeRegistry::Instance().IsA(Type, StaticTypeId<BaseNode>()))
        {
            return nullptr;
        }

        return static_cast<BaseNode*>(Object->Instance);
    }

    return static_cast<BaseNode*>(lua_touserdata(L, Index));
}

[[nodiscard]] int LuaObjectWrap(lua_State* L)
{
    if (auto* Existing = TryGetReflectedObject(L, 1))
    {
        (void)Existing;
        lua_settop(L, 1);
        return 1;
    }

    void* Instance = lua_touserdata(L, 1);
    if (!Instance)
    {
        return luaL_error(L, "object.wrap requires a non-null instance pointer");
    }

    const std::optional<TypeId> Type = GetTypeIdArg(L, 2);
    if (!Type)
    {
        return luaL_error(L, "object.wrap requires a valid type id or type name");
    }

    PushReflectedObject(L, Instance, *Type);
    return 1;
}

[[nodiscard]] int LuaNodeWrap(lua_State* L)
{
    if (auto* Existing = TryGetReflectedObject(L, 1))
    {
        const TypeId Type = TypeIdFromLuaObject(*Existing);
        if (TypeRegistry::Instance().IsA(Type, StaticTypeId<BaseNode>()))
        {
            lua_settop(L, 1);
            return 1;
        }
    }

    BaseNode* Node = static_cast<BaseNode*>(lua_touserdata(L, 1));
    if (!Node)
    {
        return luaL_error(L, "node.wrap requires a non-null node pointer");
    }

    PushReflectedObject(L, Node, Node->TypeKey());
    return 1;
}

[[nodiscard]] int LuaComponentWrap(lua_State* L)
{
    if (auto* Existing = TryGetReflectedObject(L, 1))
    {
        const TypeId Type = TypeIdFromLuaObject(*Existing);
        if (TypeRegistry::Instance().IsA(Type, StaticTypeId<BaseComponent>()))
        {
            lua_settop(L, 1);
            return 1;
        }
    }

    BaseComponent* Component = static_cast<BaseComponent*>(lua_touserdata(L, 1));
    if (!Component)
    {
        return luaL_error(L, "component.wrap requires a non-null component pointer");
    }

    PushReflectedObject(L, Component, Component->TypeKey());
    return 1;
}

[[nodiscard]] int LuaNodeGetComponent(lua_State* L)
{
    BaseNode* Node = ResolveNodePointerArg(L, 1);
    if (!Node)
    {
        return luaL_error(L, "node.get_component requires a valid node pointer/object");
    }

    const std::optional<TypeId> ComponentType = GetTypeIdArg(L, 2);
    if (!ComponentType)
    {
        return luaL_error(L, "node.get_component requires a valid component type id or name");
    }

    IWorld* WorldRef = Node->World();
    if (!WorldRef)
    {
        lua_pushnil(L);
        return 1;
    }

    void* ComponentInstance = WorldRef->BorrowedComponent(Node->Handle(), *ComponentType);
    if (!ComponentInstance)
    {
        lua_pushnil(L);
        return 1;
    }

    PushReflectedObject(L, ComponentInstance, *ComponentType);
    return 1;
}

[[nodiscard]] int LuaReflectionTypeIdFromName(lua_State* L)
{
    std::size_t Size = 0;
    const char* Name = luaL_checklstring(L, 1, &Size);
    if (!Name)
    {
        return luaL_error(L, "type_id_from_name requires a type name");
    }

    const TypeId Type = TypeIdFromName(std::string_view(Name, Size));
    const std::string Text = ToString(Type);
    lua_pushlstring(L, Text.data(), Text.size());
    return 1;
}

[[nodiscard]] int LuaReflectionTypeIsRegistered(lua_State* L)
{
    const std::optional<TypeId> Type = GetTypeIdArg(L, 1);
    if (!Type)
    {
        return luaL_error(L, "type_is_registered requires a valid type id");
    }

    const bool Registered = TypeRegistry::Instance().Find(*Type) != nullptr;
    lua_pushboolean(L, Registered ? 1 : 0);
    return 1;
}

[[nodiscard]] int LuaReflectionFieldNames(lua_State* L)
{
    const std::optional<TypeId> Type = GetTypeIdArg(L, 1);
    if (!Type)
    {
        return luaL_error(L, "field_names requires a valid type id");
    }

    const auto Fields = TypeRegistry::Instance().CollectFields(*Type, true);
    lua_newtable(L);

    int LuaIndex = 1;
    for (const auto& FieldRef : Fields)
    {
        if (!FieldRef.Field)
        {
            continue;
        }

        lua_pushinteger(L, LuaIndex++);
        lua_pushlstring(L, FieldRef.Field->Name.data(), FieldRef.Field->Name.size());
        lua_settable(L, -3);
    }

    return 1;
}

[[nodiscard]] int LuaReflectionMethodNames(lua_State* L)
{
    const std::optional<TypeId> Type = GetTypeIdArg(L, 1);
    if (!Type)
    {
        return luaL_error(L, "method_names requires a valid type id");
    }

    const auto Methods = TypeRegistry::Instance().CollectMethods(*Type, true);
    lua_newtable(L);

    int LuaIndex = 1;
    for (const auto& MethodRef : Methods)
    {
        if (!MethodRef.Method)
        {
            continue;
        }

        lua_pushinteger(L, LuaIndex++);
        lua_pushlstring(L, MethodRef.Method->Name.data(), MethodRef.Method->Name.size());
        lua_settable(L, -3);
    }

    return 1;
}

[[nodiscard]] int LuaReflectionGetField(lua_State* L)
{
    void* Instance = nullptr;
    TypeId Type{};
    int NameIndex = 0;
    int FirstArgIndex = 0;
    if (!ResolveReflectionTarget(L, Instance, Type, NameIndex, FirstArgIndex))
    {
        return luaL_error(L, "get_field requires (object, field) or (instance_ptr, type, field)");
    }
    (void)FirstArgIndex;

    std::size_t NameSize = 0;
    const char* Name = luaL_checklstring(L, NameIndex, &NameSize);
    if (!Name)
    {
        return luaL_error(L, "get_field requires a field name");
    }

    const FieldInfo* Field = FindFieldByName(Type, std::string_view(Name, NameSize));
    if (!Field)
    {
        return luaL_error(L, "Field not found");
    }
    if (!Field->Getter)
    {
        return luaL_error(L, "Field getter is not available");
    }

    auto ValueResult = Field->Getter(Instance);
    if (!ValueResult)
    {
        return luaL_error(L, "Field getter failed: %s", ValueResult.error().Message.c_str());
    }

    std::string ConversionError{};
    if (!PushVariantToLua(L, *ValueResult, &ConversionError))
    {
        return luaL_error(L, "Field value conversion failed: %s", ConversionError.c_str());
    }

    return 1;
}

[[nodiscard]] int LuaReflectionSetField(lua_State* L)
{
    void* Instance = nullptr;
    TypeId Type{};
    int NameIndex = 0;
    int ValueIndex = 0;
    if (!ResolveReflectionTarget(L, Instance, Type, NameIndex, ValueIndex))
    {
        return luaL_error(L, "set_field requires (object, field, value) or (instance_ptr, type, field, value)");
    }

    std::size_t NameSize = 0;
    const char* Name = luaL_checklstring(L, NameIndex, &NameSize);
    if (!Name)
    {
        return luaL_error(L, "set_field requires a field name");
    }

    const FieldInfo* Field = FindFieldByName(Type, std::string_view(Name, NameSize));
    if (!Field)
    {
        return luaL_error(L, "Field not found");
    }
    if (!Field->Setter)
    {
        return luaL_error(L, "Field setter is not available");
    }

    Variant Converted{};
    std::string ConvertError{};
    if (!LuaValueToExpectedType(L, ValueIndex, Field->FieldType, Converted, ConvertError))
    {
        return luaL_error(L, "Field argument conversion failed: %s", ConvertError.c_str());
    }

    auto SetResult = Field->Setter(Instance, Converted);
    if (!SetResult)
    {
        return luaL_error(L, "Field setter failed: %s", SetResult.error().Message.c_str());
    }

    lua_pushboolean(L, 1);
    return 1;
}

[[nodiscard]] int LuaReflectionInvoke(lua_State* L)
{
    void* Instance = nullptr;
    TypeId Type{};
    int NameIndex = 0;
    int FirstArgIndex = 0;
    if (!ResolveReflectionTarget(L, Instance, Type, NameIndex, FirstArgIndex))
    {
        return luaL_error(L, "invoke requires (object, method, ...) or (instance_ptr, type, method, ...)");
    }

    std::size_t NameSize = 0;
    const char* Name = luaL_checklstring(L, NameIndex, &NameSize);
    if (!Name)
    {
        return luaL_error(L, "invoke requires a method name");
    }

    return InvokeReflectedMethodFromLua(L, Instance, Type, std::string_view(Name, NameSize), FirstArgIndex);
}

void RegisterLuaReflectionApi(lua_State* L)
{
    lua_getglobal(L, "snapi");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "snapi");
    }

    lua_getfield(L, -1, "reflection");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "reflection");
    }

    lua_pushcfunction(L, &LuaReflectionTypeIdFromName);
    lua_setfield(L, -2, "type_id_from_name");

    lua_pushcfunction(L, &LuaReflectionTypeIsRegistered);
    lua_setfield(L, -2, "type_is_registered");

    lua_pushcfunction(L, &LuaReflectionFieldNames);
    lua_setfield(L, -2, "field_names");

    lua_pushcfunction(L, &LuaReflectionMethodNames);
    lua_setfield(L, -2, "method_names");

    lua_pushcfunction(L, &LuaReflectionGetField);
    lua_setfield(L, -2, "get_field");

    lua_pushcfunction(L, &LuaReflectionSetField);
    lua_setfield(L, -2, "set_field");

    lua_pushcfunction(L, &LuaReflectionInvoke);
    lua_setfield(L, -2, "invoke");

    lua_pop(L, 1); // reflection table

    lua_getfield(L, -1, "object");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "object");
    }

    lua_pushcfunction(L, &LuaObjectWrap);
    lua_setfield(L, -2, "wrap");

    lua_pop(L, 1); // object table

    lua_getfield(L, -1, "node");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "node");
    }

    lua_pushcfunction(L, &LuaNodeWrap);
    lua_setfield(L, -2, "wrap");

    lua_pushcfunction(L, &LuaNodeGetComponent);
    lua_setfield(L, -2, "get_component");

    lua_pop(L, 1); // node table

    lua_getfield(L, -1, "component");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "component");
    }

    lua_pushcfunction(L, &LuaComponentWrap);
    lua_setfield(L, -2, "wrap");

    lua_pop(L, 2); // component table + snapi
}
#endif

[[nodiscard]] std::string HookMethodName(const EScriptHook Hook)
{
    switch (Hook)
    {
    case EScriptHook::OnCreate:
        return "OnCreate";
    case EScriptHook::OnDestroy:
        return "OnDestroy";
    case EScriptHook::PreTick:
        return "PreTick";
    case EScriptHook::Tick:
        return "Tick";
    case EScriptHook::FixedTick:
        return "FixedTick";
    case EScriptHook::LateTick:
        return "LateTick";
    case EScriptHook::PostTick:
        return "PostTick";
    }

    return {};
}

[[nodiscard]] std::pair<std::filesystem::file_time_type, bool> ReadLastWriteTime(const std::string& Path)
{
    std::error_code Ec;
    const auto Time = std::filesystem::last_write_time(Path, Ec);
    if (Ec)
    {
        return {std::filesystem::file_time_type{}, false};
    }

    return {Time, true};
}
#endif

} // namespace

#if defined(SNAPI_GF_ENABLE_LUA)
namespace
{
class LuaScriptEngineBackend;

class LuaScript final : public IScript
{
public:
    LuaScript(LuaScriptEngineBackend& Backend,
              const ScriptInstanceId Instance,
              std::string ScriptPath,
              const std::uint64_t Generation,
              const int RegistryRef)
        : m_backend(Backend)
        , m_instanceId(Instance)
        , m_scriptPath(std::move(ScriptPath))
        , m_moduleGeneration(Generation)
        , m_registryRef(RegistryRef)
    {
    }

    ~LuaScript() override;

    [[nodiscard]] ScriptInstanceId InstanceId() const override
    {
        return m_instanceId;
    }

    [[nodiscard]] EScriptBackend BackendType() const override
    {
        return EScriptBackend::Lua;
    }

    [[nodiscard]] std::string_view ScriptPath() const override
    {
        return m_scriptPath;
    }

    [[nodiscard]] std::uint64_t ModuleGeneration() const override
    {
        return m_moduleGeneration;
    }

    Result InvokeHook(const EScriptHook Hook, std::span<const Variant> Args) override;
    TExpected<Variant> Invoke(std::string_view Method, std::span<const Variant> Args) override;

    TExpected<Variant> GetMember(std::string_view Name) const override;
    Result SetMember(std::string_view Name, const Variant& Value) override;

private:
    friend class LuaScriptEngineBackend;

    LuaScriptEngineBackend& m_backend;
    ScriptInstanceId m_instanceId = 0;
    std::string m_scriptPath{};
    std::uint64_t m_moduleGeneration = 0;
    int m_registryRef = LUA_NOREF;
};

class LuaScriptEngineBackend final : public IScriptEngineBackend
{
public:
    ~LuaScriptEngineBackend() override
    {
        (void)Shutdown();
    }

    [[nodiscard]] EScriptBackend BackendType() const override
    {
        return EScriptBackend::Lua;
    }

    Result Initialize() override
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (m_state)
        {
            return Ok();
        }

        m_state = luaL_newstate();
        if (!m_state)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create Lua state"));
        }

        luaL_openlibs(m_state);
        auto ApiResult = RegisterLuaScriptingApi(m_state);
        if (!ApiResult)
        {
            const Error ErrorValue = ApiResult.error();
            lua_close(m_state);
            m_state = nullptr;
            return std::unexpected(ErrorValue);
        }
        return Ok();
    }

    Result Shutdown() override
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (!m_state)
        {
            return Ok();
        }

        for (auto& [Path, Record] : m_modules)
        {
            (void)Path;
            if (Record.RegistryRef != LUA_NOREF)
            {
                luaL_unref(m_state, LUA_REGISTRYINDEX, Record.RegistryRef);
                Record.RegistryRef = LUA_NOREF;
            }
        }

        m_modules.clear();
        lua_close(m_state);
        m_state = nullptr;
        m_nextInstanceId = 1;
        return Ok();
    }

    Result LoadModule(const std::string_view ScriptPath) override
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (!m_state)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Lua backend is not initialized"));
        }

        const std::string NormalizedPath = NormalizePath(ScriptPath);
        if (NormalizedPath.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Script path is empty"));
        }

        return LoadOrReloadModuleLocked(NormalizedPath);
    }

    Result ReloadModule(const std::string_view ScriptPath) override
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (!m_state)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Lua backend is not initialized"));
        }

        const std::string NormalizedPath = NormalizePath(ScriptPath);
        if (NormalizedPath.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Script path is empty"));
        }

        return LoadOrReloadModuleLocked(NormalizedPath);
    }

    [[nodiscard]] std::uint64_t ModuleGeneration(const std::string_view ScriptPath) const override
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        const std::string NormalizedPath = NormalizePath(ScriptPath);
        if (NormalizedPath.empty())
        {
            return 0;
        }

        const auto It = m_modules.find(NormalizedPath);
        if (It == m_modules.end())
        {
            return 0;
        }

        return It->second.Generation;
    }

    TExpected<std::shared_ptr<IScript>> CreateScript(const ScriptCreateInfo& CreateInfo) override
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (!m_state)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Lua backend is not initialized"));
        }

        const std::string NormalizedPath = NormalizePath(CreateInfo.ScriptPath);
        if (NormalizedPath.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Script path is empty"));
        }

        if (auto LoadResult = LoadOrReloadModuleLocked(NormalizedPath); !LoadResult)
        {
            return std::unexpected(LoadResult.error());
        }

        const auto ModuleIt = m_modules.find(NormalizedPath);
        if (ModuleIt == m_modules.end() || ModuleIt->second.RegistryRef == LUA_NOREF)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Script module is not loaded"));
        }

        const int StackTop = lua_gettop(m_state);
        lua_rawgeti(m_state, LUA_REGISTRYINDEX, ModuleIt->second.RegistryRef);

        // Module result may be a factory function that returns the module table.
        if (lua_isfunction(m_state, -1))
        {
            if (lua_pcall(m_state, 0, 1, 0) != LUA_OK)
            {
                const char* ErrorMessage = lua_tostring(m_state, -1);
                const std::string Message = ErrorMessage ? ErrorMessage : "Failed to call Lua module factory";
                lua_settop(m_state, StackTop);
                return std::unexpected(MakeError(EErrorCode::InternalError, Message));
            }
        }

        if (lua_isnil(m_state, -1))
        {
            lua_pop(m_state, 1);
            lua_newtable(m_state);
        }

        if (!CreateInfo.EntryPoint.empty())
        {
            if (!lua_istable(m_state, -1))
            {
                lua_settop(m_state, StackTop);
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "Lua module must evaluate to a table when EntryPoint is used"));
            }

            lua_getfield(m_state, -1, CreateInfo.EntryPoint.c_str());
            lua_remove(m_state, -2);

            if (lua_isnil(m_state, -1))
            {
                lua_settop(m_state, StackTop);
                return std::unexpected(MakeError(EErrorCode::NotFound, "Lua EntryPoint was not found"));
            }

            // Entry point may be a factory function that returns instance table.
            if (lua_isfunction(m_state, -1))
            {
                if (lua_pcall(m_state, 0, 1, 0) != LUA_OK)
                {
                    const char* ErrorMessage = lua_tostring(m_state, -1);
                    const std::string Message = ErrorMessage ? ErrorMessage : "Failed to call Lua EntryPoint";
                    lua_settop(m_state, StackTop);
                    return std::unexpected(MakeError(EErrorCode::InternalError, Message));
                }
            }
        }

        if (!lua_istable(m_state, -1))
        {
            lua_settop(m_state, StackTop);
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Lua script instance must be a table"));
        }

        // Inject context fields for script-side access.
        // New names are Lua-friendly snake_case; __snapi_* aliases remain for compatibility.
        lua_pushlightuserdata(m_state, CreateInfo.Context.World);
        lua_setfield(m_state, -2, "world");
        lua_pushlightuserdata(m_state, CreateInfo.Context.World);
        lua_setfield(m_state, -2, "__snapi_world_ptr");

        lua_pushlightuserdata(m_state, CreateInfo.Context.OwnerNode);
        lua_setfield(m_state, -2, "node");
        lua_pushlightuserdata(m_state, CreateInfo.Context.OwnerNode);
        lua_setfield(m_state, -2, "__snapi_owner_node_ptr");

        const std::string OwnerNodeTypeText = CreateInfo.Context.OwnerNode
            ? ToString(CreateInfo.Context.OwnerNode->TypeKey())
            : std::string{};
        lua_pushlstring(m_state, OwnerNodeTypeText.data(), OwnerNodeTypeText.size());
        lua_setfield(m_state, -2, "node_type");
        lua_pushlstring(m_state, OwnerNodeTypeText.data(), OwnerNodeTypeText.size());
        lua_setfield(m_state, -2, "__snapi_owner_node_type");

        TypeId OwnerComponentType = CreateInfo.Context.OwnerComponentType;
        if (OwnerComponentType == TypeId{} && CreateInfo.Context.OwnerComponent)
        {
            OwnerComponentType = CreateInfo.Context.OwnerComponent->TypeKey();
        }

        lua_pushlightuserdata(m_state, CreateInfo.Context.OwnerComponent);
        lua_setfield(m_state, -2, "component");
        lua_pushlightuserdata(m_state, CreateInfo.Context.OwnerComponent);
        lua_setfield(m_state, -2, "__snapi_owner_component_ptr");

        const std::string OwnerTypeText = ToString(OwnerComponentType);
        lua_pushlstring(m_state, OwnerTypeText.data(), OwnerTypeText.size());
        lua_setfield(m_state, -2, "component_type");
        lua_pushlstring(m_state, OwnerTypeText.data(), OwnerTypeText.size());
        lua_setfield(m_state, -2, "__snapi_owner_component_type");

        const int InstanceRef = luaL_ref(m_state, LUA_REGISTRYINDEX);
        lua_settop(m_state, StackTop);

        const ScriptInstanceId InstanceId = m_nextInstanceId++;
        if (m_nextInstanceId == 0)
        {
            m_nextInstanceId = 1;
        }

        auto Instance = std::make_shared<LuaScript>(*this,
                                                    InstanceId,
                                                    NormalizedPath,
                                                    ModuleIt->second.Generation,
                                                    InstanceRef);
        return std::static_pointer_cast<IScript>(std::move(Instance));
    }

    Result TickHotReload() override
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (!m_state)
        {
            return Ok();
        }

        Error FirstError{};
        std::vector<std::string> DirtyModules{};
        DirtyModules.reserve(m_modules.size());

        for (const auto& [Path, Record] : m_modules)
        {
            if (!Record.HasLastWriteTime)
            {
                continue;
            }

            const auto [LatestWriteTime, HasLatestWriteTime] = ReadLastWriteTime(Path);
            if (!HasLatestWriteTime)
            {
                continue;
            }

            if (LatestWriteTime != Record.LastWriteTime)
            {
                DirtyModules.push_back(Path);
            }
        }

        for (const std::string& Path : DirtyModules)
        {
            auto ReloadResult = LoadOrReloadModuleLocked(Path);
            if (!ReloadResult && !FirstError)
            {
                FirstError = ReloadResult.error();
            }
        }

        if (FirstError)
        {
            return std::unexpected(FirstError);
        }

        return Ok();
    }

    void ReleaseInstanceRef(const int RegistryRef)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (!m_state || RegistryRef == LUA_NOREF)
        {
            return;
        }

        luaL_unref(m_state, LUA_REGISTRYINDEX, RegistryRef);
    }

    Result InvokeHook(const LuaScript& Script, const EScriptHook Hook, std::span<const Variant> Args)
    {
        const std::string MethodName = HookMethodName(Hook);
        if (MethodName.empty())
        {
            return Ok();
        }

        auto InvokeResult = InvokeMethod(Script, MethodName, Args, true);
        if (!InvokeResult)
        {
            return std::unexpected(InvokeResult.error());
        }

        return Ok();
    }

    TExpected<Variant> InvokeMethod(const LuaScript& Script,
                                    const std::string_view Method,
                                    std::span<const Variant> Args,
                                    const bool AllowMissing)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (!m_state)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Lua backend is not initialized"));
        }

        const int StackTop = lua_gettop(m_state);
        lua_rawgeti(m_state, LUA_REGISTRYINDEX, Script.m_registryRef);
        if (!lua_istable(m_state, -1))
        {
            lua_settop(m_state, StackTop);
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Lua script instance is not a table"));
        }

        std::string MethodName(Method);
        lua_getfield(m_state, -1, MethodName.c_str());
        if (lua_isnil(m_state, -1))
        {
            lua_settop(m_state, StackTop);
            if (AllowMissing)
            {
                return Variant::Void();
            }

            return std::unexpected(MakeError(EErrorCode::NotFound, "Lua script method not found"));
        }

        if (!lua_isfunction(m_state, -1))
        {
            lua_settop(m_state, StackTop);
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Lua script method is not callable"));
        }

        // Push self table for method invocation.
        lua_pushvalue(m_state, -2);

        for (const Variant& Arg : Args)
        {
            std::string ConversionError{};
            if (!PushVariantToLua(m_state, Arg, &ConversionError))
            {
                lua_settop(m_state, StackTop);
                return std::unexpected(MakeError(EErrorCode::TypeMismatch,
                                                 std::string("Failed to push script argument: ") + ConversionError));
            }
        }

        if (lua_pcall(m_state, 1 + static_cast<int>(Args.size()), 1, 0) != LUA_OK)
        {
            const char* ErrorMessage = lua_tostring(m_state, -1);
            const std::string Message = ErrorMessage ? ErrorMessage : "Lua script method invocation failed";
            lua_settop(m_state, StackTop);
            return std::unexpected(MakeError(EErrorCode::InternalError, Message));
        }

        auto ReturnResult = LuaValueToVariant(m_state, -1);
        lua_settop(m_state, StackTop);
        if (!ReturnResult)
        {
            return std::unexpected(ReturnResult.error());
        }

        return *ReturnResult;
    }

    TExpected<Variant> GetMember(const LuaScript& Script, const std::string_view Name)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (!m_state)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Lua backend is not initialized"));
        }

        const int StackTop = lua_gettop(m_state);
        lua_rawgeti(m_state, LUA_REGISTRYINDEX, Script.m_registryRef);
        if (!lua_istable(m_state, -1))
        {
            lua_settop(m_state, StackTop);
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Lua script instance is not a table"));
        }

        std::string FieldName(Name);
        lua_getfield(m_state, -1, FieldName.c_str());
        auto ValueResult = LuaValueToVariant(m_state, -1);
        lua_settop(m_state, StackTop);
        if (!ValueResult)
        {
            return std::unexpected(ValueResult.error());
        }

        return *ValueResult;
    }

    Result SetMember(const LuaScript& Script, const std::string_view Name, const Variant& Value)
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        if (!m_state)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Lua backend is not initialized"));
        }

        const int StackTop = lua_gettop(m_state);
        lua_rawgeti(m_state, LUA_REGISTRYINDEX, Script.m_registryRef);
        if (!lua_istable(m_state, -1))
        {
            lua_settop(m_state, StackTop);
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Lua script instance is not a table"));
        }

        std::string PushError{};
        if (!PushVariantToLua(m_state, Value, &PushError))
        {
            lua_settop(m_state, StackTop);
            return std::unexpected(MakeError(EErrorCode::TypeMismatch,
                                             std::string("Failed to push member value: ") + PushError));
        }

        std::string FieldName(Name);
        lua_setfield(m_state, -2, FieldName.c_str());
        lua_settop(m_state, StackTop);
        return Ok();
    }

private:
    struct ModuleRecord
    {
        int RegistryRef = LUA_NOREF;
        std::uint64_t Generation = 0;
        std::filesystem::file_time_type LastWriteTime{};
        bool HasLastWriteTime = false;
    };

    Result LoadOrReloadModuleLocked(const std::string& NormalizedPath)
    {
        if (!m_state)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Lua backend is not initialized"));
        }

        if (luaL_loadfile(m_state, NormalizedPath.c_str()) != LUA_OK)
        {
            const char* ErrorMessage = lua_tostring(m_state, -1);
            const std::string Message = ErrorMessage ? ErrorMessage : "Failed to compile Lua script";
            lua_pop(m_state, 1);
            return std::unexpected(MakeError(EErrorCode::InternalError, Message));
        }

        if (lua_pcall(m_state, 0, 1, 0) != LUA_OK)
        {
            const char* ErrorMessage = lua_tostring(m_state, -1);
            const std::string Message = ErrorMessage ? ErrorMessage : "Failed to execute Lua script";
            lua_pop(m_state, 1);
            return std::unexpected(MakeError(EErrorCode::InternalError, Message));
        }

        if (lua_isnil(m_state, -1))
        {
            lua_pop(m_state, 1);
            lua_newtable(m_state);
        }

        if (!lua_istable(m_state, -1) && !lua_isfunction(m_state, -1))
        {
            lua_pop(m_state, 1);
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Lua script module must evaluate to table or function"));
        }

        const int NewRef = luaL_ref(m_state, LUA_REGISTRYINDEX);

        auto& Module = m_modules[NormalizedPath];
        if (Module.RegistryRef != LUA_NOREF)
        {
            luaL_unref(m_state, LUA_REGISTRYINDEX, Module.RegistryRef);
        }

        Module.RegistryRef = NewRef;
        ++Module.Generation;
        if (Module.Generation == 0)
        {
            Module.Generation = 1;
        }

        const auto [LatestWriteTime, HasLatestWriteTime] = ReadLastWriteTime(NormalizedPath);
        Module.LastWriteTime = LatestWriteTime;
        Module.HasLastWriteTime = HasLatestWriteTime;

        return Ok();
    }

    mutable std::mutex m_mutex{};
    lua_State* m_state = nullptr;
    ScriptInstanceId m_nextInstanceId = 1;
    std::unordered_map<std::string, ModuleRecord> m_modules{};
};

LuaScript::~LuaScript()
{
    m_backend.ReleaseInstanceRef(m_registryRef);
    m_registryRef = LUA_NOREF;
}

Result LuaScript::InvokeHook(const EScriptHook Hook, std::span<const Variant> Args)
{
    return m_backend.InvokeHook(*this, Hook, Args);
}

TExpected<Variant> LuaScript::Invoke(const std::string_view Method, std::span<const Variant> Args)
{
    return m_backend.InvokeMethod(*this, Method, Args, false);
}

TExpected<Variant> LuaScript::GetMember(std::string_view Name) const
{
    return m_backend.GetMember(*this, Name);
}

Result LuaScript::SetMember(std::string_view Name, const Variant& Value)
{
    return m_backend.SetMember(*this, Name, Value);
}
} // namespace
#endif

const char* ToString(const EScriptBackend Backend)
{
    switch (Backend)
    {
    case EScriptBackend::None:
        return "None";
    case EScriptBackend::Lua:
        return "Lua";
    }

    return "Unknown";
}

ScriptRuntimeService::~ScriptRuntimeService()
{
    Shutdown();
}

Result ScriptRuntimeService::RegisterBackend(std::unique_ptr<IScriptEngineBackend> Backend)
{
    if (!Backend)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Cannot register null script backend"));
    }

    const EScriptBackend BackendType = Backend->BackendType();
    if (BackendType == EScriptBackend::None)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Cannot register script backend of type None"));
    }

    const std::size_t Index = BackendIndex(BackendType);
    if (Index >= kBackendSlotCount)
    {
        return std::unexpected(MakeError(EErrorCode::OutOfRange, "Script backend enum is out of range"));
    }

    RuntimeEntry& Entry = m_entries[Index];
    if (Entry.Backend)
    {
        return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Script backend is already registered"));
    }

    Entry.Backend = std::move(Backend);
    Entry.Initialized = false;
    return Ok();
}

bool ScriptRuntimeService::HasBackend(const EScriptBackend BackendType) const
{
    const std::size_t Index = BackendIndex(BackendType);
    if (Index >= kBackendSlotCount)
    {
        return false;
    }

    return m_entries[Index].Backend != nullptr;
}

TExpected<IScriptEngineBackend*> ScriptRuntimeService::Backend(const EScriptBackend BackendType)
{
    const std::size_t Index = BackendIndex(BackendType);
    if (Index >= kBackendSlotCount)
    {
        return std::unexpected(MakeError(EErrorCode::OutOfRange, "Script backend enum is out of range"));
    }

    RuntimeEntry& Entry = m_entries[Index];
    if (!Entry.Backend)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Script backend is not registered"));
    }

    return Entry.Backend.get();
}

TExpected<const IScriptEngineBackend*> ScriptRuntimeService::Backend(const EScriptBackend BackendType) const
{
    const std::size_t Index = BackendIndex(BackendType);
    if (Index >= kBackendSlotCount)
    {
        return std::unexpected(MakeError(EErrorCode::OutOfRange, "Script backend enum is out of range"));
    }

    const RuntimeEntry& Entry = m_entries[Index];
    if (!Entry.Backend)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Script backend is not registered"));
    }

    return Entry.Backend.get();
}

TExpected<std::shared_ptr<IScript>> ScriptRuntimeService::CreateScript(const EScriptBackend BackendType,
                                                                        const ScriptCreateInfo& CreateInfo)
{
    const std::size_t Index = BackendIndex(BackendType);
    if (Index >= kBackendSlotCount)
    {
        return std::unexpected(MakeError(EErrorCode::OutOfRange, "Script backend enum is out of range"));
    }

    RuntimeEntry& Entry = m_entries[Index];
    if (!Entry.Backend)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Script backend is not registered"));
    }

    auto InitResult = EnsureBackendInitialized(Entry);
    if (!InitResult)
    {
        return std::unexpected(InitResult.error());
    }

    return Entry.Backend->CreateScript(CreateInfo);
}

std::uint64_t ScriptRuntimeService::ModuleGeneration(const EScriptBackend BackendType, const std::string_view ScriptPath) const
{
    const std::size_t Index = BackendIndex(BackendType);
    if (Index >= kBackendSlotCount)
    {
        return 0;
    }

    const RuntimeEntry& Entry = m_entries[Index];
    if (!Entry.Backend)
    {
        return 0;
    }

    return Entry.Backend->ModuleGeneration(ScriptPath);
}

Result ScriptRuntimeService::TickHotReload()
{
    Error FirstError{};

    for (RuntimeEntry& Entry : m_entries)
    {
        if (!Entry.Backend || !Entry.Initialized)
        {
            continue;
        }

        auto TickResult = Entry.Backend->TickHotReload();
        if (!TickResult && !FirstError)
        {
            FirstError = TickResult.error();
        }
    }

    if (FirstError)
    {
        return std::unexpected(FirstError);
    }

    return Ok();
}

void ScriptRuntimeService::Shutdown()
{
    for (RuntimeEntry& Entry : m_entries)
    {
        if (!Entry.Backend)
        {
            continue;
        }

        if (Entry.Initialized)
        {
            (void)Entry.Backend->Shutdown();
        }

        Entry.Initialized = false;
    }
}

std::size_t ScriptRuntimeService::BackendIndex(const EScriptBackend BackendType)
{
    return static_cast<std::size_t>(BackendType);
}

Result ScriptRuntimeService::EnsureBackendInitialized(RuntimeEntry& Entry)
{
    if (!Entry.Backend)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Script backend is not registered"));
    }

    if (Entry.Initialized)
    {
        return Ok();
    }

    auto InitResult = Entry.Backend->Initialize();
    if (!InitResult)
    {
        return std::unexpected(InitResult.error());
    }

    Entry.Initialized = true;
    return Ok();
}

void RegisterBuiltinScriptBackends(ScriptRuntimeService& Runtime)
{
#if defined(SNAPI_GF_ENABLE_LUA)
    auto RegisterResult = Runtime.RegisterBackend(std::make_unique<LuaScriptEngineBackend>());
    if (!RegisterResult)
    {
        std::cerr << "Warning: Failed to register Lua script backend: " << RegisterResult.error().Message << '\n';
    }
#else
    (void)Runtime;
#endif
}

} // namespace SnAPI::GameFramework
