#include "PackageOutput.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <ranges>
#include <sstream>
#include <system_error>

namespace SnAPI::GameFramework
{
    namespace
    {

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
         * @brief Convert one build configuration enum into canonical text.
         * @param Configuration Build configuration to stringify.
         * @return Canonical configuration name.
         */
        [[nodiscard]] std::string ToString(const EBuildConfiguration Configuration)
        {
            switch (Configuration)
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
         * @brief Normalize one filesystem path for stable comparisons and storage.
         * @param Path Filesystem path to normalize.
         * @return Generic-string normalized path.
         */
        [[nodiscard]] std::string NormalizePathString(const std::filesystem::path& Path)
        {
            return Path.lexically_normal().generic_string();
        }

        /**
         * @brief Create one directory tree when it does not already exist.
         * @param Directory Directory path to create.
         * @return Success or a structured filesystem error.
         */
        [[nodiscard]] Result EnsureDirectory(const std::filesystem::path& Directory)
        {
            if (Directory.empty())
            {
                return Ok();
            }

            std::error_code Error{};
            std::filesystem::create_directories(Directory, Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError,
                              "Failed to create directory '" + Directory.string() + "': " + Error.message()));
            }

            return Ok();
        }

        /**
         * @brief Sanitize one output-name token for filesystem use.
         * @param Token Raw authored token.
         * @return Sanitized token.
         */
        [[nodiscard]] std::string SanitizeToken(const std::string_view Token)
        {
            const std::string Trimmed = TrimCopy(Token);
            std::string Result{};
            Result.reserve(Trimmed.size());

            bool PreviousWasSeparator = false;
            for (const unsigned char Character : Trimmed)
            {
                if (std::isalnum(Character) != 0)
                {
                    Result.push_back(static_cast<char>(Character));
                    PreviousWasSeparator = false;
                    continue;
                }

                if (Character == '_' || Character == '-')
                {
                    Result.push_back(static_cast<char>(Character));
                    PreviousWasSeparator = false;
                    continue;
                }

                if (!PreviousWasSeparator)
                {
                    Result.push_back('_');
                    PreviousWasSeparator = true;
                }
            }

            while (!Result.empty() && Result.back() == '_')
            {
                Result.pop_back();
            }

            return Result.empty() ? "Unnamed" : Result;
        }

        /**
         * @brief Build the default copied package-directory name for one build.
         * @param Request Frozen request that owns the package.
         * @param Graph Planned graph that owns the build id.
         * @return Default package-directory leaf name.
         */
        [[nodiscard]] std::string MakeDefaultPackageDirectoryName(const ResolvedBuildRequest& Request,
                                                                  const BuildGraph& Graph)
        {
            const std::string ProfileName = Request.ProfileName.empty() ? std::string("AdHoc") : Request.ProfileName;
            return SanitizeToken(Request.Project.Descriptor.Project.Name) + "_" + SanitizeToken(ProfileName) + "_" +
                   SanitizeToken(Request.Profile.Platform) + "_" +
                   SanitizeToken(ToString(Request.Profile.Configuration)) + "_" + SanitizeToken(Graph.BuildId);
        }

        /**
         * @brief Build the default archive file name for one build.
         * @param Request Frozen request that owns the package.
         * @param ArchiveFormat Resolved archive format.
         * @return Default archive file name.
         */
        [[nodiscard]] std::string MakeDefaultArchiveFileName(const ResolvedBuildRequest& Request,
                                                             const std::string_view ArchiveFormat)
        {
            const std::string Extension = TrimCopy(ArchiveFormat).empty() ? std::string("zip") : TrimCopy(ArchiveFormat);
            return SanitizeToken(Request.Project.Descriptor.Project.Name) + "_" + SanitizeToken(Request.Profile.Platform) +
                   "_" + SanitizeToken(ToString(Request.Profile.Configuration)) + "." + Extension;
        }

        /**
         * @brief Return the effective archive enablement for one package output request.
         * @param Request Frozen request that owns the package.
         * @param Options Caller-supplied output options.
         * @return `true` when an archive should be emitted.
         */
        [[nodiscard]] bool ResolveArchiveEnabled(const ResolvedBuildRequest& Request, const PackageOutputOptions& Options)
        {
            return Options.ArchiveEnabled || Request.Profile.ArchiveEnabled;
        }

        /**
         * @brief Return the effective archive format for one package output request.
         * @param Request Frozen request that owns the package.
         * @param Options Caller-supplied output options.
         * @return Effective archive format, currently defaulting to `zip`.
         */
        [[nodiscard]] std::string ResolveArchiveFormat(const ResolvedBuildRequest& Request,
                                                       const PackageOutputOptions& Options)
        {
            if (!TrimCopy(Options.ArchiveFormat).empty())
            {
                return TrimCopy(Options.ArchiveFormat);
            }
            if (!TrimCopy(Request.Profile.ArchiveFormat).empty())
            {
                return TrimCopy(Request.Profile.ArchiveFormat);
            }
            return "zip";
        }

        /**
         * @brief Copy one directory tree into another directory tree.
         * @param SourceDirectory Directory tree to copy from.
         * @param DestinationDirectory Directory tree to copy into.
         * @return Copied regular file paths or a structured filesystem error.
         */
        [[nodiscard]] TExpected<std::vector<std::filesystem::path>> CopyDirectoryTree(
            const std::filesystem::path& SourceDirectory, const std::filesystem::path& DestinationDirectory)
        {
            std::vector<std::filesystem::path> CopiedFiles{};

            std::error_code Error{};
            if (!std::filesystem::exists(SourceDirectory, Error) || Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::NotFound, "Source stage directory does not exist for package finalization"));
            }

            if (Result DirectoryResult = EnsureDirectory(DestinationDirectory); !DirectoryResult)
            {
                return std::unexpected(DirectoryResult.error());
            }

            for (const auto& Entry : std::filesystem::recursive_directory_iterator(SourceDirectory, Error))
            {
                if (Error)
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError, "Failed to enumerate staged output tree: " + Error.message()));
                }

                const std::filesystem::path RelativePath = std::filesystem::relative(Entry.path(), SourceDirectory, Error);
                if (Error)
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError, "Failed to relativize staged output path: " + Error.message()));
                }

                const std::filesystem::path DestinationPath = DestinationDirectory / RelativePath;
                if (Entry.is_directory())
                {
                    if (Result DirectoryResult = EnsureDirectory(DestinationPath); !DirectoryResult)
                    {
                        return std::unexpected(DirectoryResult.error());
                    }
                    continue;
                }

                if (!Entry.is_regular_file())
                {
                    continue;
                }

                if (Result DirectoryResult = EnsureDirectory(DestinationPath.parent_path()); !DirectoryResult)
                {
                    return std::unexpected(DirectoryResult.error());
                }

                std::filesystem::copy_file(Entry.path(), DestinationPath, std::filesystem::copy_options::overwrite_existing,
                                           Error);
                if (Error)
                {
                    return std::unexpected(
                        MakeError(EErrorCode::InternalError, "Failed to copy package output file: " + Error.message()));
                }

                CopiedFiles.push_back(DestinationPath.lexically_normal());
            }

            std::ranges::sort(CopiedFiles, [](const std::filesystem::path& Left, const std::filesystem::path& Right)
                              { return NormalizePathString(Left) < NormalizePathString(Right); });
            return CopiedFiles;
        }

        /**
         * @brief Quote one shell token for the current host shell.
         * @param Text Raw token text.
         * @return Quoted token.
         */
        [[nodiscard]] std::string QuoteForShell(const std::string_view Text)
        {
#if defined(_WIN32)
            std::string Result = "\"";
            for (const char Character : Text)
            {
                if (Character == '"')
                {
                    Result += "\\\"";
                }
                else
                {
                    Result.push_back(Character);
                }
            }
            Result.push_back('"');
            return Result;
#else
            std::string Result = "'";
            for (const char Character : Text)
            {
                if (Character == '\'')
                {
                    Result += "'\\''";
                }
                else
                {
                    Result.push_back(Character);
                }
            }
            Result.push_back('\'');
            return Result;
#endif
        }

        /**
         * @brief Emit one archive file using `cmake -E tar`.
         * @param OutputRootDirectory Parent directory that contains the copied package directory.
         * @param PackageDirectoryPath Copied package directory to archive.
         * @param ArchiveFilePath Destination archive file path.
         * @param ArchiveFormat Effective archive format.
         * @param CMakeExecutable CMake executable used to invoke `-E tar`.
         * @return Success or a structured tool-execution error.
         */
        [[nodiscard]] Result WriteArchive(const std::filesystem::path& OutputRootDirectory,
                                          const std::filesystem::path& PackageDirectoryPath,
                                          const std::filesystem::path& ArchiveFilePath,
                                          const std::string_view ArchiveFormat,
                                          const std::string_view CMakeExecutable)
        {
            if (TrimCopy(ArchiveFormat) != "zip")
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Only 'zip' archive output is currently supported"));
            }

            if (Result DirectoryResult = EnsureDirectory(ArchiveFilePath.parent_path()); !DirectoryResult)
            {
                return DirectoryResult;
            }

            std::error_code Error{};
            std::filesystem::remove(ArchiveFilePath, Error);
            Error.clear();

            const std::string CommandLine =
                "cd " + QuoteForShell(OutputRootDirectory.string()) + " && " + QuoteForShell(std::string(CMakeExecutable)) +
                " -E tar cf " + QuoteForShell(ArchiveFilePath.filename().string()) + " --format=zip " +
                QuoteForShell(PackageDirectoryPath.filename().string());

            const int ExitCode = std::system(CommandLine.c_str());
            if (ExitCode != 0)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to write package archive via cmake -E tar"));
            }

            if (!std::filesystem::exists(ArchiveFilePath, Error) || Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Archive command completed but the output archive is missing"));
            }

            return Ok();
        }

    } // namespace

    TExpected<PackageOutputResult> PackageOutputService::Finalize(const ResolvedBuildRequest& Request,
                                                                  const BuildGraph& Graph,
                                                                  const PackageOutputOptions& Options)
    {
        PackageOutputResult OutputResult{};
        OutputResult.OutputRootDirectory = Options.OutputRootDirectory.empty()
                                               ? (Request.Project.SavedRootDirectory / "Packages").lexically_normal()
                                               : Options.OutputRootDirectory.lexically_normal();

        const bool ArchiveEnabled = ResolveArchiveEnabled(Request, Options);
        if (!Options.CopyStageToOutput && !ArchiveEnabled)
        {
            return OutputResult;
        }

        if (Result DirectoryResult = EnsureDirectory(OutputResult.OutputRootDirectory); !DirectoryResult)
        {
            return std::unexpected(DirectoryResult.error());
        }

        const std::string PackageDirectoryName = TrimCopy(Options.PackageDirectoryName).empty()
                                                     ? MakeDefaultPackageDirectoryName(Request, Graph)
                                                     : TrimCopy(Options.PackageDirectoryName);
        OutputResult.PackageDirectoryPath = (OutputResult.OutputRootDirectory / PackageDirectoryName).lexically_normal();

        std::error_code Error{};
        std::filesystem::remove_all(OutputResult.PackageDirectoryPath, Error);
        Error.clear();

        auto CopiedFiles = CopyDirectoryTree(Graph.StageDirectory, OutputResult.PackageDirectoryPath);
        if (!CopiedFiles)
        {
            return std::unexpected(CopiedFiles.error());
        }
        OutputResult.CopiedFiles = std::move(*CopiedFiles);

        if (!ArchiveEnabled)
        {
            return OutputResult;
        }

        const std::string ArchiveFormat = ResolveArchiveFormat(Request, Options);
        const std::string ArchiveFileName = TrimCopy(Options.ArchiveFileName).empty()
                                                ? MakeDefaultArchiveFileName(Request, ArchiveFormat)
                                                : TrimCopy(Options.ArchiveFileName);
        OutputResult.ArchiveFilePath = (OutputResult.OutputRootDirectory / ArchiveFileName).lexically_normal();

        if (Result ArchiveResult =
                WriteArchive(OutputResult.OutputRootDirectory, OutputResult.PackageDirectoryPath,
                             OutputResult.ArchiveFilePath,
                             ArchiveFormat, Options.CMakeExecutable);
            !ArchiveResult)
        {
            return std::unexpected(ArchiveResult.error());
        }

        return OutputResult;
    }

} // namespace SnAPI::GameFramework
