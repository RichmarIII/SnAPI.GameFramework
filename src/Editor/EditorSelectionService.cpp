#include "Editor/EditorSelectionService.h"

#include "CameraComponent.h"
#include "GameRuntime.h"

namespace SnAPI::GameFramework::Editor
{
std::string_view EditorSelectionService::Name() const
{
    return "EditorSelectionService";
}

std::vector<std::type_index> EditorSelectionService::Dependencies() const
{
    return {std::type_index(typeid(EditorSceneService))};
}

Result EditorSelectionService::Initialize(EditorServiceContext& Context)
{
    m_selection.Clear();
    auto* SceneService = Context.GetService<EditorSceneService>();
    EnsureSelectionValid(Context, SceneService != nullptr ? SceneService->ActiveCameraComponent() : nullptr);
    return Ok();
}

void EditorSelectionService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;
    auto* SceneService = Context.GetService<EditorSceneService>();
    EnsureSelectionValid(Context, SceneService != nullptr ? SceneService->ActiveCameraComponent() : nullptr);
}

void EditorSelectionService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    m_selection.Clear();
}

void EditorSelectionService::EnsureSelectionValid(EditorServiceContext& Context, CameraComponent* ActiveCamera)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        m_selection.Clear();
        return;
    }

    const NodeHandle SelectedNode = m_selection.SelectedNode();
    if (!SelectedNode.IsNull())
    {
        if (auto* Resolved = m_selection.ResolveSelectedNode(*WorldPtr))
        {
            const NodeHandle ResolvedHandle = Resolved->Handle();
            if (!ResolvedHandle.IsNull() && ResolvedHandle != SelectedNode)
            {
                (void)m_selection.SelectNode(ResolvedHandle);
            }
            return;
        }
    }

    if (ActiveCamera && !ActiveCamera->Owner().IsNull())
    {
        (void)m_selection.SelectNode(ActiveCamera->Owner());
        return;
    }

    m_selection.Clear();
}


} // namespace SnAPI::GameFramework::Editor
