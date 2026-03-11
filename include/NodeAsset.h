#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "Conduit/Value.h"
#include "Expected.h"
#include "Export.h"
#include "IAsset.h"
#include "Level.h"
#include "Serialization.h"
#include "TypeName.h"
#include "Uuid.h"
#include "World.h"

namespace SnAPI::GameFramework
{

struct NodeFieldAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::NodeFieldAsset";

    std::string Name{};
    Conduit::SerializedValue Value{};

    bool operator==(const NodeFieldAsset&) const = default;
};

struct NodeComponentAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::NodeComponentAsset";

    Uuid Id{};
    TypeId Type{};
    std::vector<NodeFieldAsset> Fields{};

    bool operator==(const NodeComponentAsset&) const = default;
};

struct NodeObjectAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::NodeObjectAsset";

    Uuid Id{};
    TypeId Type{};
    std::string Name{};
    bool Active = true;
    std::vector<NodeFieldAsset> Fields{};
    std::vector<NodeComponentAsset> Components{};
    std::vector<NodeObjectAsset> Children{};

    bool operator==(const NodeObjectAsset&) const = default;
};

struct NodeAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::NodeAsset";
    static constexpr std::uint32_t kSchemaVersion = 1;

    std::string Name{};
    std::vector<NodeObjectAsset> Nodes{};

    bool operator==(const NodeAsset&) const = default;

    [[nodiscard]] std::string_view DisplayName() const override { return "Prefab"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".prefab"; }
    [[nodiscard]] std::string_view Category() const override { return "World"; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindNode(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadNodeSource(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const override { return AssetKindNode(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedPayloadType() const override { return PayloadNode(); }
};

struct LevelAsset : public NodeAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::LevelAsset";
    static constexpr std::uint32_t kSchemaVersion = 1;

    [[nodiscard]] std::string_view DisplayName() const override { return "Level"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".level"; }
    [[nodiscard]] std::string_view Category() const override { return "World"; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindLevel(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadLevelSource(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const override { return AssetKindLevel(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedPayloadType() const override { return PayloadLevel(); }
};

struct WorldAsset : public NodeAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::WorldAsset";
    static constexpr std::uint32_t kSchemaVersion = 1;

    [[nodiscard]] std::string_view DisplayName() const override { return "World"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".world"; }
    [[nodiscard]] std::string_view Category() const override { return "World"; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindWorld(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadWorldSource(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const override { return AssetKindWorld(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedPayloadType() const override { return PayloadWorld(); }
};

SNAPI_GAMEFRAMEWORK_API TExpected<NodeAsset> CaptureNodeAsset(const BaseNode& NodeRef);
SNAPI_GAMEFRAMEWORK_API TExpected<LevelAsset> CaptureLevelAsset(const Level& LevelRef);
SNAPI_GAMEFRAMEWORK_API TExpected<WorldAsset> CaptureWorldAsset(const World& WorldRef);

SNAPI_GAMEFRAMEWORK_API TExpected<NodePayload> CookNodeAsset(const NodeAsset& Asset);
SNAPI_GAMEFRAMEWORK_API TExpected<LevelPayload> CookLevelAsset(const LevelAsset& Asset);
SNAPI_GAMEFRAMEWORK_API TExpected<WorldPayload> CookWorldAsset(const WorldAsset& Asset);

SNAPI_GAMEFRAMEWORK_API TExpected<void> SerializeNodeAsset(const NodeAsset& Asset, std::vector<uint8_t>& OutBytes);
SNAPI_GAMEFRAMEWORK_API TExpected<NodeAsset> DeserializeNodeAsset(const uint8_t* Bytes, size_t Size);
SNAPI_GAMEFRAMEWORK_API TExpected<void> SerializeLevelAsset(const LevelAsset& Asset, std::vector<uint8_t>& OutBytes);
SNAPI_GAMEFRAMEWORK_API TExpected<LevelAsset> DeserializeLevelAsset(const uint8_t* Bytes, size_t Size);
SNAPI_GAMEFRAMEWORK_API TExpected<void> SerializeWorldAsset(const WorldAsset& Asset, std::vector<uint8_t>& OutBytes);
SNAPI_GAMEFRAMEWORK_API TExpected<WorldAsset> DeserializeWorldAsset(const uint8_t* Bytes, size_t Size);

SNAPI_DEFINE_TYPE_NAME(std::vector<NodeFieldAsset>, "std::vector<SnAPI::GameFramework::NodeFieldAsset>")
SNAPI_DEFINE_TYPE_NAME(std::vector<NodeComponentAsset>, "std::vector<SnAPI::GameFramework::NodeComponentAsset>")
SNAPI_DEFINE_TYPE_NAME(std::vector<NodeObjectAsset>, "std::vector<SnAPI::GameFramework::NodeObjectAsset>")

} // namespace SnAPI::GameFramework
