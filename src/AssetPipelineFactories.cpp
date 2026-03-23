#include "AssetPipelineFactories.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

#include "AssetPipelineIds.h"
#include "AssetPipelineSerializers.h"
#include "AssetRef.h"
#include "BaseNode.h"
#include "Conduit/Asset.h"
#include "IAssetCooker.h"
#include "IAssetImporter.h"
#include "Level.h"
#include "NodeCast.h"
#include "RenderAssets/MeshRuntimeAssets.h"
#include "RenderAssetPayloads.h"
#include "Serialization.h"
#include "TextureCompressorIds.h"
#include "TextureCompressorPayloads.h"
#include "TextureCompressorPayloadSerializers.h"
#include "World.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <GraphicsAPITypes.hpp>
#include <IGraphicsAPI.hpp>
#include <Image.hpp>
#include <IVertexStreamSource.hpp>
#include <Material.hpp>
#include <MaterialContracts.hpp>
#include <MaterialInstance.hpp>
#include <MaterialRuntimeDescriptor.hpp>
#include <TMaterialFor.hpp>
#include <VulkanGraphicsAPI.hpp>
#endif

namespace TextureCompressorPlugin
{
std::unique_ptr<SnAPI::AssetPipeline::IAssetImporter> CreateTextureCompressorImporter();
std::unique_ptr<SnAPI::AssetPipeline::IAssetCooker> CreateTextureCompressorCooker();
} // namespace TextureCompressorPlugin

namespace SnAPI::GameFramework
{
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateAuthoredAssetJsonImporter();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateAuthoredAssetPassThroughCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateNodeSourceCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateLevelSourceCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateWorldSourceCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetJsonImporter();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetAssimpImporter();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderTextureCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialInstanceCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderSkeletonCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderAnimationCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderStaticMeshCooker();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderSkeletalMeshCooker();

namespace
{
std::expected<void, std::string> PrefixNodeNameInPayload(NodePayload& Payload)
{
    try
    {
        std::ostringstream NameStream(std::ios::binary);
        cereal::BinaryOutputArchive NameArchive(NameStream);
        NameArchive(Payload.Name);
        const std::string EncodedName = NameStream.str();

        std::vector<uint8_t> MigratedNodeBytes{};
        MigratedNodeBytes.reserve(EncodedName.size() + Payload.NodeBytes.size());
        MigratedNodeBytes.insert(MigratedNodeBytes.end(), EncodedName.begin(), EncodedName.end());
        MigratedNodeBytes.insert(MigratedNodeBytes.end(), Payload.NodeBytes.begin(), Payload.NodeBytes.end());
        Payload.NodeBytes = std::move(MigratedNodeBytes);
        Payload.HasNodeData = true;

        for (NodePayload& Child : Payload.Children)
        {
            auto ChildResult = PrefixNodeNameInPayload(Child);
            if (!ChildResult)
            {
                return ChildResult;
            }
        }

        return {};
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(std::string("Failed to migrate node payload bytes: ") + Ex.what());
    }
}

std::expected<void, std::string> MigrateNodePayloadBaseNodeName(std::vector<uint8_t>& InOutBytes)
{
    auto PayloadResult = DeserializeNodePayload(InOutBytes.data(), InOutBytes.size());
    if (!PayloadResult)
    {
        return std::unexpected(PayloadResult.error().Message);
    }

    auto PrefixResult = PrefixNodeNameInPayload(*PayloadResult);
    if (!PrefixResult)
    {
        return PrefixResult;
    }

    std::vector<uint8_t> MigratedBytes{};
    auto SerializeResult = SerializeNodePayload(*PayloadResult, MigratedBytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error().Message);
    }

    InOutBytes = std::move(MigratedBytes);
    return {};
}

std::expected<void, std::string> MigrateLevelPayloadBaseNodeName(std::vector<uint8_t>& InOutBytes)
{
    auto PayloadResult = DeserializeLevelPayload(InOutBytes.data(), InOutBytes.size());
    if (!PayloadResult)
    {
        return std::unexpected(PayloadResult.error().Message);
    }

    for (NodePayload& Root : PayloadResult->Nodes)
    {
        auto PrefixResult = PrefixNodeNameInPayload(Root);
        if (!PrefixResult)
        {
            return PrefixResult;
        }
    }

    std::vector<uint8_t> MigratedBytes{};
    auto SerializeResult = SerializeLevelPayload(*PayloadResult, MigratedBytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error().Message);
    }

    InOutBytes = std::move(MigratedBytes);
    return {};
}

std::expected<void, std::string> MigrateWorldPayloadBaseNodeName(std::vector<uint8_t>& InOutBytes)
{
    auto PayloadResult = DeserializeWorldPayload(InOutBytes.data(), InOutBytes.size());
    if (!PayloadResult)
    {
        return std::unexpected(PayloadResult.error().Message);
    }

    for (NodePayload& Root : PayloadResult->Nodes)
    {
        auto PrefixResult = PrefixNodeNameInPayload(Root);
        if (!PrefixResult)
        {
            return PrefixResult;
        }
    }

    std::vector<uint8_t> MigratedBytes{};
    auto SerializeResult = SerializeWorldPayload(*PayloadResult, MigratedBytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error().Message);
    }

    InOutBytes = std::move(MigratedBytes);
    return {};
}

#if defined(SNAPI_GF_ENABLE_RENDERER)
using SnAPI::Graphics::EVertexStream;
using SnAPI::Graphics::GetStreamByIndex;
using SnAPI::Graphics::GetStreamInfo;
using SnAPI::Graphics::HasStream;
using SnAPI::Graphics::IVertexStreamSource;
using SnAPI::Graphics::Material;
using SnAPI::Graphics::MaterialDomain;
using SnAPI::Graphics::MaterialInstance;
using SnAPI::Graphics::MaterialRuntimeDescriptor;
using SnAPI::Graphics::MaterialRuntimeParameterDesc;
using SnAPI::Graphics::MaterialRuntimeResourceDesc;
using MeshBulkLoadCallback = std::function<std::expected<std::vector<uint8_t>, std::string>(uint32_t)>;

constexpr uint64_t kFnv1aOffset = 1469598103934665603ull;
constexpr uint64_t kFnv1aPrime = 1099511628211ull;

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
    std::vector<SnAPI::Graphics::VertexSourceSubMesh> SubMeshes{};
    std::vector<SnAPI::Graphics::VertexSourceMaterial> Materials{};
    uint64_t SourceId = 0;
    uint64_t SourceRevision = 0;
};

uint64_t HashBytes64(const void* Data, const size_t Size, const uint64_t Seed = kFnv1aOffset)
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

uint64_t HashString64(const std::string_view Value, const uint64_t Seed = kFnv1aOffset)
{
    return HashBytes64(Value.data(), Value.size(), Seed);
}

[[nodiscard]] std::string PointerKey(const void* Address)
{
    std::ostringstream Stream{};
    Stream << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(Address);
    return Stream.str();
}

[[nodiscard]] std::optional<EVertexStream> ToRendererStream(const EMeshStreamSemantic Semantic)
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

[[nodiscard]] const MeshStreamChunkRef* FindStreamRef(const StaticMeshPayload& RuntimeMesh, const EMeshStreamSemantic Semantic)
{
    const auto It = std::ranges::find_if(RuntimeMesh.Streams, [Semantic](const MeshStreamChunkRef& StreamRef) {
        return StreamRef.Semantic == Semantic;
    });
    return (It == RuntimeMesh.Streams.end()) ? nullptr : &(*It);
}

bool LoadRawStreamBytes(const MeshBulkLoadCallback& LoadBulk, const MeshStreamChunkRef& StreamRef, RuntimeStreamBuffer& Out)
{
    if (!LoadBulk || StreamRef.ElementCount == 0 || StreamRef.StrideBytes == 0)
    {
        return false;
    }

    auto BulkResult = LoadBulk(StreamRef.BulkIndex);
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

bool BuildRuntimeMeshSourceData(
    const StaticMeshPayload& RuntimeMesh,
    const MeshBulkLoadCallback& LoadBulk,
    const std::string_view StableKey,
    RuntimeMeshSourceData& Out)
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
    if (!LoadRawStreamBytes(LoadBulk, *PositionRef, PositionStream)
        || !LoadRawStreamBytes(LoadBulk, *IndexRef, IndexStream))
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

    const auto TryReadPosition = [&Out](const uint32_t VertexIndex, SnAPI::Vector3DF& OutPosition) -> bool
    {
        const auto PositionIt = Out.Streams.find(EVertexStream::Position);
        if (PositionIt == Out.Streams.end())
        {
            return false;
        }

        const RuntimeStreamBuffer& PositionBuffer = PositionIt->second;
        if (VertexIndex >= PositionBuffer.ElementCount || PositionBuffer.StrideBytes < sizeof(float) * 3u)
        {
            return false;
        }

        const size_t ByteOffset = static_cast<size_t>(VertexIndex) * static_cast<size_t>(PositionBuffer.StrideBytes);
        if (ByteOffset + (sizeof(float) * 3u) > PositionBuffer.Bytes.size())
        {
            return false;
        }

        std::array<float, 3> Position{};
        std::memcpy(Position.data(), PositionBuffer.Bytes.data() + ByteOffset, sizeof(float) * 3u);
        OutPosition = {Position[0], Position[1], Position[2]};
        return true;
    };

    const auto ComputeSubMeshBounds =
        [&Out, &TryReadPosition](const uint32_t IndexOffset, const uint32_t IndexCount, SnAPI::Vector3DF& OutMin, SnAPI::Vector3DF& OutMax) -> bool
    {
        if (IndexCount == 0 || static_cast<size_t>(IndexOffset) + static_cast<size_t>(IndexCount) > Out.Indices.size())
        {
            return false;
        }

        OutMin = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
        OutMax = {
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max()};

        for (uint32_t RelativeIndex = 0; RelativeIndex < IndexCount; ++RelativeIndex)
        {
            const uint32_t VertexIndex = Out.Indices[static_cast<size_t>(IndexOffset) + RelativeIndex];
            SnAPI::Vector3DF Position{};
            if (!TryReadPosition(VertexIndex, Position))
            {
                return false;
            }

            OutMin = OutMin.cwiseMin(Position);
            OutMax = OutMax.cwiseMax(Position);
        }

        return true;
    };

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
        if (!LoadRawStreamBytes(LoadBulk, *StreamRef, Stream))
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
        SnAPI::Graphics::VertexSourceSubMesh SubMesh{};
        SubMesh.IndexOffset = RuntimeSubMesh.IndexOffset;
        SubMesh.IndexCount = RuntimeSubMesh.IndexCount;
        SubMesh.MaterialSlot = RuntimeSubMesh.MaterialSlot;

        if (!ComputeSubMeshBounds(SubMesh.IndexOffset, SubMesh.IndexCount, SubMesh.BoundingBoxMin, SubMesh.BoundingBoxMax))
        {
            SubMesh.BoundingBoxMin = {
                RuntimeSubMesh.BoundsMin[0],
                RuntimeSubMesh.BoundsMin[1],
                RuntimeSubMesh.BoundsMin[2]};
            SubMesh.BoundingBoxMax = {
                RuntimeSubMesh.BoundsMax[0],
                RuntimeSubMesh.BoundsMax[1],
                RuntimeSubMesh.BoundsMax[2]};
        }

        Out.SubMeshes.push_back(SubMesh);
        MaterialSlotCount = std::max(MaterialSlotCount, RuntimeSubMesh.MaterialSlot + 1);
    }

    if (Out.SubMeshes.empty())
    {
        SnAPI::Graphics::VertexSourceSubMesh SubMesh{};
        SubMesh.IndexOffset = 0;
        SubMesh.IndexCount = static_cast<uint32_t>(Out.Indices.size());
        SubMesh.MaterialSlot = 0;
        (void)ComputeSubMeshBounds(SubMesh.IndexOffset, SubMesh.IndexCount, SubMesh.BoundingBoxMin, SubMesh.BoundingBoxMax);
        Out.SubMeshes.push_back(SubMesh);
        MaterialSlotCount = 1;
    }

    MaterialSlotCount = std::max(MaterialSlotCount, static_cast<uint32_t>(RuntimeMesh.MaterialInstances.size()));
    MaterialSlotCount = std::max(MaterialSlotCount, 1u);
    Out.Materials.resize(MaterialSlotCount);
    for (uint32_t Slot = 0; Slot < MaterialSlotCount; ++Slot)
    {
        SnAPI::Graphics::VertexSourceMaterial MaterialRecord{};
        if (Slot < RuntimeMesh.MaterialInstances.size())
        {
            const auto& Ref = RuntimeMesh.MaterialInstances[Slot];
            const std::string Name = !Ref.GetAssetName().empty() ? Ref.GetAssetName() : Ref.GetAssetId();
            if (!Name.empty())
            {
                MaterialRecord.Name = Name;
            }
        }

        if (MaterialRecord.Name.empty())
        {
            MaterialRecord.Name = "MaterialSlot_" + std::to_string(Slot);
        }

        Out.Materials[Slot] = std::move(MaterialRecord);
    }

    Out.DebugName = RuntimeMesh.Name.empty() ? "RuntimeMesh" : RuntimeMesh.Name;
    if (!StableKey.empty())
    {
        Out.DebugName += " [" + std::string(StableKey) + "]";
    }

    const std::string SourceKey = !StableKey.empty()
        ? std::string(StableKey)
        : (RuntimeMesh.Name.empty() ? PointerKey(&RuntimeMesh) : RuntimeMesh.Name);
    Out.SourceId = HashString64(SourceKey);

    uint64_t Revision = HashString64(Out.DebugName, Out.SourceId);
    Revision = HashBytes64(&Out.VertexCount, sizeof(Out.VertexCount), Revision);
    for (uint32_t Index = 0; Index < SnAPI::Graphics::kStreamCount; ++Index)
    {
        const EVertexStream Stream = GetStreamByIndex(Index);
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
    for (const SnAPI::Graphics::VertexSourceSubMesh& SubMesh : Out.SubMeshes)
    {
        Revision = HashBytes64(&SubMesh, sizeof(SnAPI::Graphics::VertexSourceSubMesh), Revision);
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

    [[nodiscard]] bool SubMesh(const uint32_t SubMeshIndex, SnAPI::Graphics::VertexSourceSubMesh& OutSubMesh) const override
    {
        if (SubMeshIndex >= m_data.SubMeshes.size())
        {
            return false;
        }

        OutSubMesh = m_data.SubMeshes[SubMeshIndex];
        return true;
    }

    [[nodiscard]] bool Material(const uint32_t MaterialSlot, SnAPI::Graphics::VertexSourceMaterial& OutMaterial) const override
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

[[nodiscard]] std::string ToLowerCopy(const std::string_view Text)
{
    std::string Value(Text);
    std::transform(Value.begin(), Value.end(), Value.begin(), [](const unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });
    return Value;
}

bool EqualsIgnoreCase(const std::string_view Left, const std::string_view Right)
{
    if (Left.size() != Right.size())
    {
        return false;
    }

    for (size_t Index = 0; Index < Left.size(); ++Index)
    {
        if (static_cast<char>(std::tolower(static_cast<unsigned char>(Left[Index])))
            != static_cast<char>(std::tolower(static_cast<unsigned char>(Right[Index]))))
        {
            return false;
        }
    }
    return true;
}

bool TryParseFloatValue(const std::string_view Text, float& OutValue)
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

[[nodiscard]] const SnAPI::Graphics::ShaderMetaData::UserAttribute* FindParameterDefaultAttribute(
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

bool TryReadAttributeNumber(
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

bool TryResolveAttributeScalarDefault(const MaterialRuntimeParameterDesc& Parameter, float& OutValue)
{
    const auto* Attribute = FindParameterDefaultAttribute(Parameter);
    if (!Attribute)
    {
        return false;
    }
    return TryReadAttributeNumber(*Attribute, 0, OutValue);
}

bool TryResolveAttributeVectorDefault(
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

void CollectParameterLookupKeys(const MaterialRuntimeParameterDesc& Parameter, std::vector<std::string>& OutKeys)
{
    OutKeys.clear();
    OutKeys.reserve(6);

    auto AppendKey = [&OutKeys](const std::string_view Key) {
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

    AppendKey(Parameter.Name);
    if (const size_t DotIndex = Parameter.Name.rfind('.'); DotIndex != std::string::npos)
    {
        AppendKey(std::string_view(Parameter.Name).substr(DotIndex + 1));
    }

    if (const auto* DefaultAttribute = FindParameterDefaultAttribute(Parameter))
    {
        for (const std::string& Argument : DefaultAttribute->Arguments)
        {
            AppendKey(Argument);
        }
    }
}

bool TryResolveScalarValue(
    const MaterialRuntimeParameterDesc& Parameter,
    const std::unordered_map<std::string, float>& Scalars,
    float& OutValue)
{
    std::vector<std::string> Keys{};
    CollectParameterLookupKeys(Parameter, Keys);
    for (const std::string& Key : Keys)
    {
        if (const auto It = Scalars.find(Key); It != Scalars.end())
        {
            OutValue = It->second;
            return true;
        }
    }

    return TryResolveAttributeScalarDefault(Parameter, OutValue);
}

bool TryResolveVectorValue(
    const MaterialRuntimeParameterDesc& Parameter,
    const std::unordered_map<std::string, std::array<float, 4>>& Vectors,
    std::array<float, 4>& OutValue)
{
    std::vector<std::string> Keys{};
    CollectParameterLookupKeys(Parameter, Keys);
    for (const std::string& Key : Keys)
    {
        if (const auto It = Vectors.find(Key); It != Vectors.end())
        {
            OutValue = It->second;
            return true;
        }
    }

    const size_t ComponentCount = [&Parameter]() -> size_t {
        switch (Parameter.eValueType)
        {
        case SnAPI::Graphics::ShaderMetaData::EValueType::Float2:
            return 2;
        case SnAPI::Graphics::ShaderMetaData::EValueType::Float3:
            return 3;
        case SnAPI::Graphics::ShaderMetaData::EValueType::Float4:
            return 4;
        default:
            return 0;
        }
    }();

    return TryResolveAttributeVectorDefault(Parameter, ComponentCount, OutValue);
}

[[nodiscard]] bool HasAssetReference(const AssetRefPayload& Ref)
{
    return !Ref.AssetName.empty() || !Ref.AssetId.empty();
}

void CollectResourceLookupKeys(const MaterialRuntimeResourceDesc& Resource, std::vector<std::string>& OutKeys)
{
    OutKeys.clear();
    OutKeys.reserve(3);

    auto AppendKey = [&OutKeys](const std::string_view Key) {
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

[[nodiscard]] MaterialDomain DomainFromShadingModelName(const std::string_view ShadingModel)
{
    if (ShadingModel == "GBufferShadingModel")
    {
        return MaterialDomain::GBuffer;
    }
    if (ShadingModel == "ShadowShadingModel")
    {
        return MaterialDomain::ShadowCaster;
    }
    if (ShadingModel == "UIShadingModel")
    {
        return MaterialDomain::UI;
    }
    if (ShadingModel == "PostProcessShadingModel")
    {
        return MaterialDomain::PostProcess;
    }
    if (ShadingModel == "DeferredShadingShadingModel")
    {
        return MaterialDomain::DeferredLit;
    }
    return MaterialDomain::GBuffer;
}

template<typename TValue>
bool WriteParameterValue(
    std::vector<uint8_t>& BufferBytes,
    const MaterialRuntimeParameterDesc& Parameter,
    const TValue& Value)
{
    if (Parameter.OffsetBytes + sizeof(TValue) > BufferBytes.size())
    {
        return false;
    }

    std::memcpy(BufferBytes.data() + Parameter.OffsetBytes, &Value, sizeof(TValue));
    return true;
}

bool ApplyParameterToBuffer(
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
            const std::array<float, 2> Value{VectorValue[0], VectorValue[1]};
            return WriteParameterValue(BufferBytes, Parameter, Value);
        }
    case SnAPI::Graphics::ShaderMetaData::EValueType::Float3:
        {
            std::array<float, 4> VectorValue{};
            if (!TryResolveVectorValue(Parameter, Vectors, VectorValue))
            {
                return false;
            }
            const std::array<float, 3> Value{VectorValue[0], VectorValue[1], VectorValue[2]};
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

void ApplyDescriptorBuffersToMaterialInstance(
    MaterialInstance& TargetInstance,
    const MaterialRuntimeDescriptor& Descriptor,
    const MaterialInstanceAsset& RuntimeMaterialInstance)
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
        if (BufferDesc.Name == std::string(SnAPI::Graphics::GBufferContract::ParamBlockName)
            && BufferSize >= sizeof(SnAPI::Graphics::GBufferContract::ParamBlock))
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
                    return Resource.Name == BufferDesc.Name
                        && Resource.eBindingType == SnAPI::Graphics::ShaderMetaData::EBindingType::Buffer;
                });
            ResourceIt != Descriptor.Resources.end()
            && ResourceIt->eDescriptorType == SnAPI::Graphics::EDescriptorType::StorageBuffer)
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

void ApplyDescriptorTexturesToMaterialInstance(
    MaterialInstance& TargetInstance,
    const MaterialRuntimeDescriptor& Descriptor,
    const MaterialInstanceAsset& RuntimeMaterialInstance,
    ::SnAPI::AssetPipeline::AssetManager& AssetManager)
{
    const size_t TextureCount = RuntimeMaterialInstance.Textures.size();
    if (TextureCount == 0)
    {
        return;
    }

    std::unordered_map<std::string, size_t> TextureIndexBySlot{};
    TextureIndexBySlot.reserve(TextureCount);
    for (size_t Index = 0; Index < TextureCount; ++Index)
    {
        const MaterialTextureParamPayload& TextureParam = RuntimeMaterialInstance.Textures[Index];
        if (!HasAssetReference(TextureParam.Texture))
        {
            continue;
        }

        const std::string SlotKey = ToLowerCopy(TextureParam.SlotName);
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

        const MaterialTextureParamPayload& TextureParam = RuntimeMaterialInstance.Textures[*TextureIndex];
        TAssetRef<TextureAsset> TextureRef(TextureParam.Texture.AssetName, TextureParam.Texture.AssetId);
        auto TextureResult = TextureRef.GetRuntimeShared<SnAPI::Graphics::IGPUImage>(AssetManager);
        if (!TextureResult || !*TextureResult)
        {
            continue;
        }

        TargetInstance.Texture(Resource.Name, TextureResult->get());
    }
}

std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> BuildSharedRuntimeMeshStreamSource(
    const StaticMeshPayload& RuntimeMesh,
    MeshBulkLoadCallback LoadBulk,
    const std::string_view StableKey)
{
    RuntimeMeshSourceData SourceData{};
    if (!BuildRuntimeMeshSourceData(RuntimeMesh, LoadBulk, StableKey, SourceData))
    {
        return {};
    }

    return std::make_shared<RuntimeMeshVertexStreamSource>(std::move(SourceData));
}

std::shared_ptr<StaticMeshRuntime> BuildSharedRuntimeStaticMesh(
    const StaticMeshPayload& RuntimeMesh,
    MeshBulkLoadCallback LoadBulk,
    const std::string_view StableKey)
{
    auto Runtime = std::make_shared<StaticMeshRuntime>();
    if (!Runtime)
    {
        return {};
    }

    Runtime->StreamSource = BuildSharedRuntimeMeshStreamSource(RuntimeMesh, std::move(LoadBulk), StableKey);
    if (!Runtime->StreamSource)
    {
        return {};
    }

    Runtime->MaterialRefs = RuntimeMesh.MaterialInstances;
    return Runtime;
}

std::shared_ptr<SkeletalMeshRuntime> BuildSharedRuntimeSkeletalMesh(
    const SkeletalMeshPayload& RuntimeMesh,
    MeshBulkLoadCallback LoadBulk,
    const std::string_view StableKey)
{
    auto Runtime = std::make_shared<SkeletalMeshRuntime>();
    if (!Runtime)
    {
        return {};
    }

    Runtime->StreamSource = BuildSharedRuntimeMeshStreamSource(RuntimeMesh.BaseMesh, std::move(LoadBulk), StableKey);
    if (!Runtime->StreamSource)
    {
        return {};
    }

    Runtime->MaterialRefs = RuntimeMesh.BaseMesh.MaterialInstances;
    return Runtime;
}

std::shared_ptr<SnAPI::Graphics::Material> BuildSharedRuntimeMaterial(
    const MaterialAsset& MaterialPayload,
    const std::string_view StableKey)
{
    (void)StableKey;

    if (MaterialPayload.ShaderModule.empty())
    {
        return {};
    }

    if (MaterialPayload.ShadingModel == "GBufferShadingModel")
    {
        auto RuntimeMaterial = std::make_shared<SnAPI::Graphics::GBufferMaterial>(MaterialPayload.ShaderModule);
        RuntimeMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlbedoMap, MaterialPayload.FeatureAlbedoMap);
        RuntimeMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::NormalMap, MaterialPayload.FeatureNormalMap);
        RuntimeMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::RoughnessMap, MaterialPayload.FeatureRoughnessMap);
        RuntimeMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::MetalnessMap, MaterialPayload.FeatureMetalnessMap);
        RuntimeMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::OcclusionMap, MaterialPayload.FeatureOcclusionMap);
        RuntimeMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlphaTest, MaterialPayload.FeatureAlphaTest);
        RuntimeMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlphaBlend, MaterialPayload.FeatureAlphaBlend);
        RuntimeMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::DoubleSided, MaterialPayload.FeatureDoubleSided);
        RuntimeMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::Instancing, MaterialPayload.FeatureInstancing);
        RuntimeMaterial->BakeCompileTimeParams();
        return RuntimeMaterial;
    }

    if (MaterialPayload.ShadingModel == "ShadowShadingModel")
    {
        auto RuntimeMaterial = std::make_shared<SnAPI::Graphics::ShadowMaterial>(MaterialPayload.ShaderModule);
        RuntimeMaterial->BakeCompileTimeParams();
        return RuntimeMaterial;
    }

    auto RuntimeMaterial = std::make_shared<Material>(
        MaterialPayload.ShaderModule,
        MaterialPayload.ShadingModel,
        DomainFromShadingModelName(MaterialPayload.ShadingModel));
    RuntimeMaterial->BakeAndCompile();
    return RuntimeMaterial;
}

std::shared_ptr<SnAPI::Graphics::MaterialInstance> BuildSharedRuntimeMaterialInstance(
    const MaterialInstanceAsset& MaterialPayload,
    const std::string_view StableKey,
    ::SnAPI::AssetPipeline::AssetManager& AssetManager)
{
    (void)StableKey;

    if (!HasAssetReference(MaterialPayload.ParentMaterial))
    {
        return {};
    }

    TAssetRef<MaterialAsset> ParentMaterialRef(MaterialPayload.ParentMaterial.AssetName, MaterialPayload.ParentMaterial.AssetId);
    auto ParentMaterialResult = ParentMaterialRef.GetRuntimeShared<SnAPI::Graphics::Material>(AssetManager);
    if (!ParentMaterialResult || !*ParentMaterialResult)
    {
        return {};
    }

    auto Created = (*ParentMaterialResult)->CreateMaterialInstance();
    if (!Created)
    {
        return {};
    }

    const MaterialRuntimeDescriptor Descriptor = SnAPI::Graphics::BuildMaterialRuntimeDescriptor(*Created);
    ApplyDescriptorBuffersToMaterialInstance(*Created, Descriptor, MaterialPayload);
    ApplyDescriptorTexturesToMaterialInstance(*Created, Descriptor, MaterialPayload, AssetManager);
    return Created;
}
#endif

/**
 * @brief AssetFactory for Node runtime objects.
 */
class TNodeFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<BaseNode>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadNode();
    }

protected:
    std::expected<BaseNode, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<NodePayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        BaseNode Loaded(PayloadResult->Name);
        Loaded.TypeKey(PayloadResult->NodeType);

        const auto* Params = std::any_cast<NodeAssetLoadParams>(&Context.Params);
        if (Params && Params->TargetWorld)
        {
            TDeserializeOptions DeserializeOptions{};
            DeserializeOptions.RegenerateObjectIds = Params->InstantiateAsCopy;
            auto DeserializeResult = NodeSerializer::Deserialize(
                *PayloadResult,
                *Params->TargetWorld,
                Params->Parent,
                DeserializeOptions);
            if (!DeserializeResult)
            {
                return std::unexpected(DeserializeResult.error().Message);
            }

            if (Params->OutCreatedRoot)
            {
                *Params->OutCreatedRoot = *DeserializeResult;
            }

            if (BaseNode* CreatedNode = DeserializeResult->Borrowed())
            {
                Loaded.Name(CreatedNode->Name());
                Loaded.TypeKey(CreatedNode->TypeKey());
                Loaded.Active(CreatedNode->Active());
                Loaded.Replicated(CreatedNode->Replicated());
            }
        }

        return Loaded;
    }
};

/**
 * @brief AssetFactory for Level runtime objects.
 */
class TLevelFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<Level>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadLevel();
    }

protected:
    std::expected<Level, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<LevelPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        const auto* Params = std::any_cast<LevelAssetLoadParams>(&Context.Params);
        const std::string ResolvedName = (Params && !Params->NameOverride.empty())
            ? Params->NameOverride
            : (PayloadResult->Name.empty() ? Context.Info.Name : PayloadResult->Name);

        if (Params && Params->TargetWorld)
        {
            auto CreateResult = Params->TargetWorld->CreateLevel(ResolvedName);
            if (!CreateResult)
            {
                return std::unexpected(CreateResult.error().Message);
            }

            if (Params->OutCreatedLevel)
            {
                *Params->OutCreatedLevel = *CreateResult;
            }

            auto* CreatedLevel = NodeCast<Level>(CreateResult->Borrowed());
            if (!CreatedLevel)
            {
                return std::unexpected("Failed to resolve created level node");
            }

            TDeserializeOptions DeserializeOptions{};
            DeserializeOptions.RegenerateObjectIds = Params->InstantiateAsCopy;
            auto DeserializeResult = LevelSerializer::Deserialize(*PayloadResult, *CreatedLevel, DeserializeOptions);
            if (!DeserializeResult)
            {
                return std::unexpected(DeserializeResult.error().Message);
            }
        }

        return Level(ResolvedName);
    }
};

/**
 * @brief AssetFactory for World runtime objects.
 */
class TWorldFactory final : public ::SnAPI::AssetPipeline::IAssetFactory
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadWorld();
    }

    std::expected<::SnAPI::AssetPipeline::UniqueVoidPtr, std::string> Load(
        const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<WorldPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        auto* LoadedWorld = new World();
        const auto* Params = std::any_cast<WorldAssetLoadParams>(&Context.Params);
        if (Params && Params->TargetWorld)
        {
            TDeserializeOptions DeserializeOptions{};
            DeserializeOptions.RegenerateObjectIds = Params->InstantiateAsCopy;
            auto DeserializeIntoTarget = WorldSerializer::Deserialize(*PayloadResult, *Params->TargetWorld, DeserializeOptions);
            if (!DeserializeIntoTarget)
            {
                delete LoadedWorld;
                return std::unexpected(DeserializeIntoTarget.error().Message);
            }
            return ::SnAPI::AssetPipeline::UniqueVoidPtr(LoadedWorld, [](void* Ptr) {
                delete static_cast<World*>(Ptr);
            });
        }

        auto DeserializeResult = WorldSerializer::Deserialize(*PayloadResult, *LoadedWorld);
        if (!DeserializeResult)
        {
            delete LoadedWorld;
            return std::unexpected(DeserializeResult.error().Message);
        }
        return ::SnAPI::AssetPipeline::UniqueVoidPtr(LoadedWorld, [](void* Ptr) {
            delete static_cast<World*>(Ptr);
        });
    }
};

/**
 * @brief AssetFactory for authored Conduit graph assets.
 */
class TConduitGraphFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<Conduit::GraphAsset>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadConduitGraph();
    }

protected:
    std::expected<Conduit::GraphAsset, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<Conduit::GraphAsset>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        return std::move(*PayloadResult);
    }
};

/**
 * @brief AssetFactory for authored Conduit class assets.
 */
class TConduitClassFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<Conduit::ClassAsset>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadConduitClass();
    }

protected:
    std::expected<Conduit::ClassAsset, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<Conduit::ClassAsset>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        return std::move(*PayloadResult);
    }
};

#if defined(SNAPI_GF_ENABLE_RENDERER)
[[nodiscard]] std::optional<::SnAPI::Graphics::ETextureFormat> ToRendererTextureFormat(
    const TextureCompressorPlugin::ECompressedFormat Format)
{
    using TextureCompressorPlugin::ECompressedFormat;
    using ::SnAPI::Graphics::ETextureFormat;

    switch (Format)
    {
    case ECompressedFormat::BC1:
        return ETextureFormat::BC1_Unorm;
    case ECompressedFormat::BC3:
        return ETextureFormat::BC3_Unorm;
    case ECompressedFormat::BC4:
        return ETextureFormat::BC4_Unorm;
    case ECompressedFormat::BC5:
        return ETextureFormat::BC5_Unorm;
    case ECompressedFormat::BC6H:
        return ETextureFormat::BC6H_UFloat;
    case ECompressedFormat::BC7:
        return ETextureFormat::BC7_Unorm;
    case ECompressedFormat::ASTC_4x4:
        return ETextureFormat::ASTC_4x4_Unorm;
    case ECompressedFormat::ASTC_5x5:
        return ETextureFormat::ASTC_5x5_Unorm;
    case ECompressedFormat::ASTC_6x6:
        return ETextureFormat::ASTC_6x6_Unorm;
    case ECompressedFormat::ASTC_8x8:
        return ETextureFormat::ASTC_8x8_Unorm;
    case ECompressedFormat::ASTC_10x10:
        return ETextureFormat::ASTC_10x10_Unorm;
    case ECompressedFormat::ASTC_12x12:
        return ETextureFormat::ASTC_12x12_Unorm;
    case ECompressedFormat::ASTC_4x4_HDR:
        return ETextureFormat::ASTC_4x4_SFloat;
    case ECompressedFormat::ASTC_6x6_HDR:
        return ETextureFormat::ASTC_6x6_SFloat;
    case ECompressedFormat::ASTC_8x8_HDR:
        return ETextureFormat::ASTC_8x8_SFloat;
    case ECompressedFormat::Unknown:
    default:
        return std::nullopt;
    }
}

/**
 * @brief AssetFactory for compressed texture GPU images.
 */
class TCompressedTextureImageFactory final : public ::SnAPI::AssetPipeline::IAssetFactory
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return TextureCompressorPlugin::Payload_CompressorCookedInfo;
    }

    std::expected<::SnAPI::AssetPipeline::UniqueVoidPtr, std::string> Load(
        const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto CookedInfo = Context.DeserializeCooked<TextureCompressorPlugin::TextureCompressorCookedInfo>();
        if (!CookedInfo)
        {
            return std::unexpected(CookedInfo.error());
        }

        const auto RendererFormat = ToRendererTextureFormat(CookedInfo->Format);
        if (!RendererFormat.has_value())
        {
            return std::unexpected("Unsupported compressed texture format for renderer image upload");
        }

        auto* GraphicsAPI = ::SnAPI::Graphics::IGraphicsAPI::Instance();
        if (!GraphicsAPI)
        {
            return std::unexpected("GraphicsAPI is not initialized");
        }

        uint32_t MipsToLoad = CookedInfo->MipCount;
        if (const auto* LoadParams = std::any_cast<TextureCompressorPlugin::TextureCompressorLoadParams>(&Context.Params))
        {
            if (LoadParams->MaxMipLevel >= 0)
            {
                MipsToLoad = std::min<uint32_t>(MipsToLoad, static_cast<uint32_t>(LoadParams->MaxMipLevel));
            }
        }

        MipsToLoad = std::min<uint32_t>(MipsToLoad, static_cast<uint32_t>(CookedInfo->MipLevels.size()));
        if (MipsToLoad == 0)
        {
            return std::unexpected("Compressed texture has no mips to load");
        }

        std::vector<uint8_t> TextureBytes{};
        std::vector<uint32_t> MipByteSizes{};
        size_t EstimatedTextureBytes = 0;
        for (uint32_t MipIndex = 0; MipIndex < MipsToLoad; ++MipIndex)
        {
            EstimatedTextureBytes += CookedInfo->MipLevels[MipIndex].CompressedSize;
        }
        TextureBytes.reserve(EstimatedTextureBytes);
        MipByteSizes.reserve(MipsToLoad);

        for (uint32_t MipIndex = 0; MipIndex < MipsToLoad; ++MipIndex)
        {
            auto BulkResult = Context.LoadBulk(MipIndex);
            if (!BulkResult)
            {
                return std::unexpected("Failed to load compressed texture mip bulk chunk " + std::to_string(MipIndex) + ": " + BulkResult.error());
            }

            const uint32_t ByteSize = static_cast<uint32_t>(BulkResult->size());
            if (ByteSize == 0)
            {
                return std::unexpected("Compressed texture mip bulk chunk is empty at index " + std::to_string(MipIndex));
            }

            MipByteSizes.push_back(ByteSize);
            const size_t ExistingSize = TextureBytes.size();
            TextureBytes.resize(ExistingSize + BulkResult->size());
            std::memcpy(TextureBytes.data() + ExistingSize, BulkResult->data(), BulkResult->size());
        }

        ::SnAPI::Graphics::ImageCreateInfo ImageCI = ::SnAPI::Graphics::ImageCreateInfo::VisualDefault(
            {CookedInfo->BaseWidth, CookedInfo->BaseHeight},
            *RendererFormat,
            MipsToLoad);
        ImageCI.Data = std::move(TextureBytes);
        ImageCI.MipByteSizes = std::move(MipByteSizes);
        ImageCI.EnableAnisotropy = true;

        auto Image = GraphicsAPI->CreateImage2D(ImageCI);
        if (!Image)
        {
            return std::unexpected("GraphicsAPI failed to create texture image");
        }

        return ::SnAPI::AssetPipeline::UniqueVoidPtr(
            Image.release(),
            [](void* Ptr) {
                delete static_cast<::SnAPI::Graphics::IGPUImage*>(Ptr);
            });
    }
};

class TSharedMaterialFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<std::shared_ptr<::SnAPI::Graphics::Material>>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadMaterial();
    }

protected:
    std::expected<std::shared_ptr<::SnAPI::Graphics::Material>, std::string> DoLoad(
        const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<MaterialAsset>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        const std::string StableKey = !Context.Info.Id.IsNull()
            ? Context.Info.Id.ToString()
            : Context.Info.Name;
        auto RuntimeMaterial = BuildSharedRuntimeMaterial(*PayloadResult, StableKey);
        if (!RuntimeMaterial)
        {
            return std::unexpected("Failed to build shared runtime material");
        }
        return RuntimeMaterial;
    }
};

class TSharedMaterialInstanceFactory final
    : public ::SnAPI::AssetPipeline::TAssetFactory<std::shared_ptr<::SnAPI::Graphics::MaterialInstance>>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadMaterialInstance();
    }

protected:
    std::expected<std::shared_ptr<::SnAPI::Graphics::MaterialInstance>, std::string> DoLoad(
        const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        if (!Context.Manager)
        {
            return std::unexpected("Asset manager is required for shared material-instance loading");
        }

        auto PayloadResult = Context.DeserializeCooked<MaterialInstanceAsset>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        const std::string StableKey = !Context.Info.Id.IsNull()
            ? Context.Info.Id.ToString()
            : Context.Info.Name;
        auto RuntimeMaterialInstance = BuildSharedRuntimeMaterialInstance(*PayloadResult, StableKey, *Context.Manager);
        if (!RuntimeMaterialInstance)
        {
            return std::unexpected("Failed to build shared runtime material instance");
        }
        return RuntimeMaterialInstance;
    }
};

class TSharedStaticMeshSourceFactory final
    : public ::SnAPI::AssetPipeline::TAssetFactory<std::shared_ptr<::SnAPI::Graphics::IVertexStreamSource>>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadStaticMesh();
    }

protected:
    std::expected<std::shared_ptr<::SnAPI::Graphics::IVertexStreamSource>, std::string> DoLoad(
        const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<StaticMeshPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        const std::string StableKey = !Context.Info.Id.IsNull()
            ? "asset-id://" + Context.Info.Id.ToString()
            : (!Context.Info.Name.empty() ? "asset-name://" + Context.Info.Name : PayloadResult->Name);
        auto RuntimeSource = BuildSharedRuntimeMeshStreamSource(*PayloadResult, Context.LoadBulk, StableKey);
        if (!RuntimeSource)
        {
            return std::unexpected("Failed to build shared runtime vertex stream source");
        }
        return RuntimeSource;
    }
};

class TSharedStaticMeshRuntimeFactory final
    : public ::SnAPI::AssetPipeline::TAssetFactory<std::shared_ptr<StaticMeshRuntime>>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadStaticMesh();
    }

protected:
    std::expected<std::shared_ptr<StaticMeshRuntime>, std::string> DoLoad(
        const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<StaticMeshPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        const std::string StableKey = !Context.Info.Id.IsNull()
            ? "asset-id://" + Context.Info.Id.ToString()
            : (!Context.Info.Name.empty() ? "asset-name://" + Context.Info.Name : PayloadResult->Name);
        auto RuntimeMesh = BuildSharedRuntimeStaticMesh(*PayloadResult, Context.LoadBulk, StableKey);
        if (!RuntimeMesh)
        {
            return std::unexpected("Failed to build shared runtime static mesh");
        }
        return RuntimeMesh;
    }
};

class TSharedSkeletalMeshSourceFactory final
    : public ::SnAPI::AssetPipeline::TAssetFactory<std::shared_ptr<::SnAPI::Graphics::IVertexStreamSource>>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadSkeletalMesh();
    }

protected:
    std::expected<std::shared_ptr<::SnAPI::Graphics::IVertexStreamSource>, std::string> DoLoad(
        const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<SkeletalMeshPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        const std::string StableKey = !Context.Info.Id.IsNull()
            ? "asset-id://" + Context.Info.Id.ToString()
            : (!Context.Info.Name.empty() ? "asset-name://" + Context.Info.Name : PayloadResult->BaseMesh.Name);
        auto RuntimeSource = BuildSharedRuntimeMeshStreamSource(PayloadResult->BaseMesh, Context.LoadBulk, StableKey);
        if (!RuntimeSource)
        {
            return std::unexpected("Failed to build shared runtime skeletal vertex stream source");
        }
        return RuntimeSource;
    }
};

class TSharedSkeletalMeshRuntimeFactory final
    : public ::SnAPI::AssetPipeline::TAssetFactory<std::shared_ptr<SkeletalMeshRuntime>>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadSkeletalMesh();
    }

protected:
    std::expected<std::shared_ptr<SkeletalMeshRuntime>, std::string> DoLoad(
        const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<SkeletalMeshPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        const std::string StableKey = !Context.Info.Id.IsNull()
            ? "asset-id://" + Context.Info.Id.ToString()
            : (!Context.Info.Name.empty() ? "asset-name://" + Context.Info.Name : PayloadResult->BaseMesh.Name);
        auto RuntimeMesh = BuildSharedRuntimeSkeletalMesh(*PayloadResult, Context.LoadBulk, StableKey);
        if (!RuntimeMesh)
        {
            return std::unexpected("Failed to build shared runtime skeletal mesh");
        }
        return RuntimeMesh;
    }
};
#endif

} // namespace

void RegisterAssetPipelinePayloads(::SnAPI::AssetPipeline::PayloadRegistry& Registry)
{
    Registry.Register(CreateNodePayloadSerializer());
    Registry.Register(CreateLevelPayloadSerializer());
    Registry.Register(CreateWorldPayloadSerializer());
    Registry.Register(CreateNodeSourcePayloadSerializer());
    Registry.Register(CreateLevelSourcePayloadSerializer());
    Registry.Register(CreateWorldSourcePayloadSerializer());
    Registry.Register(CreateConduitGraphPayloadSerializer());
    Registry.Register(CreateConduitClassPayloadSerializer());
    Registry.Register(CreateStaticMeshPayloadSerializer());
    Registry.Register(CreateSkeletalMeshPayloadSerializer());
    Registry.Register(CreateMaterialPayloadSerializer());
    Registry.Register(CreateMaterialInstancePayloadSerializer());
    Registry.Register(CreateSkeletonPayloadSerializer());
    Registry.Register(CreateAnimationPayloadSerializer());
    Registry.Register(CreateTextureSourcePayloadSerializer());
    Registry.Register(CreateStaticMeshSourcePayloadSerializer());
    Registry.Register(CreateSkeletalMeshSourcePayloadSerializer());
}

void RegisterAssetPipelineFactories(::SnAPI::AssetPipeline::AssetManager& Manager)
{
    Manager.RegisterPayloadMigration(PayloadNode(), 1u, NodeSerializer::kSchemaVersion, MigrateNodePayloadBaseNodeName);
    Manager.RegisterPayloadMigration(PayloadLevel(), 5u, LevelSerializer::kSchemaVersion, MigrateLevelPayloadBaseNodeName);
    Manager.RegisterPayloadMigration(PayloadWorld(), 5u, WorldSerializer::kSchemaVersion, MigrateWorldPayloadBaseNodeName);

    Manager.RegisterFactory<BaseNode>(std::make_unique<TNodeFactory>());
    Manager.RegisterFactory<Level>(std::make_unique<TLevelFactory>());
    Manager.RegisterFactory<World>(std::make_unique<TWorldFactory>());
    Manager.RegisterFactory<Conduit::GraphAsset>(std::make_unique<TConduitGraphFactory>());
    Manager.RegisterFactory<Conduit::ClassAsset>(std::make_unique<TConduitClassFactory>());
#if defined(SNAPI_GF_ENABLE_RENDERER)
    Manager.RegisterFactory<::SnAPI::Graphics::IGPUImage>(std::make_unique<TCompressedTextureImageFactory>());
    Manager.RegisterFactory<std::shared_ptr<::SnAPI::Graphics::Material>>(std::make_unique<TSharedMaterialFactory>());
    Manager.RegisterFactory<std::shared_ptr<::SnAPI::Graphics::MaterialInstance>>(std::make_unique<TSharedMaterialInstanceFactory>());
    Manager.RegisterFactory<std::shared_ptr<::SnAPI::Graphics::IVertexStreamSource>>(std::make_unique<TSharedStaticMeshSourceFactory>());
    Manager.RegisterFactory<std::shared_ptr<StaticMeshRuntime>>(std::make_unique<TSharedStaticMeshRuntimeFactory>());
    Manager.RegisterFactory<std::shared_ptr<::SnAPI::Graphics::IVertexStreamSource>>(std::make_unique<TSharedSkeletalMeshSourceFactory>());
    Manager.RegisterFactory<std::shared_ptr<SkeletalMeshRuntime>>(std::make_unique<TSharedSkeletalMeshRuntimeFactory>());
#endif
}

void RegisterAssetPipelineSourceStages(::SnAPI::AssetPipeline::AssetManager& Manager)
{
    Manager.RegisterSerializer(TextureCompressorPlugin::CreateCompressorImageIntermediateSerializer());
    Manager.RegisterSerializer(TextureCompressorPlugin::CreateCompressorCookedInfoSerializer());

    Manager.RegisterImporter(TextureCompressorPlugin::CreateTextureCompressorImporter());
    Manager.RegisterCooker(TextureCompressorPlugin::CreateTextureCompressorCooker());

    Manager.RegisterImporter(CreateAuthoredAssetJsonImporter());
    Manager.RegisterCooker(CreateAuthoredAssetPassThroughCooker());
    Manager.RegisterCooker(CreateNodeSourceCooker());
    Manager.RegisterCooker(CreateLevelSourceCooker());
    Manager.RegisterCooker(CreateWorldSourceCooker());

    Manager.RegisterImporter(CreateRenderAssetAssimpImporter());
    Manager.RegisterImporter(CreateRenderAssetJsonImporter());

    Manager.RegisterCooker(CreateRenderTextureCooker());
    Manager.RegisterCooker(CreateRenderMaterialCooker());
    Manager.RegisterCooker(CreateRenderMaterialInstanceCooker());
    Manager.RegisterCooker(CreateRenderSkeletonCooker());
    Manager.RegisterCooker(CreateRenderAnimationCooker());
    Manager.RegisterCooker(CreateRenderStaticMeshCooker());
    Manager.RegisterCooker(CreateRenderSkeletalMeshCooker());
}

} // namespace SnAPI::GameFramework
