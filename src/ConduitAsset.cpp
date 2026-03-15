#include "Conduit/Asset.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "AuthoredAssetCereal.h"
#include "Handles.h"

namespace SnAPI::GameFramework::Conduit
{

Result GraphAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetViaCerealJsonStream(*this, Output);
}

Result ClassAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetViaCerealJsonStream(*this, Output);
}

namespace
{

[[nodiscard]] bool IsHandleCarrierType(const TypeId& Type)
{
    return Type == StaticTypeId<NodeHandle>() || Type == StaticTypeId<ComponentHandle>();
}

[[nodiscard]] ESlotKind ResolveSlotKindForType(const TypeId& Type)
{
    return IsHandleCarrierType(Type) ? ESlotKind::Handle : ESlotKind::Value;
}

TExpected<const TypeInfo*> ResolveRegisteredType(const TypeId& Type, const std::string_view Context)
{
    const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, std::string(Context) + " type is not registered"));
    }
    return Info;
}

TExpected<SlotId> ResolveSlotRef(const std::vector<SlotId>& Slots, const SlotId Ref, const std::string_view Context)
{
    if (!Ref.IsValid() || Ref.Value >= Slots.size())
    {
        return std::unexpected(MakeError(EErrorCode::OutOfRange, std::string(Context) + " slot reference is invalid"));
    }
    return Slots[Ref.Value];
}

TExpected<std::optional<SlotId>> ResolveOptionalSlotRef(const std::vector<SlotId>& Slots,
                                                        const SlotId Ref,
                                                        const std::string_view Context)
{
    if (!Ref.IsValid())
    {
        return std::optional<SlotId>{};
    }

    auto SlotResult = ResolveSlotRef(Slots, Ref, Context);
    if (!SlotResult)
    {
        return std::unexpected(SlotResult.error());
    }
    return std::optional<SlotId>{*SlotResult};
}

TExpected<std::vector<SlotId>> ResolveInputSlots(const std::vector<SlotId>& Slots,
                                                 const std::vector<SlotId>& AuthoredInputs,
                                                 const std::string_view Context)
{
    std::vector<SlotId> Result{};
    Result.reserve(AuthoredInputs.size());
    for (const SlotId Authored : AuthoredInputs)
    {
        auto SlotResult = ResolveSlotRef(Slots, Authored, Context);
        if (!SlotResult)
        {
            return std::unexpected(SlotResult.error());
        }
        Result.push_back(*SlotResult);
    }
    return Result;
}

TExpected<CompiledGraphVariable> ResolveVariableAsset(::SnAPI::GameFramework::Conduit::GraphBuilder& Builder,
                                                      const GraphVariableAsset& Variable)
{
    if (Variable.Id == Uuid{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit graph variable id is missing"));
    }
    if (Variable.Name.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit graph variable name is empty"));
    }
    if (Variable.Type == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit graph variable type is missing"));
    }

    auto TypeResult = ResolveRegisteredType(Variable.Type, "Conduit graph variable");
    if (!TypeResult)
    {
        return std::unexpected(TypeResult.error());
    }
    if (!(*TypeResult)->RuntimeOps)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit graph variable type has no runtime ops"));
    }

    if (Variable.DefaultValue.Type != TypeId{})
    {
        if (Variable.DefaultValue.Type != Variable.Type)
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit graph variable default type mismatch"));
        }
    }
    else if (!(*TypeResult)->RuntimeOps->DefaultConstruct)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Conduit graph variable without a default value must be default-constructible"));
    }

    auto SlotResult = Builder.AddSlot(**TypeResult, ResolveSlotKindForType(Variable.Type));
    if (!SlotResult)
    {
        return std::unexpected(SlotResult.error());
    }

    return CompiledGraphVariable{
        .Id = Variable.Id,
        .Name = Variable.Name,
        .Slot = *SlotResult,
        .Type = *TypeResult,
        .DefaultValue = Variable.DefaultValue,
    };
}

TExpected<LabelId> GetOrCreateLabel(GraphBuilder& Builder,
                                    std::unordered_map<std::string, LabelId>& Labels,
                                    const std::string_view Name)
{
    if (Name.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit asset label name is empty"));
    }

    if (const auto It = Labels.find(std::string(Name)); It != Labels.end())
    {
        return It->second;
    }

    const LabelId Label = Builder.CreateLabel();
    Labels.emplace(std::string(Name), Label);
    return Label;
}

[[nodiscard]] bool GraphUsesExplicitExecTargets(const GraphAsset& Asset)
{
    return std::any_of(Asset.Nodes.begin(), Asset.Nodes.end(), [](const GraphNodeAsset& Node) {
        return Node.ExecTargetNodeId != Uuid{} || Node.FalseExecTargetNodeId != Uuid{};
    });
}

[[nodiscard]] bool ProducesAuthoredSlot(const GraphNodeAsset& Node)
{
    switch (Node.Kind)
    {
    case EGraphAssetNodeKind::Constant:
    case EGraphAssetNodeKind::VariableGet:
    case EGraphAssetNodeKind::UnaryIntrinsic:
    case EGraphAssetNodeKind::BinaryIntrinsic:
    case EGraphAssetNodeKind::SelfFieldRead:
    case EGraphAssetNodeKind::InstanceFieldRead:
        return Node.Output.IsValid();
    case EGraphAssetNodeKind::SelfMethodCall:
    case EGraphAssetNodeKind::InstanceMethodCall:
        return Node.ReturnSlot.IsValid();
    default:
        return false;
    }
}

[[nodiscard]] std::optional<SlotId> ProducedAuthoredSlot(const GraphNodeAsset& Node)
{
    if (!ProducesAuthoredSlot(Node))
    {
        return std::nullopt;
    }
    if (Node.Kind == EGraphAssetNodeKind::SelfMethodCall || Node.Kind == EGraphAssetNodeKind::InstanceMethodCall)
    {
        return Node.ReturnSlot;
    }
    return Node.Output;
}

[[nodiscard]] std::vector<SlotId> ConsumedAuthoredSlots(const GraphNodeAsset& Node)
{
    std::vector<SlotId> Result{};
    switch (Node.Kind)
    {
    case EGraphAssetNodeKind::VariableSet:
    case EGraphAssetNodeKind::SelfFieldWrite:
        if (Node.Input.IsValid())
        {
            Result.push_back(Node.Input);
        }
        break;
    case EGraphAssetNodeKind::UnaryIntrinsic:
        if (Node.Input.IsValid())
        {
            Result.push_back(Node.Input);
        }
        break;
    case EGraphAssetNodeKind::BinaryIntrinsic:
        if (Node.Left.IsValid())
        {
            Result.push_back(Node.Left);
        }
        if (Node.Right.IsValid())
        {
            Result.push_back(Node.Right);
        }
        break;
    case EGraphAssetNodeKind::Branch:
        if (Node.Condition.IsValid())
        {
            Result.push_back(Node.Condition);
        }
        break;
    case EGraphAssetNodeKind::InstanceFieldWrite:
        if (Node.Input.IsValid())
        {
            Result.push_back(Node.Input);
        }
        [[fallthrough]];
    case EGraphAssetNodeKind::InstanceFieldRead:
    case EGraphAssetNodeKind::InstanceMethodCall:
        if (Node.Instance.IsValid())
        {
            Result.push_back(Node.Instance);
        }
        break;
    default:
        break;
    }

    for (const SlotId Input : Node.Inputs)
    {
        if (Input.IsValid())
        {
            Result.push_back(Input);
        }
    }

    return Result;
}

TExpected<const MethodInfo*> ResolveAuthoredMethodInfo(const GraphAsset& Asset,
                                                      const GraphNodeAsset& Node,
                                                      const TypeInfo& SelfType)
{
    if (Node.Kind != EGraphAssetNodeKind::SelfMethodCall && Node.Kind != EGraphAssetNodeKind::InstanceMethodCall)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit authored node is not a method call"));
    }

    const TypeInfo* OwnerType = &SelfType;
    if (Node.Kind == EGraphAssetNodeKind::InstanceMethodCall)
    {
        auto OwnerTypeResult = ResolveRegisteredType(Node.OwnerType, "Conduit instance method owner");
        if (!OwnerTypeResult)
        {
            return std::unexpected(OwnerTypeResult.error());
        }
        OwnerType = *OwnerTypeResult;
    }

    std::vector<TypeId> InputTypes{};
    InputTypes.reserve(Node.Inputs.size());
    for (const SlotId Input : Node.Inputs)
    {
        if (!Input.IsValid() || Input.Value >= Asset.Slots.size())
        {
            InputTypes.push_back(TypeId{});
            continue;
        }
        InputTypes.push_back(Asset.Slots[Input.Value].Type);
    }

    const auto Methods = TypeRegistry::Instance().CollectMethods(OwnerType->Id, true);
    const MethodInfo* Fallback = nullptr;
    for (const ReflectedMethodRef& Ref : Methods)
    {
        if (!Ref.Method || Ref.Method->RawInvoke == nullptr)
        {
            continue;
        }
        if (Ref.Method->Name != Node.MemberName || Ref.Method->ParamTypes.size() != InputTypes.size())
        {
            continue;
        }

        bool ExactTypes = true;
        for (std::size_t Index = 0; Index < InputTypes.size(); ++Index)
        {
            if (InputTypes[Index] != TypeId{} && Ref.Method->ParamTypes[Index] != InputTypes[Index])
            {
                ExactTypes = false;
                break;
            }
        }
        if (ExactTypes)
        {
            return Ref.Method;
        }
        if (!Fallback)
        {
            Fallback = Ref.Method;
        }
    }

    if (Fallback)
    {
        return Fallback;
    }

    return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit method metadata was not found for the authored node"));
}

TExpected<bool> NodeUsesExecFlow(const GraphAsset& Asset, const GraphNodeAsset& Node, const TypeInfo& SelfType)
{
    switch (Node.Kind)
    {
    case EGraphAssetNodeKind::EntryPoint:
    case EGraphAssetNodeKind::Label:
    case EGraphAssetNodeKind::VariableSet:
    case EGraphAssetNodeKind::Jump:
    case EGraphAssetNodeKind::Branch:
    case EGraphAssetNodeKind::SelfFieldWrite:
    case EGraphAssetNodeKind::InstanceFieldWrite:
        return true;
    case EGraphAssetNodeKind::SelfMethodCall:
    case EGraphAssetNodeKind::InstanceMethodCall:
    {
        auto MethodResult = ResolveAuthoredMethodInfo(Asset, Node, SelfType);
        if (!MethodResult)
        {
            return std::unexpected(MethodResult.error());
        }
        return !(*MethodResult)->IsConst;
    }
    default:
        return false;
    }
}

TExpected<bool> NodeHasExecInput(const GraphAsset& Asset, const GraphNodeAsset& Node, const TypeInfo& SelfType)
{
    switch (Node.Kind)
    {
    case EGraphAssetNodeKind::EntryPoint:
        return false;
    default:
        return NodeUsesExecFlow(Asset, Node, SelfType);
    }
}

TExpected<void> EmitNonControlNode(GraphBuilder& Builder,
                                   const GraphAsset& Asset,
                                   const std::vector<SlotId>& Slots,
                                   const std::vector<CompiledGraphVariable>& Variables,
                                   const std::unordered_map<Uuid, std::size_t, UuidHash>& VariableById,
                                   const GraphNodeAsset& Node)
{
    switch (Node.Kind)
    {
    case EGraphAssetNodeKind::Constant:
    {
        auto OutputResult = ResolveSlotRef(Slots, Node.Output, "Conduit constant output");
        if (!OutputResult)
        {
            return std::unexpected(OutputResult.error());
        }
        auto AddResult = Builder.AddSerializedConstant(*OutputResult, Node.ConstantValue);
        if (!AddResult)
        {
            return std::unexpected(AddResult.error());
        }
        return Ok();
    }
    case EGraphAssetNodeKind::VariableGet:
    {
        const auto It = VariableById.find(Node.VariableId);
        if (It == VariableById.end())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit variable-get target was not found"));
        }
        auto OutputResult = ResolveSlotRef(Slots, Node.Output, "Conduit variable-get output");
        if (!OutputResult)
        {
            return std::unexpected(OutputResult.error());
        }
        auto CopyResult = Builder.AddCopy(Variables[It->second].Slot, *OutputResult);
        if (!CopyResult)
        {
            return std::unexpected(CopyResult.error());
        }
        return Ok();
    }
    case EGraphAssetNodeKind::VariableSet:
    {
        const auto It = VariableById.find(Node.VariableId);
        if (It == VariableById.end())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit variable-set target was not found"));
        }
        auto InputResult = ResolveSlotRef(Slots, Node.Input, "Conduit variable-set input");
        if (!InputResult)
        {
            return std::unexpected(InputResult.error());
        }
        auto CopyResult = Builder.AddCopy(*InputResult, Variables[It->second].Slot);
        if (!CopyResult)
        {
            return std::unexpected(CopyResult.error());
        }
        return Ok();
    }
    case EGraphAssetNodeKind::UnaryIntrinsic:
    {
        auto InputResult = ResolveSlotRef(Slots, Node.Input, "Conduit unary intrinsic input");
        if (!InputResult)
        {
            return std::unexpected(InputResult.error());
        }
        auto OutputResult = ResolveSlotRef(Slots, Node.Output, "Conduit unary intrinsic output");
        if (!OutputResult)
        {
            return std::unexpected(OutputResult.error());
        }
        auto AddResult = Builder.AddUnaryIntrinsic(Node.UnaryOp, *InputResult, *OutputResult);
        if (!AddResult)
        {
            return std::unexpected(AddResult.error());
        }
        return Ok();
    }
    case EGraphAssetNodeKind::BinaryIntrinsic:
    {
        auto LeftResult = ResolveSlotRef(Slots, Node.Left, "Conduit binary intrinsic left");
        if (!LeftResult)
        {
            return std::unexpected(LeftResult.error());
        }
        auto RightResult = ResolveSlotRef(Slots, Node.Right, "Conduit binary intrinsic right");
        if (!RightResult)
        {
            return std::unexpected(RightResult.error());
        }
        auto OutputResult = ResolveSlotRef(Slots, Node.Output, "Conduit binary intrinsic output");
        if (!OutputResult)
        {
            return std::unexpected(OutputResult.error());
        }
        auto AddResult = Builder.AddBinaryIntrinsic(Node.BinaryOp, *LeftResult, *RightResult, *OutputResult);
        if (!AddResult)
        {
            return std::unexpected(AddResult.error());
        }
        return Ok();
    }
    case EGraphAssetNodeKind::SelfFieldRead:
    {
        auto OutputResult = ResolveSlotRef(Slots, Node.Output, "Conduit self field read output");
        if (!OutputResult)
        {
            return std::unexpected(OutputResult.error());
        }
        auto AddResult = Builder.AddSelfFieldRead(Node.MemberName, *OutputResult);
        if (!AddResult)
        {
            return std::unexpected(AddResult.error());
        }
        return Ok();
    }
    case EGraphAssetNodeKind::SelfFieldWrite:
    {
        auto InputResult = ResolveSlotRef(Slots, Node.Input, "Conduit self field write input");
        if (!InputResult)
        {
            return std::unexpected(InputResult.error());
        }
        auto AddResult = Builder.AddSelfFieldWrite(Node.MemberName, *InputResult);
        if (!AddResult)
        {
            return std::unexpected(AddResult.error());
        }
        return Ok();
    }
    case EGraphAssetNodeKind::SelfMethodCall:
    {
        auto InputsResult = ResolveInputSlots(Slots, Node.Inputs, "Conduit self method input");
        if (!InputsResult)
        {
            return std::unexpected(InputsResult.error());
        }
        auto OutputResult = ResolveOptionalSlotRef(Slots, Node.ReturnSlot, "Conduit self method output");
        if (!OutputResult)
        {
            return std::unexpected(OutputResult.error());
        }
        auto AddResult = Builder.AddSelfMethodCall(Node.MemberName, *InputsResult, *OutputResult);
        if (!AddResult)
        {
            return std::unexpected(AddResult.error());
        }
        return Ok();
    }
    case EGraphAssetNodeKind::InstanceFieldRead:
    {
        auto OwnerTypeResult = ResolveRegisteredType(Node.OwnerType, "Conduit instance field read owner");
        if (!OwnerTypeResult)
        {
            return std::unexpected(OwnerTypeResult.error());
        }
        auto InstanceResult = ResolveSlotRef(Slots, Node.Instance, "Conduit instance field read instance");
        if (!InstanceResult)
        {
            return std::unexpected(InstanceResult.error());
        }
        auto OutputResult = ResolveSlotRef(Slots, Node.Output, "Conduit instance field read output");
        if (!OutputResult)
        {
            return std::unexpected(OutputResult.error());
        }
        auto AddResult = Builder.AddFieldRead(**OwnerTypeResult, *InstanceResult, Node.MemberName, *OutputResult);
        if (!AddResult)
        {
            return std::unexpected(AddResult.error());
        }
        return Ok();
    }
    case EGraphAssetNodeKind::InstanceFieldWrite:
    {
        auto OwnerTypeResult = ResolveRegisteredType(Node.OwnerType, "Conduit instance field write owner");
        if (!OwnerTypeResult)
        {
            return std::unexpected(OwnerTypeResult.error());
        }
        auto InstanceResult = ResolveSlotRef(Slots, Node.Instance, "Conduit instance field write instance");
        if (!InstanceResult)
        {
            return std::unexpected(InstanceResult.error());
        }
        auto InputResult = ResolveSlotRef(Slots, Node.Input, "Conduit instance field write input");
        if (!InputResult)
        {
            return std::unexpected(InputResult.error());
        }
        auto AddResult = Builder.AddFieldWrite(**OwnerTypeResult, *InstanceResult, Node.MemberName, *InputResult);
        if (!AddResult)
        {
            return std::unexpected(AddResult.error());
        }
        return Ok();
    }
    case EGraphAssetNodeKind::InstanceMethodCall:
    {
        auto OwnerTypeResult = ResolveRegisteredType(Node.OwnerType, "Conduit instance method owner");
        if (!OwnerTypeResult)
        {
            return std::unexpected(OwnerTypeResult.error());
        }
        auto InstanceResult = ResolveSlotRef(Slots, Node.Instance, "Conduit instance method instance");
        if (!InstanceResult)
        {
            return std::unexpected(InstanceResult.error());
        }
        auto InputsResult = ResolveInputSlots(Slots, Node.Inputs, "Conduit instance method input");
        if (!InputsResult)
        {
            return std::unexpected(InputsResult.error());
        }
        auto OutputResult = ResolveOptionalSlotRef(Slots, Node.ReturnSlot, "Conduit instance method output");
        if (!OutputResult)
        {
            return std::unexpected(OutputResult.error());
        }
        auto AddResult = Builder.AddMethodCall(**OwnerTypeResult,
                                               *InstanceResult,
                                               Node.MemberName,
                                               *InputsResult,
                                               *OutputResult);
        if (!AddResult)
        {
            return std::unexpected(AddResult.error());
        }
        return Ok();
    }
    default:
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit authored node kind cannot be emitted as a non-control node"));
    }
}

} // namespace

TExpected<CompiledGraph> GraphAsset::Compile() const
{
    return CompileGraphAsset(*this);
}

TExpected<CompiledGraph> CompileGraphAsset(const GraphAsset& Asset)
{
    const TypeId SelfTypeId = Asset.SelfType == TypeId{} ? TypeIdFromName("void") : Asset.SelfType;
    auto SelfTypeResult = ResolveRegisteredType(SelfTypeId, "Conduit self");
    if (!SelfTypeResult)
    {
        return std::unexpected(SelfTypeResult.error());
    }

    GraphBuilder Builder(**SelfTypeResult);
    std::vector<SlotId> Slots{};
    Slots.reserve(Asset.Slots.size());
    for (const GraphSlotAsset& SlotAsset : Asset.Slots)
    {
        if (SlotAsset.Type == TypeId{})
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit asset slot type is missing"));
        }
        auto SlotResult = Builder.AddSlot(SlotAsset.Type, SlotAsset.Kind);
        if (!SlotResult)
        {
            return std::unexpected(SlotResult.error());
        }
        Slots.push_back(*SlotResult);
    }

    std::vector<CompiledGraphVariable> Variables{};
    Variables.reserve(Asset.Variables.size());
    std::unordered_map<Uuid, std::size_t, UuidHash> VariableById{};
    std::unordered_map<std::string, std::size_t> VariableByName{};
    for (const GraphVariableAsset& Variable : Asset.Variables)
    {
        auto VariableResult = ResolveVariableAsset(Builder, Variable);
        if (!VariableResult)
        {
            return std::unexpected(VariableResult.error());
        }
        if (VariableById.contains(VariableResult->Id))
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit graph variable id is duplicated"));
        }
        if (VariableByName.contains(VariableResult->Name))
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit graph variable name is duplicated"));
        }
        VariableById.emplace(VariableResult->Id, Variables.size());
        VariableByName.emplace(VariableResult->Name, Variables.size());
        Variables.push_back(std::move(*VariableResult));
    }

    const TypeInfo& SelfType = **SelfTypeResult;
    const auto EmitLegacyLinearGraph = [&]() -> Result {
        std::unordered_map<std::string, LabelId> Labels{};
        for (const GraphNodeAsset& Node : Asset.Nodes)
        {
            switch (Node.Kind)
            {
            case EGraphAssetNodeKind::EntryPoint:
            {
                auto DeltaSlotResult = ResolveOptionalSlotRef(Slots, Node.Output, "Conduit entrypoint delta-seconds");
                if (!DeltaSlotResult)
                {
                    return std::unexpected(DeltaSlotResult.error());
                }

                const std::string_view EntryName = Node.BuiltinEntryPoint == EBuiltinEntryPoint::None
                                                       ? std::string_view(Node.EntryPointName)
                                                       : BuiltinEntryPointName(Node.BuiltinEntryPoint);
                auto AddResult = Builder.AddEntryPoint(EntryName,
                                                       Node.BuiltinEntryPoint,
                                                       DeltaSlotResult.value().value_or(SlotId{}));
                if (!AddResult)
                {
                    return std::unexpected(AddResult.error());
                }
                break;
            }
            case EGraphAssetNodeKind::Label:
            {
                auto LabelResult = GetOrCreateLabel(Builder, Labels, Node.LabelName);
                if (!LabelResult)
                {
                    return std::unexpected(LabelResult.error());
                }
                auto MarkResult = Builder.MarkLabel(*LabelResult);
                if (!MarkResult)
                {
                    return std::unexpected(MarkResult.error());
                }
                break;
            }
            case EGraphAssetNodeKind::Jump:
            {
                auto LabelResult = GetOrCreateLabel(Builder, Labels, Node.LabelName);
                if (!LabelResult)
                {
                    return std::unexpected(LabelResult.error());
                }
                auto AddResult = Builder.AddJump(*LabelResult);
                if (!AddResult)
                {
                    return std::unexpected(AddResult.error());
                }
                break;
            }
            case EGraphAssetNodeKind::Branch:
            {
                auto ConditionResult = ResolveSlotRef(Slots, Node.Condition, "Conduit branch condition");
                if (!ConditionResult)
                {
                    return std::unexpected(ConditionResult.error());
                }
                auto TrueLabelResult = GetOrCreateLabel(Builder, Labels, Node.LabelName);
                if (!TrueLabelResult)
                {
                    return std::unexpected(TrueLabelResult.error());
                }
                auto FalseLabelResult = GetOrCreateLabel(Builder, Labels, Node.FalseLabelName);
                if (!FalseLabelResult)
                {
                    return std::unexpected(FalseLabelResult.error());
                }
                auto AddResult = Builder.AddBranch(*ConditionResult, *TrueLabelResult, *FalseLabelResult);
                if (!AddResult)
                {
                    return std::unexpected(AddResult.error());
                }
                break;
            }
            default:
            {
                auto EmitResult = EmitNonControlNode(Builder, Asset, Slots, Variables, VariableById, Node);
                if (!EmitResult)
                {
                    return std::unexpected(EmitResult.error());
                }
                break;
            }
            }
        }
        return Ok();
    };

    if (!GraphUsesExplicitExecTargets(Asset))
    {
        auto EmitResult = EmitLegacyLinearGraph();
        if (!EmitResult)
        {
            return std::unexpected(EmitResult.error());
        }
    }
    else
    {
        std::unordered_map<Uuid, const GraphNodeAsset*, UuidHash> NodeById{};
        NodeById.reserve(Asset.Nodes.size());
        std::unordered_map<std::string, const GraphNodeAsset*> LabelNodesByName{};
        std::unordered_map<std::uint32_t, const GraphNodeAsset*> ProducerByAuthoredSlot{};
        ProducerByAuthoredSlot.reserve(Asset.Slots.size());
        std::vector<const GraphNodeAsset*> EntryNodes{};

        for (const GraphNodeAsset& Node : Asset.Nodes)
        {
            if (Node.Id != Uuid{})
            {
                NodeById.emplace(Node.Id, &Node);
            }
            if (Node.Kind == EGraphAssetNodeKind::Label && !Node.LabelName.empty())
            {
                LabelNodesByName.emplace(Node.LabelName, &Node);
            }
            if (Node.Kind == EGraphAssetNodeKind::EntryPoint)
            {
                EntryNodes.push_back(&Node);
            }
            if (const auto Produced = ProducedAuthoredSlot(Node); Produced.has_value() && Produced->IsValid())
            {
                ProducerByAuthoredSlot.emplace(Produced->Value, &Node);
            }
        }

        if (EntryNodes.empty())
        {
            auto EmitResult = EmitLegacyLinearGraph();
            if (!EmitResult)
            {
                return std::unexpected(EmitResult.error());
            }
        }
        else
        {
            const auto ResolveExecTarget = [&](const GraphNodeAsset& SourceNode,
                                               const std::string_view SourcePin)
                -> TExpected<const GraphNodeAsset*> {
                Uuid TargetNodeId{};
                if (SourcePin == "Out" || SourcePin == "True")
                {
                    TargetNodeId = SourceNode.ExecTargetNodeId;
                }
                else if (SourcePin == "False")
                {
                    TargetNodeId = SourceNode.FalseExecTargetNodeId;
                }

                if (TargetNodeId != Uuid{})
                {
                    const auto It = NodeById.find(TargetNodeId);
                    if (It == NodeById.end())
                    {
                        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit exec target node was not found"));
                    }
                    return It->second;
                }

                if (SourceNode.Kind == EGraphAssetNodeKind::Jump && SourcePin == "Out" && !SourceNode.LabelName.empty())
                {
                    const auto It = LabelNodesByName.find(SourceNode.LabelName);
                    if (It == LabelNodesByName.end())
                    {
                        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit jump target label was not found"));
                    }
                    return It->second;
                }

                if (SourceNode.Kind == EGraphAssetNodeKind::Branch)
                {
                    const std::string_view LabelName = SourcePin == "True" ? SourceNode.LabelName : SourceNode.FalseLabelName;
                    if (!LabelName.empty())
                    {
                        const auto It = LabelNodesByName.find(std::string(LabelName));
                        if (It == LabelNodesByName.end())
                        {
                            return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit branch target label was not found"));
                        }
                        return It->second;
                    }
                }

                return static_cast<const GraphNodeAsset*>(nullptr);
            };

            for (const GraphNodeAsset* EntryNode : EntryNodes)
            {
                auto DeltaSlotResult = ResolveOptionalSlotRef(Slots, EntryNode->Output, "Conduit entrypoint delta-seconds");
                if (!DeltaSlotResult)
                {
                    return std::unexpected(DeltaSlotResult.error());
                }

                const std::string_view EntryName = EntryNode->BuiltinEntryPoint == EBuiltinEntryPoint::None
                                                       ? std::string_view(EntryNode->EntryPointName)
                                                       : BuiltinEntryPointName(EntryNode->BuiltinEntryPoint);
                auto AddEntryResult = Builder.AddEntryPoint(EntryName,
                                                            EntryNode->BuiltinEntryPoint,
                                                            DeltaSlotResult.value().value_or(SlotId{}));
                if (!AddEntryResult)
                {
                    return std::unexpected(AddEntryResult.error());
                }

                std::unordered_map<const GraphNodeAsset*, LabelId> NodeLabels{};
                NodeLabels.reserve(NodeById.size());
                for (const auto& [_, NodePtr] : NodeById)
                {
                    NodeLabels.emplace(NodePtr, Builder.CreateLabel());
                }
                const LabelId EntryExitLabel = Builder.CreateLabel();

                std::unordered_set<const GraphNodeAsset*> ActiveExec{};
                std::unordered_set<const GraphNodeAsset*> ActivePure{};
                std::unordered_set<const GraphNodeAsset*> EmittedExec{};
                std::unordered_set<const GraphNodeAsset*> EmittedPure{};
                std::unordered_set<const GraphNodeAsset*> MarkedLabels{};

                std::function<TExpected<void>(const GraphNodeAsset&)> EmitPureNode;
                std::function<TExpected<void>(const GraphNodeAsset&)> EmitExecNode;

                EmitPureNode = [&](const GraphNodeAsset& Node) -> TExpected<void> {
                    if (EmittedPure.contains(&Node) || EmittedExec.contains(&Node))
                    {
                        return Ok();
                    }
                    if (ActivePure.contains(&Node))
                    {
                        return std::unexpected(MakeError(EErrorCode::OutOfRange,
                                                         "Conduit pure data dependency cycle was detected"));
                    }

                    auto UsesExecResult = NodeUsesExecFlow(Asset, Node, SelfType);
                    if (!UsesExecResult)
                    {
                        return std::unexpected(UsesExecResult.error());
                    }
                    if (*UsesExecResult)
                    {
                        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                         "Conduit impure node must be reached through exec flow before its outputs are consumed"));
                    }

                    ActivePure.insert(&Node);
                    for (const SlotId Input : ConsumedAuthoredSlots(Node))
                    {
                        const auto ProducerIt = ProducerByAuthoredSlot.find(Input.Value);
                        if (ProducerIt == ProducerByAuthoredSlot.end())
                        {
                            continue;
                        }

                        const GraphNodeAsset& ProducerNode = *ProducerIt->second;
                        auto ProducerUsesExecResult = NodeUsesExecFlow(Asset, ProducerNode, SelfType);
                        if (!ProducerUsesExecResult)
                        {
                            ActivePure.erase(&Node);
                            return std::unexpected(ProducerUsesExecResult.error());
                        }

                        if (*ProducerUsesExecResult)
                        {
                            if (!EmittedExec.contains(&ProducerNode) && !EmittedPure.contains(&ProducerNode))
                            {
                                ActivePure.erase(&Node);
                                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                                 "Conduit impure producer must execute before one dependent node consumes its output"));
                            }
                            continue;
                        }

                        auto EmitDependencyResult = EmitPureNode(ProducerNode);
                        if (!EmitDependencyResult)
                        {
                            ActivePure.erase(&Node);
                            return EmitDependencyResult;
                        }
                    }

                    auto EmitNodeResult = EmitNonControlNode(Builder, Asset, Slots, Variables, VariableById, Node);
                    ActivePure.erase(&Node);
                    if (!EmitNodeResult)
                    {
                        return std::unexpected(EmitNodeResult.error());
                    }

                    EmittedPure.insert(&Node);
                    return Ok();
                };

                EmitExecNode = [&](const GraphNodeAsset& Node) -> TExpected<void> {
                    auto HasExecInputResult = NodeHasExecInput(Asset, Node, SelfType);
                    if (!HasExecInputResult)
                    {
                        return std::unexpected(HasExecInputResult.error());
                    }
                    if (!*HasExecInputResult)
                    {
                        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                         "Conduit exec flow targeted a node that has no exec input"));
                    }

                    if (EmittedExec.contains(&Node))
                    {
                        auto AddJumpResult = Builder.AddJump(NodeLabels.at(&Node));
                        if (!AddJumpResult)
                        {
                            return std::unexpected(AddJumpResult.error());
                        }
                        return Ok();
                    }
                    if (ActiveExec.contains(&Node))
                    {
                        auto AddJumpResult = Builder.AddJump(NodeLabels.at(&Node));
                        if (!AddJumpResult)
                        {
                            return std::unexpected(AddJumpResult.error());
                        }
                        return Ok();
                    }

                    ActiveExec.insert(&Node);
                    if (!MarkedLabels.contains(&Node))
                    {
                        auto MarkResult = Builder.MarkLabel(NodeLabels.at(&Node));
                        if (!MarkResult)
                        {
                            ActiveExec.erase(&Node);
                            return std::unexpected(MarkResult.error());
                        }
                        MarkedLabels.insert(&Node);
                    }

                    for (const SlotId Input : ConsumedAuthoredSlots(Node))
                    {
                        const auto ProducerIt = ProducerByAuthoredSlot.find(Input.Value);
                        if (ProducerIt == ProducerByAuthoredSlot.end())
                        {
                            continue;
                        }

                        const GraphNodeAsset& ProducerNode = *ProducerIt->second;
                        auto ProducerUsesExecResult = NodeUsesExecFlow(Asset, ProducerNode, SelfType);
                        if (!ProducerUsesExecResult)
                        {
                            ActiveExec.erase(&Node);
                            return std::unexpected(ProducerUsesExecResult.error());
                        }

                        if (*ProducerUsesExecResult)
                        {
                            if (!EmittedExec.contains(&ProducerNode) && !EmittedPure.contains(&ProducerNode))
                            {
                                ActiveExec.erase(&Node);
                                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                                 "Conduit impure producer must execute before one dependent node consumes its output"));
                            }
                            continue;
                        }

                        auto EmitDependencyResult = EmitPureNode(ProducerNode);
                        if (!EmitDependencyResult)
                        {
                            ActiveExec.erase(&Node);
                            return EmitDependencyResult;
                        }
                    }

                    if (Node.Kind == EGraphAssetNodeKind::Label)
                    {
                        EmittedExec.insert(&Node);
                        ActiveExec.erase(&Node);
                        auto NextTargetResult = ResolveExecTarget(Node, "Out");
                        if (!NextTargetResult)
                        {
                            return std::unexpected(NextTargetResult.error());
                        }
                        if (*NextTargetResult)
                        {
                            return EmitExecNode(**NextTargetResult);
                        }
                        auto AddExitJumpResult = Builder.AddJump(EntryExitLabel);
                        if (!AddExitJumpResult)
                        {
                            return std::unexpected(AddExitJumpResult.error());
                        }
                        return Ok();
                    }

                    if (Node.Kind == EGraphAssetNodeKind::Jump)
                    {
                        auto TargetResult = ResolveExecTarget(Node, "Out");
                        if (!TargetResult)
                        {
                            ActiveExec.erase(&Node);
                            return std::unexpected(TargetResult.error());
                        }
                        if (!*TargetResult)
                        {
                            ActiveExec.erase(&Node);
                            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit jump node has no target"));
                        }

                        auto AddJumpResult = Builder.AddJump(NodeLabels.at(*TargetResult));
                        if (!AddJumpResult)
                        {
                            ActiveExec.erase(&Node);
                            return std::unexpected(AddJumpResult.error());
                        }

                        EmittedExec.insert(&Node);
                        ActiveExec.erase(&Node);
                        if (!EmittedExec.contains(*TargetResult) && !ActiveExec.contains(*TargetResult))
                        {
                            return EmitExecNode(**TargetResult);
                        }
                        return Ok();
                    }

                    if (Node.Kind == EGraphAssetNodeKind::Branch)
                    {
                        auto ConditionResult = ResolveSlotRef(Slots, Node.Condition, "Conduit branch condition");
                        if (!ConditionResult)
                        {
                            ActiveExec.erase(&Node);
                            return std::unexpected(ConditionResult.error());
                        }

                        auto TrueTargetResult = ResolveExecTarget(Node, "True");
                        if (!TrueTargetResult)
                        {
                            ActiveExec.erase(&Node);
                            return std::unexpected(TrueTargetResult.error());
                        }
                        auto FalseTargetResult = ResolveExecTarget(Node, "False");
                        if (!FalseTargetResult)
                        {
                            ActiveExec.erase(&Node);
                            return std::unexpected(FalseTargetResult.error());
                        }
                        if (!*TrueTargetResult || !*FalseTargetResult)
                        {
                            ActiveExec.erase(&Node);
                            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                             "Conduit branch node requires both true and false exec targets"));
                        }

                        auto AddBranchResult = Builder.AddBranch(*ConditionResult,
                                                                 NodeLabels.at(*TrueTargetResult),
                                                                 NodeLabels.at(*FalseTargetResult));
                        if (!AddBranchResult)
                        {
                            ActiveExec.erase(&Node);
                            return std::unexpected(AddBranchResult.error());
                        }

                        EmittedExec.insert(&Node);
                        ActiveExec.erase(&Node);

                        if (!EmittedExec.contains(*TrueTargetResult) && !ActiveExec.contains(*TrueTargetResult))
                        {
                            auto EmitTrueResult = EmitExecNode(**TrueTargetResult);
                            if (!EmitTrueResult)
                            {
                                return EmitTrueResult;
                            }
                        }
                        if (!EmittedExec.contains(*FalseTargetResult) && !ActiveExec.contains(*FalseTargetResult))
                        {
                            return EmitExecNode(**FalseTargetResult);
                        }
                        return Ok();
                    }

                    auto EmitNodeResult = EmitNonControlNode(Builder, Asset, Slots, Variables, VariableById, Node);
                    if (!EmitNodeResult)
                    {
                        ActiveExec.erase(&Node);
                        return std::unexpected(EmitNodeResult.error());
                    }

                    EmittedExec.insert(&Node);
                    ActiveExec.erase(&Node);

                    auto NextTargetResult = ResolveExecTarget(Node, "Out");
                    if (!NextTargetResult)
                    {
                        return std::unexpected(NextTargetResult.error());
                    }
                    if (!*NextTargetResult)
                    {
                        auto AddExitJumpResult = Builder.AddJump(EntryExitLabel);
                        if (!AddExitJumpResult)
                        {
                            return std::unexpected(AddExitJumpResult.error());
                        }
                        return Ok();
                    }

                    if (EmittedExec.contains(*NextTargetResult) || ActiveExec.contains(*NextTargetResult))
                    {
                        auto AddJumpResult = Builder.AddJump(NodeLabels.at(*NextTargetResult));
                        if (!AddJumpResult)
                        {
                            return std::unexpected(AddJumpResult.error());
                        }
                        return Ok();
                    }

                    return EmitExecNode(**NextTargetResult);
                };

                auto EntryTargetResult = ResolveExecTarget(*EntryNode, "Out");
                if (!EntryTargetResult)
                {
                    return std::unexpected(EntryTargetResult.error());
                }
                if (*EntryTargetResult)
                {
                    auto EmitExecResult = EmitExecNode(**EntryTargetResult);
                    if (!EmitExecResult)
                    {
                        return std::unexpected(EmitExecResult.error());
                    }
                }

                auto MarkExitResult = Builder.MarkLabel(EntryExitLabel);
                if (!MarkExitResult)
                {
                    return std::unexpected(MarkExitResult.error());
                }
            }
        }
    }

    auto GraphResult = std::move(Builder).Build();
    if (!GraphResult)
    {
        return std::unexpected(GraphResult.error());
    }
    GraphResult->Variables = std::move(Variables);
    return std::move(*GraphResult);
}

TExpected<CompiledClass> CompileClassAsset(const ClassAsset& Asset, ::SnAPI::AssetPipeline::AssetManager& AssetManager)
{
    if (Asset.HostType == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit class host type is missing"));
    }
    if (Asset.Graph.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit class graph reference is empty"));
    }

    auto HostTypeResult = ResolveRegisteredType(Asset.HostType, "Conduit class host");
    if (!HostTypeResult)
    {
        return std::unexpected(HostTypeResult.error());
    }
    if (!TypeRegistry::Instance().IsA(Asset.HostType, StaticTypeId<BaseNode>()))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit class host type must derive from BaseNode"));
    }

    auto LoadResult = Asset.Graph.Load(AssetManager);
    if (!LoadResult)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Failed to load Conduit class graph: " + LoadResult.error()));
    }
    if (!*LoadResult)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Loaded Conduit class graph asset was null"));
    }

    GraphAsset ResolvedGraph = std::move(**LoadResult);
    if (ResolvedGraph.SelfType == TypeId{})
    {
        ResolvedGraph.SelfType = Asset.HostType;
    }
    else if (!TypeRegistry::Instance().IsA(Asset.HostType, ResolvedGraph.SelfType))
    {
        return std::unexpected(
            MakeError(EErrorCode::TypeMismatch, "Conduit class host type is incompatible with the referenced graph self type"));
    }

    auto RuntimeGraphResult = CompileGraphAsset(ResolvedGraph);
    if (!RuntimeGraphResult)
    {
        return std::unexpected(RuntimeGraphResult.error());
    }

    CompiledClass Result{};
    Result.Name = Asset.Name;
    Result.HostType = Asset.HostType;
    Result.EffectiveSelfType = ResolvedGraph.SelfType;
    Result.Graph = Asset.Graph;
    Result.SourceGraph = std::move(ResolvedGraph);
    Result.RuntimeGraph = std::move(*RuntimeGraphResult);
    return Result;
}

TExpected<void> SerializeGraphAsset(const GraphAsset& Asset, std::vector<uint8_t>& OutBytes)
{
    try
    {
        std::ostringstream Stream(std::ios::binary);
        cereal::BinaryOutputArchive Archive(Stream);
        Archive(Asset);
        const std::string Bytes = Stream.str();
        OutBytes.assign(Bytes.begin(), Bytes.end());
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
    catch (...)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Unknown exception while serializing Conduit graph asset"));
    }
}

TExpected<GraphAsset> DeserializeGraphAsset(const uint8_t* Bytes, const size_t Size)
{
    if (!Bytes && Size > 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null Conduit graph asset bytes"));
    }

    try
    {
        const std::string Data(reinterpret_cast<const char*>(Bytes), Size);
        std::istringstream Stream(Data, std::ios::binary);
        cereal::BinaryInputArchive Archive(Stream);
        GraphAsset Asset{};
        Archive(Asset);
        return Asset;
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
    }
    catch (...)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Unknown exception while deserializing Conduit graph asset"));
    }
}

TExpected<void> SerializeClassAsset(const ClassAsset& Asset, std::vector<uint8_t>& OutBytes)
{
    try
    {
        std::ostringstream Stream(std::ios::binary);
        cereal::BinaryOutputArchive Archive(Stream);
        Archive(Asset);
        const std::string Bytes = Stream.str();
        OutBytes.assign(Bytes.begin(), Bytes.end());
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
    catch (...)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Unknown exception while serializing Conduit class asset"));
    }
}

TExpected<ClassAsset> DeserializeClassAsset(const uint8_t* Bytes, const size_t Size)
{
    if (!Bytes && Size > 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Null Conduit class asset bytes"));
    }

    try
    {
        const std::string Data(reinterpret_cast<const char*>(Bytes), Size);
        std::istringstream Stream(Data, std::ios::binary);
        cereal::BinaryInputArchive Archive(Stream);
        ClassAsset Asset{};
        Archive(Asset);
        return Asset;
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
    }
    catch (...)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Unknown exception while deserializing Conduit class asset"));
    }
}

} // namespace SnAPI::GameFramework::Conduit
