#pragma once

#include <string>
#include <vector>

#include "ReflectionAnnotations.h"
#include "TypeName.h"

namespace SnAPI::GameFramework
{

SnType()
struct ImportBuildOptionPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ImportBuildOptionPayload";

    SnField(SnKey("Key"), SnReadOnly)
    std::string Key{};
    SnField(SnKey("Value"), SnReadOnly)
    std::string Value{};

    bool operator==(const ImportBuildOptionPayload&) const = default;
};

SnType()
struct ImportedAssetProvenancePayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ImportedAssetProvenancePayload";

    SnField(SnKey("Profile"), SnReadOnly)
    std::string Profile{};
    SnField(SnKey("SourcePath"), SnReadOnly)
    std::string SourcePath{};
    SnField(SnKey("DestinationFolder"), SnReadOnly)
    std::string DestinationFolder{};
    SnField(SnKey("ImporterName"), SnReadOnly)
    std::string ImporterName{};
    SnField(SnKey("BuildOptions"), SnReadOnly, SnAdvanced)
    std::vector<ImportBuildOptionPayload> BuildOptions{};

    bool operator==(const ImportedAssetProvenancePayload&) const = default;
};

SNAPI_DEFINE_TYPE_NAME(std::vector<ImportBuildOptionPayload>, "std::vector<SnAPI::GameFramework::ImportBuildOptionPayload>")

} // namespace SnAPI::GameFramework
