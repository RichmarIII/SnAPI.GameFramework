#include "LocalPlayerService.h"

#include "GameplayHost.h"
#include "LocalPlayer.h"
#include "NodeCast.h"
#include "Profiling.h"

#include <algorithm>
#include <vector>

#include "GameRuntime.h"

namespace SnAPI::GameFramework
{

std::string_view LocalPlayerService::Name() const
{
    return "LocalPlayerService";
}

int LocalPlayerService::Priority() const
{
    return -100;
}

Result LocalPlayerService::Initialize(GameplayHost& Host)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    RefreshAssignments(Host);
    return Ok();
}

void LocalPlayerService::Tick(GameplayHost& Host, const float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    (void)DeltaSeconds;
    RefreshAssignments(Host);
}

void LocalPlayerService::OnLocalPlayerAdded(GameplayHost& Host, const NodeHandle& PlayerHandle)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    (void)PlayerHandle;
    RefreshAssignments(Host);
}

void LocalPlayerService::OnLocalPlayerRemoved(GameplayHost& Host, const Uuid& PlayerId)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    (void)PlayerId;
    RefreshAssignments(Host);
}

void LocalPlayerService::Shutdown(GameplayHost& Host)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
    (void)Host;
}

void LocalPlayerService::RefreshAssignments(GameplayHost& Host)
{
    SNAPI_GF_PROFILE_FUNCTION("Gameplay");
#if defined(SNAPI_GF_ENABLE_INPUT)
    auto* WorldPtr = Host.Runtime().WorldPtr();
    if (!WorldPtr || !WorldPtr->Input().IsInitialized())
    {
        return;
    }

    const auto* Devices = WorldPtr->Input().Devices();
    if (!Devices)
    {
        return;
    }

    std::vector<SnAPI::Input::DeviceId> GamepadIds{};
    GamepadIds.reserve(Devices->size());
    for (const auto& Device : *Devices)
    {
        if (!Device || !Device->IsConnected())
        {
            continue;
        }

        const auto& Identity = Device->Identity();
        if (Identity.DeviceType != SnAPI::Input::EInputDeviceType::Gamepad)
        {
            continue;
        }
        if (!Identity.Id.IsValid())
        {
            continue;
        }

        GamepadIds.push_back(Identity.Id);
    }

    std::sort(GamepadIds.begin(),
              GamepadIds.end(),
              [](const SnAPI::Input::DeviceId Left, const SnAPI::Input::DeviceId Right) {
                  return Left.Value < Right.Value;
              });

    auto Players = Host.LocalPlayers();
    std::vector<NodeHandle> LocalOwnedPlayers{};
    LocalOwnedPlayers.reserve(Players.size());
    for (const NodeHandle PlayerHandle : Players)
    {
        auto* Player = NodeCast<LocalPlayer>(PlayerHandle.Borrowed());
        if (!Player)
        {
            continue;
        }

        if (Player->GetOwnerConnectionId() != 0)
        {
            // Remote-owned players should never consume local runtime input devices.
            if (Player->GetAssignedInputDevice().IsValid())
            {
                Player->EditAssignedInputDevice() = {};
            }
            if (Player->GetUseAssignedInputDevice())
            {
                Player->EditUseAssignedInputDevice() = false;
            }
            continue;
        }

        LocalOwnedPlayers.push_back(PlayerHandle);
    }

    std::sort(LocalOwnedPlayers.begin(), LocalOwnedPlayers.end(), [](const NodeHandle& Left, const NodeHandle& Right) {
        const auto* LeftPlayer = NodeCast<LocalPlayer>(Left.Borrowed());
        const auto* RightPlayer = NodeCast<LocalPlayer>(Right.Borrowed());
        const unsigned int LeftIndex = LeftPlayer ? LeftPlayer->GetPlayerIndex() : 0U;
        const unsigned int RightIndex = RightPlayer ? RightPlayer->GetPlayerIndex() : 0U;
        return LeftIndex < RightIndex;
    });

    for (const NodeHandle PlayerHandle : LocalOwnedPlayers)
    {
        auto* Player = NodeCast<LocalPlayer>(PlayerHandle.Borrowed());
        if (!Player)
        {
            continue;
        }

        SnAPI::Input::DeviceId DesiredDevice{};
        bool DesiredUseAssignedDevice = false;
        const std::size_t PlayerSlot = static_cast<std::size_t>(Player->GetPlayerIndex());
        if (PlayerSlot < GamepadIds.size())
        {
            DesiredDevice = GamepadIds[PlayerSlot];
            DesiredUseAssignedDevice = true;
        }

        if (Player->GetAssignedInputDevice() != DesiredDevice)
        {
            Player->EditAssignedInputDevice() = DesiredDevice;
        }
        if (Player->GetUseAssignedInputDevice() != DesiredUseAssignedDevice)
        {
            Player->EditUseAssignedInputDevice() = DesiredUseAssignedDevice;
        }
    }
#else
    (void)Host;
#endif
}

} // namespace SnAPI::GameFramework
