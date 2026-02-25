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
/**
 * @brief AssetFactory for Level runtime objects.
 * @remarks Converts cooked Level payloads into Level instances.
 */
class TLevelGraphFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<Level>
{
public:
    /**
     * @brief Get the cooked payload type handled by this factory.
     * @return Payload TypeId.
     */
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadLevelGraph();
    }

protected:
    /**
     * @brief Load a Level from cooked data.
     * @param Context Asset load context.
     * @return Loaded Level or error.
     */
    std::expected<Level, std::string> DoLoad(const ::SnAPI::AssetPipeline::AssetLoadContext& Context) override
    {
        auto PayloadResult = Context.DeserializeCooked<LevelGraphPayload>();
        if (!PayloadResult)
        {
            return std::unexpected(PayloadResult.error());
        }

        Level Graph;
        auto DeserializeResult = LevelGraphSerializer::Deserialize(*PayloadResult, Graph);
        if (!DeserializeResult)
        {
            return std::unexpected(DeserializeResult.error().Message);
        }

        return Graph;
    }
};

/**
 * @brief AssetFactory for Level runtime objects.
 */
class TLevelFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<Level>
{
public:
    /** @brief Get the cooked payload type handled by this factory. */
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadLevel();
    }

protected:
    /**
     * @brief Load a Level from cooked data.
     * @param Context Asset load context.
     * @return Loaded Level or error.
     */
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

/**
 * @brief AssetFactory for World runtime objects.
 */
class TWorldFactory final : public ::SnAPI::AssetPipeline::TAssetFactory<World>
{
public:
    /** @brief Get the cooked payload type handled by this factory. */
    ::SnAPI::AssetPipeline::TypeId GetCookedPayloadType() const override
    {
        return PayloadWorld();
    }

protected:
    /**
     * @brief Load a World from cooked data.
     * @param Context Asset load context.
     * @return Loaded World or error.
     */
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

/**
 * @brief Register GameFramework payload serializers with the AssetPipeline registry.
 * @param Registry Payload registry.
 */
void RegisterAssetPipelinePayloads(::SnAPI::AssetPipeline::PayloadRegistry& Registry)
{
    Registry.Register(CreateLevelGraphPayloadSerializer());
    Registry.Register(CreateLevelPayloadSerializer());
    Registry.Register(CreateWorldPayloadSerializer());
}

/**
 * @brief Register GameFramework runtime factories with the AssetManager.
 * @param Manager Asset manager.
 */
void RegisterAssetPipelineFactories(::SnAPI::AssetPipeline::AssetManager& Manager)
{
    Manager.RegisterFactory<Level>(std::make_unique<TLevelGraphFactory>());
    Manager.RegisterFactory<Level>(std::make_unique<TLevelFactory>());
    Manager.RegisterFactory<World>(std::make_unique<TWorldFactory>());
}

} // namespace SnAPI::GameFramework
