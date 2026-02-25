#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "World.h"
#include "WorldEcsRuntime.h"
#include "TypeRegistration.h"

using namespace SnAPI::GameFramework;

namespace
{
[[maybe_unused]] const bool kBuiltinsRegistered = [] {
    RegisterBuiltinTypes();
    return true;
}();

struct THighPriorityRuntimeType final : TRuntimeTickCRTP<THighPriorityRuntimeType>
{
    static constexpr const char* kTypeName = "Tests::THighPriorityRuntimeType";
    static constexpr int kTickPriority = -10;

    explicit THighPriorityRuntimeType(std::vector<int>* InLog = nullptr)
        : Log(InLog)
    {
    }

    void TickImpl(IWorld&, float)
    {
        if (Log)
        {
            Log->push_back(1);
        }
    }

    std::vector<int>* Log = nullptr;
};

struct TLowPriorityRuntimeType final : TRuntimeTickCRTP<TLowPriorityRuntimeType>
{
    static constexpr const char* kTypeName = "Tests::TLowPriorityRuntimeType";
    static constexpr int kTickPriority = 25;

    explicit TLowPriorityRuntimeType(std::vector<int>* InLog = nullptr)
        : Log(InLog)
    {
    }

    void TickImpl(IWorld&, float)
    {
        if (Log)
        {
            Log->push_back(2);
        }
    }

    std::vector<int>* Log = nullptr;
};

struct THandleRuntimeType final : TRuntimeTickCRTP<THandleRuntimeType>
{
    static constexpr const char* kTypeName = "Tests::THandleRuntimeType";

    explicit THandleRuntimeType(int InValue = 0)
        : Value(InValue)
    {
    }

    int Value = 0;
};

struct TPolymorphicRuntimeType : TRuntimeTickCRTP<TPolymorphicRuntimeType>
{
    static constexpr const char* kTypeName = "Tests::TPolymorphicRuntimeType";
    virtual ~TPolymorphicRuntimeType() = default;
};

struct TPhaseRuntimeType final : TRuntimeTickCRTP<TPhaseRuntimeType>
{
    static constexpr const char* kTypeName = "Tests::TPhaseRuntimeType";

    struct Counters
    {
        int Pre = 0;
        int Tick = 0;
        int Fixed = 0;
        int Late = 0;
        int Post = 0;
    };

    explicit TPhaseRuntimeType(Counters* InCounters = nullptr)
        : Counts(InCounters)
    {
    }

    void PreTickImpl(IWorld&, float)
    {
        if (Counts)
        {
            ++Counts->Pre;
        }
    }

    void TickImpl(IWorld&, float)
    {
        if (Counts)
        {
            ++Counts->Tick;
        }
    }

    void FixedTickImpl(IWorld&, float)
    {
        if (Counts)
        {
            ++Counts->Fixed;
        }
    }

    void LateTickImpl(IWorld&, float)
    {
        if (Counts)
        {
            ++Counts->Late;
        }
    }

    void PostTickImpl(IWorld&, float)
    {
        if (Counts)
        {
            ++Counts->Post;
        }
    }

    Counters* Counts = nullptr;
};

struct TAttachedRuntimeComponent final : TRuntimeTickCRTP<TAttachedRuntimeComponent>
{
    static constexpr const char* kTypeName = "Tests::TAttachedRuntimeComponent";

    explicit TAttachedRuntimeComponent(int InValue = 0)
        : Value(InValue)
    {
    }

    int Value = 0;
};

struct TDualTickEntryRuntimeType final : TRuntimeTickCRTP<TDualTickEntryRuntimeType>
{
    static constexpr const char* kTypeName = "Tests::TDualTickEntryRuntimeType";

    explicit TDualTickEntryRuntimeType(int* InTickCalls = nullptr, int* InTickImplCalls = nullptr)
        : TickCalls(InTickCalls)
        , TickImplCalls(InTickImplCalls)
    {
    }

    // Intentionally provided to ensure ECS dispatch does not call this path.
    void Tick(IWorld&, float)
    {
        if (TickCalls)
        {
            ++(*TickCalls);
        }
    }

    void TickImpl(IWorld&, float)
    {
        if (TickImplCalls)
        {
            ++(*TickImplCalls);
        }
    }

    int* TickCalls = nullptr;
    int* TickImplCalls = nullptr;
};

struct TDefaultRuntimeComponent final : TRuntimeTickCRTP<TDefaultRuntimeComponent>
{
    static constexpr const char* kTypeName = "Tests::TDefaultRuntimeComponent";

    int Value = 77;
};

static_assert(RuntimeTickType<THighPriorityRuntimeType>);
static_assert(RuntimeTickType<TLowPriorityRuntimeType>);
static_assert(RuntimeTickType<THandleRuntimeType>);
static_assert(RuntimeTickType<TPhaseRuntimeType>);
static_assert(RuntimeTickType<TAttachedRuntimeComponent>);
static_assert(RuntimeTickType<TDualTickEntryRuntimeType>);
static_assert(RuntimeTickType<TDefaultRuntimeComponent>);
static_assert(!NonPolymorphicRuntimeType<TPolymorphicRuntimeType>);
static_assert(!RuntimeTickType<TPolymorphicRuntimeType>);

void CollectRuntimeChild(void* UserData, const RuntimeNodeHandle ChildHandle)
{
    auto* Children = static_cast<std::vector<RuntimeNodeHandle>*>(UserData);
    if (!Children)
    {
        return;
    }
    Children->push_back(ChildHandle);
}
} // namespace

TEST_CASE("World ECS runtime ticks storages by static priority")
{
    World WorldInstance{"RuntimePriorityWorld"};
    auto& Runtime = WorldInstance.EcsRuntime();

    std::vector<int> TickLog{};

    auto& HighStorage = Runtime.Storage<THighPriorityRuntimeType>();
    auto& LowStorage = Runtime.Storage<TLowPriorityRuntimeType>();

    auto HighCreate = HighStorage.Create(WorldInstance, &TickLog);
    REQUIRE(HighCreate.has_value());

    auto LowCreate = LowStorage.Create(WorldInstance, &TickLog);
    REQUIRE(LowCreate.has_value());

    Runtime.Tick(WorldInstance, 1.0f / 60.0f);

    REQUIRE(TickLog.size() == 2);
    REQUIRE(TickLog[0] == 1);
    REQUIRE(TickLog[1] == 2);
}

TEST_CASE("World ECS runtime handles reject stale generations")
{
    World WorldInstance{"RuntimeHandleWorld"};
    auto& Runtime = WorldInstance.EcsRuntime();
    auto& Storage = Runtime.Storage<THandleRuntimeType>();

    auto HandleAResult = Storage.Create(WorldInstance, 7);
    REQUIRE(HandleAResult.has_value());
    auto HandleA = *HandleAResult;

    auto* InstanceA = Storage.Resolve(HandleA);
    REQUIRE(InstanceA != nullptr);
    REQUIRE(InstanceA->Value == 7);

    REQUIRE(Storage.Destroy(WorldInstance, HandleA));

    auto HandleBResult = Storage.CreateWithId(WorldInstance, HandleA.Id, 11);
    REQUIRE(HandleBResult.has_value());
    auto HandleB = *HandleBResult;

    REQUIRE(HandleA.Generation != HandleB.Generation);
    REQUIRE(Storage.Resolve(HandleA) == nullptr);

    auto* InstanceB = Storage.Resolve(HandleB);
    REQUIRE(InstanceB != nullptr);
    REQUIRE(InstanceB->Value == 11);

    auto* SlowResolved = Storage.ResolveSlowById(HandleB.Id);
    REQUIRE(SlowResolved != nullptr);
    REQUIRE(SlowResolved->Value == 11);
}

TEST_CASE("World runtime components attach to runtime nodes and resolve by type")
{
    World WorldInstance{"RuntimeComponentAttachWorld"};
    const TypeId NodeType = StaticTypeId<BaseNode>();
    auto NodeResult = WorldInstance.CreateRuntimeNode("RuntimeNode", NodeType);
    REQUIRE(NodeResult.has_value());

    const RuntimeNodeHandle Owner = *NodeResult;
    auto AddResult = WorldInstance.EcsRuntime().AddComponent<TAttachedRuntimeComponent>(WorldInstance, Owner, 42);
    REQUIRE(AddResult.has_value());

    const TypeId ComponentType = StaticTypeId<TAttachedRuntimeComponent>();
    REQUIRE(WorldInstance.HasRuntimeComponent(Owner, ComponentType));

    auto ComponentHandleResult = WorldInstance.RuntimeComponentByType(Owner, ComponentType);
    REQUIRE(ComponentHandleResult.has_value());

    void* Raw = WorldInstance.ResolveRuntimeComponentRaw(*ComponentHandleResult, ComponentType);
    REQUIRE(Raw != nullptr);
    auto* Component = static_cast<TAttachedRuntimeComponent*>(Raw);
    REQUIRE(Component->Value == 42);

    REQUIRE(WorldInstance.RemoveRuntimeComponent(Owner, ComponentType));
    REQUIRE_FALSE(WorldInstance.HasRuntimeComponent(Owner, ComponentType));
}

TEST_CASE("World runtime components can be added by TypeId when storage is registered")
{
    World WorldInstance{"RuntimeComponentErasedAddWorld"};
    const TypeId NodeType = StaticTypeId<BaseNode>();
    auto NodeResult = WorldInstance.CreateRuntimeNode("RuntimeNode", NodeType);
    REQUIRE(NodeResult.has_value());
    const RuntimeNodeHandle Owner = *NodeResult;

    (void)WorldInstance.EcsRuntime().Storage<TDefaultRuntimeComponent>();
    const TypeId ComponentType = StaticTypeId<TDefaultRuntimeComponent>();

    auto AddResult = WorldInstance.AddRuntimeComponent(Owner, ComponentType);
    REQUIRE(AddResult.has_value());
    REQUIRE(WorldInstance.HasRuntimeComponent(Owner, ComponentType));

    auto ComponentHandleResult = WorldInstance.RuntimeComponentByType(Owner, ComponentType);
    REQUIRE(ComponentHandleResult.has_value());

    const void* Raw = WorldInstance.ResolveRuntimeComponentRaw(*ComponentHandleResult, ComponentType);
    REQUIRE(Raw != nullptr);
    const auto* Component = static_cast<const TDefaultRuntimeComponent*>(Raw);
    REQUIRE(Component->Value == 77);
}

TEST_CASE("Destroying runtime node recursively destroys attached runtime components")
{
    World WorldInstance{"RuntimeComponentDestroyWorld"};
    const TypeId NodeType = StaticTypeId<BaseNode>();

    auto ParentResult = WorldInstance.CreateRuntimeNode("Parent", NodeType);
    auto ChildResult = WorldInstance.CreateRuntimeNode("Child", NodeType);
    REQUIRE(ParentResult.has_value());
    REQUIRE(ChildResult.has_value());
    const RuntimeNodeHandle Parent = *ParentResult;
    const RuntimeNodeHandle Child = *ChildResult;
    REQUIRE(WorldInstance.AttachRuntimeChild(Parent, Child));

    auto ParentComponent = WorldInstance.EcsRuntime().AddComponent<TAttachedRuntimeComponent>(WorldInstance, Parent, 1);
    auto ChildComponent = WorldInstance.EcsRuntime().AddComponent<TAttachedRuntimeComponent>(WorldInstance, Child, 2);
    REQUIRE(ParentComponent.has_value());
    REQUIRE(ChildComponent.has_value());

    const TypeId ComponentType = StaticTypeId<TAttachedRuntimeComponent>();
    auto ParentHandleResult = WorldInstance.RuntimeComponentByType(Parent, ComponentType);
    auto ChildHandleResult = WorldInstance.RuntimeComponentByType(Child, ComponentType);
    REQUIRE(ParentHandleResult.has_value());
    REQUIRE(ChildHandleResult.has_value());

    REQUIRE(WorldInstance.DestroyRuntimeNode(Parent));
    REQUIRE_FALSE(WorldInstance.RuntimeNodeById(Parent.Id).has_value());
    REQUIRE_FALSE(WorldInstance.RuntimeNodeById(Child.Id).has_value());

    REQUIRE(WorldInstance.ResolveRuntimeComponentRaw(*ParentHandleResult, ComponentType) == nullptr);
    REQUIRE(WorldInstance.ResolveRuntimeComponentRaw(*ChildHandleResult, ComponentType) == nullptr);
}

TEST_CASE("BaseNode runtime component helpers route through world ECS runtime")
{
    World WorldInstance{"RuntimeNodeHelperWorld"};

    auto NodeResult = WorldInstance.CreateNode<BaseNode>("RuntimeOwner");
    REQUIRE(NodeResult.has_value());

    BaseNode* Owner = NodeResult->Borrowed();
    REQUIRE(Owner != nullptr);

    auto AddResult = Owner->AddRuntimeComponent<TAttachedRuntimeComponent>(99);
    REQUIRE(AddResult.has_value());
    REQUIRE(Owner->HasRuntimeComponent<TAttachedRuntimeComponent>());

    auto ComponentResult = Owner->RuntimeComponent<TAttachedRuntimeComponent>();
    REQUIRE(ComponentResult);
    REQUIRE(ComponentResult->Value == 99);

    REQUIRE(Owner->RemoveRuntimeComponent<TAttachedRuntimeComponent>());
    REQUIRE_FALSE(Owner->HasRuntimeComponent<TAttachedRuntimeComponent>());
}

TEST_CASE("DestroyNode end-frame flush destroys BaseNode runtime ECS attachments")
{
    World WorldInstance{"RuntimeNodeDestroyFlushWorld"};

    auto NodeResult = WorldInstance.CreateNode<BaseNode>("RuntimeOwner");
    REQUIRE(NodeResult.has_value());

    BaseNode* Owner = NodeResult->Borrowed();
    REQUIRE(Owner != nullptr);
    REQUIRE(Owner->AddRuntimeComponent<TAttachedRuntimeComponent>(13).has_value());

    const RuntimeNodeHandle RuntimeOwner = Owner->RuntimeNode();
    REQUIRE_FALSE(RuntimeOwner.IsNull());

    const TypeId ComponentType = StaticTypeId<TAttachedRuntimeComponent>();
    auto ComponentHandleResult = WorldInstance.RuntimeComponentByType(RuntimeOwner, ComponentType);
    REQUIRE(ComponentHandleResult.has_value());

    REQUIRE(WorldInstance.DestroyNode(*NodeResult));
    WorldInstance.EndFrame();

    REQUIRE_FALSE(WorldInstance.RuntimeNodeById(NodeResult->Id).has_value());
    REQUIRE(WorldInstance.ResolveRuntimeComponentRaw(*ComponentHandleResult, ComponentType) == nullptr);
}

TEST_CASE("World frame phases drive ECS runtime storage phases")
{
    World WorldInstance{"RuntimeWorldTickBridge"};
    auto& Storage = WorldInstance.EcsRuntime().Storage<TPhaseRuntimeType>();

    TPhaseRuntimeType::Counters Counts{};
    auto HandleResult = Storage.Create(WorldInstance, &Counts);
    REQUIRE(HandleResult.has_value());

    WorldInstance.Tick(1.0f / 60.0f);
    WorldInstance.FixedTick(1.0f / 60.0f);
    WorldInstance.LateTick(1.0f / 60.0f);

    REQUIRE(Counts.Pre == 1);
    REQUIRE(Counts.Tick == 1);
    REQUIRE(Counts.Fixed == 1);
    REQUIRE(Counts.Late == 1);
    REQUIRE(Counts.Post == 1);
}

TEST_CASE("World ECS runtime tick dispatch executes only TickImpl once per object")
{
    World WorldInstance{"RuntimeSingleTickEntryWorld"};
    auto& Storage = WorldInstance.EcsRuntime().Storage<TDualTickEntryRuntimeType>();

    int TickCalls = 0;
    int TickImplCalls = 0;
    auto HandleResult = Storage.Create(WorldInstance, &TickCalls, &TickImplCalls);
    REQUIRE(HandleResult.has_value());

    WorldInstance.Tick(1.0f / 60.0f);

    REQUIRE(TickCalls == 0);
    REQUIRE(TickImplCalls == 1);
}

TEST_CASE("World can tick ECS runtime when runtime phases are enabled")
{
    World WorldInstance{"RuntimeWorldEcsOnly"};
    WorldExecutionProfile Profile = WorldInstance.ExecutionProfile();
    Profile.TickEcsRuntime = true;
    WorldInstance.SetExecutionProfile(Profile);

    auto& Storage = WorldInstance.EcsRuntime().Storage<TPhaseRuntimeType>();

    TPhaseRuntimeType::Counters Counts{};
    auto HandleResult = Storage.Create(WorldInstance, &Counts);
    REQUIRE(HandleResult.has_value());

    WorldInstance.Tick(1.0f / 60.0f);
    WorldInstance.FixedTick(1.0f / 60.0f);
    WorldInstance.LateTick(1.0f / 60.0f);

    REQUIRE(Counts.Pre == 1);
    REQUIRE(Counts.Tick == 1);
    REQUIRE(Counts.Fixed == 1);
    REQUIRE(Counts.Late == 1);
    REQUIRE(Counts.Post == 1);
}

TEST_CASE("World runtime node hierarchy tracks parents, children, and roots")
{
    World WorldInstance{"RuntimeHierarchyWorld"};
    const TypeId NodeType = StaticTypeId<BaseNode>();

    auto ParentResult = WorldInstance.CreateRuntimeNode("Parent", NodeType);
    auto ChildResult = WorldInstance.CreateRuntimeNode("Child", NodeType);
    REQUIRE(ParentResult.has_value());
    REQUIRE(ChildResult.has_value());

    const RuntimeNodeHandle Parent = *ParentResult;
    const RuntimeNodeHandle Child = *ChildResult;

    auto Roots = WorldInstance.RuntimeRoots();
    REQUIRE(Roots.size() == 2);

    REQUIRE(WorldInstance.AttachRuntimeChild(Parent, Child));

    const RuntimeNodeHandle ResolvedParent = WorldInstance.RuntimeParent(Child);
    REQUIRE(ResolvedParent == Parent);

    const auto Children = WorldInstance.RuntimeChildren(Parent);
    REQUIRE(Children.size() == 1);
    REQUIRE(Children[0] == Child);

    Roots = WorldInstance.RuntimeRoots();
    REQUIRE(Roots.size() == 1);
    REQUIRE(Roots[0] == Parent);

    REQUIRE(WorldInstance.DetachRuntimeChild(Child));

    REQUIRE(WorldInstance.RuntimeParent(Child).IsNull());
    Roots = WorldInstance.RuntimeRoots();
    REQUIRE(Roots.size() == 2);
}

TEST_CASE("World runtime child iterator matches snapshot children API")
{
    World WorldInstance{"RuntimeChildIteratorWorld"};
    const TypeId NodeType = StaticTypeId<BaseNode>();

    auto ParentResult = WorldInstance.CreateRuntimeNode("Parent", NodeType);
    auto ChildAResult = WorldInstance.CreateRuntimeNode("ChildA", NodeType);
    auto ChildBResult = WorldInstance.CreateRuntimeNode("ChildB", NodeType);
    REQUIRE(ParentResult.has_value());
    REQUIRE(ChildAResult.has_value());
    REQUIRE(ChildBResult.has_value());

    const RuntimeNodeHandle Parent = *ParentResult;
    const RuntimeNodeHandle ChildA = *ChildAResult;
    const RuntimeNodeHandle ChildB = *ChildBResult;
    REQUIRE(WorldInstance.AttachRuntimeChild(Parent, ChildA));
    REQUIRE(WorldInstance.AttachRuntimeChild(Parent, ChildB));

    std::vector<RuntimeNodeHandle> IteratedChildren{};
    WorldInstance.EcsRuntime().Nodes().ForEachChild(Parent, [&](const RuntimeNodeHandle ChildHandle) {
        IteratedChildren.push_back(ChildHandle);
    });

    const auto SnapshotChildren = WorldInstance.RuntimeChildren(Parent);
    REQUIRE(IteratedChildren == SnapshotChildren);
}

TEST_CASE("IWorld runtime child callback API matches snapshot children")
{
    World WorldInstance{"RuntimeWorldChildCallbackWorld"};
    const TypeId NodeType = StaticTypeId<BaseNode>();

    auto ParentResult = WorldInstance.CreateRuntimeNode("Parent", NodeType);
    auto ChildAResult = WorldInstance.CreateRuntimeNode("ChildA", NodeType);
    auto ChildBResult = WorldInstance.CreateRuntimeNode("ChildB", NodeType);
    REQUIRE(ParentResult.has_value());
    REQUIRE(ChildAResult.has_value());
    REQUIRE(ChildBResult.has_value());

    const RuntimeNodeHandle Parent = *ParentResult;
    const RuntimeNodeHandle ChildA = *ChildAResult;
    const RuntimeNodeHandle ChildB = *ChildBResult;
    REQUIRE(WorldInstance.AttachRuntimeChild(Parent, ChildA));
    REQUIRE(WorldInstance.AttachRuntimeChild(Parent, ChildB));

    std::vector<RuntimeNodeHandle> CallbackChildren{};
    WorldInstance.ForEachRuntimeChild(Parent, &CollectRuntimeChild, &CallbackChildren);

    const auto SnapshotChildren = WorldInstance.RuntimeChildren(Parent);
    REQUIRE(CallbackChildren == SnapshotChildren);
}

TEST_CASE("World runtime node destroy recursively removes descendants")
{
    World WorldInstance{"RuntimeDestroyWorld"};
    const TypeId NodeType = StaticTypeId<BaseNode>();

    auto RootResult = WorldInstance.CreateRuntimeNode("Root", NodeType);
    auto ChildResult = WorldInstance.CreateRuntimeNode("Child", NodeType);
    auto GrandChildResult = WorldInstance.CreateRuntimeNode("GrandChild", NodeType);
    REQUIRE(RootResult.has_value());
    REQUIRE(ChildResult.has_value());
    REQUIRE(GrandChildResult.has_value());

    const RuntimeNodeHandle Root = *RootResult;
    const RuntimeNodeHandle Child = *ChildResult;
    const RuntimeNodeHandle GrandChild = *GrandChildResult;

    REQUIRE(WorldInstance.AttachRuntimeChild(Root, Child));
    REQUIRE(WorldInstance.AttachRuntimeChild(Child, GrandChild));

    REQUIRE(WorldInstance.DestroyRuntimeNode(Root));

    REQUIRE_FALSE(WorldInstance.RuntimeNodeById(Root.Id).has_value());
    REQUIRE_FALSE(WorldInstance.RuntimeNodeById(Child.Id).has_value());
    REQUIRE_FALSE(WorldInstance.RuntimeNodeById(GrandChild.Id).has_value());
    REQUIRE(WorldInstance.RuntimeRoots().empty());
}

TEST_CASE("World node graph create and attach mirror into runtime hierarchy")
{
    World WorldInstance{"RuntimeMirrorWorld"};

    auto ParentResult = WorldInstance.CreateNode<BaseNode>("Parent");
    auto ChildResult = WorldInstance.CreateNode<BaseNode>("Child");
    REQUIRE(ParentResult.has_value());
    REQUIRE(ChildResult.has_value());

    const NodeHandle ParentNode = *ParentResult;
    const NodeHandle ChildNode = *ChildResult;

    auto ParentRuntimeResult = WorldInstance.RuntimeNodeById(ParentNode.Id);
    auto ChildRuntimeResult = WorldInstance.RuntimeNodeById(ChildNode.Id);
    REQUIRE(ParentRuntimeResult.has_value());
    REQUIRE(ChildRuntimeResult.has_value());

    const RuntimeNodeHandle ParentRuntime = *ParentRuntimeResult;
    const RuntimeNodeHandle ChildRuntime = *ChildRuntimeResult;

    REQUIRE(WorldInstance.AttachChild(ParentNode, ChildNode));
    REQUIRE(WorldInstance.RuntimeParent(ChildRuntime) == ParentRuntime);

    const auto RuntimeChildren = WorldInstance.RuntimeChildren(ParentRuntime);
    REQUIRE(RuntimeChildren.size() == 1);
    REQUIRE(RuntimeChildren[0] == ChildRuntime);
}

TEST_CASE("Detached levels cannot create nodes until bound to a world")
{
    World WorldInstance{"DetachedGraphBindWorld"};
    Level DetachedGraph;

    auto RootResult = DetachedGraph.CreateNode<BaseNode>("Root");
    REQUIRE_FALSE(RootResult.has_value());

    DetachedGraph.World(&WorldInstance);

    RootResult = DetachedGraph.CreateNode<BaseNode>("Root");
    auto LeafResult = DetachedGraph.CreateNode<BaseNode>("Leaf");
    REQUIRE(RootResult.has_value());
    REQUIRE(LeafResult.has_value());

    const NodeHandle RootNode = *RootResult;
    const NodeHandle LeafNode = *LeafResult;

    auto RootRuntimeResult = WorldInstance.RuntimeNodeById(RootNode.Id);
    auto LeafRuntimeResult = WorldInstance.RuntimeNodeById(LeafNode.Id);
    REQUIRE(RootRuntimeResult.has_value());
    REQUIRE(LeafRuntimeResult.has_value());
}

TEST_CASE("World-owned nodes cache and resolve runtime handles")
{
    World WorldInstance{"RuntimeHandleCacheWorld"};
    auto RootResult = WorldInstance.CreateNode<BaseNode>("Root");
    REQUIRE(RootResult.has_value());

    BaseNode* RootNode = RootResult->Borrowed();
    REQUIRE(RootNode != nullptr);
    REQUIRE_FALSE(RootNode->RuntimeNode().IsNull());

    const RuntimeNodeHandle CachedHandle = RootNode->RuntimeNode();
    REQUIRE(WorldInstance.EcsRuntime().Nodes().Resolve(CachedHandle) != nullptr);

    auto RuntimeById = WorldInstance.RuntimeNodeById(RootNode->Id());
    REQUIRE(RuntimeById.has_value());
    REQUIRE(*RuntimeById == CachedHandle);
}
