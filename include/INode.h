#pragma once

#include <string>
#include <vector>

#include "Handles.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class NodeGraph;

/**
 * @brief Abstract node interface for the scene graph.
 * @remarks BaseNode provides the default implementation.
 * @note Implementing INode directly is supported but requires manual management.
 */
class INode
{
public:
    /**
     * @brief Virtual destructor for interface.
     */
    virtual ~INode() = default;

    /**
     * @brief Per-frame update hook.
     * @param DeltaSeconds Time since last tick.
     * @remarks Called only when the node is considered active.
     */
    virtual void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Fixed-step update hook.
     * @param DeltaSeconds Fixed time step.
     * @remarks Used for deterministic updates (e.g., physics).
     */
    virtual void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Late update hook.
     * @param DeltaSeconds Time since last tick.
     * @remarks Invoked after Tick for post-processing.
     */
    virtual void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }

    /**
     * @brief Get the display name of the node.
     * @return Node name.
     */
    virtual const std::string& Name() const = 0;
    /**
     * @brief Set the display name of the node.
     * @param Name New name.
     */
    virtual void Name(std::string Name) = 0;

    /**
     * @brief Get the handle for this node.
     * @return Node handle.
     * @remarks Handles are UUID-based and resolve via ObjectRegistry.
     */
    virtual NodeHandle Handle() const = 0;
    /**
     * @brief Set the node handle.
     * @param Handle New handle.
     */
    virtual void Handle(NodeHandle Handle) = 0;

    /**
     * @brief Get the node UUID.
     * @return UUID value.
     */
    virtual const Uuid& Id() const = 0;
    /**
     * @brief Set the node UUID.
     * @param Id UUID value.
     */
    virtual void Id(Uuid Id) = 0;

    /**
     * @brief Get the reflected type id for this node.
     * @return TypeId value.
     */
    virtual const TypeId& TypeKey() const = 0;
    /**
     * @brief Set the reflected type id for this node.
     * @param Id TypeId value.
     */
    virtual void TypeKey(const TypeId& Id) = 0;

    /**
     * @brief Get the parent node handle.
     * @return Parent handle or null handle if root.
     */
    virtual NodeHandle Parent() const = 0;
    /**
     * @brief Set the parent node handle.
     * @param Parent Parent handle.
     */
    virtual void Parent(NodeHandle Parent) = 0;

    /**
     * @brief Get the list of child handles.
     * @return Vector of child handles.
     */
    virtual const std::vector<NodeHandle>& Children() const = 0;
    /**
     * @brief Add a child handle to the node.
     * @param Child Child handle.
     */
    virtual void AddChild(NodeHandle Child) = 0;
    /**
     * @brief Remove a child handle from the node.
     * @param Child Child handle to remove.
     */
    virtual void RemoveChild(NodeHandle Child) = 0;

    /**
     * @brief Check if the node is active.
     * @return True if active.
     */
    virtual bool Active() const = 0;
    /**
     * @brief Set the active state for the node.
     * @param Active New active state.
     */
    virtual void Active(bool Active) = 0;

    /**
     * @brief Check if the node is replicated over the network.
     * @return True if replicated.
     */
    virtual bool Replicated() const = 0;
    /**
     * @brief Set whether the node is replicated over the network.
     * @param Replicated New replicated state.
     */
    virtual void Replicated(bool Replicated) = 0;

    /**
     * @brief Access the list of component type ids.
     * @return Mutable reference to the type id list.
     */
    virtual std::vector<TypeId>& ComponentTypes() = 0;
    /**
     * @brief Access the list of component type ids (const).
     * @return Const reference to the type id list.
     */
    virtual const std::vector<TypeId>& ComponentTypes() const = 0;

    /**
     * @brief Access the component bitmask storage.
     * @return Mutable reference to the component mask.
     */
    virtual std::vector<uint64_t>& ComponentMask() = 0;
    /**
     * @brief Access the component bitmask storage (const).
     * @return Const reference to the component mask.
     */
    virtual const std::vector<uint64_t>& ComponentMask() const = 0;

    /**
     * @brief Get the component mask version.
     * @return Version id.
     */
    virtual uint32_t MaskVersion() const = 0;
    /**
     * @brief Set the component mask version.
     * @param Version New version id.
     */
    virtual void MaskVersion(uint32_t Version) = 0;

    /**
     * @brief Get the owning graph.
     * @return Pointer to owner graph or nullptr if unowned.
     */
    virtual NodeGraph* OwnerGraph() const = 0;
    /**
     * @brief Set the owning graph.
     * @param Graph Owner graph pointer.
     */
    virtual void OwnerGraph(NodeGraph* Graph) = 0;

    /**
     * @brief Tick this node and its subtree.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void TickTree(float DeltaSeconds) = 0;
    /**
     * @brief Fixed-step tick for this node and its subtree.
     * @param DeltaSeconds Fixed time step.
     */
    virtual void FixedTickTree(float DeltaSeconds) = 0;
    /**
     * @brief Late tick for this node and its subtree.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void LateTickTree(float DeltaSeconds) = 0;
};

} // namespace SnAPI::GameFramework
