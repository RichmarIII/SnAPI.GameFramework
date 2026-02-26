#pragma once

#if defined(SNAPI_GF_ENABLE_UI)

#include <cstdint>
#include <cstddef>
#include <array>
#include <string>
#include <type_traits>
#include <vector>

#include "Export.h"
#include "Handles.h"
#include "Math.h"
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

class SNAPI_GAMEFRAMEWORK_API UIPropertyPanel final : public SnAPI::UI::UIScrollContainer
{
public:
  UIPropertyPanel();

  void Initialize(SnAPI::UI::UIContext* Context, SnAPI::UI::ElementId Id);

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

  bool BindObject(const TypeId& Type, void* Instance);
  bool BindNode(BaseNode* Node);
  void ClearObject();
  void RefreshFromModel();
  void SetComponentContextMenuHandler(
    SnAPI::UI::TDelegate<void(NodeHandle, const TypeId&, const SnAPI::UI::PointerEvent&)> Handler);

  void OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context) override;
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
  void AddUnsupportedRow(
    SnAPI::UI::ElementId Parent,
    std::string_view Label,
    std::string_view Reason);

  [[nodiscard]] EEditorKind ResolveEditorKind(const TypeId& Type) const;
  [[nodiscard]] bool IsNestedStructType(const TypeId& Type) const;
  [[nodiscard]] std::string PrettyTypeName(const TypeId& Type) const;
  [[nodiscard]] std::string PrettyFieldName(std::string_view Name) const;

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
