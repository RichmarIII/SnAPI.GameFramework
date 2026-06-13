#include "RenderAssetSerializers/StaticMeshSourcePayloadSerializer.h"

#include "AssetPipelineIds.h"
#include "RenderAssets/StaticMeshAsset.h"
#include "PayloadSerializerHelpers.h"

namespace SnAPI::GameFramework
{
namespace
{

class StaticMeshSourcePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override { return PayloadStaticMeshSource(); }
    const char* GetTypeName() const override { return kPayloadStaticMeshSourceName; }
    uint32_t GetSchemaVersion() const override { return 1u; }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        Detail::SerializePayloadObject<StaticMeshAsset, SerializeStaticMeshSourcePayload>(Object, OutBytes);
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        return Detail::DeserializePayloadObject<StaticMeshAsset, DeserializeStaticMeshSourcePayload>(Object, Bytes, Size);
    }
};

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateStaticMeshSourcePayloadSerializer()
{
    return std::make_unique<StaticMeshSourcePayloadSerializer>();
}

} // namespace SnAPI::GameFramework
