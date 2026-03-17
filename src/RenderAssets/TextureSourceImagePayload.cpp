#include "RenderAssets/TextureSourceImagePayload.h"

#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    TextureSourceImagePayload,
    (TTypeBuilder<TextureSourceImagePayload>(TextureSourceImagePayload::kTypeName)
        .Field("Width", &TextureSourceImagePayload::Width, EFieldFlagBits::Serialized)
        .Field("Height", &TextureSourceImagePayload::Height, EFieldFlagBits::Serialized)
        .Field("Channels", &TextureSourceImagePayload::Channels, EFieldFlagBits::Serialized)
        .Field("BitsPerChannel", &TextureSourceImagePayload::BitsPerChannel, EFieldFlagBits::Serialized)
        .Field("IsFloat", &TextureSourceImagePayload::IsFloat, EFieldFlagBits::Serialized)
        .Field("HasNonTrivialAlpha", &TextureSourceImagePayload::HasNonTrivialAlpha, EFieldFlagBits::Serialized)
        .Field("SRGB", &TextureSourceImagePayload::SRGB, EFieldFlagBits::Serialized)
        .Field("SourceFilename", &TextureSourceImagePayload::SourceFilename, EFieldFlagBits::Serialized)
        .Field("Pixels", &TextureSourceImagePayload::Pixels, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

} // namespace SnAPI::GameFramework
