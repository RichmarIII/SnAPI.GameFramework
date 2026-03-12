#include "GameProjectRuntime.h"

#include "AssetPipelineFactories.h"
#include "AssetPipelineSerializers.h"
#include "AssetRef.h"
#include "NodeCast.h"
#include "PathResolver.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "WorldRenderSettings.h"
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "CameraComponent.h"

namespace SnAPI::GameFramework
{
namespace
{
constexpr std::string_view kDefaultProjectAssetRoot = "Assets";
constexpr std::string_view kDefaultProjectStartupLevelAsset = "Levels/StarterLevel.level";
constexpr std::uint32_t kProjectConfigVersion = 1u;

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

[[nodiscard]] std::expected<std::string, std::string> JsonParseString(const std::string& Text, std::size_t& Position)
{
    if (Position >= Text.size() || Text[Position] != '"')
    {
        return std::unexpected("Expected JSON string");
    }
    ++Position;

    std::string Output{};
    while (Position < Text.size())
    {
        const char Character = Text[Position++];
        if (Character == '"')
        {
            return Output;
        }
        if (Character != '\\')
        {
            Output.push_back(Character);
            continue;
        }
        if (Position >= Text.size())
        {
            return std::unexpected("Invalid JSON escape sequence");
        }
        const char Escape = Text[Position++];
        switch (Escape)
        {
        case '"':
            Output.push_back('"');
            break;
        case '\\':
            Output.push_back('\\');
            break;
        case '/':
            Output.push_back('/');
            break;
        case 'b':
            Output.push_back('\b');
            break;
        case 'f':
            Output.push_back('\f');
            break;
        case 'n':
            Output.push_back('\n');
            break;
        case 'r':
            Output.push_back('\r');
            break;
        case 't':
            Output.push_back('\t');
            break;
        default:
            return std::unexpected("Unsupported JSON escape sequence");
        }
    }
    return std::unexpected("Unterminated JSON string");
}

[[nodiscard]] bool JsonTryReadStringField(const std::string& Text, const std::string_view Key, std::string& OutValue)
{
    const std::string KeyToken = "\"" + std::string(Key) + "\"";
    std::size_t SearchOffset = 0;
    while (true)
    {
        const std::size_t KeyPos = Text.find(KeyToken, SearchOffset);
        if (KeyPos == std::string::npos)
        {
            return false;
        }

        std::size_t ValuePos = KeyPos + KeyToken.size();
        while (ValuePos < Text.size() && std::isspace(static_cast<unsigned char>(Text[ValuePos])) != 0)
        {
            ++ValuePos;
        }
        if (ValuePos >= Text.size() || Text[ValuePos] != ':')
        {
            SearchOffset = KeyPos + KeyToken.size();
            continue;
        }

        ++ValuePos;
        while (ValuePos < Text.size() && std::isspace(static_cast<unsigned char>(Text[ValuePos])) != 0)
        {
            ++ValuePos;
        }

        auto Parsed = JsonParseString(Text, ValuePos);
        if (!Parsed)
        {
            SearchOffset = KeyPos + KeyToken.size();
            continue;
        }

        OutValue = std::move(*Parsed);
        return true;
    }
}

[[nodiscard]] bool JsonTryReadUnsignedField(const std::string& Text,
                                            const std::string_view Key,
                                            std::uint32_t& OutValue)
{
    const std::string KeyToken = "\"" + std::string(Key) + "\"";
    const std::size_t KeyPos = Text.find(KeyToken);
    if (KeyPos == std::string::npos)
    {
        return false;
    }

    std::size_t ValuePos = Text.find(':', KeyPos + KeyToken.size());
    if (ValuePos == std::string::npos)
    {
        return false;
    }

    ++ValuePos;
    while (ValuePos < Text.size() && std::isspace(static_cast<unsigned char>(Text[ValuePos])) != 0)
    {
        ++ValuePos;
    }

    std::size_t EndPos = ValuePos;
    while (EndPos < Text.size() && std::isdigit(static_cast<unsigned char>(Text[EndPos])) != 0)
    {
        ++EndPos;
    }
    if (EndPos <= ValuePos)
    {
        return false;
    }

    try
    {
        OutValue = static_cast<std::uint32_t>(std::stoul(Text.substr(ValuePos, EndPos - ValuePos)));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

[[nodiscard]] std::string NormalizeProjectPathField(const std::string_view RawValue)
{
    std::string Value = TrimCopy(std::string(RawValue));
    if (Value.empty())
    {
        return {};
    }

    std::replace(Value.begin(), Value.end(), '\\', '/');
    return std::filesystem::path(Value).lexically_normal().generic_string();
}

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

    if (auto LoadProjectResult = LoadProjectMetadata(m_settings.ProjectFilePath); !LoadProjectResult)
    {
        Shutdown();
        return LoadProjectResult;
    }

    if (auto InitRuntimeResult = m_runtime.Init(m_settings.Runtime); !InitRuntimeResult)
    {
        Shutdown();
        return InitRuntimeResult;
    }

    if (auto* WorldPtr = m_runtime.WorldPtr(); WorldPtr && !m_project.Name.empty())
    {
        WorldPtr->Name(m_project.Name);
    }

    const bool RestartGameplayHost = m_settings.Runtime.Gameplay.has_value();
    if (RestartGameplayHost)
    {
        m_runtime.StopGameplayHost();
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

    if (RestartGameplayHost)
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

    std::string ProjectFileText = TrimCopy(std::string(ProjectFilePath));
    if (ProjectFileText.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project file path cannot be empty"));
    }

    std::filesystem::path ResolvedProjectFile(ProjectFileText);
    if (auto Resolved = SPathResolver::Instance().Resolve(ProjectFileText); Resolved)
    {
        ResolvedProjectFile = *Resolved;
    }
    if (!ResolvedProjectFile.is_absolute() && !HasUriScheme(ProjectFileText))
    {
        std::error_code Error{};
        ResolvedProjectFile = std::filesystem::absolute(ResolvedProjectFile, Error);
        if (Error)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to resolve project file path: " + Error.message()));
        }
    }

    std::error_code Error{};
    if (!std::filesystem::exists(ResolvedProjectFile, Error) || Error)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound,
                                         "Project file was not found: " + ResolvedProjectFile.string()));
    }

    std::ifstream Input(ResolvedProjectFile, std::ios::binary);
    if (!Input.is_open())
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to open project file"));
    }

    std::ostringstream Buffer{};
    Buffer << Input.rdbuf();
    const std::string JsonText = Buffer.str();
    if (JsonText.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project file is empty"));
    }

    std::uint32_t Version = kProjectConfigVersion;
    (void)JsonTryReadUnsignedField(JsonText, "version", Version);
    if (Version != kProjectConfigVersion)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported project file version"));
    }

    std::string Name = ResolvedProjectFile.stem().string();
    (void)JsonTryReadStringField(JsonText, "name", Name);
    Name = TrimCopy(Name);
    if (Name.empty())
    {
        Name = "Project";
    }

    std::string AssetRootField = std::string(kDefaultProjectAssetRoot);
    (void)JsonTryReadStringField(JsonText, "assetRoot", AssetRootField);
    AssetRootField = TrimCopy(AssetRootField);
    if (AssetRootField.empty())
    {
        AssetRootField = std::string(kDefaultProjectAssetRoot);
    }
    else if (!HasUriScheme(AssetRootField))
    {
        AssetRootField = NormalizeProjectPathField(AssetRootField);
    }

    std::string StartupLevelAssetField{};
    (void)JsonTryReadStringField(JsonText, "startupLevelAsset", StartupLevelAssetField);
    StartupLevelAssetField = TrimCopy(StartupLevelAssetField);
    if (StartupLevelAssetField.empty())
    {
        std::string LegacyStartupLevelPack{};
        (void)JsonTryReadStringField(JsonText, "startupLevelPack", LegacyStartupLevelPack);
        LegacyStartupLevelPack = TrimCopy(LegacyStartupLevelPack);
        if (!LegacyStartupLevelPack.empty())
        {
            if (!HasUriScheme(LegacyStartupLevelPack))
            {
                LegacyStartupLevelPack = NormalizeProjectPathField(LegacyStartupLevelPack);
                std::filesystem::path LegacyPath(LegacyStartupLevelPack);
                if (LegacyPath.extension() == ".snpak")
                {
                    LegacyPath.replace_extension(".level");
                }
                StartupLevelAssetField = LegacyPath.lexically_normal().generic_string();
            }
            else
            {
                StartupLevelAssetField = LegacyStartupLevelPack;
            }
        }
    }
    if (StartupLevelAssetField.empty())
    {
        StartupLevelAssetField = std::string(kDefaultProjectStartupLevelAsset);
    }
    else if (!HasUriScheme(StartupLevelAssetField))
    {
        StartupLevelAssetField = NormalizeProjectPathField(StartupLevelAssetField);
    }

    std::string DefaultRenderSettingsField{};
    (void)JsonTryReadStringField(JsonText, "defaultRenderSettings", DefaultRenderSettingsField);
    DefaultRenderSettingsField = TrimCopy(DefaultRenderSettingsField);

    const std::filesystem::path ProjectRoot = ResolvedProjectFile.parent_path();

    std::filesystem::path ResolvedAssetRoot(AssetRootField);
    if (HasUriScheme(AssetRootField))
    {
        auto Resolved = SPathResolver::Instance().Resolve(AssetRootField);
        if (!Resolved)
        {
            return std::unexpected(Resolved.error());
        }
        ResolvedAssetRoot = *Resolved;
    }
    else if (!ResolvedAssetRoot.is_absolute())
    {
        ResolvedAssetRoot = ProjectRoot / ResolvedAssetRoot;
    }
    ResolvedAssetRoot = ResolvedAssetRoot.lexically_normal();

    std::filesystem::create_directories(ResolvedAssetRoot, Error);
    if (Error)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Failed to create project asset root directory: " + Error.message()));
    }

    if (auto SetRootResult = SPathResolver::Instance().SetAssetRoot(ResolvedAssetRoot); !SetRootResult)
    {
        return SetRootResult;
    }

    m_project.IsLoaded = true;
    m_project.Name = std::move(Name);
    m_project.ProjectFilePath = ResolvedProjectFile.string();
    m_project.ProjectRootDirectory = ProjectRoot.string();
    m_project.AssetRoot = AssetRootField;
    m_project.AssetRootDirectory = ResolvedAssetRoot.string();
    m_project.StartupLevelAsset = StartupLevelAssetField;
    m_project.DefaultRenderSettingsAssetId = DefaultRenderSettingsField;

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

    std::filesystem::path StartupAssetPath(m_project.StartupLevelAsset);
    if (HasUriScheme(m_project.StartupLevelAsset))
    {
        auto Resolved = SPathResolver::Instance().Resolve(m_project.StartupLevelAsset);
        if (!Resolved)
        {
            return std::unexpected(Resolved.error());
        }
        StartupAssetPath = *Resolved;
    }
    else if (!StartupAssetPath.is_absolute())
    {
        StartupAssetPath = std::filesystem::path(m_project.AssetRootDirectory) / StartupAssetPath;
    }
    StartupAssetPath = StartupAssetPath.lexically_normal();

    std::error_code Error{};
    if (!std::filesystem::exists(StartupAssetPath, Error) || Error)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound,
                                         "Project startup level asset was not found: " + StartupAssetPath.string()));
    }

    const std::string LogicalName =
        BuildSourceLogicalName(std::filesystem::path(m_project.AssetRootDirectory), StartupAssetPath);
    if (LogicalName.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Project startup level asset is not under the configured asset root"));
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
    const auto Lvl = NodeCast<Level>(LoadParams.TargetWorld->Levels()[0].Borrowed());
    const auto CameraNode = Lvl->CreateNode<BaseNode>("DefaultCamera")->Borrowed();
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

    if (!m_defaultRenderSettingsNode.IsNull())
    {
        (void)WorldPtr->DestroyNode(m_defaultRenderSettingsNode);
        m_defaultRenderSettingsNode = {};
    }

    const std::string DefaultSettingsAssetId = TrimCopy(m_project.DefaultRenderSettingsAssetId);
    if (DefaultSettingsAssetId.empty())
    {
        return Ok();
    }

    TAssetRef<WorldRenderSettings> SettingsRef{};
    SettingsRef.EditAssetId() = DefaultSettingsAssetId;

    auto InstantiateResult = SettingsRef.Instantiate(*m_assetManager, *WorldPtr);
    if (!InstantiateResult)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, InstantiateResult.error()));
    }

    m_defaultRenderSettingsNode = *InstantiateResult;
    if (auto* CreatedNode = m_defaultRenderSettingsNode.Borrowed();
        NodeCast<WorldRenderSettings>(CreatedNode) != nullptr)
    {
        (void)WorldPtr->RequestNodeOnCreate(m_defaultRenderSettingsNode);
        return Ok();
    }

    (void)WorldPtr->DestroyNode(m_defaultRenderSettingsNode);
    m_defaultRenderSettingsNode = {};
#endif

    return Ok();
}

} // namespace SnAPI::GameFramework
