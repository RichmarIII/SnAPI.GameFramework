#pragma once

#include "BuildPlanner.h"
#include "Expected.h"
#include "Export.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace SnAPI::GameFramework
{

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One staged output file recorded in `PackageManifest.json`.
     */
    struct PackageManifestOutputFile
    {
        std::string RelativePath{}; /**< @brief Stage-root relative file path using normalized `/` separators. */
        std::uint64_t SizeBytes = 0u; /**< @brief File size in bytes. */
        std::string ContentHash{}; /**< @brief Deterministic lowercase content hash for the staged file. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One staged `.snpak` bundle recorded in `PackageManifest.json`.
     */
    struct PackageManifestSnpakFile
    {
        std::string RelativePath{}; /**< @brief Stage-root relative `.snpak` path using normalized `/` separators. */
        std::string ChunkId{}; /**< @brief Deterministic chunk identifier, currently derived from the pack file stem. */
        std::uint64_t AssetCount = 0u; /**< @brief Number of assets discovered in the bundle when readable. */
        std::uint64_t SizeBytes = 0u; /**< @brief Bundle size in bytes. */
        std::string ContentHash{}; /**< @brief Deterministic lowercase content hash for the bundle. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One project module recorded in the final package manifest.
     */
    struct PackageManifestModule
    {
        std::string Name{}; /**< @brief Stable module name. */
        EProjectModuleType Type = EProjectModuleType::Runtime; /**< @brief Authored module role. */
        bool LoadInEditor = false; /**< @brief `true` when the module participates in editor targets. */
        bool LoadInRuntime = false; /**< @brief `true` when the module participates in runtime package builds. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One warning surfaced into `PackageManifest.json`.
     */
    struct PackageManifestWarning
    {
        std::string RuleId{}; /**< @brief Stable warning rule identifier. */
        std::string Message{}; /**< @brief Human-readable warning text. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Canonical staged package manifest emitted for one build invocation.
     *
     * The package manifest is the packaging-side truth record for the staged
     * output tree. It captures the build identity, resolved target settings,
     * included levels, participating runtime modules, staged files with hashes,
     * and `.snpak` bundle metadata.
     */
    struct PackageManifest
    {
        std::string BuildId{}; /**< @brief Unique build invocation id. */
        std::string RequestHash{}; /**< @brief Frozen build-request hash. */
        std::string ProjectId{}; /**< @brief Stable authored project identifier. */
        std::string ProjectName{}; /**< @brief Stable authored project name. */
        std::string ProfileName{}; /**< @brief Authored profile name, or empty for ad hoc requests. */
        std::string TargetPlatform{}; /**< @brief Resolved target-platform identifier. */
        EBuildConfiguration Configuration = EBuildConfiguration::Development; /**< @brief Resolved build configuration. */
        std::string ExecutionEnvironment{}; /**< @brief Resolved host or container execution environment. */
        std::uint32_t DescriptorSchemaVersion = 0u; /**< @brief Project-descriptor schema version used by the build. */
        std::string MinimumToolVersion{}; /**< @brief Minimum tool version recorded in the project descriptor. */
        std::filesystem::path StageDirectory{}; /**< @brief Canonical staged output root represented by the manifest. */
        std::vector<std::string> IncludedLevels{}; /**< @brief Selected or implied packaged level logical names. */
        std::vector<PackageManifestOutputFile> OutputFiles{}; /**< @brief Stage-root regular files with sizes and hashes. */
        std::vector<PackageManifestSnpakFile> SnpakFiles{}; /**< @brief Staged `.snpak` bundle metadata. */
        std::vector<PackageManifestModule> Modules{}; /**< @brief Participating project modules. */
        std::vector<PackageManifestWarning> Warnings{}; /**< @brief Non-blocking warnings surfaced into the package. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shared service that emits canonical staged package manifests.
     *
     * The package-manifest service bridges the staged filesystem tree with the
     * machine-readable package metadata required by editor tooling, CLI output,
     * history/reporting, and future archive/signing stages. It is responsible for:
     * - enumerating staged regular files deterministically
     * - hashing staged contents
     * - collecting `.snpak` bundle metadata
     * - surfacing packaged modules and non-blocking warnings
     * - serializing `PackageManifest.json` and `StageFileHashes.json`
     */
    class SNAPI_GAMEFRAMEWORK_API PackageManifestService final
    {
    public:
        /**
         * @brief Create one canonical package manifest from a frozen request and staged build graph.
         * @param Request Frozen request that produced the stage tree.
         * @param Graph Planned graph that owns the stage directory and build id.
         * @return Canonical package manifest or a structured validation/filesystem error.
         */
        [[nodiscard]] static TExpected<PackageManifest> Create(const ResolvedBuildRequest& Request,
                                                               const BuildGraph& Graph);

        /**
         * @brief Validate one package manifest.
         * @param Manifest Package manifest to validate.
         * @return Flat list of validation issues.
         */
        [[nodiscard]] static std::vector<BuildValidationIssue> Validate(const PackageManifest& Manifest);

        /**
         * @brief Serialize one package manifest into canonical JSON text.
         * @param Manifest Package manifest to serialize.
         * @param Indent Pretty-print indentation width passed to `nlohmann::json::dump`.
         * @return Canonical JSON text or a structured serialization error.
         */
        [[nodiscard]] static TExpected<std::string> Serialize(const PackageManifest& Manifest, int Indent = 2);

        /**
         * @brief Serialize the stage-file hash view for one package manifest.
         * @param Manifest Package manifest that owns the staged output-file list.
         * @param Indent Pretty-print indentation width passed to `nlohmann::json::dump`.
         * @return Canonical JSON text or a structured serialization error.
         * @remarks
         * `StageFileHashes.json` intentionally stays redundant with `PackageManifest.json`
         * so automation can diff file hashes without reading the rest of the package metadata.
         */
        [[nodiscard]] static TExpected<std::string> SerializeStageFileHashes(const PackageManifest& Manifest,
                                                                             int Indent = 2);
    };

} // namespace SnAPI::GameFramework
