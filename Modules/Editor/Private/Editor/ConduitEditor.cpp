#include "Conduit/Editor.h"

#include "Editor/EditorAssetService.h"
#include "BaseComponent.h"
#include "Editor/EditorCoreServices.h"
#include "BaseNode.h"
#include "IWorld.h"
#if defined(SNAPI_GF_ENABLE_INPUT)
#include "InputSystem.h"
#endif
#if defined(SNAPI_GF_ENABLE_UI)
#include "UISystem.h"
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
#include "AudioSystem.h"
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetworkSystem.h"
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "PhysicsSystem.h"
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "RendererSystem.h"
#endif
#include "Serialization.h"
#include "Uuid.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <memory>
#include <new>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace SnAPI::GameFramework::Conduit::Editor
{
namespace
{

[[nodiscard]] bool CanConduitReadField(const FieldInfo& Field)
{
    return static_cast<bool>(Field.ConstPointer) || static_cast<bool>(Field.ViewGetter) || static_cast<bool>(Field.Getter);
}

[[nodiscard]] bool CanConduitWriteField(const FieldInfo& Field)
{
    return !Field.IsConst && (static_cast<bool>(Field.RawSetter) || static_cast<bool>(Field.MutablePointer) || static_cast<bool>(Field.Setter));
}

[[nodiscard]] bool IsHandleCarrierType(const TypeId& Type)
{
    return Type == StaticTypeId<NodeHandle>() || Type == StaticTypeId<ComponentHandle>();
}

[[nodiscard]] ESlotKind ResolveSlotKindForType(const TypeId& Type)
{
    return IsHandleCarrierType(Type) ? ESlotKind::Handle : ESlotKind::Value;
}

[[nodiscard]] TypeId ResolveExpectedHandleStorageType(const TypeId& ExpectedTargetType)
{
    if (ExpectedTargetType == TypeId{})
    {
        return {};
    }
    if (IsHandleCarrierType(ExpectedTargetType))
    {
        return ExpectedTargetType;
    }
    if (TypeRegistry::Instance().IsA(ExpectedTargetType, StaticTypeId<BaseNode>()))
    {
        return StaticTypeId<NodeHandle>();
    }
    if (ComponentSerializationRegistry::Instance().Has(ExpectedTargetType))
    {
        return StaticTypeId<ComponentHandle>();
    }
    return {};
}

[[nodiscard]] bool IsPointerStorageAssignableToExpected(const TypeId& StorageType,
                                                        const TypeId& ExpectedPointerType)
{
    const TypeInfo* StorageInfo = TypeRegistry::Instance().Find(StorageType);
    const TypeInfo* ExpectedInfo = TypeRegistry::Instance().Find(ExpectedPointerType);
    if (!StorageInfo || !ExpectedInfo || !StorageInfo->IsPointer || !ExpectedInfo->IsPointer)
    {
        return false;
    }
    if (StorageInfo->PointeeType == TypeId{} || ExpectedInfo->PointeeType == TypeId{})
    {
        return false;
    }
    if (!TypeRegistry::Instance().IsA(StorageInfo->PointeeType, ExpectedInfo->PointeeType))
    {
        return false;
    }
    if (StorageInfo->PointerPointeeConst && !ExpectedInfo->PointerPointeeConst)
    {
        return false;
    }
    return true;
}

[[nodiscard]] bool IsInstanceTargetStorageCompatible(const TypeId& StorageType,
                                                     const TypeId& ExpectedTargetType,
                                                     const bool RequireMutableInstance)
{
    if (StorageType == ResolveExpectedHandleStorageType(ExpectedTargetType))
    {
        return true;
    }

    const TypeInfo* StorageInfo = TypeRegistry::Instance().Find(StorageType);
    if (!StorageInfo || !StorageInfo->IsPointer || StorageInfo->PointeeType == TypeId{})
    {
        return false;
    }
    if (!TypeRegistry::Instance().IsA(StorageInfo->PointeeType, ExpectedTargetType))
    {
        return false;
    }
    if (RequireMutableInstance && StorageInfo->PointerPointeeConst)
    {
        return false;
    }
    return true;
}

std::vector<SchemaNodeDescriptor> DescribeMembers(const TypeInfo& OwnerType,
                                                  bool SelfContext,
                                                  const std::unordered_set<TypeId, UuidHash>* AvailableInstanceTypes = nullptr);

[[nodiscard]] const SchemaNodeDescriptor* FindSchemaDescriptorForNode(const std::vector<SchemaNodeDescriptor>& Descriptors,
                                                                      const GraphNodeAsset& Node);

[[nodiscard]] SchemaPinDescriptor MakeExecPin(const std::string_view Name, const ESchemaPinDirection Direction)
{
    SchemaPinDescriptor Pin{};
    Pin.Name = std::string(Name);
    Pin.Direction = Direction;
    Pin.Type.IsExec = true;
    return Pin;
}

[[nodiscard]] SchemaPinDescriptor MakeValuePin(const std::string_view Name,
                                               const ESchemaPinDirection Direction,
                                               const TypeId& Type,
                                               const bool SupportsLiteral = false)
{
    SchemaPinDescriptor Pin{};
    Pin.Name = std::string(Name);
    Pin.Direction = Direction;
    Pin.Type.Type = Type;
    Pin.Type.Kind = ResolveSlotKindForType(Type);
    Pin.SupportsLiteral = SupportsLiteral;
    return Pin;
}

[[nodiscard]] SchemaPinDescriptor MakeHandlePin(const std::string_view Name,
                                                const ESchemaPinDirection Direction,
                                                const TypeId& ExpectedTargetType)
{
    SchemaPinDescriptor Pin{};
    Pin.Name = std::string(Name);
    Pin.Direction = Direction;
    Pin.Type.Type = ExpectedTargetType;
    Pin.Type.Kind = ESlotKind::Handle;
    return Pin;
}

[[nodiscard]] bool IsVariableNodeKind(const EGraphAssetNodeKind Kind)
{
    return Kind == EGraphAssetNodeKind::VariableGet || Kind == EGraphAssetNodeKind::VariableSet;
}

[[nodiscard]] bool IsEntryPointNameAvailable(const GraphAsset& Asset,
                                             const std::string_view Name,
                                             const std::optional<Uuid>& IgnoreId = std::nullopt)
{
    if (Name.empty())
    {
        return false;
    }

    return std::none_of(Asset.Nodes.begin(), Asset.Nodes.end(), [&Name, &IgnoreId](const GraphNodeAsset& Node) {
        return Node.Kind == EGraphAssetNodeKind::EntryPoint &&
               Node.BuiltinEntryPoint == EBuiltinEntryPoint::None &&
               Node.EntryPointName == Name &&
               (!IgnoreId.has_value() || Node.Id != *IgnoreId);
    });
}

[[nodiscard]] bool IsLabelNameAvailable(const GraphAsset& Asset,
                                        const std::string_view Name,
                                        const std::optional<Uuid>& IgnoreId = std::nullopt)
{
    if (Name.empty())
    {
        return false;
    }

    return std::none_of(Asset.Nodes.begin(), Asset.Nodes.end(), [&Name, &IgnoreId](const GraphNodeAsset& Node) {
        return Node.Kind == EGraphAssetNodeKind::Label &&
               Node.LabelName == Name &&
               (!IgnoreId.has_value() || Node.Id != *IgnoreId);
    });
}

[[nodiscard]] std::string MakeUniqueEntryPointName(const GraphAsset& Asset, const std::string_view BaseName)
{
    std::string Candidate(BaseName.empty() ? "Entry" : BaseName);
    if (IsEntryPointNameAvailable(Asset, Candidate))
    {
        return Candidate;
    }

    for (std::size_t Suffix = 2; ; ++Suffix)
    {
        Candidate = std::string(BaseName.empty() ? "Entry" : BaseName) + std::to_string(Suffix);
        if (IsEntryPointNameAvailable(Asset, Candidate))
        {
            return Candidate;
        }
    }
}

[[nodiscard]] std::string MakeUniqueLabelName(const GraphAsset& Asset, const std::string_view BaseName)
{
    std::string Candidate(BaseName.empty() ? "Label" : BaseName);
    if (IsLabelNameAvailable(Asset, Candidate))
    {
        return Candidate;
    }

    for (std::size_t Suffix = 2; ; ++Suffix)
    {
        Candidate = std::string(BaseName.empty() ? "Label" : BaseName) + std::to_string(Suffix);
        if (IsLabelNameAvailable(Asset, Candidate))
        {
            return Candidate;
        }
    }
}

[[nodiscard]] std::optional<std::string> FindFirstLabelName(const GraphAsset& Asset)
{
    const auto It = std::find_if(Asset.Nodes.begin(), Asset.Nodes.end(), [](const GraphNodeAsset& Node) {
        return Node.Kind == EGraphAssetNodeKind::Label && !Node.LabelName.empty();
    });
    if (It == Asset.Nodes.end())
    {
        return std::nullopt;
    }

    return It->LabelName;
}

[[nodiscard]] const GraphNodeAsset* FindLabelNodeByName(const GraphAsset& Asset, const std::string_view LabelName)
{
    if (LabelName.empty())
    {
        return nullptr;
    }

    const auto It = std::find_if(Asset.Nodes.begin(), Asset.Nodes.end(), [LabelName](const GraphNodeAsset& Node) {
        return Node.Kind == EGraphAssetNodeKind::Label && Node.LabelName == LabelName;
    });
    return It != Asset.Nodes.end() ? &(*It) : nullptr;
}

[[nodiscard]] Uuid* ResolveMutableExecTargetNodeId(GraphNodeAsset& Node, const std::string_view SourcePin)
{
    if (SourcePin == "Out" || SourcePin == "True")
    {
        return &Node.ExecTargetNodeId;
    }
    if (SourcePin == "False")
    {
        return &Node.FalseExecTargetNodeId;
    }
    return nullptr;
}

[[nodiscard]] const Uuid* ResolveConstExecTargetNodeId(const GraphNodeAsset& Node, const std::string_view SourcePin)
{
    if (SourcePin == "Out" || SourcePin == "True")
    {
        return &Node.ExecTargetNodeId;
    }
    if (SourcePin == "False")
    {
        return &Node.FalseExecTargetNodeId;
    }
    return nullptr;
}

[[nodiscard]] Uuid ResolveExecTargetNodeId(const GraphAsset& Asset,
                                           const GraphNodeAsset& Node,
                                           const std::string_view SourcePin)
{
    if (const Uuid* ExplicitTarget = ResolveConstExecTargetNodeId(Node, SourcePin);
        ExplicitTarget && *ExplicitTarget != Uuid{})
    {
        return *ExplicitTarget;
    }

    if (SourcePin == "Out" && Node.Kind == EGraphAssetNodeKind::Jump)
    {
        if (const GraphNodeAsset* LabelNode = FindLabelNodeByName(Asset, Node.LabelName))
        {
            return LabelNode->Id;
        }
    }
    if (Node.Kind == EGraphAssetNodeKind::Branch)
    {
        if (SourcePin == "True")
        {
            if (const GraphNodeAsset* LabelNode = FindLabelNodeByName(Asset, Node.LabelName))
            {
                return LabelNode->Id;
            }
        }
        else if (SourcePin == "False")
        {
            if (const GraphNodeAsset* LabelNode = FindLabelNodeByName(Asset, Node.FalseLabelName))
            {
                return LabelNode->Id;
            }
        }
    }

    return {};
}

[[nodiscard]] std::string ResolveExecInputPinName(const SchemaNodeDescriptor& Descriptor)
{
    const auto It = std::find_if(Descriptor.Pins.begin(), Descriptor.Pins.end(), [](const SchemaPinDescriptor& Pin) {
        return Pin.Direction == ESchemaPinDirection::Input && Pin.Type.IsExec;
    });
    return It != Descriptor.Pins.end() ? It->Name : std::string("In");
}

void DisconnectExecConnectionsToNode(GraphAsset& Asset, const std::unordered_set<Uuid, UuidHash>& RemovedNodeIds)
{
    if (RemovedNodeIds.empty())
    {
        return;
    }

    for (GraphNodeAsset& Node : Asset.Nodes)
    {
        if (RemovedNodeIds.contains(Node.ExecTargetNodeId))
        {
            Node.ExecTargetNodeId = {};
        }
        if (RemovedNodeIds.contains(Node.FalseExecTargetNodeId))
        {
            Node.FalseExecTargetNodeId = {};
        }
    }
}

void NormalizeEditorState(GraphAsset& Asset)
{
    std::unordered_set<Uuid, UuidHash> LiveNodeIds{};
    LiveNodeIds.reserve(Asset.Nodes.size());

    for (GraphNodeAsset& Node : Asset.Nodes)
    {
        if (Node.Id == Uuid{} || LiveNodeIds.contains(Node.Id))
        {
            Node.Id = NewUuid();
        }
        LiveNodeIds.insert(Node.Id);
    }

    std::unordered_map<Uuid, GraphNodeEditorAsset, UuidHash> ExistingNodeState{};
    ExistingNodeState.reserve(Asset.EditorState.Nodes.size());
    for (const GraphNodeEditorAsset& NodeState : Asset.EditorState.Nodes)
    {
        if (NodeState.NodeId != Uuid{})
        {
            ExistingNodeState.insert_or_assign(NodeState.NodeId, NodeState);
        }
    }

    std::vector<GraphNodeEditorAsset> NormalizedNodeState{};
    NormalizedNodeState.reserve(Asset.Nodes.size());
    for (std::size_t Index = 0; Index < Asset.Nodes.size(); ++Index)
    {
        const GraphNodeAsset& Node = Asset.Nodes[Index];
        if (const auto It = ExistingNodeState.find(Node.Id); It != ExistingNodeState.end())
        {
            NormalizedNodeState.push_back(It->second);
            continue;
        }

        GraphNodeEditorAsset NodeState{};
        NodeState.NodeId = Node.Id;
        NodeState.X = 96.0f + static_cast<float>(Index % 6) * 320.0f;
        NodeState.Y = 96.0f + static_cast<float>(Index / 6) * 220.0f;
        NormalizedNodeState.push_back(std::move(NodeState));
    }
    Asset.EditorState.Nodes = std::move(NormalizedNodeState);

    if (Asset.EditorState.Viewport.Zoom <= 0.0f)
    {
        Asset.EditorState.Viewport.Zoom = 1.0f;
    }

    for (GraphBookmarkAsset& Bookmark : Asset.EditorState.Bookmarks)
    {
        if (Bookmark.Id == Uuid{})
        {
            Bookmark.Id = NewUuid();
        }
        if (Bookmark.Zoom <= 0.0f)
        {
            Bookmark.Zoom = 1.0f;
        }
    }

    for (GraphCommentAsset& Comment : Asset.EditorState.Comments)
    {
        if (Comment.Id == Uuid{})
        {
            Comment.Id = NewUuid();
        }

        std::vector<Uuid> FilteredNodeIds{};
        FilteredNodeIds.reserve(Comment.NodeIds.size());
        for (const Uuid& NodeId : Comment.NodeIds)
        {
            if (LiveNodeIds.contains(NodeId))
            {
                FilteredNodeIds.push_back(NodeId);
            }
        }
        Comment.NodeIds = std::move(FilteredNodeIds);
    }

    for (GraphVariableAsset& Variable : Asset.Variables)
    {
        if (Variable.Id == Uuid{})
        {
            Variable.Id = NewUuid();
        }
    }

    for (GraphNodeAsset& Node : Asset.Nodes)
    {
        if (Node.ExecTargetNodeId != Uuid{} && !LiveNodeIds.contains(Node.ExecTargetNodeId))
        {
            Node.ExecTargetNodeId = {};
        }
        if (Node.FalseExecTargetNodeId != Uuid{} && !LiveNodeIds.contains(Node.FalseExecTargetNodeId))
        {
            Node.FalseExecTargetNodeId = {};
        }
    }
}

void NormalizeMethodNodeInputs(GraphAsset& Asset)
{
    const TypeId FallbackSelfType = Asset.SelfType;

    for (GraphNodeAsset& Node : Asset.Nodes)
    {
        if (Node.Kind != EGraphAssetNodeKind::SelfMethodCall &&
            Node.Kind != EGraphAssetNodeKind::InstanceMethodCall)
        {
            continue;
        }

        const TypeId OwnerType = Node.OwnerType != TypeId{} ? Node.OwnerType : FallbackSelfType;
        if (OwnerType == TypeId{} || Node.MemberName.empty())
        {
            continue;
        }

        const auto Methods = TypeRegistry::Instance().CollectMethods(OwnerType, true);
        const auto MethodIt = std::find_if(Methods.begin(), Methods.end(), [&Node](const ReflectedMethodRef& Ref) {
            return Ref.Method && Ref.Method->RawInvoke && Ref.Method->Name == Node.MemberName;
        });
        if (MethodIt == Methods.end() || !MethodIt->Method)
        {
            continue;
        }

        Node.OwnerType = MethodIt->OwnerType;
        Node.Inputs.resize(MethodIt->Method->ParamTypes.size());
    }
}

[[nodiscard]] std::string ResolveTypeLabel(const TypeId& Type)
{
    if (Type == TypeId{})
    {
        return "Any";
    }

    if (const TypeInfo* Info = TypeRegistry::Instance().Find(Type))
    {
        return PrettyReflectedTypeName(Info->Name);
    }

    return ToString(Type);
}

[[nodiscard]] std::string ResolveTypeDisplayLabel(const TypeId& Type)
{
    if (Type == TypeId{})
    {
        return "Any";
    }

    if (const TypeInfo* Info = TypeRegistry::Instance().Find(Type))
    {
        if (!Info->DisplayName.empty())
        {
            return Info->DisplayName;
        }
    }

    return ResolveTypeLabel(Type);
}

[[nodiscard]] std::string BuildArgName(const std::size_t Index)
{
    return "Arg" + std::to_string(Index);
}

[[nodiscard]] std::string ResolveFieldDisplayLabel(const FieldInfo& Field)
{
    return Field.DisplayName.empty() ? Field.Name : Field.DisplayName;
}

[[nodiscard]] std::string ResolveMethodDisplayLabel(const MethodInfo& Method)
{
    return Method.DisplayName.empty() ? Method.Name : Method.DisplayName;
}

[[nodiscard]] const FieldInfo* FindDirectFieldInfo(const TypeId& OwnerType, const std::string_view Name)
{
    const TypeInfo* OwnerInfo = TypeRegistry::Instance().Find(OwnerType);
    if (!OwnerInfo)
    {
        return nullptr;
    }

    const auto It = std::find_if(OwnerInfo->Fields.begin(), OwnerInfo->Fields.end(), [Name](const FieldInfo& Field) {
        return Field.Name == Name;
    });
    return It != OwnerInfo->Fields.end() ? &(*It) : nullptr;
}

[[nodiscard]] const MethodInfo* FindDirectMethodInfo(const TypeId& OwnerType, const std::string_view Name)
{
    const TypeInfo* OwnerInfo = TypeRegistry::Instance().Find(OwnerType);
    if (!OwnerInfo)
    {
        return nullptr;
    }

    const auto It = std::find_if(OwnerInfo->Methods.begin(), OwnerInfo->Methods.end(), [Name](const MethodInfo& Method) {
        return Method.Name == Name;
    });
    return It != OwnerInfo->Methods.end() ? &(*It) : nullptr;
}

[[nodiscard]] std::string FormatTooltipNumber(const double Value)
{
    std::ostringstream Stream;
    Stream << std::setprecision(15) << Value;
    std::string Text = Stream.str();

    if (const auto Dot = Text.find('.'); Dot != std::string::npos)
    {
        while (!Text.empty() && Text.back() == '0')
        {
            Text.pop_back();
        }
        if (!Text.empty() && Text.back() == '.')
        {
            Text.pop_back();
        }
    }

    return Text;
}

[[nodiscard]] std::string WrapTooltipText(const std::string_view Text, const std::size_t MaxCharsPerLine = 64)
{
    std::istringstream Lines{std::string(Text)};
    std::string RawLine{};
    std::string Result{};
    bool FirstLine = true;

    while (std::getline(Lines, RawLine))
    {
        if (!FirstLine)
        {
            Result += '\n';
        }
        FirstLine = false;

        if (RawLine.empty())
        {
            continue;
        }

        std::istringstream Words(RawLine);
        std::string Word{};
        std::size_t CurrentLineLength = 0;
        bool FirstWord = true;
        while (Words >> Word)
        {
            const std::size_t NextLength = FirstWord ? Word.size() : (CurrentLineLength + 1 + Word.size());
            if (!FirstWord && CurrentLineLength > 0 && NextLength > MaxCharsPerLine)
            {
                Result += '\n';
                Result += Word;
                CurrentLineLength = Word.size();
                FirstWord = false;
                continue;
            }

            if (!FirstWord)
            {
                Result += ' ';
                ++CurrentLineLength;
            }

            Result += Word;
            CurrentLineLength += Word.size();
            FirstWord = false;
        }
    }

    return Result;
}

void AppendTooltipLine(std::string& Tooltip, const std::string_view Line)
{
    if (Line.empty())
    {
        return;
    }

    if (!Tooltip.empty())
    {
        Tooltip += '\n';
    }
    Tooltip += Line;
}

void AppendWrappedTooltipLine(std::string& Tooltip, const std::string_view Line)
{
    if (Line.empty())
    {
        return;
    }

    AppendTooltipLine(Tooltip, WrapTooltipText(Line));
}

void AppendFieldValueTooltipLines(std::string& Tooltip, const FieldInfo::NumericValueInfo& ValueInfo)
{
    if (ValueInfo.Min.has_value() || ValueInfo.Max.has_value())
    {
        std::string RangeLine = "Range: ";
        RangeLine += ValueInfo.Min.has_value() ? FormatTooltipNumber(*ValueInfo.Min) : std::string("-inf");
        RangeLine += " to ";
        RangeLine += ValueInfo.Max.has_value() ? FormatTooltipNumber(*ValueInfo.Max) : std::string("+inf");
        AppendTooltipLine(Tooltip, RangeLine);
    }

    if (ValueInfo.Step.has_value())
    {
        AppendTooltipLine(Tooltip, "Step: " + FormatTooltipNumber(*ValueInfo.Step));
    }
}

[[nodiscard]] std::string BuildReflectedNodeTooltipSummary(const std::string_view Summary,
                                                           const std::string_view Doc,
                                                           const TypeId& ValueType,
                                                           const FieldInfo::NumericValueInfo* ValueInfo = nullptr)
{
    std::string Tooltip{};
    AppendWrappedTooltipLine(Tooltip, Summary);
    if (Doc.empty())
    {
        if (const TypeInfo* Type = TypeRegistry::Instance().Find(ValueType); Type && !Type->Doc.empty())
        {
            AppendWrappedTooltipLine(Tooltip, Type->Doc);
        }
    }
    else
    {
        AppendWrappedTooltipLine(Tooltip, Doc);
    }

    if (ValueType != TypeId{})
    {
        AppendTooltipLine(Tooltip, "Type: " + ResolveTypeDisplayLabel(ValueType));
    }

    if (ValueInfo)
    {
        AppendFieldValueTooltipLines(Tooltip, *ValueInfo);
    }

    return Tooltip;
}

[[nodiscard]] std::string BuildTargetPinTooltip(const TypeId& TargetType, const std::string_view Summary)
{
    std::string Tooltip{};
    AppendWrappedTooltipLine(Tooltip, Summary);
    AppendTooltipLine(Tooltip, "Type: " + ResolveTypeDisplayLabel(TargetType));
    if (const TypeInfo* Type = TypeRegistry::Instance().Find(TargetType); Type && !Type->Doc.empty())
    {
        AppendWrappedTooltipLine(Tooltip, Type->Doc);
    }
    return Tooltip;
}

[[nodiscard]] std::string_view UnaryIntrinsicName(const EUnaryIntrinsicOp Op)
{
    switch (Op)
    {
        case EUnaryIntrinsicOp::LogicalNot:
            return "Logical Not";
        case EUnaryIntrinsicOp::Negate:
            return "Negate";
    }

    return "Unary Intrinsic";
}

[[nodiscard]] std::string_view BinaryIntrinsicName(const EBinaryIntrinsicOp Op)
{
    switch (Op)
    {
        case EBinaryIntrinsicOp::Add:
            return "Add";
        case EBinaryIntrinsicOp::Subtract:
            return "Subtract";
        case EBinaryIntrinsicOp::Multiply:
            return "Multiply";
        case EBinaryIntrinsicOp::Divide:
            return "Divide";
        case EBinaryIntrinsicOp::Equal:
            return "Equal";
        case EBinaryIntrinsicOp::NotEqual:
            return "Not Equal";
        case EBinaryIntrinsicOp::Less:
            return "Less";
        case EBinaryIntrinsicOp::LessEqual:
            return "Less Or Equal";
        case EBinaryIntrinsicOp::Greater:
            return "Greater";
        case EBinaryIntrinsicOp::GreaterEqual:
            return "Greater Or Equal";
        case EBinaryIntrinsicOp::LogicalAnd:
            return "Logical And";
        case EBinaryIntrinsicOp::LogicalOr:
            return "Logical Or";
    }

    return "Binary Intrinsic";
}

[[nodiscard]] std::vector<SchemaNodeDescriptor> BuildActiveSchemaDescriptors(const SchemaRegistry& Schema, const GraphAsset& Asset)
{
    std::vector<SchemaNodeDescriptor> Result = Schema.Builtins();

    if (Asset.SelfType != TypeId{})
    {
        if (const TypeInfo* SelfType = TypeRegistry::Instance().Find(Asset.SelfType))
        {
            auto SelfNodes = Schema.DescribeSelf(*SelfType);
            Result.insert(Result.end(),
                          std::make_move_iterator(SelfNodes.begin()),
                          std::make_move_iterator(SelfNodes.end()));
        }
    }

    std::vector<TypeId> InstanceTypes{};
    if (const TypeInfo* BaseNodeType = TypeRegistry::Instance().Find(StaticTypeId<BaseNode>()))
    {
        InstanceTypes.push_back(BaseNodeType->Id);
    }

    const auto NodeTypes = TypeRegistry::Instance().Derived(StaticTypeId<BaseNode>());
    InstanceTypes.reserve(InstanceTypes.size() + NodeTypes.size() + ComponentSerializationRegistry::Instance().Types().size());
    for (const TypeInfo* Type : NodeTypes)
    {
        if (Type)
        {
            InstanceTypes.push_back(Type->Id);
        }
    }

    const auto ComponentTypes = ComponentSerializationRegistry::Instance().Types();
    InstanceTypes.insert(InstanceTypes.end(), ComponentTypes.begin(), ComponentTypes.end());

    const auto AppendInstanceTypeIfReflected = [&InstanceTypes]() {
        auto AppendOne = [&InstanceTypes]<typename T>(std::type_identity<T>) {
            if (const TypeInfo* Info = TypeRegistry::Instance().Find(StaticTypeId<T>()))
            {
                InstanceTypes.push_back(Info->Id);
            }
        };

        AppendOne(std::type_identity<IWorld>{});
#if defined(SNAPI_GF_ENABLE_INPUT)
        AppendOne(std::type_identity<InputSystem>{});
        AppendOne(std::type_identity<InputBootstrapSettings>{});
#endif
#if defined(SNAPI_GF_ENABLE_UI)
        AppendOne(std::type_identity<UISystem>{});
        AppendOne(std::type_identity<UIBootstrapSettings>{});
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
        AppendOne(std::type_identity<AudioSystem>{});
#endif
#if defined(SNAPI_GF_ENABLE_NETWORKING)
        AppendOne(std::type_identity<NetworkSystem>{});
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        AppendOne(std::type_identity<PhysicsSystem>{});
        AppendOne(std::type_identity<PhysicsBootstrapSettings>{});
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
        AppendOne(std::type_identity<RendererSystem>{});
        AppendOne(std::type_identity<RendererBootstrapSettings>{});
#endif
    };
    AppendInstanceTypeIfReflected();

    std::sort(InstanceTypes.begin(), InstanceTypes.end());
    InstanceTypes.erase(std::unique(InstanceTypes.begin(), InstanceTypes.end()), InstanceTypes.end());
    const std::unordered_set<TypeId, UuidHash> InstanceTypeLookup(InstanceTypes.begin(), InstanceTypes.end());

    for (const TypeId& InstanceTypeId : InstanceTypes)
    {
        const TypeInfo* InstanceType = TypeRegistry::Instance().Find(InstanceTypeId);
        if (!InstanceType)
        {
            continue;
        }

        auto InstanceNodes = DescribeMembers(*InstanceType, false, &InstanceTypeLookup);
        Result.insert(Result.end(),
                      std::make_move_iterator(InstanceNodes.begin()),
                      std::make_move_iterator(InstanceNodes.end()));
    }

    auto VariableNodes = Schema.DescribeVariables(Asset);
    Result.insert(Result.end(),
                  std::make_move_iterator(VariableNodes.begin()),
                  std::make_move_iterator(VariableNodes.end()));

    std::sort(Result.begin(), Result.end(), [](const SchemaNodeDescriptor& Left, const SchemaNodeDescriptor& Right) {
        if (Left.Category != Right.Category)
        {
            return Left.Category < Right.Category;
        }
        if (Left.DisplayName != Right.DisplayName)
        {
            return Left.DisplayName < Right.DisplayName;
        }
        return Left.StableId < Right.StableId;
    });

    return Result;
}

template<typename TNode>
void VisitAllNodeSlotRefs(TNode& Node, const auto& Visitor)
{
    Visitor(Node.Input);
    Visitor(Node.Left);
    Visitor(Node.Right);
    Visitor(Node.Output);
    Visitor(Node.Condition);
    Visitor(Node.Instance);
    Visitor(Node.ReturnSlot);
    for (auto& Input : Node.Inputs)
    {
        Visitor(Input);
    }
}

template<typename TNode>
void VisitConsumerNodeSlotRefs(TNode& Node, const auto& Visitor)
{
    Visitor(Node.Input);
    Visitor(Node.Left);
    Visitor(Node.Right);
    Visitor(Node.Condition);
    Visitor(Node.Instance);
    for (auto& Input : Node.Inputs)
    {
        Visitor(Input);
    }
}

template<typename TNode>
void VisitProducerNodeSlotRefs(TNode& Node, const auto& Visitor)
{
    switch (Node.Kind)
    {
    case EGraphAssetNodeKind::EntryPoint:
        if (BuiltinEntryPointUsesDeltaSeconds(Node.BuiltinEntryPoint))
        {
            Visitor(Node.Output);
        }
        break;
    case EGraphAssetNodeKind::Constant:
    case EGraphAssetNodeKind::VariableGet:
    case EGraphAssetNodeKind::UnaryIntrinsic:
    case EGraphAssetNodeKind::BinaryIntrinsic:
    case EGraphAssetNodeKind::SelfFieldRead:
    case EGraphAssetNodeKind::InstanceFieldRead:
        Visitor(Node.Output);
        break;
    case EGraphAssetNodeKind::SelfMethodCall:
    case EGraphAssetNodeKind::InstanceMethodCall:
        Visitor(Node.ReturnSlot);
        break;
    case EGraphAssetNodeKind::VariableSet:
    case EGraphAssetNodeKind::Jump:
    case EGraphAssetNodeKind::Branch:
    case EGraphAssetNodeKind::Label:
    case EGraphAssetNodeKind::SelfFieldWrite:
    case EGraphAssetNodeKind::InstanceFieldWrite:
        break;
    }
}

void CompactAuthoredSlots(GraphAsset& Asset)
{
    std::vector<bool> Used{};
    Used.resize(Asset.Slots.size(), false);

    for (GraphNodeAsset& Node : Asset.Nodes)
    {
        VisitAllNodeSlotRefs(Node, [&Used](SlotId& Ref) {
            if (!Ref.IsValid())
            {
                return;
            }
            if (Ref.Value >= Used.size())
            {
                Ref = {};
                return;
            }
            Used[Ref.Value] = true;
        });
    }

    std::vector<std::uint32_t> Remap(Asset.Slots.size(), SlotId::InvalidValue);
    std::vector<GraphSlotAsset> CompactedSlots{};
    CompactedSlots.reserve(Asset.Slots.size());
    for (std::uint32_t Index = 0; Index < static_cast<std::uint32_t>(Asset.Slots.size()); ++Index)
    {
        if (!Used[Index])
        {
            continue;
        }

        Remap[Index] = static_cast<std::uint32_t>(CompactedSlots.size());
        CompactedSlots.push_back(Asset.Slots[Index]);
    }

    for (GraphNodeAsset& Node : Asset.Nodes)
    {
        VisitAllNodeSlotRefs(Node, [&Remap](SlotId& Ref) {
            if (!Ref.IsValid())
            {
                return;
            }
            if (Ref.Value >= Remap.size() || Remap[Ref.Value] == SlotId::InvalidValue)
            {
                Ref = {};
                return;
            }
            Ref.Value = Remap[Ref.Value];
        });
    }

    Asset.Slots = std::move(CompactedSlots);
}

void DisconnectConsumersOfProducedSlots(GraphAsset& Asset, const std::unordered_set<std::uint32_t>& ProducedSlots)
{
    if (ProducedSlots.empty())
    {
        return;
    }

    for (GraphNodeAsset& Node : Asset.Nodes)
    {
        VisitConsumerNodeSlotRefs(Node, [&ProducedSlots](SlotId& Ref) {
            if (Ref.IsValid() && ProducedSlots.contains(Ref.Value))
            {
                Ref = {};
            }
        });
    }
}

[[nodiscard]] bool TryParseArgPinIndex(const std::string_view PinName, std::size_t& OutIndex)
{
    if (!PinName.starts_with("Arg"))
    {
        return false;
    }

    const std::string_view Suffix = PinName.substr(3);
    if (Suffix.empty())
    {
        return false;
    }

    std::size_t Parsed = 0;
    const char* Begin = Suffix.data();
    const char* End = Suffix.data() + Suffix.size();
    const auto [Ptr, Error] = std::from_chars(Begin, End, Parsed);
    if (Error != std::errc{} || Ptr != End)
    {
        return false;
    }

    OutIndex = Parsed;
    return true;
}

[[nodiscard]] std::optional<std::size_t> ResolveMethodPinIndex(const GraphNodeAsset& Node, const std::string_view PinName)
{
    std::size_t ArgIndex = 0;
    if (TryParseArgPinIndex(PinName, ArgIndex))
    {
        return ArgIndex;
    }

    const MethodInfo* Method = FindDirectMethodInfo(Node.OwnerType, Node.MemberName);
    if (!Method)
    {
        return std::nullopt;
    }

    for (std::size_t Index = 0; Index < Method->ParamTypes.size(); ++Index)
    {
        const CallableParamInfo* Param = Index < Method->Params.size() ? &Method->Params[Index] : nullptr;
        const std::string ParamName = (Param && !Param->Name.empty()) ? Param->Name : BuildArgName(Index);
        if (ParamName == PinName)
        {
            return Index;
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::string CanonicalizeNodeInputPinKey(const GraphNodeAsset& Node, const std::string_view PinName)
{
    if (Node.Kind == EGraphAssetNodeKind::SelfMethodCall || Node.Kind == EGraphAssetNodeKind::InstanceMethodCall)
    {
        if (const auto ArgIndex = ResolveMethodPinIndex(Node, PinName); ArgIndex.has_value())
        {
            return BuildArgName(*ArgIndex);
        }
    }
    return std::string(PinName);
}

[[nodiscard]] GraphNodeInputDefaultAsset* FindMutableNodeInputDefault(GraphNodeAsset& Node, const std::string_view PinKey)
{
    const auto It = std::find_if(Node.InputDefaults.begin(),
                                 Node.InputDefaults.end(),
                                 [PinKey](const GraphNodeInputDefaultAsset& Entry) {
                                     return Entry.PinKey == PinKey;
                                 });
    return It != Node.InputDefaults.end() ? &(*It) : nullptr;
}

[[nodiscard]] const GraphNodeInputDefaultAsset* FindNodeInputDefault(const GraphNodeAsset& Node, const std::string_view PinKey)
{
    const auto It = std::find_if(Node.InputDefaults.begin(),
                                 Node.InputDefaults.end(),
                                 [PinKey](const GraphNodeInputDefaultAsset& Entry) {
                                     return Entry.PinKey == PinKey;
                                 });
    return It != Node.InputDefaults.end() ? &(*It) : nullptr;
}

[[nodiscard]] SlotId* ResolveMutableNodePinSlot(GraphNodeAsset& Node, const std::string_view PinName)
{
    switch (Node.Kind)
    {
    case EGraphAssetNodeKind::EntryPoint:
        return PinName == "DeltaSeconds" ? &Node.Output : nullptr;
    case EGraphAssetNodeKind::Constant:
    case EGraphAssetNodeKind::VariableGet:
    case EGraphAssetNodeKind::SelfFieldRead:
    case EGraphAssetNodeKind::InstanceFieldRead:
        return PinName == "Value" ? &Node.Output : nullptr;
    case EGraphAssetNodeKind::VariableSet:
    case EGraphAssetNodeKind::SelfFieldWrite:
    case EGraphAssetNodeKind::InstanceFieldWrite:
        if (PinName == "Target")
        {
            return &Node.Instance;
        }
        return PinName == "Value" ? &Node.Input : nullptr;
    case EGraphAssetNodeKind::UnaryIntrinsic:
        if (PinName == "Input")
        {
            return &Node.Input;
        }
        return PinName == "Value" ? &Node.Output : nullptr;
    case EGraphAssetNodeKind::BinaryIntrinsic:
        if (PinName == "Left")
        {
            return &Node.Left;
        }
        if (PinName == "Right")
        {
            return &Node.Right;
        }
        return PinName == "Value" ? &Node.Output : nullptr;
    case EGraphAssetNodeKind::Branch:
        return PinName == "Condition" ? &Node.Condition : nullptr;
    case EGraphAssetNodeKind::SelfMethodCall:
    case EGraphAssetNodeKind::InstanceMethodCall:
        if (PinName == "Target")
        {
            return &Node.Instance;
        }
        if (PinName == "Return")
        {
            return &Node.ReturnSlot;
        }
        {
            const auto ArgIndex = ResolveMethodPinIndex(Node, PinName);
            if (!ArgIndex.has_value())
            {
                return nullptr;
            }
            if (Node.Inputs.size() <= *ArgIndex)
            {
                Node.Inputs.resize(*ArgIndex + 1);
            }
            return &Node.Inputs[*ArgIndex];
        }
    case EGraphAssetNodeKind::Jump:
    case EGraphAssetNodeKind::Label:
        return nullptr;
    }

    return nullptr;
}

[[nodiscard]] const SlotId* ResolveConstNodePinSlot(const GraphNodeAsset& Node, const std::string_view PinName)
{
    switch (Node.Kind)
    {
    case EGraphAssetNodeKind::EntryPoint:
        return PinName == "DeltaSeconds" ? &Node.Output : nullptr;
    case EGraphAssetNodeKind::Constant:
    case EGraphAssetNodeKind::VariableGet:
    case EGraphAssetNodeKind::SelfFieldRead:
    case EGraphAssetNodeKind::InstanceFieldRead:
        return PinName == "Value" ? &Node.Output : nullptr;
    case EGraphAssetNodeKind::VariableSet:
    case EGraphAssetNodeKind::SelfFieldWrite:
    case EGraphAssetNodeKind::InstanceFieldWrite:
        if (PinName == "Target")
        {
            return &Node.Instance;
        }
        return PinName == "Value" ? &Node.Input : nullptr;
    case EGraphAssetNodeKind::UnaryIntrinsic:
        if (PinName == "Input")
        {
            return &Node.Input;
        }
        return PinName == "Value" ? &Node.Output : nullptr;
    case EGraphAssetNodeKind::BinaryIntrinsic:
        if (PinName == "Left")
        {
            return &Node.Left;
        }
        if (PinName == "Right")
        {
            return &Node.Right;
        }
        return PinName == "Value" ? &Node.Output : nullptr;
    case EGraphAssetNodeKind::Branch:
        return PinName == "Condition" ? &Node.Condition : nullptr;
    case EGraphAssetNodeKind::SelfMethodCall:
    case EGraphAssetNodeKind::InstanceMethodCall:
        if (PinName == "Target")
        {
            return &Node.Instance;
        }
        if (PinName == "Return")
        {
            return &Node.ReturnSlot;
        }
        {
            const auto ArgIndex = ResolveMethodPinIndex(Node, PinName);
            if (!ArgIndex.has_value() || *ArgIndex >= Node.Inputs.size())
            {
                return nullptr;
            }
            return &Node.Inputs[*ArgIndex];
        }
    case EGraphAssetNodeKind::Jump:
    case EGraphAssetNodeKind::Label:
        return nullptr;
    }

    return nullptr;
}

struct NodePinBinding
{
    const SchemaPinDescriptor* Pin = nullptr;
    SlotId* MutableSlot = nullptr;
    const SlotId* Slot = nullptr;
    TypeId DisplayType{};
    TypeId StorageType{};
    bool IsInstanceTarget = false;
    bool RequiresMutableInstance = false;
};

template<typename TNode>
TExpected<NodePinBinding> ResolveNodePinBindingImpl(const GraphAsset& Asset,
                                                    TNode& Node,
                                                    const SchemaNodeDescriptor& Descriptor,
                                                    const std::string_view PinName,
                                                    const ESchemaPinDirection Direction)
{
    const auto PinIt = std::find_if(Descriptor.Pins.begin(), Descriptor.Pins.end(), [PinName, Direction](const SchemaPinDescriptor& Pin) {
        return Pin.Name == PinName && Pin.Direction == Direction;
    });
    if (PinIt == Descriptor.Pins.end())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit pin was not found on the authored node schema"));
    }

    NodePinBinding Binding{};
    Binding.Pin = &(*PinIt);
    Binding.DisplayType = PinIt->Type.Type;
    Binding.StorageType = PinIt->Type.Kind == ESlotKind::Handle
        ? ResolveExpectedHandleStorageType(PinIt->Type.Type)
        : PinIt->Type.Type;
    Binding.IsInstanceTarget = Direction == ESchemaPinDirection::Input &&
                               PinName == "Target" &&
                               (Node.Kind == EGraphAssetNodeKind::InstanceFieldRead ||
                                Node.Kind == EGraphAssetNodeKind::InstanceFieldWrite ||
                                Node.Kind == EGraphAssetNodeKind::InstanceMethodCall);
    Binding.RequiresMutableInstance = Direction == ESchemaPinDirection::Input &&
                                      PinName == "Target" &&
                                      (Node.Kind == EGraphAssetNodeKind::InstanceFieldWrite ||
                                       (Node.Kind == EGraphAssetNodeKind::InstanceMethodCall && !Descriptor.IsPure));

    if constexpr (std::is_const_v<TNode>)
    {
        Binding.Slot = ResolveConstNodePinSlot(Node, PinName);
    }
    else
    {
        Binding.MutableSlot = ResolveMutableNodePinSlot(Node, PinName);
        Binding.Slot = Binding.MutableSlot;
    }

    if (!PinIt->Type.IsExec && !Binding.Slot)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit pin does not map to authored slot data"));
    }

    if (Binding.Slot && Binding.Slot->IsValid() && Binding.Slot->Value < Asset.Slots.size())
    {
        Binding.StorageType = Asset.Slots[Binding.Slot->Value].Type;
        if (Binding.DisplayType == TypeId{} || PinIt->Type.IsPolymorphic)
        {
            Binding.DisplayType = Binding.StorageType;
        }
    }
    else if (Node.Kind == EGraphAssetNodeKind::Constant &&
             PinName == "Value" &&
             Direction == ESchemaPinDirection::Output &&
             Node.ConstantValue.Type != TypeId{})
    {
        Binding.StorageType = Node.ConstantValue.Type;
        if (Binding.DisplayType == TypeId{} || PinIt->Type.IsPolymorphic)
        {
            Binding.DisplayType = Node.ConstantValue.Type;
        }
    }

    return Binding;
}

TExpected<NodePinBinding> ResolveNodePinBinding(const GraphAsset& Asset,
                                                GraphNodeAsset& Node,
                                                const SchemaNodeDescriptor& Descriptor,
                                                const std::string_view PinName,
                                                const ESchemaPinDirection Direction)
{
    return ResolveNodePinBindingImpl(Asset, Node, Descriptor, PinName, Direction);
}

TExpected<NodePinBinding> ResolveNodePinBinding(const GraphAsset& Asset,
                                                const GraphNodeAsset& Node,
                                                const SchemaNodeDescriptor& Descriptor,
                                                const std::string_view PinName,
                                                const ESchemaPinDirection Direction)
{
    return ResolveNodePinBindingImpl(Asset, Node, Descriptor, PinName, Direction);
}

[[nodiscard]] bool IsBindingCompatibleWithStorageType(const NodePinBinding& Binding, const TypeId& StorageType)
{
    if (!Binding.Pin)
    {
        return false;
    }
    if (Binding.Pin->Type.IsExec)
    {
        return true;
    }
    if (Binding.IsInstanceTarget)
    {
        return IsInstanceTargetStorageCompatible(StorageType, Binding.DisplayType, Binding.RequiresMutableInstance);
    }
    if (ResolveSlotKindForType(StorageType) != Binding.Pin->Type.Kind)
    {
        return false;
    }
    if (Binding.StorageType == TypeId{})
    {
        return true;
    }
    if (Binding.StorageType == StorageType)
    {
        return true;
    }
    return IsPointerStorageAssignableToExpected(StorageType, Binding.StorageType);
}

TExpected<TypeId> ResolveConnectionStorageType(const NodePinBinding& Source, const NodePinBinding& Target)
{
    if (!Source.Pin || !Target.Pin)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit pin binding is incomplete"));
    }
    if (Source.Pin->Type.IsExec || Target.Pin->Type.IsExec)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit slot connections cannot be created from exec pins"));
    }
    const bool AllowsValueToInstanceTarget = Source.Pin->Type.Kind == ESlotKind::Value && Target.IsInstanceTarget;
    if (Source.Pin->Type.Kind != Target.Pin->Type.Kind && !AllowsValueToInstanceTarget)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit pin kinds do not match"));
    }

    TypeId StorageType = Source.StorageType != TypeId{} ? Source.StorageType : Target.StorageType;
    if (StorageType == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit polymorphic pins need one concrete typed endpoint before they can connect"));
    }

    if (!IsBindingCompatibleWithStorageType(Source, StorageType) ||
        !IsBindingCompatibleWithStorageType(Target, StorageType))
    {
        const TypeId AlternateStorageType = Target.StorageType != TypeId{} ? Target.StorageType : Source.StorageType;
        if (AlternateStorageType == TypeId{} ||
            !IsBindingCompatibleWithStorageType(Source, AlternateStorageType) ||
            !IsBindingCompatibleWithStorageType(Target, AlternateStorageType))
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit pin storage type is incompatible with one endpoint"));
        }
        StorageType = AlternateStorageType;
    }
    return StorageType;
}

[[nodiscard]] SpawnMenuEntryView MakeSpawnMenuEntry(const SchemaNodeDescriptor& Descriptor, std::string TargetPin = {})
{
    return SpawnMenuEntryView{
        .StableId = Descriptor.StableId,
        .DisplayName = Descriptor.DisplayName,
        .Category = Descriptor.Category,
        .Tooltip = Descriptor.Tooltip,
        .TargetPin = std::move(TargetPin),
    };
}

[[nodiscard]] NodePinBinding BuildProspectiveInputBinding(const SchemaNodeDescriptor& Descriptor,
                                                          const SchemaPinDescriptor& Pin)
{
    NodePinBinding Binding{};
    Binding.Pin = &Pin;
    Binding.DisplayType = Pin.Type.Type;
    Binding.StorageType = Pin.Type.Kind == ESlotKind::Handle
        ? ResolveExpectedHandleStorageType(Pin.Type.Type)
        : Pin.Type.Type;
    const auto LoweredKind = Descriptor.LoweredKind.value_or(EGraphAssetNodeKind::Constant);
    Binding.IsInstanceTarget = Pin.Direction == ESchemaPinDirection::Input &&
                               Pin.Name == "Target" &&
                               (LoweredKind == EGraphAssetNodeKind::InstanceFieldRead ||
                                LoweredKind == EGraphAssetNodeKind::InstanceFieldWrite ||
                                LoweredKind == EGraphAssetNodeKind::InstanceMethodCall);
    Binding.RequiresMutableInstance = Pin.Direction == ESchemaPinDirection::Input &&
                                      Pin.Name == "Target" &&
                                      (LoweredKind == EGraphAssetNodeKind::InstanceFieldWrite ||
                                       (LoweredKind == EGraphAssetNodeKind::InstanceMethodCall && !Descriptor.IsPure));
    return Binding;
}

[[nodiscard]] std::optional<SpawnMenuEntryView> BuildCompatibleSpawnEntry(const NodePinBinding& SourceBinding,
                                                                          const SchemaNodeDescriptor& Descriptor)
{
    if (!SourceBinding.Pin || !Descriptor.LoweredKind.has_value())
    {
        return std::nullopt;
    }

    for (const SchemaPinDescriptor& Pin : Descriptor.Pins)
    {
        if (Pin.Direction != ESchemaPinDirection::Input)
        {
            continue;
        }

        if (SourceBinding.Pin->Type.IsExec || Pin.Type.IsExec)
        {
            if (SourceBinding.Pin->Type.IsExec && Pin.Type.IsExec)
            {
                return MakeSpawnMenuEntry(Descriptor, Pin.Name);
            }
            continue;
        }

        NodePinBinding TargetBinding = BuildProspectiveInputBinding(Descriptor, Pin);
        if (ResolveConnectionStorageType(SourceBinding, TargetBinding))
        {
            return MakeSpawnMenuEntry(Descriptor, Pin.Name);
        }
    }

    return std::nullopt;
}

void SortSpawnMenuEntries(std::vector<SpawnMenuEntryView>& Entries)
{
    std::sort(Entries.begin(), Entries.end(), [](const SpawnMenuEntryView& Left, const SpawnMenuEntryView& Right) {
        if (Left.Category != Right.Category)
        {
            return Left.Category < Right.Category;
        }
        if (Left.DisplayName != Right.DisplayName)
        {
            return Left.DisplayName < Right.DisplayName;
        }
        if (Left.TargetPin != Right.TargetPin)
        {
            return Left.TargetPin < Right.TargetPin;
        }
        return Left.StableId < Right.StableId;
    });

    Entries.erase(std::unique(Entries.begin(),
                              Entries.end(),
                              [](const SpawnMenuEntryView& Left, const SpawnMenuEntryView& Right) {
                                  return Left.StableId == Right.StableId && Left.TargetPin == Right.TargetPin;
                              }),
                 Entries.end());
}

[[nodiscard]] SlotId AppendAuthoredSlot(GraphAsset& Asset,
                                        const std::string_view Name,
                                        const TypeId& Type,
                                        const ESlotKind Kind)
{
    Asset.Slots.push_back(GraphSlotAsset{
        .Name = std::string(Name),
        .Type = Type,
        .Kind = Kind,
    });
    return SlotId{static_cast<std::uint32_t>(Asset.Slots.size() - 1)};
}

struct SlotProducerView
{
    Uuid NodeId{};
    std::string PinName{};
    ESlotKind Kind = ESlotKind::Value;
};

void AppendDataCanvasWires(const GraphAsset& Asset,
                           const std::vector<SchemaNodeDescriptor>& Descriptors,
                           std::vector<CanvasWireView>& OutWires)
{
    std::unordered_map<std::uint32_t, SlotProducerView> Producers{};
    Producers.reserve(Asset.Slots.size());

    for (const GraphNodeAsset& Node : Asset.Nodes)
    {
        const SchemaNodeDescriptor* Descriptor = FindSchemaDescriptorForNode(Descriptors, Node);
        if (!Descriptor)
        {
            continue;
        }

        for (const SchemaPinDescriptor& Pin : Descriptor->Pins)
        {
            if (Pin.Direction != ESchemaPinDirection::Output || Pin.Type.IsExec)
            {
                continue;
            }

            auto BindingResult = ResolveNodePinBinding(Asset, Node, *Descriptor, Pin.Name, ESchemaPinDirection::Output);
            if (!BindingResult || !BindingResult->Slot || !BindingResult->Slot->IsValid())
            {
                continue;
            }

            Producers.emplace(BindingResult->Slot->Value,
                              SlotProducerView{
                                  .NodeId = Node.Id,
                                  .PinName = Pin.Name,
                                  .Kind = Pin.Type.Kind,
                              });
        }
    }

    for (const GraphNodeAsset& Node : Asset.Nodes)
    {
        const SchemaNodeDescriptor* Descriptor = FindSchemaDescriptorForNode(Descriptors, Node);
        if (!Descriptor)
        {
            continue;
        }

        for (const SchemaPinDescriptor& Pin : Descriptor->Pins)
        {
            if (Pin.Direction != ESchemaPinDirection::Input || Pin.Type.IsExec)
            {
                continue;
            }

            auto BindingResult = ResolveNodePinBinding(Asset, Node, *Descriptor, Pin.Name, ESchemaPinDirection::Input);
            if (!BindingResult || !BindingResult->Slot || !BindingResult->Slot->IsValid())
            {
                continue;
            }

            const auto ProducerIt = Producers.find(BindingResult->Slot->Value);
            if (ProducerIt == Producers.end())
            {
                continue;
            }

            OutWires.push_back(CanvasWireView{
                .SourceNodeId = ProducerIt->second.NodeId,
                .SourcePin = ProducerIt->second.PinName,
                .TargetNodeId = Node.Id,
                .TargetPin = Pin.Name,
                .Kind = ProducerIt->second.Kind,
                .IsExec = false,
            });
        }
    }
}

[[nodiscard]] std::string ResolveVariableLabel(const GraphAsset& Asset, const Uuid& VariableId)
{
    const auto It = std::find_if(Asset.Variables.begin(), Asset.Variables.end(), [&VariableId](const GraphVariableAsset& Variable) {
        return Variable.Id == VariableId;
    });
    return It != Asset.Variables.end() ? It->Name : std::string("Unknown Variable");
}

[[nodiscard]] std::string DescribeNodeTitle(const GraphAsset& Asset, const GraphNodeAsset& Node)
{
    switch (Node.Kind)
    {
        case EGraphAssetNodeKind::EntryPoint:
            return Node.BuiltinEntryPoint != EBuiltinEntryPoint::None
                ? std::string(BuiltinEntryPointName(Node.BuiltinEntryPoint))
                : (Node.EntryPointName.empty() ? std::string("Custom Entry") : Node.EntryPointName);
        case EGraphAssetNodeKind::Label:
            return Node.LabelName.empty() ? std::string("Label") : "Label " + Node.LabelName;
        case EGraphAssetNodeKind::Constant:
            return "Constant";
        case EGraphAssetNodeKind::VariableGet:
            return "Get " + ResolveVariableLabel(Asset, Node.VariableId);
        case EGraphAssetNodeKind::VariableSet:
            return "Set " + ResolveVariableLabel(Asset, Node.VariableId);
        case EGraphAssetNodeKind::UnaryIntrinsic:
            return std::string(UnaryIntrinsicName(Node.UnaryOp));
        case EGraphAssetNodeKind::BinaryIntrinsic:
            return std::string(BinaryIntrinsicName(Node.BinaryOp));
        case EGraphAssetNodeKind::Jump:
            return "Jump";
        case EGraphAssetNodeKind::Branch:
            return "Branch";
        case EGraphAssetNodeKind::SelfFieldRead:
            if (const FieldInfo* Field = FindDirectFieldInfo(Node.OwnerType, Node.MemberName))
            {
                return "Get " + ResolveFieldDisplayLabel(*Field);
            }
            return "Get " + Node.MemberName;
        case EGraphAssetNodeKind::SelfFieldWrite:
            if (const FieldInfo* Field = FindDirectFieldInfo(Node.OwnerType, Node.MemberName))
            {
                return "Set " + ResolveFieldDisplayLabel(*Field);
            }
            return "Set " + Node.MemberName;
        case EGraphAssetNodeKind::SelfMethodCall:
            if (const MethodInfo* Method = FindDirectMethodInfo(Node.OwnerType, Node.MemberName))
            {
                return "Call " + ResolveMethodDisplayLabel(*Method);
            }
            return "Call " + Node.MemberName;
        case EGraphAssetNodeKind::InstanceFieldRead:
            if (const FieldInfo* Field = FindDirectFieldInfo(Node.OwnerType, Node.MemberName))
            {
                return "Get " + ResolveFieldDisplayLabel(*Field);
            }
            return "Get " + Node.MemberName;
        case EGraphAssetNodeKind::InstanceFieldWrite:
            if (const FieldInfo* Field = FindDirectFieldInfo(Node.OwnerType, Node.MemberName))
            {
                return "Set " + ResolveFieldDisplayLabel(*Field);
            }
            return "Set " + Node.MemberName;
        case EGraphAssetNodeKind::InstanceMethodCall:
            if (const MethodInfo* Method = FindDirectMethodInfo(Node.OwnerType, Node.MemberName))
            {
                return "Call " + ResolveMethodDisplayLabel(*Method);
            }
            return "Call " + Node.MemberName;
    }

    return "Node";
}

[[nodiscard]] std::string DescribeNodeDetail(const GraphAsset& Asset, const GraphNodeAsset& Node)
{
    switch (Node.Kind)
    {
        case EGraphAssetNodeKind::EntryPoint:
            return Node.BuiltinEntryPoint != EBuiltinEntryPoint::None ? "Entry Node" : "Custom Entry Node";
        case EGraphAssetNodeKind::Label:
            return "Flow Label";
        case EGraphAssetNodeKind::Constant:
            return Node.ConstantValue.Type != TypeId{} ? ResolveTypeLabel(Node.ConstantValue.Type) : std::string("Unconfigured Constant");
        case EGraphAssetNodeKind::VariableGet:
        case EGraphAssetNodeKind::VariableSet:
            return ResolveVariableLabel(Asset, Node.VariableId);
        case EGraphAssetNodeKind::UnaryIntrinsic:
            return "Intrinsic";
        case EGraphAssetNodeKind::BinaryIntrinsic:
            return "Intrinsic";
        case EGraphAssetNodeKind::Jump:
            if (Node.ExecTargetNodeId != Uuid{})
            {
                return "Connected Jump";
            }
            return Node.LabelName.empty() ? std::string("Target label pending") : "To " + Node.LabelName;
        case EGraphAssetNodeKind::Branch:
            if (Node.ExecTargetNodeId != Uuid{} || Node.FalseExecTargetNodeId != Uuid{})
            {
                return "Connected True/False flow";
            }
            if (Node.LabelName.empty() && Node.FalseLabelName.empty())
            {
                return "True/False labels pending";
            }
            return "True: " + Node.LabelName + " | False: " + Node.FalseLabelName;
        case EGraphAssetNodeKind::SelfFieldRead:
            return "Self Field";
        case EGraphAssetNodeKind::SelfFieldWrite:
            return "Self Field";
        case EGraphAssetNodeKind::SelfMethodCall:
            return "Self Method Call";
        case EGraphAssetNodeKind::InstanceFieldRead:
        case EGraphAssetNodeKind::InstanceFieldWrite:
            return ResolveTypeDisplayLabel(Node.OwnerType);
        case EGraphAssetNodeKind::InstanceMethodCall:
            return "Call on " + ResolveTypeDisplayLabel(Node.OwnerType);
    }

    return {};
}

[[nodiscard]] const SchemaNodeDescriptor* FindSchemaDescriptorForNode(const std::vector<SchemaNodeDescriptor>& Descriptors,
                                                                      const GraphNodeAsset& Node)
{
    for (const SchemaNodeDescriptor& Descriptor : Descriptors)
    {
        if (!Descriptor.LoweredKind.has_value() || *Descriptor.LoweredKind != Node.Kind)
        {
            continue;
        }

        switch (Node.Kind)
        {
            case EGraphAssetNodeKind::EntryPoint:
                if (Descriptor.BuiltinEntryPoint == Node.BuiltinEntryPoint)
                {
                    return &Descriptor;
                }
                break;
            case EGraphAssetNodeKind::VariableGet:
            case EGraphAssetNodeKind::VariableSet:
                if (Descriptor.VariableId == Node.VariableId)
                {
                    return &Descriptor;
                }
                break;
            case EGraphAssetNodeKind::UnaryIntrinsic:
                if (Descriptor.UnaryOp == Node.UnaryOp)
                {
                    return &Descriptor;
                }
                break;
            case EGraphAssetNodeKind::BinaryIntrinsic:
                if (Descriptor.BinaryOp == Node.BinaryOp)
                {
                    return &Descriptor;
                }
                break;
            case EGraphAssetNodeKind::SelfFieldRead:
            case EGraphAssetNodeKind::SelfFieldWrite:
            case EGraphAssetNodeKind::SelfMethodCall:
            case EGraphAssetNodeKind::InstanceFieldRead:
            case EGraphAssetNodeKind::InstanceFieldWrite:
            case EGraphAssetNodeKind::InstanceMethodCall:
                if (Descriptor.OwnerType == Node.OwnerType && Descriptor.MemberName == Node.MemberName)
                {
                    return &Descriptor;
                }
                break;
            case EGraphAssetNodeKind::Label:
            case EGraphAssetNodeKind::Constant:
            case EGraphAssetNodeKind::Jump:
            case EGraphAssetNodeKind::Branch:
                return &Descriptor;
        }
    }

    return nullptr;
}

[[nodiscard]] std::string BuildFallbackPinTooltip(const SchemaPinDescriptor& Pin)
{
    if (!Pin.Tooltip.empty())
    {
        return Pin.Tooltip;
    }

    std::string Tooltip{};
    if (Pin.Type.IsExec)
    {
        AppendWrappedTooltipLine(
            Tooltip,
            std::string(Pin.Direction == ESchemaPinDirection::Input ? "Exec input" : "Exec output") +
                " pin '" + Pin.Name + "'.");
        return Tooltip;
    }

    const char* DirectionLabel = Pin.Direction == ESchemaPinDirection::Input ? "Input" : "Output";
    const char* KindLabel = Pin.Type.Kind == ESlotKind::Handle ? "handle" : "value";
    AppendWrappedTooltipLine(Tooltip, std::string(DirectionLabel) + " " + KindLabel + " pin '" + Pin.Name + "'.");

    if (Pin.Type.Type != TypeId{})
    {
        AppendTooltipLine(Tooltip, "Type: " + ResolveTypeDisplayLabel(Pin.Type.Type));
    }
    else if (Pin.Type.IsPolymorphic)
    {
        AppendTooltipLine(Tooltip, "Type: inferred");
    }

    if (Pin.SupportsLiteral)
    {
        AppendTooltipLine(Tooltip, "Supports inline literal values.");
    }

    return Tooltip;
}

[[nodiscard]] std::string BuildCanvasNodeTooltip(const GraphAsset& Asset,
                                                 const GraphNodeAsset& Node,
                                                 const SchemaNodeDescriptor* Descriptor)
{
    if (Descriptor && !Descriptor->Tooltip.empty())
    {
        return Descriptor->Tooltip;
    }

    std::string Tooltip{};
    AppendWrappedTooltipLine(Tooltip, DescribeNodeTitle(Asset, Node));
    if (const std::string Detail = DescribeNodeDetail(Asset, Node); !Detail.empty())
    {
        AppendWrappedTooltipLine(Tooltip, Detail);
    }
    return Tooltip;
}

[[nodiscard]] std::optional<CanvasWireView> BuildCanvasWire(const GraphAsset& Asset,
                                                            const GraphNodeAsset& SourceNode,
                                                            const GraphNodeAsset& TargetNode,
                                                            const std::string_view SourcePin,
                                                            const std::string_view TargetPin)
{
    const Uuid TargetNodeId = ResolveExecTargetNodeId(Asset, SourceNode, SourcePin);
    if (TargetNodeId != Uuid{} && TargetNodeId == TargetNode.Id)
    {
        return CanvasWireView{
            .SourceNodeId = SourceNode.Id,
            .SourcePin = std::string(SourcePin),
            .TargetNodeId = TargetNode.Id,
            .TargetPin = std::string(TargetPin),
            .Kind = ESlotKind::Value,
            .IsExec = true,
        };
    }

    return std::nullopt;
}

template<typename T>
[[nodiscard]] bool DeserializeSerializedValue(const SerializedValue& Value, T& OutValue)
{
    if (Value.Type != StaticTypeId<T>())
    {
        return false;
    }

    T LocalValue{};
    const auto DeserializeResult = DeserializeReflectedValueInto(Value.Type,
                                                                 &LocalValue,
                                                                 Value.Bytes.data(),
                                                                 Value.Bytes.size());
    if (!DeserializeResult)
    {
        return false;
    }

    OutValue = std::move(LocalValue);
    return true;
}

[[nodiscard]] bool ParseBoolText(std::string_view Text, bool& OutValue)
{
    std::string Lower{};
    Lower.reserve(Text.size());
    for (const char Character : Text)
    {
        if (!std::isspace(static_cast<unsigned char>(Character)))
        {
            Lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(Character))));
        }
    }

    if (Lower == "true" || Lower == "1")
    {
        OutValue = true;
        return true;
    }
    if (Lower == "false" || Lower == "0")
    {
        OutValue = false;
        return true;
    }
    return false;
}

template<typename T>
[[nodiscard]] bool ParseIntegralText(std::string_view Text, T& OutValue)
{
    const char* Begin = Text.data();
    const char* End = Text.data() + Text.size();
    while (Begin != End && std::isspace(static_cast<unsigned char>(*Begin)))
    {
        ++Begin;
    }
    while (End != Begin && std::isspace(static_cast<unsigned char>(*(End - 1))))
    {
        --End;
    }
    if (Begin == End)
    {
        return false;
    }

    T ParsedValue{};
    const auto [Ptr, Error] = std::from_chars(Begin, End, ParsedValue);
    if (Error != std::errc{} || Ptr != End)
    {
        return false;
    }

    OutValue = ParsedValue;
    return true;
}

template<typename T>
[[nodiscard]] bool ParseFloatingText(std::string_view Text, T& OutValue)
{
    std::string Buffer(Text);
    char* ParseEnd = nullptr;
    errno = 0;
    const long double Parsed = std::strtold(Buffer.c_str(), &ParseEnd);
    if (ParseEnd == Buffer.c_str() || *ParseEnd != '\0' || errno == ERANGE)
    {
        return false;
    }

    OutValue = static_cast<T>(Parsed);
    return true;
}

[[nodiscard]] bool ParseUuidText(std::string_view Text, Uuid& OutValue)
{
    const auto Parsed = uuids::uuid::from_string(std::string(Text));
    if (!Parsed.has_value())
    {
        return false;
    }

    OutValue = *Parsed;
    return true;
}

[[nodiscard]] bool TryFormatTextSerializedValue(const TypeId& Type, const SerializedValue& Value, std::string& OutText)
{
    if (Value.Type == TypeId{})
    {
        OutText.clear();
        return true;
    }

    if (Type == StaticTypeId<std::string>())
    {
        std::string Decoded{};
        if (!DeserializeSerializedValue(Value, Decoded))
        {
            return false;
        }
        OutText = std::move(Decoded);
        return true;
    }
    if (Type == StaticTypeId<std::int32_t>())
    {
        std::int32_t Decoded = 0;
        if (!DeserializeSerializedValue(Value, Decoded))
        {
            return false;
        }
        OutText = std::to_string(Decoded);
        return true;
    }
    if (Type == StaticTypeId<int>())
    {
        int Decoded = 0;
        if (!DeserializeSerializedValue(Value, Decoded))
        {
            return false;
        }
        OutText = std::to_string(Decoded);
        return true;
    }
    if (Type == StaticTypeId<std::int64_t>())
    {
        std::int64_t Decoded = 0;
        if (!DeserializeSerializedValue(Value, Decoded))
        {
            return false;
        }
        OutText = std::to_string(Decoded);
        return true;
    }
    if (Type == StaticTypeId<std::uint32_t>())
    {
        std::uint32_t Decoded = 0;
        if (!DeserializeSerializedValue(Value, Decoded))
        {
            return false;
        }
        OutText = std::to_string(Decoded);
        return true;
    }
    if (Type == StaticTypeId<unsigned int>())
    {
        unsigned int Decoded = 0;
        if (!DeserializeSerializedValue(Value, Decoded))
        {
            return false;
        }
        OutText = std::to_string(Decoded);
        return true;
    }
    if (Type == StaticTypeId<std::uint64_t>())
    {
        std::uint64_t Decoded = 0;
        if (!DeserializeSerializedValue(Value, Decoded))
        {
            return false;
        }
        OutText = std::to_string(Decoded);
        return true;
    }
    if (Type == StaticTypeId<float>())
    {
        float Decoded = 0.0f;
        if (!DeserializeSerializedValue(Value, Decoded))
        {
            return false;
        }
        std::ostringstream Stream{};
        Stream << Decoded;
        OutText = Stream.str();
        return true;
    }
    if (Type == StaticTypeId<double>())
    {
        double Decoded = 0.0;
        if (!DeserializeSerializedValue(Value, Decoded))
        {
            return false;
        }
        std::ostringstream Stream{};
        Stream << Decoded;
        OutText = Stream.str();
        return true;
    }
    if (Type == StaticTypeId<Uuid>())
    {
        Uuid Decoded{};
        if (!DeserializeSerializedValue(Value, Decoded))
        {
            return false;
        }
        OutText = ToString(Decoded);
        return true;
    }

    return false;
}

[[nodiscard]] bool TryFormatTextDefault(const GraphVariableAsset& Variable, std::string& OutText)
{
    return TryFormatTextSerializedValue(Variable.Type, Variable.DefaultValue, OutText);
}

template<typename T>
[[nodiscard]] TExpected<SerializedValue> MakeSerializedFromTextParser(const std::string_view Text,
                                                                      bool (*Parser)(std::string_view, T&))
{
    T Value{};
    if (!Parser(Text, Value))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit variable default text could not be parsed"));
    }
    return SerializedValue::FromValue(Value);
}

[[nodiscard]] TExpected<SerializedValue> TryParseTextSerializedValue(const TypeId& Type, const std::string_view Text)
{
    if (Type == StaticTypeId<std::string>())
    {
        return SerializedValue::FromValue(std::string(Text));
    }
    if (Type == StaticTypeId<std::int32_t>())
    {
        return MakeSerializedFromTextParser<std::int32_t>(Text, &ParseIntegralText<std::int32_t>);
    }
    if (Type == StaticTypeId<int>())
    {
        return MakeSerializedFromTextParser<int>(Text, &ParseIntegralText<int>);
    }
    if (Type == StaticTypeId<std::int64_t>())
    {
        return MakeSerializedFromTextParser<std::int64_t>(Text, &ParseIntegralText<std::int64_t>);
    }
    if (Type == StaticTypeId<std::uint32_t>())
    {
        return MakeSerializedFromTextParser<std::uint32_t>(Text, &ParseIntegralText<std::uint32_t>);
    }
    if (Type == StaticTypeId<unsigned int>())
    {
        return MakeSerializedFromTextParser<unsigned int>(Text, &ParseIntegralText<unsigned int>);
    }
    if (Type == StaticTypeId<std::uint64_t>())
    {
        return MakeSerializedFromTextParser<std::uint64_t>(Text, &ParseIntegralText<std::uint64_t>);
    }
    if (Type == StaticTypeId<float>())
    {
        return MakeSerializedFromTextParser<float>(Text, &ParseFloatingText<float>);
    }
    if (Type == StaticTypeId<double>())
    {
        return MakeSerializedFromTextParser<double>(Text, &ParseFloatingText<double>);
    }
    if (Type == StaticTypeId<Uuid>())
    {
        return MakeSerializedFromTextParser<Uuid>(Text, &ParseUuidText);
    }

    return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit variable type does not support text defaults"));
}

[[nodiscard]] TExpected<SerializedValue> TryParseTextDefault(const GraphVariableAsset& Variable, const std::string_view Text)
{
    return TryParseTextSerializedValue(Variable.Type, Text);
}

[[nodiscard]] std::shared_ptr<void> AllocateRuntimeStorage(const TypeInfo& Type)
{
    if (Type.Size == 0 || Type.Align == 0)
    {
        return {};
    }

    void* Storage = ::operator new(Type.Size, std::align_val_t(Type.Align));
    return std::shared_ptr<void>(Storage, [&Type](void* Ptr) {
        if (!Ptr)
        {
            return;
        }

        if (Type.RuntimeOps && Type.RuntimeOps->Destroy)
        {
            Type.RuntimeOps->Destroy(Ptr);
        }
        ::operator delete(Ptr, std::align_val_t(Type.Align));
    });
}

[[nodiscard]] bool SupportsTextVariableEditor(const TypeId& Type)
{
    return Type == StaticTypeId<std::string>() ||
           Type == StaticTypeId<std::int32_t>() ||
           Type == StaticTypeId<int>() ||
           Type == StaticTypeId<std::int64_t>() ||
           Type == StaticTypeId<std::uint32_t>() ||
           Type == StaticTypeId<unsigned int>() ||
           Type == StaticTypeId<std::uint64_t>() ||
           Type == StaticTypeId<float>() ||
           Type == StaticTypeId<double>() ||
           Type == StaticTypeId<Uuid>();
}

[[nodiscard]] bool SupportsConduitVariableType(const TypeInfo& Type)
{
    if (Type.Id == TypeId{} || Type.Size == 0 || Type.Align == 0 || !Type.RuntimeOps)
    {
        return false;
    }

    if (Type.Id == StaticTypeId<void>() ||
        TypeRegistry::Instance().IsA(Type.Id, StaticTypeId<BaseNode>()))
    {
        return false;
    }

    return true;
}

[[nodiscard]] bool TryReadEnumValueBits(const TypeInfo& Type, const SerializedValue& Value, std::uint64_t& OutBits)
{
    if (!Type.IsEnum || Value.Type != Type.Id)
    {
        return false;
    }

    const std::shared_ptr<void> Storage = AllocateRuntimeStorage(Type);
    if (!Storage)
    {
        return false;
    }

    if (const auto ConstructResult = Value.ConstructInto(Storage.get()); !ConstructResult)
    {
        return false;
    }

    OutBits = 0;
    std::memcpy(&OutBits, Storage.get(), std::min(Type.Size, sizeof(OutBits)));
    return true;
}

[[nodiscard]] TExpected<SerializedValue> MakeSerializedEnumValue(const TypeInfo& Type, const EnumValueInfo& Entry)
{
    if (!Type.IsEnum || Type.Size == 0)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit variable type is not an enum"));
    }

    const std::shared_ptr<void> Storage = AllocateRuntimeStorage(Type);
    if (!Storage)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit enum storage could not be allocated"));
    }

    std::memset(Storage.get(), 0, Type.Size);
    std::memcpy(Storage.get(), &Entry.Value, std::min(Type.Size, sizeof(Entry.Value)));

    SerializedValue Result{};
    Result.Type = Type.Id;
    const auto SerializeResult = SerializeReflectedValue(Type.Id, Storage.get(), Result.Bytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error());
    }

    return Result;
}

[[nodiscard]] bool IsVariableNameAvailable(const GraphAsset& Asset,
                                           const std::string_view Name,
                                           const std::optional<Uuid>& IgnoreId = std::nullopt)
{
    if (Name.empty())
    {
        return false;
    }

    return std::none_of(Asset.Variables.begin(), Asset.Variables.end(), [&Name, &IgnoreId](const GraphVariableAsset& Variable) {
        return Variable.Name == Name && (!IgnoreId.has_value() || Variable.Id != *IgnoreId);
    });
}

[[nodiscard]] std::size_t CountDiagnostics(const std::optional<CompileOutput>& Output,
                                          const ECompileDiagnosticSeverity Severity)
{
    if (!Output)
    {
        return 0;
    }

    std::size_t Count = 0;
    for (const CompileDiagnostic& Diagnostic : Output->Diagnostics)
    {
        if (Diagnostic.Severity == Severity)
        {
            ++Count;
        }
    }
    return Count;
}

[[nodiscard]] bool ShouldDescribeMemberOnCurrentInstanceType(
    const TypeId& InstanceType,
    const TypeId& DeclaringType,
    const std::unordered_set<TypeId, UuidHash>* AvailableInstanceTypes)
{
    return !AvailableInstanceTypes || !AvailableInstanceTypes->contains(DeclaringType) || InstanceType == DeclaringType;
}

[[nodiscard]] TypeId ResolveInstancePaletteType(const TypeId& InstanceType,
                                                const TypeId& DeclaringType,
                                                const std::unordered_set<TypeId, UuidHash>* AvailableInstanceTypes)
{
    return (AvailableInstanceTypes && AvailableInstanceTypes->contains(DeclaringType)) ? DeclaringType : InstanceType;
}

[[nodiscard]] std::string BuildReflectionPaletteCategory(const bool SelfContext,
                                                         const std::string_view MemberKind,
                                                         const std::string_view TypeLabel)
{
    std::string Category = SelfContext ? "Reflection/Self/" : "Reflection/Instance/";
    Category += MemberKind;
    Category += "/";
    Category += TypeLabel;
    return Category;
}

[[nodiscard]] std::string BuildReflectedFieldNodeTooltip(const FieldInfo& Field,
                                                         const TypeId& OwnerType,
                                                         const bool SelfContext,
                                                         const bool IsWrite)
{
    const std::string FieldLabel = ResolveFieldDisplayLabel(Field);
    const std::string OwnerLabel = ResolveTypeDisplayLabel(OwnerType);
    const std::string Summary = IsWrite
        ? ("Write reflected field '" + FieldLabel + "' on " + (SelfContext ? std::string("self") : OwnerLabel) + ".")
        : ("Read reflected field '" + FieldLabel + "' from " + (SelfContext ? std::string("self") : OwnerLabel) + ".");
    return BuildReflectedNodeTooltipSummary(Summary, Field.Doc, Field.FieldType, &Field.Value);
}

[[nodiscard]] std::string BuildReflectedFieldValuePinTooltip(const FieldInfo& Field, const bool IsInput)
{
    const std::string Summary =
        (IsInput ? std::string("Input value for field '") : std::string("Output value for field '")) +
        ResolveFieldDisplayLabel(Field) + "'.";
    return BuildReflectedNodeTooltipSummary(Summary, Field.Doc, Field.FieldType, &Field.Value);
}

[[nodiscard]] std::string BuildReflectedMethodNodeTooltip(const MethodInfo& Method,
                                                          const TypeId& OwnerType,
                                                          const bool SelfContext)
{
    const std::string Summary = "Invoke reflected method '" + ResolveMethodDisplayLabel(Method) + "' on " +
        (SelfContext ? std::string("self") : ResolveTypeDisplayLabel(OwnerType)) + ".";
    std::string Tooltip = BuildReflectedNodeTooltipSummary(Summary, Method.Doc, {});
    if (Method.ReturnType != StaticTypeId<void>())
    {
        AppendTooltipLine(Tooltip, "Return: " + ResolveTypeDisplayLabel(Method.ReturnType));
    }
    return Tooltip;
}

[[nodiscard]] std::string BuildReflectedMethodParamTooltip(const MethodInfo& Method, const std::size_t Index)
{
    const CallableParamInfo* Param = Index < Method.Params.size() ? &Method.Params[Index] : nullptr;
    const std::string ParamName = (Param && !Param->Name.empty()) ? Param->Name : BuildArgName(Index);
    const TypeId ParamType = Index < Method.ParamTypes.size() ? Method.ParamTypes[Index] : TypeId{};
    const std::string ParamDoc = (Param != nullptr) ? Param->Doc : std::string{};
    return BuildReflectedNodeTooltipSummary("Input parameter '" + ParamName + "'.", ParamDoc, ParamType);
}

[[nodiscard]] std::string BuildReflectedMethodReturnTooltip(const MethodInfo& Method)
{
    return BuildReflectedNodeTooltipSummary("Return value.", {}, Method.ReturnType);
}

std::vector<SchemaNodeDescriptor> DescribeMembers(const TypeInfo& OwnerType,
                                                  const bool SelfContext,
                                                  const std::unordered_set<TypeId, UuidHash>* AvailableInstanceTypes)
{
    std::vector<SchemaNodeDescriptor> Result{};

    const auto Fields = TypeRegistry::Instance().CollectFields(OwnerType.Id, true);
    for (const ReflectedFieldRef& Ref : Fields)
    {
        if (!Ref.Field)
        {
            continue;
        }

        const std::string ContextPrefix = SelfContext ? "self" : "instance";
        const TypeId PaletteType = SelfContext
            ? Ref.OwnerType
            : ResolveInstancePaletteType(OwnerType.Id, Ref.OwnerType, AvailableInstanceTypes);
        if (!SelfContext &&
            !ShouldDescribeMemberOnCurrentInstanceType(OwnerType.Id, Ref.OwnerType, AvailableInstanceTypes))
        {
            continue;
        }
        const TypeInfo* PaletteTypeInfo = TypeRegistry::Instance().Find(PaletteType);
        const std::string PaletteTypeLabel = PaletteTypeInfo ? ResolveTypeLabel(PaletteTypeInfo->Id) : ResolveTypeLabel(PaletteType);
        const std::string FieldLabel = ResolveFieldDisplayLabel(*Ref.Field);

        if (CanConduitReadField(*Ref.Field))
        {
            SchemaNodeDescriptor Descriptor{};
            Descriptor.StableId = ContextPrefix + std::string(".field.read.") + PaletteTypeLabel + "." + Ref.Field->Name;
            Descriptor.DisplayName = "Get " + FieldLabel;
            Descriptor.Category = BuildReflectionPaletteCategory(SelfContext, "Fields", PaletteTypeLabel);
            Descriptor.Tooltip = BuildReflectedFieldNodeTooltip(*Ref.Field, Ref.OwnerType, SelfContext, false);
            Descriptor.Family = ESchemaNodeFamily::FieldRead;
            Descriptor.IsPure = true;
            Descriptor.OwnerType = Ref.OwnerType;
            Descriptor.MemberName = Ref.Field->Name;
            Descriptor.LoweredKind = SelfContext ? EGraphAssetNodeKind::SelfFieldRead : EGraphAssetNodeKind::InstanceFieldRead;
            if (!SelfContext)
            {
                auto TargetPin = MakeHandlePin("Target", ESchemaPinDirection::Input, PaletteType);
                TargetPin.Tooltip = BuildTargetPinTooltip(
                    PaletteType,
                    "Resolved target instance used to read reflected field '" + FieldLabel + "'.");
                Descriptor.Pins.push_back(std::move(TargetPin));
            }
            auto ValuePin = MakeValuePin("Value", ESchemaPinDirection::Output, Ref.Field->FieldType);
            ValuePin.Tooltip = BuildReflectedFieldValuePinTooltip(*Ref.Field, false);
            Descriptor.Pins.push_back(std::move(ValuePin));
            Result.push_back(std::move(Descriptor));
        }

        if (CanConduitWriteField(*Ref.Field))
        {
            SchemaNodeDescriptor Descriptor{};
            Descriptor.StableId = ContextPrefix + std::string(".field.write.") + PaletteTypeLabel + "." + Ref.Field->Name;
            Descriptor.DisplayName = "Set " + FieldLabel;
            Descriptor.Category = BuildReflectionPaletteCategory(SelfContext, "Fields", PaletteTypeLabel);
            Descriptor.Tooltip = BuildReflectedFieldNodeTooltip(*Ref.Field, Ref.OwnerType, SelfContext, true);
            Descriptor.Family = ESchemaNodeFamily::FieldWrite;
            Descriptor.IsPure = false;
            Descriptor.OwnerType = Ref.OwnerType;
            Descriptor.MemberName = Ref.Field->Name;
            Descriptor.LoweredKind = SelfContext ? EGraphAssetNodeKind::SelfFieldWrite : EGraphAssetNodeKind::InstanceFieldWrite;
            Descriptor.Pins.push_back(MakeExecPin("In", ESchemaPinDirection::Input));
            if (!SelfContext)
            {
                auto TargetPin = MakeHandlePin("Target", ESchemaPinDirection::Input, PaletteType);
                TargetPin.Tooltip = BuildTargetPinTooltip(
                    PaletteType,
                    "Resolved target instance used to write reflected field '" + FieldLabel + "'.");
                Descriptor.Pins.push_back(std::move(TargetPin));
            }
            auto ValuePin = MakeValuePin("Value", ESchemaPinDirection::Input, Ref.Field->FieldType, true);
            ValuePin.Tooltip = BuildReflectedFieldValuePinTooltip(*Ref.Field, true);
            Descriptor.Pins.push_back(std::move(ValuePin));
            Descriptor.Pins.push_back(MakeExecPin("Out", ESchemaPinDirection::Output));
            Result.push_back(std::move(Descriptor));
        }
    }

    const auto Methods = TypeRegistry::Instance().CollectMethods(OwnerType.Id, true);
    for (const ReflectedMethodRef& Ref : Methods)
    {
        if (!Ref.Method || Ref.Method->RawInvoke == nullptr)
        {
            continue;
        }

        const std::string ContextPrefix = SelfContext ? "self" : "instance";
        const TypeId PaletteType = SelfContext
            ? Ref.OwnerType
            : ResolveInstancePaletteType(OwnerType.Id, Ref.OwnerType, AvailableInstanceTypes);
        if (!SelfContext &&
            !ShouldDescribeMemberOnCurrentInstanceType(OwnerType.Id, Ref.OwnerType, AvailableInstanceTypes))
        {
            continue;
        }
        const TypeInfo* PaletteTypeInfo = TypeRegistry::Instance().Find(PaletteType);
        const std::string PaletteTypeLabel = PaletteTypeInfo ? ResolveTypeLabel(PaletteTypeInfo->Id) : ResolveTypeLabel(PaletteType);
        const std::string MethodLabel = ResolveMethodDisplayLabel(*Ref.Method);

        SchemaNodeDescriptor Descriptor{};
        Descriptor.StableId = ContextPrefix + std::string(".method.") + PaletteTypeLabel + "." + Ref.Method->Name;
        Descriptor.DisplayName = SelfContext
            ? "Call " + MethodLabel
            : "Call " + PaletteTypeLabel + "::" + MethodLabel;
        Descriptor.Category = BuildReflectionPaletteCategory(SelfContext, "Methods", PaletteTypeLabel);
        Descriptor.Tooltip = BuildReflectedMethodNodeTooltip(*Ref.Method, Ref.OwnerType, SelfContext);
        Descriptor.Family = ESchemaNodeFamily::MethodCall;
        Descriptor.IsPure = Ref.Method->IsConst;
        Descriptor.OwnerType = Ref.OwnerType;
        Descriptor.MemberName = Ref.Method->Name;
        Descriptor.LoweredKind = SelfContext ? EGraphAssetNodeKind::SelfMethodCall : EGraphAssetNodeKind::InstanceMethodCall;

        if (!Descriptor.IsPure)
        {
            Descriptor.Pins.push_back(MakeExecPin("In", ESchemaPinDirection::Input));
        }

        if (!SelfContext)
        {
            auto TargetPin = MakeHandlePin("Target", ESchemaPinDirection::Input, PaletteType);
            TargetPin.Tooltip = BuildTargetPinTooltip(
                PaletteType,
                "Resolved target instance used to invoke reflected method '" + MethodLabel + "'.");
            Descriptor.Pins.push_back(std::move(TargetPin));
        }

        for (std::size_t Index = 0; Index < Ref.Method->ParamTypes.size(); ++Index)
        {
            const CallableParamInfo* Param = Index < Ref.Method->Params.size() ? &Ref.Method->Params[Index] : nullptr;
            const std::string ParamName = (Param && !Param->Name.empty()) ? Param->Name : BuildArgName(Index);
            auto ParamPin = MakeValuePin(ParamName,
                                         ESchemaPinDirection::Input,
                                         Ref.Method->ParamTypes[Index],
                                         true);
            ParamPin.Tooltip = BuildReflectedMethodParamTooltip(*Ref.Method, Index);
            Descriptor.Pins.push_back(std::move(ParamPin));
        }

        if (Ref.Method->ReturnType != StaticTypeId<void>())
        {
            auto ReturnPin = MakeValuePin("Return", ESchemaPinDirection::Output, Ref.Method->ReturnType);
            ReturnPin.Tooltip = BuildReflectedMethodReturnTooltip(*Ref.Method);
            Descriptor.Pins.push_back(std::move(ReturnPin));
        }

        if (!Descriptor.IsPure)
        {
            Descriptor.Pins.push_back(MakeExecPin("Out", ESchemaPinDirection::Output));
        }

        Result.push_back(std::move(Descriptor));
    }

    return Result;
}

std::vector<SchemaNodeDescriptor> DescribeVariablesInternal(const GraphAsset& Asset)
{
    std::vector<SchemaNodeDescriptor> Result{};
    Result.reserve(Asset.Variables.size() * 2);

    for (const GraphVariableAsset& Variable : Asset.Variables)
    {
        SchemaNodeDescriptor GetDescriptor{};
        GetDescriptor.StableId = "variable.get." + ToString(Variable.Id);
        GetDescriptor.DisplayName = "Get " + Variable.Name;
        GetDescriptor.Category = "Variables";
        GetDescriptor.Tooltip = "Read graph variable '" + Variable.Name + "'.";
        GetDescriptor.Family = ESchemaNodeFamily::Variable;
        GetDescriptor.IsPure = true;
        GetDescriptor.VariableId = Variable.Id;
        GetDescriptor.LoweredKind = EGraphAssetNodeKind::VariableGet;
        GetDescriptor.Pins.push_back(MakeValuePin("Value", ESchemaPinDirection::Output, Variable.Type));
        Result.push_back(std::move(GetDescriptor));

        SchemaNodeDescriptor SetDescriptor{};
        SetDescriptor.StableId = "variable.set." + ToString(Variable.Id);
        SetDescriptor.DisplayName = "Set " + Variable.Name;
        SetDescriptor.Category = "Variables";
        SetDescriptor.Tooltip = "Write graph variable '" + Variable.Name + "'.";
        SetDescriptor.Family = ESchemaNodeFamily::Variable;
        SetDescriptor.VariableId = Variable.Id;
        SetDescriptor.LoweredKind = EGraphAssetNodeKind::VariableSet;
        SetDescriptor.Pins.push_back(MakeExecPin("In", ESchemaPinDirection::Input));
        SetDescriptor.Pins.push_back(MakeValuePin("Value", ESchemaPinDirection::Input, Variable.Type, true));
        SetDescriptor.Pins.push_back(MakeExecPin("Out", ESchemaPinDirection::Output));
        Result.push_back(std::move(SetDescriptor));
    }

    return Result;
}

} // namespace

GraphVariableAsset* GraphDocument::FindVariable(const Uuid& Id)
{
    const auto It = std::find_if(m_asset.Variables.begin(), m_asset.Variables.end(), [&Id](const GraphVariableAsset& Variable) {
        return Variable.Id == Id;
    });
    return It != m_asset.Variables.end() ? &(*It) : nullptr;
}

const GraphVariableAsset* GraphDocument::FindVariable(const Uuid& Id) const
{
    const auto It = std::find_if(m_asset.Variables.begin(), m_asset.Variables.end(), [&Id](const GraphVariableAsset& Variable) {
        return Variable.Id == Id;
    });
    return It != m_asset.Variables.end() ? &(*It) : nullptr;
}

GraphNodeAsset* GraphDocument::FindNode(const Uuid& Id)
{
    const auto It = std::find_if(m_asset.Nodes.begin(), m_asset.Nodes.end(), [&Id](const GraphNodeAsset& Node) {
        return Node.Id == Id;
    });
    return It != m_asset.Nodes.end() ? &(*It) : nullptr;
}

const GraphNodeAsset* GraphDocument::FindNode(const Uuid& Id) const
{
    const auto It = std::find_if(m_asset.Nodes.begin(), m_asset.Nodes.end(), [&Id](const GraphNodeAsset& Node) {
        return Node.Id == Id;
    });
    return It != m_asset.Nodes.end() ? &(*It) : nullptr;
}

GraphNodeEditorAsset* GraphDocument::FindNodeEditorState(const Uuid& Id)
{
    const auto It = std::find_if(m_asset.EditorState.Nodes.begin(),
                                 m_asset.EditorState.Nodes.end(),
                                 [&Id](const GraphNodeEditorAsset& NodeState) {
                                     return NodeState.NodeId == Id;
                                 });
    return It != m_asset.EditorState.Nodes.end() ? &(*It) : nullptr;
}

const GraphNodeEditorAsset* GraphDocument::FindNodeEditorState(const Uuid& Id) const
{
    const auto It = std::find_if(m_asset.EditorState.Nodes.begin(),
                                 m_asset.EditorState.Nodes.end(),
                                 [&Id](const GraphNodeEditorAsset& NodeState) {
                                     return NodeState.NodeId == Id;
                                 });
    return It != m_asset.EditorState.Nodes.end() ? &(*It) : nullptr;
}

TExpected<GraphVariableAsset*> GraphDocument::AddVariable(const std::string_view Name, const TypeId& Type)
{
    if (Name.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit graph variable name is empty"));
    }
    if (Type == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit graph variable type is missing"));
    }
    if (!TypeRegistry::Instance().Find(Type))
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph variable type is not registered"));
    }
    if (!IsVariableNameAvailable(m_asset, Name))
    {
        return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit graph variable name already exists"));
    }

    GraphVariableAsset Variable{};
    Variable.Id = NewUuid();
    Variable.Name = std::string(Name);
    Variable.Type = Type;
    m_asset.Variables.push_back(std::move(Variable));
    MarkMutated();
    return &m_asset.Variables.back();
}

TExpected<GraphNodeAsset*> GraphDocument::AddNode(const SchemaNodeDescriptor& Descriptor)
{
    if (!Descriptor.LoweredKind.has_value())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit schema node cannot be lowered into an authored node"));
    }

    GraphNodeAsset Node{};
    Node.Id = NewUuid();
    Node.Kind = *Descriptor.LoweredKind;
    Node.BuiltinEntryPoint = Descriptor.BuiltinEntryPoint;
    Node.MemberName = Descriptor.MemberName;
    Node.VariableId = Descriptor.VariableId;
    Node.OwnerType = Descriptor.OwnerType;
    Node.UnaryOp = Descriptor.UnaryOp;
    Node.BinaryOp = Descriptor.BinaryOp;

    switch (Node.Kind)
    {
        case EGraphAssetNodeKind::EntryPoint:
        {
            if (Node.BuiltinEntryPoint != EBuiltinEntryPoint::None)
            {
                const bool Duplicate = std::any_of(m_asset.Nodes.begin(), m_asset.Nodes.end(), [&Node](const GraphNodeAsset& Existing) {
                    return Existing.Kind == EGraphAssetNodeKind::EntryPoint &&
                           Existing.BuiltinEntryPoint == Node.BuiltinEntryPoint;
                });
                if (Duplicate)
                {
                    return std::unexpected(MakeError(EErrorCode::AlreadyExists, "That built-in Conduit entry node already exists"));
                }
            }
            else
            {
                Node.EntryPointName = MakeUniqueEntryPointName(m_asset, "Entry");
            }
            break;
        }
        case EGraphAssetNodeKind::Label:
            Node.LabelName = MakeUniqueLabelName(m_asset, "Label");
            break;
        case EGraphAssetNodeKind::Jump:
            Node.LabelName = FindFirstLabelName(m_asset).value_or("Label");
            break;
        case EGraphAssetNodeKind::Branch:
            Node.LabelName = FindFirstLabelName(m_asset).value_or("True");
            Node.FalseLabelName = "False";
            break;
        default:
            break;
    }

    if (Node.Kind == EGraphAssetNodeKind::SelfMethodCall || Node.Kind == EGraphAssetNodeKind::InstanceMethodCall)
    {
        std::size_t ArgCount = 0;
        for (const SchemaPinDescriptor& Pin : Descriptor.Pins)
        {
            if (Pin.Direction != ESchemaPinDirection::Input || Pin.Type.IsExec)
            {
                continue;
            }

            const auto ArgIndex = ResolveMethodPinIndex(Node, Pin.Name);
            if (ArgIndex.has_value())
            {
                ArgCount = std::max(ArgCount, *ArgIndex + 1);
            }
        }
        Node.Inputs.resize(ArgCount);
    }

    const std::size_t Index = m_asset.Nodes.size();
    m_asset.Nodes.push_back(std::move(Node));
    m_asset.EditorState.Nodes.push_back(GraphNodeEditorAsset{
        .NodeId = m_asset.Nodes.back().Id,
        .X = 96.0f + static_cast<float>(Index % 6) * 320.0f,
        .Y = 96.0f + static_cast<float>(Index / 6) * 220.0f,
    });
    MarkMutated();
    return &m_asset.Nodes.back();
}

bool GraphDocument::RemoveVariable(const Uuid& Id)
{
    const auto VariableIt = std::find_if(m_asset.Variables.begin(), m_asset.Variables.end(), [&Id](const GraphVariableAsset& Variable) {
        return Variable.Id == Id;
    });
    if (VariableIt == m_asset.Variables.end())
    {
        return false;
    }

    std::unordered_set<Uuid, UuidHash> RemovedNodeIds{};
    std::unordered_set<std::uint32_t> RemovedProducedSlots{};
    for (const GraphNodeAsset& Node : m_asset.Nodes)
    {
        if (Node.VariableId == Id && IsVariableNodeKind(Node.Kind) && Node.Id != Uuid{})
        {
            RemovedNodeIds.insert(Node.Id);
            VisitProducerNodeSlotRefs(Node, [&RemovedProducedSlots](const SlotId& Ref) {
                if (Ref.IsValid())
                {
                    RemovedProducedSlots.insert(Ref.Value);
                }
            });
        }
    }

    m_asset.Nodes.erase(std::remove_if(m_asset.Nodes.begin(),
                                       m_asset.Nodes.end(),
                                       [&Id](const GraphNodeAsset& Node) {
                                           return Node.VariableId == Id && IsVariableNodeKind(Node.Kind);
                                       }),
                        m_asset.Nodes.end());

    if (!RemovedNodeIds.empty())
    {
        DisconnectConsumersOfProducedSlots(m_asset, RemovedProducedSlots);
        DisconnectExecConnectionsToNode(m_asset, RemovedNodeIds);
        m_asset.EditorState.Nodes.erase(
            std::remove_if(m_asset.EditorState.Nodes.begin(),
                           m_asset.EditorState.Nodes.end(),
                           [&RemovedNodeIds](const GraphNodeEditorAsset& NodeState) {
                               return RemovedNodeIds.contains(NodeState.NodeId);
                           }),
            m_asset.EditorState.Nodes.end());

        for (GraphCommentAsset& Comment : m_asset.EditorState.Comments)
        {
            Comment.NodeIds.erase(std::remove_if(Comment.NodeIds.begin(),
                                                 Comment.NodeIds.end(),
                                                 [&RemovedNodeIds](const Uuid& NodeId) {
                                                     return RemovedNodeIds.contains(NodeId);
                                                 }),
                                  Comment.NodeIds.end());
        }

        m_selection.NodeIds.erase(std::remove_if(m_selection.NodeIds.begin(),
                                                 m_selection.NodeIds.end(),
                                                 [&RemovedNodeIds](const Uuid& NodeId) {
                                                     return RemovedNodeIds.contains(NodeId);
                                                 }),
                                  m_selection.NodeIds.end());
    }

    m_selection.VariableIds.erase(std::remove(m_selection.VariableIds.begin(), m_selection.VariableIds.end(), Id),
                                  m_selection.VariableIds.end());
    m_asset.Variables.erase(VariableIt);
    CompactAuthoredSlots(m_asset);
    MarkMutated();
    return true;
}

bool GraphDocument::RemoveNode(const Uuid& Id)
{
    const auto NodeIt = std::find_if(m_asset.Nodes.begin(), m_asset.Nodes.end(), [&Id](const GraphNodeAsset& Node) {
        return Node.Id == Id;
    });
    if (NodeIt == m_asset.Nodes.end())
    {
        return false;
    }

    std::unordered_set<std::uint32_t> ProducedSlots{};
    VisitProducerNodeSlotRefs(*NodeIt, [&ProducedSlots](const SlotId& Ref) {
        if (Ref.IsValid())
        {
            ProducedSlots.insert(Ref.Value);
        }
    });

    m_asset.Nodes.erase(NodeIt);
    DisconnectConsumersOfProducedSlots(m_asset, ProducedSlots);
    std::unordered_set<Uuid, UuidHash> RemovedNodeIds{};
    RemovedNodeIds.insert(Id);
    DisconnectExecConnectionsToNode(m_asset, RemovedNodeIds);
    m_asset.EditorState.Nodes.erase(
        std::remove_if(m_asset.EditorState.Nodes.begin(),
                       m_asset.EditorState.Nodes.end(),
                       [&Id](const GraphNodeEditorAsset& NodeState) {
                           return NodeState.NodeId == Id;
                       }),
        m_asset.EditorState.Nodes.end());

    for (GraphCommentAsset& Comment : m_asset.EditorState.Comments)
    {
        Comment.NodeIds.erase(std::remove(Comment.NodeIds.begin(), Comment.NodeIds.end(), Id), Comment.NodeIds.end());
    }

    m_selection.NodeIds.erase(std::remove(m_selection.NodeIds.begin(), m_selection.NodeIds.end(), Id), m_selection.NodeIds.end());
    CompactAuthoredSlots(m_asset);
    MarkMutated();
    return true;
}

Result GraphDocument::RenameVariable(const Uuid& Id, const std::string_view Name)
{
    if (Name.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit graph variable name is empty"));
    }

    GraphVariableAsset* Variable = FindVariable(Id);
    if (!Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph variable was not found"));
    }
    if (!IsVariableNameAvailable(m_asset, Name, Id))
    {
        return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit graph variable name already exists"));
    }
    if (Variable->Name == Name)
    {
        return Ok();
    }

    Variable->Name = std::string(Name);
    MarkMutated();
    return Ok();
}

Result GraphDocument::SetVariableType(const Uuid& Id, const TypeId& Type)
{
    if (Type == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit graph variable type is missing"));
    }
    if (!TypeRegistry::Instance().Find(Type))
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph variable type is not registered"));
    }

    GraphVariableAsset* Variable = FindVariable(Id);
    if (!Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph variable was not found"));
    }
    if (Variable->Type == Type)
    {
        return Ok();
    }

    Variable->Type = Type;
    Variable->DefaultValue = {};
    MarkMutated();
    return Ok();
}

Result GraphDocument::SetVariableDefault(const Uuid& Id, const SerializedValue& Value)
{
    GraphVariableAsset* Variable = FindVariable(Id);
    if (!Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph variable was not found"));
    }
    if (Value.Type == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit graph variable default type is missing"));
    }
    if (Value.Type != Variable->Type)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit graph variable default type mismatch"));
    }

    Variable->DefaultValue = Value;
    MarkMutated();
    return Ok();
}

Result GraphDocument::ClearVariableDefault(const Uuid& Id)
{
    GraphVariableAsset* Variable = FindVariable(Id);
    if (!Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph variable was not found"));
    }
    if (Variable->DefaultValue.Type == TypeId{})
    {
        return Ok();
    }

    Variable->DefaultValue = {};
    MarkMutated();
    return Ok();
}

Result GraphDocument::SetNodeEntryPointName(const Uuid& Id, const std::string_view Name)
{
    if (Name.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit entry point name is empty"));
    }

    GraphNodeAsset* Node = FindNode(Id);
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph node was not found"));
    }
    if (Node->Kind != EGraphAssetNodeKind::EntryPoint || Node->BuiltinEntryPoint != EBuiltinEntryPoint::None)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit node is not a custom entry point"));
    }
    if (!IsEntryPointNameAvailable(m_asset, Name, Id))
    {
        return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit entry point name already exists"));
    }
    if (Node->EntryPointName == Name)
    {
        return Ok();
    }

    Node->EntryPointName = std::string(Name);
    MarkMutated();
    return Ok();
}

Result GraphDocument::SetNodeLabelName(const Uuid& Id, const std::string_view Label)
{
    if (Label.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit label text is empty"));
    }

    GraphNodeAsset* Node = FindNode(Id);
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph node was not found"));
    }

    switch (Node->Kind)
    {
    case EGraphAssetNodeKind::Label:
        if (!IsLabelNameAvailable(m_asset, Label, Id))
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit label name already exists"));
        }
        if (Node->LabelName == Label)
        {
            return Ok();
        }
        Node->LabelName = std::string(Label);
        MarkMutated();
        return Ok();
    case EGraphAssetNodeKind::Jump:
    case EGraphAssetNodeKind::Branch:
        if (Node->LabelName == Label)
        {
            return Ok();
        }
        Node->LabelName = std::string(Label);
        MarkMutated();
        return Ok();
    default:
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit node has no editable primary label"));
    }
}

Result GraphDocument::SetNodeFalseLabelName(const Uuid& Id, const std::string_view Label)
{
    if (Label.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit false label text is empty"));
    }

    GraphNodeAsset* Node = FindNode(Id);
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph node was not found"));
    }
    if (Node->Kind != EGraphAssetNodeKind::Branch)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit node has no editable false label"));
    }
    if (Node->FalseLabelName == Label)
    {
        return Ok();
    }

    Node->FalseLabelName = std::string(Label);
    MarkMutated();
    return Ok();
}

Result GraphDocument::SetNodeInputDefault(const Uuid& Id, const std::string_view PinKey, const SerializedValue& Value)
{
    if (PinKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit node input pin key is empty"));
    }
    if (Value.Type == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit node input default type is missing"));
    }

    GraphNodeAsset* Node = FindNode(Id);
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph node was not found"));
    }

    SchemaRegistry Schema{};
    Schema.RebuildBuiltins();
    const auto Descriptors = BuildActiveSchemaDescriptors(Schema, m_asset);
    const SchemaNodeDescriptor* Descriptor = FindSchemaDescriptorForNode(Descriptors, *Node);
    if (!Descriptor)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit schema node descriptor was not found"));
    }

    const SchemaPinDescriptor* Pin = nullptr;
    std::string ResolvedPinName{};
    for (const SchemaPinDescriptor& Candidate : Descriptor->Pins)
    {
        if (Candidate.Direction != ESchemaPinDirection::Input || Candidate.Type.IsExec || !Candidate.SupportsLiteral)
        {
            continue;
        }

        if (CanonicalizeNodeInputPinKey(*Node, Candidate.Name) == PinKey)
        {
            Pin = &Candidate;
            ResolvedPinName = Candidate.Name;
            break;
        }
    }

    if (!Pin)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit node input pin was not found or does not support defaults"));
    }

    auto BindingResult = ResolveNodePinBinding(m_asset, *Node, *Descriptor, ResolvedPinName, ESchemaPinDirection::Input);
    if (!BindingResult)
    {
        return std::unexpected(BindingResult.error());
    }

    const TypeId ExpectedType = BindingResult->StorageType != TypeId{} ? BindingResult->StorageType : BindingResult->DisplayType;
    if (ExpectedType == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch,
                                         "Conduit node input type must be inferred before a default can be authored"));
    }
    if (Value.Type != ExpectedType)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit node input default type mismatch"));
    }

    if (GraphNodeInputDefaultAsset* Existing = FindMutableNodeInputDefault(*Node, PinKey))
    {
        Existing->Value = Value;
    }
    else
    {
        Node->InputDefaults.push_back(GraphNodeInputDefaultAsset{
            .PinKey = std::string(PinKey),
            .Value = Value,
        });
    }

    MarkMutated();
    return Ok();
}

Result GraphDocument::ClearNodeInputDefault(const Uuid& Id, const std::string_view PinKey)
{
    if (PinKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit node input pin key is empty"));
    }

    GraphNodeAsset* Node = FindNode(Id);
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph node was not found"));
    }

    const auto It = std::find_if(Node->InputDefaults.begin(),
                                 Node->InputDefaults.end(),
                                 [PinKey](const GraphNodeInputDefaultAsset& Entry) {
                                     return Entry.PinKey == PinKey;
                                 });
    if (It == Node->InputDefaults.end())
    {
        return Ok();
    }

    Node->InputDefaults.erase(It);
    MarkMutated();
    return Ok();
}

Result GraphDocument::SetNodePosition(const Uuid& Id, const float X, const float Y)
{
    if (!std::isfinite(X) || !std::isfinite(Y))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit node graph position must be finite"));
    }
    if (!FindNode(Id))
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph node was not found"));
    }

    GraphNodeEditorAsset* NodeState = FindNodeEditorState(Id);
    if (!NodeState)
    {
        m_asset.EditorState.Nodes.push_back(GraphNodeEditorAsset{
            .NodeId = Id,
            .X = X,
            .Y = Y,
        });
        MarkCanvasMutated();
        return Ok();
    }

    if (NodeState->X == X && NodeState->Y == Y)
    {
        return Ok();
    }

    NodeState->X = X;
    NodeState->Y = Y;
    MarkCanvasMutated();
    return Ok();
}

Result GraphDocument::ConnectPins(const Uuid& SourceNodeId,
                                  const std::string_view SourcePin,
                                  const Uuid& TargetNodeId,
                                  const std::string_view TargetPin)
{
    if (SourcePin.empty() || TargetPin.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit pin names are empty"));
    }

    GraphNodeAsset* SourceNode = FindNode(SourceNodeId);
    GraphNodeAsset* TargetNode = FindNode(TargetNodeId);
    if (!SourceNode || !TargetNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit source or target node was not found"));
    }

    SchemaRegistry Schema{};
    Schema.RebuildBuiltins();
    const auto Descriptors = BuildActiveSchemaDescriptors(Schema, m_asset);
    const SchemaNodeDescriptor* SourceDescriptor = FindSchemaDescriptorForNode(Descriptors, *SourceNode);
    const SchemaNodeDescriptor* TargetDescriptor = FindSchemaDescriptorForNode(Descriptors, *TargetNode);
    if (!SourceDescriptor || !TargetDescriptor)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit schema node descriptor was not found"));
    }

    auto SourceBindingResult = ResolveNodePinBinding(m_asset, *SourceNode, *SourceDescriptor, SourcePin, ESchemaPinDirection::Output);
    if (!SourceBindingResult)
    {
        return std::unexpected(SourceBindingResult.error());
    }
    auto TargetBindingResult = ResolveNodePinBinding(m_asset, *TargetNode, *TargetDescriptor, TargetPin, ESchemaPinDirection::Input);
    if (!TargetBindingResult)
    {
        return std::unexpected(TargetBindingResult.error());
    }

    NodePinBinding& SourceBinding = *SourceBindingResult;
    NodePinBinding& TargetBinding = *TargetBindingResult;
    if (SourceBinding.Pin->Type.IsExec || TargetBinding.Pin->Type.IsExec)
    {
        if (!(SourceBinding.Pin->Type.IsExec && TargetBinding.Pin->Type.IsExec))
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit exec pins can only connect to exec pins"));
        }
        Uuid* MutableExecTarget = ResolveMutableExecTargetNodeId(*SourceNode, SourcePin);
        if (!MutableExecTarget)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit exec pin cannot author an outgoing exec target"));
        }
        if (*MutableExecTarget == TargetNode->Id)
        {
            return Ok();
        }

        *MutableExecTarget = TargetNode->Id;
        if (TargetNode->Kind == EGraphAssetNodeKind::Label && !TargetNode->LabelName.empty())
        {
            if (SourceNode->Kind == EGraphAssetNodeKind::Jump && SourcePin == "Out")
            {
                SourceNode->LabelName = TargetNode->LabelName;
            }
            else if (SourceNode->Kind == EGraphAssetNodeKind::Branch && SourcePin == "True")
            {
                SourceNode->LabelName = TargetNode->LabelName;
            }
            else if (SourceNode->Kind == EGraphAssetNodeKind::Branch && SourcePin == "False")
            {
                SourceNode->FalseLabelName = TargetNode->LabelName;
            }
        }
        MarkMutated();
        return Ok();
    }

    auto StorageTypeResult = ResolveConnectionStorageType(SourceBinding, TargetBinding);
    if (!StorageTypeResult)
    {
        return std::unexpected(StorageTypeResult.error());
    }
    if (!SourceBinding.MutableSlot || !TargetBinding.MutableSlot)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit pin connection does not map to mutable authored slot refs"));
    }

    const ESlotKind SlotKind = SourceBinding.Pin->Type.Kind;
    if (!SourceBinding.MutableSlot->IsValid())
    {
        *SourceBinding.MutableSlot = AppendAuthoredSlot(m_asset, SourcePin, *StorageTypeResult, SlotKind);
    }

    const bool AlreadyConnected = TargetBinding.MutableSlot->IsValid() &&
                                  TargetBinding.MutableSlot->Value == SourceBinding.MutableSlot->Value;
    if (AlreadyConnected)
    {
        return Ok();
    }

    *TargetBinding.MutableSlot = *SourceBinding.MutableSlot;
    CompactAuthoredSlots(m_asset);
    MarkMutated();
    return Ok();
}

Result GraphDocument::SetViewport(const float PanX, const float PanY, const float Zoom)
{
    if (!std::isfinite(PanX) || !std::isfinite(PanY) || !std::isfinite(Zoom) || Zoom <= 0.0f)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit graph viewport values are invalid"));
    }

    if (m_asset.EditorState.Viewport.PanX == PanX &&
        m_asset.EditorState.Viewport.PanY == PanY &&
        m_asset.EditorState.Viewport.Zoom == Zoom)
    {
        return Ok();
    }

    m_asset.EditorState.Viewport.PanX = PanX;
    m_asset.EditorState.Viewport.PanY = PanY;
    m_asset.EditorState.Viewport.Zoom = Zoom;
    MarkCanvasMutated();
    return Ok();
}

Result GraphDocument::SetSelfType(const TypeId& Type)
{
    if (Type != TypeId{} && !TypeRegistry::Instance().Find(Type))
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit graph self type is not registered"));
    }
    if (m_asset.SelfType == Type)
    {
        return Ok();
    }

    m_asset.SelfType = Type;
    MarkMutated();
    return Ok();
}

void GraphDocument::MarkMutated()
{
    ClearLastCompile();
    Touch();
}

void GraphDocument::MarkCanvasMutated()
{
    // Canvas layout edits are persisted authoring metadata, not compile-affecting graph changes.
    TouchCanvas();
}

void SchemaRegistry::RebuildBuiltins()
{
    m_builtins.clear();

    {
        SchemaNodeDescriptor Descriptor{};
        Descriptor.StableId = "entry.custom";
        Descriptor.DisplayName = "Custom Entry";
        Descriptor.Category = "Entry Nodes";
        Descriptor.Tooltip = "Declare a named externally callable entrypoint.";
        Descriptor.Family = ESchemaNodeFamily::EntryPoint;
        Descriptor.RequiresSpecialization = true;
        Descriptor.BuiltinEntryPoint = EBuiltinEntryPoint::None;
        Descriptor.LoweredKind = EGraphAssetNodeKind::EntryPoint;
        Descriptor.Pins.push_back(MakeExecPin("Out", ESchemaPinDirection::Output));
        m_builtins.push_back(std::move(Descriptor));
    }

    for (const EBuiltinEntryPoint EntryPoint :
         {EBuiltinEntryPoint::OnCreate,
          EBuiltinEntryPoint::PreTick,
          EBuiltinEntryPoint::Tick,
          EBuiltinEntryPoint::FixedTick,
          EBuiltinEntryPoint::LateTick,
          EBuiltinEntryPoint::PostTick,
          EBuiltinEntryPoint::OnDestroy})
    {
        SchemaNodeDescriptor Descriptor{};
        Descriptor.StableId = std::string("entry.") + std::string(BuiltinEntryPointName(EntryPoint));
        Descriptor.DisplayName = std::string(BuiltinEntryPointName(EntryPoint));
        Descriptor.Category = "Entry Nodes";
        Descriptor.Tooltip = "Declare a built-in lifecycle entrypoint.";
        Descriptor.Family = ESchemaNodeFamily::EntryPoint;
        Descriptor.BuiltinEntryPoint = EntryPoint;
        Descriptor.LoweredKind = EGraphAssetNodeKind::EntryPoint;
        Descriptor.Pins.push_back(MakeExecPin("Out", ESchemaPinDirection::Output));
        if (BuiltinEntryPointUsesDeltaSeconds(EntryPoint))
        {
            Descriptor.Pins.push_back(MakeValuePin("DeltaSeconds", ESchemaPinDirection::Output, StaticTypeId<float>()));
        }
        m_builtins.push_back(std::move(Descriptor));
    }

    {
        SchemaNodeDescriptor Descriptor{};
        Descriptor.StableId = "builtin.label";
        Descriptor.DisplayName = "Label";
        Descriptor.Category = "Flow";
        Descriptor.Tooltip = "Declare one internal control-flow label target.";
        Descriptor.Family = ESchemaNodeFamily::ControlFlow;
        Descriptor.RequiresSpecialization = true;
        Descriptor.LoweredKind = EGraphAssetNodeKind::Label;
        Descriptor.Pins.push_back(MakeExecPin("In", ESchemaPinDirection::Input));
        Descriptor.Pins.push_back(MakeExecPin("Out", ESchemaPinDirection::Output));
        m_builtins.push_back(std::move(Descriptor));
    }

    {
        SchemaNodeDescriptor Descriptor{};
        Descriptor.StableId = "builtin.constant";
        Descriptor.DisplayName = "Constant";
        Descriptor.Category = "Data";
        Descriptor.Tooltip = "Emit one authored constant value into a slot.";
        Descriptor.Family = ESchemaNodeFamily::Constant;
        Descriptor.IsPure = true;
        Descriptor.RequiresSpecialization = true;
        Descriptor.LoweredKind = EGraphAssetNodeKind::Constant;
        Descriptor.Pins.push_back(MakeValuePin("Value", ESchemaPinDirection::Output, TypeId{}));
        Descriptor.Pins.back().Type.IsPolymorphic = true;
        m_builtins.push_back(std::move(Descriptor));
    }

    {
        SchemaNodeDescriptor Descriptor{};
        Descriptor.StableId = "builtin.branch";
        Descriptor.DisplayName = "Branch";
        Descriptor.Category = "Flow";
        Descriptor.Tooltip = "Split control flow on a bool condition.";
        Descriptor.Family = ESchemaNodeFamily::ControlFlow;
        Descriptor.LoweredKind = EGraphAssetNodeKind::Branch;
        Descriptor.Pins.push_back(MakeExecPin("In", ESchemaPinDirection::Input));
        Descriptor.Pins.push_back(MakeValuePin("Condition", ESchemaPinDirection::Input, StaticTypeId<bool>(), true));
        Descriptor.Pins.push_back(MakeExecPin("True", ESchemaPinDirection::Output));
        Descriptor.Pins.push_back(MakeExecPin("False", ESchemaPinDirection::Output));
        m_builtins.push_back(std::move(Descriptor));
    }

    {
        SchemaNodeDescriptor Descriptor{};
        Descriptor.StableId = "builtin.jump";
        Descriptor.DisplayName = "Jump";
        Descriptor.Category = "Flow";
        Descriptor.Tooltip = "Unconditionally continue execution at another authored label.";
        Descriptor.Family = ESchemaNodeFamily::ControlFlow;
        Descriptor.LoweredKind = EGraphAssetNodeKind::Jump;
        Descriptor.Pins.push_back(MakeExecPin("In", ESchemaPinDirection::Input));
        Descriptor.Pins.push_back(MakeExecPin("Out", ESchemaPinDirection::Output));
        m_builtins.push_back(std::move(Descriptor));
    }

    const std::array<std::pair<EBinaryIntrinsicOp, std::string_view>, 12> BinaryOps{{
        {EBinaryIntrinsicOp::Add, "Add"},
        {EBinaryIntrinsicOp::Subtract, "Subtract"},
        {EBinaryIntrinsicOp::Multiply, "Multiply"},
        {EBinaryIntrinsicOp::Divide, "Divide"},
        {EBinaryIntrinsicOp::Equal, "Equal"},
        {EBinaryIntrinsicOp::NotEqual, "Not Equal"},
        {EBinaryIntrinsicOp::Less, "Less"},
        {EBinaryIntrinsicOp::LessEqual, "Less Or Equal"},
        {EBinaryIntrinsicOp::Greater, "Greater"},
        {EBinaryIntrinsicOp::GreaterEqual, "Greater Or Equal"},
        {EBinaryIntrinsicOp::LogicalAnd, "Logical And"},
        {EBinaryIntrinsicOp::LogicalOr, "Logical Or"},
    }};

    for (const auto& [Op, Name] : BinaryOps)
    {
        SchemaNodeDescriptor Descriptor{};
        Descriptor.StableId = std::string("intrinsic.binary.") + std::to_string(static_cast<int>(Op));
        Descriptor.DisplayName = std::string(Name);
        Descriptor.Category = "Math/Intrinsic";
        Descriptor.Tooltip = "Evaluate one builtin binary intrinsic.";
        Descriptor.Family = ESchemaNodeFamily::Intrinsic;
        Descriptor.IsPure = true;
        Descriptor.RequiresSpecialization = true;
        Descriptor.BinaryOp = Op;
        Descriptor.LoweredKind = EGraphAssetNodeKind::BinaryIntrinsic;
        Descriptor.Pins.push_back(MakeValuePin("Left", ESchemaPinDirection::Input, TypeId{}, true));
        Descriptor.Pins.back().Type.IsPolymorphic = true;
        Descriptor.Pins.push_back(MakeValuePin("Right", ESchemaPinDirection::Input, TypeId{}, true));
        Descriptor.Pins.back().Type.IsPolymorphic = true;
        Descriptor.Pins.push_back(MakeValuePin("Value", ESchemaPinDirection::Output, TypeId{}));
        Descriptor.Pins.back().Type.IsPolymorphic = true;
        m_builtins.push_back(std::move(Descriptor));
    }

    const std::array<std::pair<EUnaryIntrinsicOp, std::string_view>, 2> UnaryOps{{
        {EUnaryIntrinsicOp::LogicalNot, "Logical Not"},
        {EUnaryIntrinsicOp::Negate, "Negate"},
    }};

    for (const auto& [Op, Name] : UnaryOps)
    {
        SchemaNodeDescriptor Descriptor{};
        Descriptor.StableId = std::string("intrinsic.unary.") + std::to_string(static_cast<int>(Op));
        Descriptor.DisplayName = std::string(Name);
        Descriptor.Category = "Math/Intrinsic";
        Descriptor.Tooltip = "Evaluate one builtin unary intrinsic.";
        Descriptor.Family = ESchemaNodeFamily::Intrinsic;
        Descriptor.IsPure = true;
        Descriptor.RequiresSpecialization = true;
        Descriptor.UnaryOp = Op;
        Descriptor.LoweredKind = EGraphAssetNodeKind::UnaryIntrinsic;
        Descriptor.Pins.push_back(MakeValuePin("Input", ESchemaPinDirection::Input, TypeId{}, true));
        Descriptor.Pins.back().Type.IsPolymorphic = true;
        Descriptor.Pins.push_back(MakeValuePin("Value", ESchemaPinDirection::Output, TypeId{}));
        Descriptor.Pins.back().Type.IsPolymorphic = true;
        m_builtins.push_back(std::move(Descriptor));
    }
}

std::vector<SchemaNodeDescriptor> SchemaRegistry::DescribeSelf(const TypeInfo& SelfType) const
{
    return DescribeMembers(SelfType, true);
}

std::vector<SchemaNodeDescriptor> SchemaRegistry::DescribeVariables(const GraphAsset& Asset) const
{
    return DescribeVariablesInternal(Asset);
}

std::vector<SchemaNodeDescriptor> SchemaRegistry::DescribeInstance(const TypeInfo& OwnerType) const
{
    return DescribeMembers(OwnerType, false);
}

CompileOutput CompilerBridge::Compile(const GraphAsset& Asset) const
{
    CompileOutput Output{};

    auto CompileResult = CompileGraphAsset(Asset);
    if (!CompileResult)
    {
        Output.Diagnostics.push_back(CompileDiagnostic{
            .Severity = ECompileDiagnosticSeverity::Error,
            .Message = CompileResult.error().Message,
        });
        return Output;
    }

    Output.Graph.emplace(std::move(*CompileResult));
    return Output;
}

CompileOutput CompilerBridge::Compile(const GraphDocument& Document) const
{
    return Compile(Document.Asset());
}

std::string_view ConduitEditorService::Name() const
{
    return "ConduitEditorService";
}

std::vector<std::type_index> ConduitEditorService::Dependencies() const
{
    return {std::type_index(typeid(::SnAPI::GameFramework::Editor::EditorAssetService))};
}

Result ConduitEditorService::Initialize(::SnAPI::GameFramework::Editor::EditorServiceContext& Context)
{
    m_assetService = Context.GetService<::SnAPI::GameFramework::Editor::EditorAssetService>();
    m_documents.clear();
    m_classDocuments.clear();
    m_activeDocumentKey.clear();
    m_activeDocumentKind = EWorkspaceDocumentKind::None;
    InvalidateVariableScratch();
    m_workspaceRevision = 0;
    m_schema.RebuildBuiltins();
    return Ok();
}

void ConduitEditorService::Shutdown(::SnAPI::GameFramework::Editor::EditorServiceContext& Context)
{
    (void)Context;
    m_documents.clear();
    m_classDocuments.clear();
    m_activeDocumentKey.clear();
    m_activeDocumentKind = EWorkspaceDocumentKind::None;
    InvalidateVariableScratch();
    m_assetService = nullptr;
}

GraphDocument* ConduitEditorService::ActiveDocument()
{
    if (m_activeDocumentKind != EWorkspaceDocumentKind::Graph || m_activeDocumentKey.empty())
    {
        return nullptr;
    }
    return FindDocument(m_activeDocumentKey);
}

const GraphDocument* ConduitEditorService::ActiveDocument() const
{
    if (m_activeDocumentKind != EWorkspaceDocumentKind::Graph || m_activeDocumentKey.empty())
    {
        return nullptr;
    }
    return FindDocument(m_activeDocumentKey);
}

ClassDocument* ConduitEditorService::ActiveClassDocument()
{
    if (m_activeDocumentKind != EWorkspaceDocumentKind::Class || m_activeDocumentKey.empty())
    {
        return nullptr;
    }
    return FindClassDocument(m_activeDocumentKey);
}

const ClassDocument* ConduitEditorService::ActiveClassDocument() const
{
    if (m_activeDocumentKind != EWorkspaceDocumentKind::Class || m_activeDocumentKey.empty())
    {
        return nullptr;
    }
    return FindClassDocument(m_activeDocumentKey);
}

ConduitEditorService::WorkspaceView ConduitEditorService::ActiveWorkspaceView() const
{
    WorkspaceView View{};
    if (const GraphDocument* Document = ActiveDocument())
    {
        View.Kind = EWorkspaceDocumentKind::Graph;
        View.Open = true;
        View.AssetKey = Document->AssetKey();
        View.Title = Document->Title();
        View.SelfTypeLabel = ResolveTypeLabel(Document->Asset().SelfType);
        View.SlotCount = Document->Asset().Slots.size();
        View.VariableCount = Document->Asset().Variables.size();
        View.NodeCount = Document->Asset().Nodes.size();
        View.IsDirty = Document->IsDirty();
        View.HasCompile = Document->LastCompile().has_value();
        View.CompileSucceeded = Document->LastCompile().has_value() && Document->LastCompile()->Succeeded();
        View.WarningCount = CountDiagnostics(Document->LastCompile(), ECompileDiagnosticSeverity::Warning);
        View.ErrorCount = CountDiagnostics(Document->LastCompile(), ECompileDiagnosticSeverity::Error);
        View.Revision = m_workspaceRevision + Document->WorkspaceRevision();
        View.CanvasRevision = Document->CanvasRevision();
        return View;
    }

    if (const ClassDocument* Document = ActiveClassDocument())
    {
        View.Kind = EWorkspaceDocumentKind::Class;
        View.Open = true;
        View.AssetKey = Document->AssetKey();
        View.Title = Document->Title();
        View.HostTypeLabel = ResolveTypeLabel(Document->Asset().HostType);
        View.GraphAssetLabel = Document->Asset().Graph.ResolvedAssetName();
        View.IsDirty = Document->IsDirty();
        View.Revision = m_workspaceRevision + Document->Revision();
        return View;
    }

    {
        View.Revision = m_workspaceRevision;
        return View;
    }
}

GraphDocument* ConduitEditorService::FindDocument(const std::string_view AssetKey)
{
    const auto It = std::find_if(m_documents.begin(), m_documents.end(), [AssetKey](const GraphDocument& Document) {
        return Document.AssetKey() == AssetKey;
    });
    return It != m_documents.end() ? &(*It) : nullptr;
}

const GraphDocument* ConduitEditorService::FindDocument(const std::string_view AssetKey) const
{
    const auto It = std::find_if(m_documents.begin(), m_documents.end(), [AssetKey](const GraphDocument& Document) {
        return Document.AssetKey() == AssetKey;
    });
    return It != m_documents.end() ? &(*It) : nullptr;
}

ClassDocument* ConduitEditorService::FindClassDocument(const std::string_view AssetKey)
{
    const auto It = std::find_if(m_classDocuments.begin(), m_classDocuments.end(), [AssetKey](const ClassDocument& Document) {
        return Document.AssetKey() == AssetKey;
    });
    return It != m_classDocuments.end() ? &(*It) : nullptr;
}

const ClassDocument* ConduitEditorService::FindClassDocument(const std::string_view AssetKey) const
{
    const auto It = std::find_if(m_classDocuments.begin(), m_classDocuments.end(), [AssetKey](const ClassDocument& Document) {
        return Document.AssetKey() == AssetKey;
    });
    return It != m_classDocuments.end() ? &(*It) : nullptr;
}

TExpected<GraphDocument*> ConduitEditorService::OpenDocument(const std::string_view AssetKey,
                                                             const std::string_view Title,
                                                             const GraphAsset& Asset)
{
    if (m_schema.Builtins().empty())
    {
        m_schema.RebuildBuiltins();
    }

    if (AssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit document asset key is empty"));
    }

    if (GraphDocument* Existing = FindDocument(AssetKey))
    {
        if (m_activeDocumentKey != AssetKey || m_activeDocumentKind != EWorkspaceDocumentKind::Graph)
        {
            m_activeDocumentKey = std::string(AssetKey);
            m_activeDocumentKind = EWorkspaceDocumentKind::Graph;
            InvalidateVariableScratch();
            BumpWorkspaceRevision();
        }
        return Existing;
    }

    GraphAsset WorkingCopy = Asset;
    NormalizeEditorState(WorkingCopy);
    NormalizeMethodNodeInputs(WorkingCopy);

    std::string DocumentTitle = Title.empty() ? WorkingCopy.Name : std::string(Title);
    if (DocumentTitle.empty())
    {
        DocumentTitle = std::string(AssetKey);
    }

    m_documents.emplace_back(std::string(AssetKey), std::move(DocumentTitle), std::move(WorkingCopy));
    m_activeDocumentKey = std::string(AssetKey);
    m_activeDocumentKind = EWorkspaceDocumentKind::Graph;
    InvalidateVariableScratch();
    BumpWorkspaceRevision();
    return &m_documents.back();
}

TExpected<ClassDocument*> ConduitEditorService::OpenClassDocument(const std::string_view AssetKey,
                                                                  const std::string_view Title,
                                                                  const ClassAsset& Asset)
{
    if (AssetKey.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit class document asset key is empty"));
    }

    if (ClassDocument* Existing = FindClassDocument(AssetKey))
    {
        if (m_activeDocumentKey != AssetKey || m_activeDocumentKind != EWorkspaceDocumentKind::Class)
        {
            m_activeDocumentKey = std::string(AssetKey);
            m_activeDocumentKind = EWorkspaceDocumentKind::Class;
            InvalidateVariableScratch();
            BumpWorkspaceRevision();
        }
        return Existing;
    }

    std::string DocumentTitle = Title.empty() ? Asset.Name : std::string(Title);
    if (DocumentTitle.empty())
    {
        DocumentTitle = std::string(AssetKey);
    }

    m_classDocuments.emplace_back(std::string(AssetKey), std::move(DocumentTitle), Asset);
    m_activeDocumentKey = std::string(AssetKey);
    m_activeDocumentKind = EWorkspaceDocumentKind::Class;
    InvalidateVariableScratch();
    BumpWorkspaceRevision();
    return &m_classDocuments.back();
}

bool ConduitEditorService::CloseDocument(const std::string_view AssetKey)
{
    const auto It = std::find_if(m_documents.begin(), m_documents.end(), [AssetKey](const GraphDocument& Document) {
        return Document.AssetKey() == AssetKey;
    });
    if (It == m_documents.end())
    {
        return false;
    }

    m_documents.erase(It);
    if (m_activeDocumentKey == AssetKey && m_activeDocumentKind == EWorkspaceDocumentKind::Graph)
    {
        if (!m_documents.empty())
        {
            m_activeDocumentKey = m_documents.back().AssetKey();
            m_activeDocumentKind = EWorkspaceDocumentKind::Graph;
        }
        else if (!m_classDocuments.empty())
        {
            m_activeDocumentKey = m_classDocuments.back().AssetKey();
            m_activeDocumentKind = EWorkspaceDocumentKind::Class;
        }
        else
        {
            m_activeDocumentKey.clear();
            m_activeDocumentKind = EWorkspaceDocumentKind::None;
        }
        InvalidateVariableScratch();
    }
    BumpWorkspaceRevision();
    return true;
}

bool ConduitEditorService::CloseAnyDocument(const std::string_view AssetKey)
{
    if (CloseDocument(AssetKey))
    {
        return true;
    }

    const auto It = std::find_if(m_classDocuments.begin(), m_classDocuments.end(), [AssetKey](const ClassDocument& Document) {
        return Document.AssetKey() == AssetKey;
    });
    if (It == m_classDocuments.end())
    {
        return false;
    }

    m_classDocuments.erase(It);
    if (m_activeDocumentKey == AssetKey && m_activeDocumentKind == EWorkspaceDocumentKind::Class)
    {
        if (!m_classDocuments.empty())
        {
            m_activeDocumentKey = m_classDocuments.back().AssetKey();
            m_activeDocumentKind = EWorkspaceDocumentKind::Class;
        }
        else if (!m_documents.empty())
        {
            m_activeDocumentKey = m_documents.back().AssetKey();
            m_activeDocumentKind = EWorkspaceDocumentKind::Graph;
        }
        else
        {
            m_activeDocumentKey.clear();
            m_activeDocumentKind = EWorkspaceDocumentKind::None;
        }
        InvalidateVariableScratch();
    }
    BumpWorkspaceRevision();
    return true;
}

bool ConduitEditorService::FocusDocument(const std::string_view AssetKey)
{
    if (!FindDocument(AssetKey))
    {
        return false;
    }
    if (m_activeDocumentKey == AssetKey)
    {
        return true;
    }

    m_activeDocumentKey = std::string(AssetKey);
    m_activeDocumentKind = EWorkspaceDocumentKind::Graph;
    InvalidateVariableScratch();
    BumpWorkspaceRevision();
    return true;
}

bool ConduitEditorService::FocusClassDocument(const std::string_view AssetKey)
{
    if (!FindClassDocument(AssetKey))
    {
        return false;
    }
    if (m_activeDocumentKey == AssetKey && m_activeDocumentKind == EWorkspaceDocumentKind::Class)
    {
        return true;
    }

    m_activeDocumentKey = std::string(AssetKey);
    m_activeDocumentKind = EWorkspaceDocumentKind::Class;
    InvalidateVariableScratch();
    BumpWorkspaceRevision();
    return true;
}

bool ConduitEditorService::IsDocumentDirty(const std::string_view AssetKey) const
{
    if (const GraphDocument* Document = FindDocument(AssetKey))
    {
        return Document->IsDirty();
    }
    if (const ClassDocument* Document = FindClassDocument(AssetKey))
    {
        return Document->IsDirty();
    }
    return false;
}

TExpected<const CompileOutput*> ConduitEditorService::CompileDocument(const std::string_view AssetKey)
{
    GraphDocument* Document = FindDocument(AssetKey);
    if (!Document)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit document was not found"));
    }

    Document->SetLastCompile(m_compiler.Compile(*Document));
    BumpWorkspaceRevision();
    return &Document->LastCompile().value();
}

GraphVariableAsset* ConduitEditorService::SelectedVariable()
{
    GraphDocument* Document = ActiveDocument();
    if (!Document || Document->Selection().VariableIds.empty())
    {
        return nullptr;
    }

    return Document->FindVariable(Document->Selection().VariableIds.front());
}

const GraphVariableAsset* ConduitEditorService::SelectedVariable() const
{
    const GraphDocument* Document = ActiveDocument();
    if (!Document || Document->Selection().VariableIds.empty())
    {
        return nullptr;
    }

    return Document->FindVariable(Document->Selection().VariableIds.front());
}

std::vector<VariableEntryView> ConduitEditorService::ActiveVariableEntries() const
{
    const GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return {};
    }

    const Uuid SelectedId = Document->Selection().VariableIds.empty() ? Uuid{} : Document->Selection().VariableIds.front();
    std::vector<VariableEntryView> Result{};
    Result.reserve(Document->Asset().Variables.size());

    for (const GraphVariableAsset& Variable : Document->Asset().Variables)
    {
        Result.push_back(VariableEntryView{
            .Id = Variable.Id,
            .Name = Variable.Name,
            .TypeLabel = ResolveTypeLabel(Variable.Type),
            .HasDefault = Variable.DefaultValue.Type != TypeId{},
            .Selected = Variable.Id == SelectedId,
        });
    }

    std::sort(Result.begin(), Result.end(), [](const VariableEntryView& Left, const VariableEntryView& Right) {
        return Left.Name < Right.Name;
    });
    return Result;
}

std::vector<PaletteEntryView> ConduitEditorService::ActivePaletteEntries() const
{
    const GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return {};
    }

    const auto Descriptors = BuildActiveSchemaDescriptors(m_schema, Document->Asset());
    std::vector<PaletteEntryView> Result{};
    Result.reserve(Descriptors.size());

    for (const SchemaNodeDescriptor& Descriptor : Descriptors)
    {
        Result.push_back(PaletteEntryView{
            .StableId = Descriptor.StableId,
            .DisplayName = Descriptor.DisplayName,
            .Category = Descriptor.Category,
            .Tooltip = Descriptor.Tooltip,
            .RequiresSpecialization = Descriptor.RequiresSpecialization,
        });
    }

    return Result;
}

std::vector<SpawnMenuEntryView> ConduitEditorService::BuildSpawnMenuEntries(const GraphSpawnMenuRequest& Request) const
{
    const GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return {};
    }

    const auto Descriptors = BuildActiveSchemaDescriptors(m_schema, Document->Asset());
    std::vector<SpawnMenuEntryView> Result{};
    Result.reserve(Descriptors.size());

    if (!Request.FromPinDrag || Request.SourceNodeId == Uuid{} || Request.SourcePin.empty())
    {
        for (const SchemaNodeDescriptor& Descriptor : Descriptors)
        {
            if (!Descriptor.LoweredKind.has_value())
            {
                continue;
            }
            Result.push_back(MakeSpawnMenuEntry(Descriptor));
        }

        SortSpawnMenuEntries(Result);
        return Result;
    }

    const GraphNodeAsset* SourceNode = Document->FindNode(Request.SourceNodeId);
    if (!SourceNode)
    {
        return {};
    }

    const SchemaNodeDescriptor* SourceDescriptor = FindSchemaDescriptorForNode(Descriptors, *SourceNode);
    if (!SourceDescriptor)
    {
        return {};
    }

    auto SourceBindingResult = ResolveNodePinBinding(
        Document->Asset(),
        *SourceNode,
        *SourceDescriptor,
        Request.SourcePin,
        ESchemaPinDirection::Output);
    if (!SourceBindingResult)
    {
        return {};
    }

    for (const SchemaNodeDescriptor& Descriptor : Descriptors)
    {
        if (auto Entry = BuildCompatibleSpawnEntry(*SourceBindingResult, Descriptor); Entry.has_value())
        {
            Result.push_back(std::move(*Entry));
        }
    }

    SortSpawnMenuEntries(Result);
    return Result;
}

std::vector<NodeEntryView> ConduitEditorService::ActiveNodeEntries() const
{
    const GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return {};
    }

    const Uuid SelectedId = Document->Selection().NodeIds.empty() ? Uuid{} : Document->Selection().NodeIds.front();
    std::vector<NodeEntryView> Result{};
    Result.reserve(Document->Asset().Nodes.size());

    for (const GraphNodeAsset& Node : Document->Asset().Nodes)
    {
        Result.push_back(NodeEntryView{
            .Id = Node.Id,
            .Title = DescribeNodeTitle(Document->Asset(), Node),
            .Detail = DescribeNodeDetail(Document->Asset(), Node),
            .Selected = Node.Id == SelectedId,
        });
    }

    return Result;
}

GraphCanvasView ConduitEditorService::ActiveCanvasView() const
{
    const GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return {};
    }

    GraphCanvasView Result{};
    const auto Descriptors = BuildActiveSchemaDescriptors(m_schema, Document->Asset());
    Result.Viewport = Document->Asset().EditorState.Viewport;
    Result.Nodes.reserve(Document->Asset().Nodes.size());
    for (const GraphNodeAsset& Node : Document->Asset().Nodes)
    {
        CanvasNodeView View{};
        View.Id = Node.Id;
        View.Title = DescribeNodeTitle(Document->Asset(), Node);
        View.Detail = DescribeNodeDetail(Document->Asset(), Node);
        View.Selected = std::find(Document->Selection().NodeIds.begin(),
                                  Document->Selection().NodeIds.end(),
                                  Node.Id) != Document->Selection().NodeIds.end();

        if (const GraphNodeEditorAsset* NodeState = Document->FindNodeEditorState(Node.Id))
        {
            View.X = NodeState->X;
            View.Y = NodeState->Y;
            View.Width = NodeState->Width;
            View.IsCollapsed = NodeState->IsCollapsed;
        }

        if (const SchemaNodeDescriptor* Descriptor = FindSchemaDescriptorForNode(Descriptors, Node))
        {
            View.Tooltip = BuildCanvasNodeTooltip(Document->Asset(), Node, Descriptor);
            for (const SchemaPinDescriptor& Pin : Descriptor->Pins)
            {
                CanvasPinView PinView{};
                PinView.Name = Pin.Name;
                PinView.Kind = Pin.Type.Kind;
                PinView.IsInput = Pin.Direction == ESchemaPinDirection::Input;
                PinView.IsExec = Pin.Type.IsExec;
                PinView.TypeLabel = Pin.Type.IsExec ? std::string("Exec") : ResolveTypeDisplayLabel(Pin.Type.Type);
                PinView.Tooltip = BuildFallbackPinTooltip(Pin);

                if (PinView.IsInput)
                {
                    View.InputPins.push_back(std::move(PinView));
                }
                else
                {
                    View.OutputPins.push_back(std::move(PinView));
                }
            }
        }
        else
        {
            View.Tooltip = BuildCanvasNodeTooltip(Document->Asset(), Node, nullptr);
        }

        Result.Nodes.push_back(std::move(View));
    }

    Result.Comments.reserve(Document->Asset().EditorState.Comments.size());
    for (const GraphCommentAsset& Comment : Document->Asset().EditorState.Comments)
    {
        Result.Comments.push_back(CanvasCommentView{
            .Id = Comment.Id,
            .Title = Comment.Title,
            .X = Comment.X,
            .Y = Comment.Y,
            .Width = Comment.Width,
            .Height = Comment.Height,
            .ColorRgba = Comment.ColorRgba,
            .Selected = std::find(Document->Selection().CommentIds.begin(),
                                  Document->Selection().CommentIds.end(),
                                  Comment.Id) != Document->Selection().CommentIds.end(),
        });
    }

    AppendDataCanvasWires(Document->Asset(), Descriptors, Result.Wires);

    for (const GraphNodeAsset& Node : Document->Asset().Nodes)
    {
        const SchemaNodeDescriptor* SourceDescriptor = FindSchemaDescriptorForNode(Descriptors, Node);
        if (!SourceDescriptor)
        {
            continue;
        }

        for (const GraphNodeAsset& CandidateTarget : Document->Asset().Nodes)
        {
            const SchemaNodeDescriptor* TargetDescriptor = FindSchemaDescriptorForNode(Descriptors, CandidateTarget);
            if (!TargetDescriptor)
            {
                continue;
            }

            const std::string TargetExecPin = ResolveExecInputPinName(*TargetDescriptor);
            for (const SchemaPinDescriptor& Pin : SourceDescriptor->Pins)
            {
                if (Pin.Direction != ESchemaPinDirection::Output || !Pin.Type.IsExec)
                {
                    continue;
                }

                if (auto Wire = BuildCanvasWire(Document->Asset(), Node, CandidateTarget, Pin.Name, TargetExecPin);
                    Wire.has_value())
                {
                    Result.Wires.push_back(std::move(*Wire));
                }
            }
        }
    }

    return Result;
}

VariableInspectorView ConduitEditorService::ActiveVariableInspectorView() const
{
    VariableInspectorView View{};
    const GraphVariableAsset* Variable = SelectedVariable();
    if (!Variable)
    {
        return View;
    }

    View.HasSelection = true;
    View.VariableId = Variable->Id;
    View.Name = Variable->Name;
    View.Type = Variable->Type;
    View.TypeLabel = ResolveTypeLabel(Variable->Type);
    View.HasDefault = Variable->DefaultValue.Type != TypeId{};

    const TypeInfo* Type = TypeRegistry::Instance().Find(Variable->Type);
    if (!Type)
    {
        return View;
    }

    if (Variable->Type == StaticTypeId<bool>())
    {
        View.DefaultEditorKind = EVariableDefaultEditorKind::Bool;
        if (View.HasDefault)
        {
            (void)DeserializeSerializedValue(Variable->DefaultValue, View.BoolValue);
        }
        return View;
    }

    if (Type->IsEnum)
    {
        View.DefaultEditorKind = EVariableDefaultEditorKind::Enum;
        View.EnumOptions.reserve(Type->EnumValues.size());
        std::uint64_t SelectedBits = 0;
        const bool HasEnumValue = View.HasDefault && TryReadEnumValueBits(*Type, Variable->DefaultValue, SelectedBits);

        for (std::size_t Index = 0; Index < Type->EnumValues.size(); ++Index)
        {
            const EnumValueInfo& Entry = Type->EnumValues[Index];
            View.EnumOptions.push_back(Entry.Name);
            if (HasEnumValue && Entry.Value == SelectedBits)
            {
                View.SelectedEnumIndex = static_cast<int32_t>(Index);
            }
        }
        return View;
    }

    if (SupportsTextVariableEditor(Variable->Type))
    {
        View.DefaultEditorKind = EVariableDefaultEditorKind::Text;
        (void)TryFormatTextDefault(*Variable, View.TextValue);
        return View;
    }

    if (const auto ScratchResult = EnsureVariableScratch(*Variable); ScratchResult)
    {
        View.DefaultEditorKind = EVariableDefaultEditorKind::Complex;
        View.ComplexObject = m_variableScratch.Storage.get();
        View.ComplexType = m_variableScratch.Type;
    }

    return View;
}

NodeInspectorView ConduitEditorService::ActiveNodeInspectorView() const
{
    NodeInspectorView View{};
    const GraphDocument* Document = ActiveDocument();
    if (!Document || Document->Selection().NodeIds.empty())
    {
        return View;
    }

    const GraphNodeAsset* Node = Document->FindNode(Document->Selection().NodeIds.front());
    if (!Node)
    {
        return View;
    }

    View.HasSelection = true;
    View.NodeId = Node->Id;
    View.Kind = Node->Kind;
    View.Title = DescribeNodeTitle(Document->Asset(), *Node);
    View.Detail = DescribeNodeDetail(Document->Asset(), *Node);

    const auto Descriptors = BuildActiveSchemaDescriptors(m_schema, Document->Asset());
    if (const SchemaNodeDescriptor* Descriptor = FindSchemaDescriptorForNode(Descriptors, *Node))
    {
        for (const SchemaPinDescriptor& Pin : Descriptor->Pins)
        {
            if (Pin.Direction != ESchemaPinDirection::Input || Pin.Type.IsExec || !Pin.SupportsLiteral)
            {
                continue;
            }

            auto BindingResult = ResolveNodePinBinding(Document->Asset(), *Node, *Descriptor, Pin.Name, ESchemaPinDirection::Input);
            if (!BindingResult)
            {
                continue;
            }

            const std::string PinKey = CanonicalizeNodeInputPinKey(*Node, Pin.Name);
            const TypeId EffectiveType = BindingResult->StorageType != TypeId{} ? BindingResult->StorageType : BindingResult->DisplayType;
            if (EffectiveType == TypeId{})
            {
                continue;
            }

            NodeInputDefaultInspectorEntry Entry{};
            Entry.PinKey = PinKey;
            Entry.DisplayName = Pin.Name;
            Entry.Type = EffectiveType;
            Entry.TypeLabel = ResolveTypeDisplayLabel(EffectiveType);
            Entry.Tooltip = Pin.Tooltip;
            Entry.Connected = BindingResult->Slot && BindingResult->Slot->IsValid();

            if (const GraphNodeInputDefaultAsset* Default = FindNodeInputDefault(*Node, PinKey);
                Default && Default->Value.Type == EffectiveType)
            {
                Entry.HasDefault = true;
                if (EffectiveType == StaticTypeId<bool>())
                {
                    Entry.DefaultEditorKind = EVariableDefaultEditorKind::Bool;
                    (void)DeserializeSerializedValue(Default->Value, Entry.BoolValue);
                }
                else if (const TypeInfo* Type = TypeRegistry::Instance().Find(EffectiveType); Type && Type->IsEnum)
                {
                    Entry.DefaultEditorKind = EVariableDefaultEditorKind::Enum;
                    Entry.EnumOptions.reserve(Type->EnumValues.size());
                    std::uint64_t SelectedBits = 0;
                    const bool HasEnumValue = TryReadEnumValueBits(*Type, Default->Value, SelectedBits);
                    for (std::size_t Index = 0; Index < Type->EnumValues.size(); ++Index)
                    {
                        const EnumValueInfo& EnumValue = Type->EnumValues[Index];
                        Entry.EnumOptions.push_back(EnumValue.Name);
                        if (HasEnumValue && EnumValue.Value == SelectedBits)
                        {
                            Entry.SelectedEnumIndex = static_cast<int32_t>(Index);
                        }
                    }
                }
                else if (SupportsTextVariableEditor(EffectiveType))
                {
                    Entry.DefaultEditorKind = EVariableDefaultEditorKind::Text;
                    (void)TryFormatTextSerializedValue(EffectiveType, Default->Value, Entry.TextValue);
                }
            }
            else if (EffectiveType == StaticTypeId<bool>())
            {
                Entry.DefaultEditorKind = EVariableDefaultEditorKind::Bool;
            }
            else if (const TypeInfo* Type = TypeRegistry::Instance().Find(EffectiveType); Type && Type->IsEnum)
            {
                Entry.DefaultEditorKind = EVariableDefaultEditorKind::Enum;
                Entry.EnumOptions.reserve(Type->EnumValues.size());
                for (const EnumValueInfo& EnumValue : Type->EnumValues)
                {
                    Entry.EnumOptions.push_back(EnumValue.Name);
                }
            }
            else if (SupportsTextVariableEditor(EffectiveType))
            {
                Entry.DefaultEditorKind = EVariableDefaultEditorKind::Text;
            }

            View.InputDefaults.push_back(std::move(Entry));
        }
    }

    switch (Node->Kind)
    {
    case EGraphAssetNodeKind::EntryPoint:
        if (Node->BuiltinEntryPoint == EBuiltinEntryPoint::None)
        {
            View.CanEditPrimaryText = true;
            View.PrimaryTextLabel = "Entry Name";
            View.PrimaryTextValue = Node->EntryPointName;
        }
        break;
    case EGraphAssetNodeKind::Label:
        View.CanEditPrimaryText = true;
        View.PrimaryTextLabel = "Label Name";
        View.PrimaryTextValue = Node->LabelName;
        break;
    case EGraphAssetNodeKind::Jump:
        View.CanEditPrimaryText = true;
        View.PrimaryTextLabel = "Target Label";
        View.PrimaryTextValue = Node->LabelName;
        break;
    case EGraphAssetNodeKind::Branch:
        View.CanEditPrimaryText = true;
        View.PrimaryTextLabel = "True Label";
        View.PrimaryTextValue = Node->LabelName;
        View.CanEditSecondaryText = true;
        View.SecondaryTextLabel = "False Label";
        View.SecondaryTextValue = Node->FalseLabelName;
        break;
    default:
        break;
    }

    return View;
}

ClassInspectorView ConduitEditorService::ActiveClassInspectorView() const
{
    ClassInspectorView View{};
    const ClassDocument* Document = ActiveClassDocument();
    if (!Document)
    {
        return View;
    }

    View.HasSelection = true;
    View.Name = Document->Asset().Name;
    View.HostType = Document->Asset().HostType;
    View.HostTypeLabel = ResolveTypeLabel(Document->Asset().HostType);
    View.GraphAssetKey = Document->Asset().Graph.GetAssetName();
    View.GraphAssetLabel = Document->Asset().Graph.ResolvedAssetName();
    return View;
}

std::vector<VariableTypeOption> ConduitEditorService::AvailableVariableTypes() const
{
    std::vector<VariableTypeOption> Result{};
    const auto Types = TypeRegistry::Instance().All();
    Result.reserve(Types.size());

    for (const TypeInfo* Type : Types)
    {
        if (!Type || !SupportsConduitVariableType(*Type))
        {
            continue;
        }

        Result.push_back(VariableTypeOption{
            .Type = Type->Id,
            .Label = ResolveTypeLabel(Type->Id),
        });
    }

    std::sort(Result.begin(), Result.end(), [](const VariableTypeOption& Left, const VariableTypeOption& Right) {
        return Left.Label < Right.Label;
    });
    return Result;
}

std::vector<GraphSelfTypeOption> ConduitEditorService::AvailableGraphSelfTypes() const
{
    std::vector<GraphSelfTypeOption> Result{};
    Result.push_back(GraphSelfTypeOption{
        .Type = {},
        .Label = "None",
    });

    if (const TypeInfo* BaseNodeType = TypeRegistry::Instance().Find(StaticTypeId<BaseNode>()))
    {
        Result.push_back(GraphSelfTypeOption{
            .Type = BaseNodeType->Id,
            .Label = ResolveTypeLabel(BaseNodeType->Id),
        });
    }

    const auto Types = TypeRegistry::Instance().Derived(StaticTypeId<BaseNode>());
    for (const TypeInfo* Type : Types)
    {
        if (!Type || Type->IsAbstract || Type->IsInterface)
        {
            continue;
        }

        Result.push_back(GraphSelfTypeOption{
            .Type = Type->Id,
            .Label = ResolveTypeLabel(Type->Id),
        });
    }

    std::sort(Result.begin() + 1, Result.end(), [](const GraphSelfTypeOption& Left, const GraphSelfTypeOption& Right) {
        if (Left.Label != Right.Label)
        {
            return Left.Label < Right.Label;
        }
        return Left.Type < Right.Type;
    });
    Result.erase(std::unique(Result.begin() + 1,
                             Result.end(),
                             [](const GraphSelfTypeOption& Left, const GraphSelfTypeOption& Right) {
                                 return Left.Type == Right.Type;
                             }),
                 Result.end());
    return Result;
}

std::vector<ClassHostTypeOption> ConduitEditorService::AvailableClassHostTypes() const
{
    std::vector<ClassHostTypeOption> Result{};
    if (const TypeInfo* BaseNodeType = TypeRegistry::Instance().Find(StaticTypeId<BaseNode>()))
    {
        if (!BaseNodeType->IsAbstract && !BaseNodeType->IsInterface && BaseNodeType->Id != TypeId{})
        {
            Result.push_back(ClassHostTypeOption{
                .Type = BaseNodeType->Id,
                .Label = ResolveTypeLabel(BaseNodeType->Id),
            });
        }
    }

    const auto Types = TypeRegistry::Instance().Derived(StaticTypeId<BaseNode>());
    Result.reserve(Result.size() + Types.size());

    for (const TypeInfo* Type : Types)
    {
        if (!Type || Type->Id == TypeId{} || Type->IsAbstract || Type->IsInterface)
        {
            continue;
        }

        Result.push_back(ClassHostTypeOption{
            .Type = Type->Id,
            .Label = ResolveTypeLabel(Type->Id),
        });
    }

    std::sort(Result.begin(), Result.end(), [](const ClassHostTypeOption& Left, const ClassHostTypeOption& Right) {
        if (Left.Label != Right.Label)
        {
            return Left.Label < Right.Label;
        }
        return Left.Type < Right.Type;
    });
    Result.erase(std::unique(Result.begin(),
                             Result.end(),
                             [](const ClassHostTypeOption& Left, const ClassHostTypeOption& Right) {
                                 return Left.Type == Right.Type;
                             }),
                 Result.end());
    return Result;
}

std::vector<ClassGraphOption> ConduitEditorService::AvailableClassGraphAssets() const
{
    std::vector<ClassGraphOption> Result{};
    if (!m_assetService)
    {
        return Result;
    }

    const auto& Assets = m_assetService->Assets();
    Result.reserve(Assets.size());
    for (const auto& Asset : Assets)
    {
        if (Asset.AssetType != StaticTypeId<GraphAsset>())
        {
            continue;
        }

        Result.push_back(ClassGraphOption{
            .AssetKey = Asset.Key,
            .Label = Asset.Key,
        });
    }

    std::sort(Result.begin(), Result.end(), [](const ClassGraphOption& Left, const ClassGraphOption& Right) {
        return Left.Label < Right.Label;
    });
    return Result;
}

bool ConduitEditorService::SelectVariable(const Uuid& VariableId)
{
    GraphDocument* Document = ActiveDocument();
    if (!Document || !Document->FindVariable(VariableId))
    {
        return false;
    }

    if (Document->Selection().VariableIds.size() == 1 && Document->Selection().VariableIds.front() == VariableId)
    {
        return true;
    }

    Document->Selection().NodeIds.clear();
    Document->Selection().CommentIds.clear();
    Document->Selection().VariableIds = {VariableId};
    InvalidateVariableScratch();
    BumpWorkspaceRevision();
    return true;
}

bool ConduitEditorService::SelectNode(const Uuid& NodeId)
{
    GraphDocument* Document = ActiveDocument();
    if (!Document || !Document->FindNode(NodeId))
    {
        return false;
    }

    if (Document->Selection().NodeIds.size() == 1 && Document->Selection().NodeIds.front() == NodeId)
    {
        return true;
    }

    Document->Selection().NodeIds = {NodeId};
    Document->Selection().CommentIds.clear();
    Document->Selection().VariableIds.clear();
    InvalidateVariableScratch();
    BumpWorkspaceRevision();
    return true;
}

TExpected<GraphVariableAsset*> ConduitEditorService::CreateVariable(const std::string_view Name, const TypeId& Type)
{
    GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active Conduit document is open"));
    }

    auto CreateResult = Document->AddVariable(Name, Type);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    Document->Selection().NodeIds.clear();
    Document->Selection().CommentIds.clear();
    Document->Selection().VariableIds = {(*CreateResult)->Id};
    InvalidateVariableScratch();
    BumpWorkspaceRevision();
    return *CreateResult;
}

TExpected<GraphNodeAsset*> ConduitEditorService::SpawnNode(const std::string_view StableId)
{
    GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active Conduit document is open"));
    }
    if (StableId.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit schema node id is empty"));
    }

    const auto Descriptors = BuildActiveSchemaDescriptors(m_schema, Document->Asset());
    const auto DescriptorIt = std::find_if(Descriptors.begin(), Descriptors.end(), [StableId](const SchemaNodeDescriptor& Descriptor) {
        return Descriptor.StableId == StableId;
    });
    if (DescriptorIt == Descriptors.end())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit schema node was not found in the active palette"));
    }

    auto AddResult = Document->AddNode(*DescriptorIt);
    if (!AddResult)
    {
        return std::unexpected(AddResult.error());
    }

    Document->Selection().NodeIds = {(*AddResult)->Id};
    Document->Selection().CommentIds.clear();
    Document->Selection().VariableIds.clear();
    InvalidateVariableScratch();
    BumpWorkspaceRevision();
    return *AddResult;
}

TExpected<GraphNodeAsset*> ConduitEditorService::SpawnNode(const std::string_view StableId, const float X, const float Y)
{
    auto SpawnResult = SpawnNode(StableId);
    if (!SpawnResult)
    {
        return std::unexpected(SpawnResult.error());
    }

    if (const Result MoveResult = MoveNode((*SpawnResult)->Id, X, Y); !MoveResult)
    {
        return std::unexpected(MoveResult.error());
    }

    return *SpawnResult;
}

bool ConduitEditorService::RemoveSelectedVariable()
{
    GraphDocument* Document = ActiveDocument();
    GraphVariableAsset* Variable = SelectedVariable();
    if (!Document || !Variable)
    {
        return false;
    }

    const bool Removed = Document->RemoveVariable(Variable->Id);
    if (Removed)
    {
        InvalidateVariableScratch();
        BumpWorkspaceRevision();
    }
    return Removed;
}

bool ConduitEditorService::RemoveSelectedNode()
{
    GraphDocument* Document = ActiveDocument();
    if (!Document || Document->Selection().NodeIds.empty())
    {
        return false;
    }

    const Uuid NodeId = Document->Selection().NodeIds.front();
    const bool Removed = Document->RemoveNode(NodeId);
    if (Removed)
    {
        InvalidateVariableScratch();
        BumpWorkspaceRevision();
    }
    return Removed;
}

Result ConduitEditorService::MoveNode(const Uuid& NodeId, const float X, const float Y)
{
    GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active Conduit document is open"));
    }

    return Document->SetNodePosition(NodeId, X, Y);
}

Result ConduitEditorService::ConnectPins(const Uuid& SourceNodeId,
                                         const std::string_view SourcePin,
                                         const Uuid& TargetNodeId,
                                         const std::string_view TargetPin)
{
    GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active Conduit document is open"));
    }

    const std::uint64_t RevisionBefore = Document->Revision();
    const Result ConnectResult = Document->ConnectPins(SourceNodeId, SourcePin, TargetNodeId, TargetPin);
    if (!ConnectResult)
    {
        return ConnectResult;
    }

    if (Document->Revision() != RevisionBefore)
    {
        BumpWorkspaceRevision();
    }
    return Ok();
}

Result ConduitEditorService::SetViewport(const float PanX, const float PanY, const float Zoom)
{
    GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active Conduit document is open"));
    }

    return Document->SetViewport(PanX, PanY, Zoom);
}

Result ConduitEditorService::SetActiveGraphSelfType(const TypeId& Type)
{
    GraphDocument* Document = ActiveDocument();
    if (!Document)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active Conduit document is open"));
    }
    if (Type != TypeId{})
    {
        const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
        if (!Info)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Selected Conduit self type is not registered"));
        }
        if (Info->IsAbstract || Info->IsInterface || !TypeRegistry::Instance().IsA(Type, StaticTypeId<BaseNode>()))
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit self type is not a concrete node type"));
        }
    }

    const Result SetResult = Document->SetSelfType(Type);
    if (SetResult)
    {
        InvalidateVariableScratch();
        BumpWorkspaceRevision();
    }
    return SetResult;
}

Result ConduitEditorService::RenameSelectedVariable(const std::string_view Name)
{
    GraphDocument* Document = ActiveDocument();
    GraphVariableAsset* Variable = SelectedVariable();
    if (!Document || !Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph variable is selected"));
    }

    const auto RenameResult = Document->RenameVariable(Variable->Id, Name);
    if (RenameResult)
    {
        BumpWorkspaceRevision();
    }
    return RenameResult;
}

Result ConduitEditorService::SetSelectedVariableType(const TypeId& Type)
{
    GraphDocument* Document = ActiveDocument();
    GraphVariableAsset* Variable = SelectedVariable();
    if (!Document || !Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph variable is selected"));
    }

    const auto SetTypeResult = Document->SetVariableType(Variable->Id, Type);
    if (SetTypeResult)
    {
        InvalidateVariableScratch();
        BumpWorkspaceRevision();
    }
    return SetTypeResult;
}

Result ConduitEditorService::SetSelectedVariableDefaultBool(const bool Value)
{
    GraphDocument* Document = ActiveDocument();
    GraphVariableAsset* Variable = SelectedVariable();
    if (!Document || !Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph variable is selected"));
    }
    if (Variable->Type != StaticTypeId<bool>())
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit graph variable is not a bool"));
    }

    auto SerializedResult = SerializedValue::FromValue(Value);
    if (!SerializedResult)
    {
        return std::unexpected(SerializedResult.error());
    }

    const auto SetDefaultResult = Document->SetVariableDefault(Variable->Id, *SerializedResult);
    if (SetDefaultResult)
    {
        InvalidateVariableScratch();
        BumpWorkspaceRevision();
    }
    return SetDefaultResult;
}

Result ConduitEditorService::SetSelectedVariableDefaultText(const std::string_view Value)
{
    GraphDocument* Document = ActiveDocument();
    GraphVariableAsset* Variable = SelectedVariable();
    if (!Document || !Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph variable is selected"));
    }

    auto SerializedResult = TryParseTextDefault(*Variable, Value);
    if (!SerializedResult)
    {
        return std::unexpected(SerializedResult.error());
    }

    const auto SetDefaultResult = Document->SetVariableDefault(Variable->Id, *SerializedResult);
    if (SetDefaultResult)
    {
        InvalidateVariableScratch();
        BumpWorkspaceRevision();
    }
    return SetDefaultResult;
}

Result ConduitEditorService::SetSelectedVariableDefaultEnum(const std::string_view EnumName)
{
    GraphDocument* Document = ActiveDocument();
    GraphVariableAsset* Variable = SelectedVariable();
    if (!Document || !Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph variable is selected"));
    }

    const TypeInfo* Type = TypeRegistry::Instance().Find(Variable->Type);
    if (!Type || !Type->IsEnum)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit graph variable is not an enum"));
    }

    const auto It = std::find_if(Type->EnumValues.begin(), Type->EnumValues.end(), [EnumName](const EnumValueInfo& Entry) {
        return Entry.Name == EnumName;
    });
    if (It == Type->EnumValues.end())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit enum default value was not found"));
    }

    auto SerializedResult = MakeSerializedEnumValue(*Type, *It);
    if (!SerializedResult)
    {
        return std::unexpected(SerializedResult.error());
    }

    const auto SetDefaultResult = Document->SetVariableDefault(Variable->Id, *SerializedResult);
    if (SetDefaultResult)
    {
        InvalidateVariableScratch();
        BumpWorkspaceRevision();
    }
    return SetDefaultResult;
}

Result ConduitEditorService::ClearSelectedVariableDefault()
{
    GraphDocument* Document = ActiveDocument();
    GraphVariableAsset* Variable = SelectedVariable();
    if (!Document || !Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph variable is selected"));
    }

    const auto ClearResult = Document->ClearVariableDefault(Variable->Id);
    if (ClearResult)
    {
        InvalidateVariableScratch();
        BumpWorkspaceRevision();
    }
    return ClearResult;
}

Result ConduitEditorService::CommitSelectedVariableComplexDefault()
{
    GraphDocument* Document = ActiveDocument();
    const GraphVariableAsset* Variable = SelectedVariable();
    if (!Document || !Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph variable is selected"));
    }

    auto ScratchResult = EnsureVariableScratch(*Variable);
    if (!ScratchResult)
    {
        return std::unexpected(ScratchResult.error());
    }

    SerializedValue Value{};
    Value.Type = Variable->Type;
    const auto SerializeResult = SerializeReflectedValue(Variable->Type, m_variableScratch.Storage.get(), Value.Bytes);
    if (!SerializeResult)
    {
        return std::unexpected(SerializeResult.error());
    }

    const auto SetDefaultResult = Document->SetVariableDefault(Variable->Id, Value);
    if (SetDefaultResult)
    {
        BumpWorkspaceRevision();
    }
    return SetDefaultResult;
}

Result ConduitEditorService::ResetSelectedVariableDefaultEditor()
{
    const GraphVariableAsset* Variable = SelectedVariable();
    if (!Variable)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph variable is selected"));
    }

    InvalidateVariableScratch();
    auto ScratchResult = EnsureVariableScratch(*Variable);
    if (!ScratchResult)
    {
        return std::unexpected(ScratchResult.error());
    }

    BumpWorkspaceRevision();
    return Ok();
}

Result ConduitEditorService::SetSelectedNodeInputDefaultBool(const std::string_view PinKey, const bool Value)
{
    GraphDocument* Document = ActiveDocument();
    const NodeInspectorView Inspector = ActiveNodeInspectorView();
    if (!Document || !Inspector.HasSelection)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph node is selected"));
    }

    const auto It = std::find_if(Inspector.InputDefaults.begin(),
                                 Inspector.InputDefaults.end(),
                                 [PinKey](const NodeInputDefaultInspectorEntry& Entry) {
                                     return Entry.PinKey == PinKey;
                                 });
    if (It == Inspector.InputDefaults.end())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Selected Conduit node input default was not found"));
    }
    if (It->Type != StaticTypeId<bool>())
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit node input is not a bool"));
    }

    auto SerializedResult = SerializedValue::FromValue(Value);
    if (!SerializedResult)
    {
        return std::unexpected(SerializedResult.error());
    }

    const Result SetResult = Document->SetNodeInputDefault(Inspector.NodeId, PinKey, *SerializedResult);
    if (SetResult)
    {
        BumpWorkspaceRevision();
    }
    return SetResult;
}

Result ConduitEditorService::SetSelectedNodeInputDefaultText(const std::string_view PinKey, const std::string_view Value)
{
    GraphDocument* Document = ActiveDocument();
    const NodeInspectorView Inspector = ActiveNodeInspectorView();
    if (!Document || !Inspector.HasSelection)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph node is selected"));
    }

    const auto It = std::find_if(Inspector.InputDefaults.begin(),
                                 Inspector.InputDefaults.end(),
                                 [PinKey](const NodeInputDefaultInspectorEntry& Entry) {
                                     return Entry.PinKey == PinKey;
                                 });
    if (It == Inspector.InputDefaults.end())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Selected Conduit node input default was not found"));
    }

    auto SerializedResult = TryParseTextSerializedValue(It->Type, Value);
    if (!SerializedResult)
    {
        return std::unexpected(SerializedResult.error());
    }

    const Result SetResult = Document->SetNodeInputDefault(Inspector.NodeId, PinKey, *SerializedResult);
    if (SetResult)
    {
        BumpWorkspaceRevision();
    }
    return SetResult;
}

Result ConduitEditorService::SetSelectedNodeInputDefaultEnum(const std::string_view PinKey, const std::string_view EnumName)
{
    GraphDocument* Document = ActiveDocument();
    const NodeInspectorView Inspector = ActiveNodeInspectorView();
    if (!Document || !Inspector.HasSelection)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph node is selected"));
    }

    const auto It = std::find_if(Inspector.InputDefaults.begin(),
                                 Inspector.InputDefaults.end(),
                                 [PinKey](const NodeInputDefaultInspectorEntry& Entry) {
                                     return Entry.PinKey == PinKey;
                                 });
    if (It == Inspector.InputDefaults.end())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Selected Conduit node input default was not found"));
    }

    const TypeInfo* Type = TypeRegistry::Instance().Find(It->Type);
    if (!Type || !Type->IsEnum)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit node input is not an enum"));
    }

    const auto EnumIt = std::find_if(Type->EnumValues.begin(), Type->EnumValues.end(), [EnumName](const EnumValueInfo& Entry) {
        return Entry.Name == EnumName;
    });
    if (EnumIt == Type->EnumValues.end())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit enum default value was not found"));
    }

    auto SerializedResult = MakeSerializedEnumValue(*Type, *EnumIt);
    if (!SerializedResult)
    {
        return std::unexpected(SerializedResult.error());
    }

    const Result SetResult = Document->SetNodeInputDefault(Inspector.NodeId, PinKey, *SerializedResult);
    if (SetResult)
    {
        BumpWorkspaceRevision();
    }
    return SetResult;
}

Result ConduitEditorService::ClearSelectedNodeInputDefault(const std::string_view PinKey)
{
    GraphDocument* Document = ActiveDocument();
    const NodeInspectorView Inspector = ActiveNodeInspectorView();
    if (!Document || !Inspector.HasSelection)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph node is selected"));
    }

    const Result ClearResult = Document->ClearNodeInputDefault(Inspector.NodeId, PinKey);
    if (ClearResult)
    {
        BumpWorkspaceRevision();
    }
    return ClearResult;
}

Result ConduitEditorService::SetSelectedNodePrimaryText(const std::string_view Value)
{
    GraphDocument* Document = ActiveDocument();
    if (!Document || Document->Selection().NodeIds.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph node is selected"));
    }

    GraphNodeAsset* Node = Document->FindNode(Document->Selection().NodeIds.front());
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Selected Conduit graph node was not found"));
    }

    Result SetResult = Ok();
    switch (Node->Kind)
    {
    case EGraphAssetNodeKind::EntryPoint:
        SetResult = Document->SetNodeEntryPointName(Node->Id, Value);
        break;
    case EGraphAssetNodeKind::Label:
    case EGraphAssetNodeKind::Jump:
    case EGraphAssetNodeKind::Branch:
        SetResult = Document->SetNodeLabelName(Node->Id, Value);
        break;
    default:
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit node has no editable primary text"));
    }

    if (SetResult)
    {
        BumpWorkspaceRevision();
    }
    return SetResult;
}

Result ConduitEditorService::SetSelectedNodeSecondaryText(const std::string_view Value)
{
    GraphDocument* Document = ActiveDocument();
    if (!Document || Document->Selection().NodeIds.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No Conduit graph node is selected"));
    }

    GraphNodeAsset* Node = Document->FindNode(Document->Selection().NodeIds.front());
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Selected Conduit graph node was not found"));
    }
    if (Node->Kind != EGraphAssetNodeKind::Branch)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit node has no editable secondary text"));
    }

    const Result SetResult = Document->SetNodeFalseLabelName(Node->Id, Value);
    if (SetResult)
    {
        BumpWorkspaceRevision();
    }
    return SetResult;
}

Result ConduitEditorService::RenameActiveClass(const std::string_view Name)
{
    ClassDocument* Document = ActiveClassDocument();
    if (!Document)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active Conduit class document is open"));
    }

    const Result RenameResult = Document->SetName(Name);
    if (RenameResult)
    {
        if (!Name.empty())
        {
            Document->SetTitle(Name);
        }
        BumpWorkspaceRevision();
    }
    return RenameResult;
}

Result ConduitEditorService::SetActiveClassHostType(const TypeId& Type)
{
    ClassDocument* Document = ActiveClassDocument();
    if (!Document)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active Conduit class document is open"));
    }

    const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Selected Conduit host type is not registered"));
    }
    if (Info->IsAbstract || Info->IsInterface || !TypeRegistry::Instance().IsA(Type, StaticTypeId<BaseNode>()))
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Selected Conduit host type is not a concrete node type"));
    }

    const Result SetResult = Document->SetHostType(Type);
    if (SetResult)
    {
        BumpWorkspaceRevision();
    }
    return SetResult;
}

Result ConduitEditorService::SetActiveClassGraph(const std::string_view AssetKey)
{
    ClassDocument* Document = ActiveClassDocument();
    if (!Document)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "No active Conduit class document is open"));
    }

    if (!AssetKey.empty() && m_assetService)
    {
        const auto Options = AvailableClassGraphAssets();
        const bool Exists = std::any_of(Options.begin(), Options.end(), [AssetKey](const ClassGraphOption& Option) {
            return Option.AssetKey == AssetKey;
        });
        if (!Exists)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Selected Conduit graph asset is not available"));
        }
    }

    const Result SetResult = Document->SetGraphAsset(AssetKey);
    if (SetResult)
    {
        BumpWorkspaceRevision();
    }
    return SetResult;
}

void ConduitEditorService::InvalidateVariableScratch()
{
    m_variableScratch = {};
}

TExpected<void> ConduitEditorService::EnsureVariableScratch(const GraphVariableAsset& Variable) const
{
    if (m_variableScratch.VariableId == Variable.Id &&
        m_variableScratch.Type == Variable.Type &&
        m_variableScratch.Storage)
    {
        return Ok();
    }

    const TypeInfo* Type = TypeRegistry::Instance().Find(Variable.Type);
    if (!Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit variable scratch type is not registered"));
    }
    if (!Type->RuntimeOps)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit variable scratch type has no runtime ops"));
    }

    const std::shared_ptr<void> Storage = AllocateRuntimeStorage(*Type);
    if (!Storage)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit variable scratch storage could not be allocated"));
    }

    if (Variable.DefaultValue.Type != TypeId{})
    {
        const auto ConstructResult = Variable.DefaultValue.ConstructInto(Storage.get());
        if (!ConstructResult)
        {
            return std::unexpected(ConstructResult.error());
        }
    }
    else if (Type->RuntimeOps->DefaultConstruct)
    {
        Type->RuntimeOps->DefaultConstruct(Storage.get());
    }
    else
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Selected Conduit graph variable type is not default-constructible"));
    }

    m_variableScratch.VariableId = Variable.Id;
    m_variableScratch.Type = Variable.Type;
    m_variableScratch.Storage = Storage;
    return Ok();
}

} // namespace SnAPI::GameFramework::Conduit::Editor
