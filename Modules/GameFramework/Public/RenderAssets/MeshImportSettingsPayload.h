#pragma once

#include <cstdint>

#include "ReflectionAnnotations.h"
#include "TypeName.h"

namespace SnAPI::GameFramework
{

SnType(SnDisplayName("Mesh Import Settings"))
struct MeshImportSettingsPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::MeshImportSettingsPayload";

    SnField(SnDisplayName("Generate Normals"))
    bool GenerateNormals = true;
    SnField(SnDisplayName("Generate Tangents"))
    bool GenerateTangents = true;
    SnField(SnDisplayName("Flip UVs"))
    bool FlipUVs = false;
    SnField(SnDisplayName("Optimize Meshes"))
    bool OptimizeMeshes = true;
    SnField(SnDisplayName("Force Skeletal"))
    bool ForceSkeletal = false;
    SnField(SnDisplayName("Force Static"))
    bool ForceStatic = false;
    SnField(SnDisplayName("Import Materials"))
    bool ImportMaterials = true;
    SnField(SnDisplayName("Import Textures"))
    bool ImportTextures = true;
    SnField(SnDisplayName("Import Animations"))
    bool ImportAnimations = true;
    SnField(SnDisplayName("Import Skeleton"))
    bool ImportSkeleton = true;
    SnField(SnDisplayName("Max Bones Per Vertex"))
    uint32_t MaxBonesPerVertex = 4;

    bool operator==(const MeshImportSettingsPayload&) const = default;
};

SNAPI_DEFINE_TYPE_NAME(MeshImportSettingsPayload, "SnAPI::GameFramework::MeshImportSettingsPayload")

} // namespace SnAPI::GameFramework
