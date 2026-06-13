#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "AssetManager.h"
#include "Export.h"
#include "GameRuntime.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Resolved project-file metadata used by `GameProjectRuntime`.
 *
 * This is the runtime-facing projection of one `.snproj.json` file after path
 * normalization and asset-root resolution.
 */
struct GameProjectInfo
{
    bool IsLoaded = false;
    std::string Name{};
    std::string ProjectFilePath{};
    std::string ProjectRootDirectory{};
    std::string AssetRoot{};
    std::string AssetRootDirectory{};
    std::string StartupLevelAsset{};
    std::string DefaultRenderSettingsAssetId{};
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Bootstrap settings for `GameProjectRuntime`.
 *
 * `ProjectFilePath` selects the authored project configuration to load during
 * normal development/runtime launches. `BootstrapPath` optionally overrides
 * that with either a project descriptor path, a packaged
 * `Config/ResolvedRuntimeConfig.json` path, or a directory that contains one of
 * those bootstrap files. `Runtime` controls the underlying `GameRuntime`
 * session that will host it.
 */
struct GameProjectRuntimeSettings
{
    std::string ProjectFilePath{}; /**< @brief Authored project descriptor path used by development/runtime launches. */
    std::string BootstrapPath{}; /**< @brief Optional bootstrap override path. Accepts a project descriptor, a packaged
                                      `Config/ResolvedRuntimeConfig.json` file, or a directory that contains one. */
    GameRuntimeSettings Runtime{};
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime host that loads one project file into a `GameRuntime`.
 *
 * This is the non-editor counterpart to the editor executable bootstrap path:
 * it owns one runtime session, one project-scoped asset manager, and the
 * resolved project metadata needed to load the startup level plus optional
 * default render settings asset.
 */
class SNAPI_GAMEFRAMEWORK_API GameProjectRuntime final
{
public:
    Result Initialize(const GameProjectRuntimeSettings& Settings);
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const;
    bool Update(float DeltaSeconds);

    [[nodiscard]] GameRuntime& Runtime();
    [[nodiscard]] const GameRuntime& Runtime() const;

    [[nodiscard]] const GameProjectInfo& Project() const;
    [[nodiscard]] ::SnAPI::AssetPipeline::AssetManager* AssetManager() const;

private:
    Result LoadProjectMetadata(std::string_view ProjectFilePath);
    Result CreateAssetManager();
    Result LoadStartupLevel();
    Result LoadDefaultRenderSettings();

    GameProjectRuntimeSettings m_settings{};
    GameProjectInfo m_project{};
    GameRuntime m_runtime{};
    std::unique_ptr<::SnAPI::AssetPipeline::AssetManager> m_assetManager{};
    NodeHandle m_defaultRenderSettingsNode{};
    bool m_initialized = false;
};

} // namespace SnAPI::GameFramework
