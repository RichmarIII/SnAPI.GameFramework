#pragma once

#if defined(SNAPI_GF_ENABLE_UI)

#include <cstdint>
#include <cstddef>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "AssetPipelineIds.h"
#include "Export.h"
#include "Handles.h"
#include "Math.h"
#include "RenderAssetPayloads.h"
#include "StaticTypeId.h"
#include "TypeRegistry.h"
#include "Uuid.h"

#include <UIAccordion.h>
#include <UIDelegates.h>
#include <UIEvents.h>
#include <UIScrollContainer.h>

namespace SnAPI::GameFramework
{
class BaseNode;
template<typename TBase, typename TNameTag>
class TAssetRef;
struct MaterialInstanceAssetRuntime;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reflection-driven inspector panel for nodes, components, and other reflected objects.
 *
 * `UIPropertyPanel` builds editor widgets from `TypeRegistry` field metadata at runtime.
 * It is the editor-facing bridge between reflected object models and a live UI tree.
 * Typical usage binds either:
 * - a plain reflected object via `BindObject()`
 * - a node via `BindNode()`, which also enumerates currently attached components
 *
 * Core semantics:
 * - Binding is borrowed; the panel never owns the inspected object.
 * - Binding the same object again avoids a full rebuild and only re-syncs widget values.
 * - `RefreshFromModel()` pushes current model state into existing widgets but does not
 *   rediscover a changed component list.
 * - Unsupported reflected field types remain visible as informational rows instead of
 *   being silently dropped.
 *
 * Ownership and lifetime:
 * - The caller owns the bound object and must keep it alive while the panel is bound.
 * - UI element lifetime is owned by the attached `UIContext`.
 * - Component pointers discovered through `BindNode()` are borrowed from the world and
 *   are only valid while those components remain attached.
 *
 * Threading:
 * - Main-thread only.
 * - UI events, model writes, and synchronization all assume the owning `UIContext`
 *   thread.
 *
 * @warning Binding a node does not make the component list self-updating. If components
 *          are added or removed, call `BindNode()` again to rebuild the sections.
 */
class SNAPI_GAMEFRAMEWORK_API UIPropertyPanel final : public SnAPI::UI::UIScrollContainer
{
public:
  /** @brief Construct an empty scroll-based property panel with editor-oriented default styling. */
  UIPropertyPanel();

  /**
   * @brief Initialize the panel as a UI element inside a context.
   * @param Context Owning UI context.
   * @param Id Element id assigned by the context.
   *
   * The panel does not build any inspector UI here; it only binds itself to the
   * framework UI runtime. A subsequent `BindObject()` or `BindNode()` call is required
   * to populate content.
   */
  void Initialize(SnAPI::UI::UIContext* Context, SnAPI::UI::ElementId Id);

  /**
   * @brief Bind a reflected object instance using its static reflected type id.
   * @tparam TObject Reflected object type.
   * @param Instance Borrowed pointer to inspect.
   * @return `true` when the binding is accepted, otherwise `false`.
   *
   * Passing `nullptr` clears the panel.
   */
  template<typename TObject>
  bool BindObject(TObject* Instance)
  {
    if (!Instance)
    {
      return false;
    }

    using TObjectNoCv = std::remove_cv_t<TObject>;
    return BindObject(StaticTypeId<TObjectNoCv>(), const_cast<TObjectNoCv*>(Instance));
  }

  /**
   * @brief Bind an arbitrary reflected object instance by explicit type id.
   * @param Type Reflected type to inspect.
   * @param Instance Borrowed object pointer.
   * @return `true` when the object is bound and the UI is available, otherwise `false`.
   *
   * If the same `Type` and `Instance` are already bound and the panel has already been
   * built, this call skips the rebuild and only re-synchronizes model values into the
   * existing editors.
   */
  bool BindObject(const TypeId& Type, void* Instance);

  /**
   * @brief Bind a node and build one inspector section for the node plus one per currently attached component.
   * @param Node Borrowed node pointer to inspect.
   * @return `true` when the inspector UI was rebuilt successfully, otherwise `false`.
   *
   * Component discovery uses the node's world and owner handle. When the stored runtime
   * identity on that handle has drifted, the implementation falls back to UUID-based
   * component lookup so the inspector remains usable.
   */
  bool BindNode(BaseNode* Node);

  /**
   * @brief Clear the current binding and destroy the generated inspector subtree.
   *
   * Any active pointer capture inside the property-panel subtree is released before the
   * content elements are destroyed.
   */
  void ClearObject();

  /**
   * @brief Push current model values into the existing editor widgets.
   *
   * This does not rebuild the UI tree. Use it when the underlying object changed but the
   * reflected shape of the inspected type did not.
   */
  void RefreshFromModel();

  /**
   * @brief Set the callback invoked when a component section requests a context menu.
   * @param Handler Delegate receiving the owner node handle, component type id, and the
   *        originating pointer event.
   *
   * The handler is stored by value. Replacing it overwrites the previous callback.
   */
  void SetComponentContextMenuHandler(
    SnAPI::UI::TDelegate<void(NodeHandle, const TypeId&, const SnAPI::UI::PointerEvent&)> Handler);

  /** @brief Forward routed UI events to the scroll container base implementation. */
  void OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context) override;
  /** @brief Paint the panel through the scroll container base implementation. */
  void Paint(SnAPI::UI::UIPaintContext& Context) const override;

private:
  struct FieldPathEntry
  {
    TypeId OwnerType{};
    std::string FieldName{};
    bool IsConst = false;
  };

  enum class EEditorKind : uint8_t
  {
    Bool = 0,
    Signed,
    Unsigned,
    Float,
    Double,
    String,
    Vec2,
    Vec3,
    Vec4,
    Quat,
    Color,
    Uuid,
    Enum,
    SubClass,
    AssetRef,
    Unsupported
  };

  struct FieldBinding
  {
    void* RootInstance = nullptr;
    std::vector<FieldPathEntry> Path{};
    TypeId FieldType{};
    EEditorKind EditorKind = EEditorKind::Unsupported;
    bool ReadOnly = false;
    std::uint64_t Generation = 0;
    SnAPI::UI::ElementId EditorId{};
    std::array<SnAPI::UI::ElementId, 4> ComponentEditorIds{};
    std::uint8_t ComponentCount = 0;
    std::size_t EditorHookHandle = 0;
    std::array<std::size_t, 4> ComponentHookHandles{};
    bool UsesFilesystemPicker = false;
    bool PathSelectsDirectories = false;
  };

  struct BoundSection
  {
    TypeId Type{};
    void* Instance = nullptr;
    std::string Heading{};
    NodeHandle ComponentOwner{};
    bool IsComponent = false;
  };

  bool RebuildUi();
  void BuildTypeIntoContainer(
    SnAPI::UI::ElementId Parent,
    const TypeId& Type,
    void* RootInstance,
    const std::vector<FieldPathEntry>& PathPrefix,
    int Depth);
  void AddFieldEditor(
    SnAPI::UI::ElementId Parent,
    const FieldInfo& Field,
    void* RootInstance,
    std::vector<FieldPathEntry> Path,
    int Depth);
  void AddMaterialScalarCollectionEditor(
    SnAPI::UI::ElementId Parent,
    std::vector<MaterialScalarParamPayload>& Scalars,
    bool ReadOnly,
    std::string_view Heading);
  void AddMaterialVectorCollectionEditor(
    SnAPI::UI::ElementId Parent,
    std::vector<MaterialVectorParamPayload>& Vectors,
    bool ReadOnly,
    std::string_view Heading);
  void AddMaterialTextureCollectionEditor(
    SnAPI::UI::ElementId Parent,
    std::vector<MaterialTextureParamPayload>& Textures,
    bool ReadOnly,
    std::string_view Heading);
  void AddAssetRefCollectionEditor(
    SnAPI::UI::ElementId Parent,
    std::vector<AssetRefPayload>& References,
    bool ReadOnly,
    std::string_view Heading,
    const std::optional<::SnAPI::AssetPipeline::TypeId>& AssetKindFilter,
    std::string_view RowPrefix);
  void AddMaterialInstanceAssetRefCollectionEditor(
    SnAPI::UI::ElementId Parent,
    std::vector<TAssetRef<MaterialInstanceAssetRuntime, void>>& References,
    bool ReadOnly,
    std::string_view Heading,
    std::string_view RowPrefix);
  void AddUnsupportedRow(
    SnAPI::UI::ElementId Parent,
    std::string_view Label,
    std::string_view Reason);

  [[nodiscard]] EEditorKind ResolveEditorKind(const TypeId& Type) const;
  [[nodiscard]] bool IsNestedStructType(const TypeId& Type) const;
  [[nodiscard]] std::string PrettyTypeName(const TypeId& Type) const;
  [[nodiscard]] std::string PrettyFieldName(std::string_view Name) const;
  [[nodiscard]] bool IsPathLikeField(const FieldInfo& Field) const;
  [[nodiscard]] bool IsDirectoryLikeField(const FieldInfo& Field) const;
  [[nodiscard]] std::filesystem::path ResolveAssetRootPath() const;
  [[nodiscard]] std::string NormalizeToAssetUri(std::string_view Candidate) const;

  [[nodiscard]] bool ResolveLeafPath(
    void* Root,
    const std::vector<FieldPathEntry>& Path,
    void*& OutOwner,
    const FieldInfo*& OutField) const;
  [[nodiscard]] bool ReadFieldValue(
    const FieldBinding& Binding,
    std::string& OutText,
    bool& OutBool) const;
  bool WriteFieldValue(
    const FieldBinding& Binding,
    std::string_view TextValue,
    bool BoolValue);

  [[nodiscard]] bool ParseBool(std::string_view Text, bool& OutValue) const;
  [[nodiscard]] bool ParseSigned(std::string_view Text, std::int64_t& OutValue) const;
  [[nodiscard]] bool ParseUnsigned(std::string_view Text, std::uint64_t& OutValue) const;
  [[nodiscard]] bool ParseDouble(std::string_view Text, double& OutValue) const;
  [[nodiscard]] bool ParseVec2(std::string_view Text, Vec2& OutValue) const;
  [[nodiscard]] bool ParseVec3(std::string_view Text, Vec3& OutValue) const;
  [[nodiscard]] bool ParseVec4(std::string_view Text, Vec4& OutValue) const;
  [[nodiscard]] bool ParseQuat(std::string_view Text, Quat& OutValue) const;
  [[nodiscard]] bool ParseColor(std::string_view Text, SnAPI::UI::Color& OutValue) const;
  [[nodiscard]] bool ParseUuid(std::string_view Text, Uuid& OutValue) const;

  [[nodiscard]] std::string FormatVec2(const Vec2& Value) const;
  [[nodiscard]] std::string FormatVec3(const Vec3& Value) const;
  [[nodiscard]] std::string FormatVec4(const Vec4& Value) const;
  [[nodiscard]] std::string FormatQuat(const Quat& Value) const;
  [[nodiscard]] std::string FormatColor(const SnAPI::UI::Color& Value) const;

  FieldBinding* ResolveLiveBinding(std::size_t BindingIndex, std::uint64_t Generation);
  [[nodiscard]] bool IsEditorFocused(const FieldBinding& Binding) const;
  void AttachEditorHooks(std::size_t BindingIndex);
  void ClearBindingHooks();
  void CommitBindingFromEditor(
    std::size_t BindingIndex,
    std::uint64_t Generation,
    std::string_view TextValue,
    bool BoolValue);
  void CommitBindingFromComponents(std::size_t BindingIndex, std::uint64_t Generation);
  void SyncBindingToEditor(FieldBinding& Binding);

  void SyncModelToEditors();

  TypeId m_BoundType{};
  void* m_BoundInstance = nullptr;
  std::vector<BoundSection> m_BoundSections{};
  SnAPI::UI::ElementId m_ContentRoot{};
  std::vector<FieldBinding> m_Bindings{};
  std::uint64_t m_BindingGeneration = 0;
  bool m_Built = false;
  bool m_RebuildInProgress = false;
  bool m_SyncingModelToEditors = false;
  bool m_CommittingEditorToModel = false;
  SnAPI::UI::TDelegate<void(NodeHandle, const TypeId&, const SnAPI::UI::PointerEvent&)>
    m_OnComponentContextMenuRequested{};
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
