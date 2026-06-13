#pragma once

#include "Expected.h"
#include "Export.h"
#include "PluginDescriptor.h"
#include "ProjectDescriptor.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SnAPI::GameFramework
{

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Concrete request used to add one new code module to an existing project workspace.
     *
     * The request updates both the project descriptor and the generated project-level
     * CMake wiring. Module roots are descriptor-relative by default and resolve under
     * the project's authored `CodeRoot` when `ModuleRoot` is empty.
     */
    struct ModuleCreationRequest
    {
        std::filesystem::path ProjectFilePath{}; /**< @brief Project descriptor file to load and update. */
        std::string ModuleName{}; /**< @brief Stable module name and default generated namespace/type stem. */
        EProjectModuleType ModuleType = EProjectModuleType::Runtime; /**< @brief Authored module role to create. */
        std::string
            ModuleRoot{}; /**< @brief Optional module-root field. Empty defaults to `<CodeRoot>/<ModuleName>`. */
        std::string NamespaceRoot{}; /**< @brief Optional C++ namespace root. Empty defaults to `ModuleName`. */
        std::vector<std::string> PublicDependencies{}; /**< @brief Additional public dependency module names. */
        std::vector<std::string> PrivateDependencies{}; /**< @brief Additional private dependency module names. */
        std::vector<std::string>
            Platforms{}; /**< @brief Optional platform allow/deny filter entries copied into the descriptor. */
        std::vector<std::string>
            PreprocessorDefinitions{}; /**< @brief Optional module-local preprocessor definitions. */
        bool UseReflectionGen = false; /**< @brief `true` when the module should opt into reflection generation. */
        bool UseSWIG = false; /**< @brief `true` when the module should opt into SWIG generation. */
        bool GenerateGameplayBootstrap = false; /**< @brief `true` to emit starter `IGame` and `IGameMode`
                                                   implementations for runtime-oriented modules. */
        std::optional<bool> LoadInEditor{}; /**< @brief Optional explicit editor-host linkage override. Empty uses
                                               module-type defaults. */
        std::optional<bool> LoadInRuntime{}; /**< @brief Optional explicit runtime-host linkage override. Empty uses
                                                module-type defaults. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Filesystem and descriptor snapshot produced by `ModuleCreationService`.
     */
    struct ModuleCreationResult
    {
        ResolvedProjectDescriptor
            Project{}; /**< @brief Resolved project descriptor after the new module is authored. */
        ProjectModuleDescriptor Module{}; /**< @brief Canonical descriptor entry created for the new module. */
        std::filesystem::path ModuleDirectory{}; /**< @brief Resolved module root directory. */
        std::filesystem::path ModuleRootCMakePath{}; /**< @brief Resolved module-root `CMakeLists.txt` wrapper path. */
        std::filesystem::path ModuleCMakePath{}; /**< @brief Resolved module CMake fragment path. */
        std::filesystem::path ModuleHeaderPath{}; /**< @brief Resolved primary generated module header path. */
        std::filesystem::path ModuleSourcePath{}; /**< @brief Resolved primary generated module source path. */
        std::filesystem::path GameHeaderPath{}; /**< @brief Resolved starter `IGame` header path when gameplay
                                                   bootstrap generation is enabled. */
        std::filesystem::path GameSourcePath{}; /**< @brief Resolved starter `IGame` source path when gameplay
                                                   bootstrap generation is enabled. */
        std::filesystem::path GameModeHeaderPath{}; /**< @brief Resolved starter `IGameMode` header path when gameplay
                                                       bootstrap generation is enabled. */
        std::filesystem::path GameModeSourcePath{}; /**< @brief Resolved starter `IGameMode` source path when gameplay
                                                       bootstrap generation is enabled. */
        std::filesystem::path
            ProjectCodeRootCMakePath{}; /**< @brief Resolved code-root `CMakeLists.txt` bridge file path. */
        std::filesystem::path GeneratedProjectModulesCMakePath{}; /**< @brief Resolved generated project
                                                                     module-registration/linkage file path. */
        std::vector<std::filesystem::path>
            GeneratedFiles{}; /**< @brief Flat list of generated files written by the service. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Concrete request used to add one new code module to an existing plugin workspace.
     *
     * The request updates both the plugin descriptor and the generated plugin-level
     * CMake wiring. Module roots are descriptor-relative by default and resolve under
     * the plugin's authored code root when `ModuleRoot` is empty.
     */
    struct PluginModuleCreationRequest
    {
        std::filesystem::path PluginFilePath{}; /**< @brief Plugin descriptor file to load and update. */
        std::string ModuleName{}; /**< @brief Stable module name and default generated namespace/type stem. */
        EProjectModuleType ModuleType = EProjectModuleType::Runtime; /**< @brief Authored module role to create. */
        std::string
            ModuleRoot{}; /**< @brief Optional module-root field. Empty defaults to `<CodeRoot>/<ModuleName>`. */
        std::string NamespaceRoot{}; /**< @brief Optional C++ namespace root. Empty defaults to `ModuleName`. */
        std::vector<std::string> PublicDependencies{}; /**< @brief Additional public dependency module names. */
        std::vector<std::string> PrivateDependencies{}; /**< @brief Additional private dependency module names. */
        std::vector<std::string>
            Platforms{}; /**< @brief Optional platform allow/deny filter entries copied into the descriptor. */
        std::vector<std::string>
            PreprocessorDefinitions{}; /**< @brief Optional module-local preprocessor definitions. */
        bool UseReflectionGen = false; /**< @brief `true` when the module should opt into reflection generation. */
        bool UseSWIG = false; /**< @brief `true` when the module should opt into SWIG generation. */
        bool GenerateGameplayBootstrap = false; /**< @brief `true` to emit starter `IGame` and `IGameMode`
                                                   implementations for runtime-oriented plugin modules. */
        std::optional<bool> LoadInEditor{}; /**< @brief Optional explicit editor-host linkage override. Empty uses
                                               module-type defaults. */
        std::optional<bool> LoadInRuntime{}; /**< @brief Optional explicit runtime-host linkage override. Empty uses
                                                module-type defaults. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Filesystem and descriptor snapshot produced by `CreatePluginModule`.
     */
    struct PluginModuleCreationResult
    {
        ResolvedPluginDescriptor Plugin{}; /**< @brief Resolved plugin descriptor after the new module is authored. */
        ProjectModuleDescriptor Module{}; /**< @brief Canonical descriptor entry created for the new module. */
        std::filesystem::path ModuleDirectory{}; /**< @brief Resolved module root directory. */
        std::filesystem::path ModuleRootCMakePath{}; /**< @brief Resolved module-root `CMakeLists.txt` wrapper path. */
        std::filesystem::path ModuleCMakePath{}; /**< @brief Resolved module CMake fragment path. */
        std::filesystem::path ModuleHeaderPath{}; /**< @brief Resolved primary generated module header path. */
        std::filesystem::path ModuleSourcePath{}; /**< @brief Resolved primary generated module source path. */
        std::filesystem::path GameHeaderPath{}; /**< @brief Resolved starter `IGame` header path when gameplay
                                                   bootstrap generation is enabled. */
        std::filesystem::path GameSourcePath{}; /**< @brief Resolved starter `IGame` source path when gameplay
                                                   bootstrap generation is enabled. */
        std::filesystem::path GameModeHeaderPath{}; /**< @brief Resolved starter `IGameMode` header path when gameplay
                                                       bootstrap generation is enabled. */
        std::filesystem::path GameModeSourcePath{}; /**< @brief Resolved starter `IGameMode` source path when gameplay
                                                       bootstrap generation is enabled. */
        std::filesystem::path
            PluginCodeRootCMakePath{}; /**< @brief Resolved code-root `CMakeLists.txt` bridge file for the plugin. */
        std::filesystem::path GeneratedPluginModulesCMakePath{}; /**< @brief Resolved generated plugin
                                                                    module-registration/linkage file path. */
        std::vector<std::filesystem::path>
            GeneratedFiles{}; /**< @brief Flat list of generated files written by the service. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shared service for adding new code modules to an existing SnAPI project.
     *
     * The service owns the descriptor mutation and filesystem scaffolding required to
     * add one new project module:
     * - load and validate the existing project descriptor
     * - append one new module declaration with type-appropriate defaults
     * - emit starter source/CMake files for the module
     * - regenerate project-level module-registration/linkage files
     * - save the updated descriptor
     *
     * The current milestone supports library-style module scaffolds for runtime,
     * editor, shared, developer, and test modules. Program-style targets will be
     * handled separately once standalone program packaging is implemented.
     */
    class SNAPI_GAMEFRAMEWORK_API ModuleCreationService final
    {
    public:
        /**
         * @brief Add one new module to an existing project workspace.
         * @param Request Concrete module-creation request.
         * @param OutResult Optional result snapshot populated on success.
         * @return Success or a structured error.
         */
        [[nodiscard]] static Result CreateModule(const ModuleCreationRequest& Request,
                                                 ModuleCreationResult* OutResult = nullptr);

        /**
         * @brief Add one new module to an existing plugin workspace.
         * @param Request Concrete plugin-module-creation request.
         * @param OutResult Optional result snapshot populated on success.
         * @return Success or a structured error.
         */
        [[nodiscard]] static Result CreatePluginModule(const PluginModuleCreationRequest& Request,
                                                       PluginModuleCreationResult* OutResult = nullptr);
    };

} // namespace SnAPI::GameFramework
