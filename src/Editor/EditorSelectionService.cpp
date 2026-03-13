#include "Editor/EditorSelectionService.h"

#include "CameraComponent.h"
#include "GameRuntime.h"
#include "World.h"

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
    EnsureSelectionValid(Context, SceneService != nullptr ? SceneService->ActiveCameraHandle() : ComponentHandle{});
    return Ok();
}

void EditorSelectionService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;
    auto* SceneService = Context.GetService<EditorSceneService>();
    EnsureSelectionValid(Context, SceneService != nullptr ? SceneService->ActiveCameraHandle() : ComponentHandle{});
}

void EditorSelectionService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    m_selection.Clear();
}

void EditorSelectionService::EnsureSelectionValid(EditorServiceContext& Context, ComponentHandle ActiveCamera)
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

    auto* ActiveCameraComponent = ActiveCamera.IsNull()
        ? nullptr
        : static_cast<CameraComponent*>(WorldPtr->BorrowedComponent(ActiveCamera));
    if (ActiveCameraComponent && !ActiveCameraComponent->Owner().IsNull())
    {
        (void)m_selection.SelectNode(ActiveCameraComponent->Owner());
        return;
    }

    m_selection.Clear();
}


} // namespace SnAPI::GameFramework::Editor
