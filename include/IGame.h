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
 * @brief Session-wide gameplay root (GameInstance-style).
 * @remarks
 * `IGame` may exist on both server and clients and coordinates session-level
 * startup/shutdown flow. Server-only authoritative rules belong in `IGameMode`.
 */
class SNAPI_GAMEFRAMEWORK_API IGame
{
public:
    virtual ~IGame() = default;

    /**
     * @brief Stable game name for diagnostics.
     */
    [[nodiscard]] virtual std::string_view Name() const = 0;

    /**
     * @brief Initialize game state.
     */
    virtual Result Initialize(GameplayHost& Host) = 0;

    /**
     * @brief Per-frame game update.
     */
    virtual void Tick(GameplayHost& Host, float DeltaSeconds)
    {
        (void)Host;
        (void)DeltaSeconds;
    }

    /**
     * @brief Optional server-only initial game mode factory hook.
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
     * @brief Optional player-start resolver for newly joined players.
     * @remarks Return null handle to defer to host fallback selection.
     */
    virtual NodeHandle SelectPlayerStart(GameplayHost& Host, LocalPlayer& Player)
    {
        (void)Host;
        (void)Player;
        return {};
    }

    /**
     * @brief Optional spawned-pawn class override for newly joined players.
     * @remarks Return nullopt to keep host/default player-start class.
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
     * @brief Optional spawned-pawn replication override for newly joined players.
     * @remarks Return nullopt to keep host default (`true`).
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
    virtual void OnLevelLoaded(GameplayHost& Host, const NodeHandle& LevelHandle)
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
    virtual void OnLocalPlayerAdded(GameplayHost& Host, const NodeHandle& PlayerHandle)
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
     * @brief Shutdown game state.
     */
    virtual void Shutdown(GameplayHost& Host) = 0;
};

} // namespace SnAPI::GameFramework
