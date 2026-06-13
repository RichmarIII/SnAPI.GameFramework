#include "RenderAssetSerializers/MaterialInstancePayloadSerializer.h"

#include "AssetPipelineIds.h"
#include "RenderAssets/MaterialInstanceAsset.h"
#include "PayloadSerializerHelpers.h"

namespace SnAPI::GameFramework
{
namespace
{

class MaterialInstancePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override { return PayloadMaterialInstance(); }
    const char* GetTypeName() const override { return kPayloadMaterialInstanceName; }
    uint32_t GetSchemaVersion() const override { return 1u; }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        Detail::SerializePayloadObject<MaterialInstanceAsset, SerializeMaterialInstancePayload>(Object, OutBytes);
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        return Detail::DeserializePayloadObject<MaterialInstanceAsset, DeserializeMaterialInstancePayload>(Object, Bytes, Size);
    }
};

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateMaterialInstancePayloadSerializer()
{
    return std::make_unique<MaterialInstancePayloadSerializer>();
}

} // namespace SnAPI::GameFramework
