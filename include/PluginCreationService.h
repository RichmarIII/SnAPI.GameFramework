#pragma once

#include "Expected.h"
#include "Export.h"
#include "PluginDescriptor.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Starter code-generation settings applied during plugin creation.
 *
 * Plugin creation emits library-style module scaffolds rather than gameplay
 * bootstrap classes, but it follows the same explicit module-declaration and
 * generated-CMake pattern as project creation.
 */
struct PluginCreationCodeOptions
{
    bool CreateStarterRuntimeModule = true; /**< @brief `true` to emit one starter runtime plugin module. */
    std::string RuntimeModuleName{}; /**< @brief Optional runtime-module target name. Empty defaults to `PluginName`. */
    std::string NamespaceRoot{}; /**< @brief Optional C++ namespace root for generated starter types. Empty defaults to `RuntimeModuleName`. */
    bool CreateStarterEditorModule = false; /**< @brief `true` to emit one companion editor-plugin module. */
    std::string EditorModuleName{}; /**< @brief Optional editor-module target name. Empty defaults to `<RuntimeModuleName>Editor`. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Concrete request used to materialize one new plugin workspace on disk.
 */
struct PluginCreationRequest
{
    std::string PluginName{}; /**< @brief Stable plugin name and default root-folder name. */
    std::filesystem::path ParentDirectory{}; /**< @brief Parent directory that will contain the new plugin root. */
    std::filesystem::path PluginFileName = std::string(PluginDescriptorService::kDefaultPluginFileName); /**< @brief Relative descriptor file path written under the new plugin root. */
    PluginDescriptor Descriptor{}; /**< @brief Seed descriptor authored into the new plugin file after normalization. */
    PluginCreationCodeOptions Code{}; /**< @brief Optional starter code generation settings. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Filesystem and descriptor snapshot produced by `PluginCreationService`.
 */
struct PluginCreationResult
{
    ResolvedPluginDescriptor Plugin{}; /**< @brief Resolved plugin descriptor snapshot after the new plugin file is written. */
    std::filesystem::path PluginCodeRootCMakePath{}; /**< @brief Resolved `Code/CMakeLists.txt` bridge that connects the plugin into generated module wiring. */
    std::filesystem::path GeneratedPluginModulesCMakePath{}; /**< @brief Resolved generated module-registration/linkage file under `Intermediate/Build/Generated/`. */
    std::filesystem::path RuntimeModuleDirectory{}; /**< @brief Resolved starter runtime-module directory when starter code generation is enabled. */
    std::filesystem::path RuntimeModuleRootCMakePath{}; /**< @brief Resolved starter runtime module-root `CMakeLists.txt` wrapper when generated. */
    std::filesystem::path RuntimeModuleCMakePath{}; /**< @brief Resolved starter runtime module CMake fragment path when generated. */
    std::filesystem::path EditorModuleDirectory{}; /**< @brief Resolved starter editor-module directory when editor-module generation is enabled. */
    std::filesystem::path EditorModuleRootCMakePath{}; /**< @brief Resolved starter editor module-root `CMakeLists.txt` wrapper when generated. */
    std::filesystem::path EditorModuleCMakePath{}; /**< @brief Resolved starter editor module CMake fragment path when generated. */
    std::vector<std::filesystem::path> GeneratedFiles{}; /**< @brief Flat list of generated starter files created by the service. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Shared workspace-creation service for new SnAPI plugins.
 *
 * The current milestone focuses on the descriptor, directory layout, starter
 * module scaffolds, and generated CMake bridge files required for plugin-owned
 * modules to build on the same path as project-owned modules.
 */
class SNAPI_GAMEFRAMEWORK_API PluginCreationService final
{
public:
    /**
     * @brief Build a normalized default plugin descriptor seed for one plugin name.
     * @param PluginName Stable plugin name.
     * @return Descriptor seed or an error when the name is invalid.
     */
    [[nodiscard]] static TExpected<PluginDescriptor> BuildDefaultDescriptor(std::string_view PluginName);

    /**
     * @brief Create one plugin workspace on disk.
     * @param Request Concrete plugin-creation request.
     * @param OutResult Optional result snapshot populated on success.
     * @return Success or a structured error.
     */
    [[nodiscard]] static Result CreatePlugin(const PluginCreationRequest& Request,
                                             PluginCreationResult* OutResult = nullptr);
};

} // namespace SnAPI::GameFramework
