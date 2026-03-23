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
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_package_manifest_test_" + Stamp);
            std::filesystem::create_directories(Path);
        }

        ~TempDir()
        {
            std::error_code Ec{};
            std::filesystem::remove_all(Path, Ec);
        }
    };

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

    /**
     * @brief Create one starter project used by package-manifest tests.
     * @param Root Temporary parent directory.
     * @param ProjectName Stable project name.
     * @return Created project descriptor path.
     */
    [[nodiscard]] std::filesystem::path CreateStarterProject(const std::filesystem::path& Root,
                                                             const std::string_view ProjectName)
    {
        auto Descriptor = ProjectCreationService::BuildDefaultDescriptor(ProjectName);
        if (!Descriptor)
        {
            throw std::runtime_error(Descriptor.error().Message);
        }

        BuildProfile WindowsDevelopment{};
        WindowsDevelopment.Name = "WindowsDevelopment";
        WindowsDevelopment.Platform = SetValue(std::string("Windows"));
        WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
        WindowsDevelopment.SelectedLevels.IsSet = true;
        WindowsDevelopment.SelectedLevels.Values = {Descriptor->Startup.StartupLevelAsset};
        Descriptor->Profiles = {WindowsDevelopment};

        ProjectCreationRequest CreateRequest{};
        CreateRequest.ProjectName = std::string(ProjectName);
        CreateRequest.ParentDirectory = Root;
        CreateRequest.Descriptor = *Descriptor;

        ProjectCreationResult CreateResult{};
        const Result CreateValue = ProjectCreationService::CreateProject(CreateRequest, &CreateResult);
        if (!CreateValue)
        {
            throw std::runtime_error(CreateValue.error().Message);
        }

        return CreateResult.Project.ProjectFilePath;
    }

    /**
     * @brief Write one small UTF-8 text file.
     * @param FilePath File to write.
     * @param Text File contents.
     */
    void WriteTextFile(const std::filesystem::path& FilePath, const std::string_view Text)
    {
        std::filesystem::create_directories(FilePath.parent_path());
        std::ofstream Output(FilePath, std::ios::binary | std::ios::trunc);
        if (!Output.is_open())
        {
            throw std::runtime_error("Failed to open test output file");
        }
        Output.write(Text.data(), static_cast<std::streamsize>(Text.size()));
        Output.flush();
        if (!Output.good())
        {
            throw std::runtime_error("Failed to write test output file");
        }
    }

} // namespace

TEST_CASE("PackageManifestService enumerates staged files and runtime modules", "[Build][Package]")
{
    TempDir Root{};
    const std::filesystem::path ProjectFile = CreateStarterProject(Root.Path, "PackageManifestHost");

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-070101-packagemanifest";
    auto Plan = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    REQUIRE(Plan);

    WriteTextFile(Plan->StageDirectory / "Bin" / "Runtime.placeholder.txt", "runtime\n");
    WriteTextFile(Plan->StageDirectory / "Assets" / "Primary.snpak", "not-a-real-pack\n");
    WriteTextFile(Plan->StageDirectory / "Config" / "DefaultGame.json", "{\n  \"Version\": 1\n}\n");

    auto Manifest = PackageManifestService::Create(*Resolved, *Plan);
    REQUIRE(Manifest);

    CHECK(Manifest->BuildId == PlannerOptions.BuildId);
    CHECK(Manifest->ProjectName == "PackageManifestHost");
    CHECK(Manifest->ProjectId == Resolved->Project.Descriptor.Project.ProjectId);
    CHECK(Manifest->ProfileName == "WindowsDevelopment");
    CHECK(Manifest->TargetPlatform == "Windows");
    CHECK(Manifest->Configuration == EBuildConfiguration::Development);
    CHECK(Manifest->IncludedLevels == Resolved->Profile.SelectedLevels);
    REQUIRE(Manifest->Modules.size() == 1u);
    CHECK(Manifest->Modules.front().Name == "PackageManifestHost");
    CHECK(Manifest->Modules.front().LoadInRuntime);
    REQUIRE(Manifest->OutputFiles.size() == 3u);
    REQUIRE(Manifest->SnpakFiles.size() == 1u);
    CHECK(Manifest->SnpakFiles.front().RelativePath == "Assets/Primary.snpak");
    CHECK(Manifest->SnpakFiles.front().AssetCount == 0u);

    auto ManifestText = PackageManifestService::Serialize(*Manifest, 2);
    REQUIRE(ManifestText);
    const nlohmann::ordered_json ManifestJson = nlohmann::ordered_json::parse(*ManifestText, nullptr, false);
    REQUIRE_FALSE(ManifestJson.is_discarded());
    CHECK(ManifestJson["BuildId"] == PlannerOptions.BuildId);
    CHECK(ManifestJson["ProjectName"] == "PackageManifestHost");
    CHECK(ManifestJson["Modules"].size() == 1u);
    CHECK(ManifestJson["OutputFiles"].size() == 3u);

    auto StageHashesText = PackageManifestService::SerializeStageFileHashes(*Manifest, 2);
    REQUIRE(StageHashesText);
    const nlohmann::ordered_json StageHashesJson = nlohmann::ordered_json::parse(*StageHashesText, nullptr, false);
    REQUIRE_FALSE(StageHashesJson.is_discarded());
    CHECK(StageHashesJson["BuildId"] == PlannerOptions.BuildId);
    CHECK(StageHashesJson["Files"].size() == 3u);
}
