#pragma once

#include "Assert.h"
#include "Expected.h"

namespace SnAPI::GameFramework
{

template<typename T, typename... Args>
TExpectedRef<T> BaseNode::Add(Args&&... args)
{
    DEBUG_ASSERT(m_ownerGraph, "Node has no owner graph");
    if (!m_ownerGraph)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Node is not owned by a graph"));
    }
    return m_ownerGraph->AddComponent<T>(m_self, std::forward<Args>(args)...);
}

template<typename T>
TExpectedRef<T> BaseNode::Component()
{
    DEBUG_ASSERT(m_ownerGraph, "Node has no owner graph");
    if (!m_ownerGraph)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Node is not owned by a graph"));
    }
    return m_ownerGraph->Component<T>(m_self);
}

template<typename T>
bool BaseNode::Has() const
{
    if (!m_ownerGraph)
    {
        return false;
    }
    return m_ownerGraph->HasComponent<T>(m_self);
}

template<typename T>
void BaseNode::Remove()
{
    if (!m_ownerGraph)
    {
        return;
    }
    m_ownerGraph->RemoveComponent<T>(m_self);
}

} // namespace SnAPI::GameFramework
