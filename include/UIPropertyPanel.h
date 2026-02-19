#pragma once

#if defined(SNAPI_GF_ENABLE_UI)

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "Export.h"
#include "Math.h"
#include "StaticTypeId.h"
#include "TypeRegistry.h"
#include "Uuid.h"

#include <UIAccordion.h>
#include <UIScrollContainer.h>

namespace SnAPI::GameFramework
{

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
  void ClearObject();
  void RefreshFromModel();

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
    Vec3,
    Quat,
    Uuid,
    Enum,
    Unsupported
  };

  struct FieldBinding
  {
    std::vector<FieldPathEntry> Path{};
    TypeId FieldType{};
    EEditorKind EditorKind = EEditorKind::Unsupported;
    bool ReadOnly = false;
    SnAPI::UI::ElementId EditorId{};
    std::string LastText{};
    bool LastBool = false;
  };

  bool RebuildUi();
  void BuildTypeIntoContainer(
    SnAPI::UI::ElementId Parent,
    const TypeId& Type,
    std::vector<FieldPathEntry> PathPrefix,
    int Depth);
  void AddFieldEditor(
    SnAPI::UI::ElementId Parent,
    const FieldInfo& Field,
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
  [[nodiscard]] bool ParseVec3(std::string_view Text, Vec3& OutValue) const;
  [[nodiscard]] bool ParseQuat(std::string_view Text, Quat& OutValue) const;
  [[nodiscard]] bool ParseUuid(std::string_view Text, Uuid& OutValue) const;

  [[nodiscard]] std::string FormatVec3(const Vec3& Value) const;
  [[nodiscard]] std::string FormatQuat(const Quat& Value) const;

  void SyncModelToEditors();
  void SyncEditorsToModel();

  TypeId m_BoundType{};
  void* m_BoundInstance = nullptr;
  SnAPI::UI::ElementId m_ContentRoot{};
  std::vector<FieldBinding> m_Bindings{};
  bool m_Built = false;
  bool m_RebuildInProgress = false;
  mutable bool m_PaintSyncInProgress = false;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
