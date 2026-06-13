#include "RenderAssets/ImportedAssetProvenancePayload.h"

#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    ImportBuildOptionPayload,
    (TTypeBuilder<ImportBuildOptionPayload>(ImportBuildOptionPayload::kTypeName)
        .Field("Key", &ImportBuildOptionPayload::Key, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("Value", &ImportBuildOptionPayload::Value, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    ImportedAssetProvenancePayload,
    (TTypeBuilder<ImportedAssetProvenancePayload>(ImportedAssetProvenancePayload::kTypeName)
        .Field("Profile", &ImportedAssetProvenancePayload::Profile, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field(
            "SourcePath",
            &ImportedAssetProvenancePayload::SourcePath,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field(
            "DestinationFolder",
            &ImportedAssetProvenancePayload::DestinationFolder,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field(
            "ImporterName",
            &ImportedAssetProvenancePayload::ImporterName,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field(
            "BuildOptions",
            &ImportedAssetProvenancePayload::BuildOptions,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly | EFieldEditorFlagBits::Advanced)
        .Constructor<>()
        .Register()));

} // namespace SnAPI::GameFramework
