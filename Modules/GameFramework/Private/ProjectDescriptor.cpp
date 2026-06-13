#include "ProjectDescriptor.h"

#include "PathResolver.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace SnAPI::GameFramework
{
    namespace
    {
        using Json = nlohmann::ordered_json;

        constexpr std::string_view kRuleUnsupportedSchema = "Project.Format.UnsupportedSchemaVersion";
        constexpr std::string_view kRuleMissingName = "Project.NameMissing";
        constexpr std::string_view kRuleMissingAssetRoot = "Project.Paths.AssetRootMissing";
        constexpr std::string_view kRuleMissingCodeRoot = "Project.Paths.CodeRootMissing";
        constexpr std::string_view kRuleMissingConfigRoot = "Project.Paths.ConfigRootMissing";
        constexpr std::string_view kRuleMissingIntermediateRoot = "Project.Paths.IntermediateRootMissing";
        constexpr std::string_view kRuleMissingSavedRoot = "Project.Paths.SavedRootMissing";
        constexpr std::string_view kRuleMissingStartupAsset = "Project.Startup.StartupLevelAssetMissing";
        constexpr std::string_view kRuleMissingProjectFilePath = "Project.Resolved.ProjectFilePathMissing";
        constexpr std::string_view kRuleMissingProjectRoot = "Project.Resolved.ProjectRootDirectoryMissing";
        constexpr std::string_view kRuleMissingResolvedAssetRoot = "Project.Resolved.AssetRootDirectoryMissing";
        constexpr std::string_view kRuleMissingResolvedStartupAsset = "Project.Resolved.StartupLevelAssetPathMissing";
        constexpr std::string_view kRuleStartupAssetMissingOnDisk = "Project.Resolved.StartupLevelAssetMissingOnDisk";
        constexpr std::string_view kRuleModuleNameMissing = "Project.Modules.NameMissing";
        constexpr std::string_view kRuleModuleRootMissing = "Project.Modules.RootMissing";

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
         * @brief Normalize one project-relative stored path field.
         * @param RawValue Authored descriptor field.
         * @return Normalized generic path string, or empty when the value normalizes to the current directory.
         */
        [[nodiscard]] std::string NormalizeProjectPathField(const std::string_view RawValue)
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
        [[nodiscard]] std::string NormalizeStoredPathField(const std::string_view RawValue,
                                                           const std::string_view DefaultValue)
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

            Value = NormalizeProjectPathField(Value);
            if (Value.empty())
            {
                return std::string(DefaultValue);
            }
            return Value;
        }

        /**
         * @brief Canonicalize an asset extension for migration comparisons.
         * @param Extension Raw extension including the leading dot when present.
         * @return Lowercase extension text.
         */
        [[nodiscard]] std::string NormalizeAssetExtension(std::string Extension)
        {
            std::transform(Extension.begin(), Extension.end(), Extension.begin(),
                           [](const unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
            return Extension;
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
                throw std::runtime_error("Project descriptor is not valid JSON");
            }
            if (!Document.is_object())
            {
                throw std::runtime_error("Project descriptor root must be a JSON object");
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
        [[nodiscard]] std::string ReadStringField(const Json& Object, const char* Key,
                                                  const std::string_view DefaultValue = {})
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
        [[nodiscard]] std::uint32_t ReadUnsignedField(const Json& Object, const char* Key,
                                                      const std::uint32_t DefaultValue)
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
         * @brief Detect whether a document already uses the structured project schema.
         * @param Root Parsed descriptor root object.
         * @return `true` when any structured top-level section is present.
         */
        [[nodiscard]] bool UsesStructuredSchema(const Json& Root)
        {
            return Root.contains("Format") || Root.contains("Project") || Root.contains("Paths") ||
                Root.contains("Startup") || Root.contains("Modules") || Root.contains("Profiles") ||
                Root.contains("Platforms") || Root.contains("Packaging") || Root.contains("AssetRules") ||
                Root.contains("Templates");
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
         * @brief Normalize one descriptor into the canonical in-memory form used by the service.
         * @param Descriptor Parsed or partially-authored descriptor model.
         * @return Canonicalized descriptor with defaults, trimming, normalization, and sanitized raw sections applied.
         */
        ProjectDescriptor CanonicalizeDescriptor(ProjectDescriptor Descriptor)
        {
            if (Descriptor.Format.SchemaVersion == 0u)
            {
                Descriptor.Format.SchemaVersion = ProjectDescriptorService::kCurrentSchemaVersion;
            }

            Descriptor.Format.MinimumToolVersion =
                TrimCopy(Descriptor.Format.MinimumToolVersion.empty()
                             ? std::string(ProjectDescriptorService::kDefaultMinimumToolVersion)
                             : Descriptor.Format.MinimumToolVersion);

            Descriptor.Project.Name = TrimCopy(std::move(Descriptor.Project.Name));
            Descriptor.Project.DisplayName = TrimCopy(std::move(Descriptor.Project.DisplayName));
            Descriptor.Project.Company = TrimCopy(std::move(Descriptor.Project.Company));
            Descriptor.Project.ProjectId = TrimCopy(std::move(Descriptor.Project.ProjectId));
            Descriptor.Project.Description = TrimCopy(std::move(Descriptor.Project.Description));
            if (Descriptor.Project.DisplayName.empty() && !Descriptor.Project.Name.empty())
            {
                Descriptor.Project.DisplayName = Descriptor.Project.Name;
            }

            Descriptor.Paths.AssetRoot =
                NormalizeStoredPathField(Descriptor.Paths.AssetRoot, ProjectDescriptorService::kDefaultAssetRoot);
            Descriptor.Paths.CodeRoot =
                NormalizeStoredPathField(Descriptor.Paths.CodeRoot, ProjectDescriptorService::kDefaultCodeRoot);
            Descriptor.Paths.ConfigRoot =
                NormalizeStoredPathField(Descriptor.Paths.ConfigRoot, ProjectDescriptorService::kDefaultConfigRoot);
            Descriptor.Paths.IntermediateRoot = NormalizeStoredPathField(
                Descriptor.Paths.IntermediateRoot, ProjectDescriptorService::kDefaultIntermediateRoot);
            Descriptor.Paths.SavedRoot =
                NormalizeStoredPathField(Descriptor.Paths.SavedRoot, ProjectDescriptorService::kDefaultSavedRoot);

            Descriptor.Startup.StartupLevelAsset = NormalizeStoredPathField(
                Descriptor.Startup.StartupLevelAsset, ProjectDescriptorService::kDefaultStartupLevelAsset);
            Descriptor.Startup.DefaultRenderSettingsAssetId =
                TrimCopy(std::move(Descriptor.Startup.DefaultRenderSettingsAssetId));
            Descriptor.Startup.DefaultGameClass = TrimCopy(std::move(Descriptor.Startup.DefaultGameClass));
            Descriptor.Startup.DefaultGameModeClass = TrimCopy(std::move(Descriptor.Startup.DefaultGameModeClass));

            for (ProjectModuleDescriptor& Module : Descriptor.Modules)
            {
                Module.Name = TrimCopy(std::move(Module.Name));
                Module.Root = NormalizeProjectPathField(Module.Root);

                std::vector<std::string> PublicDependencies{};
                for (std::string& Dependency : Module.PublicDependencies)
                {
                    AppendUniqueTrimmedString(PublicDependencies, std::move(Dependency));
                }
                Module.PublicDependencies = std::move(PublicDependencies);

                std::vector<std::string> PrivateDependencies{};
                for (std::string& Dependency : Module.PrivateDependencies)
                {
                    AppendUniqueTrimmedString(PrivateDependencies, std::move(Dependency));
                }
                Module.PrivateDependencies = std::move(PrivateDependencies);

                std::vector<std::string> Platforms{};
                for (std::string& Platform : Module.Platforms)
                {
                    AppendUniqueTrimmedString(Platforms, std::move(Platform));
                }
                Module.Platforms = std::move(Platforms);

                std::vector<std::string> Definitions{};
                for (std::string& Definition : Module.PreprocessorDefinitions)
                {
                    AppendUniqueTrimmedString(Definitions, std::move(Definition));
                }
                Module.PreprocessorDefinitions = std::move(Definitions);
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
         * @brief Convert one build-profile issue into a descriptor-validation issue.
         * @param Issue Build-profile validation issue to convert.
         * @return Descriptor-validation issue with matching severity and message.
         */
        [[nodiscard]] ProjectDescriptorValidationIssue ConvertBuildIssue(const BuildValidationIssue& Issue)
        {
            EProjectDescriptorValidationSeverity Severity = EProjectDescriptorValidationSeverity::Error;
            switch (Issue.Severity)
            {
            case EBuildValidationSeverity::Info:
                Severity = EProjectDescriptorValidationSeverity::Info;
                break;
            case EBuildValidationSeverity::Warning:
                Severity = EProjectDescriptorValidationSeverity::Warning;
                break;
            case EBuildValidationSeverity::Error:
                Severity = EProjectDescriptorValidationSeverity::Error;
                break;
            }

            return ProjectDescriptorValidationIssue{
                .Severity = Severity,
                .RuleId = Issue.RuleId,
                .Message = Issue.Message,
            };
        }

        /**
         * @brief Migrate the legacy flat runtime/editor project schema into the canonical descriptor model.
         * @param Root Parsed legacy descriptor root.
         * @return Canonicalized descriptor equivalent to the legacy document.
         */
        [[nodiscard]] ProjectDescriptor ParseLegacyDescriptor(const Json& Root)
        {
            ProjectDescriptor Descriptor{};
            Descriptor.Format.SchemaVersion =
                ReadUnsignedField(Root, "version", ProjectDescriptorService::kCurrentSchemaVersion);
            Descriptor.Project.Name = ReadStringField(Root, "name");
            Descriptor.Project.DisplayName = Descriptor.Project.Name;
            Descriptor.Paths.AssetRoot =
                ReadStringField(Root, "assetRoot", ProjectDescriptorService::kDefaultAssetRoot);

            std::string StartupLevelAsset = ReadStringField(Root, "startupLevelAsset");
            if (TrimCopy(StartupLevelAsset).empty())
            {
                std::string LegacyStartupLevelPack = ReadStringField(Root, "startupLevelPack");
                LegacyStartupLevelPack = TrimCopy(std::move(LegacyStartupLevelPack));
                if (!LegacyStartupLevelPack.empty())
                {
                    if (!HasUriScheme(LegacyStartupLevelPack))
                    {
                        LegacyStartupLevelPack = NormalizeProjectPathField(LegacyStartupLevelPack);
                        std::filesystem::path LegacyPath(LegacyStartupLevelPack);
                        if (NormalizeAssetExtension(LegacyPath.extension().string()) == ".snpak")
                        {
                            LegacyPath.replace_extension(".level");
                        }
                        StartupLevelAsset = LegacyPath.lexically_normal().generic_string();
                    }
                    else
                    {
                        StartupLevelAsset = LegacyStartupLevelPack;
                    }
                }
            }
            Descriptor.Startup.StartupLevelAsset = std::move(StartupLevelAsset);
            Descriptor.Startup.DefaultRenderSettingsAssetId = ReadStringField(Root, "defaultRenderSettings");
            return CanonicalizeDescriptor(std::move(Descriptor));
        }

        /**
         * @brief Parse the current structured descriptor schema.
         * @param Root Parsed structured descriptor root.
         * @return Canonicalized descriptor.
         */
        [[nodiscard]] ProjectDescriptor ParseStructuredDescriptor(const Json& Root)
        {
            ProjectDescriptor Descriptor{};

            const Json Format = ReadObjectSection(Root, "Format");
            Descriptor.Format.SchemaVersion =
                ReadUnsignedField(Format, "SchemaVersion", ProjectDescriptorService::kCurrentSchemaVersion);
            Descriptor.Format.MinimumToolVersion =
                ReadStringField(Format, "MinimumToolVersion", ProjectDescriptorService::kDefaultMinimumToolVersion);

            const Json Project = ReadObjectSection(Root, "Project");
            Descriptor.Project.Name = ReadStringField(Project, "Name");
            Descriptor.Project.DisplayName = ReadStringField(Project, "DisplayName");
            Descriptor.Project.Company = ReadStringField(Project, "Company");
            Descriptor.Project.ProjectId = ReadStringField(Project, "ProjectId");
            Descriptor.Project.Description = ReadStringField(Project, "Description");

            const Json Paths = ReadObjectSection(Root, "Paths");
            Descriptor.Paths.AssetRoot =
                ReadStringField(Paths, "AssetRoot", ProjectDescriptorService::kDefaultAssetRoot);
            Descriptor.Paths.CodeRoot = ReadStringField(Paths, "CodeRoot", ProjectDescriptorService::kDefaultCodeRoot);
            Descriptor.Paths.ConfigRoot =
                ReadStringField(Paths, "ConfigRoot", ProjectDescriptorService::kDefaultConfigRoot);
            Descriptor.Paths.IntermediateRoot =
                ReadStringField(Paths, "IntermediateRoot", ProjectDescriptorService::kDefaultIntermediateRoot);
            Descriptor.Paths.SavedRoot =
                ReadStringField(Paths, "SavedRoot", ProjectDescriptorService::kDefaultSavedRoot);

            const Json Startup = ReadObjectSection(Root, "Startup");
            Descriptor.Startup.StartupLevelAsset =
                ReadStringField(Startup, "StartupLevelAsset", ProjectDescriptorService::kDefaultStartupLevelAsset);
            Descriptor.Startup.DefaultRenderSettingsAssetId = ReadStringField(Startup, "DefaultRenderSettingsAssetId");
            Descriptor.Startup.DefaultGameClass = ReadStringField(Startup, "DefaultGameClass");
            Descriptor.Startup.DefaultGameModeClass = ReadStringField(Startup, "DefaultGameModeClass");

            const auto ModulesIt = Root.find("Modules");
            if (ModulesIt != Root.end() && !ModulesIt->is_null())
            {
                if (!ModulesIt->is_array())
                {
                    throw std::runtime_error("Section 'Modules' must be an array");
                }

                for (const Json& ModuleJson : *ModulesIt)
                {
                    if (!ModuleJson.is_object())
                    {
                        throw std::runtime_error("Section 'Modules' must contain only objects");
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

            auto ProfilesResult = BuildProfileService::ParseProfiles(ReadObjectSection(Root, "Profiles"));
            if (!ProfilesResult)
            {
                throw std::runtime_error(ProfilesResult.error().Message);
            }
            Descriptor.Profiles = std::move(*ProfilesResult);
            Descriptor.Platforms = ReadObjectSection(Root, "Platforms");
            Descriptor.Packaging = ReadObjectSection(Root, "Packaging");
            Descriptor.AssetRules = ReadObjectSection(Root, "AssetRules");
            Descriptor.Templates = ReadObjectSection(Root, "Templates");

            return CanonicalizeDescriptor(std::move(Descriptor));
        }

        /**
         * @brief Resolve one project-file path through the path resolver and absolute-path fallback.
         * @param ProjectFilePath Authored or user-supplied project-file path text.
         * @return Normalized filesystem path to the descriptor file.
         */
        [[nodiscard]] TExpected<std::filesystem::path> ResolveProjectFilePath(const std::string_view ProjectFilePath)
        {
            const std::string PathText = TrimCopy(std::string(ProjectFilePath));
            if (PathText.empty())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project file path cannot be empty"));
            }

            std::filesystem::path ResolvedPath(PathText);
            if (HasUriScheme(PathText))
            {
                auto ResolverResult = SPathResolver::Instance().Resolve(PathText);
                if (!ResolverResult)
                {
                    return std::unexpected(ResolverResult.error());
                }
                ResolvedPath = *ResolverResult;
            }
            else if (auto ResolverResult = SPathResolver::Instance().Resolve(PathText); ResolverResult)
            {
                ResolvedPath = *ResolverResult;
            }
            else if (!ResolvedPath.is_absolute())
            {
                std::error_code Error{};
                ResolvedPath = std::filesystem::absolute(ResolvedPath, Error);
                if (Error)
                {
                    return std::unexpected(MakeError(EErrorCode::InternalError,
                                                     "Failed to resolve project file path: " + Error.message()));
                }
            }

            return ResolvedPath.lexically_normal();
        }

        /**
         * @brief Read an entire descriptor file into memory.
         * @param FilePath File to read.
         * @return Raw UTF-8 text contents.
         */
        [[nodiscard]] TExpected<std::string> ReadFileText(const std::filesystem::path& FilePath)
        {
            std::error_code Error{};
            if (!std::filesystem::exists(FilePath, Error) || Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::NotFound, "Project file was not found: " + FilePath.string()));
            }

            std::ifstream Input(FilePath, std::ios::binary);
            if (!Input.is_open())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to open project file"));
            }

            std::ostringstream Buffer{};
            Buffer << Input.rdbuf();
            const std::string Text = Buffer.str();
            if (Text.empty())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project file is empty"));
            }

            return Text;
        }

        /**
         * @brief Load, parse, canonicalize, and validate one descriptor file.
         * @param ProjectFilePath Resolved descriptor file path.
         * @return Canonical descriptor ready for path resolution.
         */
        [[nodiscard]] TExpected<ProjectDescriptor> LoadDescriptorFromFile(const std::filesystem::path& ProjectFilePath)
        {
            auto TextResult = ReadFileText(ProjectFilePath);
            if (!TextResult)
            {
                return std::unexpected(TextResult.error());
            }

            auto DescriptorResult = ProjectDescriptorService::Parse(*TextResult);
            if (!DescriptorResult)
            {
                return std::unexpected(DescriptorResult.error());
            }

            if (DescriptorResult->Project.Name.empty())
            {
                DescriptorResult->Project.Name = TrimCopy(ProjectFilePath.stem().string());
            }
            if (DescriptorResult->Project.Name.empty())
            {
                DescriptorResult->Project.Name = "Project";
            }
            if (DescriptorResult->Project.DisplayName.empty())
            {
                DescriptorResult->Project.DisplayName = DescriptorResult->Project.Name;
            }

            const auto Issues = ProjectDescriptorService::Validate(*DescriptorResult);
            for (const ProjectDescriptorValidationIssue& Issue : Issues)
            {
                if (Issue.Severity == EProjectDescriptorValidationSeverity::Error)
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument, Issue.Message));
                }
            }

            return *DescriptorResult;
        }

        /**
         * @brief Resolve one stored descriptor path field against a base directory.
         * @param StoredValue Authored path field.
         * @param BaseRoot Descriptor-relative base directory.
         * @return Normalized resolved filesystem path.
         */
        [[nodiscard]] TExpected<std::filesystem::path> ResolveStoredPath(const std::string_view StoredValue,
                                                                         const std::filesystem::path& BaseRoot)
        {
            const std::string Value = TrimCopy(std::string(StoredValue));
            if (Value.empty())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Descriptor path field cannot be empty"));
            }

            if (HasUriScheme(Value))
            {
                auto ResolverResult = SPathResolver::Instance().Resolve(Value);
                if (!ResolverResult)
                {
                    return std::unexpected(ResolverResult.error());
                }
                return ResolverResult->lexically_normal();
            }

            std::filesystem::path Result(Value);
            if (!Result.is_absolute())
            {
                Result = BaseRoot / Result;
            }
            return Result.lexically_normal();
        }

        /**
         * @brief Append one structured validation issue to an issue list.
         * @param Issues Destination issue list.
         * @param Severity Validation severity to record.
         * @param RuleId Stable rule identifier.
         * @param Message Human-readable diagnostic message.
         */
        void AppendIssue(std::vector<ProjectDescriptorValidationIssue>& Issues,
                         const EProjectDescriptorValidationSeverity Severity, const std::string_view RuleId,
                         std::string Message)
        {
            Issues.push_back(ProjectDescriptorValidationIssue{
                .Severity = Severity,
                .RuleId = std::string(RuleId),
                .Message = std::move(Message),
            });
        }

        /**
         * @brief Convert one canonical descriptor into the current structured JSON schema.
         * @param Descriptor Canonical descriptor model.
         * @return Ordered JSON document ready for pretty-print serialization.
         */
        [[nodiscard]] Json SerializeDescriptor(const ProjectDescriptor& Descriptor)
        {
            Json Root = Json::object();

            Root["Format"] = Json::object({
                {"SchemaVersion", Descriptor.Format.SchemaVersion},
                {"MinimumToolVersion", Descriptor.Format.MinimumToolVersion},
            });

            Json Project = Json::object({{"Name", Descriptor.Project.Name}});
            if (!Descriptor.Project.DisplayName.empty())
            {
                Project["DisplayName"] = Descriptor.Project.DisplayName;
            }
            if (!Descriptor.Project.Company.empty())
            {
                Project["Company"] = Descriptor.Project.Company;
            }
            if (!Descriptor.Project.ProjectId.empty())
            {
                Project["ProjectId"] = Descriptor.Project.ProjectId;
            }
            if (!Descriptor.Project.Description.empty())
            {
                Project["Description"] = Descriptor.Project.Description;
            }
            Root["Project"] = std::move(Project);

            Root["Paths"] = Json::object({
                {"AssetRoot", Descriptor.Paths.AssetRoot},
                {"CodeRoot", Descriptor.Paths.CodeRoot},
                {"ConfigRoot", Descriptor.Paths.ConfigRoot},
                {"IntermediateRoot", Descriptor.Paths.IntermediateRoot},
                {"SavedRoot", Descriptor.Paths.SavedRoot},
            });

            Json Startup = Json::object({
                {"StartupLevelAsset", Descriptor.Startup.StartupLevelAsset},
                {"DefaultRenderSettingsAssetId", Descriptor.Startup.DefaultRenderSettingsAssetId},
            });
            if (!Descriptor.Startup.DefaultGameClass.empty())
            {
                Startup["DefaultGameClass"] = Descriptor.Startup.DefaultGameClass;
            }
            if (!Descriptor.Startup.DefaultGameModeClass.empty())
            {
                Startup["DefaultGameModeClass"] = Descriptor.Startup.DefaultGameModeClass;
            }
            Root["Startup"] = std::move(Startup);

            if (!Descriptor.Modules.empty())
            {
                Root["Modules"] = Json::array();
                for (const ProjectModuleDescriptor& Module : Descriptor.Modules)
                {
                    Json ModuleJson = Json::object({
                        {"Name", Module.Name},
                        {"Type", ToString(Module.Type)},
                        {"Root", Module.Root},
                        {"LoadInEditor", Module.LoadInEditor},
                        {"LoadInRuntime", Module.LoadInRuntime},
                        {"UseReflectionGen", Module.UseReflectionGen},
                        {"UseSWIG", Module.UseSWIG},
                    });

                    if (!Module.PublicDependencies.empty())
                    {
                        ModuleJson["PublicDependencies"] = Module.PublicDependencies;
                    }
                    if (!Module.PrivateDependencies.empty())
                    {
                        ModuleJson["PrivateDependencies"] = Module.PrivateDependencies;
                    }
                    if (!Module.Platforms.empty())
                    {
                        ModuleJson["Platforms"] = Module.Platforms;
                    }
                    if (!Module.PreprocessorDefinitions.empty())
                    {
                        ModuleJson["PreprocessorDefinitions"] = Module.PreprocessorDefinitions;
                    }

                    Root["Modules"].push_back(std::move(ModuleJson));
                }
            }

            if (!Descriptor.Profiles.empty())
            {
                auto ProfilesJson = BuildProfileService::SerializeProfiles(Descriptor.Profiles);
                if (!ProfilesJson)
                {
                    throw std::runtime_error(ProfilesJson.error().Message);
                }
                Root["Profiles"] = std::move(*ProfilesJson);
            }
            if (!Descriptor.Platforms.empty())
            {
                Root["Platforms"] = Descriptor.Platforms;
            }
            if (!Descriptor.Packaging.empty())
            {
                Root["Packaging"] = Descriptor.Packaging;
            }
            if (!Descriptor.AssetRules.empty())
            {
                Root["AssetRules"] = Descriptor.AssetRules;
            }
            if (!Descriptor.Templates.empty())
            {
                Root["Templates"] = Descriptor.Templates;
            }

            return Root;
        }

    } // namespace

    TExpected<ProjectDescriptor> ProjectDescriptorService::Parse(const std::string_view JsonText)
    {
        try
        {
            const Json Root = ParseJsonDocument(JsonText);
            ProjectDescriptor Descriptor =
                UsesStructuredSchema(Root) ? ParseStructuredDescriptor(Root) : ParseLegacyDescriptor(Root);
            return CanonicalizeDescriptor(std::move(Descriptor));
        }
        catch (const std::exception& Ex)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
        }
    }

    TExpected<ProjectDescriptor> ProjectDescriptorService::Load(const std::string_view ProjectFilePath)
    {
        auto ResolvedProjectFile = ResolveProjectFilePath(ProjectFilePath);
        if (!ResolvedProjectFile)
        {
            return std::unexpected(ResolvedProjectFile.error());
        }
        return LoadDescriptorFromFile(*ResolvedProjectFile);
    }

    TExpected<ResolvedProjectDescriptor> ProjectDescriptorService::LoadResolved(const std::string_view ProjectFilePath)
    {
        auto ResolvedProjectFile = ResolveProjectFilePath(ProjectFilePath);
        if (!ResolvedProjectFile)
        {
            return std::unexpected(ResolvedProjectFile.error());
        }

        auto DescriptorResult = LoadDescriptorFromFile(*ResolvedProjectFile);
        if (!DescriptorResult)
        {
            return std::unexpected(DescriptorResult.error());
        }

        ResolvedProjectDescriptor Resolved{};
        Resolved.Descriptor = *DescriptorResult;
        Resolved.ProjectFilePath = *ResolvedProjectFile;
        Resolved.ProjectRootDirectory = Resolved.ProjectFilePath.parent_path().lexically_normal();

        auto AssetRootDirectory = ResolveStoredPath(Resolved.Descriptor.Paths.AssetRoot, Resolved.ProjectRootDirectory);
        if (!AssetRootDirectory)
        {
            return std::unexpected(AssetRootDirectory.error());
        }
        Resolved.AssetRootDirectory = *AssetRootDirectory;

        auto CodeRootDirectory = ResolveStoredPath(Resolved.Descriptor.Paths.CodeRoot, Resolved.ProjectRootDirectory);
        if (!CodeRootDirectory)
        {
            return std::unexpected(CodeRootDirectory.error());
        }
        Resolved.CodeRootDirectory = *CodeRootDirectory;

        auto ConfigRootDirectory =
            ResolveStoredPath(Resolved.Descriptor.Paths.ConfigRoot, Resolved.ProjectRootDirectory);
        if (!ConfigRootDirectory)
        {
            return std::unexpected(ConfigRootDirectory.error());
        }
        Resolved.ConfigRootDirectory = *ConfigRootDirectory;

        auto IntermediateRootDirectory =
            ResolveStoredPath(Resolved.Descriptor.Paths.IntermediateRoot, Resolved.ProjectRootDirectory);
        if (!IntermediateRootDirectory)
        {
            return std::unexpected(IntermediateRootDirectory.error());
        }
        Resolved.IntermediateRootDirectory = *IntermediateRootDirectory;

        auto SavedRootDirectory = ResolveStoredPath(Resolved.Descriptor.Paths.SavedRoot, Resolved.ProjectRootDirectory);
        if (!SavedRootDirectory)
        {
            return std::unexpected(SavedRootDirectory.error());
        }
        Resolved.SavedRootDirectory = *SavedRootDirectory;

        auto StartupLevelAssetPath =
            ResolveStoredPath(Resolved.Descriptor.Startup.StartupLevelAsset, Resolved.AssetRootDirectory);
        if (!StartupLevelAssetPath)
        {
            return std::unexpected(StartupLevelAssetPath.error());
        }
        Resolved.StartupLevelAssetPath = *StartupLevelAssetPath;

        const auto Issues = Validate(Resolved);
        for (const ProjectDescriptorValidationIssue& Issue : Issues)
        {
            if (Issue.Severity == EProjectDescriptorValidationSeverity::Error)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, Issue.Message));
            }
        }

        return Resolved;
    }

    TExpected<std::string> ProjectDescriptorService::Serialize(const ProjectDescriptor& Descriptor, const int Indent)
    {
        try
        {
            ProjectDescriptor Normalized = CanonicalizeDescriptor(Descriptor);
            const auto Issues = Validate(Normalized);
            for (const ProjectDescriptorValidationIssue& Issue : Issues)
            {
                if (Issue.Severity == EProjectDescriptorValidationSeverity::Error)
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument, Issue.Message));
                }
            }

            const Json Root = SerializeDescriptor(Normalized);
            return Root.dump(Indent) + "\n";
        }
        catch (const std::exception& Ex)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
        }
    }

    Result ProjectDescriptorService::Save(const ProjectDescriptor& Descriptor, const std::string_view ProjectFilePath)
    {
        auto ResolvedProjectFile = ResolveProjectFilePath(ProjectFilePath);
        if (!ResolvedProjectFile)
        {
            return std::unexpected(ResolvedProjectFile.error());
        }

        ProjectDescriptor Normalized = CanonicalizeDescriptor(Descriptor);
        if (Normalized.Project.Name.empty())
        {
            Normalized.Project.Name = TrimCopy(ResolvedProjectFile->stem().string());
        }
        if (Normalized.Project.Name.empty())
        {
            Normalized.Project.Name = "Project";
        }
        if (Normalized.Project.DisplayName.empty())
        {
            Normalized.Project.DisplayName = Normalized.Project.Name;
        }

        auto TextResult = Serialize(Normalized, 2);
        if (!TextResult)
        {
            return std::unexpected(TextResult.error());
        }

        std::error_code Error{};
        std::filesystem::create_directories(ResolvedProjectFile->parent_path(), Error);
        if (Error)
        {
            return std::unexpected(
                MakeError(EErrorCode::InternalError, "Failed to create project directory: " + Error.message()));
        }

        std::ofstream Output(*ResolvedProjectFile, std::ios::binary | std::ios::trunc);
        if (!Output.is_open())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to open project file for writing"));
        }

        Output.write(TextResult->data(), static_cast<std::streamsize>(TextResult->size()));
        if (!Output.good())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to write project file"));
        }
        Output.flush();
        if (!Output.good())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to flush project file"));
        }

        return Ok();
    }

    std::vector<ProjectDescriptorValidationIssue>
    ProjectDescriptorService::Validate(const ProjectDescriptor& Descriptor)
    {
        std::vector<ProjectDescriptorValidationIssue> Issues{};

        if (Descriptor.Format.SchemaVersion != kCurrentSchemaVersion)
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleUnsupportedSchema,
                        "Unsupported project descriptor schema version: " +
                            std::to_string(Descriptor.Format.SchemaVersion));
        }
        if (TrimCopy(Descriptor.Project.Name).empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingName,
                        "Project descriptor requires a non-empty project name");
        }
        if (TrimCopy(Descriptor.Paths.AssetRoot).empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingAssetRoot,
                        "Project descriptor requires a non-empty asset root");
        }
        if (TrimCopy(Descriptor.Paths.CodeRoot).empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingCodeRoot,
                        "Project descriptor requires a non-empty code root");
        }
        if (TrimCopy(Descriptor.Paths.ConfigRoot).empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingConfigRoot,
                        "Project descriptor requires a non-empty config root");
        }
        if (TrimCopy(Descriptor.Paths.IntermediateRoot).empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingIntermediateRoot,
                        "Project descriptor requires a non-empty intermediate root");
        }
        if (TrimCopy(Descriptor.Paths.SavedRoot).empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingSavedRoot,
                        "Project descriptor requires a non-empty saved root");
        }
        if (TrimCopy(Descriptor.Startup.StartupLevelAsset).empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingStartupAsset,
                        "Project descriptor requires a non-empty startup level asset");
        }

        for (const ProjectModuleDescriptor& Module : Descriptor.Modules)
        {
            if (TrimCopy(Module.Name).empty())
            {
                AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleModuleNameMissing,
                            "Project module entries require a non-empty name");
            }
            if (TrimCopy(Module.Root).empty())
            {
                AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleModuleRootMissing,
                            "Project module '" + Module.Name + "' requires a non-empty root path");
            }
        }

        for (const BuildValidationIssue& ProfileIssue : BuildProfileService::Validate(Descriptor.Profiles))
        {
            Issues.push_back(ConvertBuildIssue(ProfileIssue));
        }

        return Issues;
    }

    std::vector<ProjectDescriptorValidationIssue>
    ProjectDescriptorService::Validate(const ResolvedProjectDescriptor& Descriptor)
    {
        std::vector<ProjectDescriptorValidationIssue> Issues = Validate(Descriptor.Descriptor);

        if (Descriptor.ProjectFilePath.empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingProjectFilePath,
                        "Resolved project descriptor is missing the project file path");
        }
        if (Descriptor.ProjectRootDirectory.empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingProjectRoot,
                        "Resolved project descriptor is missing the project root directory");
        }
        if (Descriptor.AssetRootDirectory.empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingResolvedAssetRoot,
                        "Resolved project descriptor is missing the asset root directory");
        }
        if (Descriptor.StartupLevelAssetPath.empty())
        {
            AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleMissingResolvedStartupAsset,
                        "Resolved project descriptor is missing the startup level asset path");
        }
        else if (!HasUriScheme(Descriptor.Descriptor.Startup.StartupLevelAsset))
        {
            std::error_code ExistsError{};
            if (!std::filesystem::exists(Descriptor.StartupLevelAssetPath, ExistsError))
            {
                std::string Message =
                    "Resolved startup level asset does not exist: `" +
                    Descriptor.StartupLevelAssetPath.lexically_normal().generic_string() + "`";
                if (ExistsError)
                {
                    Message += " (" + ExistsError.message() + ")";
                }
                AppendIssue(Issues, EProjectDescriptorValidationSeverity::Error, kRuleStartupAssetMissingOnDisk,
                            std::move(Message));
            }
        }

        return Issues;
    }

    std::string ProjectDescriptorService::ToProjectRelativePathField(const std::string_view RawValue,
                                                                     const std::filesystem::path& BaseRoot)
    {
        std::string Value = TrimCopy(std::string(RawValue));
        if (Value.empty())
        {
            return {};
        }

        if (HasUriScheme(Value))
        {
            return Value;
        }

        std::filesystem::path ValuePath = std::filesystem::path(Value).lexically_normal();
        if (ValuePath.is_absolute() && !BaseRoot.empty())
        {
            std::error_code RelativeError{};
            std::filesystem::path RelativePath = std::filesystem::relative(ValuePath, BaseRoot, RelativeError);
            if (!RelativeError && !RelativePath.empty())
            {
                const std::string RelativeText = RelativePath.generic_string();
                if (!RelativeText.starts_with("../") && RelativeText != "..")
                {
                    return std::filesystem::path(RelativeText).lexically_normal().generic_string();
                }
            }
        }

        return ValuePath.generic_string();
    }

} // namespace SnAPI::GameFramework
