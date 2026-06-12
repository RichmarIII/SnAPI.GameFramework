#include <ranges>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

namespace
{

    /**
     * @brief Parse one JSON `Profiles` object and fail the test immediately on error.
     * @param ProfilesJson Descriptor-style `Profiles` object.
     * @return Parsed typed build profiles.
     */
    [[nodiscard]] std::vector<BuildProfile> RequireParsedProfiles(const nlohmann::ordered_json& ProfilesJson)
    {
        auto ProfilesResult = BuildProfileService::ParseProfiles(ProfilesJson);
        if (!ProfilesResult)
        {
            throw std::runtime_error(ProfilesResult.error().Message);
        }
        return *ProfilesResult;
    }

} // namespace

TEST_CASE("BuildProfileService parses and serializes typed profiles", "[Build][Profile]")
{
    const nlohmann::ordered_json ProfilesJson = nlohmann::ordered_json::object({
        {"WindowsDevelopment",
         {
             {"Platform", "Windows"},
             {"ExecutionEnvironment", "docker://snapi/windows-msvc:2026.03"},
             {"Configuration", "Development"},
             {"SelectedLevels", nlohmann::ordered_json::array({"Levels/Main.level"})},
             {"IncludeFolders", nlohmann::ordered_json::array({"Assets/Shared"})},
             {"Archive", {{"Enabled", false}}},
         }},
        {"WindowsShipping",
         {
             {"Inherits", "WindowsDevelopment"},
             {"Configuration", "Shipping"},
             {"IncludeFolders",
              {
                  {"Values", nlohmann::ordered_json::array({"Assets/Demo"})},
                  {"Replace", true},
              }},
             {"AllowExplicitOverrideExcludes", true},
         }},
    });

    const std::vector<BuildProfile> Profiles = RequireParsedProfiles(ProfilesJson);
    REQUIRE(Profiles.size() == 2);

    CHECK(Profiles[0].Name == "WindowsDevelopment");
    REQUIRE(Profiles[0].Platform.Value.has_value());
    CHECK(*Profiles[0].Platform.Value == "Windows");
    REQUIRE(Profiles[0].ExecutionEnvironment.Value.has_value());
    CHECK(*Profiles[0].ExecutionEnvironment.Value == "docker://snapi/windows-msvc:2026.03");
    REQUIRE(Profiles[0].Configuration.Value.has_value());
    CHECK(*Profiles[0].Configuration.Value == EBuildConfiguration::Development);
    CHECK(Profiles[0].SelectedLevels.Values == std::vector<std::string>{"Levels/Main.level"});
    CHECK(Profiles[0].Archive.IsSet);
    CHECK(Profiles[0].Archive.Enabled.IsSet);
    REQUIRE(Profiles[0].Archive.Enabled.Value.has_value());
    CHECK_FALSE(*Profiles[0].Archive.Enabled.Value);

    CHECK(Profiles[1].Name == "WindowsShipping");
    CHECK(Profiles[1].Inherits == "WindowsDevelopment");
    REQUIRE(Profiles[1].Configuration.Value.has_value());
    CHECK(*Profiles[1].Configuration.Value == EBuildConfiguration::Shipping);
    CHECK(Profiles[1].IncludeFolders.IsSet);
    CHECK(Profiles[1].IncludeFolders.Replace);
    CHECK(Profiles[1].IncludeFolders.Values == std::vector<std::string>{"Assets/Demo"});

    auto SerializedResult = BuildProfileService::SerializeProfiles(Profiles);
    REQUIRE(SerializedResult);
    CHECK(*SerializedResult == ProfilesJson);
}

TEST_CASE("BuildProfileService resolves inherited profiles with append and replace semantics", "[Build][Profile]")
{
    const nlohmann::ordered_json ProfilesJson = nlohmann::ordered_json::object({
        {"Base",
         {
             {"Platform", "Windows"},
             {"ExecutionEnvironment", "docker://snapi/windows-msvc:2026.03"},
             {"Configuration", "Development"},
             {"SelectedLevels", nlohmann::ordered_json::array({"Levels/Main.level"})},
             {"IncludeFolders", nlohmann::ordered_json::array({"Assets/Shared"})},
             {"ExcludeFolders", nlohmann::ordered_json::array({"Assets/EditorOnly"})},
             {"DependencyPolicy", "HardAndSoft"},
             {"ChunkStrategy", "Monolithic"},
             {"Archive", {{"Enabled", false}, {"Format", "zip"}}},
         }},
        {"DemoShipping",
         {
             {"Inherits", "Base"},
             {"Configuration", "Shipping"},
             {"SelectedLevels", nlohmann::ordered_json::array({"Levels/Demo.level"})},
             {"IncludeFolders", nlohmann::ordered_json::array({"Assets/Demo"})},
             {"ExcludeFolders",
              {
                  {"Values", nlohmann::ordered_json::array({"Assets/Internal"})},
                  {"Replace", true},
              }},
             {"ChunkStrategy", "SharedPlusPerLevel"},
             {"AllowExplicitOverrideExcludes", true},
             {"Archive", {{"ReplaceEntireObject", true}, {"Enabled", true}, {"Format", "zip"}}},
         }},
    });

    const std::vector<BuildProfile> Profiles = RequireParsedProfiles(ProfilesJson);
    auto ResolvedResult = BuildProfileService::ResolveProfile(Profiles, "DemoShipping");
    REQUIRE(ResolvedResult);

    const ResolvedBuildProfile& Profile = *ResolvedResult;
    CHECK(Profile.Name == "DemoShipping");
    CHECK(Profile.Inherits == "Base");
    CHECK(Profile.Platform == "Windows");
    CHECK(Profile.ExecutionEnvironment == "docker://snapi/windows-msvc:2026.03");
    CHECK(Profile.Configuration == EBuildConfiguration::Shipping);
    CHECK(Profile.SelectedLevels == std::vector<std::string>{"Levels/Main.level", "Levels/Demo.level"});
    CHECK(Profile.IncludeFolders == std::vector<std::string>{"Assets/Shared", "Assets/Demo"});
    CHECK(Profile.ExcludeFolders == std::vector<std::string>{"Assets/Internal"});
    CHECK(Profile.DependencyPolicy == EAssetDependencyPolicy::HardAndSoft);
    CHECK(Profile.ChunkStrategy == EAssetChunkStrategy::SharedPlusPerLevel);
    CHECK(Profile.AllowExplicitOverrideExcludes);
    CHECK(Profile.ArchiveEnabled);
    CHECK(Profile.ArchiveFormat == "zip");
}

TEST_CASE("BuildProfileService respects explicit null clears for inherited values", "[Build][Profile]")
{
    const nlohmann::ordered_json ProfilesJson = nlohmann::ordered_json::object({
        {"Base",
         {
             {"Platform", "Linux"},
             {"ExecutionEnvironment", "docker://snapi/linux-clang:2026.03"},
             {"IncludeFolders", nlohmann::ordered_json::array({"Assets/Shared"})},
             {"Archive", {{"Enabled", true}, {"Format", "tar.gz"}}},
         }},
        {"Reset",
         {
             {"Inherits", "Base"},
             {"Platform", nullptr},
             {"ExecutionEnvironment", nullptr},
             {"IncludeFolders", nullptr},
             {"Archive", nullptr},
         }},
    });

    const std::vector<BuildProfile> Profiles = RequireParsedProfiles(ProfilesJson);
    auto ResolvedResult = BuildProfileService::ResolveProfile(Profiles, "Reset");
    REQUIRE(ResolvedResult);

    const ResolvedBuildProfile& Profile = *ResolvedResult;
    CHECK(Profile.Platform.empty());
    CHECK(Profile.ExecutionEnvironment.empty());
    CHECK(Profile.IncludeFolders.empty());
    CHECK_FALSE(Profile.ArchiveEnabled);
    CHECK(Profile.ArchiveFormat.empty());
}

TEST_CASE("BuildProfileService validates duplicate names and inheritance cycles", "[Build][Profile]")
{
    std::vector<BuildProfile> Profiles{};

    BuildProfile First{};
    First.Name = "LoopA";
    First.Inherits = "LoopB";
    Profiles.push_back(First);

    BuildProfile Second{};
    Second.Name = "LoopB";
    Second.Inherits = "LoopA";
    Profiles.push_back(Second);

    BuildProfile Duplicate{};
    Duplicate.Name = "LoopA";
    Profiles.push_back(Duplicate);

    const std::vector<BuildValidationIssue> Issues = BuildProfileService::Validate(Profiles);
    REQUIRE_FALSE(Issues.empty());

    CHECK(std::ranges::any_of(Issues, [](const BuildValidationIssue& Issue)
                              { return Issue.RuleId == "BuildProfile.NameDuplicate"; }));
    CHECK(std::ranges::any_of(Issues, [](const BuildValidationIssue& Issue)
                              { return Issue.RuleId == "BuildProfile.InheritanceCycle"; }));
}
