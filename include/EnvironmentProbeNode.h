#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <string>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::GameFramework
{
/**
 * @ingroup SnAPI_GameFramework
 * @brief Scene node that composes the default environment-probe authoring stack.
 *
 * `EnvironmentProbeNode` is the editor-facing convenience node for probe placement. On creation it
 * ensures three components exist:
 * - `TransformComponent`
 * - `EnvironmentCaptureComponent`
 * - `StaticMeshComponent` configured with a sphere vertex source as the preview mesh
 *
 * The preview sphere is configured as a non-shadow-casting reflective sphere so the currently active
 * environment probe can be inspected directly in the editor.
 *
 * @see EnvironmentCaptureComponent
 * @see StaticMeshComponent
 * @see TransformComponent
 */
class SNAPI_GAMEFRAMEWORK_API EnvironmentProbeNode final : public BaseNode, public NodeCRTP<EnvironmentProbeNode>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::EnvironmentProbeNode";

    EnvironmentProbeNode();
    explicit EnvironmentProbeNode(std::string Name);

    void OnCreate();
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    void EditorTick(float DeltaSeconds);
#endif

private:
    void EnsureDefaultComponents();
    void RefreshPreviewMeshState();
};
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
