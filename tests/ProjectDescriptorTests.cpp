#include <chrono>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "ProjectDescriptor.h"

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
            const auto Stamp = std::to_string(
                static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_project_descriptor_test_" + Stamp);
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

TEST_CASE("ProjectDescriptorService migrates legacy flat project files", "[Project][Descriptor]")
{
    TempDir Root{};
    const std::filesystem::path ProjectRoot = Root.Path / "LegacyProject";
    const std::filesystem::path ProjectFilePath = ProjectRoot / "project.snproj.json";

    const std::string LegacyDescriptor = std::string("{\n") +
        "  \"version\": 1,\n"
        "  \"name\": \"LegacyProject\",\n"
        "  \"assetRoot\": \"Assets\",\n"
        "  \"startupLevelPack\": \"Packs/Intro.snpak\",\n"
        "  \"defaultRenderSettings\": \"render-settings-id\"\n"
        "}\n";
    WriteTextFile(ProjectFilePath, LegacyDescriptor);
    WriteTextFile(ProjectRoot / "Assets" / "Packs" / "Intro.level", "{}");

    auto DescriptorResult = ProjectDescriptorService::LoadResolved(ProjectFilePath.string());
    REQUIRE(DescriptorResult);

    CHECK(DescriptorResult->Descriptor.Project.Name == "LegacyProject");
    CHECK(DescriptorResult->Descriptor.Paths.AssetRoot == "Assets");
    CHECK(DescriptorResult->Descriptor.Startup.StartupLevelAsset == "Packs/Intro.level");
    CHECK(DescriptorResult->Descriptor.Startup.DefaultRenderSettingsAssetId == "render-settings-id");
    CHECK(DescriptorResult->ProjectRootDirectory.lexically_normal() == ProjectRoot.lexically_normal());
    CHECK(DescriptorResult->AssetRootDirectory.lexically_normal() == (ProjectRoot / "Assets").lexically_normal());
    CHECK(DescriptorResult->CodeRootDirectory.lexically_normal() == (ProjectRoot / "Modules").lexically_normal());
    CHECK(DescriptorResult->StartupLevelAssetPath.lexically_normal() ==
          (ProjectRoot / "Assets" / "Packs" / "Intro.level").lexically_normal());
}

TEST_CASE("ProjectDescriptorService serializes the structured schema", "[Project][Descriptor]")
{
    ProjectDescriptor Descriptor{};
    Descriptor.Project.Name = "MyGame";
    Descriptor.Project.DisplayName = "My Game";
    Descriptor.Project.ProjectId = "6e8d0f01-0d6c-4fb7-95c7-4d70b5450001";
    Descriptor.Startup.DefaultGameClass = "MyGame::MyGame";
    Descriptor.Startup.DefaultGameModeClass = "MyGame::MyGameMode";
    Descriptor.Modules.push_back(ProjectModuleDescriptor{
        .Name = "MyGame",
        .Type = EProjectModuleType::Runtime,
        .Root = "Code/MyGame",
        .PublicDependencies = {"SnAPI.GameFramework"},
        .PrivateDependencies = {"SnAPI.AssetPipeline"},
        .UseReflectionGen = true,
    });
    BuildProfile WindowsDevelopment{};
    WindowsDevelopment.Name = "WindowsDevelopment";
    WindowsDevelopment.Platform = BuildProfileValue<std::string>{.IsSet = true, .Value = std::string("Windows")};
    WindowsDevelopment.Configuration =
        BuildProfileValue<EBuildConfiguration>{.IsSet = true, .Value = EBuildConfiguration::Development};
    Descriptor.Profiles.push_back(std::move(WindowsDevelopment));

    auto TextResult = ProjectDescriptorService::Serialize(Descriptor, 2);
    REQUIRE(TextResult);

    const nlohmann::ordered_json Root = nlohmann::ordered_json::parse(*TextResult, nullptr, false);
    REQUIRE_FALSE(Root.is_discarded());
    REQUIRE(Root.is_object());

    CHECK(Root.contains("Format"));
    CHECK(Root.contains("Project"));
    CHECK(Root.contains("Paths"));
    CHECK(Root.contains("Startup"));
    CHECK(Root.contains("Modules"));
    CHECK(Root.contains("Profiles"));
    CHECK_FALSE(Root.contains("version"));
    CHECK_FALSE(Root.contains("name"));
    CHECK(Root["Project"]["Name"] == "MyGame");
    CHECK(Root["Project"]["DisplayName"] == "My Game");
    CHECK(Root["Modules"][0]["Type"] == "Runtime");
    CHECK(Root["Modules"][0]["Root"] == "Code/MyGame");
    CHECK(Root["Profiles"]["WindowsDevelopment"]["Platform"] == "Windows");
    CHECK(Root["Profiles"]["WindowsDevelopment"]["Configuration"] == "Development");

    auto RoundTrip = ProjectDescriptorService::Parse(*TextResult);
    REQUIRE(RoundTrip);
    REQUIRE(RoundTrip->Modules.size() == 1);
    CHECK(RoundTrip->Modules.front().Name == "MyGame");
    CHECK(RoundTrip->Modules.front().UseReflectionGen);
    REQUIRE(RoundTrip->Profiles.size() == 1);
    CHECK(RoundTrip->Profiles.front().Name == "WindowsDevelopment");
    REQUIRE(RoundTrip->Profiles.front().Platform.Value.has_value());
    CHECK(*RoundTrip->Profiles.front().Platform.Value == "Windows");
}

TEST_CASE("ProjectDescriptorService rewrites absolute paths to project-relative fields when possible",
          "[Project][Descriptor]")
{
    TempDir Root{};
    const std::filesystem::path ProjectRoot = Root.Path / "ProjectRoot";
    const std::filesystem::path InsidePath = ProjectRoot / "Assets" / "Levels" / "Startup.level";
    const std::filesystem::path OutsidePath = Root.Path / "Shared" / "Global.level";

    const std::string RelativeResult =
        ProjectDescriptorService::ToProjectRelativePathField(InsidePath.string(), ProjectRoot);
    const std::string OutsideResult =
        ProjectDescriptorService::ToProjectRelativePathField(OutsidePath.string(), ProjectRoot);
    const std::string UriResult =
        ProjectDescriptorService::ToProjectRelativePathField("asset://Mounted/Levels/Startup.level", ProjectRoot);

    CHECK(RelativeResult == "Assets/Levels/Startup.level");
    CHECK(std::filesystem::path(OutsideResult).is_absolute());
    CHECK(UriResult == "asset://Mounted/Levels/Startup.level");
}
