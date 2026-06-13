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
#include "ReflectionAnnotations.h"

namespace SnAPI::GameFramework
{

SnType()
struct NodeFieldAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::NodeFieldAsset";

    SnField(SnKey("Name"))
    std::string Name{};
    SnField(SnKey("Value"))
    Conduit::SerializedValue Value{};

    bool operator==(const NodeFieldAsset&) const = default;
};

SnType()
struct NodeComponentAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::NodeComponentAsset";

    SnField(SnKey("Id"))
    Uuid Id{};
    SnField(SnKey("Type"))
    TypeId Type{};
    SnField(SnKey("Fields"))
    std::vector<NodeFieldAsset> Fields{};

    bool operator==(const NodeComponentAsset&) const = default;
};

SnType()
struct NodeObjectAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::NodeObjectAsset";

    SnField(SnKey("Id"))
    Uuid Id{};
    SnField(SnKey("Type"))
    TypeId Type{};
    SnField(SnKey("Name"))
    std::string Name{};
    SnField(SnKey("Active"))
    bool Active = true;
    SnField(SnKey("Fields"))
    std::vector<NodeFieldAsset> Fields{};
    SnField(SnKey("Components"))
    std::vector<NodeComponentAsset> Components{};
    SnField(SnKey("Children"))
    std::vector<NodeObjectAsset> Children{};

    bool operator==(const NodeObjectAsset&) const = default;
};

SnType()
struct NodeAsset : public IAsset
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::NodeAsset";
    static constexpr std::uint32_t kSchemaVersion = 1;

    SnField(SnKey("Name"))
    std::string Name{};
    SnField(SnKey("Nodes"))
    std::vector<NodeObjectAsset> Nodes{};

    bool operator==(const NodeAsset&) const = default;

    [[nodiscard]] std::string_view DisplayName() const override { return "Prefab"; }
    [[nodiscard]] std::string_view FileExtension() const override { return ".prefab"; }
    [[nodiscard]] std::string_view Category() const override { return "World"; }
    [[nodiscard]] bool CanCreate() const override { return false; }
    [[nodiscard]] Result Save(std::ostream& Output) const override;
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourceAssetKind() const override { return AssetKindNode(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId SourcePayloadType() const override { return PayloadNodeSource(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedAssetKind() const override { return AssetKindNode(); }
    [[nodiscard]] ::SnAPI::AssetPipeline::TypeId CookedPayloadType() const override { return PayloadNode(); }
};

SnType()
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

SnType()
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
