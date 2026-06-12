#pragma once

#include "Expected.h"
#include "Handle.h"
#include "Handles.h"

#include <vector>


namespace SnAPI::GameFramework
{
class CameraComponent;
class GameRuntime;
class GameRenderCamera;
class World;
} // namespace SnAPI::GameFramework

namespace SnAPI::GameFramework::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Creates and tracks the default editor scene bootstrap.
 *
 * `EditorSceneBootstrap` is a convenience helper that ensures an editor session starts with
 * something useful to render and manipulate. In renderer-enabled builds it is responsible for
 * creating or refreshing:
 * - an editor level
 * - an editor camera node and camera component
 * - baseline scene content such as lighting and sample primitives
 *
 * Core semantics:
 * - `Initialize()` destroys stale bootstrap-owned nodes and recreates the tracked scene.
 * - `EnsureEditorCamera()` only guarantees a camera for editor or PIE worlds.
 * - `SyncActiveCamera()` keeps the runtime renderer pointed at a live camera component when possible.
 * - In non-renderer builds the methods degrade to inert or no-op behavior.
 *
 * Ownership and lifetime:
 * - The class stores only non-owning handles into the runtime world.
 * - Tracked handles become invalid when the world is cleared, the nodes are destroyed,
 *   or `Shutdown()` is called.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see GameRuntime
 * @see CameraComponent
 */
class EditorSceneBootstrap final
{
public:
    /**
     * @brief Create or refresh the editor bootstrap scene in the runtime world.
     * @param Runtime Initialized runtime whose world receives the bootstrap content.
     * @return Success or an error.
     */
    Result Initialize(GameRuntime& Runtime);
    /**
     * @brief Destroy tracked bootstrap nodes when they still exist.
     * @param Runtime Optional runtime used for world access during teardown. May be null during late shutdown.
     */
    void Shutdown(GameRuntime* Runtime);
    /**
     * @brief Ensure that an editor camera exists in the supplied world.
     * @param WorldRef Target world.
     * @return Success or an error.
     * @remarks This is intended for editor and PIE worlds only.
     */
    Result EnsureEditorCamera(World& WorldRef);

    /**
     * @brief Synchronize the renderer's active camera choice with the world.
     * @param WorldRef Target world.
     * @remarks
     * When no active render camera can be resolved and the world is editor or PIE flavored,
     * this may create an editor camera as a fallback.
     */
    void SyncActiveCamera(World& WorldRef);

    /**
     * @brief Access the currently tracked active camera handle.
     * @return Cached camera-component handle, or null when none is known.
     */
    [[nodiscard]] ComponentHandle ActiveCameraHandle() const { return m_cameraComponent; }
    /**
     * @brief Access the currently tracked active camera component.
     * @return Non-owning pointer or `nullptr` when no active camera component is known.
     */
    [[nodiscard]] CameraComponent* ActiveCameraComponent() const;
    /**
     * @brief Access the currently tracked render-camera interface.
     * @return Non-owning pointer or `nullptr` when no active camera is available.
     */
    [[nodiscard]] GameRenderCamera* ActiveRenderCamera() const;

private:
    [[nodiscard]] CameraComponent* ResolveActiveCameraComponent(World& WorldRef) const;

    NodeHandle m_levelNode{};
    NodeHandle m_cameraNode{};
    std::vector<NodeHandle> m_sceneNodes{};
    ComponentHandle m_cameraComponent{};
};

} // namespace SnAPI::GameFramework::Editor
