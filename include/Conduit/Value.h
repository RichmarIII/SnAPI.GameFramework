#pragma once

#include <utility>
#include <vector>

#include "Serialization.h"
#include "TypeName.h"

namespace SnAPI::GameFramework::Conduit
{

/**
 * @ingroup SnAPI_GameFramework_Conduit
 * @brief Serialized logical value payload used by authored assets and compiled constants.
 *
 * `SerializedValue` is the durable constant/default-value representation for Conduit.
 * It intentionally stores logical serialized bytes rather than raw object memory so it can
 * safely represent non-trivial reflected types such as `std::string` and reflected structs.
 */
struct SerializedValue
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Conduit::SerializedValue";
    TypeId Type{}; /**< @brief Reflected type id of the stored value. */
    std::vector<uint8_t> Bytes{}; /**< @brief Serialized byte payload for the value. */

    bool operator==(const SerializedValue&) const = default;

    /**
     * @brief Serialize one typed C++ value into a `SerializedValue`.
     * @tparam T Concrete value type.
     * @param Value Source value.
     * @param Context Borrowed serialization context.
     * @return Serialized value or an error.
     */
    template<typename T>
    static TExpected<SerializedValue> FromValue(const T& Value, const TSerializationContext& Context = {})
    {
        SerializedValue Result{};
        Result.Type = StaticTypeId<T>();
        auto SerializeResult = SerializeReflectedValue(Result.Type, &Value, Result.Bytes, Context);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error());
        }
        return Result;
    }

    /**
     * @brief Serialize a reflected `Variant` payload into durable bytes.
     * @param Value Source variant.
     * @param Context Borrowed serialization context.
     * @return Serialized value or an error.
     */
    static TExpected<SerializedValue> FromVariant(const Variant& Value, const TSerializationContext& Context = {})
    {
        if (Value.IsVoid())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit serialized value cannot be void"));
        }

        const void* Payload = Value.UnsafeBorrowed();
        if (!Payload)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Conduit serialized value source variant has no payload"));
        }

        SerializedValue Result{};
        Result.Type = Value.Type();
        auto SerializeResult = SerializeReflectedValue(Result.Type, Payload, Result.Bytes, Context);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error());
        }
        return Result;
    }

    /**
     * @brief Materialize the serialized value into uninitialized storage.
     * @param Storage Destination storage.
     * @param Context Borrowed serialization context.
     * @return Success or error.
     */
    TExpected<void> ConstructInto(void* Storage, const TSerializationContext& Context = {}) const
    {
        if (Type == TypeId{})
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit serialized value type is missing"));
        }
        return ConstructReflectedValue(Type, Storage, Bytes.data(), Bytes.size(), Context);
    }
};

} // namespace SnAPI::GameFramework::Conduit
