#pragma once

#include "BuildProfile.h"
#include "Expected.h"
#include "Export.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace SnAPI::GameFramework
{

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Schema-version and compatibility metadata for one project descriptor.
     *
     * `ProjectDescriptorFormat` captures the authored document version rather than
     * transient runtime state. `SchemaVersion` selects the structural schema used by
     * the descriptor file, while `MinimumToolVersion` communicates the minimum build
     * tooling version expected to understand the authored document.
     */
    struct ProjectDescriptorFormat
    {
        std::uint32_t SchemaVersion = 1u; /**< @brief Authored descriptor schema version. */
        std::string MinimumToolVersion =
            "0.9.0"; /**< @brief Minimum tool version expected to load and author this descriptor. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Human-facing project identity block stored in a descriptor.
     *
     * This block is the stable authored identity for the project. It is intended to
     * remain lightweight and diffable rather than accumulating machine-local state.
     */
    struct ProjectDescriptorProject
    {
        std::string Name{}; /**< @brief Stable project name used for file generation, target naming, and defaults. */
        std::string DisplayName{}; /**< @brief Human-facing display name shown in tooling and reports. */
        std::string Company{}; /**< @brief Optional owning company or organization name. */
        std::string ProjectId{}; /**< @brief Optional stable project UUID string. */
        std::string Description{}; /**< @brief Optional human-authored project summary. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Authored directory and root-path settings for a project.
     *
     * All non-URI relative paths are interpreted relative to the descriptor file's
     * parent directory when resolved through `ProjectDescriptorService`.
     */
    struct ProjectDescriptorPaths
    {
        std::string AssetRoot = "Assets"; /**< @brief Source asset root field as authored in the descriptor. */
        std::string CodeRoot = "Code"; /**< @brief Project code root field as authored in the descriptor. */
        std::string ConfigRoot = "Config"; /**< @brief Project config root field as authored in the descriptor. */
        std::string IntermediateRoot = "Intermediate"; /**< @brief Regenerable intermediate-output root field. */
        std::string SavedRoot = "Saved"; /**< @brief User-local or machine-local state root field. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Startup and runtime-bootstrap metadata stored in a descriptor.
     *
     * These values bridge authoring-time project configuration with runtime boot
     * paths such as `GameProjectRuntime`.
     */
    struct ProjectDescriptorStartup
    {
        std::string StartupLevelAsset = "Levels/StarterLevel.level"; /**< @brief Startup level asset field, typically
                                                                        logical and asset-root relative. */
        std::string DefaultRenderSettingsAssetId{}; /**< @brief Optional default render-settings asset id applied during
                                                       startup. */
        std::string DefaultGameClass{}; /**< @brief Optional default gameplay host class name. */
        std::string DefaultGameModeClass{}; /**< @brief Optional default game-mode class name. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Authored module role used by project and plugin module declarations.
     */
    enum class EProjectModuleType : std::uint8_t
    {
        Runtime = 0, /**< @brief Runtime module linked into packaged game builds and typically editor builds. */
        Editor, /**< @brief Editor-only module excluded from packaged runtime builds. */
        Shared, /**< @brief Utility module intended for both editor and runtime use. */
        Developer, /**< @brief Internal tooling or diagnostic module not required in packaged runtime builds. */
        Test, /**< @brief Test-focused module used for validation and automation. */
        Program, /**< @brief Standalone program-style module for tools or utilities. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One authored code-module declaration inside a project descriptor.
     *
     * The descriptor view exists for editor tooling, validation, planning, and
     * generation. Native toolchain integration is emitted separately from this model.
     */
    struct ProjectModuleDescriptor
    {
        std::string Name{}; /**< @brief Stable module name. */
        EProjectModuleType Type = EProjectModuleType::Runtime; /**< @brief Authored module role. */
        std::string Root{}; /**< @brief Module root path field, typically project-relative. */
        std::vector<std::string>
            PublicDependencies{}; /**< @brief Public dependency module names exposed through headers. */
        std::vector<std::string>
            PrivateDependencies{}; /**< @brief Private dependency module names used only internally. */
        std::vector<std::string> Platforms{}; /**< @brief Optional platform allow/deny filter entries. */
        std::vector<std::string> PreprocessorDefinitions{}; /**< @brief Module-local preprocessor definitions. */
        bool UseReflectionGen = false; /**< @brief `true` when the module participates in reflection generation. */
        bool UseSWIG = false; /**< @brief `true` when the module contributes SWIG bindings. */
        bool LoadInEditor = true; /**< @brief `true` when editor targets should include or load the module. */
        bool LoadInRuntime =
            true; /**< @brief `true` when packaged runtime targets should include or load the module. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Canonical authored project descriptor model.
     *
     * `ProjectDescriptor` stores the high-signal authored project configuration used
     * by runtime bootstrap, editor workflows, and future build-system planning. Some
     * later-stage sections remain modeled as raw ordered JSON objects for now so the
     * descriptor can already carry them without forcing premature schema lock-in for
     * every subsystem at once. Build profiles are now represented as typed authored
     * values so inheritance, validation, and resolution can operate on explicit
     * fields rather than unstructured JSON blobs.
     */
    struct ProjectDescriptor
    {
        ProjectDescriptorFormat Format{}; /**< @brief Schema and compatibility block. */
        ProjectDescriptorProject Project{}; /**< @brief Project identity block. */
        ProjectDescriptorPaths Paths{}; /**< @brief Authored path-settings block. */
        ProjectDescriptorStartup Startup{}; /**< @brief Startup and runtime-bootstrap block. */
        std::vector<ProjectModuleDescriptor> Modules{}; /**< @brief Declared project or plugin modules. */
        std::vector<BuildProfile>
            Profiles{}; /**< @brief Typed named build-profile declarations authored in the descriptor. */
        nlohmann::ordered_json Platforms = nlohmann::ordered_json::object(); /**< @brief Raw platform-override block
                                                                                reserved for later typed resolution. */
        nlohmann::ordered_json Packaging = nlohmann::ordered_json::object(); /**< @brief Raw packaging-default block
                                                                                reserved for later typed resolution. */
        nlohmann::ordered_json AssetRules =
            nlohmann::ordered_json::object(); /**< @brief Raw asset-rule block reserved for later typed resolution. */
        nlohmann::ordered_json Templates = nlohmann::ordered_json::object(); /**< @brief Raw template provenance block
                                                                                reserved for later typed resolution. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Severity level used by project-descriptor validation.
     */
    enum class EProjectDescriptorValidationSeverity : std::uint8_t
    {
        Info = 0, /**< @brief Non-blocking informational observation. */
        Warning, /**< @brief Potentially actionable issue that does not block use by default. */
        Error, /**< @brief Blocking validation issue. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One structured project-descriptor validation result.
     *
     * `RuleId` is intended to remain stable enough for future filtering, suppression,
     * and diagnostics tooling, while `Message` is human-readable diagnostic text.
     */
    struct ProjectDescriptorValidationIssue
    {
        EProjectDescriptorValidationSeverity Severity =
            EProjectDescriptorValidationSeverity::Error; /**< @brief Validation severity. */
        std::string RuleId{}; /**< @brief Stable rule identifier for diagnostics and future policy control. */
        std::string Message{}; /**< @brief Human-readable validation detail. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Descriptor plus resolved filesystem view derived from it.
     *
     * This structure is the bridge between authored descriptor fields and concrete
     * filesystem paths. It preserves the authored descriptor while also carrying the
     * normalized absolute or resolved directories needed by runtime and editor code.
     */
    struct ResolvedProjectDescriptor
    {
        ProjectDescriptor Descriptor{}; /**< @brief Canonical authored descriptor model. */
        std::filesystem::path ProjectFilePath{}; /**< @brief Resolved descriptor file path. */
        std::filesystem::path ProjectRootDirectory{}; /**< @brief Resolved project root directory. */
        std::filesystem::path AssetRootDirectory{}; /**< @brief Resolved asset-root directory. */
        std::filesystem::path CodeRootDirectory{}; /**< @brief Resolved code-root directory. */
        std::filesystem::path ConfigRootDirectory{}; /**< @brief Resolved config-root directory. */
        std::filesystem::path IntermediateRootDirectory{}; /**< @brief Resolved intermediate-root directory. */
        std::filesystem::path SavedRootDirectory{}; /**< @brief Resolved saved-root directory. */
        std::filesystem::path StartupLevelAssetPath{}; /**< @brief Resolved startup level source-asset path. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shared load/save/migrate/validate service for `.snproj.json` files.
     *
     * `ProjectDescriptorService` is the canonical project-file boundary for the
     * framework. It replaces ad hoc per-caller parsing with one centralized path
     * that handles legacy migration, structured JSON parsing via `nlohmann::json`,
     * canonical serialization, validation, and descriptor-relative path resolution.
     */
    class SNAPI_GAMEFRAMEWORK_API ProjectDescriptorService final
    {
    public:
        static constexpr std::uint32_t kCurrentSchemaVersion =
            1u; /**< @brief Current structured descriptor schema version understood by the service. */
        static constexpr std::string_view kDefaultProjectFileName =
            "project.snproj.json"; /**< @brief Default project-descriptor file name used by editor project creation. */
        static constexpr std::string_view kDefaultAssetRoot =
            "Assets"; /**< @brief Default authored asset-root field. */
        static constexpr std::string_view kDefaultCodeRoot = "Code"; /**< @brief Default authored code-root field. */
        static constexpr std::string_view kDefaultConfigRoot =
            "Config"; /**< @brief Default authored config-root field. */
        static constexpr std::string_view kDefaultIntermediateRoot =
            "Intermediate"; /**< @brief Default authored intermediate-root field. */
        static constexpr std::string_view kDefaultSavedRoot = "Saved"; /**< @brief Default authored saved-root field. */
        static constexpr std::string_view kDefaultStartupLevelAsset =
            "Levels/StarterLevel.level"; /**< @brief Default authored startup level asset field. */
        static constexpr std::string_view kDefaultMinimumToolVersion =
            "0.9.0"; /**< @brief Default minimum tool version written into new descriptors. */

        /**
         * @brief Parse one descriptor JSON document into the canonical model.
         * @param JsonText UTF-8 JSON document text.
         * @return Parsed descriptor or a structured error.
         * @remarks
         * Supports both the new structured schema and the legacy flat schema used by
         * earlier runtime/editor project loading code. Legacy documents are migrated
         * in memory to the canonical model returned here.
         */
        [[nodiscard]] static TExpected<ProjectDescriptor> Parse(std::string_view JsonText);
        /**
         * @brief Load and validate one descriptor file from disk.
         * @param ProjectFilePath Path or resolvable URI-like string naming the project file.
         * @return Canonical descriptor model or a structured error.
         * @remarks
         * This method performs file I/O, parsing, in-memory migration, canonicalization,
         * and blocking validation, but it does not resolve descriptor-relative filesystem
         * directories into `ResolvedProjectDescriptor`.
         */
        [[nodiscard]] static TExpected<ProjectDescriptor> Load(std::string_view ProjectFilePath);
        /**
         * @brief Load, validate, and resolve one descriptor file into filesystem paths.
         * @param ProjectFilePath Path or resolvable URI-like string naming the project file.
         * @return Resolved descriptor view or a structured error.
         * @remarks
         * Relative path fields are resolved against the descriptor directory, while the
         * startup level asset field is resolved against the resolved asset root unless it
         * is absolute or URI-based.
         */
        [[nodiscard]] static TExpected<ResolvedProjectDescriptor> LoadResolved(std::string_view ProjectFilePath);
        /**
         * @brief Serialize one canonical descriptor model into structured JSON text.
         * @param Descriptor Descriptor model to serialize.
         * @param Indent Pretty-print indentation width passed to `nlohmann::json::dump`.
         * @return Serialized JSON text or a structured validation/serialization error.
         * @remarks
         * Output is always written in the current structured schema, even when the input
         * descriptor originated from a legacy flat document.
         */
        [[nodiscard]] static TExpected<std::string> Serialize(const ProjectDescriptor& Descriptor, int Indent = 2);
        /**
         * @brief Validate and write one descriptor file to disk.
         * @param Descriptor Descriptor model to save.
         * @param ProjectFilePath Output project file path.
         * @return Success or a structured file/validation error.
         * @remarks
         * Parent directories are created when possible. The file is written using the
         * current structured schema.
         */
        static Result Save(const ProjectDescriptor& Descriptor, std::string_view ProjectFilePath);

        /**
         * @brief Validate the authored descriptor model without resolving filesystem paths.
         * @param Descriptor Descriptor model to validate.
         * @return Flat list of validation issues.
         */
        [[nodiscard]] static std::vector<ProjectDescriptorValidationIssue>
        Validate(const ProjectDescriptor& Descriptor);
        /**
         * @brief Validate the resolved descriptor view including resolved paths.
         * @param Descriptor Resolved descriptor model to validate.
         * @return Flat list of validation issues.
         */
        [[nodiscard]] static std::vector<ProjectDescriptorValidationIssue>
        Validate(const ResolvedProjectDescriptor& Descriptor);

        /**
         * @brief Convert an authored or resolved path value back into a project-relative field when possible.
         * @param RawValue Input path text to normalize.
         * @param BaseRoot Root directory used when attempting to express the path relatively.
         * @return Project-relative path field when possible, otherwise the normalized original path.
         * @remarks
         * URI-like strings are returned unchanged. Absolute filesystem paths are converted
         * to relative form only when they remain inside `BaseRoot`.
         */
        [[nodiscard]] static std::string ToProjectRelativePathField(std::string_view RawValue,
                                                                    const std::filesystem::path& BaseRoot);
    };

} // namespace SnAPI::GameFramework
