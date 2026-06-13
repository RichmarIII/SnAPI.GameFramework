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
 * @ingroup SnAPI_GameFramework
 * @brief Contract for modular gameplay services owned by `GameplayHost`.
 *
 * `IGameService` is the extension point for gameplay features that do not belong inside
 * one monolithic `IGame` or `IGameMode` implementation. Services are registered into the
 * host, ordered by declared dependencies and priority, initialized once, ticked every frame,
 * and shut down in reverse order.
 *
 * Use services for:
 * - modular policy layers
 * - player/session support systems
 * - gameplay-side adapters that need lifecycle hooks but are not node/component types
 *
 * Ownership and lifetime:
 * - Services are heap-allocated and transferred to `GameplayHost`.
 * - The host owns the service instances and controls their lifecycle.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see GameplayHost
 * @see IGame
 * @see IGameMode
 */
class SNAPI_GAMEFRAMEWORK_API IGameService
{
public:
    virtual ~IGameService() = default;

    /** @brief Stable diagnostic name for the concrete service implementation. */
    [[nodiscard]] virtual std::string_view Name() const = 0;

    /**
     * @brief Optional dependency list expressed as concrete service types.
     * @return Types that must already be initialized before this service may initialize.
     * @remarks Missing or circular dependencies cause host initialization to fail.
     */
    [[nodiscard]] virtual std::vector<std::type_index> Dependencies() const
    {
        return {};
    }

    /**
     * @brief Optional ordering priority among dependency-ready services.
     * @return Relative priority value.
     * @remarks Lower values initialize earlier when dependencies are otherwise satisfied.
     */
    [[nodiscard]] virtual int Priority() const
    {
        return 0;
    }

    /**
     * @brief Initialize service state.
     * @param Host Borrowed gameplay host.
     * @return Success or an initialization error.
     */
    virtual Result Initialize(GameplayHost& Host) = 0;

    /**
     * @brief Per-frame service update.
     * @param Host Borrowed gameplay host.
     * @param DeltaSeconds Frame delta time in seconds.
     */
    virtual void Tick(GameplayHost& Host, float DeltaSeconds)
    {
        (void)Host;
        (void)DeltaSeconds;
    }

    /**
     * @brief Optional initial possession-target resolver for a newly joined player.
     * @param Host Borrowed gameplay host.
     * @param Player Borrowed player node being initialized.
     * @return Handle to a possession target, or a null handle to defer to later resolvers.
     */
    virtual NodeHandle SelectInitialPossessionTarget(GameplayHost& Host, LocalPlayer& Player)
    {
        (void)Host;
        (void)Player;
        return {};
    }

    /**
     * @brief Policy hook for connection-authored join requests.
     * @param Host Borrowed gameplay host.
     * @param OwnerConnectionId Requesting connection id.
     * @param RequestedName Requested player name, possibly empty.
     * @param PreferredPlayerIndex Requested player index, if any.
     * @param ReplicatedPlayer Requested replication state for the created player node.
     * @return `true` to allow the request, `false` to deny it.
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
     * @param Host Borrowed gameplay host.
     * @param OwnerConnectionId Requesting connection id.
     * @param PlayerIndex Requested player index, or `std::nullopt` for all caller-owned players.
     * @return `true` to allow the request, `false` to deny it.
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
     * @param Host Borrowed gameplay host.
     * @param OwnerConnectionId Requesting connection id.
     * @param RequestedName Requested level name, possibly empty.
     * @return `true` to allow the request, `false` to deny it.
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
     * @param Host Borrowed gameplay host.
     * @param OwnerConnectionId Requesting connection id.
     * @param LevelId Stable id of the level targeted for unload.
     * @return `true` to allow the request, `false` to deny it.
     */
    virtual bool AllowLevelUnloadRequest(GameplayHost& Host, std::uint64_t OwnerConnectionId, const Uuid& LevelId)
    {
        (void)Host;
        (void)OwnerConnectionId;
        (void)LevelId;
        return true;
    }

    /** @brief Notification that a level became present in the world. @param Host Borrowed gameplay host. @param LevelHandle Loaded level handle. */
    virtual void OnLevelLoaded(GameplayHost& Host, const NodeHandle& LevelHandle)
    {
        (void)Host;
        (void)LevelHandle;
    }

    /** @brief Notification that a level was removed from the world. @param Host Borrowed gameplay host. @param LevelId Stable id of the unloaded level. */
    virtual void OnLevelUnloaded(GameplayHost& Host, const Uuid& LevelId)
    {
        (void)Host;
        (void)LevelId;
    }

    /** @brief Notification that a local-player node was added. @param Host Borrowed gameplay host. @param PlayerHandle Added player handle. */
    virtual void OnLocalPlayerAdded(GameplayHost& Host, const NodeHandle& PlayerHandle)
    {
        (void)Host;
        (void)PlayerHandle;
    }

    /** @brief Notification that a local-player node was removed. @param Host Borrowed gameplay host. @param PlayerId Stable id of the removed player. */
    virtual void OnLocalPlayerRemoved(GameplayHost& Host, const Uuid& PlayerId)
    {
        (void)Host;
        (void)PlayerId;
    }

    /** @brief Notification that a connection became visible to the gameplay host. @param Host Borrowed gameplay host. @param OwnerConnectionId Connection id. */
    virtual void OnConnectionAdded(GameplayHost& Host, std::uint64_t OwnerConnectionId)
    {
        (void)Host;
        (void)OwnerConnectionId;
    }

    /** @brief Notification that a connection is no longer visible to the gameplay host. @param Host Borrowed gameplay host. @param OwnerConnectionId Connection id. */
    virtual void OnConnectionRemoved(GameplayHost& Host, std::uint64_t OwnerConnectionId)
    {
        (void)Host;
        (void)OwnerConnectionId;
    }

    /**
     * @brief Shutdown and release service state.
     * @param Host Borrowed gameplay host.
     */
    virtual void Shutdown(GameplayHost& Host) = 0;
};

} // namespace SnAPI::GameFramework
