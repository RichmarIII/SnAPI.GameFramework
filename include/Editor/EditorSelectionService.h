#pragma once

#include "Editor/EditorExport.h"
#include "Editor/EditorSceneService.h"
#include "Editor/EditorSelectionModel.h"
#include "Editor/IEditorService.h"

namespace SnAPI::GameFramework
{
class CameraComponent;
}

namespace SnAPI::GameFramework::Editor
{

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorSelectionService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;

    [[nodiscard]] EditorSelectionModel& Model() { return m_selection; }
    [[nodiscard]] const EditorSelectionModel& Model() const { return m_selection; }

private:
    void EnsureSelectionValid(EditorServiceContext& Context, ComponentHandle ActiveCamera);

    EditorSelectionModel m_selection{};
};

} // namespace SnAPI::GameFramework::Editor
