#include "UIPropertyPanel.h"

#if defined(SNAPI_GF_ENABLE_UI)

#include "AuthoredAssetRegistry.h"
#include "AssetRef.h"
#include "AssetPipelineIds.h"
#include "BaseNode.h"
#include "BuiltinTypes.h"
#include "Editor/EditorImportSettings.h"
#include "IWorld.h"
#include "PathResolver.h"
#include "RenderAssetPayloads.h"
#include "RenderAssetRuntime.h"
#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "AtmosphereCompositeParamsNode.h"
#include "AtmosphereParamsNode.h"
#include "BloomParamsNode.h"
#include "HeightFogParamsNode.h"
#include "SSAOParamsNode.h"
#include "SSGIParamsNode.h"
#include "SSRParamsNode.h"
#include "TAAParamsNode.h"
#include "StaticMeshComponent.h"
#include "ToneMapParamsNode.h"
#include "WorldRenderSettings.h"
#endif
#include "SubClassOf.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <TextureCompressorIds.h>

#include <UIContext.h>
#include <UIButton.h>
#include <UICheckbox.h>
#include <UIComboBox.h>
#include <UIElementBase.h>
#include <UIEvents.h>
#include <UIFilesystemPicker.h>
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
constexpr int kFormatNumberPrecision = 9;
constexpr uint32_t kFloatEditorPrecision = 7u;
constexpr uint32_t kDoubleEditorPrecision = 9u;

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

[[nodiscard]] std::string TrimCopy(std::string_view Text);

[[nodiscard]] bool IsElementWithinSubtree(SnAPI::UI::UIContext& Context,
                                          SnAPI::UI::ElementId Element,
                                          const SnAPI::UI::ElementId SubtreeRoot)
{
  while (Element.Value != 0)
  {
    if (Element == SubtreeRoot)
    {
      return true;
    }
    Element = Context.GetParent(Element);
  }
  return false;
}

template<typename TAssetRefType>
void PopulateAssetRefOptions(std::vector<std::string>& OutOptions)
{
  const auto entries = TAssetRefType::EnumerateCompatibleAssets();
  OutOptions.reserve(OutOptions.size() + entries.size() + 1);
  OutOptions.emplace_back(kAssetRefNoneOption);
  for (const auto& entry : entries)
  {
    OutOptions.push_back(entry.Label);
  }
}

template<typename TAssetRefType>
bool TryReadAssetRefSelectionLabel(const void* ConstPointer, std::string& OutText)
{
  const auto* assetRefValue = static_cast<const TAssetRefType*>(ConstPointer);
  if (!assetRefValue)
  {
    return false;
  }

  const std::string selectedName = assetRefValue->ResolvedAssetName();
  const std::string selectedId = TrimCopy(assetRefValue->GetAssetId());
  if (selectedName.empty() && selectedId.empty())
  {
    OutText = std::string(kAssetRefNoneOption);
    return true;
  }

  const auto entries = TAssetRefType::EnumerateCompatibleAssets();
  const auto it = std::ranges::find_if(entries, [&](const typename TAssetRefType::TEntry& entry) {
    if (!selectedId.empty())
    {
      return entry.AssetId == selectedId;
    }
    return entry.Name == selectedName;
  });

  OutText = (it != entries.end()) ? it->Label : assetRefValue->DisplayLabel();
  return true;
}

template<typename TAssetRefType>
bool TryWriteAssetRefSelection(void* MutablePointer, const std::string& Selected)
{
  auto* value = static_cast<TAssetRefType*>(MutablePointer);
  if (!value)
  {
    return false;
  }

  if (Selected.empty() || Selected == kAssetRefNoneOption)
  {
    value->Clear();
    return true;
  }

  const auto entries = TAssetRefType::EnumerateCompatibleAssets();
  const auto it = std::ranges::find_if(entries, [&](const typename TAssetRefType::TEntry& entry) {
    return entry.Label == Selected || entry.Name == Selected || entry.AssetId == Selected;
  });

  if (it == entries.end())
  {
    value->SetAsset(Selected, {});
    return true;
  }

  value->SetAsset(it->Name, it->AssetId);
  return true;
}

void PopulateEditorValueOptions(const TypeInfo& ValueType, std::vector<std::string>& OutOptions)
{
  if (ValueType.EditorValueAdapter.PopulateOptions)
  {
    ValueType.EditorValueAdapter.PopulateOptions(OutOptions);
  }
}

bool TryReadEditorValueSelectionLabel(const TypeInfo& ValueType, const void* ConstPointer, std::string& OutText)
{
  return ValueType.EditorValueAdapter.ReadSelectionLabel
      && ValueType.EditorValueAdapter.ReadSelectionLabel(ConstPointer, OutText);
}

bool TryWriteEditorValueSelection(const TypeInfo& ValueType, void* MutablePointer, std::string_view Selected)
{
  return ValueType.EditorValueAdapter.WriteSelection
      && ValueType.EditorValueAdapter.WriteSelection(MutablePointer, Selected);
}

[[nodiscard]] std::string ShortEnumValueName(std::string_view Name)
{
  return PrettyReflectedTypeName(Name);
}

struct FlagEditorEntry
{
  std::string Label{};
  std::uint64_t Value = 0;
};

[[nodiscard]] std::vector<FlagEditorEntry> BuildFlagEditorEntries(const TypeInfo& EnumInfo)
{
  std::vector<FlagEditorEntry> SingleBitEntries{};
  std::vector<FlagEditorEntry> FallbackEntries{};

  for (const EnumValueInfo& EnumValue : EnumInfo.EnumValues)
  {
    if (EnumValue.Value == 0)
    {
      continue;
    }

    FlagEditorEntry Entry{
      .Label = ShortEnumValueName(EnumValue.Name),
      .Value = EnumValue.Value,
    };

    FallbackEntries.push_back(Entry);
    if (std::has_single_bit(EnumValue.Value))
    {
      SingleBitEntries.push_back(std::move(Entry));
    }
  }

  auto SortEntries = [](std::vector<FlagEditorEntry>& Entries) {
    std::ranges::sort(Entries, [](const FlagEditorEntry& Left, const FlagEditorEntry& Right) {
      if (Left.Value != Right.Value)
      {
        return Left.Value < Right.Value;
      }
      return Left.Label < Right.Label;
    });
  };

  SortEntries(SingleBitEntries);
  SortEntries(FallbackEntries);
  return !SingleBitEntries.empty() ? SingleBitEntries : FallbackEntries;
}

[[nodiscard]] std::string FormatFlagsSummary(const std::uint64_t Bits, const TypeInfo& EnumInfo)
{
  if (Bits == 0)
  {
    return "None";
  }

  const auto Entries = BuildFlagEditorEntries(EnumInfo);
  std::vector<std::string> Enabled{};
  for (const FlagEditorEntry& Entry : Entries)
  {
    if ((Bits & Entry.Value) != 0)
    {
      Enabled.push_back(Entry.Label);
    }
  }

  if (Enabled.empty())
  {
    return std::to_string(Bits);
  }

  if (Enabled.size() <= 3)
  {
    std::string Summary{};
    for (size_t Index = 0; Index < Enabled.size(); ++Index)
    {
      if (Index > 0)
      {
        Summary += ", ";
      }
      Summary += Enabled[Index];
    }
    return Summary;
  }

  return Enabled.front() + ", " + Enabled[1] + ", +" + std::to_string(Enabled.size() - 2);
}

struct GenericAssetRefEntry
{
  std::string Label{};
  std::string Name{};
  std::string AssetId{};
  ::SnAPI::AssetPipeline::TypeId AssetKind{};
};

[[nodiscard]] std::vector<GenericAssetRefEntry> EnumerateAnyAssetRefEntries(
  const std::optional<::SnAPI::AssetPipeline::TypeId>& AssetKindFilter = std::nullopt)
{
  std::vector<GenericAssetRefEntry> Entries{};
  auto* manager = ResolveDefaultAssetManager();
  if (!manager)
  {
    return Entries;
  }

  std::unordered_set<std::string> SeenAssetIds{};
  const auto TryAppendEntry = [&Entries, &SeenAssetIds, &AssetKindFilter](
                                const std::string& Name,
                                const ::SnAPI::AssetPipeline::AssetId& AssetId,
                                const ::SnAPI::AssetPipeline::TypeId& AssetKind) {
    if (Name.empty() || AssetId.IsNull())
    {
      return;
    }
    if (AssetKindFilter.has_value() && AssetKind != *AssetKindFilter)
    {
      return;
    }

    const std::string AssetIdText = AssetId.ToString();
    if (!SeenAssetIds.insert(AssetIdText).second)
    {
      return;
    }

    GenericAssetRefEntry Entry{};
    Entry.Name = Name;
    Entry.AssetId = AssetIdText;
    Entry.AssetKind = AssetKind;
    Entry.Label = Entry.Name + " [" + (Entry.AssetId.size() > 8 ? Entry.AssetId.substr(0, 8) : Entry.AssetId) + "]";
    Entries.push_back(std::move(Entry));
  };

  const auto catalog = manager->ListAssetCatalog();
  Entries.reserve(catalog.size());
  for (const auto& catalogEntry : catalog)
  {
    TryAppendEntry(catalogEntry.Info.Name.empty() ? catalogEntry.Info.Id.ToString() : catalogEntry.Info.Name,
                   catalogEntry.Info.Id,
                   catalogEntry.Info.AssetKind);
  }

  AuthoredAssetRegistry::Instance().EnsureBuilt();
  const std::filesystem::path AssetRoot = SPathResolver::Instance().AssetRoot();
  std::error_code Error{};
  if (!AssetRoot.empty() && std::filesystem::exists(AssetRoot, Error) && !Error)
  {
    std::filesystem::recursive_directory_iterator It(
      AssetRoot,
      std::filesystem::directory_options::skip_permission_denied,
      Error);
    const std::filesystem::recursive_directory_iterator End{};
    for (; !Error && It != End; It.increment(Error))
    {
      const std::filesystem::directory_entry& EntryRef = *It;
      if (!EntryRef.is_regular_file())
      {
        continue;
      }

      std::filesystem::path SourcePath = EntryRef.path().lexically_normal();
      if (SourcePath.extension() == ".snpak")
      {
        continue;
      }

      std::string Extension = SourcePath.extension().string();
      std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](const unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
      });
      const AuthoredAssetDescriptor* Descriptor = AuthoredAssetRegistry::Instance().FindByExtension(Extension);
      if (!Descriptor)
      {
        continue;
      }

      std::error_code RelativeError{};
      std::filesystem::path Relative = std::filesystem::relative(SourcePath, AssetRoot, RelativeError);
      if (RelativeError)
      {
        Relative = SourcePath.filename();
      }

      std::string LogicalName = Relative.generic_string();
      std::replace(LogicalName.begin(), LogicalName.end(), '\\', '/');
      while (LogicalName.rfind("./", 0) == 0)
      {
        LogicalName.erase(0, 2u);
      }
      while (!LogicalName.empty() && LogicalName.front() == '/')
      {
        LogicalName.erase(LogicalName.begin());
      }
      if (LogicalName.empty())
      {
        continue;
      }

      TryAppendEntry(LogicalName,
                     SourceAssetIdFromLogicalName(LogicalName),
                     Descriptor->CookedAssetKind);
    }
  }

  std::ranges::sort(Entries, [](const GenericAssetRefEntry& Left, const GenericAssetRefEntry& Right) {
    if (Left.Name != Right.Name)
    {
      return Left.Name < Right.Name;
    }
    return Left.AssetId < Right.AssetId;
  });

  return Entries;
}

[[nodiscard]] std::optional<::SnAPI::AssetPipeline::TypeId> AssetRefPayloadKindFilterFromField(const FieldInfo& Field)
{
  std::string NameLower(Field.Name);
  std::ranges::transform(NameLower, NameLower.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (NameLower == "parentmaterial")
  {
    return AssetKindMaterial();
  }
  if (NameLower == "materialinstances")
  {
    return AssetKindMaterialInstance();
  }
  return std::nullopt;
}

void PopulateAssetRefPayloadOptions(
  std::vector<std::string>& OutOptions,
  const std::optional<::SnAPI::AssetPipeline::TypeId>& AssetKindFilter = std::nullopt)
{
  const auto entries = EnumerateAnyAssetRefEntries(AssetKindFilter);
  OutOptions.reserve(OutOptions.size() + entries.size() + 1);
  OutOptions.emplace_back(kAssetRefNoneOption);
  for (const auto& entry : entries)
  {
    OutOptions.push_back(entry.Label);
  }
}

bool TryReadAssetRefPayloadSelectionLabel(
  const void* ConstPointer,
  std::string& OutText,
  const std::optional<::SnAPI::AssetPipeline::TypeId>& AssetKindFilter = std::nullopt)
{
  const auto* assetRefValue = static_cast<const AssetRefPayload*>(ConstPointer);
  if (!assetRefValue)
  {
    return false;
  }

  const std::string selectedName = TrimCopy(assetRefValue->AssetName);
  const std::string selectedId = TrimCopy(assetRefValue->AssetId);
  if (selectedName.empty() && selectedId.empty())
  {
    OutText = std::string(kAssetRefNoneOption);
    return true;
  }

  const auto entries = EnumerateAnyAssetRefEntries(AssetKindFilter);
  const auto it = std::ranges::find_if(entries, [&](const GenericAssetRefEntry& entry) {
    if (!selectedId.empty())
    {
      return entry.AssetId == selectedId;
    }
    return entry.Name == selectedName;
  });

  if (it != entries.end())
  {
    OutText = it->Label;
    return true;
  }

  if (!selectedName.empty() && !selectedId.empty())
  {
    OutText = selectedName + " [" + selectedId + "]";
  }
  else if (!selectedName.empty())
  {
    OutText = selectedName;
  }
  else
  {
    OutText = selectedId;
  }
  return true;
}

bool TryWriteAssetRefPayloadSelection(
  void* MutablePointer,
  const std::string& Selected,
  const std::optional<::SnAPI::AssetPipeline::TypeId>& AssetKindFilter = std::nullopt)
{
  auto* value = static_cast<AssetRefPayload*>(MutablePointer);
  if (!value)
  {
    return false;
  }

  if (Selected.empty() || Selected == kAssetRefNoneOption)
  {
    value->AssetName.clear();
    value->AssetId.clear();
    return true;
  }

  const auto entries = EnumerateAnyAssetRefEntries(AssetKindFilter);
  const auto it = std::ranges::find_if(entries, [&](const GenericAssetRefEntry& entry) {
    return entry.Label == Selected || entry.Name == Selected || entry.AssetId == Selected;
  });
  if (it == entries.end())
  {
    value->AssetName = Selected;
    value->AssetId.clear();
    return true;
  }

  value->AssetName = it->Name;
  value->AssetId = it->AssetId;
  return true;
}

void PopulateTypeIdOptions(std::vector<std::string>& OutOptions)
{
  const auto Types = TypeRegistry::Instance().All();
  OutOptions.reserve(OutOptions.size() + Types.size() + 1);
  OutOptions.emplace_back("<None>");

  auto MakeShortTypeLabel = [] (const TypeInfo& Info) {
    return PrettyReflectedTypeName(Info.Name);
  };

  std::unordered_map<std::string, std::size_t> LabelCounts{};
  LabelCounts.reserve(Types.size());
  for (const TypeInfo* Info : Types)
  {
    if (!Info || Info->Id == TypeId{})
    {
      continue;
    }

    ++LabelCounts[MakeShortTypeLabel(*Info)];
  }

  std::vector<std::string> Labels{};
  Labels.reserve(Types.size());
  for (const TypeInfo* Info : Types)
  {
    if (!Info || Info->Id == TypeId{})
    {
      continue;
    }

    std::string Label = MakeShortTypeLabel(*Info);
    if (LabelCounts[Label] > 1)
    {
      Label = Info->Name;
    }
    Labels.push_back(std::move(Label));
  }

  std::ranges::sort(Labels);
  Labels.erase(std::unique(Labels.begin(), Labels.end()), Labels.end());
  for (std::string& Label : Labels)
  {
    OutOptions.push_back(std::move(Label));
  }
}

bool TryReadTypeIdSelectionLabel(const void* ConstPointer, std::string& OutText)
{
  const auto* Id = static_cast<const TypeId*>(ConstPointer);
  if (!Id)
  {
    return false;
  }

  if (Id->is_nil())
  {
    OutText = "<None>";
    return true;
  }

  if (const TypeInfo* Info = TypeRegistry::Instance().Find(*Id))
  {
    OutText = PrettyReflectedTypeName(Info->Name);
  }
  else
  {
    OutText = ToString(*Id);
  }
  return true;
}

bool TryWriteTypeIdSelection(void* MutablePointer, std::string_view Selected)
{
  auto* Id = static_cast<TypeId*>(MutablePointer);
  if (!Id)
  {
    return false;
  }

  const std::string Trimmed = TrimCopy(Selected);
  if (Trimmed.empty() || Trimmed == "<None>")
  {
    *Id = TypeId{};
    return true;
  }

  if (const TypeInfo* Info = TypeRegistry::Instance().FindByName(Trimmed))
  {
    *Id = Info->Id;
    return true;
  }

  const auto Types = TypeRegistry::Instance().All();
  std::unordered_map<std::string, std::size_t> LabelCounts{};
  LabelCounts.reserve(Types.size());
  for (const TypeInfo* Info : Types)
  {
    if (!Info || Info->Id == TypeId{})
    {
      continue;
    }

    std::string Label = PrettyReflectedTypeName(Info->Name);
    ++LabelCounts[Label];
  }

  for (const TypeInfo* Info : Types)
  {
    if (!Info || Info->Id == TypeId{})
    {
      continue;
    }

    std::string Label = PrettyReflectedTypeName(Info->Name);
    if (LabelCounts[Label] > 1)
    {
      Label = Info->Name;
    }

    if (Label == Trimmed)
    {
      *Id = Info->Id;
      return true;
    }
  }

  const auto Parsed = Uuid::from_string(Trimmed);
  if (!Parsed)
  {
    return false;
  }

  *Id = TypeId(*Parsed);
  return true;
}

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
  stream << std::fixed << std::setprecision(kFormatNumberPrecision) << Value;
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
  m_BoundNodeHandle = {};
  m_BoundSections.clear();
  m_BoundSections.push_back(BoundSection{
    .Type = Type,
    .Instance = Instance,
    .Heading = PrettyTypeName(Type),
    .RuntimeTarget = {},
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
  m_BoundNodeHandle = Node->Handle();
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
    .RuntimeTarget = m_BoundNodeHandle,
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
        NodeHandle UuidOnlyOwner{Node->Id()};
        ComponentInstance = WorldRef->BorrowedComponent(UuidOnlyOwner, ComponentType);
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
      .RuntimeTarget = ComponentOwner,
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
  if (m_Context && m_ContentRoot.Value != 0)
  {
    const SnAPI::UI::ElementId CapturedElement = m_Context->GetCapture();
    if (IsElementWithinSubtree(*m_Context, CapturedElement, m_ContentRoot))
    {
      m_Context->ReleaseCapture();
    }
    m_Context->DestroyElement(m_ContentRoot);
  }

  ++m_BindingGeneration;
  ClearBindingHooks();

  m_BoundType = TypeId{};
  m_BoundInstance = nullptr;
  m_BoundNodeHandle = {};
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
  RefreshBoundSectionInstances();
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
  if (!m_Context)
  {
    return false;
  }

  RefreshBoundSectionInstances();
  if (!m_BoundInstance)
  {
    return false;
  }

  if (m_RebuildInProgress)
  {
    return false;
  }
  m_RebuildInProgress = true;
  m_Built = false;

  // Property panel is single-content by design.
  if (m_Context && m_ContentRoot.Value != 0)
  {
    const SnAPI::UI::ElementId CapturedElement = m_Context->GetCapture();
    if (IsElementWithinSubtree(*m_Context, CapturedElement, m_ContentRoot))
    {
      m_Context->ReleaseCapture();
    }
    m_Context->DestroyElement(m_ContentRoot);
  }

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
    BuildTypeIntoContainer(m_ContentRoot, Section.Type, Section.Instance, {}, 0, 0);
  }
  else
  {
    for (std::size_t SectionIndex = 0; SectionIndex < m_BoundSections.size(); ++SectionIndex)
    {
      const BoundSection& Section = m_BoundSections[SectionIndex];
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

      BuildTypeIntoContainer(bodyHandle.Id, Section.Type, Section.Instance, {}, 0, SectionIndex);
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

void UIPropertyPanel::RefreshBoundSectionInstances()
{
  if (m_BoundNodeHandle.IsNull())
  {
    return;
  }

  BaseNode* RootNode = m_BoundNodeHandle.Borrowed();
  if (!RootNode)
  {
    RootNode = m_BoundNodeHandle.BorrowedSlowByUuid();
  }

  m_BoundInstance = RootNode;
  if (m_BoundSections.empty())
  {
    RefreshBindingRootInstances();
    return;
  }

  if (!RootNode)
  {
    for (BoundSection& Section : m_BoundSections)
    {
      Section.Instance = nullptr;
    }
    RefreshBindingRootInstances();
    return;
  }

  m_BoundType = RootNode->TypeKey();
  m_BoundSections.front().Type = RootNode->TypeKey();
  m_BoundSections.front().Instance = RootNode;
  m_BoundSections.front().RuntimeTarget = RootNode->Handle();

  IWorld* WorldRef = RootNode->World();
  for (std::size_t SectionIndex = 1; SectionIndex < m_BoundSections.size(); ++SectionIndex)
  {
    BoundSection& Section = m_BoundSections[SectionIndex];
    if (!Section.IsComponent || !WorldRef)
    {
      continue;
    }

    NodeHandle OwnerHandle = Section.RuntimeTarget;
    if (OwnerHandle.IsNull())
    {
      OwnerHandle = RootNode->Handle();
    }

    void* ComponentInstance = WorldRef->BorrowedComponent(OwnerHandle, Section.Type);
    if (!ComponentInstance && !OwnerHandle.Id.is_nil())
    {
      NodeHandle UuidOnlyOwner{OwnerHandle.Id};
      ComponentInstance = WorldRef->BorrowedComponent(UuidOnlyOwner, Section.Type);
      if (ComponentInstance)
      {
        OwnerHandle = UuidOnlyOwner;
      }
    }

    Section.RuntimeTarget = OwnerHandle;
    Section.ComponentOwner = OwnerHandle;
    Section.Instance = ComponentInstance;
  }

  RefreshBindingRootInstances();
}

void UIPropertyPanel::RefreshBindingRootInstances()
{
  for (FieldBinding& Binding : m_Bindings)
  {
    Binding.RootInstance =
      (Binding.SectionIndex < m_BoundSections.size()) ? m_BoundSections[Binding.SectionIndex].Instance : nullptr;
  }
}

void UIPropertyPanel::BuildTypeIntoContainer(
  const SnAPI::UI::ElementId Parent,
  const TypeId& Type,
  void* RootInstance,
  const std::vector<FieldPathEntry>& PathPrefix,
  const int Depth,
  const std::size_t SectionIndex)
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

  if (Type == StaticTypeId<MaterialInstancePayload>())
  {
    auto* payload = static_cast<MaterialInstancePayload*>(RootInstance);
    if (!payload)
    {
      AddUnsupportedRow(Parent, PrettyTypeName(Type), "Null material instance payload");
      return;
    }

    const auto reflectedFields = TypeRegistry::Instance().CollectFields(Type);
    for (const ReflectedFieldRef& fieldRef : reflectedFields)
    {
      if (!fieldRef.Field)
      {
        continue;
      }

      if (!EqualsIgnoreCase(fieldRef.Field->Name, "ParentMaterial"))
      {
        continue;
      }

      auto parentPath = PathPrefix;
      parentPath.push_back(FieldPathEntry{fieldRef.OwnerType, fieldRef.Field->Name, fieldRef.Field->IsConst});
      AddFieldEditor(Parent, *fieldRef.Field, RootInstance, std::move(parentPath), Depth, SectionIndex);
      break;
    }

    const bool readOnly = std::ranges::any_of(PathPrefix, [](const FieldPathEntry& entry) { return entry.IsConst; });
    AddMaterialScalarCollectionEditor(Parent, payload->Scalars, readOnly, "Scalars");
    AddMaterialVectorCollectionEditor(Parent, payload->Vectors, readOnly, "Vectors");
    AddMaterialTextureCollectionEditor(Parent, payload->Textures, readOnly, "Textures");
    return;
  }

  if (Type == StaticTypeId<Editor::StaticMeshAssetEditorPayload>())
  {
    auto* payload = static_cast<Editor::StaticMeshAssetEditorPayload*>(RootInstance);
    if (!payload)
    {
      AddUnsupportedRow(Parent, PrettyTypeName(Type), "Null static mesh payload");
      return;
    }

    const auto reflectedFields = TypeRegistry::Instance().CollectFields(Type);
    for (const ReflectedFieldRef& fieldRef : reflectedFields)
    {
      if (!fieldRef.Field)
      {
        continue;
      }

      if (EqualsIgnoreCase(fieldRef.Field->Name, "MaterialInstances"))
      {
        continue;
      }

      auto path = PathPrefix;
      path.push_back(FieldPathEntry{fieldRef.OwnerType, fieldRef.Field->Name, fieldRef.Field->IsConst});
      AddFieldEditor(Parent, *fieldRef.Field, RootInstance, std::move(path), Depth, SectionIndex);
    }

    const bool readOnly = std::ranges::any_of(PathPrefix, [](const FieldPathEntry& entry) { return entry.IsConst; });
    AddAssetRefCollectionEditor(
      Parent,
      payload->MaterialInstances,
      readOnly,
      "Material Instances",
      AssetKindMaterialInstance(),
      "Slot");
    return;
  }

#if defined(SNAPI_GF_ENABLE_RENDERER)
  if (Type == StaticTypeId<StaticMeshComponent::Settings>())
  {
    const auto reflectedFields = TypeRegistry::Instance().CollectFields(Type);
    std::vector<FieldPathEntry> overridePath{};
    bool hasOverrideField = false;
    for (const ReflectedFieldRef& fieldRef : reflectedFields)
    {
      if (!fieldRef.Field)
      {
        continue;
      }

      if (EqualsIgnoreCase(fieldRef.Field->Name, "MaterialInstanceOverrides"))
      {
        overridePath = PathPrefix;
        overridePath.push_back(FieldPathEntry{fieldRef.OwnerType, fieldRef.Field->Name, fieldRef.Field->IsConst});
        hasOverrideField = true;
        continue;
      }

      auto path = PathPrefix;
      path.push_back(FieldPathEntry{fieldRef.OwnerType, fieldRef.Field->Name, fieldRef.Field->IsConst});
      AddFieldEditor(Parent, *fieldRef.Field, RootInstance, std::move(path), Depth, SectionIndex);
    }

    if (!hasOverrideField)
    {
      AddUnsupportedRow(Parent, "Material Instance Overrides", "Field metadata is missing");
      return;
    }

    void* owner = nullptr;
    const FieldInfo* field = nullptr;
    if (!ResolveLeafPath(RootInstance, overridePath, owner, field) || !field || !field->MutablePointer)
    {
      AddUnsupportedRow(Parent, "Material Instance Overrides", "Unable to resolve override storage");
      return;
    }

    if (field->FieldType != StaticTypeId<std::vector<TAssetRef<MaterialInstanceAssetRuntime>>>())
    {
      AddUnsupportedRow(Parent, "Material Instance Overrides", "Override field type mismatch");
      return;
    }

    auto* overrides = static_cast<std::vector<TAssetRef<MaterialInstanceAssetRuntime>>*>(field->MutablePointer(owner));
    if (!overrides)
    {
      AddUnsupportedRow(Parent, "Material Instance Overrides", "Override storage is unavailable");
      return;
    }

    const bool readOnly = std::ranges::any_of(overridePath, [](const FieldPathEntry& entry) { return entry.IsConst; })
                       || field->IsConst
                       || !field->Setter;
    AddMaterialInstanceAssetRefCollectionEditor(
      Parent,
      *overrides,
      readOnly,
      "Material Instance Overrides",
      "Slot");
    return;
  }
#endif

  const auto reflectedFields = TypeRegistry::Instance().CollectFields(Type);
  const auto reflectedMethods = TypeRegistry::Instance().CollectMethods(Type);
  const bool hasEditorActions = std::ranges::any_of(reflectedMethods, [](const ReflectedMethodRef& methodRef) {
    return methodRef.Method
        && methodRef.Method->ParamTypes.empty()
        && methodRef.Method->Flags.Has(EMethodFlagBits::EditorAction);
  });

  if (reflectedFields.empty() && !hasEditorActions)
  {
    AddUnsupportedRow(Parent, PrettyTypeName(Type), "No reflected fields");
    return;
  }

  for (const ReflectedFieldRef& fieldRef : reflectedFields)
  {
    if (!fieldRef.Field)
    {
      continue;
    }

    auto path = PathPrefix;
    path.push_back(FieldPathEntry{fieldRef.OwnerType, fieldRef.Field->Name, fieldRef.Field->IsConst});
    AddFieldEditor(Parent, *fieldRef.Field, RootInstance, std::move(path), Depth, SectionIndex);
  }

  AddMethodActionEditors(Parent, Type, RootInstance, SectionIndex);
}

void UIPropertyPanel::AddMethodActionEditors(
  const SnAPI::UI::ElementId Parent,
  const TypeId& Type,
  void* RootInstance,
  const std::size_t SectionIndex)
{
  if (!m_Context || !RootInstance)
  {
    return;
  }

  const auto reflectedMethods = TypeRegistry::Instance().CollectMethods(Type);
  std::vector<ReflectedMethodRef> actionMethods{};
  actionMethods.reserve(reflectedMethods.size());
  for (const ReflectedMethodRef& methodRef : reflectedMethods)
  {
    if (!methodRef.Method
        || !methodRef.Method->ParamTypes.empty()
        || !methodRef.Method->Flags.Has(EMethodFlagBits::EditorAction))
    {
      continue;
    }

    actionMethods.push_back(methodRef);
  }

  if (actionMethods.empty())
  {
    return;
  }

  const auto rowHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.ActionRow");
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

  const auto labelHandle = m_Context->CreateElement<SnAPI::UI::UIText>("Actions");
  if (labelHandle.Id.Value != 0)
  {
    m_Context->AddChild(rowHandle.Id, labelHandle.Id);
    if (auto* label = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(labelHandle.Id)))
    {
      label->Width().Set(SnAPI::UI::Sizing::Ratio(kLabelLaneRatio));
      label->HAlign().Set(SnAPI::UI::EAlignment::Start);
      label->VAlign().Set(SnAPI::UI::EAlignment::Center);
      label->TextColor().Set(kLabelColor);
    }
  }

  const auto actionsHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.ActionButtons");
  if (actionsHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(rowHandle.Id, actionsHandle.Id);
  if (auto* actions = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(actionsHandle.Id)))
  {
    actions->Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    actions->Width().Set(SnAPI::UI::Sizing::Ratio(kValueLaneRatio));
    actions->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
    actions->VAlign().Set(SnAPI::UI::EAlignment::Center);
    actions->Gap().Set(6.0f);
    actions->Background().Set(Color::Transparent());
    actions->BorderColor().Set(Color::Transparent());
    actions->BorderThickness().Set(0.0f);
  }

  for (const ReflectedMethodRef& methodRef : actionMethods)
  {
    const MethodInfo* method = methodRef.Method;
    const auto buttonHandle = m_Context->CreateElement<SnAPI::UI::UIButton>();
    if (buttonHandle.Id.Value == 0)
    {
      continue;
    }

    m_Context->AddChild(actionsHandle.Id, buttonHandle.Id);
    if (auto* button = dynamic_cast<SnAPI::UI::UIButton*>(&m_Context->GetElement(buttonHandle.Id)))
    {
      button->Width().Set(SnAPI::UI::Sizing::Auto());
      button->Height().Set(SnAPI::UI::Sizing::Auto());
      button->ElementPadding().Set(SnAPI::UI::Padding{10.0f, 5.0f, 10.0f, 5.0f});
      button->Background().Set(kValueBg);
      button->BorderColor().Set(kValueBorder);
      button->BorderThickness().Set(1.0f);
      button->CornerRadius().Set(4.0f);

      button->OnClick([this, method, RootInstance, SectionIndex]() {
        RefreshBoundSectionInstances();
        void* LiveInstance = RootInstance;
        if (SectionIndex < m_BoundSections.size() && m_BoundSections[SectionIndex].Instance)
        {
          LiveInstance = m_BoundSections[SectionIndex].Instance;
        }
        const auto Result = method->Invoke(LiveInstance, {});
        if (!Result)
        {
          return;
        }
        RefreshFromModel();
      });
    }

    const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>(PrettyFieldName(method->Name));
    if (textHandle.Id.Value == 0)
    {
      continue;
    }

    m_Context->AddChild(buttonHandle.Id, textHandle.Id);
    if (auto* text = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(textHandle.Id)))
    {
      text->Visibility().Set(SnAPI::UI::EVisibility::HitTestInvisible);
      text->TextColor().Set(Color{210, 216, 226, 255});
      text->TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
    }
  }
}

void UIPropertyPanel::AddFieldEditor(
  const SnAPI::UI::ElementId Parent,
  const FieldInfo& Field,
  void* RootInstance,
  std::vector<FieldPathEntry> Path,
  const int Depth,
  const std::size_t SectionIndex)
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

      BuildTypeIntoContainer(bodyHandle.Id, Field.FieldType, RootInstance, std::move(Path), Depth + 1, SectionIndex);
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

    BuildTypeIntoContainer(bodyHandle.Id, Field.FieldType, RootInstance, std::move(Path), Depth + 1, SectionIndex);
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
  const TypeInfo* fieldTypeInfo = TypeRegistry::Instance().Find(Field.FieldType);
  const bool readOnly = pathConst || Field.IsConst || !Field.Setter;
  const bool isPathField = editorKind == EEditorKind::String && IsPathLikeField(Field);
  const bool pathSelectsDirectories = isPathField && IsDirectoryLikeField(Field);

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
  binding.SectionIndex = SectionIndex;
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
           editorKind == EEditorKind::TypeId ||
           editorKind == EEditorKind::Flags ||
           editorKind == EEditorKind::SubClass ||
           editorKind == EEditorKind::AssetRef)
  {
    if (editorKind == EEditorKind::Flags)
    {
      const TypeInfo* enumInfo =
        (fieldTypeInfo && !fieldTypeInfo->EditorValueTargetType.is_nil())
          ? TypeRegistry::Instance().Find(fieldTypeInfo->EditorValueTargetType)
          : nullptr;
      if (!enumInfo || !enumInfo->IsEnum)
      {
        const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>("<unsupported flags>");
        if (textHandle.Id.Value != 0)
        {
          m_Context->AddChild(valueHostHandle.Id, textHandle.Id);
        }
        return;
      }

      const auto accordionHandle = m_Context->CreateElement<SnAPI::UI::UIAccordion>();
      if (accordionHandle.Id.Value == 0)
      {
        return;
      }
      m_Context->AddChild(valueHostHandle.Id, accordionHandle.Id);
      binding.EditorId = accordionHandle.Id;

      auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id));
      if (!accordion)
      {
        return;
      }

      accordion->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
      accordion->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
      accordion->AllowMultipleExpanded().Set(true);
      accordion->DefaultExpanded().Set(true);
      accordion->Gap().Set(4.0f);
      accordion->Padding().Set(0.0f);
      accordion->HeaderHeight().Set(24.0f);
      accordion->HeaderPadding().Set(6.0f);
      accordion->ContentPadding().Set(4.0f);
      accordion->BackgroundColor().Set(kValueBg);
      accordion->BorderColor().Set(kValueBorder);
      accordion->BorderThickness().Set(1.0f);
      accordion->CornerRadius().Set(4.0f);
      accordion->HeaderColor().Set(Color{22, 26, 34, 252});
      accordion->HeaderHoverColor().Set(Color{26, 30, 39, 252});
      accordion->HeaderExpandedColor().Set(Color{31, 36, 46, 252});
      accordion->HeaderTextColor().Set(Color{224, 229, 237, 255});
      accordion->HeaderTextExpandedColor().Set(Color{224, 229, 237, 255});
      accordion->HeaderBorderColor().Set(kValueBorder);
      accordion->HeaderBorderThickness().Set(1.0f);
      accordion->ArrowSize().Set(18.0f);
      accordion->ArrowGap().Set(6.0f);

      const auto bodyHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.FlagsBody");
      if (bodyHandle.Id.Value == 0)
      {
        return;
      }
      m_Context->AddChild(accordionHandle.Id, bodyHandle.Id);
      binding.AuxiliaryContainerId = bodyHandle.Id;

      if (auto* body = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(bodyHandle.Id)))
      {
        body->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
        body->Padding().Set(2.0f);
        body->Gap().Set(2.0f);
        body->Width().Set(SnAPI::UI::Sizing::Fill());
        body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
        body->Background().Set(Color::Transparent());
        body->BorderColor().Set(Color::Transparent());
        body->BorderThickness().Set(0.0f);
      }

      std::uint64_t initialBits = 0;
      const TypeInfo* liveEnumInfo = nullptr;
      (void)ReadFlagsBits(binding, initialBits, liveEnumInfo);
      if (!liveEnumInfo)
      {
        liveEnumInfo = enumInfo;
      }

      accordion->SetSectionHeading(bodyHandle.Id, FormatFlagsSummary(initialBits, *liveEnumInfo));
      accordion->SetSectionExpanded(bodyHandle.Id, true);

      const auto entries = BuildFlagEditorEntries(*liveEnumInfo);
      if (entries.empty())
      {
        const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>("No flag entries");
        if (textHandle.Id.Value != 0)
        {
          m_Context->AddChild(bodyHandle.Id, textHandle.Id);
        }
      }
      else
      {
        binding.AuxiliaryEditorIds.reserve(entries.size());
        binding.AuxiliaryHookHandles.resize(entries.size(), 0);
        for (const FlagEditorEntry& entry : entries)
        {
          const auto checkboxHandle = m_Context->CreateElement<SnAPI::UI::UICheckbox>(entry.Label);
          if (checkboxHandle.Id.Value == 0)
          {
            continue;
          }

          m_Context->AddChild(bodyHandle.Id, checkboxHandle.Id);
          binding.AuxiliaryEditorIds.push_back(checkboxHandle.Id);

          if (auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(checkboxHandle.Id)))
          {
            checkbox->Width().Set(SnAPI::UI::Sizing::Fill());
            checkbox->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
            checkbox->VAlign().Set(SnAPI::UI::EAlignment::Center);
            checkbox->LabelColor().Set(kLabelColor);
            checkbox->BoxSize().Set(12.0f);
            checkbox->CheckInset().Set(2.0f);
            checkbox->ElementStyle().Apply("editor.checkbox");
            checkbox->Checked().Set((initialBits & entry.Value) != 0);
            checkbox->SetDisabled(readOnly);
          }
        }
      }
    }
    else
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
          if (fieldTypeInfo && fieldTypeInfo->IsEnum)
          {
            options.reserve(fieldTypeInfo->EnumValues.size());
            for (const EnumValueInfo& enumValue : fieldTypeInfo->EnumValues)
            {
              options.push_back(enumValue.Name);
            }
          }
        }
        else if (editorKind == EEditorKind::TypeId)
        {
          PopulateTypeIdOptions(options);
        }
        else if (editorKind == EEditorKind::AssetRef && Field.FieldType == StaticTypeId<AssetRefPayload>())
        {
          PopulateAssetRefPayloadOptions(options, AssetRefPayloadKindFilterFromField(Field));
        }
        else if (fieldTypeInfo)
        {
          PopulateEditorValueOptions(*fieldTypeInfo, options);
        }

        combo->SetItems(std::move(options));
      }
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
        if (binding.FieldType == StaticTypeId<std::int64_t>())
        {
          numberField->MinValue().Set(static_cast<double>(std::numeric_limits<std::int64_t>::min()));
          numberField->MaxValue().Set(static_cast<double>(std::numeric_limits<std::int64_t>::max()));
        }
        else
        {
          numberField->MinValue().Set(static_cast<double>(std::numeric_limits<int>::min()));
          numberField->MaxValue().Set(static_cast<double>(std::numeric_limits<int>::max()));
        }
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
        numberField->Precision().Set(kFloatEditorPrecision);
        numberField->Step().Set(0.01);
        numberField->MinValue().Set(-static_cast<double>(std::numeric_limits<float>::max()));
        numberField->MaxValue().Set(static_cast<double>(std::numeric_limits<float>::max()));
      }
      else
      {
        numberField->Precision().Set(kDoubleEditorPrecision);
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
          numberField->Precision().Set(kFloatEditorPrecision);
          numberField->Step().Set(0.01);
          numberField->MinValue().Set(-std::numeric_limits<double>::max());
          numberField->MaxValue().Set(std::numeric_limits<double>::max());
        }
      }
    }
  }
  else
  {
    const std::string fieldNameLower = ToLowerCopy(Field.Name);
    const bool allowCustomSchemePath =
      fieldNameLower == "meshpath";
    const bool useFilesystemPicker = isPathField && !allowCustomSchemePath;

    if (useFilesystemPicker)
    {
      const auto pickerHandle = m_Context->CreateElement<SnAPI::UI::UIFilesystemPicker>();
      if (pickerHandle.Id.Value == 0)
      {
        return;
      }
      m_Context->AddChild(valueHostHandle.Id, pickerHandle.Id);
      binding.EditorId = pickerHandle.Id;
      binding.UsesFilesystemPicker = true;
      binding.PathSelectsDirectories = pathSelectsDirectories;

      if (auto* picker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_Context->GetElement(pickerHandle.Id)))
      {
        picker->ReadOnly().Set(readOnly);
        picker->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        picker->Height().Set(SnAPI::UI::Sizing::Auto());
        picker->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
        picker->VAlign().Set(SnAPI::UI::EAlignment::Center);
        picker->AllowMultiSelect().Set(false);
        picker->PickDirectories().Set(pathSelectsDirectories);
        picker->ShowDirectories().Set(true);
        picker->ShowFiles().Set(!pathSelectsDirectories);
        picker->ShowHidden().Set(false);
        picker->RestrictToRoot().Set(true);
        picker->OutputScheme().Set(std::string("asset"));
        picker->Placeholder().Set(pathSelectsDirectories ? "asset://Folder" : "asset://file.ext");
        picker->BackgroundColor().Set(kValueBg);
        picker->BorderColor().Set(kValueBorder);
        picker->TextColor().Set(Color{224, 229, 237, 255});
        picker->PlaceholderColor().Set(Color{138, 147, 163, 255});
        picker->DropdownBackgroundColor().Set(Color{12, 15, 21, 252});
        picker->ItemHoverColor().Set(Color{26, 30, 39, 252});
        picker->ItemSelectedColor().Set(Color{52, 84, 130, 225});
        picker->ArrowColor().Set(Color{185, 193, 205, 255});

        const std::filesystem::path AssetRoot = ResolveAssetRootPath();
        if (!AssetRoot.empty())
        {
          picker->RootPath().Set(AssetRoot.string());
          picker->CurrentPath().Set(AssetRoot.string());
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
        if (allowCustomSchemePath)
        {
          editor->Placeholder().Set("primitive://box or asset://Meshes/mesh.glb");
        }
        else if (isPathField)
        {
          editor->Placeholder().Set(pathSelectsDirectories ? "asset://Folder" : "asset://file.ext");
        }
        else
        {
          editor->Placeholder().Set(PrettyTypeName(Field.FieldType));
        }
      }
    }
  }

  const std::size_t bindingIndex = m_Bindings.size();
  m_Bindings.push_back(std::move(binding));
  AttachEditorHooks(bindingIndex);
}

void UIPropertyPanel::AddMaterialScalarCollectionEditor(
  const SnAPI::UI::ElementId Parent,
  std::vector<MaterialScalarParamPayload>& Scalars,
  const bool ReadOnly,
  const std::string_view Heading)
{
  if (!m_Context)
  {
    return;
  }
  auto* scalarsPtr = &Scalars;

  const auto accordionHandle = m_Context->CreateElement<SnAPI::UI::UIAccordion>();
  if (accordionHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(Parent, accordionHandle.Id);

  auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id));
  if (!accordion)
  {
    return;
  }
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

  const auto bodyHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.MaterialScalarBody");
  if (bodyHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(accordionHandle.Id, bodyHandle.Id);

  auto* body = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(bodyHandle.Id));
  if (!body)
  {
    return;
  }
  body->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
  body->Padding().Set(2.0f);
  body->Gap().Set(4.0f);
  body->Width().Set(SnAPI::UI::Sizing::Fill());
  body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
  body->Background().Set(Color::Transparent());
  body->BorderColor().Set(Color::Transparent());
  body->BorderThickness().Set(0.0f);

  std::string heading(Heading);
  heading += " (" + std::to_string(scalarsPtr->size()) + ")";
  accordion->SetSectionHeading(bodyHandle.Id, std::move(heading));
  accordion->SetSectionExpanded(bodyHandle.Id, true);

  if (scalarsPtr->empty())
  {
    const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>("No scalar parameters");
    if (textHandle.Id.Value != 0)
    {
      m_Context->AddChild(bodyHandle.Id, textHandle.Id);
      if (auto* text = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(textHandle.Id)))
      {
        text->TextColor().Set(Color{152, 160, 176, 255});
      }
    }
    return;
  }

  for (size_t index = 0; index < scalarsPtr->size(); ++index)
  {
    const auto rowHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.Row");
    if (rowHandle.Id.Value == 0)
    {
      continue;
    }
    m_Context->AddChild(bodyHandle.Id, rowHandle.Id);

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

    const std::string labelText = (*scalarsPtr)[index].Name.empty()
                                    ? ("Scalar " + std::to_string(index))
                                    : (*scalarsPtr)[index].Name;
    const auto labelHandle = m_Context->CreateElement<SnAPI::UI::UIText>(labelText);
    if (labelHandle.Id.Value != 0)
    {
      m_Context->AddChild(rowHandle.Id, labelHandle.Id);
      if (auto* label = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(labelHandle.Id)))
      {
        label->Width().Set(SnAPI::UI::Sizing::Ratio(kLabelLaneRatio));
        label->TextColor().Set(kLabelColor);
        label->TextAlignment().Set(SnAPI::UI::ETextAlignment::Start);
        label->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
      }
    }

    const auto numberHandle = m_Context->CreateElement<SnAPI::UI::UINumberField>();
    if (numberHandle.Id.Value == 0)
    {
      continue;
    }
    m_Context->AddChild(rowHandle.Id, numberHandle.Id);

    if (auto* numberField = dynamic_cast<SnAPI::UI::UINumberField*>(&m_Context->GetElement(numberHandle.Id)))
    {
      numberField->Width().Set(SnAPI::UI::Sizing::Ratio(kValueLaneRatio));
      numberField->Height().Set(SnAPI::UI::Sizing::Auto());
      numberField->HAlign().Set(SnAPI::UI::EAlignment::Center);
      numberField->VAlign().Set(SnAPI::UI::EAlignment::Center);
      numberField->ShowSpinButtons().Set(!ReadOnly);
      numberField->AllowTextInput().Set(!ReadOnly);
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
      numberField->Precision().Set(kFloatEditorPrecision);
      numberField->Step().Set(0.01);
      numberField->MinValue().Set(-std::numeric_limits<double>::max());
      numberField->MaxValue().Set(std::numeric_limits<double>::max());
      (void)numberField->SetValue(static_cast<double>((*scalarsPtr)[index].Value), false);

      if (!ReadOnly)
      {
        numberField->Value().AddSetHook([scalarsPtr, index](const double value) {
          if (index < scalarsPtr->size())
          {
            (*scalarsPtr)[index].Value = static_cast<float>(value);
          }
        });
      }
    }
  }
}

void UIPropertyPanel::AddMaterialVectorCollectionEditor(
  const SnAPI::UI::ElementId Parent,
  std::vector<MaterialVectorParamPayload>& Vectors,
  const bool ReadOnly,
  const std::string_view Heading)
{
  if (!m_Context)
  {
    return;
  }
  auto* vectorsPtr = &Vectors;

  const auto accordionHandle = m_Context->CreateElement<SnAPI::UI::UIAccordion>();
  if (accordionHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(Parent, accordionHandle.Id);

  auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id));
  if (!accordion)
  {
    return;
  }
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

  const auto bodyHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.MaterialVectorBody");
  if (bodyHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(accordionHandle.Id, bodyHandle.Id);

  auto* body = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(bodyHandle.Id));
  if (!body)
  {
    return;
  }
  body->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
  body->Padding().Set(2.0f);
  body->Gap().Set(4.0f);
  body->Width().Set(SnAPI::UI::Sizing::Fill());
  body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
  body->Background().Set(Color::Transparent());
  body->BorderColor().Set(Color::Transparent());
  body->BorderThickness().Set(0.0f);

  std::string heading(Heading);
  heading += " (" + std::to_string(vectorsPtr->size()) + ")";
  accordion->SetSectionHeading(bodyHandle.Id, std::move(heading));
  accordion->SetSectionExpanded(bodyHandle.Id, true);

  if (vectorsPtr->empty())
  {
    const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>("No vector parameters");
    if (textHandle.Id.Value != 0)
    {
      m_Context->AddChild(bodyHandle.Id, textHandle.Id);
      if (auto* text = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(textHandle.Id)))
      {
        text->TextColor().Set(Color{152, 160, 176, 255});
      }
    }
    return;
  }

  for (size_t index = 0; index < vectorsPtr->size(); ++index)
  {
    const auto rowHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.Row");
    if (rowHandle.Id.Value == 0)
    {
      continue;
    }
    m_Context->AddChild(bodyHandle.Id, rowHandle.Id);

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

    const std::string labelText = (*vectorsPtr)[index].Name.empty()
                                    ? ("Vector " + std::to_string(index))
                                    : (*vectorsPtr)[index].Name;
    const auto labelHandle = m_Context->CreateElement<SnAPI::UI::UIText>(labelText);
    if (labelHandle.Id.Value != 0)
    {
      m_Context->AddChild(rowHandle.Id, labelHandle.Id);
      if (auto* label = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(labelHandle.Id)))
      {
        label->Width().Set(SnAPI::UI::Sizing::Ratio(kLabelLaneRatio));
        label->TextColor().Set(kLabelColor);
        label->TextAlignment().Set(SnAPI::UI::ETextAlignment::Start);
        label->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
      }
    }

    const auto valueHostHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.ValueHost");
    if (valueHostHandle.Id.Value == 0)
    {
      continue;
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

    for (size_t component = 0; component < 4; ++component)
    {
      const auto numberHandle = m_Context->CreateElement<SnAPI::UI::UINumberField>();
      if (numberHandle.Id.Value == 0)
      {
        continue;
      }
      m_Context->AddChild(valueHostHandle.Id, numberHandle.Id);

      if (auto* numberField = dynamic_cast<SnAPI::UI::UINumberField*>(&m_Context->GetElement(numberHandle.Id)))
      {
        numberField->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        numberField->Height().Set(SnAPI::UI::Sizing::Auto());
        numberField->HAlign().Set(SnAPI::UI::EAlignment::Center);
        numberField->VAlign().Set(SnAPI::UI::EAlignment::Center);
        numberField->ShowSpinButtons().Set(!ReadOnly);
        numberField->AllowTextInput().Set(!ReadOnly);
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
        numberField->Precision().Set(kFloatEditorPrecision);
        numberField->Step().Set(0.01);
        numberField->MinValue().Set(-std::numeric_limits<double>::max());
        numberField->MaxValue().Set(std::numeric_limits<double>::max());
        (void)numberField->SetValue(static_cast<double>((*vectorsPtr)[index].Value[component]), false);

        if (!ReadOnly)
        {
          numberField->Value().AddSetHook([vectorsPtr, index, component](const double value) {
            if (index < vectorsPtr->size() && component < (*vectorsPtr)[index].Value.size())
            {
              (*vectorsPtr)[index].Value[component] = static_cast<float>(value);
            }
          });
        }
      }
    }
  }
}

void UIPropertyPanel::AddMaterialTextureCollectionEditor(
  const SnAPI::UI::ElementId Parent,
  std::vector<MaterialTextureParamPayload>& Textures,
  const bool ReadOnly,
  const std::string_view Heading)
{
  if (!m_Context)
  {
    return;
  }
  auto* texturesPtr = &Textures;

  const std::optional<::SnAPI::AssetPipeline::TypeId> textureKind = TextureCompressorPlugin::AssetKind_CompressedTexture;

  const auto accordionHandle = m_Context->CreateElement<SnAPI::UI::UIAccordion>();
  if (accordionHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(Parent, accordionHandle.Id);

  auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id));
  if (!accordion)
  {
    return;
  }
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

  const auto bodyHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.MaterialTextureBody");
  if (bodyHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(accordionHandle.Id, bodyHandle.Id);

  auto* body = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(bodyHandle.Id));
  if (!body)
  {
    return;
  }
  body->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
  body->Padding().Set(2.0f);
  body->Gap().Set(4.0f);
  body->Width().Set(SnAPI::UI::Sizing::Fill());
  body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
  body->Background().Set(Color::Transparent());
  body->BorderColor().Set(Color::Transparent());
  body->BorderThickness().Set(0.0f);

  std::string heading(Heading);
  heading += " (" + std::to_string(texturesPtr->size()) + ")";
  accordion->SetSectionHeading(bodyHandle.Id, std::move(heading));
  accordion->SetSectionExpanded(bodyHandle.Id, true);

  if (texturesPtr->empty())
  {
    const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>("No texture parameters");
    if (textHandle.Id.Value != 0)
    {
      m_Context->AddChild(bodyHandle.Id, textHandle.Id);
      if (auto* text = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(textHandle.Id)))
      {
        text->TextColor().Set(Color{152, 160, 176, 255});
      }
    }
    return;
  }

  for (size_t index = 0; index < texturesPtr->size(); ++index)
  {
    const auto rowHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.Row");
    if (rowHandle.Id.Value == 0)
    {
      continue;
    }
    m_Context->AddChild(bodyHandle.Id, rowHandle.Id);

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

    const std::string labelText = (*texturesPtr)[index].SlotName.empty()
                                    ? ("Texture " + std::to_string(index))
                                    : (*texturesPtr)[index].SlotName;
    const auto labelHandle = m_Context->CreateElement<SnAPI::UI::UIText>(labelText);
    if (labelHandle.Id.Value != 0)
    {
      m_Context->AddChild(rowHandle.Id, labelHandle.Id);
      if (auto* label = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(labelHandle.Id)))
      {
        label->Width().Set(SnAPI::UI::Sizing::Ratio(kLabelLaneRatio));
        label->TextColor().Set(kLabelColor);
        label->TextAlignment().Set(SnAPI::UI::ETextAlignment::Start);
        label->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
      }
    }

    const auto valueHostHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.ValueHost");
    if (valueHostHandle.Id.Value == 0)
    {
      continue;
    }
    m_Context->AddChild(rowHandle.Id, valueHostHandle.Id);
    if (auto* valueHost = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(valueHostHandle.Id)))
    {
      valueHost->Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
      valueHost->Padding().Set(0.0f);
      valueHost->Gap().Set(6.0f);
      valueHost->Width().Set(SnAPI::UI::Sizing::Ratio(kValueLaneRatio));
      valueHost->HAlign().Set(SnAPI::UI::EAlignment::End);
      valueHost->VAlign().Set(SnAPI::UI::EAlignment::Center);
      valueHost->Background().Set(Color::Transparent());
      valueHost->BorderColor().Set(Color::Transparent());
      valueHost->BorderThickness().Set(0.0f);
    }

    const auto comboHandle = m_Context->CreateElement<SnAPI::UI::UIComboBox>();
    if (comboHandle.Id.Value != 0)
    {
      m_Context->AddChild(valueHostHandle.Id, comboHandle.Id);
      if (auto* combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(comboHandle.Id)))
      {
        combo->ReadOnly().Set(ReadOnly);
        combo->Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
        combo->Height().Set(SnAPI::UI::Sizing::Auto());
        combo->VAlign().Set(SnAPI::UI::EAlignment::Center);
        combo->MaxDropdownHeight().Set(220.0f);
        combo->Placeholder().Set("Texture");
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

        std::vector<std::string> options{};
        PopulateAssetRefPayloadOptions(options, textureKind);
        combo->SetItems(std::move(options));

        std::string selected{};
        (void)TryReadAssetRefPayloadSelectionLabel(&(*texturesPtr)[index].Texture, selected, textureKind);
        (void)combo->SelectByText(selected, false);

        if (!ReadOnly)
        {
          combo->SelectedIndex().AddSetHook([this, texturesPtr, index, textureKind, comboId = comboHandle.Id](const int32_t) {
            if (!m_Context || index >= texturesPtr->size())
            {
              return;
            }

            auto* liveCombo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(comboId));
            if (!liveCombo)
            {
              return;
            }

            const std::string selectedText = liveCombo->SelectedText();
            (void)TryWriteAssetRefPayloadSelection(&(*texturesPtr)[index].Texture, selectedText, textureKind);
          });
        }
      }
    }

    const auto checkboxHandle = m_Context->CreateElement<SnAPI::UI::UICheckbox>("sRGB");
    if (checkboxHandle.Id.Value != 0)
    {
      m_Context->AddChild(valueHostHandle.Id, checkboxHandle.Id);
      if (auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(checkboxHandle.Id)))
      {
        checkbox->Width().Set(SnAPI::UI::Sizing::Auto());
        checkbox->HAlign().Set(SnAPI::UI::EAlignment::End);
        checkbox->VAlign().Set(SnAPI::UI::EAlignment::Center);
        checkbox->LabelColor().Set(kLabelColor);
        checkbox->BoxSize().Set(12.0f);
        checkbox->CheckInset().Set(2.0f);
        checkbox->ElementStyle().Apply("editor.checkbox");
        checkbox->Checked().Set((*texturesPtr)[index].SRGB);
        checkbox->SetDisabled(ReadOnly);

        if (!ReadOnly)
        {
          checkbox->Checked().AddSetHook([texturesPtr, index](const bool value) {
            if (index < texturesPtr->size())
            {
              (*texturesPtr)[index].SRGB = value;
            }
          });
        }
      }
    }
  }
}

void UIPropertyPanel::AddAssetRefCollectionEditor(
  const SnAPI::UI::ElementId Parent,
  std::vector<AssetRefPayload>& References,
  const bool ReadOnly,
  const std::string_view Heading,
  const std::optional<::SnAPI::AssetPipeline::TypeId>& AssetKindFilter,
  const std::string_view RowPrefix)
{
  if (!m_Context)
  {
    return;
  }
  auto* refsPtr = &References;

  const auto accordionHandle = m_Context->CreateElement<SnAPI::UI::UIAccordion>();
  if (accordionHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(Parent, accordionHandle.Id);

  auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id));
  if (!accordion)
  {
    return;
  }
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

  const auto bodyHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.AssetRefListBody");
  if (bodyHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(accordionHandle.Id, bodyHandle.Id);

  auto* body = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(bodyHandle.Id));
  if (!body)
  {
    return;
  }
  body->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
  body->Padding().Set(2.0f);
  body->Gap().Set(4.0f);
  body->Width().Set(SnAPI::UI::Sizing::Fill());
  body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
  body->Background().Set(Color::Transparent());
  body->BorderColor().Set(Color::Transparent());
  body->BorderThickness().Set(0.0f);

  std::string heading(Heading);
  heading += " (" + std::to_string(refsPtr->size()) + ")";
  accordion->SetSectionHeading(bodyHandle.Id, std::move(heading));
  accordion->SetSectionExpanded(bodyHandle.Id, true);

  if (refsPtr->empty())
  {
    const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>("No entries");
    if (textHandle.Id.Value != 0)
    {
      m_Context->AddChild(bodyHandle.Id, textHandle.Id);
      if (auto* text = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(textHandle.Id)))
      {
        text->TextColor().Set(Color{152, 160, 176, 255});
      }
    }
    return;
  }

  for (size_t index = 0; index < refsPtr->size(); ++index)
  {
    const auto rowHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.Row");
    if (rowHandle.Id.Value == 0)
    {
      continue;
    }
    m_Context->AddChild(bodyHandle.Id, rowHandle.Id);

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

    std::string labelText(RowPrefix);
    labelText += " ";
    labelText += std::to_string(index);

    const auto labelHandle = m_Context->CreateElement<SnAPI::UI::UIText>(labelText);
    if (labelHandle.Id.Value != 0)
    {
      m_Context->AddChild(rowHandle.Id, labelHandle.Id);
      if (auto* label = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(labelHandle.Id)))
      {
        label->Width().Set(SnAPI::UI::Sizing::Ratio(kLabelLaneRatio));
        label->TextColor().Set(kLabelColor);
        label->TextAlignment().Set(SnAPI::UI::ETextAlignment::Start);
        label->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
      }
    }

    const auto comboHandle = m_Context->CreateElement<SnAPI::UI::UIComboBox>();
    if (comboHandle.Id.Value == 0)
    {
      continue;
    }
    m_Context->AddChild(rowHandle.Id, comboHandle.Id);

    if (auto* combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(comboHandle.Id)))
    {
      combo->ReadOnly().Set(ReadOnly);
      combo->Width().Set(SnAPI::UI::Sizing::Ratio(kValueLaneRatio));
      combo->Height().Set(SnAPI::UI::Sizing::Auto());
      combo->VAlign().Set(SnAPI::UI::EAlignment::Center);
      combo->MaxDropdownHeight().Set(220.0f);
      combo->Placeholder().Set("Asset");
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

      std::vector<std::string> options{};
      PopulateAssetRefPayloadOptions(options, AssetKindFilter);
      combo->SetItems(std::move(options));

      std::string selected{};
      (void)TryReadAssetRefPayloadSelectionLabel(&(*refsPtr)[index], selected, AssetKindFilter);
      (void)combo->SelectByText(selected, false);

      if (!ReadOnly)
      {
        combo->SelectedIndex().AddSetHook([this, refsPtr, index, AssetKindFilter, comboId = comboHandle.Id](const int32_t) {
          if (!m_Context || index >= refsPtr->size())
          {
            return;
          }

          auto* liveCombo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(comboId));
          if (!liveCombo)
          {
            return;
          }

          const std::string selectedText = liveCombo->SelectedText();
          (void)TryWriteAssetRefPayloadSelection(&(*refsPtr)[index], selectedText, AssetKindFilter);
        });
      }
    }
  }
}

void UIPropertyPanel::AddMaterialInstanceAssetRefCollectionEditor(
  const SnAPI::UI::ElementId Parent,
  std::vector<TAssetRef<MaterialInstanceAssetRuntime>>& References,
  const bool ReadOnly,
  const std::string_view Heading,
  const std::string_view RowPrefix)
{
  if (!m_Context)
  {
    return;
  }
  auto* refsPtr = &References;

  const auto accordionHandle = m_Context->CreateElement<SnAPI::UI::UIAccordion>();
  if (accordionHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(Parent, accordionHandle.Id);

  auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(accordionHandle.Id));
  if (!accordion)
  {
    return;
  }
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

  const auto bodyHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.MaterialOverrideBody");
  if (bodyHandle.Id.Value == 0)
  {
    return;
  }
  m_Context->AddChild(accordionHandle.Id, bodyHandle.Id);

  auto* body = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(bodyHandle.Id));
  if (!body)
  {
    return;
  }
  body->Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
  body->Padding().Set(2.0f);
  body->Gap().Set(4.0f);
  body->Width().Set(SnAPI::UI::Sizing::Fill());
  body->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
  body->Background().Set(Color::Transparent());
  body->BorderColor().Set(Color::Transparent());
  body->BorderThickness().Set(0.0f);

  std::string heading(Heading);
  heading += " (" + std::to_string(refsPtr->size()) + ")";
  accordion->SetSectionHeading(bodyHandle.Id, std::move(heading));
  accordion->SetSectionExpanded(bodyHandle.Id, true);

  const auto notifyOverridesChanged = [this]() {
#if defined(WITH_EDITOR) && WITH_EDITOR
    if (m_BoundInstance)
    {
      if (const TypeInfo* rootType = TypeRegistry::Instance().Find(m_BoundType);
          rootType && rootType->EditorPropertyChanged)
      {
        rootType->EditorPropertyChanged(m_BoundInstance, "MaterialInstanceOverrides");
      }
    }
#endif
  };

  const auto controlsHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.MaterialOverrideControls");
  if (controlsHandle.Id.Value != 0)
  {
    m_Context->AddChild(bodyHandle.Id, controlsHandle.Id);
    if (auto* controls = dynamic_cast<SnAPI::UI::UIPanel*>(&m_Context->GetElement(controlsHandle.Id)))
    {
      controls->Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
      controls->Padding().Set(0.0f);
      controls->Gap().Set(6.0f);
      controls->Width().Set(SnAPI::UI::Sizing::Fill());
      controls->HAlign().Set(SnAPI::UI::EAlignment::Stretch);
      controls->Background().Set(Color::Transparent());
      controls->BorderColor().Set(Color::Transparent());
      controls->BorderThickness().Set(0.0f);
    }

    const auto createActionButton = [this, controlsHandle](
      const std::string_view label,
      const bool enabled,
      std::function<void()> onClick) {
      if (!m_Context)
      {
        return;
      }

      const auto buttonHandle = m_Context->CreateElement<SnAPI::UI::UIButton>();
      if (buttonHandle.Id.Value == 0)
      {
        return;
      }
      m_Context->AddChild(controlsHandle.Id, buttonHandle.Id);

      if (auto* button = dynamic_cast<SnAPI::UI::UIButton*>(&m_Context->GetElement(buttonHandle.Id)))
      {
        button->Width().Set(SnAPI::UI::Sizing::Auto());
        button->Height().Set(SnAPI::UI::Sizing::Auto());
        button->ElementPadding().Set(SnAPI::UI::Padding{6.0f, 6.0f, 6.0f, 6.0f});
        button->CornerRadius().Set(3.0f);
        button->Background().Set(Color{31, 36, 46, 252});
        button->BorderColor().Set(kValueBorder);
        button->BorderThickness().Set(1.0f);
        button->SetDisabled(!enabled);

        if (enabled)
        {
          button->OnClick([handler = std::move(onClick)]() {
            if (handler)
            {
              handler();
            }
          });
        }
      }

      const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>(std::string(label));
      if (textHandle.Id.Value == 0)
      {
        return;
      }
      m_Context->AddChild(buttonHandle.Id, textHandle.Id);
      if (auto* text = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(textHandle.Id)))
      {
        text->TextColor().Set(Color{224, 229, 237, 255});
        text->TextAlignment().Set(SnAPI::UI::ETextAlignment::Center);
        text->Wrapping().Set(SnAPI::UI::ETextWrapping::NoWrap);
        text->Properties().SetProperty(
          SnAPI::UI::UIElementBase::VisibilityKey,
          SnAPI::UI::EVisibility::HitTestInvisible);
      }
    };

    const bool canEdit = !ReadOnly;
    createActionButton("+ Add Slot", canEdit, [this, refsPtr, notifyOverridesChanged]() {
      refsPtr->emplace_back();
      notifyOverridesChanged();
      (void)RebuildUi();
      SyncModelToEditors();
    });
    createActionButton("- Remove Last", canEdit && !refsPtr->empty(), [this, refsPtr, notifyOverridesChanged]() {
      if (!refsPtr->empty())
      {
        refsPtr->pop_back();
        notifyOverridesChanged();
      }
      (void)RebuildUi();
      SyncModelToEditors();
    });
  }

  if (refsPtr->empty())
  {
    const auto textHandle = m_Context->CreateElement<SnAPI::UI::UIText>("No slots available. Use + Add Slot.");
    if (textHandle.Id.Value != 0)
    {
      m_Context->AddChild(bodyHandle.Id, textHandle.Id);
      if (auto* text = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(textHandle.Id)))
      {
        text->TextColor().Set(Color{152, 160, 176, 255});
      }
    }
    return;
  }

  for (size_t index = 0; index < refsPtr->size(); ++index)
  {
    const auto rowHandle = m_Context->CreateElement<SnAPI::UI::UIPanel>("PropertyPanel.Row");
    if (rowHandle.Id.Value == 0)
    {
      continue;
    }
    m_Context->AddChild(bodyHandle.Id, rowHandle.Id);

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

    std::string labelText(RowPrefix);
    labelText += " ";
    labelText += std::to_string(index);

    const auto labelHandle = m_Context->CreateElement<SnAPI::UI::UIText>(labelText);
    if (labelHandle.Id.Value != 0)
    {
      m_Context->AddChild(rowHandle.Id, labelHandle.Id);
      if (auto* label = dynamic_cast<SnAPI::UI::UIText*>(&m_Context->GetElement(labelHandle.Id)))
      {
        label->Width().Set(SnAPI::UI::Sizing::Ratio(kLabelLaneRatio));
        label->TextColor().Set(kLabelColor);
        label->TextAlignment().Set(SnAPI::UI::ETextAlignment::Start);
        label->Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
      }
    }

    const auto comboHandle = m_Context->CreateElement<SnAPI::UI::UIComboBox>();
    if (comboHandle.Id.Value == 0)
    {
      continue;
    }
    m_Context->AddChild(rowHandle.Id, comboHandle.Id);

    if (auto* combo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(comboHandle.Id)))
    {
      combo->ReadOnly().Set(ReadOnly);
      combo->Width().Set(SnAPI::UI::Sizing::Ratio(kValueLaneRatio));
      combo->Height().Set(SnAPI::UI::Sizing::Auto());
      combo->VAlign().Set(SnAPI::UI::EAlignment::Center);
      combo->MaxDropdownHeight().Set(220.0f);
      combo->Placeholder().Set("Material Instance");
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

      std::vector<std::string> options{};
      PopulateAssetRefOptions<TAssetRef<MaterialInstanceAssetRuntime>>(options);
      combo->SetItems(std::move(options));

      std::string selected{};
      (void)TryReadAssetRefSelectionLabel<TAssetRef<MaterialInstanceAssetRuntime>>(&(*refsPtr)[index], selected);
      (void)combo->SelectByText(selected, false);

      if (!ReadOnly)
      {
        combo->SelectedIndex().AddSetHook([this, refsPtr, index, comboId = comboHandle.Id](const int32_t) {
          if (!m_Context || index >= refsPtr->size())
          {
            return;
          }

          auto* liveCombo = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(comboId));
          if (!liveCombo)
          {
            return;
          }

          const std::string selectedText = liveCombo->SelectedText();
          (void)TryWriteAssetRefSelection<TAssetRef<MaterialInstanceAssetRuntime>>(&(*refsPtr)[index], selectedText);

          if (m_BoundInstance)
          {
#if defined(WITH_EDITOR) && WITH_EDITOR
            if (const TypeInfo* rootType = TypeRegistry::Instance().Find(m_BoundType);
                rootType && rootType->EditorPropertyChanged)
            {
              rootType->EditorPropertyChanged(m_BoundInstance, "MaterialInstanceOverrides");
            }
#endif
          }
        });
      }
    }
  }
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
  if (Type == StaticTypeId<int>() || Type == StaticTypeId<std::int64_t>())
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
  if (Type == StaticTypeId<TypeId>())
  {
    return EEditorKind::TypeId;
  }
  if (Type == StaticTypeId<Uuid>())
  {
    return EEditorKind::Uuid;
  }
  if (Type == StaticTypeId<AssetRefPayload>())
  {
    return EEditorKind::AssetRef;
  }

  if (const TypeInfo* TypeInfo = TypeRegistry::Instance().Find(Type); TypeInfo)
  {
    switch (TypeInfo->EditorValueFamily)
    {
    case EEditorValueFamily::Flags:
      return EEditorKind::Flags;
    case EEditorValueFamily::AssetRef:
      return EEditorKind::AssetRef;
    case EEditorValueFamily::SubClassOf:
      return EEditorKind::SubClass;
    case EEditorValueFamily::None:
    default:
      break;
    }

    if (TypeInfo->IsEnum)
    {
      return EEditorKind::Enum;
    }
  }

  return EEditorKind::Unsupported;
}

bool UIPropertyPanel::IsNestedStructType(const TypeId& Type) const
{
  if (ResolveEditorKind(Type) != EEditorKind::Unsupported)
  {
    return false;
  }

  const auto reflectedFields = TypeRegistry::Instance().CollectFields(Type);
  return !reflectedFields.empty();
}

std::string UIPropertyPanel::PrettyTypeName(const TypeId& Type) const
{
  if (const TypeInfo* typeInfo = TypeRegistry::Instance().Find(Type))
  {
    return PrettyReflectedTypeName(typeInfo->Name);
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

bool UIPropertyPanel::IsPathLikeField(const FieldInfo& Field) const
{
  const std::string NameLower = ToLowerCopy(Field.Name);
  return NameLower.find("path") != std::string::npos ||
         NameLower.find("file") != std::string::npos ||
         NameLower.find("folder") != std::string::npos ||
         NameLower.find("directory") != std::string::npos;
}

bool UIPropertyPanel::IsDirectoryLikeField(const FieldInfo& Field) const
{
  const std::string NameLower = ToLowerCopy(Field.Name);
  return NameLower.find("directory") != std::string::npos ||
         NameLower.find("folder") != std::string::npos ||
         NameLower.ends_with("dir");
}

std::filesystem::path UIPropertyPanel::ResolveAssetRootPath() const
{
  std::filesystem::path AssetRoot = SPathResolver::Instance().AssetRoot();
  if (!AssetRoot.empty())
  {
    return AssetRoot;
  }

  std::error_code Error{};
  const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
  if (Error || CurrentPath.empty())
  {
    return {};
  }

  return CurrentPath / "Assets";
}

std::string UIPropertyPanel::NormalizeToAssetUri(const std::string_view Candidate) const
{
  const std::string Trimmed = TrimCopy(Candidate);
  if (Trimmed.empty())
  {
    return {};
  }

  if (Trimmed.rfind("asset://", 0) == 0)
  {
    return Trimmed;
  }

  auto ResolvedPath = SPathResolver::Instance().Resolve(Trimmed);
  if (!ResolvedPath)
  {
    return Trimmed;
  }

  std::filesystem::path AssetRoot = ResolveAssetRootPath();
  if (AssetRoot.empty())
  {
    return Trimmed;
  }

  std::error_code Error{};
  AssetRoot = std::filesystem::weakly_canonical(AssetRoot, Error);
  if (Error || AssetRoot.empty())
  {
    Error.clear();
    AssetRoot = std::filesystem::absolute(AssetRoot, Error);
    if (Error)
    {
      return Trimmed;
    }
  }

  std::filesystem::path Resolved = std::filesystem::weakly_canonical(*ResolvedPath, Error);
  if (Error || Resolved.empty())
  {
    Error.clear();
    Resolved = std::filesystem::absolute(*ResolvedPath, Error);
    if (Error)
    {
      return Trimmed;
    }
  }

  std::filesystem::path Relative = std::filesystem::relative(Resolved, AssetRoot, Error);
  if (Error || Relative.empty() || Relative.is_absolute())
  {
    return Trimmed;
  }

  Relative = Relative.lexically_normal();
  const std::string RelativeText = Relative.generic_string();
  if (RelativeText.empty() || RelativeText.rfind("..", 0) == 0)
  {
    return Trimmed;
  }

  return "asset://" + RelativeText;
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
        next = view->UnsafeBorrowedMutable();
        if (!next)
        {
          next = const_cast<void*>(view->UnsafeBorrowed());
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

  const TypeInfo* fieldTypeInfo = TypeRegistry::Instance().Find(Binding.FieldType);

  if (Binding.EditorKind == EEditorKind::SubClass)
  {
    if (!fieldTypeInfo || !field->ConstPointer)
    {
      return false;
    }

    const void* constPointer = field->ConstPointer(owner);
    if (!constPointer || !TryReadEditorValueSelectionLabel(*fieldTypeInfo, constPointer, OutText))
    {
      return false;
    }

    OutBool = false;
    return true;
  }

  if (Binding.EditorKind == EEditorKind::Flags)
  {
    const TypeInfo* enumInfo = nullptr;
    std::uint64_t bits = 0;
    if (!ReadFlagsBits(Binding, bits, enumInfo) || !enumInfo)
    {
      return false;
    }

    OutText = FormatFlagsSummary(bits, *enumInfo);
    OutBool = false;
    return true;
  }

  if (Binding.EditorKind == EEditorKind::TypeId)
  {
    if (!field->ConstPointer)
    {
      return false;
    }

    const void* constPointer = field->ConstPointer(owner);
    if (!constPointer || !TryReadTypeIdSelectionLabel(constPointer, OutText))
    {
      return false;
    }

    OutBool = false;
    return true;
  }

  if (Binding.EditorKind == EEditorKind::AssetRef)
  {
    if (!field->ConstPointer)
    {
      return false;
    }

    const void* constPointer = field->ConstPointer(owner);
    if (!constPointer)
    {
      return false;
    }

    if (Binding.FieldType == StaticTypeId<AssetRefPayload>())
    {
      if (!TryReadAssetRefPayloadSelectionLabel(constPointer, OutText))
      {
        return false;
      }
      OutBool = false;
      return true;
    }

    if (!fieldTypeInfo || !TryReadEditorValueSelectionLabel(*fieldTypeInfo, constPointer, OutText))
    {
      return false;
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
      if (Binding.FieldType == StaticTypeId<int>())
      {
        const auto ref = value.AsConstRef<int>();
        if (!ref)
        {
          return false;
        }
        OutText = std::to_string(ref->get());
        return true;
      }
      if (Binding.FieldType == StaticTypeId<std::int64_t>())
      {
        const auto ref = value.AsConstRef<std::int64_t>();
        if (!ref)
        {
          return false;
        }
        OutText = std::to_string(ref->get());
        return true;
      }
      return false;
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
  case EEditorKind::TypeId:
    {
      const auto ref = value.AsConstRef<TypeId>();
      if (!ref)
      {
        return false;
      }
      OutText = ToString(ref->get());
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
  case EEditorKind::Flags:
  case EEditorKind::SubClass:
  case EEditorKind::AssetRef:
    return false;
  case EEditorKind::Unsupported:
  default:
    return false;
  }
}

bool UIPropertyPanel::ReadFlagsBits(
  const FieldBinding& Binding,
  std::uint64_t& OutBits,
  const TypeInfo*& OutEnumInfo) const
{
  OutBits = 0;
  OutEnumInfo = nullptr;

  if (!Binding.RootInstance)
  {
    return false;
  }

  void* owner = nullptr;
  const FieldInfo* field = nullptr;
  if (!ResolveLeafPath(Binding.RootInstance, Binding.Path, owner, field) || !field || !field->ConstPointer)
  {
    return false;
  }

  const TypeInfo* fieldTypeInfo = TypeRegistry::Instance().Find(Binding.FieldType);
  if (!fieldTypeInfo || fieldTypeInfo->EditorValueFamily != EEditorValueFamily::Flags ||
      fieldTypeInfo->EditorValueTargetType.is_nil())
  {
    return false;
  }

  const TypeInfo* enumInfo = TypeRegistry::Instance().Find(fieldTypeInfo->EditorValueTargetType);
  if (!enumInfo || !enumInfo->IsEnum)
  {
    return false;
  }

  if (!ReadUnsignedBits(field->ConstPointer(owner), fieldTypeInfo->Size, OutBits))
  {
    return false;
  }

  OutEnumInfo = enumInfo;
  return true;
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
    Binding.EditorKind != EEditorKind::Flags &&
    Binding.EditorKind != EEditorKind::TypeId &&
    Binding.EditorKind != EEditorKind::SubClass &&
    Binding.EditorKind != EEditorKind::AssetRef;
  if (!ResolveLeafPath(Binding.RootInstance, Binding.Path, owner, field) || !field || field->IsConst ||
      (requiresSetter && !field->Setter))
  {
    return false;
  }

  const TypeInfo* fieldTypeInfo = TypeRegistry::Instance().Find(Binding.FieldType);

  const auto notifyRootEditorPropertyChanged = [&]() {
#if defined(WITH_EDITOR) && WITH_EDITOR
    if (Binding.Path.empty())
    {
      return;
    }

    const FieldPathEntry& rootEntry = Binding.Path.front();
    if (const TypeInfo* rootType = TypeRegistry::Instance().Find(rootEntry.OwnerType);
        rootType && rootType->EditorPropertyChanged)
    {
      rootType->EditorPropertyChanged(Binding.RootInstance, Binding.Path.back().FieldName);
    }
#endif
  };

  const auto finalizeWrite = [&](const bool Success) {
    if (!Success)
    {
      return false;
    }

#if defined(WITH_EDITOR) && WITH_EDITOR
    // Nested settings fields mutate sub-objects directly; forward a root-level editor change callback.
    if (Binding.Path.size() > 1 &&
        Binding.EditorKind != EEditorKind::Enum &&
        Binding.EditorKind != EEditorKind::Flags &&
        Binding.EditorKind != EEditorKind::TypeId &&
        Binding.EditorKind != EEditorKind::SubClass &&
        Binding.EditorKind != EEditorKind::AssetRef)
    {
      notifyRootEditorPropertyChanged();
    }
#endif

    return true;
  };

  switch (Binding.EditorKind)
  {
  case EEditorKind::Bool:
    return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(BoolValue))));
  case EEditorKind::Signed:
    {
      std::int64_t parsed = 0;
      if (!ParseSigned(TextValue, parsed))
      {
        return false;
      }

      if (Binding.FieldType == StaticTypeId<int>())
      {
        if (parsed < static_cast<std::int64_t>(std::numeric_limits<int>::min()) ||
            parsed > static_cast<std::int64_t>(std::numeric_limits<int>::max()))
        {
          return false;
        }
        return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(static_cast<int>(parsed)))));
      }

      if (Binding.FieldType == StaticTypeId<std::int64_t>())
      {
        return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed))));
      }

      return false;
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
        return finalizeWrite(static_cast<bool>(
          field->Setter(owner, Variant::FromValue(static_cast<unsigned int>(parsed)))));
      }

      if (Binding.FieldType == StaticTypeId<std::uint64_t>())
      {
        return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed))));
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
      return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(static_cast<float>(parsed)))));
    }
  case EEditorKind::Double:
    {
      double parsed = 0.0;
      if (!ParseDouble(TextValue, parsed) || !std::isfinite(parsed))
      {
        return false;
      }
      return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed))));
    }
  case EEditorKind::String:
    {
      std::string nextValue = std::string(TextValue);
      if (Binding.UsesFilesystemPicker || IsPathLikeField(*field))
      {
        nextValue = NormalizeToAssetUri(nextValue);
      }
      return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(nextValue))));
    }
  case EEditorKind::Vec2:
    {
      Vec2 parsed{};
      if (!ParseVec2(TextValue, parsed))
      {
        return false;
      }
      return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed))));
    }
  case EEditorKind::Vec3:
    {
      Vec3 parsed{};
      if (!ParseVec3(TextValue, parsed))
      {
        return false;
      }
      return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed))));
    }
  case EEditorKind::Vec4:
    {
      Vec4 parsed{};
      if (!ParseVec4(TextValue, parsed))
      {
        return false;
      }
      return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed))));
    }
  case EEditorKind::Quat:
    {
      Quat parsed = Quat::Identity();
      if (!ParseQuat(TextValue, parsed))
      {
        return false;
      }
      return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed))));
    }
  case EEditorKind::Color:
    {
      SnAPI::UI::Color parsed{};
      if (!ParseColor(TextValue, parsed))
      {
        return false;
      }
      return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed))));
    }
  case EEditorKind::TypeId:
    {
      if (!field->MutablePointer)
      {
        return false;
      }

      void* mutablePointer = field->MutablePointer(owner);
      if (!mutablePointer)
      {
        return false;
      }

      const bool success = TryWriteTypeIdSelection(mutablePointer, TextValue);
      if (success)
      {
        notifyRootEditorPropertyChanged();
      }
      return finalizeWrite(success);
    }
  case EEditorKind::Uuid:
    {
      Uuid parsed{};
      if (!ParseUuid(TextValue, parsed))
      {
        return false;
      }
      return finalizeWrite(static_cast<bool>(field->Setter(owner, Variant::FromValue(parsed))));
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
      const bool success = WriteUnsignedBits(destination, enumInfo->Size, enumBits);
      if (success)
      {
        notifyRootEditorPropertyChanged();
      }
      return finalizeWrite(success);
    }
  case EEditorKind::Flags:
    return false;
  case EEditorKind::SubClass:
    {
      if (!fieldTypeInfo || !field->MutablePointer)
      {
        return false;
      }

      void* mutablePointer = field->MutablePointer(owner);
      if (!mutablePointer)
      {
        return false;
      }

      const std::string selected = TrimCopy(TextValue);
      if (selected.empty())
      {
        const bool success = TryWriteEditorValueSelection(*fieldTypeInfo, mutablePointer, selected);
        if (success)
        {
          notifyRootEditorPropertyChanged();
        }
        return finalizeWrite(success);
      }

      const bool success = TryWriteEditorValueSelection(*fieldTypeInfo, mutablePointer, selected);
      if (success)
      {
        notifyRootEditorPropertyChanged();
      }
      return finalizeWrite(success);
    }
  case EEditorKind::AssetRef:
    {
      if (!field->MutablePointer)
      {
        return false;
      }

      void* mutablePointer = field->MutablePointer(owner);
      if (!mutablePointer)
      {
        return false;
      }

      const std::string selected = TrimCopy(TextValue);
      if (Binding.FieldType == StaticTypeId<AssetRefPayload>())
      {
        const bool success = TryWriteAssetRefPayloadSelection(mutablePointer, selected);
        if (success)
        {
          notifyRootEditorPropertyChanged();
        }
        return finalizeWrite(success);
      }
      if (!fieldTypeInfo)
      {
        return false;
      }
      const bool success = TryWriteEditorValueSelection(*fieldTypeInfo, mutablePointer, selected);
      if (success)
      {
        notifyRootEditorPropertyChanged();
      }
      return finalizeWrite(success);
    }
  case EEditorKind::Unsupported:
  default:
    return false;
  }
}

bool UIPropertyPanel::WriteFlagsBits(
  const FieldBinding& Binding,
  const std::uint64_t Bits)
{
  if (!Binding.RootInstance || Binding.ReadOnly)
  {
    return false;
  }

  void* owner = nullptr;
  const FieldInfo* field = nullptr;
  if (!ResolveLeafPath(Binding.RootInstance, Binding.Path, owner, field) || !field || field->IsConst || !field->MutablePointer)
  {
    return false;
  }

  const TypeInfo* fieldTypeInfo = TypeRegistry::Instance().Find(Binding.FieldType);
  if (!fieldTypeInfo || fieldTypeInfo->EditorValueFamily != EEditorValueFamily::Flags)
  {
    return false;
  }

  void* destination = field->MutablePointer(owner);
  if (!destination)
  {
    return false;
  }

  return WriteUnsignedBits(destination, fieldTypeInfo->Size, Bits);
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

  if (Binding.EditorKind == EEditorKind::Flags)
  {
    for (const SnAPI::UI::ElementId checkboxId : Binding.AuxiliaryEditorIds)
    {
      if (checkboxId.Value == 0)
      {
        continue;
      }

      auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(checkboxId));
      if (checkbox && checkbox->IsFocused())
      {
        return true;
      }
    }
    return false;
  }

  if (Binding.EditorKind == EEditorKind::Enum ||
      Binding.EditorKind == EEditorKind::TypeId ||
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
  if (textInput)
  {
    return textInput->IsFocused();
  }

  auto* filesystemPicker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_Context->GetElement(Binding.EditorId));
  return filesystemPicker && filesystemPicker->IsFocused();
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

  if (binding.EditorKind == EEditorKind::Flags)
  {
    const TypeInfo* fieldTypeInfo = TypeRegistry::Instance().Find(binding.FieldType);
    const TypeInfo* enumInfo =
      (fieldTypeInfo && !fieldTypeInfo->EditorValueTargetType.is_nil())
        ? TypeRegistry::Instance().Find(fieldTypeInfo->EditorValueTargetType)
        : nullptr;
    if (!enumInfo)
    {
      return;
    }

    const auto entries = BuildFlagEditorEntries(*enumInfo);
    if (entries.empty())
    {
      return;
    }

    if (binding.AuxiliaryHookHandles.size() < binding.AuxiliaryEditorIds.size())
    {
      binding.AuxiliaryHookHandles.resize(binding.AuxiliaryEditorIds.size(), 0);
    }

    for (std::size_t index = 0; index < binding.AuxiliaryEditorIds.size() && index < entries.size(); ++index)
    {
      const SnAPI::UI::ElementId checkboxId = binding.AuxiliaryEditorIds[index];
      if (checkboxId.Value == 0)
      {
        continue;
      }

      auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(checkboxId));
      if (!checkbox)
      {
        continue;
      }

      binding.AuxiliaryHookHandles[index] = checkbox->Checked().AddSetHook(
        [this, BindingIndex, generation](const bool /*Value*/) {
          if (!m_Context || m_SyncingModelToEditors || m_CommittingEditorToModel)
          {
            return;
          }

          RefreshBoundSectionInstances();

          FieldBinding* liveBinding = ResolveLiveBinding(BindingIndex, generation);
          if (!liveBinding || liveBinding->ReadOnly)
          {
            return;
          }

          const TypeInfo* liveFieldTypeInfo = TypeRegistry::Instance().Find(liveBinding->FieldType);
          const TypeInfo* liveEnumInfo =
            (liveFieldTypeInfo && !liveFieldTypeInfo->EditorValueTargetType.is_nil())
              ? TypeRegistry::Instance().Find(liveFieldTypeInfo->EditorValueTargetType)
              : nullptr;
          if (!liveEnumInfo)
          {
            return;
          }

          const auto liveEntries = BuildFlagEditorEntries(*liveEnumInfo);
          std::uint64_t bits = 0;
          for (std::size_t liveIndex = 0;
               liveIndex < liveBinding->AuxiliaryEditorIds.size() && liveIndex < liveEntries.size();
               ++liveIndex)
          {
            const SnAPI::UI::ElementId liveCheckboxId = liveBinding->AuxiliaryEditorIds[liveIndex];
            if (liveCheckboxId.Value == 0)
            {
              continue;
            }

            auto* liveCheckbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(liveCheckboxId));
            if (liveCheckbox && liveCheckbox->Properties().GetPropertyOr(SnAPI::UI::UICheckbox::CheckedKey, false))
            {
              bits |= liveEntries[liveIndex].Value;
            }
          }

          ScopedFlag guard(m_CommittingEditorToModel);
          if (!WriteFlagsBits(*liveBinding, bits))
          {
            if (!IsEditorFocused(*liveBinding))
            {
              SyncBindingToEditor(*liveBinding);
            }
            return;
          }

#if defined(WITH_EDITOR) && WITH_EDITOR
          if (!liveBinding->Path.empty())
          {
            const FieldPathEntry& rootEntry = liveBinding->Path.front();
            if (const TypeInfo* rootType = TypeRegistry::Instance().Find(rootEntry.OwnerType);
                rootType && rootType->EditorPropertyChanged)
            {
              rootType->EditorPropertyChanged(liveBinding->RootInstance, liveBinding->Path.back().FieldName);
            }
          }
#endif

          SyncBindingToEditor(*liveBinding);
        });
    }
    return;
  }

  if (binding.EditorKind == EEditorKind::Enum ||
      binding.EditorKind == EEditorKind::TypeId ||
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

  if (binding.UsesFilesystemPicker)
  {
    auto* filesystemPicker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_Context->GetElement(binding.EditorId));
    if (!filesystemPicker)
    {
      return;
    }

    binding.EditorHookHandle =
      filesystemPicker->Value().AddSetHook([this, BindingIndex, generation](const std::string& Value) {
        CommitBindingFromEditor(BindingIndex, generation, Value, false);
      });
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
    if (binding.EditorKind == EEditorKind::Flags)
    {
      for (std::size_t index = 0; index < binding.AuxiliaryEditorIds.size() && index < binding.AuxiliaryHookHandles.size(); ++index)
      {
        const SnAPI::UI::ElementId checkboxId = binding.AuxiliaryEditorIds[index];
        const std::size_t handle = binding.AuxiliaryHookHandles[index];
        if (checkboxId.Value == 0 || handle == 0)
        {
          continue;
        }

        if (auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(checkboxId)))
        {
          (void)checkbox->Checked().RemoveSetHook(handle);
        }
        binding.AuxiliaryHookHandles[index] = 0;
      }
    }

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
               binding.EditorKind == EEditorKind::TypeId ||
               binding.EditorKind == EEditorKind::SubClass ||
               binding.EditorKind == EEditorKind::AssetRef)
      {
        if (auto* comboBox = dynamic_cast<SnAPI::UI::UIComboBox*>(&m_Context->GetElement(binding.EditorId)))
        {
          (void)comboBox->SelectedIndex().RemoveSetHook(binding.EditorHookHandle);
        }
      }
      else if (binding.UsesFilesystemPicker)
      {
        if (auto* filesystemPicker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&m_Context->GetElement(binding.EditorId)))
        {
          (void)filesystemPicker->Value().RemoveSetHook(binding.EditorHookHandle);
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

  RefreshBoundSectionInstances();
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

  RefreshBoundSectionInstances();
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

  if (Binding.EditorKind == EEditorKind::Flags)
  {
    const TypeInfo* enumInfo = nullptr;
    std::uint64_t bits = 0;
    if (!ReadFlagsBits(Binding, bits, enumInfo) || !enumInfo)
    {
      return;
    }

    if (Binding.EditorId.Value != 0)
    {
      if (auto* accordion = dynamic_cast<SnAPI::UI::UIAccordion*>(&m_Context->GetElement(Binding.EditorId)))
      {
        accordion->SetSectionHeading(Binding.AuxiliaryContainerId, FormatFlagsSummary(bits, *enumInfo));
      }
    }

    if (IsEditorFocused(Binding))
    {
      return;
    }

    const auto entries = BuildFlagEditorEntries(*enumInfo);
    for (std::size_t index = 0; index < Binding.AuxiliaryEditorIds.size() && index < entries.size(); ++index)
    {
      const SnAPI::UI::ElementId checkboxId = Binding.AuxiliaryEditorIds[index];
      if (checkboxId.Value == 0)
      {
        continue;
      }

      auto* checkbox = dynamic_cast<SnAPI::UI::UICheckbox*>(&m_Context->GetElement(checkboxId));
      if (!checkbox)
      {
        continue;
      }

      const bool expected = (bits & entries[index].Value) != 0;
      if (checkbox->Properties().GetPropertyOr(SnAPI::UI::UICheckbox::CheckedKey, false) != expected)
      {
        checkbox->Checked().Set(expected);
      }
    }
    return;
  }

  if (Binding.EditorKind == EEditorKind::Enum ||
      Binding.EditorKind == EEditorKind::TypeId ||
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

  if (Binding.UsesFilesystemPicker)
  {
    auto* filesystemPicker = dynamic_cast<SnAPI::UI::UIFilesystemPicker*>(&element);
    if (!filesystemPicker || filesystemPicker->IsFocused())
    {
      return;
    }

    const std::filesystem::path AssetRoot = ResolveAssetRootPath();
    if (!AssetRoot.empty())
    {
      const std::string RootText = AssetRoot.string();
      if (filesystemPicker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::RootPathKey, std::string{}) != RootText)
      {
        filesystemPicker->RootPath().Set(RootText);
      }
    }

    const std::string currentPickerValue =
      filesystemPicker->Properties().GetPropertyOr(SnAPI::UI::UIFilesystemPicker::ValueKey, std::string{});
    if (currentPickerValue != textValue)
    {
      filesystemPicker->Value().Set(textValue);
    }

    if (auto ResolvedPath = SPathResolver::Instance().Resolve(textValue); ResolvedPath)
    {
      filesystemPicker->SetSelectedFilesystemPaths({*ResolvedPath}, false);
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
