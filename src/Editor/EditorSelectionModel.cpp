#include "Editor/EditorSelectionModel.h"

#include "NodeGraph.h"
#include "StaticTypeId.h"
#include "TypeRegistry.h"
#include "World.h"

#include <unordered_set>

namespace SnAPI::GameFramework::Editor
{
namespace
{

BaseNode* ResolveNodeInGraphById(NodeGraph& Graph,
                                 const Uuid& TargetId,
                                 std::unordered_set<const NodeGraph*>& VisitedGraphs)
{
    if (TargetId.is_nil())
    {
        return nullptr;
    }

    if (!VisitedGraphs.insert(&Graph).second)
    {
        return nullptr;
    }

    const auto HandleResult = Graph.NodeHandleByIdSlow(TargetId);
    if (HandleResult)
    {
        if (auto* Direct = Graph.NodePool().Borrowed(*HandleResult))
        {
            return Direct;
        }
    }

    BaseNode* Resolved = nullptr;
    Graph.NodePool().ForEach([&](const NodeHandle&, BaseNode& Node) {
        if (Resolved != nullptr)
        {
            return;
        }

        NodeGraph* Nested = dynamic_cast<NodeGraph*>(&Node);
        if (!Nested && TypeRegistry::Instance().IsA(Node.TypeKey(), StaticTypeId<NodeGraph>()))
        {
            Nested = static_cast<NodeGraph*>(&Node);
        }
        if (Nested)
        {
            Resolved = ResolveNodeInGraphById(*Nested, TargetId, VisitedGraphs);
        }
    });

    return Resolved;
}

const BaseNode* ResolveNodeInGraphById(const NodeGraph& Graph,
                                       const Uuid& TargetId,
                                       std::unordered_set<const NodeGraph*>& VisitedGraphs)
{
    if (TargetId.is_nil())
    {
        return nullptr;
    }

    if (!VisitedGraphs.insert(&Graph).second)
    {
        return nullptr;
    }

    const auto HandleResult = Graph.NodeHandleByIdSlow(TargetId);
    if (HandleResult)
    {
        if (const auto* Direct = Graph.NodePool().Borrowed(*HandleResult))
        {
            return Direct;
        }
    }

    const BaseNode* Resolved = nullptr;
    Graph.NodePool().ForEach([&](const NodeHandle&, BaseNode& Node) {
        if (Resolved != nullptr)
        {
            return;
        }

        const NodeGraph* Nested = dynamic_cast<const NodeGraph*>(&Node);
        if (!Nested && TypeRegistry::Instance().IsA(Node.TypeKey(), StaticTypeId<NodeGraph>()))
        {
            Nested = static_cast<const NodeGraph*>(&Node);
        }
        if (Nested)
        {
            Resolved = ResolveNodeInGraphById(*Nested, TargetId, VisitedGraphs);
        }
    });

    return Resolved;
}

} // namespace

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

    if (auto* Node = m_selectedNode.Borrowed())
    {
        return Node;
    }

    std::unordered_set<const NodeGraph*> VisitedGraphs{};
    if (auto* Node = ResolveNodeInGraphById(WorldRef, m_selectedNode.Id, VisitedGraphs))
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

    if (auto* Node = m_selectedNode.Borrowed())
    {
        return Node;
    }

    std::unordered_set<const NodeGraph*> VisitedGraphs{};
    if (const auto* Node = ResolveNodeInGraphById(WorldRef, m_selectedNode.Id, VisitedGraphs))
    {
        return Node;
    }

    return m_selectedNode.BorrowedSlowByUuid();
}

} // namespace SnAPI::GameFramework::Editor
