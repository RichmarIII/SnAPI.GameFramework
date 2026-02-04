#pragma once

#include <functional>
#include <memory>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "BaseNode.h"
#include "ComponentStorage.h"
#include "Expected.h"
#include "ObjectPool.h"
#include "ObjectRegistry.h"
#include "Relevance.h"
#include "TypeRegistry.h"
#include "TypeName.h"
#include "Uuid.h"

namespace SnAPI::GameFramework
{

class ComponentSerializationRegistry;
class NodeGraphSerializer;

class NodeGraph : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::NodeGraph";

    NodeGraph(const NodeGraph&) = delete;
    NodeGraph& operator=(const NodeGraph&) = delete;
    NodeGraph(NodeGraph&& Other) noexcept;
    NodeGraph& operator=(NodeGraph&& Other) noexcept;

    NodeGraph()
        : m_nodePool(std::make_shared<TObjectPool<BaseNode>>())
    {
        TypeKey(TypeIdFromName(kTypeName));
    }
    explicit NodeGraph(std::string Name)
        : BaseNode(std::move(Name))
        , m_nodePool(std::make_shared<TObjectPool<BaseNode>>())
    {
        TypeKey(TypeIdFromName(kTypeName));
    }
    ~NodeGraph() override;

    template<typename T = BaseNode, typename... Args>
    TExpected<NodeHandle> CreateNode(std::string Name, Args&&... args)
    {
        static_assert(std::is_base_of_v<BaseNode, T>, "Nodes must derive from BaseNode");
        auto HandleResult = m_nodePool->Create<T>(std::forward<Args>(args)...);
        if (!HandleResult)
        {
            return std::unexpected(HandleResult.error());
        }
        NodeHandle Handle = HandleResult.value();
        auto* Node = m_nodePool->Borrowed(Handle);
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create node"));
        }
        Node->Handle(Handle);
        Node->Name(std::move(Name));
        Node->OwnerGraph(this);
        Node->TypeKey(TypeIdFromName(TTypeNameV<T>));
        ObjectRegistry::Instance().RegisterNode(Node->Id(), Node);
        m_rootNodes.push_back(Handle);
        return Handle;
    }

    template<typename T = BaseNode, typename... Args>
    TExpected<NodeHandle> CreateNodeWithId(const Uuid& Id, std::string Name, Args&&... args)
    {
        static_assert(std::is_base_of_v<BaseNode, T>, "Nodes must derive from BaseNode");
        auto HandleResult = m_nodePool->CreateWithId<T>(Id, std::forward<Args>(args)...);
        if (!HandleResult)
        {
            return std::unexpected(HandleResult.error());
        }
        NodeHandle Handle = HandleResult.value();
        auto* Node = m_nodePool->Borrowed(Handle);
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create node"));
        }
        Node->Handle(Handle);
        Node->Name(std::move(Name));
        Node->OwnerGraph(this);
        Node->TypeKey(TypeIdFromName(TTypeNameV<T>));
        ObjectRegistry::Instance().RegisterNode(Node->Id(), Node);
        m_rootNodes.push_back(Handle);
        return Handle;
    }

    TExpected<NodeHandle> CreateNode(const TypeId& Type, std::string Name)
    {
        auto* Info = TypeRegistry::Instance().Find(Type);
        if (!Info)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
        }
        if (!TypeRegistry::Instance().IsA(Type, TypeIdFromName(BaseNode::kTypeName)))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Type is not a node type"));
        }
        const ConstructorInfo* Ctor = nullptr;
        for (const auto& Candidate : Info->Constructors)
        {
            if (Candidate.ParamTypes.empty())
            {
                Ctor = &Candidate;
                break;
            }
        }
        if (!Ctor)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "No default constructor registered"));
        }
        std::span<const Variant> EmptyArgs;
        auto InstanceResult = Ctor->Construct(EmptyArgs);
        if (!InstanceResult)
        {
            return std::unexpected(InstanceResult.error());
        }
        auto BasePtr = std::static_pointer_cast<BaseNode>(InstanceResult.value());
        if (!BasePtr)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Node type mismatch"));
        }
        auto HandleResult = m_nodePool->CreateFromShared(std::move(BasePtr));
        if (!HandleResult)
        {
            return std::unexpected(HandleResult.error());
        }
        NodeHandle Handle = HandleResult.value();
        auto* Node = m_nodePool->Borrowed(Handle);
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create node"));
        }
        Node->Handle(Handle);
        Node->Name(std::move(Name));
        Node->OwnerGraph(this);
        Node->TypeKey(Type);
        ObjectRegistry::Instance().RegisterNode(Node->Id(), Node);
        m_rootNodes.push_back(Handle);
        return Handle;
    }

    TExpected<NodeHandle> CreateNode(const TypeId& Type, std::string Name, const Uuid& Id)
    {
        auto* Info = TypeRegistry::Instance().Find(Type);
        if (!Info)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Type not registered"));
        }
        if (!TypeRegistry::Instance().IsA(Type, TypeIdFromName(BaseNode::kTypeName)))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Type is not a node type"));
        }
        const ConstructorInfo* Ctor = nullptr;
        for (const auto& Candidate : Info->Constructors)
        {
            if (Candidate.ParamTypes.empty())
            {
                Ctor = &Candidate;
                break;
            }
        }
        if (!Ctor)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "No default constructor registered"));
        }
        std::span<const Variant> EmptyArgs;
        auto InstanceResult = Ctor->Construct(EmptyArgs);
        if (!InstanceResult)
        {
            return std::unexpected(InstanceResult.error());
        }
        auto BasePtr = std::static_pointer_cast<BaseNode>(InstanceResult.value());
        if (!BasePtr)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Node type mismatch"));
        }
        auto HandleResult = m_nodePool->CreateFromSharedWithId(std::move(BasePtr), Id);
        if (!HandleResult)
        {
            return std::unexpected(HandleResult.error());
        }
        NodeHandle Handle = HandleResult.value();
        auto* Node = m_nodePool->Borrowed(Handle);
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to create node"));
        }
        Node->Handle(Handle);
        Node->Name(std::move(Name));
        Node->OwnerGraph(this);
        Node->TypeKey(Type);
        ObjectRegistry::Instance().RegisterNode(Node->Id(), Node);
        m_rootNodes.push_back(Handle);
        return Handle;
    }

    TExpected<void> DestroyNode(NodeHandle Handle)
    {
        auto* Node = Handle.Borrowed();
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
        }
        m_pendingDestroy.push_back(Handle);
        return m_nodePool->DestroyLater(Handle);
    }

    TExpected<void> AttachChild(NodeHandle Parent, NodeHandle Child)
    {
        auto* ParentNode = Parent.Borrowed();
        auto* ChildNode = Child.Borrowed();
        if (!ParentNode || !ChildNode)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Parent or child not found"));
        }
        if (!ChildNode->Parent().IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Child already has a parent"));
        }
        ParentNode->AddChild(Child);
        ChildNode->Parent(Parent);
        for (auto It = m_rootNodes.begin(); It != m_rootNodes.end(); ++It)
        {
            if (*It == Child)
            {
                m_rootNodes.erase(It);
                break;
            }
        }
        return Ok();
    }

    TExpected<void> DetachChild(NodeHandle Child)
    {
        auto* ChildNode = Child.Borrowed();
        if (!ChildNode)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Child not found"));
        }
        if (!ChildNode->Parent().IsNull())
        {
            if (auto* ParentNode = ChildNode->Parent().Borrowed())
            {
                ParentNode->RemoveChild(Child);
            }
            ChildNode->Parent({});
        }
        m_rootNodes.push_back(Child);
        return Ok();
    }

    void Tick(float DeltaSeconds) override;
    void FixedTick(float DeltaSeconds) override;
    void LateTick(float DeltaSeconds) override;

    void EndFrame();
    void Clear();

    TObjectPool<BaseNode>& NodePool()
    {
        return *m_nodePool;
    }

    const TObjectPool<BaseNode>& NodePool() const
    {
        return *m_nodePool;
    }

    size_t RelevanceBudget() const
    {
        return m_relevanceBudget;
    }

    void RelevanceBudget(size_t Budget)
    {
        m_relevanceBudget = Budget;
    }

private:
    friend class BaseNode;
    friend class ComponentSerializationRegistry;
    friend class NodeGraphSerializer;

    template<typename T, typename... Args>
    TExpectedRef<T> AddComponent(NodeHandle Owner, Args&&... args)
    {
        auto* Node = Owner.Borrowed();
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
        }
        auto& Storage = StorageFor<T>();
        auto Result = Storage.Add(Owner, std::forward<Args>(args)...);
        if (!Result)
        {
            return std::unexpected(Result.error());
        }
        RegisterComponentOnNode(*Node, TypeIdFromName(TTypeNameV<T>));
        return Result;
    }

    template<typename T, typename... Args>
    TExpectedRef<T> AddComponentWithId(NodeHandle Owner, const Uuid& Id, Args&&... args)
    {
        auto* Node = Owner.Borrowed();
        if (!Node)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Node not found"));
        }
        auto& Storage = StorageFor<T>();
        auto Result = Storage.AddWithId(Owner, Id, std::forward<Args>(args)...);
        if (!Result)
        {
            return std::unexpected(Result.error());
        }
        RegisterComponentOnNode(*Node, TypeIdFromName(TTypeNameV<T>));
        return Result;
    }

    template<typename T>
    TExpectedRef<T> Component(NodeHandle Owner)
    {
        auto& Storage = StorageFor<T>();
        return Storage.Component(Owner);
    }

    template<typename T>
    bool HasComponent(NodeHandle Owner) const
    {
        auto It = m_storages.find(TypeIdFromName(TTypeNameV<T>));
        if (It == m_storages.end())
        {
            return false;
        }
        return It->second->Has(Owner);
    }

    template<typename T>
    void RemoveComponent(NodeHandle Owner)
    {
        auto It = m_storages.find(TypeIdFromName(TTypeNameV<T>));
        if (It == m_storages.end())
        {
            return;
        }
        auto* Node = Owner.Borrowed();
        if (!Node)
        {
            return;
        }
        It->second->Remove(Owner);
        UnregisterComponentOnNode(*Node, TypeIdFromName(TTypeNameV<T>));
    }

    void TickComponents(NodeHandle Owner, float DeltaSeconds);
    void FixedTickComponents(NodeHandle Owner, float DeltaSeconds);
    void LateTickComponents(NodeHandle Owner, float DeltaSeconds);

    void EvaluateRelevance();
    bool IsNodeActive(NodeHandle Handle);
    void RegisterComponentOnNode(BaseNode& Node, const TypeId& Type);
    void UnregisterComponentOnNode(BaseNode& Node, const TypeId& Type);
    void RebindOwnerGraph();

    template<typename T>
    TComponentStorage<T>& StorageFor()
    {
        const TypeId Type = TypeIdFromName(TTypeNameV<T>);
        auto It = m_storages.find(Type);
        if (It == m_storages.end())
        {
            auto Storage = std::make_unique<TComponentStorage<T>>();
            auto* Ptr = Storage.get();
            m_storages.emplace(Type, std::move(Storage));
            return *Ptr;
        }
        return *static_cast<TComponentStorage<T>*>(It->second.get());
    }

    IComponentStorage* Storage(const TypeId& Type)
    {
        auto It = m_storages.find(Type);
        if (It == m_storages.end())
        {
            return nullptr;
        }
        return It->second.get();
    }

    const IComponentStorage* Storage(const TypeId& Type) const
    {
        auto It = m_storages.find(Type);
        if (It == m_storages.end())
        {
            return nullptr;
        }
        return It->second.get();
    }

    void* BorrowedComponent(NodeHandle Owner, const TypeId& Type)
    {
        auto* Store = Storage(Type);
        return Store ? Store->Borrowed(Owner) : nullptr;
    }

    const void* BorrowedComponent(NodeHandle Owner, const TypeId& Type) const
    {
        auto* Store = Storage(Type);
        return Store ? Store->Borrowed(Owner) : nullptr;
    }

    std::shared_ptr<TObjectPool<BaseNode>> m_nodePool{};
    std::unordered_map<TypeId, std::unique_ptr<IComponentStorage>, UuidHash> m_storages{};
    std::vector<NodeHandle> m_rootNodes{};
    std::vector<NodeHandle> m_pendingDestroy{};
    size_t m_relevanceCursor = 0;
    size_t m_relevanceBudget = 0;
};

} // namespace SnAPI::GameFramework

#include "BaseNode.inl"
