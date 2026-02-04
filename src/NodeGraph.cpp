#include "NodeGraph.h"

namespace SnAPI::GameFramework
{

NodeGraph::NodeGraph(NodeGraph&& Other) noexcept
    : BaseNode(std::move(Other))
    , m_nodePool(std::move(Other.m_nodePool))
    , m_storages(std::move(Other.m_storages))
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
    EvaluateRelevance();
    for (const auto& Handle : m_rootNodes)
    {
        if (auto* Node = Handle.Borrowed())
        {
            Node->TickTree(DeltaSeconds);
        }
    }
}

void NodeGraph::FixedTick(float DeltaSeconds)
{
    for (const auto& Handle : m_rootNodes)
    {
        if (auto* Node = Handle.Borrowed())
        {
            Node->FixedTickTree(DeltaSeconds);
        }
    }
}

void NodeGraph::LateTick(float DeltaSeconds)
{
    for (const auto& Handle : m_rootNodes)
    {
        if (auto* Node = Handle.Borrowed())
        {
            Node->LateTickTree(DeltaSeconds);
        }
    }
}

void NodeGraph::EndFrame()
{
    for (const auto& Handle : m_pendingDestroy)
    {
        auto* Node = Handle.Borrowed();
        if (!Node)
        {
            continue;
        }
        for (const auto& Type : Node->ComponentTypes())
        {
            auto It = m_storages.find(Type);
            if (It != m_storages.end())
            {
                It->second->Remove(Handle);
            }
        }
        if (!Node->Parent().IsNull())
        {
            if (auto* Parent = Node->Parent().Borrowed())
            {
                Parent->RemoveChild(Handle);
            }
        }
        else
        {
            for (auto It = m_rootNodes.begin(); It != m_rootNodes.end(); ++It)
            {
                if (*It == Handle)
                {
                    m_rootNodes.erase(It);
                    break;
                }
            }
        }
    }
    for (auto& [Type, Storage] : m_storages)
    {
        Storage->EndFrame();
    }
    for (const auto& Handle : m_pendingDestroy)
    {
        ObjectRegistry::Instance().Unregister(Handle.Id);
    }
    m_nodePool->EndFrame();
    m_pendingDestroy.clear();
}

void NodeGraph::Clear()
{
    const size_t Budget = m_relevanceBudget;
    for (auto& [Type, Storage] : m_storages)
    {
        Storage->Clear();
    }
    if (m_nodePool)
    {
        m_nodePool->ForEachAll([&](const NodeHandle& Handle, BaseNode&) {
            ObjectRegistry::Instance().Unregister(Handle.Id);
        });
        m_nodePool->Clear();
    }
    m_storages.clear();
    m_rootNodes.clear();
    m_pendingDestroy.clear();
    m_relevanceCursor = 0;
    m_relevanceBudget = Budget;
}

void NodeGraph::TickComponents(NodeHandle Owner, float DeltaSeconds)
{
    auto* Node = Owner.Borrowed();
    if (!Node)
    {
        return;
    }
    for (const auto& Type : Node->ComponentTypes())
    {
        auto It = m_storages.find(Type);
        if (It != m_storages.end())
        {
            It->second->TickComponent(Owner, DeltaSeconds);
        }
    }
}

void NodeGraph::FixedTickComponents(NodeHandle Owner, float DeltaSeconds)
{
    auto* Node = Owner.Borrowed();
    if (!Node)
    {
        return;
    }
    for (const auto& Type : Node->ComponentTypes())
    {
        auto It = m_storages.find(Type);
        if (It != m_storages.end())
        {
            It->second->FixedTickComponent(Owner, DeltaSeconds);
        }
    }
}

void NodeGraph::LateTickComponents(NodeHandle Owner, float DeltaSeconds)
{
    auto* Node = Owner.Borrowed();
    if (!Node)
    {
        return;
    }
    for (const auto& Type : Node->ComponentTypes())
    {
        auto It = m_storages.find(Type);
        if (It != m_storages.end())
        {
            It->second->LateTickComponent(Owner, DeltaSeconds);
        }
    }
}

void NodeGraph::EvaluateRelevance()
{
    size_t Evaluated = 0;
    m_nodePool->ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (m_relevanceBudget > 0 && Evaluated >= m_relevanceBudget)
        {
            return;
        }
        if (!HasComponent<RelevanceComponent>(Handle))
        {
            return;
        }
        auto Result = Component<RelevanceComponent>(Handle);
        if (!Result)
        {
            return;
        }
        auto& Component = *Result;
        const auto* PolicyInfo = RelevancePolicyRegistry::Find(Component.PolicyId());
        if (!PolicyInfo || !Component.PolicyData())
        {
            return;
        }
        RelevanceContext Context{Handle, *this};
        const bool Active = PolicyInfo->Evaluate(Component.PolicyData().get(), Context);
        Component.Active(Active);
        ++Evaluated;
    });
}

bool NodeGraph::IsNodeActive(NodeHandle Handle)
{
    if (m_nodePool->IsPendingDestroy(Handle))
    {
        return false;
    }
    auto* Node = Handle.Borrowed();
    if (!Node || !Node->Active())
    {
        return false;
    }
    if (!HasComponent<RelevanceComponent>(Handle))
    {
        return true;
    }
    auto Result = Component<RelevanceComponent>(Handle);
    if (!Result)
    {
        return true;
    }
    return Result->Active();
}

void NodeGraph::RegisterComponentOnNode(BaseNode& Node, const TypeId& Type)
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

    for (const auto& Existing : Node.ComponentTypes())
    {
        if (Existing == Type)
        {
            return;
        }
    }
    Node.ComponentTypes().push_back(Type);
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
    for (auto It = Types.begin(); It != Types.end(); ++It)
    {
        if (*It == Type)
        {
            Types.erase(It);
            break;
        }
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
    });
}

} // namespace SnAPI::GameFramework
