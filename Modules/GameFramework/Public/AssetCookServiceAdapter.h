#pragma once

#include "BuildPlanner.h"
#include "BuildRequest.h"
#include "Expected.h"
#include "Export.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SnAPI::GameFramework
{

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One provenance entry recorded while resolving why an asset is in or out of the build.
     *
     * Provenance entries are written into selection artifacts and cook manifests so
     * later editor surfaces, CLI inspection, and history comparison can explain
     * which rule selected or excluded a concrete asset.
     */
    struct AssetSelectionProvenanceEntry
    {
        std::string Kind{}; /**< @brief Stable provenance kind such as `SelectedLevel` or `ExcludeFolder`. */
        std::string Value{}; /**< @brief Authored level, folder, or selector value associated with the rule. */
        bool Included = true; /**< @brief `true` when the rule kept the asset, `false` when it excluded it. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One selected source asset scheduled for cook/package work.
     *
     * The adapter keeps the authored logical name, concrete source path, and the
     * rule that selected the asset so later manifests and UI surfaces can explain
     * why the asset participated in the build.
     */
    struct AssetSelectionRecord
    {
        std::string LogicalName{}; /**< @brief Asset-root-relative logical source path. */
        std::filesystem::path SourcePath{}; /**< @brief Concrete source file path on disk. */
        std::string AssetKindLabel{}; /**< @brief Human-readable source kind label used by package include/exclude kind rules. */
        std::string SelectionReason{}; /**< @brief Human-readable selection reason such as `SelectedLevel`. */
        bool ExplicitSelection = false; /**< @brief `true` when the record came from an explicit level/asset selector. */
        bool Cookable = true; /**< @brief `true` when the record should be cooked through `SnAPI.AssetPipeline`. */
        bool StageVerbatim = false; /**< @brief `true` when the source file should be copied into staged `Assets/` verbatim. */
        std::string ChunkId{}; /**< @brief Resolved chunk identifier that will own the cooked runtime payload. */
        std::vector<AssetSelectionProvenanceEntry>
            Provenance{}; /**< @brief Ordered provenance entries that explain selection and exclusion decisions. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One resolved asset-selection snapshot for a frozen build request.
     *
     * The asset-selection result separates included and excluded assets so the
     * build can both execute the included set and later explain why matching
     * authored assets were removed by exclude rules.
     */
    struct AssetSelectionPlan
    {
        std::vector<AssetSelectionRecord> IncludedAssets{}; /**< @brief Ordered selected assets that will be cooked. */
        std::vector<AssetSelectionRecord>
            ExcludedAssets{}; /**< @brief Ordered assets that matched selection rules but were later excluded. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One chunk output planned for `.snpak` emission.
     *
     * Chunk plans are deterministic and derived from the resolved profile chunk
     * strategy plus the selected asset set. They are recorded into selection and
     * cook artifacts so package manifests can explain bundle structure.
     */
    struct AssetChunkPlanEntry
    {
        std::string ChunkId{}; /**< @brief Stable chunk identifier such as `Primary` or `Shared`. */
        std::string OutputFileName{}; /**< @brief Bundle file name written for the chunk. */
        std::vector<std::string> AssetLogicalNames{}; /**< @brief Ordered logical assets assigned to the chunk. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One cooked asset entry recorded by the asset-cook adapter.
     */
    struct CookedAssetRecord
    {
        struct DependencyRecord
        {
            std::string AssetId{}; /**< @brief Referenced asset id string when known. */
            std::string LogicalName{}; /**< @brief Referenced asset logical name when known. */
            std::string Kind{}; /**< @brief Generic dependency category text. */
        };

        std::string LogicalName{}; /**< @brief Asset-root-relative logical source path. */
        std::filesystem::path SourcePath{}; /**< @brief Concrete source file path on disk. */
        std::string SelectionReason{}; /**< @brief Selection reason propagated from the asset-selection stage. */
        std::string AssetId{}; /**< @brief Deterministic cooked asset id string. */
        std::string AssetKind{}; /**< @brief Cooked asset-kind identifier string. */
        std::string CookedPayloadType{}; /**< @brief Cooked payload-type identifier string. */
        std::uint64_t CookedPayloadSize = 0u; /**< @brief Serialized cooked payload size in bytes. */
        std::uint32_t BulkChunkCount = 0u; /**< @brief Number of cooked bulk chunks. */
        std::string ChunkId{}; /**< @brief Chunk identifier that owns the cooked runtime payload. */
        std::string SourceContentHash{}; /**< @brief Deterministic hash of the source asset bytes. */
        std::string SettingsHash{}; /**< @brief Deterministic hash of the resolved cook settings for the asset. */
        std::vector<DependencyRecord> Dependencies{}; /**< @brief Semantic authored asset dependencies reported by the pipeline. */
        std::vector<AssetSelectionProvenanceEntry>
            Provenance{}; /**< @brief Ordered provenance entries propagated from selection resolution. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Lightweight result returned by asset-cook node execution.
     */
    struct AssetCookNodeResult
    {
        std::string Message{}; /**< @brief Concise node-level summary. */
        std::vector<std::string> Outputs{}; /**< @brief Materialized output file paths. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Options that control whether the asset-cook adapter replaces placeholder nodes.
     */
    struct AssetCookServiceOptions
    {
        bool Enabled = false; /**< @brief `true` to replace placeholder asset-node execution with real asset cooking. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Adapter that resolves selected project assets, cooks them, and writes `.snpak` bundles.
     *
     * This adapter is the first real bridge from the build-system model layer into
     * `SnAPI.AssetPipeline`. It owns:
     * - asset selection from resolved build-profile fields
     * - supported source-asset enumeration under the project asset root
     * - source import/cook execution through `AssetPipelineEngine`
     * - cook-manifest emission
     * - `.snpak` bundle materialization into build-history artifact roots
     */
    class SNAPI_GAMEFRAMEWORK_API AssetCookServiceAdapter final
    {
    public:
        /**
         * @brief Resolve the included and excluded source assets for one frozen build request.
         * @param Request Frozen build request to inspect.
         * @return Deterministic included/excluded selection plan or a structured validation/filesystem error.
         */
        [[nodiscard]] static TExpected<AssetSelectionPlan> ResolveAssetSelection(const ResolvedBuildRequest& Request);

        /**
         * @brief Resolve the concrete source assets selected by one frozen build request.
         * @param Request Frozen build request to inspect.
         * @return Ordered included source assets or a structured validation/filesystem error.
         * @remarks
         * This convenience wrapper preserves the earlier API while the richer
         * selection-plan model is used internally for provenance and exclusion tracking.
         */
        [[nodiscard]] static TExpected<std::vector<AssetSelectionRecord>>
        ResolveSelectedAssets(const ResolvedBuildRequest& Request);

        /**
         * @brief Execute the asset-selection node for one planned build graph.
         * @param Request Frozen build request.
         * @param Graph Planned build graph.
         * @param Node Planned selection node.
         * @return Node result or a structured error.
         */
        [[nodiscard]] static TExpected<AssetCookNodeResult> ExecuteResolveAssetSelection(
            const ResolvedBuildRequest& Request, const BuildGraph& Graph, const BuildGraphNode& Node);

        /**
         * @brief Execute the asset-enumeration node for one planned build graph.
         * @param Request Frozen build request.
         * @param Graph Planned build graph.
         * @param Node Planned enumeration node.
         * @return Node result or a structured error.
         */
        [[nodiscard]] static TExpected<AssetCookNodeResult> ExecuteEnumerateAssets(
            const ResolvedBuildRequest& Request, const BuildGraph& Graph, const BuildGraphNode& Node);

        /**
         * @brief Execute the cook node for one planned build graph.
         * @param Request Frozen build request.
         * @param Graph Planned build graph.
         * @param Node Planned cook node.
         * @return Node result or a structured error.
         */
        [[nodiscard]] static TExpected<AssetCookNodeResult> ExecuteCookAssets(
            const ResolvedBuildRequest& Request, const BuildGraph& Graph, const BuildGraphNode& Node);

        /**
         * @brief Execute the cook-manifest node for one planned build graph.
         * @param Request Frozen build request.
         * @param Graph Planned build graph.
         * @param Node Planned cook-manifest node.
         * @return Node result or a structured error.
         */
        [[nodiscard]] static TExpected<AssetCookNodeResult> ExecuteWriteCookManifest(
            const ResolvedBuildRequest& Request, const BuildGraph& Graph, const BuildGraphNode& Node);

        /**
         * @brief Execute the `.snpak` bundle node for one planned build graph.
         * @param Request Frozen build request.
         * @param Graph Planned build graph.
         * @param Node Planned bundle node.
         * @return Node result or a structured error.
         */
        [[nodiscard]] static TExpected<AssetCookNodeResult> ExecuteWriteSnpak(
            const ResolvedBuildRequest& Request, const BuildGraph& Graph, const BuildGraphNode& Node);
    };

} // namespace SnAPI::GameFramework
