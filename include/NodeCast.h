#pragma once

#include "BaseNode.h"
#include "StaticTypeId.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework
{

template<typename TNode>
TNode* NodeCast(BaseNode* Node)
{
    if (!Node)
    {
        return nullptr;
    }

    if (!TypeRegistry::Instance().IsA(Node->TypeKey(), StaticTypeId<TNode>()))
    {
        return nullptr;
    }

    return static_cast<TNode*>(Node);
}

template<typename TNode>
const TNode* NodeCast(const BaseNode* Node)
{
    if (!Node)
    {
        return nullptr;
    }

    if (!TypeRegistry::Instance().IsA(Node->TypeKey(), StaticTypeId<TNode>()))
    {
        return nullptr;
    }

    return static_cast<const TNode*>(Node);
}

} // namespace SnAPI::GameFramework
