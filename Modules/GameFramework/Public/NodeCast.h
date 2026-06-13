#pragma once

#include "BaseNode.h"
#include "StaticTypeId.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Downcast a node pointer using the framework reflection hierarchy instead of C++ RTTI.
 *
 * `NodeCast` is the node equivalent of an engine-style `Cast<>` helper:
 * - `nullptr` stays `nullptr`.
 * - Unrelated types return `nullptr`.
 * - Successful casts rely on the registered `TypeRegistry` inheritance graph,
 *   not on `dynamic_cast`.
 *
 * This matters because most gameplay/editor code reasons about reflected type ids
 * (`TypeId`) rather than relying on compiler RTTI. The helper therefore matches the
 * same inheritance semantics used by serialization, property inspection, and factory
 * code.
 *
 * Lifetime and ownership:
 * - Returns a non-owning pointer to the same object passed in.
 * - The result is valid only while the original node remains alive.
 *
 * Threading:
 * - Safe under the same conditions as read-only `TypeRegistry` access.
 *
 * @tparam TNode Expected concrete or base node type.
 * @param Node Borrowed pointer to test and cast.
 * @return `Node` reinterpreted as `TNode*` when the reflected type is-a `TNode`,
 *         otherwise `nullptr`.
 *
 * @see TypeRegistry::IsA()
 * @see StaticTypeId()
 */
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

/**
 * @ingroup SnAPI_GameFramework
 * @brief Const overload of `NodeCast`.
 *
 * Semantics are identical to the mutable overload, but constness of the source
 * pointer is preserved.
 *
 * @tparam TNode Expected concrete or base node type.
 * @param Node Borrowed node pointer to test and cast.
 * @return A non-owning const pointer on success, otherwise `nullptr`.
 */
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
