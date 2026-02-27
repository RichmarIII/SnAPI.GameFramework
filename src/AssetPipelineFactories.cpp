#include "AssetPipelineFactories.h"

#include <exception>
#include <sstream>
#include <string>

#include "AssetPipelineIds.h"
#include "AssetPipelineSerializers.h"
#include "BaseNode.h"
#include "Level.h"
#include "NodeCast.h"
#include "Serialization.h"
#include "World.h"

namespace SnAPI::GameFramework
{
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
}

} // namespace SnAPI::GameFramework
