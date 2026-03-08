# SnAPI::GameFramework::Editor::EditorLayout

Builds and owns the editor shell widget tree inside the root UI context.

`EditorLayout` is the concrete UI composition object for the editor shell. It creates the menu bar, toolbar, hierarchy, game viewport tabs, inspector panes, content browser, and project/asset modal overlays inside the root `UIContext` exposed by the running `GameRuntime`.

Core semantics:
- `Build()` tears down any previous shell, registers required external elements, and rebuilds the entire widget tree for the current runtime and theme.
- `Sync()` is the steady-state update path. It refreshes the hierarchy, current inspector target, invalidation overlay state, and game viewport camera binding without rebuilding the shell.
- The class stores UI callbacks as delegates. Higher-level services provide those delegates and are responsible for translating them into editor actions.
- Content-browser and inspector payload setters copy lightweight view-model data but may contain borrowed raw object pointers for property panels; those pointers must remain valid until replaced by a subsequent state push or until the layout is shut down.

Ownership and lifetime:
- `EditorLayout` value-owns its UI element handles and modal/view-model state.
- Returned pointers such as `GameViewport()` and `Context()` are non-owning and become invalid when the layout is shut down or rebuilt.

Threading model:
- Main-thread only.

## Contents

- **Type:** SnAPI::GameFramework::Editor::EditorLayout::ContentAssetEntry
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::ContentAssetDetails
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::ContentAssetCreateRequest
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::ContentAssetImportRequest
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::ContentAssetInspectorState
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::HierarchyActionRequest
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::ProjectActionRequest
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::ProjectState
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::HierarchyEntry
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::ContentAssetCardWidgets
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::ContentBrowserEntry
- **Type:** SnAPI::GameFramework::Editor::EditorLayout::RecentProjectEntry

## Public Types

<div class="snapi-api-card" markdown="1">
### `enum EHierarchyAction`

Hierarchy action kinds emitted by hierarchy context menus.

**Values**

- `AddNodeType`: Create a new child node of the chosen reflected type.
- `AddComponentType`: Attach a component of the chosen reflected type to the target node.
- `RemoveComponentType`: Remove the specified component type from the target node.
- `DeleteNode`: Delete the target node.
- `CreatePrefab`: Create a prefab asset from the target node subtree.
</div>
<div class="snapi-api-card" markdown="1">
### `enum EToolbarAction`

Toolbar actions emitted by the editor shell.

**Values**

- `Play`: Start or resume Play-In-Editor.
- `Pause`: Pause the active PIE session.
- `Stop`: Stop PIE and restore the editor world snapshot.
- `JoinLocalPlayer2`: Request that a second local player join the current session.
</div>
<div class="snapi-api-card" markdown="1">
### `enum EGizmoSpace`

Transform-gizmo space selection exposed by the tools pane.

**Values**

- `World`: Interpret gizmo axes in world space.
- `Object`: Interpret gizmo axes in the selected object's local basis.
- `Camera`: Interpret gizmo axes relative to the active editor camera.
</div>
<div class="snapi-api-card" markdown="1">
### `enum ESnapMode`

Binary snapping toggle used by the tools pane.

**Values**

- `Off`: Do not quantize transform interaction deltas.
- `On`: Quantize transform interaction deltas using the configured snap steps.
</div>
<div class="snapi-api-card" markdown="1">
### `enum EProjectAction`

Project-management actions emitted by project modals.

**Values**

- `CreateNew`: Create a new project from the values entered in the create-project flow.
- `OpenExisting`: Open an existing project file selected by the user.
- `SaveSettings`: Persist project settings edits for the currently loaded project.
</div>

## Private Types

<div class="snapi-api-card" markdown="1">
### `enum EHierarchyEntryKind`

**Values**

- `World`
- `Level`
- `Node`
</div>
<div class="snapi-api-card" markdown="1">
### `enum EImportProfile`

**Values**

- `Unknown`
- `AssimpModel`
- `Texture`
</div>
<div class="snapi-api-card" markdown="1">
### `enum EContextMenuScope`

**Values**

- `None`
- `MenuBar`
- `HierarchyItem`
- `InspectorComponent`
- `ContentAssetItem`
- `ContentBrowser`
- `ContentInspectorHierarchyItem`
- `ContentInspectorComponent`
</div>
<div class="snapi-api-card" markdown="1">
### `enum EPendingHierarchyMenu`

**Values**

- `None`
- `Root`
- `AddNodeTypes`
- `AddComponentTypes`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::Editor::EditorLayout::PanelBuilder = SnAPI::UI::TElementBuilder<SnAPI::UI::UIPanel>`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::UIContext* SnAPI::GameFramework::Editor::EditorLayout::m_context`
</div>
<div class="snapi-api-card" markdown="1">
### `GameRuntime* SnAPI::GameFramework::Editor::EditorLayout::m_runtime`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIPanel> SnAPI::GameFramework::Editor::EditorLayout::m_shellRoot`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UITabs> SnAPI::GameFramework::Editor::EditorLayout::m_gameViewTabs`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<UIRenderViewport> SnAPI::GameFramework::Editor::EditorLayout::m_gameViewport`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<UIPropertyPanel> SnAPI::GameFramework::Editor::EditorLayout::m_inspectorPropertyPanel`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> SnAPI::GameFramework::Editor::EditorLayout::m_hierarchyTree`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIContextMenu> SnAPI::GameFramework::Editor::EditorLayout::m_contextMenu`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIBadge> SnAPI::GameFramework::Editor::EditorLayout::m_hierarchyCountBadge`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UISwitch> SnAPI::GameFramework::Editor::EditorLayout::m_invalidationDebugToggleSwitch`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_invalidationDebugToggleLabel`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> SnAPI::GameFramework::Editor::EditorLayout::m_contentSearchInput`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIBreadcrumbs> SnAPI::GameFramework::Editor::EditorLayout::m_contentPathBreadcrumbs`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetNameValue`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetTypeValue`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetVariantValue`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetIdValue`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetStatusValue`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> SnAPI::GameFramework::Editor::EditorLayout::m_contentPlaceButton`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> SnAPI::GameFramework::Editor::EditorLayout::m_contentSaveButton`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIListView> SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetsList`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetsEmptyHint`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateModalOverlay`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateTypeTree`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateSearchInput`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateNameInput`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateOkButton`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> SnAPI::GameFramework::Editor::EditorLayout::m_contentImportModalOverlay`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> SnAPI::GameFramework::Editor::EditorLayout::m_contentImportSourcePicker`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentImportSummaryText`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<UIPropertyPanel> SnAPI::GameFramework::Editor::EditorLayout::m_contentImportSettingsPanel`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> SnAPI::GameFramework::Editor::EditorLayout::m_contentImportOkButton`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorModalOverlay`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorTitleText`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorStatusText`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorHierarchyTitleText`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorPreviewStatsText`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIImage> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorPreviewImage`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UITreeView> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorHierarchyTree`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<UIPropertyPanel> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorPropertyPanel`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIText> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorImportSettingsTitleText`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<UIPropertyPanel> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorImportSettingsPanel`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorSaveButton`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorReimportButton`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> SnAPI::GameFramework::Editor::EditorLayout::m_menuFileButton`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> SnAPI::GameFramework::Editor::EditorLayout::m_projectModalOverlay`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> SnAPI::GameFramework::Editor::EditorLayout::m_projectNameInput`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> SnAPI::GameFramework::Editor::EditorLayout::m_projectDirectoryInput`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> SnAPI::GameFramework::Editor::EditorLayout::m_projectFilePathInput`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> SnAPI::GameFramework::Editor::EditorLayout::m_projectModalOkButton`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIModal> SnAPI::GameFramework::Editor::EditorLayout::m_projectSettingsModalOverlay`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UITextInput> SnAPI::GameFramework::Editor::EditorLayout::m_projectSettingsNameInput`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIFilesystemPicker> SnAPI::GameFramework::Editor::EditorLayout::m_projectSettingsStartupPackInput`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIComboBox> SnAPI::GameFramework::Editor::EditorLayout::m_projectSettingsDefaultRenderSettingsCombo`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> SnAPI::GameFramework::Editor::EditorLayout::m_projectSettingsSaveButton`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<ContentAssetCardWidgets> SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetCards`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<SnAPI::UI::ElementHandle<SnAPI::UI::UIButton> > SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetCardButtons`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::size_t> SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetCardIndices`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<ContentBrowserEntry> SnAPI::GameFramework::Editor::EditorLayout::m_contentBrowserEntries`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<ContentAssetEntry> SnAPI::GameFramework::Editor::EditorLayout::m_contentAssets`
</div>
<div class="snapi-api-card" markdown="1">
### `ContentAssetDetails SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetDetails`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetFilterText`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_contentCurrentFolder`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_selectedContentAssetKey`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_selectedContentFolderPath`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_lastContentAssetClickKey`
</div>
<div class="snapi-api-card" markdown="1">
### `std::chrono::steady_clock::time_point SnAPI::GameFramework::Editor::EditorLayout::m_lastContentAssetClickTime`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateModalOpen`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateTypeFilterText`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateNameText`
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateSelectedType`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TypeId> SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateVisibleTypes`
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<SnAPI::UI::ITreeItemSource> SnAPI::GameFramework::Editor::EditorLayout::m_contentCreateTypeSource`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::m_contentImportModalOpen`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_contentImportSourcePath`
</div>
<div class="snapi-api-card" markdown="1">
### `EImportProfile SnAPI::GameFramework::Editor::EditorLayout::m_contentImportProfile`
</div>
<div class="snapi-api-card" markdown="1">
### `AssimpImportSettings SnAPI::GameFramework::Editor::EditorLayout::m_contentImportAssimpSettings`
</div>
<div class="snapi-api-card" markdown="1">
### `TextureImportSettings SnAPI::GameFramework::Editor::EditorLayout::m_contentImportTextureSettings`
</div>
<div class="snapi-api-card" markdown="1">
### `ContentAssetInspectorState SnAPI::GameFramework::Editor::EditorLayout::m_contentAssetInspectorState`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::m_projectModalOpen`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::m_projectModalRequired`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::m_projectModalShowWelcome`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::m_projectSettingsModalOpen`
</div>
<div class="snapi-api-card" markdown="1">
### `EProjectAction SnAPI::GameFramework::Editor::EditorLayout::m_projectModalAction`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_projectNameText`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_projectDirectoryText`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_projectFilePathText`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_projectSettingsNameText`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_projectSettingsStartupPackText`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_projectSettingsDefaultRenderSettingsAssetId`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::pair<std::string, std::string> > SnAPI::GameFramework::Editor::EditorLayout::m_projectSettingsRenderSettingsOptions`
</div>
<div class="snapi-api-card" markdown="1">
### `ProjectState SnAPI::GameFramework::Editor::EditorLayout::m_projectState`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<RecentProjectEntry> SnAPI::GameFramework::Editor::EditorLayout::m_recentProjects`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeHandle> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorVisibleNodes`
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<SnAPI::UI::ITreeItemSource> SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorHierarchySource`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorTargetBound`
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorBoundObject`
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorBoundType`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorImportTargetBound`
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorImportBoundObject`
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorLayout::m_contentInspectorImportBoundType`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const std::string&, bool)> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetSelected`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const std::string&)> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetPlaceRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const std::string&)> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetSaveRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const std::string&)> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetDeleteRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const std::string&, const std::string&)> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetRenameRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void()> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetRefreshRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const ContentAssetCreateRequest&)> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetCreateRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const ContentAssetImportRequest&)> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetImportRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void()> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetInspectorSaveRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void()> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetInspectorReimportRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void()> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetInspectorCloseRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const NodeHandle&)> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetInspectorNodeSelected`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> SnAPI::GameFramework::Editor::EditorLayout::m_onContentAssetInspectorHierarchyActionRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `std::shared_ptr<SnAPI::UI::ITreeItemSource> SnAPI::GameFramework::Editor::EditorLayout::m_hierarchyItemSource`
</div>
<div class="snapi-api-card" markdown="1">
### `EContextMenuScope SnAPI::GameFramework::Editor::EditorLayout::m_contextMenuScope`
</div>
<div class="snapi-api-card" markdown="1">
### `EPendingHierarchyMenu SnAPI::GameFramework::Editor::EditorLayout::m_pendingHierarchyMenu`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<std::size_t> SnAPI::GameFramework::Editor::EditorLayout::m_pendingHierarchyMenuIndex`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::UIPoint SnAPI::GameFramework::Editor::EditorLayout::m_pendingHierarchyMenuOpenPosition`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<std::size_t> SnAPI::GameFramework::Editor::EditorLayout::m_contextMenuHierarchyIndex`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<std::size_t> SnAPI::GameFramework::Editor::EditorLayout::m_contextMenuAssetIndex`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorLayout::m_contextMenuContentInspectorNode`
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional<NodeHandle> SnAPI::GameFramework::Editor::EditorLayout::m_contextMenuComponentOwner`
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorLayout::m_contextMenuComponentType`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TypeId> SnAPI::GameFramework::Editor::EditorLayout::m_contextMenuNodeTypes`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<TypeId> SnAPI::GameFramework::Editor::EditorLayout::m_contextMenuComponentTypes`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::UIPoint SnAPI::GameFramework::Editor::EditorLayout::m_contextMenuOpenPosition`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<NodeHandle> SnAPI::GameFramework::Editor::EditorLayout::m_hierarchyVisibleNodes`
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorLayout::m_hierarchySignature`
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::Editor::EditorLayout::m_hierarchyNodeCount`
</div>
<div class="snapi-api-card" markdown="1">
### `NodeHandle SnAPI::GameFramework::Editor::EditorLayout::m_hierarchyVisualSelection`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::EditorLayout::m_hierarchyFilterText`
</div>
<div class="snapi-api-card" markdown="1">
### `EditorSelectionModel* SnAPI::GameFramework::Editor::EditorLayout::m_selection`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const NodeHandle&)> SnAPI::GameFramework::Editor::EditorLayout::m_onHierarchyNodeChosen`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const HierarchyActionRequest&)> SnAPI::GameFramework::Editor::EditorLayout::m_onHierarchyActionRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(EToolbarAction)> SnAPI::GameFramework::Editor::EditorLayout::m_onToolbarActionRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TDelegate<void(const ProjectActionRequest&)> SnAPI::GameFramework::Editor::EditorLayout::m_onProjectActionRequested`
</div>
<div class="snapi-api-card" markdown="1">
### `void* SnAPI::GameFramework::Editor::EditorLayout::m_boundInspectorObject`
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::Editor::EditorLayout::m_boundInspectorType`
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::Editor::EditorLayout::m_boundInspectorComponentSignature`
</div>
<div class="snapi-api-card" markdown="1">
### `EGizmoSpace SnAPI::GameFramework::Editor::EditorLayout::m_gizmoSpace`
</div>
<div class="snapi-api-card" markdown="1">
### `ESnapMode SnAPI::GameFramework::Editor::EditorLayout::m_snapMode`
</div>
<div class="snapi-api-card" markdown="1">
### `double SnAPI::GameFramework::Editor::EditorLayout::m_moveSnapStep`
</div>
<div class="snapi-api-card" markdown="1">
### `double SnAPI::GameFramework::Editor::EditorLayout::m_rotateSnapStepDegrees`
</div>
<div class="snapi-api-card" markdown="1">
### `double SnAPI::GameFramework::Editor::EditorLayout::m_scaleSnapStep`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::m_invalidationDebugOverlayEnabled`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::PropertyMap SnAPI::GameFramework::Editor::EditorLayout::m_viewModel`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::m_built`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::EditorLayout::Build(GameRuntime &Runtime, SnAPI::UI::Theme &Theme, CameraComponent *ActiveCamera, EditorSelectionModel *SelectionModel)`

Build or rebuild the full editor shell for the supplied runtime.

`Build()` first shuts down any previously built shell, then registers external elements such as `UIRenderViewport` and `UIPropertyPanel`, resolves the root UI context, and composes the full shell.

**Parameters**

- `Runtime`: Borrowed runtime that exposes the root UI context and world services.
- `Theme`: Borrowed theme applied to the shell.
- `ActiveCamera`: Non-owning active editor camera pointer used for initial hierarchy/inspector/game-view setup.
- `SelectionModel`: Borrowed selection model used for hierarchy and inspector binding.

**Returns:** Success or an error.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::Shutdown(GameRuntime *Runtime)`

Destroy the current shell and clear all element handles, modal state, and callback bindings.

**Parameters**

- `Runtime`: Borrowed runtime associated with the current layout, or `nullptr` when tearing down detached state.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::Sync(GameRuntime &Runtime, CameraComponent *ActiveCamera, EditorSelectionModel *SelectionModel, float DeltaSeconds)`

Synchronize the built shell with current runtime, selection, and camera state.

This is the steady-state update path and does not rebuild the shell.

**Parameters**

- `Runtime`: Borrowed runtime.
- `ActiveCamera`: Non-owning active editor camera pointer.
- `SelectionModel`: Borrowed selection model.
- `DeltaSeconds`: Frame delta time in seconds.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::IsBuilt() const`

Query whether the shell is currently built.
</div>
<div class="snapi-api-card" markdown="1">
### `UIRenderViewport * SnAPI::GameFramework::Editor::EditorLayout::GameViewport() const`

Access the embedded game-viewport UI element.

**Returns:** Non-owning pointer or `nullptr`.
</div>
<div class="snapi-api-card" markdown="1">
### `int32_t SnAPI::GameFramework::Editor::EditorLayout::GameViewportTabIndex() const`

Index of the active game-viewport tab within the game-view tab control, or a negative value if unavailable.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::UIContext * SnAPI::GameFramework::Editor::EditorLayout::Context() const`

Access the root UI context currently hosting the shell.

**Returns:** Non-owning pointer or `nullptr`.
</div>
<div class="snapi-api-card" markdown="1">
### `EGizmoSpace SnAPI::GameFramework::Editor::EditorLayout::GizmoSpace() const`

Current gizmo-space selection from the tools UI.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::GizmoSnappingEnabled() const`

Query whether transform snapping is currently enabled in the tools UI.
</div>
<div class="snapi-api-card" markdown="1">
### `double SnAPI::GameFramework::Editor::EditorLayout::MoveSnapStep() const`

Translation snap step in world units.
</div>
<div class="snapi-api-card" markdown="1">
### `double SnAPI::GameFramework::Editor::EditorLayout::RotateSnapStepDegrees() const`

Rotation snap increment in degrees.
</div>
<div class="snapi-api-card" markdown="1">
### `double SnAPI::GameFramework::Editor::EditorLayout::ScaleSnapStep() const`

Scale snap increment in scalar units.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetHierarchySelectionHandler(SnAPI::UI::TDelegate< void(const NodeHandle &)> Handler)`

Install the callback invoked when the user chooses a hierarchy node.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetHierarchyActionHandler(SnAPI::UI::TDelegate< void(const HierarchyActionRequest &)> Handler)`

Install the callback invoked when the user requests a hierarchy mutation.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetToolbarActionHandler(SnAPI::UI::TDelegate< void(EToolbarAction)> Handler)`

Install the callback invoked when the user presses a toolbar action.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetProjectActionHandler(SnAPI::UI::TDelegate< void(const ProjectActionRequest &)> Handler)`

Install the callback invoked for project create/open/save-settings flows.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetProjectState(ProjectState State)`

Replace the loaded-project view state shown across project-sensitive UI.

**Parameters**

- `State`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetProjectSelectionRequired(bool Required)`

Control whether the shell should force the user through project selection before normal editing.

**Parameters**

- `Required`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssets(std::vector< ContentAssetEntry > Assets)`

Replace the content-browser asset list.

**Parameters**

- `Assets`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetSelectionHandler(SnAPI::UI::TDelegate< void(const std::string &, bool)> Handler)`

Install the callback invoked when the user selects or double-clicks a content asset.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetPlaceHandler(SnAPI::UI::TDelegate< void(const std::string &)> Handler)`

Install the callback invoked when the user requests asset placement into the world.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetSaveHandler(SnAPI::UI::TDelegate< void(const std::string &)> Handler)`

Install the callback invoked when the user requests asset save.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetDeleteHandler(SnAPI::UI::TDelegate< void(const std::string &)> Handler)`

Install the callback invoked when the user requests asset deletion.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetRenameHandler(SnAPI::UI::TDelegate< void(const std::string &, const std::string &)> Handler)`

Install the callback invoked when the user confirms an asset rename.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetRefreshHandler(SnAPI::UI::TDelegate< void()> Handler)`

Install the callback invoked when the user requests a content refresh.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetCreateHandler(SnAPI::UI::TDelegate< void(const ContentAssetCreateRequest &)> Handler)`

Install the callback invoked when the create-asset modal is confirmed.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetImportHandler(SnAPI::UI::TDelegate< void(const ContentAssetImportRequest &)> Handler)`

Install the callback invoked when the import-asset modal is confirmed.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetInspectorSaveHandler(SnAPI::UI::TDelegate< void()> Handler)`

Install the callback invoked when the asset-inspector save action is chosen.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetInspectorReimportHandler(SnAPI::UI::TDelegate< void()> Handler)`

Install the callback invoked when the asset-inspector reimport action is chosen.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetInspectorCloseHandler(SnAPI::UI::TDelegate< void()> Handler)`

Install the callback invoked when the asset-inspector modal is closed by the user.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetInspectorNodeSelectionHandler(SnAPI::UI::TDelegate< void(const NodeHandle &)> Handler)`

Install the callback invoked when the user selects a node inside the asset-inspector hierarchy.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetInspectorHierarchyActionHandler(SnAPI::UI::TDelegate< void(const HierarchyActionRequest &)> Handler)`

Install the callback invoked when the user requests a hierarchy edit inside the asset-inspector modal.

**Parameters**

- `Handler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetDetails(ContentAssetDetails Details)`

Replace the detail-pane payload for the currently selected content asset.

**Parameters**

- `Details`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetContentAssetInspectorState(ContentAssetInspectorState State)`

Replace the asset-inspector modal state.

**Parameters**

- `State`:
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::RegisterExternalElements(GameRuntime &Runtime)`

**Parameters**

- `Runtime`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::UIContext * SnAPI::GameFramework::Editor::EditorLayout::RootContext(GameRuntime &Runtime) const`

**Parameters**

- `Runtime`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::BuildShell(SnAPI::UI::UIContext &Context, GameRuntime &Runtime, CameraComponent *ActiveCamera, EditorSelectionModel *SelectionModel)`

**Parameters**

- `Context`: 
- `Runtime`: 
- `ActiveCamera`: 
- `SelectionModel`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::ConfigureRoot(SnAPI::UI::UIContext &Context)`

**Parameters**

- `Context`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::BuildMenuBar(PanelBuilder &Root)`

**Parameters**

- `Root`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::BuildToolbar(PanelBuilder &Root)`

**Parameters**

- `Root`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::BuildWorkspace(PanelBuilder &Root, GameRuntime &Runtime, CameraComponent *ActiveCamera, EditorSelectionModel *SelectionModel)`

**Parameters**

- `Root`: 
- `Runtime`: 
- `ActiveCamera`: 
- `SelectionModel`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::BuildContentBrowser(PanelBuilder &Root)`

**Parameters**

- `Root`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::EnsureContextMenuOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::EnsureContentAssetCreateModalOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::DestroyContentAssetCreateModalOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::EnsureContentAssetImportModalOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::DestroyContentAssetImportModalOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::EnsureContentAssetInspectorModalOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::DestroyContentAssetInspectorModalOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::EnsureProjectModalOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::DestroyProjectModalOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::EnsureProjectSettingsModalOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::DestroyProjectSettingsModalOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::BuildHierarchyPane(PanelBuilder &Workspace, GameRuntime &Runtime, CameraComponent *ActiveCamera, EditorSelectionModel *SelectionModel)`

**Parameters**

- `Workspace`: 
- `Runtime`: 
- `ActiveCamera`: 
- `SelectionModel`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::BuildGamePane(PanelBuilder &Workspace, GameRuntime &Runtime, CameraComponent *ActiveCamera)`

**Parameters**

- `Workspace`: 
- `Runtime`: 
- `ActiveCamera`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::BuildInspectorPane(PanelBuilder &Workspace, BaseNode *SelectedNode, CameraComponent *ActiveCamera)`

**Parameters**

- `Workspace`: 
- `SelectedNode`: 
- `ActiveCamera`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::BuildContentDetailsPane(PanelBuilder &DetailsTab)`

**Parameters**

- `DetailsTab`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::EnsureDefaultSelection(CameraComponent *ActiveCamera)`

**Parameters**

- `ActiveCamera`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SyncHierarchy(GameRuntime &Runtime, CameraComponent *ActiveCamera)`

**Parameters**

- `Runtime`: 
- `ActiveCamera`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RebuildHierarchyTree(const std::vector< HierarchyEntry > &Entries, const NodeHandle &SelectedNode)`

**Parameters**

- `Entries`: 
- `SelectedNode`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SyncHierarchySelection(const NodeHandle &SelectedNode)`

**Parameters**

- `SelectedNode`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::CollectHierarchyEntries(World &WorldRef, std::vector< HierarchyEntry > &OutEntries) const`

**Parameters**

- `WorldRef`: 
- `OutEntries`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::EditorLayout::ComputeHierarchySignature(const std::vector< HierarchyEntry > &Entries) const`

**Parameters**

- `Entries`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OnHierarchyNodeChosen(const NodeHandle &Handle)`

**Parameters**

- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `BaseNode * SnAPI::GameFramework::Editor::EditorLayout::ResolveSelectedNode(GameRuntime &Runtime, CameraComponent *ActiveCamera) const`

**Parameters**

- `Runtime`: 
- `ActiveCamera`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::EditorLayout::QueryInvalidationDebugOverlayEnabled() const`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SetInvalidationDebugOverlayEnabled(bool Enabled)`

**Parameters**

- `Enabled`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::ToggleInvalidationDebugOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SyncInvalidationDebugOverlay()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::PublishInvalidationDebugState()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::HandleContentAssetCardClicked(std::size_t AssetIndex)`

**Parameters**

- `AssetIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SelectContentAsset(std::size_t AssetIndex, bool NotifySelection, bool IsDoubleClick)`

**Parameters**

- `AssetIndex`: 
- `NotifySelection`: 
- `IsDoubleClick`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenHierarchyContextMenu(std::size_t ItemIndex, const SnAPI::UI::PointerEvent &Event)`

**Parameters**

- `ItemIndex`: 
- `Event`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenHierarchyAddTypeMenu(bool AddComponents)`

**Parameters**

- `AddComponents`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenContentAssetContextMenu(std::size_t AssetIndex, const SnAPI::UI::PointerEvent &Event)`

**Parameters**

- `AssetIndex`: 
- `Event`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenFileMenu()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenInspectorComponentContextMenu(const NodeHandle &OwnerNode, const TypeId &ComponentType, const SnAPI::UI::PointerEvent &Event)`

**Parameters**

- `OwnerNode`: 
- `ComponentType`: 
- `Event`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenContentBrowserContextMenu(const SnAPI::UI::PointerEvent &Event)`

**Parameters**

- `Event`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenContextMenu(const SnAPI::UI::UIPoint &ScreenPosition, std::vector< SnAPI::UI::UIContextMenuItem > Items)`

**Parameters**

- `ScreenPosition`: 
- `Items`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::CloseContextMenu()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OnContextMenuItemInvoked(const SnAPI::UI::UIContextMenuItem &Item)`

**Parameters**

- `Item`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::EnsureContentAssetCardCapacity()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::UpdateContentAssetCardWidgets()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::ApplyContentAssetFilter()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenContentAssetCreateModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::CloseContentAssetCreateModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::ConfirmContentAssetCreate()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetCreateModalVisibility()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RebuildContentAssetCreateTypeTree()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetCreateOkButtonState()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenContentAssetImportModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::CloseContentAssetImportModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::ConfirmContentAssetImport()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetImportModalVisibility()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetImportProfile()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetImportSettingsPanel()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetImportSummary()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetImportOkButtonState()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenProjectWelcomeModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenProjectCreateModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenProjectOpenModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenProjectSettingsModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::CloseProjectSettingsModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::ConfirmProjectSettingsModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::CloseProjectModal(bool ForceClose=false)`

**Parameters**

- `ForceClose`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::ConfirmProjectModal()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshProjectModalVisibility()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshProjectModalOkButtonState()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshProjectSettingsModalVisibility()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshProjectSettingsModalSaveButtonState()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RememberRecentProject(const ProjectActionRequest &Request)`

**Parameters**

- `Request`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RememberRecentProjectFile(std::string ProjectFilePath, std::string ProjectName={})`

**Parameters**

- `ProjectFilePath`: 
- `ProjectName`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::CloseContentAssetInspectorModal(bool NotifyHandler)`

**Parameters**

- `NotifyHandler`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetInspectorModalVisibility()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetInspectorModalState()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RebuildContentAssetInspectorHierarchyTree()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenContentAssetInspectorHierarchyContextMenu(std::size_t ItemIndex, const SnAPI::UI::PointerEvent &Event)`

**Parameters**

- `ItemIndex`: 
- `Event`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::OpenContentAssetInspectorComponentContextMenu(const NodeHandle &OwnerNode, const TypeId &ComponentType, const SnAPI::UI::PointerEvent &Event)`

**Parameters**

- `OwnerNode`: 
- `ComponentType`: 
- `Event`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RebuildContentBrowserEntries()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentBrowserPath()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetCardSelectionStyles()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::RefreshContentAssetDetailsViewModel()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::Editor::EditorLayout::ResolveSelectedContentAssetIndex() const`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::InitializeViewModel()`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::TPropertyRef< TValue > SnAPI::GameFramework::Editor::EditorLayout::ViewModelProperty(const SnAPI::UI::PropertyKey Key)`

**Parameters**

- `Key`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::BindInspectorTarget(BaseNode *SelectedNode, CameraComponent *ActiveCamera)`

**Parameters**

- `SelectedNode`: 
- `ActiveCamera`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::EditorLayout::SyncGameViewportCamera(GameRuntime &Runtime, CameraComponent *ActiveCamera)`

**Parameters**

- `Runtime`: 
- `ActiveCamera`:
</div>
<div class="snapi-api-card" markdown="1">
### `UIRenderViewport * SnAPI::GameFramework::Editor::EditorLayout::ResolveGameViewport() const`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::UI::UITabs * SnAPI::GameFramework::Editor::EditorLayout::ResolveGameViewTabs() const`
</div>
<div class="snapi-api-card" markdown="1">
### `UIPropertyPanel * SnAPI::GameFramework::Editor::EditorLayout::ResolveInspectorPanel() const`
</div>
