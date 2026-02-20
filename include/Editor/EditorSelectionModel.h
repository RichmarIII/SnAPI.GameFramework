#pragma once

#include "Editor/EditorExport.h"

#include "Handles.h"
#include "BaseNode.h"

namespace SnAPI::GameFramework
{
class BaseNode;
class World;
} // namespace SnAPI::GameFramework

namespace SnAPI::GameFramework::Editor
{

/**
 * @brief Lightweight editor selection state shared across editor views.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorSelectionModel final
{
public:
    [[nodiscard]] NodeHandle SelectedNode() const { return m_selectedNode; }
    [[nodiscard]] bool HasSelection() const { return !m_selectedNode.IsNull(); }

    bool SelectNode(NodeHandle Node);
    void Clear();

    [[nodiscard]] BaseNode* ResolveSelectedNode(World& WorldRef) const;
    [[nodiscard]] const BaseNode* ResolveSelectedNode(const World& WorldRef) const;

private:
    NodeHandle m_selectedNode{};
};

} // namespace SnAPI::GameFramework::Editor

