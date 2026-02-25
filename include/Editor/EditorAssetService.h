#pragma once

#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"

#include "Handles.h"
#include "AssetManager.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SnAPI::GameFramework::Editor
{

/**
 * @brief Asset-discovery and asset-instantiation backend for the editor.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorAssetService final : public IEditorService
{
public:
    struct DiscoveredAsset
    {
        std::string Key{};
        std::string Name{};
        std::string TypeLabel{};
        std::string Variant{};
        ::SnAPI::AssetPipeline::AssetId AssetId{};
        ::SnAPI::AssetPipeline::TypeId AssetKind{};
        ::SnAPI::AssetPipeline::TypeId CookedPayloadType{};
        uint32_t SchemaVersion = 0;
        bool IsRuntime = false;
        bool IsDirty = false;
        bool CanSave = true;
        std::string OwningPackPath{};
    };

    [[nodiscard]] std::string_view Name() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Shutdown(EditorServiceContext& Context) override;

    [[nodiscard]] const std::vector<DiscoveredAsset>& Assets() const { return m_assets; }
    [[nodiscard]] const DiscoveredAsset* SelectedAsset() const;
    [[nodiscard]] bool IsPlacementArmed() const { return !m_placementAssetKey.empty(); }
    [[nodiscard]] const std::string& PlacementAssetKey() const { return m_placementAssetKey; }

    bool SelectAssetByKey(std::string_view Key);
    Result ArmPlacementByKey(std::string_view Key);
    void ClearPlacement();

    Result RefreshDiscovery();
    Result OpenSelectedAssetPreview();
    Result SaveSelectedAssetUpdate();
    Result SaveAssetByKey(std::string_view Key);
    Result DeleteAssetByKey(std::string_view Key);
    Result DeleteSelectedAsset();
    Result RenameAssetByKey(std::string_view Key, std::string_view NewName);
    Result RenameSelectedAsset(std::string_view NewName);
    Result CreateRuntimePrefabFromNode(EditorServiceContext& Context, const NodeHandle& SourceHandle);

    Result InstantiateArmedAsset(EditorServiceContext& Context);
    Result InstantiateAssetByKey(EditorServiceContext& Context, std::string_view Key);

    [[nodiscard]] const std::string& PreviewSummary() const { return m_previewSummary; }
    [[nodiscard]] const std::string& StatusMessage() const { return m_statusMessage; }

private:
    [[nodiscard]] static std::vector<std::string> BuildPackSearchPaths();
    [[nodiscard]] static std::vector<std::string> ParsePackSearchPathEnv(std::string_view Raw);
    [[nodiscard]] static std::string AssetKindToLabel(const ::SnAPI::AssetPipeline::TypeId& AssetKind);
    [[nodiscard]] const DiscoveredAsset* FindAssetByKey(std::string_view Key) const;
    [[nodiscard]] std::expected<std::string, std::string> ResolveOwningPackPath(
        const DiscoveredAsset& Asset) const;
    [[nodiscard]] std::expected<std::string, std::string> ResolveRuntimeSavePath(
        const DiscoveredAsset& Asset) const;
    [[nodiscard]] std::expected<::SnAPI::AssetPipeline::TypedPayload, std::string> BuildCookedPayloadForAsset(
        const DiscoveredAsset& Asset);
    Result InstantiateNodeAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset);
    Result InstantiateLevelAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset);
    Result InstantiateWorldAsset(EditorServiceContext& Context, const DiscoveredAsset& Asset);

    std::unique_ptr<::SnAPI::AssetPipeline::AssetManager> m_assetManager{};
    std::vector<DiscoveredAsset> m_assets{};
    std::unordered_map<std::string, std::size_t> m_assetIndexByKey{};
    std::unordered_map<::SnAPI::AssetPipeline::AssetId, std::string, ::SnAPI::AssetPipeline::UuidHash> m_assetRenameOverrides{};
    std::string m_selectedAssetKey{};
    std::string m_placementAssetKey{};
    std::string m_previewSummary{};
    std::string m_statusMessage{};
};

} // namespace SnAPI::GameFramework::Editor
