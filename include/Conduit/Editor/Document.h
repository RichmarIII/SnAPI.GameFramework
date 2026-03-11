#pragma once

#include "Editor/EditorExport.h"
#include "Conduit/Asset.h"
#include "Conduit/Editor/Schema.h"
#include "Conduit/Editor/Types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace SnAPI::GameFramework::Conduit::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief One open authored Conduit graph document hosted by the editor shell.
 *
 * The document owns an editable working copy of `GraphAsset`, tracks transient authoring state
 * such as selection and compile diagnostics, and exposes a revision counter so UI and tooling can
 * cheaply detect mutations.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API GraphDocument
{
public:
    /**
     * @brief Construct one document.
     * @param AssetKey Stable editor asset key.
     * @param Title UI-facing tab title.
     * @param Asset Editable graph payload.
     */
    GraphDocument(std::string AssetKey, std::string Title, GraphAsset Asset)
        : m_assetKey(std::move(AssetKey))
        , m_title(std::move(Title))
        , m_asset(std::move(Asset))
    {
    }

    /** @brief Access the owning editor asset key. */
    [[nodiscard]] const std::string& AssetKey() const { return m_assetKey; }
    /** @brief Access the current UI title. */
    [[nodiscard]] const std::string& Title() const { return m_title; }
    /** @brief Access the editable authored graph payload. */
    [[nodiscard]] GraphAsset& Asset() { return m_asset; }
    /** @brief Access the editable authored graph payload. */
    [[nodiscard]] const GraphAsset& Asset() const { return m_asset; }
    /** @brief Access current transient selection state. */
    [[nodiscard]] GraphSelection& Selection() { return m_selection; }
    /** @brief Access current transient selection state. */
    [[nodiscard]] const GraphSelection& Selection() const { return m_selection; }
    /** @brief Access the current document revision. */
    [[nodiscard]] std::uint64_t Revision() const { return m_revision; }
    /** @brief Access the current dirty flag. */
    [[nodiscard]] bool IsDirty() const { return m_dirty; }
    /** @brief Access the last compile result, if any. */
    [[nodiscard]] const std::optional<CompileOutput>& LastCompile() const { return m_lastCompile; }
    /** @brief Find one authored graph variable by stable id. */
    [[nodiscard]] GraphVariableAsset* FindVariable(const Uuid& Id);
    /** @brief Find one authored graph variable by stable id. */
    [[nodiscard]] const GraphVariableAsset* FindVariable(const Uuid& Id) const;
    /** @brief Find one authored graph node by stable id. */
    [[nodiscard]] GraphNodeAsset* FindNode(const Uuid& Id);
    /** @brief Find one authored graph node by stable id. */
    [[nodiscard]] const GraphNodeAsset* FindNode(const Uuid& Id) const;
    /** @brief Find one authored graph-node layout record by stable node id. */
    [[nodiscard]] GraphNodeEditorAsset* FindNodeEditorState(const Uuid& Id);
    /** @brief Find one authored graph-node layout record by stable node id. */
    [[nodiscard]] const GraphNodeEditorAsset* FindNodeEditorState(const Uuid& Id) const;

    /**
     * @brief Add one new authored graph variable.
     * @param Name User-facing variable name.
     * @param Type Reflected stored type.
     * @return Borrowed variable pointer or an error.
     */
    TExpected<GraphVariableAsset*> AddVariable(std::string_view Name, const TypeId& Type);
    /**
     * @brief Spawn one new authored node from one schema descriptor.
     * @param Descriptor Schema-backed node template.
     * @return Borrowed authored node pointer or an error.
     */
    TExpected<GraphNodeAsset*> AddNode(const SchemaNodeDescriptor& Descriptor);
    /**
     * @brief Remove one authored graph variable and any authored get/set nodes that target it.
     * @param Id Stable variable id.
     * @return `true` when a variable existed and was removed.
     */
    bool RemoveVariable(const Uuid& Id);
    /**
     * @brief Remove one authored graph node and any associated editor metadata.
     * @param Id Stable node id.
     * @return `true` when a node existed and was removed.
     */
    bool RemoveNode(const Uuid& Id);
    /**
     * @brief Rename one authored graph variable.
     * @param Id Stable variable id.
     * @param Name New user-facing name.
     * @return Success or an error.
     */
    Result RenameVariable(const Uuid& Id, std::string_view Name);
    /**
     * @brief Change the reflected type of one authored graph variable.
     * @param Id Stable variable id.
     * @param Type New reflected type id.
     * @return Success or an error.
     */
    Result SetVariableType(const Uuid& Id, const TypeId& Type);
    /**
     * @brief Replace the authored default value for one graph variable.
     * @param Id Stable variable id.
     * @param Value New serialized reflected default value.
     * @return Success or an error.
     */
    Result SetVariableDefault(const Uuid& Id, const SerializedValue& Value);
    /**
     * @brief Clear the authored default value for one graph variable.
     * @param Id Stable variable id.
     * @return Success or an error.
     */
    Result ClearVariableDefault(const Uuid& Id);
    /**
     * @brief Rename one authored custom entrypoint node.
     * @param Id Stable node id.
     * @param Name New custom entrypoint name.
     * @return Success or an error.
     */
    Result SetNodeEntryPointName(const Uuid& Id, std::string_view Name);
    /**
     * @brief Update the primary label text on one authored node.
     *
     * This applies to label nodes directly and to jump/branch target labels.
     *
     * @param Id Stable node id.
     * @param Label New label text.
     * @return Success or an error.
     */
    Result SetNodeLabelName(const Uuid& Id, std::string_view Label);
    /**
     * @brief Update the secondary false-target label on one authored branch node.
     * @param Id Stable node id.
     * @param Label New false-target label text.
     * @return Success or an error.
     */
    Result SetNodeFalseLabelName(const Uuid& Id, std::string_view Label);
    /**
     * @brief Update the authored graph-space position of one node.
     * @param Id Stable node id.
     * @param X New graph-space left position.
     * @param Y New graph-space top position.
     * @return Success or an error.
     */
    Result SetNodePosition(const Uuid& Id, float X, float Y);
    /**
     * @brief Update the authored graph-canvas viewport state.
     * @param PanX New graph-space horizontal pan.
     * @param PanY New graph-space vertical pan.
     * @param Zoom New graph-canvas zoom.
     * @return Success or an error.
     */
    Result SetViewport(float PanX, float PanY, float Zoom);

    /**
     * @brief Rename the UI-facing document title.
     * @param Title New title.
     */
    void SetTitle(std::string_view Title)
    {
        m_title.assign(Title);
    }

    /**
     * @brief Mark the document dirty and advance its revision.
     */
    void Touch()
    {
        m_dirty = true;
        ++m_revision;
    }

    /**
     * @brief Clear the dirty flag after a successful save.
     */
    void MarkSaved()
    {
        m_dirty = false;
        ++m_revision;
    }

    /**
     * @brief Replace the cached compile output.
     * @param Output New compile result.
     */
    void SetLastCompile(CompileOutput Output)
    {
        m_lastCompile = std::move(Output);
    }

    /**
     * @brief Clear any cached compile output.
     */
    void ClearLastCompile()
    {
        m_lastCompile.reset();
    }

private:
    void MarkMutated();

    std::string m_assetKey{};
    std::string m_title{};
    GraphAsset m_asset{};
    GraphSelection m_selection{};
    bool m_dirty = false;
    std::uint64_t m_revision = 0;
    std::optional<CompileOutput> m_lastCompile{};
};

/**
 * @ingroup SnAPI_GameFramework_Conduit_Editor
 * @brief One open authored Conduit class document hosted by the editor shell.
 *
 * The class document owns an editable working copy of `ClassAsset` and tracks only the
 * lightweight document state currently needed for Conduit class authoring.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API ClassDocument
{
public:
    ClassDocument(std::string AssetKey, std::string Title, ClassAsset Asset)
        : m_assetKey(std::move(AssetKey))
        , m_title(std::move(Title))
        , m_asset(std::move(Asset))
    {
    }

    [[nodiscard]] const std::string& AssetKey() const { return m_assetKey; }
    [[nodiscard]] const std::string& Title() const { return m_title; }
    [[nodiscard]] ClassAsset& Asset() { return m_asset; }
    [[nodiscard]] const ClassAsset& Asset() const { return m_asset; }
    [[nodiscard]] std::uint64_t Revision() const { return m_revision; }
    [[nodiscard]] bool IsDirty() const { return m_dirty; }

    void SetTitle(std::string_view Title)
    {
        m_title.assign(Title);
    }

    Result SetName(std::string_view Name)
    {
        if (m_asset.Name == Name)
        {
            return Ok();
        }

        m_asset.Name.assign(Name);
        Touch();
        return Ok();
    }

    Result SetHostType(const TypeId& HostType)
    {
        if (m_asset.HostType == HostType)
        {
            return Ok();
        }

        m_asset.HostType = HostType;
        Touch();
        return Ok();
    }

    Result SetGraphAsset(std::string_view AssetKey)
    {
        const std::string Normalized(AssetKey);
        if (m_asset.Graph.GetAssetName() == Normalized && m_asset.Graph.GetAssetId().empty())
        {
            return Ok();
        }

        m_asset.Graph.EditAssetName() = Normalized;
        m_asset.Graph.EditAssetId().clear();
        Touch();
        return Ok();
    }

    void Touch()
    {
        m_dirty = true;
        ++m_revision;
    }

    void MarkSaved()
    {
        m_dirty = false;
        ++m_revision;
    }

private:
    std::string m_assetKey{};
    std::string m_title{};
    ClassAsset m_asset{};
    bool m_dirty = false;
    std::uint64_t m_revision = 0;
};

} // namespace SnAPI::GameFramework::Conduit::Editor
