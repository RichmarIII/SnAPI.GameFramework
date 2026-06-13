#pragma once

#include "BuildProfile.h"
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
     * @brief One-shot request overrides applied after project-profile resolution.
     *
     * Override fields intentionally reuse the same patch model as authored build
     * profiles so CLI and editor actions can express append, replace, or explicit
     * clear semantics without inventing a second override language.
     */
    struct BuildRequestOverrides
    {
        BuildProfileValue<std::string> Platform{}; /**< @brief Optional target-platform override. */
        BuildProfileValue<std::string>
            ExecutionEnvironment{}; /**< @brief Optional host/container execution-environment override. */
        BuildProfileValue<EBuildConfiguration> Configuration{}; /**< @brief Optional build-configuration override. */
        BuildProfileStringList SelectedLevels{}; /**< @brief Optional selected-level override patch. */
        BuildProfileStringList ExplicitAssets{}; /**< @brief Optional explicit-asset override patch. */
        BuildProfileStringList IncludeFolders{}; /**< @brief Optional include-folder override patch. */
        BuildProfileStringList ExcludeFolders{}; /**< @brief Optional exclude-folder override patch. */
        BuildProfileStringList IncludeAssetLabels{}; /**< @brief Optional include-label override patch. */
        BuildProfileStringList ExcludeAssetLabels{}; /**< @brief Optional exclude-label override patch. */
        BuildProfileStringList IncludeAssetKinds{}; /**< @brief Optional include-kind override patch. */
        BuildProfileStringList ExcludeAssetKinds{}; /**< @brief Optional exclude-kind override patch. */
        BuildProfileValue<EAssetDependencyPolicy>
            DependencyPolicy{}; /**< @brief Optional dependency-policy override. */
        BuildProfileValue<EAssetChunkStrategy> ChunkStrategy{}; /**< @brief Optional chunk-strategy override. */
        BuildProfileValue<bool>
            AllowExplicitOverrideExcludes{}; /**< @brief Optional explicit-include precedence override. */
        BuildProfileArchiveSettings Archive{}; /**< @brief Optional archive/container override patch. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Concrete authored build invocation before descriptor/profile resolution.
     *
     * A `BuildRequest` is the user intent supplied by the editor or CLI. It selects
     * the owning project, optionally references one named profile, and then layers
     * one-shot overrides on top.
     */
    struct BuildRequest
    {
        std::filesystem::path ProjectFilePath{}; /**< @brief Project descriptor file to load and freeze. */
        std::string ProfileName{}; /**< @brief Optional named build profile to resolve before applying overrides. */
        BuildRequestOverrides Overrides{}; /**< @brief Optional one-shot overrides layered onto the resolved profile. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Fully frozen build request ready for planning, history, and hashing.
     *
     * `ResolvedBuildRequest` captures the canonical project descriptor snapshot, the
     * resolved build-profile settings, and a deterministic request hash derived from
     * the canonical frozen request JSON emitted by `BuildRequestService`.
     */
    struct ResolvedBuildRequest
    {
        ResolvedProjectDescriptor Project{}; /**< @brief Resolved owning project descriptor snapshot. */
        std::string ProfileName{}; /**< @brief Named profile used to seed resolution, or empty for ad hoc requests. */
        ResolvedBuildProfile Profile{}; /**< @brief Fully resolved build-profile settings after overrides. */
        std::string RequestHash{}; /**< @brief Deterministic lowercase hexadecimal request hash. */
        std::vector<BuildValidationIssue>
            ValidationIssues{}; /**< @brief Validation issues recorded for the frozen request. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shared service that freezes project descriptors, profiles, and one-shot overrides into build requests.
     *
     * The service is the bridge between authored project metadata and the later
     * build graph. It is responsible for:
     * - loading the project descriptor
     * - resolving an optional named profile
     * - applying one-shot request overrides
     * - validating the fully resolved request
     * - emitting deterministic frozen request JSON and request hashes
     */
    class SNAPI_GAMEFRAMEWORK_API BuildRequestService final
    {
    public:
        /**
         * @brief Resolve and freeze one build request.
         * @param Request Concrete build request to resolve.
         * @param MaxInheritanceDepth Maximum allowed build-profile inheritance depth.
         * @return Frozen build request or a structured error.
         */
        [[nodiscard]] static TExpected<ResolvedBuildRequest> Resolve(const BuildRequest& Request,
                                                                     std::size_t MaxInheritanceDepth = 4);

        /**
         * @brief Validate one frozen build request.
         * @param Request Frozen build request to validate.
         * @return Flat list of validation issues.
         */
        [[nodiscard]] static std::vector<BuildValidationIssue> Validate(const ResolvedBuildRequest& Request);

        /**
         * @brief Serialize one frozen build request into canonical JSON text.
         * @param Request Frozen build request to serialize.
         * @param Indent Pretty-print indentation width passed to `nlohmann::json::dump`.
         * @return Canonical JSON text or a structured serialization error.
         * @remarks
         * The emitted JSON is ordered deterministically and includes `RequestHash`
         * when the supplied request already carries one.
         */
        [[nodiscard]] static TExpected<std::string> SerializeResolved(const ResolvedBuildRequest& Request,
                                                                      int Indent = 2);

        /**
         * @brief Parse one frozen resolved build request from canonical JSON text.
         * @param Text Canonical JSON text previously emitted by `SerializeResolved`.
         * @return Parsed frozen request or a structured parse/validation error.
         */
        [[nodiscard]] static TExpected<ResolvedBuildRequest> DeserializeResolved(std::string_view Text);

        /**
         * @brief Load one frozen resolved build request from disk.
         * @param FilePath Path to `BuildRequest.json`.
         * @return Parsed frozen request or a structured filesystem/parse error.
         */
        [[nodiscard]] static TExpected<ResolvedBuildRequest> LoadResolved(const std::filesystem::path& FilePath);
    };

} // namespace SnAPI::GameFramework
