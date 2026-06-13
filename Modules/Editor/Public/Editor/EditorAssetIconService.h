#pragma once

#include "Editor/EditorExport.h"
#include "Editor/EditorAssetService.h"
#include "Editor/IEditorService.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace SnAPI::UI
{
class UIContext;
}

namespace SnAPI::GameFramework::Editor
{

struct CachedTextureThumbnail
{
    ::SnAPI::AssetPipeline::AssetId AssetId{};
    std::string SourcePath{};
    std::uint64_t SourceTimestamp = 0;
    std::uintmax_t SourceSize = 0;
    std::string ThumbnailFileName{};
    std::uint32_t ThumbnailWidth = 0;
    std::uint32_t ThumbnailHeight = 0;
};

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorAssetIconService final : public IEditorService
{
public:
    ~EditorAssetIconService() override;

    struct AssetIconMetadata
    {
        std::string IconSource{};
        std::uint32_t TextureId = 0;
        std::uint32_t TextureWidth = 0;
        std::uint32_t TextureHeight = 0;
    };

    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Shutdown(EditorServiceContext& Context) override;

    void Synchronize(EditorServiceContext& Context,
                     const std::vector<EditorAssetService::DiscoveredAsset>& Assets,
                     const SnAPI::UI::UIContext* UiContext);
    void InvalidateAsset(EditorServiceContext& Context, std::string_view AssetKey);
    [[nodiscard]] AssetIconMetadata ResolveAssetIcon(EditorServiceContext& Context,
                                                     const EditorAssetService::DiscoveredAsset& Asset,
                                                     const SnAPI::UI::UIContext* UiContext);

    [[nodiscard]] std::uint64_t Revision() const { return m_revision; }

private:
    [[nodiscard]] AssetIconMetadata BuildFallbackIcon(const EditorAssetService::DiscoveredAsset& Asset) const;
    void EnsureProjectCacheLoaded(EditorServiceContext& Context);
    void LoadThumbnailCacheIndex();
    void SaveThumbnailCacheIndex();
    void RemoveCachedThumbnail(std::string_view AssetKey);
    void PruneMissingTextureThumbnails(const std::vector<EditorAssetService::DiscoveredAsset>& Assets);
    [[nodiscard]] AssetIconMetadata ResolveTextureThumbnail(const EditorAssetService::DiscoveredAsset& Asset);

    std::string m_loadedProjectKey{};
    std::filesystem::path m_cacheRoot{};
    std::filesystem::path m_metadataPath{};
    std::filesystem::path m_thumbnailDirectory{};
    std::unordered_map<std::string, CachedTextureThumbnail> m_cachedTextureThumbnailsByAssetKey{};
    bool m_metadataDirty = false;
    std::uint64_t m_revision = 1;
};

} // namespace SnAPI::GameFramework::Editor
