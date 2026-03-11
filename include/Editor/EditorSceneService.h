#pragma once

#include "Editor/EditorExport.h"
#include "Editor/EditorSceneBootstrap.h"
#include "Editor/IEditorService.h"

namespace SnAPI::Graphics
{
class ICamera;
}

namespace SnAPI::GameFramework
{
class CameraComponent;
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

    [[nodiscard]] CameraComponent* ActiveCameraComponent() const;
    [[nodiscard]] SnAPI::Graphics::ICamera* ActiveRenderCamera() const;

private:
    EditorSceneBootstrap m_scene{};
};

} // namespace SnAPI::GameFramework::Editor
