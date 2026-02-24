#pragma once

#include "Expected.h"
#include "Handle.h"
#include "Handles.h"

#include <vector>

namespace SnAPI::Graphics
{
class ICamera;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{
class CameraComponent;
class GameRuntime;
class World;
} // namespace SnAPI::GameFramework

namespace SnAPI::GameFramework::Editor
{

/**
 * @brief Creates and tracks a minimal editor scene bootstrap.
 */
class EditorSceneBootstrap final
{
public:
    Result Initialize(GameRuntime& Runtime);
    void Shutdown(GameRuntime* Runtime);

    void SyncActiveCamera(World& WorldRef);

    [[nodiscard]] CameraComponent* ActiveCameraComponent() const;
    [[nodiscard]] SnAPI::Graphics::ICamera* ActiveRenderCamera() const;

private:
    [[nodiscard]] CameraComponent* ResolveActiveCameraComponent(World& WorldRef) const;

    NodeHandle m_levelNode{};
    NodeHandle m_cameraNode{};
    std::vector<NodeHandle> m_sceneNodes{};
    CameraComponent* m_cameraComponent = nullptr;
};

} // namespace SnAPI::GameFramework::Editor
