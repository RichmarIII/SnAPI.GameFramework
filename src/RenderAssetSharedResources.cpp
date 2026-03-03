#include "RenderAssetSharedResources.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <array>
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
using SnAPI::Graphics::SharedMaterialInstancePtr;
using SnAPI::Graphics::SharedMaterialPtr;
using SnAPI::Graphics::VertexSourceMaterial;
using SnAPI::Graphics::VertexSourceSubMesh;

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

constexpr uint64_t kFnv1aOffset = 1469598103934665603ull;
constexpr uint64_t kFnv1aPrime = 1099511628211ull;

uint64_t HashBytes64(const void* Data, const size_t Size, uint64_t Seed = kFnv1aOffset)
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

uint64_t HashString64(std::string_view Value, uint64_t Seed = kFnv1aOffset)
{
    return HashBytes64(Value.data(), Value.size(), Seed);
}

std::string PointerKey(const void* Address)
{
    std::ostringstream Stream{};
    Stream << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(Address);
    return Stream.str();
}

std::string BuildRuntimeMeshCacheKey(const StaticMeshAssetRuntime& RuntimeMesh, std::string_view StableKey)
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

std::optional<EVertexStream> ToRendererStream(const EMeshStreamSemantic Semantic)
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

const MeshStreamChunkRef* FindStreamRef(const StaticMeshAssetRuntime& RuntimeMesh, const EMeshStreamSemantic Semantic)
{
    const auto It = std::ranges::find_if(RuntimeMesh.Streams, [Semantic](const MeshStreamChunkRef& StreamRef) {
        return StreamRef.Semantic == Semantic;
    });
    return (It == RuntimeMesh.Streams.end()) ? nullptr : &(*It);
}

bool LoadRawStreamBytes(const StaticMeshAssetRuntime& RuntimeMesh, const MeshStreamChunkRef& StreamRef, RuntimeStreamBuffer& Out)
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

bool DecodeIndexStream(const RuntimeStreamBuffer& Stream, std::vector<uint32_t>& OutIndices)
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

bool BuildRuntimeMeshSourceData(const StaticMeshAssetRuntime& RuntimeMesh, std::string_view CacheKey, RuntimeMeshSourceData& Out)
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

std::mutex GSourceCacheMutex{};
std::unordered_map<std::string, std::weak_ptr<IVertexStreamSource>> GSourceCache{};

std::mutex GMaterialCacheMutex{};
std::unordered_map<std::string, std::weak_ptr<MaterialInstance>> GGBufferMaterialInstances{};
std::unordered_map<std::string, std::weak_ptr<MaterialInstance>> GShadowMaterialInstances{};

SharedMaterialInstancePtr GetOrCreateMaterialInstance(
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

std::string BuildMaterialCacheKey(
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
    const std::string CacheKey = BuildRuntimeMeshCacheKey(RuntimeMesh, StableKey);

    {
        std::scoped_lock Lock(GSourceCacheMutex);
        auto ExistingIt = GSourceCache.find(CacheKey);
        if (ExistingIt != GSourceCache.end())
        {
            if (auto Existing = ExistingIt->second.lock())
            {
                return Existing;
            }
            GSourceCache.erase(ExistingIt);
        }
    }

    RuntimeMeshSourceData SourceData{};
    if (!BuildRuntimeMeshSourceData(RuntimeMesh, CacheKey, SourceData))
    {
        return {};
    }

    auto Created = std::make_shared<RuntimeMeshVertexStreamSource>(std::move(SourceData));
    if (!Created)
    {
        return {};
    }

    std::scoped_lock Lock(GSourceCacheMutex);
    GSourceCache[CacheKey] = Created;
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

    std::scoped_lock Lock(GMaterialCacheMutex);
    const SharedMaterialInstancePtr SharedGBuffer =
        GetOrCreateMaterialInstance(GGBufferMaterialInstances, "__default__", GBufferMaterial);
    const SharedMaterialInstancePtr SharedShadow =
        GetOrCreateMaterialInstance(GShadowMaterialInstances, "__default__", ShadowMaterial);

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

    std::scoped_lock Lock(GMaterialCacheMutex);
    const SharedMaterialInstancePtr DefaultGBuffer =
        GetOrCreateMaterialInstance(GGBufferMaterialInstances, "__default__", GBufferMaterial);
    const SharedMaterialInstancePtr DefaultShadow =
        GetOrCreateMaterialInstance(GShadowMaterialInstances, "__default__", ShadowMaterial);

    for (uint32_t SubMeshIndex = 0; SubMeshIndex < Source->SubMeshCount(); ++SubMeshIndex)
    {
        VertexSourceSubMesh SubMesh{};
        const bool HasSubMesh = Source->SubMesh(SubMeshIndex, SubMesh);
        const uint32_t MaterialSlot = HasSubMesh ? SubMesh.MaterialSlot : SubMeshIndex;

        std::string MaterialKey{};
        if (MaterialSlot < MaterialRefs.size())
        {
            MaterialKey = BuildMaterialCacheKey(MaterialRefs[MaterialSlot], AssetManager);
        }

        SharedMaterialInstancePtr SharedGBuffer = DefaultGBuffer;
        SharedMaterialInstancePtr SharedShadow = DefaultShadow;
        if (!MaterialKey.empty())
        {
            if (const auto Cached = GetOrCreateMaterialInstance(GGBufferMaterialInstances, MaterialKey, GBufferMaterial))
            {
                SharedGBuffer = Cached;
            }
            if (const auto Cached = GetOrCreateMaterialInstance(GShadowMaterialInstances, MaterialKey, ShadowMaterial))
            {
                SharedShadow = Cached;
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
