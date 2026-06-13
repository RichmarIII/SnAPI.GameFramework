#include "PluginDescriptor.h"

#include "PathResolver.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace SnAPI::GameFramework
{
namespace
{
using Json = nlohmann::ordered_json;

constexpr std::string_view kRuleUnsupportedSchema = "Plugin.Format.UnsupportedSchemaVersion";
constexpr std::string_view kRuleMissingName = "Plugin.NameMissing";
constexpr std::string_view kRuleMissingCodeRoot = "Plugin.Paths.CodeRootMissing";
constexpr std::string_view kRuleMissingConfigRoot = "Plugin.Paths.ConfigRootMissing";
constexpr std::string_view kRuleMissingIntermediateRoot = "Plugin.Paths.IntermediateRootMissing";
constexpr std::string_view kRuleMissingSavedRoot = "Plugin.Paths.SavedRootMissing";
constexpr std::string_view kRuleMissingPluginFilePath = "Plugin.Resolved.PluginFilePathMissing";
constexpr std::string_view kRuleMissingPluginRoot = "Plugin.Resolved.PluginRootDirectoryMissing";
constexpr std::string_view kRuleMissingResolvedCodeRoot = "Plugin.Resolved.CodeRootDirectoryMissing";
constexpr std::string_view kRuleModuleNameMissing = "Plugin.Modules.NameMissing";
constexpr std::string_view kRuleModuleRootMissing = "Plugin.Modules.RootMissing";

/**
 * @brief Trim leading and trailing ASCII whitespace from one authored text field.
 * @param Value Input text copied by value so trimming can mutate it in place.
 * @return Trimmed copy.
 */
[[nodiscard]] std::string TrimCopy(std::string Value)
{
    while (!Value.empty() && std::isspace(static_cast<unsigned char>(Value.front())) != 0)
    {
        Value.erase(Value.begin());
    }
    while (!Value.empty() && std::isspace(static_cast<unsigned char>(Value.back())) != 0)
    {
        Value.pop_back();
    }
    return Value;
}

/**
 * @brief Detect whether one descriptor field should be treated as a URI-like path.
 * @param Value Candidate path or identifier text.
 * @return `true` when the text contains a URI scheme prefix such as `asset://`.
 */
[[nodiscard]] bool HasUriScheme(const std::string_view Value)
{
    const std::size_t Delimiter = Value.find("://");
    if (Delimiter == std::string_view::npos || Delimiter == 0)
    {
        return false;
    }

    const unsigned char First = static_cast<unsigned char>(Value.front());
    if (std::isalpha(First) == 0)
    {
        return false;
    }

    for (std::size_t Index = 1; Index < Delimiter; ++Index)
    {
        const unsigned char Character = static_cast<unsigned char>(Value[Index]);
        if (std::isalnum(Character) != 0 || Character == '+' || Character == '-' || Character == '.')
        {
            continue;
        }
        return false;
    }

    return true;
}

/**
 * @brief Normalize one plugin-relative stored path field.
 * @param RawValue Authored descriptor field.
 * @return Normalized generic path string, or empty when the value normalizes to the current directory.
 */
[[nodiscard]] std::string NormalizePluginPathField(const std::string_view RawValue)
{
    std::string Value = TrimCopy(std::string(RawValue));
    if (Value.empty())
    {
        return {};
    }

    std::replace(Value.begin(), Value.end(), '\\', '/');
    const std::string Normalized = std::filesystem::path(Value).lexically_normal().generic_string();
    if (Normalized == ".")
    {
        return {};
    }
    return Normalized;
}

/**
 * @brief Normalize one stored path field while preserving URI-based values and defaults.
 * @param RawValue Authored descriptor field.
 * @param DefaultValue Default field value to substitute when the authored value is empty.
 * @return Canonical stored field text.
 */
[[nodiscard]] std::string NormalizeStoredPathField(const std::string_view RawValue, const std::string_view DefaultValue)
{
    std::string Value = TrimCopy(std::string(RawValue));
    if (Value.empty())
    {
        return std::string(DefaultValue);
    }
    if (HasUriScheme(Value))
    {
        return Value;
    }

    Value = NormalizePluginPathField(Value);
    if (Value.empty())
    {
        return std::string(DefaultValue);
    }
    return Value;
}

/**
 * @brief Append one trimmed string to a vector when it is non-empty and not already present.
 * @param Values Destination ordered string list.
 * @param Value Candidate string value.
 */
void AppendUniqueTrimmedString(std::vector<std::string>& Values, std::string Value)
{
    Value = TrimCopy(std::move(Value));
    if (Value.empty())
    {
        return;
    }

    if (std::ranges::find(Values, Value) == Values.end())
    {
        Values.push_back(std::move(Value));
    }
}

/**
 * @brief Parse a descriptor JSON document and enforce an object root.
 * @param JsonText Raw UTF-8 JSON document text.
 * @return Parsed ordered JSON object.
 * @throws std::runtime_error When the document is invalid JSON or does not use an object root.
 */
[[nodiscard]] Json ParseJsonDocument(const std::string_view JsonText)
{
    Json Document = Json::parse(JsonText.begin(), JsonText.end(), nullptr, false);
    if (Document.is_discarded())
    {
        throw std::runtime_error("Plugin descriptor is not valid JSON");
    }
    if (!Document.is_object())
    {
        throw std::runtime_error("Plugin descriptor root must be a JSON object");
    }
    return Document;
}

/**
 * @brief Read one optional string field from a JSON object.
 * @param Object JSON object owning the field.
 * @param Key Field name to read.
 * @param DefaultValue Value returned when the field is missing or null.
 * @return Parsed string value.
 * @throws std::runtime_error When the field exists but is not a string.
 */
[[nodiscard]] std::string ReadStringField(const Json& Object, const char* Key, const std::string_view DefaultValue = {})
{
    const auto It = Object.find(Key);
    if (It == Object.end() || It->is_null())
    {
        return std::string(DefaultValue);
    }
    if (!It->is_string())
    {
        throw std::runtime_error(std::string("Field '") + Key + "' must be a string");
    }
    return It->get<std::string>();
}

/**
 * @brief Read one optional boolean field from a JSON object.
 * @param Object JSON object owning the field.
 * @param Key Field name to read.
 * @param DefaultValue Value returned when the field is missing or null.
 * @return Parsed boolean value.
 * @throws std::runtime_error When the field exists but is not a boolean.
 */
[[nodiscard]] bool ReadBoolField(const Json& Object, const char* Key, const bool DefaultValue)
{
    const auto It = Object.find(Key);
    if (It == Object.end() || It->is_null())
    {
        return DefaultValue;
    }
    if (!It->is_boolean())
    {
        throw std::runtime_error(std::string("Field '") + Key + "' must be a boolean");
    }
    return It->get<bool>();
}

/**
 * @brief Read one optional unsigned integer field from a JSON object.
 * @param Object JSON object owning the field.
 * @param Key Field name to read.
 * @param DefaultValue Value returned when the field is missing or null.
 * @return Parsed unsigned integer value.
 * @throws std::runtime_error When the field exists but is not numeric.
 */
[[nodiscard]] std::uint32_t ReadUnsignedField(const Json& Object, const char* Key, const std::uint32_t DefaultValue)
{
    const auto It = Object.find(Key);
    if (It == Object.end() || It->is_null())
    {
        return DefaultValue;
    }
    if (!It->is_number_unsigned() && !It->is_number_integer())
    {
        throw std::runtime_error(std::string("Field '") + Key + "' must be an unsigned integer");
    }
    return It->get<std::uint32_t>();
}

/**
 * @brief Read one string-array field while trimming and deduplicating entries.
 * @param Object JSON object owning the field.
 * @param Key Field name to read.
 * @return Normalized ordered string list.
 * @throws std::runtime_error When the field exists but is not a string array.
 */
[[nodiscard]] std::vector<std::string> ReadStringArrayField(const Json& Object, const char* Key)
{
    const auto It = Object.find(Key);
    if (It == Object.end() || It->is_null())
    {
        return {};
    }
    if (!It->is_array())
    {
        throw std::runtime_error(std::string("Field '") + Key + "' must be an array");
    }

    std::vector<std::string> Values{};
    for (const Json& Entry : *It)
    {
        if (!Entry.is_string())
        {
            throw std::runtime_error(std::string("Field '") + Key + "' must contain only strings");
        }
        AppendUniqueTrimmedString(Values, Entry.get<std::string>());
    }
    return Values;
}

/**
 * @brief Read one optional object-valued section from the descriptor root.
 * @param Root JSON root object.
 * @param Key Section name to read.
 * @return Existing object section or an empty object when omitted.
 * @throws std::runtime_error When the section exists but is not an object.
 */
[[nodiscard]] Json ReadObjectSection(const Json& Root, const char* Key)
{
    const auto It = Root.find(Key);
    if (It == Root.end() || It->is_null())
    {
        return Json::object();
    }
    if (!It->is_object())
    {
        throw std::runtime_error(std::string("Section '") + Key + "' must be a JSON object");
    }
    return *It;
}

/**
 * @brief Parse one authored module-type string.
 * @param Value Stored module type text.
 * @return Matching enum value.
 * @throws std::runtime_error When the value is not a supported module type.
 */
[[nodiscard]] EProjectModuleType ParseModuleType(const std::string_view Value)
{
    const std::string Type = TrimCopy(std::string(Value));
    if (Type == "Runtime")
    {
        return EProjectModuleType::Runtime;
    }
    if (Type == "Editor")
    {
        return EProjectModuleType::Editor;
    }
    if (Type == "Shared")
    {
        return EProjectModuleType::Shared;
    }
    if (Type == "Developer")
    {
        return EProjectModuleType::Developer;
    }
    if (Type == "Test")
    {
        return EProjectModuleType::Test;
    }
    if (Type == "Program")
    {
        return EProjectModuleType::Program;
    }

    throw std::runtime_error("Unsupported module type: " + std::string(Value));
}

/**
 * @brief Convert one module-type enum back into descriptor text.
 * @param Type Module role to serialize.
 * @return Structured-schema string representation.
 */
[[nodiscard]] std::string ToString(const EProjectModuleType Type)
{
    switch (Type)
    {
    case EProjectModuleType::Runtime:
        return "Runtime";
    case EProjectModuleType::Editor:
        return "Editor";
    case EProjectModuleType::Shared:
        return "Shared";
    case EProjectModuleType::Developer:
        return "Developer";
    case EProjectModuleType::Test:
        return "Test";
    case EProjectModuleType::Program:
        return "Program";
    }

    return "Runtime";
}

/**
 * @brief Append one validation issue to the flat issue list.
 * @param Issues Destination issue list.
 * @param Severity Validation severity.
 * @param RuleId Stable rule identifier.
 * @param Message Human-readable validation text.
 */
void AppendIssue(std::vector<ProjectDescriptorValidationIssue>& Issues,
                 const EProjectDescriptorValidationSeverity Severity,
                 const std::string_view RuleId,
                 std::string Message)
{
    Issues.push_back(ProjectDescriptorValidationIssue{
        .Severity = Severity,
        .RuleId = std::string(RuleId),
        .Message = std::move(Message),
    });
}

/**
 * @brief Normalize one plugin descriptor into the canonical in-memory form used by the service.
 * @param Descriptor Parsed or partially-authored plugin descriptor model.
 * @return Canonicalized descriptor with defaults, trimming, normalization, and sanitized raw sections applied.
 */
PluginDescriptor CanonicalizeDescriptor(PluginDescriptor Descriptor)
{
    if (Descriptor.Format.SchemaVersion == 0u)
    {
        Descriptor.Format.SchemaVersion = PluginDescriptorService::kCurrentSchemaVersion;
    }

    Descriptor.Format.MinimumToolVersion =
        TrimCopy(Descriptor.Format.MinimumToolVersion.empty()
                     ? std::string(PluginDescriptorService::kDefaultMinimumToolVersion)
                     : Descriptor.Format.MinimumToolVersion);

    Descriptor.Plugin.Name = TrimCopy(std::move(Descriptor.Plugin.Name));
    Descriptor.Plugin.DisplayName = TrimCopy(std::move(Descriptor.Plugin.DisplayName));
    Descriptor.Plugin.Company = TrimCopy(std::move(Descriptor.Plugin.Company));
    Descriptor.Plugin.PluginId = TrimCopy(std::move(Descriptor.Plugin.PluginId));
    Descriptor.Plugin.Version = TrimCopy(Descriptor.Plugin.Version.empty()
                                             ? std::string(PluginDescriptorService::kDefaultPluginVersion)
                                             : std::move(Descriptor.Plugin.Version));
    Descriptor.Plugin.Description = TrimCopy(std::move(Descriptor.Plugin.Description));
    if (Descriptor.Plugin.DisplayName.empty() && !Descriptor.Plugin.Name.empty())
    {
        Descriptor.Plugin.DisplayName = Descriptor.Plugin.Name;
    }

    Descriptor.Paths.AssetRoot =
        NormalizeStoredPathField(Descriptor.Paths.AssetRoot, PluginDescriptorService::kDefaultAssetRoot);
    Descriptor.Paths.CodeRoot =
        NormalizeStoredPathField(Descriptor.Paths.CodeRoot, PluginDescriptorService::kDefaultCodeRoot);
    Descriptor.Paths.ConfigRoot =
        NormalizeStoredPathField(Descriptor.Paths.ConfigRoot, PluginDescriptorService::kDefaultConfigRoot);
    Descriptor.Paths.IntermediateRoot =
        NormalizeStoredPathField(Descriptor.Paths.IntermediateRoot, PluginDescriptorService::kDefaultIntermediateRoot);
    Descriptor.Paths.SavedRoot =
        NormalizeStoredPathField(Descriptor.Paths.SavedRoot, PluginDescriptorService::kDefaultSavedRoot);

    for (ProjectModuleDescriptor& Module : Descriptor.Modules)
    {
        Module.Name = TrimCopy(std::move(Module.Name));
        Module.Root = NormalizePluginPathField(Module.Root);
        Module.PublicDependencies = ReadStringArrayField(Json{{"Values", Module.PublicDependencies}}, "Values");
        Module.PrivateDependencies = ReadStringArrayField(Json{{"Values", Module.PrivateDependencies}}, "Values");
        Module.Platforms = ReadStringArrayField(Json{{"Values", Module.Platforms}}, "Values");
        Module.PreprocessorDefinitions =
            ReadStringArrayField(Json{{"Values", Module.PreprocessorDefinitions}}, "Values");
    }

    if (!Descriptor.Platforms.is_object())
    {
        Descriptor.Platforms = Json::object();
    }
    if (!Descriptor.Packaging.is_object())
    {
        Descriptor.Packaging = Json::object();
    }
    if (!Descriptor.AssetRules.is_object())
    {
        Descriptor.AssetRules = Json::object();
    }
    if (!Descriptor.Templates.is_object())
    {
        Descriptor.Templates = Json::object();
    }

    return Descriptor;
}

/**
 * @brief Parse one plugin descriptor root into the canonical descriptor model.
 * @param Root Parsed JSON object root.
 * @return Parsed plugin descriptor model.
 * @throws std::runtime_error When the descriptor uses invalid field types.
 */
[[nodiscard]] PluginDescriptor ParseDescriptor(const Json& Root)
{
    const Json FormatJson = ReadObjectSection(Root, "Format");
    const Json PluginJson = ReadObjectSection(Root, "Plugin");
    const Json PathsJson = ReadObjectSection(Root, "Paths");

    PluginDescriptor Descriptor{};
    Descriptor.Format.SchemaVersion =
        ReadUnsignedField(FormatJson, "SchemaVersion", PluginDescriptorService::kCurrentSchemaVersion);
    Descriptor.Format.MinimumToolVersion =
        ReadStringField(FormatJson, "MinimumToolVersion", PluginDescriptorService::kDefaultMinimumToolVersion);

    Descriptor.Plugin.Name = ReadStringField(PluginJson, "Name");
    Descriptor.Plugin.DisplayName = ReadStringField(PluginJson, "DisplayName");
    Descriptor.Plugin.Company = ReadStringField(PluginJson, "Company");
    Descriptor.Plugin.PluginId = ReadStringField(PluginJson, "PluginId");
    Descriptor.Plugin.Version = ReadStringField(PluginJson, "Version", PluginDescriptorService::kDefaultPluginVersion);
    Descriptor.Plugin.Description = ReadStringField(PluginJson, "Description");
    Descriptor.Plugin.CanContainAssets = ReadBoolField(PluginJson, "CanContainAssets", true);

    Descriptor.Paths.AssetRoot = ReadStringField(PathsJson, "AssetRoot", PluginDescriptorService::kDefaultAssetRoot);
    Descriptor.Paths.CodeRoot = ReadStringField(PathsJson, "CodeRoot", PluginDescriptorService::kDefaultCodeRoot);
    Descriptor.Paths.ConfigRoot = ReadStringField(PathsJson, "ConfigRoot", PluginDescriptorService::kDefaultConfigRoot);
    Descriptor.Paths.IntermediateRoot =
        ReadStringField(PathsJson, "IntermediateRoot", PluginDescriptorService::kDefaultIntermediateRoot);
    Descriptor.Paths.SavedRoot = ReadStringField(PathsJson, "SavedRoot", PluginDescriptorService::kDefaultSavedRoot);

    const auto ModulesIt = Root.find("Modules");
    if (ModulesIt != Root.end() && !ModulesIt->is_null())
    {
        if (!ModulesIt->is_array())
        {
            throw std::runtime_error("Section 'Modules' must be a JSON array");
        }

        for (const Json& ModuleJson : *ModulesIt)
        {
            if (!ModuleJson.is_object())
            {
                throw std::runtime_error("Section 'Modules' must contain only JSON objects");
            }

            ProjectModuleDescriptor Module{};
            Module.Name = ReadStringField(ModuleJson, "Name");
            Module.Type = ParseModuleType(ReadStringField(ModuleJson, "Type", "Runtime"));
            Module.Root = ReadStringField(ModuleJson, "Root");
            Module.PublicDependencies = ReadStringArrayField(ModuleJson, "PublicDependencies");
            Module.PrivateDependencies = ReadStringArrayField(ModuleJson, "PrivateDependencies");
            Module.Platforms = ReadStringArrayField(ModuleJson, "Platforms");
            Module.PreprocessorDefinitions = ReadStringArrayField(ModuleJson, "PreprocessorDefinitions");
            Module.UseReflectionGen = ReadBoolField(ModuleJson, "UseReflectionGen", false);
            Module.UseSWIG = ReadBoolField(ModuleJson, "UseSWIG", false);
            Module.LoadInEditor = ReadBoolField(ModuleJson, "LoadInEditor", true);
            Module.LoadInRuntime = ReadBoolField(ModuleJson, "LoadInRuntime", true);
            Descriptor.Modules.push_back(std::move(Module));
        }
    }

    Descriptor.Platforms = ReadObjectSection(Root, "Platforms");
    Descriptor.Packaging = ReadObjectSection(Root, "Packaging");
    Descriptor.AssetRules = ReadObjectSection(Root, "AssetRules");
    Descriptor.Templates = ReadObjectSection(Root, "Templates");
    return CanonicalizeDescriptor(std::move(Descriptor));
}

/**
 * @brief Serialize one plugin descriptor model into structured ordered JSON.
 * @param Descriptor Canonical plugin descriptor model.
 * @return Structured ordered JSON root.
 */
[[nodiscard]] Json SerializeDescriptor(const PluginDescriptor& Descriptor)
{
    Json Root = Json::object();
    Root["Format"] = Json::object({
        {"SchemaVersion", Descriptor.Format.SchemaVersion},
        {"MinimumToolVersion", Descriptor.Format.MinimumToolVersion},
    });
    Root["Plugin"] = Json::object({
        {"Name", Descriptor.Plugin.Name},
        {"DisplayName", Descriptor.Plugin.DisplayName},
        {"Company", Descriptor.Plugin.Company},
        {"PluginId", Descriptor.Plugin.PluginId},
        {"Version", Descriptor.Plugin.Version},
        {"Description", Descriptor.Plugin.Description},
        {"CanContainAssets", Descriptor.Plugin.CanContainAssets},
    });
    Root["Paths"] = Json::object({
        {"AssetRoot", Descriptor.Paths.AssetRoot},
        {"CodeRoot", Descriptor.Paths.CodeRoot},
        {"ConfigRoot", Descriptor.Paths.ConfigRoot},
        {"IntermediateRoot", Descriptor.Paths.IntermediateRoot},
        {"SavedRoot", Descriptor.Paths.SavedRoot},
    });

    Json Modules = Json::array();
    for (const ProjectModuleDescriptor& Module : Descriptor.Modules)
    {
        Modules.push_back(Json::object({
            {"Name", Module.Name},
            {"Type", ToString(Module.Type)},
            {"Root", Module.Root},
            {"PublicDependencies", Module.PublicDependencies},
            {"PrivateDependencies", Module.PrivateDependencies},
            {"Platforms", Module.Platforms},
            {"PreprocessorDefinitions", Module.PreprocessorDefinitions},
            {"UseReflectionGen", Module.UseReflectionGen},
            {"UseSWIG", Module.UseSWIG},
            {"LoadInEditor", Module.LoadInEditor},
            {"LoadInRuntime", Module.LoadInRuntime},
        }));
    }
    Root["Modules"] = std::move(Modules);
    Root["Platforms"] = Descriptor.Platforms;
    Root["Packaging"] = Descriptor.Packaging;
    Root["AssetRules"] = Descriptor.AssetRules;
    Root["Templates"] = Descriptor.Templates;
    return Root;
}

/**
 * @brief Resolve one stored descriptor path against one descriptor root.
 * @param BaseDirectory Descriptor root directory.
 * @param StoredValue Authored path field.
 * @return Normalized filesystem path.
 */
[[nodiscard]] std::filesystem::path ResolveStoredPath(const std::filesystem::path& BaseDirectory,
                                                      const std::string_view StoredValue)
{
    std::filesystem::path Value{std::string(StoredValue)};
    if (Value.empty() || HasUriScheme(StoredValue))
    {
        return {};
    }
    if (Value.is_absolute())
    {
        return Value.lexically_normal();
    }
    return (BaseDirectory / Value).lexically_normal();
}

/**
 * @brief Resolve one plugin descriptor file path into a normalized filesystem path.
 * @param PluginFilePath Authored or user-supplied plugin descriptor file path.
 * @return Normalized filesystem path to the descriptor file.
 */
[[nodiscard]] TExpected<std::filesystem::path> ResolvePluginFilePath(const std::string_view PluginFilePath)
{
    const std::string RawPath = TrimCopy(std::string(PluginFilePath));
    if (RawPath.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Plugin descriptor path cannot be empty"));
    }

    std::error_code Error{};
    std::filesystem::path ResolvedPath = std::filesystem::path(RawPath);
    if (!ResolvedPath.is_absolute())
    {
        ResolvedPath = std::filesystem::absolute(ResolvedPath, Error);
        if (Error)
        {
            return std::unexpected(
                MakeError(EErrorCode::InternalError,
                          "Failed to resolve plugin descriptor path: " + Error.message()));
        }
    }

    return ResolvedPath.lexically_normal();
}

} // namespace

TExpected<PluginDescriptor> PluginDescriptorService::Parse(const std::string_view JsonText)
{
    try
    {
        return ParseDescriptor(ParseJsonDocument(JsonText));
    }
    catch (const std::exception& Exception)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, Exception.what()));
    }
}

TExpected<PluginDescriptor> PluginDescriptorService::Load(const std::string_view PluginFilePath)
{
    auto ResolvedPluginFile = ResolvePluginFilePath(PluginFilePath);
    if (!ResolvedPluginFile)
    {
        return std::unexpected(ResolvedPluginFile.error());
    }

    std::ifstream Input(*ResolvedPluginFile, std::ios::binary);
    if (!Input.is_open())
    {
        return std::unexpected(
            MakeError(EErrorCode::NotFound, "Plugin descriptor file not found: " + ResolvedPluginFile->string()));
    }

    const std::string Text((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
    auto DescriptorResult = Parse(Text);
    if (!DescriptorResult)
    {
        return std::unexpected(DescriptorResult.error());
    }

    const auto Issues = Validate(*DescriptorResult);
    const auto ErrorIt = std::ranges::find_if(Issues, [](const ProjectDescriptorValidationIssue& Issue) {
        return Issue.Severity == EProjectDescriptorValidationSeverity::Error;
    });
    if (ErrorIt != Issues.end())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, ErrorIt->Message));
    }

    return *DescriptorResult;
}

TExpected<ResolvedPluginDescriptor> PluginDescriptorService::LoadResolved(const std::string_view PluginFilePath)
{
    auto ResolvedPluginFile = ResolvePluginFilePath(PluginFilePath);
    if (!ResolvedPluginFile)
    {
        return std::unexpected(ResolvedPluginFile.error());
    }

    auto DescriptorResult = Load(PluginFilePath);
    if (!DescriptorResult)
    {
        return std::unexpected(DescriptorResult.error());
    }

    ResolvedPluginDescriptor Resolved{};
    Resolved.Descriptor = std::move(*DescriptorResult);
    Resolved.PluginFilePath = *ResolvedPluginFile;
    Resolved.PluginRootDirectory = Resolved.PluginFilePath.parent_path().lexically_normal();
    Resolved.AssetRootDirectory = ResolveStoredPath(Resolved.PluginRootDirectory, Resolved.Descriptor.Paths.AssetRoot);
    Resolved.CodeRootDirectory = ResolveStoredPath(Resolved.PluginRootDirectory, Resolved.Descriptor.Paths.CodeRoot);
    Resolved.ConfigRootDirectory = ResolveStoredPath(Resolved.PluginRootDirectory, Resolved.Descriptor.Paths.ConfigRoot);
    Resolved.IntermediateRootDirectory =
        ResolveStoredPath(Resolved.PluginRootDirectory, Resolved.Descriptor.Paths.IntermediateRoot);
    Resolved.SavedRootDirectory = ResolveStoredPath(Resolved.PluginRootDirectory, Resolved.Descriptor.Paths.SavedRoot);

    const auto Issues = Validate(Resolved);
    const auto ErrorIt = std::ranges::find_if(Issues, [](const ProjectDescriptorValidationIssue& Issue) {
        return Issue.Severity == EProjectDescriptorValidationSeverity::Error;
    });
    if (ErrorIt != Issues.end())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, ErrorIt->Message));
    }

    return Resolved;
}

TExpected<std::string> PluginDescriptorService::Serialize(const PluginDescriptor& Descriptor, const int Indent)
{
    PluginDescriptor Normalized = CanonicalizeDescriptor(Descriptor);
    const auto Issues = Validate(Normalized);
    const auto ErrorIt = std::ranges::find_if(Issues, [](const ProjectDescriptorValidationIssue& Issue) {
        return Issue.Severity == EProjectDescriptorValidationSeverity::Error;
    });
    if (ErrorIt != Issues.end())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, ErrorIt->Message));
    }

    try
    {
        return SerializeDescriptor(Normalized).dump(Indent) + "\n";
    }
    catch (const std::exception& Exception)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Exception.what()));
    }
}

Result PluginDescriptorService::Save(const PluginDescriptor& Descriptor, const std::string_view PluginFilePath)
{
    auto ResolvedPluginFile = ResolvePluginFilePath(PluginFilePath);
    if (!ResolvedPluginFile)
    {
        return std::unexpected(ResolvedPluginFile.error());
    }

    PluginDescriptor Normalized = CanonicalizeDescriptor(Descriptor);
    if (Normalized.Plugin.Name.empty())
    {
        Normalized.Plugin.Name = TrimCopy(ResolvedPluginFile->stem().string());
    }
    if (Normalized.Plugin.Name.empty())
    {
        Normalized.Plugin.Name = "Plugin";
    }
    if (Normalized.Plugin.DisplayName.empty())
    {
        Normalized.Plugin.DisplayName = Normalized.Plugin.Name;
    }

    auto TextResult = Serialize(Normalized, 2);
    if (!TextResult)
    {
        return std::unexpected(TextResult.error());
    }

    std::error_code Error{};
    std::filesystem::create_directories(ResolvedPluginFile->parent_path(), Error);
    if (Error)
    {
        return std::unexpected(
            MakeError(EErrorCode::InternalError,
                      "Failed to create plugin descriptor directory: " + Error.message()));
    }

    std::ofstream Output(*ResolvedPluginFile, std::ios::binary | std::ios::trunc);
    if (!Output.is_open())
    {
        return std::unexpected(
            MakeError(EErrorCode::InternalError,
                      "Failed to open plugin descriptor file for write: " + ResolvedPluginFile->string()));
    }
    Output.write(TextResult->data(), static_cast<std::streamsize>(TextResult->size()));
    if (!Output.good())
    {
        return std::unexpected(
            MakeError(EErrorCode::InternalError,
                      "Failed to write plugin descriptor file: " + ResolvedPluginFile->string()));
    }

    return Ok();
}

std::vector<ProjectDescriptorValidationIssue> PluginDescriptorService::Validate(const PluginDescriptor& Descriptor)
{
    std::vector<ProjectDescriptorValidationIssue> Issues{};

    if (Descriptor.Format.SchemaVersion != kCurrentSchemaVersion)
    {
        AppendIssue(Issues,
                    EProjectDescriptorValidationSeverity::Error,
                    kRuleUnsupportedSchema,
                    "Plugin descriptor schema version is not supported");
    }
    if (TrimCopy(Descriptor.Plugin.Name).empty())
    {
        AppendIssue(Issues,
                    EProjectDescriptorValidationSeverity::Error,
                    kRuleMissingName,
                    "Plugin descriptor requires a non-empty plugin name");
    }
    if (TrimCopy(Descriptor.Paths.CodeRoot).empty())
    {
        AppendIssue(Issues,
                    EProjectDescriptorValidationSeverity::Error,
                    kRuleMissingCodeRoot,
                    "Plugin descriptor requires a non-empty code root");
    }
    if (TrimCopy(Descriptor.Paths.ConfigRoot).empty())
    {
        AppendIssue(Issues,
                    EProjectDescriptorValidationSeverity::Error,
                    kRuleMissingConfigRoot,
                    "Plugin descriptor requires a non-empty config root");
    }
    if (TrimCopy(Descriptor.Paths.IntermediateRoot).empty())
    {
        AppendIssue(Issues,
                    EProjectDescriptorValidationSeverity::Error,
                    kRuleMissingIntermediateRoot,
                    "Plugin descriptor requires a non-empty intermediate root");
    }
    if (TrimCopy(Descriptor.Paths.SavedRoot).empty())
    {
        AppendIssue(Issues,
                    EProjectDescriptorValidationSeverity::Error,
                    kRuleMissingSavedRoot,
                    "Plugin descriptor requires a non-empty saved root");
    }

    for (const ProjectModuleDescriptor& Module : Descriptor.Modules)
    {
        if (TrimCopy(Module.Name).empty())
        {
            AppendIssue(Issues,
                        EProjectDescriptorValidationSeverity::Error,
                        kRuleModuleNameMissing,
                        "Plugin module entries require a non-empty name");
        }
        if (TrimCopy(Module.Root).empty())
        {
            AppendIssue(Issues,
                        EProjectDescriptorValidationSeverity::Error,
                        kRuleModuleRootMissing,
                        "Plugin module '" + Module.Name + "' requires a non-empty root path");
        }
    }

    return Issues;
}

std::vector<ProjectDescriptorValidationIssue> PluginDescriptorService::Validate(const ResolvedPluginDescriptor& Descriptor)
{
    std::vector<ProjectDescriptorValidationIssue> Issues = Validate(Descriptor.Descriptor);

    if (Descriptor.PluginFilePath.empty())
    {
        AppendIssue(Issues,
                    EProjectDescriptorValidationSeverity::Error,
                    kRuleMissingPluginFilePath,
                    "Resolved plugin descriptor is missing the plugin file path");
    }
    if (Descriptor.PluginRootDirectory.empty())
    {
        AppendIssue(Issues,
                    EProjectDescriptorValidationSeverity::Error,
                    kRuleMissingPluginRoot,
                    "Resolved plugin descriptor is missing the plugin root directory");
    }
    if (Descriptor.CodeRootDirectory.empty())
    {
        AppendIssue(Issues,
                    EProjectDescriptorValidationSeverity::Error,
                    kRuleMissingResolvedCodeRoot,
                    "Resolved plugin descriptor is missing the code root directory");
    }

    return Issues;
}

std::string PluginDescriptorService::ToPluginRelativePathField(const std::string_view RawValue,
                                                               const std::filesystem::path& BaseRoot)
{
    return ProjectDescriptorService::ToProjectRelativePathField(RawValue, BaseRoot);
}

} // namespace SnAPI::GameFramework
