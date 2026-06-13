#include "RenderAssets/TextureSourceImagePayload.h"

#include "TypeAutoRegistration.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(
    TextureSourceImagePayload,
    (TTypeBuilder<TextureSourceImagePayload>(TextureSourceImagePayload::kTypeName)
        .Field("Width", &TextureSourceImagePayload::Width, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("Height", &TextureSourceImagePayload::Height, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field(
            "Channels",
            &TextureSourceImagePayload::Channels,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field(
            "BitsPerChannel",
            &TextureSourceImagePayload::BitsPerChannel,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field("IsFloat", &TextureSourceImagePayload::IsFloat, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field(
            "HasNonTrivialAlpha",
            &TextureSourceImagePayload::HasNonTrivialAlpha,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field("SRGB", &TextureSourceImagePayload::SRGB, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field(
            "SourceFilename",
            &TextureSourceImagePayload::SourceFilename,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::ReadOnly)
        .Field(
            "EncodedBytes",
            &TextureSourceImagePayload::EncodedBytes,
            EFieldFlagBits::Serialized,
            EFieldEditorFlagBits::Hidden | EFieldEditorFlagBits::HeavyData)
        .Constructor<>()
        .Register()));

} // namespace SnAPI::GameFramework
