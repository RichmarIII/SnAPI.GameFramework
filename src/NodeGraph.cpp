#include "NodeGraph.h"
#include "Profiling.h"

#include <algorithm>

namespace SnAPI::GameFramework
{

bool NodeGraph::StorageNodeActivePredicate(void* UserData, const BaseNode& Node)
{
    auto* Graph = static_cast<NodeGraph*>(UserData);
    return Graph != nullptr && Graph->IsNodeActive(Node);
}

NodeGraph::NodeGraph(NodeGraph&& Other) noexcept
    : BaseNode(std::move(Other))
    , m_nodePool(std::move(Other.m_nodePool))
    , m_storages(std::move(Other.m_storages))
    , m_storageOrder(std::move(Other.m_storageOrder))
    , m_rootNodes(std::move(Other.m_rootNodes))
    , m_pendingDestroy(std::move(Other.m_pendingDestroy))
    , m_relevanceCursor(Other.m_relevanceCursor)
    , m_relevanceBudget(Other.m_relevanceBudget)
{
    
    RebindOwnerGraph();
    Other.m_relevanceCursor = 0;
    Other.m_relevanceBudget = 0;
}

NodeGraph::~NodeGraph()
{
    
    Clear();
}

NodeGraph& NodeGraph::operator=(NodeGraph&& Other) noexcept
{
    
    if (this == &Other)
    {
        return *this;
    }
    BaseNode::operator=(std::move(Other));
    m_nodePool = std::move(Other.m_nodePool);
    m_storages = std::move(Other.m_storages);
    m_storageOrder = std::move(Other.m_storageOrder);
    m_rootNodes = std::move(Other.m_rootNodes);
    m_pendingDestroy = std::move(Other.m_pendingDestroy);
    m_relevanceCursor = Other.m_relevanceCursor;
    m_relevanceBudget = Other.m_relevanceBudget;
    RebindOwnerGraph();
    Other.m_relevanceCursor = 0;
    Other.m_relevanceBudget = 0;
    return *this;
}

void NodeGraph::Tick(float DeltaSeconds)
{
    
    {
        
        EvaluateRelevance();
    }

    {
        
        for (const auto& Handle : m_rootNodes)
        {
            BaseNode* Node = nullptr;
            {
                
                Node = m_nodePool->Borrowed(Handle);
            }
            if (!Node)
            {
                continue;
            }

            
            Node->TickTree(DeltaSeconds);
        }
    }

    {
        for (IComponentStorage* Storage : m_storageOrder)
        {
            if (Storage)
            {
                Storage->TickAll(&StorageNodeActivePredicate, this, DeltaSeconds);
            }
        }
    }
}

void NodeGraph::FixedTick(float DeltaSeconds)
{
    
    {
        
        for (const auto& Handle : m_rootNodes)
        {
            BaseNode* Node = nullptr;
            {
                
                Node = m_nodePool->Borrowed(Handle);
            }
            if (!Node)
            {
                continue;
            }

            
            Node->FixedTickTree(DeltaSeconds);
        }
    }

    {
        for (IComponentStorage* Storage : m_storageOrder)
        {
            if (Storage)
            {
                Storage->FixedTickAll(&StorageNodeActivePredicate, this, DeltaSeconds);
            }
        }
    }
}

void NodeGraph::LateTick(float DeltaSeconds)
{
    
    {
        
        for (const auto& Handle : m_rootNodes)
        {
            BaseNode* Node = nullptr;
            {
                
                Node = m_nodePool->Borrowed(Handle);
            }
            if (!Node)
            {
                continue;
            }

            
            Node->LateTickTree(DeltaSeconds);
        }
    }

    {
        for (IComponentStorage* Storage : m_storageOrder)
        {
            if (Storage)
            {
                Storage->LateTickAll(&StorageNodeActivePredicate, this, DeltaSeconds);
            }
        }
    }
}

void NodeGraph::EndFrame()
{
    
    {
        
    for (const auto& Handle : m_pendingDestroy)
    {
        auto* Node = m_nodePool->Borrowed(Handle);
        if (!Node)
        {
            continue;
        }
        for (IComponentStorage* Storage : Node->ComponentStorages())
        {
            if (Storage)
            {
                Storage->Remove(Handle);
            }
        }
        if (!Node->Parent().IsNull())
        {
            if (auto* Parent = m_nodePool->Borrowed(Node->Parent()))
            {
                Parent->RemoveChild(Handle);
            }
        }
        else
        {
            m_rootNodes.erase(std::remove(m_rootNodes.begin(), m_rootNodes.end(), Handle), m_rootNodes.end());
        }
    }
    }

    {
        
    for (IComponentStorage* Storage : m_storageOrder)
    {
        if (Storage)
        {
            Storage->EndFrame();
        }
    }
    }

    {
        
    for (const auto& Handle : m_pendingDestroy)
    {
        ObjectRegistry::Instance().Unregister(Handle.Id);
    }
    }

    {
        
    m_nodePool->EndFrame();
    }
    m_pendingDestroy.clear();
}

void NodeGraph::Clear()
{
    
    const size_t Budget = m_relevanceBudget;
    {
        
    for (IComponentStorage* Storage : m_storageOrder)
    {
        if (Storage)
        {
            Storage->Clear();
        }
    }
    }

    if (m_nodePool)
    {
        
        m_nodePool->ForEachAll([&](const NodeHandle& Handle, BaseNode&) {
            ObjectRegistry::Instance().Unregister(Handle.Id);
        });
        m_nodePool->Clear();
    }
    m_storages.clear();
    m_storageOrder.clear();
    m_rootNodes.clear();
    m_pendingDestroy.clear();
    m_relevanceCursor = 0;
    m_relevanceBudget = Budget;
}

void NodeGraph::TickComponents(BaseNode& Owner, float DeltaSeconds)
{
    
    const NodeHandle OwnerHandle = Owner.Handle();
    
    for (IComponentStorage* Storage : Owner.ComponentStorages())
    {
        if (!Storage)
        {
            continue;
        }

        {
            
            Storage->TickComponent(OwnerHandle, DeltaSeconds);
        }
    }
}

void NodeGraph::FixedTickComponents(BaseNode& Owner, float DeltaSeconds)
{
    
    const NodeHandle OwnerHandle = Owner.Handle();
    
    for (IComponentStorage* Storage : Owner.ComponentStorages())
    {
        if (!Storage)
        {
            continue;
        }

        {
            
            Storage->FixedTickComponent(OwnerHandle, DeltaSeconds);
        }
    }
}

void NodeGraph::LateTickComponents(BaseNode& Owner, float DeltaSeconds)
{
    
    const NodeHandle OwnerHandle = Owner.Handle();
    
    for (IComponentStorage* Storage : Owner.ComponentStorages())
    {
        if (!Storage)
        {
            continue;
        }

        {
            
            Storage->LateTickComponent(OwnerHandle, DeltaSeconds);
        }
    }
}

void NodeGraph::EvaluateRelevance()
{
    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    auto* StorageBase = Storage(RelevanceType);
    if (!StorageBase)
    {
        m_relevanceCursor = 0;
        return;
    }

    auto* RelevanceStorage = static_cast<TComponentStorage<RelevanceComponent>*>(StorageBase);
    const std::size_t Count = RelevanceStorage->DenseSize();
    if (Count == 0)
    {
        m_relevanceCursor = 0;
        return;
    }

    const std::size_t Budget = (m_relevanceBudget == 0) ? Count : std::min(m_relevanceBudget, Count);
    const std::size_t Cursor = m_relevanceCursor % Count;

    std::size_t Evaluated = 0;
    while (Evaluated < Budget)
    {
        const std::size_t DenseIndex = (Cursor + Evaluated) % Count;
        NodeHandle Handle = RelevanceStorage->DenseOwner(DenseIndex);
        RelevanceComponent* Component = RelevanceStorage->DenseComponent(DenseIndex);
        if (!Component)
        {
            ++Evaluated;
            continue;
        }

        auto* Node = m_nodePool->Borrowed(Handle);
        if (!Node || Node->PendingDestroy())
        {
            ++Evaluated;
            continue;
        }

        const auto* PolicyInfo = RelevancePolicyRegistry::Find(Component->PolicyId());
        if (!PolicyInfo || !Component->PolicyData())
        {
            ++Evaluated;
            continue;
        }

        RelevanceContext Context{Handle, *this};
        const bool Active = PolicyInfo->Evaluate(Component->PolicyData().get(), Context);
        Component->Active(Active);
        ++Evaluated;
    }

    if (m_relevanceBudget == 0 || Budget >= Count)
    {
        m_relevanceCursor = 0;
    }
    else
    {
        m_relevanceCursor = (Cursor + Evaluated) % Count;
    }
}

bool NodeGraph::IsNodeActive(NodeHandle Handle)
{
    
    auto* Node = m_nodePool->Borrowed(Handle);
    if (!Node)
    {
        return false;
    }
    return IsNodeActive(*Node);
}

bool NodeGraph::IsNodeActive(const BaseNode& Node) const
{
    
    if (Node.PendingDestroy())
    {
        return false;
    }
    if (!Node.Active())
    {
        return false;
    }

    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    static const uint32_t RelevanceTypeIndex = ComponentTypeRegistry::TypeIndex(RelevanceType);
    const auto& Mask = Node.ComponentMask();
    const std::size_t Word = RelevanceTypeIndex / 64u;
    const std::size_t Bit = RelevanceTypeIndex % 64u;
    if (Word >= Mask.size() || (Mask[Word] & (1ull << Bit)) == 0)
    {
        return true;
    }

    const auto* Relevance = Node.RelevanceState();
    if (!Relevance)
    {
        return true;
    }
    return Relevance->Active();
}

void NodeGraph::RegisterComponentOnNode(BaseNode& Node, const TypeId& Type, IComponentStorage& Storage)
{
    
    const uint32_t TypeIndex = ComponentTypeRegistry::TypeIndex(Type);
    const uint32_t Version = ComponentTypeRegistry::Version();
    if (Node.MaskVersion() != Version)
    {
        Node.ComponentMask().resize(ComponentTypeRegistry::WordCount(), 0);
        Node.MaskVersion(Version);
    }
    const size_t Word = TypeIndex / 64u;
    const size_t Bit = TypeIndex % 64u;
    if (Word >= Node.ComponentMask().size())
    {
        Node.ComponentMask().resize(Word + 1, 0);
    }
    Node.ComponentMask()[Word] |= (1ull << Bit);

    auto& Types = Node.ComponentTypes();
    auto& Storages = Node.ComponentStorages();
    if (Storages.size() < Types.size())
    {
        Storages.resize(Types.size(), nullptr);
    }

    for (size_t Index = 0; Index < Types.size(); ++Index)
    {
        if (Types[Index] == Type)
        {
            Storages[Index] = &Storage;
            static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
            if (Type == RelevanceType)
            {
                Node.RelevanceState(static_cast<RelevanceComponent*>(Storage.Borrowed(Node.Handle())));
            }
            return;
        }
    }

    Types.push_back(Type);
    Storages.push_back(&Storage);

    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    if (Type == RelevanceType)
    {
        Node.RelevanceState(static_cast<RelevanceComponent*>(Storage.Borrowed(Node.Handle())));
    }
}

void NodeGraph::UnregisterComponentOnNode(BaseNode& Node, const TypeId& Type)
{
    
    const uint32_t TypeIndex = ComponentTypeRegistry::TypeIndex(Type);
    const size_t Word = TypeIndex / 64u;
    const size_t Bit = TypeIndex % 64u;
    if (Word < Node.ComponentMask().size())
    {
        Node.ComponentMask()[Word] &= ~(1ull << Bit);
    }

    auto& Types = Node.ComponentTypes();
    auto& Storages = Node.ComponentStorages();
    for (size_t Index = 0; Index < Types.size(); ++Index)
    {
        if (Types[Index] == Type)
        {
            auto TypeIt = Types.begin() + static_cast<std::vector<TypeId>::difference_type>(Index);
            Types.erase(TypeIt);
            if (Index < Storages.size())
            {
                auto StorageIt = Storages.begin() + static_cast<std::vector<IComponentStorage*>::difference_type>(Index);
                Storages.erase(StorageIt);
            }
            break;
        }
    }

    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    if (Type == RelevanceType)
    {
        Node.RelevanceState(nullptr);
    }
}

void NodeGraph::RebindOwnerGraph()
{
    
    if (!m_nodePool)
    {
        return;
    }
    m_nodePool->ForEach([this](const NodeHandle&, BaseNode& Node) {
        Node.OwnerGraph(this);
        Node.World(World());
    });
}

} // namespace SnAPI::GameFramework
