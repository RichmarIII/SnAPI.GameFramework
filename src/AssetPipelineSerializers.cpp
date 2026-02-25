#include "AssetPipelineSerializers.h"

#include "AssetPipelineIds.h"
#include "Serialization.h"

namespace SnAPI::GameFramework
{
namespace
{
/**
 * @brief AssetPipeline serializer for NodePayload.
 * @remarks Wraps reflection-based serialization into AssetPipeline payloads.
 */
class NodePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    /**
     * @brief Get the payload type id.
     * @return Payload type id for Level.
     */
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadNode();
    }

    /**
     * @brief Get the payload type name.
     * @return Payload type name string.
     */
    const char* GetTypeName() const override
    {
        return kPayloadNodeName;
    }

    /**
     * @brief Get the payload schema version.
     * @return Schema version for Level payloads.
     */
    uint32_t GetSchemaVersion() const override
    {
        return NodeSerializer::kSchemaVersion;
    }

    /**
     * @brief Serialize a NodePayload into bytes.
     * @param Object Pointer to NodePayload.
     * @param OutBytes Output byte buffer.
     * @remarks Clears OutBytes on failure.
     */
    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const NodePayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeNodePayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    /**
     * @brief Deserialize a NodePayload from bytes.
     * @param Object Pointer to destination payload.
     * @param Bytes Byte buffer.
     * @param Size Byte count.
     * @return True on success.
     */
    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<NodePayload*>(Object);
        if (!Payload)
        {
            return false;
        }
        auto Result = DeserializeNodePayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }
        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for LevelPayload.
 */
class LevelPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    /** @brief Get the payload type id. */
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadLevel();
    }

    /** @brief Get the payload type name. */
    const char* GetTypeName() const override
    {
        return kPayloadLevelName;
    }

    /** @brief Get the payload schema version. */
    uint32_t GetSchemaVersion() const override
    {
        return LevelSerializer::kSchemaVersion;
    }

    /**
     * @brief Serialize a LevelPayload into bytes.
     * @param Object Pointer to LevelPayload.
     * @param OutBytes Output byte buffer.
     */
    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const LevelPayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeLevelPayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    /**
     * @brief Deserialize a LevelPayload from bytes.
     * @param Object Pointer to destination payload.
     * @param Bytes Byte buffer.
     * @param Size Byte count.
     * @return True on success.
     */
    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<LevelPayload*>(Object);
        if (!Payload)
        {
            return false;
        }
        auto Result = DeserializeLevelPayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }
        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for WorldPayload.
 */
class WorldPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    /** @brief Get the payload type id. */
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadWorld();
    }

    /** @brief Get the payload type name. */
    const char* GetTypeName() const override
    {
        return kPayloadWorldName;
    }

    /** @brief Get the payload schema version. */
    uint32_t GetSchemaVersion() const override
    {
        return WorldSerializer::kSchemaVersion;
    }

    /**
     * @brief Serialize a WorldPayload into bytes.
     * @param Object Pointer to WorldPayload.
     * @param OutBytes Output byte buffer.
     */
    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const WorldPayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeWorldPayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    /**
     * @brief Deserialize a WorldPayload from bytes.
     * @param Object Pointer to destination payload.
     * @param Bytes Byte buffer.
     * @param Size Byte count.
     * @return True on success.
     */
    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<WorldPayload*>(Object);
        if (!Payload)
        {
            return false;
        }
        auto Result = DeserializeWorldPayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }
        *Payload = std::move(Result.value());
        return true;
    }
};

} // namespace

/**
 * @brief Create the Node payload serializer.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateNodePayloadSerializer()
{
    return std::make_unique<NodePayloadSerializer>();
}

/**
 * @brief Create the Level payload serializer.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateLevelPayloadSerializer()
{
    return std::make_unique<LevelPayloadSerializer>();
}

/**
 * @brief Create the World payload serializer.
 * @return Serializer instance.
 */
std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateWorldPayloadSerializer()
{
    return std::make_unique<WorldPayloadSerializer>();
}

} // namespace SnAPI::GameFramework
