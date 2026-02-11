#pragma once

#include <string>

#include "Handle.h"
#include "Handles.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class NodeGraph;
class BaseNode;

/**
 * @brief Base interface for components attached to nodes.
 * @remarks Components are owned by a NodeGraph and referenced by ComponentHandle.
 * @note Components are destroyed at end-of-frame to keep handles stable.
 */
class IComponent
{
public:
    /**
     * @brief Virtual destructor for interface.
     */
    virtual ~IComponent() = default;

    /**
     * @brief Called immediately after component creation.
     * @remarks Override to initialize component state.
     */
    virtual void OnCreate() {}
    /**
     * @brief Called just before component destruction.
     * @remarks Override to release resources.
     */
    virtual void OnDestroy() {}
    /**
     * @brief Per-frame update hook.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Fixed-step update hook.
     * @param DeltaSeconds Fixed time step.
     */
    virtual void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /**
     * @brief Late update hook.
     * @param DeltaSeconds Time since last tick.
     */
    virtual void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }

    /**
     * @brief Set the owning node handle.
     * @param InOwner Owner node handle.
     * @remarks Set by the owning NodeGraph.
     */
    void Owner(NodeHandle InOwner)
    {
        m_owner = InOwner;
    }

    /**
     * @brief Get the owning node handle.
     * @return Owner node handle.
     */
    NodeHandle Owner() const
    {
        return m_owner;
    }

    /**
     * @brief Check if the component is replicated over the network.
     * @return True if replicated.
     */
    bool Replicated() const
    {
        return m_replicated;
    }

    /**
     * @brief Set whether the component is replicated over the network.
     * @param Replicated New replicated state.
     */
    void Replicated(bool Replicated)
    {
        m_replicated = Replicated;
    }

    /**
     * @brief Get the component UUID.
     * @return UUID of this component.
     */
    const Uuid& Id() const
    {
        return m_id;
    }

    /**
     * @brief Set the component UUID.
     * @param Id New UUID value.
     * @remarks Set by the owning NodeGraph/component storage.
     */
    void Id(Uuid Id)
    {
        m_id = Id;
    }

    /**
     * @brief Get a handle for this component.
     * @return ComponentHandle wrapping the UUID.
     */
    ComponentHandle Handle() const
    {
        return ComponentHandle(m_id);
    }

private:
    NodeHandle m_owner{}; /**< @brief Owning node handle. */
    Uuid m_id{}; /**< @brief Component UUID. */
    bool m_replicated = false; /**< @brief Replication flag. */
};

} // namespace SnAPI::GameFramework
