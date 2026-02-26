#pragma once

#include <string>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Data-only node asset used to configure local multiplayer startup behavior.
 */
class SNAPI_GAMEFRAMEWORK_API MultiplayerConfigNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::MultiplayerConfigNode";

    MultiplayerConfigNode();
    explicit MultiplayerConfigNode(std::string Name);

    int& EditLocalPlayerCount();
    const int& GetLocalPlayerCount() const;

    bool& EditSplitscreen();
    const bool& GetSplitscreen() const;

    bool& EditAutoJoinAdditionalLocalPlayers();
    const bool& GetAutoJoinAdditionalLocalPlayers() const;

    bool& EditRequireGamepadForAdditionalPlayers();
    const bool& GetRequireGamepadForAdditionalPlayers() const;

private:
    int m_localPlayerCount = 1;
    bool m_splitscreen = true;
    bool m_autoJoinAdditionalLocalPlayers = true;
    bool m_requireGamepadForAdditionalPlayers = false;
};

} // namespace SnAPI::GameFramework
