#include "AssetPipelineFactories.h"

#include <exception>
#include <sstream>
#include <string>

#include "AssetPipelineIds.h"
#include "AssetPipelineSerializers.h"
#include "BaseNode.h"
#include "IAssetCooker.h"
#include "IAssetImporter.h"
#include "Level.h"
#include "NodeCast.h"
#include "RenderAssetPayloads.h"
#include "RenderAssetRuntime.h"
#include "Serialization.h"
#include "TextureCompressorPayloadSerializers.h"
#include "World.h"

namespace TextureCompressorPlugin
{
std::unique_ptr<SnAPI::AssetPipeline::IAssetImporter> CreateTextureCompressorImporter();
std::unique_ptr<SnAPI::AssetPipeline::IAssetCooker> CreateTextureCompressorCooker();
} // namespace TextureCompressorPlugin

namespace SnAPI::GameFramework
{
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetJsonImporter();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetAssimpImporter();
std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialCooker();
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
 * @brief AssetFactory for Material runtime objects.
 */
class TMaterialFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<MaterialAssetRuntime>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadMaterial();
    }

protected:
    std::expected<MaterialAssetRuntime, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<MaterialPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        MaterialAssetRuntime Loaded{};
        Loaded.ShaderModule = PayloadResult->ShaderModule;
        Loaded.ShadingModel = PayloadResult->ShadingModel;
        return Loaded;
    }
};

/**
 * @brief AssetFactory for MaterialInstance runtime objects.
 */
class TMaterialInstanceFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<MaterialInstanceAssetRuntime>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadMaterialInstance();
    }

protected:
    std::expected<MaterialInstanceAssetRuntime, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<MaterialInstancePayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        MaterialInstanceAssetRuntime Loaded{};
        Loaded.ParentMaterial.SetAsset(PayloadResult->ParentMaterial.AssetName, PayloadResult->ParentMaterial.AssetId);
        Loaded.Scalars = PayloadResult->Scalars;
        Loaded.Vectors = PayloadResult->Vectors;

        Loaded.TextureSlots.reserve(PayloadResult->Textures.size());
        Loaded.Textures.reserve(PayloadResult->Textures.size());
        for (const MaterialTextureParamPayload& TextureParam : PayloadResult->Textures)
        {
            Loaded.TextureSlots.push_back(TextureParam.SlotName);
            Loaded.Textures.emplace_back(TextureParam.Texture.AssetName, TextureParam.Texture.AssetId);
        }

        return Loaded;
    }
};

/**
 * @brief AssetFactory for Skeleton runtime objects.
 */
class TSkeletonFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<SkeletonAssetRuntime>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadSkeleton();
    }

protected:
    std::expected<SkeletonAssetRuntime, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<SkeletonPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        SkeletonAssetRuntime Loaded{};
        Loaded.Name = PayloadResult->Name;
        Loaded.Bones = PayloadResult->Bones;
        return Loaded;
    }
};

/**
 * @brief AssetFactory for Animation runtime objects.
 */
class TAnimationFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<AnimationAssetRuntime>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadAnimation();
    }

protected:
    std::expected<AnimationAssetRuntime, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<AnimationPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        AnimationAssetRuntime Loaded{};
        Loaded.Name = PayloadResult->Name;
        Loaded.DurationSeconds = PayloadResult->DurationSeconds;
        Loaded.TicksPerSecond = PayloadResult->TicksPerSecond;
        Loaded.Tracks = PayloadResult->Tracks;
        return Loaded;
    }
};

/**
 * @brief AssetFactory for StaticMesh runtime objects.
 */
class TStaticMeshFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<StaticMeshAssetRuntime>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadStaticMesh();
    }

protected:
    std::expected<StaticMeshAssetRuntime, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<StaticMeshPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        StaticMeshAssetRuntime Loaded{};
        Loaded.SourceAssetId = Context.Info.Id;
        Loaded.Name = PayloadResult->Name;
        Loaded.SubMeshes = PayloadResult->SubMeshes;
        Loaded.Streams = PayloadResult->Streams;
        Loaded.LoadBulk = Context.LoadBulk;
        Loaded.MaterialInstances.reserve(PayloadResult->MaterialInstances.size());
        for (const AssetRefPayload& MaterialRef : PayloadResult->MaterialInstances)
        {
            Loaded.MaterialInstances.emplace_back(MaterialRef.AssetName, MaterialRef.AssetId);
        }
        return Loaded;
    }
};

/**
 * @brief AssetFactory for SkeletalMesh runtime objects.
 */
class TSkeletalMeshFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<SkeletalMeshAssetRuntime>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadSkeletalMesh();
    }

protected:
    std::expected<SkeletalMeshAssetRuntime, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<SkeletalMeshPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        SkeletalMeshAssetRuntime Loaded{};
        Loaded.SourceAssetId = Context.Info.Id;
        Loaded.Name = PayloadResult->BaseMesh.Name;
        Loaded.SubMeshes = PayloadResult->BaseMesh.SubMeshes;
        Loaded.Streams = PayloadResult->BaseMesh.Streams;
        Loaded.LoadBulk = Context.LoadBulk;
        Loaded.Bones = PayloadResult->Bones;
        Loaded.Skeleton.SetAsset(PayloadResult->Skeleton.AssetName, PayloadResult->Skeleton.AssetId);
        Loaded.SkeletonAnimationBulkIndex = PayloadResult->SkeletonAnimationBulkIndex;
        Loaded.Animations.reserve(PayloadResult->Animations.size());
        for (const AssetRefPayload& AnimationRef : PayloadResult->Animations)
        {
            Loaded.Animations.emplace_back(AnimationRef.AssetName, AnimationRef.AssetId);
        }

        Loaded.MaterialInstances.reserve(PayloadResult->BaseMesh.MaterialInstances.size());
        for (const AssetRefPayload& MaterialRef : PayloadResult->BaseMesh.MaterialInstances)
        {
            Loaded.MaterialInstances.emplace_back(MaterialRef.AssetName, MaterialRef.AssetId);
        }

        return Loaded;
    }
};

} // namespace

/**
 * @brief Register GameFramework payload serializers with the AssetPipeline registry.
 * @param Registry Payload registry.
 */
void RegisterAssetPipelinePayloads(::SnAPI::AssetPipeline::PayloadRegistry& Registry)
{
    Registry.Register(CreateNodePayloadSerializer());
    Registry.Register(CreateLevelPayloadSerializer());
    Registry.Register(CreateWorldPayloadSerializer());
    Registry.Register(CreateStaticMeshPayloadSerializer());
    Registry.Register(CreateSkeletalMeshPayloadSerializer());
    Registry.Register(CreateMaterialPayloadSerializer());
    Registry.Register(CreateMaterialInstancePayloadSerializer());
    Registry.Register(CreateSkeletonPayloadSerializer());
    Registry.Register(CreateAnimationPayloadSerializer());
    Registry.Register(CreateStaticMeshSourcePayloadSerializer());
    Registry.Register(CreateSkeletalMeshSourcePayloadSerializer());
}

/**
 * @brief Register GameFramework runtime factories with the AssetManager.
 * @param Manager Asset manager.
 */
void RegisterAssetPipelineFactories(::SnAPI::AssetPipeline::AssetManager& Manager)
{
    Manager.RegisterPayloadMigration(PayloadNode(), 1u, NodeSerializer::kSchemaVersion, MigrateNodePayloadBaseNodeName);
    Manager.RegisterPayloadMigration(PayloadLevel(), 5u, LevelSerializer::kSchemaVersion, MigrateLevelPayloadBaseNodeName);
    Manager.RegisterPayloadMigration(PayloadWorld(), 5u, WorldSerializer::kSchemaVersion, MigrateWorldPayloadBaseNodeName);

    Manager.RegisterFactory<BaseNode>(std::make_unique<TNodeFactory>());
    Manager.RegisterFactory<Level>(std::make_unique<TLevelFactory>());
    Manager.RegisterFactory<World>(std::make_unique<TWorldFactory>());
    Manager.RegisterFactory<MaterialAssetRuntime>(std::make_unique<TMaterialFactory>());
    Manager.RegisterFactory<MaterialInstanceAssetRuntime>(std::make_unique<TMaterialInstanceFactory>());
    Manager.RegisterFactory<SkeletonAssetRuntime>(std::make_unique<TSkeletonFactory>());
    Manager.RegisterFactory<AnimationAssetRuntime>(std::make_unique<TAnimationFactory>());
    Manager.RegisterFactory<StaticMeshAssetRuntime>(std::make_unique<TStaticMeshFactory>());
    Manager.RegisterFactory<SkeletalMeshAssetRuntime>(std::make_unique<TSkeletalMeshFactory>());
}

void RegisterAssetPipelineSourceStages(::SnAPI::AssetPipeline::AssetManager& Manager)
{
    Manager.RegisterSerializer(TextureCompressorPlugin::CreateCompressorImageIntermediateSerializer());
    Manager.RegisterSerializer(TextureCompressorPlugin::CreateCompressorCookedInfoSerializer());

    Manager.RegisterImporter(TextureCompressorPlugin::CreateTextureCompressorImporter());
    Manager.RegisterCooker(TextureCompressorPlugin::CreateTextureCompressorCooker());

    Manager.RegisterImporter(CreateRenderAssetAssimpImporter());
    Manager.RegisterImporter(CreateRenderAssetJsonImporter());

    Manager.RegisterCooker(CreateRenderMaterialCooker());
    Manager.RegisterCooker(CreateRenderMaterialInstanceCooker());
    Manager.RegisterCooker(CreateRenderSkeletonCooker());
    Manager.RegisterCooker(CreateRenderAnimationCooker());
    Manager.RegisterCooker(CreateRenderStaticMeshCooker());
    Manager.RegisterCooker(CreateRenderSkeletalMeshCooker());
}

} // namespace SnAPI::GameFramework
