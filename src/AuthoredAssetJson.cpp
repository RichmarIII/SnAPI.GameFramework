#include "AuthoredAssetJson.h"

#include <array>
#include <new>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "Conduit/Asset.h"
#include "Conduit/Value.h"
#include "IAsset.h"
#include "Math.h"
#include "NodeAsset.h"
#include "RenderAssetPayloads.h"
#include "Serialization.h"
#include "TypeRegistration.h"

namespace SnAPI::GameFramework
{
namespace
{

using Json = nlohmann::json;

constexpr std::string_view kOpaqueJsonBytesField = "$bytes";
constexpr std::string_view kOpaqueJsonTypeField = "$type";

TExpected<Json> SerializeValueToJson(const TypeId& Type, const void* Value);
Result DeserializeValueFromJsonInto(const TypeId& Type, void* Value, const Json& Source, bool TolerateFailures);
Result DeserializeObjectFromJson(const TypeId& Type,
                                 void* Value,
                                 const Json& Source,
                                 bool TolerateFailures,
                                 AuthoredAssetImportDiagnostics* Diagnostics,
                                 std::string_view Path);

template<typename TScalar>
Result ReadNumericJsonComponent(const Json& Source, const std::string_view Name, const std::size_t ArrayIndex, TScalar& OutValue)
{
    try
    {
        if (Source.is_object())
        {
            const auto It = Source.find(std::string(Name));
            if (It == Source.end())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "JSON object is missing component '" + std::string(Name) + "'"));
            }

            OutValue = static_cast<TScalar>(It->get<double>());
            return Ok();
        }

        if (Source.is_array())
        {
            if (ArrayIndex >= Source.size())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "JSON array is missing component index " + std::to_string(ArrayIndex)));
            }

            OutValue = static_cast<TScalar>(Source[ArrayIndex].get<double>());
            return Ok();
        }
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
    }

    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "JSON value must be an object or array"));
}

TExpected<Json> SerializeOpaqueValueToJson(const TypeId& Type, const void* Value)
{
    if (!Value)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null JSON value"));
    }

    std::vector<std::uint8_t> Bytes{};
    auto SerializeResult = SerializeReflectedValue(Type, Value, Bytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error());
    }

    Json Out = Json::object();
    Out[std::string(kOpaqueJsonBytesField)] = std::move(Bytes);
    if (const TypeInfo* Info = TypeRegistry::Instance().Find(Type))
    {
        Out[std::string(kOpaqueJsonTypeField)] = Info->Name;
    }
    else
    {
        Out[std::string(kOpaqueJsonTypeField)] = ToString(Type);
    }
    return Out;
}

Result DeserializeOpaqueValueFromJsonInto(const TypeId& Type, void* Value, const Json& Source)
{
    if (!Value)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null JSON destination"));
    }
    if (!Source.is_object())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Opaque JSON value must be an object"));
    }

    const auto BytesIt = Source.find(std::string(kOpaqueJsonBytesField));
    if (BytesIt == Source.end() || !BytesIt->is_array())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Opaque JSON value is missing '$bytes'"));
    }

    try
    {
        const auto Bytes = BytesIt->get<std::vector<std::uint8_t>>();
        auto DeserializeResult = DeserializeReflectedValueInto(Type, Value, Bytes.data(), Bytes.size());
        if (!DeserializeResult)
        {
            return std::unexpected(DeserializeResult.error());
        }
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
    }
}

void AddImportDiagnostic(AuthoredAssetImportDiagnostics* Diagnostics,
                         const std::string_view Path,
                         const Error& Error)
{
    if (!Diagnostics)
    {
        return;
    }

    if (Path.empty())
    {
        Diagnostics->push_back(Error.Message);
        return;
    }

    Diagnostics->push_back(std::string(Path) + ": " + Error.Message);
}

struct JsonCodecEntry
{
    TExpected<Json> (*Encode)(const void* Value) = nullptr;
    Result (*DecodeInto)(void* Value, const Json& Source) = nullptr;
};

class JsonCodecRegistry
{
public:
    static JsonCodecRegistry& Instance()
    {
        static JsonCodecRegistry Registry{};
        Registry.EnsureBuilt();
        return Registry;
    }

    [[nodiscard]] const JsonCodecEntry* Find(const TypeId& Type) const
    {
        const auto It = m_entries.find(Type);
        return It != m_entries.end() ? &It->second : nullptr;
    }

private:
    template<typename T>
    void RegisterScalar()
    {
        m_entries.emplace(StaticTypeId<T>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null JSON scalar value"));
                                  }
                                  return Json(*static_cast<const T*>(Value));
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null JSON scalar destination"));
                                  }
                                  try
                                  {
                                      *static_cast<T*>(Value) = Source.get<T>();
                                      return Ok();
                                  }
                                  catch (const std::exception& Ex)
                                  {
                                      return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
                                  }
                              },
                          });
    }

    template<typename TElement>
    void RegisterVector()
    {
        m_entries.emplace(StaticTypeId<std::vector<TElement>>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null JSON vector value"));
                                  }

                                  const auto& VectorValue = *static_cast<const std::vector<TElement>*>(Value);
                                  Json Out = Json::array();
                                  for (const TElement& Element : VectorValue)
                                  {
                                      auto ElementResult = SerializeValueToJson(StaticTypeId<TElement>(), &Element);
                                      if (!ElementResult)
                                      {
                                          return std::unexpected(ElementResult.error());
                                      }
                                      Out.push_back(std::move(*ElementResult));
                                  }
                                  return Out;
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null JSON vector destination"));
                                  }
                                  if (!Source.is_array())
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "JSON value is not an array"));
                                  }

                                  std::vector<TElement> Parsed{};
                                  Parsed.reserve(Source.size());
                                  for (const auto& ElementJson : Source)
                                  {
                                      TElement Element{};
                                      auto ElementResult = DeserializeValueFromJsonInto(
                                          StaticTypeId<TElement>(),
                                          &Element,
                                          ElementJson,
                                          false);
                                      if (!ElementResult)
                                      {
                                          return ElementResult;
                                      }
                                      Parsed.push_back(std::move(Element));
                                  }
                                  *static_cast<std::vector<TElement>*>(Value) = std::move(Parsed);
                                  return Ok();
                              },
                          });
    }

    template<typename TElement, std::size_t N>
    void RegisterArray()
    {
        m_entries.emplace(StaticTypeId<std::array<TElement, N>>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null JSON array value"));
                                  }

                                  const auto& ArrayValue = *static_cast<const std::array<TElement, N>*>(Value);
                                  Json Out = Json::array();
                                  for (const TElement& Element : ArrayValue)
                                  {
                                      auto ElementResult = SerializeValueToJson(StaticTypeId<TElement>(), &Element);
                                      if (!ElementResult)
                                      {
                                          return std::unexpected(ElementResult.error());
                                      }
                                      Out.push_back(std::move(*ElementResult));
                                  }
                                  return Out;
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null JSON array destination"));
                                  }
                                  if (!Source.is_array() || Source.size() != N)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "JSON value is not the expected array size"));
                                  }

                                  std::array<TElement, N> Parsed{};
                                  for (std::size_t Index = 0; Index < N; ++Index)
                                  {
                                      auto ElementResult = DeserializeValueFromJsonInto(
                                          StaticTypeId<TElement>(),
                                          &Parsed[Index],
                                          Source[Index],
                                          false);
                                      if (!ElementResult)
                                      {
                                          return ElementResult;
                                      }
                                  }
                                  *static_cast<std::array<TElement, N>*>(Value) = std::move(Parsed);
                                  return Ok();
                              },
                          });
    }

    void RegisterVec2()
    {
        m_entries.emplace(StaticTypeId<Vec2>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null Vec2 JSON value"));
                                  }

                                  const auto& Vector = *static_cast<const Vec2*>(Value);
                                  Json Out = Json::object();
                                  Out["x"] = Vector.x();
                                  Out["y"] = Vector.y();
                                  return Out;
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null Vec2 JSON destination"));
                                  }

                                  auto& Vector = *static_cast<Vec2*>(Value);
                                  Result ReadResult = ReadNumericJsonComponent(Source, "x", 0, Vector.x());
                                  if (!ReadResult)
                                  {
                                      return ReadResult;
                                  }
                                  return ReadNumericJsonComponent(Source, "y", 1, Vector.y());
                              },
                          });
    }

    void RegisterVec3()
    {
        m_entries.emplace(StaticTypeId<Vec3>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null Vec3 JSON value"));
                                  }

                                  const auto& Vector = *static_cast<const Vec3*>(Value);
                                  Json Out = Json::object();
                                  Out["x"] = Vector.x();
                                  Out["y"] = Vector.y();
                                  Out["z"] = Vector.z();
                                  return Out;
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null Vec3 JSON destination"));
                                  }

                                  auto& Vector = *static_cast<Vec3*>(Value);
                                  Result ReadResult = ReadNumericJsonComponent(Source, "x", 0, Vector.x());
                                  if (!ReadResult)
                                  {
                                      return ReadResult;
                                  }
                                  ReadResult = ReadNumericJsonComponent(Source, "y", 1, Vector.y());
                                  if (!ReadResult)
                                  {
                                      return ReadResult;
                                  }
                                  return ReadNumericJsonComponent(Source, "z", 2, Vector.z());
                              },
                          });
    }

    void RegisterVec4()
    {
        m_entries.emplace(StaticTypeId<Vec4>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null Vec4 JSON value"));
                                  }

                                  const auto& Vector = *static_cast<const Vec4*>(Value);
                                  Json Out = Json::object();
                                  Out["x"] = Vector.x();
                                  Out["y"] = Vector.y();
                                  Out["z"] = Vector.z();
                                  Out["w"] = Vector.w();
                                  return Out;
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null Vec4 JSON destination"));
                                  }

                                  auto& Vector = *static_cast<Vec4*>(Value);
                                  Result ReadResult = ReadNumericJsonComponent(Source, "x", 0, Vector.x());
                                  if (!ReadResult)
                                  {
                                      return ReadResult;
                                  }
                                  ReadResult = ReadNumericJsonComponent(Source, "y", 1, Vector.y());
                                  if (!ReadResult)
                                  {
                                      return ReadResult;
                                  }
                                  ReadResult = ReadNumericJsonComponent(Source, "z", 2, Vector.z());
                                  if (!ReadResult)
                                  {
                                      return ReadResult;
                                  }
                                  return ReadNumericJsonComponent(Source, "w", 3, Vector.w());
                              },
                          });
    }

    void RegisterQuat()
    {
        m_entries.emplace(StaticTypeId<Quat>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null quaternion JSON value"));
                                  }

                                  const auto& Rotation = *static_cast<const Quat*>(Value);
                                  Json Out = Json::object();
                                  Out["x"] = Rotation.x();
                                  Out["y"] = Rotation.y();
                                  Out["z"] = Rotation.z();
                                  Out["w"] = Rotation.w();
                                  return Out;
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null quaternion JSON destination"));
                                  }

                                  auto& Rotation = *static_cast<Quat*>(Value);
                                  Result ReadResult = ReadNumericJsonComponent(Source, "x", 0, Rotation.x());
                                  if (!ReadResult)
                                  {
                                      return ReadResult;
                                  }
                                  ReadResult = ReadNumericJsonComponent(Source, "y", 1, Rotation.y());
                                  if (!ReadResult)
                                  {
                                      return ReadResult;
                                  }
                                  ReadResult = ReadNumericJsonComponent(Source, "z", 2, Rotation.z());
                                  if (!ReadResult)
                                  {
                                      return ReadResult;
                                  }
                                  return ReadNumericJsonComponent(Source, "w", 3, Rotation.w());
                              },
                          });
    }

    template<typename THandle>
    void RegisterHandle()
    {
        m_entries.emplace(StaticTypeId<THandle>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null handle JSON value"));
                                  }

                                  const auto& HandleValue = *static_cast<const THandle*>(Value);
                                  if (HandleValue.IsNull())
                                  {
                                      return Json(nullptr);
                                  }
                                  return Json(ToString(HandleValue.Id));
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null handle JSON destination"));
                                  }

                                  auto& HandleValue = *static_cast<THandle*>(Value);
                                  if (Source.is_null())
                                  {
                                      HandleValue = {};
                                      return Ok();
                                  }
                                  if (!Source.is_string())
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Handle JSON value must be a string or null"));
                                  }

                                  const auto Parsed = Uuid::from_string(Source.get<std::string>());
                                  if (!Parsed)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Invalid handle UUID"));
                                  }

                                  HandleValue = THandle(*Parsed);
                                  return Ok();
                              },
                          });
    }

    void EnsureBuilt()
    {
        if (m_built)
        {
            return;
        }

        RegisterScalar<bool>();
        RegisterScalar<int>();
        RegisterScalar<std::int64_t>();
        RegisterScalar<unsigned int>();
        RegisterScalar<std::uint64_t>();
        RegisterScalar<float>();
        RegisterScalar<double>();
        RegisterScalar<std::string>();
        RegisterVec2();
        RegisterVec3();
        RegisterVec4();
        RegisterQuat();
        RegisterHandle<NodeHandle>();
        RegisterHandle<ComponentHandle>();
        RegisterVector<Uuid>();
        RegisterVector<Conduit::SlotId>();
        RegisterVector<Conduit::GraphNodeEditorAsset>();
        RegisterVector<Conduit::GraphCommentAsset>();
        RegisterVector<Conduit::GraphBookmarkAsset>();
        RegisterVector<Conduit::GraphSlotAsset>();
        RegisterVector<Conduit::GraphVariableAsset>();
        RegisterVector<Conduit::GraphNodeAsset>();
        RegisterVector<NodeFieldAsset>();
        RegisterVector<NodeComponentAsset>();
        RegisterVector<NodeObjectAsset>();
        RegisterVector<MaterialScalarParamPayload>();
        RegisterVector<MaterialVectorParamPayload>();
        RegisterVector<MaterialTextureParamPayload>();
        RegisterArray<float, 4>();

        m_entries.emplace(StaticTypeId<std::vector<std::uint8_t>>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null byte-vector JSON value"));
                                  }
                                  return Json(*static_cast<const std::vector<std::uint8_t>*>(Value));
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null byte-vector JSON destination"));
                                  }
                                  if (!Source.is_array())
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Byte-vector JSON value is not an array"));
                                  }
                                  try
                                  {
                                      auto Parsed = Source.get<std::vector<std::uint8_t>>();
                                      *static_cast<std::vector<std::uint8_t>*>(Value) = std::move(Parsed);
                                      return Ok();
                                  }
                                  catch (const std::exception& Ex)
                                  {
                                      return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
                                  }
                              },
                          });

        m_entries.emplace(StaticTypeId<Uuid>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null UUID JSON value"));
                                  }

                                  const Uuid& Id = *static_cast<const Uuid*>(Value);
                                  if (const auto* Info = TypeRegistry::Instance().Find(Id))
                                  {
                                      return Json(Info->Name);
                                  }
                                  return Json(ToString(Id));
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null UUID JSON destination"));
                                  }
                                  if (!Source.is_string())
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "UUID JSON value must be a string"));
                                  }

                                  const std::string Text = Source.get<std::string>();
                                  if (const auto* Info = TypeRegistry::Instance().FindByName(Text))
                                  {
                                      *static_cast<Uuid*>(Value) = Info->Id;
                                      return Ok();
                                  }

                                  const auto Parsed = Uuid::from_string(Text);
                                  if (!Parsed)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Invalid UUID/type string: " + Text));
                                  }

                                  *static_cast<Uuid*>(Value) = *Parsed;
                                  return Ok();
                              },
                          });

        m_entries.emplace(StaticTypeId<Conduit::SerializedValue>(),
                          JsonCodecEntry{
                              .Encode = [] (const void* Value) -> TExpected<Json> {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null serialized-value JSON value"));
                                  }

                                  const auto& Serialized = *static_cast<const Conduit::SerializedValue*>(Value);
                                  Json Out = Json::object();
                                  if (Serialized.Type == TypeId{})
                                  {
                                      Out["Type"] = nullptr;
                                      Out["Value"] = nullptr;
                                      return Out;
                                  }

                                  const TypeInfo* TypeInfoPtr = TypeRegistry::Instance().Find(Serialized.Type);
                                  Out["Type"] = TypeInfoPtr ? Json(TypeInfoPtr->Name) : Json(ToString(Serialized.Type));

                                  if (!TypeInfoPtr || !TypeInfoPtr->RuntimeOps)
                                  {
                                      Out["Bytes"] = Serialized.Bytes;
                                      return Out;
                                  }

                                  void* Storage = ::operator new(TypeInfoPtr->Size, std::align_val_t(TypeInfoPtr->Align));
                                  const auto DestroyStorage = [&]() {
                                      if (TypeInfoPtr->RuntimeOps->Destroy)
                                      {
                                          TypeInfoPtr->RuntimeOps->Destroy(Storage);
                                      }
                                      ::operator delete(Storage, std::align_val_t(TypeInfoPtr->Align));
                                  };

                                  auto ConstructResult = ConstructReflectedValue(
                                      Serialized.Type,
                                      Storage,
                                      Serialized.Bytes.data(),
                                      Serialized.Bytes.size());
                                  if (!ConstructResult)
                                  {
                                      DestroyStorage();
                                      return std::unexpected(ConstructResult.error());
                                  }

                                  auto ValueResult = SerializeValueToJson(Serialized.Type, Storage);
                                  DestroyStorage();
                                  if (!ValueResult)
                                  {
                                      return std::unexpected(ValueResult.error());
                                  }

                                  Out["Value"] = std::move(*ValueResult);
                                  return Out;
                              },
                              .DecodeInto = [] (void* Value, const Json& Source) -> Result {
                                  if (!Value)
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Null serialized-value JSON destination"));
                                  }
                                  if (!Source.is_object())
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "SerializedValue JSON must be an object"));
                                  }

                                  auto& Serialized = *static_cast<Conduit::SerializedValue*>(Value);
                                  Serialized = {};

                                  const auto TypeIt = Source.find("Type");
                                  if (TypeIt == Source.end() || TypeIt->is_null())
                                  {
                                      return Ok();
                                  }
                                  if (!TypeIt->is_string())
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "SerializedValue.Type must be a string"));
                                  }

                                  const std::string TypeText = TypeIt->get<std::string>();
                                  if (const auto* TypeInfoPtr = TypeRegistry::Instance().FindByName(TypeText))
                                  {
                                      Serialized.Type = TypeInfoPtr->Id;
                                  }
                                  else if (const auto Parsed = Uuid::from_string(TypeText); Parsed)
                                  {
                                      Serialized.Type = *Parsed;
                                  }
                                  else
                                  {
                                      return std::unexpected(
                                          MakeError(EErrorCode::InvalidArgument, "Unknown reflected type: " + TypeText));
                                  }

                                  const auto ValueIt = Source.find("Value");
                                  if (ValueIt != Source.end())
                                  {
                                      const TypeInfo* TypeInfoPtr = TypeRegistry::Instance().Find(Serialized.Type);
                                      if (!TypeInfoPtr || !TypeInfoPtr->RuntimeOps || !TypeInfoPtr->RuntimeOps->DefaultConstruct)
                                      {
                                          return std::unexpected(
                                              MakeError(EErrorCode::InvalidArgument,
                                                        "SerializedValue type does not support reflective JSON load"));
                                      }

                                      void* Storage = ::operator new(TypeInfoPtr->Size, std::align_val_t(TypeInfoPtr->Align));
                                      TypeInfoPtr->RuntimeOps->DefaultConstruct(Storage);
                                      const auto DestroyStorage = [&]() {
                                          if (TypeInfoPtr->RuntimeOps->Destroy)
                                          {
                                              TypeInfoPtr->RuntimeOps->Destroy(Storage);
                                          }
                                          ::operator delete(Storage, std::align_val_t(TypeInfoPtr->Align));
                                      };

                                      auto DecodeResult = DeserializeValueFromJsonInto(
                                          Serialized.Type,
                                          Storage,
                                          *ValueIt,
                                          false);
                                      if (!DecodeResult)
                                      {
                                          DestroyStorage();
                                          return DecodeResult;
                                      }

                                      auto SerializeResult = SerializeReflectedValue(
                                          Serialized.Type,
                                          Storage,
                                          Serialized.Bytes);
                                      DestroyStorage();
                                      if (!SerializeResult)
                                      {
                                          return std::unexpected(SerializeResult.error());
                                      }
                                      return Ok();
                                  }

                                  const auto BytesIt = Source.find("Bytes");
                                  if (BytesIt != Source.end())
                                  {
                                      if (!BytesIt->is_array())
                                      {
                                          return std::unexpected(
                                              MakeError(EErrorCode::InvalidArgument, "SerializedValue.Bytes must be an array"));
                                      }
                                      try
                                      {
                                          Serialized.Bytes = BytesIt->get<std::vector<std::uint8_t>>();
                                          return Ok();
                                      }
                                      catch (const std::exception& Ex)
                                      {
                                          return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
                                      }
                                  }

                                  return Ok();
                              },
                          });

        m_built = true;
    }

    bool m_built = false;
    std::unordered_map<TypeId, JsonCodecEntry, UuidHash> m_entries{};
};

[[nodiscard]] bool TryWriteEnumValue(void* Destination, const TypeInfo& Info, const std::int64_t SignedValue, const std::uint64_t UnsignedValue)
{
    switch (Info.Size)
    {
    case 1:
        if (Info.EnumIsSigned)
        {
            *static_cast<std::int8_t*>(Destination) = static_cast<std::int8_t>(SignedValue);
        }
        else
        {
            *static_cast<std::uint8_t*>(Destination) = static_cast<std::uint8_t>(UnsignedValue);
        }
        return true;
    case 2:
        if (Info.EnumIsSigned)
        {
            *static_cast<std::int16_t*>(Destination) = static_cast<std::int16_t>(SignedValue);
        }
        else
        {
            *static_cast<std::uint16_t*>(Destination) = static_cast<std::uint16_t>(UnsignedValue);
        }
        return true;
    case 4:
        if (Info.EnumIsSigned)
        {
            *static_cast<std::int32_t*>(Destination) = static_cast<std::int32_t>(SignedValue);
        }
        else
        {
            *static_cast<std::uint32_t*>(Destination) = static_cast<std::uint32_t>(UnsignedValue);
        }
        return true;
    case 8:
        if (Info.EnumIsSigned)
        {
            *static_cast<std::int64_t*>(Destination) = SignedValue;
        }
        else
        {
            *static_cast<std::uint64_t*>(Destination) = UnsignedValue;
        }
        return true;
    default:
        return false;
    }
}

[[nodiscard]] TExpected<Json> SerializeEnumToJson(const TypeInfo& Info, const void* Value)
{
    if (!Value)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null enum JSON value"));
    }

    std::uint64_t RawUnsigned = 0;
    std::int64_t RawSigned = 0;
    switch (Info.Size)
    {
    case 1:
        if (Info.EnumIsSigned)
        {
            RawSigned = *static_cast<const std::int8_t*>(Value);
            RawUnsigned = static_cast<std::uint8_t>(RawSigned);
        }
        else
        {
            RawUnsigned = *static_cast<const std::uint8_t*>(Value);
        }
        break;
    case 2:
        if (Info.EnumIsSigned)
        {
            RawSigned = *static_cast<const std::int16_t*>(Value);
            RawUnsigned = static_cast<std::uint16_t>(RawSigned);
        }
        else
        {
            RawUnsigned = *static_cast<const std::uint16_t*>(Value);
        }
        break;
    case 4:
        if (Info.EnumIsSigned)
        {
            RawSigned = *static_cast<const std::int32_t*>(Value);
            RawUnsigned = static_cast<std::uint32_t>(RawSigned);
        }
        else
        {
            RawUnsigned = *static_cast<const std::uint32_t*>(Value);
        }
        break;
    case 8:
        if (Info.EnumIsSigned)
        {
            RawSigned = *static_cast<const std::int64_t*>(Value);
            RawUnsigned = static_cast<std::uint64_t>(RawSigned);
        }
        else
        {
            RawUnsigned = *static_cast<const std::uint64_t*>(Value);
        }
        break;
    default:
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported enum size"));
    }

    for (const auto& EnumValue : Info.EnumValues)
    {
        if (Info.EnumIsSigned)
        {
            if (static_cast<std::int64_t>(EnumValue.Value) == RawSigned)
            {
                return Json(EnumValue.Name);
            }
        }
        else if (EnumValue.Value == RawUnsigned)
        {
            return Json(EnumValue.Name);
        }
    }

    return Info.EnumIsSigned ? Json(RawSigned) : Json(RawUnsigned);
}

Result DeserializeEnumFromJsonInto(const TypeInfo& Info, void* Value, const Json& Source)
{
    if (!Value)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null enum JSON destination"));
    }

    if (Source.is_string())
    {
        const std::string Name = Source.get<std::string>();
        for (const auto& EnumValue : Info.EnumValues)
        {
            if (EnumValue.Name == Name)
            {
                if (!TryWriteEnumValue(Value, Info, static_cast<std::int64_t>(EnumValue.Value), EnumValue.Value))
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported enum size"));
                }
                return Ok();
            }
        }
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unknown enum value: " + Name));
    }

    if (Source.is_number_integer())
    {
        const std::int64_t SignedValue = Source.get<std::int64_t>();
        if (!TryWriteEnumValue(Value, Info, SignedValue, static_cast<std::uint64_t>(SignedValue)))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported enum size"));
        }
        return Ok();
    }

    if (Source.is_number_unsigned())
    {
        const std::uint64_t UnsignedValue = Source.get<std::uint64_t>();
        if (!TryWriteEnumValue(Value, Info, static_cast<std::int64_t>(UnsignedValue), UnsignedValue))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported enum size"));
        }
        return Ok();
    }

    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Enum JSON value must be a string or integer"));
}

TExpected<Json> SerializeFieldValueToJson(const FieldInfo& Field, const void* Instance);
Result DeserializeFieldValueFromJson(const FieldInfo& Field,
                                     void* Instance,
                                     const Json& Source,
                                     bool TolerateFailures,
                                     AuthoredAssetImportDiagnostics* Diagnostics,
                                     std::string_view Path);

Result AssignDecodedValue(const TypeInfo& Info, void* Dest, void* Storage)
{
    if (!Info.RuntimeOps)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Field type has no runtime ops"));
    }

    if (Info.RuntimeOps->MoveAssign)
    {
        Info.RuntimeOps->MoveAssign(Storage, Dest);
        return Ok();
    }
    if (Info.RuntimeOps->CopyAssign)
    {
        Info.RuntimeOps->CopyAssign(Storage, Dest);
        return Ok();
    }
    if (Info.RuntimeOps->Destroy && (Info.RuntimeOps->MoveConstruct || Info.RuntimeOps->CopyConstruct))
    {
        Info.RuntimeOps->Destroy(Dest);
        if (Info.RuntimeOps->MoveConstruct)
        {
            Info.RuntimeOps->MoveConstruct(Storage, Dest);
        }
        else
        {
            Info.RuntimeOps->CopyConstruct(Storage, Dest);
        }
        return Ok();
    }

    return std::unexpected(
        MakeError(EErrorCode::InvalidArgument, "Field type does not support assignment during JSON import"));
}

Result AssignDecodedFieldValue(const FieldInfo& Field, void* Instance, void* Storage, const TypeInfo& FieldTypeInfo)
{
    if (Field.RawSetter)
    {
        return Field.RawSetter(Instance, Storage);
    }

    if (Field.MutablePointer)
    {
        if (void* FieldPtr = Field.MutablePointer(Instance))
        {
            return AssignDecodedValue(FieldTypeInfo, FieldPtr, Storage);
        }
    }

    if (Field.ViewGetter)
    {
        auto ViewResult = Field.ViewGetter(Instance);
        if (ViewResult && !ViewResult->IsConst() && ViewResult->UnsafeBorrowedMutable())
        {
            return AssignDecodedValue(FieldTypeInfo, ViewResult->UnsafeBorrowedMutable(), Storage);
        }
    }

    if (Field.Getter)
    {
        auto ValueResult = Field.Getter(Instance);
        if (ValueResult && ValueResult->StorageKind() == Variant::EStorageKind::BorrowedMutable &&
            ValueResult->UnsafeBorrowedMutable())
        {
            return AssignDecodedValue(FieldTypeInfo, ValueResult->UnsafeBorrowedMutable(), Storage);
        }
    }

    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Field '" + Field.Name + "' is not writable"));
}

Result DeserializeFieldValueInPlace(const FieldInfo& Field, void* Instance, const Json& Source)
{
    if (Field.MutablePointer)
    {
        if (void* FieldPtr = Field.MutablePointer(Instance))
        {
            return DeserializeValueFromJsonInto(Field.FieldType, FieldPtr, Source, false);
        }
    }

    if (Field.ViewGetter)
    {
        auto ViewResult = Field.ViewGetter(Instance);
        if (ViewResult && !ViewResult->IsConst() && ViewResult->UnsafeBorrowedMutable())
        {
            return DeserializeValueFromJsonInto(Field.FieldType, ViewResult->UnsafeBorrowedMutable(), Source, false);
        }
    }

    if (Field.Getter)
    {
        auto ValueResult = Field.Getter(Instance);
        if (ValueResult && ValueResult->StorageKind() == Variant::EStorageKind::BorrowedMutable &&
            ValueResult->UnsafeBorrowedMutable())
        {
            return DeserializeValueFromJsonInto(Field.FieldType, ValueResult->UnsafeBorrowedMutable(), Source, false);
        }
    }

    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Field '" + Field.Name + "' is not writable"));
}

TExpected<Json> SerializeObjectToJson(const TypeId& Type, const void* Value)
{
    const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
    }

    Json Out = Json::object();
    for (const TypeId& BaseType : Info->BaseTypes)
    {
        auto BaseResult = SerializeObjectToJson(BaseType, Value);
        if (!BaseResult)
        {
            return std::unexpected(BaseResult.error());
        }
        for (auto It = BaseResult->begin(); It != BaseResult->end(); ++It)
        {
            Out[It.key()] = It.value();
        }
    }

    for (const FieldInfo& Field : Info->Fields)
    {
        auto FieldResult = SerializeFieldValueToJson(Field, Value);
        if (!FieldResult)
        {
            return std::unexpected(FieldResult.error());
        }
        Out[Field.Name] = std::move(*FieldResult);
    }

    return Out;
}

Result DeserializeObjectFromJson(const TypeId& Type,
                                 void* Value,
                                 const Json& Source,
                                 const bool TolerateFailures,
                                 AuthoredAssetImportDiagnostics* Diagnostics,
                                 const std::string_view Path)
{
    const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
    }
    if (!Source.is_object())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "JSON value is not an object"));
    }

    for (const TypeId& BaseType : Info->BaseTypes)
    {
        auto BaseResult = DeserializeObjectFromJson(BaseType, Value, Source, TolerateFailures, Diagnostics, Path);
        if (!BaseResult && !TolerateFailures)
        {
            return BaseResult;
        }
        if (!BaseResult && TolerateFailures)
        {
            AddImportDiagnostic(Diagnostics, Path, BaseResult.error());
        }
    }

    for (const FieldInfo& Field : Info->Fields)
    {
        const auto It = Source.find(Field.Name);
        if (It == Source.end())
        {
            continue;
        }

        const std::string FieldPath = Path.empty() ? Field.Name : std::string(Path) + "." + Field.Name;
        auto FieldResult = DeserializeFieldValueFromJson(Field, Value, *It, TolerateFailures, Diagnostics, FieldPath);
        if (!FieldResult && !TolerateFailures)
        {
            return FieldResult;
        }
    }

    return Ok();
}

TExpected<Json> SerializeFieldValueToJson(const FieldInfo& Field, const void* Instance)
{
    if (Field.ConstPointer)
    {
        const void* FieldPtr = Field.ConstPointer(Instance);
        if (FieldPtr)
        {
            return SerializeValueToJson(Field.FieldType, FieldPtr);
        }
    }

    if (Field.ViewGetter)
    {
        auto ViewResult = Field.ViewGetter(const_cast<void*>(Instance));
        if (ViewResult && ViewResult->UnsafeBorrowed())
        {
            return SerializeValueToJson(Field.FieldType, ViewResult->UnsafeBorrowed());
        }
    }

    if (Field.Getter)
    {
        auto ValueResult = Field.Getter(const_cast<void*>(Instance));
        if (ValueResult && ValueResult->UnsafeBorrowed())
        {
            return SerializeValueToJson(Field.FieldType, ValueResult->UnsafeBorrowed());
        }
    }

    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Field '" + Field.Name + "' is not readable"));
}

Result DeserializeFieldValueFromJson(const FieldInfo& Field,
                                     void* Instance,
                                     const Json& Source,
                                     const bool TolerateFailures,
                                     AuthoredAssetImportDiagnostics* Diagnostics,
                                     const std::string_view Path)
{
    if (Field.IsConst)
    {
        const auto ErrorValue = MakeError(EErrorCode::InvalidArgument, "Field '" + Field.Name + "' is const");
        if (TolerateFailures)
        {
            AddImportDiagnostic(Diagnostics, Path, ErrorValue);
            return Ok();
        }
        return std::unexpected(ErrorValue);
    }

    const TypeInfo* FieldTypeInfo = TypeRegistry::Instance().Find(Field.FieldType);
    if (!FieldTypeInfo || !FieldTypeInfo->RuntimeOps || !FieldTypeInfo->RuntimeOps->DefaultConstruct ||
        !FieldTypeInfo->RuntimeOps->Destroy)
    {
        auto DirectResult = DeserializeFieldValueInPlace(Field, Instance, Source);
        if (TolerateFailures)
        {
            if (!DirectResult)
            {
                AddImportDiagnostic(Diagnostics, Path, DirectResult.error());
                return Ok();
            }
        }
        return DirectResult;
    }

    void* Storage = ::operator new(FieldTypeInfo->Size, std::align_val_t(FieldTypeInfo->Align));
    FieldTypeInfo->RuntimeOps->DefaultConstruct(Storage);
    const auto DestroyStorage = [&]() {
        FieldTypeInfo->RuntimeOps->Destroy(Storage);
        ::operator delete(Storage, std::align_val_t(FieldTypeInfo->Align));
    };

    auto DecodeResult = DeserializeValueFromJsonInto(Field.FieldType, Storage, Source, false);
    if (!DecodeResult)
    {
        DestroyStorage();
        if (TolerateFailures)
        {
            AddImportDiagnostic(Diagnostics, Path, DecodeResult.error());
            return Ok();
        }
        return DecodeResult;
    }

    auto AssignResult = AssignDecodedFieldValue(Field, Instance, Storage, *FieldTypeInfo);
    DestroyStorage();
    if (!AssignResult && TolerateFailures)
    {
        AddImportDiagnostic(Diagnostics, Path, AssignResult.error());
        return Ok();
    }
    return AssignResult;
}

TExpected<Json> SerializeValueToJson(const TypeId& Type, const void* Value)
{
    (void)TypeAutoRegistry::Instance().Ensure(Type);

    if (const JsonCodecEntry* Codec = JsonCodecRegistry::Instance().Find(Type); Codec && Codec->Encode)
    {
        return Codec->Encode(Value);
    }

    const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
    }

    if (Info->IsEnum)
    {
        return SerializeEnumToJson(*Info, Value);
    }

    if (!Info->Fields.empty() || !Info->BaseTypes.empty())
    {
        return SerializeObjectToJson(Type, Value);
    }

    auto OpaqueResult = SerializeOpaqueValueToJson(Type, Value);
    if (!OpaqueResult)
    {
        return std::unexpected(OpaqueResult.error());
    }
    return OpaqueResult;
}

Result DeserializeValueFromJsonInto(const TypeId& Type, void* Value, const Json& Source, const bool TolerateFailures)
{
    (void)TypeAutoRegistry::Instance().Ensure(Type);

    if (const JsonCodecEntry* Codec = JsonCodecRegistry::Instance().Find(Type); Codec && Codec->DecodeInto)
    {
        auto ResultValue = Codec->DecodeInto(Value, Source);
        if (!ResultValue && TolerateFailures)
        {
            return Ok();
        }
        return ResultValue;
    }

    const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return TolerateFailures
                   ? Ok()
                   : std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
    }

    if (Info->IsEnum)
    {
        auto ResultValue = DeserializeEnumFromJsonInto(*Info, Value, Source);
        if (!ResultValue && TolerateFailures)
        {
            return Ok();
        }
        return ResultValue;
    }

    if (!Info->Fields.empty() || !Info->BaseTypes.empty())
    {
        auto ResultValue = DeserializeObjectFromJson(Type, Value, Source, TolerateFailures, nullptr, {});
        if (!ResultValue && TolerateFailures)
        {
            return Ok();
        }
        return ResultValue;
    }

    if (Source.is_object() && Source.contains(std::string(kOpaqueJsonBytesField)))
    {
        auto ResultValue = DeserializeOpaqueValueFromJsonInto(Type, Value, Source);
        if (!ResultValue && TolerateFailures)
        {
            return Ok();
        }
        return ResultValue;
    }

    return TolerateFailures
               ? Ok()
               : std::unexpected(MakeError(EErrorCode::NotFound, "No JSON deserializer for reflected type '" + Info->Name + "'"));
}

[[nodiscard]] const Json& ResolveAssetRoot(const Json& Root)
{
    if (Root.is_object())
    {
        if (const auto It = Root.find("Asset"); It != Root.end() && It->is_object())
        {
            return *It;
        }
    }
    return Root;
}

} // namespace

TExpected<Json> SerializeSerializedValueToJsonValue(const Conduit::SerializedValue& Value)
{
    return SerializeValueToJson(StaticTypeId<Conduit::SerializedValue>(), &Value);
}

TExpected<std::string> SerializeAuthoredAssetToJson(const TypeId& Type, const void* Asset)
{
    std::ostringstream Output{};
    auto SaveResult = SaveAuthoredAssetToJsonStream(Type, Asset, Output);
    if (!SaveResult)
    {
        return std::unexpected(SaveResult.error());
    }
    return Output.str();
}

Result SaveAuthoredAssetToJsonStream(const TypeId& Type, const void* Asset, std::ostream& Output)
{
    if (!Asset)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null authored asset source"));
    }

    (void)TypeAutoRegistry::Instance().Ensure(Type);

    const void* AssetPtr = TypeRegistry::Instance().Cast(Type, StaticTypeId<IAsset>(), Asset);
    if (!AssetPtr)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Reflected type is not an authored asset"));
    }

    const auto* AuthoredAsset = static_cast<const IAsset*>(AssetPtr);
    return AuthoredAsset->Save(Output);
}

Result DeserializeAuthoredAssetFromJson(const TypeId& Type, std::string_view Text, void* OutAsset)
{
    return DeserializeAuthoredAssetFromJson(Type, Text, OutAsset, nullptr);
}

Result DeserializeAuthoredAssetFromJson(const TypeId& Type,
                                        std::string_view Text,
                                        void* OutAsset,
                                        AuthoredAssetImportDiagnostics* OutDiagnostics)
{
    if (!OutAsset)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null authored asset destination"));
    }

    try
    {
        const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
        if (!Info || !Info->RuntimeOps)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Authored asset type is not registered"));
        }
        if (!Info->RuntimeOps->DefaultConstruct)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Authored asset type requires a default constructor for JSON import"));
        }

        Json Root = Json::parse(Text.begin(), Text.end());
        const Json& AssetRoot = ResolveAssetRoot(Root);
        if (!AssetRoot.is_object())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Authored asset JSON root must be an object"));
        }

        void* Storage = ::operator new(Info->Size, std::align_val_t(Info->Align));
        Info->RuntimeOps->DefaultConstruct(Storage);
        const auto DestroyStorage = [&]() {
            if (Info->RuntimeOps->Destroy)
            {
                Info->RuntimeOps->Destroy(Storage);
            }
            ::operator delete(Storage, std::align_val_t(Info->Align));
        };

        auto DecodeResult = DeserializeObjectFromJson(Type, Storage, AssetRoot, true, OutDiagnostics, {});
        if (!DecodeResult)
        {
            DestroyStorage();
            return DecodeResult;
        }

        if (Info->RuntimeOps->MoveAssign)
        {
            Info->RuntimeOps->MoveAssign(Storage, OutAsset);
        }
        else if (Info->RuntimeOps->CopyAssign)
        {
            Info->RuntimeOps->CopyAssign(Storage, OutAsset);
        }
        else if (Info->RuntimeOps->Destroy && (Info->RuntimeOps->MoveConstruct || Info->RuntimeOps->CopyConstruct))
        {
            Info->RuntimeOps->Destroy(OutAsset);
            if (Info->RuntimeOps->MoveConstruct)
            {
                Info->RuntimeOps->MoveConstruct(Storage, OutAsset);
            }
            else
            {
                Info->RuntimeOps->CopyConstruct(Storage, OutAsset);
            }
        }
        else
        {
            DestroyStorage();
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "Authored asset type cannot replace existing instance during JSON import"));
        }

        DestroyStorage();
        return Ok();
    }
    catch (const nlohmann::json::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
    catch (...)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Unknown exception while deserializing authored asset JSON"));
    }
}

} // namespace SnAPI::GameFramework
