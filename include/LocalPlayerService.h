#pragma once

#include <string_view>

#include "Export.h"
#include "IGameService.h"

namespace SnAPI::GameFramework
{

class GameplayHost;

/**
 * @brief Default gameplay service that assigns local input devices to local players.
 * @remarks
 * Policy:
 * - player index `N` maps to gamepad `N` when connected
 * - player index `0` falls back to unassigned (keyboard/mouse/global input path)
 *   when no gamepad is present
 */
class SNAPI_GAMEFRAMEWORK_API LocalPlayerService final : public IGameService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] int Priority() const override;

    Result Initialize(GameplayHost& Host) override;
    void Tick(GameplayHost& Host, float DeltaSeconds) override;
    void OnLocalPlayerAdded(GameplayHost& Host, const NodeHandle& PlayerHandle) override;
    void OnLocalPlayerRemoved(GameplayHost& Host, const Uuid& PlayerId) override;
    void Shutdown(GameplayHost& Host) override;

private:
    void RefreshAssignments(GameplayHost& Host);
};

} // namespace SnAPI::GameFramework
