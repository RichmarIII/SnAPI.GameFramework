#pragma once

#include "Expected.h"
#include "Export.h"
#include "ProjectDescriptor.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Optional starter-template resources copied into a newly created project workspace.
 *
 * These paths are caller-supplied and intentionally decoupled from editor runtime state so
 * the same service can be reused by editor flows, CLI tools, and future plugin/module creation
 * commands. Empty paths simply disable the corresponding copy step.
 */
struct ProjectCreationTemplateResources
{
    std::filesystem::path StarterLevelTemplateSourcePath{}; /**< @brief Optional authored level source asset copied to the new startup-level path. */
    std::filesystem::path StarterScriptTemplateSourcePath{}; /**< @brief Optional script file copied into the new asset root. */
    std::filesystem::path ShaderTemplateDirectory{}; /**< @brief Optional directory whose contents are copied into `<AssetRoot>/Shaders`. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Starter code-generation settings applied during project creation.
 *
 * The current project-creation milestone emits one starter runtime module by default so a
 * new workspace already has a descriptor module entry, basic gameplay classes, a module
 * CMake fragment, and a placeholder runtime config file. Callers can disable this when
 * creating content-only projects or future plugin/module flows that want more control.
 */
struct ProjectCreationCodeOptions
{
    bool CreateStarterRuntimeModule = true; /**< @brief `true` to emit starter runtime-module source/config files. */
    std::string RuntimeModuleName{}; /**< @brief Optional runtime-module target name. Empty defaults to `ProjectName`. */
    std::string NamespaceRoot{}; /**< @brief Optional C++ namespace root for generated starter types. Empty defaults to `RuntimeModuleName`. */
    bool CreateStarterEditorModule = false; /**< @brief `true` to emit a companion editor-module scaffold and descriptor entry. */
    std::string EditorModuleName{}; /**< @brief Optional editor-module target name. Empty defaults to `<RuntimeModuleName>Editor`. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Concrete request used to materialize one new project workspace on disk.
 *
 * `ProjectName` controls both the created root directory name and the default authored project
 * identity when the supplied descriptor leaves those fields empty. `Descriptor` acts as the
 * authored seed and is normalized to the current schema defaults before any files are written.
 */
struct ProjectCreationRequest
{
    std::string ProjectName{}; /**< @brief Stable project name and default root-folder name. */
    std::filesystem::path ParentDirectory{}; /**< @brief Parent directory that will contain the new project root. */
    std::filesystem::path ProjectFileName = std::string(ProjectDescriptorService::kDefaultProjectFileName); /**< @brief Relative descriptor file path written under the new project root. */
    ProjectDescriptor Descriptor{}; /**< @brief Seed descriptor authored into the new project file after normalization. */
    ProjectCreationTemplateResources Templates{}; /**< @brief Optional starter resources copied into the workspace. */
    ProjectCreationCodeOptions Code{}; /**< @brief Optional starter code/config generation settings. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Filesystem and descriptor snapshot produced by `ProjectCreationService`.
 */
struct ProjectCreationResult
{
    ResolvedProjectDescriptor Project{}; /**< @brief Resolved descriptor snapshot after the new project file is written. */
    std::filesystem::path StartupLevelAssetPath{}; /**< @brief Resolved startup level source-asset path created for the project. */
    std::filesystem::path StarterScriptPath{}; /**< @brief Resolved starter-script path when a script template was copied. */
    std::filesystem::path ShaderDirectory{}; /**< @brief Resolved project shader directory when shader templates were copied. */
    std::filesystem::path ProjectCodeRootCMakePath{}; /**< @brief Resolved `Code/CMakeLists.txt` wrapper that bridges the project into generated module wiring. */
    std::filesystem::path GeneratedProjectModulesCMakePath{}; /**< @brief Resolved generated module-registration/linkage file under `Intermediate/Build/Generated/`. */
    std::filesystem::path RuntimeModuleDirectory{}; /**< @brief Resolved starter runtime-module directory when starter code generation is enabled. */
    std::filesystem::path RuntimeModuleRootCMakePath{}; /**< @brief Resolved starter module-root `CMakeLists.txt` wrapper when generated. */
    std::filesystem::path RuntimeModuleCMakePath{}; /**< @brief Resolved starter module CMake fragment path when generated. */
    std::filesystem::path EditorModuleDirectory{}; /**< @brief Resolved starter editor-module directory when editor-module generation is enabled. */
    std::filesystem::path EditorModuleRootCMakePath{}; /**< @brief Resolved starter editor module-root `CMakeLists.txt` wrapper when generated. */
    std::filesystem::path EditorModuleCMakePath{}; /**< @brief Resolved starter editor module CMake fragment path when generated. */
    std::filesystem::path DefaultGameConfigPath{}; /**< @brief Resolved default runtime config path when generated. */
    std::vector<std::filesystem::path> GeneratedFiles{}; /**< @brief Flat list of generated starter files created by the service. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Shared workspace-creation service for new SnAPI projects.
 *
 * The service owns the filesystem/materialization portion of project creation:
 * - normalize the authored descriptor seed
 * - create the standard workspace directories
 * - emit the descriptor file through `ProjectDescriptorService`
 * - materialize starter content such as the startup level, optional scripts, and optional shaders
 *
 * It intentionally does not own editor-only follow-up such as rebuilding the active asset manager
 * or loading the startup level into a live runtime session. Those steps remain the caller's job.
 */
class SNAPI_GAMEFRAMEWORK_API ProjectCreationService final
{
public:
    /**
     * @brief Build a normalized default descriptor seed for one project name.
     * @param ProjectName Stable project name.
     * @return Descriptor seed or an error when the name is invalid.
     */
    [[nodiscard]] static TExpected<ProjectDescriptor> BuildDefaultDescriptor(std::string_view ProjectName);

    /**
     * @brief Create one project workspace on disk.
     * @param Request Concrete project-creation request.
     * @param OutResult Optional result snapshot populated on success.
     * @return Success or a structured error.
     */
    [[nodiscard]] static Result CreateProject(const ProjectCreationRequest& Request,
                                              ProjectCreationResult* OutResult = nullptr);
};

} // namespace SnAPI::GameFramework
