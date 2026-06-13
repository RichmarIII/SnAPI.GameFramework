#include "GameProjectRuntime.h"

#include "AssetPipelineFactories.h"
#include "AssetPipelineSerializers.h"
#include "AssetRef.h"
#include "NodeCast.h"
#include "PathResolver.h"
#include "ProjectDescriptor.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "WorldRenderSettings.h"
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "CameraComponent.h"
#include <nlohmann/json.hpp>

namespace SnAPI::GameFramework
{
namespace
{
using Json = nlohmann::ordered_json;

constexpr std::string_view kDefaultProjectFileName = "project.snproj.json";
constexpr std::string_view kPackagedRuntimeConfigFileName = "ResolvedRuntimeConfig.json";

[[nodiscard]] std::string TrimCopy(std::string Value)
{
    while (!Value.empty() && std::isspace(static_cast<unsigned char>(Value.front())) != 0)
    {
        Value.erase(Value.begin());
    }
    while (!Value.empty() && std::isspace(static_cast<unsigned char>(Value.back())) != 0)
    {
        Value.pop_back();
    }
    return Value;
}

[[nodiscard]] bool HasUriScheme(const std::string_view Value)
{
    const std::size_t Delimiter = Value.find("://");
    if (Delimiter == std::string_view::npos || Delimiter == 0)
    {
        return false;
    }

    const unsigned char First = static_cast<unsigned char>(Value.front());
    if (std::isalpha(First) == 0)
    {
        return false;
    }

    for (std::size_t Index = 1; Index < Delimiter; ++Index)
    {
        const unsigned char Character = static_cast<unsigned char>(Value[Index]);
        if (std::isalnum(Character) != 0 || Character == '+' || Character == '-' || Character == '.')
        {
            continue;
        }
        return false;
    }

    return true;
}

#if defined(SNAPI_GF_ENABLE_RENDERER)
struct WorldRenderSettingsRootSet
{
    std::vector<NodeHandle> AuthoredRoots{};
    std::vector<NodeHandle> TransientRoots{};
};

[[nodiscard]] WorldRenderSettingsRootSet CollectWorldRenderSettingsRoots(World& WorldRef)
{
    WorldRenderSettingsRootSet Result{};
    WorldRef.ForEachNode([&Result](const NodeHandle& Handle, BaseNode& Node) {
        if (NodeCast<WorldRenderSettings>(&Node) == nullptr)
        {
            return;
        }

        if (Node.EditorTransient())
        {
            Result.TransientRoots.push_back(Handle);
        }
        else
        {
            Result.AuthoredRoots.push_back(Handle);
        }
    });
    return Result;
}

void DestroyNodes(World& WorldRef, std::vector<NodeHandle>& Handles)
{
    for (NodeHandle& Handle : Handles)
    {
        (void)WorldRef.DestroyNode(Handle);
    }
}
#endif

[[nodiscard]] std::string NormalizeAssetLogicalName(std::string_view RawName)
{
    std::string Name(RawName);
    std::replace(Name.begin(), Name.end(), '\\', '/');

    while (!Name.empty() && std::isspace(static_cast<unsigned char>(Name.front())) != 0)
    {
        Name.erase(Name.begin());
    }
    while (!Name.empty() && std::isspace(static_cast<unsigned char>(Name.back())) != 0)
    {
        Name.pop_back();
    }

    while (Name.find("//") != std::string::npos)
    {
        Name.replace(Name.find("//"), 2u, "/");
    }
    while (Name.rfind("./", 0) == 0)
    {
        Name.erase(0, 2u);
    }
    while (!Name.empty() && Name.front() == '/')
    {
        Name.erase(Name.begin());
    }

    if (Name == ".")
    {
        Name.clear();
    }

    return Name;
}

[[nodiscard]] TExpected<std::string> ReadTextFile(const std::filesystem::path& FilePath)
{
    std::ifstream Input(FilePath, std::ios::binary);
    if (!Input.is_open())
    {
        return std::unexpected(
            MakeError(EErrorCode::NotFound, "Failed to open runtime bootstrap file: " + FilePath.string()));
    }

    return std::string((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
}

[[nodiscard]] TExpected<Json> ReadJsonFile(const std::filesystem::path& FilePath)
{
    auto Text = ReadTextFile(FilePath);
    if (!Text)
    {
        return std::unexpected(Text.error());
    }

    Json Root = Json::parse(*Text, nullptr, false);
    if (Root.is_discarded())
    {
        return std::unexpected(
            MakeError(EErrorCode::InvalidArgument, "Runtime bootstrap file is not valid JSON: " + FilePath.string()));
    }

    return Root;
}

[[nodiscard]] std::string ReadOptionalStringField(const Json& Root,
                                                  const std::string_view Key,
                                                  const std::string_view DefaultValue = {})
{
    const auto It = Root.find(std::string(Key));
    if (It == Root.end() || It->is_null())
    {
        return std::string(DefaultValue);
    }

    if (It->is_string())
    {
        return TrimCopy(It->get<std::string>());
    }

    return std::string(DefaultValue);
}

[[nodiscard]] const Json* ReadObjectField(const Json& Root, const std::string_view Key)
{
    const auto It = Root.find(std::string(Key));
    if (It == Root.end() || !It->is_object())
    {
        return nullptr;
    }

    return std::addressof(*It);
}

[[nodiscard]] bool IsPackagedRuntimeBootstrapPath(const std::filesystem::path& FilePath)
{
    return FilePath.filename() == kPackagedRuntimeConfigFileName;
}

[[nodiscard]] TExpected<std::filesystem::path> ResolveBootstrapPath(std::filesystem::path InputPath)
{
    if (InputPath.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Runtime bootstrap path is empty"));
    }

    std::error_code Error{};
    const std::filesystem::path AbsoluteInput = std::filesystem::absolute(InputPath, Error);
    if (!Error)
    {
        InputPath = AbsoluteInput;
    }

    Error.clear();
    if (std::filesystem::is_directory(InputPath, Error) && !Error)
    {
        const std::filesystem::path ProjectDescriptorPath = InputPath / std::string(kDefaultProjectFileName);
        if (std::filesystem::exists(ProjectDescriptorPath, Error) && !Error)
        {
            return ProjectDescriptorPath.lexically_normal();
        }

        Error.clear();
        const std::filesystem::path PackagedConfigPath =
            InputPath / "Config" / std::string(kPackagedRuntimeConfigFileName);
        if (std::filesystem::exists(PackagedConfigPath, Error) && !Error)
        {
            return PackagedConfigPath.lexically_normal();
        }

        return std::unexpected(MakeError(EErrorCode::NotFound,
                                         "Runtime bootstrap directory does not contain `" +
                                             std::string(kDefaultProjectFileName) + "` or `Config/" +
                                             std::string(kPackagedRuntimeConfigFileName) + "`"));
    }

    Error.clear();
    if (!std::filesystem::exists(InputPath, Error) || Error)
    {
        return std::unexpected(
            MakeError(EErrorCode::NotFound, "Runtime bootstrap file was not found: " + InputPath.string()));
    }

    return InputPath.lexically_normal();
}

[[nodiscard]] TExpected<GameProjectInfo> LoadPackagedRuntimeBootstrap(const std::filesystem::path& BootstrapPath)
{
    auto Root = ReadJsonFile(BootstrapPath);
    if (!Root)
    {
        return std::unexpected(Root.error());
    }

    const Json* Startup = ReadObjectField(*Root, "Startup");
    if (Startup == nullptr)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Packaged runtime bootstrap is missing the `Startup` object"));
    }

    const std::string ProjectName = ReadOptionalStringField(*Root, "ProjectName");
    const std::string AssetRoot = ReadOptionalStringField(*Root, "AssetRoot", "Assets");
    const std::string StartupLevelAsset = ReadOptionalStringField(*Startup, "StartupLevelAsset");
    if (ProjectName.empty())
    {
        return std::unexpected(
            MakeError(EErrorCode::InvalidArgument, "Packaged runtime bootstrap is missing `ProjectName`"));
    }
    if (AssetRoot.empty())
    {
        return std::unexpected(
            MakeError(EErrorCode::InvalidArgument, "Packaged runtime bootstrap is missing `AssetRoot`"));
    }
    if (StartupLevelAsset.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Packaged runtime bootstrap is missing `Startup.StartupLevelAsset`"));
    }

    const std::filesystem::path StageRootDirectory = BootstrapPath.parent_path().parent_path().lexically_normal();
    const std::filesystem::path AssetRootDirectory = (StageRootDirectory / AssetRoot).lexically_normal();
    std::error_code Error{};
    if (!std::filesystem::exists(AssetRootDirectory, Error) || Error)
    {
        return std::unexpected(
            MakeError(EErrorCode::NotFound, "Packaged asset root was not found: " + AssetRootDirectory.string()));
    }

    return GameProjectInfo{
        .IsLoaded = true,
        .Name = ProjectName,
        .ProjectFilePath = BootstrapPath.string(),
        .ProjectRootDirectory = StageRootDirectory.string(),
        .AssetRoot = AssetRoot,
        .AssetRootDirectory = AssetRootDirectory.string(),
        .StartupLevelAsset = StartupLevelAsset,
        .DefaultRenderSettingsAssetId = ReadOptionalStringField(*Startup, "DefaultRenderSettingsAssetId"),
    };
}

[[nodiscard]] std::string BuildSourceLogicalName(const std::filesystem::path& AssetRoot,
                                                 const std::filesystem::path& SourceFile)
{
    std::error_code Error{};
    std::filesystem::path RelativePath = std::filesystem::relative(SourceFile, AssetRoot, Error);
    if (Error)
    {
        RelativePath = SourceFile.filename();
    }
    return NormalizeAssetLogicalName(RelativePath.generic_string());
}

void AppendUniquePath(std::vector<std::string>& Paths,
                      std::unordered_set<std::string>& SeenPaths,
                      const std::filesystem::path& InputPath)
{
    if (InputPath.empty())
    {
        return;
    }

    std::error_code Error{};
    std::filesystem::path PathToUse = InputPath;

    if (auto ResolvedPath = SPathResolver::Instance().Resolve(InputPath.string()); ResolvedPath)
    {
        PathToUse = *ResolvedPath;
    }

    const std::filesystem::path Canonical = std::filesystem::weakly_canonical(PathToUse, Error);
    if (!Error)
    {
        PathToUse = Canonical;
    }
    else
    {
        Error.clear();
        const std::filesystem::path Absolute = std::filesystem::absolute(PathToUse, Error);
        if (!Error)
        {
            PathToUse = Absolute;
        }
    }

    Error.clear();
    if (!std::filesystem::exists(PathToUse, Error) || Error)
    {
        return;
    }

    Error.clear();
    if (!std::filesystem::is_directory(PathToUse, Error) || Error)
    {
        return;
    }

    std::string Key = PathToUse.generic_string();
    std::transform(Key.begin(), Key.end(), Key.begin(), [](const unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });

    if (!SeenPaths.insert(Key).second)
    {
        return;
    }

    Paths.push_back(PathToUse.string());
}

[[nodiscard]] std::vector<std::string> ParsePackSearchPathEnv(const std::string_view Raw)
{
    std::vector<std::string> Paths{};
    std::string Token{};
    Token.reserve(Raw.size());

    for (const char Character : Raw)
    {
        if (Character == ';' || Character == ':')
        {
            if (!Token.empty())
            {
                Paths.push_back(Token);
                Token.clear();
            }
            continue;
        }

        Token.push_back(Character);
    }

    if (!Token.empty())
    {
        Paths.push_back(Token);
    }

    return Paths;
}

[[nodiscard]] std::vector<std::string> BuildPackSearchPaths(const GameProjectInfo& Project)
{
    std::vector<std::string> Paths{};
    std::unordered_set<std::string> SeenPaths{};

    if (!Project.AssetRootDirectory.empty())
    {
        const std::filesystem::path AssetRoot(Project.AssetRootDirectory);
        AppendUniquePath(Paths, SeenPaths, AssetRoot);
        AppendUniquePath(Paths, SeenPaths, AssetRoot / "Packs");
    }

    if (!Project.ProjectRootDirectory.empty())
    {
        const std::filesystem::path ProjectRoot(Project.ProjectRootDirectory);
        AppendUniquePath(Paths, SeenPaths, ProjectRoot);
        AppendUniquePath(Paths, SeenPaths, ProjectRoot / "Packs");
    }

    std::error_code Error{};
    const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
    if (!Error)
    {
        AppendUniquePath(Paths, SeenPaths, CurrentPath);
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "Content");
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "Assets");
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "Packs");
        AppendUniquePath(Paths, SeenPaths, CurrentPath / "build");
    }

    if (const char* EnvRaw = std::getenv("SNAPI_GF_RUNTIME_ASSET_PATHS"))
    {
        const auto ExtraPaths = ParsePackSearchPathEnv(std::string_view(EnvRaw));
        for (const std::string& Path : ExtraPaths)
        {
            AppendUniquePath(Paths, SeenPaths, std::filesystem::path(Path));
        }
    }

    return Paths;
}

#if defined(SNAPI_GF_ENABLE_RENDERER)
void ConfigureRendererShaderSearchRootForProject(GameRuntime& RuntimeRef, const GameProjectInfo& Project)
{
    auto* WorldPtr = RuntimeRef.WorldPtr();
    if (!WorldPtr || Project.AssetRootDirectory.empty() || !WorldPtr->Renderer().IsInitialized())
    {
        return;
    }

    (void)WorldPtr->Renderer().SetProjectShaderSearchRoot(Project.AssetRootDirectory);
}
#endif
} // namespace

Result GameProjectRuntime::Initialize(const GameProjectRuntimeSettings& Settings)
{
    Shutdown();
    m_settings = Settings;

    const std::string BootstrapPath = !TrimCopy(m_settings.BootstrapPath).empty() ? m_settings.BootstrapPath
                                                                                   : m_settings.ProjectFilePath;
    if (auto LoadProjectResult = LoadProjectMetadata(BootstrapPath); !LoadProjectResult)
    {
        Shutdown();
        return LoadProjectResult;
    }

    GameRuntimeSettings RuntimeSettings = m_settings.Runtime;
    const bool StartGameplayAfterBootstrap = RuntimeSettings.Gameplay.has_value();
    if (StartGameplayAfterBootstrap)
    {
        // Project bootstrap needs the world, asset manager, startup level, and optional
        // project defaults loaded before gameplay systems create players or possession state.
        RuntimeSettings.AutoStartGameplay = false;
    }

    if (auto InitRuntimeResult = m_runtime.Init(RuntimeSettings); !InitRuntimeResult)
    {
        Shutdown();
        return InitRuntimeResult;
    }

    if (auto* WorldPtr = m_runtime.WorldPtr(); WorldPtr && !m_project.Name.empty())
    {
        WorldPtr->Name(m_project.Name);
    }

    if (auto AssetManagerResult = CreateAssetManager(); !AssetManagerResult)
    {
        Shutdown();
        return AssetManagerResult;
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    ConfigureRendererShaderSearchRootForProject(m_runtime, m_project);
#endif

    if (auto StartupResult = LoadStartupLevel(); !StartupResult)
    {
        Shutdown();
        return StartupResult;
    }

    if (auto DefaultSettingsResult = LoadDefaultRenderSettings(); !DefaultSettingsResult)
    {
        // Allowed to fail.
    }

    if (StartGameplayAfterBootstrap && m_runtime.WorldPtr() != nullptr && m_runtime.WorldPtr()->ShouldRunGameplay())
    {
        if (auto GameplayResult = m_runtime.StartGameplayHost(); !GameplayResult)
        {
            Shutdown();
            return GameplayResult;
        }
    }

    m_initialized = true;
    return Ok();
}

void GameProjectRuntime::Shutdown()
{
    m_initialized = false;
    m_defaultRenderSettingsNode = {};

    m_runtime.Shutdown();

    if (ResolveDefaultAssetManager() == m_assetManager.get())
    {
        ClearDefaultAssetManagerResolver();
    }
    m_assetManager.reset();

    m_project = {};
    m_settings = {};
}

bool GameProjectRuntime::IsInitialized() const
{
    return m_initialized && m_project.IsLoaded && m_runtime.IsInitialized();
}

bool GameProjectRuntime::Update(const float DeltaSeconds)
{
    if (!IsInitialized())
    {
        return false;
    }

    return m_runtime.Update(DeltaSeconds);
}

GameRuntime& GameProjectRuntime::Runtime()
{
    return m_runtime;
}

const GameRuntime& GameProjectRuntime::Runtime() const
{
    return m_runtime;
}

const GameProjectInfo& GameProjectRuntime::Project() const
{
    return m_project;
}

::SnAPI::AssetPipeline::AssetManager* GameProjectRuntime::AssetManager() const
{
    return m_assetManager.get();
}

Result GameProjectRuntime::LoadProjectMetadata(const std::string_view ProjectFilePath)
{
    m_project = {};

    auto BootstrapPath = ResolveBootstrapPath(std::filesystem::path(ProjectFilePath));
    if (!BootstrapPath)
    {
        return std::unexpected(BootstrapPath.error());
    }

    if (IsPackagedRuntimeBootstrapPath(*BootstrapPath))
    {
        auto PackagedProject = LoadPackagedRuntimeBootstrap(*BootstrapPath);
        if (!PackagedProject)
        {
            return std::unexpected(PackagedProject.error());
        }

        if (auto SetRootResult = SPathResolver::Instance().SetAssetRoot(PackagedProject->AssetRootDirectory); !SetRootResult)
        {
            return SetRootResult;
        }

        m_project = std::move(*PackagedProject);
        return Ok();
    }

    auto ResolvedProject = ProjectDescriptorService::LoadResolved(BootstrapPath->string());
    if (!ResolvedProject)
    {
        return std::unexpected(ResolvedProject.error());
    }

    std::error_code Error{};
    std::filesystem::create_directories(ResolvedProject->AssetRootDirectory, Error);
    if (Error)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create project asset root directory: " + Error.message()));
    }

    if (auto SetRootResult = SPathResolver::Instance().SetAssetRoot(ResolvedProject->AssetRootDirectory); !SetRootResult)
    {
        return SetRootResult;
    }

    m_project.IsLoaded = true;
    m_project.Name = ResolvedProject->Descriptor.Project.Name;
    m_project.ProjectFilePath = ResolvedProject->ProjectFilePath.string();
    m_project.ProjectRootDirectory = ResolvedProject->ProjectRootDirectory.string();
    m_project.AssetRoot = ResolvedProject->Descriptor.Paths.AssetRoot;
    m_project.AssetRootDirectory = ResolvedProject->AssetRootDirectory.string();
    m_project.StartupLevelAsset = ResolvedProject->Descriptor.Startup.StartupLevelAsset;
    m_project.DefaultRenderSettingsAssetId = ResolvedProject->Descriptor.Startup.DefaultRenderSettingsAssetId;

    return Ok();
}

Result GameProjectRuntime::CreateAssetManager()
{
    if (!m_project.IsLoaded || m_project.AssetRootDirectory.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Project metadata has not been loaded"));
    }

    if (ResolveDefaultAssetManager() == m_assetManager.get())
    {
        ClearDefaultAssetManagerResolver();
    }
    m_assetManager.reset();

    ::SnAPI::AssetPipeline::AssetManagerConfig Config{};
    Config.bEnableSourceAssets = true;
    Config.SourceRoots.push_back(::SnAPI::AssetPipeline::SourceMountConfig{
        .RootPath = m_project.AssetRootDirectory,
        .Priority = 0,
        .MountPoint = "",
    });
    Config.PackSearchPaths = BuildPackSearchPaths(m_project);

    m_assetManager = std::make_unique<::SnAPI::AssetPipeline::AssetManager>(Config);
    SetDefaultAssetManagerResolver([this]() -> ::SnAPI::AssetPipeline::AssetManager* {
        return m_assetManager.get();
    });

    RegisterAssetPipelinePayloads(m_assetManager->GetRegistry());
    RegisterAssetPipelineFactories(*m_assetManager);
    RegisterAssetPipelineSourceStages(*m_assetManager);
    return Ok();
}

Result GameProjectRuntime::LoadStartupLevel()
{
    if (!m_assetManager)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Project asset manager is not initialized"));
    }

    auto* WorldPtr = m_runtime.WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    WorldPtr->Clear();
    m_defaultRenderSettingsNode = {};

    const std::string StartupAssetField = TrimCopy(m_project.StartupLevelAsset);
    if (StartupAssetField.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project startup level asset is empty"));
    }

    std::string LogicalName = NormalizeAssetLogicalName(StartupAssetField);
    if (HasUriScheme(StartupAssetField))
    {
        auto Resolved = SPathResolver::Instance().Resolve(StartupAssetField);
        if (!Resolved)
        {
            return std::unexpected(Resolved.error());
        }

        const std::filesystem::path StartupAssetPath = Resolved->lexically_normal();
        std::error_code Error{};
        if (!std::filesystem::exists(StartupAssetPath, Error) || Error)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound,
                                             "Project startup level asset was not found: " +
                                                 StartupAssetPath.string()));
        }

        LogicalName = BuildSourceLogicalName(std::filesystem::path(m_project.AssetRootDirectory), StartupAssetPath);
    }
    else if (!m_project.AssetRootDirectory.empty())
    {
        std::filesystem::path StartupAssetPath(StartupAssetField);
        if (!StartupAssetPath.is_absolute())
        {
            StartupAssetPath = std::filesystem::path(m_project.AssetRootDirectory) / StartupAssetPath;
        }
        StartupAssetPath = StartupAssetPath.lexically_normal();

        std::error_code Error{};
        if (std::filesystem::exists(StartupAssetPath, Error) && !Error)
        {
            LogicalName = BuildSourceLogicalName(std::filesystem::path(m_project.AssetRootDirectory), StartupAssetPath);
        }
    }

    if (LogicalName.empty())
    {
        return std::unexpected(
            MakeError(EErrorCode::InvalidArgument, "Project startup level asset could not be resolved to a logical name"));
    }

    LevelAssetLoadParams LoadParams{};
    LoadParams.TargetWorld = WorldPtr;
    LoadParams.NameOverride = std::string("Level");
    auto LoadResult = m_assetManager->Load<Level>(LogicalName, LoadParams);
    if (!LoadResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, LoadResult.error()));
    }
    // IMPORTANT: When loading Node types, the returned node is essentially
    // a duplicate and should not be used.
    // the real node was added to TargetWorld,
    // It's a quirk of retrofitting Node load serialization
    // into the AssetManager loading api

    // Create a default camera and enable it
    NodeHandle LevelHandle = LoadParams.TargetWorld->Levels()[0];
    const auto Lvl = NodeCast<Level>(LoadParams.TargetWorld->BorrowedNode(LevelHandle));
    NodeHandle CameraHandle = *Lvl->CreateNode<BaseNode>("DefaultCamera");
    const auto CameraNode = LoadParams.TargetWorld->BorrowedNode(CameraHandle);
    auto CameraComp = CameraNode->Add<CameraComponent>();
    CameraComp->SetActive(true);

    return Ok();
}

Result GameProjectRuntime::LoadDefaultRenderSettings()
{
#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto* WorldPtr = m_runtime.WorldPtr();
    if (!WorldPtr || !m_assetManager || !WorldPtr->Renderer().IsInitialized())
    {
        return Ok();
    }

    const std::string DefaultSettingsAssetId = TrimCopy(m_project.DefaultRenderSettingsAssetId);
    if (DefaultSettingsAssetId.empty())
    {
        WorldRenderSettingsRootSet ExistingRoots = CollectWorldRenderSettingsRoots(*WorldPtr);
        DestroyNodes(*WorldPtr, ExistingRoots.TransientRoots);
        m_defaultRenderSettingsNode = {};
        return Ok();
    }

    WorldRenderSettingsRootSet ExistingRoots = CollectWorldRenderSettingsRoots(*WorldPtr);
    if (!ExistingRoots.AuthoredRoots.empty())
    {
        DestroyNodes(*WorldPtr, ExistingRoots.TransientRoots);
        m_defaultRenderSettingsNode = {};
        return Ok();
    }

    if (!ExistingRoots.TransientRoots.empty())
    {
        m_defaultRenderSettingsNode = ExistingRoots.TransientRoots.front();
        for (std::size_t Index = 1; Index < ExistingRoots.TransientRoots.size(); ++Index)
        {
            NodeHandle Duplicate = ExistingRoots.TransientRoots[Index];
            (void)WorldPtr->DestroyNode(Duplicate);
        }

        if (auto* ExistingNode = WorldPtr->BorrowedNode(m_defaultRenderSettingsNode);
            NodeCast<WorldRenderSettings>(ExistingNode) != nullptr)
        {
            ExistingNode->EditorTransient(true);
            return Ok();
        }

        m_defaultRenderSettingsNode = {};
    }

    TAssetRef<WorldRenderSettings> SettingsRef{};
    SettingsRef.EditAssetId() = DefaultSettingsAssetId;

    auto InstantiateResult = SettingsRef.Instantiate(*m_assetManager, *WorldPtr);
    if (!InstantiateResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, InstantiateResult.error()));
    }

    m_defaultRenderSettingsNode = *InstantiateResult;
    if (auto* CreatedNode = WorldPtr->BorrowedNode(m_defaultRenderSettingsNode);
        NodeCast<WorldRenderSettings>(CreatedNode) != nullptr)
    {
        CreatedNode->EditorTransient(true);
        (void)WorldPtr->RequestNodeOnCreate(m_defaultRenderSettingsNode);
        return Ok();
    }

    (void)WorldPtr->DestroyNode(m_defaultRenderSettingsNode);
    m_defaultRenderSettingsNode = {};
#endif

    return Ok();
}

} // namespace SnAPI::GameFramework
