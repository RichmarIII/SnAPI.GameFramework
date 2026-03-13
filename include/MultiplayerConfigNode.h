#pragma once

#include <string>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-only configuration node for local-multiplayer startup policy.
 *
 * `MultiplayerConfigNode` is a lightweight settings container that can be placed in content
 * or worlds to describe how many local players should exist and whether splitscreen-style
 * behavior should be enabled. It does not perform any orchestration itself; higher-level
 * systems are expected to interpret its fields.
 *
 * @see GameplayHost
 * @see LocalPlayer
 */
class SNAPI_GAMEFRAMEWORK_API MultiplayerConfigNode : public BaseNode, public NodeCRTP<MultiplayerConfigNode>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::MultiplayerConfigNode";

    MultiplayerConfigNode();
    explicit MultiplayerConfigNode(std::string Name);

    /** @brief Access the desired local-player count. @return Mutable player-count field. */
    int& EditLocalPlayerCount();
    /** @brief Access the desired local-player count. @return Const player-count field. */
    const int& GetLocalPlayerCount() const;

    /** @brief Access the splitscreen enable flag. @return Mutable splitscreen field. */
    bool& EditSplitscreen();
    /** @brief Access the splitscreen enable flag. @return Const splitscreen field. */
    const bool& GetSplitscreen() const;

    /** @brief Access the auto-join policy for additional local players. @return Mutable auto-join field. */
    bool& EditAutoJoinAdditionalLocalPlayers();
    /** @brief Access the auto-join policy for additional local players. @return Const auto-join field. */
    const bool& GetAutoJoinAdditionalLocalPlayers() const;

    /** @brief Access the policy requiring a gamepad for additional local players. @return Mutable requirement field. */
    bool& EditRequireGamepadForAdditionalPlayers();
    /** @brief Access the policy requiring a gamepad for additional local players. @return Const requirement field. */
    const bool& GetRequireGamepadForAdditionalPlayers() const;

private:
    int m_localPlayerCount = 1;
    bool m_splitscreen = true;
    bool m_autoJoinAdditionalLocalPlayers = true;
    bool m_requireGamepadForAdditionalPlayers = false;
};

} // namespace SnAPI::GameFramework
