#include "Editor/EditorCameraComponent.h"

#include "TypeAutoRegistration.h"

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_RENDERER)

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(EditorCameraComponent::Settings, (TTypeBuilder<EditorCameraComponent::Settings>(EditorCameraComponent::Settings::kTypeName)
    .Field("Enabled", &EditorCameraComponent::Settings::Enabled)
    .Field("RequireInputFocus", &EditorCameraComponent::Settings::RequireInputFocus)
    .Field("RequireRightMouseButton", &EditorCameraComponent::Settings::RequireRightMouseButton)
    .Field("RequirePointerInsideViewport", &EditorCameraComponent::Settings::RequirePointerInsideViewport)
    .Field("MoveSpeed", &EditorCameraComponent::Settings::MoveSpeed)
    .Field("FastMoveMultiplier", &EditorCameraComponent::Settings::FastMoveMultiplier)
    .Field("LookSensitivity", &EditorCameraComponent::Settings::LookSensitivity)
    .Field("InvertY", &EditorCameraComponent::Settings::InvertY)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(EditorCameraComponent, (TTypeBuilder<EditorCameraComponent>(EditorCameraComponent::kTypeName)
    .Field("Settings",
           &EditorCameraComponent::EditSettings,
           &EditorCameraComponent::GetSettings)
    .Constructor<>()
    .Register()));

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_INPUT && SNAPI_GF_ENABLE_RENDERER
