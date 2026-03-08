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
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Lightweight node-selection state shared across editor views.
 *
 * `EditorSelectionModel` stores one selected `NodeHandle` and resolves it lazily against a world.
 * It exists so inspectors, hierarchy panels, and viewport tools can share the same logical
 * selection without requiring a hard reference to a live `BaseNode`.
 *
 * Core semantics:
 * - Selection is stored as a handle, not a raw pointer.
 * - `ResolveSelectedNode()` first tries the embedded handle, then world lookup by object id,
 *   and finally a slower UUID-based fallback.
 * - The model does not own the selected node and does not keep it alive.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see NodeHandle
 * @see BaseNode
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorSelectionModel final
{
public:
    /**
     * @brief Access the currently stored selection handle.
     * @return Copy of the stored handle. May be null.
     */
    [[nodiscard]] NodeHandle SelectedNode() const { return m_selectedNode; }
    /**
     * @brief Query whether any selection is currently stored.
     * @return `true` when `SelectedNode()` is non-null.
     */
    [[nodiscard]] bool HasSelection() const { return !m_selectedNode.IsNull(); }

    /**
     * @brief Replace the stored selection handle.
     * @param Node New selection handle. May be null to represent "no selection".
     * @return `true` when the stored selection changed, `false` when the same handle was already selected.
     */
    bool SelectNode(const NodeHandle& Node);
    /**
     * @brief Clear the stored selection.
     */
    void Clear();

    /**
     * @brief Resolve the selected node against a mutable world.
     * @param WorldRef World used for id-based fallback resolution.
     * @return Non-owning pointer to the selected node, or `nullptr` when the selection no longer resolves.
     * @remarks
     * Resolution order is:
     * 1. direct `NodeHandle::Borrowed()`
     * 2. `World::NodeHandleById()` lookup using the stored object id
     * 3. `NodeHandle::BorrowedSlowByUuid()`
     */
    [[nodiscard]] BaseNode* ResolveSelectedNode(World& WorldRef) const;
    /**
     * @brief Resolve the selected node against a const world.
     * @param WorldRef World used for id-based fallback resolution.
     * @return Non-owning pointer to the selected node, or `nullptr` when the selection no longer resolves.
     */
    [[nodiscard]] const BaseNode* ResolveSelectedNode(const World& WorldRef) const;

private:
    NodeHandle m_selectedNode{};
};

} // namespace SnAPI::GameFramework::Editor
