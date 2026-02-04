#include "AssetPipelineFactories.h"

#include <string>

#include "AssetPipelineIds.h"
#include "AssetPipelineSerializers.h"
#include "Level.h"
#include "Serialization.h"
#include "World.h"

namespace SnAPI::GameFramework
{
namespace
{
class TNodeGraphFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<NodeGraph>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadNodeGraph();
    }

protected:
    std::expected<NodeGraph, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<NodeGraphPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        NodeGraph Graph;
        auto DeserializeResult = NodeGraphSerializer::Deserialize(*PayloadResult, Graph);
        if (!DeserializeResult)
        {
            return std::unexpected(DeserializeResult.error().Message);
        }

        return Graph;
    }
};

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

        Level Loaded;
        auto DeserializeResult = LevelSerializer::Deserialize(*PayloadResult, Loaded);
        if (!DeserializeResult)
        {
            return std::unexpected(DeserializeResult.error().Message);
        }

        return Loaded;
    }
};

class TWorldFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<World>
{
public:
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadWorld();
    }

protected:
    std::expected<World, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<WorldPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        World Loaded;
        auto DeserializeResult = WorldSerializer::Deserialize(*PayloadResult, Loaded);
        if (!DeserializeResult)
        {
            return std::unexpected(DeserializeResult.error().Message);
        }

        return Loaded;
    }
};

} // namespace

void RegisterAssetPipelinePayloads(::SnAPI::AssetPipeline::PayloadRegistry& Registry)
{
    Registry.Register(CreateNodeGraphPayloadSerializer());
    Registry.Register(CreateLevelPayloadSerializer());
    Registry.Register(CreateWorldPayloadSerializer());
}

void RegisterAssetPipelineFactories(::SnAPI::AssetPipeline::AssetManager& Manager)
{
    Manager.RegisterFactory<NodeGraph>(std::make_unique<TNodeGraphFactory>());
    Manager.RegisterFactory<Level>(std::make_unique<TLevelFactory>());
    Manager.RegisterFactory<World>(std::make_unique<TWorldFactory>());
}

} // namespace SnAPI::GameFramework
