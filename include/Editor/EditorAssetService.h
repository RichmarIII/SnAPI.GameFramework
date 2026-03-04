#pragma once

#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"
#include "Editor/EditorImportSettings.h"

#include "Handles.h"
#include "TypeRegistration.h"
#include "AssetManager.h"
#include "RenderAssetPayloads.h"
#include <TextureCompressorPayloads.h>

#include <filesystem>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SnAPI::GameFramework
{
class BaseNode;
class World;
}

namespace SnAPI::GameFramework::Editor
{

enum class EAssetImportProfile : std::uint8_t
{
    Unknown = 0,
    AssimpModel,
    Texture,
};

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

    struct AssetEditorSessionView
    {
        struct NodeEntry
        {
            NodeHandle Handle{};
            int Depth = 0;
            std::string Label{};
        };

        bool IsOpen = false;
        std::string AssetKey{};
        std::string Title{};
        TypeId TargetType{};
        void* TargetObject = nullptr;
        TypeId ImportSettingsType{};
        void* ImportSettingsObject = nullptr;
        std::vector<NodeEntry> Nodes{};
        NodeHandle SelectedNode{};
        bool CanEditHierarchy = false;
        bool HasImportSettings = false;
        bool RuntimeDirty = false;
        bool ImportSettingsDirty = false;
        bool IsDirty = false;
        bool CanSave = false;
        bool CanReimport = false;
        bool HasTexturePreviewStats = false;
        std::uint32_t TexturePreviewWidth = 0;
        std::uint32_t TexturePreviewHeight = 0;
        std::uint32_t TexturePreviewMipCount = 0;
        std::string TexturePreviewTarget{};
        std::string TexturePreviewFormat{};
        std::uint64_t TexturePreviewGpuSizeBytes = 0;
    };

    struct ProjectInfo
    {
        bool IsLoaded = false;
        std::string Name{};
        std::string ProjectFilePath{};
        std::string ProjectRootDirectory{};
        std::string AssetRootDirectory{};
        std::string StartupLevelPack{};
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
    Result CreateRuntimeNodeAssetByType(EditorServiceContext& Context,
                                        const TypeId& NodeType,
                                        std::string_view AssetName,
                                        std::string_view FolderPath);
    Result CreateRuntimeMaterialAsset(EditorServiceContext& Context,
                                      std::string_view AssetName,
                                      std::string_view FolderPath);
    Result CreateRuntimeMaterialInstanceAsset(EditorServiceContext& Context,
                                              std::string_view AssetName,
                                              std::string_view FolderPath);
    Result ImportSourceAsset(EditorServiceContext& Context,
                             std::string_view SourcePath,
                             std::string_view DestinationFolderPath,
                             const std::unordered_map<std::string, std::string>& BuildOptions,
                             ::SnAPI::AssetPipeline::AssetImportSettingsPtr ImportSettings = {});
    Result OpenAssetEditorByKey(std::string_view Key);
    void CloseAssetEditor();
    Result SelectAssetEditorNode(const NodeHandle& Node);
    Result AddAssetEditorNode(const NodeHandle& Parent, const TypeId& NodeType);
    Result DeleteAssetEditorNode(const NodeHandle& Node);
    Result AddAssetEditorComponent(const NodeHandle& Owner, const TypeId& ComponentType);
    Result RemoveAssetEditorComponent(const NodeHandle& Owner, const TypeId& ComponentType);
    void TickAssetEditorSession(float DeltaSeconds = 0.0f);
    Result SaveActiveAssetEditor();
    Result ReimportActiveAsset(EditorServiceContext& Context);
    [[nodiscard]] AssetEditorSessionView AssetEditorSession() const;
    [[nodiscard]] std::uint64_t AssetEditorSessionRevision() const { return m_assetEditorSessionRevision; }

    Result InstantiateArmedAsset(EditorServiceContext& Context);
    Result InstantiateAssetByKey(EditorServiceContext& Context, std::string_view Key);
    Result CreateProject(EditorServiceContext& Context, std::string_view ProjectName, std::string_view ParentDirectory);
    Result LoadProject(EditorServiceContext& Context, std::string_view ProjectFilePath);
    [[nodiscard]] const ProjectInfo& CurrentProject() const { return m_currentProject; }

    [[nodiscard]] const std::string& PreviewSummary() const { return m_previewSummary; }
    [[nodiscard]] const std::string& StatusMessage() const { return m_statusMessage; }

private:
    [[nodiscard]] std::vector<std::string> BuildPackSearchPaths() const;
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
    Result RebuildAssetManager();
    Result EnsureEditorTemplateAssets(EditorServiceContext& Context);
    Result EnsureProjectShaderDirectory(const std::filesystem::path& ProjectAssetRoot);
    Result EnsureProjectStarterLevelPack(const std::filesystem::path& ProjectAssetRoot,
                                         const std::filesystem::path& StartupPackPath);
    Result LoadProjectStartupLevel(EditorServiceContext& Context, const std::filesystem::path& StartupPackPath);
    [[nodiscard]] std::expected<::SnAPI::AssetPipeline::TypedPayload, std::string> SerializeAssetEditorPayload() const;
    Result SyncMaterialInstanceEditorPayloadFromDescriptor();
    struct AssetImportMetadataEntry
    {
        EAssetImportProfile Profile = EAssetImportProfile::Unknown;
        std::string SourcePath{};
        std::string DestinationFolder{};
        std::string ImporterName{};
        std::unordered_map<std::string, std::string> BuildOptions{};
        Editor::AssimpImportSettings Assimp{};
        Editor::TextureImportSettings Texture{};
    };
    [[nodiscard]] std::filesystem::path ResolveImportMetadataPath() const;
    [[nodiscard]] std::expected<void, std::string> LoadAssetImportMetadataDatabase();
    [[nodiscard]] std::expected<void, std::string> SaveAssetImportMetadataDatabase() const;
    [[nodiscard]] bool RefreshAssetEditorImportSettingsBinding(const DiscoveredAsset& Asset);
    [[nodiscard]] std::optional<AssetImportMetadataEntry> BuildAssetEditorImportMetadataFromCurrentState() const;
    [[nodiscard]] bool ImportMetadataRecordsEqual(const AssetImportMetadataEntry& Left, const AssetImportMetadataEntry& Right) const;
    [[nodiscard]] ::SnAPI::AssetPipeline::AssetImportSettingsPtr BuildTypedImportSettingsForRecord(
        const AssetImportMetadataEntry& Record) const;
    void ClearAssetEditorImportSettingsBinding();
    [[nodiscard]] BaseNode* ResolveAssetEditorNode(const NodeHandle& Node) const;
    void RefreshAssetEditorHierarchy();
    void ClearAssetEditorState();

    std::unique_ptr<::SnAPI::AssetPipeline::AssetManager> m_assetManager{};
    std::vector<DiscoveredAsset> m_assets{};
    std::unordered_map<std::string, std::size_t> m_assetIndexByKey{};
    std::unordered_map<::SnAPI::AssetPipeline::AssetId, std::string, ::SnAPI::AssetPipeline::UuidHash> m_assetRenameOverrides{};
    std::unordered_map<::SnAPI::AssetPipeline::AssetId, ::SnAPI::AssetPipeline::TypedPayload, ::SnAPI::AssetPipeline::UuidHash> m_assetPayloadOverrides{};
    std::string m_selectedAssetKey{};
    std::string m_placementAssetKey{};
    std::string m_previewSummary{};
    std::string m_statusMessage{};
    std::filesystem::path m_editorTemplateAssetDirectory{};
    std::filesystem::path m_editorStarterLevelTemplatePackPath{};
    std::filesystem::path m_editorStarterScriptTemplatePath{};
    ProjectInfo m_currentProject{};

    std::unique_ptr<::SnAPI::GameFramework::World> m_assetEditorWorld{};
    NodeHandle m_assetEditorRootHandle{};
    std::string m_assetEditorAssetKey{};
    ::SnAPI::AssetPipeline::AssetId m_assetEditorAssetId{};
    ::SnAPI::AssetPipeline::TypeId m_assetEditorAssetKind{};
    TypeId m_assetEditorTargetType{};
    void* m_assetEditorTargetObject = nullptr;
    bool m_assetEditorDirty = false;
    bool m_assetEditorCanSave = false;
    bool m_assetEditorCanEditHierarchy = false;
    std::optional<MaterialPayload> m_assetEditorMaterialPayload{};
    std::optional<MaterialInstancePayload> m_assetEditorMaterialInstancePayload{};
    std::optional<TextureCompressorPlugin::TextureCompressorCookedInfo> m_assetEditorTextureCookedInfo{};
    std::optional<Editor::TextureAssetEditorPayload> m_assetEditorTexturePayload{};
    std::optional<StaticMeshPayload> m_assetEditorStaticMeshPayload{};
    std::optional<Editor::StaticMeshAssetEditorPayload> m_assetEditorStaticMeshEditorPayload{};
    std::optional<Editor::AssimpImportSettings> m_assetEditorAssimpImportSettings{};
    std::optional<Editor::TextureImportSettings> m_assetEditorTextureImportSettings{};
    TypeId m_assetEditorImportSettingsType{};
    void* m_assetEditorImportSettingsObject = nullptr;
    bool m_assetEditorImportSettingsDirty = false;
    bool m_assetEditorCanReimport = false;
    std::optional<AssetImportMetadataEntry> m_assetEditorImportMetadataBaseline{};
    std::string m_assetEditorMaterialInstanceDescriptorParentKey{};
    std::vector<uint8_t> m_assetEditorBaselineCookedBytes{};
    std::string m_assetEditorTitle{};
    NodeHandle m_assetEditorSelectedNode{};
    std::vector<AssetEditorSessionView::NodeEntry> m_assetEditorHierarchy{};
    bool m_assetEditorHierarchyDirty = false;
    float m_assetEditorDirtyCheckCooldownSeconds = 0.0f;
    std::uint64_t m_assetEditorSessionRevision = 0;
    std::unordered_map<::SnAPI::AssetPipeline::AssetId, AssetImportMetadataEntry, ::SnAPI::AssetPipeline::UuidHash> m_assetImportMetadata{};
    std::filesystem::path m_assetImportMetadataPath{};
    bool m_assetImportMetadataDirty = false;
};

} // namespace SnAPI::GameFramework::Editor
