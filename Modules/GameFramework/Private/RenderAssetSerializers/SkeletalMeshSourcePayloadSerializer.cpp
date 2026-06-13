#include "RenderAssetSerializers/SkeletalMeshSourcePayloadSerializer.h"

#include "AssetPipelineIds.h"
#include "RenderAssets/SkeletalMeshAsset.h"
#include "PayloadSerializerHelpers.h"

namespace SnAPI::GameFramework
{
namespace
{

class SkeletalMeshSourcePayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override { return PayloadSkeletalMeshSource(); }
    const char* GetTypeName() const override { return kPayloadSkeletalMeshSourceName; }
    uint32_t GetSchemaVersion() const override { return 1u; }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        Detail::SerializePayloadObject<SkeletalMeshAsset, SerializeSkeletalMeshSourcePayload>(Object, OutBytes);
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        return Detail::DeserializePayloadObject<SkeletalMeshAsset, DeserializeSkeletalMeshSourcePayload>(Object, Bytes, Size);
    }
};

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletalMeshSourcePayloadSerializer()
{
    return std::make_unique<SkeletalMeshSourcePayloadSerializer>();
}

} // namespace SnAPI::GameFramework
