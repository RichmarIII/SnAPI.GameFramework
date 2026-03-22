#include "RenderAssets/TextureAsset.h"

#include "PayloadBinarySerialization.h"
#include "TypeAutoRegistration.h"
#include <TextureCompressorPayloads.h>

#if defined(SNAPI_GF_HAS_FREEIMAGE) && SNAPI_GF_HAS_FREEIMAGE
#include <FreeImage.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] bool HasEncodedSourceBytes(const TextureSourceImagePayload& Image)
{
    return !Image.EncodedBytes.empty();
}

#if defined(SNAPI_GF_HAS_FREEIMAGE) && SNAPI_GF_HAS_FREEIMAGE
void EnsureFreeImageInitialized()
{
    static std::once_flag InitOnce{};
    std::call_once(InitOnce, [] {
        FreeImage_Initialise(FALSE);
    });
}

[[nodiscard]] bool DetectNonTrivialAlphaFloat(const float* Pixels, const std::size_t PixelCount)
{
    if (!Pixels)
    {
        return false;
    }

    for (std::size_t PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
    {
        const float Alpha = Pixels[PixelIndex * 4u + 3u];
        if (std::abs(Alpha - 1.0f) > 1.0e-6f)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] TExpected<void> SaveBitmapToMemory(const FREE_IMAGE_FORMAT Format,
                                                 FIBITMAP* Bitmap,
                                                 std::vector<std::uint8_t>& OutBytes)
{
    if (!Bitmap)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Texture bitmap is null"));
    }

    FIMEMORY* Memory = FreeImage_OpenMemory();
    if (!Memory)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to open FreeImage memory stream"));
    }

    const BOOL Saved = FreeImage_SaveToMemory(Format, Bitmap, Memory, 0);
    if (Saved == FALSE)
    {
        FreeImage_CloseMemory(Memory);
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to encode texture image to memory"));
    }

    BYTE* MemoryBytes = nullptr;
    DWORD MemorySize = 0u;
    FreeImage_AcquireMemory(Memory, &MemoryBytes, &MemorySize);
    OutBytes.assign(MemoryBytes, MemoryBytes + static_cast<std::size_t>(MemorySize));
    FreeImage_CloseMemory(Memory);
    return Ok();
}

[[nodiscard]] TExpected<void> EncodeLegacyPixelsToBytes(const TextureSourceImagePayload& Image,
                                                        std::vector<std::uint8_t>& OutBytes)
{
    EnsureFreeImageInitialized();

    if (Image.Width == 0u || Image.Height == 0u)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Texture image dimensions are invalid"));
    }

    if (Image.IsFloat)
    {
        const std::size_t PixelCount = static_cast<std::size_t>(Image.Width) * static_cast<std::size_t>(Image.Height);
        const std::size_t ExpectedBytes = PixelCount * 4u * sizeof(float);
        if (Image.Pixels.size() < ExpectedBytes)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Float texture image pixel payload is truncated"));
        }

        FIBITMAP* Bitmap = FreeImage_AllocateT(FIT_RGBAF, static_cast<int>(Image.Width), static_cast<int>(Image.Height));
        if (!Bitmap)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to allocate float bitmap for texture encoding"));
        }

        for (std::uint32_t Y = 0; Y < Image.Height; ++Y)
        {
            float* ScanLine = reinterpret_cast<float*>(FreeImage_GetScanLine(Bitmap, static_cast<int>(Image.Height - 1u - Y)));
            const float* SourceRow = reinterpret_cast<const float*>(Image.Pixels.data()) +
                                     static_cast<std::size_t>(Y) * static_cast<std::size_t>(Image.Width) * 4u;
            std::memcpy(ScanLine,
                        SourceRow,
                        static_cast<std::size_t>(Image.Width) * 4u * sizeof(float));
        }

        auto SaveResult = SaveBitmapToMemory(FIF_EXR, Bitmap, OutBytes);
        FreeImage_Unload(Bitmap);
        return SaveResult;
    }

    if (Image.BitsPerChannel != 8u || Image.Channels == 0u || Image.Channels > 4u)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported texture image layout for encoded source persistence"));
    }

    const std::size_t PixelCount = static_cast<std::size_t>(Image.Width) * static_cast<std::size_t>(Image.Height);
    const std::size_t ExpectedBytes = PixelCount * static_cast<std::size_t>(Image.Channels);
    if (Image.Pixels.size() < ExpectedBytes)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Texture image pixel payload is truncated"));
    }

    FIBITMAP* Bitmap = FreeImage_Allocate(static_cast<int>(Image.Width), static_cast<int>(Image.Height), 32);
    if (!Bitmap)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to allocate bitmap for texture encoding"));
    }

    for (std::uint32_t Y = 0; Y < Image.Height; ++Y)
    {
        BYTE* ScanLine = FreeImage_GetScanLine(Bitmap, static_cast<int>(Image.Height - 1u - Y));
        for (std::uint32_t X = 0; X < Image.Width; ++X)
        {
            const std::size_t SourceIndex =
                (static_cast<std::size_t>(Y) * static_cast<std::size_t>(Image.Width) + static_cast<std::size_t>(X)) *
                static_cast<std::size_t>(Image.Channels);
            const std::uint8_t R = Image.Pixels[SourceIndex + 0u];
            const std::uint8_t G = Image.Channels >= 2u ? Image.Pixels[SourceIndex + 1u] : R;
            const std::uint8_t B = Image.Channels >= 3u ? Image.Pixels[SourceIndex + 2u] : G;
            const std::uint8_t A = Image.Channels >= 4u ? Image.Pixels[SourceIndex + 3u] : 255u;
            ScanLine[X * 4u + FI_RGBA_RED] = R;
            ScanLine[X * 4u + FI_RGBA_GREEN] = G;
            ScanLine[X * 4u + FI_RGBA_BLUE] = B;
            ScanLine[X * 4u + FI_RGBA_ALPHA] = A;
        }
    }

    auto SaveResult = SaveBitmapToMemory(FIF_PNG, Bitmap, OutBytes);
    FreeImage_Unload(Bitmap);
    return SaveResult;
}

[[nodiscard]] TExpected<void> DecodeEncodedBytesToIntermediate(const TextureSourceImagePayload& Image,
                                                               TextureCompressorPlugin::ImageIntermediate& OutIntermediate)
{
    EnsureFreeImageInitialized();

    if (Image.EncodedBytes.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Texture image encoded source bytes are empty"));
    }

    FIMEMORY* Memory = FreeImage_OpenMemory(const_cast<BYTE*>(Image.EncodedBytes.data()),
                                            static_cast<DWORD>(Image.EncodedBytes.size()));
    if (!Memory)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to open FreeImage memory stream"));
    }

    FREE_IMAGE_FORMAT Format = FreeImage_GetFileTypeFromMemory(Memory, 0);
    if (Format == FIF_UNKNOWN && !Image.SourceFilename.empty())
    {
        Format = FreeImage_GetFIFFromFilename(Image.SourceFilename.c_str());
    }

    if (Format == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(Format))
    {
        FreeImage_CloseMemory(Memory);
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unable to detect encoded texture image format"));
    }

    FIBITMAP* Bitmap = FreeImage_LoadFromMemory(Format, Memory, 0);
    FreeImage_CloseMemory(Memory);
    if (!Bitmap)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Failed to decode encoded texture image"));
    }

    const FREE_IMAGE_TYPE ImageType = FreeImage_GetImageType(Bitmap);
    const bool IsFloatSource = (ImageType == FIT_RGBF || ImageType == FIT_RGBAF || ImageType == FIT_FLOAT);

    OutIntermediate = {};
    OutIntermediate.SourceFilename = Image.SourceFilename;
    OutIntermediate.Width = FreeImage_GetWidth(Bitmap);
    OutIntermediate.Height = FreeImage_GetHeight(Bitmap);
    OutIntermediate.bSRGB = Image.SRGB;

    if (IsFloatSource)
    {
        FIBITMAP* FloatBitmap = FreeImage_ConvertToRGBAF(Bitmap);
        FreeImage_Unload(Bitmap);
        if (!FloatBitmap)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to convert texture image to RGBAF"));
        }

        OutIntermediate.Channels = 4u;
        OutIntermediate.BitsPerChannel = 32u;
        OutIntermediate.bIsFloat = true;
        OutIntermediate.Pixels.resize(
            static_cast<std::size_t>(OutIntermediate.Width) * static_cast<std::size_t>(OutIntermediate.Height) * 4u *
            sizeof(float));

        const std::uint32_t Pitch = FreeImage_GetPitch(FloatBitmap);
        const BYTE* Bits = FreeImage_GetBits(FloatBitmap);
        for (std::uint32_t Y = 0; Y < OutIntermediate.Height; ++Y)
        {
            const std::uint32_t SrcY = OutIntermediate.Height - 1u - Y;
            const float* SourceRow = reinterpret_cast<const float*>(Bits + static_cast<std::size_t>(SrcY) * Pitch);
            float* TargetRow = reinterpret_cast<float*>(OutIntermediate.Pixels.data()) +
                               static_cast<std::size_t>(Y) * static_cast<std::size_t>(OutIntermediate.Width) * 4u;
            std::memcpy(TargetRow,
                        SourceRow,
                        static_cast<std::size_t>(OutIntermediate.Width) * 4u * sizeof(float));
        }

        OutIntermediate.bHasNonTrivialAlpha = DetectNonTrivialAlphaFloat(
            reinterpret_cast<const float*>(OutIntermediate.Pixels.data()),
            static_cast<std::size_t>(OutIntermediate.Width) * static_cast<std::size_t>(OutIntermediate.Height));

        FreeImage_Unload(FloatBitmap);
        return Ok();
    }

    FIBITMAP* Bitmap32 = FreeImage_ConvertTo32Bits(Bitmap);
    FreeImage_Unload(Bitmap);
    if (!Bitmap32)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to convert texture image to 32-bit RGBA"));
    }

    OutIntermediate.Channels = 4u;
    OutIntermediate.BitsPerChannel = 8u;
    OutIntermediate.bIsFloat = false;
    OutIntermediate.Pixels.resize(
        static_cast<std::size_t>(OutIntermediate.Width) * static_cast<std::size_t>(OutIntermediate.Height) * 4u);

    const std::uint32_t Pitch = FreeImage_GetPitch(Bitmap32);
    const BYTE* Bits = FreeImage_GetBits(Bitmap32);
    bool HasAlpha = false;
    for (std::uint32_t Y = 0; Y < OutIntermediate.Height; ++Y)
    {
        const std::uint32_t SrcY = OutIntermediate.Height - 1u - Y;
        const BYTE* SourceRow = Bits + static_cast<std::size_t>(SrcY) * Pitch;
        for (std::uint32_t X = 0; X < OutIntermediate.Width; ++X)
        {
            const std::size_t TargetIndex =
                (static_cast<std::size_t>(Y) * static_cast<std::size_t>(OutIntermediate.Width) + static_cast<std::size_t>(X)) * 4u;
            OutIntermediate.Pixels[TargetIndex + 0u] = SourceRow[X * 4u + FI_RGBA_RED];
            OutIntermediate.Pixels[TargetIndex + 1u] = SourceRow[X * 4u + FI_RGBA_GREEN];
            OutIntermediate.Pixels[TargetIndex + 2u] = SourceRow[X * 4u + FI_RGBA_BLUE];
            OutIntermediate.Pixels[TargetIndex + 3u] = SourceRow[X * 4u + FI_RGBA_ALPHA];
            HasAlpha = HasAlpha || SourceRow[X * 4u + FI_RGBA_ALPHA] != 255u;
        }
    }

    OutIntermediate.bHasNonTrivialAlpha = HasAlpha;
    FreeImage_Unload(Bitmap32);
    return Ok();
}
#endif

[[nodiscard]] TExpected<void> DecodeLegacyPixelsToIntermediate(const TextureSourceImagePayload& Image,
                                                               TextureCompressorPlugin::ImageIntermediate& OutIntermediate)
{
    if (Image.Width == 0u || Image.Height == 0u)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Texture image dimensions are invalid"));
    }
    if (Image.Pixels.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Texture image pixel payload is empty"));
    }

    OutIntermediate = {};
    OutIntermediate.SourceFilename = Image.SourceFilename;
    OutIntermediate.Width = Image.Width;
    OutIntermediate.Height = Image.Height;
    OutIntermediate.bSRGB = Image.SRGB;
    OutIntermediate.bIsFloat = Image.IsFloat;
    OutIntermediate.bHasNonTrivialAlpha = Image.HasNonTrivialAlpha;

    if (Image.IsFloat)
    {
        const std::size_t ExpectedBytes =
            static_cast<std::size_t>(Image.Width) * static_cast<std::size_t>(Image.Height) * 4u * sizeof(float);
        if (Image.Pixels.size() < ExpectedBytes)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Float texture pixel payload is truncated"));
        }

        OutIntermediate.Channels = 4u;
        OutIntermediate.BitsPerChannel = 32u;
        OutIntermediate.Pixels.assign(Image.Pixels.begin(), Image.Pixels.begin() + static_cast<std::ptrdiff_t>(ExpectedBytes));
        return Ok();
    }

    if (Image.BitsPerChannel != 8u || Image.Channels == 0u || Image.Channels > 4u)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported legacy texture pixel layout"));
    }

    const std::size_t PixelCount = static_cast<std::size_t>(Image.Width) * static_cast<std::size_t>(Image.Height);
    const std::size_t ExpectedBytes = PixelCount * static_cast<std::size_t>(Image.Channels);
    if (Image.Pixels.size() < ExpectedBytes)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Texture pixel payload is truncated"));
    }

    OutIntermediate.Channels = 4u;
    OutIntermediate.BitsPerChannel = 8u;
    OutIntermediate.Pixels.resize(PixelCount * 4u);
    for (std::size_t PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
    {
        const std::uint8_t* Source = &Image.Pixels[PixelIndex * static_cast<std::size_t>(Image.Channels)];
        std::uint8_t* Target = &OutIntermediate.Pixels[PixelIndex * 4u];
        Target[0] = Source[0];
        Target[1] = Image.Channels >= 2u ? Source[1] : Source[0];
        Target[2] = Image.Channels >= 3u ? Source[2] : Target[1];
        Target[3] = Image.Channels >= 4u ? Source[3] : 255u;
    }
    return Ok();
}
} // namespace

SNAPI_REFLECT_TYPE(
    TextureAsset,
    (TTypeBuilder<TextureAsset>(TextureAsset::kTypeName)
        .Base<IAsset>()
        .Field("Image", &TextureAsset::Image, EFieldFlagBits::Serialized, EFieldEditorFlagBits::ReadOnly)
        .Field("ImportSettings", &TextureAsset::ImportSettings, EFieldFlagBits::Serialized, EFieldEditorFlagBits::Hidden)
        .Field("Provenance", &TextureAsset::Provenance, EFieldFlagBits::Serialized, EFieldEditorFlagBits::Advanced)
        .Constructor<>()
        .Register()));

Result TextureAsset::Save(std::ostream& Output) const
{
    TextureAsset Normalized = *this;
    if (auto EnsureResult = EnsureTextureSourceImageEncoded(Normalized.Image); !EnsureResult)
    {
        return std::unexpected(EnsureResult.error());
    }

    return Detail::SaveAuthoredAssetJson(Normalized, Output);
}

TExpected<void> SerializeTextureSourcePayload(const TextureAsset& Payload, std::vector<uint8_t>& OutBytes)
{
    TextureAsset Normalized = Payload;
    if (auto EnsureResult = EnsureTextureSourceImageEncoded(Normalized.Image); !EnsureResult)
    {
        return std::unexpected(EnsureResult.error());
    }

    return Detail::SerializeBinaryPayload(Normalized, OutBytes);
}

TExpected<TextureAsset> DeserializeTextureSourcePayload(const uint8_t* Bytes, const size_t Size)
{
    return Detail::DeserializeBinaryPayload<TextureAsset>(Bytes, Size, "Null source payload bytes");
}

TExpected<void> PopulateTextureSourceImageFromIntermediate(
    TextureSourceImagePayload& OutImage,
    const TextureCompressorPlugin::ImageIntermediate& Intermediate,
    const std::vector<std::uint8_t>* PreferredEncodedBytes)
{
    OutImage.Width = Intermediate.Width;
    OutImage.Height = Intermediate.Height;
    OutImage.Channels = Intermediate.Channels;
    OutImage.BitsPerChannel = Intermediate.BitsPerChannel;
    OutImage.IsFloat = Intermediate.bIsFloat;
    OutImage.HasNonTrivialAlpha = Intermediate.bHasNonTrivialAlpha;
    OutImage.SRGB = Intermediate.bSRGB;
    OutImage.SourceFilename = Intermediate.SourceFilename;
    OutImage.Pixels = Intermediate.Pixels;
    OutImage.EncodedBytes.clear();

    if (PreferredEncodedBytes != nullptr && !PreferredEncodedBytes->empty())
    {
        OutImage.EncodedBytes = *PreferredEncodedBytes;
        OutImage.Pixels.clear();
        return Ok();
    }

    if (auto EnsureResult = EnsureTextureSourceImageEncoded(OutImage); !EnsureResult)
    {
        return EnsureResult;
    }
    return Ok();
}

TExpected<void> EnsureTextureSourceImageEncoded(TextureSourceImagePayload& Image)
{
    if (HasEncodedSourceBytes(Image))
    {
        Image.Pixels.clear();
        Image.Pixels.shrink_to_fit();
        return Ok();
    }
    if (Image.Pixels.empty())
    {
        if (Image.Width == 0u && Image.Height == 0u)
        {
            return Ok();
        }
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Texture image has no encoded bytes or legacy pixels"));
    }

#if defined(SNAPI_GF_HAS_FREEIMAGE) && SNAPI_GF_HAS_FREEIMAGE
    auto EncodeResult = EncodeLegacyPixelsToBytes(Image, Image.EncodedBytes);
    if (!EncodeResult)
    {
        return EncodeResult;
    }
    Image.Pixels.clear();
    Image.Pixels.shrink_to_fit();
    return Ok();
#else
    return std::unexpected(MakeError(EErrorCode::NotSupported, "FreeImage is required to encode authored texture sources"));
#endif
}

TExpected<void> DecodeTextureSourceImageToIntermediate(const TextureSourceImagePayload& Image,
                                                       TextureCompressorPlugin::ImageIntermediate& OutIntermediate)
{
    if (HasEncodedSourceBytes(Image))
    {
#if defined(SNAPI_GF_HAS_FREEIMAGE) && SNAPI_GF_HAS_FREEIMAGE
        return DecodeEncodedBytesToIntermediate(Image, OutIntermediate);
#else
        return std::unexpected(MakeError(EErrorCode::NotSupported, "FreeImage is required to decode authored texture sources"));
#endif
    }

    return DecodeLegacyPixelsToIntermediate(Image, OutIntermediate);
}

TExpected<void> DecodeTextureSourceImageToRgba(const TextureSourceImagePayload& Image,
                                               std::vector<std::uint8_t>& OutPixels)
{
    TextureCompressorPlugin::ImageIntermediate Intermediate{};
    if (auto DecodeResult = DecodeTextureSourceImageToIntermediate(Image, Intermediate); !DecodeResult)
    {
        return DecodeResult;
    }

    if (Intermediate.bIsFloat || Intermediate.BitsPerChannel != 8u)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Texture image is not an 8-bit LDR texture"));
    }
    if (Intermediate.Channels != 4u)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Decoded texture image is not RGBA8"));
    }

    OutPixels = std::move(Intermediate.Pixels);
    return Ok();
}

} // namespace SnAPI::GameFramework
