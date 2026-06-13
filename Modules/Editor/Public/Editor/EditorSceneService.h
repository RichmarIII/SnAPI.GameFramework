#pragma once

#include "Editor/EditorExport.h"
#include "Editor/EditorSceneBootstrap.h"
#include "Editor/IEditorService.h"


namespace SnAPI::GameFramework
{
class CameraComponent;
class GameRenderCamera;
}

namespace SnAPI::GameFramework::Editor
{

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorSceneService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;
    Result EnsureEditorCamera(EditorServiceContext& Context);

    [[nodiscard]] ComponentHandle ActiveCameraHandle() const;
    [[nodiscard]] CameraComponent* ActiveCameraComponent() const;
    [[nodiscard]] GameRenderCamera* ActiveRenderCamera() const;

private:
    EditorSceneBootstrap m_scene{};
};

} // namespace SnAPI::GameFramework::Editor
