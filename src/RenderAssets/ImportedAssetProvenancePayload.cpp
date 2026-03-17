#include "RenderAssets/ImportedAssetProvenancePayload.h"

#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    ImportBuildOptionPayload,
    (TTypeBuilder<ImportBuildOptionPayload>(ImportBuildOptionPayload::kTypeName)
        .Field("Key", &ImportBuildOptionPayload::Key, EFieldFlagBits::Serialized)
        .Field("Value", &ImportBuildOptionPayload::Value, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    ImportedAssetProvenancePayload,
    (TTypeBuilder<ImportedAssetProvenancePayload>(ImportedAssetProvenancePayload::kTypeName)
        .Field("Profile", &ImportedAssetProvenancePayload::Profile, EFieldFlagBits::Serialized)
        .Field("SourcePath", &ImportedAssetProvenancePayload::SourcePath, EFieldFlagBits::Serialized)
        .Field("DestinationFolder", &ImportedAssetProvenancePayload::DestinationFolder, EFieldFlagBits::Serialized)
        .Field("ImporterName", &ImportedAssetProvenancePayload::ImporterName, EFieldFlagBits::Serialized)
        .Field("BuildOptions", &ImportedAssetProvenancePayload::BuildOptions, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

} // namespace SnAPI::GameFramework
