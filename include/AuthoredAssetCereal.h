#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iosfwd>
#include <string>
#include <type_traits>
#include <vector>

#include <nlohmann/json.hpp>

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "AuthoredAssetJson.h"
#include "Conduit/Asset.h"
#include "NodeAsset.h"
#include "RenderAssetPayloads.h"
#include "RenderAssetSourcePayloads.h"
#include "TypeRegistry.h"

namespace SnAPI::GameFramework::Detail
{

template<class TValue>
[[nodiscard]] auto Nvp(const char* Name, TValue&& Value)
{
    return cereal::make_nvp(Name, std::forward<TValue>(Value));
}

inline void WriteUnnamedJsonValue(cereal::JSONOutputArchive& Ar, const nlohmann::json& Value);

inline void WriteNamedJsonValue(cereal::JSONOutputArchive& Ar, const char* Name, const nlohmann::json& Value)
{
    if (Value.is_object())
    {
        Ar.setNextName(Name);
        Ar.startNode();
        for (auto It = Value.begin(); It != Value.end(); ++It)
        {
            const std::string Key = It.key();
            WriteNamedJsonValue(Ar, Key.c_str(), It.value());
        }
        Ar.finishNode();
        return;
    }

    if (Value.is_array())
    {
        Ar.setNextName(Name);
        Ar.startNode();
        Ar.makeArray();
        for (const auto& Element : Value)
        {
            WriteUnnamedJsonValue(Ar, Element);
        }
        Ar.finishNode();
        return;
    }

    if (Value.is_null())
    {
        std::nullptr_t NullValue = nullptr;
        Ar(Nvp(Name, NullValue));
        return;
    }

    if (Value.is_boolean())
    {
        const bool BoolValue = Value.get<bool>();
        Ar(Nvp(Name, BoolValue));
        return;
    }

    if (Value.is_number_unsigned())
    {
        const std::uint64_t UnsignedValue = Value.get<std::uint64_t>();
        Ar(Nvp(Name, UnsignedValue));
        return;
    }

    if (Value.is_number_integer())
    {
        const std::int64_t SignedValue = Value.get<std::int64_t>();
        Ar(Nvp(Name, SignedValue));
        return;
    }

    if (Value.is_number_float())
    {
        const double FloatValue = Value.get<double>();
        Ar(Nvp(Name, FloatValue));
        return;
    }

    if (Value.is_string())
    {
        const std::string Text = Value.get<std::string>();
        Ar(Nvp(Name, Text));
        return;
    }

    throw cereal::Exception("Unsupported nlohmann::json value during authored asset save");
}

inline void WriteUnnamedJsonValue(cereal::JSONOutputArchive& Ar, const nlohmann::json& Value)
{
    if (Value.is_object())
    {
        Ar.startNode();
        for (auto It = Value.begin(); It != Value.end(); ++It)
        {
            const std::string Key = It.key();
            WriteNamedJsonValue(Ar, Key.c_str(), It.value());
        }
        Ar.finishNode();
        return;
    }

    if (Value.is_array())
    {
        Ar.startNode();
        Ar.makeArray();
        for (const auto& Element : Value)
        {
            WriteUnnamedJsonValue(Ar, Element);
        }
        Ar.finishNode();
        return;
    }

    if (Value.is_null())
    {
        std::nullptr_t NullValue = nullptr;
        Ar(NullValue);
        return;
    }

    if (Value.is_boolean())
    {
        const bool BoolValue = Value.get<bool>();
        Ar(BoolValue);
        return;
    }

    if (Value.is_number_unsigned())
    {
        const std::uint64_t UnsignedValue = Value.get<std::uint64_t>();
        Ar(UnsignedValue);
        return;
    }

    if (Value.is_number_integer())
    {
        const std::int64_t SignedValue = Value.get<std::int64_t>();
        Ar(SignedValue);
        return;
    }

    if (Value.is_number_float())
    {
        const double FloatValue = Value.get<double>();
        Ar(FloatValue);
        return;
    }

    if (Value.is_string())
    {
        const std::string Text = Value.get<std::string>();
        Ar(Text);
        return;
    }

    throw cereal::Exception("Unsupported nlohmann::json value during authored asset save");
}

template<typename TAsset>
[[nodiscard]] Result SaveAuthoredAssetViaCerealJsonStream(const TAsset& Asset, std::ostream& Output)
{
    try
    {
        {
            cereal::JSONOutputArchive Archive(Output);
            Archive(Nvp("Asset", Asset));
        }

        if (!Output.good())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to write authored asset JSON"));
        }
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
    catch (...)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError,
                                         "Unknown exception while saving authored asset JSON"));
    }
}

} // namespace SnAPI::GameFramework::Detail

namespace cereal
{

template <class Archive,
          std::enable_if_t<!std::is_same_v<std::remove_cvref_t<Archive>, JSONOutputArchive> &&
                               !std::is_same_v<std::remove_cvref_t<Archive>, JSONInputArchive>,
                           int> = 0>
void save(Archive& ArchiveRef, const ::SnAPI::AssetPipeline::Uuid& Id)
{
    std::array<uint8_t, 16> Data{};
    for (size_t i = 0; i < Data.size(); ++i)
    {
        Data[i] = Id.Bytes[i];
    }
    ArchiveRef(Data);
}

template <class Archive,
          std::enable_if_t<!std::is_same_v<std::remove_cvref_t<Archive>, JSONOutputArchive> &&
                               !std::is_same_v<std::remove_cvref_t<Archive>, JSONInputArchive>,
                           int> = 0>
void load(Archive& ArchiveRef, ::SnAPI::AssetPipeline::Uuid& Id)
{
    std::array<uint8_t, 16> Data{};
    ArchiveRef(Data);
    for (size_t i = 0; i < Data.size(); ++i)
    {
        Id.Bytes[i] = Data[i];
    }
}

template <class Archive,
          std::enable_if_t<!std::is_same_v<std::remove_cvref_t<Archive>, JSONOutputArchive> &&
                               !std::is_same_v<std::remove_cvref_t<Archive>, JSONInputArchive>,
                           int> = 0>
void save(Archive& ArchiveRef, const SnAPI::GameFramework::Uuid& Id)
{
    std::array<uint8_t, 16> Data{};
    const auto& Bytes = Id.as_bytes();
    for (size_t i = 0; i < Data.size(); ++i)
    {
        Data[i] = static_cast<uint8_t>(std::to_integer<uint8_t>(Bytes[i]));
    }
    ArchiveRef(Data);
}

template <class Archive,
          std::enable_if_t<!std::is_same_v<std::remove_cvref_t<Archive>, JSONOutputArchive> &&
                               !std::is_same_v<std::remove_cvref_t<Archive>, JSONInputArchive>,
                           int> = 0>
void load(Archive& ArchiveRef, SnAPI::GameFramework::Uuid& Id)
{
    std::array<uint8_t, 16> Data{};
    ArchiveRef(Data);
    Id = SnAPI::GameFramework::Uuid(Data);
}

template <class Archive,
          std::enable_if_t<!std::is_same_v<std::remove_cvref_t<Archive>, JSONOutputArchive> &&
                               !std::is_same_v<std::remove_cvref_t<Archive>, JSONInputArchive>,
                           int> = 0>
void save(Archive& ArchiveRef, const SnAPI::GameFramework::TypeId& Id)
{
    save(ArchiveRef, Id.Value);
}

template <class Archive,
          std::enable_if_t<!std::is_same_v<std::remove_cvref_t<Archive>, JSONOutputArchive> &&
                               !std::is_same_v<std::remove_cvref_t<Archive>, JSONInputArchive>,
                           int> = 0>
void load(Archive& ArchiveRef, SnAPI::GameFramework::TypeId& Id)
{
    load(ArchiveRef, Id.Value);
}

inline std::string save_minimal(const JSONOutputArchive&, const ::SnAPI::AssetPipeline::Uuid& Id)
{
    return Id.ToString();
}

inline void load_minimal(const JSONInputArchive&, ::SnAPI::AssetPipeline::Uuid& Id, const std::string& Text)
{
    Id = ::SnAPI::AssetPipeline::Uuid::FromString(Text);
}

inline std::string save_minimal(const JSONOutputArchive&, const SnAPI::GameFramework::Uuid& Id)
{
    if (const auto* Info = SnAPI::GameFramework::TypeRegistry::Instance().Find(Id))
    {
        return Info->Name;
    }
    return SnAPI::GameFramework::ToString(Id);
}

inline void load_minimal(const JSONInputArchive&, SnAPI::GameFramework::Uuid& Id, const std::string& Text)
{
    if (const auto* Info = SnAPI::GameFramework::TypeRegistry::Instance().FindByName(Text))
    {
        Id = Info->Id.Value;
        return;
    }

    const auto Parsed = SnAPI::GameFramework::Uuid::from_string(Text);
    if (!Parsed)
    {
        throw cereal::Exception(("Invalid UUID/type string: " + Text).c_str());
    }

    Id = *Parsed;
}

inline std::string save_minimal(const JSONOutputArchive&, const SnAPI::GameFramework::TypeId& Id)
{
    if (const auto* Info = SnAPI::GameFramework::TypeRegistry::Instance().Find(Id))
    {
        return Info->Name;
    }
    return SnAPI::GameFramework::ToString(Id);
}

inline void load_minimal(const JSONInputArchive&, SnAPI::GameFramework::TypeId& Id, const std::string& Text)
{
    if (const auto* Info = SnAPI::GameFramework::TypeRegistry::Instance().FindByName(Text))
    {
        Id = Info->Id;
        return;
    }

    const auto Parsed = SnAPI::GameFramework::Uuid::from_string(Text);
    if (!Parsed)
    {
        throw cereal::Exception(("Invalid UUID/type string: " + Text).c_str());
    }

    Id = SnAPI::GameFramework::TypeId(*Parsed);
}

} // namespace cereal

namespace SnAPI::GameFramework
{

template<class Archive>
void SerializeAuthoredAssetIdentity(Archive& Ar, IAsset& Value)
{
    Ar(Detail::Nvp("AssetId", Value.AssetId),
       Detail::Nvp("LogicalName", Value.LogicalName));
}

template<class Archive>
void serialize(Archive& Ar, AssetRefPayload& Value)
{
    Ar(Detail::Nvp("AssetName", Value.AssetName),
       Detail::Nvp("AssetId", Value.AssetId));
}

template<class Archive>
void serialize(Archive& Ar, ImportBuildOptionPayload& Value)
{
    Ar(Detail::Nvp("Key", Value.Key),
       Detail::Nvp("Value", Value.Value));
}

template<class Archive>
void serialize(Archive& Ar, ImportedAssetProvenancePayload& Value)
{
    Ar(Detail::Nvp("Profile", Value.Profile),
       Detail::Nvp("SourcePath", Value.SourcePath),
       Detail::Nvp("DestinationFolder", Value.DestinationFolder),
       Detail::Nvp("ImporterName", Value.ImporterName),
       Detail::Nvp("BuildOptions", Value.BuildOptions));
}

template<class Archive, typename TBase, typename TNameTag>
void serialize(Archive& Ar, TAssetRef<TBase, TNameTag>& Value)
{
    std::string AssetName = Value.GetAssetName();
    std::string AssetId = Value.GetAssetId();
    Ar(Detail::Nvp("AssetName", AssetName),
       Detail::Nvp("AssetId", AssetId));

    if constexpr (Archive::is_loading::value)
    {
        Value = TAssetRef<TBase, TNameTag>(std::move(AssetName), std::move(AssetId));
    }
}

template<class Archive>
void serialize(Archive& Ar, MeshStreamChunkRef& Value)
{
    Ar(Detail::Nvp("Semantic", Value.Semantic),
       Detail::Nvp("BulkIndex", Value.BulkIndex),
       Detail::Nvp("ElementCount", Value.ElementCount),
       Detail::Nvp("StrideBytes", Value.StrideBytes));
}

template<class Archive>
void serialize(Archive& Ar, StaticSubMeshPayload& Value)
{
    Ar(Detail::Nvp("IndexOffset", Value.IndexOffset),
       Detail::Nvp("IndexCount", Value.IndexCount),
       Detail::Nvp("MaterialSlot", Value.MaterialSlot),
       Detail::Nvp("BoundsMin", Value.BoundsMin),
       Detail::Nvp("BoundsMax", Value.BoundsMax));
}

template<class Archive>
void serialize(Archive& Ar, StaticMeshPayload& Value)
{
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("BoundsMin", Value.BoundsMin),
       Detail::Nvp("BoundsMax", Value.BoundsMax),
       Detail::Nvp("SubMeshes", Value.SubMeshes),
       Detail::Nvp("MaterialInstances", Value.MaterialInstances),
       Detail::Nvp("Streams", Value.Streams));
}

template<class Archive>
void serialize(Archive& Ar, MeshStreamSourcePayload& Value)
{
    Ar(Detail::Nvp("Semantic", Value.Semantic),
       Detail::Nvp("SubIndex", Value.SubIndex),
       Detail::Nvp("Uri", Value.Uri),
       Detail::Nvp("Bytes", Value.Bytes),
       Detail::Nvp("ElementCount", Value.ElementCount),
       Detail::Nvp("StrideBytes", Value.StrideBytes),
       Detail::Nvp("Compress", Value.Compress));
}

template<class Archive>
void serialize(Archive& Ar, MeshImportSettingsPayload& Value)
{
    Ar(Detail::Nvp("GenerateNormals", Value.GenerateNormals),
       Detail::Nvp("GenerateTangents", Value.GenerateTangents),
       Detail::Nvp("FlipUVs", Value.FlipUVs),
       Detail::Nvp("OptimizeMeshes", Value.OptimizeMeshes),
       Detail::Nvp("ForceSkeletal", Value.ForceSkeletal),
       Detail::Nvp("ForceStatic", Value.ForceStatic),
       Detail::Nvp("ImportMaterials", Value.ImportMaterials),
       Detail::Nvp("ImportTextures", Value.ImportTextures),
       Detail::Nvp("ImportAnimations", Value.ImportAnimations),
       Detail::Nvp("ImportSkeleton", Value.ImportSkeleton),
       Detail::Nvp("MaxBonesPerVertex", Value.MaxBonesPerVertex));
}

template<class Archive>
void serialize(Archive& Ar, AssimpImporterSettings& Value)
{
    Ar(Detail::Nvp("Mesh", Value.Mesh),
       Detail::Nvp("LogicalNameOverride", Value.LogicalNameOverride),
       Detail::Nvp("DefaultShaderModule", Value.DefaultShaderModule),
       Detail::Nvp("DefaultShadingModel", Value.DefaultShadingModel));
}

template<class Archive>
void serialize(Archive& Ar, TextureSourceImagePayload& Value)
{
    Ar(Detail::Nvp("Width", Value.Width),
       Detail::Nvp("Height", Value.Height),
       Detail::Nvp("Channels", Value.Channels),
       Detail::Nvp("BitsPerChannel", Value.BitsPerChannel),
       Detail::Nvp("IsFloat", Value.IsFloat),
       Detail::Nvp("HasNonTrivialAlpha", Value.HasNonTrivialAlpha),
       Detail::Nvp("SRGB", Value.SRGB),
       Detail::Nvp("SourceFilename", Value.SourceFilename),
       Detail::Nvp("Pixels", Value.Pixels));
}

template<class Archive>
void serialize(Archive& Ar, TextureImporterSettings& Value)
{
    Ar(Detail::Nvp("Target", Value.Target),
       Detail::Nvp("Format", Value.Format),
       Detail::Nvp("Quality", Value.Quality),
       Detail::Nvp("ForceSrgb", Value.ForceSrgb),
       Detail::Nvp("ForceLinear", Value.ForceLinear),
       Detail::Nvp("ForceNormalMap", Value.ForceNormalMap),
       Detail::Nvp("MaxMips", Value.MaxMips));
}

template<class Archive>
void serialize(Archive& Ar, TextureAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("Image", Value.Image),
       Detail::Nvp("ImportSettings", Value.ImportSettings),
       Detail::Nvp("Provenance", Value.Provenance));
}

template<class Archive>
void serialize(Archive& Ar, StaticMeshAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("Mesh", Value.Mesh),
       Detail::Nvp("Streams", Value.Streams),
       Detail::Nvp("ImportSettings", Value.ImportSettings),
       Detail::Nvp("Provenance", Value.Provenance));
}

template<class Archive>
void serialize(Archive& Ar, SkeletalMeshAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("BaseMesh", Value.BaseMesh),
       Detail::Nvp("Bones", Value.Bones),
       Detail::Nvp("Skeleton", Value.Skeleton),
       Detail::Nvp("Animations", Value.Animations),
       Detail::Nvp("SkeletonAnimationUri", Value.SkeletonAnimationUri),
       Detail::Nvp("SkeletonAnimationBytes", Value.SkeletonAnimationBytes),
       Detail::Nvp("SkeletonAnimationSubIndex", Value.SkeletonAnimationSubIndex),
       Detail::Nvp("CompressSkeletonAnimation", Value.CompressSkeletonAnimation),
       Detail::Nvp("Provenance", Value.Provenance));
}

template<class Archive>
void serialize(Archive& Ar, SkeletonAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("Skeleton", Value.Skeleton),
       Detail::Nvp("Provenance", Value.Provenance));
}

template<class Archive>
void serialize(Archive& Ar, SkeletalAnimationAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("Animation", Value.Animation),
       Detail::Nvp("Provenance", Value.Provenance));
}

template<class Archive>
void serialize(Archive& Ar, SkeletalBonePayload& Value)
{
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("ParentIndex", Value.ParentIndex),
       Detail::Nvp("BindPose", Value.BindPose));
}

template<class Archive>
void serialize(Archive& Ar, SkeletonPayload& Value)
{
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("Bones", Value.Bones));
}

template<class Archive>
void serialize(Archive& Ar, AnimationKeyFramePayload& Value)
{
    Ar(Detail::Nvp("Time", Value.Time),
       Detail::Nvp("Translation", Value.Translation),
       Detail::Nvp("Rotation", Value.Rotation),
       Detail::Nvp("Scale", Value.Scale));
}

template<class Archive>
void serialize(Archive& Ar, AnimationTrackPayload& Value)
{
    Ar(Detail::Nvp("BoneName", Value.BoneName),
       Detail::Nvp("KeyFrames", Value.KeyFrames));
}

template<class Archive>
void serialize(Archive& Ar, AnimationPayload& Value)
{
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("DurationSeconds", Value.DurationSeconds),
       Detail::Nvp("TicksPerSecond", Value.TicksPerSecond),
       Detail::Nvp("Tracks", Value.Tracks));
}

template<class Archive>
void serialize(Archive& Ar, SkeletalMeshPayload& Value)
{
    Ar(Detail::Nvp("BaseMesh", Value.BaseMesh),
       Detail::Nvp("Bones", Value.Bones),
       Detail::Nvp("Skeleton", Value.Skeleton),
       Detail::Nvp("Animations", Value.Animations),
       Detail::Nvp("SkeletonAnimationBulkIndex", Value.SkeletonAnimationBulkIndex));
}

template<class Archive>
void serialize(Archive& Ar, MaterialAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("ShaderModule", Value.ShaderModule),
       Detail::Nvp("ShadingModel", Value.ShadingModel),
       Detail::Nvp("FeatureAlbedoMap", Value.FeatureAlbedoMap),
       Detail::Nvp("FeatureNormalMap", Value.FeatureNormalMap),
       Detail::Nvp("FeatureRoughnessMap", Value.FeatureRoughnessMap),
       Detail::Nvp("FeatureMetalnessMap", Value.FeatureMetalnessMap),
       Detail::Nvp("FeatureOcclusionMap", Value.FeatureOcclusionMap),
       Detail::Nvp("FeatureAlphaTest", Value.FeatureAlphaTest),
       Detail::Nvp("FeatureAlphaBlend", Value.FeatureAlphaBlend),
       Detail::Nvp("FeatureDoubleSided", Value.FeatureDoubleSided),
       Detail::Nvp("FeatureInstancing", Value.FeatureInstancing),
       Detail::Nvp("Provenance", Value.Provenance));
}

template<class Archive>
void serialize(Archive& Ar, MaterialScalarParamPayload& Value)
{
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("Value", Value.Value));
}

template<class Archive>
void serialize(Archive& Ar, MaterialVectorParamPayload& Value)
{
    Ar(Detail::Nvp("Name", Value.Name));

    using ArchiveType = std::remove_cvref_t<Archive>;
    if constexpr (std::is_same_v<ArchiveType, cereal::JSONOutputArchive>)
    {
        std::vector<float> Components(Value.Value.begin(), Value.Value.end());
        Ar(Detail::Nvp("Value", Components));
    }
    else if constexpr (std::is_same_v<ArchiveType, cereal::JSONInputArchive>)
    {
        std::vector<float> Components{};
        Ar(Detail::Nvp("Value", Components));
        if (Components.size() != Value.Value.size())
        {
            throw cereal::Exception("MaterialVectorParamPayload.Value must contain 4 elements");
        }
        std::copy(Components.begin(), Components.end(), Value.Value.begin());
    }
    else
    {
        Ar(Detail::Nvp("Value", Value.Value));
    }
}

template<class Archive>
void serialize(Archive& Ar, MaterialTextureParamPayload& Value)
{
    Ar(Detail::Nvp("SlotName", Value.SlotName),
       Detail::Nvp("Texture", Value.Texture),
       Detail::Nvp("SRGB", Value.SRGB));
}

template<class Archive>
void serialize(Archive& Ar, MaterialInstanceAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("ParentMaterial", Value.ParentMaterial),
       Detail::Nvp("Scalars", Value.Scalars),
       Detail::Nvp("Vectors", Value.Vectors),
       Detail::Nvp("Textures", Value.Textures),
       Detail::Nvp("Provenance", Value.Provenance));
}

template<class Archive>
void serialize(Archive& Ar, NodeFieldAsset& Value)
{
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("Value", Value.Value));
}

template<class Archive>
void serialize(Archive& Ar, NodeComponentAsset& Value)
{
    Ar(Detail::Nvp("Id", Value.Id),
       Detail::Nvp("Type", Value.Type),
       Detail::Nvp("Fields", Value.Fields));
}

template<class Archive>
void serialize(Archive& Ar, NodeObjectAsset& Value)
{
    Ar(Detail::Nvp("Id", Value.Id),
       Detail::Nvp("Type", Value.Type),
       Detail::Nvp("Name", Value.Name),
       Detail::Nvp("Active", Value.Active),
       Detail::Nvp("Fields", Value.Fields),
       Detail::Nvp("Components", Value.Components),
       Detail::Nvp("Children", Value.Children));
}

template<class Archive>
void serialize(Archive& Ar, NodeAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("Nodes", Value.Nodes));
}

template<class Archive>
void serialize(Archive& Ar, LevelAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("Nodes", Value.Nodes));
}

template<class Archive>
void serialize(Archive& Ar, WorldAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("Nodes", Value.Nodes));
}

namespace Conduit
{

template<class Archive>
void serialize(Archive& Ar, SlotId& Value)
{
    Ar(Detail::Nvp("Value", Value.Value));
}

template <class Archive,
          std::enable_if_t<!std::is_same_v<std::remove_cvref_t<Archive>, cereal::JSONOutputArchive> &&
                               !std::is_same_v<std::remove_cvref_t<Archive>, cereal::JSONInputArchive>,
                           int> = 0>
void serialize(Archive& Ar, SerializedValue& Value)
{
    Ar(Detail::Nvp("Type", Value.Type),
       Detail::Nvp("Bytes", Value.Bytes));
}

inline void save(cereal::JSONOutputArchive& Ar, const SerializedValue& Value)
{
    auto JsonValue = SerializeSerializedValueToJsonValue(Value);
    if (!JsonValue)
    {
        throw cereal::Exception(JsonValue.error().Message.c_str());
    }

    for (auto It = JsonValue->begin(); It != JsonValue->end(); ++It)
    {
        const std::string Key = It.key();
        Detail::WriteNamedJsonValue(Ar, Key.c_str(), It.value());
    }
}

template<class Archive>
void serialize(Archive& Ar, GraphSlotAsset& Value)
{
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("Type", Value.Type),
       Detail::Nvp("Kind", Value.Kind));
}

template<class Archive>
void serialize(Archive& Ar, GraphVariableAsset& Value)
{
    Ar(Detail::Nvp("Id", Value.Id),
       Detail::Nvp("Name", Value.Name),
       Detail::Nvp("Type", Value.Type),
       Detail::Nvp("DefaultValue", Value.DefaultValue));
}

template<class Archive>
void serialize(Archive& Ar, GraphViewportAsset& Value)
{
    Ar(Detail::Nvp("PanX", Value.PanX),
       Detail::Nvp("PanY", Value.PanY),
       Detail::Nvp("Zoom", Value.Zoom));
}

template<class Archive>
void serialize(Archive& Ar, GraphNodeInputDefaultAsset& Value)
{
    Ar(Detail::Nvp("PinKey", Value.PinKey),
       Detail::Nvp("Value", Value.Value));
}

template<class Archive>
void serialize(Archive& Ar, GraphNodeEditorAsset& Value)
{
    Ar(Detail::Nvp("NodeId", Value.NodeId),
       Detail::Nvp("X", Value.X),
       Detail::Nvp("Y", Value.Y),
       Detail::Nvp("Width", Value.Width),
       Detail::Nvp("IsCollapsed", Value.IsCollapsed));
}

template<class Archive>
void serialize(Archive& Ar, GraphCommentAsset& Value)
{
    Ar(Detail::Nvp("Id", Value.Id),
       Detail::Nvp("Title", Value.Title),
       Detail::Nvp("X", Value.X),
       Detail::Nvp("Y", Value.Y),
       Detail::Nvp("Width", Value.Width),
       Detail::Nvp("Height", Value.Height),
       Detail::Nvp("ColorRgba", Value.ColorRgba),
       Detail::Nvp("NodeIds", Value.NodeIds));
}

template<class Archive>
void serialize(Archive& Ar, GraphBookmarkAsset& Value)
{
    Ar(Detail::Nvp("Id", Value.Id),
       Detail::Nvp("Name", Value.Name),
       Detail::Nvp("PanX", Value.PanX),
       Detail::Nvp("PanY", Value.PanY),
       Detail::Nvp("Zoom", Value.Zoom));
}

template<class Archive>
void serialize(Archive& Ar, GraphEditorAssetState& Value)
{
    Ar(Detail::Nvp("Viewport", Value.Viewport),
       Detail::Nvp("Nodes", Value.Nodes),
       Detail::Nvp("Comments", Value.Comments),
       Detail::Nvp("Bookmarks", Value.Bookmarks));
}

template<class Archive>
void serialize(Archive& Ar, GraphNodeAsset& Value)
{
    Ar(Detail::Nvp("Id", Value.Id),
       Detail::Nvp("Kind", Value.Kind),
       Detail::Nvp("BuiltinEntryPoint", Value.BuiltinEntryPoint),
       Detail::Nvp("EntryPointName", Value.EntryPointName),
       Detail::Nvp("VariableId", Value.VariableId),
       Detail::Nvp("LabelName", Value.LabelName),
       Detail::Nvp("FalseLabelName", Value.FalseLabelName),
       Detail::Nvp("MemberName", Value.MemberName),
       Detail::Nvp("ExecTargetNodeId", Value.ExecTargetNodeId),
       Detail::Nvp("FalseExecTargetNodeId", Value.FalseExecTargetNodeId),
       Detail::Nvp("ConstantValue", Value.ConstantValue),
       Detail::Nvp("UnaryOp", Value.UnaryOp),
       Detail::Nvp("BinaryOp", Value.BinaryOp),
       Detail::Nvp("Input", Value.Input),
       Detail::Nvp("Left", Value.Left),
       Detail::Nvp("Right", Value.Right),
       Detail::Nvp("Output", Value.Output),
       Detail::Nvp("Condition", Value.Condition),
       Detail::Nvp("Instance", Value.Instance),
       Detail::Nvp("ReturnSlot", Value.ReturnSlot),
       Detail::Nvp("OwnerType", Value.OwnerType),
       Detail::Nvp("Inputs", Value.Inputs),
       Detail::Nvp("InputDefaults", Value.InputDefaults));
}

template<class Archive>
void serialize(Archive& Ar, GraphAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("SelfType", Value.SelfType),
       Detail::Nvp("Slots", Value.Slots),
       Detail::Nvp("Variables", Value.Variables),
       Detail::Nvp("Nodes", Value.Nodes),
       Detail::Nvp("EditorState", Value.EditorState));
}

template<class Archive>
void serialize(Archive& Ar, ClassAsset& Value)
{
    SerializeAuthoredAssetIdentity(Ar, Value);
    Ar(Detail::Nvp("Name", Value.Name),
       Detail::Nvp("HostType", Value.HostType),
       Detail::Nvp("Graph", Value.Graph));
}

} // namespace Conduit
} // namespace SnAPI::GameFramework
