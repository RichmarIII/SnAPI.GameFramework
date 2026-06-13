#include "BuildRequest.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <ranges>
#include <sstream>
#include <utility>

namespace SnAPI::GameFramework
{
    namespace
    {

        using Json = nlohmann::ordered_json;

        constexpr std::string_view kRuleMissingProjectFilePath = "BuildRequest.ProjectFilePathMissing";
        constexpr std::string_view kRuleMissingPlatform = "BuildRequest.PlatformMissing";
        constexpr std::string_view kRuleMissingArchiveFormat = "BuildRequest.ArchiveFormatMissing";
        constexpr std::string_view kRuleMissingExecutionEnvironment = "BuildRequest.ExecutionEnvironmentUnspecified";
        constexpr std::string_view kRuleProjectRootMissing = "BuildRequest.ProjectRootMissing";

        constexpr std::uint64_t kFnv1aOffset = 14695981039346656037ull;
        constexpr std::uint64_t kFnv1aPrime = 1099511628211ull;

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
         * @param Values Destination ordered list.
         * @param Value Candidate value to append.
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
         * @brief Normalize one authored string-list patch in place.
         * @param List Authored list patch to normalize.
         */
        void CanonicalizeList(BuildProfileStringList& List)
        {
            std::vector<std::string> Normalized{};
            for (std::string& Value : List.Values)
            {
                AppendUniqueTrimmedString(Normalized, std::move(Value));
            }
            List.Values = std::move(Normalized);
        }

        /**
         * @brief Normalize one authored string override in place.
         * @param Value Authored string patch to normalize.
         */
        void CanonicalizeValue(BuildProfileValue<std::string>& Value)
        {
            if (!Value.IsSet || !Value.Value.has_value())
            {
                return;
            }
            *Value.Value = TrimCopy(*Value.Value);
        }

        /**
         * @brief Normalize one build-request override block in place.
         * @param Overrides Override block to normalize.
         */
        void CanonicalizeOverrides(BuildRequestOverrides& Overrides)
        {
            CanonicalizeValue(Overrides.Platform);
            CanonicalizeValue(Overrides.ExecutionEnvironment);
            CanonicalizeValue(Overrides.Archive.Format);

            CanonicalizeList(Overrides.SelectedLevels);
            CanonicalizeList(Overrides.ExplicitAssets);
            CanonicalizeList(Overrides.IncludeFolders);
            CanonicalizeList(Overrides.ExcludeFolders);
            CanonicalizeList(Overrides.IncludeAssetLabels);
            CanonicalizeList(Overrides.ExcludeAssetLabels);
            CanonicalizeList(Overrides.IncludeAssetKinds);
            CanonicalizeList(Overrides.ExcludeAssetKinds);
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
         * @brief Merge one authored archive patch into the resolved destination values.
         * @param Destination Resolved profile to update.
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
         * @brief Convert one project-descriptor validation issue to the build-validation domain.
         * @param Issue Descriptor-validation issue to convert.
         * @return Equivalent build-validation issue.
         */
        [[nodiscard]] BuildValidationIssue ConvertProjectIssue(const ProjectDescriptorValidationIssue& Issue)
        {
            EBuildValidationSeverity Severity = EBuildValidationSeverity::Error;
            switch (Issue.Severity)
            {
            case EProjectDescriptorValidationSeverity::Info:
                Severity = EBuildValidationSeverity::Info;
                break;
            case EProjectDescriptorValidationSeverity::Warning:
                Severity = EBuildValidationSeverity::Warning;
                break;
            case EProjectDescriptorValidationSeverity::Error:
                Severity = EBuildValidationSeverity::Error;
                break;
            }

            return BuildValidationIssue{
                .Severity = Severity,
                .RuleId = Issue.RuleId,
                .Message = Issue.Message,
            };
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

        /**
         * @brief Append one build-validation issue to a destination list.
         * @param Issues Destination issue list.
         * @param Severity Validation severity to record.
         * @param RuleId Stable rule identifier.
         * @param Message Human-readable diagnostic message.
         */
        void AppendIssue(std::vector<BuildValidationIssue>& Issues, const EBuildValidationSeverity Severity,
                         const std::string_view RuleId, std::string Message)
        {
            Issues.push_back(BuildValidationIssue{
                .Severity = Severity,
                .RuleId = std::string(RuleId),
                .Message = std::move(Message),
            });
        }

        /**
         * @brief Build canonical JSON for one frozen request.
         * @param Request Frozen build request to serialize.
         * @param IncludeHash `true` to include the existing request hash field.
         * @return Canonical ordered JSON object.
         */
        [[nodiscard]] Json BuildFrozenRequestJson(const ResolvedBuildRequest& Request, const bool IncludeHash)
        {
            Json Root = Json::object();

            Json Project = Json::object({
                {"Name", Request.Project.Descriptor.Project.Name},
                {"ProjectId", Request.Project.Descriptor.Project.ProjectId},
                {"ProjectRoot", Request.Project.ProjectRootDirectory.lexically_normal().generic_string()},
                {"ProjectFile",
                 ProjectDescriptorService::ToProjectRelativePathField(Request.Project.ProjectFilePath.string(),
                                                                      Request.Project.ProjectRootDirectory)},
                {"AssetRoot", Request.Project.Descriptor.Paths.AssetRoot},
                {"CodeRoot", Request.Project.Descriptor.Paths.CodeRoot},
                {"ConfigRoot", Request.Project.Descriptor.Paths.ConfigRoot},
                {"IntermediateRoot", Request.Project.Descriptor.Paths.IntermediateRoot},
                {"SavedRoot", Request.Project.Descriptor.Paths.SavedRoot},
                {"StartupLevelAsset", Request.Project.Descriptor.Startup.StartupLevelAsset},
            });
            if (!Request.Project.Descriptor.Startup.DefaultGameClass.empty())
            {
                Project["DefaultGameClass"] = Request.Project.Descriptor.Startup.DefaultGameClass;
            }
            if (!Request.Project.Descriptor.Startup.DefaultGameModeClass.empty())
            {
                Project["DefaultGameModeClass"] = Request.Project.Descriptor.Startup.DefaultGameModeClass;
            }
            Root["Project"] = std::move(Project);

            Root["ProfileName"] = Request.ProfileName;
            Root["ResolvedProfile"] = Json::object({
                {"Name", Request.Profile.Name},
                {"Inherits", Request.Profile.Inherits},
                {"Platform", Request.Profile.Platform},
                {"ExecutionEnvironment", Request.Profile.ExecutionEnvironment},
                {"Configuration",
                 Request.Profile.Configuration == EBuildConfiguration::Debug             ? "Debug"
                     : Request.Profile.Configuration == EBuildConfiguration::Development ? "Development"
                     : Request.Profile.Configuration == EBuildConfiguration::Test        ? "Test"
                                                                                         : "Shipping"},
                {"SelectedLevels", Request.Profile.SelectedLevels},
                {"ExplicitAssets", Request.Profile.ExplicitAssets},
                {"IncludeFolders", Request.Profile.IncludeFolders},
                {"ExcludeFolders", Request.Profile.ExcludeFolders},
                {"IncludeAssetLabels", Request.Profile.IncludeAssetLabels},
                {"ExcludeAssetLabels", Request.Profile.ExcludeAssetLabels},
                {"IncludeAssetKinds", Request.Profile.IncludeAssetKinds},
                {"ExcludeAssetKinds", Request.Profile.ExcludeAssetKinds},
                {"DependencyPolicy",
                 Request.Profile.DependencyPolicy == EAssetDependencyPolicy::HardOnly          ? "HardOnly"
                     : Request.Profile.DependencyPolicy == EAssetDependencyPolicy::HardAndSoft ? "HardAndSoft"
                     : Request.Profile.DependencyPolicy == EAssetDependencyPolicy::HardSoftAndEditorPreview
                     ? "HardSoftAndEditorPreview"
                     : "CustomResolver"},
                {"ChunkStrategy",
                 Request.Profile.ChunkStrategy == EAssetChunkStrategy::Monolithic               ? "Monolithic"
                     : Request.Profile.ChunkStrategy == EAssetChunkStrategy::SharedPlusPerLevel ? "SharedPlusPerLevel"
                     : Request.Profile.ChunkStrategy == EAssetChunkStrategy::PerLabel           ? "PerLabel"
                                                                                                : "CustomGraph"},
                {"AllowExplicitOverrideExcludes", Request.Profile.AllowExplicitOverrideExcludes},
                {"Archive",
                 Json::object({
                     {"Enabled", Request.Profile.ArchiveEnabled},
                     {"Format", Request.Profile.ArchiveFormat},
                 })},
            });

            if (IncludeHash && !Request.RequestHash.empty())
            {
                Root["RequestHash"] = Request.RequestHash;
            }

            return Root;
        }

        /**
         * @brief Hash one byte sequence with 64-bit FNV-1a.
         * @param Data Input bytes.
         * @param Size Byte count.
         * @return Deterministic 64-bit hash value.
         */
        [[nodiscard]] std::uint64_t HashBytes64(const void* Data, const std::size_t Size)
        {
            std::uint64_t Hash = kFnv1aOffset;
            const auto* Bytes = static_cast<const std::uint8_t*>(Data);
            for (std::size_t Index = 0; Index < Size; ++Index)
            {
                Hash ^= static_cast<std::uint64_t>(Bytes[Index]);
                Hash *= kFnv1aPrime;
            }
            return Hash;
        }

        /**
         * @brief Convert one 64-bit hash to lowercase hexadecimal text.
         * @param Value Hash value to format.
         * @return Lowercase hexadecimal string.
         */
        [[nodiscard]] std::string ToHexString(const std::uint64_t Value)
        {
            std::ostringstream Stream{};
            Stream << std::hex << std::setfill('0') << std::setw(16) << Value;
            return Stream.str();
        }

        /**
         * @brief Parse one canonical build configuration text token.
         * @param Text Canonical text token.
         * @return Parsed configuration or a structured parse error.
         */
        [[nodiscard]] TExpected<EBuildConfiguration> ParseConfiguration(const std::string_view Text)
        {
            if (Text == "Debug")
            {
                return EBuildConfiguration::Debug;
            }
            if (Text == "Development")
            {
                return EBuildConfiguration::Development;
            }
            if (Text == "Test")
            {
                return EBuildConfiguration::Test;
            }
            if (Text == "Shipping")
            {
                return EBuildConfiguration::Shipping;
            }

            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Unknown build configuration token: " + std::string(Text)));
        }

        /**
         * @brief Parse one canonical dependency-policy text token.
         * @param Text Canonical text token.
         * @return Parsed dependency policy or a structured parse error.
         */
        [[nodiscard]] TExpected<EAssetDependencyPolicy> ParseDependencyPolicy(const std::string_view Text)
        {
            if (Text == "HardOnly")
            {
                return EAssetDependencyPolicy::HardOnly;
            }
            if (Text == "HardAndSoft")
            {
                return EAssetDependencyPolicy::HardAndSoft;
            }
            if (Text == "HardSoftAndEditorPreview")
            {
                return EAssetDependencyPolicy::HardSoftAndEditorPreview;
            }
            if (Text == "CustomResolver")
            {
                return EAssetDependencyPolicy::CustomResolver;
            }

            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Unknown dependency policy token: " + std::string(Text)));
        }

        /**
         * @brief Parse one canonical chunk-strategy text token.
         * @param Text Canonical text token.
         * @return Parsed chunk strategy or a structured parse error.
         */
        [[nodiscard]] TExpected<EAssetChunkStrategy> ParseChunkStrategy(const std::string_view Text)
        {
            if (Text == "Monolithic")
            {
                return EAssetChunkStrategy::Monolithic;
            }
            if (Text == "SharedPlusPerLevel")
            {
                return EAssetChunkStrategy::SharedPlusPerLevel;
            }
            if (Text == "PerLabel")
            {
                return EAssetChunkStrategy::PerLabel;
            }
            if (Text == "CustomGraph")
            {
                return EAssetChunkStrategy::CustomGraph;
            }

            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, "Unknown chunk strategy token: " + std::string(Text)));
        }

        /**
         * @brief Read one required string field from ordered JSON.
         * @param Object Source JSON object.
         * @param Key Required field name.
         * @return Trimmed string value or a structured parse error.
         */
        [[nodiscard]] TExpected<std::string> ReadRequiredString(const Json& Object, const std::string_view Key)
        {
            const auto It = Object.find(Key);
            if (It == Object.end() || !It->is_string())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Missing or invalid string field '" + std::string(Key) + "'"));
            }
            return TrimCopy(It->get<std::string>());
        }

        /**
         * @brief Read one required boolean field from ordered JSON.
         * @param Object Source JSON object.
         * @param Key Required field name.
         * @return Boolean value or a structured parse error.
         */
        [[nodiscard]] TExpected<bool> ReadRequiredBool(const Json& Object, const std::string_view Key)
        {
            const auto It = Object.find(Key);
            if (It == Object.end() || !It->is_boolean())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Missing or invalid boolean field '" + std::string(Key) + "'"));
            }
            return It->get<bool>();
        }

        /**
         * @brief Read one required string array field from ordered JSON.
         * @param Object Source JSON object.
         * @param Key Required field name.
         * @return Ordered string array or a structured parse error.
         */
        [[nodiscard]] TExpected<std::vector<std::string>> ReadRequiredStringArray(const Json& Object,
                                                                                  const std::string_view Key)
        {
            const auto It = Object.find(Key);
            if (It == Object.end() || !It->is_array())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Missing or invalid array field '" + std::string(Key) + "'"));
            }

            std::vector<std::string> Values{};
            for (const Json& Value : *It)
            {
                if (!Value.is_string())
                {
                    return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                     "Array field '" + std::string(Key) + "' must contain strings"));
                }
                Values.push_back(TrimCopy(Value.get<std::string>()));
            }
            return Values;
        }

    } // namespace

    TExpected<ResolvedBuildRequest> BuildRequestService::Resolve(const BuildRequest& Request,
                                                                 const std::size_t MaxInheritanceDepth)
    {
        if (Request.ProjectFilePath.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project descriptor path cannot be empty"));
        }

        auto ProjectResult = ProjectDescriptorService::LoadResolved(Request.ProjectFilePath.string());
        if (!ProjectResult)
        {
            return std::unexpected(ProjectResult.error());
        }

        BuildRequestOverrides Overrides = Request.Overrides;
        CanonicalizeOverrides(Overrides);

        ResolvedBuildProfile ResolvedProfile{};
        const std::string ProfileName = TrimCopy(Request.ProfileName);
        if (!ProfileName.empty())
        {
            auto ProfileResult = BuildProfileService::ResolveProfile(ProjectResult->Descriptor.Profiles, ProfileName,
                                                                     MaxInheritanceDepth);
            if (!ProfileResult)
            {
                return std::unexpected(ProfileResult.error());
            }
            ResolvedProfile = std::move(*ProfileResult);
        }
        else
        {
            ResolvedProfile.Name.clear();
            ResolvedProfile.Inherits.clear();
        }

        ApplyValue(ResolvedProfile.Platform, Overrides.Platform, std::string{});
        ApplyValue(ResolvedProfile.ExecutionEnvironment, Overrides.ExecutionEnvironment, std::string{});
        ApplyValue(ResolvedProfile.Configuration, Overrides.Configuration, EBuildConfiguration::Development);
        ApplyStringList(ResolvedProfile.SelectedLevels, Overrides.SelectedLevels);
        ApplyStringList(ResolvedProfile.ExplicitAssets, Overrides.ExplicitAssets);
        ApplyStringList(ResolvedProfile.IncludeFolders, Overrides.IncludeFolders);
        ApplyStringList(ResolvedProfile.ExcludeFolders, Overrides.ExcludeFolders);
        ApplyStringList(ResolvedProfile.IncludeAssetLabels, Overrides.IncludeAssetLabels);
        ApplyStringList(ResolvedProfile.ExcludeAssetLabels, Overrides.ExcludeAssetLabels);
        ApplyStringList(ResolvedProfile.IncludeAssetKinds, Overrides.IncludeAssetKinds);
        ApplyStringList(ResolvedProfile.ExcludeAssetKinds, Overrides.ExcludeAssetKinds);
        ApplyValue(ResolvedProfile.DependencyPolicy, Overrides.DependencyPolicy, EAssetDependencyPolicy::HardAndSoft);
        ApplyValue(ResolvedProfile.ChunkStrategy, Overrides.ChunkStrategy, EAssetChunkStrategy::Monolithic);
        ApplyValue(ResolvedProfile.AllowExplicitOverrideExcludes, Overrides.AllowExplicitOverrideExcludes, false);
        ApplyArchive(ResolvedProfile, Overrides.Archive);

        ResolvedBuildRequest Resolved{};
        Resolved.Project = std::move(*ProjectResult);
        Resolved.ProfileName = ProfileName;
        Resolved.Profile = std::move(ResolvedProfile);
        Resolved.ValidationIssues = Validate(Resolved);

        if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(Resolved.ValidationIssues);
            BlockingIssue != nullptr)
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
        }

        const Json FrozenJson = BuildFrozenRequestJson(Resolved, false);
        const std::string FrozenText = FrozenJson.dump();
        Resolved.RequestHash = ToHexString(HashBytes64(FrozenText.data(), FrozenText.size()));

        return Resolved;
    }

    std::vector<BuildValidationIssue> BuildRequestService::Validate(const ResolvedBuildRequest& Request)
    {
        std::vector<BuildValidationIssue> Issues{};

        for (const ProjectDescriptorValidationIssue& ProjectIssue : ProjectDescriptorService::Validate(Request.Project))
        {
            Issues.push_back(ConvertProjectIssue(ProjectIssue));
        }

        if (Request.Project.ProjectFilePath.empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleMissingProjectFilePath,
                        "Build requests require a resolved project file path.");
        }
        if (TrimCopy(Request.Profile.Platform).empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleMissingPlatform,
                        "Build requests require a resolved target platform.");
        }
        if (TrimCopy(Request.Profile.ExecutionEnvironment).empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Warning, kRuleMissingExecutionEnvironment,
                        "Build request does not specify an execution environment; host-local toolchain resolution will "
                        "be used.");
        }
        if (Request.Profile.ArchiveEnabled && TrimCopy(Request.Profile.ArchiveFormat).empty())
        {
            AppendIssue(Issues, EBuildValidationSeverity::Error, kRuleMissingArchiveFormat,
                        "Archive format must be set when archive output is enabled.");
        }

        return Issues;
    }

    TExpected<std::string> BuildRequestService::SerializeResolved(const ResolvedBuildRequest& Request, const int Indent)
    {
        try
        {
            const Json Root = BuildFrozenRequestJson(Request, true);
            return Root.dump(Indent) + "\n";
        }
        catch (const std::exception& Ex)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
        }
    }

    TExpected<ResolvedBuildRequest> BuildRequestService::DeserializeResolved(const std::string_view Text)
    {
        try
        {
            Json Root = Json::parse(Text, nullptr, false);
            if (Root.is_discarded() || !Root.is_object())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Failed to parse BuildRequest JSON"));
            }

            const auto ProjectIt = Root.find("Project");
            const auto ProfileIt = Root.find("ResolvedProfile");
            if (ProjectIt == Root.end() || !ProjectIt->is_object() || ProfileIt == Root.end() || !ProfileIt->is_object())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "BuildRequest JSON is missing required project/profile data"));
            }

            auto ProjectRootText = ReadRequiredString(*ProjectIt, "ProjectRoot");
            if (!ProjectRootText)
            {
                return std::unexpected(ProjectRootText.error());
            }
            if (TrimCopy(*ProjectRootText).empty())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, std::string(kRuleProjectRootMissing) +
                                                                     ": BuildRequest JSON is missing ProjectRoot."));
            }

            auto ProjectFileField = ReadRequiredString(*ProjectIt, "ProjectFile");
            if (!ProjectFileField)
            {
                return std::unexpected(ProjectFileField.error());
            }
            const std::filesystem::path ProjectRoot = std::filesystem::path(*ProjectRootText).lexically_normal();
            const std::filesystem::path ProjectFile =
                (ProjectRoot / std::filesystem::path(*ProjectFileField)).lexically_normal();

            auto Project = ProjectDescriptorService::LoadResolved(ProjectFile.string());
            if (!Project)
            {
                return std::unexpected(Project.error());
            }

            ResolvedBuildRequest Request{};
            Request.Project = std::move(*Project);
            Request.ProfileName = Root.value("ProfileName", std::string{});
            Request.RequestHash = Root.value("RequestHash", std::string{});
            Request.Profile.Name = ProfileIt->value("Name", std::string{});
            Request.Profile.Inherits = ProfileIt->value("Inherits", std::string{});
            auto Platform = ReadRequiredString(*ProfileIt, "Platform");
            auto ExecutionEnvironment = ReadRequiredString(*ProfileIt, "ExecutionEnvironment");
            auto ConfigurationText = ReadRequiredString(*ProfileIt, "Configuration");
            if (!Platform || !ExecutionEnvironment || !ConfigurationText)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "BuildRequest JSON is missing required profile fields"));
            }
            Request.Profile.Platform = *Platform;
            Request.Profile.ExecutionEnvironment = *ExecutionEnvironment;
            auto Configuration = ParseConfiguration(*ConfigurationText);
            if (!Configuration)
            {
                return std::unexpected(Configuration.error());
            }
            Request.Profile.Configuration = *Configuration;
            auto SelectedLevels = ReadRequiredStringArray(*ProfileIt, "SelectedLevels");
            auto ExplicitAssets = ReadRequiredStringArray(*ProfileIt, "ExplicitAssets");
            auto IncludeFolders = ReadRequiredStringArray(*ProfileIt, "IncludeFolders");
            auto ExcludeFolders = ReadRequiredStringArray(*ProfileIt, "ExcludeFolders");
            auto IncludeLabels = ReadRequiredStringArray(*ProfileIt, "IncludeAssetLabels");
            auto ExcludeLabels = ReadRequiredStringArray(*ProfileIt, "ExcludeAssetLabels");
            auto IncludeKinds = ReadRequiredStringArray(*ProfileIt, "IncludeAssetKinds");
            auto ExcludeKinds = ReadRequiredStringArray(*ProfileIt, "ExcludeAssetKinds");
            if (!SelectedLevels || !ExplicitAssets || !IncludeFolders || !ExcludeFolders || !IncludeLabels ||
                !ExcludeLabels || !IncludeKinds || !ExcludeKinds)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "BuildRequest JSON contains one or more invalid string-array fields"));
            }
            Request.Profile.SelectedLevels = std::move(*SelectedLevels);
            Request.Profile.ExplicitAssets = std::move(*ExplicitAssets);
            Request.Profile.IncludeFolders = std::move(*IncludeFolders);
            Request.Profile.ExcludeFolders = std::move(*ExcludeFolders);
            Request.Profile.IncludeAssetLabels = std::move(*IncludeLabels);
            Request.Profile.ExcludeAssetLabels = std::move(*ExcludeLabels);
            Request.Profile.IncludeAssetKinds = std::move(*IncludeKinds);
            Request.Profile.ExcludeAssetKinds = std::move(*ExcludeKinds);

            auto DependencyPolicyText = ReadRequiredString(*ProfileIt, "DependencyPolicy");
            if (!DependencyPolicyText)
            {
                return std::unexpected(DependencyPolicyText.error());
            }
            auto DependencyPolicy = ParseDependencyPolicy(*DependencyPolicyText);
            if (!DependencyPolicy)
            {
                return std::unexpected(DependencyPolicy.error());
            }
            Request.Profile.DependencyPolicy = *DependencyPolicy;

            auto ChunkStrategyText = ReadRequiredString(*ProfileIt, "ChunkStrategy");
            if (!ChunkStrategyText)
            {
                return std::unexpected(ChunkStrategyText.error());
            }
            auto ChunkStrategy = ParseChunkStrategy(*ChunkStrategyText);
            if (!ChunkStrategy)
            {
                return std::unexpected(ChunkStrategy.error());
            }
            Request.Profile.ChunkStrategy = *ChunkStrategy;

            auto AllowExplicitOverrideExcludes = ReadRequiredBool(*ProfileIt, "AllowExplicitOverrideExcludes");
            if (!AllowExplicitOverrideExcludes)
            {
                return std::unexpected(AllowExplicitOverrideExcludes.error());
            }
            Request.Profile.AllowExplicitOverrideExcludes = *AllowExplicitOverrideExcludes;

            const auto ArchiveIt = ProfileIt->find("Archive");
            if (ArchiveIt == ProfileIt->end() || !ArchiveIt->is_object())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "BuildRequest JSON is missing the Archive object"));
            }
            auto ArchiveEnabled = ReadRequiredBool(*ArchiveIt, "Enabled");
            auto ArchiveFormat = ReadRequiredString(*ArchiveIt, "Format");
            if (!ArchiveEnabled || !ArchiveFormat)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "BuildRequest JSON contains an invalid Archive object"));
            }
            Request.Profile.ArchiveEnabled = *ArchiveEnabled;
            Request.Profile.ArchiveFormat = *ArchiveFormat;

            Request.ValidationIssues = Validate(Request);
            if (const BuildValidationIssue* BlockingIssue = FindBlockingIssue(Request.ValidationIssues);
                BlockingIssue != nullptr)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, BlockingIssue->RuleId + ": " + BlockingIssue->Message));
            }

            if (TrimCopy(Request.RequestHash).empty())
            {
                const Json FrozenJson = BuildFrozenRequestJson(Request, false);
                const std::string FrozenText = FrozenJson.dump();
                Request.RequestHash = ToHexString(HashBytes64(FrozenText.data(), FrozenText.size()));
            }

            return Request;
        }
        catch (const std::exception& Ex)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, Ex.what()));
        }
    }

    TExpected<ResolvedBuildRequest> BuildRequestService::LoadResolved(const std::filesystem::path& FilePath)
    {
        std::ifstream Stream(FilePath, std::ios::binary);
        if (!Stream.is_open())
        {
            return std::unexpected(
                MakeError(EErrorCode::NotFound, "Failed to open BuildRequest file: " + FilePath.string()));
        }

        std::ostringstream Text{};
        Text << Stream.rdbuf();
        if (!Stream.good() && !Stream.eof())
        {
            return std::unexpected(
                MakeError(EErrorCode::InternalError, "Failed to read BuildRequest file: " + FilePath.string()));
        }
        return DeserializeResolved(Text.str());
    }

} // namespace SnAPI::GameFramework
