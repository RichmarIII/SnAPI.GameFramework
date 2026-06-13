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

    SnField(SnKey("Width"), SnReadOnly)
    uint32_t Width = 0;
    SnField(SnKey("Height"), SnReadOnly)
    uint32_t Height = 0;
    SnField(SnKey("Channels"), SnReadOnly)
    uint32_t Channels = 4;
    SnField(SnKey("BitsPerChannel"), SnReadOnly)
    uint32_t BitsPerChannel = 8;
    SnField(SnKey("IsFloat"), SnReadOnly)
    bool IsFloat = false;
    SnField(SnKey("HasNonTrivialAlpha"), SnReadOnly)
    bool HasNonTrivialAlpha = false;
    SnField(SnKey("SRGB"), SnReadOnly)
    bool SRGB = true;
    SnField(SnKey("SourceFilename"), SnReadOnly)
    std::string SourceFilename{};
    SnField(SnKey("EncodedBytes"), SnHidden, SnHeavyData)
    std::vector<uint8_t> EncodedBytes{};
    std::vector<uint8_t> Pixels{}; // Transient import scratch data. Saved payloads store EncodedBytes.

    bool operator==(const TextureSourceImagePayload& Other) const
    {
        return Width == Other.Width &&
               Height == Other.Height &&
               Channels == Other.Channels &&
               BitsPerChannel == Other.BitsPerChannel &&
               IsFloat == Other.IsFloat &&
               HasNonTrivialAlpha == Other.HasNonTrivialAlpha &&
               SRGB == Other.SRGB &&
               SourceFilename == Other.SourceFilename &&
               EncodedBytes == Other.EncodedBytes;
    }
};

} // namespace SnAPI::GameFramework
