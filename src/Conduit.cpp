#include "Conduit/Compiler.h"
#include "Conduit/Resolvers.h"

#include <algorithm>
#include <new>
#include <unordered_map>

#include "BaseComponent.h"
#include "BaseNode.h"
#include "GameThreading.h"
#include "Handles.h"

namespace SnAPI::GameFramework::Conduit
{

namespace
{

class HandleResolverRegistryStorage
{
public:
    Result Register(const TypeId& HandleType, const RegisteredHandleResolverFn Resolver)
    {
        GameLockGuard Lock(m_mutex);
        const auto It = m_resolvers.find(HandleType);
        if (It != m_resolvers.end())
        {
            if (It->second == Resolver)
            {
                return Ok();
            }
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit handle resolver already registered"));
        }
        m_resolvers.emplace(HandleType, Resolver);
        return Ok();
    }

    [[nodiscard]] RegisteredHandleResolverFn Find(const TypeId& HandleType) const
    {
        GameLockGuard Lock(m_mutex);
        const auto It = m_resolvers.find(HandleType);
        return It != m_resolvers.end() ? It->second : nullptr;
    }

private:
    mutable GameMutex m_mutex{};
    std::unordered_map<TypeId, RegisteredHandleResolverFn, UuidHash> m_resolvers{};
};

HandleResolverRegistryStorage& GetHandleResolverRegistryStorage()
{
    static HandleResolverRegistryStorage Storage{};
    return Storage;
}

template<typename THandle, typename TObject>
TExpected<ResolvedTarget> ResolveTypedHandleFamily(const TypeInfo& ExpectedType, const void* HandleValue)
{
    static_assert(std::is_same_v<TObject, BaseNode> || std::is_same_v<TObject, BaseComponent>,
                  "Conduit default handle resolver currently supports reflected node/component handle families");

    if (!HandleValue)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit handle payload is null"));
    }

    THandle Handle = *static_cast<const THandle*>(HandleValue);
    TObject* Instance = Handle.Borrowed();
    if (!Instance)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit handle target could not be resolved"));
    }

    const TypeInfo* ActualType = TypeRegistry::Instance().Find(Instance->TypeKey());
    if (!ActualType)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit resolved handle target type is not registered"));
    }
    if (!TypeRegistry::Instance().IsA(ActualType->Id, ExpectedType.Id))
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit handle target type mismatch"));
    }

    return ResolvedTarget{
        .Instance = Instance,
        .Type = ActualType,
    };
}

std::uint32_t AlignUp(const std::uint32_t Value, const std::size_t Align)
{
    const std::size_t Mask = Align - 1;
    return static_cast<std::uint32_t>((static_cast<std::size_t>(Value) + Mask) & ~Mask);
}

Result ValidateSelfContext(const ExecutionContext& Context)
{
    if (!Context.Self)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit self instance is null"));
    }
    return Ok();
}

TExpected<ResolvedTarget> ResolveSelfTarget(const ExecutionContext& Context, const TypeInfo& ExpectedType)
{
    auto ContextResult = ValidateSelfContext(Context);
    if (!ContextResult)
    {
        return std::unexpected(ContextResult.error());
    }
    if (Context.SelfType && !TypeRegistry::Instance().IsA(Context.SelfType->Id, ExpectedType.Id))
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit self target type mismatch"));
    }

    return ResolvedTarget{
        .Instance = Context.Self,
        .Type = Context.SelfType ? Context.SelfType : &ExpectedType,
    };
}

TExpected<ResolvedTarget> ResolveHandleTarget(const FrameStorage& Frame,
                                             const ExecutionContext& Context,
                                             const SlotId InstanceSlot,
                                             const TypeInfo& ExpectedType)
{
    const SlotDesc* Slot = Frame.Layout().FindSlot(InstanceSlot);
    if (!Slot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit instance slot was not found"));
    }
    if (Slot->Kind != ESlotKind::Handle)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit instance slot must be a handle slot"));
    }
    if (!Slot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit instance slot type is missing"));
    }

    auto HandleValueResult = Frame.ReadSlot(InstanceSlot);
    if (!HandleValueResult)
    {
        return std::unexpected(HandleValueResult.error());
    }

    TExpected<ResolvedTarget> TargetResult = std::unexpected(
        MakeError(EErrorCode::InvalidArgument, "Conduit handle resolver is missing"));
    if (Context.ResolveHandle)
    {
        TargetResult = Context.ResolveHandle(Context.HandleResolverUserData,
                                             ExpectedType,
                                             *Slot->Type,
                                             HandleValueResult.value());
    }
    else
    {
        RegisterBuiltinHandleResolvers();
        if (const RegisteredHandleResolverFn Resolver = HandleResolverRegistry::Instance().Find(Slot->Type->Id))
        {
            TargetResult = Resolver(ExpectedType, HandleValueResult.value());
        }
    }
    if (!TargetResult)
    {
        return std::unexpected(TargetResult.error());
    }

    ResolvedTarget Target = TargetResult.value();
    if (!Target.Instance)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit handle resolver returned null instance"));
    }
    if (!Target.Type)
    {
        Target.Type = &ExpectedType;
    }
    if (!TypeRegistry::Instance().IsA(Target.Type->Id, ExpectedType.Id))
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit resolved target type mismatch"));
    }
    return Target;
}

Result ExecuteFieldReadValue(const FieldInfo& Field, void* const Instance, const SlotId Output, FrameStorage& Frame)
{
    if (Field.ConstPointer)
    {
        const void* FieldPtr = Field.ConstPointer(Instance);
        if (FieldPtr)
        {
            return Frame.StoreCopy(Output, FieldPtr);
        }
    }

    if (Field.ViewGetter)
    {
        auto ViewResult = Field.ViewGetter(Instance);
        if (ViewResult)
        {
            const void* FieldPtr = ViewResult->UnsafeBorrowed();
            if (FieldPtr)
            {
                return Frame.StoreCopy(Output, FieldPtr);
            }
        }
    }

    if (Field.Getter)
    {
        auto FieldResult = Field.Getter(Instance);
        if (!FieldResult)
        {
            return std::unexpected(FieldResult.error());
        }

        const void* FieldPtr = FieldResult->UnsafeBorrowed();
        if (!FieldPtr)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit field getter returned no payload"));
        }
        return Frame.StoreCopy(Output, FieldPtr);
    }

    return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit field read path is unavailable"));
}

Result ExecuteFieldWriteValue(const FieldInfo& Field, void* const Instance, const SlotId Input, FrameStorage& Frame)
{
    auto InputResult = Frame.ReadSlot(Input);
    if (!InputResult)
    {
        return std::unexpected(InputResult.error());
    }

    if (Field.RawSetter)
    {
        return Field.RawSetter(Instance, InputResult.value());
    }

    if (!Field.MutablePointer)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit mutable field path is unavailable"));
    }

    void* FieldPtr = Field.MutablePointer(Instance);
    if (!FieldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit mutable field pointer is null"));
    }
    const TypeInfo* FieldType = TypeRegistry::Instance().Find(Field.FieldType);
    if (!FieldType || !FieldType->RuntimeOps || !FieldType->RuntimeOps->CopyAssign)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit field type is not copy-assignable"));
    }
    FieldType->RuntimeOps->CopyAssign(InputResult.value(), FieldPtr);
    return Ok();
}

Result ExecuteMethodCallValue(const MethodInfo& Method,
                              void* const Instance,
                              const std::span<const SlotId> Inputs,
                              const std::optional<SlotId> Output,
                              FrameStorage& Frame,
                              const std::span<void*> ScratchArgs)
{
    if (!Method.RawInvoke)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit method raw invoke path is unavailable"));
    }
    if (ScratchArgs.size() < Inputs.size())
    {
        return std::unexpected(MakeError(EErrorCode::OutOfRange, "Conduit scratch argument buffer is too small"));
    }

    for (std::size_t Index = 0; Index < Inputs.size(); ++Index)
    {
        auto ArgResult = Frame.BorrowMutableSlot(Inputs[Index]);
        if (!ArgResult)
        {
            return std::unexpected(ArgResult.error());
        }
        ScratchArgs[Index] = ArgResult.value();
    }

    void* ReturnStorage = nullptr;
    if (Output)
    {
        auto OutputResult = Frame.PrepareOutputSlot(*Output);
        if (!OutputResult)
        {
            return std::unexpected(OutputResult.error());
        }
        ReturnStorage = OutputResult.value();
    }

    auto InvokeResult = Method.RawInvoke(Method.RawInvokeUserData.get(),
                                         Instance,
                                         ScratchArgs.subspan(0, Inputs.size()),
                                         ReturnStorage);
    if (!InvokeResult)
    {
        return InvokeResult;
    }

    if (Output)
    {
        return Frame.MarkInitialized(*Output);
    }

    return Ok();
}

NodeExecuteResult ContinueExecution()
{
    return NodeExecutionControl{};
}

NodeExecuteResult JumpToNode(const std::uint32_t TargetNode)
{
    return NodeExecutionControl{
        .NextNodeIndex = TargetNode,
    };
}

template<typename T>
inline constexpr bool kSupportsNumericIntrinsics =
    std::is_same_v<T, int> ||
    std::is_same_v<T, std::int64_t> ||
    std::is_same_v<T, unsigned int> ||
    std::is_same_v<T, std::uint64_t> ||
    std::is_same_v<T, float> ||
    std::is_same_v<T, double>;

template<typename TInput, typename TResult, typename TOperation>
Result ExecuteUnaryIntrinsicImpl(const void* const Input, void* const Output, TOperation&& Operation)
{
    if (!Input || !Output)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit unary intrinsic received null storage"));
    }

    *static_cast<TResult*>(Output) = static_cast<TResult>(Operation(*static_cast<const TInput*>(Input)));
    return Ok();
}

template<typename TInput, typename TResult, typename TOperation>
Result ExecuteBinaryIntrinsicImpl(const void* const Left,
                                  const void* const Right,
                                  void* const Output,
                                  TOperation&& Operation)
{
    if (!Left || !Right || !Output)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit binary intrinsic received null storage"));
    }

    *static_cast<TResult*>(Output) = static_cast<TResult>(
        Operation(*static_cast<const TInput*>(Left), *static_cast<const TInput*>(Right)));
    return Ok();
}

template<typename T>
Result ExecuteLogicalNotIntrinsic(const void* const Input, void* const Output)
{
    return ExecuteUnaryIntrinsicImpl<T, bool>(Input, Output, [] (const T Value) { return !Value; });
}

template<typename T>
Result ExecuteNegateIntrinsic(const void* const Input, void* const Output)
{
    return ExecuteUnaryIntrinsicImpl<T, T>(Input, Output, [] (const T Value) { return -Value; });
}

template<typename T>
Result ExecuteAddIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, T>(Left, Right, Output, [] (const T L, const T R) { return L + R; });
}

template<typename T>
Result ExecuteSubtractIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, T>(Left, Right, Output, [] (const T L, const T R) { return L - R; });
}

template<typename T>
Result ExecuteMultiplyIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, T>(Left, Right, Output, [] (const T L, const T R) { return L * R; });
}

template<typename T>
Result ExecuteDivideIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    if (!Left || !Right || !Output)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit divide intrinsic received null storage"));
    }

    const T Divisor = *static_cast<const T*>(Right);
    if constexpr (std::integral<T>)
    {
        if (Divisor == 0)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit divide intrinsic divisor is zero"));
        }
    }

    *static_cast<T*>(Output) = *static_cast<const T*>(Left) / Divisor;
    return Ok();
}

template<typename T>
Result ExecuteEqualIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, bool>(Left, Right, Output, [] (const T& L, const T& R) { return L == R; });
}

template<typename T>
Result ExecuteNotEqualIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, bool>(Left, Right, Output, [] (const T& L, const T& R) { return L != R; });
}

template<typename T>
Result ExecuteLessIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, bool>(Left, Right, Output, [] (const T L, const T R) { return L < R; });
}

template<typename T>
Result ExecuteLessEqualIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, bool>(Left, Right, Output, [] (const T L, const T R) { return L <= R; });
}

template<typename T>
Result ExecuteGreaterIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, bool>(Left, Right, Output, [] (const T L, const T R) { return L > R; });
}

template<typename T>
Result ExecuteGreaterEqualIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, bool>(Left, Right, Output, [] (const T L, const T R) { return L >= R; });
}

template<typename T>
Result ExecuteLogicalAndIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, bool>(Left, Right, Output, [] (const T L, const T R) { return L && R; });
}

template<typename T>
Result ExecuteLogicalOrIntrinsic(const void* const Left, const void* const Right, void* const Output)
{
    return ExecuteBinaryIntrinsicImpl<T, bool>(Left, Right, Output, [] (const T L, const T R) { return L || R; });
}

template<typename T>
UnaryIntrinsicFn ResolveUnaryIntrinsicForType(const EUnaryIntrinsicOp Op, const TypeInfo& OutputType)
{
    switch (Op)
    {
    case EUnaryIntrinsicOp::LogicalNot:
        if constexpr (std::is_same_v<T, bool>)
        {
            if (OutputType.Id == StaticTypeId<bool>())
            {
                return &ExecuteLogicalNotIntrinsic<T>;
            }
        }
        break;
    case EUnaryIntrinsicOp::Negate:
        if constexpr ((std::is_same_v<T, int> || std::is_same_v<T, std::int64_t> ||
                       std::is_same_v<T, float> || std::is_same_v<T, double>) &&
                      !std::is_same_v<T, bool>)
        {
            if (OutputType.Id == StaticTypeId<T>())
            {
                return &ExecuteNegateIntrinsic<T>;
            }
        }
        break;
    }

    return nullptr;
}

UnaryIntrinsicFn ResolveUnaryIntrinsicFn(const EUnaryIntrinsicOp Op, const TypeInfo& InputType, const TypeInfo& OutputType)
{
    if (InputType.Id == StaticTypeId<bool>())
    {
        return ResolveUnaryIntrinsicForType<bool>(Op, OutputType);
    }
    if (InputType.Id == StaticTypeId<int>())
    {
        return ResolveUnaryIntrinsicForType<int>(Op, OutputType);
    }
    if (InputType.Id == StaticTypeId<std::int64_t>())
    {
        return ResolveUnaryIntrinsicForType<std::int64_t>(Op, OutputType);
    }
    if (InputType.Id == StaticTypeId<float>())
    {
        return ResolveUnaryIntrinsicForType<float>(Op, OutputType);
    }
    if (InputType.Id == StaticTypeId<double>())
    {
        return ResolveUnaryIntrinsicForType<double>(Op, OutputType);
    }
    return nullptr;
}

template<typename T>
BinaryIntrinsicFn ResolveBinaryIntrinsicForType(const EBinaryIntrinsicOp Op, const TypeInfo& OutputType)
{
    switch (Op)
    {
    case EBinaryIntrinsicOp::Add:
        if constexpr (kSupportsNumericIntrinsics<T>)
        {
            return OutputType.Id == StaticTypeId<T>() ? &ExecuteAddIntrinsic<T> : nullptr;
        }
        return nullptr;
    case EBinaryIntrinsicOp::Subtract:
        if constexpr (kSupportsNumericIntrinsics<T>)
        {
            return OutputType.Id == StaticTypeId<T>() ? &ExecuteSubtractIntrinsic<T> : nullptr;
        }
        return nullptr;
    case EBinaryIntrinsicOp::Multiply:
        if constexpr (kSupportsNumericIntrinsics<T>)
        {
            return OutputType.Id == StaticTypeId<T>() ? &ExecuteMultiplyIntrinsic<T> : nullptr;
        }
        return nullptr;
    case EBinaryIntrinsicOp::Divide:
        if constexpr (kSupportsNumericIntrinsics<T>)
        {
            return OutputType.Id == StaticTypeId<T>() ? &ExecuteDivideIntrinsic<T> : nullptr;
        }
        return nullptr;
    case EBinaryIntrinsicOp::Equal:
        return OutputType.Id == StaticTypeId<bool>() ? &ExecuteEqualIntrinsic<T> : nullptr;
    case EBinaryIntrinsicOp::NotEqual:
        return OutputType.Id == StaticTypeId<bool>() ? &ExecuteNotEqualIntrinsic<T> : nullptr;
    case EBinaryIntrinsicOp::Less:
        if constexpr (kSupportsNumericIntrinsics<T>)
        {
            return OutputType.Id == StaticTypeId<bool>() ? &ExecuteLessIntrinsic<T> : nullptr;
        }
        return nullptr;
    case EBinaryIntrinsicOp::LessEqual:
        if constexpr (kSupportsNumericIntrinsics<T>)
        {
            return OutputType.Id == StaticTypeId<bool>() ? &ExecuteLessEqualIntrinsic<T> : nullptr;
        }
        return nullptr;
    case EBinaryIntrinsicOp::Greater:
        if constexpr (kSupportsNumericIntrinsics<T>)
        {
            return OutputType.Id == StaticTypeId<bool>() ? &ExecuteGreaterIntrinsic<T> : nullptr;
        }
        return nullptr;
    case EBinaryIntrinsicOp::GreaterEqual:
        if constexpr (kSupportsNumericIntrinsics<T>)
        {
            return OutputType.Id == StaticTypeId<bool>() ? &ExecuteGreaterEqualIntrinsic<T> : nullptr;
        }
        return nullptr;
    case EBinaryIntrinsicOp::LogicalAnd:
        if constexpr (std::is_same_v<T, bool>)
        {
            return OutputType.Id == StaticTypeId<bool>() ? &ExecuteLogicalAndIntrinsic<T> : nullptr;
        }
        return nullptr;
    case EBinaryIntrinsicOp::LogicalOr:
        if constexpr (std::is_same_v<T, bool>)
        {
            return OutputType.Id == StaticTypeId<bool>() ? &ExecuteLogicalOrIntrinsic<T> : nullptr;
        }
        return nullptr;
    }

    return nullptr;
}

BinaryIntrinsicFn ResolveBinaryIntrinsicFn(const EBinaryIntrinsicOp Op,
                                           const TypeInfo& LeftType,
                                           const TypeInfo& RightType,
                                           const TypeInfo& OutputType)
{
    if (LeftType.Id != RightType.Id)
    {
        return nullptr;
    }

    if (LeftType.Id == StaticTypeId<bool>())
    {
        return ResolveBinaryIntrinsicForType<bool>(Op, OutputType);
    }
    if (LeftType.Id == StaticTypeId<int>())
    {
        return ResolveBinaryIntrinsicForType<int>(Op, OutputType);
    }
    if (LeftType.Id == StaticTypeId<std::int64_t>())
    {
        return ResolveBinaryIntrinsicForType<std::int64_t>(Op, OutputType);
    }
    if (LeftType.Id == StaticTypeId<unsigned int>())
    {
        return ResolveBinaryIntrinsicForType<unsigned int>(Op, OutputType);
    }
    if (LeftType.Id == StaticTypeId<std::uint64_t>())
    {
        return ResolveBinaryIntrinsicForType<std::uint64_t>(Op, OutputType);
    }
    if (LeftType.Id == StaticTypeId<float>())
    {
        return ResolveBinaryIntrinsicForType<float>(Op, OutputType);
    }
    if (LeftType.Id == StaticTypeId<double>())
    {
        return ResolveBinaryIntrinsicForType<double>(Op, OutputType);
    }
    return nullptr;
}

} // namespace

HandleResolverRegistry& HandleResolverRegistry::Instance()
{
    static HandleResolverRegistry Instance{};
    return Instance;
}

Result HandleResolverRegistry::Register(const TypeId& HandleType, const RegisteredHandleResolverFn Resolver)
{
    return GetHandleResolverRegistryStorage().Register(HandleType, Resolver);
}

RegisteredHandleResolverFn HandleResolverRegistry::Find(const TypeId& HandleType) const
{
    return GetHandleResolverRegistryStorage().Find(HandleType);
}

void RegisterBuiltinHandleResolvers()
{
    static const bool Registered = [] {
        auto& Registry = HandleResolverRegistry::Instance();
        (void)Registry.Register<NodeHandle>(&ResolveTypedHandleFamily<NodeHandle, BaseNode>);
        (void)Registry.Register<ComponentHandle>(&ResolveTypedHandleFamily<ComponentHandle, BaseComponent>);
        return true;
    }();
    (void)Registered;
}

const SlotDesc* FrameLayout::FindSlot(const SlotId Id) const
{
    if (!Id.IsValid() || Id.Value >= Slots.size())
    {
        return nullptr;
    }
    return &Slots[Id.Value];
}

TExpected<SlotId> FrameLayout::AddSlot(const TypeInfo& Type, const ESlotKind Kind)
{
    if (!Type.RuntimeOps)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit slot type has no runtime ops"));
    }
    if (Type.Align == 0 || Type.Size == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit slot type must have concrete size/alignment"));
    }

    SlotDesc Slot;
    Slot.Id = SlotId{static_cast<std::uint32_t>(Slots.size())};
    Slot.Type = &Type;
    Slot.Size = static_cast<std::uint32_t>(Type.Size);
    Slot.Align = static_cast<std::uint16_t>(Type.Align);
    Slot.Kind = Kind;
    Slot.Offset = AlignUp(TotalSize, Type.Align);

    TotalSize = Slot.Offset + Slot.Size;
    MaxAlign = std::max(MaxAlign, Type.Align);
    Slots.push_back(Slot);
    return Slot.Id;
}

FrameStorage::FrameStorage(const FrameLayout& Layout)
    : m_layout(&Layout)
    , m_initialized(Layout.Slots.size(), 0)
{
    if (Layout.TotalSize > 0)
    {
        m_storage = ::operator new(Layout.TotalSize, std::align_val_t(Layout.MaxAlign));
    }
}

FrameStorage::~FrameStorage()
{
    if (m_layout)
    {
        for (std::size_t Index = 0; Index < m_layout->Slots.size(); ++Index)
        {
            if (m_initialized[Index] == 0)
            {
                continue;
            }
            const SlotDesc& Slot = m_layout->Slots[Index];
            if (Slot.Type && Slot.Type->RuntimeOps && Slot.Type->RuntimeOps->Destroy)
            {
                Slot.Type->RuntimeOps->Destroy(RawSlotStorage(Slot));
            }
        }
    }

    if (m_storage)
    {
        ::operator delete(m_storage, std::align_val_t(m_layout ? m_layout->MaxAlign : alignof(std::max_align_t)));
    }
}

bool FrameStorage::IsInitialized(const SlotId Id) const
{
    const SlotDesc* Slot = RequireSlot(Id);
    if (!Slot)
    {
        return false;
    }
    return m_initialized[Slot->Id.Value] != 0;
}

Result FrameStorage::ResetSlot(const SlotId Id)
{
    const SlotDesc* Slot = RequireSlot(Id);
    if (!Slot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit slot not found"));
    }
    if (m_initialized[Slot->Id.Value] != 0 && Slot->Type && Slot->Type->RuntimeOps && Slot->Type->RuntimeOps->Destroy)
    {
        Slot->Type->RuntimeOps->Destroy(RawSlotStorage(*Slot));
        m_initialized[Slot->Id.Value] = 0;
    }
    return Ok();
}

Result FrameStorage::StoreCopy(const SlotId Id, const void* Source)
{
    const SlotDesc* Slot = RequireSlot(Id);
    if (!Slot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit slot not found"));
    }
    if (!Source)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit source pointer is null"));
    }
    if (!Slot->Type || !Slot->Type->RuntimeOps || !Slot->Type->RuntimeOps->CopyConstruct)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit slot type is not copy-constructible"));
    }

    ResetSlot(Id);
    Slot->Type->RuntimeOps->CopyConstruct(Source, RawSlotStorage(*Slot));
    m_initialized[Slot->Id.Value] = 1;
    return Ok();
}

Result FrameStorage::DefaultConstructSlot(const SlotId Id)
{
    const SlotDesc* Slot = RequireSlot(Id);
    if (!Slot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit slot not found"));
    }
    if (!Slot->Type || !Slot->Type->RuntimeOps || !Slot->Type->RuntimeOps->DefaultConstruct)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit slot type is not default-constructible"));
    }

    auto OutputResult = PrepareOutputSlot(Id);
    if (!OutputResult)
    {
        return std::unexpected(OutputResult.error());
    }
    Slot->Type->RuntimeOps->DefaultConstruct(OutputResult.value());
    return MarkInitialized(Id);
}

TExpected<const void*> FrameStorage::ReadSlot(const SlotId Id) const
{
    const SlotDesc* Slot = RequireSlot(Id);
    if (!Slot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit slot not found"));
    }
    if (m_initialized[Slot->Id.Value] == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Conduit slot is uninitialized"));
    }
    return RawSlotStorage(*Slot);
}

TExpected<void*> FrameStorage::BorrowMutableSlot(const SlotId Id)
{
    const SlotDesc* Slot = RequireSlot(Id);
    if (!Slot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit slot not found"));
    }
    if (m_initialized[Slot->Id.Value] == 0)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Conduit slot is uninitialized"));
    }
    return RawSlotStorage(*Slot);
}

TExpected<void*> FrameStorage::PrepareOutputSlot(const SlotId Id)
{
    auto ResetResult = ResetSlot(Id);
    if (!ResetResult)
    {
        return std::unexpected(ResetResult.error());
    }
    const SlotDesc* Slot = RequireSlot(Id);
    if (!Slot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit slot not found"));
    }
    return RawSlotStorage(*Slot);
}

Result FrameStorage::MarkInitialized(const SlotId Id)
{
    const SlotDesc* Slot = RequireSlot(Id);
    if (!Slot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit slot not found"));
    }
    m_initialized[Slot->Id.Value] = 1;
    return Ok();
}

const SlotDesc* FrameStorage::RequireSlot(const SlotId Id) const
{
    return m_layout ? m_layout->FindSlot(Id) : nullptr;
}

void* FrameStorage::RawSlotStorage(const SlotDesc& Slot)
{
    return static_cast<std::byte*>(m_storage) + Slot.Offset;
}

const void* FrameStorage::RawSlotStorage(const SlotDesc& Slot) const
{
    return static_cast<const std::byte*>(m_storage) + Slot.Offset;
}

NodeExecuteResult ExecuteConstantNode(const NodeData& Data,
                                      FrameStorage& Frame,
                                      const ExecutionContext& Context,
                                      const std::span<void*> ScratchArgs)
{
    (void)Context;
    (void)ScratchArgs;

    const auto& Node = std::get<ConstantNodeData>(Data);
    if (!Node.Type)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit constant type is missing"));
    }

    auto OutputResult = Frame.PrepareOutputSlot(Node.Output);
    if (!OutputResult)
    {
        return std::unexpected(OutputResult.error());
    }

    auto ConstructResult = Node.Value.ConstructInto(OutputResult.value());
    if (!ConstructResult)
    {
        return std::unexpected(ConstructResult.error());
    }

    auto MarkResult = Frame.MarkInitialized(Node.Output);
    if (!MarkResult)
    {
        return std::unexpected(MarkResult.error());
    }
    return ContinueExecution();
}

NodeExecuteResult ExecuteSlotCopyNode(const NodeData& Data,
                                      FrameStorage& Frame,
                                      const ExecutionContext& Context,
                                      const std::span<void*> ScratchArgs)
{
    (void)Context;
    (void)ScratchArgs;

    const auto& Node = std::get<SlotCopyNodeData>(Data);
    if (!Node.Type)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit slot-copy type is missing"));
    }

    auto SourceResult = Frame.ReadSlot(Node.Source);
    if (!SourceResult)
    {
        return std::unexpected(SourceResult.error());
    }
    if (const Result StoreResult = Frame.StoreCopy(Node.Destination, SourceResult.value()); !StoreResult)
    {
        return std::unexpected(StoreResult.error());
    }
    return ContinueExecution();
}

NodeExecuteResult ExecuteUnaryIntrinsicNode(const NodeData& Data,
                                            FrameStorage& Frame,
                                            const ExecutionContext& Context,
                                            const std::span<void*> ScratchArgs)
{
    (void)Context;
    (void)ScratchArgs;

    const auto& Node = std::get<UnaryIntrinsicNodeData>(Data);
    if (!Node.ExecuteIntrinsic)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit unary intrinsic is unresolved"));
    }

    auto InputResult = Frame.ReadSlot(Node.Input);
    if (!InputResult)
    {
        return std::unexpected(InputResult.error());
    }

    auto OutputResult = Frame.PrepareOutputSlot(Node.Output);
    if (!OutputResult)
    {
        return std::unexpected(OutputResult.error());
    }

    auto ExecuteResult = Node.ExecuteIntrinsic(InputResult.value(), OutputResult.value());
    if (!ExecuteResult)
    {
        return std::unexpected(ExecuteResult.error());
    }

    auto MarkResult = Frame.MarkInitialized(Node.Output);
    if (!MarkResult)
    {
        return std::unexpected(MarkResult.error());
    }

    return ContinueExecution();
}

NodeExecuteResult ExecuteBinaryIntrinsicNode(const NodeData& Data,
                                             FrameStorage& Frame,
                                             const ExecutionContext& Context,
                                             const std::span<void*> ScratchArgs)
{
    (void)Context;
    (void)ScratchArgs;

    const auto& Node = std::get<BinaryIntrinsicNodeData>(Data);

    auto LeftResult = Frame.ReadSlot(Node.Left);
    if (!LeftResult)
    {
        return std::unexpected(LeftResult.error());
    }
    auto RightResult = Frame.ReadSlot(Node.Right);
    if (!RightResult)
    {
        return std::unexpected(RightResult.error());
    }
    auto OutputResult = Frame.PrepareOutputSlot(Node.Output);
    if (!OutputResult)
    {
        return std::unexpected(OutputResult.error());
    }

    Result ExecuteResult = std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit binary intrinsic is unresolved"));
    if (Node.ExecuteIntrinsic)
    {
        ExecuteResult = Node.ExecuteIntrinsic(LeftResult.value(), RightResult.value(), OutputResult.value());
    }
    else if ((Node.Op == EBinaryIntrinsicOp::Equal || Node.Op == EBinaryIntrinsicOp::NotEqual) &&
             Node.ValueType &&
             Node.ValueType->RuntimeOps &&
             Node.ValueType->RuntimeOps->Equals)
    {
        const bool IsEqual = Node.ValueType->RuntimeOps->Equals(LeftResult.value(), RightResult.value());
        *static_cast<bool*>(OutputResult.value()) = Node.Op == EBinaryIntrinsicOp::Equal ? IsEqual : !IsEqual;
        ExecuteResult = Ok();
    }

    if (!ExecuteResult)
    {
        return std::unexpected(ExecuteResult.error());
    }

    auto MarkResult = Frame.MarkInitialized(Node.Output);
    if (!MarkResult)
    {
        return std::unexpected(MarkResult.error());
    }

    return ContinueExecution();
}

NodeExecuteResult ExecuteJumpNode(const NodeData& Data,
                                  FrameStorage& Frame,
                                  const ExecutionContext& Context,
                                  const std::span<void*> ScratchArgs)
{
    (void)Frame;
    (void)Context;
    (void)ScratchArgs;

    const auto& Node = std::get<JumpNodeData>(Data);
    if (Node.TargetNode == JumpNodeData::InvalidTarget)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit jump target is unresolved"));
    }
    return JumpToNode(Node.TargetNode);
}

NodeExecuteResult ExecuteBranchNode(const NodeData& Data,
                                    FrameStorage& Frame,
                                    const ExecutionContext& Context,
                                    const std::span<void*> ScratchArgs)
{
    (void)Context;
    (void)ScratchArgs;

    const auto& Node = std::get<BranchNodeData>(Data);
    if (Node.TrueTarget == BranchNodeData::InvalidTarget || Node.FalseTarget == BranchNodeData::InvalidTarget)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit branch targets are unresolved"));
    }

    auto ConditionResult = Frame.AsConstRef<bool>(Node.Condition);
    if (!ConditionResult)
    {
        return std::unexpected(ConditionResult.error());
    }

    return JumpToNode(ConditionResult->get() ? Node.TrueTarget : Node.FalseTarget);
}

NodeExecuteResult ExecuteSelfFieldReadNode(const NodeData& Data,
                                           FrameStorage& Frame,
                                           const ExecutionContext& Context,
                                           const std::span<void*> ScratchArgs)
{
    (void)ScratchArgs;

    const auto& Node = std::get<SelfFieldReadNodeData>(Data);
    if (!Node.Field || !Node.OwnerType)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit self field binding is missing"));
    }
    auto TargetResult = ResolveSelfTarget(Context, *Node.OwnerType);
    if (!TargetResult)
    {
        return std::unexpected(TargetResult.error());
    }
    auto ExecuteResult = ExecuteFieldReadValue(*Node.Field, TargetResult->Instance, Node.Output, Frame);
    if (!ExecuteResult)
    {
        return std::unexpected(ExecuteResult.error());
    }
    return ContinueExecution();
}

NodeExecuteResult ExecuteSelfFieldWriteNode(const NodeData& Data,
                                            FrameStorage& Frame,
                                            const ExecutionContext& Context,
                                            const std::span<void*> ScratchArgs)
{
    (void)ScratchArgs;

    const auto& Node = std::get<SelfFieldWriteNodeData>(Data);
    if (!Node.Field || !Node.OwnerType)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit self field binding is missing"));
    }
    auto TargetResult = ResolveSelfTarget(Context, *Node.OwnerType);
    if (!TargetResult)
    {
        return std::unexpected(TargetResult.error());
    }
    auto ExecuteResult = ExecuteFieldWriteValue(*Node.Field, TargetResult->Instance, Node.Input, Frame);
    if (!ExecuteResult)
    {
        return std::unexpected(ExecuteResult.error());
    }
    return ContinueExecution();
}

NodeExecuteResult ExecuteSelfMethodCallNode(const NodeData& Data,
                                            FrameStorage& Frame,
                                            const ExecutionContext& Context,
                                            const std::span<void*> ScratchArgs)
{
    const auto& Node = std::get<SelfMethodCallNodeData>(Data);
    if (!Node.Method || !Node.OwnerType)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit self method binding is missing"));
    }
    auto TargetResult = ResolveSelfTarget(Context, *Node.OwnerType);
    if (!TargetResult)
    {
        return std::unexpected(TargetResult.error());
    }
    auto ExecuteResult = ExecuteMethodCallValue(*Node.Method, TargetResult->Instance, Node.Inputs, Node.Output, Frame, ScratchArgs);
    if (!ExecuteResult)
    {
        return std::unexpected(ExecuteResult.error());
    }
    return ContinueExecution();
}

NodeExecuteResult ExecuteInstanceFieldReadNode(const NodeData& Data,
                                               FrameStorage& Frame,
                                               const ExecutionContext& Context,
                                               const std::span<void*> ScratchArgs)
{
    (void)ScratchArgs;

    const auto& Node = std::get<InstanceFieldReadNodeData>(Data);
    if (!Node.Field || !Node.OwnerType)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit instance field binding is missing"));
    }

    auto TargetResult = ResolveHandleTarget(Frame, Context, Node.Instance, *Node.OwnerType);
    if (!TargetResult)
    {
        return std::unexpected(TargetResult.error());
    }
    auto ExecuteResult = ExecuteFieldReadValue(*Node.Field, TargetResult->Instance, Node.Output, Frame);
    if (!ExecuteResult)
    {
        return std::unexpected(ExecuteResult.error());
    }
    return ContinueExecution();
}

NodeExecuteResult ExecuteInstanceFieldWriteNode(const NodeData& Data,
                                                FrameStorage& Frame,
                                                const ExecutionContext& Context,
                                                const std::span<void*> ScratchArgs)
{
    (void)ScratchArgs;

    const auto& Node = std::get<InstanceFieldWriteNodeData>(Data);
    if (!Node.Field || !Node.OwnerType)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit instance field binding is missing"));
    }

    auto TargetResult = ResolveHandleTarget(Frame, Context, Node.Instance, *Node.OwnerType);
    if (!TargetResult)
    {
        return std::unexpected(TargetResult.error());
    }
    auto ExecuteResult = ExecuteFieldWriteValue(*Node.Field, TargetResult->Instance, Node.Input, Frame);
    if (!ExecuteResult)
    {
        return std::unexpected(ExecuteResult.error());
    }
    return ContinueExecution();
}

NodeExecuteResult ExecuteInstanceMethodCallNode(const NodeData& Data,
                                                FrameStorage& Frame,
                                                const ExecutionContext& Context,
                                                const std::span<void*> ScratchArgs)
{
    const auto& Node = std::get<InstanceMethodCallNodeData>(Data);
    if (!Node.Method || !Node.OwnerType)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit instance method binding is missing"));
    }

    auto TargetResult = ResolveHandleTarget(Frame, Context, Node.Instance, *Node.OwnerType);
    if (!TargetResult)
    {
        return std::unexpected(TargetResult.error());
    }
    auto ExecuteResult = ExecuteMethodCallValue(*Node.Method, TargetResult->Instance, Node.Inputs, Node.Output, Frame, ScratchArgs);
    if (!ExecuteResult)
    {
        return std::unexpected(ExecuteResult.error());
    }
    return ContinueExecution();
}

Result ExecuteCompiledGraphRange(const std::vector<BoundNode>& Nodes,
                                 const std::uint32_t StartNodeIndex,
                                 const std::uint32_t EndNodeIndex,
                                 FrameStorage& Frame,
                                 const ExecutionContext& Context,
                                 const std::span<void*> ScratchArgs)
{
    if (StartNodeIndex > EndNodeIndex || EndNodeIndex > Nodes.size())
    {
        return std::unexpected(MakeError(EErrorCode::OutOfRange, "Conduit entrypoint range is invalid"));
    }

    std::size_t NodeIndex = StartNodeIndex;
    std::size_t ExecutedNodes = 0;

    while (NodeIndex < EndNodeIndex)
    {
        if (ExecutedNodes >= Context.MaxNodeExecutions)
        {
            return std::unexpected(MakeError(EErrorCode::OutOfRange, "Conduit graph exceeded node execution limit"));
        }

        const BoundNode& Node = Nodes[NodeIndex];
        if (!Node.Execute)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit node execute callback is missing"));
        }

        auto ExecuteResult = Node.Execute(Node.Data, Frame, Context, ScratchArgs);
        if (!ExecuteResult)
        {
            return std::unexpected(ExecuteResult.error());
        }

        ++ExecutedNodes;
        if (ExecuteResult->NextNodeIndex)
        {
            if (*ExecuteResult->NextNodeIndex > Nodes.size())
            {
                return std::unexpected(MakeError(EErrorCode::OutOfRange, "Conduit node jumped outside the graph"));
            }
            if (*ExecuteResult->NextNodeIndex < StartNodeIndex || *ExecuteResult->NextNodeIndex > EndNodeIndex)
            {
                return std::unexpected(MakeError(EErrorCode::OutOfRange, "Conduit node jumped outside its entrypoint region"));
            }
            NodeIndex = *ExecuteResult->NextNodeIndex;
            continue;
        }

        ++NodeIndex;
    }

    return Ok();
}

const GraphEntryPoint* CompiledGraph::FindEntryPoint(const std::string_view Name) const
{
    const auto It = std::find_if(EntryPoints.begin(), EntryPoints.end(), [&Name](const GraphEntryPoint& EntryPoint) {
        return EntryPoint.Name == Name;
    });
    return It != EntryPoints.end() ? &*It : nullptr;
}

const GraphEntryPoint* CompiledGraph::FindEntryPoint(const EBuiltinEntryPoint Builtin) const
{
    const auto It = std::find_if(EntryPoints.begin(), EntryPoints.end(), [Builtin](const GraphEntryPoint& EntryPoint) {
        return EntryPoint.Builtin == Builtin;
    });
    return It != EntryPoints.end() ? &*It : nullptr;
}

const CompiledGraphVariable* CompiledGraph::FindVariable(const std::string_view Name) const
{
    const auto It = std::find_if(Variables.begin(), Variables.end(), [&Name](const CompiledGraphVariable& Variable) {
        return Variable.Name == Name;
    });
    return It != Variables.end() ? &*It : nullptr;
}

Result CompiledGraph::Execute(FrameStorage& Frame, const ExecutionContext& Context, std::span<void*> ScratchArgs) const
{
    if (!EntryPoints.empty())
    {
        return ExecuteCompiledGraphRange(Nodes,
                                         EntryPoints.front().StartNodeIndex,
                                         EntryPoints.front().EndNodeIndex,
                                         Frame,
                                         Context,
                                         ScratchArgs);
    }

    return ExecuteCompiledGraphRange(Nodes,
                                     0,
                                     static_cast<std::uint32_t>(Nodes.size()),
                                     Frame,
                                     Context,
                                     ScratchArgs);
}

Result CompiledGraph::ExecuteEntry(const std::string_view Name,
                                   FrameStorage& Frame,
                                   const ExecutionContext& Context,
                                   std::span<void*> ScratchArgs) const
{
    const GraphEntryPoint* EntryPoint = FindEntryPoint(Name);
    if (!EntryPoint)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit entrypoint was not found"));
    }

    return ExecuteCompiledGraphRange(Nodes,
                                     EntryPoint->StartNodeIndex,
                                     EntryPoint->EndNodeIndex,
                                     Frame,
                                     Context,
                                     ScratchArgs);
}

Result CompiledGraph::ExecuteEntry(const EBuiltinEntryPoint Builtin,
                                   FrameStorage& Frame,
                                   const ExecutionContext& Context,
                                   std::span<void*> ScratchArgs) const
{
    const GraphEntryPoint* EntryPoint = FindEntryPoint(Builtin);
    if (!EntryPoint)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit built-in entrypoint was not found"));
    }

    return ExecuteCompiledGraphRange(Nodes,
                                     EntryPoint->StartNodeIndex,
                                     EntryPoint->EndNodeIndex,
                                     Frame,
                                     Context,
                                     ScratchArgs);
}

GraphInstance::GraphInstance(const CompiledGraph& Graph)
    : m_graph(&Graph)
    , m_frame(Graph.Layout)
    , m_scratchArgs(Graph.MaxScratchArgs)
{
    for (const CompiledGraphVariable& Variable : Graph.Variables)
    {
        Result InitResult = Ok();
        if (Variable.DefaultValue.Type != TypeId{})
        {
            auto OutputResult = m_frame.PrepareOutputSlot(Variable.Slot);
            if (!OutputResult)
            {
                InitResult = std::unexpected(OutputResult.error());
            }
            else if (const auto ConstructResult = Variable.DefaultValue.ConstructInto(OutputResult.value()); !ConstructResult)
            {
                InitResult = std::unexpected(ConstructResult.error());
            }
            else
            {
                InitResult = m_frame.MarkInitialized(Variable.Slot);
            }
        }
        else
        {
            InitResult = m_frame.DefaultConstructSlot(Variable.Slot);
        }

        if (!InitResult)
        {
            m_initializationError = InitResult.error();
            break;
        }
    }
}

Result GraphInstance::Execute(const ExecutionContext& Context)
{
    if (m_initializationError)
    {
        return std::unexpected(*m_initializationError);
    }
    return m_graph->Execute(m_frame, Context, m_scratchArgs);
}

Result GraphInstance::ExecuteEntry(const std::string_view Name, const ExecutionContext& Context)
{
    if (m_initializationError)
    {
        return std::unexpected(*m_initializationError);
    }
    return m_graph->ExecuteEntry(Name, m_frame, Context, m_scratchArgs);
}

Result GraphInstance::ExecuteEntry(const EBuiltinEntryPoint Builtin, const ExecutionContext& Context)
{
    if (m_initializationError)
    {
        return std::unexpected(*m_initializationError);
    }
    return m_graph->ExecuteEntry(Builtin, m_frame, Context, m_scratchArgs);
}

GraphBuilder::GraphBuilder(const TypeInfo& SelfType)
    : m_selfType(&SelfType)
{
}

TExpected<SlotId> GraphBuilder::AddSlot(const TypeInfo& Type, const ESlotKind Kind)
{
    return m_layout.AddSlot(Type, Kind);
}

TExpected<SlotId> GraphBuilder::AddSlot(const TypeId& Type, const ESlotKind Kind)
{
    const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit slot type is not registered"));
    }
    return AddSlot(*Info, Kind);
}

LabelId GraphBuilder::CreateLabel()
{
    const LabelId Label{static_cast<std::uint32_t>(m_labels.size())};
    m_labels.push_back(std::nullopt);
    return Label;
}

Result GraphBuilder::MarkLabel(const LabelId Label)
{
    auto LabelResult = ValidateLabel(Label);
    if (!LabelResult)
    {
        return LabelResult;
    }
    if (m_labels[Label.Value].has_value())
    {
        return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit label is already bound"));
    }

    m_labels[Label.Value] = static_cast<std::uint32_t>(m_nodes.size());
    return Ok();
}

Result GraphBuilder::AddEntryPoint(const std::string_view Name,
                                   const EBuiltinEntryPoint Builtin,
                                   const SlotId DeltaSecondsSlot)
{
    auto NameResult = ValidateEntryPointName(Name, Builtin);
    if (!NameResult)
    {
        return NameResult;
    }

    if (Builtin == EBuiltinEntryPoint::None)
    {
        if (DeltaSecondsSlot.IsValid())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Conduit custom entrypoints cannot declare a delta-seconds slot"));
        }
    }
    else if (BuiltinEntryPointUsesDeltaSeconds(Builtin))
    {
        if (!DeltaSecondsSlot.IsValid())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Conduit tick-like built-in entrypoints require a float delta-seconds slot"));
        }

        const SlotDesc* DeltaSlot = FindSlot(DeltaSecondsSlot);
        if (!DeltaSlot || !DeltaSlot->Type)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit delta-seconds slot was not found"));
        }
        if (DeltaSlot->Kind != ESlotKind::Value || DeltaSlot->Type->Id != StaticTypeId<float>())
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch,
                                             "Conduit delta-seconds slot must be a float value slot"));
        }
    }
    else if (DeltaSecondsSlot.IsValid())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Conduit non-tick built-in entrypoints cannot declare a delta-seconds slot"));
    }

    EntryPointDef EntryPoint{};
    EntryPoint.Name = std::string(Builtin == EBuiltinEntryPoint::None ? Name : BuiltinEntryPointName(Builtin));
    EntryPoint.Builtin = Builtin;
    EntryPoint.StartNodeIndex = static_cast<std::uint32_t>(m_nodes.size());
    EntryPoint.DeltaSecondsSlot = DeltaSecondsSlot;
    m_entryPoints.push_back(std::move(EntryPoint));
    return Ok();
}

Result GraphBuilder::AddConstant(const SlotId Output, Variant Value)
{
    auto SerializedResult = SerializedValue::FromVariant(Value);
    if (!SerializedResult)
    {
        return std::unexpected(SerializedResult.error());
    }
    return AddSerializedConstant(Output, std::move(*SerializedResult));
}

Result GraphBuilder::AddSerializedConstant(const SlotId Output, SerializedValue Value)
{
    const SlotDesc* Slot = FindSlot(Output);
    if (!Slot || !Slot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit output slot was not found"));
    }
    if (Value.Type != Slot->Type->Id)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit constant type mismatch"));
    }
    if (!Slot->Type->RuntimeOps)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit constant slot type has no runtime ops"));
    }

    BoundNode Node;
    Node.Kind = ENodeKind::Constant;
    Node.Execute = &ExecuteConstantNode;
    Node.Data = ConstantNodeData{.Output = Output, .Type = Slot->Type, .Value = std::move(Value)};
    m_nodes.push_back(std::move(Node));
    return Ok();
}

Result GraphBuilder::AddCopy(const SlotId Source, const SlotId Destination)
{
    if (Source == Destination)
    {
        return Ok();
    }

    const SlotDesc* SourceSlot = FindSlot(Source);
    const SlotDesc* DestinationSlot = FindSlot(Destination);
    if (!SourceSlot || !SourceSlot->Type || !DestinationSlot || !DestinationSlot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit slot-copy metadata is missing"));
    }
    if (SourceSlot->Kind != DestinationSlot->Kind)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit slot-copy storage-kind mismatch"));
    }
    if (SourceSlot->Type->Id != DestinationSlot->Type->Id)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit slot-copy type mismatch"));
    }
    if (!DestinationSlot->Type->RuntimeOps || !DestinationSlot->Type->RuntimeOps->CopyConstruct)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit slot-copy destination is not copy-constructible"));
    }

    BoundNode Node;
    Node.Kind = ENodeKind::SlotCopy;
    Node.Execute = &ExecuteSlotCopyNode;
    Node.Data = SlotCopyNodeData{
        .Source = Source,
        .Destination = Destination,
        .Type = SourceSlot->Type,
    };
    m_nodes.push_back(std::move(Node));
    return Ok();
}

Result GraphBuilder::AddUnaryIntrinsic(const EUnaryIntrinsicOp Op, const SlotId Input, const SlotId Output)
{
    auto InputValidationResult = ValidateValueSlot(Input, "Conduit unary intrinsic input slot");
    if (!InputValidationResult)
    {
        return InputValidationResult;
    }
    auto OutputValidationResult = ValidateValueSlot(Output, "Conduit unary intrinsic output slot");
    if (!OutputValidationResult)
    {
        return OutputValidationResult;
    }

    const SlotDesc* InputSlot = FindSlot(Input);
    const SlotDesc* OutputSlot = FindSlot(Output);
    if (!InputSlot || !InputSlot->Type || !OutputSlot || !OutputSlot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit unary intrinsic slot metadata is missing"));
    }

    const UnaryIntrinsicFn ExecuteIntrinsic = ResolveUnaryIntrinsicFn(Op, *InputSlot->Type, *OutputSlot->Type);
    if (!ExecuteIntrinsic)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit unary intrinsic type combination is not supported"));
    }

    BoundNode Node;
    Node.Kind = ENodeKind::UnaryIntrinsic;
    Node.Execute = &ExecuteUnaryIntrinsicNode;
    Node.Data = UnaryIntrinsicNodeData{
        .Input = Input,
        .Output = Output,
        .Op = Op,
        .InputType = InputSlot->Type,
        .OutputType = OutputSlot->Type,
        .ExecuteIntrinsic = ExecuteIntrinsic,
    };
    m_nodes.push_back(std::move(Node));
    return Ok();
}

Result GraphBuilder::AddBinaryIntrinsic(const EBinaryIntrinsicOp Op,
                                        const SlotId Left,
                                        const SlotId Right,
                                        const SlotId Output)
{
    const SlotDesc* LeftSlot = FindSlot(Left);
    const SlotDesc* RightSlot = FindSlot(Right);
    const SlotDesc* OutputSlot = FindSlot(Output);
    if (!LeftSlot || !LeftSlot->Type || !RightSlot || !RightSlot->Type || !OutputSlot || !OutputSlot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit binary intrinsic slot metadata is missing"));
    }

    if (Op == EBinaryIntrinsicOp::Equal || Op == EBinaryIntrinsicOp::NotEqual)
    {
        if (LeftSlot->Type->Id != RightSlot->Type->Id)
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit equality intrinsic requires matching input types"));
        }
        if (OutputSlot->Kind != ESlotKind::Value || OutputSlot->Type->Id != StaticTypeId<bool>())
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit equality intrinsic output must be a bool value slot"));
        }

        const BinaryIntrinsicFn ExecuteIntrinsic = ResolveBinaryIntrinsicFn(Op, *LeftSlot->Type, *RightSlot->Type, *OutputSlot->Type);
        if (!ExecuteIntrinsic &&
            (!LeftSlot->Type->RuntimeOps || !LeftSlot->Type->RuntimeOps->Equals))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit equality intrinsic requires reflected equality support"));
        }

        BoundNode Node;
        Node.Kind = ENodeKind::BinaryIntrinsic;
        Node.Execute = &ExecuteBinaryIntrinsicNode;
        Node.Data = BinaryIntrinsicNodeData{
            .Left = Left,
            .Right = Right,
            .Output = Output,
            .Op = Op,
            .ValueType = LeftSlot->Type,
            .OutputType = OutputSlot->Type,
            .ExecuteIntrinsic = ExecuteIntrinsic,
        };
        m_nodes.push_back(std::move(Node));
        return Ok();
    }

    auto LeftValidationResult = ValidateValueSlot(Left, "Conduit binary intrinsic left slot");
    if (!LeftValidationResult)
    {
        return LeftValidationResult;
    }
    auto RightValidationResult = ValidateValueSlot(Right, "Conduit binary intrinsic right slot");
    if (!RightValidationResult)
    {
        return RightValidationResult;
    }
    auto OutputValidationResult = ValidateValueSlot(Output, "Conduit binary intrinsic output slot");
    if (!OutputValidationResult)
    {
        return OutputValidationResult;
    }

    if (LeftSlot->Type->Id != RightSlot->Type->Id)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit binary intrinsic requires matching input types"));
    }

    const BinaryIntrinsicFn ExecuteIntrinsic = ResolveBinaryIntrinsicFn(Op, *LeftSlot->Type, *RightSlot->Type, *OutputSlot->Type);
    if (!ExecuteIntrinsic)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit binary intrinsic type combination is not supported"));
    }

    BoundNode Node;
    Node.Kind = ENodeKind::BinaryIntrinsic;
    Node.Execute = &ExecuteBinaryIntrinsicNode;
    Node.Data = BinaryIntrinsicNodeData{
        .Left = Left,
        .Right = Right,
        .Output = Output,
        .Op = Op,
        .ValueType = LeftSlot->Type,
        .OutputType = OutputSlot->Type,
        .ExecuteIntrinsic = ExecuteIntrinsic,
    };
    m_nodes.push_back(std::move(Node));
    return Ok();
}

Result GraphBuilder::AddJump(const LabelId Target)
{
    auto LabelResult = ValidateLabel(Target);
    if (!LabelResult)
    {
        return LabelResult;
    }

    BoundNode Node;
    Node.Kind = ENodeKind::Jump;
    Node.Execute = &ExecuteJumpNode;
    Node.Data = JumpNodeData{};
    const std::uint32_t NodeIndex = static_cast<std::uint32_t>(m_nodes.size());
    m_nodes.push_back(std::move(Node));
    m_labelFixups.push_back(LabelFixup{
        .NodeIndex = NodeIndex,
        .Label = Target,
        .Kind = ELabelFixupKind::JumpTarget,
    });
    return Ok();
}

Result GraphBuilder::AddBranch(const SlotId Condition, const LabelId TrueTarget, const LabelId FalseTarget)
{
    auto ConditionResult = ValidateConditionSlot(Condition);
    if (!ConditionResult)
    {
        return ConditionResult;
    }
    auto TrueResult = ValidateLabel(TrueTarget);
    if (!TrueResult)
    {
        return TrueResult;
    }
    auto FalseResult = ValidateLabel(FalseTarget);
    if (!FalseResult)
    {
        return FalseResult;
    }

    BoundNode Node;
    Node.Kind = ENodeKind::Branch;
    Node.Execute = &ExecuteBranchNode;
    Node.Data = BranchNodeData{
        .Condition = Condition,
    };
    const std::uint32_t NodeIndex = static_cast<std::uint32_t>(m_nodes.size());
    m_nodes.push_back(std::move(Node));
    m_labelFixups.push_back(LabelFixup{
        .NodeIndex = NodeIndex,
        .Label = TrueTarget,
        .Kind = ELabelFixupKind::BranchTrueTarget,
    });
    m_labelFixups.push_back(LabelFixup{
        .NodeIndex = NodeIndex,
        .Label = FalseTarget,
        .Kind = ELabelFixupKind::BranchFalseTarget,
    });
    return Ok();
}

Result GraphBuilder::AddSelfFieldRead(const std::string_view FieldName, const SlotId Output)
{
    const SlotDesc* Slot = FindSlot(Output);
    if (!Slot || !Slot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit output slot was not found"));
    }

    auto FieldResult = BindField(*m_selfType, FieldName);
    if (!FieldResult)
    {
        return std::unexpected(FieldResult.error());
    }
    if ((*FieldResult)->FieldType != Slot->Type->Id)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit field read slot type mismatch"));
    }
    if (!Slot->Type->RuntimeOps || !Slot->Type->RuntimeOps->CopyConstruct)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit field read output is not copyable"));
    }
    if (!(*FieldResult)->ConstPointer && !(*FieldResult)->ViewGetter && !(*FieldResult)->Getter)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit field has no readable fast path"));
    }

    BoundNode Node;
    Node.Kind = ENodeKind::SelfFieldRead;
    Node.Execute = &ExecuteSelfFieldReadNode;
    Node.Data = SelfFieldReadNodeData{.Output = Output, .OwnerType = m_selfType, .Field = *FieldResult};
    m_nodes.push_back(std::move(Node));
    return Ok();
}

Result GraphBuilder::AddSelfFieldWrite(const std::string_view FieldName, const SlotId Input)
{
    const SlotDesc* Slot = FindSlot(Input);
    if (!Slot || !Slot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit input slot was not found"));
    }

    auto FieldResult = BindField(*m_selfType, FieldName);
    if (!FieldResult)
    {
        return std::unexpected(FieldResult.error());
    }
    if ((*FieldResult)->FieldType != Slot->Type->Id)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit field write slot type mismatch"));
    }
    const TypeInfo* FieldType = TypeRegistry::Instance().Find((*FieldResult)->FieldType);
    const bool HasPointerWritePath = (*FieldResult)->MutablePointer && FieldType && FieldType->RuntimeOps && FieldType->RuntimeOps->CopyAssign;
    if (!(*FieldResult)->RawSetter && !HasPointerWritePath)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit field write requires raw setter or mutable pointer + copy assign"));
    }

    BoundNode Node;
    Node.Kind = ENodeKind::SelfFieldWrite;
    Node.Execute = &ExecuteSelfFieldWriteNode;
    Node.Data = SelfFieldWriteNodeData{.Input = Input, .OwnerType = m_selfType, .Field = *FieldResult};
    m_nodes.push_back(std::move(Node));
    return Ok();
}

Result GraphBuilder::AddSelfMethodCall(const std::string_view Name,
                                           const std::span<const SlotId> Inputs,
                                           const std::optional<SlotId> Output)
{
    auto MethodResult = BindMethod(*m_selfType, Name, Inputs);
    if (!MethodResult)
    {
        return std::unexpected(MethodResult.error());
    }

    const MethodInfo* Method = *MethodResult;
    if (Method->ReturnType == StaticTypeId<void>())
    {
        if (Output)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit void method cannot write to an output slot"));
        }
    }
    else
    {
        if (!Output)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit non-void method requires an output slot"));
        }

        const SlotDesc* Slot = FindSlot(*Output);
        if (!Slot || !Slot->Type)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit output slot was not found"));
        }
        if (Slot->Type->Id != Method->ReturnType)
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit method output slot type mismatch"));
        }
    }

    BoundNode Node;
    Node.Kind = ENodeKind::SelfMethodCall;
    Node.Execute = &ExecuteSelfMethodCallNode;
    Node.Data = SelfMethodCallNodeData{.Inputs = std::vector<SlotId>(Inputs.begin(), Inputs.end()),
                                       .Output = Output,
                                       .OwnerType = m_selfType,
                                       .Method = Method};
    m_maxScratchArgs = std::max<std::size_t>(m_maxScratchArgs, Inputs.size());
    m_nodes.push_back(std::move(Node));
    return Ok();
}

Result GraphBuilder::AddFieldRead(const TypeInfo& OwnerType,
                                      const SlotId Instance,
                                      const std::string_view FieldName,
                                      const SlotId Output)
{
    auto OwnerTypeResult = ResolveType(OwnerType.Id);
    if (!OwnerTypeResult)
    {
        return std::unexpected(OwnerTypeResult.error());
    }
    const TypeInfo* StableOwnerType = *OwnerTypeResult;

    const SlotDesc* OutputSlot = FindSlot(Output);
    if (!OutputSlot || !OutputSlot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit output slot was not found"));
    }

    const SlotDesc* InstanceSlot = FindSlot(Instance);
    if (!InstanceSlot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit instance slot was not found"));
    }
    auto InstanceSlotResult = ValidateInstanceSlot(*InstanceSlot);
    if (!InstanceSlotResult)
    {
        return std::unexpected(InstanceSlotResult.error());
    }

    auto FieldResult = BindField(*StableOwnerType, FieldName);
    if (!FieldResult)
    {
        return std::unexpected(FieldResult.error());
    }
    if ((*FieldResult)->FieldType != OutputSlot->Type->Id)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit field read slot type mismatch"));
    }
    if (!OutputSlot->Type->RuntimeOps || !OutputSlot->Type->RuntimeOps->CopyConstruct)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit field read output is not copyable"));
    }
    if (!(*FieldResult)->ConstPointer && !(*FieldResult)->ViewGetter && !(*FieldResult)->Getter)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit field has no readable fast path"));
    }

    BoundNode Node;
    Node.Kind = ENodeKind::InstanceFieldRead;
    Node.Execute = &ExecuteInstanceFieldReadNode;
    Node.Data = InstanceFieldReadNodeData{
        .Instance = Instance,
        .Output = Output,
        .OwnerType = StableOwnerType,
        .Field = *FieldResult,
    };
    m_nodes.push_back(std::move(Node));
    return Ok();
}

Result GraphBuilder::AddFieldRead(const TypeId& OwnerType,
                                      const SlotId Instance,
                                      const std::string_view FieldName,
                                      const SlotId Output)
{
    auto TypeResult = ResolveType(OwnerType);
    if (!TypeResult)
    {
        return std::unexpected(TypeResult.error());
    }
    return AddFieldRead(**TypeResult, Instance, FieldName, Output);
}

Result GraphBuilder::AddFieldWrite(const TypeInfo& OwnerType,
                                       const SlotId Instance,
                                       const std::string_view FieldName,
                                       const SlotId Input)
{
    auto OwnerTypeResult = ResolveType(OwnerType.Id);
    if (!OwnerTypeResult)
    {
        return std::unexpected(OwnerTypeResult.error());
    }
    const TypeInfo* StableOwnerType = *OwnerTypeResult;

    const SlotDesc* InputSlot = FindSlot(Input);
    if (!InputSlot || !InputSlot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit input slot was not found"));
    }

    const SlotDesc* InstanceSlot = FindSlot(Instance);
    if (!InstanceSlot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit instance slot was not found"));
    }
    auto InstanceSlotResult = ValidateInstanceSlot(*InstanceSlot);
    if (!InstanceSlotResult)
    {
        return std::unexpected(InstanceSlotResult.error());
    }

    auto FieldResult = BindField(*StableOwnerType, FieldName);
    if (!FieldResult)
    {
        return std::unexpected(FieldResult.error());
    }
    if ((*FieldResult)->FieldType != InputSlot->Type->Id)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit field write slot type mismatch"));
    }
    const TypeInfo* FieldType = TypeRegistry::Instance().Find((*FieldResult)->FieldType);
    const bool HasPointerWritePath = (*FieldResult)->MutablePointer && FieldType && FieldType->RuntimeOps && FieldType->RuntimeOps->CopyAssign;
    if (!(*FieldResult)->RawSetter && !HasPointerWritePath)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit field write requires raw setter or mutable pointer + copy assign"));
    }

    BoundNode Node;
    Node.Kind = ENodeKind::InstanceFieldWrite;
    Node.Execute = &ExecuteInstanceFieldWriteNode;
    Node.Data = InstanceFieldWriteNodeData{
        .Instance = Instance,
        .Input = Input,
        .OwnerType = StableOwnerType,
        .Field = *FieldResult,
    };
    m_nodes.push_back(std::move(Node));
    return Ok();
}

Result GraphBuilder::AddFieldWrite(const TypeId& OwnerType,
                                       const SlotId Instance,
                                       const std::string_view FieldName,
                                       const SlotId Input)
{
    auto TypeResult = ResolveType(OwnerType);
    if (!TypeResult)
    {
        return std::unexpected(TypeResult.error());
    }
    return AddFieldWrite(**TypeResult, Instance, FieldName, Input);
}

Result GraphBuilder::AddMethodCall(const TypeInfo& OwnerType,
                                       const SlotId Instance,
                                       const std::string_view Name,
                                       const std::span<const SlotId> Inputs,
                                       const std::optional<SlotId> Output)
{
    auto OwnerTypeResult = ResolveType(OwnerType.Id);
    if (!OwnerTypeResult)
    {
        return std::unexpected(OwnerTypeResult.error());
    }
    const TypeInfo* StableOwnerType = *OwnerTypeResult;

    const SlotDesc* InstanceSlot = FindSlot(Instance);
    if (!InstanceSlot)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit instance slot was not found"));
    }
    auto InstanceSlotResult = ValidateInstanceSlot(*InstanceSlot);
    if (!InstanceSlotResult)
    {
        return std::unexpected(InstanceSlotResult.error());
    }

    auto MethodResult = BindMethod(*StableOwnerType, Name, Inputs);
    if (!MethodResult)
    {
        return std::unexpected(MethodResult.error());
    }

    const MethodInfo* Method = *MethodResult;
    if (Method->ReturnType == StaticTypeId<void>())
    {
        if (Output)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit void method cannot write to an output slot"));
        }
    }
    else
    {
        if (!Output)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit non-void method requires an output slot"));
        }

        const SlotDesc* OutputSlot = FindSlot(*Output);
        if (!OutputSlot || !OutputSlot->Type)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit output slot was not found"));
        }
        if (OutputSlot->Type->Id != Method->ReturnType)
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit method output slot type mismatch"));
        }
    }

    BoundNode Node;
    Node.Kind = ENodeKind::InstanceMethodCall;
    Node.Execute = &ExecuteInstanceMethodCallNode;
    Node.Data = InstanceMethodCallNodeData{
        .Instance = Instance,
        .Inputs = std::vector<SlotId>(Inputs.begin(), Inputs.end()),
        .Output = Output,
        .OwnerType = StableOwnerType,
        .Method = Method,
    };
    m_maxScratchArgs = std::max<std::size_t>(m_maxScratchArgs, Inputs.size());
    m_nodes.push_back(std::move(Node));
    return Ok();
}

Result GraphBuilder::AddMethodCall(const TypeId& OwnerType,
                                       const SlotId Instance,
                                       const std::string_view Name,
                                       const std::span<const SlotId> Inputs,
                                       const std::optional<SlotId> Output)
{
    auto TypeResult = ResolveType(OwnerType);
    if (!TypeResult)
    {
        return std::unexpected(TypeResult.error());
    }
    return AddMethodCall(**TypeResult, Instance, Name, Inputs, Output);
}

TExpected<CompiledGraph> GraphBuilder::Build() &&
{
    auto FixupResult = ApplyLabelFixups();
    if (!FixupResult)
    {
        return std::unexpected(FixupResult.error());
    }

    CompiledGraph Graph;
    Graph.Layout = std::move(m_layout);
    Graph.Nodes = std::move(m_nodes);
    Graph.EntryPoints.reserve(m_entryPoints.size());
    for (const EntryPointDef& EntryPoint : m_entryPoints)
    {
        Graph.EntryPoints.push_back(GraphEntryPoint{
            .Name = EntryPoint.Name,
            .Builtin = EntryPoint.Builtin,
            .StartNodeIndex = EntryPoint.StartNodeIndex,
            .EndNodeIndex = static_cast<std::uint32_t>(Graph.Nodes.size()),
            .DeltaSecondsSlot = EntryPoint.DeltaSecondsSlot,
        });
    }
    std::stable_sort(Graph.EntryPoints.begin(), Graph.EntryPoints.end(), [](const GraphEntryPoint& Left, const GraphEntryPoint& Right) {
        if (Left.StartNodeIndex != Right.StartNodeIndex)
        {
            return Left.StartNodeIndex < Right.StartNodeIndex;
        }
        return Left.Name < Right.Name;
    });
    for (std::size_t Index = 0; Index < Graph.EntryPoints.size(); ++Index)
    {
        std::uint32_t EndNodeIndex = static_cast<std::uint32_t>(Graph.Nodes.size());
        for (std::size_t NextIndex = Index + 1; NextIndex < Graph.EntryPoints.size(); ++NextIndex)
        {
            if (Graph.EntryPoints[NextIndex].StartNodeIndex > Graph.EntryPoints[Index].StartNodeIndex)
            {
                EndNodeIndex = Graph.EntryPoints[NextIndex].StartNodeIndex;
                break;
            }
        }
        Graph.EntryPoints[Index].EndNodeIndex = EndNodeIndex;
    }
    Graph.MaxScratchArgs = m_maxScratchArgs;
    return Graph;
}

const SlotDesc* GraphBuilder::FindSlot(const SlotId Id) const
{
    return m_layout.FindSlot(Id);
}

TExpected<const TypeInfo*> GraphBuilder::ResolveType(const TypeId& Type) const
{
    const TypeInfo* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit owner type is not registered"));
    }
    return Info;
}

Result GraphBuilder::ValidateInstanceSlot(const SlotDesc& Slot) const
{
    if (Slot.Kind != ESlotKind::Handle)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit instance slot must use handle storage"));
    }
    if (!Slot.Type)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit instance slot type is missing"));
    }
    return Ok();
}

Result GraphBuilder::ValidateConditionSlot(const SlotId Condition) const
{
    const SlotDesc* Slot = FindSlot(Condition);
    if (!Slot || !Slot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit branch condition slot was not found"));
    }
    if (Slot->Kind != ESlotKind::Value)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit branch condition must use value storage"));
    }
    if (Slot->Type->Id != StaticTypeId<bool>())
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Conduit branch condition slot must be bool"));
    }
    return Ok();
}

Result GraphBuilder::ValidateValueSlot(const SlotId Id, const std::string_view ContextName) const
{
    const SlotDesc* Slot = FindSlot(Id);
    if (!Slot || !Slot->Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, std::string(ContextName) + " was not found"));
    }
    if (Slot->Kind != ESlotKind::Value)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, std::string(ContextName) + " must use value storage"));
    }
    return Ok();
}

Result GraphBuilder::ValidateLabel(const LabelId Label) const
{
    if (!Label.IsValid() || Label.Value >= m_labels.size())
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit label was not found"));
    }
    return Ok();
}

Result GraphBuilder::ValidateEntryPointName(const std::string_view Name, const EBuiltinEntryPoint Builtin) const
{
    const std::string_view EffectiveName = Builtin == EBuiltinEntryPoint::None ? Name : BuiltinEntryPointName(Builtin);
    if (EffectiveName.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Conduit entrypoint name is empty"));
    }

    for (const EntryPointDef& EntryPoint : m_entryPoints)
    {
        if (EntryPoint.Name == EffectiveName)
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit entrypoint name is already bound"));
        }
        if (Builtin != EBuiltinEntryPoint::None && EntryPoint.Builtin == Builtin)
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit built-in entrypoint is already bound"));
        }
    }

    if (Builtin == EBuiltinEntryPoint::None)
    {
        for (const EBuiltinEntryPoint Reserved :
             {EBuiltinEntryPoint::OnCreate,
              EBuiltinEntryPoint::PreTick,
              EBuiltinEntryPoint::Tick,
              EBuiltinEntryPoint::FixedTick,
              EBuiltinEntryPoint::LateTick,
              EBuiltinEntryPoint::PostTick,
              EBuiltinEntryPoint::OnDestroy})
        {
            if (EffectiveName == BuiltinEntryPointName(Reserved))
            {
                return std::unexpected(MakeError(EErrorCode::AlreadyExists,
                                                 "Conduit custom entrypoint name collides with a reserved built-in name"));
            }
        }
    }

    return Ok();
}

Result GraphBuilder::ApplyLabelFixups()
{
    for (const LabelFixup& Fixup : m_labelFixups)
    {
        if (Fixup.NodeIndex >= m_nodes.size())
        {
            return std::unexpected(MakeError(EErrorCode::OutOfRange, "Conduit label fixup node index is invalid"));
        }
        auto LabelResult = ValidateLabel(Fixup.Label);
        if (!LabelResult)
        {
            return LabelResult;
        }
        if (!m_labels[Fixup.Label.Value].has_value())
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Conduit label was never bound"));
        }

        const std::uint32_t TargetNode = *m_labels[Fixup.Label.Value];
        BoundNode& Node = m_nodes[Fixup.NodeIndex];
        switch (Fixup.Kind)
        {
        case ELabelFixupKind::JumpTarget:
            std::get<JumpNodeData>(Node.Data).TargetNode = TargetNode;
            break;
        case ELabelFixupKind::BranchTrueTarget:
            std::get<BranchNodeData>(Node.Data).TrueTarget = TargetNode;
            break;
        case ELabelFixupKind::BranchFalseTarget:
            std::get<BranchNodeData>(Node.Data).FalseTarget = TargetNode;
            break;
        }
    }

    return Ok();
}

TExpected<const FieldInfo*> GraphBuilder::BindField(const TypeInfo& OwnerType, const std::string_view FieldName) const
{
    for (const ReflectedFieldRef& Entry : TypeRegistry::Instance().CollectFields(OwnerType.Id))
    {
        if (Entry.Field && Entry.Field->Name == FieldName)
        {
            return Entry.Field;
        }
    }

    return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit field binding not found"));
}

TExpected<const MethodInfo*> GraphBuilder::BindMethod(const TypeInfo& OwnerType,
                                                          const std::string_view Name,
                                                          const std::span<const SlotId> Inputs) const
{
    const MethodInfo* Match = nullptr;
    for (const ReflectedMethodRef& Entry : TypeRegistry::Instance().CollectMethods(OwnerType.Id))
    {
        const MethodInfo* Method = Entry.Method;
        if (!Method || Method->Name != Name || Method->ParamTypes.size() != Inputs.size())
        {
            continue;
        }
        if (!Method->RawInvoke)
        {
            continue;
        }

        bool TypesMatch = true;
        for (std::size_t Index = 0; Index < Inputs.size(); ++Index)
        {
            const SlotDesc* Slot = FindSlot(Inputs[Index]);
            if (!Slot || !Slot->Type || Slot->Type->Id != Method->ParamTypes[Index])
            {
                TypesMatch = false;
                break;
            }
        }
        if (!TypesMatch)
        {
            continue;
        }

        if (Match)
        {
            return std::unexpected(MakeError(EErrorCode::AlreadyExists, "Conduit method binding is ambiguous"));
        }
        Match = Method;
    }

    if (!Match)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Conduit method binding not found"));
    }
    return Match;
}

} // namespace SnAPI::GameFramework::Conduit
