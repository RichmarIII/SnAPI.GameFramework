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

using MeshBulkLoadCallback = std::function<std::expected<std::vector<uint8_t>, std::string>(uint32_t)>;

constexpr uint64_t kFnv1aOffset = 1469598103934665603ull;
constexpr uint64_t kFnv1aPrime = 1099511628211ull;

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

[[nodiscard]] const MeshStreamChunkRef* FindStreamRef(const StaticMeshPayload& RuntimeMesh, const EMeshStreamSemantic Semantic)
{
    const auto It = std::ranges::find_if(RuntimeMesh.Streams, [Semantic](const MeshStreamChunkRef& StreamRef) {
        return StreamRef.Semantic == Semantic;
    });
    return (It == RuntimeMesh.Streams.end()) ? nullptr : &(*It);
}

bool LoadRuntimeMeshStream(const MeshBulkLoadCallback& LoadBulk, const MeshStreamChunkRef& StreamRef, RuntimeMeshStream& Out)
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

    Out.Semantic = StreamRef.Semantic;
    Out.ElementCount = StreamRef.ElementCount;
    Out.StrideBytes = StreamRef.StrideBytes;
    Out.Bytes = std::move(*BulkResult);
    return true;
}

bool DecodeIndexStream(const RuntimeMeshStream& Stream, std::vector<uint32_t>& OutIndices)
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

[[nodiscard]] std::shared_ptr<RuntimeMeshData> BuildRuntimeMeshData(
    const StaticMeshPayload& RuntimeMesh,
    MeshBulkLoadCallback LoadBulk,
    const std::string_view StableKey)
{
    const MeshStreamChunkRef* PositionRef = FindStreamRef(RuntimeMesh, EMeshStreamSemantic::Position);
    const MeshStreamChunkRef* IndexRef = FindStreamRef(RuntimeMesh, EMeshStreamSemantic::Index);
    if (!PositionRef || !IndexRef)
    {
        return {};
    }

    auto MeshData = std::make_shared<RuntimeMeshData>();
    if (!MeshData)
    {
        return {};
    }

    RuntimeMeshStream PositionStream{};
    RuntimeMeshStream IndexStream{};
    if (!LoadRuntimeMeshStream(LoadBulk, *PositionRef, PositionStream)
        || !LoadRuntimeMeshStream(LoadBulk, *IndexRef, IndexStream))
    {
        return {};
    }

    MeshData->VertexCount = PositionStream.ElementCount;
    MeshData->Streams.push_back(std::move(PositionStream));
    if (!DecodeIndexStream(IndexStream, MeshData->Indices) || MeshData->Indices.empty())
    {
        return {};
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

        RuntimeMeshStream Stream{};
        if (LoadRuntimeMeshStream(LoadBulk, *StreamRef, Stream) && Stream.ElementCount == MeshData->VertexCount)
        {
            MeshData->Streams.push_back(std::move(Stream));
        }
    }

    MeshData->SubMeshes.reserve(RuntimeMesh.SubMeshes.size());
    for (const StaticSubMeshPayload& RuntimeSubMesh : RuntimeMesh.SubMeshes)
    {
        MeshData->SubMeshes.push_back(RuntimeMeshSubMesh{
            .IndexOffset = RuntimeSubMesh.IndexOffset,
            .IndexCount = RuntimeSubMesh.IndexCount,
            .MaterialSlot = RuntimeSubMesh.MaterialSlot,
            .BoundsMin = RuntimeSubMesh.BoundsMin,
            .BoundsMax = RuntimeSubMesh.BoundsMax});
    }

    if (MeshData->SubMeshes.empty())
    {
        MeshData->SubMeshes.push_back(RuntimeMeshSubMesh{
            .IndexOffset = 0,
            .IndexCount = static_cast<uint32_t>(MeshData->Indices.size()),
            .MaterialSlot = 0});
    }

    MeshData->DebugName = RuntimeMesh.Name.empty() ? "RuntimeMesh" : RuntimeMesh.Name;
    if (!StableKey.empty())
    {
        MeshData->DebugName += " [" + std::string(StableKey) + "]";
    }

    MeshData->SourceId = HashString64(!StableKey.empty() ? StableKey : std::string_view(MeshData->DebugName));
    uint64_t Revision = HashString64(MeshData->DebugName, MeshData->SourceId);
    Revision = HashBytes64(&MeshData->VertexCount, sizeof(MeshData->VertexCount), Revision);
    for (const RuntimeMeshStream& Stream : MeshData->Streams)
    {
        Revision = HashBytes64(&Stream.Semantic, sizeof(Stream.Semantic), Revision);
        Revision = HashBytes64(&Stream.ElementCount, sizeof(Stream.ElementCount), Revision);
        Revision = HashBytes64(&Stream.StrideBytes, sizeof(Stream.StrideBytes), Revision);
        Revision = HashBytes64(Stream.Bytes.data(), Stream.Bytes.size(), Revision);
    }
    Revision = HashBytes64(MeshData->Indices.data(), MeshData->Indices.size() * sizeof(uint32_t), Revision);
    MeshData->SourceRevision = Revision == 0 ? 1 : Revision;
    return MeshData;
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

    Runtime->MeshData = BuildRuntimeMeshData(RuntimeMesh, std::move(LoadBulk), StableKey);
    if (!Runtime->MeshData)
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

    Runtime->MeshData = BuildRuntimeMeshData(RuntimeMesh.BaseMesh, std::move(LoadBulk), StableKey);
    if (!Runtime->MeshData)
    {
        return {};
    }

    Runtime->MaterialRefs = RuntimeMesh.BaseMesh.MaterialInstances;
    return Runtime;
}

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
    Manager.RegisterFactory<std::shared_ptr<StaticMeshRuntime>>(std::make_unique<TSharedStaticMeshRuntimeFactory>());
    Manager.RegisterFactory<std::shared_ptr<SkeletalMeshRuntime>>(std::make_unique<TSharedSkeletalMeshRuntimeFactory>());
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
