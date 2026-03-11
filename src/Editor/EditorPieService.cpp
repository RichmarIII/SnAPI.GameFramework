#include "Editor/EditorPieService.h"

#include "BaseNode.h"
#include "CameraComponent.h"
#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_RENDERER)
#include "Editor/EditorCameraComponent.h"
#endif
#include "GameRuntime.h"
#include "World.h"

namespace SnAPI::GameFramework::Editor
{
namespace
{
void SetEditorCameraEnabledForPie(World& WorldRef, const bool Enabled)
{
#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_RENDERER)
    WorldRef.NodePool().ForEach([Enabled](const NodeHandle&, BaseNode& Node) {
        auto EditorCamera = Node.Component<EditorCameraComponent>();
        if (!EditorCamera)
        {
            return;
        }

        EditorCamera->EditSettings().Enabled = Enabled;

        auto Camera = Node.Component<CameraComponent>();
        if (!Camera)
        {
            return;
        }

        Camera->SetActive(Enabled);
    });
#else
    (void)WorldRef;
    (void)Enabled;
#endif
}


} // namespace

std::string_view EditorPieService::Name() const
{
    return "EditorPieService";
}

Result EditorPieService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_state = EState::Stopped;
    m_editorSnapshot.reset();
    m_editorWorldKind = EWorldKind::Editor;
    m_editorExecutionProfile = WorldExecutionProfile::Editor();
    return Ok();
}

void EditorPieService::Shutdown(EditorServiceContext& Context)
{
    (void)StopSession(Context);
    m_state = EState::Stopped;
    m_editorSnapshot.reset();
}

Result EditorPieService::Play(EditorServiceContext& Context)
{
    if (m_state == EState::Playing)
    {
        return Ok();
    }

    if (m_state == EState::Paused)
    {
        return ResumeSession(Context);
    }

    return StartSession(Context);
}

Result EditorPieService::Pause(EditorServiceContext& Context)
{
    if (m_state != EState::Playing)
    {
        return Ok();
    }

    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    WorldPtr->SetWorldKind(EWorldKind::PIE);
    WorldPtr->SetExecutionProfile(PausedExecutionProfile());
    m_state = EState::Paused;
    return Ok();
}

Result EditorPieService::Stop(EditorServiceContext& Context)
{
    if (m_state == EState::Stopped)
    {
        return Ok();
    }

    return StopSession(Context);
}

Result EditorPieService::StartSession(EditorServiceContext& Context)
{
    auto& Runtime = Context.Runtime();
    Runtime.StopGameplayHost();

    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    auto SnapshotResult = WorldSerializer::Serialize(*WorldPtr);
    if (!SnapshotResult)
    {
        return std::unexpected(SnapshotResult.error());
    }

    m_editorSnapshot = std::move(*SnapshotResult);
    m_editorWorldKind = WorldPtr->Kind();
    m_editorExecutionProfile = WorldPtr->ExecutionProfile();

    WorldPtr->SetWorldKind(EWorldKind::PIE);
    WorldPtr->SetExecutionProfile(WorldExecutionProfile::PIE());

#if defined(SNAPI_GF_ENABLE_RENDERER)
    (void)WorldPtr->Renderer().SetActiveCamera(nullptr);
#endif

    TDeserializeOptions PieOptions{};
    PieOptions.RegenerateObjectIds = true;
    auto PieLoadResult = WorldSerializer::Deserialize(*m_editorSnapshot, *WorldPtr, PieOptions);
    if (!PieLoadResult)
    {
        TDeserializeOptions RestoreOptions{};
        RestoreOptions.RegenerateObjectIds = false;
        (void)WorldSerializer::Deserialize(*m_editorSnapshot, *WorldPtr, RestoreOptions);
        WorldPtr->SetWorldKind(m_editorWorldKind);
        WorldPtr->SetExecutionProfile(m_editorExecutionProfile);
        return std::unexpected(PieLoadResult.error());
    }

    SetEditorCameraEnabledForPie(*WorldPtr, false);

    if (Runtime.Settings().Gameplay.has_value())
    {
        auto StartGameplayResult = Runtime.StartGameplayHost();
        if (!StartGameplayResult)
        {
            TDeserializeOptions RestoreOptions{};
            RestoreOptions.RegenerateObjectIds = false;
            (void)WorldSerializer::Deserialize(*m_editorSnapshot, *WorldPtr, RestoreOptions);
            WorldPtr->SetWorldKind(m_editorWorldKind);
            WorldPtr->SetExecutionProfile(m_editorExecutionProfile);
            return std::unexpected(StartGameplayResult.error());
        }
    }

    m_state = EState::Playing;
    return Ok();
}

Result EditorPieService::ResumeSession(EditorServiceContext& Context)
{
    auto& Runtime = Context.Runtime();
    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    WorldPtr->SetWorldKind(EWorldKind::PIE);
    WorldPtr->SetExecutionProfile(WorldExecutionProfile::PIE());
    SetEditorCameraEnabledForPie(*WorldPtr, false);

    if (Runtime.Settings().Gameplay.has_value())
    {
        auto StartGameplayResult = Runtime.StartGameplayHost();
        if (!StartGameplayResult)
        {
            return std::unexpected(StartGameplayResult.error());
        }
    }

    m_state = EState::Playing;
    return Ok();
}

Result EditorPieService::StopSession(EditorServiceContext& Context)
{
    auto& Runtime = Context.Runtime();
    Runtime.StopGameplayHost();

    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        m_state = EState::Stopped;
        m_editorSnapshot.reset();
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    if (!m_editorSnapshot.has_value())
    {
        WorldPtr->SetWorldKind(m_editorWorldKind);
        WorldPtr->SetExecutionProfile(m_editorExecutionProfile);
        SetEditorCameraEnabledForPie(*WorldPtr, true);
        m_state = EState::Stopped;
        return Ok();
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    (void)WorldPtr->Renderer().SetActiveCamera(nullptr);
#endif

    TDeserializeOptions RestoreOptions{};
    RestoreOptions.RegenerateObjectIds = false;
    auto RestoreResult = WorldSerializer::Deserialize(*m_editorSnapshot, *WorldPtr, RestoreOptions);
    if (!RestoreResult)
    {
        return std::unexpected(RestoreResult.error());
    }

    WorldPtr->SetWorldKind(m_editorWorldKind);
    WorldPtr->SetExecutionProfile(m_editorExecutionProfile);
    m_editorSnapshot.reset();
    m_state = EState::Stopped;
    return Ok();
}

WorldExecutionProfile EditorPieService::PausedExecutionProfile()
{
    auto Profile = WorldExecutionProfile::PIE();
    Profile.RunGameplay = false;
    Profile.TickPhysicsSimulation = false;
    Profile.TickAudio = false;
    Profile.PumpNetworking = false;
    return Profile;
}

} // namespace SnAPI::GameFramework::Editor
