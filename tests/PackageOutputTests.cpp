#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

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
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_package_output_test_" + Stamp);
            std::filesystem::create_directories(Path);
        }

        ~TempDir()
        {
            std::error_code Error{};
            std::filesystem::remove_all(Path, Error);
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
     * @brief Create one project descriptor on disk for package-output tests.
     * @param Root Temporary parent directory.
     * @param ProjectName Stable project name.
     * @return Path to the written project descriptor.
     */
    [[nodiscard]] std::filesystem::path CreateProject(const std::filesystem::path& Root, const std::string_view ProjectName)
    {
        const std::filesystem::path ProjectRoot = Root / std::string(ProjectName);
        const std::filesystem::path ProjectFilePath = ProjectRoot / "project.snproj.json";

        ProjectDescriptor Descriptor{};
        Descriptor.Project.Name = std::string(ProjectName);
        Descriptor.Project.DisplayName = std::string(ProjectName) + " Display";
        Descriptor.Project.ProjectId = std::string(ProjectName) + "-id";
        Descriptor.Startup.StartupLevelAsset = "Levels/Main.level";

        BuildProfile WindowsDevelopment{};
        WindowsDevelopment.Name = "WindowsDevelopment";
        WindowsDevelopment.Platform = SetValue(std::string("Windows"));
        WindowsDevelopment.ExecutionEnvironment = SetValue(std::string("docker://snapi/windows-msvc:2026.03"));
        WindowsDevelopment.Configuration = SetValue(EBuildConfiguration::Development);
        Descriptor.Profiles = {WindowsDevelopment};

        const Result SaveResult = ProjectDescriptorService::Save(Descriptor, ProjectFilePath.string());
        if (!SaveResult)
        {
            throw std::runtime_error(SaveResult.error().Message);
        }
        return ProjectFilePath;
    }

    /**
     * @brief Write one text file, creating parent directories first.
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
        Output << Text;
    }

} // namespace

TEST_CASE("PackageOutputService copies staged trees and emits zip archives", "[Build][Output]")
{
    TempDir Root{};

    const std::filesystem::path ProjectFile = CreateProject(Root.Path, "PackageOutputHost");

    BuildRequest Request{};
    Request.ProjectFilePath = ProjectFile;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-120000-packageoutput";
    auto Graph = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    REQUIRE(Graph);

    WriteTextFile(Graph->StageDirectory / "Bin" / "Runtime.placeholder.txt", "runtime\n");
    WriteTextFile(Graph->StageDirectory / "Assets" / "Primary.snpak", "pack\n");
    WriteTextFile(Graph->StageDirectory / "Config" / "ResolvedRuntimeConfig.json", "{\n  \"BuildId\": \"test\"\n}\n");
    WriteTextFile(Graph->StageDirectory / "Metadata" / "PackageManifest.json", "{\n  \"BuildId\": \"test\"\n}\n");

    PackageOutputOptions Options{};
    Options.OutputRootDirectory = Root.Path / "Packages";
    Options.ArchiveEnabled = true;
    Options.ArchiveFormat = "zip";

    auto Output = PackageOutputService::Finalize(*Resolved, *Graph, Options);
    REQUIRE(Output);

    CHECK(Output->OutputRootDirectory == (Root.Path / "Packages").lexically_normal());
    CHECK(std::filesystem::exists(Output->PackageDirectoryPath));
    CHECK(std::filesystem::exists(Output->ArchiveFilePath));
    CHECK(Output->PackageDirectoryPath.filename().string().find("PackageOutputHost") != std::string::npos);
    CHECK(Output->PackageDirectoryPath.filename().string().find(PlannerOptions.BuildId) != std::string::npos);
    CHECK(Output->CopiedFiles.size() >= 4u);
    CHECK(std::filesystem::exists(Output->PackageDirectoryPath / "Bin" / "Runtime.placeholder.txt"));
    CHECK(std::filesystem::exists(Output->PackageDirectoryPath / "Assets" / "Primary.snpak"));
}
