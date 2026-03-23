#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <AssetPackReader.h>

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
            Path = std::filesystem::temp_directory_path() / ("snapi_gf_asset_cook_test_" + Stamp);
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
     * @brief Descriptor/workspace snapshot used by asset-cook tests.
     */
    struct CreatedProject
    {
        std::filesystem::path ProjectRootPath{};
        std::filesystem::path AssetRootPath{};
        std::filesystem::path ProjectFilePath{};
        std::string StartupLevelLogicalName{};
    };

    /**
     * @brief Create one starter project that contains a real level source asset.
     * @param Root Temporary parent directory.
     * @param ProjectName Stable project name.
     * @param ProfileMutator Optional authored-profile mutator applied before project creation.
     * @return Created project snapshot.
     */
    [[nodiscard]] CreatedProject CreateStarterProject(const std::filesystem::path& Root,
                                                      const std::string_view ProjectName,
                                                      const std::function<void(BuildProfile&)>& ProfileMutator = {})
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
        if (ProfileMutator)
        {
            ProfileMutator(WindowsDevelopment);
        }
        Descriptor->Profiles = {WindowsDevelopment};

        ProjectCreationRequest CreateRequest{};
        CreateRequest.ProjectName = std::string(ProjectName);
        CreateRequest.ParentDirectory = Root;
        CreateRequest.Descriptor = *Descriptor;

        ProjectCreationResult CreateResult{};
        const Result ResultValue = ProjectCreationService::CreateProject(CreateRequest, &CreateResult);
        if (!ResultValue)
        {
            throw std::runtime_error(ResultValue.error().Message);
        }

        return CreatedProject{
            .ProjectRootPath = CreateResult.Project.ProjectRootDirectory,
            .AssetRootPath = CreateResult.Project.AssetRootDirectory,
            .ProjectFilePath = CreateResult.Project.ProjectFilePath,
            .StartupLevelLogicalName = CreateResult.Project.Descriptor.Startup.StartupLevelAsset,
        };
    }

    /**
     * @brief Copy one file, creating parent directories for the destination when needed.
     * @param SourcePath Existing source file path.
     * @param DestinationPath Destination file path.
     */
    void CopyFile(const std::filesystem::path& SourcePath, const std::filesystem::path& DestinationPath)
    {
        std::filesystem::create_directories(DestinationPath.parent_path());
        std::filesystem::copy_file(SourcePath, DestinationPath, std::filesystem::copy_options::overwrite_existing);
    }

    /**
     * @brief Write one UTF-8 text file, creating parent directories as needed.
     * @param FilePath Output file path.
     * @param Text UTF-8 text payload.
     */
    void WriteTextFile(const std::filesystem::path& FilePath, const std::string_view Text)
    {
        std::filesystem::create_directories(FilePath.parent_path());
        std::ofstream Stream(FilePath, std::ios::binary | std::ios::trunc);
        if (!Stream.is_open())
        {
            throw std::runtime_error("Failed to open file for write: " + FilePath.string());
        }
        Stream.write(Text.data(), static_cast<std::streamsize>(Text.size()));
        if (!Stream.good())
        {
            throw std::runtime_error("Failed to write file: " + FilePath.string());
        }
    }

    /**
     * @brief Find one included or excluded selection record by logical name.
     * @param Records Ordered record array to inspect.
     * @param LogicalName Logical asset name to locate.
     * @return Pointer to the matching record, or `nullptr`.
     */
    [[nodiscard]] const AssetSelectionRecord* FindSelectionRecord(const std::vector<AssetSelectionRecord>& Records,
                                                                  const std::string_view LogicalName)
    {
        const auto It = std::find_if(Records.begin(), Records.end(), [&](const AssetSelectionRecord& Record) {
            return Record.LogicalName == LogicalName;
        });
        return It == Records.end() ? nullptr : std::addressof(*It);
    }

    /**
     * @brief Return `true` when one record contains the specified provenance entry.
     * @param Record Record to inspect.
     * @param Kind Expected provenance kind.
     * @param Included Expected inclusion flag.
     * @return `true` when the provenance entry exists.
     */
    [[nodiscard]] bool HasProvenance(const AssetSelectionRecord& Record,
                                     const std::string_view Kind,
                                     const bool Included)
    {
        return std::any_of(Record.Provenance.begin(), Record.Provenance.end(), [&](const AssetSelectionProvenanceEntry& Entry) {
            return Entry.Kind == Kind && Entry.Included == Included;
        });
    }

    /**
     * @brief Build the expected level-chunk id for one logical asset path.
     * @param LogicalName Asset logical name.
     * @return Sanitized level chunk id that matches the runtime chunk planner.
     */
    [[nodiscard]] std::string MakeExpectedLevelChunkId(const std::string_view LogicalName)
    {
        std::string Value(LogicalName);
        for (char& Character : Value)
        {
            if (std::isalnum(static_cast<unsigned char>(Character)) == 0)
            {
                Character = '_';
            }
        }
        while (!Value.empty() && Value.front() == '_')
        {
            Value.erase(Value.begin());
        }
        while (!Value.empty() && Value.back() == '_')
        {
            Value.pop_back();
        }
        return "Level_" + Value;
    }

} // namespace

TEST_CASE("AssetCookServiceAdapter resolves selected starter-level assets", "[Build][AssetCook]")
{
    TempDir Root{};
    const CreatedProject Project = CreateStarterProject(Root.Path, "AssetSelectionHost");

    BuildRequest Request{};
    Request.ProjectFilePath = Project.ProjectFilePath;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    auto Selection = AssetCookServiceAdapter::ResolveSelectedAssets(*Resolved);
    REQUIRE(Selection);
    REQUIRE(Selection->size() == 1u);
    CHECK((*Selection)[0].LogicalName == Project.StartupLevelLogicalName);
    CHECK((*Selection)[0].SelectionReason == "SelectedLevel");
    CHECK(std::filesystem::exists((*Selection)[0].SourcePath));
}

TEST_CASE("AssetCookServiceAdapter recursively expands selected level asset references", "[Build][AssetCook]")
{
    TempDir Root{};
    const CreatedProject Project = CreateStarterProject(Root.Path, "AssetSelectionDependencies");

    NodeAsset PawnPrefab{};
    PawnPrefab.Name = "SpawnPawn";
    PawnPrefab.Nodes.push_back(NodeObjectAsset{
        .Id = NewUuid(),
        .Type = StaticTypeId<PawnBase>(),
        .Name = "SpawnPawn",
        .Active = true,
    });
    auto PawnJson = SerializeAuthoredAssetToJson(PawnPrefab);
    REQUIRE(PawnJson);
    WriteTextFile(Project.AssetRootPath / "Prefabs" / "SpawnPawn.prefab", *PawnJson);

    TAssetRef<PawnBase> PawnRef{};
    PawnRef.EditAssetName() = "Prefabs/SpawnPawn.prefab";
    PawnRef.EditAssetId() = SourceAssetIdFromLogicalName(PawnRef.GetAssetName()).ToString();

    LevelAsset StartupLevel{};
    StartupLevel.Name = "Startup";
    auto SpawnPawnValue = Conduit::SerializedValue::FromValue(PawnRef);
    REQUIRE(SpawnPawnValue);
    StartupLevel.Nodes.push_back(NodeObjectAsset{
        .Id = NewUuid(),
        .Type = StaticTypeId<PlayerStart>(),
        .Name = "DependencyStart",
        .Active = true,
        .Fields =
            {
                NodeFieldAsset{
                    .Name = "SpawnPawnAsset",
                    .Value = *SpawnPawnValue,
                },
            },
    });
    auto LevelJson = SerializeAuthoredAssetToJson(StartupLevel);
    REQUIRE(LevelJson);
    WriteTextFile(Project.AssetRootPath / Project.StartupLevelLogicalName, *LevelJson);

    BuildRequest Request{};
    Request.ProjectFilePath = Project.ProjectFilePath;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    auto Plan = AssetCookServiceAdapter::ResolveAssetSelection(*Resolved);
    REQUIRE(Plan);
    REQUIRE(Plan->IncludedAssets.size() == 2u);

    const AssetSelectionRecord* StartupRecord = FindSelectionRecord(Plan->IncludedAssets, Project.StartupLevelLogicalName);
    REQUIRE(StartupRecord != nullptr);
    CHECK(HasProvenance(*StartupRecord, "SelectedLevel", true));

    const AssetSelectionRecord* PawnRecord = FindSelectionRecord(Plan->IncludedAssets, "Prefabs/SpawnPawn.prefab");
    REQUIRE(PawnRecord != nullptr);
    CHECK(PawnRecord->SelectionReason == "RequiredDependency");
    CHECK(HasProvenance(*PawnRecord, "RequiredDependency", true));
    CHECK(HasProvenance(*PawnRecord, "DependencyKind", true));
    CHECK(PawnRecord->ChunkId == "Shared");
}

#if defined(SNAPI_GF_ENABLE_RENDERER)
TEST_CASE("AssetCookServiceAdapter always includes project default render settings and their dependencies",
          "[Build][AssetCook]")
{
    TempDir Root{};
    const CreatedProject Project = CreateStarterProject(Root.Path, "AssetSelectionDefaultRenderSettings");

    NodeAsset FogPrefab{};
    FogPrefab.Name = "ProjectFogParams";
    FogPrefab.Nodes.push_back(NodeObjectAsset{
        .Id = NewUuid(),
        .Type = StaticTypeId<HeightFogParamsNode>(),
        .Name = "ProjectFogParams",
        .Active = true,
    });
    auto FogJson = SerializeAuthoredAssetToJson(FogPrefab);
    REQUIRE(FogJson);
    WriteTextFile(Project.AssetRootPath / "Rendering" / "ProjectFogParams.prefab", *FogJson);

    TAssetRef<HeightFogParamsNode> FogRef{};
    FogRef.EditAssetName() = "Rendering/ProjectFogParams.prefab";
    FogRef.EditAssetId() = SourceAssetIdFromLogicalName(FogRef.GetAssetName()).ToString();

    NodeAsset DefaultRenderSettingsPrefab{};
    DefaultRenderSettingsPrefab.Name = "ProjectDefaultRenderSettings";
    DefaultRenderSettingsPrefab.Nodes.push_back(NodeObjectAsset{
        .Id = NewUuid(),
        .Type = StaticTypeId<WorldRenderSettings>(),
        .Name = "ProjectDefaultRenderSettings",
        .Active = true,
        .Fields =
            {
                NodeFieldAsset{
                    .Name = "HeightFogParams",
                    .Value = Conduit::SerializedValue::FromValue(FogRef).value(),
                },
            },
    });
    auto RenderSettingsJson = SerializeAuthoredAssetToJson(DefaultRenderSettingsPrefab);
    REQUIRE(RenderSettingsJson);
    WriteTextFile(Project.AssetRootPath / "Rendering" / "ProjectDefaultRenderSettings.prefab", *RenderSettingsJson);

    auto DescriptorResult = ProjectDescriptorService::Load(Project.ProjectFilePath.string());
    REQUIRE(DescriptorResult);
    DescriptorResult->Startup.DefaultRenderSettingsAssetId =
        SourceAssetIdFromLogicalName("Rendering/ProjectDefaultRenderSettings.prefab").ToString();
    REQUIRE(ProjectDescriptorService::Save(*DescriptorResult, Project.ProjectFilePath.string()));

    BuildRequest Request{};
    Request.ProjectFilePath = Project.ProjectFilePath;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    auto Plan = AssetCookServiceAdapter::ResolveAssetSelection(*Resolved);
    REQUIRE(Plan);

    const AssetSelectionRecord* StartupRecord = FindSelectionRecord(Plan->IncludedAssets, Project.StartupLevelLogicalName);
    REQUIRE(StartupRecord != nullptr);
    CHECK(HasProvenance(*StartupRecord, "SelectedLevel", true));

    const AssetSelectionRecord* RenderSettingsRecord =
        FindSelectionRecord(Plan->IncludedAssets, "Rendering/ProjectDefaultRenderSettings.prefab");
    REQUIRE(RenderSettingsRecord != nullptr);
    CHECK(RenderSettingsRecord->SelectionReason == "DefaultRenderSettings");
    CHECK(HasProvenance(*RenderSettingsRecord, "DefaultRenderSettings", true));

    const AssetSelectionRecord* FogRecord = FindSelectionRecord(Plan->IncludedAssets, "Rendering/ProjectFogParams.prefab");
    REQUIRE(FogRecord != nullptr);
    CHECK(FogRecord->SelectionReason == "RequiredDependency");
    CHECK(HasProvenance(*FogRecord, "RequiredDependency", true));
}
#endif

TEST_CASE("AssetCookServiceAdapter records provenance and exclusion decisions for selected assets", "[Build][AssetCook]")
{
    TempDir Root{};
    const CreatedProject Project = CreateStarterProject(
        Root.Path, "AssetSelectionProvenance", [](BuildProfile& Profile) {
            Profile.IncludeFolders.IsSet = true;
            Profile.IncludeFolders.Values = {"Shared", "EditorOnly"};
            Profile.ExcludeFolders.IsSet = true;
            Profile.ExcludeFolders.Values = {"EditorOnly"};
            Profile.ChunkStrategy = SetValue(EAssetChunkStrategy::SharedPlusPerLevel);
        });

    const std::filesystem::path StartupLevelSource = Project.AssetRootPath / Project.StartupLevelLogicalName;
    CopyFile(StartupLevelSource, Project.AssetRootPath / "Shared" / "Shared.level");
    CopyFile(StartupLevelSource, Project.AssetRootPath / "EditorOnly" / "EditorPreview.level");

    BuildRequest Request{};
    Request.ProjectFilePath = Project.ProjectFilePath;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    auto Plan = AssetCookServiceAdapter::ResolveAssetSelection(*Resolved);
    REQUIRE(Plan);
    REQUIRE(Plan->IncludedAssets.size() == 2u);
    REQUIRE(Plan->ExcludedAssets.size() == 1u);

    const AssetSelectionRecord* StartupRecord = FindSelectionRecord(Plan->IncludedAssets, Project.StartupLevelLogicalName);
    REQUIRE(StartupRecord != nullptr);
    CHECK(StartupRecord->ChunkId == MakeExpectedLevelChunkId(Project.StartupLevelLogicalName));
    CHECK(HasProvenance(*StartupRecord, "SelectedLevel", true));

    const AssetSelectionRecord* SharedRecord = FindSelectionRecord(Plan->IncludedAssets, "Shared/Shared.level");
    REQUIRE(SharedRecord != nullptr);
    CHECK(SharedRecord->ChunkId == "Shared");
    CHECK(HasProvenance(*SharedRecord, "IncludeFolder", true));

    const AssetSelectionRecord* ExcludedRecord =
        FindSelectionRecord(Plan->ExcludedAssets, "EditorOnly/EditorPreview.level");
    REQUIRE(ExcludedRecord != nullptr);
    CHECK(ExcludedRecord->ChunkId == "Shared");
    CHECK(HasProvenance(*ExcludedRecord, "IncludeFolder", true));
    CHECK(HasProvenance(*ExcludedRecord, "ExcludeFolder", false));
}

TEST_CASE("AssetCookServiceAdapter adds include-kind auxiliary files without replacing selected levels",
          "[Build][AssetCook]")
{
    TempDir Root{};
    const CreatedProject Project = CreateStarterProject(
        Root.Path, "AssetSelectionKinds", [](BuildProfile& Profile) {
            Profile.IncludeAssetKinds.IsSet = true;
            Profile.IncludeAssetKinds.Values = {".slang"};
        });

    WriteTextFile(Project.AssetRootPath / "Shaders" / "MyShader.slang", "shader-entry {}\n");

    BuildRequest Request{};
    Request.ProjectFilePath = Project.ProjectFilePath;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    auto Plan = AssetCookServiceAdapter::ResolveAssetSelection(*Resolved);
    REQUIRE(Plan);

    const AssetSelectionRecord* StartupRecord = FindSelectionRecord(Plan->IncludedAssets, Project.StartupLevelLogicalName);
    REQUIRE(StartupRecord != nullptr);
    CHECK(StartupRecord->Cookable);
    CHECK_FALSE(StartupRecord->StageVerbatim);

    const AssetSelectionRecord* ShaderRecord = FindSelectionRecord(Plan->IncludedAssets, "Shaders/MyShader.slang");
    REQUIRE(ShaderRecord != nullptr);
    CHECK(ShaderRecord->SelectionReason == "IncludeAssetKind");
    CHECK(ShaderRecord->AssetKindLabel == ".slang Source");
    CHECK_FALSE(ShaderRecord->Cookable);
    CHECK(ShaderRecord->StageVerbatim);
    CHECK(ShaderRecord->ChunkId.empty());
    CHECK(HasProvenance(*ShaderRecord, "IncludeAssetKind", true));
}

TEST_CASE("AssetCookServiceAdapter resolves cookable include-kind sources without invoking cookers during selection",
          "[Build][AssetCook]")
{
    TempDir Root{};
    const CreatedProject Project = CreateStarterProject(
        Root.Path, "AssetSelectionTextures", [](BuildProfile& Profile) {
            Profile.IncludeAssetKinds.IsSet = true;
            Profile.IncludeAssetKinds.Values = {".texture"};
        });

    TextureAsset Texture{};
    Texture.Image.Width = 1u;
    Texture.Image.Height = 1u;
    Texture.Image.Channels = 4u;
    Texture.Image.BitsPerChannel = 8u;
    Texture.Image.SRGB = true;
    Texture.Image.EncodedBytes = {0x89u, 0x50u, 0x4eu, 0x47u};

    auto TextureJson = SerializeAuthoredAssetToJson(Texture);
    REQUIRE(TextureJson);
    WriteTextFile(Project.AssetRootPath / "Rendering" / "DependencyTexture.texture", *TextureJson);

    BuildRequest Request{};
    Request.ProjectFilePath = Project.ProjectFilePath;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    auto Plan = AssetCookServiceAdapter::ResolveAssetSelection(*Resolved);
    REQUIRE(Plan);

    const AssetSelectionRecord* TextureRecord =
        FindSelectionRecord(Plan->IncludedAssets, "Rendering/DependencyTexture.texture");
    REQUIRE(TextureRecord != nullptr);
    CHECK(TextureRecord->Cookable);
    CHECK_FALSE(TextureRecord->StageVerbatim);
    CHECK(TextureRecord->SelectionReason == "IncludeAssetKind");
    CHECK(HasProvenance(*TextureRecord, "IncludeAssetKind", true));
}

TEST_CASE("BuildExecutionService can drive real asset nodes through AssetCookServiceAdapter", "[Build][AssetCook]")
{
    TempDir Root{};
    const CreatedProject Project = CreateStarterProject(Root.Path, "AssetCookExecution");

    BuildRequest Request{};
    Request.ProjectFilePath = Project.ProjectFilePath;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-060101-assetcook";
    auto Plan = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    REQUIRE(Plan);

    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.AssetCook.Enabled = true;

    auto Report = BuildExecutionService::Execute(*Resolved, *Plan, ExecutionOptions);
    REQUIRE(Report);
    CHECK(Report->Status == EBuildExecutionStatus::Succeeded);

    const std::filesystem::path SnpakPath =
        Report->StageDirectory / "Assets" /
        (Resolved->Project.Descriptor.Project.Name + "_" + Resolved->Profile.Platform + "_Primary.snpak");
    CHECK(std::filesystem::exists(SnpakPath));

    ::SnAPI::AssetPipeline::AssetPackReader Reader{};
    auto OpenResult = Reader.Open(SnpakPath.string());
    REQUIRE(OpenResult.has_value());
    CHECK(Reader.GetAssetCount() >= 1u);
    CHECK_FALSE(Reader.FindAssetsByName(Project.StartupLevelLogicalName).empty());

    const std::filesystem::path CookManifestPath = Report->HistoryDirectory / "Manifests" / "CookManifest.json";
    const std::filesystem::path PackageManifestPath = Report->StageDirectory / "Metadata" / "PackageManifest.json";
    const std::filesystem::path StageFileHashesPath = Report->StageDirectory / "Metadata" / "StageFileHashes.json";
    const std::filesystem::path DefaultGameConfigPath = Report->StageDirectory / "Config" / "DefaultGame.json";
    const std::filesystem::path ResolvedRuntimeConfigPath = Report->StageDirectory / "Config" / "ResolvedRuntimeConfig.json";
    REQUIRE(std::filesystem::exists(CookManifestPath));
    REQUIRE(std::filesystem::exists(PackageManifestPath));
    REQUIRE(std::filesystem::exists(StageFileHashesPath));
    REQUIRE(std::filesystem::exists(DefaultGameConfigPath));
    REQUIRE(std::filesystem::exists(ResolvedRuntimeConfigPath));

    std::ifstream CookManifestStream(CookManifestPath, std::ios::binary);
    REQUIRE(CookManifestStream.is_open());
    const std::string CookManifestText((std::istreambuf_iterator<char>(CookManifestStream)),
                                       std::istreambuf_iterator<char>());

    const nlohmann::ordered_json CookManifest = nlohmann::ordered_json::parse(CookManifestText, nullptr, false);
    REQUIRE_FALSE(CookManifest.is_discarded());
    REQUIRE(CookManifest.contains("Assets"));
    REQUIRE(CookManifest["Assets"].is_array());
    CHECK(CookManifest["Assets"].size() >= 1u);
    CHECK(CookManifest["Assets"][0]["LogicalName"] == Project.StartupLevelLogicalName);
    CHECK(CookManifest["Assets"][0]["ChunkId"] == "Primary");
    REQUIRE(CookManifest["Assets"][0]["Provenance"].is_array());
    CHECK(CookManifest["SnpakFiles"][0]["ChunkId"] == "Primary");

    std::ifstream PackageManifestStream(PackageManifestPath, std::ios::binary);
    REQUIRE(PackageManifestStream.is_open());
    const std::string PackageManifestText((std::istreambuf_iterator<char>(PackageManifestStream)),
                                          std::istreambuf_iterator<char>());
    const nlohmann::ordered_json PackageManifest = nlohmann::ordered_json::parse(PackageManifestText, nullptr, false);
    REQUIRE_FALSE(PackageManifest.is_discarded());
    CHECK(PackageManifest["ProjectName"] == "AssetCookExecution");
    CHECK(PackageManifest["TargetPlatform"] == "Windows");
    REQUIRE(PackageManifest["IncludedLevels"].is_array());
    CHECK(PackageManifest["IncludedLevels"][0] == Project.StartupLevelLogicalName);
    REQUIRE(PackageManifest["SnpakFiles"].is_array());
    REQUIRE(PackageManifest["SnpakFiles"].size() == 1u);
    CHECK(PackageManifest["SnpakFiles"][0]["AssetCount"].get<std::uint64_t>() >= 1u);
    CHECK(PackageManifest["SnpakFiles"][0]["ChunkId"] == "Primary");

    std::ifstream StageHashesStream(StageFileHashesPath, std::ios::binary);
    REQUIRE(StageHashesStream.is_open());
    const std::string StageHashesText((std::istreambuf_iterator<char>(StageHashesStream)),
                                      std::istreambuf_iterator<char>());
    const nlohmann::ordered_json StageHashes = nlohmann::ordered_json::parse(StageHashesText, nullptr, false);
    REQUIRE_FALSE(StageHashes.is_discarded());
    REQUIRE(StageHashes["Files"].is_array());
    CHECK(StageHashes["Files"].size() >= 3u);
}

TEST_CASE("BuildExecutionService stages selected auxiliary files verbatim under Assets", "[Build][AssetCook]")
{
    TempDir Root{};
    const CreatedProject Project = CreateStarterProject(
        Root.Path, "AssetCookAuxiliary", [](BuildProfile& Profile) {
            Profile.IncludeAssetKinds.IsSet = true;
            Profile.IncludeAssetKinds.Values = {".slang"};
        });

    WriteTextFile(Project.AssetRootPath / "Shaders" / "MyShader.slang", "shader-entry {}\n");

    BuildRequest Request{};
    Request.ProjectFilePath = Project.ProjectFilePath;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260323-auxiliary";
    auto Plan = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    REQUIRE(Plan);

    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.AssetCook.Enabled = true;

    auto Report = BuildExecutionService::Execute(*Resolved, *Plan, ExecutionOptions);
    REQUIRE(Report);
    CHECK(Report->Status == EBuildExecutionStatus::Succeeded);

    const std::filesystem::path SnpakPath =
        Report->StageDirectory / "Assets" /
        (Resolved->Project.Descriptor.Project.Name + "_" + Resolved->Profile.Platform + "_Primary.snpak");
    const std::filesystem::path ShaderPath = Report->StageDirectory / "Assets" / "Shaders" / "MyShader.slang";
    REQUIRE(std::filesystem::exists(SnpakPath));
    REQUIRE(std::filesystem::exists(ShaderPath));

    std::ifstream ShaderStream(ShaderPath, std::ios::binary);
    REQUIRE(ShaderStream.is_open());
    const std::string ShaderText((std::istreambuf_iterator<char>(ShaderStream)),
                                 std::istreambuf_iterator<char>());
    CHECK(ShaderText == "shader-entry {}\n");
}

TEST_CASE("BuildExecutionService writes shared and per-level snpak bundles for SharedPlusPerLevel chunking",
          "[Build][AssetCook]")
{
    TempDir Root{};
    const CreatedProject Project = CreateStarterProject(
        Root.Path, "AssetCookChunking", [](BuildProfile& Profile) {
            Profile.IncludeFolders.IsSet = true;
            Profile.IncludeFolders.Values = {"Shared"};
            Profile.ChunkStrategy = SetValue(EAssetChunkStrategy::SharedPlusPerLevel);
        });

    const std::filesystem::path StartupLevelSource = Project.AssetRootPath / Project.StartupLevelLogicalName;
    CopyFile(StartupLevelSource, Project.AssetRootPath / "Shared" / "Shared.level");

    BuildRequest Request{};
    Request.ProjectFilePath = Project.ProjectFilePath;
    Request.ProfileName = "WindowsDevelopment";

    auto Resolved = BuildRequestService::Resolve(Request);
    REQUIRE(Resolved);

    BuildPlannerOptions PlannerOptions{};
    PlannerOptions.BuildId = "20260322-060102-assetchunk";
    auto Plan = BuildPlannerService::CreatePlan(*Resolved, PlannerOptions);
    REQUIRE(Plan);

    BuildExecutionOptions ExecutionOptions{};
    ExecutionOptions.AssetCook.Enabled = true;

    auto Report = BuildExecutionService::Execute(*Resolved, *Plan, ExecutionOptions);
    REQUIRE(Report);
    CHECK(Report->Status == EBuildExecutionStatus::Succeeded);

    const std::filesystem::path SharedPackPath =
        Report->StageDirectory / "Assets" / "AssetCookChunking_Windows_Shared.snpak";
    const std::filesystem::path LevelPackPath =
        Report->StageDirectory / "Assets" /
        ("AssetCookChunking_Windows_" + MakeExpectedLevelChunkId(Project.StartupLevelLogicalName) + ".snpak");
    REQUIRE(std::filesystem::exists(SharedPackPath));
    REQUIRE(std::filesystem::exists(LevelPackPath));

    ::SnAPI::AssetPipeline::AssetPackReader SharedReader{};
    auto SharedOpenResult = SharedReader.Open(SharedPackPath.string());
    REQUIRE(SharedOpenResult.has_value());
    CHECK_FALSE(SharedReader.FindAssetsByName("Shared/Shared.level").empty());

    ::SnAPI::AssetPipeline::AssetPackReader LevelReader{};
    auto LevelOpenResult = LevelReader.Open(LevelPackPath.string());
    REQUIRE(LevelOpenResult.has_value());
    CHECK_FALSE(LevelReader.FindAssetsByName(Project.StartupLevelLogicalName).empty());

    const std::filesystem::path CookManifestPath = Report->HistoryDirectory / "Manifests" / "CookManifest.json";
    const std::filesystem::path PackageManifestPath = Report->StageDirectory / "Metadata" / "PackageManifest.json";
    REQUIRE(std::filesystem::exists(CookManifestPath));
    REQUIRE(std::filesystem::exists(PackageManifestPath));

    std::ifstream CookManifestStream(CookManifestPath, std::ios::binary);
    REQUIRE(CookManifestStream.is_open());
    const std::string CookManifestText((std::istreambuf_iterator<char>(CookManifestStream)),
                                       std::istreambuf_iterator<char>());
    const nlohmann::ordered_json CookManifest = nlohmann::ordered_json::parse(CookManifestText, nullptr, false);
    REQUIRE_FALSE(CookManifest.is_discarded());
    REQUIRE(CookManifest["SnpakFiles"].is_array());
    REQUIRE(CookManifest["SnpakFiles"].size() == 2u);
    CHECK(CookManifest["SnpakFiles"][0]["ChunkId"] == MakeExpectedLevelChunkId(Project.StartupLevelLogicalName));
    CHECK(CookManifest["SnpakFiles"][1]["ChunkId"] == "Shared");

    std::ifstream PackageManifestStream(PackageManifestPath, std::ios::binary);
    REQUIRE(PackageManifestStream.is_open());
    const std::string PackageManifestText((std::istreambuf_iterator<char>(PackageManifestStream)),
                                          std::istreambuf_iterator<char>());
    const nlohmann::ordered_json PackageManifest = nlohmann::ordered_json::parse(PackageManifestText, nullptr, false);
    REQUIRE_FALSE(PackageManifest.is_discarded());
    REQUIRE(PackageManifest["SnpakFiles"].is_array());
    REQUIRE(PackageManifest["SnpakFiles"].size() == 2u);
    CHECK(PackageManifest["SnpakFiles"][0]["ChunkId"] == MakeExpectedLevelChunkId(Project.StartupLevelLogicalName));
    CHECK(PackageManifest["SnpakFiles"][1]["ChunkId"] == "Shared");
}
