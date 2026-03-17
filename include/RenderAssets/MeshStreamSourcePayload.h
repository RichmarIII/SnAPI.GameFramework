#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ReflectionAnnotations.h"
#include "RenderAssets/StaticMeshPayload.h"
#include "TypeName.h"

namespace SnAPI::GameFramework
{

SnType()
struct MeshStreamSourcePayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::MeshStreamSourcePayload";

    SnField(SnKey("Semantic"), SnReadOnly)
    EMeshStreamSemantic Semantic = EMeshStreamSemantic::Position;
    SnField(SnKey("SubIndex"), SnReadOnly)
    uint32_t SubIndex = 0;
    SnField(SnKey("Uri"), SnReadOnly)
    std::string Uri{};
    SnField(SnKey("Bytes"), SnHidden, SnHeavyData)
    std::vector<uint8_t> Bytes{};
    SnField(SnKey("ElementCount"), SnReadOnly)
    uint32_t ElementCount = 0;
    SnField(SnKey("StrideBytes"), SnReadOnly)
    uint32_t StrideBytes = 0;
    SnField(SnKey("Compress"), SnReadOnly)
    bool Compress = true;

    bool operator==(const MeshStreamSourcePayload&) const = default;
};

SNAPI_DEFINE_TYPE_NAME(std::vector<MeshStreamSourcePayload>, "std::vector<SnAPI::GameFramework::MeshStreamSourcePayload>")

} // namespace SnAPI::GameFramework
