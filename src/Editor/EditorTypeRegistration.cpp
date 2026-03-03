#include "Editor/EditorCameraComponent.h"
#include "Editor/EditorImportSettings.h"

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

SNAPI_REFLECT_TYPE(Editor::AssimpImportSettings, (TTypeBuilder<Editor::AssimpImportSettings>(Editor::AssimpImportSettings::kTypeName)
    .Field("GenerateNormals", &Editor::AssimpImportSettings::GenerateNormals)
    .Field("GenerateTangents", &Editor::AssimpImportSettings::GenerateTangents)
    .Field("FlipUVs", &Editor::AssimpImportSettings::FlipUVs)
    .Field("OptimizeMeshes", &Editor::AssimpImportSettings::OptimizeMeshes)
    .Field("ForceSkeletal", &Editor::AssimpImportSettings::ForceSkeletal)
    .Field("ForceStatic", &Editor::AssimpImportSettings::ForceStatic)
    .Field("MaxBonesPerVertex", &Editor::AssimpImportSettings::MaxBonesPerVertex)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Editor::TextureImportSettings, (TTypeBuilder<Editor::TextureImportSettings>(Editor::TextureImportSettings::kTypeName)
    .Field("Target", &Editor::TextureImportSettings::Target)
    .Field("FormatOverride", &Editor::TextureImportSettings::Format)
    .Field("Quality", &Editor::TextureImportSettings::Quality)
    .Field("ForceSrgb", &Editor::TextureImportSettings::ForceSrgb)
    .Field("ForceLinear", &Editor::TextureImportSettings::ForceLinear)
    .Field("ForceNormalMap", &Editor::TextureImportSettings::ForceNormalMap)
    .Field("MaxMips", &Editor::TextureImportSettings::MaxMips)
    .Constructor<>()
    .Register()));

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_INPUT && SNAPI_GF_ENABLE_RENDERER
