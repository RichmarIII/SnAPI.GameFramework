#pragma once

#include "Editor/EditorExport.h"
#include "Editor/EditorLayout.h"
#include "Editor/IEditorService.h"
#include "Math.h"

#include <UILayout.h>

#include <cstdint>
#include <memory>

namespace SnAPI::Graphics
{
class ICamera;
class IRenderObject;
}

namespace SnAPI::GameFramework
{
class BaseNode;
struct NodeTransform;
}

namespace SnAPI::GameFramework::Editor
{

enum class EEditorTransformMode : std::uint8_t
{
    Translate = 0,
    Rotate,
    Scale
};

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorTransformInteractionService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;

    void SetMode(EEditorTransformMode Mode) { m_mode = Mode; }
    [[nodiscard]] EEditorTransformMode Mode() const { return m_mode; }
    void SetSpace(EditorLayout::EGizmoSpace Space) { m_space = Space; }
    [[nodiscard]] EditorLayout::EGizmoSpace Space() const { return m_space; }
    void SetSnappingEnabled(bool Enabled) { m_snapEnabled = Enabled; }
    [[nodiscard]] bool SnappingEnabled() const { return m_snapEnabled; }
    void SetMoveSnapStep(SnAPI::Math::Scalar Step) { m_moveSnapStep = Step; }
    [[nodiscard]] SnAPI::Math::Scalar MoveSnapStep() const { return m_moveSnapStep; }
    void SetRotateSnapDegrees(SnAPI::Math::Scalar Degrees) { m_rotateSnapDegrees = Degrees; }
    [[nodiscard]] SnAPI::Math::Scalar RotateSnapDegrees() const { return m_rotateSnapDegrees; }
    void SetScaleSnapStep(SnAPI::Math::Scalar Step) { m_scaleSnapStep = Step; }
    [[nodiscard]] SnAPI::Math::Scalar ScaleSnapStep() const { return m_scaleSnapStep; }

private:
    enum class EActiveAxis : std::uint8_t
    {
        None = 0,
        X,
        Y,
        Z
    };

#if defined(SNAPI_GF_ENABLE_RENDERER)
    void EnsureGizmoRenderObjects();
    void ConfigureGizmoGeometryForMode();
    void QueueTransformGizmos(EditorServiceContext& Context,
                              BaseNode* SelectedNode,
                              const NodeTransform& SelectedTransform,
                              SnAPI::Graphics::ICamera& Camera,
                              std::uint64_t ViewportID);
    [[nodiscard]] EActiveAxis PickGizmoAxis(EditorServiceContext& Context,
                                            float ScreenX,
                                            float ScreenY,
                                            const SnAPI::UI::UIRect& ViewRect,
                                            std::uint64_t ViewportID) const;
#endif

    EEditorTransformMode m_mode = EEditorTransformMode::Translate;
    EditorLayout::EGizmoSpace m_space = EditorLayout::EGizmoSpace::World;
    bool m_snapEnabled = false;
    SnAPI::Math::Scalar m_moveSnapStep = static_cast<SnAPI::Math::Scalar>(1.0);
    SnAPI::Math::Scalar m_rotateSnapDegrees = static_cast<SnAPI::Math::Scalar>(15.0);
    SnAPI::Math::Scalar m_scaleSnapStep = static_cast<SnAPI::Math::Scalar>(0.5);
    bool m_dragging = false;
    EActiveAxis m_activeAxis = EActiveAxis::None;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
    SnAPI::Math::Scalar m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
    SnAPI::Math::Scalar m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
    bool m_freeMovePlaneActive = false;
    Vec3 m_freeMovePlaneNormal = Vec3::UnitZ();
    Vec3 m_freeMoveNodeStart = Vec3::Zero();
    Vec3 m_freeMoveHitStart = Vec3::Zero();
    bool m_axisMovePlaneActive = false;
    Vec3 m_axisMovePlaneNormal = Vec3::UnitZ();
    Vec3 m_axisMoveAxisDirection = Vec3::UnitX();
    Vec3 m_axisMoveNodeStart = Vec3::Zero();
    Vec3 m_axisMoveHitStart = Vec3::Zero();
#if defined(SNAPI_GF_ENABLE_RENDERER)
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisX{};
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisY{};
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisZ{};
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisXAux{};
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisYAux{};
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_gizmoAxisZAux{};
    std::uint32_t m_gizmoAxisXID = 0;
    std::uint32_t m_gizmoAxisYID = 0;
    std::uint32_t m_gizmoAxisZID = 0;
    std::uint32_t m_gizmoAxisXAuxID = 0;
    std::uint32_t m_gizmoAxisYAuxID = 0;
    std::uint32_t m_gizmoAxisZAuxID = 0;
    EEditorTransformMode m_gizmoGeometryMode = EEditorTransformMode::Translate;
#endif
};

} // namespace SnAPI::GameFramework::Editor
