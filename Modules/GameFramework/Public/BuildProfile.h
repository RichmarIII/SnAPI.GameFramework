#pragma once

#include "Expected.h"
#include "Export.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SnAPI::GameFramework
{

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Canonical build configuration used by packaging and code-build flows.
     */
    enum class EBuildConfiguration : std::uint8_t
    {
        Debug = 0, /**< @brief Deep local debugging configuration. */
        Development, /**< @brief Regular iteration configuration. */
        Test, /**< @brief QA and automation configuration. */
        Shipping, /**< @brief Final optimized release configuration. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Asset dependency expansion policy for one build profile.
     */
    enum class EAssetDependencyPolicy : std::uint8_t
    {
        HardOnly = 0, /**< @brief Include only hard dependencies of selected content. */
        HardAndSoft, /**< @brief Include hard and soft/reference-based dependencies. */
        HardSoftAndEditorPreview, /**< @brief Include hard, soft, and editor-preview dependencies. */
        CustomResolver, /**< @brief Delegate dependency expansion to a later custom resolver. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Asset chunking strategy for one build profile.
     */
    enum class EAssetChunkStrategy : std::uint8_t
    {
        Monolithic = 0, /**< @brief Place all cooked content into one primary bundle. */
        SharedPlusPerLevel, /**< @brief Split shared content from per-level bundles. */
        PerLabel, /**< @brief Build bundles around authored labels or groups. */
        CustomGraph, /**< @brief Use a later custom chunk graph. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Severity level used by build-profile validation and resolution diagnostics.
     */
    enum class EBuildValidationSeverity : std::uint8_t
    {
        Info = 0, /**< @brief Non-blocking informational observation. */
        Warning, /**< @brief Actionable issue that does not block resolution by default. */
        Error, /**< @brief Blocking issue that prevents resolution or execution. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One structured build-profile validation or resolution issue.
     */
    struct BuildValidationIssue
    {
        EBuildValidationSeverity Severity = EBuildValidationSeverity::Error; /**< @brief Validation severity. */
        std::string RuleId{}; /**< @brief Stable rule identifier for diagnostics and future policy control. */
        std::string Message{}; /**< @brief Human-readable validation detail. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Tri-state authored value used by build-profile inheritance resolution.
     *
     * `IsSet=false` means the profile does not author the field and therefore inherits
     * the parent value unchanged. `IsSet=true` with `Value=nullopt` is an explicit
     * authored clear. `IsSet=true` with a concrete value overrides the parent.
     */
    template <typename TValue>
    struct BuildProfileValue
    {
        bool IsSet = false;
        std::optional<TValue> Value{};
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Authored string-list patch used by build-profile inheritance resolution.
     *
     * When `IsSet=false`, the list inherits unchanged. When `Replace=true`, the child
     * replaces the inherited list with `Values`. Otherwise, `Values` are appended and
     * deduplicated in authored order.
     */
    struct BuildProfileStringList
    {
        bool IsSet = false; /**< @brief `true` when the profile authors this list. */
        bool Replace = false; /**< @brief `true` when the child replaces rather than appends. */
        std::vector<std::string> Values{}; /**< @brief Authored list entries. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Authored archive settings patch used by build-profile inheritance resolution.
     */
    struct BuildProfileArchiveSettings
    {
        bool IsSet = false; /**< @brief `true` when the profile authors any archive override. */
        bool ReplaceEntireObject = false; /**< @brief `true` when inherited archive settings should be cleared first. */
        BuildProfileValue<bool> Enabled{}; /**< @brief Optional archive enablement override. */
        BuildProfileValue<std::string> Format{}; /**< @brief Optional archive/container format override. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Canonical authored build-profile model parsed from a descriptor `Profiles` block.
     */
    struct BuildProfile
    {
        std::string Name{}; /**< @brief Stable profile name keyed by the descriptor `Profiles` object. */
        std::string Inherits{}; /**< @brief Optional parent profile name. */
        BuildProfileValue<std::string> Platform{}; /**< @brief Optional target platform override. */
        BuildProfileValue<std::string>
            ExecutionEnvironment{}; /**< @brief Optional host/container execution-environment override. */
        BuildProfileValue<EBuildConfiguration> Configuration{}; /**< @brief Optional build configuration override. */
        BuildProfileStringList SelectedLevels{}; /**< @brief Selected level asset ids. */
        BuildProfileStringList ExplicitAssets{}; /**< @brief Explicit non-level asset ids to include. */
        BuildProfileStringList IncludeFolders{}; /**< @brief Included folder rules. */
        BuildProfileStringList ExcludeFolders{}; /**< @brief Excluded folder rules. */
        BuildProfileStringList IncludeAssetLabels{}; /**< @brief Included asset-label rules. */
        BuildProfileStringList ExcludeAssetLabels{}; /**< @brief Excluded asset-label rules. */
        BuildProfileStringList IncludeAssetKinds{}; /**< @brief Included asset-kind rules. */
        BuildProfileStringList ExcludeAssetKinds{}; /**< @brief Excluded asset-kind rules. */
        BuildProfileValue<EAssetDependencyPolicy>
            DependencyPolicy{}; /**< @brief Optional dependency-expansion override. */
        BuildProfileValue<EAssetChunkStrategy> ChunkStrategy{}; /**< @brief Optional chunk-strategy override. */
        BuildProfileValue<bool>
            AllowExplicitOverrideExcludes{}; /**< @brief Optional explicit-include precedence override. */
        BuildProfileArchiveSettings Archive{}; /**< @brief Optional archive/container settings. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Fully resolved build-profile result after inheritance and defaults are applied.
     */
    struct ResolvedBuildProfile
    {
        std::string Name{}; /**< @brief Stable resolved profile name. */
        std::string Inherits{}; /**< @brief Immediate parent profile name when one was resolved. */
        std::string Platform{}; /**< @brief Resolved target platform identifier. */
        std::string ExecutionEnvironment{}; /**< @brief Resolved host/container execution-environment identifier. */
        EBuildConfiguration Configuration =
            EBuildConfiguration::Development; /**< @brief Resolved build configuration. */
        std::vector<std::string> SelectedLevels{}; /**< @brief Resolved selected levels. */
        std::vector<std::string> ExplicitAssets{}; /**< @brief Resolved explicit asset ids. */
        std::vector<std::string> IncludeFolders{}; /**< @brief Resolved include-folder rules. */
        std::vector<std::string> ExcludeFolders{}; /**< @brief Resolved exclude-folder rules. */
        std::vector<std::string> IncludeAssetLabels{}; /**< @brief Resolved include-label rules. */
        std::vector<std::string> ExcludeAssetLabels{}; /**< @brief Resolved exclude-label rules. */
        std::vector<std::string> IncludeAssetKinds{}; /**< @brief Resolved include-kind rules. */
        std::vector<std::string> ExcludeAssetKinds{}; /**< @brief Resolved exclude-kind rules. */
        EAssetDependencyPolicy DependencyPolicy =
            EAssetDependencyPolicy::HardAndSoft; /**< @brief Resolved dependency policy. */
        EAssetChunkStrategy ChunkStrategy = EAssetChunkStrategy::Monolithic; /**< @brief Resolved chunk strategy. */
        bool AllowExplicitOverrideExcludes = false; /**< @brief Resolved explicit include precedence flag. */
        bool ArchiveEnabled = false; /**< @brief Resolved archive enablement flag. */
        std::string ArchiveFormat{}; /**< @brief Resolved archive/container format. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shared parser, serializer, validator, and inheritance resolver for descriptor build profiles.
     *
     * The profile service operates on the descriptor's `Profiles` section as a typed
     * build-system model. This allows the project descriptor to continue carrying the
     * authored JSON block while the build system moves to explicit C++ types for
     * profile logic, validation, and deterministic inheritance resolution.
     */
    class SNAPI_GAMEFRAMEWORK_API BuildProfileService final
    {
    public:
        /**
         * @brief Parse one descriptor `Profiles` JSON object into typed build profiles.
         * @param ProfilesJson Descriptor `Profiles` object.
         * @return Typed profiles in authored order or a structured error.
         */
        [[nodiscard]] static TExpected<std::vector<BuildProfile>>
        ParseProfiles(const nlohmann::ordered_json& ProfilesJson);

        /**
         * @brief Serialize typed build profiles back into the descriptor `Profiles` object shape.
         * @param Profiles Typed authored profiles.
         * @return Descriptor-compatible JSON object or a structured error.
         */
        [[nodiscard]] static TExpected<nlohmann::ordered_json>
        SerializeProfiles(const std::vector<BuildProfile>& Profiles);

        /**
         * @brief Validate authored profiles without performing inheritance resolution.
         * @param Profiles Typed authored profiles to validate.
         * @return Flat list of validation issues.
         */
        [[nodiscard]] static std::vector<BuildValidationIssue> Validate(const std::vector<BuildProfile>& Profiles);

        /**
         * @brief Resolve one named profile through inheritance, clear semantics, and defaults.
         * @param Profiles Typed authored profiles.
         * @param ProfileName Stable profile name to resolve.
         * @param MaxInheritanceDepth Maximum allowed inheritance depth before resolution fails.
         * @return Fully resolved profile or a structured error.
         */
        [[nodiscard]] static TExpected<ResolvedBuildProfile> ResolveProfile(const std::vector<BuildProfile>& Profiles,
                                                                            std::string_view ProfileName,
                                                                            std::size_t MaxInheritanceDepth = 4);
    };

} // namespace SnAPI::GameFramework
