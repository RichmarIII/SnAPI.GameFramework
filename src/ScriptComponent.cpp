#include "ScriptComponent.h"

#include "IWorld.h"
#include "PathResolver.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>

namespace SnAPI::GameFramework
{
namespace
{
void LogScriptWarning(const std::string& Message)
{
    std::cerr << "Warning: ScriptComponent: " << Message << '\n';
}
} // namespace

void ScriptComponent::OnCreate(IWorld& WorldRef)
{
    (void)WorldRef;
    Instance = 0;
    m_pendingCreateHook = true;
    m_createHookDelivered = false;
    m_bindFailureLogged = false;
}

void ScriptComponent::OnDestroy(IWorld& WorldRef)
{
    (void)WorldRef;
    Unbind(true);
    m_pendingCreateHook = false;
    m_bindFailureLogged = false;
}

void ScriptComponent::PreTick(IWorld& WorldRef, const float DeltaSeconds)
{
    const std::array<Variant, 1> Args{Variant::FromValue(DeltaSeconds)};
    InvokeHook(WorldRef, EScriptHook::PreTick, std::span<const Variant>(Args));
}

void ScriptComponent::Tick(IWorld& WorldRef, const float DeltaSeconds)
{
    const std::array<Variant, 1> Args{Variant::FromValue(DeltaSeconds)};
    InvokeHook(WorldRef, EScriptHook::Tick, std::span<const Variant>(Args));
}

void ScriptComponent::FixedTick(IWorld& WorldRef, const float DeltaSeconds)
{
    const std::array<Variant, 1> Args{Variant::FromValue(DeltaSeconds)};
    InvokeHook(WorldRef, EScriptHook::FixedTick, std::span<const Variant>(Args));
}

void ScriptComponent::LateTick(IWorld& WorldRef, const float DeltaSeconds)
{
    const std::array<Variant, 1> Args{Variant::FromValue(DeltaSeconds)};
    InvokeHook(WorldRef, EScriptHook::LateTick, std::span<const Variant>(Args));
}

void ScriptComponent::PostTick(IWorld& WorldRef, const float DeltaSeconds)
{
    const std::array<Variant, 1> Args{Variant::FromValue(DeltaSeconds)};
    InvokeHook(WorldRef, EScriptHook::PostTick, std::span<const Variant>(Args));
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void ScriptComponent::EditorOnPropertyChanged(const std::string_view Name)
{
    if (Name != "ScriptModule" && Name != "ScriptType")
    {
        return;
    }

    m_bindFailureLogged = false;
    m_pendingCreateHook = true;
    m_createHookDelivered = false;

    if (auto* WorldPtr = World())
    {
        (void)EnsureBound(*WorldPtr);
    }
}
#endif

EScriptBackend ScriptComponent::ResolveBackend() const
{
    std::string ModulePathText = ScriptModule;
    if (auto ResolvedModulePath = SPathResolver::Instance().ResolveToString(ScriptModule);
        ResolvedModulePath && !ResolvedModulePath->empty())
    {
        ModulePathText = *ResolvedModulePath;
    }

    const std::filesystem::path ModulePath(ModulePathText);
    std::string Extension = ModulePath.extension().string();
    std::transform(Extension.begin(),
                   Extension.end(),
                   Extension.begin(),
                   [](const unsigned char C) {
                       return static_cast<char>(std::tolower(C));
                   });

    if (Extension.empty() || Extension == ".lua")
    {
        return EScriptBackend::Lua;
    }

    return EScriptBackend::None;
}

Result ScriptComponent::EnsureBound(IWorld& WorldRef)
{
    if (ScriptModule.empty())
    {
        if (m_script)
        {
            Unbind(true);
        }
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Script module/path is empty"));
    }

    const EScriptBackend Backend = ResolveBackend();
    if (Backend == EScriptBackend::None)
    {
        if (m_script)
        {
            Unbind(true);
        }
        return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                         "Script backend could not be resolved from ScriptModule path"));
    }

    const std::uint64_t CurrentGeneration = WorldRef.Scripts().ModuleGeneration(Backend, ScriptModule);
    const bool HasBoundScript = static_cast<bool>(m_script);

    const bool NeedsRebind = !HasBoundScript
        || m_boundBackend != Backend
        || m_boundModule != ScriptModule
        || m_boundEntryPoint != ScriptType
        || (CurrentGeneration != 0 && CurrentGeneration != m_boundModuleGeneration);

    if (!NeedsRebind)
    {
        return Ok();
    }

    if (HasBoundScript)
    {
        m_pendingCreateHook = true;
        Unbind(true);
    }

    ScriptCreateInfo CreateInfo{};
    CreateInfo.ScriptPath = ScriptModule;
    CreateInfo.EntryPoint = ScriptType;
    CreateInfo.Context.World = &WorldRef;
    CreateInfo.Context.OwnerNode = OwnerNode();
    CreateInfo.Context.OwnerComponent = this;
    CreateInfo.Context.OwnerComponentType = TypeKey();

    auto CreateResult = WorldRef.Scripts().CreateScript(Backend, CreateInfo);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    m_script = *CreateResult;
    m_boundBackend = Backend;
    m_boundModule = ScriptModule;
    m_boundEntryPoint = ScriptType;
    m_boundModuleGeneration = m_script ? m_script->ModuleGeneration() : 0;
    Instance = m_script ? m_script->InstanceId() : 0;
    m_bindFailureLogged = false;

    if (m_script && m_pendingCreateHook && !m_createHookDelivered)
    {
        auto HookResult = m_script->InvokeHook(EScriptHook::OnCreate, {});
        if (!HookResult)
        {
            return std::unexpected(HookResult.error());
        }

        m_pendingCreateHook = false;
        m_createHookDelivered = true;
    }

    return Ok();
}

void ScriptComponent::Unbind(const bool InvokeDestroyHook)
{
    if (m_script && InvokeDestroyHook && m_createHookDelivered)
    {
        auto HookResult = m_script->InvokeHook(EScriptHook::OnDestroy, {});
        if (!HookResult)
        {
            LogScriptWarning(std::string("OnDestroy hook failed: ") + HookResult.error().Message);
        }
    }

    m_script.reset();
    Instance = 0;
    m_boundModule.clear();
    m_boundEntryPoint.clear();
    m_boundBackend = EScriptBackend::None;
    m_boundModuleGeneration = 0;
    m_createHookDelivered = false;
}

void ScriptComponent::InvokeHook(IWorld& WorldRef, const EScriptHook Hook, std::span<const Variant> Args)
{
    auto BindResult = EnsureBound(WorldRef);
    if (!BindResult)
    {
        if (!m_bindFailureLogged)
        {
            LogScriptWarning(std::string("Failed to bind script: ") + BindResult.error().Message);
            m_bindFailureLogged = true;
        }
        return;
    }

    if (!m_script)
    {
        return;
    }

    auto HookResult = m_script->InvokeHook(Hook, Args);
    if (!HookResult)
    {
        LogScriptWarning(std::string("Script hook failed: ") + HookResult.error().Message);
    }
}

} // namespace SnAPI::GameFramework
