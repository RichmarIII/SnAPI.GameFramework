#pragma once

#include <type_traits>

#include "Assert.h"
#include "ComponentStorage.h"
#include "Expected.h"
#include "BaseComponent.h"
#include "IWorld.h"
#include "ObjectRegistry.h"
#include "Relevance.h"

namespace SnAPI::GameFramework
{
namespace
{
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
    auto& Storages = Node.ComponentStorages();
    if (Storages.size() < Types.size())
    {
        Storages.resize(Types.size(), nullptr);
    }

    for (std::size_t Index = 0; Index < Types.size(); ++Index)
    {
        if (Types[Index] == Type)
        {
            Storages[Index] = nullptr;
            static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
            if (Type == RelevanceType)
            {
                Node.RelevanceState(static_cast<RelevanceComponent*>(ComponentPtr));
            }
            return;
        }
    }

    Types.push_back(Type);
    Storages.push_back(nullptr);

    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    if (Type == RelevanceType)
    {
        Node.RelevanceState(static_cast<RelevanceComponent*>(ComponentPtr));
    }
}

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
    auto& Storages = Node.ComponentStorages();
    for (std::size_t Index = 0; Index < Types.size(); ++Index)
    {
        if (Types[Index] != Type)
        {
            continue;
        }

        auto TypeIt = Types.begin() + static_cast<std::vector<TypeId>::difference_type>(Index);
        Types.erase(TypeIt);
        if (Index < Storages.size())
        {
            auto StorageIt = Storages.begin() + static_cast<std::vector<ComponentStorageView*>::difference_type>(Index);
            Storages.erase(StorageIt);
        }
        break;
    }

    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    if (Type == RelevanceType)
    {
        Node.RelevanceState(nullptr);
    }
}

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
    static_assert(RuntimeTickType<T>, "BaseNode::Add<T> requires ECS runtime-compatible component types");
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
    static_assert(RuntimeTickType<T>, "BaseNode::Component<T> requires ECS runtime-compatible component types");

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
    static_assert(RuntimeTickType<T>, "BaseNode::Has<T> requires ECS runtime-compatible component types");

    if (!m_world)
    {
        return false;
    }

    return HasRuntimeComponent<T>();
}

template<typename T>
void BaseNode::Remove()
{
    static_assert(RuntimeTickType<T>, "BaseNode::Remove<T> requires ECS runtime-compatible component types");

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

    const RuntimeNodeHandle OwnerRuntime = ResolveRuntimeNodeHandleAndCache();
    if (OwnerRuntime.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node runtime handle was not found"));
    }

    auto AddResult = m_world->EcsRuntime().AddComponent<T>(*m_world, OwnerRuntime, std::forward<Args>(args)...);
    if (!AddResult)
    {
        return std::unexpected(AddResult.error());
    }

    T* ComponentPtr = m_world->EcsRuntime().Component<T>(OwnerRuntime);
    if (!ComponentPtr)
    {
        (void)m_world->EcsRuntime().RemoveComponent<T>(*m_world, OwnerRuntime);
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

    const RuntimeNodeHandle OwnerRuntime = ResolveRuntimeNodeHandleAndCache();
    if (OwnerRuntime.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node runtime handle was not found"));
    }

    auto AddResult = m_world->EcsRuntime().AddComponentWithId<T>(*m_world, OwnerRuntime, Id, std::forward<Args>(args)...);
    if (!AddResult)
    {
        return std::unexpected(AddResult.error());
    }

    T* ComponentPtr = m_world->EcsRuntime().Component<T>(OwnerRuntime);
    if (!ComponentPtr)
    {
        (void)m_world->EcsRuntime().RemoveComponent<T>(*m_world, OwnerRuntime);
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

    const RuntimeNodeHandle OwnerRuntime = ResolveRuntimeNodeHandleAndCache();
    if (OwnerRuntime.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node runtime handle was not found"));
    }

    T* ComponentPtr = m_world->EcsRuntime().Component<T>(OwnerRuntime);
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

    const RuntimeNodeHandle OwnerRuntime = ResolveRuntimeNodeHandle();
    if (OwnerRuntime.IsNull())
    {
        return false;
    }

    return m_world->HasRuntimeComponent(OwnerRuntime, StaticTypeId<T>());
}

template<RuntimeTickType T>
Result BaseNode::RemoveRuntimeComponent()
{
    if (!m_world)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Node is not bound to a world"));
    }

    const RuntimeNodeHandle OwnerRuntime = ResolveRuntimeNodeHandleAndCache();
    if (OwnerRuntime.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node runtime handle was not found"));
    }

    auto RemoveResult = m_world->RemoveRuntimeComponent(OwnerRuntime, StaticTypeId<T>());
    if (!RemoveResult)
    {
        return std::unexpected(RemoveResult.error());
    }

    UnregisterRuntimeComponentOnNode(*this, StaticTypeId<T>());
    return Ok();
}

} // namespace SnAPI::GameFramework
