#include "MultiplayerConfigNode.h"

namespace SnAPI::GameFramework
{

MultiplayerConfigNode::MultiplayerConfigNode()
{
    TypeKey(StaticTypeId<MultiplayerConfigNode>());
}

MultiplayerConfigNode::MultiplayerConfigNode(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<MultiplayerConfigNode>());
}

int& MultiplayerConfigNode::EditLocalPlayerCount()
{
    return m_localPlayerCount;
}

const int& MultiplayerConfigNode::GetLocalPlayerCount() const
{
    return m_localPlayerCount;
}

bool& MultiplayerConfigNode::EditSplitscreen()
{
    return m_splitscreen;
}

const bool& MultiplayerConfigNode::GetSplitscreen() const
{
    return m_splitscreen;
}

bool& MultiplayerConfigNode::EditAutoJoinAdditionalLocalPlayers()
{
    return m_autoJoinAdditionalLocalPlayers;
}

const bool& MultiplayerConfigNode::GetAutoJoinAdditionalLocalPlayers() const
{
    return m_autoJoinAdditionalLocalPlayers;
}

bool& MultiplayerConfigNode::EditRequireGamepadForAdditionalPlayers()
{
    return m_requireGamepadForAdditionalPlayers;
}

const bool& MultiplayerConfigNode::GetRequireGamepadForAdditionalPlayers() const
{
    return m_requireGamepadForAdditionalPlayers;
}

} // namespace SnAPI::GameFramework
