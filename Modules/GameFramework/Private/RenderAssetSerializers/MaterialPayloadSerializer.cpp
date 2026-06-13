#include "RenderAssetSerializers/MaterialPayloadSerializer.h"

#include "AssetPipelineIds.h"
#include "RenderAssets/MaterialAsset.h"
#include "PayloadSerializerHelpers.h"

namespace SnAPI::GameFramework
{
namespace
{

class MaterialPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override { return PayloadMaterial(); }
    const char* GetTypeName() const override { return kPayloadMaterialName; }
    uint32_t GetSchemaVersion() const override { return 2u; }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        Detail::SerializePayloadObject<MaterialAsset, SerializeMaterialPayload>(Object, OutBytes);
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        return Detail::DeserializePayloadObject<MaterialAsset, DeserializeMaterialPayload>(Object, Bytes, Size);
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

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateMaterialPayloadSerializer()
{
    return std::make_unique<MaterialPayloadSerializer>();
}

} // namespace SnAPI::GameFramework
