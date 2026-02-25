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
 * @brief Lightweight level node facade in the ECS-only architecture.
 * @remarks
 * Level is a regular node with convenience helpers. World owns all node/component
 * storage and lifecycle.
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

    void World(IWorld* InWorld)
    {
        BaseNode::World(InWorld);
    }

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

    void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
    void FixedTick(float DeltaSeconds) { (void)DeltaSeconds; }
    void LateTick(float DeltaSeconds) { (void)DeltaSeconds; }
    void EndFrame() {}
    void Clear() {}

    TObjectPool<BaseNode>& NodePool()
    {
        if (IWorld* OwnerWorld = World())
        {
            return OwnerWorld->NodePool();
        }
        return NullNodePool();
    }

    const TObjectPool<BaseNode>& NodePool() const
    {
        if (const IWorld* OwnerWorld = World())
        {
            return OwnerWorld->NodePool();
        }
        return NullNodePool();
    }

    TExpected<NodeHandle> NodeHandleByIdSlow(const Uuid& Id) const
    {
        const IWorld* OwnerWorld = World();
        if (!OwnerWorld)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Level is not bound to a world"));
        }
        return OwnerWorld->NodeHandleById(Id);
    }

    Result RemoveComponentByType(const NodeHandle& Owner, const TypeId& Type)
    {
        IWorld* OwnerWorld = World();
        if (!OwnerWorld)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Level is not bound to a world"));
        }
        return OwnerWorld->RemoveComponentByType(Owner, Type);
    }

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

    template<typename T>
    void RemoveComponent(const NodeHandle& Owner)
    {
        if (BaseNode* OwnerNode = Owner.Borrowed())
        {
            OwnerNode->Remove<T>();
        }
    }

    void* BorrowedComponent(const NodeHandle& Owner, const TypeId& Type)
    {
        IWorld* OwnerWorld = World();
        return OwnerWorld ? OwnerWorld->BorrowedComponent(Owner, Type) : nullptr;
    }

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
