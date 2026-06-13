#pragma once

/**
 * @file BaseNode.inl
 * @ingroup SnAPI_GameFramework
 * @brief Inline/template definitions for the runtime-component bridge on `BaseNode`.
 *
 * These helpers implement the modern ECS-runtime-backed node/component API:
 * - typed component add/query/remove convenience on `BaseNode`
 * - synchronization between dense runtime component storage and legacy node-side caches
 * - object-registry registration for runtime-owned components
 */

#include <type_traits>

#include "Assert.h"
#include "ComponentTypeRegistry.h"
#include "Expected.h"
#include "BaseComponent.h"
#include "IWorld.h"
#include "ObjectRegistry.h"
#include "Relevance.h"

namespace SnAPI::GameFramework
{
namespace
{
/**
 * @brief Mirror a newly attached runtime component into the node's legacy component caches.
 * @param Node Owning node to update.
 * @param Type Reflected component type.
 * @param ComponentPtr Borrowed component pointer for special-case cache updates.
 *
 * This updates the node-side component bit mask, reflected type list, and relevance
 * pointer when the component is a `RelevanceComponent`.
 */
inline void RegisterRuntimeComponentOnNode(BaseNode& Node, const TypeId& Type, void* ComponentPtr)
{
    const uint32_t TypeIndex = ComponentTypeRegistry::TypeIndex(Type);
    const uint32_t Version = ComponentTypeRegistry::Version();
    if (Node.MaskVersion() != Version)
    {
        Node.ComponentMask().resize(ComponentTypeRegistry::WordCount(), 0u);
        Node.MaskVersion(Version);
    }

    const std::size_t Word = TypeIndex / 64u;
    const std::size_t Bit = TypeIndex % 64u;
    if (Word >= Node.ComponentMask().size())
    {
        Node.ComponentMask().resize(Word + 1u, 0u);
    }
    Node.ComponentMask()[Word] |= (1ull << Bit);

    auto& Types = Node.ComponentTypes();
    for (std::size_t Index = 0; Index < Types.size(); ++Index)
    {
        if (Types[Index] == Type)
        {
            static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
            if (Type == RelevanceType)
            {
                Node.RelevanceState(static_cast<RelevanceComponent*>(ComponentPtr));
            }
            return;
        }
    }

    Types.push_back(Type);

    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    if (Type == RelevanceType)
    {
        Node.RelevanceState(static_cast<RelevanceComponent*>(ComponentPtr));
    }
}

/**
 * @brief Remove a runtime component from the node's legacy component caches.
 * @param Node Owning node to update.
 * @param Type Reflected component type being detached.
 *
 * This clears the component mask bit, removes the reflected type entry, and resets
 * the relevance cache when applicable.
 */
inline void UnregisterRuntimeComponentOnNode(BaseNode& Node, const TypeId& Type)
{
    const uint32_t TypeIndex = ComponentTypeRegistry::TypeIndex(Type);
    const std::size_t Word = TypeIndex / 64u;
    const std::size_t Bit = TypeIndex % 64u;
    if (Word < Node.ComponentMask().size())
    {
        Node.ComponentMask()[Word] &= ~(1ull << Bit);
    }

    auto& Types = Node.ComponentTypes();
    for (std::size_t Index = 0; Index < Types.size(); ++Index)
    {
        if (Types[Index] != Type)
        {
            continue;
        }

        auto TypeIt = Types.begin() + static_cast<std::vector<TypeId>::difference_type>(Index);
        Types.erase(TypeIt);
        break;
    }

    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    if (Type == RelevanceType)
    {
        Node.RelevanceState(nullptr);
    }
}

/**
 * @brief Initialize legacy `BaseComponent` state for a newly created dense runtime component.
 * @tparam TComponent Concrete runtime component type.
 * @param Node Owning node.
 * @param Handle Dense runtime-component handle.
 * @param Component Newly created component instance.
 *
 * When `TComponent` derives from `BaseComponent`, this bridges the dense runtime record
 * back into the legacy object-facing fields and registers the component in
 * `ObjectRegistry`.
 */
template<RuntimeTickType TComponent>
void InitializeRuntimeComponentState(BaseNode& Node,
                                     const TDenseRuntimeHandle<TComponent>& Handle,
                                     TComponent& Component)
{
    if constexpr (std::is_base_of_v<BaseComponent, TComponent>)
    {
        Component.Owner(Node.Handle());
        Component.TypeKey(StaticTypeId<TComponent>());
        Component.Id(Handle.Id);
        Component.RuntimeIdentity(Handle.StorageToken, Handle.Index, Handle.Generation);
        ObjectRegistry::Instance().RegisterComponent(
            Handle.Id,
            &Component,
            Handle.StorageToken,
            Handle.Index,
            Handle.Generation);
    }

    RegisterRuntimeComponentOnNode(Node, StaticTypeId<TComponent>(), &Component);
}
} // namespace

template<typename T, typename... Args>
TExpectedRef<T> BaseNode::Add(Args&&... args)
{
    static_assert(RuntimeTickType<T> && std::is_base_of_v<BaseComponent, T>,
                  "BaseNode::Add<T> requires ECS runtime-compatible BaseComponent types");
    static_assert(std::is_move_constructible_v<T>,
                  "BaseNode::Add<T> requires move-constructible runtime component types");

    if (!m_world)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Node is not bound to a world"));
    }

    auto RuntimeAddResult = AddRuntimeComponent<T>(std::forward<Args>(args)...);
    if (!RuntimeAddResult)
    {
        return std::unexpected(RuntimeAddResult.error());
    }

    auto RuntimeComponentResult = RuntimeComponent<T>();
    if (!RuntimeComponentResult)
    {
        return std::unexpected(RuntimeComponentResult.error());
    }
    return *RuntimeComponentResult;
}

template<typename T>
TExpectedRef<T> BaseNode::Component()
{
    static_assert(RuntimeTickType<T> && std::is_base_of_v<BaseComponent, T>,
                  "BaseNode::Component<T> requires ECS runtime-compatible BaseComponent types");

    if (!m_world)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Node is not bound to a world"));
    }

    auto RuntimeComponentResult = RuntimeComponent<T>();
    if (!RuntimeComponentResult)
    {
        return std::unexpected(RuntimeComponentResult.error());
    }
    return *RuntimeComponentResult;
}

template<typename T>
bool BaseNode::Has() const
{
    static_assert(RuntimeTickType<T> && std::is_base_of_v<BaseComponent, T>,
                  "BaseNode::Has<T> requires ECS runtime-compatible BaseComponent types");

    if (!m_world)
    {
        return false;
    }

    return HasComponentBit(StaticTypeId<T>());
}

template<typename T>
void BaseNode::Remove()
{
    static_assert(RuntimeTickType<T> && std::is_base_of_v<BaseComponent, T>,
                  "BaseNode::Remove<T> requires ECS runtime-compatible BaseComponent types");

    if (!m_world)
    {
        return;
    }

    (void)RemoveRuntimeComponent<T>();
}

template<RuntimeTickType T, typename... Args>
TExpected<TDenseRuntimeHandle<T>> BaseNode::AddRuntimeComponent(Args&&... args)
{
    static_assert(std::is_move_constructible_v<T>,
                  "Runtime dense component storage requires move-constructible component types");

    if (!m_world)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Node is not bound to a world"));
    }

    NodeHandle OwnerHandle = Handle();
    if (OwnerHandle.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node handle was not found"));
    }

    auto AddResult = m_world->EcsRuntime().AddComponent<T>(*m_world, OwnerHandle, std::forward<Args>(args)...);
    if (!AddResult)
    {
        return std::unexpected(AddResult.error());
    }

    T* ComponentPtr = m_world->EcsRuntime().Component<T>(OwnerHandle);
    if (!ComponentPtr)
    {
        (void)m_world->EcsRuntime().RemoveComponent<T>(*m_world, OwnerHandle);
        return std::unexpected(MakeError(EErrorCode::InternalError, "Runtime component creation returned null"));
    }

    InitializeRuntimeComponentState(*this, *AddResult, *ComponentPtr);
    return *AddResult;
}

template<RuntimeTickType T, typename... Args>
TExpected<TDenseRuntimeHandle<T>> BaseNode::AddRuntimeComponentWithId(const Uuid& Id, Args&&... args)
{
    static_assert(std::is_move_constructible_v<T>,
                  "Runtime dense component storage requires move-constructible component types");

    if (!m_world)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Node is not bound to a world"));
    }

    NodeHandle OwnerHandle = Handle();
    if (OwnerHandle.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node handle was not found"));
    }

    auto AddResult = m_world->EcsRuntime().AddComponentWithId<T>(*m_world, OwnerHandle, Id, std::forward<Args>(args)...);
    if (!AddResult)
    {
        return std::unexpected(AddResult.error());
    }

    T* ComponentPtr = m_world->EcsRuntime().Component<T>(OwnerHandle);
    if (!ComponentPtr)
    {
        (void)m_world->EcsRuntime().RemoveComponent<T>(*m_world, OwnerHandle);
        return std::unexpected(MakeError(EErrorCode::InternalError, "Runtime component creation returned null"));
    }

    InitializeRuntimeComponentState(*this, *AddResult, *ComponentPtr);
    return *AddResult;
}

template<RuntimeTickType T>
TExpectedRef<T> BaseNode::RuntimeComponent()
{
    if (!m_world)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Node is not bound to a world"));
    }

    NodeHandle OwnerHandle = Handle();
    if (OwnerHandle.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node handle was not found"));
    }

    T* ComponentPtr = m_world->EcsRuntime().Component<T>(OwnerHandle);
    if (!ComponentPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Runtime component was not found on node"));
    }

    return *ComponentPtr;
}

template<RuntimeTickType T>
bool BaseNode::HasRuntimeComponent() const
{
    if (!m_world)
    {
        return false;
    }

    NodeHandle OwnerHandle = Handle();
    if (OwnerHandle.IsNull())
    {
        return false;
    }

    return m_world->HasRuntimeComponent(OwnerHandle, StaticTypeId<T>());
}

template<RuntimeTickType T>
Result BaseNode::RemoveRuntimeComponent()
{
    if (!m_world)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Node is not bound to a world"));
    }

    NodeHandle OwnerHandle = Handle();
    if (OwnerHandle.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node handle was not found"));
    }

    auto RemoveResult = m_world->RemoveRuntimeComponent(OwnerHandle, StaticTypeId<T>());
    if (!RemoveResult)
    {
        return std::unexpected(RemoveResult.error());
    }

    UnregisterRuntimeComponentOnNode(*this, StaticTypeId<T>());
    return Ok();
}

} // namespace SnAPI::GameFramework
