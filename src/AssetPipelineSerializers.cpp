#include "AssetPipelineSerializers.h"

#include "AssetPipelineIds.h"
#include "Serialization.h"

namespace SnAPI::GameFramework
{
namespace
{
class NodeGraphPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadNodeGraph();
    }

    const char* GetTypeName() const override
    {
        return kPayloadNodeGraphName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return NodeGraphSerializer::kSchemaVersion;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const NodeGraphPayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeNodeGraphPayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<NodeGraphPayload*>(Object);
        if (!Payload)
        {
            return false;
        }
        auto Result = DeserializeNodeGraphPayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }
        *Payload = std::move(Result.value());
        return true;
    }
};

class LevelPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadLevel();
    }

    const char* GetTypeName() const override
    {
        return kPayloadLevelName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return LevelSerializer::kSchemaVersion;
    }

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

class WorldPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadWorld();
    }

    const char* GetTypeName() const override
    {
        return kPayloadWorldName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return WorldSerializer::kSchemaVersion;
    }

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

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateNodeGraphPayloadSerializer()
{
    return std::make_unique<NodeGraphPayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateLevelPayloadSerializer()
{
    return std::make_unique<LevelPayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateWorldPayloadSerializer()
{
    return std::make_unique<WorldPayloadSerializer>();
}

} // namespace SnAPI::GameFramework
