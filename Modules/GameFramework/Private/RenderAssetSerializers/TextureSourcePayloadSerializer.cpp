#include "RenderAssetSerializers/TextureSourcePayloadSerializer.h"

#include "AssetPipelineIds.h"
#include "RenderAssets/TextureAsset.h"
#include "PayloadSerializerHelpers.h"

namespace SnAPI::GameFramework
{
namespace
{

class TextureSourcePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override { return PayloadTextureSource(); }
    const char* GetTypeName() const override { return kPayloadTextureSourceName; }
    uint32_t GetSchemaVersion() const override { return 1u; }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        Detail::SerializePayloadObject<TextureAsset, SerializeTextureSourcePayload>(Object, OutBytes);
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        return Detail::DeserializePayloadObject<TextureAsset, DeserializeTextureSourcePayload>(Object, Bytes, Size);
    }
};

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateTextureSourcePayloadSerializer()
{
    return std::make_unique<TextureSourcePayloadSerializer>();
}

} // namespace SnAPI::GameFramework
