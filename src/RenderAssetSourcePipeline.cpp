#include "AssetPipelineIds.h"
#include "RenderAssetPayloads.h"
#include "RenderAssetSourcePayloads.h"

#include "IAssetCooker.h"
#include "IAssetImporter.h"
#include "IPipelineContext.h"
#include "IPayloadSerializer.h"

#include <TextureCompressorIds.h>
#include <TextureCompressorImportSettings.h>
#include <TextureCompressorPayloads.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace TextureCompressorPlugin
{
std::unique_ptr<SnAPI::AssetPipeline::IAssetCooker> CreateTextureCompressorCooker();
}

namespace SnAPI::GameFramework
{
namespace
{
enum class EJsonValueType : uint8_t
{
    Null = 0,
    Bool,
    Number,
    String,
    Array,
    Object,
};

struct JsonValue
{
    EJsonValueType Type = EJsonValueType::Null;
    bool BoolValue = false;
    double NumberValue = 0.0;
    std::string StringValue{};
    std::vector<JsonValue> ArrayValue{};
    std::unordered_map<std::string, JsonValue> ObjectValue{};
};

class JsonParser
{
public:
    explicit JsonParser(std::string_view Text)
        : m_text(Text)
    {
    }

    std::expected<JsonValue, std::string> ParseRoot()
    {
        SkipWhitespace();
        auto Root = ParseValue();
        if (!Root)
        {
            return Root;
        }

        SkipWhitespace();
        if (!IsEof())
        {
            return std::unexpected("Unexpected trailing JSON content");
        }

        return Root;
    }

private:
    [[nodiscard]] bool IsEof() const
    {
        return m_pos >= m_text.size();
    }

    [[nodiscard]] char Peek() const
    {
        return IsEof() ? '\0' : m_text[m_pos];
    }

    [[nodiscard]] char Take()
    {
        return IsEof() ? '\0' : m_text[m_pos++];
    }

    void SkipWhitespace()
    {
        while (!IsEof() && std::isspace(static_cast<unsigned char>(Peek())) != 0)
        {
            ++m_pos;
        }
    }

    std::expected<JsonValue, std::string> ParseValue()
    {
        SkipWhitespace();
        if (IsEof())
        {
            return std::unexpected("Unexpected end of JSON");
        }

        const char Ch = Peek();
        if (Ch == '"')
        {
            auto Parsed = ParseString();
            if (!Parsed)
            {
                return std::unexpected(Parsed.error());
            }
            JsonValue Out{};
            Out.Type = EJsonValueType::String;
            Out.StringValue = std::move(*Parsed);
            return Out;
        }
        if (Ch == '{')
        {
            return ParseObject();
        }
        if (Ch == '[')
        {
            return ParseArray();
        }
        if (Ch == 't')
        {
            if (!ConsumeKeyword("true"))
            {
                return std::unexpected("Invalid token; expected true");
            }
            JsonValue Out{};
            Out.Type = EJsonValueType::Bool;
            Out.BoolValue = true;
            return Out;
        }
        if (Ch == 'f')
        {
            if (!ConsumeKeyword("false"))
            {
                return std::unexpected("Invalid token; expected false");
            }
            JsonValue Out{};
            Out.Type = EJsonValueType::Bool;
            Out.BoolValue = false;
            return Out;
        }
        if (Ch == 'n')
        {
            if (!ConsumeKeyword("null"))
            {
                return std::unexpected("Invalid token; expected null");
            }
            JsonValue Out{};
            Out.Type = EJsonValueType::Null;
            return Out;
        }

        if (Ch == '-' || std::isdigit(static_cast<unsigned char>(Ch)) != 0)
        {
            return ParseNumber();
        }

        return std::unexpected("Unsupported JSON token");
    }

    std::expected<JsonValue, std::string> ParseObject()
    {
        if (Take() != '{')
        {
            return std::unexpected("Expected '{'");
        }

        JsonValue Out{};
        Out.Type = EJsonValueType::Object;

        SkipWhitespace();
        if (Peek() == '}')
        {
            (void)Take();
            return Out;
        }

        while (!IsEof())
        {
            SkipWhitespace();
            auto Key = ParseString();
            if (!Key)
            {
                return std::unexpected(Key.error());
            }

            SkipWhitespace();
            if (Take() != ':')
            {
                return std::unexpected("Expected ':' after object key");
            }

            auto Value = ParseValue();
            if (!Value)
            {
                return Value;
            }
            Out.ObjectValue[*Key] = std::move(*Value);

            SkipWhitespace();
            const char Delim = Take();
            if (Delim == '}')
            {
                return Out;
            }
            if (Delim != ',')
            {
                return std::unexpected("Expected ',' or '}' in object");
            }
        }

        return std::unexpected("Unterminated JSON object");
    }

    std::expected<JsonValue, std::string> ParseArray()
    {
        if (Take() != '[')
        {
            return std::unexpected("Expected '['");
        }

        JsonValue Out{};
        Out.Type = EJsonValueType::Array;

        SkipWhitespace();
        if (Peek() == ']')
        {
            (void)Take();
            return Out;
        }

        while (!IsEof())
        {
            auto Value = ParseValue();
            if (!Value)
            {
                return Value;
            }
            Out.ArrayValue.push_back(std::move(*Value));

            SkipWhitespace();
            const char Delim = Take();
            if (Delim == ']')
            {
                return Out;
            }
            if (Delim != ',')
            {
                return std::unexpected("Expected ',' or ']' in array");
            }
        }

        return std::unexpected("Unterminated JSON array");
    }

    std::expected<std::string, std::string> ParseString()
    {
        if (Take() != '"')
        {
            return std::unexpected("Expected JSON string");
        }

        std::string Out{};
        while (!IsEof())
        {
            const char Ch = Take();
            if (Ch == '"')
            {
                return Out;
            }
            if (Ch != '\\')
            {
                Out.push_back(Ch);
                continue;
            }

            if (IsEof())
            {
                return std::unexpected("Invalid escape sequence");
            }

            const char Esc = Take();
            switch (Esc)
            {
            case '"':
                Out.push_back('"');
                break;
            case '\\':
                Out.push_back('\\');
                break;
            case '/':
                Out.push_back('/');
                break;
            case 'b':
                Out.push_back('\b');
                break;
            case 'f':
                Out.push_back('\f');
                break;
            case 'n':
                Out.push_back('\n');
                break;
            case 'r':
                Out.push_back('\r');
                break;
            case 't':
                Out.push_back('\t');
                break;
            default:
                return std::unexpected("Unsupported JSON escape sequence");
            }
        }

        return std::unexpected("Unterminated JSON string");
    }

    std::expected<JsonValue, std::string> ParseNumber()
    {
        const size_t Begin = m_pos;
        if (Peek() == '-')
        {
            (void)Take();
        }
        while (std::isdigit(static_cast<unsigned char>(Peek())) != 0)
        {
            (void)Take();
        }

        if (Peek() == '.')
        {
            (void)Take();
            while (std::isdigit(static_cast<unsigned char>(Peek())) != 0)
            {
                (void)Take();
            }
        }

        if (Peek() == 'e' || Peek() == 'E')
        {
            (void)Take();
            if (Peek() == '+' || Peek() == '-')
            {
                (void)Take();
            }
            while (std::isdigit(static_cast<unsigned char>(Peek())) != 0)
            {
                (void)Take();
            }
        }

        const std::string Token(m_text.substr(Begin, m_pos - Begin));
        try
        {
            JsonValue Out{};
            Out.Type = EJsonValueType::Number;
            Out.NumberValue = std::stod(Token);
            return Out;
        }
        catch (...)
        {
            return std::unexpected("Invalid JSON number");
        }
    }

    [[nodiscard]] bool ConsumeKeyword(std::string_view Keyword)
    {
        if (m_text.substr(m_pos, Keyword.size()) != Keyword)
        {
            return false;
        }
        m_pos += Keyword.size();
        return true;
    }

    std::string_view m_text;
    size_t m_pos = 0;
};

[[nodiscard]] std::string ToLowerAscii(std::string_view Value)
{
    std::string Out(Value);
    std::transform(Out.begin(), Out.end(), Out.begin(), [](const unsigned char Ch) {
        return static_cast<char>(std::tolower(Ch));
    });
    return Out;
}

[[nodiscard]] bool EndsWithInsensitive(const std::string& Value, std::string_view Suffix)
{
    if (Suffix.size() > Value.size())
    {
        return false;
    }

    const size_t Start = Value.size() - Suffix.size();
    for (size_t I = 0; I < Suffix.size(); ++I)
    {
        const char A = static_cast<char>(std::tolower(static_cast<unsigned char>(Value[Start + I])));
        const char B = static_cast<char>(std::tolower(static_cast<unsigned char>(Suffix[I])));
        if (A != B)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] const JsonValue* TryGetField(const JsonValue& Object, std::string_view Key)
{
    if (Object.Type != EJsonValueType::Object)
    {
        return nullptr;
    }

    const auto It = Object.ObjectValue.find(std::string(Key));
    return (It == Object.ObjectValue.end()) ? nullptr : &It->second;
}

[[nodiscard]] bool TryReadString(const JsonValue& Value, std::string& Out)
{
    if (Value.Type != EJsonValueType::String)
    {
        return false;
    }
    Out = Value.StringValue;
    return true;
}

[[nodiscard]] bool TryReadBool(const JsonValue& Value, bool& Out)
{
    if (Value.Type != EJsonValueType::Bool)
    {
        return false;
    }
    Out = Value.BoolValue;
    return true;
}

[[nodiscard]] bool TryReadUnsigned(const JsonValue& Value, uint32_t& Out)
{
    if (Value.Type != EJsonValueType::Number || Value.NumberValue < 0.0)
    {
        return false;
    }

    if (Value.NumberValue > static_cast<double>(std::numeric_limits<uint32_t>::max()))
    {
        return false;
    }

    Out = static_cast<uint32_t>(Value.NumberValue);
    return true;
}

[[nodiscard]] bool TryReadSigned(const JsonValue& Value, int32_t& Out)
{
    if (Value.Type != EJsonValueType::Number)
    {
        return false;
    }

    if (Value.NumberValue < static_cast<double>(std::numeric_limits<int32_t>::min()) ||
        Value.NumberValue > static_cast<double>(std::numeric_limits<int32_t>::max()))
    {
        return false;
    }

    Out = static_cast<int32_t>(Value.NumberValue);
    return true;
}

[[nodiscard]] bool TryReadFloat(const JsonValue& Value, float& Out)
{
    if (Value.Type != EJsonValueType::Number)
    {
        return false;
    }
    Out = static_cast<float>(Value.NumberValue);
    return true;
}

template<size_t N>
[[nodiscard]] bool TryReadFloatArray(const JsonValue& Value, std::array<float, N>& Out)
{
    if (Value.Type != EJsonValueType::Array || Value.ArrayValue.size() != N)
    {
        return false;
    }

    for (size_t I = 0; I < N; ++I)
    {
        if (Value.ArrayValue[I].Type != EJsonValueType::Number)
        {
            return false;
        }
        Out[I] = static_cast<float>(Value.ArrayValue[I].NumberValue);
    }

    return true;
}

[[nodiscard]] bool ParseAssetRefPayload(const JsonValue& Value, AssetRefPayload& OutRef)
{
    if (Value.Type == EJsonValueType::String)
    {
        OutRef.AssetName = Value.StringValue;
        OutRef.AssetId.clear();
        return true;
    }

    if (Value.Type != EJsonValueType::Object)
    {
        return false;
    }

    if (const JsonValue* Name = TryGetField(Value, "assetName"); Name && Name->Type == EJsonValueType::String)
    {
        OutRef.AssetName = Name->StringValue;
    }
    if (const JsonValue* Id = TryGetField(Value, "assetId"); Id && Id->Type == EJsonValueType::String)
    {
        OutRef.AssetId = Id->StringValue;
    }
    return true;
}

[[nodiscard]] bool ParseMaterialInstanceAssetRef(const JsonValue& Value, MaterialInstanceAssetRef& OutRef)
{
    AssetRefPayload Ref{};
    if (!ParseAssetRefPayload(Value, Ref))
    {
        return false;
    }

    OutRef = MaterialInstanceAssetRef(std::move(Ref.AssetName), std::move(Ref.AssetId));
    return true;
}

[[nodiscard]] std::optional<EMeshStreamSemantic> ParseStreamSemantic(const std::string& SemanticText)
{
    const std::string Key = ToLowerAscii(SemanticText);
    if (Key == "position")
    {
        return EMeshStreamSemantic::Position;
    }
    if (Key == "normal")
    {
        return EMeshStreamSemantic::Normal;
    }
    if (Key == "tangent")
    {
        return EMeshStreamSemantic::Tangent;
    }
    if (Key == "uv0")
    {
        return EMeshStreamSemantic::UV0;
    }
    if (Key == "uv1")
    {
        return EMeshStreamSemantic::UV1;
    }
    if (Key == "color")
    {
        return EMeshStreamSemantic::Color;
    }
    if (Key == "boneindices")
    {
        return EMeshStreamSemantic::BoneIndices;
    }
    if (Key == "boneweights")
    {
        return EMeshStreamSemantic::BoneWeights;
    }
    if (Key == "index")
    {
        return EMeshStreamSemantic::Index;
    }
    return std::nullopt;
}

[[nodiscard]] std::string ResolveUriRelativeToSource(std::string_view SourceUri, std::string Uri)
{
    if (Uri.empty())
    {
        return {};
    }

    if (Uri.find("://") != std::string::npos)
    {
        return Uri;
    }

    std::filesystem::path UriPath(Uri);
    if (UriPath.is_absolute())
    {
        return Uri;
    }

    const std::filesystem::path SourcePath{std::string(SourceUri)};
    const std::filesystem::path SourceDir = SourcePath.parent_path();
    if (SourceDir.empty())
    {
        return Uri;
    }

    return (SourceDir / UriPath).lexically_normal().string();
}

[[nodiscard]] bool ParseStaticMeshPayloadFields(const JsonValue& Root, StaticMeshPayload& Out)
{
    if (const JsonValue* Name = TryGetField(Root, "name"); Name)
    {
        (void)TryReadString(*Name, Out.Name);
    }

    if (const JsonValue* BoundsMin = TryGetField(Root, "boundsMin"); BoundsMin)
    {
        (void)TryReadFloatArray(*BoundsMin, Out.BoundsMin);
    }
    if (const JsonValue* BoundsMax = TryGetField(Root, "boundsMax"); BoundsMax)
    {
        (void)TryReadFloatArray(*BoundsMax, Out.BoundsMax);
    }

    if (const JsonValue* MaterialInstances = TryGetField(Root, "materialInstances");
        MaterialInstances && MaterialInstances->Type == EJsonValueType::Array)
    {
        for (const JsonValue& RefValue : MaterialInstances->ArrayValue)
        {
            MaterialInstanceAssetRef Ref{};
            if (!ParseMaterialInstanceAssetRef(RefValue, Ref))
            {
                return false;
            }
            Out.MaterialInstances.push_back(std::move(Ref));
        }
    }

    if (const JsonValue* SubMeshes = TryGetField(Root, "subMeshes");
        SubMeshes && SubMeshes->Type == EJsonValueType::Array)
    {
        for (const JsonValue& SubMeshValue : SubMeshes->ArrayValue)
        {
            if (SubMeshValue.Type != EJsonValueType::Object)
            {
                return false;
            }

            StaticSubMeshPayload SubMesh{};
            if (const JsonValue* IndexOffset = TryGetField(SubMeshValue, "indexOffset"))
            {
                (void)TryReadUnsigned(*IndexOffset, SubMesh.IndexOffset);
            }
            if (const JsonValue* IndexCount = TryGetField(SubMeshValue, "indexCount"))
            {
                (void)TryReadUnsigned(*IndexCount, SubMesh.IndexCount);
            }
            if (const JsonValue* MaterialSlot = TryGetField(SubMeshValue, "materialSlot"))
            {
                (void)TryReadUnsigned(*MaterialSlot, SubMesh.MaterialSlot);
            }
            if (const JsonValue* BoundsMin = TryGetField(SubMeshValue, "boundsMin"))
            {
                (void)TryReadFloatArray(*BoundsMin, SubMesh.BoundsMin);
            }
            if (const JsonValue* BoundsMax = TryGetField(SubMeshValue, "boundsMax"))
            {
                (void)TryReadFloatArray(*BoundsMax, SubMesh.BoundsMax);
            }

            Out.SubMeshes.push_back(SubMesh);
        }
    }

    return true;
}

[[nodiscard]] bool ParseStreamSourceArray(
    const JsonValue& Root,
    std::string_view SourceUri,
    std::vector<MeshStreamSourcePayload>& OutStreams)
{
    const JsonValue* Streams = TryGetField(Root, "streams");
    if (!Streams || Streams->Type != EJsonValueType::Array)
    {
        return false;
    }

    for (const JsonValue& StreamValue : Streams->ArrayValue)
    {
        if (StreamValue.Type != EJsonValueType::Object)
        {
            return false;
        }

        MeshStreamSourcePayload Stream{};
        if (const JsonValue* SemanticValue = TryGetField(StreamValue, "semantic"))
        {
            std::string SemanticText{};
            if (!TryReadString(*SemanticValue, SemanticText))
            {
                return false;
            }
            const auto ParsedSemantic = ParseStreamSemantic(SemanticText);
            if (!ParsedSemantic.has_value())
            {
                return false;
            }
            Stream.Semantic = *ParsedSemantic;
        }
        else
        {
            return false;
        }

        if (const JsonValue* SubIndex = TryGetField(StreamValue, "subIndex"))
        {
            (void)TryReadUnsigned(*SubIndex, Stream.SubIndex);
        }
        if (const JsonValue* Uri = TryGetField(StreamValue, "uri"))
        {
            if (!TryReadString(*Uri, Stream.Uri))
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        if (const JsonValue* ElementCount = TryGetField(StreamValue, "elementCount"))
        {
            if (!TryReadUnsigned(*ElementCount, Stream.ElementCount))
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        if (const JsonValue* StrideBytes = TryGetField(StreamValue, "strideBytes"))
        {
            if (!TryReadUnsigned(*StrideBytes, Stream.StrideBytes))
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        if (const JsonValue* Compress = TryGetField(StreamValue, "compress"))
        {
            (void)TryReadBool(*Compress, Stream.Compress);
        }

        Stream.Uri = ResolveUriRelativeToSource(SourceUri, std::move(Stream.Uri));
        OutStreams.push_back(std::move(Stream));
    }

    return !OutStreams.empty();
}

[[nodiscard]] bool ParseMaterialAsset(const JsonValue& Root, MaterialAsset& Out)
{
    const JsonValue* ShaderModule = TryGetField(Root, "shaderModule");
    if (!ShaderModule || !TryReadString(*ShaderModule, Out.ShaderModule) || Out.ShaderModule.empty())
    {
        return false;
    }

    if (const JsonValue* ShadingModel = TryGetField(Root, "shadingModel"); ShadingModel)
    {
        (void)TryReadString(*ShadingModel, Out.ShadingModel);
    }
    if (Out.ShadingModel.empty())
    {
        Out.ShadingModel = "GBufferShadingModel";
    }

    const auto TryReadFeature = [&Root](const std::string_view FieldName, bool& OutValue) {
        if (const JsonValue* FeatureValue = TryGetField(Root, FieldName))
        {
            (void)TryReadBool(*FeatureValue, OutValue);
        }
    };

    TryReadFeature("featureAlbedoMap", Out.FeatureAlbedoMap);
    TryReadFeature("featureNormalMap", Out.FeatureNormalMap);
    TryReadFeature("featureRoughnessMap", Out.FeatureRoughnessMap);
    TryReadFeature("featureMetalnessMap", Out.FeatureMetalnessMap);
    TryReadFeature("featureOcclusionMap", Out.FeatureOcclusionMap);
    TryReadFeature("featureAlphaTest", Out.FeatureAlphaTest);
    TryReadFeature("featureAlphaBlend", Out.FeatureAlphaBlend);
    TryReadFeature("featureDoubleSided", Out.FeatureDoubleSided);
    TryReadFeature("featureInstancing", Out.FeatureInstancing);
    return true;
}

[[nodiscard]] bool ParseMaterialInstanceAsset(const JsonValue& Root, MaterialInstanceAsset& Out)
{
    const JsonValue* ParentMaterial = TryGetField(Root, "parentMaterial");
    if (!ParentMaterial || !ParseAssetRefPayload(*ParentMaterial, Out.ParentMaterial))
    {
        return false;
    }

    if (const JsonValue* Scalars = TryGetField(Root, "scalars"); Scalars && Scalars->Type == EJsonValueType::Array)
    {
        for (const JsonValue& ScalarValue : Scalars->ArrayValue)
        {
            if (ScalarValue.Type != EJsonValueType::Object)
            {
                return false;
            }

            MaterialScalarParamPayload Scalar{};
            if (const JsonValue* Name = TryGetField(ScalarValue, "name"); !Name || !TryReadString(*Name, Scalar.Name))
            {
                return false;
            }
            if (const JsonValue* Value = TryGetField(ScalarValue, "value"))
            {
                if (!TryReadFloat(*Value, Scalar.Value))
                {
                    return false;
                }
            }
            Out.Scalars.push_back(std::move(Scalar));
        }
    }

    if (const JsonValue* Vectors = TryGetField(Root, "vectors"); Vectors && Vectors->Type == EJsonValueType::Array)
    {
        for (const JsonValue& VectorValue : Vectors->ArrayValue)
        {
            if (VectorValue.Type != EJsonValueType::Object)
            {
                return false;
            }

            MaterialVectorParamPayload Vector{};
            if (const JsonValue* Name = TryGetField(VectorValue, "name"); !Name || !TryReadString(*Name, Vector.Name))
            {
                return false;
            }
            if (const JsonValue* Value = TryGetField(VectorValue, "value"))
            {
                if (!TryReadFloatArray(*Value, Vector.Value))
                {
                    return false;
                }
            }
            Out.Vectors.push_back(std::move(Vector));
        }
    }

    if (const JsonValue* Textures = TryGetField(Root, "textures"); Textures && Textures->Type == EJsonValueType::Array)
    {
        for (const JsonValue& TextureValue : Textures->ArrayValue)
        {
            if (TextureValue.Type != EJsonValueType::Object)
            {
                return false;
            }

            MaterialTextureParamPayload Texture{};
            if (const JsonValue* SlotName = TryGetField(TextureValue, "slotName");
                !SlotName || !TryReadString(*SlotName, Texture.SlotName))
            {
                return false;
            }
            if (const JsonValue* TextureRef = TryGetField(TextureValue, "texture");
                !TextureRef || !ParseAssetRefPayload(*TextureRef, Texture.Texture))
            {
                return false;
            }
            if (const JsonValue* SRGB = TryGetField(TextureValue, "sRGB"))
            {
                (void)TryReadBool(*SRGB, Texture.SRGB);
            }
            Out.Textures.push_back(std::move(Texture));
        }
    }

    return true;
}

[[nodiscard]] std::optional<std::string> ReadOptionalStringField(const JsonValue& Root, std::string_view Key)
{
    if (const JsonValue* Field = TryGetField(Root, Key); Field && Field->Type == EJsonValueType::String)
    {
        return Field->StringValue;
    }
    return std::nullopt;
}

[[nodiscard]] std::string DetermineLogicalName(const JsonValue& Root, const std::string& SourceUri)
{
    (void)Root;
    return SourceUri;
}

[[nodiscard]] ::SnAPI::AssetPipeline::AssetId DetermineAssetId(const JsonValue& Root)
{
    if (const auto AssetId = ReadOptionalStringField(Root, "AssetId"); AssetId && !AssetId->empty())
    {
        return ::SnAPI::AssetPipeline::AssetId::FromString(*AssetId);
    }
    if (const auto AssetId = ReadOptionalStringField(Root, "assetId"); AssetId && !AssetId->empty())
    {
        return ::SnAPI::AssetPipeline::AssetId::FromString(*AssetId);
    }
    return {};
}

enum class ERenderSourceType : uint8_t
{
    Unknown = 0,
    Material,
    MaterialInstance,
    StaticMesh,
    SkeletalMesh,
};

[[nodiscard]] ERenderSourceType DetermineSourceTypeFromPath(const std::string& SourceUri)
{
    if (EndsWithInsensitive(SourceUri, ".snmaterial.json"))
    {
        return ERenderSourceType::Material;
    }
    if (EndsWithInsensitive(SourceUri, ".snmatinst.json"))
    {
        return ERenderSourceType::MaterialInstance;
    }
    if (EndsWithInsensitive(SourceUri, ".snstaticmesh.json"))
    {
        return ERenderSourceType::StaticMesh;
    }
    if (EndsWithInsensitive(SourceUri, ".snskeletalmesh.json"))
    {
        return ERenderSourceType::SkeletalMesh;
    }
    return ERenderSourceType::Unknown;
}

[[nodiscard]] ERenderSourceType DetermineSourceType(const JsonValue& Root, const std::string& SourceUri)
{
    ERenderSourceType Type = DetermineSourceTypeFromPath(SourceUri);
    if (Type != ERenderSourceType::Unknown)
    {
        return Type;
    }

    const JsonValue* TypeField = TryGetField(Root, "type");
    if (!TypeField || TypeField->Type != EJsonValueType::String)
    {
        return ERenderSourceType::Unknown;
    }

    const std::string Value = ToLowerAscii(TypeField->StringValue);
    if (Value == "material")
    {
        return ERenderSourceType::Material;
    }
    if (Value == "materialinstance")
    {
        return ERenderSourceType::MaterialInstance;
    }
    if (Value == "staticmesh")
    {
        return ERenderSourceType::StaticMesh;
    }
    if (Value == "skeletalmesh")
    {
        return ERenderSourceType::SkeletalMesh;
    }
    return ERenderSourceType::Unknown;
}

constexpr ::SnAPI::AssetPipeline::EBulkSemantic kBulkSemanticMeshStream =
    static_cast<::SnAPI::AssetPipeline::EBulkSemantic>(0x00010001u);
constexpr ::SnAPI::AssetPipeline::EBulkSemantic kBulkSemanticSkeletalAnimation =
    static_cast<::SnAPI::AssetPipeline::EBulkSemantic>(0x00010002u);

class RenderAssetJsonImporter final : public ::SnAPI::AssetPipeline::IAssetImporter
{
public:
    const char* GetName() const override
    {
        return "SnAPI.GameFramework.RenderAssetJsonImporter";
    }

    bool CanImport(const ::SnAPI::AssetPipeline::SourceRef& Source) const override
    {
        return DetermineSourceTypeFromPath(Source.Uri) != ERenderSourceType::Unknown;
    }

    bool Import(
        const ::SnAPI::AssetPipeline::SourceRef& Source,
        std::vector<::SnAPI::AssetPipeline::ImportedItem>& OutItems,
        ::SnAPI::AssetPipeline::IPipelineContext& Ctx) override
    {
        std::vector<uint8_t> SourceBytes{};
        if (!Ctx.ReadAllBytes(Source.Uri, SourceBytes))
        {
            Ctx.LogError("RenderAsset importer failed to read source: %s", Source.Uri.c_str());
            return false;
        }

        const std::string SourceText(SourceBytes.begin(), SourceBytes.end());
        JsonParser Parser(SourceText);
        auto RootResult = Parser.ParseRoot();
        if (!RootResult)
        {
            Ctx.LogError("RenderAsset importer JSON parse error in %s: %s", Source.Uri.c_str(), RootResult.error().c_str());
            return false;
        }

        JsonValue& Root = *RootResult;
        if (Root.Type != EJsonValueType::Object)
        {
            Ctx.LogError("RenderAsset importer expected JSON object root: %s", Source.Uri.c_str());
            return false;
        }

        const ERenderSourceType SourceType = DetermineSourceType(Root, Source.Uri);
        if (SourceType == ERenderSourceType::Unknown)
        {
            Ctx.LogError("RenderAsset importer unknown source type: %s", Source.Uri.c_str());
            return false;
        }

        std::string VariantKey{};
        if (const auto Variant = ReadOptionalStringField(Root, "variant"); Variant)
        {
            VariantKey = *Variant;
        }

        ::SnAPI::AssetPipeline::ImportedItem Item{};
        Item.LogicalName = DetermineLogicalName(Root, Source.Uri);
        Item.VariantKey = VariantKey;
        Item.Dependencies.push_back(Source);
        const ::SnAPI::AssetPipeline::AssetId ParsedAssetId = DetermineAssetId(Root);
        const ::SnAPI::AssetPipeline::AssetId ResolvedAssetId =
            !ParsedAssetId.IsNull() ? ParsedAssetId : Ctx.MakeDeterministicAssetId(Item.LogicalName, Item.VariantKey);

        if (SourceType == ERenderSourceType::Material)
        {
            MaterialAsset Payload{};
            if (!ParseMaterialAsset(Root, Payload))
            {
                Ctx.LogError("RenderAsset importer failed to parse material payload: %s", Source.Uri.c_str());
                return false;
            }

            const auto* Serializer = Ctx.FindSerializer(PayloadMaterial());
            if (!Serializer)
            {
                Ctx.LogError("RenderAsset importer missing material serializer");
                return false;
            }

            Payload.SetPersistentIdentity(ResolvedAssetId, Item.LogicalName);
            Item.AssetKind = AssetKindMaterial();
            Item.Intermediate.PayloadType = PayloadMaterial();
            Item.Intermediate.SchemaVersion = Serializer->GetSchemaVersion();
            Serializer->SerializeToBytes(&Payload, Item.Intermediate.Bytes);
        }
        else if (SourceType == ERenderSourceType::MaterialInstance)
        {
            MaterialInstanceAsset Payload{};
            if (!ParseMaterialInstanceAsset(Root, Payload))
            {
                Ctx.LogError("RenderAsset importer failed to parse material instance payload: %s", Source.Uri.c_str());
                return false;
            }

            const auto* Serializer = Ctx.FindSerializer(PayloadMaterialInstance());
            if (!Serializer)
            {
                Ctx.LogError("RenderAsset importer missing material instance serializer");
                return false;
            }

            Payload.SetPersistentIdentity(ResolvedAssetId, Item.LogicalName);
            Item.AssetKind = AssetKindMaterialInstance();
            Item.Intermediate.PayloadType = PayloadMaterialInstance();
            Item.Intermediate.SchemaVersion = Serializer->GetSchemaVersion();
            Serializer->SerializeToBytes(&Payload, Item.Intermediate.Bytes);
        }
        else if (SourceType == ERenderSourceType::StaticMesh)
        {
            StaticMeshAsset SourcePayload{};
            if (!ParseStaticMeshPayloadFields(Root, SourcePayload.Mesh) ||
                !ParseStreamSourceArray(Root, Source.Uri, SourcePayload.Streams))
            {
                Ctx.LogError("RenderAsset importer failed to parse static mesh payload: %s", Source.Uri.c_str());
                return false;
            }

            for (const MeshStreamSourcePayload& Stream : SourcePayload.Streams)
            {
                Item.Dependencies.emplace_back(Stream.Uri, 0);
            }

            const auto* Serializer = Ctx.FindSerializer(PayloadStaticMeshSource());
            if (!Serializer)
            {
                Ctx.LogError("RenderAsset importer missing static mesh source serializer");
                return false;
            }

            SourcePayload.SetPersistentIdentity(ResolvedAssetId, Item.LogicalName);
            Item.AssetKind = AssetKindStaticMesh();
            Item.Intermediate.PayloadType = PayloadStaticMeshSource();
            Item.Intermediate.SchemaVersion = Serializer->GetSchemaVersion();
            Serializer->SerializeToBytes(&SourcePayload, Item.Intermediate.Bytes);
        }
        else if (SourceType == ERenderSourceType::SkeletalMesh)
        {
            SkeletalMeshAsset SourcePayload{};
            const JsonValue* BaseMesh = TryGetField(Root, "baseMesh");
            if (!BaseMesh || BaseMesh->Type != EJsonValueType::Object)
            {
                Ctx.LogError("RenderAsset importer skeletal source requires baseMesh object: %s", Source.Uri.c_str());
                return false;
            }

            if (!ParseStaticMeshPayloadFields(*BaseMesh, SourcePayload.BaseMesh.Mesh) ||
                !ParseStreamSourceArray(*BaseMesh, Source.Uri, SourcePayload.BaseMesh.Streams))
            {
                Ctx.LogError("RenderAsset importer failed to parse skeletal base mesh: %s", Source.Uri.c_str());
                return false;
            }

            const JsonValue* Bones = TryGetField(Root, "bones");
            if (!Bones || Bones->Type != EJsonValueType::Array)
            {
                Ctx.LogError("RenderAsset importer skeletal source requires bones array: %s", Source.Uri.c_str());
                return false;
            }

            for (const JsonValue& BoneValue : Bones->ArrayValue)
            {
                if (BoneValue.Type != EJsonValueType::Object)
                {
                    Ctx.LogError("RenderAsset importer encountered invalid bone entry");
                    return false;
                }

                SkeletalBonePayload Bone{};
                if (const JsonValue* Name = TryGetField(BoneValue, "name"); !Name || !TryReadString(*Name, Bone.Name))
                {
                    Ctx.LogError("RenderAsset importer bone missing name");
                    return false;
                }
                if (const JsonValue* ParentIndex = TryGetField(BoneValue, "parentIndex"))
                {
                    (void)TryReadSigned(*ParentIndex, Bone.ParentIndex);
                }
                if (const JsonValue* BindPose = TryGetField(BoneValue, "bindPose"))
                {
                    if (!TryReadFloatArray(*BindPose, Bone.BindPose))
                    {
                        Ctx.LogError("RenderAsset importer bone bindPose must contain 16 floats");
                        return false;
                    }
                }
                SourcePayload.Bones.push_back(std::move(Bone));
            }

            if (const JsonValue* Anim = TryGetField(Root, "skeletonAnimation");
                Anim && Anim->Type == EJsonValueType::Object)
            {
                if (const JsonValue* Uri = TryGetField(*Anim, "uri"); Uri)
                {
                    (void)TryReadString(*Uri, SourcePayload.SkeletonAnimationUri);
                }
                if (const JsonValue* SubIndex = TryGetField(*Anim, "subIndex"); SubIndex)
                {
                    (void)TryReadUnsigned(*SubIndex, SourcePayload.SkeletonAnimationSubIndex);
                }
                if (const JsonValue* Compress = TryGetField(*Anim, "compress"); Compress)
                {
                    (void)TryReadBool(*Compress, SourcePayload.CompressSkeletonAnimation);
                }
                SourcePayload.SkeletonAnimationUri =
                    ResolveUriRelativeToSource(Source.Uri, std::move(SourcePayload.SkeletonAnimationUri));
            }

            for (const MeshStreamSourcePayload& Stream : SourcePayload.BaseMesh.Streams)
            {
                Item.Dependencies.emplace_back(Stream.Uri, 0);
            }
            if (!SourcePayload.SkeletonAnimationUri.empty())
            {
                Item.Dependencies.emplace_back(SourcePayload.SkeletonAnimationUri, 0);
            }

            const auto* Serializer = Ctx.FindSerializer(PayloadSkeletalMeshSource());
            if (!Serializer)
            {
                Ctx.LogError("RenderAsset importer missing skeletal mesh source serializer");
                return false;
            }

            SourcePayload.SetPersistentIdentity(ResolvedAssetId, Item.LogicalName);
            Item.AssetKind = AssetKindSkeletalMesh();
            Item.Intermediate.PayloadType = PayloadSkeletalMeshSource();
            Item.Intermediate.SchemaVersion = Serializer->GetSchemaVersion();
            Serializer->SerializeToBytes(&SourcePayload, Item.Intermediate.Bytes);
        }

        Item.Id = ResolvedAssetId;
        OutItems.push_back(std::move(Item));

        Ctx.LogInfo("Imported render asset source: %s", Source.Uri.c_str());
        return true;
    }
};

[[nodiscard]] TextureCompressorPlugin::ECompressionTarget ParseTextureCompressionTarget(std::string_view Value)
{
    const std::string Lower = ToLowerAscii(Value);
    return Lower == "astc"
        ? TextureCompressorPlugin::ECompressionTarget::ASTC
        : TextureCompressorPlugin::ECompressionTarget::BCn;
}

[[nodiscard]] TextureCompressorPlugin::ECompressedFormat ParseTextureCompressionFormat(std::string_view Value)
{
    const std::string Lower = ToLowerAscii(Value);
    using TextureCompressorPlugin::ECompressedFormat;

    if (Lower.empty() || Lower == "auto")
    {
        return ECompressedFormat::Unknown;
    }
    if (Lower == "bc1")
    {
        return ECompressedFormat::BC1;
    }
    if (Lower == "bc3")
    {
        return ECompressedFormat::BC3;
    }
    if (Lower == "bc4")
    {
        return ECompressedFormat::BC4;
    }
    if (Lower == "bc5")
    {
        return ECompressedFormat::BC5;
    }
    if (Lower == "bc6h")
    {
        return ECompressedFormat::BC6H;
    }
    if (Lower == "bc7")
    {
        return ECompressedFormat::BC7;
    }
    if (Lower == "astc_4x4")
    {
        return ECompressedFormat::ASTC_4x4;
    }
    if (Lower == "astc_5x5")
    {
        return ECompressedFormat::ASTC_5x5;
    }
    if (Lower == "astc_6x6")
    {
        return ECompressedFormat::ASTC_6x6;
    }
    if (Lower == "astc_8x8")
    {
        return ECompressedFormat::ASTC_8x8;
    }
    if (Lower == "astc_10x10")
    {
        return ECompressedFormat::ASTC_10x10;
    }
    if (Lower == "astc_12x12")
    {
        return ECompressedFormat::ASTC_12x12;
    }
    if (Lower == "astc_4x4_hdr")
    {
        return ECompressedFormat::ASTC_4x4_HDR;
    }
    if (Lower == "astc_6x6_hdr")
    {
        return ECompressedFormat::ASTC_6x6_HDR;
    }
    if (Lower == "astc_8x8_hdr")
    {
        return ECompressedFormat::ASTC_8x8_HDR;
    }
    return ECompressedFormat::Unknown;
}

[[nodiscard]] TextureCompressorPlugin::TextureCompressorImportSettings BuildTextureCompressorImportSettings(
    const TextureImporterSettings& Settings)
{
    TextureCompressorPlugin::TextureCompressorImportSettings Typed{};
    Typed.Target = Settings.Target == ETextureCompressionTarget::ASTC
        ? TextureCompressorPlugin::ECompressionTarget::ASTC
        : TextureCompressorPlugin::ECompressionTarget::BCn;
    Typed.Format = [Format = Settings.Format]() {
        using TextureCompressorPlugin::ECompressedFormat;
        switch (Format)
        {
        case ETextureCompressionFormat::BC1: return ECompressedFormat::BC1;
        case ETextureCompressionFormat::BC3: return ECompressedFormat::BC3;
        case ETextureCompressionFormat::BC4: return ECompressedFormat::BC4;
        case ETextureCompressionFormat::BC5: return ECompressedFormat::BC5;
        case ETextureCompressionFormat::BC6H: return ECompressedFormat::BC6H;
        case ETextureCompressionFormat::BC7: return ECompressedFormat::BC7;
        case ETextureCompressionFormat::ASTC_4x4: return ECompressedFormat::ASTC_4x4;
        case ETextureCompressionFormat::ASTC_5x5: return ECompressedFormat::ASTC_5x5;
        case ETextureCompressionFormat::ASTC_6x6: return ECompressedFormat::ASTC_6x6;
        case ETextureCompressionFormat::ASTC_8x8: return ECompressedFormat::ASTC_8x8;
        case ETextureCompressionFormat::ASTC_10x10: return ECompressedFormat::ASTC_10x10;
        case ETextureCompressionFormat::ASTC_12x12: return ECompressedFormat::ASTC_12x12;
        case ETextureCompressionFormat::ASTC_4x4_HDR: return ECompressedFormat::ASTC_4x4_HDR;
        case ETextureCompressionFormat::ASTC_6x6_HDR: return ECompressedFormat::ASTC_6x6_HDR;
        case ETextureCompressionFormat::ASTC_8x8_HDR: return ECompressedFormat::ASTC_8x8_HDR;
        case ETextureCompressionFormat::Auto:
        default: return ECompressedFormat::Unknown;
        }
    }();
    Typed.Quality = std::clamp(Settings.Quality, 0.0f, 1.0f);
    Typed.ForceNormalMap = Settings.ForceNormalMap;
    Typed.MaxMipCount = Settings.MaxMips > 0u ? static_cast<int32_t>(Settings.MaxMips) : -1;
    if (Settings.ForceLinear)
    {
        Typed.ColorSpacePolicy = TextureCompressorPlugin::ETextureColorSpacePolicy::ForceLinear;
    }
    else if (Settings.ForceSrgb)
    {
        Typed.ColorSpacePolicy = TextureCompressorPlugin::ETextureColorSpacePolicy::ForceSrgb;
    }
    else
    {
        Typed.ColorSpacePolicy = TextureCompressorPlugin::ETextureColorSpacePolicy::Auto;
    }
    return Typed;
}

[[nodiscard]] TExpected<TextureCompressorPlugin::ImageIntermediate> BuildTextureIntermediate(const TextureAsset& Asset)
{
    TextureCompressorPlugin::ImageIntermediate Intermediate{};
    if (auto DecodeResult = DecodeTextureSourceImageToIntermediate(Asset.Image, Intermediate); !DecodeResult)
    {
        return std::unexpected(DecodeResult.error());
    }
    return Intermediate;
}

class RenderTextureCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    RenderTextureCooker()
        : m_textureCooker(TextureCompressorPlugin::CreateTextureCompressorCooker())
    {
    }

    [[nodiscard]] const char* GetName() const override
    {
        return "SnAPI.GameFramework.RenderTextureCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind, const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        return AssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture &&
               IntermediatePayloadType == PayloadTextureSource();
    }

    bool Cook(const ::SnAPI::AssetPipeline::CookRequest& Req,
              ::SnAPI::AssetPipeline::CookResult& Out,
              ::SnAPI::AssetPipeline::IPipelineContext& Ctx) override
    {
        if (!m_textureCooker)
        {
            Ctx.LogError("Render texture cooker is missing TextureCompressor cooker");
            return false;
        }

        const auto* SourceSerializer = Ctx.FindSerializer(PayloadTextureSource());
        const auto* IntermediateSerializer = Ctx.FindSerializer(TextureCompressorPlugin::Payload_CompressorImageIntermediate);
        if (!SourceSerializer || !IntermediateSerializer)
        {
            Ctx.LogError("Render texture cooker missing serializers");
            return false;
        }

        TextureAsset SourceAsset{};
        if (!SourceSerializer->DeserializeFromBytes(&SourceAsset, Req.Intermediate.Bytes.data(), Req.Intermediate.Bytes.size()))
        {
            Ctx.LogError("Render texture cooker failed to deserialize source payload");
            return false;
        }

        auto IntermediateResult = BuildTextureIntermediate(SourceAsset);
        if (!IntermediateResult)
        {
            Ctx.LogError("Render texture cooker failed to decode source image: %s",
                         IntermediateResult.error().Message.c_str());
            return false;
        }

        std::vector<uint8_t> IntermediateBytes{};
        IntermediateSerializer->SerializeToBytes(&*IntermediateResult, IntermediateBytes);

        ::SnAPI::AssetPipeline::CookRequest TextureReq = Req;
        TextureReq.Intermediate.PayloadType = TextureCompressorPlugin::Payload_CompressorImageIntermediate;
        TextureReq.Intermediate.SchemaVersion = IntermediateSerializer->GetSchemaVersion();
        TextureReq.Intermediate.Bytes = std::move(IntermediateBytes);
        TextureReq.ImportSettings = std::make_shared<TextureCompressorPlugin::TextureCompressorImportSettings>(
            BuildTextureCompressorImportSettings(SourceAsset.ImportSettings));

        return m_textureCooker->Cook(TextureReq, Out, Ctx);
    }

private:
    std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> m_textureCooker{};
};

class RenderMaterialCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    [[nodiscard]] const char* GetName() const override
    {
        return "SnAPI.GameFramework.RenderMaterialCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind, const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        return AssetKind == AssetKindMaterial() && IntermediatePayloadType == PayloadMaterial();
    }

    bool Cook(const ::SnAPI::AssetPipeline::CookRequest& Req, ::SnAPI::AssetPipeline::CookResult& Out, ::SnAPI::AssetPipeline::IPipelineContext&) override
    {
        Out.Cooked = Req.Intermediate;
        Out.Dependencies = Req.Dependencies;
        Out.Tags["RenderAsset.Kind"] = "Material";
        return true;
    }
};

class RenderMaterialInstanceCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    [[nodiscard]] const char* GetName() const override
    {
        return "SnAPI.GameFramework.RenderMaterialInstanceCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind, const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        return AssetKind == AssetKindMaterialInstance() && IntermediatePayloadType == PayloadMaterialInstance();
    }

    bool Cook(const ::SnAPI::AssetPipeline::CookRequest& Req, ::SnAPI::AssetPipeline::CookResult& Out, ::SnAPI::AssetPipeline::IPipelineContext&) override
    {
        Out.Cooked = Req.Intermediate;
        Out.Dependencies = Req.Dependencies;
        Out.Tags["RenderAsset.Kind"] = "MaterialInstance";
        return true;
    }
};

class RenderSkeletonCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    [[nodiscard]] const char* GetName() const override
    {
        return "SnAPI.GameFramework.RenderSkeletonCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind, const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        return AssetKind == AssetKindSkeleton() && IntermediatePayloadType == PayloadSkeleton();
    }

    bool Cook(const ::SnAPI::AssetPipeline::CookRequest& Req, ::SnAPI::AssetPipeline::CookResult& Out, ::SnAPI::AssetPipeline::IPipelineContext&) override
    {
        Out.Cooked = Req.Intermediate;
        Out.Dependencies = Req.Dependencies;
        Out.Tags["RenderAsset.Kind"] = "Skeleton";
        return true;
    }
};

class RenderAnimationCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    [[nodiscard]] const char* GetName() const override
    {
        return "SnAPI.GameFramework.RenderAnimationCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind, const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        return AssetKind == AssetKindAnimation() && IntermediatePayloadType == PayloadAnimation();
    }

    bool Cook(const ::SnAPI::AssetPipeline::CookRequest& Req, ::SnAPI::AssetPipeline::CookResult& Out, ::SnAPI::AssetPipeline::IPipelineContext&) override
    {
        Out.Cooked = Req.Intermediate;
        Out.Dependencies = Req.Dependencies;
        Out.Tags["RenderAsset.Kind"] = "Animation";
        return true;
    }
};

class RenderStaticMeshCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    [[nodiscard]] const char* GetName() const override
    {
        return "SnAPI.GameFramework.RenderStaticMeshCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind, const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        return AssetKind == AssetKindStaticMesh() && IntermediatePayloadType == PayloadStaticMeshSource();
    }

    bool Cook(
        const ::SnAPI::AssetPipeline::CookRequest& Req,
        ::SnAPI::AssetPipeline::CookResult& Out,
        ::SnAPI::AssetPipeline::IPipelineContext& Ctx) override
    {
        const auto* SourceSerializer = Ctx.FindSerializer(PayloadStaticMeshSource());
        const auto* CookedSerializer = Ctx.FindSerializer(PayloadStaticMesh());
        if (!SourceSerializer || !CookedSerializer)
        {
            Ctx.LogError("Render static mesh cooker missing serializers");
            return false;
        }

        StaticMeshAsset SourcePayload{};
        if (!SourceSerializer->DeserializeFromBytes(&SourcePayload, Req.Intermediate.Bytes.data(), Req.Intermediate.Bytes.size()))
        {
            Ctx.LogError("Render static mesh cooker failed to deserialize source payload");
            return false;
        }

        StaticMeshPayload CookedPayload = SourcePayload.Mesh;
        CookedPayload.Streams.clear();
        CookedPayload.Streams.reserve(SourcePayload.Streams.size());

        for (const MeshStreamSourcePayload& Stream : SourcePayload.Streams)
        {
            if (Stream.ElementCount == 0 || Stream.StrideBytes == 0)
            {
                Ctx.LogError("Render static mesh cooker encountered invalid stream metadata");
                return false;
            }

            std::vector<uint8_t> StreamBytes = Stream.Bytes;
            if (StreamBytes.empty())
            {
                if (Stream.Uri.empty() || !Ctx.ReadAllBytes(Stream.Uri, StreamBytes))
                {
                    Ctx.LogError("Render static mesh cooker failed to read stream uri: %s", Stream.Uri.c_str());
                    return false;
                }
            }

            const size_t MinSize = static_cast<size_t>(Stream.ElementCount) * static_cast<size_t>(Stream.StrideBytes);
            if (StreamBytes.size() < MinSize)
            {
                Ctx.LogError("Render static mesh cooker stream byte count too small: %s", Stream.Uri.c_str());
                return false;
            }

            ::SnAPI::AssetPipeline::BulkChunk Chunk{};
            Chunk.Semantic = kBulkSemanticMeshStream;
            Chunk.SubIndex = Stream.SubIndex;
            Chunk.bCompress = Stream.Compress;
            Chunk.Bytes = std::move(StreamBytes);

            const uint32_t BulkIndex = static_cast<uint32_t>(Out.Bulk.size());
            Out.Bulk.push_back(std::move(Chunk));

            MeshStreamChunkRef ChunkRef{};
            ChunkRef.Semantic = Stream.Semantic;
            ChunkRef.BulkIndex = BulkIndex;
            ChunkRef.ElementCount = Stream.ElementCount;
            ChunkRef.StrideBytes = Stream.StrideBytes;
            CookedPayload.Streams.push_back(ChunkRef);
        }

        Out.Cooked.PayloadType = PayloadStaticMesh();
        Out.Cooked.SchemaVersion = CookedSerializer->GetSchemaVersion();
        CookedSerializer->SerializeToBytes(&CookedPayload, Out.Cooked.Bytes);

        Out.Dependencies = Req.Dependencies;
        Out.Tags["RenderAsset.Kind"] = "StaticMesh";
        Out.Tags["RenderAsset.StreamCount"] = std::to_string(CookedPayload.Streams.size());
        Out.Tags["RenderAsset.SubMeshCount"] = std::to_string(CookedPayload.SubMeshes.size());
        return true;
    }
};

class RenderSkeletalMeshCooker final : public ::SnAPI::AssetPipeline::IAssetCooker
{
public:
    [[nodiscard]] const char* GetName() const override
    {
        return "SnAPI.GameFramework.RenderSkeletalMeshCooker";
    }

    bool CanCook(const ::SnAPI::AssetPipeline::TypeId AssetKind, const ::SnAPI::AssetPipeline::TypeId IntermediatePayloadType) const override
    {
        return AssetKind == AssetKindSkeletalMesh() && IntermediatePayloadType == PayloadSkeletalMeshSource();
    }

    bool Cook(
        const ::SnAPI::AssetPipeline::CookRequest& Req,
        ::SnAPI::AssetPipeline::CookResult& Out,
        ::SnAPI::AssetPipeline::IPipelineContext& Ctx) override
    {
        const auto* SourceSerializer = Ctx.FindSerializer(PayloadSkeletalMeshSource());
        const auto* CookedSerializer = Ctx.FindSerializer(PayloadSkeletalMesh());
        if (!SourceSerializer || !CookedSerializer)
        {
            Ctx.LogError("Render skeletal mesh cooker missing serializers");
            return false;
        }

        SkeletalMeshAsset SourcePayload{};
        if (!SourceSerializer->DeserializeFromBytes(&SourcePayload, Req.Intermediate.Bytes.data(), Req.Intermediate.Bytes.size()))
        {
            Ctx.LogError("Render skeletal mesh cooker failed to deserialize source payload");
            return false;
        }

        SkeletalMeshPayload CookedPayload{};
        CookedPayload.BaseMesh = SourcePayload.BaseMesh.Mesh;
        CookedPayload.Bones = SourcePayload.Bones;
        CookedPayload.Skeleton = SourcePayload.Skeleton;
        CookedPayload.Animations = SourcePayload.Animations;
        CookedPayload.BaseMesh.Streams.clear();
        CookedPayload.BaseMesh.Streams.reserve(SourcePayload.BaseMesh.Streams.size());

        for (const MeshStreamSourcePayload& Stream : SourcePayload.BaseMesh.Streams)
        {
            if (Stream.ElementCount == 0 || Stream.StrideBytes == 0)
            {
                Ctx.LogError("Render skeletal mesh cooker encountered invalid stream metadata");
                return false;
            }

            std::vector<uint8_t> StreamBytes = Stream.Bytes;
            if (StreamBytes.empty())
            {
                if (Stream.Uri.empty() || !Ctx.ReadAllBytes(Stream.Uri, StreamBytes))
                {
                    Ctx.LogError("Render skeletal mesh cooker failed to read stream uri: %s", Stream.Uri.c_str());
                    return false;
                }
            }

            const size_t MinSize = static_cast<size_t>(Stream.ElementCount) * static_cast<size_t>(Stream.StrideBytes);
            if (StreamBytes.size() < MinSize)
            {
                Ctx.LogError("Render skeletal mesh cooker stream byte count too small: %s", Stream.Uri.c_str());
                return false;
            }

            ::SnAPI::AssetPipeline::BulkChunk Chunk{};
            Chunk.Semantic = kBulkSemanticMeshStream;
            Chunk.SubIndex = Stream.SubIndex;
            Chunk.bCompress = Stream.Compress;
            Chunk.Bytes = std::move(StreamBytes);

            const uint32_t BulkIndex = static_cast<uint32_t>(Out.Bulk.size());
            Out.Bulk.push_back(std::move(Chunk));

            MeshStreamChunkRef ChunkRef{};
            ChunkRef.Semantic = Stream.Semantic;
            ChunkRef.BulkIndex = BulkIndex;
            ChunkRef.ElementCount = Stream.ElementCount;
            ChunkRef.StrideBytes = Stream.StrideBytes;
            CookedPayload.BaseMesh.Streams.push_back(ChunkRef);
        }

        if (!SourcePayload.SkeletonAnimationBytes.empty() || !SourcePayload.SkeletonAnimationUri.empty())
        {
            std::vector<uint8_t> AnimationBytes = SourcePayload.SkeletonAnimationBytes;
            if (AnimationBytes.empty())
            {
                if (!Ctx.ReadAllBytes(SourcePayload.SkeletonAnimationUri, AnimationBytes))
                {
                    Ctx.LogError(
                        "Render skeletal mesh cooker failed to read skeleton animation uri: %s",
                        SourcePayload.SkeletonAnimationUri.c_str());
                    return false;
                }
            }

            ::SnAPI::AssetPipeline::BulkChunk AnimationChunk{};
            AnimationChunk.Semantic = kBulkSemanticSkeletalAnimation;
            AnimationChunk.SubIndex = SourcePayload.SkeletonAnimationSubIndex;
            AnimationChunk.bCompress = SourcePayload.CompressSkeletonAnimation;
            AnimationChunk.Bytes = std::move(AnimationBytes);
            CookedPayload.SkeletonAnimationBulkIndex = static_cast<uint32_t>(Out.Bulk.size());
            Out.Bulk.push_back(std::move(AnimationChunk));
        }
        else
        {
            CookedPayload.SkeletonAnimationBulkIndex = std::numeric_limits<uint32_t>::max();
        }

        Out.Cooked.PayloadType = PayloadSkeletalMesh();
        Out.Cooked.SchemaVersion = CookedSerializer->GetSchemaVersion();
        CookedSerializer->SerializeToBytes(&CookedPayload, Out.Cooked.Bytes);

        Out.Dependencies = Req.Dependencies;
        Out.Tags["RenderAsset.Kind"] = "SkeletalMesh";
        Out.Tags["RenderAsset.BoneCount"] = std::to_string(CookedPayload.Bones.size());
        Out.Tags["RenderAsset.StreamCount"] = std::to_string(CookedPayload.BaseMesh.Streams.size());
        return true;
    }
};

} // namespace

std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter> CreateRenderAssetJsonImporter()
{
    return std::make_unique<RenderAssetJsonImporter>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialCooker()
{
    return std::make_unique<RenderMaterialCooker>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderTextureCooker()
{
    return std::make_unique<RenderTextureCooker>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderMaterialInstanceCooker()
{
    return std::make_unique<RenderMaterialInstanceCooker>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderSkeletonCooker()
{
    return std::make_unique<RenderSkeletonCooker>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderAnimationCooker()
{
    return std::make_unique<RenderAnimationCooker>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderStaticMeshCooker()
{
    return std::make_unique<RenderStaticMeshCooker>();
}

std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker> CreateRenderSkeletalMeshCooker()
{
    return std::make_unique<RenderSkeletalMeshCooker>();
}

} // namespace SnAPI::GameFramework
