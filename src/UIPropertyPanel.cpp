#include "UIPropertyPanel.h"

#if defined(SNAPI_GF_ENABLE_UI)

#include "BaseNode.h"
#include "ComponentStorage.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <UIContext.h>
#include <UICheckbox.h>
#include <UIComboBox.h>
#include <UIElementBase.h>
#include <UIEvents.h>
#include <UIPanel.h>
#include <UISizing.h>
#include <UIText.h>
#include <UITextInput.h>

namespace SnAPI::GameFramework
{
namespace
{
constexpr int kMaxReflectionDepth = 8;
constexpr float kLabelRatio = 0.45f;

[[nodiscard]] std::string TrimCopy(std::string_view Text)
{
  size_t begin = 0;
  while (begin < Text.size() && std::isspace(static_cast<unsigned char>(Text[begin])) != 0)
  {
    ++begin;
  }

  size_t end = Text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(Text[end - 1])) != 0)
  {
    --end;
  }

  return std::string(Text.substr(begin, end - begin));
}

[[nodiscard]] std::string ToLowerCopy(std::string_view Text)
{
  std::string out(Text);
  std::ranges::transform(out, out.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

[[nodiscard]] std::string FormatNumber(const double Value)
{
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(6) << Value;
  std::string out = stream.str();

  while (!out.empty() && out.back() == '0')
  {
    out.pop_back();
  }
  if (!out.empty() && out.back() == '.')
  {
    out.pop_back();
  }
  if (out.empty())
  {
    out = "0";
  }
  return out;
}

[[nodiscard]] bool ParseComponentList(std::string_view Text, double* OutValues, const size_t Count)
{
  if (!OutValues || Count == 0)
  {
    return false;
  }

  std::string buffer(Text);
  for (char& c : buffer)
  {
    if (c == ',' || c == ';' || c == '|')
    {
      c = ' ';
    }
  }

  std::istringstream stream(buffer);
  for (size_t index = 0; index < Count; ++index)
  {
    if (!(stream >> OutValues[index]))
    {
      return false;
    }
  }

  double extra = 0.0;
  if (stream >> extra)
  {
    return false;
  }
  return true;
}

[[nodiscard]] bool EqualsIgnoreCase(const std::string_view Left, const std::string_view Right)
{
  if (Left.size() != Right.size())
  {
    return false;
  }

  for (size_t index = 0; index < Left.size(); ++index)
  {
    const auto a = static_cast<unsigned char>(Left[index]);
    const auto b = static_cast<unsigned char>(Right[index]);
    if (std::tolower(a) != std::tolower(b))
    {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool ReadUnsignedBits(const void* Source, const size_t Size, std::uint64_t& OutValue)
{
  if (!Source)
  {
    return false;
  }

  switch (Size)
  {
  case 1:
    {
      std::uint8_t value = 0;
      std::memcpy(&value, Source, sizeof(value));
      OutValue = value;
      return true;
    }
  case 2:
    {
      std::uint16_t value = 0;
      std::memcpy(&value, Source, sizeof(value));
      OutValue = value;
      return true;
    }
  case 4:
    {
      std::uint32_t value = 0;
      std::memcpy(&value, Source, sizeof(value));
      OutValue = value;
      return true;
    }
  case 8:
    {
      std::uint64_t value = 0;
      std::memcpy(&value, Source, sizeof(value));
      OutValue = value;
      return true;
    }
  default:
    return false;
  }
}

[[nodiscard]] bool WriteUnsignedBits(void* Dest, const size_t Size, const std::uint64_t Value)
{
  if (!Dest)
  {
    return false;
  }

  switch (Size)
  {
  case 1:
    {
      if (Value > static_cast<std::uint64_t>(std::numeric_limits<std::uint8_t>::max()))
      {
        return false;
      }
      const auto casted = static_cast<std::uint8_t>(Value);
      std::memcpy(Dest, &casted, sizeof(casted));
      return true;
    }
  case 2:
    {
      if (Value > static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()))
      {
        return false;
      }
      const auto casted = static_cast<std::uint16_t>(Value);
      std::memcpy(Dest, &casted, sizeof(casted));
      return true;
    }
  case 4:
    {
      if (Value > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
      {
        return false;
      }
      const auto casted = static_cast<std::uint32_t>(Value);
      std::memcpy(Dest, &casted, sizeof(casted));
      return true;
    }
  case 8:
    {
      const auto casted = static_cast<std::uint64_t>(Value);
      std::memcpy(Dest, &casted, sizeof(casted));
      return true;
    }
  default:
    return false;
  }
}

[[nodiscard]] bool ConvertUnsignedToBits(const std::uint64_t Value, const size_t Size, std::uint64_t& OutBits)
{
  switch (Size)
  {
  case 1:
    if (Value > static_cast<std::uint64_t>(std::numeric_limits<std::uint8_t>::max()))
    {
      return false;
    }
    OutBits = static_cast<std::uint8_t>(Value);
    return true;
  case 2:
    if (Value > static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()))
    {
      return false;
    }
    OutBits = static_cast<std::uint16_t>(Value);
    return true;
  case 4:
    if (Value > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
      return false;
    }
    OutBits = static_cast<std::uint32_t>(Value);
    return true;
  case 8:
    OutBits = Value;
    return true;
  default:
    return false;
  }
}

[[nodiscard]] bool ConvertSignedToBits(const std::int64_t Value, const size_t Size, std::uint64_t& OutBits)
{
  switch (Size)
  {
  case 1:
    if (Value < static_cast<std::int64_t>(std::numeric_limits<std::int8_t>::min()) ||
        Value > static_cast<std::int64_t>(std::numeric_limits<std::int8_t>::max()))
    {
      return false;
    }
    OutBits = static_cast<std::uint8_t>(static_cast<std::int8_t>(Value));
    return true;
  case 2:
    if (Value < static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::min()) ||
        Value > static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::max()))
    {
      return false;
    }
    OutBits = static_cast<std::uint16_t>(static_cast<std::int16_t>(Value));
    return true;
  case 4:
    if (Value < static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) ||
        Value > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
    {
      return false;
    }
    OutBits = static_cast<std::uint32_t>(static_cast<std::int32_t>(Value));
    return true;
  case 8:
    OutBits = static_cast<std::uint64_t>(Value);
    return true;
  default:
    return false;
  }
}

[[nodiscard]] std::int64_t SignExtend(const std::uint64_t Value, const size_t Size)
{
  switch (Size)
  {
  case 1:
    return static_cast<std::int8_t>(Value);
  case 2:
    return static_cast<std::int16_t>(Value);
  case 4:
    return static_cast<std::int32_t>(Value);
  case 8:
    return static_cast<std::int64_t>(Value);
  default:
    return 0;
  }
}

} // namespace

UIPropertyPanel::UIPropertyPanel()
{
  // Keep panel behavior scroll-centric by default, with one vertical content root.
  Direction().SetDefault(SnAPI::UI::ELayoutDirection::Vertical);
  Padding().SetDefault(8.0f);
  Gap().SetDefault(6.0f);
  ShowHorizontalScrollbar().SetDefault(false);
  ShowVerticalScrollbar().SetDefault(true);
  Smooth().SetDefault(true);
}

void UIPropertyPanel::Initialize(SnAPI::UI::UIContext* Context, const SnAPI::UI::ElementId Id)
{
  UIScrollContainer::Initialize(Context, Id);
}

bool UIPropertyPanel::BindObject(const TypeId& Type, void* Instance)
{
  if (!Instance)
  {
    ClearObject();
    return false;
  }

  if (m_Built && m_ContentRoot.Value != 0 && m_BoundType == Type && m_BoundInstance == Instance)
  {
    SyncModelToEditors();
    return true;
  }

  m_BoundType = Type;
  m_BoundInstance = Instance;
  m_BoundSections.clear();
  m_BoundSections.push_back(BoundSection{
    .Type = Type,
    .Instance = Instance,
    .Heading = PrettyTypeName(Type)});

  if (!RebuildUi())
  {
    return false;
  }

  SyncModelToEditors();
  return true;
}

bool UIPropertyPanel::BindNode(BaseNode* Node)
{
  if (!Node)
  {
    ClearObject();
    return false;
  }

  m_BoundType = Node->TypeKey();
  m_BoundInstance = Node;
  m_BoundSections.clear();

  std::string nodeHeading = Node->Name();
  const std::string nodeTypeName = PrettyTypeName(Node->TypeKey());
  if (nodeHeading.empty())
  {
    nodeHeading = nodeTypeName;
  }
  else if (!nodeTypeName.empty())
  {
    nodeHeading += " (" + nodeTypeName + ")";
  }

  m_BoundSections.push_back(BoundSection{
    .Type = Node->TypeKey(),
    .Instance = Node,
    .Heading = std::move(nodeHeading)});

  const auto& ComponentTypes = Node->ComponentTypes();
  const auto& ComponentStorages = Node->ComponentStorages();
  const size_t ComponentCount = std::min(ComponentTypes.size(), ComponentStorages.size());
  for (size_t Index = 0; Index < ComponentCount; ++Index)
  {
    IComponentStorage* Storage = ComponentStorages[Index];
    if (!Storage)
    {
      continue;
    }

    const TypeId& ComponentType = ComponentTypes[Index];
    void* ComponentInstance = Storage->Borrowed(Node->Handle());
    if (!ComponentInstance)
    {
      continue;
    }

    m_BoundSections.push_back(BoundSection{
      .Type = ComponentType,
      .Instance = ComponentInstance,
      .Heading = PrettyTypeName(ComponentType)});
  }

  if (!RebuildUi())
  {
    return false;
  }

  SyncModelToEditors();
  return true;
}

void UIPropertyPanel::ClearObject()
{
  m_BoundType = TypeId{};
  m_BoundInstance = nullptr;
  m_BoundSections.clear();
  m_ContentRoot = {};
  std::fill(std::begin(m_Children), std::end(m_Children), SnAPI::UI::ElementId{});
  m_ChildCount = 0;
  m_Bindings.clear();
  m_Built = false;
  if (m_Context)
  {
    m_Context->MarkLayoutDirty();
  }
}

void UIPropertyPanel::RefreshFromModel()
{
  SyncModelToEditors();
}

void UIPropertyPanel::OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context)
{
  UIScrollContainer::OnRoutedEvent(Context);

  if (!m_BoundInstance)
  {
    return;
  }

  const uint32_t typeId = Context.TypeId();
  if (typeId == SnAPI::UI::RoutedEventTypes::PointerUp.Id ||
      typeId == SnAPI::UI::RoutedEventTypes::KeyDown.Id ||
      typeId == SnAPI::UI::RoutedEventTypes::TextInput.Id)
  {
    SyncEditorsToModel();
  }
}

void UIPropertyPanel::Paint(SnAPI::UI::UIPaintContext& Context) const
{
  if (m_BoundInstance && !m_PaintSyncInProgress)
  {
    auto* self = const_cast<UIPropertyPanel*>(this);
    self->m_PaintSyncInProgress = true;
    // Pull current control state first so interaction changes from this frame
    // are not overwritten by the model->editor refresh pass below.
    self->SyncEditorsToModel();
    self->SyncModelToEditors();
    self->m_PaintSyncInProgress = false;
  }

  UIScrollContainer::Paint(Context);
}

bool UIPropertyPanel::RebuildUi()
{
  if (!m_Context || !m_BoundInstance)
  {
    return false;
  }

  if (m_RebuildInProgress)
  {
    return false;
  }
  m_RebuildInProgress = true;

  // Property panel is single-content by design.
  m_ContentRoot = {};
  std::fill(std::begin(m_Children), std::end(m_Children), SnAPI::UI::ElementId{});
  m_ChildCount = 0;

  const auto contentHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.ContentRoot");
  if (contentHandle.Id.Value == 0)
  {
    m_RebuildInProgress = false;
    return false;
  }

  m_Context->AddChild(m_Id, contentHandle.Id);
  m_ContentRoot = contentHandle.Id;
  m_Bindings.clear();

  if (auto* contentPanel = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(m_ContentRoot)))
  {
    contentPanel->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    contentPanel->Padding().Set(4.0f);
    contentPanel->Gap().Set(6.0f);
    contentPanel->Width().Set(SnAPI::UI::Sizing::Fill());
    contentPanel->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
  }

  if (m_BoundSections.empty())
  {
    m_BoundSections.push_back(BoundSection{
      .Type = m_BoundType,
      .Instance = m_BoundInstance,
      .Heading = PrettyTypeName(m_BoundType)});
  }

  if (m_BoundSections.size() == 1)
  {
    const BoundSection& Section = m_BoundSections.front();
    BuildTypeIntoContainer(m_ContentRoot, Section.Type, Section.Instance, {}, 0);
  }
  else
  {
    const auto accordionHandle = m_Context->CreateElement<SnAPI::UI::UIAccordion>();
    if (accordionHandle.Id.Value != 0)
    {
      m_Context->AddChild(m_ContentRoot, accordionHandle.Id);
      if (auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id)))
      {
        accordion->Width().Set(SnAPI::UI::Sizing::Fill());
        accordion->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
        accordion->AllowMultipleExpanded().Set(true);
        accordion->DefaultExpanded().Set(true);
        accordion->Gap().Set(4.0f);
        accordion->ContentPadding().Set(6.0f);
      }

      for (const BoundSection& Section : m_BoundSections)
      {
        if (!Section.Instance)
        {
          continue;
        }

        const auto bodyHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.SectionBody");
        if (bodyHandle.Id.Value == 0)
        {
          continue;
        }

        m_Context->AddChild(accordionHandle.Id, bodyHandle.Id);
        if (auto* body = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(bodyHandle.Id)))
        {
          body->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
          body->Padding().Set(4.0f);
          body->Gap().Set(6.0f);
          body->Width().Set(SnAPI::UI::Sizing::Fill());
          body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
        }

        if (auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id)))
        {
          std::string heading = Section.Heading.empty() ? PrettyTypeName(Section.Type) : Section.Heading;
          accordion->SetSectionHeading(bodyHandle.Id, std::move(heading));
          accordion->SetSectionExpanded(bodyHandle.Id, true);
        }

        BuildTypeIntoContainer(bodyHandle.Id, Section.Type, Section.Instance, {}, 0);
      }
    }
  }

  m_Built = true;
  m_RebuildInProgress = false;
  if (m_Context)
  {
    m_Context->MarkLayoutDirty();
  }
  return true;
}

void UIPropertyPanel::BuildTypeIntoContainer(
  const SnAPI::UI::ElementId Parent,
  const TypeId& Type,
  void* RootInstance,
  const std::vector<FieldPathEntry>& PathPrefix,
  const int Depth)
{
  if (!m_Context)
  {
    return;
  }

  if (Depth > kMaxReflectionDepth)
  {
    AddUnsupportedRow(Parent, "Depth", "Maximum reflection depth reached");
    return;
  }

  if (!RootInstance)
  {
    AddUnsupportedRow(Parent, PrettyTypeName(Type), "Null instance");
    return;
  }

  const TypeInfo* typeInfo = TypeRegistry::Instance().Find(Type);
  if (!typeInfo)
  {
    AddUnsupportedRow(Parent, "Type", "Type metadata is not registered");
    return;
  }

  if (typeInfo->Fields.empty())
  {
    AddUnsupportedRow(Parent, PrettyTypeName(Type), "No reflected fields");
    return;
  }

  for (const FieldInfo& field : typeInfo->Fields)
  {
    auto path = PathPrefix;
    path.push_back(FieldPathEntry{Type, field.Name, field.IsConst});
    AddFieldEditor(Parent, field, RootInstance, std::move(path), Depth);
  }
}

void UIPropertyPanel::AddFieldEditor(
  const SnAPI::UI::ElementId Parent,
  const FieldInfo& Field,
  void* RootInstance,
  std::vector<FieldPathEntry> Path,
  const int Depth)
{
  if (!m_Context)
  {
    return;
  }

  const bool pathConst = std::ranges::any_of(Path, [](const FieldPathEntry& Entry) {
    return Entry.IsConst;
  });

  if (IsNestedStructType(Field.FieldType))
  {
    const auto accordionHandle = m_Context->CreateElement<SnAPI::UI::UIAccordion>();
    if (accordionHandle.Id.Value == 0)
    {
      AddUnsupportedRow(Parent, Field.Name, "Failed to create accordion");
      return;
    }

    m_Context->AddChild(Parent, accordionHandle.Id);
    if (auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id)))
    {
      accordion->Width().Set(SnAPI::UI::Sizing::Fill());
      accordion->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
      accordion->AllowMultipleExpanded().Set(true);
      accordion->DefaultExpanded().Set(true);
      accordion->Gap().Set(4.0f);
      accordion->ContentPadding().Set(6.0f);
    }

    const auto bodyHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.NestedBody");
    if (bodyHandle.Id.Value == 0)
    {
      AddUnsupportedRow(Parent, Field.Name, "Failed to create nested body");
      return;
    }

    m_Context->AddChild(accordionHandle.Id, bodyHandle.Id);
    if (auto* body = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(bodyHandle.Id)))
    {
      body->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
      body->Padding().Set(4.0f);
      body->Gap().Set(6.0f);
      body->Width().Set(SnAPI::UI::Sizing::Fill());
      body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
    }

    if (auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id)))
    {
      std::string heading = PrettyTypeName(Field.FieldType);
      if (heading.empty())
      {
        heading = PrettyFieldName(Field.Name);
      }
      accordion->SetSectionHeading(bodyHandle.Id, std::move(heading));
      accordion->SetSectionExpanded(bodyHandle.Id, true);
    }

    BuildTypeIntoContainer(bodyHandle.Id, Field.FieldType, RootInstance, std::move(Path), Depth + 1);
    return;
  }

  const auto rowHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.Row");
  if (rowHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(Parent, rowHandle.Id);

  if (auto* row = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(rowHandle.Id)))
  {
    row->Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    row->Padding().Set(0.0f);
    row->Gap().Set(8.0f);
    row->Width().Set(SnAPI::UI::Sizing::Fill());
    row->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
  }

  const auto labelHandle = m_Context->CreateElement<SnAPI::UI::UIText>(PrettyFieldName(Field.Name));
  if (labelHandle.Id.Value != 0)
  {
    m_Context->AddChild(rowHandle.Id, labelHandle.Id);
    if (auto* label = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(labelHandle.Id)))
    {
      label->Width().Set(SnAPI::UI::Sizing::Ratio(kLabelRatio));
      label->HAlign().Set(SnAPI::UI::EAlignment::Start);
      label->VAlign().Set(SnAPI::UI::EAlignment::Center);
      label->TextAlignment().Set(SnAPI::UI::ETextAlignment::Start);
      // Keep labels readable while preventing overlap into editors.
      label->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    }
  }

  const EEditorKind editorKind = ResolveEditorKind(Field.FieldType);
  const bool readOnly = pathConst || Field.IsConst || !Field.Setter;

  if (editorKind == EEditorKind::Unsupported)
  {
    const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>(
      "<unsupported: " + PrettyTypeName(Field.FieldType) + ">");
    if (textHandle.Id.Value != 0)
    {
      m_Context->AddChild(rowHandle.Id, textHandle.Id);
      if (auto* text = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(textHandle.Id)))
      {
        text->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        text->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
      }
    }
    return;
  }

  FieldBinding binding{};
  binding.RootInstance = RootInstance;
  binding.Path = std::move(Path);
  binding.FieldType = Field.FieldType;
  binding.EditorKind = editorKind;
  binding.ReadOnly = readOnly;

  if (editorKind == EEditorKind::Bool && !readOnly)
  {
    const auto checkboxHandle = m_Context->CreateElement<SnAPI::UI::UICheckbox>();
    if (checkboxHandle.Id.Value == 0)
    {
      return;
    }
    m_Context->AddChild(rowHandle.Id, checkboxHandle.Id);
    binding.EditorId = checkboxHandle.Id;

    if (auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(checkboxHandle.Id)))
    {
      checkbox->Width().Set(SnAPI::UI::Sizing::Auto());
      checkbox->HAlign().Set(SnAPI::UI::EAlignment::Start);
      checkbox->VAlign().Set(SnAPI::UI::EAlignment::Center);
      checkbox->Label().Set(std::string{});
    }
  }
  else if (editorKind == EEditorKind::Enum)
  {
    const auto comboHandle = m_Context->CreateElement<SnAPI::UI::UIComboBox>();
    if (comboHandle.Id.Value == 0)
    {
      return;
    }
    m_Context->AddChild(rowHandle.Id, comboHandle.Id);
    binding.EditorId = comboHandle.Id;

    if (auto* combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(comboHandle.Id)))
    {
      combo->ReadOnly().Set(readOnly);
      combo->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
      combo->MaxDropdownHeight().Set(220.0f);
      combo->Placeholder().Set(PrettyTypeName(Field.FieldType));

      std::vector<std::string> options;
      if (const TypeInfo* enumInfo = TypeRegistry::Instance().Find(Field.FieldType);
          enumInfo && enumInfo->IsEnum)
      {
        options.reserve(enumInfo->EnumValues.size());
        for (const EnumValueInfo& enumValue : enumInfo->EnumValues)
        {
          options.push_back(enumValue.Name);
        }
      }
      combo->SetItems(std::move(options));
    }
  }
  else
  {
    const auto editorHandle = m_Context->CreateElement<SnAPI::UI::UITextInput>();
    if (editorHandle.Id.Value == 0)
    {
      return;
    }
    m_Context->AddChild(rowHandle.Id, editorHandle.Id);
    binding.EditorId = editorHandle.Id;

    if (auto* editor = dynamic_cast<SnAPI::UI::UITextInput*>(&m_Context->GetElement(editorHandle.Id)))
    {
      editor->Multiline().Set(false);
      editor->WordWrap().Set(false);
      editor->AcceptTab().Set(false);
      editor->Resizable().Set(false);
      editor->EnableSpellCheck().Set(false);
      editor->EnableSyntaxHighlight().Set(false);
      editor->ReadOnly().Set(readOnly);
      // Fill remaining row width so rows do not overflow narrow panels.
      editor->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
      editor->Height().Set(SnAPI::UI::Sizing::Auto());
      editor->Placeholder().Set(PrettyTypeName(Field.FieldType));
    }
  }

  m_Bindings.push_back(std::move(binding));
}

void UIPropertyPanel::AddUnsupportedRow(
  const SnAPI::UI::ElementId Parent,
  std::string_view Label,
  std::string_view Reason)
{
  if (!m_Context)
  {
    return;
  }

  const auto rowHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.Unsupported");
  if (rowHandle.Id.Value == 0)
  {
    return;
  }

  m_Context->AddChild(Parent, rowHandle.Id);
  if (auto* row = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(rowHandle.Id)))
  {
    row->Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    row->Padding().Set(0.0f);
    row->Gap().Set(8.0f);
    row->Width().Set(SnAPI::UI::Sizing::Fill());
    row->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
  }

  const auto labelHandle = m_Context->CreateElement<SnAPI::UI::UIText>(std::string(Label));
  if (labelHandle.Id.Value != 0)
  {
    m_Context->AddChild(rowHandle.Id, labelHandle.Id);
    if (auto* label = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(labelHandle.Id)))
    {
      label->Width().Set(SnAPI::UI::Sizing::Ratio(kLabelRatio));
      label->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    }
  }

  const auto reasonHandle = m_Context->CreateElement<SnAPI::UI::UIText>(std::string(Reason));
  if (reasonHandle.Id.Value != 0)
  {
    m_Context->AddChild(rowHandle.Id, reasonHandle.Id);
    if (auto* reason = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(reasonHandle.Id)))
    {
      reason->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
      reason->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    }
  }
}

UIPropertyPanel::EEditorKind UIPropertyPanel::ResolveEditorKind(const TypeId& Type) const
{
  if (Type == StaticTypeId<bool>())
  {
    return EEditorKind::Bool;
  }
  if (Type == StaticTypeId<int>())
  {
    return EEditorKind::Signed;
  }
  if (Type == StaticTypeId<unsigned int>() || Type == StaticTypeId<std::uint64_t>())
  {
    return EEditorKind::Unsigned;
  }
  if (Type == StaticTypeId<float>())
  {
    return EEditorKind::Float;
  }
  if (Type == StaticTypeId<double>())
  {
    return EEditorKind::Double;
  }
  if (Type == StaticTypeId<std::string>())
  {
    return EEditorKind::String;
  }
  if (Type == StaticTypeId<Vec3>())
  {
    return EEditorKind::Vec3;
  }
  if (Type == StaticTypeId<Quat>())
  {
    return EEditorKind::Quat;
  }
  if (Type == StaticTypeId<Uuid>())
  {
    return EEditorKind::Uuid;
  }
  if (const TypeInfo* typeInfo = TypeRegistry::Instance().Find(Type);
      typeInfo && typeInfo->IsEnum)
  {
    return EEditorKind::Enum;
  }
  return EEditorKind::Unsupported;
}

bool UIPropertyPanel::IsNestedStructType(const TypeId& Type) const
{
  if (ResolveEditorKind(Type) != EEditorKind::Unsupported)
  {
    return false;
  }

  const TypeInfo* typeInfo = TypeRegistry::Instance().Find(Type);
  return typeInfo && !typeInfo->Fields.empty();
}

std::string UIPropertyPanel::PrettyTypeName(const TypeId& Type) const
{
  if (const TypeInfo* typeInfo = TypeRegistry::Instance().Find(Type))
  {
    std::string name = typeInfo->Name;
    const size_t sep = name.rfind("::");
    if (sep != std::string::npos)
    {
      name = name.substr(sep + 2);
    }
    return name;
  }

  return ToString(Type);
}

std::string UIPropertyPanel::PrettyFieldName(std::string_view Name) const
{
  std::string out;
  out.reserve(Name.size() + 8);

  for (size_t index = 0; index < Name.size(); ++index)
  {
    const char c = Name[index];
    if (index > 0 &&
        std::isupper(static_cast<unsigned char>(c)) != 0 &&
        (std::islower(static_cast<unsigned char>(Name[index - 1])) != 0 ||
         std::isdigit(static_cast<unsigned char>(Name[index - 1])) != 0))
    {
      out.push_back(' ');
    }
    out.push_back(c);
  }
  return out;
}

bool UIPropertyPanel::ResolveLeafPath(
  void* Root,
  const std::vector<FieldPathEntry>& Path,
  void*& OutOwner,
  const FieldInfo*& OutField) const
{
  OutOwner = nullptr;
  OutField = nullptr;

  if (!Root || Path.empty())
  {
    return false;
  }

  void* current = Root;
  for (size_t index = 0; index < Path.size(); ++index)
  {
    const FieldPathEntry& entry = Path[index];
    const TypeInfo* ownerInfo = TypeRegistry::Instance().Find(entry.OwnerType);
    if (!ownerInfo)
    {
      return false;
    }

    const auto fieldIt = std::ranges::find_if(ownerInfo->Fields,
      [&entry](const FieldInfo& candidate) {
        return candidate.Name == entry.FieldName;
      });

    if (fieldIt == ownerInfo->Fields.end())
    {
      return false;
    }

    const FieldInfo* field = &(*fieldIt);
    if (index + 1 == Path.size())
    {
      OutOwner = current;
      OutField = field;
      return true;
    }

    void* next = nullptr;
    if (field->MutablePointer)
    {
      next = field->MutablePointer(current);
    }

    if (!next && field->ViewGetter)
    {
        if (auto view = field->ViewGetter(current))
      {
        next = view->BorrowedMutable();
        if (!next)
        {
          next = const_cast<void*>(view->Borrowed());
        }
      }
    }

    if (!next)
    {
      return false;
    }

    current = next;
  }

  return false;
}

bool UIPropertyPanel::ReadFieldValue(
  const FieldBinding& Binding,
  std::string& OutText,
  bool& OutBool) const
{
  if (!Binding.RootInstance)
  {
    return false;
  }

  void* owner = nullptr;
  const FieldInfo* field = nullptr;
  if (!ResolveLeafPath(Binding.RootInstance, Binding.Path, owner, field) || !field || !field->Getter)
  {
    return false;
  }

  if (Binding.EditorKind == EEditorKind::Enum)
  {
    const TypeInfo* enumInfo = TypeRegistry::Instance().Find(Binding.FieldType);
    if (!enumInfo || !enumInfo->IsEnum || enumInfo->Size == 0 || enumInfo->Size > sizeof(std::uint64_t) ||
        !field->ConstPointer)
    {
      return false;
    }

    std::uint64_t enumBits = 0;
    if (!ReadUnsignedBits(field->ConstPointer(owner), enumInfo->Size, enumBits))
    {
      return false;
    }

    const auto it = std::ranges::find_if(enumInfo->EnumValues,
      [enumBits](const EnumValueInfo& entry) { return entry.Value == enumBits; });

    if (it != enumInfo->EnumValues.end())
    {
      OutText = it->Name;
    }
    else if (enumInfo->EnumIsSigned)
    {
      OutText = std::to_string(SignExtend(enumBits, enumInfo->Size));
    }
    else
    {
      OutText = std::to_string(enumBits);
    }

    OutBool = false;
    return true;
  }

  const auto valueResult = field->Getter(owner);
  if (!valueResult)
  {
    return false;
  }

  const Variant& value = *valueResult;
  switch (Binding.EditorKind)
  {
  case EEditorKind::Bool:
    {
      const auto ref = value.AsConstRef<bool>();
      if (!ref)
      {
        return false;
      }
      OutBool = ref->get();
      OutText = OutBool ? "true" : "false";
      return true;
    }
  case EEditorKind::Signed:
    {
      const auto ref = value.AsConstRef<int>();
      if (!ref)
      {
        return false;
      }
      OutText = std::to_string(ref->get());
      return true;
    }
  case EEditorKind::Unsigned:
    {
      if (Binding.FieldType == StaticTypeId<unsigned int>())
      {
        const auto ref = value.AsConstRef<unsigned int>();
        if (!ref)
        {
          return false;
        }
        OutText = std::to_string(ref->get());
        return true;
      }
      if (Binding.FieldType == StaticTypeId<std::uint64_t>())
      {
        const auto ref = value.AsConstRef<std::uint64_t>();
        if (!ref)
        {
          return false;
        }
        OutText = std::to_string(ref->get());
        return true;
      }
      return false;
    }
  case EEditorKind::Float:
    {
      const auto ref = value.AsConstRef<float>();
      if (!ref)
      {
        return false;
      }
      OutText = FormatNumber(ref->get());
      return true;
    }
  case EEditorKind::Double:
    {
      const auto ref = value.AsConstRef<double>();
      if (!ref)
      {
        return false;
      }
      OutText = FormatNumber(ref->get());
      return true;
    }
  case EEditorKind::String:
    {
      const auto ref = value.AsConstRef<std::string>();
      if (!ref)
      {
        return false;
      }
      OutText = ref->get();
      return true;
    }
  case EEditorKind::Vec3:
    {
      const auto ref = value.AsConstRef<Vec3>();
      if (!ref)
      {
        return false;
      }
      OutText = FormatVec3(ref->get());
      return true;
    }
  case EEditorKind::Quat:
    {
      const auto ref = value.AsConstRef<Quat>();
      if (!ref)
      {
        return false;
      }
      OutText = FormatQuat(ref->get());
      return true;
    }
  case EEditorKind::Uuid:
    {
      const auto ref = value.AsConstRef<Uuid>();
      if (!ref)
      {
        return false;
      }
      OutText = ToString(ref->get());
      return true;
    }
  case EEditorKind::Enum:
    return false;
  case EEditorKind::Unsupported:
  default:
    return false;
  }
}

bool UIPropertyPanel::WriteFieldValue(
  const FieldBinding& Binding,
  std::string_view TextValue,
  const bool BoolValue)
{
  if (!Binding.RootInstance || Binding.ReadOnly)
  {
    return false;
  }

  void* owner = nullptr;
  const FieldInfo* field = nullptr;
  const bool requiresSetter = Binding.EditorKind != EEditorKind::Enum;
  if (!ResolveLeafPath(Binding.RootInstance, Binding.Path, owner, field) || !field || field->IsConst ||
      (requiresSetter && !field->Setter))
  {
    return false;
  }

  switch (Binding.EditorKind)
  {
  case EEditorKind::Bool:
    return static_cast<bool>(field->Setter(owner, Variant::FromValue(BoolValue)));
  case EEditorKind::Signed:
    {
      std::int64_t parsed = 0;
      if (!ParseSigned(TextValue, parsed) ||
          parsed < static_cast<std::int64_t>(std::numeric_limits<int>::min()) ||
          parsed > static_cast<std::int64_t>(std::numeric_limits<int>::max()))
      {
        return false;
      }
      return static_cast<bool>(field->Setter(owner, Variant::FromValue(static_cast<int>(parsed))));
    }
  case EEditorKind::Unsigned:
    {
      std::uint64_t parsed = 0;
      if (!ParseUnsigned(TextValue, parsed))
      {
        return false;
      }

      if (Binding.FieldType == StaticTypeId<unsigned int>())
      {
        if (parsed > static_cast<std::uint64_t>(std::numeric_limits<unsigned int>::max()))
        {
          return false;
        }
        return static_cast<bool>(
          field->Setter(owner, Variant::FromValue(static_cast<unsigned int>(parsed))));
      }

      if (Binding.FieldType == StaticTypeId<std::uint64_t>())
      {
        return static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed)));
      }
      return false;
    }
  case EEditorKind::Float:
    {
      double parsed = 0.0;
      if (!ParseDouble(TextValue, parsed) ||
          !std::isfinite(parsed) ||
          parsed < static_cast<double>(-std::numeric_limits<float>::max()) ||
          parsed > static_cast<double>(std::numeric_limits<float>::max()))
      {
        return false;
      }
      return static_cast<bool>(field->Setter(owner, Variant::FromValue(static_cast<float>(parsed))));
    }
  case EEditorKind::Double:
    {
      double parsed = 0.0;
      if (!ParseDouble(TextValue, parsed) || !std::isfinite(parsed))
      {
        return false;
      }
      return static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed)));
    }
  case EEditorKind::String:
    return static_cast<bool>(field->Setter(owner, Variant::FromValue(std::string(TextValue))));
  case EEditorKind::Vec3:
    {
      Vec3 parsed{};
      if (!ParseVec3(TextValue, parsed))
      {
        return false;
      }
      return static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed)));
    }
  case EEditorKind::Quat:
    {
      Quat parsed = Quat::Identity();
      if (!ParseQuat(TextValue, parsed))
      {
        return false;
      }
      return static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed)));
    }
  case EEditorKind::Uuid:
    {
      Uuid parsed{};
      if (!ParseUuid(TextValue, parsed))
      {
        return false;
      }
      return static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed)));
    }
  case EEditorKind::Enum:
    {
      const TypeInfo* enumInfo = TypeRegistry::Instance().Find(Binding.FieldType);
      if (!enumInfo || !enumInfo->IsEnum || enumInfo->Size == 0 || enumInfo->Size > sizeof(std::uint64_t) ||
          !field->MutablePointer)
      {
        return false;
      }

      const std::string trimmed = TrimCopy(TextValue);
      if (trimmed.empty())
      {
        return false;
      }

      std::uint64_t enumBits = 0;
      bool resolved = false;

      for (const EnumValueInfo& entry : enumInfo->EnumValues)
      {
        if (EqualsIgnoreCase(entry.Name, trimmed))
        {
          enumBits = entry.Value;
          resolved = true;
          break;
        }

        if (const size_t sep = entry.Name.rfind("::"); sep != std::string::npos)
        {
          const std::string_view shortName(entry.Name.c_str() + sep + 2, entry.Name.size() - (sep + 2));
          if (EqualsIgnoreCase(shortName, trimmed))
          {
            enumBits = entry.Value;
            resolved = true;
            break;
          }
        }
      }

      if (!resolved)
      {
        if (enumInfo->EnumIsSigned)
        {
            if (std::int64_t parsed = 0;
                !ParseSigned(trimmed, parsed) || !ConvertSignedToBits(parsed, enumInfo->Size, enumBits))
          {
            return false;
          }
        }
        else
        {
            if (std::uint64_t parsed = 0;
                !ParseUnsigned(trimmed, parsed) || !ConvertUnsignedToBits(parsed, enumInfo->Size, enumBits))
          {
            return false;
          }
        }
      }

      void* destination = field->MutablePointer(owner);
      return WriteUnsignedBits(destination, enumInfo->Size, enumBits);
    }
  case EEditorKind::Unsupported:
  default:
    return false;
  }
}

bool UIPropertyPanel::ParseBool(std::string_view Text, bool& OutValue) const
{
  const std::string normalized = ToLowerCopy(TrimCopy(Text));
  if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
  {
    OutValue = true;
    return true;
  }
  if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
  {
    OutValue = false;
    return true;
  }
  return false;
}

bool UIPropertyPanel::ParseSigned(std::string_view Text, std::int64_t& OutValue) const
{
  const std::string trimmed = TrimCopy(Text);
  if (trimmed.empty())
  {
    return false;
  }

  char* end = nullptr;
  const long long parsed = std::strtoll(trimmed.c_str(), &end, 10);
  if (end == trimmed.c_str() || *end != '\0')
  {
    return false;
  }

  OutValue = static_cast<std::int64_t>(parsed);
  return true;
}

bool UIPropertyPanel::ParseUnsigned(std::string_view Text, std::uint64_t& OutValue) const
{
  const std::string trimmed = TrimCopy(Text);
  if (trimmed.empty())
  {
    return false;
  }

  char* end = nullptr;
  const unsigned long long parsed = std::strtoull(trimmed.c_str(), &end, 10);
  if (end == trimmed.c_str() || *end != '\0')
  {
    return false;
  }

  OutValue = static_cast<std::uint64_t>(parsed);
  return true;
}

bool UIPropertyPanel::ParseDouble(std::string_view Text, double& OutValue) const
{
  const std::string trimmed = TrimCopy(Text);
  if (trimmed.empty())
  {
    return false;
  }

  char* end = nullptr;
  const double parsed = std::strtod(trimmed.c_str(), &end);
  if (end == trimmed.c_str() || *end != '\0')
  {
    return false;
  }

  OutValue = parsed;
  return true;
}

bool UIPropertyPanel::ParseVec3(std::string_view Text, Vec3& OutValue) const
{
  std::array<double, 3> components{};
  if (!ParseComponentList(Text, components.data(), components.size()))
  {
    return false;
  }

  OutValue.x() = static_cast<Scalar>(components[0]);
  OutValue.y() = static_cast<Scalar>(components[1]);
  OutValue.z() = static_cast<Scalar>(components[2]);
  return true;
}

bool UIPropertyPanel::ParseQuat(std::string_view Text, Quat& OutValue) const
{
  std::array<double, 4> components{};
  if (!ParseComponentList(Text, components.data(), components.size()))
  {
    return false;
  }

  OutValue.x() = static_cast<Scalar>(components[0]);
  OutValue.y() = static_cast<Scalar>(components[1]);
  OutValue.z() = static_cast<Scalar>(components[2]);
  OutValue.w() = static_cast<Scalar>(components[3]);
  return true;
}

bool UIPropertyPanel::ParseUuid(std::string_view Text, Uuid& OutValue) const
{
  const auto parsed = uuids::uuid::from_string(TrimCopy(Text));
  if (!parsed)
  {
    return false;
  }

  OutValue = *parsed;
  return true;
}

std::string UIPropertyPanel::FormatVec3(const Vec3& Value) const
{
  return FormatNumber(static_cast<double>(Value.x())) + ", " +
         FormatNumber(static_cast<double>(Value.y())) + ", " +
         FormatNumber(static_cast<double>(Value.z()));
}

std::string UIPropertyPanel::FormatQuat(const Quat& Value) const
{
  return FormatNumber(static_cast<double>(Value.x())) + ", " +
         FormatNumber(static_cast<double>(Value.y())) + ", " +
         FormatNumber(static_cast<double>(Value.z())) + ", " +
         FormatNumber(static_cast<double>(Value.w()));
}

void UIPropertyPanel::SyncModelToEditors()
{
  if (!m_Context || !m_BoundInstance)
  {
    return;
  }

  for (FieldBinding& binding : m_Bindings)
  {
    if (binding.EditorId.Value == 0)
    {
      continue;
    }

    std::string textValue{};
    bool boolValue = false;
    if (!ReadFieldValue(binding, textValue, boolValue))
    {
      continue;
    }

    SnAPI::UI::IUIElement& element = m_Context->GetElement(binding.EditorId);
    if (binding.EditorKind == EEditorKind::Bool && !binding.ReadOnly)
    {
      auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&element);
      if (!checkbox)
      {
        continue;
      }

      if (checkbox->Properties().GetPropertyOr(SnAPI::UI::UICheckbox::CheckedKey, false) != boolValue)
      {
        checkbox->Checked().Set(boolValue);
      }
      binding.LastBool = boolValue;
      continue;
    }

    if (binding.EditorKind == EEditorKind::Enum)
    {
      auto* comboBox = dynamic_cast<SnAPI::UI::UIComboBox*>(&element);
      if (!comboBox)
      {
        continue;
      }

      // Preserve in-progress user interaction.
      if (comboBox->IsFocused())
      {
        continue;
      }

      if (!comboBox->SelectByText(textValue, false))
      {
        comboBox->SetSelectedIndex(-1, false);
      }
      binding.LastText = textValue;
      continue;
    }

    auto* textInput = dynamic_cast<SnAPI::UI::UITextInput*>(&element);
    if (!textInput)
    {
      continue;
    }

    // Preserve in-progress user edits.
    if (textInput->IsFocused())
    {
      continue;
    }

    if (textInput->Properties().GetPropertyOr(SnAPI::UI::UITextInput::TextKey, std::string{}) != textValue)
    {
      textInput->Text().Set(textValue);
    }
    binding.LastText = textValue;
  }
}

void UIPropertyPanel::SyncEditorsToModel()
{
  if (!m_Context || !m_BoundInstance)
  {
    return;
  }

  for (FieldBinding& binding : m_Bindings)
  {
    if (binding.EditorId.Value == 0 || binding.ReadOnly)
    {
      continue;
    }

    SnAPI::UI::IUIElement& element = m_Context->GetElement(binding.EditorId);

    if (binding.EditorKind == EEditorKind::Bool)
    {
      auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&element);
      if (!checkbox)
      {
        continue;
      }

      const bool currentValue = checkbox->Properties().GetPropertyOr(SnAPI::UI::UICheckbox::CheckedKey, false);
      if (currentValue == binding.LastBool)
      {
        continue;
      }

      if (WriteFieldValue(binding, {}, currentValue))
      {
        binding.LastBool = currentValue;
      }
      continue;
    }

    if (binding.EditorKind == EEditorKind::Enum)
    {
      auto* comboBox = dynamic_cast<SnAPI::UI::UIComboBox*>(&element);
      if (!comboBox)
      {
        continue;
      }

      const std::string currentText = comboBox->SelectedText();
      if (currentText.empty() || currentText == binding.LastText)
      {
        continue;
      }

      if (WriteFieldValue(binding, currentText, false))
      {
        binding.LastText = currentText;
        continue;
      }

      if (!comboBox->IsFocused())
      {
        std::string modelText{};
        bool modelBool = false;
        if (ReadFieldValue(binding, modelText, modelBool))
        {
          if (!comboBox->SelectByText(modelText, false))
          {
            comboBox->SetSelectedIndex(-1, false);
          }
          binding.LastText = modelText;
        }
      }
      continue;
    }

    auto* textInput = dynamic_cast<SnAPI::UI::UITextInput*>(&element);
    if (!textInput)
    {
      continue;
    }

    const std::string currentText = textInput->Properties().GetPropertyOr(SnAPI::UI::UITextInput::TextKey, std::string{});
    if (currentText == binding.LastText)
    {
      continue;
    }

    if (WriteFieldValue(binding, currentText, false))
    {
      binding.LastText = currentText;
      continue;
    }

    if (!textInput->IsFocused())
    {
      std::string modelText{};
      bool modelBool = false;
      if (ReadFieldValue(binding, modelText, modelBool))
      {
        textInput->Text().Set(modelText);
        binding.LastText = modelText;
      }
    }
  }
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
