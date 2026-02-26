#include "UIPropertyPanel.h"

#if defined(SNAPI_GF_ENABLE_UI)

#include "AssetRef.h"
#include "BaseNode.h"
#include "BuiltinTypes.h"
#include "IWorld.h"
#include "PawnBase.h"
#include "SubClassOf.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <UIContext.h>
#include <UICheckbox.h>
#include <UIComboBox.h>
#include <UIElementBase.h>
#include <UIEvents.h>
#include <UINumberField.h>
#include <UIPanel.h>
#include <UISizing.h>
#include <UIText.h>
#include <UITextInput.h>

namespace SnAPI::GameFramework
{
namespace
{
constexpr int kMaxReflectionDepth = 8;
constexpr float kLabelLaneRatio = 1.0f;
constexpr float kValueLaneRatio = 1.8f;
constexpr float kRowPadding = 4.0f;
constexpr float kAxisTagWidth = 12.0f;
constexpr float kVectorGap = 2.0f;

using SnAPI::UI::Color;

constexpr Color kCardBackground{16, 18, 23, 236};
constexpr Color kCardBorder{52, 58, 70, 226};
constexpr Color kRowBackground{19, 22, 30, 246};
constexpr Color kRowBorder{50, 56, 68, 232};
constexpr Color kLabelColor{170, 178, 192, 255};
constexpr Color kValueBg{12, 15, 21, 252};
constexpr Color kValueBorder{58, 65, 80, 236};
constexpr Color kValueBorderFocused{114, 122, 138, 242};
constexpr Color kAxisX{232, 110, 90, 255};
constexpr Color kAxisY{118, 208, 142, 255};
constexpr Color kAxisZ{116, 156, 235, 255};
constexpr Color kAxisW{220, 196, 138, 255};
constexpr std::string_view kAssetRefNoneOption = "<None>";

[[nodiscard]] Color AxisTintForIndex(const size_t Index)
{
  switch (Index)
  {
  case 0:
    return kAxisX;
  case 1:
    return kAxisY;
  case 2:
    return kAxisZ;
  default:
    return kAxisW;
  }
}

[[nodiscard]] const char* AxisLabelForIndex(const size_t Index)
{
  switch (Index)
  {
  case 0:
    return "X";
  case 1:
    return "Y";
  case 2:
    return "Z";
  default:
    return "W";
  }
}

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

[[nodiscard]] std::string FormatComponentValues(const std::span<const double> Values)
{
  std::string out;
  out.reserve(Values.size() * 10);
  for (size_t index = 0; index < Values.size(); ++index)
  {
    if (index > 0)
    {
      out += ", ";
    }
    out += FormatNumber(Values[index]);
  }
  return out;
}

[[nodiscard]] bool NearlyEqual(const double A, const double B, const double Epsilon = 1e-6)
{
  return std::fabs(A - B) <= Epsilon;
}

struct ScopedFlag
{
  explicit ScopedFlag(bool& FlagRef)
    : Flag(FlagRef)
  {
    Flag = true;
  }

  ~ScopedFlag()
  {
    Flag = false;
  }

  bool& Flag;
};

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
  Padding().SetDefault(4.0f);
  Gap().SetDefault(4.0f);
  ShowHorizontalScrollbar().SetDefault(false);
  ShowVerticalScrollbar().SetDefault(true);
  Smooth().SetDefault(true);
  ScrollbarThickness().SetDefault(7.0f);
  BackgroundColor().SetDefault(kCardBackground);
  BorderColor().SetDefault(kCardBorder);
  BorderThickness().SetDefault(1.0f);
  CornerRadius().SetDefault(4.0f);
  ScrollbarTrackColor().SetDefault(Color{12, 14, 19, 230});
  ScrollbarThumbColor().SetDefault(Color{67, 74, 88, 242});
  ScrollbarThumbHoverColor().SetDefault(Color{102, 110, 126, 246});
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
    .Heading = PrettyTypeName(Type),
    .ComponentOwner = {},
    .IsComponent = false});

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
    .Heading = std::move(nodeHeading),
    .ComponentOwner = {},
    .IsComponent = false});

  NodeHandle ComponentOwner = Node->Handle();
  if (ComponentOwner.IsNull() || ComponentOwner.Borrowed() == nullptr)
  {
    if (auto* WorldRef = Node->World())
    {
      const auto FreshOwnerHandle = WorldRef->NodeHandleById(Node->Id());
      if (FreshOwnerHandle)
      {
        ComponentOwner = *FreshOwnerHandle;
      }
    }
  }

  const auto& ComponentTypes = Node->ComponentTypes();
  for (const TypeId& ComponentType : ComponentTypes)
  {
    void* ComponentInstance = nullptr;
    if (auto* WorldRef = Node->World())
    {
      ComponentInstance = WorldRef->BorrowedComponent(ComponentOwner, ComponentType);
      if (!ComponentInstance)
      {
        // UUID-resolved fallback keeps inspector usable when runtime key fields drift.
        ComponentInstance = WorldRef->BorrowedComponent(NodeHandle{Node->Id()}, ComponentType);
      }
    }
    if (!ComponentInstance)
    {
      continue;
    }

    m_BoundSections.push_back(BoundSection{
      .Type = ComponentType,
      .Instance = ComponentInstance,
      .Heading = PrettyTypeName(ComponentType),
      .ComponentOwner = ComponentOwner,
      .IsComponent = true});
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
  ++m_BindingGeneration;
  ClearBindingHooks();

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

void UIPropertyPanel::SetComponentContextMenuHandler(
  SnAPI::UI::TDelegate<void(NodeHandle, const TypeId&, const SnAPI::UI::PointerEvent&)> Handler)
{
  m_OnComponentContextMenuRequested = std::move(Handler);
}

void UIPropertyPanel::OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context)
{
  UIScrollContainer::OnRoutedEvent(Context);
}

void UIPropertyPanel::Paint(SnAPI::UI::UIPaintContext& Context) const
{
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
  ++m_BindingGeneration;
  ClearBindingHooks();
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
    contentPanel->Padding().Set(2.0f);
    contentPanel->Gap().Set(4.0f);
    contentPanel->Width().Set(SnAPI::UI::Sizing::Fill());
    contentPanel->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
    contentPanel->Background().Set(Color::Transparent());
    contentPanel->BorderColor().Set(Color::Transparent());
    contentPanel->BorderThickness().Set(0.0f);
    contentPanel->CornerRadius().Set(0.0f);
  }

  if (m_BoundSections.empty())
  {
    m_BoundSections.push_back(BoundSection{
      .Type = m_BoundType,
      .Instance = m_BoundInstance,
      .Heading = PrettyTypeName(m_BoundType),
      .ComponentOwner = {},
      .IsComponent = false});
  }

  if (m_BoundSections.size() == 1)
  {
    const BoundSection& Section = m_BoundSections.front();
    BuildTypeIntoContainer(m_ContentRoot, Section.Type, Section.Instance, {}, 0);
  }
  else
  {
    for (const BoundSection& Section : m_BoundSections)
    {
      if (!Section.Instance)
      {
        continue;
      }

      const auto accordionHandle = m_Context->CreateElement<SnAPI::UI::UIAccordion>();
      if (accordionHandle.Id.Value == 0)
      {
        continue;
      }
      m_Context->AddChild(m_ContentRoot, accordionHandle.Id);

      auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id));
      if (accordion)
      {
        accordion->Width().Set(SnAPI::UI::Sizing::Fill());
        accordion->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
        accordion->AllowMultipleExpanded().Set(true);
        accordion->DefaultExpanded().Set(true);
        accordion->Gap().Set(4.0f);
        accordion->Padding().Set(0.0f);
        accordion->HeaderHeight().Set(28.0f);
        accordion->HeaderPadding().Set(9.0f);
        accordion->ContentPadding().Set(5.0f);
        accordion->BackgroundColor().Set(kCardBackground);
        accordion->BorderColor().Set(kCardBorder);
        accordion->BorderThickness().Set(1.0f);
        accordion->CornerRadius().Set(4.0f);
        accordion->HeaderColor().Set(Color{48, 55, 68, 246});
        accordion->HeaderHoverColor().Set(Color{58, 66, 80, 248});
        accordion->HeaderExpandedColor().Set(Color{56, 64, 78, 248});
        accordion->HeaderTextColor().Set(Color{214, 220, 231, 255});
        accordion->HeaderTextExpandedColor().Set(Color{230, 234, 241, 255});
        accordion->HeaderBorderColor().Set(kCardBorder);
        accordion->HeaderBorderThickness().Set(1.0f);
        accordion->ArrowSize().Set(18.0f);
        accordion->ArrowGap().Set(6.0f);
        if (Section.IsComponent && !Section.ComponentOwner.IsNull())
        {
          const NodeHandle ComponentOwner = Section.ComponentOwner;
          const TypeId ComponentType = Section.Type;
          accordion->OnSectionContextMenuRequested(
            [this, ComponentOwner, ComponentType](
              const int32_t, const SnAPI::UI::ElementId, const SnAPI::UI::PointerEvent& Event) {
              if (m_OnComponentContextMenuRequested)
              {
                m_OnComponentContextMenuRequested(ComponentOwner, ComponentType, Event);
              }
            });
        }
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
        body->Padding().Set(2.0f);
        body->Gap().Set(4.0f);
        body->Width().Set(SnAPI::UI::Sizing::Fill());
        body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
        body->Background().Set(Color::Transparent());
        body->BorderColor().Set(Color::Transparent());
        body->BorderThickness().Set(0.0f);
      }

      if (accordion)
      {
        std::string heading = Section.Heading.empty() ? PrettyTypeName(Section.Type) : Section.Heading;
        accordion->SetSectionHeading(bodyHandle.Id, std::move(heading));
        accordion->SetSectionExpanded(bodyHandle.Id, true);
      }

      BuildTypeIntoContainer(bodyHandle.Id, Section.Type, Section.Instance, {}, 0);
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
    const std::string nestedTitle = PrettyFieldName(Field.Name);
    const bool flattenSettings = EqualsIgnoreCase(nestedTitle, "Settings");
    if (flattenSettings)
    {
      const auto bodyHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.SettingsBody");
      if (bodyHandle.Id.Value == 0)
      {
        AddUnsupportedRow(Parent, Field.Name, "Failed to create nested body");
        return;
      }

      m_Context->AddChild(Parent, bodyHandle.Id);
      if (auto* body = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(bodyHandle.Id)))
      {
        body->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        body->Padding().Set(2.0f);
        body->Gap().Set(4.0f);
        body->Width().Set(SnAPI::UI::Sizing::Fill());
        body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
        body->Background().Set(Color::Transparent());
        body->BorderColor().Set(Color::Transparent());
        body->BorderThickness().Set(0.0f);
      }

      BuildTypeIntoContainer(bodyHandle.Id, Field.FieldType, RootInstance, std::move(Path), Depth + 1);
      return;
    }

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
      accordion->Padding().Set(0.0f);
      accordion->HeaderHeight().Set(26.0f);
      accordion->HeaderPadding().Set(8.0f);
      accordion->ContentPadding().Set(5.0f);
      accordion->BackgroundColor().Set(kCardBackground);
      accordion->BorderColor().Set(kCardBorder);
      accordion->BorderThickness().Set(1.0f);
      accordion->CornerRadius().Set(4.0f);
      accordion->HeaderColor().Set(Color{45, 51, 63, 246});
      accordion->HeaderHoverColor().Set(Color{56, 63, 77, 248});
      accordion->HeaderExpandedColor().Set(Color{54, 61, 74, 248});
      accordion->HeaderTextColor().Set(Color{205, 212, 223, 255});
      accordion->HeaderTextExpandedColor().Set(Color{225, 229, 236, 255});
      accordion->HeaderBorderColor().Set(kCardBorder);
      accordion->HeaderBorderThickness().Set(1.0f);
      accordion->ArrowSize().Set(18.0f);
      accordion->ArrowGap().Set(6.0f);
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
      body->Padding().Set(2.0f);
      body->Gap().Set(4.0f);
      body->Width().Set(SnAPI::UI::Sizing::Fill());
      body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
      body->Background().Set(Color::Transparent());
      body->BorderColor().Set(Color::Transparent());
      body->BorderThickness().Set(0.0f);
    }

    if (auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id)))
    {
      std::string heading = nestedTitle;
      if (heading.empty())
      {
        heading = PrettyTypeName(Field.FieldType);
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
    row->Padding().Set(kRowPadding);
    row->Gap().Set(8.0f);
    row->Width().Set(SnAPI::UI::Sizing::Fill());
    row->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
    row->Background().Set(kRowBackground);
    row->BorderColor().Set(kRowBorder);
    row->BorderThickness().Set(1.0f);
    row->CornerRadius().Set(4.0f);
  }

  const std::string fieldLabel = PrettyFieldName(Field.Name);
  const auto labelHandle = m_Context->CreateElement<SnAPI::UI::UIText>(fieldLabel);
  if (labelHandle.Id.Value != 0)
  {
    m_Context->AddChild(rowHandle.Id, labelHandle.Id);
    if (auto* label = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(labelHandle.Id)))
    {
      label->Width().Set(SnAPI::UI::Sizing::Ratio(kLabelLaneRatio));
      label->HAlign().Set(SnAPI::UI::EAlignment::Start);
      label->VAlign().Set(SnAPI::UI::EAlignment::Center);
      label->TextAlignment().Set(SnAPI::UI::ETextAlignment::Start);
      label->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
      label->TextColor().Set(kLabelColor);
    }
  }

  const auto valueHostHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.ValueHost");
  if (valueHostHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(rowHandle.Id, valueHostHandle.Id);
  if (auto* valueHost = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(valueHostHandle.Id)))
  {
    valueHost->Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    valueHost->Padding().Set(0.0f);
    valueHost->Gap().Set(kVectorGap);
    valueHost->Width().Set(SnAPI::UI::Sizing::Ratio(kValueLaneRatio));
    valueHost->HAlign().Set(SnAPI::UI::EAlignment::End);
    valueHost->VAlign().Set(SnAPI::UI::EAlignment::Center);
    valueHost->Background().Set(Color::Transparent());
    valueHost->BorderColor().Set(Color::Transparent());
    valueHost->BorderThickness().Set(0.0f);
  }

  const EEditorKind editorKind = ResolveEditorKind(Field.FieldType);
  const bool readOnly = pathConst || Field.IsConst || !Field.Setter;

  if (editorKind == EEditorKind::Unsupported)
  {
    const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>(
      "<unsupported: " + PrettyTypeName(Field.FieldType) + ">");
    if (textHandle.Id.Value != 0)
    {
      m_Context->AddChild(valueHostHandle.Id, textHandle.Id);
      if (auto* text = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(textHandle.Id)))
      {
        text->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        text->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
        text->TextColor().Set(Color{152, 160, 176, 255});
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
  binding.Generation = m_BindingGeneration;

  if (editorKind == EEditorKind::Bool && !readOnly)
  {
    const auto checkboxHandle = m_Context->CreateElement<SnAPI::UI::UICheckbox>();
    if (checkboxHandle.Id.Value == 0)
    {
      return;
    }
    m_Context->AddChild(valueHostHandle.Id, checkboxHandle.Id);
    binding.EditorId = checkboxHandle.Id;

    if (auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(checkboxHandle.Id)))
    {
      checkbox->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
      checkbox->HAlign().Set(SnAPI::UI::EAlignment::End);
      checkbox->VAlign().Set(SnAPI::UI::EAlignment::Center);
      checkbox->Label().Set(std::string{});
      checkbox->BoxSize().Set(12.0f);
      checkbox->CheckInset().Set(2.0f);
      checkbox->ElementStyle().Apply("editor.checkbox");
    }
  }
  else if (editorKind == EEditorKind::Enum ||
           editorKind == EEditorKind::SubClass ||
           editorKind == EEditorKind::AssetRef)
  {
    const auto comboHandle = m_Context->CreateElement<SnAPI::UI::UIComboBox>();
    if (comboHandle.Id.Value == 0)
    {
      return;
    }
    m_Context->AddChild(valueHostHandle.Id, comboHandle.Id);
    binding.EditorId = comboHandle.Id;

    if (auto* combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(comboHandle.Id)))
    {
      combo->ReadOnly().Set(readOnly);
      combo->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
      combo->Height().Set(SnAPI::UI::Sizing::Auto());
      combo->VAlign().Set(SnAPI::UI::EAlignment::Center);
      combo->MaxDropdownHeight().Set(220.0f);
      combo->Placeholder().Set(PrettyTypeName(Field.FieldType));
      combo->BackgroundColor().Set(kValueBg);
      combo->HoverColor().Set(Color{26, 30, 39, 252});
      combo->PressedColor().Set(Color{31, 36, 46, 252});
      combo->BorderColor().Set(kValueBorder);
      combo->BorderThickness().Set(1.0f);
      combo->CornerRadius().Set(3.0f);
      combo->TextColor().Set(Color{224, 229, 237, 255});
      combo->PlaceholderColor().Set(Color{138, 147, 163, 255});
      combo->ArrowColor().Set(Color{185, 193, 205, 255});
      combo->Padding().Set(6.0f);
      combo->RowHeight().Set(24.0f);

      std::vector<std::string> options;
      if (editorKind == EEditorKind::Enum)
      {
        if (const TypeInfo* enumInfo = TypeRegistry::Instance().Find(Field.FieldType);
            enumInfo && enumInfo->IsEnum)
        {
          options.reserve(enumInfo->EnumValues.size());
          for (const EnumValueInfo& enumValue : enumInfo->EnumValues)
          {
            options.push_back(enumValue.Name);
          }
        }
      }
      else if (editorKind == EEditorKind::SubClass && Field.FieldType == StaticTypeId<TSubClassOf<PawnBase>>())
      {
        const auto entries = TSubClassOf<PawnBase>::EnumerateTypes();
        options.reserve(entries.size());
        for (const auto& entry : entries)
        {
          options.push_back(entry.Name);
        }
      }
      else if (editorKind == EEditorKind::AssetRef && Field.FieldType == StaticTypeId<TAssetRef<PawnBase>>())
      {
        const auto entries = TAssetRef<PawnBase>::EnumerateCompatibleAssets();
        options.reserve(entries.size() + 1);
        options.emplace_back(kAssetRefNoneOption);
        for (const auto& entry : entries)
        {
          options.push_back(entry.Label);
        }
      }
      combo->SetItems(std::move(options));
    }
  }
  else if ((editorKind == EEditorKind::Signed || editorKind == EEditorKind::Unsigned ||
            editorKind == EEditorKind::Float || editorKind == EEditorKind::Double) && !readOnly)
  {
    const auto numberHandle = m_Context->CreateElement<SnAPI::UI::UINumberField>();
    if (numberHandle.Id.Value == 0)
    {
      return;
    }
    m_Context->AddChild(valueHostHandle.Id, numberHandle.Id);
    binding.EditorId = numberHandle.Id;
    binding.ComponentEditorIds[0] = numberHandle.Id;
    binding.ComponentCount = 1;

    if (auto* numberField = dynamic_cast<SnAPI::UI::UINumberField*>(&m_Context->GetElement(numberHandle.Id)))
    {
      numberField->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
      numberField->Height().Set(SnAPI::UI::Sizing::Auto());
      numberField->HAlign().Set(SnAPI::UI::EAlignment::Center);
      numberField->VAlign().Set(SnAPI::UI::EAlignment::Center);
      numberField->ShowSpinButtons().Set(true);
      numberField->AllowTextInput().Set(true);
      numberField->Padding().Set(3.0f);
      numberField->BorderThickness().Set(1.0f);
      numberField->CornerRadius().Set(3.0f);
      numberField->BackgroundColor().Set(kValueBg);
      numberField->HoverColor().Set(Color{26, 30, 39, 252});
      numberField->PressedColor().Set(Color{31, 36, 46, 252});
      numberField->BorderColor().Set(kValueBorder);
      numberField->TextColor().Set(Color{224, 229, 237, 255});
      numberField->CaretColor().Set(kValueBorderFocused);
      numberField->SpinButtonColor().Set(Color{23, 27, 35, 252});
      numberField->SpinButtonHoverColor().Set(Color{31, 36, 46, 252});

      if (editorKind == EEditorKind::Signed)
      {
        numberField->Precision().Set(0u);
        numberField->Step().Set(1.0);
        numberField->MinValue().Set(static_cast<double>(std::numeric_limits<int>::min()));
        numberField->MaxValue().Set(static_cast<double>(std::numeric_limits<int>::max()));
      }
      else if (editorKind == EEditorKind::Unsigned)
      {
        numberField->Precision().Set(0u);
        numberField->Step().Set(1.0);
        numberField->MinValue().Set(0.0);
        if (binding.FieldType == StaticTypeId<unsigned int>())
        {
          numberField->MaxValue().Set(static_cast<double>(std::numeric_limits<unsigned int>::max()));
        }
        else
        {
          numberField->MaxValue().Set(static_cast<double>(std::numeric_limits<std::uint64_t>::max()));
        }
      }
      else if (editorKind == EEditorKind::Float)
      {
        numberField->Precision().Set(3u);
        numberField->Step().Set(0.01);
        numberField->MinValue().Set(-static_cast<double>(std::numeric_limits<float>::max()));
        numberField->MaxValue().Set(static_cast<double>(std::numeric_limits<float>::max()));
      }
      else
      {
        numberField->Precision().Set(4u);
        numberField->Step().Set(0.01);
        numberField->MinValue().Set(-std::numeric_limits<double>::max());
        numberField->MaxValue().Set(std::numeric_limits<double>::max());
      }
    }
  }
  else if ((editorKind == EEditorKind::Vec2 || editorKind == EEditorKind::Vec3 ||
            editorKind == EEditorKind::Vec4 || editorKind == EEditorKind::Quat ||
            editorKind == EEditorKind::Color) && !readOnly)
  {
    const std::uint8_t componentCount =
      (editorKind == EEditorKind::Vec2) ? 2 :
      ((editorKind == EEditorKind::Vec3) ? 3 : 4);
    binding.ComponentCount = componentCount;

    for (std::uint8_t componentIndex = 0; componentIndex < componentCount; ++componentIndex)
    {
      const auto chipHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.VectorChip");
      if (chipHandle.Id.Value == 0)
      {
        continue;
      }
      m_Context->AddChild(valueHostHandle.Id, chipHandle.Id);
      if (auto* chip = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(chipHandle.Id)))
      {
        chip->Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
        chip->Padding().Set(1.0f);
        chip->Gap().Set(2.0f);
        chip->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        chip->Height().Set(SnAPI::UI::Sizing::Auto());
        chip->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
        chip->VAlign().Set(SnAPI::UI::EAlignment::Center);
        chip->Background().Set(kValueBg);
        chip->BorderColor().Set(kValueBorder);
        chip->BorderThickness().Set(1.0f);
        chip->CornerRadius().Set(3.0f);
      }

      const char* axisLabel = AxisLabelForIndex(componentIndex);
      if (editorKind == EEditorKind::Color)
      {
        switch (componentIndex)
        {
        case 0:
          axisLabel = "R";
          break;
        case 1:
          axisLabel = "G";
          break;
        case 2:
          axisLabel = "B";
          break;
        default:
          axisLabel = "A";
          break;
        }
      }

      const auto axisHandle = m_Context->CreateElement<SnAPI::UI::UIText>(axisLabel);
      if (axisHandle.Id.Value != 0)
      {
        m_Context->AddChild(chipHandle.Id, axisHandle.Id);
        if (auto* axisText = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(axisHandle.Id)))
        {
          axisText->Width().Set(SnAPI::UI::Sizing::Auto());
          axisText->HAlign().Set(SnAPI::UI::EAlignment::Center);
          axisText->VAlign().Set(SnAPI::UI::EAlignment::Center);
          axisText->TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
          axisText->Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
          axisText->TextColor().Set(AxisTintForIndex(componentIndex));
        }
      }

      const auto numberHandle = m_Context->CreateElement<SnAPI::UI::UINumberField>();
      if (numberHandle.Id.Value == 0)
      {
        continue;
      }
      m_Context->AddChild(chipHandle.Id, numberHandle.Id);
      if (binding.EditorId.Value == 0)
      {
        binding.EditorId = numberHandle.Id;
      }
      binding.ComponentEditorIds[componentIndex] = numberHandle.Id;

      if (auto* numberField = dynamic_cast<SnAPI::UI::UINumberField*>(&m_Context->GetElement(numberHandle.Id)))
      {
        numberField->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        numberField->Height().Set(SnAPI::UI::Sizing::Auto());
        numberField->HAlign().Set(SnAPI::UI::EAlignment::Center);
        numberField->VAlign().Set(SnAPI::UI::EAlignment::Center);
        numberField->ShowSpinButtons().Set(true);
        numberField->AllowTextInput().Set(true);
        numberField->Padding().Set(2.0f);
        numberField->BorderThickness().Set(0.0f);
        numberField->CornerRadius().Set(0.0f);
        numberField->BackgroundColor().Set(Color::Transparent());
        numberField->HoverColor().Set(Color::Transparent());
        numberField->PressedColor().Set(Color::Transparent());
        numberField->BorderColor().Set(Color::Transparent());
        numberField->TextColor().Set(Color{224, 229, 237, 255});
        numberField->CaretColor().Set(kValueBorderFocused);
          numberField->SpinButtonColor().Set(Color{23, 27, 35, 252});
          numberField->SpinButtonHoverColor().Set(Color{31, 36, 46, 252});

        if (editorKind == EEditorKind::Color)
        {
          numberField->Precision().Set(0u);
          numberField->Step().Set(1.0);
          numberField->MinValue().Set(0.0);
          numberField->MaxValue().Set(255.0);
        }
        else
        {
          numberField->Precision().Set(3u);
          numberField->Step().Set(0.01);
          numberField->MinValue().Set(-std::numeric_limits<double>::max());
          numberField->MaxValue().Set(std::numeric_limits<double>::max());
        }
      }
    }
  }
  else
  {
    const auto editorHandle = m_Context->CreateElement<SnAPI::UI::UITextInput>();
    if (editorHandle.Id.Value == 0)
    {
      return;
    }
    m_Context->AddChild(valueHostHandle.Id, editorHandle.Id);
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
      editor->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
      editor->Height().Set(SnAPI::UI::Sizing::Auto());
      editor->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
      editor->VAlign().Set(SnAPI::UI::EAlignment::Center);
      editor->Padding().Set(6.0f);
      editor->BorderThickness().Set(1.0f);
      editor->CornerRadius().Set(3.0f);
      editor->BackgroundColor().Set(kValueBg);
      editor->BorderColor().Set(kValueBorder);
      editor->TextColor().Set(Color{224, 229, 237, 255});
      editor->PlaceholderColor().Set(Color{138, 147, 163, 255});
      editor->SelectionColor().Set(Color{74, 90, 122, 194});
      editor->CaretColor().Set(kValueBorderFocused);
      editor->Placeholder().Set(PrettyTypeName(Field.FieldType));
    }
  }

  const std::size_t bindingIndex = m_Bindings.size();
  m_Bindings.push_back(std::move(binding));
  AttachEditorHooks(bindingIndex);
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
    row->Padding().Set(kRowPadding);
    row->Gap().Set(8.0f);
    row->Width().Set(SnAPI::UI::Sizing::Fill());
    row->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
    row->Background().Set(Color{24, 20, 24, 246});
    row->BorderColor().Set(Color{86, 66, 78, 236});
    row->BorderThickness().Set(1.0f);
    row->CornerRadius().Set(4.0f);
  }

  const auto labelHandle = m_Context->CreateElement<SnAPI::UI::UIText>(std::string(Label));
  if (labelHandle.Id.Value != 0)
  {
    m_Context->AddChild(rowHandle.Id, labelHandle.Id);
    if (auto* label = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(labelHandle.Id)))
    {
      label->Width().Set(SnAPI::UI::Sizing::Ratio(kLabelLaneRatio));
      label->TextColor().Set(Color{212, 170, 182, 255});
      label->TextAlignment().Set(SnAPI::UI::ETextAlignment::Start);
      label->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    }
  }

  const auto reasonHandle = m_Context->CreateElement<SnAPI::UI::UIText>(std::string(Reason));
  if (reasonHandle.Id.Value != 0)
  {
    m_Context->AddChild(rowHandle.Id, reasonHandle.Id);
    if (auto* reason = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(reasonHandle.Id)))
    {
      reason->Width().Set(SnAPI::UI::Sizing::Ratio(kValueLaneRatio));
      reason->TextAlignment().Set(SnAPI::UI::ETextAlignment::Start);
      reason->TextColor().Set(Color{198, 162, 174, 255});
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
  if (Type == StaticTypeId<Vec2>())
  {
    return EEditorKind::Vec2;
  }
  if (Type == StaticTypeId<Vec3>())
  {
    return EEditorKind::Vec3;
  }
  if (Type == StaticTypeId<Vec4>())
  {
    return EEditorKind::Vec4;
  }
  if (Type == StaticTypeId<Quat>())
  {
    return EEditorKind::Quat;
  }
  if (Type == StaticTypeId<SnAPI::UI::Color>())
  {
    return EEditorKind::Color;
  }
  if (Type == StaticTypeId<Uuid>())
  {
    return EEditorKind::Uuid;
  }
  if (Type == StaticTypeId<TSubClassOf<PawnBase>>())
  {
    return EEditorKind::SubClass;
  }
  if (Type == StaticTypeId<TAssetRef<PawnBase>>())
  {
    return EEditorKind::AssetRef;
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

  if (Binding.EditorKind == EEditorKind::SubClass)
  {
    if (Binding.FieldType != StaticTypeId<TSubClassOf<PawnBase>>() || !field->ConstPointer)
    {
      return false;
    }

    const auto* subClassValue = static_cast<const TSubClassOf<PawnBase>*>(field->ConstPointer(owner));
    if (!subClassValue)
    {
      return false;
    }

    OutText = subClassValue->ResolvedTypeName();
    OutBool = false;
    return true;
  }

  if (Binding.EditorKind == EEditorKind::AssetRef)
  {
    if (Binding.FieldType != StaticTypeId<TAssetRef<PawnBase>>() || !field->ConstPointer)
    {
      return false;
    }

    const auto* assetRefValue = static_cast<const TAssetRef<PawnBase>*>(field->ConstPointer(owner));
    if (!assetRefValue)
    {
      return false;
    }

    const std::string selectedName = assetRefValue->ResolvedAssetName();
    const std::string selectedId = TrimCopy(assetRefValue->GetAssetId());
    if (selectedName.empty() && selectedId.empty())
    {
      OutText = std::string(kAssetRefNoneOption);
      OutBool = false;
      return true;
    }

    const auto entries = TAssetRef<PawnBase>::EnumerateCompatibleAssets();
    const auto it = std::ranges::find_if(entries, [&](const TAssetRef<PawnBase>::TEntry& entry) {
      if (!selectedId.empty())
      {
        return entry.AssetId == selectedId;
      }
      return entry.Name == selectedName;
    });

    if (it != entries.end())
    {
      OutText = it->Label;
    }
    else
    {
      OutText = assetRefValue->DisplayLabel();
    }

    OutBool = false;
    return true;
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
  case EEditorKind::Vec2:
    {
      const auto ref = value.AsConstRef<Vec2>();
      if (!ref)
      {
        return false;
      }
      OutText = FormatVec2(ref->get());
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
  case EEditorKind::Vec4:
    {
      const auto ref = value.AsConstRef<Vec4>();
      if (!ref)
      {
        return false;
      }
      OutText = FormatVec4(ref->get());
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
  case EEditorKind::Color:
    {
      const auto ref = value.AsConstRef<SnAPI::UI::Color>();
      if (!ref)
      {
        return false;
      }
      OutText = FormatColor(ref->get());
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
  case EEditorKind::SubClass:
  case EEditorKind::AssetRef:
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
  const bool requiresSetter =
    Binding.EditorKind != EEditorKind::Enum &&
    Binding.EditorKind != EEditorKind::SubClass &&
    Binding.EditorKind != EEditorKind::AssetRef;
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
  case EEditorKind::Vec2:
    {
      Vec2 parsed{};
      if (!ParseVec2(TextValue, parsed))
      {
        return false;
      }
      return static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed)));
    }
  case EEditorKind::Vec3:
    {
      Vec3 parsed{};
      if (!ParseVec3(TextValue, parsed))
      {
        return false;
      }
      return static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed)));
    }
  case EEditorKind::Vec4:
    {
      Vec4 parsed{};
      if (!ParseVec4(TextValue, parsed))
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
  case EEditorKind::Color:
    {
      SnAPI::UI::Color parsed{};
      if (!ParseColor(TextValue, parsed))
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
  case EEditorKind::SubClass:
    {
      if (Binding.FieldType != StaticTypeId<TSubClassOf<PawnBase>>() || !field->MutablePointer)
      {
        return false;
      }

      auto* value = static_cast<TSubClassOf<PawnBase>*>(field->MutablePointer(owner));
      if (!value)
      {
        return false;
      }

      const std::string selected = TrimCopy(TextValue);
      if (selected.empty())
      {
        value->Clear();
        return true;
      }

      return value->SetTypeByName(selected);
    }
  case EEditorKind::AssetRef:
    {
      if (Binding.FieldType != StaticTypeId<TAssetRef<PawnBase>>() || !field->MutablePointer)
      {
        return false;
      }

      auto* value = static_cast<TAssetRef<PawnBase>*>(field->MutablePointer(owner));
      if (!value)
      {
        return false;
      }

      const std::string selected = TrimCopy(TextValue);
      if (selected.empty() || selected == kAssetRefNoneOption)
      {
        value->Clear();
        return true;
      }

      const auto entries = TAssetRef<PawnBase>::EnumerateCompatibleAssets();
      const auto it = std::ranges::find_if(entries, [&](const TAssetRef<PawnBase>::TEntry& entry) {
        return entry.Label == selected || entry.Name == selected || entry.AssetId == selected;
      });

      if (it == entries.end())
      {
        value->SetAsset(selected, {});
        return true;
      }

      value->SetAsset(it->Name, it->AssetId);
      return true;
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

bool UIPropertyPanel::ParseVec2(std::string_view Text, Vec2& OutValue) const
{
  std::array<double, 2> components{};
  if (!ParseComponentList(Text, components.data(), components.size()))
  {
    return false;
  }

  OutValue.x() = static_cast<Vec2::Scalar>(components[0]);
  OutValue.y() = static_cast<Vec2::Scalar>(components[1]);
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

bool UIPropertyPanel::ParseVec4(std::string_view Text, Vec4& OutValue) const
{
  std::array<double, 4> components{};
  if (!ParseComponentList(Text, components.data(), components.size()))
  {
    return false;
  }

  OutValue.x() = static_cast<Vec4::Scalar>(components[0]);
  OutValue.y() = static_cast<Vec4::Scalar>(components[1]);
  OutValue.z() = static_cast<Vec4::Scalar>(components[2]);
  OutValue.w() = static_cast<Vec4::Scalar>(components[3]);
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

bool UIPropertyPanel::ParseColor(std::string_view Text, SnAPI::UI::Color& OutValue) const
{
  std::array<double, 4> components{};
  if (!ParseComponentList(Text, components.data(), components.size()))
  {
    return false;
  }

  const auto ClampChannel = [](const double Value) -> std::uint8_t {
    return static_cast<std::uint8_t>(std::clamp(std::llround(Value), 0ll, 255ll));
  };

  OutValue.R = ClampChannel(components[0]);
  OutValue.G = ClampChannel(components[1]);
  OutValue.B = ClampChannel(components[2]);
  OutValue.A = ClampChannel(components[3]);
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

std::string UIPropertyPanel::FormatVec2(const Vec2& Value) const
{
  const std::array<double, 2> components{
    static_cast<double>(Value.x()),
    static_cast<double>(Value.y())};
  return FormatComponentValues(components);
}

std::string UIPropertyPanel::FormatVec3(const Vec3& Value) const
{
  const std::array<double, 3> components{
    static_cast<double>(Value.x()),
    static_cast<double>(Value.y()),
    static_cast<double>(Value.z())};
  return FormatComponentValues(components);
}

std::string UIPropertyPanel::FormatVec4(const Vec4& Value) const
{
  const std::array<double, 4> components{
    static_cast<double>(Value.x()),
    static_cast<double>(Value.y()),
    static_cast<double>(Value.z()),
    static_cast<double>(Value.w())};
  return FormatComponentValues(components);
}

std::string UIPropertyPanel::FormatQuat(const Quat& Value) const
{
  const std::array<double, 4> components{
    static_cast<double>(Value.x()),
    static_cast<double>(Value.y()),
    static_cast<double>(Value.z()),
    static_cast<double>(Value.w())};
  return FormatComponentValues(components);
}

std::string UIPropertyPanel::FormatColor(const SnAPI::UI::Color& Value) const
{
  const std::array<double, 4> components{
    static_cast<double>(Value.R),
    static_cast<double>(Value.G),
    static_cast<double>(Value.B),
    static_cast<double>(Value.A)};
  return FormatComponentValues(components);
}

UIPropertyPanel::FieldBinding* UIPropertyPanel::ResolveLiveBinding(
  const std::size_t BindingIndex,
  const std::uint64_t Generation)
{
  if (Generation != m_BindingGeneration || BindingIndex >= m_Bindings.size())
  {
    return nullptr;
  }

  FieldBinding& binding = m_Bindings[BindingIndex];
  if (binding.Generation != Generation)
  {
    return nullptr;
  }

  return &binding;
}

bool UIPropertyPanel::IsEditorFocused(const FieldBinding& Binding) const
{
  if (!m_Context)
  {
    return false;
  }

  if (Binding.ComponentCount > 0)
  {
    for (std::uint8_t index = 0; index < Binding.ComponentCount; ++index)
    {
      const SnAPI::UI::ElementId componentId = Binding.ComponentEditorIds[index];
      if (componentId.Value == 0)
      {
        continue;
      }

      auto* numberField = dynamic_cast<SnAPI::UI::UINumberField*>(&m_Context->GetElement(componentId));
      if (numberField && numberField->IsFocused())
      {
        return true;
      }
    }
    return false;
  }

  if (Binding.EditorKind == EEditorKind::Enum ||
      Binding.EditorKind == EEditorKind::SubClass ||
      Binding.EditorKind == EEditorKind::AssetRef)
  {
    auto* comboBox = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(Binding.EditorId));
    return comboBox && comboBox->IsFocused();
  }

  if (Binding.EditorKind == EEditorKind::Bool)
  {
    auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(Binding.EditorId));
    return checkbox && checkbox->IsFocused();
  }

  auto* textInput = dynamic_cast<SnAPI::UI::UITextInput*>(&m_Context->GetElement(Binding.EditorId));
  return textInput && textInput->IsFocused();
}

void UIPropertyPanel::AttachEditorHooks(const std::size_t BindingIndex)
{
  if (!m_Context || BindingIndex >= m_Bindings.size())
  {
    return;
  }

  FieldBinding& binding = m_Bindings[BindingIndex];
  if (binding.ReadOnly || binding.EditorId.Value == 0)
  {
    return;
  }

  const std::uint64_t generation = binding.Generation;

  if (binding.EditorKind == EEditorKind::Bool)
  {
    auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(binding.EditorId));
    if (!checkbox)
    {
      return;
    }

    binding.EditorHookHandle = checkbox->Checked().AddSetHook([this, BindingIndex, generation](const bool Value) {
      CommitBindingFromEditor(BindingIndex, generation, {}, Value);
    });
    return;
  }

  if (binding.EditorKind == EEditorKind::Enum ||
      binding.EditorKind == EEditorKind::SubClass ||
      binding.EditorKind == EEditorKind::AssetRef)
  {
    auto* comboBox = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(binding.EditorId));
    if (!comboBox)
    {
      return;
    }

    binding.EditorHookHandle =
      comboBox->SelectedIndex().AddSetHook([this, BindingIndex, generation](const std::int32_t Index) {
        if (Index < 0)
        {
          return;
        }

        FieldBinding* liveBinding = ResolveLiveBinding(BindingIndex, generation);
        if (!liveBinding || !m_Context)
        {
          return;
        }

        auto* liveComboBox = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(liveBinding->EditorId));
        if (!liveComboBox)
        {
          return;
        }

        const std::string selectedText = liveComboBox->SelectedText();
        if (selectedText.empty())
        {
          return;
        }

        CommitBindingFromEditor(BindingIndex, generation, selectedText, false);
      });
    return;
  }

  if (binding.ComponentCount > 0)
  {
    for (std::uint8_t index = 0; index < binding.ComponentCount; ++index)
    {
      const SnAPI::UI::ElementId componentId = binding.ComponentEditorIds[index];
      if (componentId.Value == 0)
      {
        continue;
      }

      auto* numberField = dynamic_cast<SnAPI::UI::UINumberField*>(&m_Context->GetElement(componentId));
      if (!numberField)
      {
        continue;
      }

      binding.ComponentHookHandles[index] = numberField->Value().AddSetHook(
        [this, BindingIndex, generation](const double /*Value*/) {
          CommitBindingFromComponents(BindingIndex, generation);
        });
    }
    return;
  }

  auto* textInput = dynamic_cast<SnAPI::UI::UITextInput*>(&m_Context->GetElement(binding.EditorId));
  if (!textInput)
  {
    return;
  }

  binding.EditorHookHandle = textInput->Text().AddSetHook([this, BindingIndex, generation](const std::string& Value) {
    CommitBindingFromEditor(BindingIndex, generation, Value, false);
  });
}

void UIPropertyPanel::ClearBindingHooks()
{
  if (!m_Context)
  {
    return;
  }

  for (FieldBinding& binding : m_Bindings)
  {
    if (binding.EditorHookHandle != 0 && binding.EditorId.Value != 0)
    {
      if (binding.EditorKind == EEditorKind::Bool)
      {
        if (auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(binding.EditorId)))
        {
          (void)checkbox->Checked().RemoveSetHook(binding.EditorHookHandle);
        }
      }
      else if (binding.EditorKind == EEditorKind::Enum ||
               binding.EditorKind == EEditorKind::SubClass ||
               binding.EditorKind == EEditorKind::AssetRef)
      {
        if (auto* comboBox = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(binding.EditorId)))
        {
          (void)comboBox->SelectedIndex().RemoveSetHook(binding.EditorHookHandle);
        }
      }
      else if (auto* textInput = dynamic_cast<SnAPI::UI::UITextInput*>(&m_Context->GetElement(binding.EditorId)))
      {
        (void)textInput->Text().RemoveSetHook(binding.EditorHookHandle);
      }
    }
    binding.EditorHookHandle = 0;

    for (std::uint8_t index = 0; index < binding.ComponentCount; ++index)
    {
      const std::size_t handle = binding.ComponentHookHandles[index];
      const SnAPI::UI::ElementId componentId = binding.ComponentEditorIds[index];
      if (handle == 0 || componentId.Value == 0)
      {
        continue;
      }

      if (auto* numberField = dynamic_cast<SnAPI::UI::UINumberField*>(&m_Context->GetElement(componentId)))
      {
        (void)numberField->Value().RemoveSetHook(handle);
      }
      binding.ComponentHookHandles[index] = 0;
    }
  }
}

void UIPropertyPanel::CommitBindingFromEditor(
  const std::size_t BindingIndex,
  const std::uint64_t Generation,
  const std::string_view TextValue,
  const bool BoolValue)
{
  if (m_SyncingModelToEditors || m_CommittingEditorToModel)
  {
    return;
  }

  FieldBinding* binding = ResolveLiveBinding(BindingIndex, Generation);
  if (!binding || binding->ReadOnly)
  {
    return;
  }

  ScopedFlag guard(m_CommittingEditorToModel);
  if (!WriteFieldValue(*binding, TextValue, BoolValue) && !IsEditorFocused(*binding))
  {
    SyncBindingToEditor(*binding);
  }
}

void UIPropertyPanel::CommitBindingFromComponents(
  const std::size_t BindingIndex,
  const std::uint64_t Generation)
{
  if (m_SyncingModelToEditors || m_CommittingEditorToModel || !m_Context)
  {
    return;
  }

  FieldBinding* binding = ResolveLiveBinding(BindingIndex, Generation);
  if (!binding || binding->ReadOnly || binding->ComponentCount == 0)
  {
    return;
  }

  std::array<double, 4> currentComponents{};
  bool hasAnyComponent = false;
  for (std::uint8_t index = 0; index < binding->ComponentCount; ++index)
  {
    const SnAPI::UI::ElementId componentId = binding->ComponentEditorIds[index];
    if (componentId.Value == 0)
    {
      continue;
    }

    auto* numberField = dynamic_cast<SnAPI::UI::UINumberField*>(&m_Context->GetElement(componentId));
    if (!numberField)
    {
      continue;
    }

    hasAnyComponent = true;
    currentComponents[index] = numberField->GetValue();
  }

  if (!hasAnyComponent)
  {
    return;
  }

  std::string nextText{};
  switch (binding->EditorKind)
  {
  case EEditorKind::Signed:
    nextText = std::to_string(static_cast<std::int64_t>(std::llround(currentComponents[0])));
    break;
  case EEditorKind::Unsigned:
    nextText = std::to_string(static_cast<std::uint64_t>(std::max<std::int64_t>(
      0,
      static_cast<std::int64_t>(std::llround(currentComponents[0])))));
    break;
  case EEditorKind::Float:
  case EEditorKind::Double:
    nextText = FormatNumber(currentComponents[0]);
    break;
  case EEditorKind::Vec2:
    nextText = FormatComponentValues(std::span<const double>(currentComponents.data(), 2));
    break;
  case EEditorKind::Vec3:
    nextText = FormatComponentValues(std::span<const double>(currentComponents.data(), 3));
    break;
  case EEditorKind::Vec4:
  case EEditorKind::Quat:
    nextText = FormatComponentValues(std::span<const double>(currentComponents.data(), 4));
    break;
  case EEditorKind::Color:
    {
      std::array<double, 4> colorChannels{
        static_cast<double>(std::clamp(std::llround(currentComponents[0]), 0ll, 255ll)),
        static_cast<double>(std::clamp(std::llround(currentComponents[1]), 0ll, 255ll)),
        static_cast<double>(std::clamp(std::llround(currentComponents[2]), 0ll, 255ll)),
        static_cast<double>(std::clamp(std::llround(currentComponents[3]), 0ll, 255ll))};
      nextText = FormatComponentValues(colorChannels);
      break;
    }
  default:
    break;
  }

  if (nextText.empty())
  {
    return;
  }

  ScopedFlag guard(m_CommittingEditorToModel);
  if (!WriteFieldValue(*binding, nextText, false) && !IsEditorFocused(*binding))
  {
    SyncBindingToEditor(*binding);
  }
}

void UIPropertyPanel::SyncBindingToEditor(FieldBinding& Binding)
{
  if (!m_Context || Binding.EditorId.Value == 0)
  {
    return;
  }

  std::string textValue{};
  bool boolValue = false;
  if (!ReadFieldValue(Binding, textValue, boolValue))
  {
    return;
  }

  SnAPI::UI::IUIElement& element = m_Context->GetElement(Binding.EditorId);
  if (Binding.EditorKind == EEditorKind::Bool && !Binding.ReadOnly)
  {
    auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&element);
    if (!checkbox)
    {
      return;
    }

    if (checkbox->Properties().GetPropertyOr(SnAPI::UI::UICheckbox::CheckedKey, false) != boolValue)
    {
      checkbox->Checked().Set(boolValue);
    }
    return;
  }

  if (Binding.EditorKind == EEditorKind::Enum ||
      Binding.EditorKind == EEditorKind::SubClass ||
      Binding.EditorKind == EEditorKind::AssetRef)
  {
    auto* comboBox = dynamic_cast<SnAPI::UI::UIComboBox*>(&element);
    if (!comboBox || comboBox->IsFocused())
    {
      return;
    }

    if (!comboBox->SelectByText(textValue, false))
    {
      comboBox->SetSelectedIndex(-1, false);
    }
    return;
  }

  if (Binding.ComponentCount > 0)
  {
    std::array<double, 4> nextComponents{};
    bool parseOk = true;

    switch (Binding.EditorKind)
    {
    case EEditorKind::Signed:
    case EEditorKind::Unsigned:
    case EEditorKind::Float:
    case EEditorKind::Double:
      parseOk = ParseDouble(textValue, nextComponents[0]);
      break;
    case EEditorKind::Vec2:
      parseOk = ParseComponentList(textValue, nextComponents.data(), 2);
      break;
    case EEditorKind::Vec3:
      parseOk = ParseComponentList(textValue, nextComponents.data(), 3);
      break;
    case EEditorKind::Vec4:
    case EEditorKind::Quat:
    case EEditorKind::Color:
      parseOk = ParseComponentList(textValue, nextComponents.data(), 4);
      break;
    default:
      parseOk = false;
      break;
    }

    if (!parseOk || IsEditorFocused(Binding))
    {
      return;
    }

    for (std::uint8_t index = 0; index < Binding.ComponentCount; ++index)
    {
      const SnAPI::UI::ElementId componentId = Binding.ComponentEditorIds[index];
      if (componentId.Value == 0)
      {
        continue;
      }

      auto* numberField = dynamic_cast<SnAPI::UI::UINumberField*>(&m_Context->GetElement(componentId));
      if (!numberField)
      {
        continue;
      }

      if (!NearlyEqual(numberField->GetValue(), nextComponents[index]))
      {
        (void)numberField->SetValue(nextComponents[index], false);
      }
    }
    return;
  }

  auto* textInput = dynamic_cast<SnAPI::UI::UITextInput*>(&element);
  if (!textInput || textInput->IsFocused())
  {
    return;
  }

  if (textInput->Properties().GetPropertyOr(SnAPI::UI::UITextInput::TextKey, std::string{}) != textValue)
  {
    textInput->Text().Set(textValue);
  }
}

void UIPropertyPanel::SyncModelToEditors()
{
  if (!m_Context || !m_BoundInstance)
  {
    return;
  }

  ScopedFlag guard(m_SyncingModelToEditors);
  for (FieldBinding& binding : m_Bindings)
  {
    SyncBindingToEditor(binding);
  }
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
