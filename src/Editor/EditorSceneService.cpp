#include "Editor/EditorSceneService.h"

#include "GameRuntime.h"

namespace SnAPI::GameFramework::Editor
{
std::string_view EditorSceneService::Name() const
{
    return "EditorSceneService";
}

Result EditorSceneService::Initialize(EditorServiceContext& Context)
{
    return m_scene.Initialize(Context.Runtime());
}

void EditorSceneService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;
    if (auto* WorldPtr = Context.Runtime().WorldPtr())
    {
        m_scene.SyncActiveCamera(*WorldPtr);
    }
}

void EditorSceneService::Shutdown(EditorServiceContext& Context)
{
    m_scene.Shutdown(&Context.Runtime());
}

Result EditorSceneService::EnsureEditorCamera(EditorServiceContext& Context)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }
    return m_scene.EnsureEditorCamera(*WorldPtr);
}

ComponentHandle EditorSceneService::ActiveCameraHandle() const
{
    return m_scene.ActiveCameraHandle();
}

CameraComponent* EditorSceneService::ActiveCameraComponent() const
{
    return m_scene.ActiveCameraComponent();
}

GameRenderCamera* EditorSceneService::ActiveRenderCamera() const
{
    return m_scene.ActiveRenderCamera();
}

} // namespace SnAPI::GameFramework::Editor
