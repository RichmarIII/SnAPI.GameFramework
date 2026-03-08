#pragma once

#include <string_view>

#include "Export.h"
#include "IGameService.h"

namespace SnAPI::GameFramework
{

class GameplayHost;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Default gameplay service that maps locally owned players to local input devices.
 *
 * `LocalPlayerService` is the stock service registered by `GameplayHost` when
 * `GameRuntimeGameplaySettings::RegisterDefaultLocalPlayerService` is enabled.
 * It inspects the current input snapshot and keeps `LocalPlayer` device assignment state
 * synchronized with the available local gamepads.
 *
 * Policy:
 * - locally owned player index `N` maps to gamepad slot `N` when one exists
 * - remote-owned players are explicitly stripped of local device assignments
 * - player index `0` naturally falls back to the unassigned keyboard/mouse path when no gamepad exists
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see LocalPlayer
 * @see GameplayHost
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
