#include "Editor/EditorCameraComponent.h"
#include "Editor/EditorImportSettings.h"

#include "TypeAutoRegistration.h"
#include "TypeRegistry.h"

#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_RENDERER)

namespace SnAPI::GameFramework
{

namespace
{
void RegisterEditorEnum(
    const char* Name,
    const size_t Size,
    const size_t Align,
    const bool IsSigned,
    const std::initializer_list<EnumValueInfo> Values)
{
    TypeInfo Info{};
    Info.Id = TypeIdFromName(Name);
    Info.Name = Name;
    Info.Size = Size;
    Info.Align = Align;
    Info.IsEnum = true;
    Info.EnumIsSigned = IsSigned;
    Info.EnumValues.assign(Values.begin(), Values.end());
    (void)TypeRegistry::Instance().Register(std::move(Info));
}

[[maybe_unused]] const bool g_editorEnumsRegistered = []() {
    RegisterEditorEnum(
        TTypeNameV<Editor::ETextureCompressionTarget>,
        sizeof(Editor::ETextureCompressionTarget),
        alignof(Editor::ETextureCompressionTarget),
        false,
        {
            EnumValueInfo{"BCn", static_cast<std::uint64_t>(Editor::ETextureCompressionTarget::BCn)},
            EnumValueInfo{"ASTC", static_cast<std::uint64_t>(Editor::ETextureCompressionTarget::ASTC)},
        });

    RegisterEditorEnum(
        TTypeNameV<Editor::ETextureCompressionFormat>,
        sizeof(Editor::ETextureCompressionFormat),
        alignof(Editor::ETextureCompressionFormat),
        false,
        {
            EnumValueInfo{"Auto", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::Auto)},
            EnumValueInfo{"BC1", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::BC1)},
            EnumValueInfo{"BC3", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::BC3)},
            EnumValueInfo{"BC4", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::BC4)},
            EnumValueInfo{"BC5", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::BC5)},
            EnumValueInfo{"BC6H", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::BC6H)},
            EnumValueInfo{"BC7", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::BC7)},
            EnumValueInfo{"ASTC_4x4", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::ASTC_4x4)},
            EnumValueInfo{"ASTC_5x5", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::ASTC_5x5)},
            EnumValueInfo{"ASTC_6x6", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::ASTC_6x6)},
            EnumValueInfo{"ASTC_8x8", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::ASTC_8x8)},
            EnumValueInfo{"ASTC_10x10", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::ASTC_10x10)},
            EnumValueInfo{"ASTC_12x12", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::ASTC_12x12)},
            EnumValueInfo{"ASTC_4x4_HDR", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::ASTC_4x4_HDR)},
            EnumValueInfo{"ASTC_6x6_HDR", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::ASTC_6x6_HDR)},
            EnumValueInfo{"ASTC_8x8_HDR", static_cast<std::uint64_t>(Editor::ETextureCompressionFormat::ASTC_8x8_HDR)},
        });
    return true;
}();
} // namespace

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
    .Field("ImportMaterials", &Editor::AssimpImportSettings::ImportMaterials)
    .Field("ImportTextures", &Editor::AssimpImportSettings::ImportTextures)
    .Field("ImportAnimations", &Editor::AssimpImportSettings::ImportAnimations)
    .Field("ImportSkeleton", &Editor::AssimpImportSettings::ImportSkeleton)
    .Field("MaxBonesPerVertex", &Editor::AssimpImportSettings::MaxBonesPerVertex)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Editor::TextureImportSettings, (TTypeBuilder<Editor::TextureImportSettings>(Editor::TextureImportSettings::kTypeName)
    .Field("Target", &Editor::TextureImportSettings::Target)
    .Field("Format", &Editor::TextureImportSettings::Format)
    .Field("Quality", &Editor::TextureImportSettings::Quality)
    .Field("ForceSrgb", &Editor::TextureImportSettings::ForceSrgb)
    .Field("ForceLinear", &Editor::TextureImportSettings::ForceLinear)
    .Field("ForceNormalMap", &Editor::TextureImportSettings::ForceNormalMap)
    .Field("MaxMips", &Editor::TextureImportSettings::MaxMips)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Editor::TextureAssetEditorPayload, (TTypeBuilder<Editor::TextureAssetEditorPayload>(Editor::TextureAssetEditorPayload::kTypeName)
    .Field("Target", &Editor::TextureAssetEditorPayload::Target)
    .Field("Format", &Editor::TextureAssetEditorPayload::Format)
    .Field("Quality", &Editor::TextureAssetEditorPayload::Quality)
    .Field("Width", &Editor::TextureAssetEditorPayload::Width)
    .Field("Height", &Editor::TextureAssetEditorPayload::Height)
    .Field("MipCount", &Editor::TextureAssetEditorPayload::MipCount)
    .Field("SRGB", &Editor::TextureAssetEditorPayload::SRGB)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Editor::StaticMeshAssetEditorPayload, (TTypeBuilder<Editor::StaticMeshAssetEditorPayload>(Editor::StaticMeshAssetEditorPayload::kTypeName)
    .Field("Name", &Editor::StaticMeshAssetEditorPayload::Name)
    .Constructor<>()
    .Register()));

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_INPUT && SNAPI_GF_ENABLE_RENDERER
