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

    void Tick(IWorld&, float)
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

    void Tick(IWorld&, float)
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

    void PreTick(IWorld&, float)
    {
        if (Counts)
        {
            ++Counts->Pre;
        }
    }

    void Tick(IWorld&, float)
    {
        if (Counts)
        {
            ++Counts->Tick;
        }
    }

    void FixedTick(IWorld&, float)
    {
        if (Counts)
        {
            ++Counts->Fixed;
        }
    }

    void LateTick(IWorld&, float)
    {
        if (Counts)
        {
            ++Counts->Late;
        }
    }

    void PostTick(IWorld&, float)
    {
        if (Counts)
        {
            ++Counts->Post;
        }
    }

    Counters* Counts = nullptr;
};

struct TPerPhasePriorityFirstRuntimeType final : TRuntimeTickCRTP<TPerPhasePriorityFirstRuntimeType>
{
    static constexpr const char* kTypeName = "Tests::TPerPhasePriorityFirstRuntimeType";
    static constexpr int kPreTickPriority = -50;
    static constexpr int kTickPriority = 50;

    explicit TPerPhasePriorityFirstRuntimeType(std::vector<int>* InLog = nullptr)
        : Log(InLog)
    {
    }

    void PreTick(IWorld&, float)
    {
        if (Log)
        {
            Log->push_back(1);
        }
    }

    void Tick(IWorld&, float)
    {
        if (Log)
        {
            Log->push_back(4);
        }
    }

    std::vector<int>* Log = nullptr;
};

struct TPerPhasePrioritySecondRuntimeType final : TRuntimeTickCRTP<TPerPhasePrioritySecondRuntimeType>
{
    static constexpr const char* kTypeName = "Tests::TPerPhasePrioritySecondRuntimeType";
    static constexpr int kPreTickPriority = 25;
    static constexpr int kTickPriority = -25;

    explicit TPerPhasePrioritySecondRuntimeType(std::vector<int>* InLog = nullptr)
        : Log(InLog)
    {
    }

    void PreTick(IWorld&, float)
    {
        if (Log)
        {
            Log->push_back(2);
        }
    }

    void Tick(IWorld&, float)
    {
        if (Log)
        {
            Log->push_back(3);
        }
    }

    std::vector<int>* Log = nullptr;
};

struct TSelectiveOnCreateRuntimeType final : TRuntimeTickCRTP<TSelectiveOnCreateRuntimeType>
{
    static constexpr const char* kTypeName = "Tests::TSelectiveOnCreateRuntimeType";

    explicit TSelectiveOnCreateRuntimeType(int* InCounter = nullptr)
        : Counter(InCounter)
    {
    }

    void OnCreate(IWorld&)
    {
        if (Counter)
        {
            ++(*Counter);
        }
    }

    int* Counter = nullptr;
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

    explicit TDualTickEntryRuntimeType(int* InTickCalls = nullptr)
        : TickCalls(InTickCalls)
    {
    }

    void Tick(IWorld&, float)
    {
        if (TickCalls)
        {
            ++(*TickCalls);
        }
    }

    int* TickCalls = nullptr;
};

struct TDefaultRuntimeComponent final : TRuntimeTickCRTP<TDefaultRuntimeComponent>
{
    static constexpr const char* kTypeName = "Tests::TDefaultRuntimeComponent";

    int Value = 77;
};

struct TOwnerAwareDestroyComponent final : BaseComponent, ComponentCRTP<TOwnerAwareDestroyComponent>
{
    static constexpr const char* kTypeName = "Tests::TOwnerAwareDestroyComponent";

    explicit TOwnerAwareDestroyComponent(bool* InOwnerWasAlive = nullptr, bool* InWorldWasAlive = nullptr)
        : OwnerWasAlive(InOwnerWasAlive)
        , WorldWasAlive(InWorldWasAlive)
    {
    }

    void OnDestroy()
    {
        if (OwnerWasAlive)
        {
            *OwnerWasAlive = (OwnerNode() != nullptr);
        }
        if (WorldWasAlive)
        {
            *WorldWasAlive = (World() != nullptr);
        }
    }

    bool* OwnerWasAlive = nullptr;
    bool* WorldWasAlive = nullptr;
};

#if defined(WITH_EDITOR) && WITH_EDITOR
struct TEditorTickRuntimeType final : TRuntimeTickCRTP<TEditorTickRuntimeType>
{
    static constexpr const char* kTypeName = "Tests::TEditorTickRuntimeType";

    explicit TEditorTickRuntimeType(int* InTickCalls = nullptr)
        : TickCalls(InTickCalls)
    {
    }

    void EditorTick(IWorld&, float)
    {
        if (TickCalls)
        {
            ++(*TickCalls);
        }
    }

    int* TickCalls = nullptr;
};
#endif

static_assert(RuntimeTickType<THighPriorityRuntimeType>);
static_assert(RuntimeTickType<TLowPriorityRuntimeType>);
static_assert(RuntimeTickType<THandleRuntimeType>);
static_assert(RuntimeTickType<TPhaseRuntimeType>);
static_assert(RuntimeTickType<TPerPhasePriorityFirstRuntimeType>);
static_assert(RuntimeTickType<TPerPhasePrioritySecondRuntimeType>);
static_assert(RuntimeTickType<TAttachedRuntimeComponent>);
static_assert(RuntimeTickType<TDualTickEntryRuntimeType>);
static_assert(RuntimeTickType<TDefaultRuntimeComponent>);
static_assert(RuntimeTickType<TOwnerAwareDestroyComponent>);
static_assert(RuntimeTickType<TSelectiveOnCreateRuntimeType>);
#if defined(WITH_EDITOR) && WITH_EDITOR
static_assert(RuntimeTickType<TEditorTickRuntimeType>);
#endif
static_assert(!NonPolymorphicRuntimeType<TPolymorphicRuntimeType>);
static_assert(!RuntimeTickType<TPolymorphicRuntimeType>);
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

TEST_CASE("World ECS runtime storage supports deferred destroy until end-frame")
{
    World WorldInstance{"RuntimeDeferredDestroyWorld"};
    auto& Runtime = WorldInstance.EcsRuntime();
    auto& Storage = Runtime.Storage<THandleRuntimeType>();

    auto HandleResult = Storage.Create(WorldInstance, 19);
    REQUIRE(HandleResult.has_value());
    auto Handle = *HandleResult;

    auto* BorrowedBeforeDestroy = Storage.Resolve(Handle);
    REQUIRE(BorrowedBeforeDestroy != nullptr);
    REQUIRE(BorrowedBeforeDestroy->Value == 19);

    REQUIRE(Storage.DestroyLater(Handle));
    REQUIRE(Storage.DestroyLater(Handle));

    REQUIRE(Storage.Resolve(Handle) == nullptr);
    REQUIRE(Storage.ResolveIncludingPendingDestroy(Handle) == BorrowedBeforeDestroy);
    REQUIRE_FALSE(Storage.HandleById(Handle.Id).has_value());
    REQUIRE(BorrowedBeforeDestroy->Value == 19);

    Storage.EndFrame(WorldInstance);

    REQUIRE(Storage.ResolveIncludingPendingDestroy(Handle) == nullptr);

    auto RecreatedResult = Storage.CreateWithId(WorldInstance, Handle.Id, 23);
    REQUIRE(RecreatedResult.has_value());
    REQUIRE(RecreatedResult->Generation != Handle.Generation);
}

TEST_CASE("World ECS runtime storage can flush one pending OnCreate by handle")
{
    World WorldInstance{"RuntimeSelectiveOnCreateWorld"};
    auto& Runtime = WorldInstance.EcsRuntime();
    auto& Storage = Runtime.Storage<TSelectiveOnCreateRuntimeType>();

    int CounterA = 0;
    int CounterB = 0;

    ScopedComponentOnCreateSuppression SuppressOnCreate{};
    auto HandleAResult = Storage.Create(WorldInstance, &CounterA);
    auto HandleBResult = Storage.Create(WorldInstance, &CounterB);
    REQUIRE(HandleAResult.has_value());
    REQUIRE(HandleBResult.has_value());
    REQUIRE(CounterA == 0);
    REQUIRE(CounterB == 0);

    REQUIRE(Storage.FlushPendingOnCreate(WorldInstance, *HandleAResult));
    REQUIRE(CounterA == 1);
    REQUIRE(CounterB == 0);

    REQUIRE_FALSE(Storage.FlushPendingOnCreate(WorldInstance, *HandleAResult));
    REQUIRE(CounterA == 1);

    Storage.FlushPendingOnCreate(WorldInstance);
    REQUIRE(CounterA == 1);
    REQUIRE(CounterB == 1);
}

TEST_CASE("World runtime components attach to nodes and resolve by type")
{
    World WorldInstance{"RuntimeComponentAttachWorld"};
    auto NodeResult = WorldInstance.CreateNode<BaseNode>("RuntimeNode");
    REQUIRE(NodeResult.has_value());

    NodeHandle Owner = *NodeResult;
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

TEST_CASE("World runtime component removal hides the attachment immediately and flushes at end-frame")
{
    World WorldInstance{"RuntimeComponentDeferredRemoveWorld"};
    auto NodeResult = WorldInstance.CreateNode<BaseNode>("RuntimeNode");
    REQUIRE(NodeResult.has_value());

    NodeHandle Owner = *NodeResult;
    auto AddResult = WorldInstance.EcsRuntime().AddComponent<TAttachedRuntimeComponent>(WorldInstance, Owner, 17);
    REQUIRE(AddResult.has_value());
    const auto Handle = *AddResult;

    auto& Storage = WorldInstance.EcsRuntime().Storage<TAttachedRuntimeComponent>();
    auto* BorrowedBeforeRemove = Storage.Resolve(Handle);
    REQUIRE(BorrowedBeforeRemove != nullptr);
    REQUIRE(BorrowedBeforeRemove->Value == 17);

    const TypeId ComponentType = StaticTypeId<TAttachedRuntimeComponent>();
    REQUIRE(WorldInstance.RemoveRuntimeComponent(Owner, ComponentType));
    REQUIRE_FALSE(WorldInstance.HasRuntimeComponent(Owner, ComponentType));
    REQUIRE(WorldInstance.ResolveRuntimeComponentRaw(ToRuntimeComponentHandle(Handle), ComponentType) == nullptr);
    REQUIRE(Storage.Resolve(Handle) == nullptr);
    REQUIRE(Storage.ResolveIncludingPendingDestroy(Handle) == BorrowedBeforeRemove);

    WorldInstance.EndFrame();
    REQUIRE(Storage.ResolveIncludingPendingDestroy(Handle) == nullptr);
}

TEST_CASE("World ECS runtime honors per-phase static priorities independently")
{
    World WorldInstance{"RuntimePerPhasePriorityWorld"};
    auto& Runtime = WorldInstance.EcsRuntime();

    std::vector<int> Log{};

    auto& FirstStorage = Runtime.Storage<TPerPhasePriorityFirstRuntimeType>();
    auto& SecondStorage = Runtime.Storage<TPerPhasePrioritySecondRuntimeType>();

    REQUIRE(FirstStorage.Create(WorldInstance, &Log).has_value());
    REQUIRE(SecondStorage.Create(WorldInstance, &Log).has_value());

    Runtime.Tick(WorldInstance, 1.0f / 60.0f);

    REQUIRE(Log.size() == 4);
    REQUIRE(Log[0] == 1);
    REQUIRE(Log[1] == 2);
    REQUIRE(Log[2] == 3);
    REQUIRE(Log[3] == 4);
}

TEST_CASE("World runtime components can be added by TypeId when storage is registered")
{
    World WorldInstance{"RuntimeComponentErasedAddWorld"};
    auto NodeResult = WorldInstance.CreateNode<BaseNode>("RuntimeNode");
    REQUIRE(NodeResult.has_value());
    NodeHandle Owner = *NodeResult;

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

    const TypeId ComponentType = StaticTypeId<TAttachedRuntimeComponent>();
    NodeHandle OwnerHandle = *NodeResult;
    auto ComponentHandleResult = WorldInstance.RuntimeComponentByType(OwnerHandle, ComponentType);
    REQUIRE(ComponentHandleResult.has_value());

    REQUIRE(WorldInstance.DestroyNode(OwnerHandle));
    WorldInstance.EndFrame();

    REQUIRE(NodeResult->Borrowed() == nullptr);
    REQUIRE(WorldInstance.ResolveRuntimeComponentRaw(*ComponentHandleResult, ComponentType) == nullptr);
}

TEST_CASE("World clear destroys components while their owner node is still available")
{
    World WorldInstance{"RuntimeClearOwnerAwareDestroyWorld"};

    auto NodeResult = WorldInstance.CreateNode<BaseNode>("RuntimeOwner");
    REQUIRE(NodeResult.has_value());

    BaseNode* Owner = NodeResult->Borrowed();
    REQUIRE(Owner != nullptr);

    bool OwnerWasAlive = false;
    bool WorldWasAlive = false;
    auto ComponentResult = Owner->Add<TOwnerAwareDestroyComponent>(&OwnerWasAlive, &WorldWasAlive);
    REQUIRE(ComponentResult);

    WorldInstance.Clear();

    REQUIRE(OwnerWasAlive);
    REQUIRE(WorldWasAlive);
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

TEST_CASE("World ECS runtime tick dispatch executes only Tick once per object")
{
    World WorldInstance{"RuntimeSingleTickEntryWorld"};
    auto& Storage = WorldInstance.EcsRuntime().Storage<TDualTickEntryRuntimeType>();

    int TickCalls = 0;
    auto HandleResult = Storage.Create(WorldInstance, &TickCalls);
    REQUIRE(HandleResult.has_value());

    WorldInstance.Tick(1.0f / 60.0f);

    REQUIRE(TickCalls == 1);
}

TEST_CASE("World RunGameplay execution flag gates ECS runtime phases")
{
    World WorldInstance{"RuntimeWorldEcsOnly"};
    WorldExecutionProfile Profile = WorldInstance.ExecutionProfile();
    Profile.RunGameplay = false;
    WorldInstance.SetExecutionProfile(Profile);

    auto& Storage = WorldInstance.EcsRuntime().Storage<TPhaseRuntimeType>();

    TPhaseRuntimeType::Counters Counts{};
    auto HandleResult = Storage.Create(WorldInstance, &Counts);
    REQUIRE(HandleResult.has_value());

    WorldInstance.Tick(1.0f / 60.0f);
    WorldInstance.FixedTick(1.0f / 60.0f);
    WorldInstance.LateTick(1.0f / 60.0f);

    REQUIRE(Counts.Pre == 0);
    REQUIRE(Counts.Tick == 0);
    REQUIRE(Counts.Fixed == 0);
    REQUIRE(Counts.Late == 0);
    REQUIRE(Counts.Post == 0);

    Profile.RunGameplay = true;
    WorldInstance.SetExecutionProfile(Profile);

    WorldInstance.Tick(1.0f / 60.0f);
    WorldInstance.FixedTick(1.0f / 60.0f);
    WorldInstance.LateTick(1.0f / 60.0f);

    REQUIRE(Counts.Pre == 1);
    REQUIRE(Counts.Tick == 1);
    REQUIRE(Counts.Fixed == 1);
    REQUIRE(Counts.Late == 1);
    REQUIRE(Counts.Post == 1);
}

TEST_CASE("Editor execution profile disables gameplay runtime phases")
{
    World WorldInstance{"EditorWorldNoGameplayTicks"};
    WorldInstance.SetExecutionProfile(WorldExecutionProfile::Editor());

    auto& Storage = WorldInstance.EcsRuntime().Storage<TPhaseRuntimeType>();

    TPhaseRuntimeType::Counters Counts{};
    auto HandleResult = Storage.Create(WorldInstance, &Counts);
    REQUIRE(HandleResult.has_value());

    WorldInstance.Tick(1.0f / 60.0f);
    WorldInstance.FixedTick(1.0f / 60.0f);
    WorldInstance.LateTick(1.0f / 60.0f);

    REQUIRE(Counts.Pre == 0);
    REQUIRE(Counts.Tick == 0);
    REQUIRE(Counts.Fixed == 0);
    REQUIRE(Counts.Late == 0);
    REQUIRE(Counts.Post == 0);
}

#if defined(WITH_EDITOR) && WITH_EDITOR
TEST_CASE("World ECS runtime clear removes stale editor tick phase entries")
{
    World WorldInstance{"EditorTickClearWorld"};
    WorldInstance.SetWorldKind(EWorldKind::Editor);
    WorldInstance.SetExecutionProfile(WorldExecutionProfile::Editor());

    int FirstTickCalls = 0;
    auto& FirstStorage = WorldInstance.EcsRuntime().Storage<TEditorTickRuntimeType>();
    REQUIRE(FirstStorage.Create(WorldInstance, &FirstTickCalls).has_value());

    WorldInstance.Tick(1.0f / 60.0f);
    REQUIRE(FirstTickCalls == 1);

    WorldInstance.Clear();

    int SecondTickCalls = 0;
    auto& SecondStorage = WorldInstance.EcsRuntime().Storage<TEditorTickRuntimeType>();
    REQUIRE(SecondStorage.Create(WorldInstance, &SecondTickCalls).has_value());

    WorldInstance.Tick(1.0f / 60.0f);
    REQUIRE(SecondTickCalls == 1);
}
#endif

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
}
