#pragma once

#include "Editor/EditorExport.h"
#include "Editor/EditorLayout.h"
#include "Editor/IEditorService.h"
#include "Handles.h"
#include "TypeRegistry.h"
#include "Uuid.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace SnAPI::GameFramework
{
class UIRenderViewport;
}

namespace SnAPI::GameFramework::Editor
{

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorLayoutService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;
    [[nodiscard]] UIRenderViewport* GameViewportElement() const;
    [[nodiscard]] int32_t GameViewportTabIndex() const;
    [[nodiscard]] EditorLayout::EGizmoSpace GizmoSpace() const;
    [[nodiscard]] bool GizmoSnappingEnabled() const;
    [[nodiscard]] double MoveSnapStep() const;
    [[nodiscard]] double RotateSnapStepDegrees() const;
    [[nodiscard]] double ScaleSnapStep() const;

private:
    void ApplyAssetBrowserState(EditorServiceContext& Context);
    void QueueLayoutRebuild() { m_layoutRebuildRequested = true; }
    void RebuildLayout(EditorServiceContext& Context);

    EditorLayout m_layout{};
    bool m_hasPendingSelectionRequest = false;
    NodeHandle m_pendingSelectionRequest{};
    bool m_hasPendingHierarchyActionRequest = false;
    EditorLayout::HierarchyActionRequest m_pendingHierarchyActionRequest{};
    bool m_hasPendingToolbarAction = false;
    EditorLayout::EToolbarAction m_pendingToolbarAction = EditorLayout::EToolbarAction::Play;
    bool m_hasPendingProjectActionRequest = false;
    EditorLayout::ProjectActionRequest m_pendingProjectActionRequest{};
    bool m_hasPendingAssetSelection = false;
    bool m_pendingAssetSelectionDoubleClick = false;
    std::string m_pendingAssetSelectionKey{};
    bool m_hasPendingAssetPlaceRequest = false;
    std::string m_pendingAssetPlaceKey{};
    bool m_hasPendingAssetSaveRequest = false;
    std::string m_pendingAssetSaveKey{};
    bool m_hasPendingAssetDeleteRequest = false;
    std::string m_pendingAssetDeleteKey{};
    bool m_hasPendingAssetRenameRequest = false;
    std::string m_pendingAssetRenameKey{};
    std::string m_pendingAssetRenameValue{};
    bool m_hasPendingAssetRefreshRequest = false;
    bool m_hasPendingAssetCreateRequest = false;
    EditorLayout::ContentAssetCreateRequest m_pendingAssetCreateRequest{};
    bool m_hasPendingAssetImportRequest = false;
    EditorLayout::ContentAssetImportRequest m_pendingAssetImportRequest{};
    bool m_hasPendingAssetInspectorSaveRequest = false;
    bool m_hasPendingAssetInspectorReimportRequest = false;
    bool m_hasPendingAssetInspectorCloseRequest = false;
    bool m_hasPendingAssetInspectorNodeSelectionRequest = false;
    NodeHandle m_pendingAssetInspectorNodeSelection{};
    bool m_hasPendingAssetInspectorHierarchyActionRequest = false;
    EditorLayout::HierarchyActionRequest m_pendingAssetInspectorHierarchyActionRequest{};
    bool m_hasPendingConduitVariableSelectionRequest = false;
    Uuid m_pendingConduitVariableSelection{};
    bool m_hasPendingConduitVariableCreateRequest = false;
    std::string m_pendingConduitVariableCreateName{};
    TypeId m_pendingConduitVariableCreateType{};
    bool m_hasPendingConduitVariableRemoveRequest = false;
    bool m_hasPendingConduitVariableRenameRequest = false;
    std::string m_pendingConduitVariableRenameValue{};
    bool m_hasPendingConduitVariableTypeRequest = false;
    TypeId m_pendingConduitVariableType{};
    bool m_hasPendingConduitVariableDefaultBoolRequest = false;
    bool m_pendingConduitVariableDefaultBool = false;
    bool m_hasPendingConduitVariableDefaultTextRequest = false;
    std::string m_pendingConduitVariableDefaultText{};
    bool m_hasPendingConduitVariableDefaultEnumRequest = false;
    std::string m_pendingConduitVariableDefaultEnum{};
    bool m_hasPendingConduitVariableClearDefaultRequest = false;
    bool m_hasPendingConduitVariableCommitDefaultRequest = false;
    bool m_hasPendingConduitVariableResetDefaultRequest = false;
    bool m_hasPendingConduitNodeSelectionRequest = false;
    Uuid m_pendingConduitNodeSelection{};
    bool m_hasPendingConduitNodeCreateRequest = false;
    std::string m_pendingConduitNodeCreateStableId{};
    bool m_hasPendingConduitNodeRemoveRequest = false;
    bool m_hasPendingConduitNodeMoveRequest = false;
    Uuid m_pendingConduitNodeMoveId{};
    float m_pendingConduitNodeMoveX = 0.0f;
    float m_pendingConduitNodeMoveY = 0.0f;
    bool m_hasPendingConduitNodePrimaryTextRequest = false;
    std::string m_pendingConduitNodePrimaryText{};
    bool m_hasPendingConduitNodeSecondaryTextRequest = false;
    std::string m_pendingConduitNodeSecondaryText{};
    bool m_hasPendingConduitViewportRequest = false;
    float m_pendingConduitViewportPanX = 0.0f;
    float m_pendingConduitViewportPanY = 0.0f;
    float m_pendingConduitViewportZoom = 1.0f;
    bool m_hasPendingConduitClassNameRequest = false;
    std::string m_pendingConduitClassName{};
    bool m_hasPendingConduitClassHostTypeRequest = false;
    TypeId m_pendingConduitClassHostType{};
    bool m_hasPendingConduitClassGraphRequest = false;
    std::string m_pendingConduitClassGraph{};
    bool m_layoutRebuildRequested = false;
    std::size_t m_assetListSignature = 0;
    std::size_t m_assetDetailsSignature = 0;
    std::uint64_t m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t m_assetInspectorIconRevision = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t m_conduitWorkspaceRevision = std::numeric_limits<std::uint64_t>::max();
};

} // namespace SnAPI::GameFramework::Editor
