#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "Expected.h"
#include "Export.h"
#include "Handle.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class BaseNode;
class GameplayHost;
class IGameMode;
class LocalPlayer;

using NodeHandle = THandle<BaseNode>;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Session-wide gameplay root that exists independently of server-only rule enforcement.
 *
 * `IGame` is the gameplay/session object that most closely corresponds to a "game instance"
 * style abstraction. It can exist on both server and clients and is responsible for
 * session-wide behavior that is not inherently server-only, such as broad lifecycle flow,
 * player-start selection hints, possession defaults, and policy checks that should run
 * everywhere the session is represented.
 *
 * Responsibility split:
 * - `IGame` is the session-wide layer and may exist on both server and client.
 * - `IGameMode` is the authoritative server-only rule layer.
 * - `GameplayHost` owns the actual `IGame` instance and calls it at well-defined points.
 *
 * Ownership and lifetime:
 * - Implementations are heap-allocated and transferred to `GameplayHost`.
 * - The host owns the instance for the duration of the active gameplay session.
 * - `GameplayHost&` parameters are borrowed and remain owned by the runtime.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see GameplayHost
 * @see IGameMode
 */
class SNAPI_GAMEFRAMEWORK_API IGame
{
public:
    virtual ~IGame() = default;

    /** @brief Stable diagnostic name for the concrete game implementation. */
    [[nodiscard]] virtual std::string_view Name() const = 0;

    /**
     * @brief Initialize game state for a newly started gameplay session.
     * @param Host Borrowed gameplay host that owns this instance.
     * @return Success or an initialization error.
     * @pre `Host` is initialized and bound to a valid runtime/world.
     * @post On success, the instance is considered active until `Shutdown()` is called.
     */
    virtual Result Initialize(GameplayHost& Host) = 0;

    /**
     * @brief Per-frame session update.
     * @param Host Borrowed gameplay host.
     * @param DeltaSeconds Frame delta time in seconds.
     * @remarks Called from `GameplayHost::Tick()` after service ticks.
     */
    virtual void Tick(GameplayHost& Host, float DeltaSeconds)
    {
        (void)Host;
        (void)DeltaSeconds;
    }

    /**
     * @brief Optional hook that creates the initial authoritative game mode.
     * @param Host Borrowed gameplay host.
     * @return Owned game mode instance or `nullptr` to skip automatic mode creation.
     * @remarks
     * Called only on server authority when runtime settings do not override
     * mode creation explicitly.
     */
    virtual std::unique_ptr<IGameMode> CreateInitialGameMode(GameplayHost& Host)
    {
        (void)Host;
        return {};
    }

    /**
     * @brief Optional initial possession-target resolver for a newly joined player.
     * @param Host Borrowed gameplay host.
     * @param Player Borrowed player node being initialized.
     * @return Handle to the desired possession target, or a null handle to defer to later resolvers.
     * @remarks
     * Returning a non-null handle does not guarantee it will be used; the host validates
     * that the target belongs to the same world and is otherwise acceptable.
     */
    virtual NodeHandle SelectInitialPossessionTarget(GameplayHost& Host, LocalPlayer& Player)
    {
        (void)Host;
        (void)Player;
        return {};
    }

    /**
     * @brief Optional player-start resolver for a newly joined player.
     * @param Host Borrowed gameplay host.
     * @param Player Borrowed player node being initialized.
     * @return Handle to a `PlayerStart`, or a null handle to defer to later resolvers.
     */
    virtual NodeHandle SelectPlayerStart(GameplayHost& Host, LocalPlayer& Player)
    {
        (void)Host;
        (void)Player;
        return {};
    }

    /**
     * @brief Optional spawned-pawn type override for a newly joined player.
     * @param Host Borrowed gameplay host.
     * @param Player Borrowed player node being initialized.
     * @param PlayerStart Borrowed handle to the chosen player start, which may be null.
     * @return Concrete pawn type id to spawn, or `std::nullopt` to keep host/default behavior.
     * @remarks The returned type must derive from `PawnBase` or it will be ignored by the host.
     */
    virtual std::optional<TypeId> SelectSpawnedPawnType(GameplayHost& Host,
                                                        LocalPlayer& Player,
                                                        const NodeHandle& PlayerStart)
    {
        (void)Host;
        (void)Player;
        (void)PlayerStart;
        return std::nullopt;
    }

    /**
     * @brief Optional replication-policy override for the pawn spawned for a newly joined player.
     * @param Host Borrowed gameplay host.
     * @param Player Borrowed player node being initialized.
     * @param PlayerStart Borrowed handle to the chosen player start, which may be null.
     * @return Replication preference or `std::nullopt` to keep host/default behavior.
     */
    virtual std::optional<bool> SelectSpawnedPawnReplicated(GameplayHost& Host,
                                                            LocalPlayer& Player,
                                                            const NodeHandle& PlayerStart)
    {
        (void)Host;
        (void)Player;
        (void)PlayerStart;
        return std::nullopt;
    }

    /**
     * @brief Policy hook for connection-authored join requests.
     * @param Host Borrowed gameplay host.
     * @param OwnerConnectionId Requesting connection id. `0` represents local/standalone authority.
     * @param RequestedName Requested player name, possibly empty.
     * @param PreferredPlayerIndex Requested player index, if any.
     * @param ReplicatedPlayer Requested replication state for the created player node.
     * @return `true` to allow the request, `false` to deny it before host mutation occurs.
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
     * @brief Shutdown game state and release host-dependent resources.
     * @param Host Borrowed gameplay host.
     * @remarks Called before the instance is discarded or replaced.
     */
    virtual void Shutdown(GameplayHost& Host) = 0;
};

} // namespace SnAPI::GameFramework
