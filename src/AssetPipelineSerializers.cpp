#include "AssetPipelineSerializers.h"

#include "AssetPipelineIds.h"
#include "Conduit/Asset.h"
#include "NodeAsset.h"
#include "RenderAssetPayloads.h"
#include "RenderAssetSourcePayloads.h"
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

/**
 * @brief AssetPipeline serializer for authored NodeAsset source payloads.
 */
class NodeSourcePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadNodeSource();
    }

    const char* GetTypeName() const override
    {
        return kPayloadNodeSourceName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return NodeAsset::kSchemaVersion;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const NodeAsset*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeNodeAsset(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<NodeAsset*>(Object);
        if (!Payload)
        {
            return false;
        }
        auto Result = DeserializeNodeAsset(Bytes, Size);
        if (!Result)
        {
            return false;
        }
        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for authored LevelAsset source payloads.
 */
class LevelSourcePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadLevelSource();
    }

    const char* GetTypeName() const override
    {
        return kPayloadLevelSourceName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return LevelAsset::kSchemaVersion;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const LevelAsset*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeLevelAsset(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<LevelAsset*>(Object);
        if (!Payload)
        {
            return false;
        }
        auto Result = DeserializeLevelAsset(Bytes, Size);
        if (!Result)
        {
            return false;
        }
        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for authored WorldAsset source payloads.
 */
class WorldSourcePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadWorldSource();
    }

    const char* GetTypeName() const override
    {
        return kPayloadWorldSourceName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return WorldAsset::kSchemaVersion;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const WorldAsset*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeWorldAsset(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<WorldAsset*>(Object);
        if (!Payload)
        {
            return false;
        }
        auto Result = DeserializeWorldAsset(Bytes, Size);
        if (!Result)
        {
            return false;
        }
        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for authored Conduit graph assets.
 */
class ConduitGraphPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadConduitGraph();
    }

    const char* GetTypeName() const override
    {
        return kPayloadConduitGraphName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return Conduit::GraphAsset::kSchemaVersion;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const Conduit::GraphAsset*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = Conduit::SerializeGraphAsset(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<Conduit::GraphAsset*>(Object);
        if (!Payload)
        {
            return false;
        }
        auto Result = Conduit::DeserializeGraphAsset(Bytes, Size);
        if (!Result)
        {
            return false;
        }
        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for authored Conduit class assets.
 */
class ConduitClassPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadConduitClass();
    }

    const char* GetTypeName() const override
    {
        return kPayloadConduitClassName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return Conduit::ClassAsset::kSchemaVersion;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const Conduit::ClassAsset*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = Conduit::SerializeClassAsset(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<Conduit::ClassAsset*>(Object);
        if (!Payload)
        {
            return false;
        }
        auto Result = Conduit::DeserializeClassAsset(Bytes, Size);
        if (!Result)
        {
            return false;
        }
        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for StaticMeshPayload.
 */
class StaticMeshPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadStaticMesh();
    }

    const char* GetTypeName() const override
    {
        return kPayloadStaticMeshName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return 2u;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const StaticMeshPayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeStaticMeshPayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<StaticMeshPayload*>(Object);
        if (!Payload)
        {
            return false;
        }

        auto Result = DeserializeStaticMeshPayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }

        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for SkeletalMeshPayload.
 */
class SkeletalMeshPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadSkeletalMesh();
    }

    const char* GetTypeName() const override
    {
        return kPayloadSkeletalMeshName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return 2u;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const SkeletalMeshPayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeSkeletalMeshPayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<SkeletalMeshPayload*>(Object);
        if (!Payload)
        {
            return false;
        }

        auto Result = DeserializeSkeletalMeshPayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }

        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for MaterialPayload.
 */
class MaterialPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadMaterial();
    }

    const char* GetTypeName() const override
    {
        return kPayloadMaterialName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return 2u;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const MaterialPayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeMaterialPayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<MaterialPayload*>(Object);
        if (!Payload)
        {
            return false;
        }

        auto Result = DeserializeMaterialPayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }

        *Payload = std::move(Result.value());
        return true;
    }

    bool MigrateBytes(const uint32_t FromVersion, const uint32_t ToVersion, std::vector<uint8_t>& InOutBytes) const override
    {
        if (FromVersion == ToVersion)
        {
            return true;
        }

        if (FromVersion == 1u && ToVersion == 2u)
        {
            auto DeserializeResult = DeserializeMaterialPayload(InOutBytes.data(), InOutBytes.size());
            if (!DeserializeResult)
            {
                return false;
            }

            std::vector<uint8_t> MigratedBytes{};
            auto SerializeResult = SerializeMaterialPayload(DeserializeResult.value(), MigratedBytes);
            if (!SerializeResult)
            {
                return false;
            }

            InOutBytes = std::move(MigratedBytes);
            return true;
        }

        return false;
    }
};

/**
 * @brief AssetPipeline serializer for MaterialInstancePayload.
 */
class MaterialInstancePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadMaterialInstance();
    }

    const char* GetTypeName() const override
    {
        return kPayloadMaterialInstanceName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return 1u;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const MaterialInstancePayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeMaterialInstancePayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<MaterialInstancePayload*>(Object);
        if (!Payload)
        {
            return false;
        }

        auto Result = DeserializeMaterialInstancePayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }

        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for SkeletonPayload.
 */
class SkeletonPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadSkeleton();
    }

    const char* GetTypeName() const override
    {
        return kPayloadSkeletonName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return 1u;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const SkeletonPayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeSkeletonPayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<SkeletonPayload*>(Object);
        if (!Payload)
        {
            return false;
        }

        auto Result = DeserializeSkeletonPayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }

        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for AnimationPayload.
 */
class AnimationPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadAnimation();
    }

    const char* GetTypeName() const override
    {
        return kPayloadAnimationName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return 1u;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const AnimationPayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeAnimationPayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<AnimationPayload*>(Object);
        if (!Payload)
        {
            return false;
        }

        auto Result = DeserializeAnimationPayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }

        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for StaticMeshSourcePayload.
 */
class StaticMeshSourcePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadStaticMeshSource();
    }

    const char* GetTypeName() const override
    {
        return kPayloadStaticMeshSourceName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return 1u;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const StaticMeshSourcePayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeStaticMeshSourcePayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<StaticMeshSourcePayload*>(Object);
        if (!Payload)
        {
            return false;
        }

        auto Result = DeserializeStaticMeshSourcePayload(Bytes, Size);
        if (!Result)
        {
            return false;
        }

        *Payload = std::move(Result.value());
        return true;
    }
};

/**
 * @brief AssetPipeline serializer for SkeletalMeshSourcePayload.
 */
class SkeletalMeshSourcePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override
    {
        return PayloadSkeletalMeshSource();
    }

    const char* GetTypeName() const override
    {
        return kPayloadSkeletalMeshSourceName;
    }

    uint32_t GetSchemaVersion() const override
    {
        return 1u;
    }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        const auto* Payload = static_cast<const SkeletalMeshSourcePayload*>(Object);
        if (!Payload)
        {
            OutBytes.clear();
            return;
        }
        auto Result = SerializeSkeletalMeshSourcePayload(*Payload, OutBytes);
        if (!Result)
        {
            OutBytes.clear();
        }
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        auto* Payload = static_cast<SkeletalMeshSourcePayload*>(Object);
        if (!Payload)
        {
            return false;
        }

        auto Result = DeserializeSkeletalMeshSourcePayload(Bytes, Size);
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

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateNodeSourcePayloadSerializer()
{
    return std::make_unique<NodeSourcePayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateLevelSourcePayloadSerializer()
{
    return std::make_unique<LevelSourcePayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateWorldSourcePayloadSerializer()
{
    return std::make_unique<WorldSourcePayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateConduitGraphPayloadSerializer()
{
    return std::make_unique<ConduitGraphPayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateConduitClassPayloadSerializer()
{
    return std::make_unique<ConduitClassPayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateStaticMeshPayloadSerializer()
{
    return std::make_unique<StaticMeshPayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletalMeshPayloadSerializer()
{
    return std::make_unique<SkeletalMeshPayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateMaterialPayloadSerializer()
{
    return std::make_unique<MaterialPayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateMaterialInstancePayloadSerializer()
{
    return std::make_unique<MaterialInstancePayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletonPayloadSerializer()
{
    return std::make_unique<SkeletonPayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateAnimationPayloadSerializer()
{
    return std::make_unique<AnimationPayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateStaticMeshSourcePayloadSerializer()
{
    return std::make_unique<StaticMeshSourcePayloadSerializer>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletalMeshSourcePayloadSerializer()
{
    return std::make_unique<SkeletalMeshSourcePayloadSerializer>();
}

} // namespace SnAPI::GameFramework
