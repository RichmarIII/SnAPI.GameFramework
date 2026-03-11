#pragma once

#include "Editor/EditorExport.h"
#include "Editor/EditorAssetService.h"
#include "Editor/IEditorService.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace SnAPI::UI
{
class UIContext;
}

namespace SnAPI::GameFramework::Editor
{

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
    struct TextureBinding;

    [[nodiscard]] AssetIconMetadata BuildFallbackIcon(const EditorAssetService::DiscoveredAsset& Asset) const;
    [[nodiscard]] std::uint32_t AllocateTextureId();
    void RemoveBinding(EditorServiceContext& Context, std::string_view AssetKey);
    void ResetAllBindings(EditorServiceContext& Context);

    const SnAPI::UI::UIContext* m_boundContext = nullptr;
    std::unordered_map<std::string, std::shared_ptr<TextureBinding>> m_textureBindingsByAssetKey{};
    std::uint32_t m_nextTextureId = 0x70000000u;
    std::uint64_t m_revision = 1;
};

} // namespace SnAPI::GameFramework::Editor
