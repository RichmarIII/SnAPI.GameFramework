#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "GameFramework.hpp"

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
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_build_request_test_" + Stamp);
            std::filesystem::create_directories(Path);
        }

        ~TempDir()
        {
            std::error_code Ec{};
            std::filesystem::remove_all(Path, Ec);
        }
    };

    /**
     * @brief Create one project descriptor on disk for build-request tests.
     * @param Root Temporary parent directory.
     * @param ProjectName Stable project name.
     * @param Profiles Authored build profiles to store in the descriptor.
     * @return Path to the written project descriptor.
     */
    [[nodiscard]] std::filesystem::path CreateProject(const std::filesystem::path& Root,
                                                      const std::string_view ProjectName,
                                                      std::vector<BuildProfile> Profiles = {})
    {
        const std::filesystem::path ProjectRoot = Root / std::string(ProjectName);
        const std::filesystem::path ProjectFilePath = ProjectRoot / "project.snproj.json";

        ProjectDescriptor Descriptor{};
        Descriptor.Project.Name = std::string(ProjectName);
        Descriptor.Project.DisplayName = std::string(ProjectName) + " Display";
        Descriptor.Project.ProjectId = std::string(ProjectName) + "-id";
        Descriptor.Paths.AssetRoot = "Assets";
        Descriptor.Paths.CodeRoot = "Code";
        Descriptor.Paths.ConfigRoot = "Config";
        Descriptor.Paths.IntermediateRoot = "Intermediate";
        Descriptor.Paths.SavedRoot = "Saved";
        Descriptor.Startup.StartupLevelAsset = "Levels/Main.level";
        Descriptor.Startup.DefaultGameClass = std::string(ProjectName) + "::Game";
        Descriptor.Startup.DefaultGameModeClass = std::string(ProjectName) + "::GameMode";
        Descriptor.Profiles = std::move(Profiles);

        const Result SaveResult = ProjectDescriptorService::Save(Descriptor, ProjectFilePath.string());
        if (!SaveResult)
        {
            throw std::runtime_error(SaveResult.error().Message);
        }
        return ProjectFilePath;
    }

    /**
     * @brief Build one authored scalar profile patch with a concrete value.
     * @tparam TValue Value type.
     * @param Value Concrete authored value.
     * @return Authored patch.
     */
    template <typename TValue>
    [[nodiscard]] BuildProfileValue<TValue> SetValue(TValue Value)
    {
        return BuildProfileValue<TValue>{
            .IsSet = true,
            .Value = std::move(Value),
        };
    }

} // namespace

TEST_CASE("BuildRequestService resolves named profiles with overrides and deterministic hashes", "[Build][Request]")
{
    TempDir Root{};

    BuildProfile Development{};
    Development.Name = "WindowsDevelopment";
    Development.Platform = SetValue(std::string("Windows"));
    Development.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    Development.Configuration = SetValue(EBuildConfiguration::Development);
    Development.SelectedLevels.IsSet = true;
    Development.SelectedLevels.Values = {"Levels/Main.level"};
    Development.IncludeFolders.IsSet = true;
    Development.IncludeFolders.Values = {"Assets/Shared"};

    BuildProfile Shipping{};
    Shipping.Name = "WindowsShipping";
    Shipping.Inherits = "WindowsDevelopment";
    Shipping.Configuration = SetValue(EBuildConfiguration::Shipping);
    Shipping.Archive.IsSet = true;
    Shipping.Archive.Enabled = SetValue(true);
    Shipping.Archive.Format = SetValue(std::string("zip"));

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "RequestHost", {Development, Shipping});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsShipping";
    Request.Overrides.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.04"));
    Request.Overrides.SelectedLevels.IsSet = true;
    Request.Overrides.SelectedLevels.Values = {"Levels/Demo.level"};

    auto FirstResolved = BuildRequestService::Resolve(Request);
    REQUIRE(FirstResolved);
    auto SecondResolved = BuildRequestService::Resolve(Request);
    REQUIRE(SecondResolved);

    CHECK(FirstResolved->ProfileName == "WindowsShipping");
    CHECK(FirstResolved->Profile.Name == "WindowsShipping");
    CHECK(FirstResolved->Profile.Inherits == "WindowsDevelopment");
    CHECK(FirstResolved->Profile.Platform == "Windows");
    CHECK(FirstResolved->Profile.ExecutionEnvironment == "docker://snapi/windows-msvc:2026.04");
    CHECK(FirstResolved->Profile.Configuration == EBuildConfiguration::Shipping);
    CHECK(FirstResolved->Profile.SelectedLevels == std::vector<std::string>{"Levels/Main.level", "Levels/Demo.level"});
    CHECK(FirstResolved->Profile.IncludeFolders == std::vector<std::string>{"Assets/Shared"});
    CHECK(FirstResolved->Profile.ArchiveEnabled);
    CHECK(FirstResolved->Profile.ArchiveFormat == "zip");
    CHECK_FALSE(FirstResolved->RequestHash.empty());
    CHECK(FirstResolved->RequestHash == SecondResolved->RequestHash);

    auto SerializedResult = BuildRequestService::SerializeResolved(*FirstResolved, 2);
    REQUIRE(SerializedResult);
    const nlohmann::ordered_json Serialized = nlohmann::ordered_json::parse(*SerializedResult, nullptr, false);
    REQUIRE_FALSE(Serialized.is_discarded());
    CHECK(Serialized["ProfileName"] == "WindowsShipping");
    CHECK(Serialized["ResolvedProfile"]["ExecutionEnvironment"] == "docker://snapi/windows-msvc:2026.04");
    CHECK(Serialized["RequestHash"] == FirstResolved->RequestHash);

    BuildRequest DifferentRequest = Request;
    DifferentRequest.Overrides.SelectedLevels.Values = {"Levels/Hotfix.level"};
    auto DifferentResolved = BuildRequestService::Resolve(DifferentRequest);
    REQUIRE(DifferentResolved);
    CHECK(DifferentResolved->RequestHash != FirstResolved->RequestHash);
}

TEST_CASE("BuildRequestService resolves ad hoc requests without a named profile", "[Build][Request]")
{
    TempDir Root{};
    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "AdHocRequestHost");

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.Overrides.Platform = SetValue(std::string("Linux"));
    Request.Overrides.ExecutionEnvironment = SetValue(std::string("docker://snapi/linux-clang:2026.03"));
    Request.Overrides.Configuration = SetValue(EBuildConfiguration::Test);
    Request.Overrides.IncludeFolders.IsSet = true;
    Request.Overrides.IncludeFolders.Values = {"Assets/Shared"};

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    CHECK(Resolved->ProfileName.empty());
    CHECK(Resolved->Profile.Name.empty());
    CHECK(Resolved->Profile.Platform == "Linux");
    CHECK(Resolved->Profile.ExecutionEnvironment == "docker://snapi/linux-clang:2026.03");
    CHECK(Resolved->Profile.Configuration == EBuildConfiguration::Test);
    CHECK(Resolved->Profile.IncludeFolders == std::vector<std::string>{"Assets/Shared"});
    CHECK(Resolved->ValidationIssues.empty());
}

TEST_CASE("BuildRequestService reports an error when overrides clear the resolved platform", "[Build][Request]")
{
    TempDir Root{};

    BuildProfile Development{};
    Development.Name = "WindowsDevelopment";
    Development.Platform = SetValue(std::string("Windows"));
    Development.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "ClearPlatformHost", {Development});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";
    Request.Overrides.Platform.IsSet = true;
    Request.Overrides.Platform.Value = std::nullopt;

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE_FALSE(Resolved);
    CHECK(Resolved.error().Message.find("BuildRequest.PlatformMissing") != std::string::npos);
}

TEST_CASE("BuildRequestService reports an error when archive output is enabled without a format", "[Build][Request]")
{
    TempDir Root{};
    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "ArchiveHost");

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.Overrides.Platform = SetValue(std::string("Windows"));
    Request.Overrides.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    Request.Overrides.Archive.IsSet = true;
    Request.Overrides.Archive.Enabled = SetValue(true);

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE_FALSE(Resolved);
    CHECK(Resolved.error().Message.find("BuildRequest.ArchiveFormatMissing") != std::string::npos);
}

TEST_CASE("BuildRequestService can deserialize frozen resolved requests from serialized history artifacts",
          "[Build][Request]")
{
    TempDir Root{};

    BuildProfile Development{};
    Development.Name = "WindowsDevelopment";
    Development.Platform = SetValue(std::string("Windows"));
    Development.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
    Development.Configuration = SetValue(EBuildConfiguration::Development);
    Development.SelectedLevels.IsSet = true;
    Development.SelectedLevels.Values = {"Levels/Main.level"};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "DeserializeRequestHost", {Development});

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    auto Serialized = BuildRequestService::SerializeResolved(*Resolved, 2);
    REQUIRE(Serialized);

    auto Parsed = BuildRequestService::DeserializeResolved(*Serialized);
    REQUIRE(Parsed);

    CHECK(Parsed->Project.ProjectFilePath == Resolved->Project.ProjectFilePath);
    CHECK(Parsed->ProfileName == Resolved->ProfileName);
    CHECK(Parsed->Profile.Platform == Resolved->Profile.Platform);
    CHECK(Parsed->Profile.ExecutionEnvironment == Resolved->Profile.ExecutionEnvironment);
    CHECK(Parsed->Profile.Configuration == Resolved->Profile.Configuration);
    CHECK(Parsed->Profile.SelectedLevels == Resolved->Profile.SelectedLevels);
    CHECK(Parsed->RequestHash == Resolved->RequestHash);

    const std::filesystem::path BuildRequestPath = Root.Path / "HistoryBuildRequest.json";
    std::ofstream Output(BuildRequestPath, std::ios::binary | std::ios::trunc);
    REQUIRE(Output.is_open());
    Output << *Serialized;
    Output.close();

    auto Loaded = BuildRequestService::LoadResolved(BuildRequestPath);
    REQUIRE(Loaded);
    CHECK(Loaded->RequestHash == Resolved->RequestHash);
    CHECK(Loaded->Project.ProjectRootDirectory == Resolved->Project.ProjectRootDirectory);
}
