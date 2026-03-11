#include <algorithm>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Editor/GameFrameworkEditor.hpp"
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;
using namespace SnAPI::GameFramework::Conduit;
using namespace SnAPI::GameFramework::Conduit::Editor;

namespace
{

struct ConduitEditorNodeHost : BaseNode
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::ConduitEditorNodeHost";
};

struct ConduitEditorDotNamedHost : BaseNode
{
    static constexpr const char* kTypeName = "SnAPI.GameFramework.Tests.ConduitEditorDotNamedHost";
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
        .Register();
    REQUIRE(RegisterResult);

    if (!TypeRegistry::Instance().Find(StaticTypeId<ConduitEditorDotNamedHost>()))
    {
        auto DotRegisterResult = TTypeBuilder<ConduitEditorDotNamedHost>(ConduitEditorDotNamedHost::kTypeName)
            .Base<BaseNode>()
            .Constructor<>()
            .Register();
        REQUIRE(DotRegisterResult);
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
