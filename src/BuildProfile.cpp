#include "BuildProfile.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace SnAPI::GameFramework
{
    namespace
    {

        using Json = nlohmann::ordered_json;

        /**
         * @brief Trim leading and trailing ASCII whitespace from one string copy.
         * @param Text Source text.
         * @return Trimmed copy.
         */
        [[nodiscard]] std::string TrimCopy(const std::string_view Text)
        {
            std::size_t Begin = 0;
            std::size_t End = Text.size();
            while (Begin < End && std::isspace(static_cast<unsigned char>(Text[Begin])) != 0)
            {
                ++Begin;
            }
            while (End > Begin && std::isspace(static_cast<unsigned char>(Text[End - 1])) != 0)
            {
                --End;
            }
            return std::string(Text.substr(Begin, End - Begin));
        }

        /**
         * @brief Append one trimmed string when it is non-empty and not already present.
         * @param Values Destination ordered string list.
         * @param Value Candidate string to append.
         */
        void AppendUniqueTrimmedString(std::vector<std::string>& Values, std::string Value)
        {
            Value = TrimCopy(Value);
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
         * @brief Read one authored string-array field from JSON.
         * @param FieldName Human-readable field name used in diagnostics.
         * @param Value JSON field to parse.
         * @return Normalized authored string values or a structured error.
         */
        [[nodiscard]] TExpected<std::vector<std::string>> ReadStringArray(const std::string_view FieldName,
                                                                          const Json& Value)
        {
            if (!Value.is_array())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, std::string(FieldName) + " must be an array of strings"));
            }

            std::vector<std::string> Values{};
            for (const Json& Entry : Value)
            {
                if (!Entry.is_string())
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InvalidArgument, std::string(FieldName) + " must contain only strings"));
                }
                AppendUniqueTrimmedString(Values, Entry.get<std::string>());
            }
            return Values;
        }

        /**
         * @brief Parse one build configuration string.
         * @param Value Authored configuration string.
         * @return Parsed build configuration or a structured error.
         */
        [[nodiscard]] TExpected<EBuildConfiguration> ParseBuildConfiguration(const std::string_view Value)
        {
            const std::string Normalized = TrimCopy(Value);
            if (Normalized == "Debug")
            {
                return EBuildConfiguration::Debug;
            }
            if (Normalized == "Development")
            {
                return EBuildConfiguration::Development;
            }
            if (Normalized == "Test")
            {
                return EBuildConfiguration::Test;
            }
            if (Normalized == "Shipping")
            {
                return EBuildConfiguration::Shipping;
            }
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "Unknown build configuration '" + Normalized + "'"));
        }

        /**
         * @brief Serialize one build configuration enum to its authored string form.
         * @param Value Build configuration enum.
         * @return Canonical authored string.
         */
        [[nodiscard]] std::string ToString(const EBuildConfiguration Value)
        {
            switch (Value)
            {
            case EBuildConfiguration::Debug:
                return "Debug";
            case EBuildConfiguration::Development:
                return "Development";
            case EBuildConfiguration::Test:
                return "Test";
            case EBuildConfiguration::Shipping:
                return "Shipping";
            }
            return "Development";
        }

        /**
         * @brief Parse one dependency-policy string.
         * @param Value Authored dependency-policy string.
         * @return Parsed dependency policy or a structured error.
         */
        [[nodiscard]] TExpected<EAssetDependencyPolicy> ParseDependencyPolicy(const std::string_view Value)
        {
            const std::string Normalized = TrimCopy(Value);
            if (Normalized == "HardOnly")
            {
                return EAssetDependencyPolicy::HardOnly;
            }
            if (Normalized == "HardAndSoft")
            {
                return EAssetDependencyPolicy::HardAndSoft;
            }
            if (Normalized == "HardSoftAndEditorPreview")
            {
                return EAssetDependencyPolicy::HardSoftAndEditorPreview;
            }
            if (Normalized == "CustomResolver")
            {
                return EAssetDependencyPolicy::CustomResolver;
            }
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "Unknown dependency policy '" + Normalized + "'"));
        }

        /**
         * @brief Serialize one dependency-policy enum to its authored string form.
         * @param Value Dependency-policy enum.
         * @return Canonical authored string.
         */
        [[nodiscard]] std::string ToString(const EAssetDependencyPolicy Value)
        {
            switch (Value)
            {
            case EAssetDependencyPolicy::HardOnly:
                return "HardOnly";
            case EAssetDependencyPolicy::HardAndSoft:
                return "HardAndSoft";
            case EAssetDependencyPolicy::HardSoftAndEditorPreview:
                return "HardSoftAndEditorPreview";
            case EAssetDependencyPolicy::CustomResolver:
                return "CustomResolver";
            }
            return "HardAndSoft";
        }

        /**
         * @brief Parse one chunk-strategy string.
         * @param Value Authored chunk-strategy string.
         * @return Parsed chunk strategy or a structured error.
         */
        [[nodiscard]] TExpected<EAssetChunkStrategy> ParseChunkStrategy(const std::string_view Value)
        {
            const std::string Normalized = TrimCopy(Value);
            if (Normalized == "Monolithic")
            {
                return EAssetChunkStrategy::Monolithic;
            }
            if (Normalized == "SharedPlusPerLevel")
            {
                return EAssetChunkStrategy::SharedPlusPerLevel;
            }
            if (Normalized == "PerLabel")
            {
                return EAssetChunkStrategy::PerLabel;
            }
            if (Normalized == "CustomGraph")
            {
                return EAssetChunkStrategy::CustomGraph;
            }
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "Unknown chunk strategy '" + Normalized + "'"));
        }

        /**
         * @brief Serialize one chunk-strategy enum to its authored string form.
         * @param Value Chunk-strategy enum.
         * @return Canonical authored string.
         */
        [[nodiscard]] std::string ToString(const EAssetChunkStrategy Value)
        {
            switch (Value)
            {
            case EAssetChunkStrategy::Monolithic:
                return "Monolithic";
            case EAssetChunkStrategy::SharedPlusPerLevel:
                return "SharedPlusPerLevel";
            case EAssetChunkStrategy::PerLabel:
                return "PerLabel";
            case EAssetChunkStrategy::CustomGraph:
                return "CustomGraph";
            }
            return "Monolithic";
        }

        /**
         * @brief Read one optional string override field that supports explicit null clearing.
         * @param Object JSON object to inspect.
         * @param FieldName Field name.
         * @return Parsed profile value patch or a structured error.
         */
        [[nodiscard]] TExpected<BuildProfileValue<std::string>> ReadStringValueField(const Json& Object,
                                                                                     const std::string_view FieldName)
        {
            BuildProfileValue<std::string> Result{};
            const auto It = Object.find(FieldName);
            if (It == Object.end())
            {
                return Result;
            }

            Result.IsSet = true;
            if (It->is_null())
            {
                return Result;
            }
            if (!It->is_string())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, std::string(FieldName) + " must be a string or null"));
            }

            Result.Value = TrimCopy(It->get<std::string>());
            return Result;
        }

        /**
         * @brief Read one optional boolean override field that supports explicit null clearing.
         * @param Object JSON object to inspect.
         * @param FieldName Field name.
         * @return Parsed profile value patch or a structured error.
         */
        [[nodiscard]] TExpected<BuildProfileValue<bool>> ReadBoolValueField(const Json& Object,
                                                                            const std::string_view FieldName)
        {
            BuildProfileValue<bool> Result{};
            const auto It = Object.find(FieldName);
            if (It == Object.end())
            {
                return Result;
            }

            Result.IsSet = true;
            if (It->is_null())
            {
                return Result;
            }
            if (!It->is_boolean())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, std::string(FieldName) + " must be a boolean or null"));
            }

            Result.Value = It->get<bool>();
            return Result;
        }

        /**
         * @brief Read one optional enum override field that supports explicit null clearing.
         * @param Object JSON object to inspect.
         * @param FieldName Field name.
         * @param Parser Enum parser function.
         * @return Parsed profile value patch or a structured error.
         */
        template <typename TEnum, typename TParser>
        [[nodiscard]] TExpected<BuildProfileValue<TEnum>>
        ReadEnumValueField(const Json& Object, const std::string_view FieldName, TParser&& Parser)
        {
            BuildProfileValue<TEnum> Result{};
            const auto It = Object.find(FieldName);
            if (It == Object.end())
            {
                return Result;
            }

            Result.IsSet = true;
            if (It->is_null())
            {
                return Result;
            }
            if (!It->is_string())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, std::string(FieldName) + " must be a string or null"));
            }

            auto ValueResult = Parser(It->get<std::string>());
            if (!ValueResult)
            {
                return std::unexpected(ValueResult.error());
            }
            Result.Value = *ValueResult;
            return Result;
        }

        /**
         * @brief Read one authored string-list patch field.
         * @param Object JSON object to inspect.
         * @param FieldName Field name.
         * @return Parsed string-list patch or a structured error.
         */
        [[nodiscard]] TExpected<BuildProfileStringList> ReadStringListField(const Json& Object,
                                                                            const std::string_view FieldName)
        {
            BuildProfileStringList Result{};
            const auto It = Object.find(FieldName);
            if (It == Object.end())
            {
                return Result;
            }

            Result.IsSet = true;
            if (It->is_null())
            {
                Result.Replace = true;
                return Result;
            }
            if (It->is_array())
            {
                auto ValuesResult = ReadStringArray(FieldName, *It);
                if (!ValuesResult)
                {
                    return std::unexpected(ValuesResult.error());
                }
                Result.Values = std::move(*ValuesResult);
                return Result;
            }
            if (!It->is_object())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 std::string(FieldName) + " must be an array, object, or null"));
            }

            const Json& ListObject = *It;
            const auto ReplaceIt = ListObject.find("Replace");
            if (ReplaceIt != ListObject.end())
            {
                if (!ReplaceIt->is_boolean())
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InvalidArgument, std::string(FieldName) + ".Replace must be a boolean"));
                }
                Result.Replace = ReplaceIt->get<bool>();
            }

            const auto ValuesIt = ListObject.find("Values");
            if (ValuesIt != ListObject.end())
            {
                auto ValuesResult = ReadStringArray(std::string(FieldName) + ".Values", *ValuesIt);
                if (!ValuesResult)
                {
                    return std::unexpected(ValuesResult.error());
                }
                Result.Values = std::move(*ValuesResult);
            }

            return Result;
        }

        /**
         * @brief Read one authored archive-settings patch field.
         * @param Object JSON object to inspect.
         * @param FieldName Field name.
         * @return Parsed archive-settings patch or a structured error.
         */
        [[nodiscard]] TExpected<BuildProfileArchiveSettings> ReadArchiveField(const Json& Object,
                                                                              const std::string_view FieldName)
        {
            BuildProfileArchiveSettings Result{};
            const auto It = Object.find(FieldName);
            if (It == Object.end())
            {
                return Result;
            }

            Result.IsSet = true;
            if (It->is_null())
            {
                Result.ReplaceEntireObject = true;
                return Result;
            }
            if (!It->is_object())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, std::string(FieldName) + " must be an object or null"));
            }

            const Json& ArchiveObject = *It;
            const auto ReplaceIt = ArchiveObject.find("ReplaceEntireObject");
            if (ReplaceIt != ArchiveObject.end())
            {
                if (!ReplaceIt->is_boolean())
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InvalidArgument,
                                  std::string(FieldName) + ".ReplaceEntireObject must be a boolean"));
                }
                Result.ReplaceEntireObject = ReplaceIt->get<bool>();
            }

            auto EnabledResult = ReadBoolValueField(ArchiveObject, "Enabled");
            if (!EnabledResult)
            {
                return std::unexpected(EnabledResult.error());
            }
            Result.Enabled = std::move(*EnabledResult);

            auto FormatResult = ReadStringValueField(ArchiveObject, "Format");
            if (!FormatResult)
            {
                return std::unexpected(FormatResult.error());
            }
            Result.Format = std::move(*FormatResult);

            return Result;
        }

        /**
         * @brief Serialize one string-list patch into descriptor JSON.
         * @param Value Authored string-list patch.
         * @return JSON field value.
         */
        [[nodiscard]] Json SerializeStringList(const BuildProfileStringList& Value)
        {
            if (Value.Replace)
            {
                return Json::object({{"Values", Value.Values}, {"Replace", true}});
            }
            return Json(Value.Values);
        }

        /**
         * @brief Return `true` when the archive patch carries any authored child fields.
         * @param Value Archive patch to inspect.
         * @return `true` when child fields are authored.
         */
        [[nodiscard]] bool HasArchiveFields(const BuildProfileArchiveSettings& Value)
        {
            return Value.Enabled.IsSet || Value.Format.IsSet;
        }

        /**
         * @brief Serialize one archive-settings patch into descriptor JSON.
         * @param Value Authored archive patch.
         * @return JSON field value.
         */
        [[nodiscard]] Json SerializeArchive(const BuildProfileArchiveSettings& Value)
        {
            if (Value.ReplaceEntireObject && !HasArchiveFields(Value))
            {
                return nullptr;
            }

            Json Archive = Json::object();
            if (Value.ReplaceEntireObject)
            {
                Archive["ReplaceEntireObject"] = true;
            }
            if (Value.Enabled.IsSet)
            {
                Archive["Enabled"] = Value.Enabled.Value.has_value() ? Json(*Value.Enabled.Value) : Json(nullptr);
            }
            if (Value.Format.IsSet)
            {
                Archive["Format"] = Value.Format.Value.has_value() ? Json(*Value.Format.Value) : Json(nullptr);
            }
            return Archive;
        }

        /**
         * @brief Merge one authored list patch into the resolved destination list.
         * @param Destination Resolved destination list.
         * @param Patch Authored list patch.
         */
        void ApplyStringList(std::vector<std::string>& Destination, const BuildProfileStringList& Patch)
        {
            if (!Patch.IsSet)
            {
                return;
            }
            if (Patch.Replace)
            {
                Destination.clear();
            }
            for (const std::string& Value : Patch.Values)
            {
                AppendUniqueTrimmedString(Destination, Value);
            }
        }

        /**
         * @brief Merge one authored scalar patch into the resolved destination value.
         * @param Destination Resolved destination value.
         * @param Patch Authored scalar patch.
         * @param DefaultValue Default value used when the patch explicitly clears the field.
         */
        template <typename TValue>
        void ApplyValue(TValue& Destination, const BuildProfileValue<TValue>& Patch, const TValue& DefaultValue)
        {
            if (!Patch.IsSet)
            {
                return;
            }
            Destination = Patch.Value.has_value() ? *Patch.Value : DefaultValue;
        }

        /**
         * @brief Merge one authored archive patch into the resolved destination values.
         * @param Destination Resolved destination profile.
         * @param Patch Authored archive patch.
         */
        void ApplyArchive(ResolvedBuildProfile& Destination, const BuildProfileArchiveSettings& Patch)
        {
            if (!Patch.IsSet)
            {
                return;
            }

            if (Patch.ReplaceEntireObject)
            {
                Destination.ArchiveEnabled = false;
                Destination.ArchiveFormat.clear();
            }

            ApplyValue(Destination.ArchiveEnabled, Patch.Enabled, false);
            ApplyValue(Destination.ArchiveFormat, Patch.Format, std::string{});
        }

        /**
         * @brief Return the first blocking validation issue when one exists.
         * @param Issues Validation issues to inspect.
         * @return First error issue or `nullptr`.
         */
        [[nodiscard]] const BuildValidationIssue* FindBlockingIssue(const std::vector<BuildValidationIssue>& Issues)
        {
            const auto It = std::ranges::find_if(Issues, [](const BuildValidationIssue& Issue)
                                                 { return Issue.Severity == EBuildValidationSeverity::Error; });
            return It == Issues.end() ? nullptr : std::addressof(*It);
        }

    } // namespace

    TExpected<std::vector<BuildProfile>> BuildProfileService::ParseProfiles(const nlohmann::ordered_json& ProfilesJson)
    {
        if (!ProfilesJson.is_object())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Profiles must be a JSON object"));
        }

        std::vector<BuildProfile> Profiles{};
        Profiles.reserve(ProfilesJson.size());
        for (const auto& [ProfileName, ProfileJson] : ProfilesJson.items())
        {
            if (!ProfileJson.is_object())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Profile '" + ProfileName + "' must be a JSON object"));
            }

            BuildProfile Profile{};
            Profile.Name = TrimCopy(ProfileName);

            auto InheritsResult = ReadStringValueField(ProfileJson, "Inherits");
            if (!InheritsResult)
            {
                return std::unexpected(InheritsResult.error());
            }
            Profile.Inherits = InheritsResult->Value.value_or(std::string{});

            auto PlatformResult = ReadStringValueField(ProfileJson, "Platform");
            if (!PlatformResult)
            {
                return std::unexpected(PlatformResult.error());
            }
            Profile.Platform = std::move(*PlatformResult);

            auto EnvironmentResult = ReadStringValueField(ProfileJson, "ExecutionEnvironment");
            if (!EnvironmentResult)
            {
                return std::unexpected(EnvironmentResult.error());
            }
            Profile.ExecutionEnvironment = std::move(*EnvironmentResult);

            auto ConfigurationResult =
                ReadEnumValueField<EBuildConfiguration>(ProfileJson, "Configuration", ParseBuildConfiguration);
            if (!ConfigurationResult)
            {
                return std::unexpected(ConfigurationResult.error());
            }
            Profile.Configuration = std::move(*ConfigurationResult);

            auto SelectedLevelsResult = ReadStringListField(ProfileJson, "SelectedLevels");
            if (!SelectedLevelsResult)
            {
                return std::unexpected(SelectedLevelsResult.error());
            }
            Profile.SelectedLevels = std::move(*SelectedLevelsResult);

            auto ExplicitAssetsResult = ReadStringListField(ProfileJson, "ExplicitAssets");
            if (!ExplicitAssetsResult)
            {
                return std::unexpected(ExplicitAssetsResult.error());
            }
            Profile.ExplicitAssets = std::move(*ExplicitAssetsResult);

            auto IncludeFoldersResult = ReadStringListField(ProfileJson, "IncludeFolders");
            if (!IncludeFoldersResult)
            {
                return std::unexpected(IncludeFoldersResult.error());
            }
            Profile.IncludeFolders = std::move(*IncludeFoldersResult);

            auto ExcludeFoldersResult = ReadStringListField(ProfileJson, "ExcludeFolders");
            if (!ExcludeFoldersResult)
            {
                return std::unexpected(ExcludeFoldersResult.error());
            }
            Profile.ExcludeFolders = std::move(*ExcludeFoldersResult);

            auto IncludeLabelsResult = ReadStringListField(ProfileJson, "IncludeAssetLabels");
            if (!IncludeLabelsResult)
            {
                return std::unexpected(IncludeLabelsResult.error());
            }
            Profile.IncludeAssetLabels = std::move(*IncludeLabelsResult);

            auto ExcludeLabelsResult = ReadStringListField(ProfileJson, "ExcludeAssetLabels");
            if (!ExcludeLabelsResult)
            {
                return std::unexpected(ExcludeLabelsResult.error());
            }
            Profile.ExcludeAssetLabels = std::move(*ExcludeLabelsResult);

            auto IncludeKindsResult = ReadStringListField(ProfileJson, "IncludeAssetKinds");
            if (!IncludeKindsResult)
            {
                return std::unexpected(IncludeKindsResult.error());
            }
            Profile.IncludeAssetKinds = std::move(*IncludeKindsResult);

            auto ExcludeKindsResult = ReadStringListField(ProfileJson, "ExcludeAssetKinds");
            if (!ExcludeKindsResult)
            {
                return std::unexpected(ExcludeKindsResult.error());
            }
            Profile.ExcludeAssetKinds = std::move(*ExcludeKindsResult);

            auto DependencyPolicyResult =
                ReadEnumValueField<EAssetDependencyPolicy>(ProfileJson, "DependencyPolicy", ParseDependencyPolicy);
            if (!DependencyPolicyResult)
            {
                return std::unexpected(DependencyPolicyResult.error());
            }
            Profile.DependencyPolicy = std::move(*DependencyPolicyResult);

            auto ChunkStrategyResult =
                ReadEnumValueField<EAssetChunkStrategy>(ProfileJson, "ChunkStrategy", ParseChunkStrategy);
            if (!ChunkStrategyResult)
            {
                return std::unexpected(ChunkStrategyResult.error());
            }
            Profile.ChunkStrategy = std::move(*ChunkStrategyResult);

            auto OverrideExcludesResult = ReadBoolValueField(ProfileJson, "AllowExplicitOverrideExcludes");
            if (!OverrideExcludesResult)
            {
                return std::unexpected(OverrideExcludesResult.error());
            }
            Profile.AllowExplicitOverrideExcludes = std::move(*OverrideExcludesResult);

            auto ArchiveResult = ReadArchiveField(ProfileJson, "Archive");
            if (!ArchiveResult)
            {
                return std::unexpected(ArchiveResult.error());
            }
            Profile.Archive = std::move(*ArchiveResult);

            Profiles.push_back(std::move(Profile));
        }

        return Profiles;
    }

    TExpected<nlohmann::ordered_json> BuildProfileService::SerializeProfiles(const std::vector<BuildProfile>& Profiles)
    {
        const auto Issues = Validate(Profiles);
        if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(Issues); BlockingIssue != nullptr)
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
        }

        Json Root = Json::object();
        for (const BuildProfile& Profile : Profiles)
        {
            Json ProfileJson = Json::object();
            if (!Profile.Inherits.empty())
            {
                ProfileJson["Inherits"] = Profile.Inherits;
            }
            if (Profile.Platform.IsSet)
            {
                ProfileJson["Platform"] =
                    Profile.Platform.Value.has_value() ? Json(*Profile.Platform.Value) : Json(nullptr);
            }
            if (Profile.ExecutionEnvironment.IsSet)
            {
                ProfileJson["ExecutionEnvironment"] = Profile.ExecutionEnvironment.Value.has_value()
                    ? Json(*Profile.ExecutionEnvironment.Value)
                    : Json(nullptr);
            }
            if (Profile.Configuration.IsSet)
            {
                ProfileJson["Configuration"] = Profile.Configuration.Value.has_value()
                    ? Json(ToString(*Profile.Configuration.Value))
                    : Json(nullptr);
            }
            if (Profile.SelectedLevels.IsSet)
            {
                ProfileJson["SelectedLevels"] = SerializeStringList(Profile.SelectedLevels);
            }
            if (Profile.ExplicitAssets.IsSet)
            {
                ProfileJson["ExplicitAssets"] = SerializeStringList(Profile.ExplicitAssets);
            }
            if (Profile.IncludeFolders.IsSet)
            {
                ProfileJson["IncludeFolders"] = SerializeStringList(Profile.IncludeFolders);
            }
            if (Profile.ExcludeFolders.IsSet)
            {
                ProfileJson["ExcludeFolders"] = SerializeStringList(Profile.ExcludeFolders);
            }
            if (Profile.IncludeAssetLabels.IsSet)
            {
                ProfileJson["IncludeAssetLabels"] = SerializeStringList(Profile.IncludeAssetLabels);
            }
            if (Profile.ExcludeAssetLabels.IsSet)
            {
                ProfileJson["ExcludeAssetLabels"] = SerializeStringList(Profile.ExcludeAssetLabels);
            }
            if (Profile.IncludeAssetKinds.IsSet)
            {
                ProfileJson["IncludeAssetKinds"] = SerializeStringList(Profile.IncludeAssetKinds);
            }
            if (Profile.ExcludeAssetKinds.IsSet)
            {
                ProfileJson["ExcludeAssetKinds"] = SerializeStringList(Profile.ExcludeAssetKinds);
            }
            if (Profile.DependencyPolicy.IsSet)
            {
                ProfileJson["DependencyPolicy"] = Profile.DependencyPolicy.Value.has_value()
                    ? Json(ToString(*Profile.DependencyPolicy.Value))
                    : Json(nullptr);
            }
            if (Profile.ChunkStrategy.IsSet)
            {
                ProfileJson["ChunkStrategy"] = Profile.ChunkStrategy.Value.has_value()
                    ? Json(ToString(*Profile.ChunkStrategy.Value))
                    : Json(nullptr);
            }
            if (Profile.AllowExplicitOverrideExcludes.IsSet)
            {
                ProfileJson["AllowExplicitOverrideExcludes"] = Profile.AllowExplicitOverrideExcludes.Value.has_value()
                    ? Json(*Profile.AllowExplicitOverrideExcludes.Value)
                    : Json(nullptr);
            }
            if (Profile.Archive.IsSet)
            {
                ProfileJson["Archive"] = SerializeArchive(Profile.Archive);
            }

            Root[Profile.Name] = std::move(ProfileJson);
        }

        return Root;
    }

    std::vector<BuildValidationIssue> BuildProfileService::Validate(const std::vector<BuildProfile>& Profiles)
    {
        std::vector<BuildValidationIssue> Issues{};
        std::unordered_map<std::string, const BuildProfile*> ProfileByName{};
        ProfileByName.reserve(Profiles.size());

        for (const BuildProfile& Profile : Profiles)
        {
            if (TrimCopy(Profile.Name).empty())
            {
                Issues.push_back(BuildValidationIssue{
                    .Severity = EBuildValidationSeverity::Error,
                    .RuleId = "BuildProfile.NameMissing",
                    .Message = "Build profiles must have a non-empty name.",
                });
                continue;
            }

            const auto [It, Inserted] = ProfileByName.emplace(Profile.Name, std::addressof(Profile));
            if (!Inserted)
            {
                Issues.push_back(BuildValidationIssue{
                    .Severity = EBuildValidationSeverity::Error,
                    .RuleId = "BuildProfile.NameDuplicate",
                    .Message = "Build profile '" + Profile.Name + "' is declared more than once.",
                });
            }

            if (!Profile.Inherits.empty() && Profile.Inherits == Profile.Name)
            {
                Issues.push_back(BuildValidationIssue{
                    .Severity = EBuildValidationSeverity::Error,
                    .RuleId = "BuildProfile.InheritsSelf",
                    .Message = "Build profile '" + Profile.Name + "' cannot inherit from itself.",
                });
            }
        }

        for (const BuildProfile& Profile : Profiles)
        {
            if (!Profile.Inherits.empty() && !ProfileByName.contains(Profile.Inherits))
            {
                Issues.push_back(BuildValidationIssue{
                    .Severity = EBuildValidationSeverity::Error,
                    .RuleId = "BuildProfile.UnknownParent",
                    .Message =
                        "Build profile '" + Profile.Name + "' inherits unknown profile '" + Profile.Inherits + "'.",
                });
            }
        }

        std::unordered_set<std::string> Visited{};
        std::unordered_set<std::string> Active{};
        std::function<void(const BuildProfile&)> Visit = [&](const BuildProfile& Profile)
        {
            if (Visited.contains(Profile.Name))
            {
                return;
            }
            if (Active.contains(Profile.Name))
            {
                Issues.push_back(BuildValidationIssue{
                    .Severity = EBuildValidationSeverity::Error,
                    .RuleId = "BuildProfile.InheritanceCycle",
                    .Message = "Build profile inheritance contains a cycle at '" + Profile.Name + "'.",
                });
                return;
            }

            Active.insert(Profile.Name);
            if (!Profile.Inherits.empty())
            {
                const auto It = ProfileByName.find(Profile.Inherits);
                if (It != ProfileByName.end())
                {
                    Visit(*It->second);
                }
            }
            Active.erase(Profile.Name);
            Visited.insert(Profile.Name);
        };

        for (const BuildProfile& Profile : Profiles)
        {
            Visit(Profile);
        }

        return Issues;
    }

    TExpected<ResolvedBuildProfile> BuildProfileService::ResolveProfile(const std::vector<BuildProfile>& Profiles,
                                                                        const std::string_view ProfileName,
                                                                        const std::size_t MaxInheritanceDepth)
    {
        const auto Issues = Validate(Profiles);
        if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(Issues); BlockingIssue != nullptr)
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
        }

        std::unordered_map<std::string, const BuildProfile*> ProfileByName{};
        ProfileByName.reserve(Profiles.size());
        for (const BuildProfile& Profile : Profiles)
        {
            ProfileByName.emplace(Profile.Name, std::addressof(Profile));
        }

        const std::string Name = TrimCopy(ProfileName);
        const auto RootIt = ProfileByName.find(Name);
        if (RootIt == ProfileByName.end())
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Unknown build profile '" + Name + "'"));
        }

        std::function<TExpected<ResolvedBuildProfile>(const BuildProfile&, std::size_t)> ResolveRecursive =
            [&](const BuildProfile& Profile, const std::size_t Depth) -> TExpected<ResolvedBuildProfile>
        {
            if (Depth > MaxInheritanceDepth)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "Build profile inheritance depth exceeded the maximum of " +
                                                     std::to_string(MaxInheritanceDepth) + " while resolving '" +
                                                     Profile.Name + "'."));
            }

            ResolvedBuildProfile Resolved{};
            Resolved.Name = Profile.Name;
            Resolved.Inherits = Profile.Inherits;

            if (!Profile.Inherits.empty())
            {
                const auto ParentIt = ProfileByName.find(Profile.Inherits);
                if (ParentIt == ProfileByName.end())
                {
                    return std::unexpected(
                        MakeError(EErrorCode::NotFound, "Unknown parent build profile '" + Profile.Inherits + "'."));
                }

                auto ParentResolved = ResolveRecursive(*ParentIt->second, Depth + 1);
                if (!ParentResolved)
                {
                    return std::unexpected(ParentResolved.error());
                }
                Resolved = std::move(*ParentResolved);
                Resolved.Name = Profile.Name;
                Resolved.Inherits = Profile.Inherits;
            }

            ApplyValue(Resolved.Platform, Profile.Platform, std::string{});
            ApplyValue(Resolved.ExecutionEnvironment, Profile.ExecutionEnvironment, std::string{});
            ApplyValue(Resolved.Configuration, Profile.Configuration, EBuildConfiguration::Development);
            ApplyStringList(Resolved.SelectedLevels, Profile.SelectedLevels);
            ApplyStringList(Resolved.ExplicitAssets, Profile.ExplicitAssets);
            ApplyStringList(Resolved.IncludeFolders, Profile.IncludeFolders);
            ApplyStringList(Resolved.ExcludeFolders, Profile.ExcludeFolders);
            ApplyStringList(Resolved.IncludeAssetLabels, Profile.IncludeAssetLabels);
            ApplyStringList(Resolved.ExcludeAssetLabels, Profile.ExcludeAssetLabels);
            ApplyStringList(Resolved.IncludeAssetKinds, Profile.IncludeAssetKinds);
            ApplyStringList(Resolved.ExcludeAssetKinds, Profile.ExcludeAssetKinds);
            ApplyValue(Resolved.DependencyPolicy, Profile.DependencyPolicy, EAssetDependencyPolicy::HardAndSoft);
            ApplyValue(Resolved.ChunkStrategy, Profile.ChunkStrategy, EAssetChunkStrategy::Monolithic);
            ApplyValue(Resolved.AllowExplicitOverrideExcludes, Profile.AllowExplicitOverrideExcludes, false);
            ApplyArchive(Resolved, Profile.Archive);

            return Resolved;
        };

        return ResolveRecursive(*RootIt->second, 0);
    }

} // namespace SnAPI::GameFramework
