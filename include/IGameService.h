#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <typeindex>
#include <vector>

#include "Expected.h"
#include "Export.h"
#include "Handle.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class GameplayHost;
class LocalPlayer;

using NodeHandle = THandle<BaseNode>;

/**
 * @brief Contract for modular gameplay subsystems.
 * @remarks
 * Services are registered into `GameplayHost`, initialized in dependency order,
 * ticked every frame, and shut down in reverse order.
 */
class SNAPI_GAMEFRAMEWORK_API IGameService
{
public:
    virtual ~IGameService() = default;

    /**
     * @brief Stable service name for diagnostics/logging.
     */
    [[nodiscard]] virtual std::string_view Name() const = 0;

    /**
     * @brief Optional dependency list by concrete service type.
     */
    [[nodiscard]] virtual std::vector<std::type_index> Dependencies() const
    {
        return {};
    }

    /**
     * @brief Optional ordering priority among dependency-ready services.
     * @remarks Lower values initialize earlier.
     */
    [[nodiscard]] virtual int Priority() const
    {
        return 0;
    }

    /**
     * @brief Initialize service state.
     */
    virtual Result Initialize(GameplayHost& Host) = 0;

    /**
     * @brief Per-frame update hook.
     */
    virtual void Tick(GameplayHost& Host, float DeltaSeconds)
    {
        (void)Host;
        (void)DeltaSeconds;
    }

    /**
     * @brief Optional initial possession target resolver for newly joined players.
     * @remarks Return null handle to defer to host fallback selection.
     */
    virtual NodeHandle SelectInitialPossessionTarget(GameplayHost& Host, LocalPlayer& Player)
    {
        (void)Host;
        (void)Player;
        return {};
    }

    /**
     * @brief Policy hook for connection-authored join requests.
     * @remarks Return false to deny the request before host mutation occurs.
     */
    virtual bool AllowPlayerJoinRequest(GameplayHost& Host,
                                        std::uint64_t OwnerConnectionId,
                                        const std::string& RequestedName,
                                        std::optional<unsigned int> PreferredPlayerIndex,
                                        bool ReplicatedPlayer)
    {
        (void)Host;
        (void)OwnerConnectionId;
        (void)RequestedName;
        (void)PreferredPlayerIndex;
        (void)ReplicatedPlayer;
        return true;
    }

    /**
     * @brief Policy hook for connection-authored leave requests.
     * @remarks Return false to deny the request before host mutation occurs.
     */
    virtual bool AllowPlayerLeaveRequest(GameplayHost& Host,
                                         std::uint64_t OwnerConnectionId,
                                         std::optional<unsigned int> PlayerIndex)
    {
        (void)Host;
        (void)OwnerConnectionId;
        (void)PlayerIndex;
        return true;
    }

    /**
     * @brief Policy hook for connection-authored level-load requests.
     * @remarks Return false to deny the request before host mutation occurs.
     */
    virtual bool AllowLevelLoadRequest(GameplayHost& Host,
                                       std::uint64_t OwnerConnectionId,
                                       const std::string& RequestedName)
    {
        (void)Host;
        (void)OwnerConnectionId;
        (void)RequestedName;
        return true;
    }

    /**
     * @brief Policy hook for connection-authored level-unload requests.
     * @remarks Return false to deny the request before host mutation occurs.
     */
    virtual bool AllowLevelUnloadRequest(GameplayHost& Host, std::uint64_t OwnerConnectionId, const Uuid& LevelId)
    {
        (void)Host;
        (void)OwnerConnectionId;
        (void)LevelId;
        return true;
    }

    /**
     * @brief Level lifecycle callback.
     */
    virtual void OnLevelLoaded(GameplayHost& Host, NodeHandle LevelHandle)
    {
        (void)Host;
        (void)LevelHandle;
    }

    /**
     * @brief Level lifecycle callback.
     */
    virtual void OnLevelUnloaded(GameplayHost& Host, const Uuid& LevelId)
    {
        (void)Host;
        (void)LevelId;
    }

    /**
     * @brief Local-player lifecycle callback.
     */
    virtual void OnLocalPlayerAdded(GameplayHost& Host, NodeHandle PlayerHandle)
    {
        (void)Host;
        (void)PlayerHandle;
    }

    /**
     * @brief Local-player lifecycle callback.
     */
    virtual void OnLocalPlayerRemoved(GameplayHost& Host, const Uuid& PlayerId)
    {
        (void)Host;
        (void)PlayerId;
    }

    /**
     * @brief Connection lifecycle callback.
     */
    virtual void OnConnectionAdded(GameplayHost& Host, std::uint64_t OwnerConnectionId)
    {
        (void)Host;
        (void)OwnerConnectionId;
    }

    /**
     * @brief Connection lifecycle callback.
     */
    virtual void OnConnectionRemoved(GameplayHost& Host, std::uint64_t OwnerConnectionId)
    {
        (void)Host;
        (void)OwnerConnectionId;
    }

    /**
     * @brief Shutdown and release service state.
     */
    virtual void Shutdown(GameplayHost& Host) = 0;
};

} // namespace SnAPI::GameFramework
