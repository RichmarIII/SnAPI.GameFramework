#pragma once

#include <string>
#include <type_traits>

#include "BaseNode.h"
#include "Expected.h"
#include "IWorld.h"
#include "ObjectPool.h"
#include "StaticTypeId.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Level node facade that forwards graph operations into the owning world.
 *
 * `Level` is a regular `BaseNode`-derived object used as a convenient grouping root for
 * gameplay content. In the ECS-only architecture it does not own separate storage; instead,
 * it forwards creation, destruction, attachment, and component operations into the `IWorld`
 * it is bound to. This gives users a level-centric authoring API without splitting ownership
 * away from the world.
 *
 * Core semantics:
 * - a level is just another node as far as world ownership is concerned
 * - child nodes created through a bound level are attached under that level when possible
 * - all storage, identity, and destruction rules still come from the world
 *
 * Ownership and lifetime:
 * - The world owns the level and every node/component reachable through it.
 * - References returned by level APIs are borrowed views into world-owned objects.
 *
 * Threading model:
 * - Main-thread only for graph mutation.
 *
 * @see IWorld
 * @see World
 * @see BaseNode
 */
class Level : public BaseNode
{
public:
    using BaseNode::World;

    static constexpr const char* kTypeName = "SnAPI::GameFramework::Level";

    Level()
    {
        TypeKey(StaticTypeId<Level>());
    }

    explicit Level(std::string Name)
        : BaseNode(std::move(Name))
    {
        TypeKey(StaticTypeId<Level>());
    }

    Level(const Level&) = delete;
    Level& operator=(const Level&) = delete;
    Level(Level&&) noexcept = default;
    Level& operator=(Level&&) noexcept = default;
    ~Level() = default;

    /**
     * @brief Bind this level to an owning world.
     * @param InWorld Non-owning world pointer.
     * @remarks Managed by world/runtime code; callers rarely need this directly.
     */
    void World(IWorld* InWorld)
    {
        BaseNode::World(InWorld);
    }

    /**
     * @brief Create a child node of reflected type `T`.
     * @tparam T Concrete node type deriving from `BaseNode`.
     * @tparam Args Additional constructor arguments; currently unsupported by the ECS-only creation path.
     * @param Name Display/debug name for the created node.
     * @param args Additional constructor arguments. Must be omitted; passing any value causes the call
     *        to fail with `EErrorCode::InvalidArgument`.
     * @return Handle to the created node or an error.
     * @remarks
     * When this level has a valid handle, the created node is attached under the level.
     * If the level itself is not yet world-owned, creation falls back to direct world creation semantics.
     */
    template<typename T = BaseNode, typename... Args>
    TExpected<NodeHandle> CreateNode(std::string Name, Args&&... args)
    {
        static_assert(std::is_base_of_v<BaseNode, T>, "Nodes must derive from BaseNode");
        if constexpr (sizeof...(args) != 0)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "ECS-only node creation requires default-constructible reflected nodes"));
        }
        return CreateNode(StaticTypeId<T>(), std::move(Name));
    }

    /**
     * @brief Create a child node of reflected type `T` with an explicit UUID.
     * @tparam T Concrete node type deriving from `BaseNode`.
     * @tparam Args Additional constructor arguments; currently unsupported by the ECS-only creation path.
     * @param Id Explicit stable identity for the created node.
     * @param Name Display/debug name for the created node.
     * @param args Additional constructor arguments. Must be omitted; passing any value causes the call
     *        to fail with `EErrorCode::InvalidArgument`.
     * @return Handle to the created node or an error.
     */
    template<typename T = BaseNode, typename... Args>
    TExpected<NodeHandle> CreateNodeWithId(const Uuid& Id, std::string Name, Args&&... args)
    {
        static_assert(std::is_base_of_v<BaseNode, T>, "Nodes must derive from BaseNode");
        if constexpr (sizeof...(args) != 0)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "ECS-only node creation requires default-constructible reflected nodes"));
        }
        return CreateNode(StaticTypeId<T>(), std::move(Name), Id);
    }

    /**
     * @brief Create a child node by reflected type.
     * @param Type Reflected node type id.
     * @param Name Display/debug name for the created node.
     * @return Handle to the created node or an error.
     * @pre This level must be bound to a world.
     */
    TExpected<NodeHandle> CreateNode(const TypeId& Type, std::string Name)
    {
        IWorld* OwnerWorld = World();
        if (!OwnerWorld)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Level is not bound to a world"));
        }

        if (Handle().IsNull())
        {
            return OwnerWorld->CreateNode(Type, std::move(Name));
        }

        auto CreateResult = OwnerWorld->CreateNode(Type, std::move(Name));
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }

        const NodeHandle CreatedHandle = *CreateResult;
        auto AttachResult = OwnerWorld->AttachChild(Handle(), CreatedHandle);
        if (!AttachResult)
        {
            (void)OwnerWorld->DestroyNode(CreatedHandle);
            return std::unexpected(AttachResult.error());
        }
        return CreatedHandle;
    }

    /**
     * @brief Create a child node by reflected type with an explicit UUID.
     * @param Type Reflected node type id.
     * @param Name Display/debug name for the created node.
     * @param Id Explicit stable identity for the created node.
     * @return Handle to the created node or an error.
     * @pre This level must be bound to a world.
     */
    TExpected<NodeHandle> CreateNode(const TypeId& Type, std::string Name, const Uuid& Id)
    {
        IWorld* OwnerWorld = World();
        if (!OwnerWorld)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Level is not bound to a world"));
        }

        if (Handle().IsNull())
        {
            return OwnerWorld->CreateNodeWithId(Type, std::move(Name), Id);
        }

        auto CreateResult = OwnerWorld->CreateNodeWithId(Type, std::move(Name), Id);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }

        const NodeHandle CreatedHandle = *CreateResult;
        auto AttachResult = OwnerWorld->AttachChild(Handle(), CreatedHandle);
        if (!AttachResult)
        {
            (void)OwnerWorld->DestroyNode(CreatedHandle);
            return std::unexpected(AttachResult.error());
        }
        return CreatedHandle;
    }

    /**
     * @brief Destroy a node through the owning world.
     * @param Handle Handle of the node to destroy.
     * @return Success or error.
     * @remarks Destruction semantics are whatever the owning world implements, typically end-of-frame deferred.
     */
    TExpected<void> DestroyNode(const NodeHandle& Handle)
    {
        IWorld* OwnerWorld = World();
        if (!OwnerWorld)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Level is not bound to a world"));
        }

        auto DestroyResult = OwnerWorld->DestroyNode(Handle);
        if (!DestroyResult)
        {
            return std::unexpected(DestroyResult.error());
        }
        return Ok();
    }

    /**
     * @brief Attach a child node under a parent node.
     * @param Parent Parent node handle.
     * @param Child Child node handle.
     * @return Success or error.
     * @remarks This forwards directly into the owning world.
     */
    TExpected<void> AttachChild(const NodeHandle& Parent, const NodeHandle& Child)
    {
        IWorld* OwnerWorld = World();
        if (!OwnerWorld)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Level is not bound to a world"));
        }

        auto AttachResult = OwnerWorld->AttachChild(Parent, Child);
        if (!AttachResult)
        {
            return std::unexpected(AttachResult.error());
        }
        return Ok();
    }

    /**
     * @brief Detach a child node from its current parent.
     * @param Child Child node handle.
     * @return Success or error.
     */
    TExpected<void> DetachChild(const NodeHandle& Child)
    {
        IWorld* OwnerWorld = World();
        if (!OwnerWorld)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Level is not bound to a world"));
        }

        auto DetachResult = OwnerWorld->DetachChild(Child);
        if (!DetachResult)
        {
            return std::unexpected(DetachResult.error());
        }
        return Ok();
    }

    /** @brief Variable-step level hook. @param DeltaSeconds Frame delta time in seconds. @remarks No-op by default. */
    void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    /** @brief Fixed-step level hook. @param DeltaSeconds Fixed delta time in seconds. @remarks No-op by default. */
    void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /** @brief Late-step level hook. @param DeltaSeconds Frame delta time in seconds. @remarks No-op by default. */
    void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }
    /** @brief End-of-frame level hook. @remarks No-op by default. */
    void EndFrame() {}
    /** @brief Clear level-owned state. @remarks No-op because storage ownership remains in the world. */
    void Clear() {}

    /**
     * @brief Access the owning world's node pool.
     * @return Mutable node pool reference.
     * @remarks Returns a process-local null pool facade when the level is unbound.
     */
    TObjectPool<BaseNode>& NodePool()
    {
        if (IWorld* OwnerWorld = World())
        {
            return OwnerWorld->NodePool();
        }
        return NullNodePool();
    }

    /**
     * @brief Access the owning world's node pool.
     * @return Const node pool reference.
     * @remarks Returns a process-local null pool facade when the level is unbound.
     */
    const TObjectPool<BaseNode>& NodePool() const
    {
        if (const IWorld* OwnerWorld = World())
        {
            return OwnerWorld->NodePool();
        }
        return NullNodePool();
    }

    /**
     * @brief Resolve a node by UUID through the owning world.
     * @param Id Stable node UUID.
     * @return Handle to the resolved node or an error.
     * @remarks This is a slow lookup path intended for persistence/bridging code rather than hot loops.
     */
    TExpected<NodeHandle> NodeHandleByIdSlow(const Uuid& Id) const
    {
        const IWorld* OwnerWorld = World();
        if (!OwnerWorld)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Level is not bound to a world"));
        }
        return OwnerWorld->NodeHandleById(Id);
    }

    /**
     * @brief Remove a component by reflected type from a node.
     * @param Owner Owning node handle.
     * @param Type Reflected component type id.
     * @return Success or error.
     */
    Result RemoveComponentByType(const NodeHandle& Owner, const TypeId& Type)
    {
        IWorld* OwnerWorld = World();
        if (!OwnerWorld)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Level is not bound to a world"));
        }
        return OwnerWorld->RemoveComponentByType(Owner, Type);
    }

    /**
     * @brief Attach a runtime-compatible component to a node.
     * @tparam T Concrete component type.
     * @tparam Args Constructor argument types forwarded into the component creation path.
     * @param Owner Owning node handle.
     * @param args Constructor arguments forwarded to `BaseNode::Add<T>()`.
     * @return Borrowed reference wrapper to the attached component or an error.
     */
    template<typename T, typename... Args>
    TExpectedRef<T> AddComponent(const NodeHandle& Owner, Args&&... args)
    {
        BaseNode* OwnerNode = Owner.Borrowed();
        if (!OwnerNode)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node not found"));
        }
        return OwnerNode->Add<T>(std::forward<Args>(args)...);
    }

    /**
     * @brief Attach a runtime-compatible component with an explicit UUID.
     * @tparam T Concrete component type.
     * @tparam Args Constructor argument types forwarded into the runtime component creation path.
     * @param Owner Owning node handle.
     * @param Id Explicit stable component identity.
     * @param args Constructor arguments forwarded to the runtime storage path.
     * @return Borrowed reference wrapper to the attached component or an error.
     */
    template<typename T, typename... Args>
    TExpectedRef<T> AddComponentWithId(const NodeHandle& Owner, const Uuid& Id, Args&&... args)
    {
        BaseNode* OwnerNode = Owner.Borrowed();
        if (!OwnerNode)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node not found"));
        }

        if constexpr (RuntimeTickType<T> && std::is_move_constructible_v<T>)
        {
            auto AddResult = OwnerNode->AddRuntimeComponentWithId<T>(Id, std::forward<Args>(args)...);
            if (!AddResult)
            {
                return std::unexpected(AddResult.error());
            }
            auto ComponentResult = OwnerNode->RuntimeComponent<T>();
            if (!ComponentResult)
            {
                return std::unexpected(ComponentResult.error());
            }
            return *ComponentResult;
        }
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "ECS-only components must be runtime-compatible and move constructible"));
    }

    /**
     * @brief Resolve a typed component attached to a node.
     * @tparam T Concrete component type.
     * @param Owner Owning node handle.
     * @return Borrowed reference wrapper to the component or an error.
     */
    template<typename T>
    TExpectedRef<T> Component(const NodeHandle& Owner)
    {
        BaseNode* OwnerNode = Owner.Borrowed();
        if (!OwnerNode)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Owner node not found"));
        }
        return OwnerNode->Component<T>();
    }

    /**
     * @brief Check whether a node currently has a component of type `T`.
     * @tparam T Concrete component type.
     * @param Owner Owning node handle.
     * @return `true` when the component is attached.
     */
    template<typename T>
    bool HasComponent(const NodeHandle& Owner) const
    {
        const BaseNode* OwnerNode = Owner.Borrowed();
        if (!OwnerNode)
        {
            return false;
        }
        return OwnerNode->Has<T>();
    }

    /**
     * @brief Remove a typed component from a node if it exists.
     * @tparam T Concrete component type.
     * @param Owner Owning node handle.
     * @remarks Missing owners are silently ignored.
     */
    template<typename T>
    void RemoveComponent(const NodeHandle& Owner)
    {
        if (BaseNode* OwnerNode = Owner.Borrowed())
        {
            OwnerNode->Remove<T>();
        }
    }

    /**
     * @brief Borrow a raw component pointer by reflected type.
     * @param Owner Owning node handle.
     * @param Type Reflected component type id.
     * @return Non-owning raw component pointer or `nullptr`.
     */
    void* BorrowedComponent(const NodeHandle& Owner, const TypeId& Type)
    {
        IWorld* OwnerWorld = World();
        return OwnerWorld ? OwnerWorld->BorrowedComponent(Owner, Type) : nullptr;
    }

    /**
     * @brief Borrow a raw component pointer by reflected type.
     * @param Owner Owning node handle.
     * @param Type Reflected component type id.
     * @return Non-owning raw component pointer or `nullptr`.
     */
    const void* BorrowedComponent(const NodeHandle& Owner, const TypeId& Type) const
    {
        const IWorld* OwnerWorld = World();
        return OwnerWorld ? OwnerWorld->BorrowedComponent(Owner, Type) : nullptr;
    }

private:
    static TObjectPool<BaseNode>& NullNodePool()
    {
        static TObjectPool<BaseNode> Pool{};
        return Pool;
    }
};

} // namespace SnAPI::GameFramework

#include "BaseNode.inl"
