#include <chrono>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "PluginDescriptor.h"

using namespace SnAPI::GameFramework;

namespace
{

/**
 * @brief Temporary per-test directory that is deleted on scope exit.
 */
struct TempDir
{
    std::filesystem::path Path{};

    TempDir()
    {
        const auto Stamp = std::to_string(static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        Path = std::filesystem::temp_directory_path() / ("snapi_gf_plugin_descriptor_test_" + Stamp);
        std::filesystem::create_directories(Path);
    }

    ~TempDir()
    {
        std::error_code Ec{};
        std::filesystem::remove_all(Path, Ec);
    }
};

/**
 * @brief Write one UTF-8 test file, creating parent directories as needed.
 * @param Path File path to write.
 * @param Text Full file contents.
 */
void WriteTextFile(const std::filesystem::path& Path, const std::string& Text)
{
    std::error_code Ec{};
    std::filesystem::create_directories(Path.parent_path(), Ec);
    REQUIRE_FALSE(Ec);

    std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
    REQUIRE(Out.is_open());
    Out.write(Text.data(), static_cast<std::streamsize>(Text.size()));
    REQUIRE(Out.good());
}

} // namespace

TEST_CASE("PluginDescriptorService serializes the structured schema", "[Plugin][Descriptor]")
{
    PluginDescriptor Descriptor{};
    Descriptor.Plugin.Name = "Inventory";
    Descriptor.Plugin.DisplayName = "Inventory";
    Descriptor.Plugin.PluginId = "6e8d0f01-0d6c-4fb7-95c7-4d70b5451001";
    Descriptor.Modules.push_back(ProjectModuleDescriptor{
        .Name = "Inventory",
        .Type = EProjectModuleType::Runtime,
        .Root = "Code/Inventory",
        .PublicDependencies = {"SnAPI.GameFramework"},
        .UseReflectionGen = true,
    });

    auto TextResult = PluginDescriptorService::Serialize(Descriptor, 2);
    REQUIRE(TextResult);

    const nlohmann::ordered_json Root = nlohmann::ordered_json::parse(*TextResult, nullptr, false);
    REQUIRE_FALSE(Root.is_discarded());
    REQUIRE(Root.is_object());

    CHECK(Root.contains("Format"));
    CHECK(Root.contains("Plugin"));
    CHECK(Root.contains("Paths"));
    CHECK(Root.contains("Modules"));
    CHECK(Root["Plugin"]["Name"] == "Inventory");
    CHECK(Root["Plugin"]["Version"] == "0.1.0");
    CHECK(Root["Modules"][0]["Type"] == "Runtime");
    CHECK(Root["Modules"][0]["Root"] == "Code/Inventory");

    auto RoundTrip = PluginDescriptorService::Parse(*TextResult);
    REQUIRE(RoundTrip);
    REQUIRE(RoundTrip->Modules.size() == 1);
    CHECK(RoundTrip->Modules.front().Name == "Inventory");
    CHECK(RoundTrip->Modules.front().UseReflectionGen);
}

TEST_CASE("PluginDescriptorService resolves plugin workspace paths", "[Plugin][Descriptor]")
{
    TempDir Root{};
    const std::filesystem::path PluginRoot = Root.Path / "Inventory";
    const std::filesystem::path PluginFilePath = PluginRoot / "plugin.snplugin.json";

    const std::string DescriptorText =
        std::string("{\n") +
        "  \"Format\": { \"SchemaVersion\": 1, \"MinimumToolVersion\": \"0.9.0\" },\n"
        "  \"Plugin\": { \"Name\": \"Inventory\", \"DisplayName\": \"Inventory\" },\n"
        "  \"Paths\": {\n"
        "    \"AssetRoot\": \"Assets\",\n"
        "    \"CodeRoot\": \"Modules\",\n"
        "    \"ConfigRoot\": \"Config\",\n"
        "    \"IntermediateRoot\": \"Intermediate\",\n"
        "    \"SavedRoot\": \"Saved\"\n"
        "  },\n"
        "  \"Modules\": []\n"
        "}\n";
    WriteTextFile(PluginFilePath, DescriptorText);

    auto DescriptorResult = PluginDescriptorService::LoadResolved(PluginFilePath.string());
    REQUIRE(DescriptorResult);

    CHECK(DescriptorResult->Descriptor.Plugin.Name == "Inventory");
    CHECK(DescriptorResult->PluginRootDirectory.lexically_normal() == PluginRoot.lexically_normal());
    CHECK(DescriptorResult->AssetRootDirectory.lexically_normal() == (PluginRoot / "Assets").lexically_normal());
    CHECK(DescriptorResult->CodeRootDirectory.lexically_normal() == (PluginRoot / "Modules").lexically_normal());
    CHECK(DescriptorResult->ConfigRootDirectory.lexically_normal() == (PluginRoot / "Config").lexically_normal());
}

TEST_CASE("PluginDescriptorService rewrites absolute paths to plugin-relative fields when possible",
          "[Plugin][Descriptor]")
{
    TempDir Root{};
    const std::filesystem::path PluginRoot = Root.Path / "PluginRoot";
    const std::filesystem::path InsidePath = PluginRoot / "Assets" / "Data" / "Inventory.asset";
    const std::filesystem::path OutsidePath = Root.Path / "Shared" / "Inventory.asset";

    const std::string RelativeResult =
        PluginDescriptorService::ToPluginRelativePathField(InsidePath.string(), PluginRoot);
    const std::string OutsideResult =
        PluginDescriptorService::ToPluginRelativePathField(OutsidePath.string(), PluginRoot);

    CHECK(RelativeResult == "Assets/Data/Inventory.asset");
    CHECK(std::filesystem::path(OutsideResult).is_absolute());
}
