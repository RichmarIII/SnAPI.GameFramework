#include "RenderAssetSerializers/SkeletalMeshPayloadSerializer.h"

#include "AssetPipelineIds.h"
#include "RenderAssets/SkeletalMeshPayload.h"
#include "PayloadSerializerHelpers.h"

namespace SnAPI::GameFramework
{
namespace
{

class SkeletalMeshPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override { return PayloadSkeletalMesh(); }
    const char* GetTypeName() const override { return kPayloadSkeletalMeshName; }
    uint32_t GetSchemaVersion() const override { return 2u; }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        Detail::SerializePayloadObject<SkeletalMeshPayload, SerializeSkeletalMeshPayload>(Object, OutBytes);
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        return Detail::DeserializePayloadObject<SkeletalMeshPayload, DeserializeSkeletalMeshPayload>(Object, Bytes, Size);
    }
};

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletalMeshPayloadSerializer()
{
    return std::make_unique<SkeletalMeshPayloadSerializer>();
}

} // namespace SnAPI::GameFramework
