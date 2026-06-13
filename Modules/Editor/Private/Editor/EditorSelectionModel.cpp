#include "Editor/EditorSelectionModel.h"

#include "Level.h"
#include "NodeCast.h"
#include "StaticTypeId.h"
#include "TypeRegistry.h"
#include "World.h"

#include <unordered_set>

namespace SnAPI::GameFramework::Editor
{
namespace
{

BaseNode* ResolveNodeInWorldById(World& WorldRef, const Uuid& TargetId)
{
    if (TargetId.is_nil())
    {
        return nullptr;
    }

    const auto HandleResult = WorldRef.NodeHandleById(TargetId);
    if (HandleResult)
    {
        NodeHandle ResolvedHandle = *HandleResult;
        if (auto* Direct = WorldRef.BorrowedNode(ResolvedHandle))
        {
            return Direct;
        }
    }
    return nullptr;
}

const BaseNode* ResolveNodeInWorldById(const World& WorldRef, const Uuid& TargetId)
{
    if (TargetId.is_nil())
    {
        return nullptr;
    }

    const auto HandleResult = WorldRef.NodeHandleById(TargetId);
    if (HandleResult)
    {
        NodeHandle ResolvedHandle = *HandleResult;
        if (const auto* Direct = WorldRef.BorrowedNode(ResolvedHandle))
        {
            return Direct;
        }
    }
    return nullptr;
}

} // namespace

bool EditorSelectionModel::SelectNode(const NodeHandle& Node)
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

BaseNode* EditorSelectionModel::ResolveSelectedNode(World& WorldRef)
{
    if (m_selectedNode.IsNull())
    {
        return nullptr;
    }

    if (auto* Node = WorldRef.BorrowedNode(m_selectedNode))
    {
        return Node;
    }

    if (auto* Node = ResolveNodeInWorldById(WorldRef, m_selectedNode.Id))
    {
        return Node;
    }

    return m_selectedNode.BorrowedSlowByUuid();
}

const BaseNode* EditorSelectionModel::ResolveSelectedNode(const World& WorldRef) const
{
    if (m_selectedNode.IsNull())
    {
        return nullptr;
    }

    NodeHandle SelectedNode = m_selectedNode;
    if (const auto* Node = WorldRef.BorrowedNode(SelectedNode))
    {
        return Node;
    }

    if (const auto* Node = ResolveNodeInWorldById(WorldRef, SelectedNode.Id))
    {
        return Node;
    }

    return SelectedNode.BorrowedSlowByUuid();
}

} // namespace SnAPI::GameFramework::Editor
