#include "RenderAssetSerializers/StaticMeshPayloadSerializer.h"

#include "AssetPipelineIds.h"
#include "RenderAssets/StaticMeshPayload.h"
#include "PayloadSerializerHelpers.h"

namespace SnAPI::GameFramework
{
namespace
{

class StaticMeshPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override { return PayloadStaticMesh(); }
    const char* GetTypeName() const override { return kPayloadStaticMeshName; }
    uint32_t GetSchemaVersion() const override { return 2u; }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        Detail::SerializePayloadObject<StaticMeshPayload, SerializeStaticMeshPayload>(Object, OutBytes);
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        return Detail::DeserializePayloadObject<StaticMeshPayload, DeserializeStaticMeshPayload>(Object, Bytes, Size);
    }
};

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateStaticMeshPayloadSerializer()
{
    return std::make_unique<StaticMeshPayloadSerializer>();
}

} // namespace SnAPI::GameFramework
