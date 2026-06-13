#pragma once

#include "Editor/EditorExport.h"
#include "Editor/EditorViewportBinding.h"
#include "Editor/IEditorService.h"

namespace SnAPI::GameFramework::Editor
{

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorRootViewportService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;

private:
    EditorViewportBinding m_binding{};
};

} // namespace SnAPI::GameFramework::Editor
