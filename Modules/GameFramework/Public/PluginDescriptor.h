#pragma once

#include "Expected.h"
#include "Export.h"
#include "ProjectDescriptor.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Human-facing plugin identity block stored in a plugin descriptor.
 *
 * Plugins intentionally reuse the same high-signal style as project descriptors:
 * stable identity, explicit authorship, and no machine-local state.
 */
struct PluginDescriptorPlugin
{
    std::string Name{}; /**< @brief Stable plugin name used for file generation and default module naming. */
    std::string DisplayName{}; /**< @brief Human-facing display name shown in tooling and reports. */
    std::string Company{}; /**< @brief Optional owning company or organization name. */
    std::string PluginId{}; /**< @brief Optional stable plugin UUID string. */
    std::string Version = "0.1.0"; /**< @brief Human-authored semantic or studio-local plugin version string. */
    std::string Description{}; /**< @brief Optional human-authored plugin summary. */
    bool CanContainAssets = true; /**< @brief `true` when the plugin owns authored assets under its asset root. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Canonical authored plugin descriptor model.
 *
 * Plugin descriptors intentionally reuse the existing path and module blocks so
 * project and plugin modules follow the same declaration shape, validation
 * expectations, and generated CMake orchestration.
 */
struct PluginDescriptor
{
    ProjectDescriptorFormat Format{}; /**< @brief Schema and compatibility block. */
    PluginDescriptorPlugin Plugin{}; /**< @brief Plugin identity block. */
    ProjectDescriptorPaths Paths{}; /**< @brief Authored path-settings block. */
    std::vector<ProjectModuleDescriptor> Modules{}; /**< @brief Declared plugin-owned modules. */
    nlohmann::ordered_json Platforms = nlohmann::ordered_json::object(); /**< @brief Raw platform-override block reserved for later typed resolution. */
    nlohmann::ordered_json Packaging = nlohmann::ordered_json::object(); /**< @brief Raw packaging-default block reserved for later typed resolution. */
    nlohmann::ordered_json AssetRules = nlohmann::ordered_json::object(); /**< @brief Raw asset-rule block reserved for later typed resolution. */
    nlohmann::ordered_json Templates = nlohmann::ordered_json::object(); /**< @brief Raw template provenance block reserved for later typed resolution. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Descriptor plus resolved filesystem view derived from a plugin descriptor.
 */
struct ResolvedPluginDescriptor
{
    PluginDescriptor Descriptor{}; /**< @brief Canonical authored plugin descriptor model. */
    std::filesystem::path PluginFilePath{}; /**< @brief Resolved plugin descriptor file path. */
    std::filesystem::path PluginRootDirectory{}; /**< @brief Resolved plugin root directory. */
    std::filesystem::path AssetRootDirectory{}; /**< @brief Resolved asset-root directory. */
    std::filesystem::path CodeRootDirectory{}; /**< @brief Resolved code-root directory. */
    std::filesystem::path ConfigRootDirectory{}; /**< @brief Resolved config-root directory. */
    std::filesystem::path IntermediateRootDirectory{}; /**< @brief Resolved intermediate-root directory. */
    std::filesystem::path SavedRootDirectory{}; /**< @brief Resolved saved-root directory. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Shared load/save/validate service for `.snplugin.json` files.
 *
 * The plugin service intentionally mirrors `ProjectDescriptorService` so plugin
 * creation, future plugin module management, and later build planning all go
 * through one canonical descriptor boundary.
 */
class SNAPI_GAMEFRAMEWORK_API PluginDescriptorService final
{
public:
    static constexpr std::uint32_t kCurrentSchemaVersion = 1u; /**< @brief Current structured plugin schema version understood by the service. */
    static constexpr std::string_view kDefaultPluginFileName = "plugin.snplugin.json"; /**< @brief Default plugin-descriptor file name used by plugin creation. */
    static constexpr std::string_view kDefaultAssetRoot = "Assets"; /**< @brief Default authored asset-root field. */
    static constexpr std::string_view kDefaultCodeRoot = "Modules"; /**< @brief Default authored code-root field. */
    static constexpr std::string_view kDefaultConfigRoot = "Config"; /**< @brief Default authored config-root field. */
    static constexpr std::string_view kDefaultIntermediateRoot = "Intermediate"; /**< @brief Default authored intermediate-root field. */
    static constexpr std::string_view kDefaultSavedRoot = "Saved"; /**< @brief Default authored saved-root field. */
    static constexpr std::string_view kDefaultMinimumToolVersion = "0.9.0"; /**< @brief Default minimum tool version written into new plugin descriptors. */
    static constexpr std::string_view kDefaultPluginVersion = "0.1.0"; /**< @brief Default plugin version written into new plugin descriptors. */

    /**
     * @brief Parse one plugin descriptor JSON document into the canonical model.
     * @param JsonText UTF-8 JSON document text.
     * @return Parsed plugin descriptor or a structured error.
     */
    [[nodiscard]] static TExpected<PluginDescriptor> Parse(std::string_view JsonText);

    /**
     * @brief Load and validate one plugin descriptor file from disk.
     * @param PluginFilePath Path naming the plugin descriptor file.
     * @return Canonical plugin descriptor model or a structured error.
     */
    [[nodiscard]] static TExpected<PluginDescriptor> Load(std::string_view PluginFilePath);

    /**
     * @brief Load, validate, and resolve one plugin descriptor into filesystem paths.
     * @param PluginFilePath Path naming the plugin descriptor file.
     * @return Resolved plugin descriptor view or a structured error.
     */
    [[nodiscard]] static TExpected<ResolvedPluginDescriptor> LoadResolved(std::string_view PluginFilePath);

    /**
     * @brief Serialize one canonical plugin descriptor model into structured JSON text.
     * @param Descriptor Plugin descriptor model to serialize.
     * @param Indent Pretty-print indentation width passed to `nlohmann::json::dump`.
     * @return Serialized JSON text or a structured validation/serialization error.
     */
    [[nodiscard]] static TExpected<std::string> Serialize(const PluginDescriptor& Descriptor, int Indent = 2);

    /**
     * @brief Validate and write one plugin descriptor file to disk.
     * @param Descriptor Plugin descriptor model to save.
     * @param PluginFilePath Output plugin descriptor file path.
     * @return Success or a structured file/validation error.
     */
    static Result Save(const PluginDescriptor& Descriptor, std::string_view PluginFilePath);

    /**
     * @brief Validate the authored plugin descriptor model without resolving filesystem paths.
     * @param Descriptor Plugin descriptor model to validate.
     * @return Flat list of validation issues.
     */
    [[nodiscard]] static std::vector<ProjectDescriptorValidationIssue> Validate(const PluginDescriptor& Descriptor);

    /**
     * @brief Validate the resolved plugin descriptor view including resolved paths.
     * @param Descriptor Resolved plugin descriptor model to validate.
     * @return Flat list of validation issues.
     */
    [[nodiscard]] static std::vector<ProjectDescriptorValidationIssue> Validate(const ResolvedPluginDescriptor& Descriptor);

    /**
     * @brief Convert an authored or resolved path value back into a plugin-relative field when possible.
     * @param RawValue Input path text to normalize.
     * @param BaseRoot Root directory used when attempting to express the path relatively.
     * @return Plugin-relative path field when possible, otherwise the normalized original path.
     */
    [[nodiscard]] static std::string ToPluginRelativePathField(std::string_view RawValue,
                                                               const std::filesystem::path& BaseRoot);
};

} // namespace SnAPI::GameFramework
