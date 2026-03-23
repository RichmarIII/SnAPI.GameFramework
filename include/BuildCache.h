#pragma once

#include "BuildPlanner.h"
#include "BuildRequest.h"
#include "Expected.h"
#include "Export.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SnAPI::GameFramework
{

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Artifact kind recorded by one persistent build-cache entry.
     */
    enum class EBuildCacheArtifactKind : std::uint8_t
    {
        File = 0, /**< @brief Artifact is one regular file. */
        Directory, /**< @brief Artifact is one directory tree. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief One cached artifact path recorded for one cacheable build node.
     */
    struct BuildCacheArtifact
    {
        std::filesystem::path Path{}; /**< @brief Canonical stored artifact path from a prior successful execution. */
        EBuildCacheArtifactKind Kind = EBuildCacheArtifactKind::File; /**< @brief Artifact path kind. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Persistent cache entry stored for one cacheable build node.
     *
     * Build-cache entries keep the node cache key, the source request hash, and the
     * materialized artifact paths that can be restored into a future build's output
     * locations when the same node cache key is encountered again.
     */
    struct BuildCacheEntry
    {
        std::string CacheKey{}; /**< @brief Stable node cache key produced by `BuildPlannerService`. */
        std::string RequestHash{}; /**< @brief Frozen request hash that produced the cached outputs. */
        EBuildNodeType NodeType = EBuildNodeType::LoadProject; /**< @brief Node type that owns the entry. */
        std::string StoredAtUtc{}; /**< @brief ISO-8601 UTC timestamp for the cache entry write. */
        std::vector<BuildCacheArtifact> Artifacts{}; /**< @brief Ordered cached artifacts aligned with the node outputs. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Result returned when one build node successfully restores cached outputs.
     */
    struct BuildCacheRestoreResult
    {
        std::string Message{}; /**< @brief Concise node-level cache-hit summary. */
        std::vector<std::string> Outputs{}; /**< @brief Restored output paths written into the current build. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Persistent node-cache service used by build execution.
     *
     * The build planner already emits deterministic base node cache keys. This service
     * combines those base keys with current node-input fingerprints and turns the result
     * into actual reusable outputs by:
     * - storing successful node outputs under `Intermediate/BuildCache/`
     * - restoring cached artifacts into a later build's planned output paths
     * - keeping the cache format inspectable through small JSON entry files
     *
     * Code-build nodes intentionally rely on CMake's own incremental state instead of
     * this higher-level cache so changes in engine/runtime source do not get masked by
     * coarse cross-build artifact reuse.
     */
    class SNAPI_GAMEFRAMEWORK_API BuildCacheService final
    {
    public:
        /**
         * @brief Return the root directory used for persistent node-cache entries.
         * @param Request Frozen request that owns the project intermediate directory.
         * @return Canonical persistent build-cache root.
         */
        [[nodiscard]] static std::filesystem::path CacheRootDirectory(const ResolvedBuildRequest& Request);

        /**
         * @brief Return `true` when one planned node type participates in persistent cache reuse.
         * @param Node Planned node to inspect.
         * @return `true` when the node is eligible for persistent cache restore/store.
         */
        [[nodiscard]] static bool SupportsPersistentCache(const BuildGraphNode& Node);

        /**
         * @brief Attempt to restore one cacheable node's outputs into the current build.
         * @param Request Frozen request that owns the cache root.
         * @param Node Planned node whose outputs should be restored.
         * @return Cache-hit restore result, `std::nullopt` on cache miss, or a structured restore error.
         */
        [[nodiscard]] static TExpected<std::optional<BuildCacheRestoreResult>>
        TryRestore(const ResolvedBuildRequest& Request, const BuildGraphNode& Node);

        /**
         * @brief Persist one successful cacheable node's outputs for future reuse.
         * @param Request Frozen request that owns the cache root.
         * @param Node Successful planned node whose outputs should be recorded.
         * @return Success or a structured filesystem/serialization error.
         */
        [[nodiscard]] static Result Store(const ResolvedBuildRequest& Request, const BuildGraphNode& Node);
    };

} // namespace SnAPI::GameFramework
