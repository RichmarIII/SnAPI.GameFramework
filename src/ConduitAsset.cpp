#include "Conduit/Asset.h"

#include <cstddef>
#include <exception>
#include <unordered_map>
#include <utility>

#include "AuthoredAssetCereal.h"

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

    auto SlotResult = Builder.AddSlot(**TypeResult, ESlotKind::Value);
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
            break;
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
            break;
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
            break;
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
            break;
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
            break;
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
            break;
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
            break;
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
            break;
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
            break;
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
            break;
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
