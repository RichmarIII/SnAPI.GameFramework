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

/**
 * @brief A node that owns and manages a hierarchy of child nodes.
 * @remarks NodeGraph itself is a node and can be nested.
 * @note All nodes and components are stored with UUID handles.
 */
class NodeGraph : public BaseNode
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::NodeGraph";

    /** @brief Non-copyable. */
    NodeGraph(const NodeGraph&) = delete;
    /** @brief Non-copyable. */
    NodeGraph& operator=(const NodeGraph&) = delete;
    /**
     * @brief Move construct a graph.
     * @param Other Graph to move from.
     * @remarks Rebinds owner pointers on nodes/components.
     */
    NodeGraph(NodeGraph&& Other) noexcept;
    /**
     * @brief Move assign a graph.
     * @param Other Graph to move from.
     * @return Reference to this graph.
     * @remarks Rebinds owner pointers on nodes/components.
     */
    NodeGraph& operator=(NodeGraph&& Other) noexcept;

    /**
     * @brief Construct an empty graph with default name.
     * @remarks Initializes the internal node pool.
     */
    NodeGraph()
        : m_nodePool(std::make_shared<TObjectPool<BaseNode>>())
    {
        TypeKey(TypeIdFromName(kTypeName));
    }
    /**
     * @brief Construct an empty graph with a name.
     * @param Name Graph name.
     */
    explicit NodeGraph(std::string Name)
        : BaseNode(std::move(Name))
        , m_nodePool(std::make_shared<TObjectPool<BaseNode>>())
    {
        TypeKey(TypeIdFromName(kTypeName));
    }
    /**
     * @brief Destructor.
     * @remarks Clears nodes/components and unregisters them from ObjectRegistry.
     */
    ~NodeGraph() override;

    /**
     * @brief Create a node of type T with a generated UUID.
     * @tparam T Node type (must derive from BaseNode).
     * @param Name Node name.
     * @param args Constructor arguments for T.
     * @return Handle to the created node or error.
     * @remarks Registers the node in ObjectRegistry and adds it to root nodes.
     */
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

    /**
     * @brief Create a node of type T with an explicit UUID.
     * @tparam T Node type (must derive from BaseNode).
     * @param Id UUID to assign.
     * @param Name Node name.
     * @param args Constructor arguments for T.
     * @return Handle to the created node or error.
     * @remarks Used by serialization to preserve identity.
     */
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

    /**
     * @brief Create a node by reflected TypeId with a generated UUID.
     * @param Type Reflected type id.
     * @param Name Node name.
     * @return Handle to the created node or error.
     * @remarks Requires a registered default constructor.
     */
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

    /**
     * @brief Create a node by reflected TypeId with an explicit UUID.
     * @param Type Reflected type id.
     * @param Name Node name.
     * @param Id UUID to assign.
     * @return Handle to the created node or error.
     * @remarks Used by serialization to preserve identity.
     */
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

    /**
     * @brief Destroy a node at end-of-frame.
     * @param Handle Node handle to destroy.
     * @return Success or error.
     * @remarks The handle remains valid until EndFrame.
     */
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

    /**
     * @brief Attach a child node to a parent.
     * @param Parent Parent handle.
     * @param Child Child handle.
     * @return Success or error.
     * @remarks Updates both parent/child lists and root set.
     */
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

    /**
     * @brief Detach a child node from its parent.
     * @param Child Child handle.
     * @return Success or error.
     * @remarks Detached nodes become root nodes.
     */
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

    /**
     * @brief Tick the graph (relevance + node tree).
     * @param DeltaSeconds Time since last tick.
     * @remarks Evaluates relevance before ticking roots.
     */
    void Tick(float DeltaSeconds) override;
    /**
     * @brief Fixed-step tick for the graph.
     * @param DeltaSeconds Fixed time step.
     */
    void FixedTick(float DeltaSeconds) override;
    /**
     * @brief Late tick for the graph.
     * @param DeltaSeconds Time since last tick.
     */
    void LateTick(float DeltaSeconds) override;

    /**
     * @brief Process end-of-frame destruction for nodes/components.
     * @remarks Unregisters objects and clears pending lists.
     */
    void EndFrame();
    /**
     * @brief Remove all nodes/components immediately.
     * @remarks Unregisters everything and clears internal storage.
     */
    void Clear();

    /**
     * @brief Access the node pool (mutable).
     * @return Reference to the pool.
     */
    TObjectPool<BaseNode>& NodePool()
    {
        return *m_nodePool;
    }

    /**
     * @brief Access the node pool (const).
     * @return Const reference to the pool.
     */
    const TObjectPool<BaseNode>& NodePool() const
    {
        return *m_nodePool;
    }

    /**
     * @brief Get the relevance evaluation budget.
     * @return Max number of nodes evaluated per tick.
     */
    size_t RelevanceBudget() const
    {
        return m_relevanceBudget;
    }

    /**
     * @brief Set the relevance evaluation budget.
     * @param Budget Max number of nodes evaluated per tick (0 = unlimited).
     */
    void RelevanceBudget(size_t Budget)
    {
        m_relevanceBudget = Budget;
    }

private:
    friend class BaseNode;
    friend class ComponentSerializationRegistry;
    friend class NodeGraphSerializer;

    /**
     * @brief Add a component of type T to a node.
     * @tparam T Component type.
     * @param Owner Owner node handle.
     * @param args Constructor arguments.
     * @return Reference wrapper or error.
     * @remarks Registers the component type on the node.
     */
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

    /**
     * @brief Add a component of type T to a node with explicit UUID.
     * @tparam T Component type.
     * @param Owner Owner node handle.
     * @param Id Component UUID.
     * @param args Constructor arguments.
     * @return Reference wrapper or error.
     * @remarks Used by serialization to preserve identity.
     */
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

    /**
     * @brief Get a component of type T from a node.
     * @tparam T Component type.
     * @param Owner Owner node handle.
     * @return Reference wrapper or error.
     */
    template<typename T>
    TExpectedRef<T> Component(NodeHandle Owner)
    {
        auto& Storage = StorageFor<T>();
        return Storage.Component(Owner);
    }

    /**
     * @brief Check if a node has a component of type T.
     * @tparam T Component type.
     * @param Owner Owner node handle.
     * @return True if the component exists.
     */
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

    /**
     * @brief Remove a component of type T from a node.
     * @tparam T Component type.
     * @param Owner Owner node handle.
     * @remarks Removal is deferred until EndFrame.
     */
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

    /**
     * @brief Tick all components for a node.
     * @param Owner Owner node handle.
     * @param DeltaSeconds Time since last tick.
     */
    void TickComponents(NodeHandle Owner, float DeltaSeconds);
    /**
     * @brief Fixed-step tick all components for a node.
     * @param Owner Owner node handle.
     * @param DeltaSeconds Fixed time step.
     */
    void FixedTickComponents(NodeHandle Owner, float DeltaSeconds);
    /**
     * @brief Late tick all components for a node.
     * @param Owner Owner node handle.
     * @param DeltaSeconds Time since last tick.
     */
    void LateTickComponents(NodeHandle Owner, float DeltaSeconds);

    /**
     * @brief Evaluate relevance policies to enable/disable nodes.
     * @remarks Limited by RelevanceBudget.
     */
    void EvaluateRelevance();
    /**
     * @brief Check whether a node is active for ticking.
     * @param Handle Node handle.
     * @return True if node is active and relevant.
     */
    bool IsNodeActive(NodeHandle Handle);
    /**
     * @brief Register a component type on a node's type list/mask.
     * @param Node Node to update.
     * @param Type Component type id.
     */
    void RegisterComponentOnNode(BaseNode& Node, const TypeId& Type);
    /**
     * @brief Unregister a component type from a node's type list/mask.
     * @param Node Node to update.
     * @param Type Component type id.
     */
    void UnregisterComponentOnNode(BaseNode& Node, const TypeId& Type);
    /**
     * @brief Rebind owner graph pointers after move.
     * @remarks Updates nodes and component storages to point at this graph.
     */
    void RebindOwnerGraph();

    /**
     * @brief Get or create a component storage for type T.
     * @tparam T Component type.
     * @return Reference to the storage.
     * @remarks Lazily creates storage on first use.
     */
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

    /**
     * @brief Get a component storage by type id (mutable).
     * @param Type Component type id.
     * @return Pointer to storage or nullptr if not found.
     */
    IComponentStorage* Storage(const TypeId& Type)
    {
        auto It = m_storages.find(Type);
        if (It == m_storages.end())
        {
            return nullptr;
        }
        return It->second.get();
    }

    /**
     * @brief Get a component storage by type id (const).
     * @param Type Component type id.
     * @return Pointer to storage or nullptr if not found.
     */
    const IComponentStorage* Storage(const TypeId& Type) const
    {
        auto It = m_storages.find(Type);
        if (It == m_storages.end())
        {
            return nullptr;
        }
        return It->second.get();
    }

    /**
     * @brief Borrow a component instance by owner/type (mutable).
     * @param Owner Owner node handle.
     * @param Type Component type id.
     * @return Pointer to component or nullptr if missing.
     * @note Borrowed pointers must not be cached.
     */
    void* BorrowedComponent(NodeHandle Owner, const TypeId& Type)
    {
        auto* Store = Storage(Type);
        return Store ? Store->Borrowed(Owner) : nullptr;
    }

    /**
     * @brief Borrow a component instance by owner/type (const).
     * @param Owner Owner node handle.
     * @param Type Component type id.
     * @return Pointer to component or nullptr if missing.
     */
    const void* BorrowedComponent(NodeHandle Owner, const TypeId& Type) const
    {
        auto* Store = Storage(Type);
        return Store ? Store->Borrowed(Owner) : nullptr;
    }

    std::shared_ptr<TObjectPool<BaseNode>> m_nodePool{}; /**< @brief Pool for node storage. */
    std::unordered_map<TypeId, std::unique_ptr<IComponentStorage>, UuidHash> m_storages{}; /**< @brief Component storage by type id. */
    std::vector<NodeHandle> m_rootNodes{}; /**< @brief Root nodes (no parent). */
    std::vector<NodeHandle> m_pendingDestroy{}; /**< @brief Nodes scheduled for deletion. */
    size_t m_relevanceCursor = 0; /**< @brief Reserved for incremental relevance updates. */
    size_t m_relevanceBudget = 0; /**< @brief Max relevance evaluations per tick (0 = unlimited). */
};

} // namespace SnAPI::GameFramework

#include "BaseNode.inl"
