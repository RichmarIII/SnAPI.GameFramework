#include "Conduit/Editor.h"

#include "Editor/EditorAssetService.h"
#include "Editor/EditorCoreServices.h"
#include "BaseNode.h"
#include "Uuid.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstring>
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
    Pin.Type.Kind = ESlotKind::Value;
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
}

[[nodiscard]] std::string ResolveTypeLabel(const TypeId& Type)
{
    if (Type == TypeId{})
    {
        return "Any";
    }

    if (const TypeInfo* Info = TypeRegistry::Instance().Find(Type))
    {
        const std::string_view QualifiedName = Info->Name;
        const std::size_t CppScope = QualifiedName.rfind("::");
        const std::size_t DotScope = QualifiedName.rfind('.');
        std::size_t Start = 0;
        if (CppScope != std::string_view::npos)
        {
            Start = CppScope + 2;
        }
        if (DotScope != std::string_view::npos)
        {
            Start = std::max(Start, DotScope + 1);
        }
        return std::string(QualifiedName.substr(Start));
    }

    return ToString(Type);
}

[[nodiscard]] std::string BuildArgName(const std::size_t Index)
{
    return "Arg" + std::to_string(Index);
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
            return "Get " + Node.MemberName;
        case EGraphAssetNodeKind::SelfFieldWrite:
            return "Set " + Node.MemberName;
        case EGraphAssetNodeKind::SelfMethodCall:
            return Node.MemberName;
        case EGraphAssetNodeKind::InstanceFieldRead:
            return "Get " + Node.MemberName;
        case EGraphAssetNodeKind::InstanceFieldWrite:
            return "Set " + Node.MemberName;
        case EGraphAssetNodeKind::InstanceMethodCall:
            return Node.MemberName;
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
            return Node.LabelName.empty() ? std::string("Target label pending") : "To " + Node.LabelName;
        case EGraphAssetNodeKind::Branch:
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
            return "Self Method";
        case EGraphAssetNodeKind::InstanceFieldRead:
        case EGraphAssetNodeKind::InstanceFieldWrite:
            return ResolveTypeLabel(Node.OwnerType);
        case EGraphAssetNodeKind::InstanceMethodCall:
            return ResolveTypeLabel(Node.OwnerType);
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

[[nodiscard]] std::optional<CanvasWireView> BuildCanvasWire(const GraphAsset& Asset,
                                                            const GraphNodeAsset& SourceNode,
                                                            const GraphNodeAsset& TargetNode,
                                                            const std::string_view SourcePin)
{
    const auto HasLabelTarget = [&TargetNode](const std::string_view LabelName) {
        return !LabelName.empty() && TargetNode.Kind == EGraphAssetNodeKind::Label && TargetNode.LabelName == LabelName;
    };

    if (SourceNode.Kind == EGraphAssetNodeKind::Jump)
    {
        if (HasLabelTarget(SourceNode.LabelName))
        {
            return CanvasWireView{
                .SourceNodeId = SourceNode.Id,
                .SourcePin = std::string(SourcePin),
                .TargetNodeId = TargetNode.Id,
                .TargetPin = "In",
                .Kind = ESlotKind::Value,
                .IsExec = true,
            };
        }
        return std::nullopt;
    }

    if (SourceNode.Kind == EGraphAssetNodeKind::Branch)
    {
        if (SourcePin == "True" && HasLabelTarget(SourceNode.LabelName))
        {
            return CanvasWireView{
                .SourceNodeId = SourceNode.Id,
                .SourcePin = "True",
                .TargetNodeId = TargetNode.Id,
                .TargetPin = "In",
                .Kind = ESlotKind::Value,
                .IsExec = true,
            };
        }
        if (SourcePin == "False" && HasLabelTarget(SourceNode.FalseLabelName))
        {
            return CanvasWireView{
                .SourceNodeId = SourceNode.Id,
                .SourcePin = "False",
                .TargetNodeId = TargetNode.Id,
                .TargetPin = "In",
                .Kind = ESlotKind::Value,
                .IsExec = true,
            };
        }
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

[[nodiscard]] bool TryFormatTextDefault(const GraphVariableAsset& Variable, std::string& OutText)
{
    if (Variable.DefaultValue.Type == TypeId{})
    {
        OutText.clear();
        return true;
    }

    if (Variable.Type == StaticTypeId<std::string>())
    {
        std::string Value{};
        if (!DeserializeSerializedValue(Variable.DefaultValue, Value))
        {
            return false;
        }
        OutText = std::move(Value);
        return true;
    }
    if (Variable.Type == StaticTypeId<std::int32_t>())
    {
        std::int32_t Value = 0;
        if (!DeserializeSerializedValue(Variable.DefaultValue, Value))
        {
            return false;
        }
        OutText = std::to_string(Value);
        return true;
    }
    if (Variable.Type == StaticTypeId<int>())
    {
        int Value = 0;
        if (!DeserializeSerializedValue(Variable.DefaultValue, Value))
        {
            return false;
        }
        OutText = std::to_string(Value);
        return true;
    }
    if (Variable.Type == StaticTypeId<std::int64_t>())
    {
        std::int64_t Value = 0;
        if (!DeserializeSerializedValue(Variable.DefaultValue, Value))
        {
            return false;
        }
        OutText = std::to_string(Value);
        return true;
    }
    if (Variable.Type == StaticTypeId<std::uint32_t>())
    {
        std::uint32_t Value = 0;
        if (!DeserializeSerializedValue(Variable.DefaultValue, Value))
        {
            return false;
        }
        OutText = std::to_string(Value);
        return true;
    }
    if (Variable.Type == StaticTypeId<unsigned int>())
    {
        unsigned int Value = 0;
        if (!DeserializeSerializedValue(Variable.DefaultValue, Value))
        {
            return false;
        }
        OutText = std::to_string(Value);
        return true;
    }
    if (Variable.Type == StaticTypeId<std::uint64_t>())
    {
        std::uint64_t Value = 0;
        if (!DeserializeSerializedValue(Variable.DefaultValue, Value))
        {
            return false;
        }
        OutText = std::to_string(Value);
        return true;
    }
    if (Variable.Type == StaticTypeId<float>())
    {
        float Value = 0.0f;
        if (!DeserializeSerializedValue(Variable.DefaultValue, Value))
        {
            return false;
        }
        std::ostringstream Stream{};
        Stream << Value;
        OutText = Stream.str();
        return true;
    }
    if (Variable.Type == StaticTypeId<double>())
    {
        double Value = 0.0;
        if (!DeserializeSerializedValue(Variable.DefaultValue, Value))
        {
            return false;
        }
        std::ostringstream Stream{};
        Stream << Value;
        OutText = Stream.str();
        return true;
    }
    if (Variable.Type == StaticTypeId<Uuid>())
    {
        Uuid Value{};
        if (!DeserializeSerializedValue(Variable.DefaultValue, Value))
        {
            return false;
        }
        OutText = ToString(Value);
        return true;
    }

    return false;
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

[[nodiscard]] TExpected<SerializedValue> TryParseTextDefault(const GraphVariableAsset& Variable, const std::string_view Text)
{
    if (Variable.Type == StaticTypeId<std::string>())
    {
        return SerializedValue::FromValue(std::string(Text));
    }
    if (Variable.Type == StaticTypeId<std::int32_t>())
    {
        return MakeSerializedFromTextParser<std::int32_t>(Text, &ParseIntegralText<std::int32_t>);
    }
    if (Variable.Type == StaticTypeId<int>())
    {
        return MakeSerializedFromTextParser<int>(Text, &ParseIntegralText<int>);
    }
    if (Variable.Type == StaticTypeId<std::int64_t>())
    {
        return MakeSerializedFromTextParser<std::int64_t>(Text, &ParseIntegralText<std::int64_t>);
    }
    if (Variable.Type == StaticTypeId<std::uint32_t>())
    {
        return MakeSerializedFromTextParser<std::uint32_t>(Text, &ParseIntegralText<std::uint32_t>);
    }
    if (Variable.Type == StaticTypeId<unsigned int>())
    {
        return MakeSerializedFromTextParser<unsigned int>(Text, &ParseIntegralText<unsigned int>);
    }
    if (Variable.Type == StaticTypeId<std::uint64_t>())
    {
        return MakeSerializedFromTextParser<std::uint64_t>(Text, &ParseIntegralText<std::uint64_t>);
    }
    if (Variable.Type == StaticTypeId<float>())
    {
        return MakeSerializedFromTextParser<float>(Text, &ParseFloatingText<float>);
    }
    if (Variable.Type == StaticTypeId<double>())
    {
        return MakeSerializedFromTextParser<double>(Text, &ParseFloatingText<double>);
    }
    if (Variable.Type == StaticTypeId<Uuid>())
    {
        return MakeSerializedFromTextParser<Uuid>(Text, &ParseUuidText);
    }

    return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit variable type does not support text defaults"));
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

std::vector<SchemaNodeDescriptor> DescribeMembers(const TypeInfo& OwnerType, const bool SelfContext)
{
    std::vector<SchemaNodeDescriptor> Result{};

    const auto Fields = TypeRegistry::Instance().CollectFields(OwnerType.Id, true);
    for (const ReflectedFieldRef& Ref : Fields)
    {
        if (!Ref.Field)
        {
            continue;
        }

        const TypeInfo* DeclaringType = TypeRegistry::Instance().Find(Ref.OwnerType);
        const std::string OwnerLabel = DeclaringType ? ResolveTypeLabel(DeclaringType->Id) : ResolveTypeLabel(Ref.OwnerType);
        const std::string ContextPrefix = SelfContext ? "self" : "instance";

        if (CanConduitReadField(*Ref.Field))
        {
            SchemaNodeDescriptor Descriptor{};
            Descriptor.StableId = ContextPrefix + std::string(".field.read.") + OwnerLabel + "." + Ref.Field->Name;
            Descriptor.DisplayName = "Get " + Ref.Field->Name;
            Descriptor.Category = SelfContext ? "Reflection/Self/Fields" : "Reflection/Instance/Fields";
            Descriptor.Tooltip = "Read reflected field '" + Ref.Field->Name + "' from " + (SelfContext ? std::string("self") : std::string("a resolved instance")) + ".";
            Descriptor.Family = ESchemaNodeFamily::FieldRead;
            Descriptor.IsPure = true;
            Descriptor.OwnerType = Ref.OwnerType;
            Descriptor.MemberName = Ref.Field->Name;
            Descriptor.LoweredKind = SelfContext ? EGraphAssetNodeKind::SelfFieldRead : EGraphAssetNodeKind::InstanceFieldRead;
            if (!SelfContext)
            {
                Descriptor.Pins.push_back(MakeHandlePin("Target", ESchemaPinDirection::Input, OwnerType.Id));
            }
            Descriptor.Pins.push_back(MakeValuePin("Value", ESchemaPinDirection::Output, Ref.Field->FieldType));
            Result.push_back(std::move(Descriptor));
        }

        if (CanConduitWriteField(*Ref.Field))
        {
            SchemaNodeDescriptor Descriptor{};
            Descriptor.StableId = ContextPrefix + std::string(".field.write.") + OwnerLabel + "." + Ref.Field->Name;
            Descriptor.DisplayName = "Set " + Ref.Field->Name;
            Descriptor.Category = SelfContext ? "Reflection/Self/Fields" : "Reflection/Instance/Fields";
            Descriptor.Tooltip = "Write reflected field '" + Ref.Field->Name + "'.";
            Descriptor.Family = ESchemaNodeFamily::FieldWrite;
            Descriptor.IsPure = false;
            Descriptor.OwnerType = Ref.OwnerType;
            Descriptor.MemberName = Ref.Field->Name;
            Descriptor.LoweredKind = SelfContext ? EGraphAssetNodeKind::SelfFieldWrite : EGraphAssetNodeKind::InstanceFieldWrite;
            Descriptor.Pins.push_back(MakeExecPin("In", ESchemaPinDirection::Input));
            if (!SelfContext)
            {
                Descriptor.Pins.push_back(MakeHandlePin("Target", ESchemaPinDirection::Input, OwnerType.Id));
            }
            Descriptor.Pins.push_back(MakeValuePin("Value", ESchemaPinDirection::Input, Ref.Field->FieldType, true));
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

        const TypeInfo* DeclaringType = TypeRegistry::Instance().Find(Ref.OwnerType);
        const std::string OwnerLabel = DeclaringType ? ResolveTypeLabel(DeclaringType->Id) : ResolveTypeLabel(Ref.OwnerType);
        const std::string ContextPrefix = SelfContext ? "self" : "instance";

        SchemaNodeDescriptor Descriptor{};
        Descriptor.StableId = ContextPrefix + std::string(".method.") + OwnerLabel + "." + Ref.Method->Name;
        Descriptor.DisplayName = Ref.Method->Name;
        Descriptor.Category = SelfContext ? "Reflection/Self/Methods" : "Reflection/Instance/Methods";
        Descriptor.Tooltip = "Invoke reflected method '" + Ref.Method->Name + "'.";
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
            Descriptor.Pins.push_back(MakeHandlePin("Target", ESchemaPinDirection::Input, OwnerType.Id));
        }

        for (std::size_t Index = 0; Index < Ref.Method->ParamTypes.size(); ++Index)
        {
            Descriptor.Pins.push_back(MakeValuePin(BuildArgName(Index),
                                                   ESchemaPinDirection::Input,
                                                   Ref.Method->ParamTypes[Index],
                                                   true));
        }

        if (Ref.Method->ReturnType != StaticTypeId<void>())
        {
            Descriptor.Pins.push_back(MakeValuePin("Return", ESchemaPinDirection::Output, Ref.Method->ReturnType));
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
    for (const GraphNodeAsset& Node : m_asset.Nodes)
    {
        if (Node.VariableId == Id && IsVariableNodeKind(Node.Kind) && Node.Id != Uuid{})
        {
            RemovedNodeIds.insert(Node.Id);
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

    m_asset.Nodes.erase(NodeIt);
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
        MarkMutated();
        return Ok();
    }

    if (NodeState->X == X && NodeState->Y == Y)
    {
        return Ok();
    }

    NodeState->X = X;
    NodeState->Y = Y;
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
    MarkMutated();
    return Ok();
}

void GraphDocument::MarkMutated()
{
    ClearLastCompile();
    Touch();
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
        View.Revision = m_workspaceRevision + Document->Revision();
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
            for (const SchemaPinDescriptor& Pin : Descriptor->Pins)
            {
                CanvasPinView PinView{};
                PinView.Name = Pin.Name;
                PinView.Kind = Pin.Type.Kind;
                PinView.IsInput = Pin.Direction == ESchemaPinDirection::Input;
                PinView.IsExec = Pin.Type.IsExec;
                PinView.TypeLabel = Pin.Type.IsExec ? std::string("Exec") : ResolveTypeLabel(Pin.Type.Type);

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

    for (const GraphNodeAsset& Node : Document->Asset().Nodes)
    {
        if (Node.Kind != EGraphAssetNodeKind::Jump && Node.Kind != EGraphAssetNodeKind::Branch)
        {
            continue;
        }

        for (const GraphNodeAsset& CandidateTarget : Document->Asset().Nodes)
        {
            if (Node.Kind == EGraphAssetNodeKind::Jump)
            {
                if (auto Wire = BuildCanvasWire(Document->Asset(), Node, CandidateTarget, "Out"); Wire.has_value())
                {
                    Result.Wires.push_back(std::move(*Wire));
                }
                continue;
            }

            if (auto Wire = BuildCanvasWire(Document->Asset(), Node, CandidateTarget, "True"); Wire.has_value())
            {
                Result.Wires.push_back(std::move(*Wire));
            }
            if (auto Wire = BuildCanvasWire(Document->Asset(), Node, CandidateTarget, "False"); Wire.has_value())
            {
                Result.Wires.push_back(std::move(*Wire));
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

std::vector<ClassHostTypeOption> ConduitEditorService::AvailableClassHostTypes() const
{
    std::vector<ClassHostTypeOption> Result{};
    const auto Types = TypeRegistry::Instance().Derived(StaticTypeId<BaseNode>());
    Result.reserve(Types.size());

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
        return Left.Label < Right.Label;
    });
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

    const std::uint64_t RevisionBefore = Document->Revision();
    const Result MoveResult = Document->SetNodePosition(NodeId, X, Y);
    if (!MoveResult)
    {
        return MoveResult;
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

    const std::uint64_t RevisionBefore = Document->Revision();
    const Result ViewportResult = Document->SetViewport(PanX, PanY, Zoom);
    if (!ViewportResult)
    {
        return ViewportResult;
    }

    if (Document->Revision() != RevisionBefore)
    {
        BumpWorkspaceRevision();
    }
    return Ok();
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
