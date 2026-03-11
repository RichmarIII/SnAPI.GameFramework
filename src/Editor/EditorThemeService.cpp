#include "Editor/EditorThemeService.h"

namespace SnAPI::GameFramework::Editor
{
std::string_view EditorThemeService::Name() const
{
    return "EditorThemeService";
}

Result EditorThemeService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_theme.Initialize();
    return Ok();
}

void EditorThemeService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
}


} // namespace SnAPI::GameFramework::Editor
