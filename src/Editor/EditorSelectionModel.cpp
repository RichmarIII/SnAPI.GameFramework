#include "Editor/EditorSelectionModel.h"

#include "World.h"

namespace SnAPI::GameFramework::Editor
{

bool EditorSelectionModel::SelectNode(const NodeHandle Node)
{
    if (m_selectedNode == Node)
    {
        return false;
    }

    m_selectedNode = Node;
    return true;
}

void EditorSelectionModel::Clear()
{
    m_selectedNode = {};
}

BaseNode* EditorSelectionModel::ResolveSelectedNode(World& WorldRef) const
{
    if (m_selectedNode.IsNull())
    {
        return nullptr;
    }

    return WorldRef.NodePool().Borrowed(m_selectedNode);
}

const BaseNode* EditorSelectionModel::ResolveSelectedNode(const World& WorldRef) const
{
    if (m_selectedNode.IsNull())
    {
        return nullptr;
    }

    return WorldRef.NodePool().Borrowed(m_selectedNode);
}

} // namespace SnAPI::GameFramework::Editor

