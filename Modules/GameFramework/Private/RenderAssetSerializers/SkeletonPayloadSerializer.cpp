#include "RenderAssetSerializers/SkeletonPayloadSerializer.h"

#include "AssetPipelineIds.h"
#include "RenderAssets/SkeletonPayload.h"
#include "PayloadSerializerHelpers.h"

namespace SnAPI::GameFramework
{
namespace
{

class SkeletonPayloadSerializer final : public ::SnAPI::AssetPipeline::IPayloadSerializer
{
public:
    ::SnAPI::AssetPipeline::TypeId GetTypeId() const override { return PayloadSkeleton(); }
    const char* GetTypeName() const override { return kPayloadSkeletonName; }
    uint32_t GetSchemaVersion() const override { return 1u; }

    void SerializeToBytes(const void* Object, std::vector<uint8_t>& OutBytes) const override
    {
        Detail::SerializePayloadObject<SkeletonPayload, SerializeSkeletonPayload>(Object, OutBytes);
    }

    bool DeserializeFromBytes(void* Object, const uint8_t* Bytes, std::size_t Size) const override
    {
        return Detail::DeserializePayloadObject<SkeletonPayload, DeserializeSkeletonPayload>(Object, Bytes, Size);
    }
};

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IPayloadSerializer> CreateSkeletonPayloadSerializer()
{
    return std::make_unique<SkeletonPayloadSerializer>();
}

} // namespace SnAPI::GameFramework
