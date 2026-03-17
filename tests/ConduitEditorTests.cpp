#include <algorithm>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Editor/GameFrameworkEditor.hpp"
#include "GameFramework.hpp"
#include <UIEvents.h>

using namespace SnAPI::GameFramework;
using namespace SnAPI::GameFramework::Conduit;
using namespace SnAPI::GameFramework::Conduit::Editor;

namespace
{

struct ConduitEditorNodeHost : BaseNode, NodeCRTP<ConduitEditorNodeHost>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitEditorNodeHost";

    int Score = 0;

    void AddScore(const int Delta)
    {
        Score += Delta;
    }

    [[nodiscard]] int GetScore() const
    {
        return Score;
    }

    [[nodiscard]] ConduitEditorNodeHost& GetSelfReference()
    {
        return *this;
    }

    [[nodiscard]] ConduitEditorNodeHost* GetSelfPointer() const
    {
        return const_cast<ConduitEditorNodeHost*>(this);
    }
};

struct ConduitEditorDotNamedHost : BaseNode, NodeCRTP<ConduitEditorDotNamedHost>
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Tests.ConduitEditorDotNamedHost";
};

struct ConduitEditorComponentHost : BaseComponent, ComponentCRTP<ConduitEditorComponentHost>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitEditorComponentHost";

    int Charge = 0;

    void AddCharge(const int Delta)
    {
        Charge += Delta;
    }
};

struct ConduitPaletteBaseNode : BaseNode, NodeCRTP<ConduitPaletteBaseNode>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitPaletteBaseNode";

    int BaseValue = 0;

    void BasePing() {}
};

struct ConduitPaletteDerivedNode : ConduitPaletteBaseNode, NodeCRTP<ConduitPaletteDerivedNode>
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitPaletteDerivedNode";

    int DerivedValue = 0;

    void DerivedPing() {}
};

void EnsureConduitEditorNodeHostRegistered()
{
    RegisterBuiltinTypes();

    if (TypeRegistry::Instance().Find(StaticTypeId<ConduitEditorNodeHost>()))
    {
        return;
    }

    auto RegisterResult = TTypeBuilder<ConduitEditorNodeHost>(ConduitEditorNodeHost::kTypeName)
        .Base<BaseNode>()
        .Constructor<>()
        .Field("Score", &ConduitEditorNodeHost::Score)
        .Method("AddScore", &ConduitEditorNodeHost::AddScore)
        .Method("GetScore", &ConduitEditorNodeHost::GetScore)
        .Method("GetSelfReference", &ConduitEditorNodeHost::GetSelfReference)
        .Method("GetSelfPointer", &ConduitEditorNodeHost::GetSelfPointer)
        .Register();
    REQUIRE(RegisterResult);
    TypeInfo* NodeHostInfo = *RegisterResult;
    REQUIRE(NodeHostInfo != nullptr);
    NodeHostInfo->DisplayName = "Editor Node Host";
    NodeHostInfo->Doc = "Test node host used to validate reflected Conduit metadata and tooltips.";
    if (auto FieldIt = std::find_if(NodeHostInfo->Fields.begin(), NodeHostInfo->Fields.end(), [](const FieldInfo& Field) {
            return Field.Name == "Score";
        });
        FieldIt != NodeHostInfo->Fields.end())
    {
        FieldIt->DisplayName = "Score";
        FieldIt->Doc = "Current score accumulated on the test host.";
        FieldIt->Value.Min = 0.0;
        FieldIt->Value.Max = 500.0;
        FieldIt->Value.Step = 1.0;
    }
    if (auto MethodIt = std::find_if(NodeHostInfo->Methods.begin(), NodeHostInfo->Methods.end(), [](const MethodInfo& Method) {
            return Method.Name == "AddScore";
        });
        MethodIt != NodeHostInfo->Methods.end())
    {
        MethodIt->DisplayName = "Add Score";
        MethodIt->Doc = "Add a delta to the current score.";
        REQUIRE(MethodIt->Params.size() == 1);
        MethodIt->Params[0].Name = "Delta";
        MethodIt->Params[0].Doc = "Signed amount to add to the score.";
    }
    if (auto MethodIt = std::find_if(NodeHostInfo->Methods.begin(), NodeHostInfo->Methods.end(), [](const MethodInfo& Method) {
            return Method.Name == "GetScore";
        });
        MethodIt != NodeHostInfo->Methods.end())
    {
        MethodIt->DisplayName = "Get Score";
        MethodIt->Doc = "Read the current score without mutating the node.";
    }
    if (auto MethodIt = std::find_if(NodeHostInfo->Methods.begin(), NodeHostInfo->Methods.end(), [](const MethodInfo& Method) {
            return Method.Name == "GetSelfReference";
        });
        MethodIt != NodeHostInfo->Methods.end())
    {
        MethodIt->DisplayName = "Get Self Reference";
        MethodIt->Doc = "Expose the current node instance as a mutable reference.";
    }
    if (auto MethodIt = std::find_if(NodeHostInfo->Methods.begin(), NodeHostInfo->Methods.end(), [](const MethodInfo& Method) {
            return Method.Name == "GetSelfPointer";
        });
        MethodIt != NodeHostInfo->Methods.end())
    {
        MethodIt->DisplayName = "Get Self Pointer";
        MethodIt->Doc = "Expose the current node instance as a raw pointer.";
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitEditorDotNamedHost>()))
    {
        auto DotRegisterResult = TTypeBuilder<ConduitEditorDotNamedHost>(ConduitEditorDotNamedHost::kTypeName)
            .Base<BaseNode>()
            .Constructor<>()
            .Register();
        REQUIRE(DotRegisterResult);
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitEditorComponentHost>()))
    {
        auto ComponentRegisterResult = TTypeBuilder<ConduitEditorComponentHost>(ConduitEditorComponentHost::kTypeName)
            .Constructor<>()
            .Field("Charge", &ConduitEditorComponentHost::Charge)
            .Method("AddCharge", &ConduitEditorComponentHost::AddCharge)
            .Register();
        REQUIRE(ComponentRegisterResult);
        TypeInfo* ComponentInfo = *ComponentRegisterResult;
        REQUIRE(ComponentInfo != nullptr);
        ComponentInfo->DisplayName = "Editor Component Host";
        ComponentInfo->Doc = "Test component host used to validate reflected instance palette entries.";
        if (auto FieldIt = std::find_if(ComponentInfo->Fields.begin(), ComponentInfo->Fields.end(), [](const FieldInfo& Field) {
                return Field.Name == "Charge";
            });
            FieldIt != ComponentInfo->Fields.end())
        {
            FieldIt->DisplayName = "Charge";
            FieldIt->Doc = "Current charge stored on the component host.";
        }
        if (auto MethodIt = std::find_if(ComponentInfo->Methods.begin(), ComponentInfo->Methods.end(), [](const MethodInfo& Method) {
                return Method.Name == "AddCharge";
            });
            MethodIt != ComponentInfo->Methods.end())
        {
            MethodIt->DisplayName = "Add Charge";
            MethodIt->Doc = "Add charge to the current component host value.";
            REQUIRE(MethodIt->Params.size() == 1);
            MethodIt->Params[0].Name = "Delta";
            MethodIt->Params[0].Doc = "Signed amount to add to the current charge.";
        }
    }
}

void EnsureConduitPaletteHierarchyRegistered()
{
    RegisterBuiltinTypes();

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitPaletteBaseNode>()))
    {
        auto BaseRegisterResult = TTypeBuilder<ConduitPaletteBaseNode>(ConduitPaletteBaseNode::kTypeName)
            .Base<BaseNode>()
            .Constructor<>()
            .Field("BaseValue", &ConduitPaletteBaseNode::BaseValue)
            .Method("BasePing", &ConduitPaletteBaseNode::BasePing)
            .Register();
        REQUIRE(BaseRegisterResult);
    }

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitPaletteDerivedNode>()))
    {
        auto DerivedRegisterResult = TTypeBuilder<ConduitPaletteDerivedNode>(ConduitPaletteDerivedNode::kTypeName)
            .Base<ConduitPaletteBaseNode>()
            .Constructor<>()
            .Field("DerivedValue", &ConduitPaletteDerivedNode::DerivedValue)
            .Method("DerivedPing", &ConduitPaletteDerivedNode::DerivedPing)
            .Register();
        REQUIRE(DerivedRegisterResult);
    }
}

template<typename T>
SerializedValue MakeSerializedValue(const T& Value)
{
    auto Result = SerializedValue::FromValue(Value);
    REQUIRE(Result);
    return std::move(*Result);
}

} // namespace

TEST_CASE("Conduit editor graph documents manage authored variables", "[Conduit][Editor]")
{
    RegisterBuiltinTypes();

    const Uuid ExistingVariableId = NewUuid();
    const Uuid VariableNodeId = NewUuid();

    GraphAsset Asset{};
    Asset.Name = "VariableDoc";
    Asset.Variables = {
        GraphVariableAsset{
            .Id = ExistingVariableId,
            .Name = "Health",
            .Type = StaticTypeId<int>(),
            .DefaultValue = MakeSerializedValue(5),
        },
    };
    Asset.Nodes = {
        GraphNodeAsset{
            .Id = VariableNodeId,
            .Kind = EGraphAssetNodeKind::VariableGet,
            .VariableId = ExistingVariableId,
            .Output = SlotId{0},
        },
    };
    Asset.EditorState.Nodes = {
        GraphNodeEditorAsset{
            .NodeId = VariableNodeId,
        },
    };
    Asset.EditorState.Comments = {
        GraphCommentAsset{
            .Id = NewUuid(),
            .Title = "Tracked",
            .NodeIds = {VariableNodeId},
        },
    };

    GraphDocument Document("Conduit/VariableDoc", "VariableDoc", Asset);
    Document.Selection().NodeIds.push_back(VariableNodeId);
    Document.Selection().VariableIds.push_back(ExistingVariableId);
    Document.SetLastCompile(CompileOutput{
        .Diagnostics = {
            CompileDiagnostic{
                .Severity = ECompileDiagnosticSeverity::Warning,
                .Message = "stale",
            },
        },
    });

    const std::uint64_t InitialRevision = Document.Revision();

    auto AddedVariableResult = Document.AddVariable("Counter", StaticTypeId<int>());
    REQUIRE(AddedVariableResult);
    const Uuid AddedVariableId = (*AddedVariableResult)->Id;
    CHECK(Document.IsDirty());
    CHECK(Document.Revision() == InitialRevision + 1);
    CHECK_FALSE(Document.LastCompile().has_value());

    auto DuplicateVariableResult = Document.AddVariable("Counter", StaticTypeId<int>());
    CHECK_FALSE(DuplicateVariableResult);

    REQUIRE(Document.RenameVariable(AddedVariableId, "LoopCounter"));
    CHECK(Document.FindVariable(AddedVariableId) != nullptr);
    CHECK(Document.FindVariable(AddedVariableId)->Name == "LoopCounter");

    REQUIRE(Document.SetVariableDefault(AddedVariableId, MakeSerializedValue(12)));
    CHECK(Document.FindVariable(AddedVariableId)->DefaultValue.Type == StaticTypeId<int>());

    REQUIRE(Document.ClearVariableDefault(AddedVariableId));
    CHECK(Document.FindVariable(AddedVariableId)->DefaultValue.Type == TypeId{});

    REQUIRE(Document.RemoveVariable(ExistingVariableId));
    CHECK(Document.FindVariable(ExistingVariableId) == nullptr);
    CHECK(Document.Asset().Nodes.empty());
    CHECK(Document.Asset().EditorState.Nodes.empty());
    REQUIRE(Document.Asset().EditorState.Comments.size() == 1);
    CHECK(Document.Asset().EditorState.Comments.front().NodeIds.empty());
    CHECK(Document.Selection().NodeIds.empty());
    CHECK(Document.Selection().VariableIds.empty());
}

TEST_CASE("Conduit editor schema describes graph variables as Get and Set nodes", "[Conduit][Editor]")
{
    RegisterBuiltinTypes();

    const Uuid VariableId = NewUuid();

    GraphAsset Asset{};
    Asset.Variables = {
        GraphVariableAsset{
            .Id = VariableId,
            .Name = "Label",
            .Type = StaticTypeId<std::string>(),
            .DefaultValue = MakeSerializedValue(std::string("Ready")),
        },
    };

    SchemaRegistry Schema{};
    Schema.RebuildBuiltins();

    const auto VariableDescriptors = Schema.DescribeVariables(Asset);
    REQUIRE(VariableDescriptors.size() == 2);

    const auto GetIt = std::find_if(VariableDescriptors.begin(), VariableDescriptors.end(), [](const SchemaNodeDescriptor& Descriptor) {
        return Descriptor.LoweredKind == EGraphAssetNodeKind::VariableGet;
    });
    REQUIRE(GetIt != VariableDescriptors.end());
    CHECK(GetIt->Family == ESchemaNodeFamily::Variable);
    CHECK(GetIt->IsPure);
    CHECK(GetIt->VariableId == VariableId);
    CHECK(GetIt->DisplayName == "Get Label");
    REQUIRE(GetIt->Pins.size() == 1);
    CHECK(GetIt->Pins.front().Direction == ESchemaPinDirection::Output);
    CHECK(GetIt->Pins.front().Type.Type == StaticTypeId<std::string>());
    CHECK(GetIt->Pins.front().Type.Kind == ESlotKind::Value);
    CHECK_FALSE(GetIt->Pins.front().Type.IsExec);

    const auto SetIt = std::find_if(VariableDescriptors.begin(), VariableDescriptors.end(), [](const SchemaNodeDescriptor& Descriptor) {
        return Descriptor.LoweredKind == EGraphAssetNodeKind::VariableSet;
    });
    REQUIRE(SetIt != VariableDescriptors.end());
    CHECK(SetIt->Family == ESchemaNodeFamily::Variable);
    CHECK_FALSE(SetIt->IsPure);
    CHECK(SetIt->VariableId == VariableId);
    CHECK(SetIt->DisplayName == "Set Label");
    REQUIRE(SetIt->Pins.size() == 3);
    CHECK(SetIt->Pins[0].Type.IsExec);
    CHECK(SetIt->Pins[1].Type.Type == StaticTypeId<std::string>());
    CHECK(SetIt->Pins[1].SupportsLiteral);
    CHECK(SetIt->Pins[2].Type.IsExec);
}

TEST_CASE("Conduit editor service exposes typed variable inspector state", "[Conduit][Editor]")
{
    RegisterBuiltinTypes();

    const Uuid CounterId = NewUuid();
    const Uuid EnabledId = NewUuid();
    const Uuid LabelId = NewUuid();
    const Uuid PositionId = NewUuid();

    GraphAsset Asset{};
    Asset.Name = "ServiceDoc";
    Asset.Variables = {
        GraphVariableAsset{
            .Id = CounterId,
            .Name = "Counter",
            .Type = StaticTypeId<int>(),
            .DefaultValue = MakeSerializedValue(12),
        },
        GraphVariableAsset{
            .Id = EnabledId,
            .Name = "Enabled",
            .Type = StaticTypeId<bool>(),
        },
        GraphVariableAsset{
            .Id = LabelId,
            .Name = "Label",
            .Type = StaticTypeId<std::string>(),
            .DefaultValue = MakeSerializedValue(std::string("Ready")),
        },
        GraphVariableAsset{
            .Id = PositionId,
            .Name = "Position",
            .Type = StaticTypeId<Vec3>(),
        },
    };

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/ServiceDoc", "ServiceDoc", Asset);
    REQUIRE(OpenResult);

    const auto Entries = Service.ActiveVariableEntries();
    REQUIRE(Entries.size() == 4);
    CHECK(Entries[0].Name == "Counter");
    CHECK(Entries[1].Name == "Enabled");
    CHECK(Entries[2].Name == "Label");
    CHECK(Entries[3].Name == "Position");
    CHECK(Entries[0].HasDefault);
    CHECK_FALSE(Entries[1].HasDefault);

    REQUIRE(Service.SelectVariable(CounterId));
    {
        const auto Inspector = Service.ActiveVariableInspectorView();
        CHECK(Inspector.HasSelection);
        CHECK(Inspector.VariableId == CounterId);
        CHECK(Inspector.DefaultEditorKind == EVariableDefaultEditorKind::Text);
        CHECK(Inspector.TextValue == "12");
    }

    REQUIRE(Service.SetSelectedVariableDefaultText("64"));
    CHECK(Service.SelectedVariable() != nullptr);
    CHECK(Service.SelectedVariable()->DefaultValue.Type == StaticTypeId<int>());
    {
        const auto Inspector = Service.ActiveVariableInspectorView();
        CHECK(Inspector.TextValue == "64");
    }

    REQUIRE(Service.SelectVariable(EnabledId));
    {
        const auto Inspector = Service.ActiveVariableInspectorView();
        CHECK(Inspector.DefaultEditorKind == EVariableDefaultEditorKind::Bool);
        CHECK_FALSE(Inspector.BoolValue);
    }
    REQUIRE(Service.SetSelectedVariableDefaultBool(true));
    {
        const auto Inspector = Service.ActiveVariableInspectorView();
        CHECK(Inspector.BoolValue);
        CHECK(Inspector.HasDefault);
    }

    REQUIRE(Service.SelectVariable(PositionId));
    {
        const auto Inspector = Service.ActiveVariableInspectorView();
        CHECK(Inspector.DefaultEditorKind == EVariableDefaultEditorKind::Complex);
        CHECK(Inspector.ComplexType == StaticTypeId<Vec3>());
        CHECK(Inspector.ComplexObject != nullptr);
    }

    REQUIRE(Service.SetSelectedVariableType(StaticTypeId<float>()));
    {
        const auto Inspector = Service.ActiveVariableInspectorView();
        CHECK(Inspector.DefaultEditorKind == EVariableDefaultEditorKind::Text);
        CHECK_FALSE(Inspector.HasDefault);
        CHECK(Inspector.TextValue.empty());
    }

    const auto Types = Service.AvailableVariableTypes();
    CHECK(std::find_if(Types.begin(), Types.end(), [](const VariableTypeOption& Entry) {
              return Entry.Type == StaticTypeId<int>();
          }) != Types.end());
    CHECK(std::find_if(Types.begin(), Types.end(), [](const VariableTypeOption& Entry) {
              return Entry.Type == StaticTypeId<std::string>();
          }) != Types.end());
    CHECK(std::find_if(Types.begin(), Types.end(), [](const VariableTypeOption& Entry) {
              return Entry.Type == StaticTypeId<BaseNode>() && Entry.Label == "BaseNode";
          }) == Types.end());
}

TEST_CASE("Conduit editor service manages Conduit class documents", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    ClassAsset Asset{};
    Asset.Name = "EnemyLogic";

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenClassDocument("Conduit/EnemyLogic.conduitclass", "EnemyLogic", Asset);
    REQUIRE(OpenResult);

    CHECK(Service.ActiveDocument() == nullptr);
    REQUIRE(Service.ActiveClassDocument() != nullptr);
    CHECK(Service.ActiveClassDocument()->Asset().Name == "EnemyLogic");

    const auto HostTypes = Service.AvailableClassHostTypes();
    CHECK(std::find_if(HostTypes.begin(), HostTypes.end(), [](const ClassHostTypeOption& Entry) {
              return Entry.Type == StaticTypeId<BaseNode>() && Entry.Label == "BaseNode";
          }) != HostTypes.end());
    CHECK(std::find_if(HostTypes.begin(), HostTypes.end(), [](const ClassHostTypeOption& Entry) {
              return Entry.Type == StaticTypeId<ConduitEditorNodeHost>() && Entry.Label == "ConduitEditorNodeHost";
          }) != HostTypes.end());
    CHECK(std::find_if(HostTypes.begin(), HostTypes.end(), [](const ClassHostTypeOption& Entry) {
              return Entry.Type == StaticTypeId<ConduitEditorDotNamedHost>() && Entry.Label == "ConduitEditorDotNamedHost";
          }) != HostTypes.end());

    REQUIRE(Service.RenameActiveClass("EnemyBrain"));
    REQUIRE(Service.SetActiveClassHostType(StaticTypeId<ConduitEditorNodeHost>()));
    REQUIRE(Service.SetActiveClassGraph("Conduit/Behavior.conduitgraph"));

    const auto Inspector = Service.ActiveClassInspectorView();
    CHECK(Inspector.HasSelection);
    CHECK(Inspector.Name == "EnemyBrain");
    CHECK(Inspector.HostType == StaticTypeId<ConduitEditorNodeHost>());
    CHECK(Inspector.GraphAssetKey == "Conduit/Behavior.conduitgraph");

    const auto Workspace = Service.ActiveWorkspaceView();
    CHECK(Workspace.Kind == EWorkspaceDocumentKind::Class);
    CHECK(Workspace.HostTypeLabel == "ConduitEditorNodeHost");
    CHECK(Workspace.GraphAssetLabel == "Conduit/Behavior.conduitgraph");
}

TEST_CASE("Conduit editor service exposes schema palette and authored node outline", "[Conduit][Editor]")
{
    RegisterBuiltinTypes();

    const Uuid VariableId = NewUuid();

    GraphAsset Asset{};
    Asset.Name = "NodeDoc";
    Asset.Variables = {
        GraphVariableAsset{
            .Id = VariableId,
            .Name = "Counter",
            .Type = StaticTypeId<int>(),
        },
    };

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/NodeDoc", "NodeDoc", Asset);
    REQUIRE(OpenResult);

    const auto PaletteEntries = Service.ActivePaletteEntries();
    const std::string VariableGetStableId = "variable.get." + ToString(VariableId);
    CHECK(std::find_if(PaletteEntries.begin(), PaletteEntries.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "entry.Tick";
          }) != PaletteEntries.end());
    CHECK(std::find_if(PaletteEntries.begin(), PaletteEntries.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "builtin.label";
          }) != PaletteEntries.end());
    CHECK(std::find_if(PaletteEntries.begin(), PaletteEntries.end(), [&VariableGetStableId](const PaletteEntryView& Entry) {
              return Entry.StableId == VariableGetStableId;
          }) != PaletteEntries.end());

    auto TickNodeResult = Service.SpawnNode("entry.Tick");
    REQUIRE(TickNodeResult);
    CHECK((*TickNodeResult)->Kind == EGraphAssetNodeKind::EntryPoint);
    CHECK((*TickNodeResult)->BuiltinEntryPoint == EBuiltinEntryPoint::Tick);

    auto DuplicateTickResult = Service.SpawnNode("entry.Tick");
    CHECK_FALSE(DuplicateTickResult);

    auto LabelNodeResult = Service.SpawnNode("builtin.label");
    REQUIRE(LabelNodeResult);
    CHECK((*LabelNodeResult)->Kind == EGraphAssetNodeKind::Label);
    CHECK((*LabelNodeResult)->LabelName == "Label");

    auto VariableGetResult = Service.SpawnNode(VariableGetStableId);
    REQUIRE(VariableGetResult);
    CHECK((*VariableGetResult)->Kind == EGraphAssetNodeKind::VariableGet);
    CHECK((*VariableGetResult)->VariableId == VariableId);

    const auto NodeEntries = Service.ActiveNodeEntries();
    REQUIRE(NodeEntries.size() == 3);
    CHECK(NodeEntries[0].Title == "Tick");
    CHECK(NodeEntries[1].Title == "Label Label");
    CHECK(NodeEntries[2].Title == "Get Counter");
    CHECK(NodeEntries[2].Selected);

    REQUIRE(Service.SelectNode((*LabelNodeResult)->Id));
    {
        const auto SelectedEntries = Service.ActiveNodeEntries();
        REQUIRE(SelectedEntries.size() == 3);
        CHECK_FALSE(SelectedEntries[0].Selected);
        CHECK(SelectedEntries[1].Selected);
        CHECK_FALSE(SelectedEntries[2].Selected);
    }

    REQUIRE(Service.RemoveSelectedNode());
    {
        const auto SelectedEntries = Service.ActiveNodeEntries();
        REQUIRE(SelectedEntries.size() == 2);
        CHECK(SelectedEntries[0].Title == "Tick");
        CHECK(SelectedEntries[1].Title == "Get Counter");
    }
}

TEST_CASE("Conduit editor service exposes editable node inspector state", "[Conduit][Editor]")
{
    RegisterBuiltinTypes();

    ConduitEditorService Service{};
    GraphAsset Asset{};
    Asset.Name = "InspectorDoc";

    auto OpenResult = Service.OpenDocument("Conduit/InspectorDoc", "InspectorDoc", Asset);
    REQUIRE(OpenResult);

    auto CustomEntryResult = Service.SpawnNode("entry.custom");
    REQUIRE(CustomEntryResult);
    REQUIRE(Service.SelectNode((*CustomEntryResult)->Id));

    {
        const auto Inspector = Service.ActiveNodeInspectorView();
        CHECK(Inspector.HasSelection);
        CHECK(Inspector.Kind == EGraphAssetNodeKind::EntryPoint);
        CHECK(Inspector.CanEditPrimaryText);
        CHECK(Inspector.PrimaryTextLabel == "Entry Name");
        CHECK(Inspector.PrimaryTextValue == "Entry");
        CHECK_FALSE(Inspector.CanEditSecondaryText);
    }

    REQUIRE(Service.SetSelectedNodePrimaryText("OnInteract"));
    CHECK(Service.ActiveDocument()->FindNode((*CustomEntryResult)->Id)->EntryPointName == "OnInteract");

    auto LabelNodeResult = Service.SpawnNode("builtin.label");
    REQUIRE(LabelNodeResult);
    REQUIRE(Service.SelectNode((*LabelNodeResult)->Id));
    REQUIRE(Service.SetSelectedNodePrimaryText("LoopStart"));
    CHECK(Service.ActiveDocument()->FindNode((*LabelNodeResult)->Id)->LabelName == "LoopStart");

    auto BranchNodeResult = Service.SpawnNode("builtin.branch");
    REQUIRE(BranchNodeResult);
    REQUIRE(Service.SelectNode((*BranchNodeResult)->Id));

    {
        const auto Inspector = Service.ActiveNodeInspectorView();
        CHECK(Inspector.HasSelection);
        CHECK(Inspector.Kind == EGraphAssetNodeKind::Branch);
        CHECK(Inspector.CanEditPrimaryText);
        CHECK(Inspector.PrimaryTextLabel == "True Label");
        CHECK(Inspector.CanEditSecondaryText);
        CHECK(Inspector.SecondaryTextLabel == "False Label");
    }

    REQUIRE(Service.SetSelectedNodePrimaryText("LoopStart"));
    REQUIRE(Service.SetSelectedNodeSecondaryText("LoopExit"));
    const GraphNodeAsset* BranchNode = Service.ActiveDocument()->FindNode((*BranchNodeResult)->Id);
    REQUIRE(BranchNode != nullptr);
    CHECK(BranchNode->LabelName == "LoopStart");
    CHECK(BranchNode->FalseLabelName == "LoopExit");
}

TEST_CASE("Conduit editor service exposes editable reflected node input defaults", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/NodeInputDefaultsDoc", "NodeInputDefaultsDoc", GraphAsset{});
    REQUIRE(OpenResult);
    REQUIRE(Service.SetActiveGraphSelfType(StaticTypeId<ConduitEditorNodeHost>()));

    auto CallNodeResult = Service.SpawnNode("self.method.ConduitEditorNodeHost.AddScore");
    REQUIRE(CallNodeResult);
    REQUIRE(Service.SelectNode((*CallNodeResult)->Id));

    {
        const auto Inspector = Service.ActiveNodeInspectorView();
        REQUIRE(Inspector.HasSelection);
        REQUIRE(Inspector.InputDefaults.size() == 1);
        CHECK(Inspector.InputDefaults[0].PinKey == "Arg0");
        CHECK(Inspector.InputDefaults[0].DisplayName == "Delta");
        CHECK(Inspector.InputDefaults[0].Type == StaticTypeId<int>());
        CHECK(Inspector.InputDefaults[0].DefaultEditorKind == EVariableDefaultEditorKind::Text);
        CHECK_FALSE(Inspector.InputDefaults[0].Connected);
        CHECK_FALSE(Inspector.InputDefaults[0].HasDefault);
        CHECK(Inspector.InputDefaults[0].Tooltip.find("Signed amount to add") != std::string::npos);
    }

    REQUIRE(Service.SetSelectedNodeInputDefaultText("Arg0", "7"));

    {
        const auto Inspector = Service.ActiveNodeInspectorView();
        REQUIRE(Inspector.InputDefaults.size() == 1);
        CHECK(Inspector.InputDefaults[0].HasDefault);
        CHECK(Inspector.InputDefaults[0].TextValue == "7");
    }

    const GraphNodeAsset* StoredNode = Service.ActiveDocument()->FindNode((*CallNodeResult)->Id);
    REQUIRE(StoredNode != nullptr);
    REQUIRE(StoredNode->InputDefaults.size() == 1);
    CHECK(StoredNode->InputDefaults[0].PinKey == "Arg0");
    CHECK(StoredNode->InputDefaults[0].Value.Type == StaticTypeId<int>());
    int DecodedValue = 0;
    REQUIRE(DeserializeReflectedValueInto(StoredNode->InputDefaults[0].Value.Type,
                                          &DecodedValue,
                                          StoredNode->InputDefaults[0].Value.Bytes.data(),
                                          StoredNode->InputDefaults[0].Value.Bytes.size()));
    CHECK(DecodedValue == 7);

    REQUIRE(Service.ClearSelectedNodeInputDefault("Arg0"));

    {
        const auto Inspector = Service.ActiveNodeInspectorView();
        REQUIRE(Inspector.InputDefaults.size() == 1);
        CHECK_FALSE(Inspector.InputDefaults[0].HasDefault);
        CHECK(Inspector.InputDefaults[0].TextValue.empty());
    }

    StoredNode = Service.ActiveDocument()->FindNode((*CallNodeResult)->Id);
    REQUIRE(StoredNode != nullptr);
    CHECK(StoredNode->InputDefaults.empty());
}

TEST_CASE("Conduit editor service exposes and persists graph canvas state", "[Conduit][Editor]")
{
    RegisterBuiltinTypes();

    const Uuid TickId = NewUuid();
    const Uuid JumpId = NewUuid();
    const Uuid LabelId = NewUuid();
    const Uuid CommentId = NewUuid();

    GraphAsset Asset{};
    Asset.Name = "CanvasDoc";
    Asset.Nodes = {
        GraphNodeAsset{
            .Id = TickId,
            .Kind = EGraphAssetNodeKind::EntryPoint,
            .BuiltinEntryPoint = EBuiltinEntryPoint::Tick,
        },
        GraphNodeAsset{
            .Id = JumpId,
            .Kind = EGraphAssetNodeKind::Jump,
            .LabelName = "LoopStart",
        },
        GraphNodeAsset{
            .Id = LabelId,
            .Kind = EGraphAssetNodeKind::Label,
            .LabelName = "LoopStart",
        },
    };
    Asset.EditorState.Viewport = GraphViewportAsset{
        .PanX = 48.0f,
        .PanY = 24.0f,
        .Zoom = 1.2f,
    };
    Asset.EditorState.Nodes = {
        GraphNodeEditorAsset{
            .NodeId = TickId,
            .X = 96.0f,
            .Y = 128.0f,
            .Width = 280.0f,
        },
        GraphNodeEditorAsset{
            .NodeId = JumpId,
            .X = 280.0f,
            .Y = 168.0f,
            .Width = 240.0f,
        },
        GraphNodeEditorAsset{
            .NodeId = LabelId,
            .X = 420.0f,
            .Y = 256.0f,
            .Width = 240.0f,
            .IsCollapsed = true,
        },
    };
    Asset.EditorState.Comments = {
        GraphCommentAsset{
            .Id = CommentId,
            .Title = "Loop Body",
            .X = 72.0f,
            .Y = 92.0f,
            .Width = 640.0f,
            .Height = 360.0f,
            .ColorRgba = 0x445E7DCCu,
            .NodeIds = {TickId, JumpId, LabelId},
        },
    };

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/CanvasDoc", "CanvasDoc", Asset);
    REQUIRE(OpenResult);

    auto CanvasView = Service.ActiveCanvasView();
    CHECK(CanvasView.Viewport.PanX == Catch::Approx(48.0f));
    CHECK(CanvasView.Viewport.PanY == Catch::Approx(24.0f));
    CHECK(CanvasView.Viewport.Zoom == Catch::Approx(1.2f));
    REQUIRE(CanvasView.Nodes.size() == 3);
    CHECK(CanvasView.Nodes[0].Id == TickId);
    CHECK(CanvasView.Nodes[0].X == Catch::Approx(96.0f));
    CHECK(CanvasView.Nodes[0].Width == Catch::Approx(280.0f));
    CHECK_FALSE(CanvasView.Nodes[0].IsCollapsed);
    REQUIRE(CanvasView.Nodes[0].OutputPins.size() == 2);
    CHECK(CanvasView.Nodes[0].OutputPins[0].Name == "Out");
    CHECK(CanvasView.Nodes[0].OutputPins[0].IsExec);
    CHECK(CanvasView.Nodes[0].OutputPins[1].Name == "DeltaSeconds");
    CHECK_FALSE(CanvasView.Nodes[0].OutputPins[1].IsExec);
    CHECK(CanvasView.Nodes[1].Id == JumpId);
    REQUIRE(CanvasView.Nodes[1].InputPins.size() == 1);
    CHECK(CanvasView.Nodes[1].InputPins[0].Name == "In");
    REQUIRE(CanvasView.Nodes[1].OutputPins.size() == 1);
    CHECK(CanvasView.Nodes[1].OutputPins[0].Name == "Out");
    CHECK(CanvasView.Nodes[2].Id == LabelId);
    CHECK(CanvasView.Nodes[2].Y == Catch::Approx(256.0f));
    CHECK(CanvasView.Nodes[2].IsCollapsed);
    REQUIRE(CanvasView.Comments.size() == 1);
    CHECK(CanvasView.Comments[0].Id == CommentId);
    CHECK(CanvasView.Comments[0].Title == "Loop Body");
    REQUIRE(CanvasView.Wires.size() == 1);
    CHECK(CanvasView.Wires[0].SourceNodeId == JumpId);
    CHECK(CanvasView.Wires[0].SourcePin == "Out");
    CHECK(CanvasView.Wires[0].TargetNodeId == LabelId);
    CHECK(CanvasView.Wires[0].TargetPin == "In");
    CHECK(CanvasView.Wires[0].IsExec);

    REQUIRE(Service.SelectNode(LabelId));
    CanvasView = Service.ActiveCanvasView();
    CHECK_FALSE(CanvasView.Nodes[0].Selected);
    CHECK_FALSE(CanvasView.Nodes[1].Selected);
    CHECK(CanvasView.Nodes[2].Selected);

    REQUIRE(Service.MoveNode(LabelId, 512.0f, 300.0f));
    REQUIRE(Service.SetViewport(-32.0f, 144.0f, 1.65f));

    CanvasView = Service.ActiveCanvasView();
    CHECK(CanvasView.Viewport.PanX == Catch::Approx(-32.0f));
    CHECK(CanvasView.Viewport.PanY == Catch::Approx(144.0f));
    CHECK(CanvasView.Viewport.Zoom == Catch::Approx(1.65f));
    REQUIRE(CanvasView.Nodes.size() == 3);
    CHECK(CanvasView.Nodes[2].X == Catch::Approx(512.0f));
    CHECK(CanvasView.Nodes[2].Y == Catch::Approx(300.0f));

    const GraphDocument* Document = Service.ActiveDocument();
    REQUIRE(Document != nullptr);
    REQUIRE(Document->FindNodeEditorState(LabelId) != nullptr);
    CHECK(Document->FindNodeEditorState(LabelId)->X == Catch::Approx(512.0f));
    CHECK(Document->FindNodeEditorState(LabelId)->Y == Catch::Approx(300.0f));
    CHECK(Document->Asset().EditorState.Viewport.PanX == Catch::Approx(-32.0f));
    CHECK(Document->Asset().EditorState.Viewport.PanY == Catch::Approx(144.0f));
    CHECK(Document->Asset().EditorState.Viewport.Zoom == Catch::Approx(1.65f));
}

TEST_CASE("Conduit editor service isolates canvas-only revisions from workspace revisions", "[Conduit][Editor]")
{
    RegisterBuiltinTypes();

    const Uuid LabelId = NewUuid();

    GraphAsset Asset{};
    Asset.Name = "CanvasRevisionDoc";
    Asset.Nodes = {
        GraphNodeAsset{
            .Id = LabelId,
            .Kind = EGraphAssetNodeKind::Label,
            .LabelName = "Start",
        },
    };
    Asset.EditorState.Nodes = {
        GraphNodeEditorAsset{
            .NodeId = LabelId,
            .X = 32.0f,
            .Y = 64.0f,
        },
    };

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/CanvasRevisionDoc", "CanvasRevisionDoc", Asset);
    REQUIRE(OpenResult);

    GraphDocument* Document = Service.ActiveDocument();
    REQUIRE(Document != nullptr);
    Document->SetLastCompile(CompileOutput{
        .Diagnostics = {
            CompileDiagnostic{
                .Severity = ECompileDiagnosticSeverity::Warning,
                .Message = "cached warning",
            },
        },
    });

    const std::uint64_t InitialDocumentRevision = Document->Revision();
    const auto InitialWorkspace = Service.ActiveWorkspaceView();
    CHECK(InitialWorkspace.CanvasRevision == 0);

    REQUIRE(Service.MoveNode(LabelId, 64.0f, 96.0f));
    const auto AfterFirstMove = Service.ActiveWorkspaceView();
    CHECK(Document->Revision() == InitialDocumentRevision + 1);
    CHECK(AfterFirstMove.IsDirty);
    CHECK(AfterFirstMove.Revision > InitialWorkspace.Revision);
    CHECK(AfterFirstMove.CanvasRevision > InitialWorkspace.CanvasRevision);
    CHECK(Document->LastCompile().has_value());

    const std::uint64_t WorkspaceRevisionAfterFirstMove = AfterFirstMove.Revision;
    const std::uint64_t CanvasRevisionAfterFirstMove = AfterFirstMove.CanvasRevision;

    REQUIRE(Service.MoveNode(LabelId, 96.0f, 144.0f));
    const auto AfterSecondMove = Service.ActiveWorkspaceView();
    CHECK(Document->Revision() == InitialDocumentRevision + 2);
    CHECK(AfterSecondMove.Revision == WorkspaceRevisionAfterFirstMove);
    CHECK(AfterSecondMove.CanvasRevision > CanvasRevisionAfterFirstMove);
    CHECK(Document->LastCompile().has_value());

    const std::uint64_t CanvasRevisionAfterSecondMove = AfterSecondMove.CanvasRevision;

    REQUIRE(Service.SetViewport(12.0f, -18.0f, 1.25f));
    const auto AfterViewport = Service.ActiveWorkspaceView();
    CHECK(Document->Revision() == InitialDocumentRevision + 3);
    CHECK(AfterViewport.Revision == WorkspaceRevisionAfterFirstMove);
    CHECK(AfterViewport.CanvasRevision > CanvasRevisionAfterSecondMove);
    CHECK(Document->LastCompile().has_value());
}

TEST_CASE("Conduit editor service exposes reflected method call nodes for self and instance contexts", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    GraphAsset Asset{};
    Asset.Name = "MethodPaletteDoc";

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/MethodPaletteDoc", "MethodPaletteDoc", Asset);
    REQUIRE(OpenResult);

    const auto SelfTypes = Service.AvailableGraphSelfTypes();
    CHECK(std::find_if(SelfTypes.begin(), SelfTypes.end(), [](const GraphSelfTypeOption& Entry) {
              return Entry.Type == StaticTypeId<ConduitEditorNodeHost>() && Entry.Label == "ConduitEditorNodeHost";
          }) != SelfTypes.end());

    const auto PaletteBefore = Service.ActivePaletteEntries();
    CHECK(std::find_if(PaletteBefore.begin(), PaletteBefore.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "self.method.ConduitEditorNodeHost.AddScore";
          }) == PaletteBefore.end());
    CHECK(std::find_if(PaletteBefore.begin(), PaletteBefore.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "instance.method.ConduitEditorNodeHost.AddScore" &&
                     Entry.DisplayName == "Call ConduitEditorNodeHost::Add Score";
          }) != PaletteBefore.end());
    CHECK(std::find_if(PaletteBefore.begin(), PaletteBefore.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "instance.method.ConduitEditorComponentHost.AddCharge" &&
                     Entry.DisplayName == "Call ConduitEditorComponentHost::Add Charge";
          }) != PaletteBefore.end());

    REQUIRE(Service.SetActiveGraphSelfType(StaticTypeId<ConduitEditorNodeHost>()));
    {
        const auto Workspace = Service.ActiveWorkspaceView();
        CHECK(Workspace.SelfTypeLabel == "ConduitEditorNodeHost");
    }

    const auto PaletteAfter = Service.ActivePaletteEntries();
    CHECK(std::find_if(PaletteAfter.begin(), PaletteAfter.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "self.method.ConduitEditorNodeHost.AddScore" &&
                     Entry.DisplayName == "Call Add Score";
          }) != PaletteAfter.end());
    CHECK(std::find_if(PaletteAfter.begin(), PaletteAfter.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "self.method.ConduitEditorNodeHost.GetScore" &&
                     Entry.DisplayName == "Call Get Score";
          }) != PaletteAfter.end());
    const auto ScoreFieldIt = std::find_if(PaletteAfter.begin(), PaletteAfter.end(), [](const PaletteEntryView& Entry) {
        return Entry.StableId == "self.field.read.ConduitEditorNodeHost.Score";
    });
    REQUIRE(ScoreFieldIt != PaletteAfter.end());
    CHECK(ScoreFieldIt->Tooltip.find("Current score accumulated on the test host.") != std::string::npos);
    CHECK(ScoreFieldIt->Tooltip.find("Range: 0 to 500") != std::string::npos);
    CHECK(ScoreFieldIt->Tooltip.find("Step: 1") != std::string::npos);

    auto CallNodeResult = Service.SpawnNode("self.method.ConduitEditorNodeHost.AddScore");
    REQUIRE(CallNodeResult);
    const Uuid CallNodeId = (*CallNodeResult)->Id;
    CHECK((*CallNodeResult)->Kind == EGraphAssetNodeKind::SelfMethodCall);
    CHECK((*CallNodeResult)->OwnerType == StaticTypeId<ConduitEditorNodeHost>());
    CHECK((*CallNodeResult)->MemberName == "AddScore");
    CHECK((*CallNodeResult)->Inputs.size() == 1);

    auto FieldNodeResult = Service.SpawnNode("self.field.read.ConduitEditorNodeHost.Score");
    REQUIRE(FieldNodeResult);
    const Uuid FieldNodeId = (*FieldNodeResult)->Id;

    const auto Canvas = Service.ActiveCanvasView();
    const auto CanvasNodeIt = std::find_if(Canvas.Nodes.begin(), Canvas.Nodes.end(), [CallNodeId](const CanvasNodeView& Node) {
        return Node.Id == CallNodeId;
    });
    REQUIRE(CanvasNodeIt != Canvas.Nodes.end());
    REQUIRE(CanvasNodeIt->InputPins.size() == 2);
    CHECK(CanvasNodeIt->InputPins[0].Name == "In");
    CHECK(CanvasNodeIt->InputPins[0].IsExec);
    CHECK(CanvasNodeIt->InputPins[1].Name == "Delta");
    CHECK(CanvasNodeIt->Tooltip.find("Add a delta to the current score.") != std::string::npos);
    CHECK(CanvasNodeIt->InputPins[1].Tooltip.find("Signed amount to add to the score.") != std::string::npos);
    CHECK_FALSE(CanvasNodeIt->InputPins[1].IsExec);
    REQUIRE(CanvasNodeIt->OutputPins.size() == 1);
    CHECK(CanvasNodeIt->OutputPins[0].Name == "Out");
    CHECK(CanvasNodeIt->OutputPins[0].IsExec);

    const auto FieldCanvasNodeIt = std::find_if(Canvas.Nodes.begin(), Canvas.Nodes.end(), [FieldNodeId](const CanvasNodeView& Node) {
        return Node.Id == FieldNodeId;
    });
    REQUIRE(FieldCanvasNodeIt != Canvas.Nodes.end());
    REQUIRE(FieldCanvasNodeIt->OutputPins.size() == 1);
    CHECK(FieldCanvasNodeIt->OutputPins[0].Tooltip.find("Range: 0 to 500") != std::string::npos);
}

TEST_CASE("Conduit editor palette groups reflected members by declaring type and dedups inherited instance entries",
          "[Conduit][Editor]")
{
    EnsureConduitPaletteHierarchyRegistered();

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/PaletteHierarchyDoc", "PaletteHierarchyDoc", GraphAsset{});
    REQUIRE(OpenResult);

    const auto Palette = Service.ActivePaletteEntries();
    CHECK(std::count_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "instance.method.ConduitPaletteBaseNode.BasePing";
          }) == 1);
    CHECK(std::count_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "instance.field.read.ConduitPaletteBaseNode.BaseValue";
          }) == 1);

    const auto BaseMethodIt = std::find_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
        return Entry.StableId == "instance.method.ConduitPaletteBaseNode.BasePing";
    });
    REQUIRE(BaseMethodIt != Palette.end());
    CHECK(BaseMethodIt->Category == "Reflection/Instance/Methods/ConduitPaletteBaseNode");

    const auto DerivedMethodIt = std::find_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
        return Entry.StableId == "instance.method.ConduitPaletteDerivedNode.DerivedPing";
    });
    REQUIRE(DerivedMethodIt != Palette.end());
    CHECK(DerivedMethodIt->Category == "Reflection/Instance/Methods/ConduitPaletteDerivedNode");

    const auto BaseFieldIt = std::find_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
        return Entry.StableId == "instance.field.read.ConduitPaletteBaseNode.BaseValue";
    });
    REQUIRE(BaseFieldIt != Palette.end());
    CHECK(BaseFieldIt->Category == "Reflection/Instance/Fields/ConduitPaletteBaseNode");

    REQUIRE(Service.SetActiveGraphSelfType(StaticTypeId<ConduitPaletteDerivedNode>()));
    const auto SelfPalette = Service.ActivePaletteEntries();

    const auto SelfBaseMethodIt = std::find_if(SelfPalette.begin(), SelfPalette.end(), [](const PaletteEntryView& Entry) {
        return Entry.StableId == "self.method.ConduitPaletteBaseNode.BasePing";
    });
    REQUIRE(SelfBaseMethodIt != SelfPalette.end());
    CHECK(SelfBaseMethodIt->Category == "Reflection/Self/Methods/ConduitPaletteBaseNode");

    const auto SelfDerivedFieldIt = std::find_if(SelfPalette.begin(), SelfPalette.end(), [](const PaletteEntryView& Entry) {
        return Entry.StableId == "self.field.read.ConduitPaletteDerivedNode.DerivedValue";
    });
    REQUIRE(SelfDerivedFieldIt != SelfPalette.end());
    CHECK(SelfDerivedFieldIt->Category == "Reflection/Self/Fields/ConduitPaletteDerivedNode");
}

TEST_CASE("Conduit editor palette includes reflected world and subsystem instance methods", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/SystemPaletteDoc", "SystemPaletteDoc", GraphAsset{});
    REQUIRE(OpenResult);
    REQUIRE(Service.SetActiveGraphSelfType(StaticTypeId<ConduitEditorNodeHost>()));

    const auto Palette = Service.ActivePaletteEntries();
    CHECK(std::find_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "instance.method.IWorld.Renderer";
          }) != Palette.end());
    CHECK(std::find_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "instance.method.RendererSystem.IsInitialized";
          }) != Palette.end());
    CHECK(std::find_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "instance.method.RendererSystem.Settings";
          }) != Palette.end());
    CHECK(std::find_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "instance.field.read.RendererBootstrapSettings.CreateGraphicsApi";
          }) != Palette.end());
    CHECK(std::find_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "instance.field.write.RendererBootstrapSettings.CreateGraphicsApi";
          }) != Palette.end());

    auto QueueTextNodeResult = Service.SpawnNode("instance.method.RendererSystem.QueueText");
    REQUIRE(QueueTextNodeResult);
    CHECK((*QueueTextNodeResult)->Kind == EGraphAssetNodeKind::InstanceMethodCall);
    CHECK((*QueueTextNodeResult)->OwnerType == StaticTypeId<RendererSystem>());
    CHECK((*QueueTextNodeResult)->MemberName == "QueueText");
    CHECK((*QueueTextNodeResult)->Inputs.size() == 3);
}

TEST_CASE("Conduit editor repairs stale reflected method node input arity on open", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    const Uuid QueueTextNodeId = NewUuid();

    GraphAsset Asset{};
    Asset.Name = "StaleRendererMethodDoc";
    Asset.Nodes = {
        GraphNodeAsset{
            .Id = QueueTextNodeId,
            .Kind = EGraphAssetNodeKind::InstanceMethodCall,
            .MemberName = "QueueText",
            .OwnerType = StaticTypeId<RendererSystem>(),
        },
    };

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/StaleRendererMethodDoc", "StaleRendererMethodDoc", Asset);
    REQUIRE(OpenResult);

    const GraphDocument* Document = Service.ActiveDocument();
    REQUIRE(Document != nullptr);

    const GraphNodeAsset* QueueTextNode = Document->FindNode(QueueTextNodeId);
    REQUIRE(QueueTextNode != nullptr);
    CHECK(QueueTextNode->OwnerType == StaticTypeId<RendererSystem>());
    CHECK(QueueTextNode->Inputs.size() == 3);
}

TEST_CASE("Conduit editor service connects data pins into authored slots and visible wires", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    const Uuid VariableId = NewUuid();

    GraphAsset Asset{};
    Asset.Name = "MethodWireDoc";
    Asset.Variables = {
        GraphVariableAsset{
            .Id = VariableId,
            .Name = "Delta",
            .Type = StaticTypeId<int>(),
        },
    };

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/MethodWireDoc", "MethodWireDoc", Asset);
    REQUIRE(OpenResult);
    REQUIRE(Service.SetActiveGraphSelfType(StaticTypeId<ConduitEditorNodeHost>()));

    const std::string VariableGetStableId = "variable.get." + ToString(VariableId);
    auto VariableNodeResult = Service.SpawnNode(VariableGetStableId);
    REQUIRE(VariableNodeResult);
    const Uuid VariableNodeId = (*VariableNodeResult)->Id;

    auto MethodNodeResult = Service.SpawnNode("self.method.ConduitEditorNodeHost.AddScore");
    REQUIRE(MethodNodeResult);
    const Uuid MethodNodeId = (*MethodNodeResult)->Id;

    const Result ConnectResult = Service.ConnectPins(VariableNodeId, "Value", MethodNodeId, "Delta");
    const std::string ConnectMessage = ConnectResult ? std::string("connect ok") : ConnectResult.error().Message;
    INFO(ConnectMessage);
    REQUIRE(ConnectResult);

    const GraphDocument* Document = Service.ActiveDocument();
    REQUIRE(Document != nullptr);

    const GraphNodeAsset* VariableNode = Document->FindNode(VariableNodeId);
    const GraphNodeAsset* MethodNode = Document->FindNode(MethodNodeId);
    REQUIRE(VariableNode != nullptr);
    REQUIRE(MethodNode != nullptr);
    REQUIRE(VariableNode->Output.IsValid());
    REQUIRE(MethodNode->Inputs.size() == 1);
    REQUIRE(MethodNode->Inputs[0].IsValid());
    CHECK(MethodNode->Inputs[0].Value == VariableNode->Output.Value);
    REQUIRE(Document->Asset().Slots.size() == 1);

    const auto Canvas = Service.ActiveCanvasView();
    const auto WireIt = std::find_if(Canvas.Wires.begin(), Canvas.Wires.end(), [VariableNodeId, MethodNodeId](const CanvasWireView& Wire) {
        return Wire.SourceNodeId == VariableNodeId &&
               Wire.SourcePin == "Value" &&
               Wire.TargetNodeId == MethodNodeId &&
               Wire.TargetPin == "Delta";
    });
    REQUIRE(WireIt != Canvas.Wires.end());
    CHECK_FALSE(WireIt->IsExec);
    CHECK(WireIt->Kind == ESlotKind::Value);
}

TEST_CASE("Conduit editor service connects pointer outputs into instance target pins", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    GraphAsset Asset{};
    Asset.Name = "PointerTargetDoc";

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/PointerTargetDoc", "PointerTargetDoc", Asset);
    REQUIRE(OpenResult);
    REQUIRE(Service.SetActiveGraphSelfType(StaticTypeId<ConduitEditorNodeHost>()));

    auto PointerNodeResult = Service.SpawnNode("self.method.ConduitEditorNodeHost.GetSelfPointer");
    REQUIRE(PointerNodeResult);
    const Uuid PointerNodeId = (*PointerNodeResult)->Id;

    auto TargetMethodNodeResult = Service.SpawnNode("instance.method.ConduitEditorNodeHost.AddScore");
    REQUIRE(TargetMethodNodeResult);
    const Uuid TargetMethodNodeId = (*TargetMethodNodeResult)->Id;

    const Result ConnectResult = Service.ConnectPins(PointerNodeId, "Return", TargetMethodNodeId, "Target");
    const std::string ConnectMessage = ConnectResult ? std::string("connect ok") : ConnectResult.error().Message;
    INFO(ConnectMessage);
    REQUIRE(ConnectResult);

    const GraphDocument* Document = Service.ActiveDocument();
    REQUIRE(Document != nullptr);

    const GraphNodeAsset* PointerNode = Document->FindNode(PointerNodeId);
    const GraphNodeAsset* TargetMethodNode = Document->FindNode(TargetMethodNodeId);
    REQUIRE(PointerNode != nullptr);
    REQUIRE(TargetMethodNode != nullptr);
    REQUIRE(PointerNode->ReturnSlot.IsValid());
    REQUIRE(TargetMethodNode->Instance.IsValid());
    CHECK(TargetMethodNode->Instance.Value == PointerNode->ReturnSlot.Value);

    const auto Canvas = Service.ActiveCanvasView();
    const auto WireIt = std::find_if(Canvas.Wires.begin(), Canvas.Wires.end(), [PointerNodeId, TargetMethodNodeId](const CanvasWireView& Wire) {
        return Wire.SourceNodeId == PointerNodeId &&
               Wire.SourcePin == "Return" &&
               Wire.TargetNodeId == TargetMethodNodeId &&
               Wire.TargetPin == "Target";
    });
    REQUIRE(WireIt != Canvas.Wires.end());
    CHECK_FALSE(WireIt->IsExec);
    CHECK(WireIt->Kind == ESlotKind::Value);
}

TEST_CASE("Conduit editor service exposes reference-return methods as pointer outputs for instance targets", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/ReferenceTargetDoc", "ReferenceTargetDoc", GraphAsset{});
    REQUIRE(OpenResult);
    REQUIRE(Service.SetActiveGraphSelfType(StaticTypeId<ConduitEditorNodeHost>()));

    const auto Palette = Service.ActivePaletteEntries();
    CHECK(std::find_if(Palette.begin(), Palette.end(), [](const PaletteEntryView& Entry) {
              return Entry.StableId == "self.method.ConduitEditorNodeHost.GetSelfReference";
          }) != Palette.end());

    auto SourceNodeResult = Service.SpawnNode("self.method.ConduitEditorNodeHost.GetSelfReference");
    REQUIRE(SourceNodeResult);
    const Uuid SourceNodeId = (*SourceNodeResult)->Id;

    auto TargetNodeResult = Service.SpawnNode("instance.method.ConduitEditorNodeHost.AddScore");
    REQUIRE(TargetNodeResult);
    const Uuid TargetNodeId = (*TargetNodeResult)->Id;

    const Result ConnectResult = Service.ConnectPins(SourceNodeId, "Return", TargetNodeId, "Target");
    const std::string ConnectMessage = ConnectResult ? std::string("connect ok") : ConnectResult.error().Message;
    INFO(ConnectMessage);
    REQUIRE(ConnectResult);

    const GraphDocument* Document = Service.ActiveDocument();
    REQUIRE(Document != nullptr);

    const GraphNodeAsset* SourceNode = Document->FindNode(SourceNodeId);
    const GraphNodeAsset* TargetNode = Document->FindNode(TargetNodeId);
    REQUIRE(SourceNode != nullptr);
    REQUIRE(TargetNode != nullptr);
    REQUIRE(SourceNode->ReturnSlot.IsValid());
    REQUIRE(TargetNode->Instance.IsValid());
    CHECK(TargetNode->Instance.Value == SourceNode->ReturnSlot.Value);
}

TEST_CASE("Conduit editor service connects exec pins into authored node targets and visible wires", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    const Uuid VariableId = NewUuid();

    GraphAsset Asset{};
    Asset.Name = "ExecWireDoc";
    Asset.Variables = {
        GraphVariableAsset{
            .Id = VariableId,
            .Name = "Score",
            .Type = StaticTypeId<int>(),
        },
    };

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/ExecWireDoc", "ExecWireDoc", Asset);
    REQUIRE(OpenResult);

    auto TickNodeResult = Service.SpawnNode("entry.Tick");
    REQUIRE(TickNodeResult);
    const Uuid TickNodeId = (*TickNodeResult)->Id;

    const std::string VariableSetStableId = "variable.set." + ToString(VariableId);
    auto SetNodeResult = Service.SpawnNode(VariableSetStableId);
    REQUIRE(SetNodeResult);
    const Uuid SetNodeId = (*SetNodeResult)->Id;

    const Result ConnectResult = Service.ConnectPins(TickNodeId, "Out", SetNodeId, "In");
    REQUIRE(ConnectResult);

    const GraphDocument* Document = Service.ActiveDocument();
    REQUIRE(Document != nullptr);

    const GraphNodeAsset* TickNode = Document->FindNode(TickNodeId);
    REQUIRE(TickNode != nullptr);
    CHECK(TickNode->ExecTargetNodeId == SetNodeId);

    const auto Canvas = Service.ActiveCanvasView();
    const auto WireIt = std::find_if(Canvas.Wires.begin(), Canvas.Wires.end(), [TickNodeId, SetNodeId](const CanvasWireView& Wire) {
        return Wire.SourceNodeId == TickNodeId &&
               Wire.SourcePin == "Out" &&
               Wire.TargetNodeId == SetNodeId &&
               Wire.TargetPin == "In" &&
               Wire.IsExec;
    });
    REQUIRE(WireIt != Canvas.Wires.end());
}

TEST_CASE("Conduit editor service builds compatible spawn-menu entries from a dragged output pin", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    const Uuid VariableId = NewUuid();

    GraphAsset Asset{};
    Asset.Name = "SpawnMenuDoc";
    Asset.Variables = {
        GraphVariableAsset{
            .Id = VariableId,
            .Name = "Delta",
            .Type = StaticTypeId<int>(),
        },
    };

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/SpawnMenuDoc", "SpawnMenuDoc", Asset);
    REQUIRE(OpenResult);
    REQUIRE(Service.SetActiveGraphSelfType(StaticTypeId<ConduitEditorNodeHost>()));

    const std::string VariableGetStableId = "variable.get." + ToString(VariableId);
    auto VariableNodeResult = Service.SpawnNode(VariableGetStableId);
    REQUIRE(VariableNodeResult);

    GraphSpawnMenuRequest Request{};
    Request.GraphX = 512.0f;
    Request.GraphY = 224.0f;
    Request.SourceNodeId = (*VariableNodeResult)->Id;
    Request.SourcePin = "Value";
    Request.FromPinDrag = true;

    const auto Entries = Service.BuildSpawnMenuEntries(Request);
    CHECK_FALSE(Entries.empty());
    CHECK(std::find_if(Entries.begin(), Entries.end(), [](const SpawnMenuEntryView& Entry) {
              return Entry.StableId == "self.method.ConduitEditorNodeHost.AddScore" && Entry.TargetPin == "Delta";
          }) != Entries.end());
    CHECK(std::find_if(Entries.begin(), Entries.end(), [VariableId](const SpawnMenuEntryView& Entry) {
              return Entry.StableId == ("variable.set." + ToString(VariableId)) && Entry.TargetPin == "Value";
          }) != Entries.end());
    CHECK(std::find_if(Entries.begin(), Entries.end(), [](const SpawnMenuEntryView& Entry) {
              return Entry.StableId == "entry.Tick";
          }) == Entries.end());
}

TEST_CASE("Conduit editor service builds compatible spawn-menu entries from pointer outputs into instance targets", "[Conduit][Editor]")
{
    EnsureConduitEditorNodeHostRegistered();

    ConduitEditorService Service{};
    auto OpenResult = Service.OpenDocument("Conduit/PointerSpawnMenuDoc", "PointerSpawnMenuDoc", GraphAsset{});
    REQUIRE(OpenResult);
    REQUIRE(Service.SetActiveGraphSelfType(StaticTypeId<ConduitEditorNodeHost>()));

    auto SourceNodeResult = Service.SpawnNode("self.method.ConduitEditorNodeHost.GetSelfReference");
    REQUIRE(SourceNodeResult);

    GraphSpawnMenuRequest Request{};
    Request.GraphX = 320.0f;
    Request.GraphY = 192.0f;
    Request.SourceNodeId = (*SourceNodeResult)->Id;
    Request.SourcePin = "Return";
    Request.FromPinDrag = true;

    const auto Entries = Service.BuildSpawnMenuEntries(Request);
    CHECK_FALSE(Entries.empty());
    CHECK(std::find_if(Entries.begin(), Entries.end(), [](const SpawnMenuEntryView& Entry) {
              return Entry.StableId == "instance.method.ConduitEditorNodeHost.AddScore" &&
                     Entry.TargetPin == "Target";
          }) != Entries.end());
    CHECK(std::find_if(Entries.begin(), Entries.end(), [](const SpawnMenuEntryView& Entry) {
              return Entry.StableId == "instance.method.ConduitEditorNodeHost.GetScore" &&
                     Entry.TargetPin == "Target";
          }) != Entries.end());
}

TEST_CASE("Conduit graph canvas connects when dropped on the input row label area", "[Conduit][Editor]")
{
    UIConduitGraphCanvas Canvas{};
    Canvas.Initialize(nullptr, {});
    Canvas.Arrange(SnAPI::UI::UIRect{0.0f, 0.0f, 900.0f, 700.0f});

    GraphCanvasView View{};
    View.Viewport.PanX = 0.0f;
    View.Viewport.PanY = 0.0f;
    View.Viewport.Zoom = 1.0f;
    View.Nodes = {
        CanvasNodeView{
            .Id = NewUuid(),
            .Title = "Source",
            .Detail = {},
            .X = 100.0f,
            .Y = 100.0f,
            .Width = 240.0f,
            .IsCollapsed = false,
            .Selected = false,
            .InputPins = {},
            .OutputPins = {
                CanvasPinView{
                    .Name = "Value",
                    .TypeLabel = "int",
                    .Kind = ESlotKind::Value,
                    .IsInput = false,
                    .IsExec = false,
                },
            },
        },
        CanvasNodeView{
            .Id = NewUuid(),
            .Title = "Target",
            .Detail = {},
            .X = 450.0f,
            .Y = 100.0f,
            .Width = 240.0f,
            .IsCollapsed = false,
            .Selected = false,
            .InputPins = {
                CanvasPinView{
                    .Name = "Arg0",
                    .TypeLabel = "int",
                    .Kind = ESlotKind::Value,
                    .IsInput = true,
                    .IsExec = false,
                },
            },
            .OutputPins = {},
        },
    };
    Canvas.SetViewState(std::move(View));

    bool Connected = false;
    Uuid ConnectedSourceNode{};
    std::string ConnectedSourcePin{};
    Uuid ConnectedTargetNode{};
    std::string ConnectedTargetPin{};
    Canvas.SetPinConnectedHandler(
        SnAPI::UI::TDelegate<void(const Uuid&, const std::string&, const Uuid&, const std::string&)>::Bind(
            [&](const Uuid& SourceNodeId,
                const std::string& SourcePin,
                const Uuid& TargetNodeId,
                const std::string& TargetPin) {
                Connected = true;
                ConnectedSourceNode = SourceNodeId;
                ConnectedSourcePin = SourcePin;
                ConnectedTargetNode = TargetNodeId;
                ConnectedTargetPin = TargetPin;
            }));

    SnAPI::UI::PointerEvent Pointer{};
    Pointer.Position = SnAPI::UI::UIPoint{328.0f, 160.0f};
    Pointer.LeftDown = true;
    SnAPI::UI::RoutedEventContext PointerDown(
        SnAPI::UI::RoutedEventTypes::PointerDown.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerDown.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerDown);
    REQUIRE(PointerDown.Handled());

    Pointer.Position = SnAPI::UI::UIPoint{620.0f, 160.0f};
    Pointer.LeftDown = true;
    SnAPI::UI::RoutedEventContext PointerMove(
        SnAPI::UI::RoutedEventTypes::PointerMove.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerMove.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerMove);
    REQUIRE(PointerMove.Handled());

    Pointer.Position = SnAPI::UI::UIPoint{620.0f, 160.0f};
    Pointer.LeftDown = false;
    SnAPI::UI::RoutedEventContext PointerUp(
        SnAPI::UI::RoutedEventTypes::PointerUp.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerUp.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerUp);

    REQUIRE(Connected);
    CHECK(ConnectedSourceNode == Canvas.ViewState().Nodes[0].Id);
    CHECK(ConnectedSourcePin == "Value");
    CHECK(ConnectedTargetNode == Canvas.ViewState().Nodes[1].Id);
    CHECK(ConnectedTargetPin == "Arg0");
}

TEST_CASE("Conduit graph canvas connects when button release is observed on pointer move before pointer up", "[Conduit][Editor]")
{
    UIConduitGraphCanvas Canvas{};
    Canvas.Initialize(nullptr, {});
    Canvas.Arrange(SnAPI::UI::UIRect{0.0f, 0.0f, 900.0f, 700.0f});

    GraphCanvasView View{};
    View.Viewport.PanX = 0.0f;
    View.Viewport.PanY = 0.0f;
    View.Viewport.Zoom = 1.0f;
    View.Nodes = {
        CanvasNodeView{
            .Id = NewUuid(),
            .Title = "Source",
            .Detail = {},
            .X = 100.0f,
            .Y = 100.0f,
            .Width = 240.0f,
            .IsCollapsed = false,
            .Selected = false,
            .InputPins = {},
            .OutputPins = {
                CanvasPinView{
                    .Name = "Value",
                    .TypeLabel = "int",
                    .Kind = ESlotKind::Value,
                    .IsInput = false,
                    .IsExec = false,
                },
            },
        },
        CanvasNodeView{
            .Id = NewUuid(),
            .Title = "Target",
            .Detail = {},
            .X = 450.0f,
            .Y = 100.0f,
            .Width = 240.0f,
            .IsCollapsed = false,
            .Selected = false,
            .InputPins = {
                CanvasPinView{
                    .Name = "Arg0",
                    .TypeLabel = "int",
                    .Kind = ESlotKind::Value,
                    .IsInput = true,
                    .IsExec = false,
                },
            },
            .OutputPins = {},
        },
    };
    Canvas.SetViewState(std::move(View));

    bool Connected = false;
    Canvas.SetPinConnectedHandler(
        SnAPI::UI::TDelegate<void(const Uuid&, const std::string&, const Uuid&, const std::string&)>::Bind(
            [&](const Uuid&, const std::string&, const Uuid&, const std::string&) { Connected = true; }));

    SnAPI::UI::PointerEvent Pointer{};
    Pointer.Position = SnAPI::UI::UIPoint{328.0f, 160.0f};
    Pointer.LeftDown = true;
    SnAPI::UI::RoutedEventContext PointerDown(
        SnAPI::UI::RoutedEventTypes::PointerDown.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerDown.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerDown);
    REQUIRE(PointerDown.Handled());

    Pointer.Position = SnAPI::UI::UIPoint{620.0f, 160.0f};
    Pointer.LeftDown = true;
    SnAPI::UI::RoutedEventContext PointerMoveDrag(
        SnAPI::UI::RoutedEventTypes::PointerMove.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerMoveDrag.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerMoveDrag);
    REQUIRE(PointerMoveDrag.Handled());

    Pointer.Position = SnAPI::UI::UIPoint{620.0f, 160.0f};
    Pointer.LeftDown = false;
    SnAPI::UI::RoutedEventContext PointerMoveRelease(
        SnAPI::UI::RoutedEventTypes::PointerMove.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerMoveRelease.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerMoveRelease);
    REQUIRE(PointerMoveRelease.Handled());
    REQUIRE(Connected);

    SnAPI::UI::RoutedEventContext PointerUp(
        SnAPI::UI::RoutedEventTypes::PointerUp.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerUp.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerUp);
}

TEST_CASE("Conduit graph canvas requests a spawn menu when a dragged output pin is released on empty canvas", "[Conduit][Editor]")
{
    UIConduitGraphCanvas Canvas{};
    Canvas.Initialize(nullptr, {});
    Canvas.Arrange(SnAPI::UI::UIRect{0.0f, 0.0f, 900.0f, 700.0f});

    GraphCanvasView View{};
    View.Viewport.PanX = 0.0f;
    View.Viewport.PanY = 0.0f;
    View.Viewport.Zoom = 1.0f;
    View.Nodes = {
        CanvasNodeView{
            .Id = NewUuid(),
            .Title = "Source",
            .X = 100.0f,
            .Y = 100.0f,
            .Width = 240.0f,
            .OutputPins = {
                CanvasPinView{
                    .Name = "Value",
                    .TypeLabel = "int",
                    .Kind = ESlotKind::Value,
                    .IsInput = false,
                    .IsExec = false,
                },
            },
        },
    };
    Canvas.SetViewState(std::move(View));

    bool Requested = false;
    GraphSpawnMenuRequest Request{};
    Canvas.SetSpawnMenuRequestedHandler(
        SnAPI::UI::TDelegate<void(const GraphSpawnMenuRequest&)>::Bind([&](const GraphSpawnMenuRequest& InRequest) {
            Requested = true;
            Request = InRequest;
        }));

    SnAPI::UI::PointerEvent Pointer{};
    Pointer.Position = SnAPI::UI::UIPoint{328.0f, 160.0f};
    Pointer.LeftDown = true;
    SnAPI::UI::RoutedEventContext PointerDown(
        SnAPI::UI::RoutedEventTypes::PointerDown.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerDown.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerDown);
    REQUIRE(PointerDown.Handled());

    Pointer.Position = SnAPI::UI::UIPoint{720.0f, 340.0f};
    Pointer.LeftDown = true;
    SnAPI::UI::RoutedEventContext PointerMove(
        SnAPI::UI::RoutedEventTypes::PointerMove.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerMove.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerMove);
    REQUIRE(PointerMove.Handled());

    Pointer.LeftDown = false;
    SnAPI::UI::RoutedEventContext PointerUp(
        SnAPI::UI::RoutedEventTypes::PointerUp.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerUp.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerUp);

    REQUIRE(Requested);
    CHECK(Request.FromPinDrag);
    CHECK(Request.SourceNodeId == Canvas.ViewState().Nodes[0].Id);
    CHECK(Request.SourcePin == "Value");
    CHECK(Request.GraphX == Catch::Approx(720.0f));
    CHECK(Request.GraphY == Catch::Approx(340.0f));
}

TEST_CASE("Conduit graph canvas requests a spawn menu on right click release over empty canvas", "[Conduit][Editor]")
{
    UIConduitGraphCanvas Canvas{};
    Canvas.Initialize(nullptr, {});
    Canvas.Arrange(SnAPI::UI::UIRect{0.0f, 0.0f, 900.0f, 700.0f});

    GraphCanvasView View{};
    View.Viewport.PanX = 32.0f;
    View.Viewport.PanY = 48.0f;
    View.Viewport.Zoom = 1.0f;
    Canvas.SetViewState(std::move(View));

    bool Requested = false;
    GraphSpawnMenuRequest Request{};
    Canvas.SetSpawnMenuRequestedHandler(
        SnAPI::UI::TDelegate<void(const GraphSpawnMenuRequest&)>::Bind([&](const GraphSpawnMenuRequest& InRequest) {
            Requested = true;
            Request = InRequest;
        }));

    SnAPI::UI::PointerEvent Pointer{};
    Pointer.Position = SnAPI::UI::UIPoint{400.0f, 240.0f};
    Pointer.RightDown = true;
    SnAPI::UI::RoutedEventContext PointerDown(
        SnAPI::UI::RoutedEventTypes::PointerDown.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerDown.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerDown);
    REQUIRE(PointerDown.Handled());

    Pointer.RightDown = false;
    SnAPI::UI::RoutedEventContext PointerUp(
        SnAPI::UI::RoutedEventTypes::PointerUp.Id,
        SnAPI::UI::EEventRoutePhase::Target,
        {});
    PointerUp.SetPayload(&Pointer);
    Canvas.OnRoutedEvent(PointerUp);

    REQUIRE(Requested);
    CHECK_FALSE(Request.FromPinDrag);
    CHECK(Request.SourceNodeId == Uuid{});
    CHECK(Request.SourcePin.empty());
    CHECK(Request.GraphX == Catch::Approx(432.0f));
    CHECK(Request.GraphY == Catch::Approx(288.0f));
}
