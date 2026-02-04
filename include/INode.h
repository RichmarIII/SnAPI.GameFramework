#pragma once

#include <string>
#include <vector>

#include "Handles.h"

namespace SnAPI::GameFramework
{

class BaseNode;

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
};

} // namespace SnAPI::GameFramework
