#pragma once

#include "Expected.h"
#include "ProjectDescriptor.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace SnAPI::GameFramework::Detail
{

    /**
     * @brief Naming and variable settings used when generating workspace-level build bridge files.
     *
     * Projects and plugins share the same generated-CMake structure but differ in
     * the environment variable name and generated module-fragment file name used by
     * the checked-in bridge wrapper.
     */
    struct BuildIntegrationSettings
    {
        std::string RootVariableName = "SNAPI_PROJECT_ROOT_DIR"; /**< @brief CMake variable storing the host workspace root. */
        std::string GeneratedModulesFileName = "ProjectModules.cmake"; /**< @brief Generated module-registration fragment file name. */
        std::string HostDisplayName = "project"; /**< @brief Human-readable host label used in generated comments. */
    };

    /**
     * @brief Resolved workspace-level generated build-file locations.
     *
     * These paths are derived from the host descriptor's authored `CodeRoot` and
     * `IntermediateRoot` fields and are reused whenever generated module
     * registration/linkage files need to be regenerated.
     */
    struct ProjectBuildIntegrationLayout
    {
        std::filesystem::path GeneratedBuildDirectory{}; /**< @brief Directory that owns generated build fragments under
                                                            `Intermediate/Build/Generated/`. */
        std::filesystem::path ProjectCodeRootCMakePath{}; /**< @brief Checked-in code-root `CMakeLists.txt` bridge file. */
        std::filesystem::path
            GeneratedProjectModulesCMakePath{}; /**< @brief Generated module-registration/linkage fragment. */
    };

    /**
     * @brief Trim leading and trailing ASCII whitespace from one string copy.
     * @param Text Source text.
     * @return Trimmed copy.
     */
    [[nodiscard]] std::string TrimCopy(std::string_view Text);

    /**
     * @brief Validate and normalize one generated identifier field.
     * @param RawValue Caller-supplied identifier text.
     * @param FieldName Human-readable field name used in diagnostics.
     * @return Trimmed identifier or an error.
     */
    [[nodiscard]] TExpected<std::string> NormalizeIdentifier(std::string_view RawValue, std::string_view FieldName);

    /**
     * @brief Build one PascalCase type stem from an identifier-like token.
     * @param Identifier Source identifier.
     * @return PascalCase stem used for generated type names.
     */
    [[nodiscard]] std::string BuildPascalCaseStem(std::string_view Identifier);

    /**
     * @brief Return one project-relative default module-root field.
     * @param CodeRoot Authored code-root field from the descriptor.
     * @param ModuleName Stable module name.
     * @return Relative module-root field stored in the descriptor.
     */
    [[nodiscard]] std::string BuildDefaultModuleRootField(std::string_view CodeRoot, std::string_view ModuleName);

    /**
     * @brief Find one declared module by exact name.
     * @param Descriptor Descriptor to inspect.
     * @param ModuleName Stable module name to find.
     * @return Mutable descriptor entry or `nullptr`.
     */
    [[nodiscard]] ProjectModuleDescriptor* FindModuleDescriptor(ProjectDescriptor& Descriptor,
                                                                std::string_view ModuleName);

    /**
     * @brief Find one declared module by exact name.
     * @param Descriptor Descriptor to inspect.
     * @param ModuleName Stable module name to find.
     * @return Const descriptor entry or `nullptr`.
     */
    [[nodiscard]] const ProjectModuleDescriptor* FindModuleDescriptor(const ProjectDescriptor& Descriptor,
                                                                      std::string_view ModuleName);

    /**
     * @brief Append one dependency name when it is not already present.
     * @param Dependencies Dependency list to update.
     * @param DependencyName Dependency to append.
     */
    void EnsureDependency(std::vector<std::string>& Dependencies, std::string_view DependencyName);

    /**
     * @brief Create one directory tree when it does not already exist.
     * @param Directory Directory path to create.
     * @return Success or a filesystem error.
     */
    [[nodiscard]] Result EnsureDirectory(const std::filesystem::path& Directory);

    /**
     * @brief Write one UTF-8 text file, creating parent directories as needed.
     * @param DestinationPath File path to write.
     * @param Text Complete file contents.
     * @return Success or a filesystem error.
     */
    [[nodiscard]] Result WriteTextFile(const std::filesystem::path& DestinationPath, const std::string& Text);

    /**
     * @brief Build the resolved project-level generated build-integration layout.
     * @param Descriptor Normalized descriptor seed.
     * @param ProjectRoot Resolved project root directory.
     * @return Build-integration layout.
     */
    [[nodiscard]] ProjectBuildIntegrationLayout
    BuildProjectBuildIntegrationLayout(const ProjectDescriptor& Descriptor, const std::filesystem::path& ProjectRoot);

    /**
     * @brief Build the resolved workspace-level generated build-integration layout.
     * @param Paths Authored path-settings block from the owning descriptor.
     * @param WorkspaceRoot Resolved workspace root directory.
     * @param Settings Naming settings used to choose the generated fragment file name.
     * @return Build-integration layout.
     */
    [[nodiscard]] ProjectBuildIntegrationLayout
    BuildWorkspaceBuildIntegrationLayout(const ProjectDescriptorPaths& Paths, const std::filesystem::path& WorkspaceRoot,
                                         const BuildIntegrationSettings& Settings);

    /**
     * @brief Build the project code-root `CMakeLists.txt` bridge file.
     * @return CMake wrapper text.
     */
    [[nodiscard]] std::string BuildProjectCodeRootCMake();

    /**
     * @brief Build one generic workspace code-root `CMakeLists.txt` bridge file.
     * @param Settings Naming settings that drive generated comments and CMake variable names.
     * @return CMake wrapper text.
     */
    [[nodiscard]] std::string BuildWorkspaceCodeRootCMake(const BuildIntegrationSettings& Settings);

    /**
     * @brief Build the generated project module-registration/linkage CMake file.
     * @param Descriptor Normalized descriptor whose module order drives emitted build wiring.
     * @return Generated CMake text.
     */
    [[nodiscard]] std::string BuildGeneratedProjectModulesCMake(const ProjectDescriptor& Descriptor);

    /**
     * @brief Build one generic generated module-registration/linkage CMake file.
     * @param Modules Normalized module declarations whose order drives emitted build wiring.
     * @param Settings Naming settings that drive generated comments and root-variable references.
     * @return Generated CMake text.
     */
    [[nodiscard]] std::string BuildGeneratedWorkspaceModulesCMake(const std::vector<ProjectModuleDescriptor>& Modules,
                                                                  const BuildIntegrationSettings& Settings);

    /**
     * @brief Write the project-level generated build integration files.
     * @param Descriptor Normalized descriptor used for module registration/linkage generation.
     * @param Layout Resolved project build-integration layout.
     * @param OutGeneratedFiles Optional flat list populated with written files.
     * @return Success or the first file-write error.
     */
    [[nodiscard]] Result
    WriteProjectBuildIntegrationFiles(const ProjectDescriptor& Descriptor, const ProjectBuildIntegrationLayout& Layout,
                                      std::vector<std::filesystem::path>* OutGeneratedFiles = nullptr);

    /**
     * @brief Write one generic set of workspace-level generated build integration files.
     * @param Modules Normalized module declarations used for module registration/linkage generation.
     * @param Layout Resolved workspace build-integration layout.
     * @param Settings Naming settings that drive generated comments and file contents.
     * @param OutGeneratedFiles Optional flat list populated with written files.
     * @return Success or the first file-write error.
     */
    [[nodiscard]] Result
    WriteWorkspaceBuildIntegrationFiles(const std::vector<ProjectModuleDescriptor>& Modules,
                                        const ProjectBuildIntegrationLayout& Layout,
                                        const BuildIntegrationSettings& Settings,
                                        std::vector<std::filesystem::path>* OutGeneratedFiles = nullptr);

} // namespace SnAPI::GameFramework::Detail
