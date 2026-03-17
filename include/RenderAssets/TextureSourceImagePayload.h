#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ReflectionAnnotations.h"

namespace SnAPI::GameFramework
{

SnType()
struct TextureSourceImagePayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TextureSourceImagePayload";

    SnField(SnKey("Width"))
    uint32_t Width = 0;
    SnField(SnKey("Height"))
    uint32_t Height = 0;
    SnField(SnKey("Channels"))
    uint32_t Channels = 4;
    SnField(SnKey("BitsPerChannel"))
    uint32_t BitsPerChannel = 8;
    SnField(SnKey("IsFloat"))
    bool IsFloat = false;
    SnField(SnKey("HasNonTrivialAlpha"))
    bool HasNonTrivialAlpha = false;
    SnField(SnKey("SRGB"))
    bool SRGB = true;
    SnField(SnKey("SourceFilename"))
    std::string SourceFilename{};
    SnField(SnKey("Pixels"), SnHidden, SnHeavyData)
    std::vector<uint8_t> Pixels{};

    bool operator==(const TextureSourceImagePayload&) const = default;
};

} // namespace SnAPI::GameFramework
