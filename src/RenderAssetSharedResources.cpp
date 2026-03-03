#include "RenderAssetSharedResources.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <IRenderObject.hpp>
#include <IVertexStreamSource.hpp>
#include <IGraphicsAPI.hpp>
#include <Material.hpp>
#include <MaterialContracts.hpp>
#include <MaterialInstance.hpp>
#include <MaterialRuntimeDescriptor.hpp>
#include <TMaterialFor.hpp>
#include <VulkanGraphicsAPI.hpp>

#include "AssetRef.h"
#include "RendererSystem.h"

namespace SnAPI::GameFramework
{
namespace
{
using SnAPI::Graphics::EVertexStream;
using SnAPI::Graphics::GetStreamInfo;
using SnAPI::Graphics::HasStream;
using SnAPI::Graphics::IVertexStreamSource;
using SnAPI::Graphics::Material;
using SnAPI::Graphics::MaterialInstance;
using SnAPI::Graphics::MaterialRuntimeDescriptor;
using SnAPI::Graphics::MaterialRuntimeParameterDesc;
using SnAPI::Graphics::MaterialRuntimeResourceDesc;
using SnAPI::Graphics::SharedMaterialInstancePtr;
using SnAPI::Graphics::SharedMaterialPtr;
using SnAPI::Graphics::VertexSourceMaterial;
using SnAPI::Graphics::VertexSourceSubMesh;
using RuntimeTextureRefAsset = ::SnAPI::GameFramework::RuntimeTextureAsset;
using RuntimeTextureHandle = ::SnAPI::AssetPipeline::AssetHandle<RuntimeTextureRefAsset>;

struct RuntimeStreamBuffer
{
    uint32_t ElementCount = 0;
    uint32_t StrideBytes = 0;
    std::vector<uint8_t> Bytes{};
};

struct RuntimeMeshSourceData
{
    std::string DebugName{};
    uint32_t VertexCount = 0;
    EVertexStream AvailableStreams = EVertexStream::None;
    std::unordered_map<EVertexStream, RuntimeStreamBuffer> Streams{};
    std::vector<uint32_t> Indices{};
    std::vector<VertexSourceSubMesh> SubMeshes{};
    std::vector<VertexSourceMaterial> Materials{};
    uint64_t SourceId = 0;
    uint64_t SourceRevision = 0;
};

struct RuntimeMaterialInstanceCacheEntry
{
    std::weak_ptr<MaterialInstance> Instance{};
    std::vector<RuntimeTextureHandle> TextureHandles{};
};

struct RenderAssetSharedResourcesState;
struct RuntimeMaterialFeatureFlags;

class RenderAssetSharedResourcesOps final
{
public:
    static constexpr uint64_t kFnv1aOffset = 1469598103934665603ull;
    static constexpr uint64_t kFnv1aPrime = 1099511628211ull;

    static uint64_t HashBytes64(const void* Data, size_t Size, uint64_t Seed = kFnv1aOffset);
    static uint64_t HashString64(std::string_view Value, uint64_t Seed = kFnv1aOffset);
    static std::string PointerKey(const void* Address);
    static std::string BuildRuntimeMeshCacheKey(const StaticMeshAssetRuntime& RuntimeMesh, std::string_view StableKey);
    static std::optional<EVertexStream> ToRendererStream(EMeshStreamSemantic Semantic);
    static const MeshStreamChunkRef* FindStreamRef(const StaticMeshAssetRuntime& RuntimeMesh, EMeshStreamSemantic Semantic);
    static bool LoadRawStreamBytes(const StaticMeshAssetRuntime& RuntimeMesh, const MeshStreamChunkRef& StreamRef, RuntimeStreamBuffer& Out);
    static bool DecodeIndexStream(const RuntimeStreamBuffer& Stream, std::vector<uint32_t>& OutIndices);
    static bool BuildRuntimeMeshSourceData(const StaticMeshAssetRuntime& RuntimeMesh, std::string_view CacheKey, RuntimeMeshSourceData& Out);

    static RenderAssetSharedResourcesState& SharedState();
    static std::string ToLowerCopy(std::string_view Text);
    static bool EqualsIgnoreCase(std::string_view Left, std::string_view Right);
    static bool TryParseFloatValue(std::string_view Text, float& OutValue);
    static const SnAPI::Graphics::ShaderMetaData::UserAttribute* FindParameterDefaultAttribute(
        const MaterialRuntimeParameterDesc& Parameter);
    static bool TryReadAttributeNumber(
        const SnAPI::Graphics::ShaderMetaData::UserAttribute& Attribute,
        size_t Index,
        float& OutValue);
    static bool TryResolveAttributeScalarDefault(const MaterialRuntimeParameterDesc& Parameter, float& OutValue);
    static bool TryResolveAttributeVectorDefault(
        const MaterialRuntimeParameterDesc& Parameter,
        size_t ComponentCount,
        std::array<float, 4>& OutValue);

    static SnAPI::Graphics::MaterialDomain DomainFromShadingModelName(std::string_view ShadingModel);

    static SharedMaterialInstancePtr GetOrCreateMaterialInstance(
        std::unordered_map<std::string, std::weak_ptr<MaterialInstance>>& Cache,
        const std::string& Key,
        const SharedMaterialPtr& BaseMaterial);
    static SharedMaterialInstancePtr GetOrCreateMaterialInstance(
        std::unordered_map<std::string, RuntimeMaterialInstanceCacheEntry>& Cache,
        const std::string& Key,
        const SharedMaterialPtr& BaseMaterial);

    static void CollectParameterLookupKeys(const MaterialRuntimeParameterDesc& Parameter, std::vector<std::string>& OutKeys);
    static bool TryResolveScalarValue(
        const MaterialRuntimeParameterDesc& Parameter,
        const std::unordered_map<std::string, float>& Scalars,
        float& OutValue);
    static bool TryResolveVectorValue(
        const MaterialRuntimeParameterDesc& Parameter,
        const std::unordered_map<std::string, std::array<float, 4>>& Vectors,
        std::array<float, 4>& OutValue);

    static bool HasTextureReference(const TAssetRef<RuntimeTextureRefAsset>& TextureRef);
    static void ApplyFeatureFlagsFromTextureSlot(RuntimeMaterialFeatureFlags& OutFeatures, const std::string& SlotLower);
    static RuntimeMaterialFeatureFlags BuildRuntimeMaterialFeatureFlags(
        const MaterialAssetRuntime& RuntimeMaterial,
        const MaterialInstanceAssetRuntime& RuntimeMaterialInstance);
    static void ApplyRuntimeMaterialFeatures(SnAPI::Graphics::GBufferMaterial& RuntimeMaterial, const RuntimeMaterialFeatureFlags& Features);

    static void CollectResourceLookupKeys(const MaterialRuntimeResourceDesc& Resource, std::vector<std::string>& OutKeys);
    static void ApplyDescriptorTexturesToMaterialInstance(
        MaterialInstance& TargetInstance,
        const MaterialRuntimeDescriptor& Descriptor,
        const MaterialInstanceAssetRuntime& RuntimeMaterialInstance,
        ::SnAPI::AssetPipeline::AssetManager& AssetManager,
        std::vector<RuntimeTextureHandle>& OutBoundTextureHandles);

    template<typename TValue>
    static bool WriteParameterValue(
        std::vector<uint8_t>& BufferBytes,
        const MaterialRuntimeParameterDesc& Parameter,
        const TValue& Value);

    static bool ApplyParameterToBuffer(
        const MaterialRuntimeParameterDesc& Parameter,
        const std::unordered_map<std::string, float>& Scalars,
        const std::unordered_map<std::string, std::array<float, 4>>& Vectors,
        std::vector<uint8_t>& BufferBytes);
    static void ApplyDescriptorBuffersToMaterialInstance(
        MaterialInstance& TargetInstance,
        const MaterialRuntimeDescriptor& Descriptor,
        const MaterialInstanceAssetRuntime& RuntimeMaterialInstance);

    static SharedMaterialPtr GetOrCreateRuntimeMaterial(
        const std::string& Key,
        const MaterialAssetRuntime& RuntimeMaterial,
        const RuntimeMaterialFeatureFlags& FeatureFlags);
    static std::string BuildRuntimeMaterialKey(
        const MaterialAssetRuntime& RuntimeMaterial,
        std::string_view MaterialAssetId,
        const RuntimeMaterialFeatureFlags& FeatureFlags);
    static SharedMaterialInstancePtr BuildRuntimeMaterialInstance(
        const TAssetRef<MaterialInstanceAssetRuntime>& MaterialRef,
        ::SnAPI::AssetPipeline::AssetManager* AssetManager,
        std::vector<RuntimeTextureHandle>* OutTextureHandles);
    static SharedMaterialInstancePtr GetOrCreateRuntimeMaterialInstance(
        const std::string& Key,
        const TAssetRef<MaterialInstanceAssetRuntime>& MaterialRef,
        ::SnAPI::AssetPipeline::AssetManager* AssetManager);

    static std::string BuildMaterialCacheKey(
        const TAssetRef<MaterialInstanceAssetRuntime>& MaterialRef,
        ::SnAPI::AssetPipeline::AssetManager* AssetManager);
};

uint64_t RenderAssetSharedResourcesOps::HashBytes64(const void* Data, const size_t Size, uint64_t Seed)
{
    uint64_t Hash = Seed;
    const auto* Bytes = static_cast<const uint8_t*>(Data);
    for (size_t Index = 0; Index < Size; ++Index)
    {
        Hash ^= static_cast<uint64_t>(Bytes[Index]);
        Hash *= kFnv1aPrime;
    }
    return Hash;
}

uint64_t RenderAssetSharedResourcesOps::HashString64(std::string_view Value, uint64_t Seed)
{
    return HashBytes64(Value.data(), Value.size(), Seed);
}

std::string RenderAssetSharedResourcesOps::PointerKey(const void* Address)
{
    std::ostringstream Stream{};
    Stream << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(Address);
    return Stream.str();
}

std::string RenderAssetSharedResourcesOps::BuildRuntimeMeshCacheKey(const StaticMeshAssetRuntime& RuntimeMesh, std::string_view StableKey)
{
    std::string Key{};
    if (!StableKey.empty())
    {
        Key.assign(StableKey.begin(), StableKey.end());
        return Key;
    }
    if (!RuntimeMesh.SourceAssetId.IsNull())
    {
        Key = "asset-id://" + RuntimeMesh.SourceAssetId.ToString();
        return Key;
    }
    if (!RuntimeMesh.Name.empty())
    {
        Key = "asset-name://" + RuntimeMesh.Name;
        return Key;
    }

    return "runtime-mesh@" + PointerKey(&RuntimeMesh);
}

std::optional<EVertexStream> RenderAssetSharedResourcesOps::ToRendererStream(const EMeshStreamSemantic Semantic)
{
    switch (Semantic)
    {
    case EMeshStreamSemantic::Position:
        return EVertexStream::Position;
    case EMeshStreamSemantic::Normal:
        return EVertexStream::Normal;
    case EMeshStreamSemantic::Tangent:
        return EVertexStream::Tangent;
    case EMeshStreamSemantic::UV0:
        return EVertexStream::UV0;
    case EMeshStreamSemantic::UV1:
        return EVertexStream::UV1;
    case EMeshStreamSemantic::Color:
        return EVertexStream::Color;
    case EMeshStreamSemantic::BoneIndices:
        return EVertexStream::BoneIndices;
    case EMeshStreamSemantic::BoneWeights:
        return EVertexStream::BoneWeights;
    case EMeshStreamSemantic::Index:
    default:
        return std::nullopt;
    }
}

const MeshStreamChunkRef* RenderAssetSharedResourcesOps::FindStreamRef(const StaticMeshAssetRuntime& RuntimeMesh, const EMeshStreamSemantic Semantic)
{
    const auto It = std::ranges::find_if(RuntimeMesh.Streams, [Semantic](const MeshStreamChunkRef& StreamRef) {
        return StreamRef.Semantic == Semantic;
    });
    return (It == RuntimeMesh.Streams.end()) ? nullptr : &(*It);
}

bool RenderAssetSharedResourcesOps::LoadRawStreamBytes(const StaticMeshAssetRuntime& RuntimeMesh, const MeshStreamChunkRef& StreamRef, RuntimeStreamBuffer& Out)
{
    if (!RuntimeMesh.LoadBulk || StreamRef.ElementCount == 0 || StreamRef.StrideBytes == 0)
    {
        return false;
    }

    auto BulkResult = RuntimeMesh.LoadBulk(StreamRef.BulkIndex);
    if (!BulkResult)
    {
        return false;
    }

    const size_t RequiredBytes =
        static_cast<size_t>(StreamRef.ElementCount) * static_cast<size_t>(StreamRef.StrideBytes);
    if (BulkResult->size() < RequiredBytes)
    {
        return false;
    }

    Out.ElementCount = StreamRef.ElementCount;
    Out.StrideBytes = StreamRef.StrideBytes;
    Out.Bytes = std::move(*BulkResult);
    return true;
}

bool RenderAssetSharedResourcesOps::DecodeIndexStream(const RuntimeStreamBuffer& Stream, std::vector<uint32_t>& OutIndices)
{
    OutIndices.clear();
    OutIndices.reserve(Stream.ElementCount);

    if (Stream.StrideBytes >= sizeof(uint32_t))
    {
        for (uint32_t Index = 0; Index < Stream.ElementCount; ++Index)
        {
            const size_t ByteOffset = static_cast<size_t>(Index) * static_cast<size_t>(Stream.StrideBytes);
            if (ByteOffset + sizeof(uint32_t) > Stream.Bytes.size())
            {
                return false;
            }
            uint32_t Value = 0;
            std::memcpy(&Value, Stream.Bytes.data() + ByteOffset, sizeof(uint32_t));
            OutIndices.push_back(Value);
        }
        return true;
    }

    if (Stream.StrideBytes >= sizeof(uint16_t))
    {
        for (uint32_t Index = 0; Index < Stream.ElementCount; ++Index)
        {
            const size_t ByteOffset = static_cast<size_t>(Index) * static_cast<size_t>(Stream.StrideBytes);
            if (ByteOffset + sizeof(uint16_t) > Stream.Bytes.size())
            {
                return false;
            }
            uint16_t Value = 0;
            std::memcpy(&Value, Stream.Bytes.data() + ByteOffset, sizeof(uint16_t));
            OutIndices.push_back(static_cast<uint32_t>(Value));
        }
        return true;
    }

    return false;
}

bool RenderAssetSharedResourcesOps::BuildRuntimeMeshSourceData(const StaticMeshAssetRuntime& RuntimeMesh, std::string_view CacheKey, RuntimeMeshSourceData& Out)
{
    Out = RuntimeMeshSourceData{};

    const MeshStreamChunkRef* PositionRef = FindStreamRef(RuntimeMesh, EMeshStreamSemantic::Position);
    const MeshStreamChunkRef* IndexRef = FindStreamRef(RuntimeMesh, EMeshStreamSemantic::Index);
    if (!PositionRef || !IndexRef)
    {
        return false;
    }

    RuntimeStreamBuffer PositionStream{};
    RuntimeStreamBuffer IndexStream{};
    if (!LoadRawStreamBytes(RuntimeMesh, *PositionRef, PositionStream) ||
        !LoadRawStreamBytes(RuntimeMesh, *IndexRef, IndexStream))
    {
        return false;
    }

    if (PositionStream.ElementCount == 0 || PositionStream.StrideBytes < GetStreamInfo(EVertexStream::Position).ByteSize)
    {
        return false;
    }

    Out.VertexCount = PositionStream.ElementCount;
    Out.Streams.emplace(EVertexStream::Position, std::move(PositionStream));
    Out.AvailableStreams |= EVertexStream::Position;

    if (!DecodeIndexStream(IndexStream, Out.Indices) || Out.Indices.empty())
    {
        return false;
    }

    constexpr std::array<EMeshStreamSemantic, 7> OptionalSemantics{
        EMeshStreamSemantic::Normal,
        EMeshStreamSemantic::Tangent,
        EMeshStreamSemantic::UV0,
        EMeshStreamSemantic::UV1,
        EMeshStreamSemantic::Color,
        EMeshStreamSemantic::BoneIndices,
        EMeshStreamSemantic::BoneWeights};

    for (const EMeshStreamSemantic Semantic : OptionalSemantics)
    {
        const MeshStreamChunkRef* StreamRef = FindStreamRef(RuntimeMesh, Semantic);
        if (!StreamRef)
        {
            continue;
        }

        const auto RendererStream = ToRendererStream(Semantic);
        if (!RendererStream.has_value())
        {
            continue;
        }

        RuntimeStreamBuffer Stream{};
        if (!LoadRawStreamBytes(RuntimeMesh, *StreamRef, Stream))
        {
            continue;
        }

        if (Stream.ElementCount != Out.VertexCount)
        {
            continue;
        }

        const auto Info = GetStreamInfo(*RendererStream);
        if (Info.ByteSize == 0 || Stream.StrideBytes < Info.ByteSize)
        {
            continue;
        }

        Out.AvailableStreams |= *RendererStream;
        Out.Streams.emplace(*RendererStream, std::move(Stream));
    }

    uint32_t MaterialSlotCount = 0;
    Out.SubMeshes.reserve(RuntimeMesh.SubMeshes.size());
    for (const StaticSubMeshPayload& RuntimeSubMesh : RuntimeMesh.SubMeshes)
    {
        VertexSourceSubMesh SubMesh{};
        SubMesh.IndexOffset = RuntimeSubMesh.IndexOffset;
        SubMesh.IndexCount = RuntimeSubMesh.IndexCount;
        SubMesh.MaterialSlot = RuntimeSubMesh.MaterialSlot;
        SubMesh.BoundingBoxMin = {
            RuntimeSubMesh.BoundsMin[0],
            RuntimeSubMesh.BoundsMin[1],
            RuntimeSubMesh.BoundsMin[2]};
        SubMesh.BoundingBoxMax = {
            RuntimeSubMesh.BoundsMax[0],
            RuntimeSubMesh.BoundsMax[1],
            RuntimeSubMesh.BoundsMax[2]};

        Out.SubMeshes.push_back(SubMesh);
        MaterialSlotCount = std::max(MaterialSlotCount, RuntimeSubMesh.MaterialSlot + 1);
    }

    if (Out.SubMeshes.empty())
    {
        VertexSourceSubMesh SubMesh{};
        SubMesh.IndexOffset = 0;
        SubMesh.IndexCount = static_cast<uint32_t>(Out.Indices.size());
        SubMesh.MaterialSlot = 0;
        Out.SubMeshes.push_back(SubMesh);
        MaterialSlotCount = 1;
    }

    MaterialSlotCount = std::max(MaterialSlotCount, static_cast<uint32_t>(RuntimeMesh.MaterialInstances.size()));
    MaterialSlotCount = std::max(MaterialSlotCount, 1u);
    Out.Materials.resize(MaterialSlotCount);
    for (uint32_t Slot = 0; Slot < MaterialSlotCount; ++Slot)
    {
        VertexSourceMaterial Material{};
        if (Slot < RuntimeMesh.MaterialInstances.size())
        {
            const auto& Ref = RuntimeMesh.MaterialInstances[Slot];
            const std::string Name = Ref.DisplayLabel();
            if (!Name.empty())
            {
                Material.Name = Name;
            }
        }

        if (Material.Name.empty())
        {
            Material.Name = "MaterialSlot_" + std::to_string(Slot);
        }

        Out.Materials[Slot] = std::move(Material);
    }

    Out.DebugName = RuntimeMesh.Name.empty() ? "RuntimeMesh" : RuntimeMesh.Name;
    if (!CacheKey.empty())
    {
        Out.DebugName += " [" + std::string(CacheKey) + "]";
    }

    Out.SourceId = HashString64(CacheKey.empty() ? Out.DebugName : CacheKey);
    uint64_t Revision = HashString64(Out.DebugName, Out.SourceId);
    Revision = HashBytes64(&Out.VertexCount, sizeof(Out.VertexCount), Revision);
    for (uint32_t Index = 0; Index < SnAPI::Graphics::kStreamCount; ++Index)
    {
        const EVertexStream Stream = SnAPI::Graphics::GetStreamByIndex(Index);
        if (!HasStream(Out.AvailableStreams, Stream))
        {
            continue;
        }

        const auto It = Out.Streams.find(Stream);
        if (It == Out.Streams.end())
        {
            continue;
        }

        const RuntimeStreamBuffer& Buffer = It->second;
        Revision = HashBytes64(&Buffer.ElementCount, sizeof(Buffer.ElementCount), Revision);
        Revision = HashBytes64(&Buffer.StrideBytes, sizeof(Buffer.StrideBytes), Revision);
        Revision = HashBytes64(Buffer.Bytes.data(), Buffer.Bytes.size(), Revision);
    }
    Revision = HashBytes64(Out.Indices.data(), Out.Indices.size() * sizeof(uint32_t), Revision);
    for (const VertexSourceSubMesh& SubMesh : Out.SubMeshes)
    {
        Revision = HashBytes64(&SubMesh, sizeof(VertexSourceSubMesh), Revision);
    }
    Out.SourceRevision = Revision == 0 ? 1 : Revision;
    return true;
}

class RuntimeMeshVertexStreamSource final : public IVertexStreamSource
{
public:
    explicit RuntimeMeshVertexStreamSource(RuntimeMeshSourceData Data)
        : m_data(std::move(Data))
    {
    }

    [[nodiscard]] uint64_t SourceID() const override
    {
        return m_data.SourceId;
    }

    [[nodiscard]] uint64_t SourceRevision() const override
    {
        return m_data.SourceRevision;
    }

    [[nodiscard]] std::string_view DebugName() const override
    {
        return m_data.DebugName;
    }

    [[nodiscard]] uint32_t VertexCount() const override
    {
        return m_data.VertexCount;
    }

    [[nodiscard]] uint32_t IndexCount() const override
    {
        return static_cast<uint32_t>(m_data.Indices.size());
    }

    [[nodiscard]] uint32_t SubMeshCount() const override
    {
        return static_cast<uint32_t>(m_data.SubMeshes.size());
    }

    [[nodiscard]] EVertexStream AvailableStreams() const override
    {
        return m_data.AvailableStreams;
    }

    [[nodiscard]] bool SubMesh(const uint32_t SubMeshIndex, VertexSourceSubMesh& OutSubMesh) const override
    {
        if (SubMeshIndex >= m_data.SubMeshes.size())
        {
            return false;
        }
        OutSubMesh = m_data.SubMeshes[SubMeshIndex];
        return true;
    }

    [[nodiscard]] bool Material(const uint32_t MaterialSlot, VertexSourceMaterial& OutMaterial) const override
    {
        if (MaterialSlot >= m_data.Materials.size())
        {
            return false;
        }
        OutMaterial = m_data.Materials[MaterialSlot];
        return true;
    }

    [[nodiscard]] bool BuildStreamData(const EVertexStream Stream, std::vector<uint8_t>& OutData) const override
    {
        const auto It = m_data.Streams.find(Stream);
        if (It == m_data.Streams.end())
        {
            return false;
        }

        const RuntimeStreamBuffer& Source = It->second;
        const auto Info = GetStreamInfo(Stream);
        if (Info.ByteSize == 0 || Source.ElementCount != m_data.VertexCount || Source.StrideBytes < Info.ByteSize)
        {
            return false;
        }

        const size_t PackedSize = static_cast<size_t>(m_data.VertexCount) * static_cast<size_t>(Info.ByteSize);
        OutData.resize(PackedSize);

        if (Source.StrideBytes == Info.ByteSize)
        {
            std::memcpy(OutData.data(), Source.Bytes.data(), PackedSize);
            return true;
        }

        for (uint32_t VertexIndex = 0; VertexIndex < m_data.VertexCount; ++VertexIndex)
        {
            const size_t SourceOffset = static_cast<size_t>(VertexIndex) * static_cast<size_t>(Source.StrideBytes);
            const size_t DestinationOffset = static_cast<size_t>(VertexIndex) * static_cast<size_t>(Info.ByteSize);
            if (SourceOffset + Info.ByteSize > Source.Bytes.size())
            {
                return false;
            }
            std::memcpy(OutData.data() + DestinationOffset, Source.Bytes.data() + SourceOffset, Info.ByteSize);
        }

        return true;
    }

    [[nodiscard]] bool BuildIndexData(std::vector<uint32_t>& OutIndices) const override
    {
        OutIndices = m_data.Indices;
        return !OutIndices.empty();
    }

private:
    RuntimeMeshSourceData m_data{};
};

struct RenderAssetSharedResourcesState
{
    std::mutex SourceCacheMutex{};
    std::unordered_map<std::string, std::weak_ptr<IVertexStreamSource>> SourceCache{};

    std::mutex MaterialCacheMutex{};
    std::unordered_map<std::string, RuntimeMaterialInstanceCacheEntry> GBufferMaterialInstances{};
    std::unordered_map<std::string, std::weak_ptr<MaterialInstance>> ShadowMaterialInstances{};
    std::unordered_map<std::string, std::weak_ptr<Material>> RuntimeMaterials{};

    static RenderAssetSharedResourcesState& Instance()
    {
        static RenderAssetSharedResourcesState State{};
        return State;
    }

    RenderAssetSharedResourcesState(const RenderAssetSharedResourcesState&) = delete;
    RenderAssetSharedResourcesState& operator=(const RenderAssetSharedResourcesState&) = delete;

private:
    RenderAssetSharedResourcesState() = default;
};

RenderAssetSharedResourcesState& RenderAssetSharedResourcesOps::SharedState()
{
    return RenderAssetSharedResourcesState::Instance();
}

std::string RenderAssetSharedResourcesOps::ToLowerCopy(std::string_view Text)
{
    std::string Value(Text);
    std::transform(Value.begin(), Value.end(), Value.begin(), [](const unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });
    return Value;
}

bool RenderAssetSharedResourcesOps::EqualsIgnoreCase(std::string_view Left, std::string_view Right)
{
    if (Left.size() != Right.size())
    {
        return false;
    }
    for (size_t Index = 0; Index < Left.size(); ++Index)
    {
        if (static_cast<char>(std::tolower(static_cast<unsigned char>(Left[Index]))) !=
            static_cast<char>(std::tolower(static_cast<unsigned char>(Right[Index]))))
        {
            return false;
        }
    }
    return true;
}

bool RenderAssetSharedResourcesOps::TryParseFloatValue(std::string_view Text, float& OutValue)
{
    std::string Buffer(Text);
    char* End = nullptr;
    const float Parsed = std::strtof(Buffer.c_str(), &End);
    if (End == Buffer.c_str())
    {
        return false;
    }
    while (End && *End != '\0' && std::isspace(static_cast<unsigned char>(*End)))
    {
        ++End;
    }
    if (End && *End != '\0')
    {
        return false;
    }
    OutValue = Parsed;
    return true;
}

const SnAPI::Graphics::ShaderMetaData::UserAttribute* RenderAssetSharedResourcesOps::FindParameterDefaultAttribute(
    const MaterialRuntimeParameterDesc& Parameter)
{
    for (const auto& Attribute : Parameter.Attributes)
    {
        if (EqualsIgnoreCase(Attribute.Name, "EditorDefault") || EqualsIgnoreCase(Attribute.Name, "Default"))
        {
            return &Attribute;
        }
    }
    return nullptr;
}

bool RenderAssetSharedResourcesOps::TryReadAttributeNumber(
    const SnAPI::Graphics::ShaderMetaData::UserAttribute& Attribute,
    const size_t Index,
    float& OutValue)
{
    if (Index < Attribute.FloatArguments.size())
    {
        OutValue = Attribute.FloatArguments[Index];
        return true;
    }
    if (Index < Attribute.IntArguments.size())
    {
        OutValue = static_cast<float>(Attribute.IntArguments[Index]);
        return true;
    }
    if (Index < Attribute.Arguments.size())
    {
        return TryParseFloatValue(Attribute.Arguments[Index], OutValue);
    }
    return false;
}

bool RenderAssetSharedResourcesOps::TryResolveAttributeScalarDefault(
    const MaterialRuntimeParameterDesc& Parameter,
    float& OutValue)
{
    const auto* Attribute = FindParameterDefaultAttribute(Parameter);
    if (!Attribute)
    {
        return false;
    }
    return TryReadAttributeNumber(*Attribute, 0, OutValue);
}

bool RenderAssetSharedResourcesOps::TryResolveAttributeVectorDefault(
    const MaterialRuntimeParameterDesc& Parameter,
    const size_t ComponentCount,
    std::array<float, 4>& OutValue)
{
    if (ComponentCount == 0 || ComponentCount > OutValue.size())
    {
        return false;
    }

    const auto* Attribute = FindParameterDefaultAttribute(Parameter);
    if (!Attribute)
    {
        return false;
    }

    OutValue = {0.0f, 0.0f, 0.0f, 0.0f};

    float FirstValue = 0.0f;
    if (!TryReadAttributeNumber(*Attribute, 0, FirstValue))
    {
        return false;
    }

    for (size_t Component = 0; Component < ComponentCount; ++Component)
    {
        float Value = FirstValue;
        (void)TryReadAttributeNumber(*Attribute, Component, Value);
        OutValue[Component] = Value;
    }
    return true;
}

SnAPI::Graphics::MaterialDomain RenderAssetSharedResourcesOps::DomainFromShadingModelName(std::string_view ShadingModel)
{
    if (ShadingModel == "GBufferShadingModel")
    {
        return SnAPI::Graphics::MaterialDomain::GBuffer;
    }
    if (ShadingModel == "ShadowShadingModel")
    {
        return SnAPI::Graphics::MaterialDomain::ShadowCaster;
    }
    if (ShadingModel == "UIShadingModel")
    {
        return SnAPI::Graphics::MaterialDomain::UI;
    }
    if (ShadingModel == "PostProcessShadingModel")
    {
        return SnAPI::Graphics::MaterialDomain::PostProcess;
    }
    if (ShadingModel == "DeferredShadingShadingModel")
    {
        return SnAPI::Graphics::MaterialDomain::DeferredLit;
    }
    return SnAPI::Graphics::MaterialDomain::GBuffer;
}

SharedMaterialInstancePtr RenderAssetSharedResourcesOps::GetOrCreateMaterialInstance(
    std::unordered_map<std::string, std::weak_ptr<MaterialInstance>>& Cache,
    const std::string& Key,
    const SharedMaterialPtr& BaseMaterial)
{
    if (!BaseMaterial || Key.empty())
    {
        return {};
    }

    auto ExistingIt = Cache.find(Key);
    if (ExistingIt != Cache.end())
    {
        if (auto Existing = ExistingIt->second.lock())
        {
            return Existing;
        }
        Cache.erase(ExistingIt);
    }

    auto Created = BaseMaterial->CreateMaterialInstance();
    if (Created && BaseMaterial->Domain() == SnAPI::Graphics::MaterialDomain::GBuffer)
    {
        auto* GraphicsAPI = SnAPI::Graphics::IGraphicsAPI::Instance();
        if (auto* VulkanAPI = static_cast<SnAPI::Graphics::VulkanGraphicsAPI*>(GraphicsAPI))
        {
            SnAPI::Graphics::BufferCreateInfo BufferCI{};
            BufferCI.Size = sizeof(SnAPI::Graphics::GBufferContract::ParamBlock);
            BufferCI.Usage = vk::BufferUsageFlagBits::eUniformBuffer;
            BufferCI.MemoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

            if (auto MaterialBuffer = VulkanAPI->CreateBuffer(BufferCI))
            {
                SnAPI::Graphics::GBufferContract::ParamBlock MatData{};
                MatData.Color[0] = 1.0f;
                MatData.Color[1] = 1.0f;
                MatData.Color[2] = 1.0f;
                MatData.Color[3] = 1.0f;
                MatData.Roughness = 0.8f;
                MatData.Metallic = 0.0f;
                MatData.Occlusion = 1.0f;
                MatData._Pad0 = 0.0f;

                if (void* Mapped = MaterialBuffer->Map(0, sizeof(SnAPI::Graphics::GBufferContract::ParamBlock)); Mapped)
                {
                    std::memcpy(Mapped, &MatData, sizeof(SnAPI::Graphics::GBufferContract::ParamBlock));
                    MaterialBuffer->UnMap();
                    Created->Buffer(std::string(SnAPI::Graphics::GBufferContract::ParamBlockName), std::move(MaterialBuffer));
                }
            }
        }
    }
    Cache.emplace(Key, Created);
    return Created;
}

SharedMaterialInstancePtr RenderAssetSharedResourcesOps::GetOrCreateMaterialInstance(
    std::unordered_map<std::string, RuntimeMaterialInstanceCacheEntry>& Cache,
    const std::string& Key,
    const SharedMaterialPtr& BaseMaterial)
{
    if (!BaseMaterial || Key.empty())
    {
        return {};
    }

    auto ExistingIt = Cache.find(Key);
    if (ExistingIt != Cache.end())
    {
        if (auto Existing = ExistingIt->second.Instance.lock())
        {
            return Existing;
        }
        Cache.erase(ExistingIt);
    }

    auto Created = BaseMaterial->CreateMaterialInstance();
    if (Created && BaseMaterial->Domain() == SnAPI::Graphics::MaterialDomain::GBuffer)
    {
        auto* GraphicsAPI = SnAPI::Graphics::IGraphicsAPI::Instance();
        if (auto* VulkanAPI = static_cast<SnAPI::Graphics::VulkanGraphicsAPI*>(GraphicsAPI))
        {
            SnAPI::Graphics::BufferCreateInfo BufferCI{};
            BufferCI.Size = sizeof(SnAPI::Graphics::GBufferContract::ParamBlock);
            BufferCI.Usage = vk::BufferUsageFlagBits::eUniformBuffer;
            BufferCI.MemoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

            if (auto MaterialBuffer = VulkanAPI->CreateBuffer(BufferCI))
            {
                SnAPI::Graphics::GBufferContract::ParamBlock MatData{};
                MatData.Color[0] = 1.0f;
                MatData.Color[1] = 1.0f;
                MatData.Color[2] = 1.0f;
                MatData.Color[3] = 1.0f;
                MatData.Roughness = 0.8f;
                MatData.Metallic = 0.0f;
                MatData.Occlusion = 1.0f;
                MatData._Pad0 = 0.0f;

                if (void* Mapped = MaterialBuffer->Map(0, sizeof(SnAPI::Graphics::GBufferContract::ParamBlock)); Mapped)
                {
                    std::memcpy(Mapped, &MatData, sizeof(SnAPI::Graphics::GBufferContract::ParamBlock));
                    MaterialBuffer->UnMap();
                    Created->Buffer(std::string(SnAPI::Graphics::GBufferContract::ParamBlockName), std::move(MaterialBuffer));
                }
            }
        }
    }

    RuntimeMaterialInstanceCacheEntry Entry{};
    Entry.Instance = Created;
    Cache.emplace(Key, std::move(Entry));
    return Created;
}

void RenderAssetSharedResourcesOps::CollectParameterLookupKeys(
    const MaterialRuntimeParameterDesc& Parameter,
    std::vector<std::string>& OutKeys)
{
    OutKeys.clear();
    OutKeys.reserve(6);

    auto AppendKey = [&OutKeys](std::string_view Key) {
        if (Key.empty())
        {
            return;
        }
        const std::string Lower = ToLowerCopy(Key);
        if (std::ranges::find(OutKeys, Lower) == OutKeys.end())
        {
            OutKeys.push_back(Lower);
        }
    };

    AppendKey(Parameter.Path);
    AppendKey(Parameter.Name);

    if (const size_t DotIndex = Parameter.Path.rfind('.'); DotIndex != std::string::npos)
    {
        AppendKey(std::string_view(Parameter.Path).substr(DotIndex + 1));
    }
}

bool RenderAssetSharedResourcesOps::TryResolveScalarValue(
    const MaterialRuntimeParameterDesc& Parameter,
    const std::unordered_map<std::string, float>& Scalars,
    float& OutValue)
{
    std::vector<std::string> LookupKeys{};
    CollectParameterLookupKeys(Parameter, LookupKeys);
    for (const std::string& LookupKey : LookupKeys)
    {
        if (const auto It = Scalars.find(LookupKey); It != Scalars.end())
        {
            OutValue = It->second;
            return true;
        }
    }
    return TryResolveAttributeScalarDefault(Parameter, OutValue);
}

bool RenderAssetSharedResourcesOps::TryResolveVectorValue(
    const MaterialRuntimeParameterDesc& Parameter,
    const std::unordered_map<std::string, std::array<float, 4>>& Vectors,
    std::array<float, 4>& OutValue)
{
    std::vector<std::string> LookupKeys{};
    CollectParameterLookupKeys(Parameter, LookupKeys);
    for (const std::string& LookupKey : LookupKeys)
    {
        if (const auto It = Vectors.find(LookupKey); It != Vectors.end())
        {
            OutValue = It->second;
            return true;
        }
    }

    size_t ComponentCount = 0;
    switch (Parameter.eValueType)
    {
    case SnAPI::Graphics::ShaderMetaData::EValueType::Float2:
        ComponentCount = 2;
        break;
    case SnAPI::Graphics::ShaderMetaData::EValueType::Float3:
        ComponentCount = 3;
        break;
    case SnAPI::Graphics::ShaderMetaData::EValueType::Float4:
        ComponentCount = 4;
        break;
    default:
        break;
    }
    if (ComponentCount == 0)
    {
        return false;
    }
    return TryResolveAttributeVectorDefault(Parameter, ComponentCount, OutValue);
}

struct RuntimeMaterialFeatureFlags
{
    bool AlbedoMap = false;
    bool NormalMap = false;
    bool RoughnessMap = false;
    bool MetalnessMap = false;
    bool OcclusionMap = false;
    bool AlphaTest = false;
    bool AlphaBlend = false;
    bool DoubleSided = false;
    bool Instancing = false;

    [[nodiscard]] uint32_t PackedMask() const
    {
        uint32_t Mask = 0u;
        Mask |= AlbedoMap ? (1u << 0) : 0u;
        Mask |= NormalMap ? (1u << 1) : 0u;
        Mask |= RoughnessMap ? (1u << 2) : 0u;
        Mask |= MetalnessMap ? (1u << 3) : 0u;
        Mask |= OcclusionMap ? (1u << 4) : 0u;
        Mask |= AlphaTest ? (1u << 5) : 0u;
        Mask |= AlphaBlend ? (1u << 6) : 0u;
        Mask |= DoubleSided ? (1u << 7) : 0u;
        Mask |= Instancing ? (1u << 8) : 0u;
        return Mask;
    }
};

bool RenderAssetSharedResourcesOps::HasTextureReference(const TAssetRef<RuntimeTextureRefAsset>& TextureRef)
{
    return !TextureRef.GetAssetId().empty() || !TextureRef.ResolvedAssetName().empty();
}

void RenderAssetSharedResourcesOps::ApplyFeatureFlagsFromTextureSlot(RuntimeMaterialFeatureFlags& OutFeatures, const std::string& SlotLower)
{
    if (SlotLower == "material_albedo" || SlotLower == "albedo" || SlotLower == "basecolor" || SlotLower == "base_color" || SlotLower == "diffuse")
    {
        OutFeatures.AlbedoMap = true;
    }

    if (SlotLower == "material_normal" || SlotLower.find("normal") != std::string::npos || SlotLower == "nrm")
    {
        OutFeatures.NormalMap = true;
    }

    if (SlotLower == "material_orm" || SlotLower == "orm")
    {
        OutFeatures.RoughnessMap = true;
        OutFeatures.MetalnessMap = true;
        OutFeatures.OcclusionMap = true;
        return;
    }

    if (SlotLower.find("rough") != std::string::npos)
    {
        OutFeatures.RoughnessMap = true;
    }

    if (SlotLower.find("metal") != std::string::npos)
    {
        OutFeatures.MetalnessMap = true;
    }

    if (SlotLower == "ao" || SlotLower.find("occlusion") != std::string::npos || SlotLower.find("ambientocclusion") != std::string::npos ||
        SlotLower.find("ambient_occlusion") != std::string::npos)
    {
        OutFeatures.OcclusionMap = true;
    }
}

RuntimeMaterialFeatureFlags RenderAssetSharedResourcesOps::BuildRuntimeMaterialFeatureFlags(
    const MaterialAssetRuntime& RuntimeMaterial,
    const MaterialInstanceAssetRuntime& RuntimeMaterialInstance)
{
    RuntimeMaterialFeatureFlags Features{};
    Features.AlbedoMap = RuntimeMaterial.FeatureAlbedoMap;
    Features.NormalMap = RuntimeMaterial.FeatureNormalMap;
    Features.RoughnessMap = RuntimeMaterial.FeatureRoughnessMap;
    Features.MetalnessMap = RuntimeMaterial.FeatureMetalnessMap;
    Features.OcclusionMap = RuntimeMaterial.FeatureOcclusionMap;
    Features.AlphaTest = RuntimeMaterial.FeatureAlphaTest;
    Features.AlphaBlend = RuntimeMaterial.FeatureAlphaBlend;
    Features.DoubleSided = RuntimeMaterial.FeatureDoubleSided;
    Features.Instancing = RuntimeMaterial.FeatureInstancing;
    if (RuntimeMaterial.ShadingModel != "GBufferShadingModel")
    {
        return Features;
    }

    if (!RuntimeMaterial.bLegacyInferFeaturesFromTextures)
    {
        return Features;
    }

    const size_t TextureCount = std::min(RuntimeMaterialInstance.TextureSlots.size(), RuntimeMaterialInstance.Textures.size());
    for (size_t Index = 0; Index < TextureCount; ++Index)
    {
        if (!HasTextureReference(RuntimeMaterialInstance.Textures[Index]))
        {
            continue;
        }
        ApplyFeatureFlagsFromTextureSlot(Features, ToLowerCopy(RuntimeMaterialInstance.TextureSlots[Index]));
    }
    return Features;
}

void RenderAssetSharedResourcesOps::ApplyRuntimeMaterialFeatures(SnAPI::Graphics::GBufferMaterial& RuntimeMaterial, const RuntimeMaterialFeatureFlags& Features)
{
    RuntimeMaterial.SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlbedoMap, Features.AlbedoMap);
    RuntimeMaterial.SetFeature(SnAPI::Graphics::GBufferContract::Feature::NormalMap, Features.NormalMap);
    RuntimeMaterial.SetFeature(SnAPI::Graphics::GBufferContract::Feature::RoughnessMap, Features.RoughnessMap);
    RuntimeMaterial.SetFeature(SnAPI::Graphics::GBufferContract::Feature::MetalnessMap, Features.MetalnessMap);
    RuntimeMaterial.SetFeature(SnAPI::Graphics::GBufferContract::Feature::OcclusionMap, Features.OcclusionMap);
    RuntimeMaterial.SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlphaTest, Features.AlphaTest);
    RuntimeMaterial.SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlphaBlend, Features.AlphaBlend);
    RuntimeMaterial.SetFeature(SnAPI::Graphics::GBufferContract::Feature::DoubleSided, Features.DoubleSided);
    RuntimeMaterial.SetFeature(SnAPI::Graphics::GBufferContract::Feature::Instancing, Features.Instancing);
}

void RenderAssetSharedResourcesOps::CollectResourceLookupKeys(const MaterialRuntimeResourceDesc& Resource, std::vector<std::string>& OutKeys)
{
    OutKeys.clear();
    OutKeys.reserve(3);

    auto AppendKey = [&OutKeys](std::string_view Key) {
        if (Key.empty())
        {
            return;
        }
        const std::string Lower = ToLowerCopy(Key);
        if (std::ranges::find(OutKeys, Lower) == OutKeys.end())
        {
            OutKeys.push_back(Lower);
        }
    };

    AppendKey(Resource.Name);
    if (const size_t DotIndex = Resource.Name.rfind('.'); DotIndex != std::string::npos)
    {
        AppendKey(std::string_view(Resource.Name).substr(DotIndex + 1));
    }
}

void RenderAssetSharedResourcesOps::ApplyDescriptorTexturesToMaterialInstance(
    MaterialInstance& TargetInstance,
    const MaterialRuntimeDescriptor& Descriptor,
    const MaterialInstanceAssetRuntime& RuntimeMaterialInstance,
    ::SnAPI::AssetPipeline::AssetManager& AssetManager,
    std::vector<RuntimeTextureHandle>& OutBoundTextureHandles)
{
    OutBoundTextureHandles.clear();

    const size_t TextureCount = std::min(RuntimeMaterialInstance.TextureSlots.size(), RuntimeMaterialInstance.Textures.size());
    if (TextureCount == 0)
    {
        return;
    }

    std::unordered_map<std::string, size_t> TextureIndexBySlot{};
    TextureIndexBySlot.reserve(TextureCount);
    for (size_t Index = 0; Index < TextureCount; ++Index)
    {
        if (!HasTextureReference(RuntimeMaterialInstance.Textures[Index]))
        {
            continue;
        }

        const std::string SlotKey = ToLowerCopy(RuntimeMaterialInstance.TextureSlots[Index]);
        if (SlotKey.empty() || TextureIndexBySlot.contains(SlotKey))
        {
            continue;
        }
        TextureIndexBySlot.emplace(SlotKey, Index);
    }

    std::vector<std::string> ResourceLookupKeys{};
    for (const MaterialRuntimeResourceDesc& Resource : Descriptor.Resources)
    {
        if (Resource.Set != 0 || Resource.eBindingType != SnAPI::Graphics::ShaderMetaData::EBindingType::SampledImage)
        {
            continue;
        }

        std::optional<size_t> TextureIndex{};
        CollectResourceLookupKeys(Resource, ResourceLookupKeys);
        for (const std::string& LookupKey : ResourceLookupKeys)
        {
            if (const auto Match = TextureIndexBySlot.find(LookupKey); Match != TextureIndexBySlot.end())
            {
                TextureIndex = Match->second;
                break;
            }
        }

        if (!TextureIndex.has_value() && static_cast<size_t>(Resource.Binding) < TextureCount)
        {
            TextureIndex = static_cast<size_t>(Resource.Binding);
        }

        if (!TextureIndex.has_value() || *TextureIndex >= TextureCount)
        {
            continue;
        }

        auto TextureHandleResult = RuntimeMaterialInstance.Textures[*TextureIndex].GetShared<RuntimeTextureRefAsset>(AssetManager);
        if (!TextureHandleResult || !TextureHandleResult->Get())
        {
            continue;
        }

        OutBoundTextureHandles.push_back(*TextureHandleResult);
        TargetInstance.Texture(Resource.Name, TextureHandleResult->Get());
    }
}

template<typename TValue>
bool RenderAssetSharedResourcesOps::WriteParameterValue(std::vector<uint8_t>& BufferBytes, const MaterialRuntimeParameterDesc& Parameter, const TValue& Value)
{
    if (Parameter.OffsetBytes + sizeof(TValue) > BufferBytes.size())
    {
        return false;
    }
    std::memcpy(BufferBytes.data() + Parameter.OffsetBytes, &Value, sizeof(TValue));
    return true;
}

bool RenderAssetSharedResourcesOps::ApplyParameterToBuffer(
    const MaterialRuntimeParameterDesc& Parameter,
    const std::unordered_map<std::string, float>& Scalars,
    const std::unordered_map<std::string, std::array<float, 4>>& Vectors,
    std::vector<uint8_t>& BufferBytes)
{
    switch (Parameter.eValueType)
    {
    case SnAPI::Graphics::ShaderMetaData::EValueType::Bool:
        {
            float Scalar = 0.0f;
            if (!TryResolveScalarValue(Parameter, Scalars, Scalar))
            {
                return false;
            }
            const uint32_t Value = Scalar != 0.0f ? 1u : 0u;
            return WriteParameterValue(BufferBytes, Parameter, Value);
        }
    case SnAPI::Graphics::ShaderMetaData::EValueType::Int:
        {
            float Scalar = 0.0f;
            if (!TryResolveScalarValue(Parameter, Scalars, Scalar))
            {
                return false;
            }
            const int32_t Value = static_cast<int32_t>(Scalar);
            return WriteParameterValue(BufferBytes, Parameter, Value);
        }
    case SnAPI::Graphics::ShaderMetaData::EValueType::UInt:
        {
            float Scalar = 0.0f;
            if (!TryResolveScalarValue(Parameter, Scalars, Scalar))
            {
                return false;
            }
            const uint32_t Value = static_cast<uint32_t>(std::max(0.0f, Scalar));
            return WriteParameterValue(BufferBytes, Parameter, Value);
        }
    case SnAPI::Graphics::ShaderMetaData::EValueType::Float:
        {
            float Scalar = 0.0f;
            if (!TryResolveScalarValue(Parameter, Scalars, Scalar))
            {
                return false;
            }
            return WriteParameterValue(BufferBytes, Parameter, Scalar);
        }
    case SnAPI::Graphics::ShaderMetaData::EValueType::Float2:
        {
            std::array<float, 4> VectorValue{};
            if (!TryResolveVectorValue(Parameter, Vectors, VectorValue))
            {
                return false;
            }
            std::array<float, 2> Value{VectorValue[0], VectorValue[1]};
            return WriteParameterValue(BufferBytes, Parameter, Value);
        }
    case SnAPI::Graphics::ShaderMetaData::EValueType::Float3:
        {
            std::array<float, 4> VectorValue{};
            if (!TryResolveVectorValue(Parameter, Vectors, VectorValue))
            {
                return false;
            }
            std::array<float, 3> Value{VectorValue[0], VectorValue[1], VectorValue[2]};
            return WriteParameterValue(BufferBytes, Parameter, Value);
        }
    case SnAPI::Graphics::ShaderMetaData::EValueType::Float4:
        {
            std::array<float, 4> VectorValue{};
            if (!TryResolveVectorValue(Parameter, Vectors, VectorValue))
            {
                return false;
            }
            return WriteParameterValue(BufferBytes, Parameter, VectorValue);
        }
    case SnAPI::Graphics::ShaderMetaData::EValueType::Float3x3:
    case SnAPI::Graphics::ShaderMetaData::EValueType::Float4x4:
    case SnAPI::Graphics::ShaderMetaData::EValueType::Unknown:
    default:
        return false;
    }
}

void RenderAssetSharedResourcesOps::ApplyDescriptorBuffersToMaterialInstance(
    MaterialInstance& TargetInstance,
    const MaterialRuntimeDescriptor& Descriptor,
    const MaterialInstanceAssetRuntime& RuntimeMaterialInstance)
{
    auto* GraphicsAPI = SnAPI::Graphics::IGraphicsAPI::Instance();
    auto* VulkanAPI = static_cast<SnAPI::Graphics::VulkanGraphicsAPI*>(GraphicsAPI);
    if (!VulkanAPI)
    {
        return;
    }

    std::unordered_map<std::string, float> Scalars{};
    Scalars.reserve(RuntimeMaterialInstance.Scalars.size());
    for (const MaterialScalarParamPayload& Scalar : RuntimeMaterialInstance.Scalars)
    {
        Scalars[ToLowerCopy(Scalar.Name)] = Scalar.Value;
    }

    std::unordered_map<std::string, std::array<float, 4>> Vectors{};
    Vectors.reserve(RuntimeMaterialInstance.Vectors.size());
    for (const MaterialVectorParamPayload& Vector : RuntimeMaterialInstance.Vectors)
    {
        Vectors[ToLowerCopy(Vector.Name)] = Vector.Value;
    }

    for (const auto& BufferDesc : Descriptor.Buffers)
    {
        uint32_t BufferSize = BufferDesc.SizeBytes;
        if (BufferSize == 0)
        {
            for (const auto& Parameter : Descriptor.Parameters)
            {
                if (Parameter.BufferName != BufferDesc.Name)
                {
                    continue;
                }
                BufferSize = std::max(BufferSize, Parameter.OffsetBytes + Parameter.SizeBytes);
            }
        }
        if (BufferSize == 0)
        {
            continue;
        }

        std::vector<uint8_t> BufferBytes(BufferSize, 0u);
        bool HasBaselineDefaults = false;
        if (BufferDesc.Name == std::string(SnAPI::Graphics::GBufferContract::ParamBlockName) &&
            BufferSize >= sizeof(SnAPI::Graphics::GBufferContract::ParamBlock))
        {
            SnAPI::Graphics::GBufferContract::ParamBlock Defaults{};
            Defaults.Color[0] = 1.0f;
            Defaults.Color[1] = 1.0f;
            Defaults.Color[2] = 1.0f;
            Defaults.Color[3] = 1.0f;
            Defaults.Roughness = 0.8f;
            Defaults.Metallic = 0.0f;
            Defaults.Occlusion = 1.0f;
            Defaults._Pad0 = 0.0f;
            std::memcpy(BufferBytes.data(), &Defaults, sizeof(Defaults));
            HasBaselineDefaults = true;
        }

        bool WroteAny = false;
        for (const auto& Parameter : Descriptor.Parameters)
        {
            if (Parameter.BufferName != BufferDesc.Name)
            {
                continue;
            }
            if (ApplyParameterToBuffer(Parameter, Scalars, Vectors, BufferBytes))
            {
                WroteAny = true;
            }
        }

        if (!WroteAny && !HasBaselineDefaults)
        {
            continue;
        }

        SnAPI::Graphics::BufferCreateInfo BufferCI{};
        BufferCI.Size = BufferSize;
        BufferCI.Usage = vk::BufferUsageFlagBits::eUniformBuffer;
        BufferCI.MemoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        if (const auto ResourceIt = std::ranges::find_if(
                Descriptor.Resources,
                [&BufferDesc](const MaterialRuntimeResourceDesc& Resource) {
                    return Resource.Name == BufferDesc.Name &&
                           Resource.eBindingType == SnAPI::Graphics::ShaderMetaData::EBindingType::Buffer;
                });
            ResourceIt != Descriptor.Resources.end() &&
            ResourceIt->eDescriptorType == SnAPI::Graphics::EDescriptorType::StorageBuffer)
        {
            BufferCI.Usage = vk::BufferUsageFlagBits::eStorageBuffer;
        }

        if (auto MaterialBuffer = VulkanAPI->CreateBuffer(BufferCI))
        {
            if (void* Mapped = MaterialBuffer->Map(0, BufferBytes.size()); Mapped)
            {
                std::memcpy(Mapped, BufferBytes.data(), BufferBytes.size());
                MaterialBuffer->UnMap();
                TargetInstance.Buffer(BufferDesc.Name, std::move(MaterialBuffer));
            }
        }
    }
}

SharedMaterialPtr RenderAssetSharedResourcesOps::GetOrCreateRuntimeMaterial(
    const std::string& Key,
    const MaterialAssetRuntime& RuntimeMaterial,
    const RuntimeMaterialFeatureFlags& FeatureFlags)
{
    if (Key.empty() || RuntimeMaterial.ShaderModule.empty())
    {
        return {};
    }

    auto& SharedState = RenderAssetSharedResourcesOps::SharedState();
    auto ExistingMaterialIt = SharedState.RuntimeMaterials.find(Key);
    if (ExistingMaterialIt != SharedState.RuntimeMaterials.end())
    {
        if (auto Existing = ExistingMaterialIt->second.lock())
        {
            return Existing;
        }
        SharedState.RuntimeMaterials.erase(ExistingMaterialIt);
    }

    SharedMaterialPtr Created{};
    if (RuntimeMaterial.ShadingModel == "GBufferShadingModel")
    {
        auto GBufferRuntimeMaterial = std::make_shared<SnAPI::Graphics::GBufferMaterial>(RuntimeMaterial.ShaderModule);
        ApplyRuntimeMaterialFeatures(*GBufferRuntimeMaterial, FeatureFlags);
        GBufferRuntimeMaterial->BakeCompileTimeParams();
        Created = std::move(GBufferRuntimeMaterial);
    }
    else if (RuntimeMaterial.ShadingModel == "ShadowShadingModel")
    {
        auto ShadowRuntimeMaterial = std::make_shared<SnAPI::Graphics::ShadowMaterial>(RuntimeMaterial.ShaderModule);
        ShadowRuntimeMaterial->BakeCompileTimeParams();
        Created = std::move(ShadowRuntimeMaterial);
    }
    else
    {
        auto GenericRuntimeMaterial = std::make_shared<Material>(
            RuntimeMaterial.ShaderModule,
            RuntimeMaterial.ShadingModel,
            DomainFromShadingModelName(RuntimeMaterial.ShadingModel));
        GenericRuntimeMaterial->BakeAndCompile();
        Created = std::move(GenericRuntimeMaterial);
    }

    SharedState.RuntimeMaterials.emplace(Key, Created);
    return Created;
}

std::string RenderAssetSharedResourcesOps::BuildRuntimeMaterialKey(
    const MaterialAssetRuntime& RuntimeMaterial,
    std::string_view MaterialAssetId,
    const RuntimeMaterialFeatureFlags& FeatureFlags)
{
    const std::string FeatureSuffix = "|features=" + std::to_string(FeatureFlags.PackedMask());
    if (!MaterialAssetId.empty())
    {
        return std::string("material-asset-id://") + std::string(MaterialAssetId) + FeatureSuffix;
    }
    return std::string("material://") + RuntimeMaterial.ShadingModel + "|" + RuntimeMaterial.ShaderModule + FeatureSuffix;
}

SharedMaterialInstancePtr RenderAssetSharedResourcesOps::BuildRuntimeMaterialInstance(
    const TAssetRef<MaterialInstanceAssetRuntime>& MaterialRef,
    ::SnAPI::AssetPipeline::AssetManager* AssetManager,
    std::vector<RuntimeTextureHandle>* OutTextureHandles)
{
    if (!AssetManager)
    {
        return {};
    }

    auto MaterialInstanceHandleResult = MaterialRef.GetShared<MaterialInstanceAssetRuntime>(*AssetManager);
    if (!MaterialInstanceHandleResult || !MaterialInstanceHandleResult->Get())
    {
        return {};
    }

    const MaterialInstanceAssetRuntime& RuntimeMaterialInstance = *MaterialInstanceHandleResult->Get();
    if (RuntimeMaterialInstance.ParentMaterial.IsNull())
    {
        return {};
    }

    auto ParentMaterialHandleResult = RuntimeMaterialInstance.ParentMaterial.GetShared<MaterialAssetRuntime>(*AssetManager);
    if (!ParentMaterialHandleResult || !ParentMaterialHandleResult->Get())
    {
        return {};
    }

    const MaterialAssetRuntime& ParentMaterial = *ParentMaterialHandleResult->Get();
    const RuntimeMaterialFeatureFlags FeatureFlags = BuildRuntimeMaterialFeatureFlags(ParentMaterial, RuntimeMaterialInstance);
    const std::string ParentMaterialId = ParentMaterialHandleResult->GetAssetId().IsNull()
        ? std::string{}
        : ParentMaterialHandleResult->GetAssetId().ToString();
    const std::string ParentKey = BuildRuntimeMaterialKey(ParentMaterial, ParentMaterialId, FeatureFlags);

    SharedMaterialPtr BaseMaterial{};
    {
        std::scoped_lock Lock(RenderAssetSharedResourcesOps::SharedState().MaterialCacheMutex);
        BaseMaterial = GetOrCreateRuntimeMaterial(ParentKey, ParentMaterial, FeatureFlags);
    }
    if (!BaseMaterial)
    {
        return {};
    }

    SharedMaterialInstancePtr Created = BaseMaterial->CreateMaterialInstance();
    if (!Created)
    {
        return {};
    }

    const MaterialRuntimeDescriptor Descriptor = SnAPI::Graphics::BuildMaterialRuntimeDescriptor(*Created);
    ApplyDescriptorBuffersToMaterialInstance(*Created, Descriptor, RuntimeMaterialInstance);
    if (OutTextureHandles)
    {
        ApplyDescriptorTexturesToMaterialInstance(*Created, Descriptor, RuntimeMaterialInstance, *AssetManager, *OutTextureHandles);
    }
    else
    {
        std::vector<RuntimeTextureHandle> ScratchHandles{};
        ApplyDescriptorTexturesToMaterialInstance(*Created, Descriptor, RuntimeMaterialInstance, *AssetManager, ScratchHandles);
    }
    return Created;
}

SharedMaterialInstancePtr RenderAssetSharedResourcesOps::GetOrCreateRuntimeMaterialInstance(
    const std::string& Key,
    const TAssetRef<MaterialInstanceAssetRuntime>& MaterialRef,
    ::SnAPI::AssetPipeline::AssetManager* AssetManager)
{
    if (Key.empty())
    {
        return {};
    }

    {
        auto& SharedState = RenderAssetSharedResourcesOps::SharedState();
        std::scoped_lock Lock(SharedState.MaterialCacheMutex);
        if (auto ExistingIt = SharedState.GBufferMaterialInstances.find(Key);
            ExistingIt != SharedState.GBufferMaterialInstances.end())
        {
            if (auto Existing = ExistingIt->second.Instance.lock())
            {
                return Existing;
            }
            SharedState.GBufferMaterialInstances.erase(ExistingIt);
        }
    }

    std::vector<RuntimeTextureHandle> TextureHandles{};
    auto Created = BuildRuntimeMaterialInstance(MaterialRef, AssetManager, &TextureHandles);
    if (!Created)
    {
        return {};
    }

    auto& SharedState = RenderAssetSharedResourcesOps::SharedState();
    std::scoped_lock Lock(SharedState.MaterialCacheMutex);
    if (auto ExistingIt = SharedState.GBufferMaterialInstances.find(Key);
        ExistingIt != SharedState.GBufferMaterialInstances.end())
    {
        if (auto Existing = ExistingIt->second.Instance.lock())
        {
            return Existing;
        }
        SharedState.GBufferMaterialInstances.erase(ExistingIt);
    }

    RuntimeMaterialInstanceCacheEntry Entry{};
    Entry.Instance = Created;
    Entry.TextureHandles = std::move(TextureHandles);
    SharedState.GBufferMaterialInstances.emplace(Key, std::move(Entry));
    return Created;
}

std::string RenderAssetSharedResourcesOps::BuildMaterialCacheKey(
    const TAssetRef<MaterialInstanceAssetRuntime>& MaterialRef,
    ::SnAPI::AssetPipeline::AssetManager* AssetManager)
{
    if (!MaterialRef.GetAssetId().empty())
    {
        return "material-instance-ref-id://" + MaterialRef.GetAssetId();
    }
    if (!MaterialRef.ResolvedAssetName().empty())
    {
        return "material-instance-name://" + MaterialRef.ResolvedAssetName();
    }

    if (!MaterialRef.IsNull() && AssetManager)
    {
        auto SharedMaterialResult = MaterialRef.GetShared<MaterialInstanceAssetRuntime>(*AssetManager);
        if (SharedMaterialResult && SharedMaterialResult->Get())
        {
            if (!SharedMaterialResult->GetAssetId().IsNull())
            {
                return "material-instance-id://" + SharedMaterialResult->GetAssetId().ToString();
            }
        }
    }

    return {};
}

} // namespace

std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> AcquireSharedRuntimeMeshStreamSource(
    const StaticMeshAssetRuntime& RuntimeMesh,
    std::string_view StableKey)
{
    const std::string CacheKey = RenderAssetSharedResourcesOps::BuildRuntimeMeshCacheKey(RuntimeMesh, StableKey);

    {
        auto& SharedState = RenderAssetSharedResourcesOps::SharedState();
        std::scoped_lock Lock(SharedState.SourceCacheMutex);
        auto ExistingIt = SharedState.SourceCache.find(CacheKey);
        if (ExistingIt != SharedState.SourceCache.end())
        {
            if (auto Existing = ExistingIt->second.lock())
            {
                return Existing;
            }
            SharedState.SourceCache.erase(ExistingIt);
        }
    }

    RuntimeMeshSourceData SourceData{};
    if (!RenderAssetSharedResourcesOps::BuildRuntimeMeshSourceData(RuntimeMesh, CacheKey, SourceData))
    {
        return {};
    }

    auto Created = std::make_shared<RuntimeMeshVertexStreamSource>(std::move(SourceData));
    if (!Created)
    {
        return {};
    }

    auto& SharedState = RenderAssetSharedResourcesOps::SharedState();
    std::scoped_lock Lock(SharedState.SourceCacheMutex);
    SharedState.SourceCache[CacheKey] = Created;
    return Created;
}

void ApplyDefaultMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject, RendererSystem& Renderer)
{
    const auto& Source = RenderObject.VertexStreamSource();
    if (!Source)
    {
        return;
    }

    const SharedMaterialPtr GBufferMaterial = Renderer.DefaultGBufferMaterial();
    const SharedMaterialPtr ShadowMaterial = Renderer.DefaultShadowMaterial();

    auto& SharedState = RenderAssetSharedResourcesOps::SharedState();
    std::scoped_lock Lock(SharedState.MaterialCacheMutex);
    const SharedMaterialInstancePtr SharedGBuffer =
        RenderAssetSharedResourcesOps::GetOrCreateMaterialInstance(SharedState.GBufferMaterialInstances, "__default__", GBufferMaterial);
    const SharedMaterialInstancePtr SharedShadow =
        RenderAssetSharedResourcesOps::GetOrCreateMaterialInstance(SharedState.ShadowMaterialInstances, "__default__", ShadowMaterial);

    for (uint32_t SubMeshIndex = 0; SubMeshIndex < Source->SubMeshCount(); ++SubMeshIndex)
    {
        if (SharedGBuffer)
        {
            RenderObject.SetMaterialInstance(SubMeshIndex, SharedGBuffer);
        }
        if (SharedShadow)
        {
            RenderObject.SetShadowMaterialInstance(SubMeshIndex, SharedShadow);
        }
    }
}

void ApplyRuntimeOrDefaultMaterialInstances(
    SnAPI::Graphics::IRenderObject& RenderObject,
    RendererSystem& Renderer,
    const std::vector<TAssetRef<MaterialInstanceAssetRuntime>>& MaterialRefs,
    ::SnAPI::AssetPipeline::AssetManager* AssetManager)
{
    const auto& Source = RenderObject.VertexStreamSource();
    if (!Source)
    {
        return;
    }

    const SharedMaterialPtr GBufferMaterial = Renderer.DefaultGBufferMaterial();
    const SharedMaterialPtr ShadowMaterial = Renderer.DefaultShadowMaterial();

    SharedMaterialInstancePtr DefaultGBuffer{};
    SharedMaterialInstancePtr DefaultShadow{};
    {
        auto& SharedState = RenderAssetSharedResourcesOps::SharedState();
        std::scoped_lock Lock(SharedState.MaterialCacheMutex);
        DefaultGBuffer = RenderAssetSharedResourcesOps::GetOrCreateMaterialInstance(
            SharedState.GBufferMaterialInstances, "__default__", GBufferMaterial);
        DefaultShadow = RenderAssetSharedResourcesOps::GetOrCreateMaterialInstance(
            SharedState.ShadowMaterialInstances, "__default__", ShadowMaterial);
    }

    for (uint32_t SubMeshIndex = 0; SubMeshIndex < Source->SubMeshCount(); ++SubMeshIndex)
    {
        VertexSourceSubMesh SubMesh{};
        const bool HasSubMesh = Source->SubMesh(SubMeshIndex, SubMesh);
        const uint32_t MaterialSlot = HasSubMesh ? SubMesh.MaterialSlot : SubMeshIndex;

        std::string MaterialKey{};
        if (MaterialSlot < MaterialRefs.size())
        {
            MaterialKey = RenderAssetSharedResourcesOps::BuildMaterialCacheKey(MaterialRefs[MaterialSlot], AssetManager);
        }

        SharedMaterialInstancePtr SharedGBuffer = DefaultGBuffer;
        SharedMaterialInstancePtr SharedShadow = DefaultShadow;
        if (!MaterialKey.empty())
        {
            if (MaterialSlot < MaterialRefs.size())
            {
                if (const auto RuntimeInstance =
                        RenderAssetSharedResourcesOps::GetOrCreateRuntimeMaterialInstance(
                            MaterialKey, MaterialRefs[MaterialSlot], AssetManager))
                {
                    SharedGBuffer = RuntimeInstance;
                }
            }
        }

        if (SharedGBuffer)
        {
            RenderObject.SetMaterialInstance(SubMeshIndex, SharedGBuffer);
        }
        if (SharedShadow)
        {
            RenderObject.SetShadowMaterialInstance(SubMeshIndex, SharedShadow);
        }
    }
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
