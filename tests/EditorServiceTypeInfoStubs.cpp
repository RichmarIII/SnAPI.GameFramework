#include "Editor/EditorLayoutService.h"
#include "Editor/EditorSceneService.h"

namespace SnAPI::GameFramework::Editor
{

/**
 * @brief Test-only stub implementation that anchors RTTI/vtable emission for `EditorLayoutService`.
 *
 * `GameFrameworkEditorTests` links `EditorAssetService.cpp` directly, but it intentionally does
 * not link the full layout stack. `EditorAssetService` still instantiates `GetService<T>()` for
 * `EditorLayoutService`, which requires the class typeinfo to exist at link time. This shim
 * provides the out-of-line virtual definitions needed by the test target without pulling the full
 * production layout implementation into the editor test executable.
 */
std::string_view EditorLayoutService::Name() const
{
    return "EditorLayoutService";
}

/**
 * @brief Test-only dependency list for the layout-service shim.
 * @return Empty dependency list because the shim never participates in real service startup.
 */
std::vector<std::type_index> EditorLayoutService::Dependencies() const
{
    return {};
}

/**
 * @brief No-op test initialization hook for the layout-service shim.
 * @param Context Unused editor-service context.
 * @return Success.
 */
Result EditorLayoutService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    return Ok();
}

/**
 * @brief No-op test tick for the layout-service shim.
 * @param Context Unused editor-service context.
 * @param DeltaSeconds Unused frame delta.
 */
void EditorLayoutService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)Context;
    (void)DeltaSeconds;
}

/**
 * @brief No-op test shutdown hook for the layout-service shim.
 * @param Context Unused editor-service context.
 */
void EditorLayoutService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
}

/**
 * @brief Return no viewport for the layout-service shim.
 * @return Always `nullptr`.
 */
UIRenderViewport* EditorLayoutService::GameViewportElement() const
{
    return nullptr;
}

/**
 * @brief Return the default tab index for the layout-service shim.
 * @return Always `-1`.
 */
int32_t EditorLayoutService::GameViewportTabIndex() const
{
    return -1;
}

/**
 * @brief Return the default gizmo space for the layout-service shim.
 * @return World-space gizmo mode.
 */
EditorLayout::EGizmoSpace EditorLayoutService::GizmoSpace() const
{
    return EditorLayout::EGizmoSpace::World;
}

/**
 * @brief Report snapping as disabled in the layout-service shim.
 * @return Always `false`.
 */
bool EditorLayoutService::GizmoSnappingEnabled() const
{
    return false;
}

/**
 * @brief Return the default move snap step for the layout-service shim.
 * @return Always `0.0`.
 */
double EditorLayoutService::MoveSnapStep() const
{
    return 0.0;
}

/**
 * @brief Return the default rotation snap step for the layout-service shim.
 * @return Always `0.0`.
 */
double EditorLayoutService::RotateSnapStepDegrees() const
{
    return 0.0;
}

/**
 * @brief Return the default scale snap step for the layout-service shim.
 * @return Always `0.0`.
 */
double EditorLayoutService::ScaleSnapStep() const
{
    return 0.0;
}

/**
 * @brief Test-only stub implementation that anchors RTTI/vtable emission for `EditorSceneService`.
 *
 * The editor asset tests do not boot the full editor scene stack, but `EditorAssetService`
 * instantiates `GetService<T>()` for `EditorSceneService` in viewport-placement helpers. This
 * shim keeps those references linkable while remaining intentionally inert.
 */
std::string_view EditorSceneService::Name() const
{
    return "EditorSceneService";
}

/**
 * @brief No-op test initialization hook for the scene-service shim.
 * @param Context Unused editor-service context.
 * @return Success.
 */
Result EditorSceneService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    return Ok();
}

/**
 * @brief No-op test tick for the scene-service shim.
 * @param Context Unused editor-service context.
 * @param DeltaSeconds Unused frame delta.
 */
void EditorSceneService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)Context;
    (void)DeltaSeconds;
}

/**
 * @brief No-op test shutdown hook for the scene-service shim.
 * @param Context Unused editor-service context.
 */
void EditorSceneService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
}

/**
 * @brief Report that no editor camera is available in the scene-service shim.
 * @param Context Unused editor-service context.
 * @return Not-ready error because the shim does not create a scene camera.
 */
Result EditorSceneService::EnsureEditorCamera(EditorServiceContext& Context)
{
    (void)Context;
    return std::unexpected(MakeError(EErrorCode::NotReady, "EditorSceneService test shim does not create cameras"));
}

/**
 * @brief Return no active camera handle from the scene-service shim.
 * @return Null handle.
 */
ComponentHandle EditorSceneService::ActiveCameraHandle() const
{
    return {};
}

/**
 * @brief Return no active camera component from the scene-service shim.
 * @return Always `nullptr`.
 */
CameraComponent* EditorSceneService::ActiveCameraComponent() const
{
    return nullptr;
}

/**
 * @brief Return no active render camera from the scene-service shim.
 * @return Always `nullptr`.
 */
GameRenderCamera* EditorSceneService::ActiveRenderCamera() const
{
    return nullptr;
}

} // namespace SnAPI::GameFramework::Editor
