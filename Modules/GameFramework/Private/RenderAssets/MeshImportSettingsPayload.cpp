#include "RenderAssets/MeshImportSettingsPayload.h"

#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    MeshImportSettingsPayload,
    (TTypeBuilder<MeshImportSettingsPayload>(MeshImportSettingsPayload::kTypeName)
        .Field("GenerateNormals", &MeshImportSettingsPayload::GenerateNormals, EFieldFlagBits::Serialized)
        .Field("GenerateTangents", &MeshImportSettingsPayload::GenerateTangents, EFieldFlagBits::Serialized)
        .Field("FlipUVs", &MeshImportSettingsPayload::FlipUVs, EFieldFlagBits::Serialized)
        .Field("OptimizeMeshes", &MeshImportSettingsPayload::OptimizeMeshes, EFieldFlagBits::Serialized)
        .Field("ForceSkeletal", &MeshImportSettingsPayload::ForceSkeletal, EFieldFlagBits::Serialized)
        .Field("ForceStatic", &MeshImportSettingsPayload::ForceStatic, EFieldFlagBits::Serialized)
        .Field("ImportMaterials", &MeshImportSettingsPayload::ImportMaterials, EFieldFlagBits::Serialized)
        .Field("ImportTextures", &MeshImportSettingsPayload::ImportTextures, EFieldFlagBits::Serialized)
        .Field("ImportAnimations", &MeshImportSettingsPayload::ImportAnimations, EFieldFlagBits::Serialized)
        .Field("ImportSkeleton", &MeshImportSettingsPayload::ImportSkeleton, EFieldFlagBits::Serialized)
        .Field("MaxBonesPerVertex", &MeshImportSettingsPayload::MaxBonesPerVertex, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

} // namespace SnAPI::GameFramework
