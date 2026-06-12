#include "PluginCreationService.h"

#include "ModuleScaffoldingShared.h"
#include "ProjectBuildGenerationShared.h"

#include <ranges>
#include <utility>

namespace SnAPI::GameFramework
{
namespace
{

/**
 * @brief Return `true` when the descriptor already contains one module with the requested name.
 * @param Descriptor Plugin descriptor to inspect.
 * @param ModuleName Stable module name to find.
 * @return `true` when a matching module already exists.
 */
[[nodiscard]] bool HasModuleDescriptor(const PluginDescriptor& Descriptor, const std::string_view ModuleName)
{
    return std::ranges::any_of(Descriptor.Modules, [&](const ProjectModuleDescriptor& Module) {
        return Module.Name == ModuleName;
    });
}

/**
 * @brief Return the build-integration naming used by plugin workspaces.
 * @return Plugin build-integration settings.
 */
[[nodiscard]] Detail::BuildIntegrationSettings BuildPluginIntegrationSettings()
{
    return Detail::BuildIntegrationSettings{
        .RootVariableName = "SNAPI_PLUGIN_ROOT_DIR",
        .GeneratedModulesFileName = "PluginModules.cmake",
        .HostDisplayName = "plugin",
    };
}

/**
 * @brief Normalize one caller-supplied plugin-file name.
 * @param PluginFileName Raw request field.
 * @return Relative descriptor file path or an error.
 */
[[nodiscard]] TExpected<std::filesystem::path> NormalizePluginFileName(const std::filesystem::path& PluginFileName)
{
    std::filesystem::path Normalized = PluginFileName;
    if (Normalized.empty())
    {
        Normalized = std::filesystem::path(PluginDescriptorService::kDefaultPluginFileName);
    }

    if (Normalized.is_absolute())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Plugin descriptor path must be relative to the plugin root"));
    }

    return Normalized.lexically_normal();
}

/**
 * @brief Resolve one parent directory to an absolute normalized path.
 * @param ParentDirectory Raw request directory.
 * @return Absolute parent directory or an error.
 */
[[nodiscard]] TExpected<std::filesystem::path> ResolveParentDirectory(const std::filesystem::path& ParentDirectory)
{
    if (ParentDirectory.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Plugin parent directory cannot be empty"));
    }

    std::error_code Error{};
    std::filesystem::path AbsoluteParent = ParentDirectory;
    if (!AbsoluteParent.is_absolute())
    {
        AbsoluteParent = std::filesystem::absolute(AbsoluteParent, Error);
        if (Error)
        {
            return std::unexpected(
                MakeError(EErrorCode::InternalError,
                          "Failed to resolve plugin parent directory: " + Error.message()));
        }
    }

    return AbsoluteParent.lexically_normal();
}

/**
 * @brief Normalize one plugin descriptor seed for workspace-creation defaults.
 * @param PluginName Stable plugin name supplied by the request.
 * @param Descriptor Seed descriptor to normalize in place.
 */
void NormalizeDescriptorDefaults(const std::string& PluginName, PluginDescriptor& Descriptor)
{
    Descriptor.Format.SchemaVersion = PluginDescriptorService::kCurrentSchemaVersion;
    if (Descriptor.Format.MinimumToolVersion.empty())
    {
        Descriptor.Format.MinimumToolVersion = std::string(PluginDescriptorService::kDefaultMinimumToolVersion);
    }

    if (Descriptor.Plugin.Name.empty())
    {
        Descriptor.Plugin.Name = PluginName;
    }
    if (Descriptor.Plugin.DisplayName.empty())
    {
        Descriptor.Plugin.DisplayName = Descriptor.Plugin.Name;
    }
    if (Descriptor.Plugin.Version.empty())
    {
        Descriptor.Plugin.Version = std::string(PluginDescriptorService::kDefaultPluginVersion);
    }

    if (Descriptor.Paths.AssetRoot.empty())
    {
        Descriptor.Paths.AssetRoot = std::string(PluginDescriptorService::kDefaultAssetRoot);
    }
    if (Descriptor.Paths.CodeRoot.empty())
    {
        Descriptor.Paths.CodeRoot = std::string(PluginDescriptorService::kDefaultCodeRoot);
    }
    if (Descriptor.Paths.ConfigRoot.empty())
    {
        Descriptor.Paths.ConfigRoot = std::string(PluginDescriptorService::kDefaultConfigRoot);
    }
    if (Descriptor.Paths.IntermediateRoot.empty())
    {
        Descriptor.Paths.IntermediateRoot = std::string(PluginDescriptorService::kDefaultIntermediateRoot);
    }
    if (Descriptor.Paths.SavedRoot.empty())
    {
        Descriptor.Paths.SavedRoot = std::string(PluginDescriptorService::kDefaultSavedRoot);
    }
}

/**
 * @brief Build one runtime-plugin module scaffold request from the plugin creation request.
 * @param Request Original plugin-creation request.
 * @param Descriptor Normalized plugin descriptor seed.
 * @param PluginRoot Resolved plugin root directory.
 * @return Generic module-scaffold request or an error.
 */
[[nodiscard]] TExpected<Detail::ModuleScaffoldOptions> BuildRuntimeModuleOptions(
    const PluginCreationRequest& Request,
    const PluginDescriptor& Descriptor,
    const std::filesystem::path& PluginRoot)
{
    const std::string ModuleName = Request.Code.RuntimeModuleName.empty() ? Request.PluginName : Request.Code.RuntimeModuleName;
    const std::string NamespaceRoot = Request.Code.NamespaceRoot.empty() ? ModuleName : Request.Code.NamespaceRoot;

    Detail::ModuleScaffoldOptions Options{};
    Options.ModuleName = ModuleName;
    Options.ModuleType = EProjectModuleType::Runtime;
    Options.CodeRootField = Descriptor.Paths.CodeRoot;
    Options.WorkspaceRootDirectory = PluginRoot;
    Options.NamespaceRoot = NamespaceRoot;
    return Options;
}

/**
 * @brief Build one editor-plugin module scaffold request from the plugin creation request.
 * @param Request Original plugin-creation request.
 * @param Descriptor Normalized plugin descriptor seed.
 * @param PluginRoot Resolved plugin root directory.
 * @return Generic module-scaffold request or an error.
 */
[[nodiscard]] TExpected<Detail::ModuleScaffoldOptions> BuildEditorModuleOptions(
    const PluginCreationRequest& Request,
    const PluginDescriptor& Descriptor,
    const std::filesystem::path& PluginRoot,
    const bool IncludeRuntimeDependency)
{
    const std::string RuntimeModuleName = Request.Code.RuntimeModuleName.empty() ? Request.PluginName : Request.Code.RuntimeModuleName;
    const std::string ModuleName =
        Request.Code.EditorModuleName.empty() ? (RuntimeModuleName + "Editor") : Request.Code.EditorModuleName;
    const std::string NamespaceRoot = Request.Code.NamespaceRoot.empty() ? RuntimeModuleName : Request.Code.NamespaceRoot;

    Detail::ModuleScaffoldOptions Options{};
    Options.ModuleName = ModuleName;
    Options.ModuleType = EProjectModuleType::Editor;
    Options.CodeRootField = Descriptor.Paths.CodeRoot;
    Options.WorkspaceRootDirectory = PluginRoot;
    Options.NamespaceRoot = NamespaceRoot;
    if (IncludeRuntimeDependency)
    {
        Detail::EnsureDependency(Options.PrivateDependencies, RuntimeModuleName);
    }
    return Options;
}

} // namespace

TExpected<PluginDescriptor> PluginCreationService::BuildDefaultDescriptor(const std::string_view PluginName)
{
    const std::string Name = Detail::TrimCopy(PluginName);
    if (Name.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Plugin name cannot be empty"));
    }

    PluginDescriptor Descriptor{};
    NormalizeDescriptorDefaults(Name, Descriptor);
    return Descriptor;
}

Result PluginCreationService::CreatePlugin(const PluginCreationRequest& Request, PluginCreationResult* OutResult)
{
    const std::string Name = Detail::TrimCopy(Request.PluginName);
    if (Name.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Plugin name cannot be empty"));
    }

    auto ParentDirectoryResult = ResolveParentDirectory(Request.ParentDirectory);
    if (!ParentDirectoryResult)
    {
        return std::unexpected(ParentDirectoryResult.error());
    }

    auto PluginFileNameResult = NormalizePluginFileName(Request.PluginFileName);
    if (!PluginFileNameResult)
    {
        return std::unexpected(PluginFileNameResult.error());
    }

    PluginDescriptor Descriptor = Request.Descriptor;
    NormalizeDescriptorDefaults(Name, Descriptor);

    const std::filesystem::path PluginRoot = ParentDirectoryResult.value() / Name;
    bool WriteStarterRuntimeModule = false;
    Detail::ModuleScaffoldOptions RuntimeModuleOptions{};
    Detail::ModuleLayout RuntimeModuleLayout{};
    if (Request.Code.CreateStarterRuntimeModule)
    {
        auto RuntimeModuleOptionsResult = BuildRuntimeModuleOptions(Request, Descriptor, PluginRoot);
        if (!RuntimeModuleOptionsResult)
        {
            return std::unexpected(RuntimeModuleOptionsResult.error());
        }

        RuntimeModuleOptions = std::move(*RuntimeModuleOptionsResult);
        auto RuntimeModuleLayoutResult = Detail::BuildModuleLayout(RuntimeModuleOptions);
        if (!RuntimeModuleLayoutResult)
        {
            return std::unexpected(RuntimeModuleLayoutResult.error());
        }

        RuntimeModuleLayout = std::move(*RuntimeModuleLayoutResult);
        if (HasModuleDescriptor(Descriptor, RuntimeModuleLayout.ModuleName))
        {
            return std::unexpected(
                MakeError(EErrorCode::AlreadyExists,
                          "Plugin module '" + RuntimeModuleLayout.ModuleName + "' already exists"));
        }
        if (Result ExistingFileResult = Detail::EnsureModuleFilesDoNotExist(RuntimeModuleLayout); !ExistingFileResult)
        {
            return ExistingFileResult;
        }
        Descriptor.Modules.push_back(Detail::BuildModuleDescriptor(RuntimeModuleOptions, RuntimeModuleLayout));
        WriteStarterRuntimeModule = true;
    }

    bool WriteStarterEditorModule = false;
    Detail::ModuleScaffoldOptions EditorModuleOptions{};
    Detail::ModuleLayout EditorModuleLayout{};
    if (Request.Code.CreateStarterEditorModule)
    {
        auto EditorModuleOptionsResult = BuildEditorModuleOptions(Request, Descriptor, PluginRoot, WriteStarterRuntimeModule);
        if (!EditorModuleOptionsResult)
        {
            return std::unexpected(EditorModuleOptionsResult.error());
        }

        EditorModuleOptions = std::move(*EditorModuleOptionsResult);
        auto EditorModuleLayoutResult = Detail::BuildModuleLayout(EditorModuleOptions);
        if (!EditorModuleLayoutResult)
        {
            return std::unexpected(EditorModuleLayoutResult.error());
        }

        EditorModuleLayout = std::move(*EditorModuleLayoutResult);
        if (HasModuleDescriptor(Descriptor, EditorModuleLayout.ModuleName))
        {
            return std::unexpected(
                MakeError(EErrorCode::AlreadyExists,
                          "Plugin module '" + EditorModuleLayout.ModuleName + "' already exists"));
        }
        if (Result ExistingFileResult = Detail::EnsureModuleFilesDoNotExist(EditorModuleLayout); !ExistingFileResult)
        {
            return ExistingFileResult;
        }
        Descriptor.Modules.push_back(Detail::BuildModuleDescriptor(EditorModuleOptions, EditorModuleLayout));
        WriteStarterEditorModule = true;
    }

    const Detail::BuildIntegrationSettings BuildSettings = BuildPluginIntegrationSettings();
    const Detail::ProjectBuildIntegrationLayout PluginBuildLayout =
        Detail::BuildWorkspaceBuildIntegrationLayout(Descriptor.Paths, PluginRoot, BuildSettings);

    const std::filesystem::path PluginFilePath = PluginRoot / PluginFileNameResult.value();
    if (Result DirectoryResult = Detail::EnsureDirectory(PluginRoot); !DirectoryResult)
    {
        return DirectoryResult;
    }
    if (Result DirectoryResult =
            Detail::EnsureDirectory(PluginRoot / std::filesystem::path(Descriptor.Paths.AssetRoot));
        !DirectoryResult)
    {
        return DirectoryResult;
    }
    if (Result DirectoryResult =
            Detail::EnsureDirectory(PluginRoot / std::filesystem::path(Descriptor.Paths.CodeRoot));
        !DirectoryResult)
    {
        return DirectoryResult;
    }
    if (Result DirectoryResult =
            Detail::EnsureDirectory(PluginRoot / std::filesystem::path(Descriptor.Paths.ConfigRoot));
        !DirectoryResult)
    {
        return DirectoryResult;
    }
    if (Result DirectoryResult =
            Detail::EnsureDirectory(PluginRoot / std::filesystem::path(Descriptor.Paths.IntermediateRoot));
        !DirectoryResult)
    {
        return DirectoryResult;
    }
    if (Result DirectoryResult =
            Detail::EnsureDirectory(PluginRoot / std::filesystem::path(Descriptor.Paths.SavedRoot));
        !DirectoryResult)
    {
        return DirectoryResult;
    }

    std::vector<std::filesystem::path> GeneratedFiles{};
    if (WriteStarterRuntimeModule)
    {
        if (Result WriteModuleResult =
                Detail::WriteModuleFiles(Descriptor.Modules.front(), RuntimeModuleLayout, &GeneratedFiles);
            !WriteModuleResult)
        {
            return WriteModuleResult;
        }
    }
    if (WriteStarterEditorModule)
    {
        const ProjectModuleDescriptor& EditorModule = Descriptor.Modules.back();
        if (Result WriteEditorModuleResult = Detail::WriteModuleFiles(EditorModule, EditorModuleLayout, &GeneratedFiles);
            !WriteEditorModuleResult)
        {
            return WriteEditorModuleResult;
        }
    }
    if (Result WriteBuildFilesResult =
            Detail::WriteWorkspaceBuildIntegrationFiles(Descriptor.Modules, PluginBuildLayout, BuildSettings, &GeneratedFiles);
        !WriteBuildFilesResult)
    {
        return WriteBuildFilesResult;
    }
    if (Result SaveResult = PluginDescriptorService::Save(Descriptor, PluginFilePath.string()); !SaveResult)
    {
        return SaveResult;
    }

    if (OutResult != nullptr)
    {
        auto ResolvedPluginResult = PluginDescriptorService::LoadResolved(PluginFilePath.string());
        if (!ResolvedPluginResult)
        {
            return std::unexpected(ResolvedPluginResult.error());
        }

        OutResult->Plugin = std::move(*ResolvedPluginResult);
        OutResult->PluginCodeRootCMakePath = PluginBuildLayout.ProjectCodeRootCMakePath;
        OutResult->GeneratedPluginModulesCMakePath = PluginBuildLayout.GeneratedProjectModulesCMakePath;
        OutResult->RuntimeModuleDirectory =
            WriteStarterRuntimeModule ? RuntimeModuleLayout.ModuleRootDirectory : std::filesystem::path{};
        OutResult->RuntimeModuleRootCMakePath =
            WriteStarterRuntimeModule ? RuntimeModuleLayout.ModuleRootCMakePath : std::filesystem::path{};
        OutResult->RuntimeModuleCMakePath =
            WriteStarterRuntimeModule ? RuntimeModuleLayout.CMakeFragmentPath : std::filesystem::path{};
        OutResult->EditorModuleDirectory =
            WriteStarterEditorModule ? EditorModuleLayout.ModuleRootDirectory : std::filesystem::path{};
        OutResult->EditorModuleRootCMakePath =
            WriteStarterEditorModule ? EditorModuleLayout.ModuleRootCMakePath : std::filesystem::path{};
        OutResult->EditorModuleCMakePath =
            WriteStarterEditorModule ? EditorModuleLayout.CMakeFragmentPath : std::filesystem::path{};
        OutResult->GeneratedFiles = std::move(GeneratedFiles);
    }

    return Ok();
}

} // namespace SnAPI::GameFramework
