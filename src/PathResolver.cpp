#include "PathResolver.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] bool IsAsciiAlpha(const unsigned char Character)
{
    return (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z');
}

[[nodiscard]] bool IsAsciiDigit(const unsigned char Character)
{
    return Character >= '0' && Character <= '9';
}

[[nodiscard]] bool IsSchemaCharacter(const unsigned char Character)
{
    return IsAsciiAlpha(Character) || IsAsciiDigit(Character) || Character == '+' || Character == '-' || Character == '.';
}

[[nodiscard]] std::filesystem::path ResolveAppDataRootPath()
{
#if defined(_WIN32)
    if (const char* LocalAppData = std::getenv("LOCALAPPDATA"))
    {
        return std::filesystem::path(LocalAppData);
    }
    if (const char* RoamingAppData = std::getenv("APPDATA"))
    {
        return std::filesystem::path(RoamingAppData);
    }
    return {};
#elif defined(__APPLE__)
    if (const char* Home = std::getenv("HOME"))
    {
        return std::filesystem::path(Home) / "Library" / "Application Support";
    }
    return {};
#else
    if (const char* XdgDataHome = std::getenv("XDG_DATA_HOME"))
    {
        return std::filesystem::path(XdgDataHome);
    }
    if (const char* Home = std::getenv("HOME"))
    {
        return std::filesystem::path(Home) / ".local" / "share";
    }
    return {};
#endif
}

[[nodiscard]] std::filesystem::path ResolveEditorAssetAppDataDirectory()
{
    const std::filesystem::path AppDataRoot = ResolveAppDataRootPath();
    if (AppDataRoot.empty())
    {
        return {};
    }
    return AppDataRoot / "SnAPI" / "GameFramework" / "Editor" / "Assets";
}

[[nodiscard]] std::filesystem::path ResolveCompiledDefaultAssetRoot()
{
#if defined(SNAPI_GF_DEFAULT_ASSET_ROOT)
    constexpr const char* CompiledRootPath = SNAPI_GF_DEFAULT_ASSET_ROOT;
    if (CompiledRootPath[0] != '\0')
    {
        return std::filesystem::path(CompiledRootPath);
    }
#endif
    return {};
}
} // namespace

SPathResolver& SPathResolver::Instance()
{
    static SPathResolver Resolver{};
    return Resolver;
}

TExpected<std::filesystem::path> SPathResolver::Resolve(const std::string_view Value) const
{
    if (Value.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Path resolver input is empty"));
    }

    if (const std::optional<ParsedSchema> Parsed = ParseSchema(Value); Parsed.has_value())
    {
        SchemaHandler Handler{};
        {
            std::scoped_lock Lock(m_mutex);
            const auto It = m_handlers.find(Parsed->Scheme);
            if (It == m_handlers.end())
            {
                return std::unexpected(MakeError(EErrorCode::NotFound,
                                                 "No path resolver schema handler is registered for '" + Parsed->Scheme + "'"));
            }
            Handler = It->second;
        }

        auto Resolved = Handler(Parsed->Remainder);
        if (!Resolved)
        {
            return std::unexpected(Resolved.error());
        }

        if (Resolved->empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                             "Schema handler resolved an empty filesystem path"));
        }

        return NormalizeForFilesystem(*Resolved);
    }

    return NormalizeForFilesystem(std::filesystem::path(Value));
}

TExpected<std::string> SPathResolver::ResolveToString(const std::string_view Value) const
{
    auto ResolvedPath = Resolve(Value);
    if (!ResolvedPath)
    {
        return std::unexpected(ResolvedPath.error());
    }
    return ResolvedPath->string();
}

Result SPathResolver::RegisterSchemaHandler(const std::string_view Scheme, SchemaHandler Handler)
{
    const std::string LowerScheme = ToLowerAscii(Scheme);
    if (!IsValidSchemaName(LowerScheme))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Schema name is invalid"));
    }

    if (!Handler)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Schema handler is invalid"));
    }

    std::scoped_lock Lock(m_mutex);
    m_handlers[LowerScheme] = std::move(Handler);
    return Ok();
}

bool SPathResolver::UnregisterSchemaHandler(const std::string_view Scheme)
{
    const std::string LowerScheme = ToLowerAscii(Scheme);
    if (!IsValidSchemaName(LowerScheme))
    {
        return false;
    }

    std::scoped_lock Lock(m_mutex);
    return m_handlers.erase(LowerScheme) > 0;
}

Result SPathResolver::SetAssetRoot(std::filesystem::path RootPath)
{
    if (RootPath.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Asset root path is empty"));
    }

    RootPath = NormalizeForFilesystem(std::move(RootPath));

    std::error_code Error{};
    if (!std::filesystem::exists(RootPath, Error) || Error)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Asset root path does not exist"));
    }

    Error.clear();
    if (!std::filesystem::is_directory(RootPath, Error) || Error)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Asset root path is not a directory"));
    }

    std::scoped_lock Lock(m_mutex);
    m_assetRoot = std::move(RootPath);
    return Ok();
}

std::filesystem::path SPathResolver::AssetRoot() const
{
    std::scoped_lock Lock(m_mutex);
    return m_assetRoot;
}

Result SPathResolver::SetEditorRoot(std::filesystem::path RootPath)
{
    if (RootPath.empty())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Editor root path is empty"));
    }

    RootPath = NormalizeForFilesystem(std::move(RootPath));

    std::error_code Error{};
    if (!std::filesystem::exists(RootPath, Error) || Error)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Editor root path does not exist"));
    }

    Error.clear();
    if (!std::filesystem::is_directory(RootPath, Error) || Error)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Editor root path is not a directory"));
    }

    std::scoped_lock Lock(m_mutex);
    m_editorRoot = std::move(RootPath);
    return Ok();
}

std::filesystem::path SPathResolver::EditorRoot() const
{
    std::scoped_lock Lock(m_mutex);
    return m_editorRoot;
}

SPathResolver::SPathResolver()
    : m_assetRoot(ResolveDefaultAssetRoot())
    , m_editorRoot(ResolveDefaultEditorRoot())
{
    m_handlers.emplace("asset", [this](const std::string_view Remainder) {
        return ResolveAssetPath(Remainder);
    });
    m_handlers.emplace("editor", [this](const std::string_view Remainder) {
        return ResolveEditorPath(Remainder);
    });
}

std::optional<SPathResolver::ParsedSchema> SPathResolver::ParseSchema(const std::string_view Value)
{
    const std::size_t Delimiter = Value.find("://");
    if (Delimiter == std::string_view::npos)
    {
        return std::nullopt;
    }

    const std::string_view Scheme = Value.substr(0, Delimiter);
    if (!IsValidSchemaName(Scheme))
    {
        return std::nullopt;
    }

    ParsedSchema Parsed{};
    Parsed.Scheme = ToLowerAscii(Scheme);
    Parsed.Remainder = Value.substr(Delimiter + 3u);
    return Parsed;
}

bool SPathResolver::IsValidSchemaName(const std::string_view Name)
{
    if (Name.empty())
    {
        return false;
    }

    const unsigned char First = static_cast<unsigned char>(Name.front());
    if (!IsAsciiAlpha(First))
    {
        return false;
    }

    for (const char Character : Name)
    {
        if (!IsSchemaCharacter(static_cast<unsigned char>(Character)))
        {
            return false;
        }
    }

    return true;
}

std::string SPathResolver::ToLowerAscii(const std::string_view Value)
{
    std::string Output(Value);
    std::transform(Output.begin(), Output.end(), Output.begin(), [](const unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });
    return Output;
}

std::filesystem::path SPathResolver::NormalizeForFilesystem(std::filesystem::path Value)
{
    std::error_code Error{};
    if (!Value.is_absolute())
    {
        std::filesystem::path Absolute = std::filesystem::absolute(Value, Error);
        if (!Error)
        {
            Value = std::move(Absolute);
        }
        Error.clear();
    }

    std::filesystem::path Canonical = std::filesystem::weakly_canonical(Value, Error);
    if (!Error)
    {
        Value = std::move(Canonical);
    }

    return Value.lexically_normal();
}

std::filesystem::path SPathResolver::ResolveDefaultAssetRoot()
{
    namespace fs = std::filesystem;

    if (const char* EnvPath = std::getenv("SNAPI_GF_ASSET_ROOT"))
    {
        if (EnvPath[0] != '\0')
        {
            return NormalizeForFilesystem(fs::path(EnvPath));
        }
    }

    if (const char* EnvPath = std::getenv("SNAPI_ASSET_ROOT"))
    {
        if (EnvPath[0] != '\0')
        {
            return NormalizeForFilesystem(fs::path(EnvPath));
        }
    }

    const fs::path CompiledAssetRoot = ResolveCompiledDefaultAssetRoot();
    if (!CompiledAssetRoot.empty())
    {
        return NormalizeForFilesystem(CompiledAssetRoot);
    }

    const fs::path EditorAppDataAssetRoot = ResolveEditorAssetAppDataDirectory();
    if (!EditorAppDataAssetRoot.empty())
    {
        std::error_code Error{};
        if (fs::exists(EditorAppDataAssetRoot, Error) && !Error)
        {
            Error.clear();
            if (fs::is_directory(EditorAppDataAssetRoot, Error) && !Error)
            {
                return NormalizeForFilesystem(EditorAppDataAssetRoot);
            }
        }
    }

    std::error_code Error{};
    const fs::path CurrentPath = fs::current_path(Error);
    if (!Error && !CurrentPath.empty())
    {
        const std::array<fs::path, 4> Candidates{
            CurrentPath / "Assets",
            CurrentPath / "Content",
            CurrentPath / "Editor" / "Assets",
            CurrentPath / "src" / "Editor" / "Assets"};

        for (const fs::path& Candidate : Candidates)
        {
            Error.clear();
            if (fs::exists(Candidate, Error) && !Error)
            {
                Error.clear();
                if (fs::is_directory(Candidate, Error) && !Error)
                {
                    return NormalizeForFilesystem(Candidate);
                }
            }
        }

        return NormalizeForFilesystem(CurrentPath / "Assets");
    }

    return {};
}

std::filesystem::path SPathResolver::ResolveDefaultEditorRoot()
{
    namespace fs = std::filesystem;

    if (const char* EnvPath = std::getenv("SNAPI_GF_EDITOR_ROOT"))
    {
        if (EnvPath[0] != '\0')
        {
            return NormalizeForFilesystem(fs::path(EnvPath));
        }
    }

    if (const char* EnvPath = std::getenv("SNAPI_EDITOR_ROOT"))
    {
        if (EnvPath[0] != '\0')
        {
            return NormalizeForFilesystem(fs::path(EnvPath));
        }
    }

    const fs::path EditorAssetPath = ResolveEditorAssetAppDataDirectory();
    if (!EditorAssetPath.empty())
    {
        const fs::path EditorRoot = EditorAssetPath.parent_path();
        if (!EditorRoot.empty())
        {
            return NormalizeForFilesystem(EditorRoot);
        }
    }

    std::error_code Error{};
    const fs::path CurrentPath = fs::current_path(Error);
    if (!Error && !CurrentPath.empty())
    {
        return NormalizeForFilesystem(CurrentPath / "Editor");
    }

    return {};
}

TExpected<std::filesystem::path> SPathResolver::ResolveAssetPath(const std::string_view Remainder) const
{
    std::filesystem::path RootPath{};
    {
        std::scoped_lock Lock(m_mutex);
        RootPath = m_assetRoot;
    }

    if (RootPath.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "asset:// root is not configured"));
    }

    std::string RelativeText(Remainder);
    std::replace(RelativeText.begin(), RelativeText.end(), '\\', '/');
    while (!RelativeText.empty() && RelativeText.front() == '/')
    {
        RelativeText.erase(RelativeText.begin());
    }

    std::filesystem::path Relative(RelativeText);
    if (Relative.is_absolute())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "asset:// path cannot be absolute"));
    }

    std::filesystem::path Candidate = NormalizeForFilesystem(RootPath / Relative);
    const std::filesystem::path CanonicalRoot = NormalizeForFilesystem(RootPath);
    if (!IsPathWithin(CanonicalRoot, Candidate))
    {
        return std::unexpected(MakeError(EErrorCode::OutOfRange,
                                         "asset:// path escapes configured asset root"));
    }

    return Candidate;
}

TExpected<std::filesystem::path> SPathResolver::ResolveEditorPath(const std::string_view Remainder) const
{
    std::filesystem::path RootPath{};
    {
        std::scoped_lock Lock(m_mutex);
        RootPath = m_editorRoot;
    }

    if (RootPath.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "editor:// root is not configured"));
    }

    std::string RelativeText(Remainder);
    std::replace(RelativeText.begin(), RelativeText.end(), '\\', '/');
    while (!RelativeText.empty() && RelativeText.front() == '/')
    {
        RelativeText.erase(RelativeText.begin());
    }

    std::filesystem::path Relative(RelativeText);
    if (Relative.is_absolute())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "editor:// path cannot be absolute"));
    }

    std::filesystem::path Candidate = NormalizeForFilesystem(RootPath / Relative);
    const std::filesystem::path CanonicalRoot = NormalizeForFilesystem(RootPath);
    if (!IsPathWithin(CanonicalRoot, Candidate))
    {
        return std::unexpected(MakeError(EErrorCode::OutOfRange,
                                         "editor:// path escapes configured editor root"));
    }

    return Candidate;
}

bool SPathResolver::IsPathWithin(const std::filesystem::path& Root, const std::filesystem::path& Candidate)
{
    const auto RootBegin = Root.begin();
    const auto RootEnd = Root.end();
    const auto CandidateBegin = Candidate.begin();
    const auto CandidateEnd = Candidate.end();

    auto RootIt = RootBegin;
    auto CandidateIt = CandidateBegin;
    for (; RootIt != RootEnd; ++RootIt, ++CandidateIt)
    {
        if (CandidateIt == CandidateEnd)
        {
            return false;
        }
        if (*RootIt != *CandidateIt)
        {
            return false;
        }
    }

    return true;
}

} // namespace SnAPI::GameFramework
