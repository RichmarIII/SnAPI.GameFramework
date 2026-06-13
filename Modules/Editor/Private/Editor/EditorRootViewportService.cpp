#include "Editor/EditorRootViewportService.h"

#include "GameRuntime.h"

namespace SnAPI::GameFramework::Editor
{
std::string_view EditorRootViewportService::Name() const
{
    return "EditorRootViewportService";
}

Result EditorRootViewportService::Initialize(EditorServiceContext& Context)
{
    return m_binding.Initialize(Context.Runtime(), "Editor.RootViewport");
}

void EditorRootViewportService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;
    (void)m_binding.SyncToWindow(Context.Runtime());
}

void EditorRootViewportService::Shutdown(EditorServiceContext& Context)
{
    m_binding.Shutdown(&Context.Runtime());
}


} // namespace SnAPI::GameFramework::Editor
