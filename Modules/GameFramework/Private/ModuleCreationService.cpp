#include "ModuleCreationService.h"

#include "ModuleScaffoldingShared.h"
#include "ProjectBuildGenerationShared.h"

#include <ranges>
#include <utility>

namespace SnAPI::GameFramework
{
    namespace
    {

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
         * @brief Translate one project-module request into the shared scaffolding settings.
         * @param Request Concrete project-module creation request.
         * @param Project Resolved owning project descriptor.
         * @return Shared scaffolding settings.
         */
        [[nodiscard]] Detail::ModuleScaffoldOptions BuildProjectModuleOptions(const ModuleCreationRequest& Request,
                                                                              const ResolvedProjectDescriptor& Project)
        {
            return Detail::ModuleScaffoldOptions{
                .ModuleName = Request.ModuleName,
                .ModuleType = Request.ModuleType,
                .CodeRootField = Project.Descriptor.Paths.CodeRoot,
                .WorkspaceRootDirectory = Project.ProjectRootDirectory,
                .ModuleRootField = Request.ModuleRoot,
                .NamespaceRoot = Request.NamespaceRoot,
                .PublicDependencies = Request.PublicDependencies,
                .PrivateDependencies = Request.PrivateDependencies,
                .Platforms = Request.Platforms,
                .PreprocessorDefinitions = Request.PreprocessorDefinitions,
                .UseReflectionGen = Request.UseReflectionGen,
                .UseSWIG = Request.UseSWIG,
                .GenerateGameplayBootstrap = Request.GenerateGameplayBootstrap,
                .LoadInEditor = Request.LoadInEditor,
                .LoadInRuntime = Request.LoadInRuntime,
            };
        }

        /**
         * @brief Translate one plugin-module request into the shared scaffolding settings.
         * @param Request Concrete plugin-module creation request.
         * @param Plugin Resolved owning plugin descriptor.
         * @return Shared scaffolding settings.
         */
        [[nodiscard]] Detail::ModuleScaffoldOptions BuildPluginModuleOptions(const PluginModuleCreationRequest& Request,
                                                                             const ResolvedPluginDescriptor& Plugin)
        {
            return Detail::ModuleScaffoldOptions{
                .ModuleName = Request.ModuleName,
                .ModuleType = Request.ModuleType,
                .CodeRootField = Plugin.Descriptor.Paths.CodeRoot,
                .WorkspaceRootDirectory = Plugin.PluginRootDirectory,
                .ModuleRootField = Request.ModuleRoot,
                .NamespaceRoot = Request.NamespaceRoot,
                .PublicDependencies = Request.PublicDependencies,
                .PrivateDependencies = Request.PrivateDependencies,
                .Platforms = Request.Platforms,
                .PreprocessorDefinitions = Request.PreprocessorDefinitions,
                .UseReflectionGen = Request.UseReflectionGen,
                .UseSWIG = Request.UseSWIG,
                .GenerateGameplayBootstrap = Request.GenerateGameplayBootstrap,
                .LoadInEditor = Request.LoadInEditor,
                .LoadInRuntime = Request.LoadInRuntime,
            };
        }

        /**
         * @brief Return `true` when the plugin descriptor already contains one module with the requested name.
         * @param Descriptor Plugin descriptor to inspect.
         * @param ModuleName Stable module name to find.
         * @return `true` when a matching module already exists.
         */
        [[nodiscard]] bool HasPluginModuleDescriptor(const PluginDescriptor& Descriptor,
                                                     const std::string_view ModuleName)
        {
            return std::ranges::any_of(Descriptor.Modules, [&](const ProjectModuleDescriptor& Module)
                                       { return Module.Name == ModuleName; });
        }

    } // namespace

    Result ModuleCreationService::CreateModule(const ModuleCreationRequest& Request, ModuleCreationResult* OutResult)
    {
        if (Request.ProjectFilePath.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project descriptor path cannot be empty"));
        }
        if (!Detail::IsSupportedScaffoldModuleType(Request.ModuleType))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Program modules are not yet supported by ModuleCreationService"));
        }

        auto ProjectResult = ProjectDescriptorService::LoadResolved(Request.ProjectFilePath.string());
        if (!ProjectResult)
        {
            return std::unexpected(ProjectResult.error());
        }

        ResolvedProjectDescriptor Project = std::move(*ProjectResult);
        const Detail::ModuleScaffoldOptions Options = BuildProjectModuleOptions(Request, Project);
        auto LayoutResult = Detail::BuildModuleLayout(Options);
        if (!LayoutResult)
        {
            return std::unexpected(LayoutResult.error());
        }

        Detail::ModuleLayout Layout = std::move(*LayoutResult);
        if (Detail::FindModuleDescriptor(Project.Descriptor, Layout.ModuleName) != nullptr)
        {
            return std::unexpected(
                MakeError(EErrorCode::AlreadyExists, "Project module '" + Layout.ModuleName + "' already exists"));
        }
        if (Result ExistingFileResult = Detail::EnsureModuleFilesDoNotExist(Layout); !ExistingFileResult)
        {
            return ExistingFileResult;
        }

        const ProjectModuleDescriptor Module = Detail::BuildModuleDescriptor(Options, Layout);
        Project.Descriptor.Modules.push_back(Module);

        const Detail::ProjectBuildIntegrationLayout ProjectBuildLayout =
            Detail::BuildProjectBuildIntegrationLayout(Project.Descriptor, Project.ProjectRootDirectory);

        std::vector<std::filesystem::path> GeneratedFiles{};
        if (Result WriteModuleResult = Detail::WriteModuleFiles(Module, Layout, &GeneratedFiles); !WriteModuleResult)
        {
            return WriteModuleResult;
        }
        if (Result WriteBuildFilesResult =
                Detail::WriteProjectBuildIntegrationFiles(Project.Descriptor, ProjectBuildLayout, &GeneratedFiles);
            !WriteBuildFilesResult)
        {
            return WriteBuildFilesResult;
        }
        if (Result SaveResult = ProjectDescriptorService::Save(Project.Descriptor, Project.ProjectFilePath.string());
            !SaveResult)
        {
            return SaveResult;
        }

        if (OutResult != nullptr)
        {
            auto ResolvedProjectResult = ProjectDescriptorService::LoadResolved(Project.ProjectFilePath.string());
            if (!ResolvedProjectResult)
            {
                return std::unexpected(ResolvedProjectResult.error());
            }

            OutResult->Project = std::move(*ResolvedProjectResult);
            OutResult->Module = Module;
            OutResult->ModuleDirectory = Layout.ModuleRootDirectory;
            OutResult->ModuleRootCMakePath = Layout.ModuleRootCMakePath;
            OutResult->ModuleCMakePath = Layout.CMakeFragmentPath;
            OutResult->ModuleHeaderPath = Layout.ModuleHeaderPath;
            OutResult->ModuleSourcePath = Layout.ModuleSourcePath;
            OutResult->GameHeaderPath = Layout.GameHeaderPath;
            OutResult->GameSourcePath = Layout.GameSourcePath;
            OutResult->GameModeHeaderPath = Layout.GameModeHeaderPath;
            OutResult->GameModeSourcePath = Layout.GameModeSourcePath;
            OutResult->ProjectCodeRootCMakePath = ProjectBuildLayout.ProjectCodeRootCMakePath;
            OutResult->GeneratedProjectModulesCMakePath = ProjectBuildLayout.GeneratedProjectModulesCMakePath;
            OutResult->GeneratedFiles = std::move(GeneratedFiles);
        }

        return Ok();
    }

    Result ModuleCreationService::CreatePluginModule(const PluginModuleCreationRequest& Request,
                                                     PluginModuleCreationResult* OutResult)
    {
        if (Request.PluginFilePath.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Plugin descriptor path cannot be empty"));
        }
        if (!Detail::IsSupportedScaffoldModuleType(Request.ModuleType))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Program modules are not yet supported by ModuleCreationService"));
        }

        auto PluginResult = PluginDescriptorService::LoadResolved(Request.PluginFilePath.string());
        if (!PluginResult)
        {
            return std::unexpected(PluginResult.error());
        }

        ResolvedPluginDescriptor Plugin = std::move(*PluginResult);
        const Detail::ModuleScaffoldOptions Options = BuildPluginModuleOptions(Request, Plugin);
        auto LayoutResult = Detail::BuildModuleLayout(Options);
        if (!LayoutResult)
        {
            return std::unexpected(LayoutResult.error());
        }

        Detail::ModuleLayout Layout = std::move(*LayoutResult);
        if (HasPluginModuleDescriptor(Plugin.Descriptor, Layout.ModuleName))
        {
            return std::unexpected(
                MakeError(EErrorCode::AlreadyExists, "Plugin module '" + Layout.ModuleName + "' already exists"));
        }
        if (Result ExistingFileResult = Detail::EnsureModuleFilesDoNotExist(Layout); !ExistingFileResult)
        {
            return ExistingFileResult;
        }

        const ProjectModuleDescriptor Module = Detail::BuildModuleDescriptor(Options, Layout);
        Plugin.Descriptor.Modules.push_back(Module);

        const Detail::BuildIntegrationSettings BuildSettings = BuildPluginIntegrationSettings();
        const Detail::ProjectBuildIntegrationLayout PluginBuildLayout = Detail::BuildWorkspaceBuildIntegrationLayout(
            Plugin.Descriptor.Paths, Plugin.PluginRootDirectory, BuildSettings);

        std::vector<std::filesystem::path> GeneratedFiles{};
        if (Result WriteModuleResult = Detail::WriteModuleFiles(Module, Layout, &GeneratedFiles); !WriteModuleResult)
        {
            return WriteModuleResult;
        }
        if (Result WriteBuildFilesResult = Detail::WriteWorkspaceBuildIntegrationFiles(
                Plugin.Descriptor.Modules, PluginBuildLayout, BuildSettings, &GeneratedFiles);
            !WriteBuildFilesResult)
        {
            return WriteBuildFilesResult;
        }
        if (Result SaveResult = PluginDescriptorService::Save(Plugin.Descriptor, Plugin.PluginFilePath.string());
            !SaveResult)
        {
            return SaveResult;
        }

        if (OutResult != nullptr)
        {
            auto ResolvedPluginResult = PluginDescriptorService::LoadResolved(Plugin.PluginFilePath.string());
            if (!ResolvedPluginResult)
            {
                return std::unexpected(ResolvedPluginResult.error());
            }

            OutResult->Plugin = std::move(*ResolvedPluginResult);
            OutResult->Module = Module;
            OutResult->ModuleDirectory = Layout.ModuleRootDirectory;
            OutResult->ModuleRootCMakePath = Layout.ModuleRootCMakePath;
            OutResult->ModuleCMakePath = Layout.CMakeFragmentPath;
            OutResult->ModuleHeaderPath = Layout.ModuleHeaderPath;
            OutResult->ModuleSourcePath = Layout.ModuleSourcePath;
            OutResult->GameHeaderPath = Layout.GameHeaderPath;
            OutResult->GameSourcePath = Layout.GameSourcePath;
            OutResult->GameModeHeaderPath = Layout.GameModeHeaderPath;
            OutResult->GameModeSourcePath = Layout.GameModeSourcePath;
            OutResult->PluginCodeRootCMakePath = PluginBuildLayout.ProjectCodeRootCMakePath;
            OutResult->GeneratedPluginModulesCMakePath = PluginBuildLayout.GeneratedProjectModulesCMakePath;
            OutResult->GeneratedFiles = std::move(GeneratedFiles);
        }

        return Ok();
    }

} // namespace SnAPI::GameFramework
