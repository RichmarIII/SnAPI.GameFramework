#include <array>
#include <string>
#include <unordered_map>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"
#include "NodeCast.h"

using namespace SnAPI::GameFramework;
using namespace SnAPI::GameFramework::Conduit;

namespace
{

struct ConduitHarness
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitHarness";

    int Health = 0;
    std::string Label = "Unset";

    void AddHealth(const int Delta)
    {
        Health += Delta;
    }

    int SumHealth(const int Delta) const
    {
        return Health + Delta;
    }

    int GetHealthValue() const
    {
        return Health;
    }

    int GetPower() const
    {
        return m_power;
    }

    void SetPower(const int Value)
    {
        m_power = Value;
    }

private:
    int m_power = 0;
};

struct ConduitHarnessHandle
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitHarnessHandle";

    int Id = 0;
};

struct ConduitNodeHarness : BaseNode, NodeCRTP<ConduitNodeHarness>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitNodeHarness";

    int Score = 0;
    float DeltaSum = 0.0f;

    void AddScore(const int Delta)
    {
        Score += Delta;
    }

    void AddDelta(const float Delta)
    {
        DeltaSum += Delta;
    }
};

struct ConduitComponentHarness : BaseComponent, ComponentCRTP<ConduitComponentHarness>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitComponentHarness";

    int Charge = 0;

    void AddCharge(const int Delta)
    {
        Charge += Delta;
    }
};

struct ConduitPointerBase
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitPointerBase";

    int Value = 0;

    [[nodiscard]] int ReadValue() const
    {
        return Value;
    }
};

struct ConduitPointerDerived : ConduitPointerBase
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitPointerDerived";
};

struct ConduitPointerEmitter
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitPointerEmitter";

    ConduitPointerDerived* Target = nullptr;
    ConduitPointerDerived OwnedTarget{};

    [[nodiscard]] ConduitPointerDerived* GetTarget() const
    {
        return Target;
    }

    [[nodiscard]] ConduitPointerDerived& GetOwnedTarget()
    {
        return OwnedTarget;
    }

    [[nodiscard]] const ConduitPointerBase& GetOwnedTargetBase() const
    {
        return OwnedTarget;
    }

    [[nodiscard]] int ReadPeerValue(const ConduitPointerBase* Peer) const
    {
        return Peer ? Peer->ReadValue() : -1;
    }
};

struct HandleResolverState
{
    std::unordered_map<int, ConduitHarness*> Harnesses;
    const TypeInfo* HarnessType = nullptr;
};

struct AssetManagerResolverScope
{
    explicit AssetManagerResolverScope(::SnAPI::AssetPipeline::AssetManager& Manager)
    {
        SetDefaultAssetManagerResolver([&Manager]() -> ::SnAPI::AssetPipeline::AssetManager* {
            return &Manager;
        });
    }

    ~AssetManagerResolverScope()
    {
        ClearDefaultAssetManagerResolver();
    }

    AssetManagerResolverScope(const AssetManagerResolverScope&) = delete;
    AssetManagerResolverScope& operator=(const AssetManagerResolverScope&) = delete;
};

TExpected<ResolvedTarget> ResolveConduitHarnessHandle(const void* UserData,
                                                      const TypeInfo& ExpectedType,
                                                      const TypeInfo& HandleType,
                                                      const void* HandleValue)
{
    if (!UserData || !HandleValue)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit test handle resolver received null input"));
    }
    if (HandleType.Id != StaticTypeId<ConduitHarnessHandle>())
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit test handle type mismatch"));
    }

    const auto* State = static_cast<const HandleResolverState*>(UserData);
    const auto* Handle = static_cast<const ConduitHarnessHandle*>(HandleValue);
    auto It = State->Harnesses.find(Handle->Id);
    if (It == State->Harnesses.end())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit test handle target not found"));
    }
    if (State->HarnessType && !TypeRegistry::Instance().IsA(State->HarnessType->Id, ExpectedType.Id))
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit test resolved target type mismatch"));
    }

    return ResolvedTarget{
        .Instance = It->second,
        .Type = State->HarnessType,
    };
}

void EnsureConduitHarnessRegistered()
{
    RegisterBuiltinTypes();

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>()))
    {
        auto RegisterResult = TTypeBuilder<ConduitHarness>(ConduitHarness::kTypeName)
            .Field("Health", &ConduitHarness::Health)
            .Field("Label", &ConduitHarness::Label)
            .Field("HealthValue", &ConduitHarness::GetHealthValue)
            .Field("Power", &ConduitHarness::GetPower, &ConduitHarness::SetPower)
            .Method("AddHealth", &ConduitHarness::AddHealth)
            .Method("SumHealth", &ConduitHarness::SumHealth)
            .Constructor<>()
            .Register();
        REQUIRE(RegisterResult);
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitHarnessHandle>()))
    {
        auto HandleRegisterResult = TTypeBuilder<ConduitHarnessHandle>(ConduitHarnessHandle::kTypeName)
            .Field("Id", &ConduitHarnessHandle::Id)
            .Constructor<>()
            .Register();
        REQUIRE(HandleRegisterResult);
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitNodeHarness>()))
    {
        auto NodeRegisterResult = TTypeBuilder<ConduitNodeHarness>(ConduitNodeHarness::kTypeName)
            .Base<BaseNode>()
            .Field("Score", &ConduitNodeHarness::Score)
            .Field("DeltaSum", &ConduitNodeHarness::DeltaSum)
            .Method("AddScore", &ConduitNodeHarness::AddScore)
            .Method("AddDelta", &ConduitNodeHarness::AddDelta)
            .Constructor<>()
            .Register();
        REQUIRE(NodeRegisterResult);
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitComponentHarness>()))
    {
        auto ComponentRegisterResult = TTypeBuilder<ConduitComponentHarness>(ConduitComponentHarness::kTypeName)
            .Field("Charge", &ConduitComponentHarness::Charge)
            .Method("AddCharge", &ConduitComponentHarness::AddCharge)
            .Constructor<>()
            .Register();
        REQUIRE(ComponentRegisterResult);
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitPointerBase>()))
    {
        auto BaseRegisterResult = TTypeBuilder<ConduitPointerBase>(ConduitPointerBase::kTypeName)
            .Field("Value", &ConduitPointerBase::Value)
            .Method("ReadValue", &ConduitPointerBase::ReadValue)
            .Constructor<>()
            .Register();
        REQUIRE(BaseRegisterResult);
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitPointerDerived>()))
    {
        auto DerivedRegisterResult = TTypeBuilder<ConduitPointerDerived>(ConduitPointerDerived::kTypeName)
            .Base<ConduitPointerBase>()
            .Constructor<>()
            .Register();
        REQUIRE(DerivedRegisterResult);
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitPointerEmitter>()))
    {
        auto EmitterRegisterResult = TTypeBuilder<ConduitPointerEmitter>(ConduitPointerEmitter::kTypeName)
            .Field("Target", &ConduitPointerEmitter::Target)
            .Method("GetTarget", &ConduitPointerEmitter::GetTarget)
            .Method("GetOwnedTarget", &ConduitPointerEmitter::GetOwnedTarget)
            .Method("GetOwnedTargetBase", &ConduitPointerEmitter::GetOwnedTargetBase)
            .Method("ReadPeerValue", &ConduitPointerEmitter::ReadPeerValue)
            .Constructor<>()
            .Register();
        REQUIRE(EmitterRegisterResult);
    }
}

template<typename T>
SerializedValue MakeSerializedValue(const T& Value)
{
    auto Result = SerializedValue::FromValue(Value);
    REQUIRE(Result);
    return std::move(*Result);
}

::SnAPI::AssetPipeline::AssetId StoreRuntimeGraphAsset(::SnAPI::AssetPipeline::AssetManager& Manager,
                                                       const GraphAsset& Asset,
                                                       const std::string& Name)
{
    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeGraphAsset(Asset, Bytes));

    ::SnAPI::AssetPipeline::RuntimeAssetUpsert RuntimeAsset{};
    RuntimeAsset.Name = Name;
    RuntimeAsset.AssetKind = AssetKindConduitGraph();
    RuntimeAsset.Cooked = ::SnAPI::AssetPipeline::TypedPayload(
        PayloadConduitGraph(),
        GraphAsset::kSchemaVersion,
        std::move(Bytes));

    auto UpsertResult = Manager.UpsertRuntimeAsset(std::move(RuntimeAsset));
    REQUIRE(UpsertResult);
    return *UpsertResult;
}

::SnAPI::AssetPipeline::AssetId StoreRuntimeClassAsset(::SnAPI::AssetPipeline::AssetManager& Manager,
                                                       const ClassAsset& Asset,
                                                       const std::string& Name)
{
    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeClassAsset(Asset, Bytes));

    ::SnAPI::AssetPipeline::RuntimeAssetUpsert RuntimeAsset{};
    RuntimeAsset.Name = Name;
    RuntimeAsset.AssetKind = AssetKindConduitClass();
    RuntimeAsset.Cooked = ::SnAPI::AssetPipeline::TypedPayload(
        PayloadConduitClass(),
        ClassAsset::kSchemaVersion,
        std::move(Bytes));

    auto UpsertResult = Manager.UpsertRuntimeAsset(std::move(RuntimeAsset));
    REQUIRE(UpsertResult);
    return *UpsertResult;
}

} // namespace

TEST_CASE("Conduit executes compiled self-bound reflected graphs")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    GraphBuilder Builder(*SelfType);

    auto SetHealthSlot = Builder.AddSlot(StaticTypeId<int>());
    auto AddDeltaSlot = Builder.AddSlot(StaticTypeId<int>());
    auto ReadHealthSlot = Builder.AddSlot(StaticTypeId<int>());
    auto SumDeltaSlot = Builder.AddSlot(StaticTypeId<int>());
    auto SumOutputSlot = Builder.AddSlot(StaticTypeId<int>());

    REQUIRE(SetHealthSlot);
    REQUIRE(AddDeltaSlot);
    REQUIRE(ReadHealthSlot);
    REQUIRE(SumDeltaSlot);
    REQUIRE(SumOutputSlot);

    REQUIRE(Builder.AddConstant(*SetHealthSlot, Variant::FromValue(10)));
    REQUIRE(Builder.AddSelfFieldWrite("Health", *SetHealthSlot));

    REQUIRE(Builder.AddConstant(*AddDeltaSlot, Variant::FromValue(5)));
    const std::array<SlotId, 1> AddArgs{*AddDeltaSlot};
    REQUIRE(Builder.AddSelfMethodCall("AddHealth", AddArgs));

    REQUIRE(Builder.AddSelfFieldRead("Health", *ReadHealthSlot));

    REQUIRE(Builder.AddConstant(*SumDeltaSlot, Variant::FromValue(7)));
    const std::array<SlotId, 1> SumArgs{*SumDeltaSlot};
    REQUIRE(Builder.AddSelfMethodCall("SumHealth", SumArgs, *SumOutputSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;
    Harness.Label = "Bound";

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.Execute(Context));
    REQUIRE(Harness.Health == 15);

    auto ReadHealth = Instance.Frame().AsConstRef<int>(*ReadHealthSlot);
    REQUIRE(ReadHealth);
    REQUIRE(ReadHealth->get() == 15);

    auto SumOutput = Instance.Frame().AsConstRef<int>(*SumOutputSlot);
    REQUIRE(SumOutput);
    REQUIRE(SumOutput->get() == 22);
}

TEST_CASE("Conduit field reads support getter-only by-value reflection")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    GraphBuilder Builder(*SelfType);

    auto OutputSlot = Builder.AddSlot(StaticTypeId<int>());
    REQUIRE(OutputSlot);
    REQUIRE(Builder.AddSelfFieldRead("HealthValue", *OutputSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;
    Harness.Health = 41;

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.Execute(Context));

    auto ReadValue = Instance.Frame().AsConstRef<int>(*OutputSlot);
    REQUIRE(ReadValue);
    REQUIRE(ReadValue->get() == 41);
}

TEST_CASE("Conduit field writes support setter-based reflected properties")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    GraphBuilder Builder(*SelfType);

    auto SetPowerSlot = Builder.AddSlot(StaticTypeId<int>());
    auto ReadPowerSlot = Builder.AddSlot(StaticTypeId<int>());
    REQUIRE(SetPowerSlot);
    REQUIRE(ReadPowerSlot);

    REQUIRE(Builder.AddConstant(*SetPowerSlot, Variant::FromValue(12)));
    REQUIRE(Builder.AddSelfFieldWrite("Power", *SetPowerSlot));
    REQUIRE(Builder.AddSelfFieldRead("Power", *ReadPowerSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.Execute(Context));
    REQUIRE(Harness.GetPower() == 12);

    auto ReadPower = Instance.Frame().AsConstRef<int>(*ReadPowerSlot);
    REQUIRE(ReadPower);
    REQUIRE(ReadPower->get() == 12);
}

TEST_CASE("Conduit branches between reflected execution paths")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    GraphBuilder Builder(*SelfType);

    auto ConditionSlot = Builder.AddSlot(StaticTypeId<bool>());
    auto TrueHealthSlot = Builder.AddSlot(StaticTypeId<int>());
    auto FalseHealthSlot = Builder.AddSlot(StaticTypeId<int>());
    auto OutputSlot = Builder.AddSlot(StaticTypeId<int>());

    REQUIRE(ConditionSlot);
    REQUIRE(TrueHealthSlot);
    REQUIRE(FalseHealthSlot);
    REQUIRE(OutputSlot);

    const LabelId TrueLabel = Builder.CreateLabel();
    const LabelId FalseLabel = Builder.CreateLabel();
    const LabelId EndLabel = Builder.CreateLabel();

    REQUIRE(Builder.AddConstant(*ConditionSlot, Variant::FromValue(false)));
    REQUIRE(Builder.AddConstant(*TrueHealthSlot, Variant::FromValue(11)));
    REQUIRE(Builder.AddConstant(*FalseHealthSlot, Variant::FromValue(27)));
    REQUIRE(Builder.AddBranch(*ConditionSlot, TrueLabel, FalseLabel));

    REQUIRE(Builder.MarkLabel(FalseLabel));
    REQUIRE(Builder.AddSelfFieldWrite("Health", *FalseHealthSlot));
    REQUIRE(Builder.AddJump(EndLabel));

    REQUIRE(Builder.MarkLabel(TrueLabel));
    REQUIRE(Builder.AddSelfFieldWrite("Health", *TrueHealthSlot));

    REQUIRE(Builder.MarkLabel(EndLabel));
    REQUIRE(Builder.AddSelfFieldRead("Health", *OutputSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.Execute(Context));
    REQUIRE(Harness.Health == 27);

    auto Output = Instance.Frame().AsConstRef<int>(*OutputSlot);
    REQUIRE(Output);
    REQUIRE(Output->get() == 27);
}

TEST_CASE("Conduit executes arithmetic and equality intrinsics")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    GraphBuilder Builder(*SelfType);

    auto LeftSlot = Builder.AddSlot(StaticTypeId<int>());
    auto RightSlot = Builder.AddSlot(StaticTypeId<int>());
    auto SumSlot = Builder.AddSlot(StaticTypeId<int>());
    auto TenSlot = Builder.AddSlot(StaticTypeId<int>());
    auto LessSlot = Builder.AddSlot(StaticTypeId<bool>());
    auto EqualSlot = Builder.AddSlot(StaticTypeId<bool>());
    auto LabelLeftSlot = Builder.AddSlot(StaticTypeId<std::string>());
    auto LabelRightSlot = Builder.AddSlot(StaticTypeId<std::string>());
    auto LabelEqualSlot = Builder.AddSlot(StaticTypeId<bool>());

    REQUIRE(LeftSlot);
    REQUIRE(RightSlot);
    REQUIRE(SumSlot);
    REQUIRE(TenSlot);
    REQUIRE(LessSlot);
    REQUIRE(EqualSlot);
    REQUIRE(LabelLeftSlot);
    REQUIRE(LabelRightSlot);
    REQUIRE(LabelEqualSlot);

    REQUIRE(Builder.AddConstant(*LeftSlot, Variant::FromValue(4)));
    REQUIRE(Builder.AddConstant(*RightSlot, Variant::FromValue(6)));
    REQUIRE(Builder.AddConstant(*TenSlot, Variant::FromValue(10)));
    REQUIRE(Builder.AddConstant(*LabelLeftSlot, Variant::FromValue(std::string("Alpha"))));
    REQUIRE(Builder.AddConstant(*LabelRightSlot, Variant::FromValue(std::string("Alpha"))));

    REQUIRE(Builder.AddBinaryIntrinsic(EBinaryIntrinsicOp::Add, *LeftSlot, *RightSlot, *SumSlot));
    REQUIRE(Builder.AddBinaryIntrinsic(EBinaryIntrinsicOp::Less, *LeftSlot, *RightSlot, *LessSlot));
    REQUIRE(Builder.AddBinaryIntrinsic(EBinaryIntrinsicOp::Equal, *SumSlot, *TenSlot, *EqualSlot));
    REQUIRE(Builder.AddBinaryIntrinsic(EBinaryIntrinsicOp::Equal, *LabelLeftSlot, *LabelRightSlot, *LabelEqualSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;
    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.Execute(Context));

    auto Sum = Instance.Frame().AsConstRef<int>(*SumSlot);
    REQUIRE(Sum);
    REQUIRE(Sum->get() == 10);

    auto Less = Instance.Frame().AsConstRef<bool>(*LessSlot);
    REQUIRE(Less);
    REQUIRE(Less->get());

    auto Equal = Instance.Frame().AsConstRef<bool>(*EqualSlot);
    REQUIRE(Equal);
    REQUIRE(Equal->get());

    auto LabelEqual = Instance.Frame().AsConstRef<bool>(*LabelEqualSlot);
    REQUIRE(LabelEqual);
    REQUIRE(LabelEqual->get());
}

TEST_CASE("Conduit loops over reflected state with branch and jump control flow")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    GraphBuilder Builder(*SelfType);

    auto CurrentHealthSlot = Builder.AddSlot(StaticTypeId<int>());
    auto LimitSlot = Builder.AddSlot(StaticTypeId<int>());
    auto DeltaSlot = Builder.AddSlot(StaticTypeId<int>());
    auto ConditionSlot = Builder.AddSlot(StaticTypeId<bool>());
    auto NextHealthSlot = Builder.AddSlot(StaticTypeId<int>());
    auto OutputSlot = Builder.AddSlot(StaticTypeId<int>());

    REQUIRE(CurrentHealthSlot);
    REQUIRE(LimitSlot);
    REQUIRE(DeltaSlot);
    REQUIRE(ConditionSlot);
    REQUIRE(NextHealthSlot);
    REQUIRE(OutputSlot);

    const LabelId LoopConditionLabel = Builder.CreateLabel();
    const LabelId LoopBodyLabel = Builder.CreateLabel();
    const LabelId ExitLabel = Builder.CreateLabel();

    REQUIRE(Builder.AddConstant(*LimitSlot, Variant::FromValue(5)));
    REQUIRE(Builder.AddConstant(*DeltaSlot, Variant::FromValue(2)));

    REQUIRE(Builder.MarkLabel(LoopConditionLabel));
    REQUIRE(Builder.AddSelfFieldRead("Health", *CurrentHealthSlot));
    REQUIRE(Builder.AddBinaryIntrinsic(EBinaryIntrinsicOp::Less, *CurrentHealthSlot, *LimitSlot, *ConditionSlot));
    REQUIRE(Builder.AddBranch(*ConditionSlot, LoopBodyLabel, ExitLabel));

    REQUIRE(Builder.MarkLabel(LoopBodyLabel));
    REQUIRE(Builder.AddBinaryIntrinsic(EBinaryIntrinsicOp::Add, *CurrentHealthSlot, *DeltaSlot, *NextHealthSlot));
    REQUIRE(Builder.AddSelfFieldWrite("Health", *NextHealthSlot));
    REQUIRE(Builder.AddJump(LoopConditionLabel));

    REQUIRE(Builder.MarkLabel(ExitLabel));
    REQUIRE(Builder.AddSelfFieldRead("Health", *OutputSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;
    Harness.Health = 1;

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.Execute(Context));
    REQUIRE(Harness.Health == 5);

    auto Output = Instance.Frame().AsConstRef<int>(*OutputSlot);
    REQUIRE(Output);
    REQUIRE(Output->get() == 5);
}

TEST_CASE("Conduit detects runaway control-flow loops")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    GraphBuilder Builder(*SelfType);
    const LabelId LoopLabel = Builder.CreateLabel();

    REQUIRE(Builder.MarkLabel(LoopLabel));
    REQUIRE(Builder.AddJump(LoopLabel));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
        .MaxNodeExecutions = 8,
    };

    const Result ExecuteResult = Instance.Execute(Context);
    REQUIRE_FALSE(ExecuteResult);
    REQUIRE(ExecuteResult.error().Code == EErrorCode::OutOfRange);
}

TEST_CASE("Conduit executes bounded named entrypoints without falling through")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    GraphBuilder Builder(*SelfType);

    auto CreateDeltaSlot = Builder.AddSlot(StaticTypeId<int>());
    auto CustomDeltaSlot = Builder.AddSlot(StaticTypeId<int>());
    REQUIRE(CreateDeltaSlot);
    REQUIRE(CustomDeltaSlot);

    REQUIRE(Builder.AddEntryPoint("OnCreate", EBuiltinEntryPoint::OnCreate));
    REQUIRE(Builder.AddConstant(*CreateDeltaSlot, Variant::FromValue(2)));
    const std::array<SlotId, 1> CreateArgs{*CreateDeltaSlot};
    REQUIRE(Builder.AddSelfMethodCall("AddHealth", CreateArgs));

    REQUIRE(Builder.AddEntryPoint("Boost"));
    REQUIRE(Builder.AddConstant(*CustomDeltaSlot, Variant::FromValue(5)));
    const std::array<SlotId, 1> BoostArgs{*CustomDeltaSlot};
    REQUIRE(Builder.AddSelfMethodCall("AddHealth", BoostArgs));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);
    REQUIRE(GraphResult->FindEntryPoint(EBuiltinEntryPoint::OnCreate) != nullptr);
    REQUIRE(GraphResult->FindEntryPoint("Boost") != nullptr);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.ExecuteEntry(EBuiltinEntryPoint::OnCreate, Context));
    REQUIRE(Harness.Health == 2);

    REQUIRE(Instance.ExecuteEntry("Boost", Context));
    REQUIRE(Harness.Health == 7);
}

TEST_CASE("Conduit resolves handle slots to reflected instances")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* HarnessType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(HarnessType != nullptr);

    GraphBuilder Builder(*HarnessType);

    auto HandleSlot = Builder.AddSlot(StaticTypeId<ConduitHarnessHandle>(), ESlotKind::Handle);
    auto SetHealthSlot = Builder.AddSlot(StaticTypeId<int>());
    auto AddDeltaSlot = Builder.AddSlot(StaticTypeId<int>());
    auto ReadHealthSlot = Builder.AddSlot(StaticTypeId<int>());
    auto SumDeltaSlot = Builder.AddSlot(StaticTypeId<int>());
    auto SumOutputSlot = Builder.AddSlot(StaticTypeId<int>());

    REQUIRE(HandleSlot);
    REQUIRE(SetHealthSlot);
    REQUIRE(AddDeltaSlot);
    REQUIRE(ReadHealthSlot);
    REQUIRE(SumDeltaSlot);
    REQUIRE(SumOutputSlot);

    REQUIRE(Builder.AddConstant(*HandleSlot, Variant::FromValue(ConduitHarnessHandle{.Id = 7})));
    REQUIRE(Builder.AddConstant(*SetHealthSlot, Variant::FromValue(20)));
    REQUIRE(Builder.AddFieldWrite(*HarnessType, *HandleSlot, "Health", *SetHealthSlot));

    REQUIRE(Builder.AddConstant(*AddDeltaSlot, Variant::FromValue(3)));
    const std::array<SlotId, 1> AddArgs{*AddDeltaSlot};
    REQUIRE(Builder.AddMethodCall(*HarnessType, *HandleSlot, "AddHealth", AddArgs));

    REQUIRE(Builder.AddFieldRead(*HarnessType, *HandleSlot, "Health", *ReadHealthSlot));

    REQUIRE(Builder.AddConstant(*SumDeltaSlot, Variant::FromValue(5)));
    const std::array<SlotId, 1> SumArgs{*SumDeltaSlot};
    REQUIRE(Builder.AddMethodCall(*HarnessType, *HandleSlot, "SumHealth", SumArgs, *SumOutputSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;
    HandleResolverState ResolverState{
        .Harnesses = {{7, &Harness}},
        .HarnessType = HarnessType,
    };

    ExecutionContext Context{
        .ResolveHandle = &ResolveConduitHarnessHandle,
        .HandleResolverUserData = &ResolverState,
    };

    REQUIRE(Instance.Execute(Context));
    REQUIRE(Harness.Health == 23);

    auto ReadHealth = Instance.Frame().AsConstRef<int>(*ReadHealthSlot);
    REQUIRE(ReadHealth);
    REQUIRE(ReadHealth->get() == 23);

    auto SumOutput = Instance.Frame().AsConstRef<int>(*SumOutputSlot);
    REQUIRE(SumOutput);
    REQUIRE(SumOutput->get() == 28);
}

TEST_CASE("Conduit resolves NodeHandle automatically for reflected node types")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* NodeType = TypeRegistry::Instance().Find(StaticTypeId<ConduitNodeHarness>());
    REQUIRE(NodeType != nullptr);

    World WorldInstance("ConduitAutoHandle");
    auto NodeResult = WorldInstance.CreateNode<ConduitNodeHarness>("AutoNode");
    REQUIRE(NodeResult);

    NodeHandle Handle = *NodeResult;
    auto* Node = NodeCast<ConduitNodeHarness>(Handle.Borrowed());
    REQUIRE(Node != nullptr);

    GraphBuilder Builder(*NodeType);

    auto HandleSlot = Builder.AddSlot(StaticTypeId<NodeHandle>(), ESlotKind::Handle);
    auto DeltaSlot = Builder.AddSlot(StaticTypeId<int>());
    auto ReadScoreSlot = Builder.AddSlot(StaticTypeId<int>());

    REQUIRE(HandleSlot);
    REQUIRE(DeltaSlot);
    REQUIRE(ReadScoreSlot);

    REQUIRE(Builder.AddConstant(*HandleSlot, Variant::FromValue(Handle)));
    REQUIRE(Builder.AddConstant(*DeltaSlot, Variant::FromValue(6)));
    const std::array<SlotId, 1> AddArgs{*DeltaSlot};
    REQUIRE(Builder.AddMethodCall(*NodeType, *HandleSlot, "AddScore", AddArgs));
    REQUIRE(Builder.AddFieldRead(*NodeType, *HandleSlot, "Score", *ReadScoreSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ExecutionContext Context{};

    REQUIRE(Instance.Execute(Context));
    REQUIRE(Node->Score == 6);

    auto ReadScore = Instance.Frame().AsConstRef<int>(*ReadScoreSlot);
    REQUIRE(ReadScore);
    REQUIRE(ReadScore->get() == 6);
}

TEST_CASE("Conduit resolves ComponentHandle automatically for reflected component types")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* ComponentType = TypeRegistry::Instance().Find(StaticTypeId<ConduitComponentHarness>());
    REQUIRE(ComponentType != nullptr);

    World WorldInstance("ConduitAutoComponentHandle");
    auto OwnerResult = WorldInstance.CreateNode<BaseNode>("Owner");
    REQUIRE(OwnerResult);

    auto* Owner = OwnerResult->Borrowed();
    REQUIRE(Owner != nullptr);

    auto ComponentResult = Owner->Add<ConduitComponentHarness>();
    REQUIRE(ComponentResult);

    ComponentHandle Handle = ComponentResult->Handle();

    GraphBuilder Builder(*ComponentType);

    auto HandleSlot = Builder.AddSlot(StaticTypeId<ComponentHandle>(), ESlotKind::Handle);
    auto DeltaSlot = Builder.AddSlot(StaticTypeId<int>());
    auto ReadChargeSlot = Builder.AddSlot(StaticTypeId<int>());

    REQUIRE(HandleSlot);
    REQUIRE(DeltaSlot);
    REQUIRE(ReadChargeSlot);

    REQUIRE(Builder.AddConstant(*HandleSlot, Variant::FromValue(Handle)));
    REQUIRE(Builder.AddConstant(*DeltaSlot, Variant::FromValue(4)));
    const std::array<SlotId, 1> AddArgs{*DeltaSlot};
    REQUIRE(Builder.AddMethodCall(*ComponentType, *HandleSlot, "AddCharge", AddArgs));
    REQUIRE(Builder.AddFieldRead(*ComponentType, *HandleSlot, "Charge", *ReadChargeSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ExecutionContext Context{};

    REQUIRE(Instance.Execute(Context));
    REQUIRE(ComponentResult->Charge == 4);

    auto ReadCharge = Instance.Frame().AsConstRef<int>(*ReadChargeSlot);
    REQUIRE(ReadCharge);
    REQUIRE(ReadCharge->get() == 4);
}

TEST_CASE("Conduit resolves reflected pointer values as instance targets")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* EmitterType = TypeRegistry::Instance().Find(StaticTypeId<ConduitPointerEmitter>());
    const TypeInfo* BaseType = TypeRegistry::Instance().Find(StaticTypeId<ConduitPointerBase>());
    REQUIRE(EmitterType != nullptr);
    REQUIRE(BaseType != nullptr);

    GraphBuilder Builder(*EmitterType);

    auto TargetSlot = Builder.AddSlot(StaticTypeId<ConduitPointerDerived*>());
    auto ReadValueSlot = Builder.AddSlot(StaticTypeId<int>());
    REQUIRE(TargetSlot);
    REQUIRE(ReadValueSlot);

    REQUIRE(Builder.AddSelfMethodCall("GetTarget", {}, *TargetSlot));
    REQUIRE(Builder.AddMethodCall(*BaseType, *TargetSlot, "ReadValue", {}, *ReadValueSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitPointerDerived Target{};
    Target.Value = 33;

    ConduitPointerEmitter Emitter{};
    Emitter.Target = &Target;

    ExecutionContext Context{
        .Self = &Emitter,
        .SelfType = EmitterType,
    };

    REQUIRE(Instance.Execute(Context));
    auto ReadValue = Instance.Frame().AsConstRef<int>(*ReadValueSlot);
    REQUIRE(ReadValue);
    CHECK(ReadValue->get() == 33);
}

TEST_CASE("Conduit matches derived reflected pointers to base pointer params")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* EmitterType = TypeRegistry::Instance().Find(StaticTypeId<ConduitPointerEmitter>());
    REQUIRE(EmitterType != nullptr);

    GraphBuilder Builder(*EmitterType);

    auto TargetSlot = Builder.AddSlot(StaticTypeId<ConduitPointerDerived*>());
    auto ReadValueSlot = Builder.AddSlot(StaticTypeId<int>());
    REQUIRE(TargetSlot);
    REQUIRE(ReadValueSlot);

    REQUIRE(Builder.AddSelfMethodCall("GetTarget", {}, *TargetSlot));
    const std::array<SlotId, 1> Args{*TargetSlot};
    REQUIRE(Builder.AddSelfMethodCall("ReadPeerValue", Args, *ReadValueSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitPointerDerived Target{};
    Target.Value = 91;

    ConduitPointerEmitter Emitter{};
    Emitter.Target = &Target;

    ExecutionContext Context{
        .Self = &Emitter,
        .SelfType = EmitterType,
    };

    REQUIRE(Instance.Execute(Context));
    auto ReadValue = Instance.Frame().AsConstRef<int>(*ReadValueSlot);
    REQUIRE(ReadValue);
    CHECK(ReadValue->get() == 91);
}

TEST_CASE("Conduit treats reflected reference returns as pointer-valued outputs")
{
    EnsureConduitHarnessRegistered();

    const TypeInfo* EmitterType = TypeRegistry::Instance().Find(StaticTypeId<ConduitPointerEmitter>());
    const TypeInfo* BaseType = TypeRegistry::Instance().Find(StaticTypeId<ConduitPointerBase>());
    REQUIRE(EmitterType != nullptr);
    REQUIRE(BaseType != nullptr);

    GraphBuilder Builder(*EmitterType);

    auto TargetSlot = Builder.AddSlot(StaticTypeId<const ConduitPointerBase*>());
    auto ReadValueSlot = Builder.AddSlot(StaticTypeId<int>());
    REQUIRE(TargetSlot);
    REQUIRE(ReadValueSlot);

    REQUIRE(Builder.AddSelfMethodCall("GetOwnedTargetBase", {}, *TargetSlot));
    REQUIRE(Builder.AddMethodCall(*BaseType, *TargetSlot, "ReadValue", {}, *ReadValueSlot));

    auto GraphResult = std::move(Builder).Build();
    REQUIRE(GraphResult);

    const auto* ReadMethod = std::get<InstanceMethodCallNodeData>(GraphResult->Nodes.back().Data).Method;
    REQUIRE(ReadMethod != nullptr);

    GraphInstance Instance(*GraphResult);
    ConduitPointerEmitter Emitter{};
    Emitter.OwnedTarget.Value = 47;

    ExecutionContext Context{
        .Self = &Emitter,
        .SelfType = EmitterType,
    };

    REQUIRE(Instance.Execute(Context));
    auto ReadValue = Instance.Frame().AsConstRef<int>(*ReadValueSlot);
    REQUIRE(ReadValue);
    CHECK(ReadValue->get() == 47);
}

TEST_CASE("Conduit graph asset payload serializer roundtrips authored data")
{
    EnsureConduitHarnessRegistered();

    GraphAsset Asset{};
    Asset.Name = "SerializerRoundTrip";
    Asset.SelfType = StaticTypeId<ConduitHarness>();
    Asset.Slots = {
        GraphSlotAsset{.Name = "Message", .Type = StaticTypeId<std::string>(), .Kind = ESlotKind::Value},
    };
    const Uuid VariableId = NewUuid();
    Asset.Variables = {
        GraphVariableAsset{
            .Id = VariableId,
            .Name = "GreetingCount",
            .Type = StaticTypeId<int>(),
            .DefaultValue = MakeSerializedValue(1),
        },
    };
    Asset.Nodes = {
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .EntryPointName = "Entry",
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(std::string("Hello Conduit")),
            .Output = SlotId{0},
        },
    };

    auto Serializer = CreateConduitGraphPayloadSerializer();
    REQUIRE(Serializer != nullptr);
    REQUIRE(Serializer->GetTypeId() == PayloadConduitGraph());

    std::vector<uint8_t> Bytes{};
    Serializer->SerializeToBytes(&Asset, Bytes);
    REQUIRE_FALSE(Bytes.empty());

    GraphAsset Loaded{};
    REQUIRE(Serializer->DeserializeFromBytes(&Loaded, Bytes.data(), Bytes.size()));
    REQUIRE(Loaded.Name == Asset.Name);
    REQUIRE(Loaded.SelfType == Asset.SelfType);
    REQUIRE(Loaded.Slots.size() == 1);
    REQUIRE(Loaded.Variables.size() == 1);
    REQUIRE(Loaded.Variables[0].Id == VariableId);
    REQUIRE(Loaded.Variables[0].Name == "GreetingCount");
    REQUIRE(Loaded.Variables[0].DefaultValue.Type == StaticTypeId<int>());
    REQUIRE(Loaded.Nodes.size() == 2);
    REQUIRE(Loaded.Nodes[0].Kind == EGraphAssetNodeKind::EntryPoint);
    REQUIRE(Loaded.Nodes[0].EntryPointName == "Entry");
    REQUIRE(Loaded.Nodes[1].Kind == EGraphAssetNodeKind::Constant);
    REQUIRE(Loaded.Nodes[1].ConstantValue.Type == StaticTypeId<std::string>());
}

TEST_CASE("Conduit graph assets preserve editor metadata")
{
    EnsureConduitHarnessRegistered();

    const Uuid EntryNodeId = NewUuid();
    const Uuid CommentId = NewUuid();
    const Uuid BookmarkId = NewUuid();

    GraphAsset Asset{};
    Asset.Name = "EditorMetadataRoundTrip";
    Asset.SelfType = StaticTypeId<ConduitHarness>();
    Asset.Nodes = {
        GraphNodeAsset{
            .Id = EntryNodeId,
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .EntryPointName = "Entry",
        },
    };
    Asset.EditorState.Viewport = GraphViewportAsset{
        .PanX = 128.0f,
        .PanY = -64.0f,
        .Zoom = 1.5f,
    };
    Asset.EditorState.Nodes = {
        GraphNodeEditorAsset{
            .NodeId = EntryNodeId,
            .X = 320.0f,
            .Y = 160.0f,
            .Width = 360.0f,
            .IsCollapsed = true,
        },
    };
    Asset.EditorState.Comments = {
        GraphCommentAsset{
            .Id = CommentId,
            .Title = "Setup",
            .X = 240.0f,
            .Y = 96.0f,
            .Width = 640.0f,
            .Height = 320.0f,
            .ColorRgba = 0x55667788u,
            .NodeIds = {EntryNodeId},
        },
    };
    Asset.EditorState.Bookmarks = {
        GraphBookmarkAsset{
            .Id = BookmarkId,
            .Name = "Start",
            .PanX = 320.0f,
            .PanY = 160.0f,
            .Zoom = 1.25f,
        },
    };

    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeGraphAsset(Asset, Bytes));

    auto LoadedAsset = DeserializeGraphAsset(Bytes.data(), Bytes.size());
    REQUIRE(LoadedAsset);
    REQUIRE(LoadedAsset->EditorState.Viewport.PanX == Catch::Approx(128.0f));
    REQUIRE(LoadedAsset->EditorState.Viewport.PanY == Catch::Approx(-64.0f));
    REQUIRE(LoadedAsset->EditorState.Viewport.Zoom == Catch::Approx(1.5f));
    REQUIRE(LoadedAsset->EditorState.Nodes.size() == 1);
    REQUIRE(LoadedAsset->EditorState.Nodes[0].NodeId == EntryNodeId);
    REQUIRE(LoadedAsset->EditorState.Nodes[0].Width == Catch::Approx(360.0f));
    REQUIRE(LoadedAsset->EditorState.Nodes[0].IsCollapsed);
    REQUIRE(LoadedAsset->EditorState.Comments.size() == 1);
    REQUIRE(LoadedAsset->EditorState.Comments[0].Id == CommentId);
    REQUIRE(LoadedAsset->EditorState.Comments[0].NodeIds == std::vector<Uuid>{EntryNodeId});
    REQUIRE(LoadedAsset->EditorState.Bookmarks.size() == 1);
    REQUIRE(LoadedAsset->EditorState.Bookmarks[0].Id == BookmarkId);
    REQUIRE(LoadedAsset->EditorState.Bookmarks[0].Zoom == Catch::Approx(1.25f));
}

TEST_CASE("Conduit authored graph assets serialize compile and execute")
{
    EnsureConduitHarnessRegistered();

    GraphAsset Authored{};
    Authored.Name = "LoopAsset";
    Authored.SelfType = StaticTypeId<ConduitHarness>();
    Authored.Slots = {
        GraphSlotAsset{.Name = "CurrentHealth", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "Limit", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "Delta", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "Condition", .Type = StaticTypeId<bool>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "NextHealth", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "Output", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
    };
    Authored.Nodes = {
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(5),
            .Output = SlotId{1},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(2),
            .Output = SlotId{2},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Label,
            .LabelName = "LoopCondition",
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::SelfFieldRead,
            .MemberName = "Health",
            .Output = SlotId{0},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::BinaryIntrinsic,
            .BinaryOp = EBinaryIntrinsicOp::Less,
            .Left = SlotId{0},
            .Right = SlotId{1},
            .Output = SlotId{3},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Branch,
            .LabelName = "LoopBody",
            .FalseLabelName = "Exit",
            .Condition = SlotId{3},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Label,
            .LabelName = "LoopBody",
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::BinaryIntrinsic,
            .BinaryOp = EBinaryIntrinsicOp::Add,
            .Left = SlotId{0},
            .Right = SlotId{2},
            .Output = SlotId{4},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::SelfFieldWrite,
            .MemberName = "Health",
            .Input = SlotId{4},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Jump,
            .LabelName = "LoopCondition",
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Label,
            .LabelName = "Exit",
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::SelfFieldRead,
            .MemberName = "Health",
            .Output = SlotId{5},
        },
    };

    std::vector<uint8_t> Bytes{};
    REQUIRE(SerializeGraphAsset(Authored, Bytes));

    auto LoadedAsset = DeserializeGraphAsset(Bytes.data(), Bytes.size());
    REQUIRE(LoadedAsset);
    REQUIRE(LoadedAsset->Nodes.size() == Authored.Nodes.size());

    auto GraphResult = LoadedAsset->Compile();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;
    Harness.Health = 1;

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.Execute(Context));
    REQUIRE(Harness.Health == 5);

    auto Output = Instance.Frame().AsConstRef<int>(SlotId{5});
    REQUIRE(Output);
    REQUIRE(Output->get() == 5);
}

TEST_CASE("Conduit graph variables initialize defaults and persist across entrypoints")
{
    EnsureConduitHarnessRegistered();

    const Uuid CounterId = NewUuid();

    GraphAsset Authored{};
    Authored.Name = "VariablePersistence";
    Authored.SelfType = StaticTypeId<ConduitHarness>();
    Authored.Slots = {
        GraphSlotAsset{.Name = "Current", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "Delta", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "Next", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "Output", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
    };
    Authored.Variables = {
        GraphVariableAsset{
            .Id = CounterId,
            .Name = "Counter",
            .Type = StaticTypeId<int>(),
            .DefaultValue = MakeSerializedValue(3),
        },
    };
    Authored.Nodes = {
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .EntryPointName = "AddOnce",
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::VariableGet,
            .VariableId = CounterId,
            .Output = SlotId{0},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(2),
            .Output = SlotId{1},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::BinaryIntrinsic,
            .BinaryOp = EBinaryIntrinsicOp::Add,
            .Left = SlotId{0},
            .Right = SlotId{1},
            .Output = SlotId{2},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::VariableSet,
            .VariableId = CounterId,
            .Input = SlotId{2},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .EntryPointName = "ReadCounter",
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::VariableGet,
            .VariableId = CounterId,
            .Output = SlotId{3},
        },
    };

    auto GraphResult = Authored.Compile();
    REQUIRE(GraphResult);
    REQUIRE(GraphResult->Variables.size() == 1);
    REQUIRE(GraphResult->FindVariable("Counter") != nullptr);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.ExecuteEntry("ReadCounter", Context));
    auto Output = Instance.Frame().AsConstRef<int>(SlotId{3});
    REQUIRE(Output);
    REQUIRE(Output->get() == 3);

    REQUIRE(Instance.ExecuteEntry("AddOnce", Context));
    REQUIRE(Instance.ExecuteEntry("ReadCounter", Context));
    Output = Instance.Frame().AsConstRef<int>(SlotId{3});
    REQUIRE(Output);
    REQUIRE(Output->get() == 5);

    REQUIRE(Instance.ExecuteEntry("AddOnce", Context));
    REQUIRE(Instance.ExecuteEntry("ReadCounter", Context));
    Output = Instance.Frame().AsConstRef<int>(SlotId{3});
    REQUIRE(Output);
    REQUIRE(Output->get() == 7);
}

TEST_CASE("Conduit graph assets compile explicit exec targets and run pure producers before impure consumers")
{
    EnsureConduitHarnessRegistered();

    const Uuid EntryId = NewUuid();
    const Uuid ConstantId = NewUuid();
    const Uuid SetId = NewUuid();
    const Uuid CounterId = NewUuid();

    GraphAsset Authored{};
    Authored.Name = "ExplicitExecFlow";
    Authored.SelfType = StaticTypeId<ConduitHarness>();
    Authored.Slots = {
        GraphSlotAsset{.Name = "Value", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
    };
    Authored.Variables = {
        GraphVariableAsset{
            .Id = CounterId,
            .Name = "Counter",
            .Type = StaticTypeId<int>(),
            .DefaultValue = MakeSerializedValue(1),
        },
    };
    Authored.Nodes = {
        GraphNodeAsset{
            .Id = EntryId,
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .EntryPointName = "AddOnce",
            .ExecTargetNodeId = SetId,
        },
        GraphNodeAsset{
            .Id = ConstantId,
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(9),
            .Output = SlotId{0},
        },
        GraphNodeAsset{
            .Id = SetId,
            .Kind = EGraphAssetNodeKind::VariableSet,
            .VariableId = CounterId,
            .Input = SlotId{0},
        },
    };

    auto GraphResult = Authored.Compile();
    REQUIRE(GraphResult);
    const CompiledGraphVariable* CounterVariable = GraphResult->FindVariable("Counter");
    REQUIRE(CounterVariable != nullptr);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.ExecuteEntry("AddOnce", Context));

    auto CounterValue = Instance.Frame().AsConstRef<int>(CounterVariable->Slot);
    REQUIRE(CounterValue);
    CHECK(CounterValue->get() == 9);
}

TEST_CASE("Conduit explicit exec branches consume pure string equality producers before branching")
{
    EnsureConduitHarnessRegistered();

    const Uuid EntryId = NewUuid();
    const Uuid EqualId = NewUuid();
    const Uuid BranchId = NewUuid();
    const Uuid TrueSetId = NewUuid();
    const Uuid FalseSetId = NewUuid();

    GraphAsset Authored{};
    Authored.Name = "ExplicitBranchStringEqual";
    Authored.SelfType = StaticTypeId<ConduitHarness>();
    Authored.Slots = {
        GraphSlotAsset{.Name = "Left", .Type = StaticTypeId<std::string>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "Right", .Type = StaticTypeId<std::string>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "Condition", .Type = StaticTypeId<bool>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "TrueValue", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "FalseValue", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
    };
    Authored.Nodes = {
        GraphNodeAsset{
            .Id = EntryId,
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .EntryPointName = "ChoosePath",
            .ExecTargetNodeId = BranchId,
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(std::string("Alpha")),
            .Output = SlotId{0},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(std::string("Alpha")),
            .Output = SlotId{1},
        },
        GraphNodeAsset{
            .Id = EqualId,
            .Kind = EGraphAssetNodeKind::BinaryIntrinsic,
            .BinaryOp = EBinaryIntrinsicOp::Equal,
            .Left = SlotId{0},
            .Right = SlotId{1},
            .Output = SlotId{2},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(11),
            .Output = SlotId{3},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(27),
            .Output = SlotId{4},
        },
        GraphNodeAsset{
            .Id = BranchId,
            .Kind = EGraphAssetNodeKind::Branch,
            .ExecTargetNodeId = TrueSetId,
            .FalseExecTargetNodeId = FalseSetId,
            .Condition = SlotId{2},
        },
        GraphNodeAsset{
            .Id = TrueSetId,
            .Kind = EGraphAssetNodeKind::SelfFieldWrite,
            .MemberName = "Health",
            .Input = SlotId{3},
        },
        GraphNodeAsset{
            .Id = FalseSetId,
            .Kind = EGraphAssetNodeKind::SelfFieldWrite,
            .MemberName = "Health",
            .Input = SlotId{4},
        },
    };

    auto GraphResult = Authored.Compile();
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    const Result ExecuteResult = Instance.ExecuteEntry("ChoosePath", Context);
    REQUIRE(ExecuteResult);
    CHECK(Harness.Health == 11);
}

TEST_CASE("Conduit explicit exec branches honor literal true conditions")
{
    EnsureConduitHarnessRegistered();

    const Uuid EntryId = NewUuid();
    const Uuid ConditionId = NewUuid();
    const Uuid BranchId = NewUuid();
    const Uuid TrueValueId = NewUuid();
    const Uuid FalseValueId = NewUuid();
    const Uuid TrueSetId = NewUuid();
    const Uuid FalseSetId = NewUuid();

    GraphAsset Authored{};
    Authored.Name = "ExplicitBranchLiteralTrue";
    Authored.SelfType = StaticTypeId<ConduitHarness>();
    Authored.Slots = {
        GraphSlotAsset{.Name = "Condition", .Type = StaticTypeId<bool>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "TrueValue", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "FalseValue", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
    };
    Authored.Nodes = {
        GraphNodeAsset{
            .Id = EntryId,
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .EntryPointName = "ChoosePath",
            .ExecTargetNodeId = BranchId,
        },
        GraphNodeAsset{
            .Id = ConditionId,
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(true),
            .Output = SlotId{0},
        },
        GraphNodeAsset{
            .Id = TrueValueId,
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(11),
            .Output = SlotId{1},
        },
        GraphNodeAsset{
            .Id = FalseValueId,
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(27),
            .Output = SlotId{2},
        },
        GraphNodeAsset{
            .Id = BranchId,
            .Kind = EGraphAssetNodeKind::Branch,
            .ExecTargetNodeId = TrueSetId,
            .FalseExecTargetNodeId = FalseSetId,
            .Condition = SlotId{0},
        },
        GraphNodeAsset{
            .Id = TrueSetId,
            .Kind = EGraphAssetNodeKind::SelfFieldWrite,
            .MemberName = "Health",
            .Input = SlotId{1},
        },
        GraphNodeAsset{
            .Id = FalseSetId,
            .Kind = EGraphAssetNodeKind::SelfFieldWrite,
            .MemberName = "Health",
            .Input = SlotId{2},
        },
    };

    auto GraphResult = CompileGraphAsset(Authored);
    REQUIRE(GraphResult);

    GraphInstance Instance(*GraphResult);
    ConduitHarness Harness;
    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitHarness>());
    REQUIRE(SelfType != nullptr);

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.ExecuteEntry("ChoosePath", Context));
    REQUIRE(Harness.Health == 11);
}

TEST_CASE("Conduit class assets serialize load and compile against host nodes")
{
    EnsureConduitHarnessRegistered();

    ::SnAPI::AssetPipeline::AssetManager Manager{};
    RegisterAssetPipelinePayloads(Manager.GetRegistry());
    RegisterAssetPipelineFactories(Manager);

    GraphAsset Graph{};
    Graph.Name = "ScoreLogic";
    Graph.Slots = {
        GraphSlotAsset{.Name = "Delta", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "Score", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
    };
    Graph.Nodes = {
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(3),
            .Output = SlotId{0},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::SelfMethodCall,
            .MemberName = "AddScore",
            .Inputs = {SlotId{0}},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::SelfFieldRead,
            .MemberName = "Score",
            .Output = SlotId{1},
        },
    };

    const auto GraphId = StoreRuntimeGraphAsset(Manager, Graph, "ScoreLogic");

    ClassAsset AuthoredClass{};
    AuthoredClass.Name = "ScoreClass";
    AuthoredClass.HostType = StaticTypeId<ConduitNodeHarness>();
    AuthoredClass.Graph.EditAssetName() = "ScoreLogic";
    AuthoredClass.Graph.EditAssetId() = GraphId.ToString();

    const auto ClassId = StoreRuntimeClassAsset(Manager, AuthoredClass, "ScoreClass");
    auto LoadedClass = Manager.Load<ClassAsset>(ClassId);
    REQUIRE(LoadedClass);
    REQUIRE(*LoadedClass);
    REQUIRE((*LoadedClass)->HostType == StaticTypeId<ConduitNodeHarness>());
    REQUIRE((*LoadedClass)->Graph.GetAssetId() == GraphId.ToString());

    auto CompiledClass = CompileClassAsset(**LoadedClass, Manager);
    REQUIRE(CompiledClass);
    REQUIRE(CompiledClass->HostType == StaticTypeId<ConduitNodeHarness>());
    REQUIRE(CompiledClass->EffectiveSelfType == StaticTypeId<ConduitNodeHarness>());
    REQUIRE(CompiledClass->SourceGraph.SelfType == StaticTypeId<ConduitNodeHarness>());

    GraphInstance Instance(CompiledClass->RuntimeGraph);
    ConduitNodeHarness Harness;

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(StaticTypeId<ConduitNodeHarness>());
    REQUIRE(SelfType != nullptr);

    ExecutionContext Context{
        .Self = &Harness,
        .SelfType = SelfType,
    };

    REQUIRE(Instance.Execute(Context));
    REQUIRE(Harness.Score == 3);

    auto Score = Instance.Frame().AsConstRef<int>(SlotId{1});
    REQUIRE(Score);
    REQUIRE(Score->get() == 3);
}

TEST_CASE("Conduit class assets reject incompatible graph self types")
{
    EnsureConduitHarnessRegistered();

    ::SnAPI::AssetPipeline::AssetManager Manager{};
    RegisterAssetPipelinePayloads(Manager.GetRegistry());
    RegisterAssetPipelineFactories(Manager);

    GraphAsset Graph{};
    Graph.Name = "HarnessOnlyGraph";
    Graph.SelfType = StaticTypeId<ConduitHarness>();

    const auto GraphId = StoreRuntimeGraphAsset(Manager, Graph, "HarnessOnlyGraph");

    ClassAsset AuthoredClass{};
    AuthoredClass.Name = "InvalidNodeBinding";
    AuthoredClass.HostType = StaticTypeId<ConduitNodeHarness>();
    AuthoredClass.Graph.EditAssetName() = "HarnessOnlyGraph";
    AuthoredClass.Graph.EditAssetId() = GraphId.ToString();

    auto CompileResult = CompileClassAsset(AuthoredClass, Manager);
    REQUIRE_FALSE(CompileResult);
    REQUIRE(CompileResult.error().Message.find("incompatible") != std::string::npos);
}

TEST_CASE("Conduit class components execute authored OnCreate graphs on host nodes")
{
    EnsureConduitHarnessRegistered();

    ::SnAPI::AssetPipeline::AssetManager Manager{};
    RegisterAssetPipelinePayloads(Manager.GetRegistry());
    RegisterAssetPipelineFactories(Manager);

    AssetManagerResolverScope ResolverScope(Manager);

    GraphAsset Graph{};
    Graph.Name = "ComponentCreateScore";
    Graph.Slots = {
        GraphSlotAsset{.Name = "Amount", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
        GraphSlotAsset{.Name = "TickAmount", .Type = StaticTypeId<float>(), .Kind = ESlotKind::Value},
    };
    Graph.Nodes = {
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .BuiltinEntryPoint = EBuiltinEntryPoint::OnCreate,
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(4),
            .Output = SlotId{0},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::SelfMethodCall,
            .MemberName = "AddScore",
            .Inputs = {SlotId{0}},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .BuiltinEntryPoint = EBuiltinEntryPoint::Tick,
            .Output = SlotId{1},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::SelfMethodCall,
            .MemberName = "AddDelta",
            .Inputs = {SlotId{1}},
        },
    };

    const auto GraphId = StoreRuntimeGraphAsset(Manager, Graph, "ComponentCreateScore");

    ClassAsset AuthoredClass{};
    AuthoredClass.Name = "ComponentCreateScoreClass";
    AuthoredClass.HostType = StaticTypeId<ConduitNodeHarness>();
    AuthoredClass.Graph.EditAssetName() = "ComponentCreateScore";
    AuthoredClass.Graph.EditAssetId() = GraphId.ToString();

    const auto ClassId = StoreRuntimeClassAsset(Manager, AuthoredClass, "ComponentCreateScoreClass");

    World WorldInstance("ConduitClassComponentCreate");
    auto NodeHandleResult = WorldInstance.CreateNode<ConduitNodeHarness>("HostNode");
    REQUIRE(NodeHandleResult);

    auto* Node = NodeCast<ConduitNodeHarness>(NodeHandleResult->Borrowed());
    REQUIRE(Node != nullptr);
    REQUIRE(Node->Score == 0);

    {
        ScopedComponentOnCreateSuppression SuppressOnCreate{};
        auto ComponentResult = Node->Add<ClassComponent>();
        REQUIRE(ComponentResult);
        auto& Component = *ComponentResult;
        Component.Class.EditAssetName() = "ComponentCreateScoreClass";
        Component.Class.EditAssetId() = ClassId.ToString();
    }

    WorldInstance.EcsRuntime().FlushPendingOnCreate(WorldInstance);

    auto ComponentResult = Node->Component<ClassComponent>();
    REQUIRE(ComponentResult);
    REQUIRE(ComponentResult->IsBound());
    REQUIRE(ComponentResult->LastError().empty());
    REQUIRE(Node->Score == 4);
    REQUIRE(Node->DeltaSum == Catch::Approx(0.0f));

    WorldInstance.Tick(0.5f);
    REQUIRE(Node->Score == 4);
    REQUIRE(Node->DeltaSum == Catch::Approx(0.5f));
}

TEST_CASE("Conduit class components inject DeltaSeconds into tick graphs")
{
    EnsureConduitHarnessRegistered();

    ::SnAPI::AssetPipeline::AssetManager Manager{};
    RegisterAssetPipelinePayloads(Manager.GetRegistry());
    RegisterAssetPipelineFactories(Manager);

    AssetManagerResolverScope ResolverScope(Manager);

    GraphAsset Graph{};
    Graph.Name = "ComponentTickDelta";
    Graph.Slots = {
        GraphSlotAsset{.Name = "DeltaSeconds", .Type = StaticTypeId<float>(), .Kind = ESlotKind::Value},
    };
    Graph.Nodes = {
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .BuiltinEntryPoint = EBuiltinEntryPoint::Tick,
            .Output = SlotId{0},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::SelfMethodCall,
            .MemberName = "AddDelta",
            .Inputs = {SlotId{0}},
        },
    };

    const auto GraphId = StoreRuntimeGraphAsset(Manager, Graph, "ComponentTickDelta");

    ClassAsset AuthoredClass{};
    AuthoredClass.Name = "ComponentTickDeltaClass";
    AuthoredClass.HostType = StaticTypeId<ConduitNodeHarness>();
    AuthoredClass.Graph.EditAssetName() = "ComponentTickDelta";
    AuthoredClass.Graph.EditAssetId() = GraphId.ToString();

    const auto ClassId = StoreRuntimeClassAsset(Manager, AuthoredClass, "ComponentTickDeltaClass");

    World WorldInstance("ConduitClassComponentTick");
    auto NodeHandleResult = WorldInstance.CreateNode<ConduitNodeHarness>("TickHost");
    REQUIRE(NodeHandleResult);

    auto* Node = NodeCast<ConduitNodeHarness>(NodeHandleResult->Borrowed());
    REQUIRE(Node != nullptr);
    REQUIRE(Node->DeltaSum == Catch::Approx(0.0f));

    {
        ScopedComponentOnCreateSuppression SuppressOnCreate{};
        auto ComponentResult = Node->Add<ClassComponent>();
        REQUIRE(ComponentResult);
        auto& Component = *ComponentResult;
        Component.Class.EditAssetName() = "ComponentTickDeltaClass";
        Component.Class.EditAssetId() = ClassId.ToString();
    }

    WorldInstance.EcsRuntime().FlushPendingOnCreate(WorldInstance);
    WorldInstance.Tick(0.25f);
    WorldInstance.Tick(0.50f);

    auto ComponentResult = Node->Component<ClassComponent>();
    REQUIRE(ComponentResult);
    REQUIRE(ComponentResult->IsBound());
    REQUIRE(ComponentResult->LastError().empty());
    REQUIRE(Node->DeltaSum == Catch::Approx(0.75f));
}

TEST_CASE("Conduit class components execute custom named entrypoints on demand")
{
    EnsureConduitHarnessRegistered();

    ::SnAPI::AssetPipeline::AssetManager Manager{};
    RegisterAssetPipelinePayloads(Manager.GetRegistry());
    RegisterAssetPipelineFactories(Manager);

    AssetManagerResolverScope ResolverScope(Manager);

    GraphAsset Graph{};
    Graph.Name = "ComponentCustomEntry";
    Graph.Slots = {
        GraphSlotAsset{.Name = "Amount", .Type = StaticTypeId<int>(), .Kind = ESlotKind::Value},
    };
    Graph.Nodes = {
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .EntryPointName = "AwardPoints",
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::Constant,
            .ConstantValue = MakeSerializedValue(9),
            .Output = SlotId{0},
        },
        GraphNodeAsset{
            .Kind = EGraphAssetNodeKind::SelfMethodCall,
            .MemberName = "AddScore",
            .Inputs = {SlotId{0}},
        },
    };

    const auto GraphId = StoreRuntimeGraphAsset(Manager, Graph, "ComponentCustomEntry");

    ClassAsset AuthoredClass{};
    AuthoredClass.Name = "ComponentCustomEntryClass";
    AuthoredClass.HostType = StaticTypeId<ConduitNodeHarness>();
    AuthoredClass.Graph.EditAssetName() = "ComponentCustomEntry";
    AuthoredClass.Graph.EditAssetId() = GraphId.ToString();

    const auto ClassId = StoreRuntimeClassAsset(Manager, AuthoredClass, "ComponentCustomEntryClass");

    World WorldInstance("ConduitClassComponentCustomEntry");
    auto NodeHandleResult = WorldInstance.CreateNode<ConduitNodeHarness>("EntryHost");
    REQUIRE(NodeHandleResult);

    auto* Node = NodeCast<ConduitNodeHarness>(NodeHandleResult->Borrowed());
    REQUIRE(Node != nullptr);
    REQUIRE(Node->Score == 0);

    {
        ScopedComponentOnCreateSuppression SuppressOnCreate{};
        auto ComponentResult = Node->Add<ClassComponent>();
        REQUIRE(ComponentResult);
        auto& Component = *ComponentResult;
        Component.Class.EditAssetName() = "ComponentCustomEntryClass";
        Component.Class.EditAssetId() = ClassId.ToString();
    }

    WorldInstance.EcsRuntime().FlushPendingOnCreate(WorldInstance);

    auto ComponentResult = Node->Component<ClassComponent>();
    REQUIRE(ComponentResult);
    REQUIRE(ComponentResult->IsBound());
    REQUIRE(ComponentResult->ExecuteEntry("AwardPoints"));
    REQUIRE(Node->Score == 9);
    REQUIRE(ComponentResult->ExecuteEntry("AwardPoints"));
    REQUIRE(Node->Score == 18);
}
