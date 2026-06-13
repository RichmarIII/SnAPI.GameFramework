#include "Editor/EditorAssetIconService.h"

#include "AuthoredAssetLoading.h"
#include "RenderAssets/TextureAsset.h"
#include "TextureCompressorIds.h"
#include "TypeAutoRegistry.h"

#include <FreeImage.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <unordered_set>

namespace SnAPI::GameFramework::Editor
{
namespace
{
constexpr std::uint32_t kMaxThumbnailExtent = 128u;
constexpr int kThumbnailCacheVersion = 1;

struct SourceFileFingerprint
{
    std::uint64_t Timestamp = 0;
    std::uintmax_t Size = 0;
};

[[nodiscard]] std::filesystem::path ResolveUserCacheRootPath()
{
#if defined(_WIN32)
    if (const char* LocalAppData = std::getenv("LOCALAPPDATA"))
    {
        return std::filesystem::path(LocalAppData);
    }
    if (const char* RoamingAppData = std::getenv("APPDATA"))
    {
        return std::filesystem::path(RoamingAppData);
    }
    return {};
#elif defined(__APPLE__)
    if (const char* Home = std::getenv("HOME"))
    {
        return std::filesystem::path(Home) / "Library" / "Caches";
    }
    return {};
#else
    if (const char* XdgCacheHome = std::getenv("XDG_CACHE_HOME"))
    {
        return std::filesystem::path(XdgCacheHome);
    }
    if (const char* Home = std::getenv("HOME"))
    {
        return std::filesystem::path(Home) / ".cache";
    }
    return {};
#endif
}

[[nodiscard]] std::uint64_t HashStringFNV1a(const std::string_view Value)
{
    std::uint64_t Hash = 14695981039346656037ull;
    for (const unsigned char Character : Value)
    {
        Hash ^= static_cast<std::uint64_t>(Character);
        Hash *= 1099511628211ull;
    }
    return Hash;
}

[[nodiscard]] std::string BuildProjectCacheKey(const EditorAssetService::ProjectInfo& Project)
{
    const std::string_view Identity = !Project.ProjectFilePath.empty()
        ? std::string_view(Project.ProjectFilePath)
        : std::string_view(Project.AssetRootDirectory);
    if (Identity.empty())
    {
        return "default";
    }

    std::ostringstream Output{};
    Output << std::hex << HashStringFNV1a(Identity);
    return Output.str();
}

[[nodiscard]] std::uint64_t ToTimestampValue(const std::filesystem::file_time_type Timestamp)
{
    return static_cast<std::uint64_t>(Timestamp.time_since_epoch().count());
}

[[nodiscard]] std::optional<SourceFileFingerprint> ReadSourceFileFingerprint(const std::filesystem::path& Path)
{
    if (Path.empty())
    {
        return std::nullopt;
    }

    std::error_code Error{};
    const auto Timestamp = std::filesystem::last_write_time(Path, Error);
    if (Error)
    {
        return std::nullopt;
    }

    Error.clear();
    const auto Size = std::filesystem::file_size(Path, Error);
    if (Error)
    {
        return std::nullopt;
    }

    return SourceFileFingerprint{
        .Timestamp = ToTimestampValue(Timestamp),
        .Size = Size,
    };
}

[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> ResolveThumbnailExtent(const std::uint32_t Width,
                                                                              const std::uint32_t Height)
{
    if (Width == 0u || Height == 0u)
    {
        return {0u, 0u};
    }

    const double Scale = std::min(
        1.0,
        std::min(
            static_cast<double>(kMaxThumbnailExtent) / static_cast<double>(Width),
            static_cast<double>(kMaxThumbnailExtent) / static_cast<double>(Height)));

    const auto ThumbnailWidth = static_cast<std::uint32_t>(std::max(1.0, std::round(static_cast<double>(Width) * Scale)));
    const auto ThumbnailHeight = static_cast<std::uint32_t>(std::max(1.0, std::round(static_cast<double>(Height) * Scale)));
    return {ThumbnailWidth, ThumbnailHeight};
}

[[nodiscard]] bool ConvertTextureImageToRgba(const TextureAsset& Asset, std::vector<std::uint8_t>& OutPixels)
{
    return DecodeTextureSourceImageToRgba(Asset.Image, OutPixels).has_value();
}

[[nodiscard]] std::vector<std::uint8_t> ScaleRgbaNearest(const std::vector<std::uint8_t>& SourcePixels,
                                                         const std::uint32_t SourceWidth,
                                                         const std::uint32_t SourceHeight,
                                                         const std::uint32_t TargetWidth,
                                                         const std::uint32_t TargetHeight)
{
    if (SourceWidth == 0u || SourceHeight == 0u || TargetWidth == 0u || TargetHeight == 0u)
    {
        return {};
    }

    if (SourceWidth == TargetWidth && SourceHeight == TargetHeight)
    {
        return SourcePixels;
    }

    std::vector<std::uint8_t> TargetPixels(static_cast<std::size_t>(TargetWidth) * static_cast<std::size_t>(TargetHeight) * 4u);
    for (std::uint32_t Y = 0; Y < TargetHeight; ++Y)
    {
        const std::uint32_t SourceY = std::min(SourceHeight - 1u, (Y * SourceHeight) / TargetHeight);
        for (std::uint32_t X = 0; X < TargetWidth; ++X)
        {
            const std::uint32_t SourceX = std::min(SourceWidth - 1u, (X * SourceWidth) / TargetWidth);
            const std::size_t SourceIndex =
                (static_cast<std::size_t>(SourceY) * static_cast<std::size_t>(SourceWidth) + static_cast<std::size_t>(SourceX)) * 4u;
            const std::size_t TargetIndex =
                (static_cast<std::size_t>(Y) * static_cast<std::size_t>(TargetWidth) + static_cast<std::size_t>(X)) * 4u;
            std::copy_n(SourcePixels.data() + SourceIndex, 4u, TargetPixels.data() + TargetIndex);
        }
    }

    return TargetPixels;
}

[[nodiscard]] bool WritePngThumbnail(const std::filesystem::path& OutputPath,
                                     const std::uint32_t Width,
                                     const std::uint32_t Height,
                                     const std::vector<std::uint8_t>& Pixels)
{
    static std::once_flag FreeImageInitOnce{};
    std::call_once(FreeImageInitOnce, [] {
        FreeImage_Initialise(FALSE);
    });

    if (OutputPath.empty() || Width == 0u || Height == 0u ||
        Pixels.size() < static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height) * 4u)
    {
        return false;
    }

    std::error_code Error{};
    std::filesystem::create_directories(OutputPath.parent_path(), Error);
    if (Error)
    {
        return false;
    }

    FIBITMAP* Bitmap = FreeImage_Allocate(static_cast<int>(Width), static_cast<int>(Height), 32);
    if (!Bitmap)
    {
        return false;
    }

    for (std::uint32_t Y = 0; Y < Height; ++Y)
    {
        BYTE* ScanLine = FreeImage_GetScanLine(Bitmap, static_cast<int>(Height - 1u - Y));
        for (std::uint32_t X = 0; X < Width; ++X)
        {
            const std::size_t SourceIndex =
                (static_cast<std::size_t>(Y) * static_cast<std::size_t>(Width) + static_cast<std::size_t>(X)) * 4u;
            ScanLine[X * 4u + FI_RGBA_RED] = Pixels[SourceIndex + 0u];
            ScanLine[X * 4u + FI_RGBA_GREEN] = Pixels[SourceIndex + 1u];
            ScanLine[X * 4u + FI_RGBA_BLUE] = Pixels[SourceIndex + 2u];
            ScanLine[X * 4u + FI_RGBA_ALPHA] = Pixels[SourceIndex + 3u];
        }
    }

    const bool Saved = FreeImage_Save(FIF_PNG, Bitmap, OutputPath.string().c_str(), PNG_DEFAULT) != FALSE;
    FreeImage_Unload(Bitmap);
    return Saved;
}
} // namespace

EditorAssetIconService::~EditorAssetIconService() = default;

std::string_view EditorAssetIconService::Name() const
{
    return "EditorAssetIconService";
}

std::vector<std::type_index> EditorAssetIconService::Dependencies() const
{
    return {std::type_index(typeid(EditorAssetService))};
}

Result EditorAssetIconService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_loadedProjectKey.clear();
    m_cacheRoot.clear();
    m_metadataPath.clear();
    m_thumbnailDirectory.clear();
    m_cachedTextureThumbnailsByAssetKey.clear();
    m_metadataDirty = false;
    ++m_revision;
    return Ok();
}

void EditorAssetIconService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    SaveThumbnailCacheIndex();
    m_loadedProjectKey.clear();
    m_cacheRoot.clear();
    m_metadataPath.clear();
    m_thumbnailDirectory.clear();
    m_cachedTextureThumbnailsByAssetKey.clear();
    m_metadataDirty = false;
}

void EditorAssetIconService::EnsureProjectCacheLoaded(EditorServiceContext& Context)
{
    auto* AssetService = Context.GetService<EditorAssetService>();
    const EditorAssetService::ProjectInfo EmptyProject{};
    const EditorAssetService::ProjectInfo& Project = AssetService ? AssetService->CurrentProject() : EmptyProject;
    const std::string ProjectKey = BuildProjectCacheKey(Project);
    if (ProjectKey == m_loadedProjectKey)
    {
        return;
    }

    SaveThumbnailCacheIndex();
    m_cachedTextureThumbnailsByAssetKey.clear();
    m_metadataDirty = false;

    const std::filesystem::path CacheBase = ResolveUserCacheRootPath();
    if (CacheBase.empty())
    {
        m_loadedProjectKey = ProjectKey;
        m_cacheRoot.clear();
        m_metadataPath.clear();
        m_thumbnailDirectory.clear();
        return;
    }

    m_loadedProjectKey = ProjectKey;
    m_cacheRoot = CacheBase / "SnAPI" / "GameFramework" / "Editor" / "AssetIcons" / ProjectKey;
    m_metadataPath = m_cacheRoot / "asseticoncache.json";
    m_thumbnailDirectory = m_cacheRoot / "thumbnails";
    LoadThumbnailCacheIndex();
}

void EditorAssetIconService::LoadThumbnailCacheIndex()
{
    m_cachedTextureThumbnailsByAssetKey.clear();
    if (m_metadataPath.empty())
    {
        return;
    }

    std::ifstream Input(m_metadataPath, std::ios::binary);
    if (!Input.is_open())
    {
        return;
    }

    nlohmann::json Json = nlohmann::json::parse(Input, nullptr, false);
    if (Json.is_discarded() || !Json.is_object())
    {
        return;
    }
    if (Json.value("version", 0) != kThumbnailCacheVersion)
    {
        return;
    }

    const nlohmann::json& Entries = Json["entries"];
    if (!Entries.is_array())
    {
        return;
    }

    for (const nlohmann::json& Entry : Entries)
    {
        if (!Entry.is_object())
        {
            continue;
        }

        const std::string AssetKey = Entry.value("assetKey", std::string{});
        const std::string AssetIdText = Entry.value("assetId", std::string{});
        const std::string ThumbnailFileName = Entry.value("thumbnailFile", std::string{});
        if (AssetKey.empty() || AssetIdText.empty() || ThumbnailFileName.empty())
        {
            continue;
        }

        const auto AssetId = ::SnAPI::AssetPipeline::AssetId::FromString(AssetIdText);
        if (AssetId.IsNull())
        {
            continue;
        }

        const std::filesystem::path ThumbnailPath = m_thumbnailDirectory / ThumbnailFileName;
        std::error_code ExistsError{};
        if (!std::filesystem::exists(ThumbnailPath, ExistsError) || ExistsError)
        {
            continue;
        }

        CachedTextureThumbnail Thumbnail{};
        Thumbnail.AssetId = AssetId;
        Thumbnail.SourcePath = Entry.value("sourcePath", std::string{});
        Thumbnail.SourceTimestamp = Entry.value("sourceTimestamp", std::uint64_t{0});
        Thumbnail.SourceSize = Entry.value("sourceSize", std::uintmax_t{0});
        Thumbnail.ThumbnailFileName = ThumbnailFileName;
        Thumbnail.ThumbnailWidth = Entry.value("thumbnailWidth", std::uint32_t{0});
        Thumbnail.ThumbnailHeight = Entry.value("thumbnailHeight", std::uint32_t{0});
        m_cachedTextureThumbnailsByAssetKey[AssetKey] = std::move(Thumbnail);
    }
}

void EditorAssetIconService::SaveThumbnailCacheIndex()
{
    if (!m_metadataDirty || m_metadataPath.empty() || m_cacheRoot.empty())
    {
        return;
    }

    std::error_code Error{};
    std::filesystem::create_directories(m_thumbnailDirectory, Error);
    if (Error)
    {
        return;
    }

    nlohmann::json Json;
    Json["version"] = kThumbnailCacheVersion;
    Json["entries"] = nlohmann::json::array();

    std::vector<std::string> AssetKeys{};
    AssetKeys.reserve(m_cachedTextureThumbnailsByAssetKey.size());
    for (const auto& [AssetKey, _] : m_cachedTextureThumbnailsByAssetKey)
    {
        AssetKeys.push_back(AssetKey);
    }
    std::sort(AssetKeys.begin(), AssetKeys.end());

    for (const std::string& AssetKey : AssetKeys)
    {
        const auto It = m_cachedTextureThumbnailsByAssetKey.find(AssetKey);
        if (It == m_cachedTextureThumbnailsByAssetKey.end())
        {
            continue;
        }

        const CachedTextureThumbnail& Thumbnail = It->second;
        nlohmann::json Entry;
        Entry["assetKey"] = AssetKey;
        Entry["assetId"] = Thumbnail.AssetId.ToString();
        Entry["sourcePath"] = Thumbnail.SourcePath;
        Entry["sourceTimestamp"] = Thumbnail.SourceTimestamp;
        Entry["sourceSize"] = Thumbnail.SourceSize;
        Entry["thumbnailFile"] = Thumbnail.ThumbnailFileName;
        Entry["thumbnailWidth"] = Thumbnail.ThumbnailWidth;
        Entry["thumbnailHeight"] = Thumbnail.ThumbnailHeight;
        Json["entries"].push_back(std::move(Entry));
    }

    const std::filesystem::path TempPath = m_metadataPath.string() + ".tmp";
    std::ofstream Output(TempPath, std::ios::binary | std::ios::trunc);
    if (!Output.is_open())
    {
        return;
    }

    Output << Json.dump(2);
    Output.flush();
    if (!Output.good())
    {
        return;
    }
    Output.close();
    if (!Output.good())
    {
        return;
    }

    Error.clear();
    std::filesystem::rename(TempPath, m_metadataPath, Error);
    if (Error)
    {
        Error.clear();
        std::filesystem::remove(m_metadataPath, Error);
        Error.clear();
        std::filesystem::rename(TempPath, m_metadataPath, Error);
    }
    if (!Error)
    {
        m_metadataDirty = false;
    }
}

void EditorAssetIconService::RemoveCachedThumbnail(const std::string_view AssetKey)
{
    const auto It = m_cachedTextureThumbnailsByAssetKey.find(std::string(AssetKey));
    if (It == m_cachedTextureThumbnailsByAssetKey.end())
    {
        return;
    }

    if (!It->second.ThumbnailFileName.empty() && !m_thumbnailDirectory.empty())
    {
        std::error_code Error{};
        (void)std::filesystem::remove(m_thumbnailDirectory / It->second.ThumbnailFileName, Error);
    }

    m_cachedTextureThumbnailsByAssetKey.erase(It);
    m_metadataDirty = true;
    ++m_revision;
}

void EditorAssetIconService::PruneMissingTextureThumbnails(
    const std::vector<EditorAssetService::DiscoveredAsset>& Assets)
{
    if (m_cachedTextureThumbnailsByAssetKey.empty())
    {
        return;
    }

    std::unordered_set<std::string> LiveTextureKeys{};
    LiveTextureKeys.reserve(Assets.size());
    for (const auto& Asset : Assets)
    {
        if (Asset.AssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture)
        {
            LiveTextureKeys.insert(Asset.Key);
        }
    }

    std::vector<std::string> KeysToRemove{};
    KeysToRemove.reserve(m_cachedTextureThumbnailsByAssetKey.size());
    for (const auto& [AssetKey, _] : m_cachedTextureThumbnailsByAssetKey)
    {
        if (!LiveTextureKeys.contains(AssetKey))
        {
            KeysToRemove.push_back(AssetKey);
        }
    }

    if (KeysToRemove.empty())
    {
        return;
    }

    for (const std::string& AssetKey : KeysToRemove)
    {
        RemoveCachedThumbnail(AssetKey);
    }
    SaveThumbnailCacheIndex();
}

void EditorAssetIconService::Synchronize(EditorServiceContext& Context,
                                         const std::vector<EditorAssetService::DiscoveredAsset>& Assets,
                                         const SnAPI::UI::UIContext* UiContext)
{
    (void)UiContext;
    EnsureProjectCacheLoaded(Context);
    PruneMissingTextureThumbnails(Assets);
}

void EditorAssetIconService::InvalidateAsset(EditorServiceContext& Context, std::string_view AssetKey)
{
    EnsureProjectCacheLoaded(Context);
    if (AssetKey.empty())
    {
        return;
    }
    RemoveCachedThumbnail(AssetKey);
    SaveThumbnailCacheIndex();
}

EditorAssetIconService::AssetIconMetadata EditorAssetIconService::ResolveAssetIcon(
    EditorServiceContext& Context,
    const EditorAssetService::DiscoveredAsset& Asset,
    const SnAPI::UI::UIContext* UiContext)
{
    (void)UiContext;
    AssetIconMetadata Metadata = BuildFallbackIcon(Asset);
    if (Asset.AssetKind != TextureCompressorPlugin::AssetKind_CompressedTexture)
    {
        return Metadata;
    }

    EnsureProjectCacheLoaded(Context);
    AssetIconMetadata ThumbnailMetadata = ResolveTextureThumbnail(Asset);
    if (!ThumbnailMetadata.IconSource.empty())
    {
        return ThumbnailMetadata;
    }
    return Metadata;
}

EditorAssetIconService::AssetIconMetadata EditorAssetIconService::ResolveTextureThumbnail(
    const EditorAssetService::DiscoveredAsset& Asset)
{
    AssetIconMetadata Metadata{};
    if (Asset.SourceFilePath.empty() || m_thumbnailDirectory.empty())
    {
        return Metadata;
    }

    const std::filesystem::path SourcePath = std::filesystem::path(Asset.SourceFilePath).lexically_normal();
    const std::optional<SourceFileFingerprint> Fingerprint = ReadSourceFileFingerprint(SourcePath);
    if (!Fingerprint.has_value())
    {
        return Metadata;
    }

    if (const auto ExistingIt = m_cachedTextureThumbnailsByAssetKey.find(Asset.Key);
        ExistingIt != m_cachedTextureThumbnailsByAssetKey.end())
    {
        const CachedTextureThumbnail& Existing = ExistingIt->second;
        const std::filesystem::path ThumbnailPath = m_thumbnailDirectory / Existing.ThumbnailFileName;
        std::error_code ExistsError{};
        if (Existing.AssetId == Asset.AssetId &&
            Existing.SourcePath == SourcePath.string() &&
            Existing.SourceTimestamp == Fingerprint->Timestamp &&
            Existing.SourceSize == Fingerprint->Size &&
            !Existing.ThumbnailFileName.empty() &&
            std::filesystem::exists(ThumbnailPath, ExistsError) &&
            !ExistsError)
        {
            Metadata.IconSource = ThumbnailPath.generic_string();
            Metadata.TextureWidth = Existing.ThumbnailWidth;
            Metadata.TextureHeight = Existing.ThumbnailHeight;
            return Metadata;
        }

        RemoveCachedThumbnail(Asset.Key);
    }

    (void)TypeAutoRegistry::Instance().Ensure(StaticTypeId<TextureAsset>());
    TextureAsset Texture{};
    const Result LoadResult = LoadAuthoredAssetFromPath(StaticTypeId<TextureAsset>(), SourcePath, &Texture);
    if (!LoadResult)
    {
        return Metadata;
    }

    std::vector<std::uint8_t> RgbaPixels{};
    if (!ConvertTextureImageToRgba(Texture, RgbaPixels))
    {
        return Metadata;
    }

    const auto [ThumbnailWidth, ThumbnailHeight] = ResolveThumbnailExtent(Texture.Image.Width, Texture.Image.Height);
    if (ThumbnailWidth == 0u || ThumbnailHeight == 0u)
    {
        return Metadata;
    }

    std::vector<std::uint8_t> ThumbnailPixels = ScaleRgbaNearest(
        RgbaPixels,
        Texture.Image.Width,
        Texture.Image.Height,
        ThumbnailWidth,
        ThumbnailHeight);
    if (ThumbnailPixels.empty())
    {
        return Metadata;
    }

    std::ostringstream FileName{};
    FileName << Asset.AssetId.ToString()
             << '_'
             << Fingerprint->Timestamp
             << '_'
             << Fingerprint->Size
             << '.'
             << "png";
    const std::string ThumbnailFileName = FileName.str();
    const std::filesystem::path ThumbnailPath = m_thumbnailDirectory / ThumbnailFileName;
    if (!WritePngThumbnail(ThumbnailPath, ThumbnailWidth, ThumbnailHeight, ThumbnailPixels))
    {
        return Metadata;
    }

    CachedTextureThumbnail Thumbnail{};
    Thumbnail.AssetId = Asset.AssetId;
    Thumbnail.SourcePath = SourcePath.string();
    Thumbnail.SourceTimestamp = Fingerprint->Timestamp;
    Thumbnail.SourceSize = Fingerprint->Size;
    Thumbnail.ThumbnailFileName = ThumbnailFileName;
    Thumbnail.ThumbnailWidth = ThumbnailWidth;
    Thumbnail.ThumbnailHeight = ThumbnailHeight;
    m_cachedTextureThumbnailsByAssetKey[Asset.Key] = std::move(Thumbnail);
    m_metadataDirty = true;
    SaveThumbnailCacheIndex();
    ++m_revision;

    Metadata.IconSource = ThumbnailPath.generic_string();
    Metadata.TextureWidth = ThumbnailWidth;
    Metadata.TextureHeight = ThumbnailHeight;
    return Metadata;
}

EditorAssetIconService::AssetIconMetadata EditorAssetIconService::BuildFallbackIcon(
    const EditorAssetService::DiscoveredAsset& Asset) const
{
    AssetIconMetadata Metadata{};
    if (Asset.AssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture)
    {
        Metadata.IconSource = "editor://Assets/sphere.svg";
    }
    else if (Asset.AssetKind == AssetKindMaterial())
    {
        Metadata.IconSource = "editor://Assets/component.svg";
    }
    else if (Asset.AssetKind == AssetKindMaterialInstance())
    {
        Metadata.IconSource = "editor://Assets/box.svg";
    }
    else if (Asset.AssetKind == AssetKindStaticMesh() ||
             Asset.AssetKind == AssetKindSkeletalMesh())
    {
        Metadata.IconSource = "editor://Assets/box.svg";
    }
    else if (Asset.AssetKind == AssetKindLevel())
    {
        Metadata.IconSource = "editor://Assets/level.svg";
    }
    else if (Asset.AssetKind == AssetKindWorld())
    {
        Metadata.IconSource = "editor://Assets/world.svg";
    }
    else
    {
        Metadata.IconSource = "editor://Assets/component.svg";
    }
    return Metadata;
}

} // namespace SnAPI::GameFramework::Editor
