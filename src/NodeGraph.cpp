#include "NodeGraph.h"
#include "Profiling.h"

#include <algorithm>

namespace SnAPI::GameFramework
{

NodeGraph::NodeGraph(NodeGraph&& Other) noexcept
    : BaseNode(std::move(Other))
    , m_nodePool(std::move(Other.m_nodePool))
    , m_storages(std::move(Other.m_storages))
    , m_rootNodes(std::move(Other.m_rootNodes))
    , m_pendingDestroy(std::move(Other.m_pendingDestroy))
    , m_pendingDestroyIds(std::move(Other.m_pendingDestroyIds))
    , m_relevanceCursor(Other.m_relevanceCursor)
    , m_relevanceBudget(Other.m_relevanceBudget)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    RebindOwnerGraph();
    Other.m_relevanceCursor = 0;
    Other.m_relevanceBudget = 0;
}

NodeGraph::~NodeGraph()
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    Clear();
}

NodeGraph& NodeGraph::operator=(NodeGraph&& Other) noexcept
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    if (this == &Other)
    {
        return *this;
    }
    BaseNode::operator=(std::move(Other));
    m_nodePool = std::move(Other.m_nodePool);
    m_storages = std::move(Other.m_storages);
    m_rootNodes = std::move(Other.m_rootNodes);
    m_pendingDestroy = std::move(Other.m_pendingDestroy);
    m_pendingDestroyIds = std::move(Other.m_pendingDestroyIds);
    m_relevanceCursor = Other.m_relevanceCursor;
    m_relevanceBudget = Other.m_relevanceBudget;
    RebindOwnerGraph();
    Other.m_relevanceCursor = 0;
    Other.m_relevanceBudget = 0;
    return *this;
}

void NodeGraph::Tick(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.EvaluateRelevance", "SceneGraph");
        EvaluateRelevance();
    }

    {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.Tick.RootTraversal", "SceneGraph");
        for (const auto& Handle : m_rootNodes)
        {
            BaseNode* Node = nullptr;
            {
                SNAPI_GF_PROFILE_SCOPE("NodeGraph.Tick.ResolveRoot", "SceneGraph");
                Node = m_nodePool->Borrowed(Handle);
            }
            if (!Node)
            {
                continue;
            }

            SNAPI_GF_PROFILE_SCOPE("NodeGraph.TickTree", "SceneGraph");
            Node->TickTree(DeltaSeconds);
        }
    }
}

void NodeGraph::FixedTick(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.FixedTick.RootTraversal", "SceneGraph");
        for (const auto& Handle : m_rootNodes)
        {
            BaseNode* Node = nullptr;
            {
                SNAPI_GF_PROFILE_SCOPE("NodeGraph.FixedTick.ResolveRoot", "SceneGraph");
                Node = m_nodePool->Borrowed(Handle);
            }
            if (!Node)
            {
                continue;
            }

            SNAPI_GF_PROFILE_SCOPE("NodeGraph.FixedTickTree", "SceneGraph");
            Node->FixedTickTree(DeltaSeconds);
        }
    }
}

void NodeGraph::LateTick(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.LateTick.RootTraversal", "SceneGraph");
        for (const auto& Handle : m_rootNodes)
        {
            BaseNode* Node = nullptr;
            {
                SNAPI_GF_PROFILE_SCOPE("NodeGraph.LateTick.ResolveRoot", "SceneGraph");
                Node = m_nodePool->Borrowed(Handle);
            }
            if (!Node)
            {
                continue;
            }

            SNAPI_GF_PROFILE_SCOPE("NodeGraph.LateTickTree", "SceneGraph");
            Node->LateTickTree(DeltaSeconds);
        }
    }
}

void NodeGraph::EndFrame()
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.DestroyPending", "SceneGraph");
    for (const auto& Handle : m_pendingDestroy)
    {
        auto* Node = m_nodePool->Borrowed(Handle);
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
    }

    {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.ComponentEndFrame", "SceneGraph");
    for (auto& [Type, Storage] : m_storages)
    {
        (void)Type;
        Storage->EndFrame();
    }
    }

    {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.RegistryUnregister", "SceneGraph");
    for (const auto& Handle : m_pendingDestroy)
    {
        ObjectRegistry::Instance().Unregister(Handle.Id);
    }
    }

    {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.NodePoolEndFrame", "SceneGraph");
    m_nodePool->EndFrame();
    }
    m_pendingDestroy.clear();
    m_pendingDestroyIds.clear();
}

void NodeGraph::Clear()
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    const size_t Budget = m_relevanceBudget;
    {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.StorageClear", "SceneGraph");
    for (auto& [Type, Storage] : m_storages)
    {
        (void)Type;
        Storage->Clear();
    }
    }

    if (m_nodePool)
    {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.NodePoolClear", "SceneGraph");
        m_nodePool->ForEachAll([&](const NodeHandle& Handle, BaseNode&) {
            ObjectRegistry::Instance().Unregister(Handle.Id);
        });
        m_nodePool->Clear();
    }
    m_storages.clear();
    m_rootNodes.clear();
    m_pendingDestroy.clear();
    m_pendingDestroyIds.clear();
    m_relevanceCursor = 0;
    m_relevanceBudget = Budget;
}

void NodeGraph::TickComponents(NodeHandle Owner, float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    auto* Node = m_nodePool->Borrowed(Owner);
    if (!Node)
    {
        return;
    }

    SNAPI_GF_PROFILE_SCOPE("NodeGraph.Tick.ComponentTraversal", "SceneGraph");
    for (const auto& Type : Node->ComponentTypes())
    {
        auto It = m_storages.end();
        {
            SNAPI_GF_PROFILE_SCOPE("NodeGraph.Tick.ComponentLookup", "SceneGraph");
            It = m_storages.find(Type);
        }
        if (It == m_storages.end())
        {
            continue;
        }

        {
            SNAPI_GF_PROFILE_SCOPE("NodeGraph.Tick.ComponentDispatch", "SceneGraph");
            It->second->TickComponent(Owner, DeltaSeconds);
        }
    }
}

void NodeGraph::FixedTickComponents(NodeHandle Owner, float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    auto* Node = m_nodePool->Borrowed(Owner);
    if (!Node)
    {
        return;
    }
    SNAPI_GF_PROFILE_SCOPE("NodeGraph.FixedTick.ComponentTraversal", "SceneGraph");
    for (const auto& Type : Node->ComponentTypes())
    {
        auto It = m_storages.end();
        {
            SNAPI_GF_PROFILE_SCOPE("NodeGraph.FixedTick.ComponentLookup", "SceneGraph");
            It = m_storages.find(Type);
        }
        if (It == m_storages.end())
        {
            continue;
        }

        {
            SNAPI_GF_PROFILE_SCOPE("NodeGraph.FixedTick.ComponentDispatch", "SceneGraph");
            It->second->FixedTickComponent(Owner, DeltaSeconds);
        }
    }
}

void NodeGraph::LateTickComponents(NodeHandle Owner, float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    auto* Node = m_nodePool->Borrowed(Owner);
    if (!Node)
    {
        return;
    }

    SNAPI_GF_PROFILE_SCOPE("NodeGraph.LateTick.ComponentTraversal", "SceneGraph");
    for (const auto& Type : Node->ComponentTypes())
    {
        auto It = m_storages.end();
        {
            SNAPI_GF_PROFILE_SCOPE("NodeGraph.LateTick.ComponentLookup", "SceneGraph");
            It = m_storages.find(Type);
        }
        if (It == m_storages.end())
        {
            continue;
        }

        {
            SNAPI_GF_PROFILE_SCOPE("NodeGraph.LateTick.ComponentDispatch", "SceneGraph");
            It->second->LateTickComponent(Owner, DeltaSeconds);
        }
    }
}

void NodeGraph::EvaluateRelevance()
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    size_t Evaluated = 0;
    m_nodePool->ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.EvaluateRelevance.Node", "SceneGraph");
        if (m_relevanceBudget > 0 && Evaluated >= m_relevanceBudget)
        {
            return;
        }

        const auto& NodeTypes = Node.ComponentTypes();
        if (std::find(NodeTypes.begin(), NodeTypes.end(), RelevanceType) == NodeTypes.end())
        {
            return;
        }

        auto* RelevanceStorage = Storage(RelevanceType);
        if (!RelevanceStorage)
        {
            return;
        }

        auto* Component = static_cast<RelevanceComponent*>(RelevanceStorage->Borrowed(Handle));
        if (!Component)
        {
            return;
        }

        const auto* PolicyInfo = RelevancePolicyRegistry::Find(Component->PolicyId());
        if (!PolicyInfo || !Component->PolicyData())
        {
            return;
        }
        RelevanceContext Context{Handle, *this};
        const bool Active = PolicyInfo->Evaluate(Component->PolicyData().get(), Context);
        Component->Active(Active);
        ++Evaluated;
    });
}

bool NodeGraph::IsNodeActive(NodeHandle Handle)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    auto* Node = m_nodePool->Borrowed(Handle);
    if (!Node)
    {
        return false;
    }
    return IsNodeActive(*Node);
}

bool NodeGraph::IsNodeActive(const BaseNode& Node) const
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
    if (m_pendingDestroyIds.contains(Node.Id()))
    {
        return false;
    }
    if (!Node.Active())
    {
        return false;
    }

    static const TypeId RelevanceType = StaticTypeId<RelevanceComponent>();
    const auto& NodeTypes = Node.ComponentTypes();
    if (std::find(NodeTypes.begin(), NodeTypes.end(), RelevanceType) == NodeTypes.end())
    {
        return true;
    }

    const auto* Relevance = [&]() -> const RelevanceComponent* {
        SNAPI_GF_PROFILE_SCOPE("NodeGraph.IsNodeActive.ResolveRelevance", "SceneGraph");
        return static_cast<const RelevanceComponent*>(BorrowedComponent(Node.Handle(), RelevanceType));
    }();
    if (!Relevance)
    {
        return true;
    }
    return Relevance->Active();
}

void NodeGraph::RegisterComponentOnNode(BaseNode& Node, const TypeId& Type)
{
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
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
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
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
    SNAPI_GF_PROFILE_FUNCTION("SceneGraph");
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
