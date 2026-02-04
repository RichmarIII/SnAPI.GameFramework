#pragma once

#include <string>

#include "Handle.h"
#include "Handles.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class NodeGraph;
class BaseNode;

class IComponent
{
public:
    virtual ~IComponent() = default;

    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    virtual void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    virtual void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }

    void Owner(NodeHandle InOwner)
    {
        m_owner = InOwner;
    }

    NodeHandle Owner() const
    {
        return m_owner;
    }

    const Uuid& Id() const
    {
        return m_id;
    }

    void Id(Uuid Id)
    {
        m_id = Id;
    }

    ComponentHandle Handle() const
    {
        return ComponentHandle(m_id);
    }

private:
    NodeHandle m_owner{};
    Uuid m_id{};
};

} // namespace SnAPI::GameFramework
