#pragma once

#include "Expected.h"
#include "ProjectDescriptor.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SnAPI::GameFramework::Detail
{

/**
 * @brief Request-neutral settings used to generate one library-style module scaffold.
 */
struct ModuleScaffoldOptions
{
    std::string ModuleName{}; /**< @brief Stable module name. */
    EProjectModuleType ModuleType = EProjectModuleType::Runtime; /**< @brief Authored module role to scaffold. */
    std::string CodeRootField{}; /**< @brief Host descriptor's authored `CodeRoot` field. */
    std::filesystem::path WorkspaceRootDirectory{}; /**< @brief Resolved host workspace root directory. */
    std::string ModuleRootField{}; /**< @brief Optional explicit module-root field override. */
    std::string NamespaceRoot{}; /**< @brief Optional generated C++ namespace root. */
    std::vector<std::string> PublicDependencies{}; /**< @brief Additional public dependency module names. */
    std::vector<std::string> PrivateDependencies{}; /**< @brief Additional private dependency module names. */
    std::vector<std::string> Platforms{}; /**< @brief Optional platform allow/deny filter entries copied into the descriptor. */
    std::vector<std::string> PreprocessorDefinitions{}; /**< @brief Optional module-local preprocessor definitions. */
    bool UseReflectionGen = false; /**< @brief `true` when the module should opt into reflection generation. */
    bool UseSWIG = false; /**< @brief `true` when the module should opt into SWIG generation. */
    bool GenerateGameplayBootstrap = false; /**< @brief `true` to emit starter `IGame` and `IGameMode`
                                               implementations for runtime modules. */
    std::optional<bool> LoadInEditor{}; /**< @brief Optional explicit editor-host linkage override. */
    std::optional<bool> LoadInRuntime{}; /**< @brief Optional explicit runtime-host linkage override. */
};

/**
 * @brief Filesystem layout for one generated library-style project or plugin module.
 */
struct ModuleLayout
{
    std::string ModuleName{};
    std::string NamespaceRoot{};
    std::string ModuleClassName{};
    std::string GameClassName{};
    std::string GameModeClassName{};
    EProjectModuleType ModuleType = EProjectModuleType::Runtime;
    bool GenerateGameplayBootstrap = false;
    std::string ModuleRootField{};
    std::filesystem::path ModuleRootDirectory{};
    std::filesystem::path IncludeDirectory{};
    std::filesystem::path PublicHeaderDirectory{};
    std::filesystem::path SourceDirectory{};
    std::filesystem::path ModuleRootCMakePath{};
    std::filesystem::path CMakeFragmentPath{};
    std::filesystem::path ModuleHeaderPath{};
    std::filesystem::path ModuleSourcePath{};
    std::filesystem::path GameHeaderPath{};
    std::filesystem::path GameSourcePath{};
    std::filesystem::path GameModeHeaderPath{};
    std::filesystem::path GameModeSourcePath{};
};

/**
 * @brief Return `true` when the module type uses the editor-style starter scaffold.
 * @param ModuleType Authored module type.
 * @return `true` for editor modules.
 */
[[nodiscard]] bool UsesEditorModuleTemplate(EProjectModuleType ModuleType);

/**
 * @brief Return `true` when the module type is currently supported by the starter generator.
 * @param ModuleType Authored module type.
 * @return `true` when the shared scaffolding layer can emit starter files for the type.
 */
[[nodiscard]] bool IsSupportedScaffoldModuleType(EProjectModuleType ModuleType);

/**
 * @brief Compute the default editor-host linkage policy for one module type.
 * @param ModuleType Authored module type.
 * @return Default `LoadInEditor` value.
 */
[[nodiscard]] bool DefaultLoadInEditor(EProjectModuleType ModuleType);

/**
 * @brief Compute the default runtime-host linkage policy for one module type.
 * @param ModuleType Authored module type.
 * @return Default `LoadInRuntime` value.
 */
[[nodiscard]] bool DefaultLoadInRuntime(EProjectModuleType ModuleType);

/**
 * @brief Build the resolved module layout for one scaffold request.
 * @param Options Module scaffolding settings.
 * @return Layout or an identifier/path error.
 */
[[nodiscard]] TExpected<ModuleLayout> BuildModuleLayout(const ModuleScaffoldOptions& Options);

/**
 * @brief Build one default descriptor entry for the requested scaffolded module.
 * @param Options Module scaffolding settings.
 * @param Layout Resolved generated module layout.
 * @return Descriptor entry with type-appropriate defaults applied.
 */
[[nodiscard]] ProjectModuleDescriptor BuildModuleDescriptor(const ModuleScaffoldOptions& Options,
                                                            const ModuleLayout& Layout);

/**
 * @brief Fail when any generated module file already exists on disk.
 * @param Layout Generated module layout to validate.
 * @return Success when all generated files are absent, otherwise `AlreadyExists`.
 */
[[nodiscard]] Result EnsureModuleFilesDoNotExist(const ModuleLayout& Layout);

/**
 * @brief Materialize all generated module files.
 * @param Module Descriptor entry that drives dependency/flag emission.
 * @param Layout Generated module layout.
 * @param OutGeneratedFiles Optional flat list populated with written files.
 * @return Success or the first file-write error.
 */
[[nodiscard]] Result WriteModuleFiles(const ProjectModuleDescriptor& Module, const ModuleLayout& Layout,
                                      std::vector<std::filesystem::path>* OutGeneratedFiles = nullptr);

} // namespace SnAPI::GameFramework::Detail
