#pragma once

#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"
#include "Conduit/Editor/CompilerBridge.h"
#include "Conduit/Editor/Schema.h"

#include <deque>
#include <memory>
#include <string_view>

namespace SnAPI::GameFramework::Editor
{
class EditorAssetService;
}

namespace SnAPI::GameFramework::Conduit::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief Editor service that owns open Conduit graph documents and schema state.
 *
 * This is the editor-system root for Conduit authoring. It is intentionally small in the first
 * slice and currently owns:
 * - the builtin/reflection-backed schema registry
 * - open `GraphDocument` instances
 * - the compile bridge used by future canvas/inspector UI
 *
 * The eventual graph canvas, palette, diagnostics, and command routing should consume this service
 * rather than talking directly to low-level runtime Conduit APIs.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API ConduitEditorService final : public ::SnAPI::GameFramework::Editor::IEditorService
{
public:
    /**
     * @brief Lightweight active-document summary consumed by shell/layout code.
     */
    struct WorkspaceView
    {
        EWorkspaceDocumentKind Kind = EWorkspaceDocumentKind::None; /**< @brief Active Conduit document category. */
        bool Open = false; /**< @brief `true` when one Conduit document is currently active. */
        std::string AssetKey{}; /**< @brief Stable editor asset key for the active document. */
        std::string Title{}; /**< @brief UI-facing document title. */
        std::string SelfTypeLabel{}; /**< @brief Human-readable reflected self type label. */
        std::string HostTypeLabel{}; /**< @brief Human-readable host type label for class documents. */
        std::string GraphAssetLabel{}; /**< @brief Human-readable graph asset label for class documents. */
        std::size_t SlotCount = 0; /**< @brief Authored slot count. */
        std::size_t VariableCount = 0; /**< @brief Authored graph-variable count. */
        std::size_t NodeCount = 0; /**< @brief Authored node count. */
        bool IsDirty = false; /**< @brief `true` when the document has unsaved edits. */
        bool HasCompile = false; /**< @brief `true` when a compile result is cached on the document. */
        bool CompileSucceeded = false; /**< @brief `true` when the cached compile has a runtime graph and no errors. */
        std::size_t WarningCount = 0; /**< @brief Number of warning diagnostics in the cached compile output. */
        std::size_t ErrorCount = 0; /**< @brief Number of error diagnostics in the cached compile output. */
        std::uint64_t Revision = 0; /**< @brief Monotonic UI-invalidating revision for this active-document view. */
    };

    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(::SnAPI::GameFramework::Editor::EditorServiceContext& Context) override;
    void Shutdown(::SnAPI::GameFramework::Editor::EditorServiceContext& Context) override;

    /** @brief Access the Conduit schema registry. */
    [[nodiscard]] SchemaRegistry& Schema() { return m_schema; }
    /** @brief Access the Conduit schema registry. */
    [[nodiscard]] const SchemaRegistry& Schema() const { return m_schema; }
    /** @brief Access the Conduit compile bridge. */
    [[nodiscard]] CompilerBridge& Compiler() { return m_compiler; }
    /** @brief Access the Conduit compile bridge. */
    [[nodiscard]] const CompilerBridge& Compiler() const { return m_compiler; }
    /** @brief Access the currently open documents. */
    [[nodiscard]] const std::deque<GraphDocument>& Documents() const { return m_documents; }
    /** @brief Access the currently open class documents. */
    [[nodiscard]] const std::deque<ClassDocument>& ClassDocuments() const { return m_classDocuments; }
    /** @brief Access the currently active document, if any. */
    [[nodiscard]] GraphDocument* ActiveDocument();
    /** @brief Access the currently active document, if any. */
    [[nodiscard]] const GraphDocument* ActiveDocument() const;
    /** @brief Access the currently active class document, if any. */
    [[nodiscard]] ClassDocument* ActiveClassDocument();
    /** @brief Access the currently active class document, if any. */
    [[nodiscard]] const ClassDocument* ActiveClassDocument() const;
    /** @brief Access the currently selected graph variable on the active document, if any. */
    [[nodiscard]] GraphVariableAsset* SelectedVariable();
    /** @brief Access the currently selected graph variable on the active document, if any. */
    [[nodiscard]] const GraphVariableAsset* SelectedVariable() const;
    /** @brief Access the current shell-facing active-document view. */
    [[nodiscard]] WorkspaceView ActiveWorkspaceView() const;
    /** @brief Access the current active-document variable list view. */
    [[nodiscard]] std::vector<VariableEntryView> ActiveVariableEntries() const;
    /** @brief Access the current selected-variable inspector payload. */
    [[nodiscard]] VariableInspectorView ActiveVariableInspectorView() const;
    /** @brief Access the current selected-node inspector payload. */
    [[nodiscard]] NodeInspectorView ActiveNodeInspectorView() const;
    /** @brief Access the current active Conduit class inspector payload. */
    [[nodiscard]] ClassInspectorView ActiveClassInspectorView() const;
    /** @brief Access the current active-document schema-backed palette entries. */
    [[nodiscard]] std::vector<PaletteEntryView> ActivePaletteEntries() const;
    /** @brief Access the current active-document authored node outline. */
    [[nodiscard]] std::vector<NodeEntryView> ActiveNodeEntries() const;
    /** @brief Access the current active-document graph-canvas view state. */
    [[nodiscard]] GraphCanvasView ActiveCanvasView() const;
    /** @brief Enumerate reflected types eligible for authored graph variables. */
    [[nodiscard]] std::vector<VariableTypeOption> AvailableVariableTypes() const;
    /** @brief Enumerate reflected types eligible as Conduit class host node types. */
    [[nodiscard]] std::vector<ClassHostTypeOption> AvailableClassHostTypes() const;
    /** @brief Enumerate discovered Conduit graph assets eligible for Conduit class binding. */
    [[nodiscard]] std::vector<ClassGraphOption> AvailableClassGraphAssets() const;
    /** @brief Monotonic revision used by layout code to avoid rebuilding unchanged workspace state. */
    [[nodiscard]] std::uint64_t WorkspaceRevision() const { return m_workspaceRevision; }
    /** @brief Query whether any open Conduit document for this asset key is dirty. */
    [[nodiscard]] bool IsDocumentDirty(std::string_view AssetKey) const;

    /**
     * @brief Find one open document by editor asset key.
     * @param AssetKey Stable asset key.
     * @return Borrowed document pointer or null.
     */
    [[nodiscard]] GraphDocument* FindDocument(std::string_view AssetKey);
    /**
     * @brief Find one open document by editor asset key.
     * @param AssetKey Stable asset key.
     * @return Borrowed document pointer or null.
     */
    [[nodiscard]] const GraphDocument* FindDocument(std::string_view AssetKey) const;
    /** @brief Find one open Conduit class document by editor asset key. */
    [[nodiscard]] ClassDocument* FindClassDocument(std::string_view AssetKey);
    /** @brief Find one open Conduit class document by editor asset key. */
    [[nodiscard]] const ClassDocument* FindClassDocument(std::string_view AssetKey) const;

    /**
     * @brief Open or focus one authored graph document.
     * @param AssetKey Stable asset key.
     * @param Title UI-facing title.
     * @param Asset Working-copy payload.
     * @return Borrowed document pointer or an error.
     */
    TExpected<GraphDocument*> OpenDocument(std::string_view AssetKey, std::string_view Title, const GraphAsset& Asset);
    /**
     * @brief Open or focus one authored Conduit class document.
     * @param AssetKey Stable asset key.
     * @param Title UI-facing title.
     * @param Asset Working-copy payload.
     * @return Borrowed document pointer or an error.
     */
    TExpected<ClassDocument*> OpenClassDocument(std::string_view AssetKey, std::string_view Title, const ClassAsset& Asset);

    /**
     * @brief Close one open document.
     * @param AssetKey Stable asset key.
     * @return `true` when a document was found and removed.
     */
    bool CloseDocument(std::string_view AssetKey);
    /** @brief Close any open Conduit document by editor asset key. */
    bool CloseAnyDocument(std::string_view AssetKey);

    /**
     * @brief Make one existing document the active/focused Conduit document.
     * @param AssetKey Stable asset key.
     * @return `true` when the document exists and focus changed.
     */
    bool FocusDocument(std::string_view AssetKey);
    /** @brief Make one existing class document the active/focused Conduit document. */
    bool FocusClassDocument(std::string_view AssetKey);

    /**
     * @brief Compile one open document and cache the result on the document.
     * @param AssetKey Stable asset key.
     * @return Borrowed compile output pointer or an error.
     */
    TExpected<const CompileOutput*> CompileDocument(std::string_view AssetKey);

    /** @brief Select one authored graph variable on the active document. */
    bool SelectVariable(const Uuid& VariableId);
    /** @brief Select one authored graph node on the active document. */
    bool SelectNode(const Uuid& NodeId);
    /** @brief Create and select one new authored graph variable on the active document. */
    TExpected<GraphVariableAsset*> CreateVariable(std::string_view Name, const TypeId& Type);
    /** @brief Spawn and select one new authored node from one schema/template id. */
    TExpected<GraphNodeAsset*> SpawnNode(std::string_view StableId);
    /** @brief Remove the currently selected graph variable from the active document. */
    bool RemoveSelectedVariable();
    /** @brief Remove the currently selected authored graph node from the active document. */
    bool RemoveSelectedNode();
    /** @brief Update the authored graph-space position of one node on the active document. */
    Result MoveNode(const Uuid& NodeId, float X, float Y);
    /** @brief Update the authored graph-canvas viewport state on the active document. */
    Result SetViewport(float PanX, float PanY, float Zoom);
    /** @brief Rename the currently selected graph variable. */
    Result RenameSelectedVariable(std::string_view Name);
    /** @brief Change the reflected type of the currently selected graph variable. */
    Result SetSelectedVariableType(const TypeId& Type);
    /** @brief Apply a bool default value to the currently selected graph variable. */
    Result SetSelectedVariableDefaultBool(bool Value);
    /** @brief Apply a text-encoded default value to the currently selected graph variable. */
    Result SetSelectedVariableDefaultText(std::string_view Value);
    /** @brief Apply an enum-entry default value to the currently selected graph variable. */
    Result SetSelectedVariableDefaultEnum(std::string_view EnumName);
    /** @brief Clear the authored default value on the currently selected graph variable. */
    Result ClearSelectedVariableDefault();
    /** @brief Commit edits from service-owned scratch storage back into the selected graph variable default. */
    Result CommitSelectedVariableComplexDefault();
    /** @brief Reset service-owned scratch storage from the selected graph variable default. */
    Result ResetSelectedVariableDefaultEditor();
    /** @brief Apply the primary editable text field for the currently selected node. */
    Result SetSelectedNodePrimaryText(std::string_view Value);
    /** @brief Apply the secondary editable text field for the currently selected node. */
    Result SetSelectedNodeSecondaryText(std::string_view Value);
    /** @brief Rename the active Conduit class document. */
    Result RenameActiveClass(std::string_view Name);
    /** @brief Change the reflected host node type on the active Conduit class document. */
    Result SetActiveClassHostType(const TypeId& Type);
    /** @brief Change the referenced Conduit graph asset on the active Conduit class document. */
    Result SetActiveClassGraph(std::string_view AssetKey);

private:
    void BumpWorkspaceRevision() { ++m_workspaceRevision; }
    void InvalidateVariableScratch();

    struct VariableScratchState
    {
        Uuid VariableId{};
        TypeId Type{};
        std::shared_ptr<void> Storage{};
    };

    TExpected<void> EnsureVariableScratch(const GraphVariableAsset& Variable) const;

    SchemaRegistry m_schema{};
    CompilerBridge m_compiler{};
    std::deque<GraphDocument> m_documents{};
    std::deque<ClassDocument> m_classDocuments{};
    std::string m_activeDocumentKey{};
    EWorkspaceDocumentKind m_activeDocumentKind = EWorkspaceDocumentKind::None;
    std::uint64_t m_workspaceRevision = 0;
    mutable VariableScratchState m_variableScratch{};
    ::SnAPI::GameFramework::Editor::EditorAssetService* m_assetService = nullptr;
};

} // namespace SnAPI::GameFramework::Conduit::Editor
