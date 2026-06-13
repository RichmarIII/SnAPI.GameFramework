#pragma once

#include "BuildRequest.h"
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
     * @brief High-level stage grouping used by the build planner and execution graph.
     *
     * Stages keep the planned graph readable in tooling and reports while still
     * allowing each stage to contain multiple explicit nodes with dependencies.
     */
    enum class EBuildStage : std::uint8_t
    {
        Preflight = 0, /**< @brief Descriptor loading, request validation, and environment resolution. */
        Planning, /**< @brief Module and asset-selection planning. */
        Code, /**< @brief Build-file generation, configure, and code compilation. */
        Assets, /**< @brief Asset enumeration, cooking, manifests, and bundle generation. */
        Staging, /**< @brief Stage-tree creation and file staging. */
        Finalize, /**< @brief Final manifests, reports, archives, and signing. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Typed build-graph node role emitted by the planner.
     *
     * The planner uses explicit node kinds so editor UI, CLI output, reports, and
     * later executors can reason about the graph without inferring behavior from
     * ad hoc string names.
     */
    enum class EBuildNodeType : std::uint8_t
    {
        LoadProject = 0, /**< @brief Load and freeze the owning project descriptor. */
        ValidateResolvedRequest, /**< @brief Validate the frozen request before execution planning. */
        ResolveExecutionEnvironment, /**< @brief Resolve the container or host execution environment. */
        ResolveModuleSet, /**< @brief Resolve the participating module set for the request. */
        ResolveAssetSelection, /**< @brief Resolve the selected asset and level set for the request. */
        GenerateProjectBuildFiles, /**< @brief Emit generated project/plugin CMake bridge files. */
        ConfigureCMake, /**< @brief Configure the native CMake build tree. */
        BuildCode, /**< @brief Compile and link project code outputs. */
        EnumerateAssets, /**< @brief Enumerate candidate assets before cook filtering. */
        CookAssets, /**< @brief Cook dirty asset payloads for the target platform. */
        WriteCookManifest, /**< @brief Emit the canonical cook manifest. */
        WriteSnpak, /**< @brief Bundle cooked asset payloads into `.snpak` outputs. */
        CreateStageTree, /**< @brief Create the normalized staging-directory tree. */
        StageBinaries, /**< @brief Copy compiled binaries and symbols into the stage tree. */
        StageAssets, /**< @brief Copy `.snpak` outputs into the stage tree. */
        StageConfigs, /**< @brief Copy resolved runtime configuration into the stage tree. */
        WritePackageManifest, /**< @brief Emit the final package manifest. */
        WriteBuildReport, /**< @brief Emit the final machine-readable build report. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One planned build-graph node emitted by `BuildPlannerService`.
     *
     * `Inputs` and `Outputs` are normalized logical or filesystem paths in authored
     * order. They are intentionally lightweight at this stage so the planner can
     * express intent before per-node executors and richer payload typing arrive.
     */
    struct BuildGraphNode
    {
        std::uint32_t Id = 0u; /**< @brief Stable node identifier unique within one graph. */
        EBuildStage Stage = EBuildStage::Preflight; /**< @brief Stage bucket that owns the node. */
        EBuildNodeType Type = EBuildNodeType::LoadProject; /**< @brief Typed node role. */
        std::string Name{}; /**< @brief Human-readable node name shown in UI and reports. */
        std::vector<std::uint32_t> Dependencies{}; /**< @brief Upstream node ids that must complete first. */
        std::vector<std::string> Inputs{}; /**< @brief Normalized logical or filesystem inputs. */
        std::vector<std::string> Outputs{}; /**< @brief Normalized logical or filesystem outputs. */
        std::string CacheKey{}; /**< @brief Deterministic cache key seed for the node. */
        bool Cacheable = true; /**< @brief `true` when node outputs are intended to participate in caching. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Deterministic staged build graph produced from one frozen build request.
     *
     * `BuildGraph` is the planner-facing executable description of the requested
     * package/build operation. It records the concrete build invocation id, the
     * request hash it was derived from, normalized history/stage directories, and
     * the ordered node list that later execution services will schedule.
     */
    struct BuildGraph
    {
        std::string BuildId{}; /**< @brief Unique build invocation id. */
        std::string RequestHash{}; /**< @brief Frozen request hash used to seed cache keys and reports. */
        std::filesystem::path HistoryDirectory{}; /**< @brief Planned build-history directory for this invocation. */
        std::filesystem::path StageDirectory{}; /**< @brief Planned normalized staging-directory root. */
        std::vector<BuildGraphNode> Nodes{}; /**< @brief Ordered staged node list. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Optional planner overrides applied when generating one build graph.
     *
     * The planner options are intentionally narrow in the first slice so callers
     * can request a stable `BuildId` for tests, automation, or dry-run diffing
     * without introducing a second full request-resolution model.
     */
    struct BuildPlannerOptions
    {
        std::string BuildId{}; /**< @brief Optional explicit build invocation id. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shared service that turns frozen requests into deterministic staged build graphs.
     *
     * The planner is the first concrete bridge between request resolution and later
     * build execution. It is responsible for:
     * - validating the frozen request before planning
     * - assigning a build id and normalized history/staging directories
     * - emitting an explicit staged node list with deterministic ordering
     * - validating and serializing the resulting graph for UI, CLI, and tests
     */
    class SNAPI_GAMEFRAMEWORK_API BuildPlannerService final
    {
    public:
        /**
         * @brief Create one deterministic staged build graph from a frozen request.
         * @param Request Frozen request resolved by `BuildRequestService`.
         * @param Options Optional planner overrides such as a fixed build id.
         * @return Planned build graph or a structured error.
         */
        [[nodiscard]] static TExpected<BuildGraph> CreatePlan(const ResolvedBuildRequest& Request,
                                                              const BuildPlannerOptions& Options = {});

        /**
         * @brief Validate one planned build graph.
         * @param Graph Planned graph to validate.
         * @return Flat list of validation issues.
         */
        [[nodiscard]] static std::vector<BuildValidationIssue> Validate(const BuildGraph& Graph);

        /**
         * @brief Serialize one planned build graph into canonical JSON text.
         * @param Graph Planned graph to serialize.
         * @param Indent Pretty-print indentation width passed to `nlohmann::json::dump`.
         * @return Canonical JSON text or a structured serialization error.
         */
        [[nodiscard]] static TExpected<std::string> Serialize(const BuildGraph& Graph, int Indent = 2);
    };

} // namespace SnAPI::GameFramework
