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
     * @brief Output-finalization options applied after staged package generation succeeds.
     *
     * The stage tree under `Saved/BuildHistory/<BuildId>/Stage/` remains the canonical
     * intermediate package representation. `PackageOutputOptions` control the final
     * user-facing copy and optional archive emitted from that staged tree.
     */
    struct PackageOutputOptions
    {
        bool CopyStageToOutput = true; /**< @brief `true` to copy the final stage tree into a user-facing package directory. */
        std::filesystem::path
            OutputRootDirectory{}; /**< @brief Optional destination root. Empty defaults to `<Saved>/Packages`. */
        std::string
            PackageDirectoryName{}; /**< @brief Optional package-directory leaf name. Empty uses the standard naming convention. */
        bool ArchiveEnabled = false; /**< @brief `true` to emit an archive from the copied package directory. */
        std::string ArchiveFormat{}; /**< @brief Optional archive format. Empty defaults to the resolved profile format or `zip`. */
        std::string ArchiveFileName{}; /**< @brief Optional archive file name. Empty uses the standard naming convention. */
        std::string CMakeExecutable = "cmake"; /**< @brief CMake executable used for archive emission via `cmake -E tar`. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Final copied/archive output produced from one staged package tree.
     */
    struct PackageOutputResult
    {
        std::filesystem::path OutputRootDirectory{}; /**< @brief Resolved final output root directory. */
        std::filesystem::path PackageDirectoryPath{}; /**< @brief Copied package directory path when stage copying is enabled. */
        std::filesystem::path ArchiveFilePath{}; /**< @brief Archive output path when archive emission is enabled. */
        std::vector<std::filesystem::path> CopiedFiles{}; /**< @brief Copied regular files under the user-facing package directory. */
    };

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Shared service that promotes a staged build tree into final user-facing outputs.
     *
     * `PackageOutputService` keeps output naming, destination handling, stage copying,
     * and archive emission out of `BuildExecutionService` so the same behavior can be
     * reused by CLI flows, editor packaging, and later automation surfaces.
     */
    class SNAPI_GAMEFRAMEWORK_API PackageOutputService final
    {
    public:
        /**
         * @brief Copy and optionally archive one completed staged package tree.
         * @param Request Frozen request that produced the stage tree.
         * @param Graph Planned graph that owns the build id and stage directory.
         * @param Options Final output and archive options.
         * @return Final copied/archive output result or a structured filesystem/tool error.
         */
        [[nodiscard]] static TExpected<PackageOutputResult> Finalize(const ResolvedBuildRequest& Request,
                                                                     const BuildGraph& Graph,
                                                                     const PackageOutputOptions& Options = {});
    };

} // namespace SnAPI::GameFramework
