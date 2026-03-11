#include "Conduit/ClassComponent.h"

#include <iostream>

#include "AssetRef.h"
#include "IWorld.h"

namespace SnAPI::GameFramework::Conduit
{
namespace
{
void LogClassComponentWarning(const std::string& Message)
{
    std::cerr << "Warning: Conduit::ClassComponent: " << Message << '\n';
}
} // namespace

void ClassComponent::OnCreate(IWorld& WorldRef)
{
    m_pendingOnCreate = true;
    m_bindFailureLogged = false;

    if (m_compiledClass && m_boundClass != Class)
    {
        ClearBinding();
    }

    (void)ExecutePendingOnCreate(WorldRef);
}

void ClassComponent::OnDestroy(IWorld& WorldRef)
{
    if (m_compiledClass && m_instance)
    {
        (void)ExecuteBuiltinEntry(WorldRef, EBuiltinEntryPoint::OnDestroy, 0.0f, false);
    }

    ClearBinding();
    m_pendingOnCreate = false;
    m_bindFailureLogged = false;
}

void ClassComponent::PreTick(IWorld& WorldRef, const float DeltaSeconds)
{
    if (const Result CreateResult = ExecutePendingOnCreate(WorldRef); !CreateResult)
    {
        LogWarningOnce(CreateResult.error().Message);
        return;
    }
    if (const Result PhaseResult = ExecuteBuiltinEntry(WorldRef, EBuiltinEntryPoint::PreTick, DeltaSeconds, true); !PhaseResult)
    {
        LogWarningOnce(PhaseResult.error().Message);
    }
}

void ClassComponent::Tick(IWorld& WorldRef, const float DeltaSeconds)
{
    if (const Result CreateResult = ExecutePendingOnCreate(WorldRef); !CreateResult)
    {
        LogWarningOnce(CreateResult.error().Message);
        return;
    }
    if (const Result PhaseResult = ExecuteBuiltinEntry(WorldRef, EBuiltinEntryPoint::Tick, DeltaSeconds, true); !PhaseResult)
    {
        LogWarningOnce(PhaseResult.error().Message);
    }
}

void ClassComponent::FixedTick(IWorld& WorldRef, const float DeltaSeconds)
{
    if (const Result CreateResult = ExecutePendingOnCreate(WorldRef); !CreateResult)
    {
        LogWarningOnce(CreateResult.error().Message);
        return;
    }
    if (const Result PhaseResult = ExecuteBuiltinEntry(WorldRef, EBuiltinEntryPoint::FixedTick, DeltaSeconds, true); !PhaseResult)
    {
        LogWarningOnce(PhaseResult.error().Message);
    }
}

void ClassComponent::LateTick(IWorld& WorldRef, const float DeltaSeconds)
{
    if (const Result CreateResult = ExecutePendingOnCreate(WorldRef); !CreateResult)
    {
        LogWarningOnce(CreateResult.error().Message);
        return;
    }
    if (const Result PhaseResult = ExecuteBuiltinEntry(WorldRef, EBuiltinEntryPoint::LateTick, DeltaSeconds, true); !PhaseResult)
    {
        LogWarningOnce(PhaseResult.error().Message);
    }
}

void ClassComponent::PostTick(IWorld& WorldRef, const float DeltaSeconds)
{
    if (const Result CreateResult = ExecutePendingOnCreate(WorldRef); !CreateResult)
    {
        LogWarningOnce(CreateResult.error().Message);
        return;
    }
    if (const Result PhaseResult = ExecuteBuiltinEntry(WorldRef, EBuiltinEntryPoint::PostTick, DeltaSeconds, true); !PhaseResult)
    {
        LogWarningOnce(PhaseResult.error().Message);
    }
}

Result ClassComponent::ExecuteEntry(const std::string_view Name)
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        RememberError("Conduit class component world is missing");
        return std::unexpected(MakeError(EErrorCode::NotFound, m_lastError));
    }

    if (const Result CreateResult = ExecutePendingOnCreate(*WorldPtr); !CreateResult)
    {
        return CreateResult;
    }
    if (const Result BindResult = EnsureBound(*WorldPtr); !BindResult)
    {
        return BindResult;
    }
    if (!m_compiledClass || !m_instance)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Conduit class component is not bound"));
    }

    const GraphEntryPoint* EntryPoint = m_compiledClass->RuntimeGraph.FindEntryPoint(Name);
    if (!EntryPoint)
    {
        RememberError("Conduit class entrypoint was not found");
        return std::unexpected(MakeError(EErrorCode::NotFound, m_lastError));
    }
    if (EntryPoint->Builtin != EBuiltinEntryPoint::None)
    {
        RememberError("Conduit built-in entrypoints are reserved for lifecycle execution");
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, m_lastError));
    }

    return ExecuteResolvedEntry(*WorldPtr, *EntryPoint, std::nullopt);
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void ClassComponent::EditorOnPropertyChanged(const std::string_view Name)
{
    if (Name != "Class")
    {
        return;
    }

    ClearBinding();
    m_pendingOnCreate = true;
    m_bindFailureLogged = false;

    if (auto* WorldPtr = World())
    {
        (void)ExecutePendingOnCreate(*WorldPtr);
    }
}
#endif

Result ClassComponent::Rebind(IWorld& WorldRef)
{
    ClearBinding();
    m_pendingOnCreate = true;
    m_bindFailureLogged = false;

    if (const Result BindResult = EnsureBound(WorldRef); !BindResult)
    {
        return BindResult;
    }
    return ExecutePendingOnCreate(WorldRef);
}

bool ClassComponent::IsBound() const
{
    return static_cast<bool>(m_compiledClass) && static_cast<bool>(m_instance);
}

const std::string& ClassComponent::LastError() const
{
    return m_lastError;
}

Result ClassComponent::EnsureBound(IWorld& WorldRef)
{
    (void)WorldRef;

    BaseNode* Owner = OwnerNode();
    if (!Owner)
    {
        RememberError("Conduit class component owner node is missing");
        return std::unexpected(MakeError(EErrorCode::NotFound, m_lastError));
    }

    if (m_compiledClass && m_instance && m_boundClass == Class)
    {
        m_lastError.clear();
        return Ok();
    }

    if (Class.IsNull())
    {
        ClearBinding();
        RememberError("Conduit class asset reference is empty");
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, m_lastError));
    }

    auto* AssetManager = ResolveDefaultAssetManager();
    if (!AssetManager)
    {
        RememberError("No default AssetManager resolver is configured for Conduit class binding");
        return std::unexpected(MakeError(EErrorCode::NotFound, m_lastError));
    }

    auto LoadResult = Class.Load(*AssetManager);
    if (!LoadResult)
    {
        RememberError("Failed to load Conduit class asset: " + LoadResult.error());
        return std::unexpected(MakeError(EErrorCode::NotFound, m_lastError));
    }
    if (!*LoadResult)
    {
        RememberError("Loaded Conduit class asset was null");
        return std::unexpected(MakeError(EErrorCode::NotFound, m_lastError));
    }

    auto CompileResult = CompileClassAsset(**LoadResult, *AssetManager);
    if (!CompileResult)
    {
        RememberError(CompileResult.error().Message);
        return std::unexpected(CompileResult.error());
    }

    const TypeId OwnerTypeId = Owner->TypeKey();
    if (!TypeRegistry::Instance().IsA(OwnerTypeId, CompileResult->HostType))
    {
        RememberError("Owning node type is incompatible with the referenced Conduit class host type");
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, m_lastError));
    }

    m_compiledClass = std::make_unique<CompiledClass>(std::move(*CompileResult));
    m_instance = std::make_unique<GraphInstance>(m_compiledClass->RuntimeGraph);
    m_boundClass = Class;
    m_lastError.clear();
    m_bindFailureLogged = false;
    return Ok();
}

Result ClassComponent::ExecutePendingOnCreate(IWorld& WorldRef)
{
    if (!m_pendingOnCreate)
    {
        return Ok();
    }

    if (const Result BindResult = EnsureBound(WorldRef); !BindResult)
    {
        return BindResult;
    }

    m_pendingOnCreate = false;
    return ExecuteBuiltinEntry(WorldRef, EBuiltinEntryPoint::OnCreate, 0.0f, false);
}

Result ClassComponent::ExecuteBuiltinEntry(IWorld& WorldRef,
                                           const EBuiltinEntryPoint EntryPointKind,
                                           const float DeltaSeconds,
                                           const bool HasDeltaSeconds)
{
    if (EntryPointKind != EBuiltinEntryPoint::OnDestroy)
    {
        if (const Result BindResult = EnsureBound(WorldRef); !BindResult)
        {
            return BindResult;
        }
    }

    if (!m_compiledClass || !m_instance)
    {
        return Ok();
    }

    const GraphEntryPoint* EntryPoint = m_compiledClass->RuntimeGraph.FindEntryPoint(EntryPointKind);
    if (!EntryPoint)
    {
        return Ok();
    }

    return ExecuteResolvedEntry(WorldRef, *EntryPoint, HasDeltaSeconds ? std::optional<float>{DeltaSeconds} : std::nullopt);
}

Result ClassComponent::ExecuteResolvedEntry(IWorld& WorldRef,
                                            const GraphEntryPoint& EntryPoint,
                                            const std::optional<float> DeltaSeconds)
{
    (void)WorldRef;

    if (!m_compiledClass || !m_instance)
    {
        return Ok();
    }

    BaseNode* Owner = OwnerNode();
    if (!Owner)
    {
        RememberError("Conduit class component owner node is missing");
        return std::unexpected(MakeError(EErrorCode::NotFound, m_lastError));
    }

    const TypeInfo* SelfType = TypeRegistry::Instance().Find(Owner->TypeKey());
    if (!SelfType)
    {
        RememberError("Conduit class owner node type is not registered");
        return std::unexpected(MakeError(EErrorCode::NotFound, m_lastError));
    }

    if (DeltaSeconds && EntryPoint.DeltaSecondsSlot.IsValid())
    {
        const float DeltaValue = *DeltaSeconds;
        if (const Result StoreResult = m_instance->Frame().StoreCopy(EntryPoint.DeltaSecondsSlot, &DeltaValue); !StoreResult)
        {
            RememberError(StoreResult.error().Message);
            return std::unexpected(StoreResult.error());
        }
    }

    ExecutionContext Context{
        .Self = Owner,
        .SelfType = SelfType,
    };
    if (const Result ExecuteResult = m_instance->ExecuteEntry(EntryPoint.Name, Context); !ExecuteResult)
    {
        RememberError(ExecuteResult.error().Message);
        return std::unexpected(ExecuteResult.error());
    }

    m_lastError.clear();
    return Ok();
}

void ClassComponent::ClearBinding()
{
    m_instance.reset();
    m_compiledClass.reset();
    m_boundClass.Clear();
}

void ClassComponent::RememberError(const std::string& Message)
{
    m_lastError = Message;
}

void ClassComponent::LogWarningOnce(const std::string& Message)
{
    RememberError(Message);
    if (m_bindFailureLogged)
    {
        return;
    }
    LogClassComponentWarning(Message);
    m_bindFailureLogged = true;
}

} // namespace SnAPI::GameFramework::Conduit
