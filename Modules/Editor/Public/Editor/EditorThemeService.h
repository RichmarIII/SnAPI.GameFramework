#pragma once

#include "Editor/EditorExport.h"
#include "Editor/EditorTheme.h"
#include "Editor/IEditorService.h"

namespace SnAPI::GameFramework::Editor
{

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorThemeService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Shutdown(EditorServiceContext& Context) override;

    [[nodiscard]] EditorTheme& Theme() { return m_theme; }
    [[nodiscard]] const EditorTheme& Theme() const { return m_theme; }

private:
    EditorTheme m_theme{};
};

} // namespace SnAPI::GameFramework::Editor
