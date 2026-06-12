#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/Documentation.h>
#include <clang-c/Index.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
namespace fs = std::filesystem;

enum class DiagnosticSeverity
{
    Warning,
    Error,
};

struct Diagnostic
{
    DiagnosticSeverity Severity = DiagnosticSeverity::Error;
    fs::path File{};
    unsigned Line = 0;
    unsigned Column = 0;
    std::string Message{};
};

struct ParsedComment
{
    std::string Doc{};
    std::unordered_map<std::string, std::string> ParamDocs{};
};

struct AnnotationPayload
{
    std::string Kind{};
    std::unordered_map<std::string, std::string> Values{};
    std::unordered_set<std::string> Flags{};
};

enum class MarkerKind
{
    Type,
    Field,
    Function,
    EnumValue,
    Generated,
};

struct ReflectionMarker
{
    fs::path File{};
    unsigned Line = 0;
    unsigned Column = 0;
    unsigned StartOffset = 0;
    unsigned EndOffset = 0;
    MarkerKind Kind = MarkerKind::Type;
    std::string MacroName{};
    AnnotationPayload Payload{};
};

struct HeaderMarkers
{
    std::string Source{};
    std::vector<ReflectionMarker> Type{};
    std::vector<ReflectionMarker> Field{};
    std::vector<ReflectionMarker> Function{};
    std::vector<ReflectionMarker> EnumValue{};
    std::vector<ReflectionMarker> Generated{};
};

struct HeaderMarkerCursor
{
    std::vector<unsigned char> UsedType{};
    std::vector<unsigned char> UsedField{};
    std::vector<unsigned char> UsedFunction{};
    std::vector<unsigned char> UsedEnumValue{};
    std::vector<unsigned char> UsedGenerated{};
};

struct ParamSpec
{
    std::string Type{};
    std::string Name{};
    std::string Doc{};
};

struct FieldSpec
{
    std::string Name{};
    std::string DisplayName{};
    std::string Category{};
    std::string Doc{};
    std::string FlagsExpr = "{}";
    std::string EditorFlagsExpr = "{}";
    std::string MinExpr{};
    std::string MaxExpr{};
    std::string StepExpr{};
    std::string ConditionExpr{};
    std::string BuilderExpr{};
};

struct MethodSpec
{
    std::string Name{};
    std::string PointerExpr{};
    std::string DisplayName{};
    std::string Category{};
    std::string Doc{};
    std::string FlagsExpr = "{}";
    std::string ConditionExpr{};
    std::vector<ParamSpec> Params{};
};

enum class GeneratedRpcKind
{
    Server,
    Client,
    Multicast,
};

struct GeneratedRpcSpec
{
    GeneratedRpcKind Kind = GeneratedRpcKind::Server;
    unsigned SourceLine = 0;
    std::string PublicName{};
    std::string ImplName{};
    std::string ServerEntryName{};
    std::string ClientEntryName{};
    std::string ServerEntryAccessor{};
    std::string ClientEntryAccessor{};
    std::string ReliabilityFlagsExpr = "{}";
    std::vector<ParamSpec> Params{};
};

struct ConstructorSpec
{
    std::string Doc{};
    std::vector<ParamSpec> Params{};
};

struct BaseSpec
{
    std::string TypeExpr{};
    std::string ConditionExpr{};
};

struct EnumValueSpec
{
    std::string Name{};
    std::string DisplayName{};
    std::string Doc{};
    std::string QualifiedValueExpr{};
};

struct TypeSpec
{
    fs::path Header{};
    fs::path TypeNameHeader{};
    std::string DeclName{};
    std::string QualifiedName{};
    std::string ReflectedName{};
    std::string DisplayName{};
    std::string Category{};
    std::string Doc{};
    bool IsEnum = false;
    bool EnumIsSigned = false;
    bool IsInterface = false;
    bool HasNativeTypeNameMember = false;
    bool HasDefaultConstructor = false;
    bool HasHeaderVisibleTypeName = false;
    bool NeedsGeneratedTypeName = false;
    bool IsNodeLike = false;
    bool IsComponentLike = false;
    unsigned GeneratedLine = 0;
    std::optional<ConstructorSpec> DefaultConstructor{};
    std::vector<BaseSpec> Bases{};
    std::vector<FieldSpec> Fields{};
    std::vector<MethodSpec> Methods{};
    std::vector<GeneratedRpcSpec> GeneratedRpcs{};
    std::vector<EnumValueSpec> EnumValues{};
};

struct TemplateSpecializationCandidate
{
    std::size_t TemplateIndex = 0;
    fs::path File{};
    unsigned Line = 0;
    unsigned Column = 0;
    std::string QualifiedName{};
    std::string ReflectedName{};
    std::vector<std::string> TemplateArgumentKeys{};
    std::vector<std::pair<std::string, std::string>> Substitutions{};
};

struct AnnotatedDeclaration
{
    fs::path Header{};
    CXCursor Cursor{};
    AnnotationPayload Payload{};
    bool IsTemplate = false;
};

struct HeaderParseContext
{
    std::unordered_set<std::string> HeaderKeys{};
    std::vector<TypeSpec> Types{};
    std::vector<Diagnostic>* Diagnostics = nullptr;
    const struct RegistrationKnowledge* Knowledge = nullptr;
    const std::unordered_map<std::string, HeaderMarkers>* Markers = nullptr;
    std::unordered_map<std::string, HeaderMarkerCursor>* MarkerCursors = nullptr;
};

struct ScopedTypeExpression
{
    fs::path File{};
    unsigned Line = 0;
    unsigned Column = 0;
    std::string Namespace{};
    std::string Expression{};
    std::string DeclaredReflectedName{};
};

struct RegistrationKnowledge
{
    std::unordered_set<std::string> RegisteredTypeKeys{};
    std::unordered_set<std::string> HeaderVisibleTypeKeys{};
};

struct RegistrationScanData
{
    std::uint64_t Fingerprint = 0;
    std::vector<ScopedTypeExpression> Expressions{};
    std::vector<ScopedTypeExpression> HeaderExpressions{};
    std::unordered_map<std::string, std::uint64_t> FileSourceHashes{};
};

struct HeaderScanInfo
{
    fs::path Header{};
    std::uint64_t HeaderSetHash = 0;
    std::uint64_t SourceHash = 0;
    bool HasMarkers = false;
    bool HasTypeExpressions = false;
    bool IsTemplatePrimary = false;
};

struct TypeNameCacheEntry
{
    fs::path Header{};
    fs::path TypeNameHeader{};
    std::string QualifiedName{};
    std::string ReflectedName{};
    bool IsEnum = false;
    bool HasHeaderVisibleTypeName = false;
    bool NeedsGeneratedTypeName = false;
};

struct HeaderCacheEntry
{
    std::uint64_t Fingerprint = 0;
    bool HasMarkers = false;
    bool HasTypeExpressions = false;
    bool IsTemplatePrimary = false;
    std::unordered_set<std::string> KnowledgeDependencies{};
    std::vector<TemplateSpecializationCandidate> TemplateCandidates{};
    std::vector<TypeNameCacheEntry> TypeNameEntries{};
};

struct FileDependencySnapshot
{
    fs::path Path{};
    std::uint64_t Size = 0;
    std::int64_t WriteTime = 0;
};

struct RegistrationFileCacheEntry
{
    std::uint64_t SourceHash = 0;
    std::vector<FileDependencySnapshot> Dependencies{};
    std::unordered_set<std::string> ResolvedTypeKeys{};
};

struct ReflectionCache
{
    std::uint64_t SchemaVersion = 0;
    std::uint64_t RegistrationScanFingerprint = 0;
    std::uint64_t RegistrationCompileArgsHash = 0;
    std::uint64_t KnowledgeFingerprint = 0;
    RegistrationKnowledge Knowledge{};
    std::unordered_map<std::string, RegistrationFileCacheEntry> RegistrationFiles{};
    std::unordered_map<std::string, HeaderCacheEntry> Headers{};
};

std::optional<std::string> ParseStringLiteralValue(std::string_view Expression);
bool IsHeaderLikePath(const fs::path& Path);
fs::path NormalizePath(const fs::path& Path);

Diagnostic MakeDiagnostic(const CXCursor Cursor, std::string Message);
Diagnostic MakeDiagnostic(const fs::path& File, unsigned Line, unsigned Column, std::string Message);
Diagnostic MakeWarning(const CXCursor Cursor, std::string Message);
Diagnostic MakeWarning(const fs::path& File, unsigned Line, unsigned Column, std::string Message);
std::string BuildScanTranslationUnitSource(const std::vector<fs::path>& Headers);
std::string BuildRegistrationProbeSource(
    const fs::path& IncludedFile,
    const std::vector<std::pair<std::size_t, ScopedTypeExpression>>& Expressions);

std::string ToStringDispose(CXString Value)
{
    const char* const Text = clang_getCString(Value);
    std::string Result = Text ? Text : "";
    clang_disposeString(Value);
    return Result;
}

std::optional<std::string> LoadFileText(const fs::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    if (!Input.is_open())
    {
        return std::nullopt;
    }

    return std::string((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
}

bool WriteFileTextIfChanged(const fs::path& Path, const std::string& Contents)
{
    if (const auto Existing = LoadFileText(Path); Existing && *Existing == Contents)
    {
        return true;
    }

    std::error_code Error{};
    fs::create_directories(Path.parent_path(), Error);

    std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
    if (!Output.is_open())
    {
        return false;
    }

    Output.write(Contents.data(), static_cast<std::streamsize>(Contents.size()));
    return static_cast<bool>(Output);
}

bool EnsureFileExistsWithContents(const fs::path& Path, const std::string& Contents)
{
    if (fs::exists(Path))
    {
        return true;
    }

    return WriteFileTextIfChanged(Path, Contents);
}

fs::path FilePathFromCXFile(CXFile File)
{
    if (!File)
    {
        return {};
    }

    std::string Path = ToStringDispose(clang_File_tryGetRealPathName(File));
    if (Path.empty())
    {
        Path = ToStringDispose(clang_getFileName(File));
    }
    return NormalizePath(fs::path(Path));
}

fs::path NormalizePath(const fs::path& Path)
{
    if (Path.empty())
    {
        return {};
    }

    std::error_code Error{};
    const fs::path Absolute = fs::absolute(Path, Error);
    if (Error)
    {
        return Path.lexically_normal();
    }
    return Absolute.lexically_normal();
}

bool ShouldTrackRegistrationDependency(const fs::path& Path)
{
    const std::string NormalizedPath = NormalizePath(Path).generic_string();
    if (NormalizedPath.empty())
    {
        return false;
    }

    if (NormalizedPath.starts_with("/usr/") || NormalizedPath.starts_with("/lib/") ||
        NormalizedPath.starts_with("/lib64/"))
    {
        return false;
    }

    return true;
}

std::optional<FileDependencySnapshot> CaptureFileDependencySnapshot(const fs::path& Path)
{
    const fs::path NormalizedPath = NormalizePath(Path);
    if (!ShouldTrackRegistrationDependency(NormalizedPath))
    {
        return std::nullopt;
    }

    std::error_code Error{};
    if (!fs::exists(NormalizedPath, Error) || Error || !fs::is_regular_file(NormalizedPath, Error) || Error)
    {
        return std::nullopt;
    }

    const std::uint64_t Size = fs::file_size(NormalizedPath, Error);
    if (Error)
    {
        return std::nullopt;
    }

    const auto WriteTime = fs::last_write_time(NormalizedPath, Error);
    if (Error)
    {
        return std::nullopt;
    }

    return FileDependencySnapshot{
        .Path = NormalizedPath,
        .Size = Size,
        .WriteTime = static_cast<std::int64_t>(WriteTime.time_since_epoch().count()),
    };
}

bool DependencySnapshotMatches(const FileDependencySnapshot& Snapshot)
{
    const std::optional<FileDependencySnapshot> Current = CaptureFileDependencySnapshot(Snapshot.Path);
    return Current &&
           Current->Size == Snapshot.Size &&
           Current->WriteTime == Snapshot.WriteTime;
}

void AddDependencySnapshot(std::vector<FileDependencySnapshot>& Out,
                           std::unordered_set<std::string>& Seen,
                           const fs::path& Path)
{
    if (const std::optional<FileDependencySnapshot> Snapshot = CaptureFileDependencySnapshot(Path))
    {
        const std::string Key = Snapshot->Path.generic_string();
        if (Seen.insert(Key).second)
        {
            Out.push_back(*Snapshot);
        }
    }
}

constexpr std::uint64_t kReflectionCacheSchemaVersion = 13;
constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

std::uint64_t HashBytes(const char* Data, const std::size_t Size)
{
    std::uint64_t Hash = kFnvOffsetBasis;
    for (std::size_t Index = 0; Index < Size; ++Index)
    {
        Hash ^= static_cast<unsigned char>(Data[Index]);
        Hash *= kFnvPrime;
    }
    return Hash;
}

std::uint64_t HashStringView(const std::string_view Value)
{
    return HashBytes(Value.data(), Value.size());
}

std::uint64_t HashPathString(const fs::path& Path)
{
    return HashStringView(NormalizePath(Path).generic_string());
}

void HashCombine(std::uint64_t& Seed, const std::uint64_t Value)
{
    Seed ^= Value + 0x9e3779b97f4a7c15ull + (Seed << 6u) + (Seed >> 2u);
}

std::uint64_t CombinedHash(const std::initializer_list<std::uint64_t> Values)
{
    std::uint64_t Seed = kFnvOffsetBasis;
    for (const std::uint64_t Value : Values)
    {
        HashCombine(Seed, Value);
    }
    return Seed;
}

std::uint64_t HashStringVector(const std::vector<std::string>& Values)
{
    std::uint64_t Seed = kFnvOffsetBasis;
    for (const std::string& Value : Values)
    {
        HashCombine(Seed, HashStringView(Value));
    }
    return Seed;
}

std::uint64_t HashStringSet(const std::unordered_set<std::string>& Values)
{
    std::vector<std::string> Sorted(Values.begin(), Values.end());
    std::sort(Sorted.begin(), Sorted.end());
    return HashStringVector(Sorted);
}

std::string NormalizePathKey(const fs::path& Path)
{
    return NormalizePath(Path).generic_string();
}

std::string Trim(std::string_view Value)
{
    std::size_t Start = 0;
    while (Start < Value.size() && std::isspace(static_cast<unsigned char>(Value[Start])) != 0)
    {
        ++Start;
    }

    std::size_t End = Value.size();
    while (End > Start && std::isspace(static_cast<unsigned char>(Value[End - 1])) != 0)
    {
        --End;
    }

    return std::string(Value.substr(Start, End - Start));
}

std::string NormalizeParagraphText(std::string_view Value)
{
    std::string Result{};
    Result.reserve(Value.size());

    bool PendingSpace = false;
    for (const char Ch : Value)
    {
        if (std::isspace(static_cast<unsigned char>(Ch)) != 0)
        {
            PendingSpace = !Result.empty();
            continue;
        }

        if (PendingSpace)
        {
            Result.push_back(' ');
            PendingSpace = false;
        }

        Result.push_back(Ch);
    }

    return Trim(Result);
}

std::string NormalizeTypeExpressionString(std::string_view Value)
{
    std::string Result{};
    Result.reserve(Value.size());

    for (const char Ch : Value)
    {
        if (std::isspace(static_cast<unsigned char>(Ch)) == 0)
        {
            Result.push_back(Ch);
        }
    }

    while (Result.starts_with("::"))
    {
        Result.erase(0, 2);
    }

    return Result;
}

std::string StripTopLevelCvRefFromTypeExpression(std::string Value)
{
    Value = Trim(Value);

    bool StrippedPrefix = true;
    while (StrippedPrefix)
    {
        StrippedPrefix = false;
        for (const std::string_view Prefix : {"const ", "volatile "})
        {
            if (Value.starts_with(Prefix))
            {
                Value.erase(0, Prefix.size());
                Value = Trim(Value);
                StrippedPrefix = true;
            }
        }
    }

    while (Value.ends_with("&&"))
    {
        Value.erase(Value.size() - 2);
        Value = Trim(Value);
    }
    while (!Value.empty() && Value.back() == '&')
    {
        Value.pop_back();
        Value = Trim(Value);
    }

    return Value;
}

std::string NormalizeReflectedTypeNameString(std::string_view Value)
{
    std::string Result = NormalizeTypeExpressionString(Value);

    // Preserve the longstanding asset-ref naming contract: `TAssetRef<T>` and
    // `TAssetRef<T, void>` are the same C++ type, but the reflected name has
    // historically omitted the default tag parameter.
    static constexpr std::string_view AssetRefPrefix = "TAssetRef<";
    const std::size_t AssetRefPos = Result.find(AssetRefPrefix);
    if (AssetRefPos != std::string::npos)
    {
        const std::size_t VoidPos = Result.rfind(",void>");
        if (VoidPos != std::string::npos && VoidPos + std::string_view(",void>").size() == Result.size())
        {
            Result.erase(VoidPos, std::string_view(",void").size());
        }
    }

    return Result;
}

std::string StripLeadingTypeKeyword(std::string Value)
{
    Value = Trim(Value);
    bool Changed = true;
    while (Changed)
    {
        Changed = false;
        for (const std::string_view Prefix : {"const ", "volatile ", "class ", "struct ", "enum "})
        {
            if (Value.starts_with(Prefix))
            {
                Value.erase(0, Prefix.size());
                Value = Trim(Value);
                Changed = true;
            }
        }
    }
    return Value;
}

void AddTypeKeyCandidate(std::unordered_set<std::string>& Out, std::string Value)
{
    Value = StripLeadingTypeKeyword(std::move(Value));
    const std::string Key = NormalizeTypeExpressionString(Value);
    if (!Key.empty())
    {
        Out.insert(Key);
    }
}

bool IsIdentifierStart(const char Ch)
{
    const unsigned char Byte = static_cast<unsigned char>(Ch);
    return std::isalpha(Byte) != 0 || Ch == '_';
}

bool IsIdentifierContinue(const char Ch)
{
    const unsigned char Byte = static_cast<unsigned char>(Ch);
    return std::isalnum(Byte) != 0 || Ch == '_';
}

void AppendTemplateParameterNames(const CXCursor Cursor, std::unordered_set<std::string>& Out)
{
    if (clang_Cursor_isNull(Cursor))
    {
        return;
    }

    clang_visitChildren(
        Cursor,
        [](CXCursor Child, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const Names = static_cast<std::unordered_set<std::string>*>(ClientData);
            if (clang_getCursorKind(Child) == CXCursor_TemplateTypeParameter)
            {
                const std::string Name = ToStringDispose(clang_getCursorSpelling(Child));
                if (!Name.empty())
                {
                    Names->insert(Name);
                }
            }
            return CXChildVisit_Continue;
        },
        &Out);
}

std::unordered_set<std::string> EnclosingTemplateParameterNames(CXCursor Cursor)
{
    std::unordered_set<std::string> Result{};
    while (!clang_Cursor_isNull(Cursor))
    {
        AppendTemplateParameterNames(Cursor, Result);
        Cursor = clang_getCursorSemanticParent(Cursor);
    }
    return Result;
}

bool TypeExpressionMentionsTemplateParameter(std::string_view TypeExpr, const std::unordered_set<std::string>& Names)
{
    if (TypeExpr.empty() || Names.empty())
    {
        return false;
    }

    for (std::size_t Pos = 0; Pos < TypeExpr.size();)
    {
        if (!IsIdentifierStart(TypeExpr[Pos]))
        {
            ++Pos;
            continue;
        }

        const std::size_t Start = Pos++;
        while (Pos < TypeExpr.size() && IsIdentifierContinue(TypeExpr[Pos]))
        {
            ++Pos;
        }

        if (Names.contains(std::string(TypeExpr.substr(Start, Pos - Start))))
        {
            return true;
        }
    }

    return false;
}

void AdvanceSourcePosition(std::string_view Contents, std::size_t& Pos, unsigned& Line, unsigned& Column)
{
    if (Pos >= Contents.size())
    {
        return;
    }

    if (Contents[Pos] == '\n')
    {
        ++Line;
        Column = 1;
    }
    else
    {
        ++Column;
    }
    ++Pos;
}

void SkipWhitespaceAndComments(std::string_view Contents, std::size_t& Pos, unsigned& Line, unsigned& Column)
{
    while (Pos < Contents.size())
    {
        if (std::isspace(static_cast<unsigned char>(Contents[Pos])) != 0)
        {
            AdvanceSourcePosition(Contents, Pos, Line, Column);
            continue;
        }

        if (Contents[Pos] == '/' && Pos + 1 < Contents.size())
        {
            if (Contents[Pos + 1] == '/')
            {
                AdvanceSourcePosition(Contents, Pos, Line, Column);
                AdvanceSourcePosition(Contents, Pos, Line, Column);
                while (Pos < Contents.size() && Contents[Pos] != '\n')
                {
                    AdvanceSourcePosition(Contents, Pos, Line, Column);
                }
                continue;
            }

            if (Contents[Pos + 1] == '*')
            {
                AdvanceSourcePosition(Contents, Pos, Line, Column);
                AdvanceSourcePosition(Contents, Pos, Line, Column);
                while (Pos + 1 < Contents.size())
                {
                    if (Contents[Pos] == '*' && Contents[Pos + 1] == '/')
                    {
                        AdvanceSourcePosition(Contents, Pos, Line, Column);
                        AdvanceSourcePosition(Contents, Pos, Line, Column);
                        break;
                    }
                    AdvanceSourcePosition(Contents, Pos, Line, Column);
                }
                continue;
            }
        }

        if (Contents[Pos] == '#')
        {
            while (Pos < Contents.size() && Contents[Pos] != '\n')
            {
                AdvanceSourcePosition(Contents, Pos, Line, Column);
            }
            continue;
        }

        break;
    }
}

unsigned CursorExtentStartOffset(const CXCursor Cursor, unsigned* OutLine = nullptr, unsigned* OutColumn = nullptr)
{
    CXFile File{};
    unsigned Line = 0;
    unsigned Column = 0;
    unsigned Offset = 0;
    clang_getFileLocation(clang_getRangeStart(clang_getCursorExtent(Cursor)), &File, &Line, &Column, &Offset);
    if (OutLine)
    {
        *OutLine = Line;
    }
    if (OutColumn)
    {
        *OutColumn = Column;
    }
    return Offset;
}

unsigned CursorExtentEndOffset(const CXCursor Cursor)
{
    CXFile File{};
    unsigned Line = 0;
    unsigned Column = 0;
    unsigned Offset = 0;
    clang_getFileLocation(clang_getRangeEnd(clang_getCursorExtent(Cursor)), &File, &Line, &Column, &Offset);
    return Offset;
}

bool ContainsOnlyTriviaBetween(std::string_view Contents, const std::size_t Begin, const std::size_t End)
{
    if (Begin > End || End > Contents.size())
    {
        return false;
    }

    std::size_t Pos = Begin;
    unsigned DummyLine = 1;
    unsigned DummyColumn = 1;
    while (Pos < End)
    {
        const std::size_t Before = Pos;
        SkipWhitespaceAndComments(Contents, Pos, DummyLine, DummyColumn);
        if (Pos >= End)
        {
            return true;
        }
        if (Pos == Before)
        {
            return false;
        }
    }

    return true;
}

std::string CurrentNamespace(const std::vector<std::pair<int, std::string>>& NamespaceStack)
{
    return NamespaceStack.empty() ? std::string{} : NamespaceStack.back().second;
}

std::optional<std::string> TryParseNamespaceOpen(std::string_view Contents,
                                                 std::size_t& Pos,
                                                 unsigned& Line,
                                                 unsigned& Column)
{
    std::size_t ProbePos = Pos;
    unsigned ProbeLine = Line;
    unsigned ProbeColumn = Column;

    SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);

    if (Contents.substr(ProbePos, 6) == "inline" &&
        (ProbePos + 6 == Contents.size() || !IsIdentifierContinue(Contents[ProbePos + 6])))
    {
        for (int Index = 0; Index < 6; ++Index)
        {
            AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
        }
        SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);
    }

    std::string NamespaceSuffix{};
    if (ProbePos < Contents.size() && Contents[ProbePos] != '{')
    {
        while (ProbePos < Contents.size())
        {
            SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);
            if (ProbePos >= Contents.size() || !IsIdentifierStart(Contents[ProbePos]))
            {
                return std::nullopt;
            }

            const std::size_t NameStart = ProbePos;
            while (ProbePos < Contents.size() && IsIdentifierContinue(Contents[ProbePos]))
            {
                AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
            }

            if (!NamespaceSuffix.empty())
            {
                NamespaceSuffix += "::";
            }
            NamespaceSuffix += std::string(Contents.substr(NameStart, ProbePos - NameStart));

            SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);
            if (ProbePos + 1 < Contents.size() && Contents[ProbePos] == ':' && Contents[ProbePos + 1] == ':')
            {
                AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
                AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
                continue;
            }
            break;
        }
    }

    SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);
    if (ProbePos >= Contents.size() || Contents[ProbePos] != '{')
    {
        return std::nullopt;
    }

    AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
    Pos = ProbePos;
    Line = ProbeLine;
    Column = ProbeColumn;
    return NamespaceSuffix;
}

std::optional<std::string> ParseTopLevelDelimitedExpression(std::string_view Contents,
                                                            std::size_t& Pos,
                                                            unsigned& Line,
                                                            unsigned& Column,
                                                            const char Delimiter,
                                                            const char Terminator)
{
    const std::size_t Start = Pos;
    int ParenDepth = 0;
    int AngleDepth = 0;
    int BraceDepth = 0;
    int BracketDepth = 0;

    while (Pos < Contents.size())
    {
        if (Contents[Pos] == '"' || Contents[Pos] == '\'')
        {
            const char Quote = Contents[Pos];
            AdvanceSourcePosition(Contents, Pos, Line, Column);
            while (Pos < Contents.size())
            {
                if (Contents[Pos] == '\\')
                {
                    AdvanceSourcePosition(Contents, Pos, Line, Column);
                    if (Pos < Contents.size())
                    {
                        AdvanceSourcePosition(Contents, Pos, Line, Column);
                    }
                    continue;
                }
                const char Current = Contents[Pos];
                AdvanceSourcePosition(Contents, Pos, Line, Column);
                if (Current == Quote)
                {
                    break;
                }
            }
            continue;
        }

        if (Contents[Pos] == '/' && Pos + 1 < Contents.size() && (Contents[Pos + 1] == '/' || Contents[Pos + 1] == '*'))
        {
            SkipWhitespaceAndComments(Contents, Pos, Line, Column);
            continue;
        }

        const char Ch = Contents[Pos];
        if (Ch == '(')
        {
            ++ParenDepth;
        }
        else if (Ch == ')' && ParenDepth > 0)
        {
            --ParenDepth;
        }
        else if (Ch == '<')
        {
            ++AngleDepth;
        }
        else if (Ch == '>' && AngleDepth > 0)
        {
            --AngleDepth;
        }
        else if (Ch == '{')
        {
            ++BraceDepth;
        }
        else if (Ch == '}' && BraceDepth > 0)
        {
            --BraceDepth;
        }
        else if (Ch == '[')
        {
            ++BracketDepth;
        }
        else if (Ch == ']' && BracketDepth > 0)
        {
            --BracketDepth;
        }
        else if (ParenDepth == 0 && AngleDepth == 0 && BraceDepth == 0 && BracketDepth == 0)
        {
            if (Ch == Delimiter || Ch == Terminator)
            {
                return Trim(Contents.substr(Start, Pos - Start));
            }
        }

        AdvanceSourcePosition(Contents, Pos, Line, Column);
    }

    return std::nullopt;
}

std::optional<std::string> TryParseBuiltinRegistrationType(std::string_view Contents,
                                                           std::size_t& Pos,
                                                           unsigned& Line,
                                                           unsigned& Column,
                                                           std::string_view Identifier)
{
    if (Identifier != "RegisterPlain" && Identifier != "RegisterEnum")
    {
        return std::nullopt;
    }

    std::size_t ProbePos = Pos;
    unsigned ProbeLine = Line;
    unsigned ProbeColumn = Column;

    SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);
    const std::string_view Expected = ".operator()";
    if (Contents.substr(ProbePos, Expected.size()) != Expected)
    {
        return std::nullopt;
    }
    for (char Ch : Expected)
    {
        (void)Ch;
        AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
    }

    SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);
    if (ProbePos >= Contents.size() || Contents[ProbePos] != '<')
    {
        return std::nullopt;
    }

    AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
    const auto Parsed = ParseTopLevelDelimitedExpression(Contents, ProbePos, ProbeLine, ProbeColumn, ',', '>');
    if (!Parsed || ProbePos >= Contents.size() || Contents[ProbePos] != '>')
    {
        return std::nullopt;
    }

    AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
    Pos = ProbePos;
    Line = ProbeLine;
    Column = ProbeColumn;
    return Parsed;
}

std::optional<std::string> TryParseUsingAliasType(std::string_view Contents,
                                                  std::size_t& Pos,
                                                  unsigned& Line,
                                                  unsigned& Column,
                                                  std::string_view Identifier)
{
    if (Identifier != "using")
    {
        return std::nullopt;
    }

    std::size_t ProbePos = Pos;
    unsigned ProbeLine = Line;
    unsigned ProbeColumn = Column;
    SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);

    if (ProbePos >= Contents.size() || !IsIdentifierStart(Contents[ProbePos]))
    {
        return std::nullopt;
    }

    while (ProbePos < Contents.size() && IsIdentifierContinue(Contents[ProbePos]))
    {
        AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
    }

    SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);
    if (ProbePos >= Contents.size() || Contents[ProbePos] != '=')
    {
        return std::nullopt;
    }
    AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);

    const auto Parsed = ParseTopLevelDelimitedExpression(Contents, ProbePos, ProbeLine, ProbeColumn, '\0', ';');
    if (!Parsed || ProbePos >= Contents.size() || Contents[ProbePos] != ';')
    {
        return std::nullopt;
    }

    AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
    Pos = ProbePos;
    Line = ProbeLine;
    Column = ProbeColumn;
    return Parsed;
}

std::optional<std::pair<std::string, std::string>> TryParseDefinedTypeName(std::string_view Contents,
                                                                           std::size_t& Pos,
                                                                           unsigned& Line,
                                                                           unsigned& Column,
                                                                           std::string_view Identifier)
{
    if (Identifier != "SNAPI_DEFINE_TYPE_NAME")
    {
        return std::nullopt;
    }

    std::size_t ProbePos = Pos;
    unsigned ProbeLine = Line;
    unsigned ProbeColumn = Column;
    SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);
    if (ProbePos >= Contents.size() || Contents[ProbePos] != '(')
    {
        return std::nullopt;
    }

    AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
    const auto ParsedType = ParseTopLevelDelimitedExpression(Contents, ProbePos, ProbeLine, ProbeColumn, ',', ')');
    if (!ParsedType)
    {
        return std::nullopt;
    }

    if (ProbePos >= Contents.size() || Contents[ProbePos] != ',')
    {
        return std::nullopt;
    }
    AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);

    const auto ParsedNameExpression =
        ParseTopLevelDelimitedExpression(Contents, ProbePos, ProbeLine, ProbeColumn, '\0', ')');
    if (!ParsedNameExpression)
    {
        return std::nullopt;
    }

    if (ProbePos >= Contents.size() || Contents[ProbePos] != ')')
    {
        return std::nullopt;
    }

    AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
    Pos = ProbePos;
    Line = ProbeLine;
    Column = ProbeColumn;

    const auto ParsedName = ParseStringLiteralValue(*ParsedNameExpression);
    if (!ParsedName)
    {
        return std::nullopt;
    }

    return std::pair<std::string, std::string>{*ParsedType, NormalizeReflectedTypeNameString(*ParsedName)};
}

std::vector<ScopedTypeExpression> ScanTypeExpressionsInFile(const fs::path& File,
                                                            std::uint64_t* const OutSourceHash = nullptr)
{
    std::ifstream Stream(File);
    if (!Stream.is_open())
    {
        return {};
    }

    const std::string Contents((std::istreambuf_iterator<char>(Stream)), std::istreambuf_iterator<char>());
    if (OutSourceHash)
    {
        *OutSourceHash = HashStringView(Contents);
    }

    std::vector<ScopedTypeExpression> Result{};
    std::size_t Pos = 0;
    unsigned Line = 1;
    unsigned Column = 1;
    int BraceDepth = 0;
    std::vector<std::pair<int, std::string>> NamespaceStack{};

    while (Pos < Contents.size())
    {
        SkipWhitespaceAndComments(Contents, Pos, Line, Column);
        if (Pos >= Contents.size())
        {
            break;
        }

        if (IsIdentifierStart(Contents[Pos]))
        {
            const unsigned TokenLine = Line;
            const unsigned TokenColumn = Column;
            const std::size_t TokenStart = Pos;
            while (Pos < Contents.size() && IsIdentifierContinue(Contents[Pos]))
            {
                AdvanceSourcePosition(Contents, Pos, Line, Column);
            }
            const std::string Identifier = Contents.substr(TokenStart, Pos - TokenStart);

            if (Identifier == "namespace")
            {
                if (auto NamespaceSuffix = TryParseNamespaceOpen(Contents, Pos, Line, Column))
                {
                    ++BraceDepth;
                    std::string QualifiedNamespace = CurrentNamespace(NamespaceStack);
                    if (!NamespaceSuffix->empty())
                    {
                        if (!QualifiedNamespace.empty())
                        {
                            QualifiedNamespace += "::";
                        }
                        QualifiedNamespace += *NamespaceSuffix;
                    }
                    NamespaceStack.emplace_back(BraceDepth, std::move(QualifiedNamespace));
                    continue;
                }
            }

            auto CaptureTypeExpression = [&](const std::string& Expression) {
                if (Expression.empty())
                {
                    return;
                }
                Result.push_back(ScopedTypeExpression{
                    .File = File,
                    .Line = TokenLine,
                    .Column = TokenColumn,
                    .Namespace = CurrentNamespace(NamespaceStack),
                    .Expression = Expression,
                    .DeclaredReflectedName = {},
                });
            };

            const int NamespaceScopeDepth = NamespaceStack.empty() ? 0 : NamespaceStack.back().first;

            if (Identifier == "SNAPI_REFLECT_TYPE" ||
                Identifier == "SNAPI_REFLECT_COMPONENT" ||
                Identifier == "SNAPI_REFLECT_METADATA")
            {
                std::size_t ProbePos = Pos;
                unsigned ProbeLine = Line;
                unsigned ProbeColumn = Column;
                SkipWhitespaceAndComments(Contents, ProbePos, ProbeLine, ProbeColumn);
                if (ProbePos < Contents.size() && Contents[ProbePos] == '(')
                {
                    AdvanceSourcePosition(Contents, ProbePos, ProbeLine, ProbeColumn);
                    if (const auto Parsed = ParseTopLevelDelimitedExpression(Contents, ProbePos, ProbeLine, ProbeColumn, ',', ')'))
                    {
                        CaptureTypeExpression(*Parsed);
                    }
                }
                continue;
            }

            if (const auto BuiltinType = TryParseBuiltinRegistrationType(Contents, Pos, Line, Column, Identifier))
            {
                CaptureTypeExpression(*BuiltinType);
                continue;
            }

            if (BraceDepth == NamespaceScopeDepth)
            {
                if (const auto AliasType = TryParseUsingAliasType(Contents, Pos, Line, Column, Identifier))
                {
                    CaptureTypeExpression(*AliasType);
                    continue;
                }
            }

            if (const auto DefinedType = TryParseDefinedTypeName(Contents, Pos, Line, Column, Identifier))
            {
                Result.push_back(ScopedTypeExpression{
                    .File = File,
                    .Line = TokenLine,
                    .Column = TokenColumn,
                    .Namespace = CurrentNamespace(NamespaceStack),
                    .Expression = DefinedType->first,
                    .DeclaredReflectedName = DefinedType->second,
                });
                continue;
            }

            continue;
        }

        if (Contents[Pos] == '{')
        {
            ++BraceDepth;
            AdvanceSourcePosition(Contents, Pos, Line, Column);
            continue;
        }

        if (Contents[Pos] == '}')
        {
            AdvanceSourcePosition(Contents, Pos, Line, Column);
            --BraceDepth;
            while (!NamespaceStack.empty() && NamespaceStack.back().first > BraceDepth)
            {
                NamespaceStack.pop_back();
            }
            continue;
        }

        AdvanceSourcePosition(Contents, Pos, Line, Column);
    }

    return Result;
}

std::string_view PayloadKindForMarker(const MarkerKind Kind)
{
    switch (Kind)
    {
    case MarkerKind::Type:
        return "snapi.type";
    case MarkerKind::Field:
        return "snapi.field";
    case MarkerKind::Function:
        return "snapi.function";
    case MarkerKind::EnumValue:
        return "snapi.enum_value";
    case MarkerKind::Generated:
        return "snapi.generated";
    }

    return "snapi.unknown";
}

std::optional<MarkerKind> MarkerKindFromMacroName(const std::string_view Name)
{
    if (Name == "SnType" || Name == "SNAPI_TYPE")
    {
        return MarkerKind::Type;
    }
    if (Name == "SnField" || Name == "SNAPI_FIELD")
    {
        return MarkerKind::Field;
    }
    if (Name == "SnFunction" || Name == "SNAPI_FUNCTION")
    {
        return MarkerKind::Function;
    }
    if (Name == "SnEnumValue" || Name == "SNAPI_ENUM_VALUE")
    {
        return MarkerKind::EnumValue;
    }
    if (Name == "SnGenerated")
    {
        return MarkerKind::Generated;
    }
    return std::nullopt;
}

std::vector<std::string> SplitTopLevelArguments(std::string_view Contents)
{
    std::vector<std::string> Result{};
    std::size_t Start = 0;
    int ParenDepth = 0;
    int AngleDepth = 0;
    int BraceDepth = 0;
    int BracketDepth = 0;

    for (std::size_t Pos = 0; Pos < Contents.size(); ++Pos)
    {
        const char Ch = Contents[Pos];
        if (Ch == '"' || Ch == '\'')
        {
            const char Quote = Ch;
            ++Pos;
            while (Pos < Contents.size())
            {
                if (Contents[Pos] == '\\')
                {
                    ++Pos;
                    if (Pos < Contents.size())
                    {
                        ++Pos;
                    }
                    continue;
                }
                if (Contents[Pos] == Quote)
                {
                    break;
                }
                ++Pos;
            }
            continue;
        }

        if (Ch == '/' && Pos + 1 < Contents.size())
        {
            if (Contents[Pos + 1] == '/')
            {
                Pos += 2;
                while (Pos < Contents.size() && Contents[Pos] != '\n')
                {
                    ++Pos;
                }
                continue;
            }
            if (Contents[Pos + 1] == '*')
            {
                Pos += 2;
                while (Pos + 1 < Contents.size() && !(Contents[Pos] == '*' && Contents[Pos + 1] == '/'))
                {
                    ++Pos;
                }
                if (Pos + 1 < Contents.size())
                {
                    ++Pos;
                }
                continue;
            }
        }

        if (Ch == '(')
        {
            ++ParenDepth;
        }
        else if (Ch == ')' && ParenDepth > 0)
        {
            --ParenDepth;
        }
        else if (Ch == '<')
        {
            ++AngleDepth;
        }
        else if (Ch == '>' && AngleDepth > 0)
        {
            --AngleDepth;
        }
        else if (Ch == '{')
        {
            ++BraceDepth;
        }
        else if (Ch == '}' && BraceDepth > 0)
        {
            --BraceDepth;
        }
        else if (Ch == '[')
        {
            ++BracketDepth;
        }
        else if (Ch == ']' && BracketDepth > 0)
        {
            --BracketDepth;
        }
        else if (Ch == ',' && ParenDepth == 0 && AngleDepth == 0 && BraceDepth == 0 && BracketDepth == 0)
        {
            Result.push_back(Trim(Contents.substr(Start, Pos - Start)));
            Start = Pos + 1;
        }
    }

    Result.push_back(Trim(Contents.substr(Start)));
    while (!Result.empty() && Result.back().empty())
    {
        Result.pop_back();
    }
    return Result;
}

std::optional<std::string> ParseMacroArguments(std::string_view Contents,
                                               std::size_t& Pos,
                                               unsigned& Line,
                                               unsigned& Column)
{
    SkipWhitespaceAndComments(Contents, Pos, Line, Column);
    if (Pos >= Contents.size() || Contents[Pos] != '(')
    {
        return std::nullopt;
    }

    AdvanceSourcePosition(Contents, Pos, Line, Column);
    const auto Parsed = ParseTopLevelDelimitedExpression(Contents, Pos, Line, Column, '\0', ')');
    if (!Parsed || Pos >= Contents.size() || Contents[Pos] != ')')
    {
        return std::nullopt;
    }

    AdvanceSourcePosition(Contents, Pos, Line, Column);
    return *Parsed;
}

std::optional<std::string> ExtractCallPayload(std::string_view Expression, const std::string_view Name)
{
    const std::string Trimmed = Trim(Expression);
    const std::string_view Value = Trimmed;
    if (!Value.starts_with(Name))
    {
        return std::nullopt;
    }

    std::size_t Pos = Name.size();
    while (Pos < Value.size() && std::isspace(static_cast<unsigned char>(Value[Pos])) != 0)
    {
        ++Pos;
    }

    if (Pos >= Value.size() || Value[Pos] != '(')
    {
        return std::nullopt;
    }

    std::size_t InnerStart = Pos + 1;
    int Depth = 1;
    for (++Pos; Pos < Value.size(); ++Pos)
    {
        const char Ch = Value[Pos];
        if (Ch == '"' || Ch == '\'')
        {
            const char Quote = Ch;
            ++Pos;
            while (Pos < Value.size())
            {
                if (Value[Pos] == '\\')
                {
                    ++Pos;
                    if (Pos < Value.size())
                    {
                        ++Pos;
                    }
                    continue;
                }
                if (Value[Pos] == Quote)
                {
                    break;
                }
                ++Pos;
            }
            continue;
        }

        if (Ch == '(')
        {
            ++Depth;
        }
        else if (Ch == ')')
        {
            --Depth;
            if (Depth == 0)
            {
                const std::string Payload = Trim(Value.substr(InnerStart, Pos - InnerStart));
                ++Pos;
                while (Pos < Value.size() && std::isspace(static_cast<unsigned char>(Value[Pos])) != 0)
                {
                    ++Pos;
                }
                if (Pos == Value.size())
                {
                    return Payload;
                }
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
}

void SkipInlineWhitespace(std::string_view Text, std::size_t& Pos)
{
    while (Pos < Text.size() && std::isspace(static_cast<unsigned char>(Text[Pos])) != 0)
    {
        ++Pos;
    }
}

bool ConsumeStringLiteral(std::string_view Text, std::size_t& Pos, std::string& Out)
{
    SkipInlineWhitespace(Text, Pos);
    if (Pos >= Text.size())
    {
        return false;
    }

    if (Text.substr(Pos, 2) == "u8")
    {
        Pos += 2;
    }
    else if (Text[Pos] == 'u' || Text[Pos] == 'U' || Text[Pos] == 'L')
    {
        ++Pos;
    }

    if (Pos >= Text.size() || Text[Pos] != '"')
    {
        return false;
    }

    ++Pos;
    while (Pos < Text.size())
    {
        const char Ch = Text[Pos++];
        if (Ch == '\\')
        {
            if (Pos >= Text.size())
            {
                return false;
            }

            const char Escaped = Text[Pos++];
            switch (Escaped)
            {
            case '\\':
                Out.push_back('\\');
                break;
            case '"':
                Out.push_back('"');
                break;
            case 'n':
                Out.push_back('\n');
                break;
            case 'r':
                Out.push_back('\r');
                break;
            case 't':
                Out.push_back('\t');
                break;
            case '0':
                Out.push_back('\0');
                break;
            default:
                Out.push_back(Escaped);
                break;
            }
            continue;
        }

        if (Ch == '"')
        {
            return true;
        }

        Out.push_back(Ch);
    }

    return false;
}

std::optional<std::string> ParseStringLiteralValue(std::string_view Expression)
{
    const std::string Trimmed = Trim(Expression);
    const std::string_view Value = Trimmed;
    if (Value.empty())
    {
        return std::nullopt;
    }

    std::size_t Pos = 0;
    std::string Result{};
    if (!ConsumeStringLiteral(Value, Pos, Result))
    {
        return std::nullopt;
    }

    SkipInlineWhitespace(Value, Pos);
    while (Pos < Value.size())
    {
        if (!ConsumeStringLiteral(Value, Pos, Result))
        {
            return std::nullopt;
        }
        SkipInlineWhitespace(Value, Pos);
    }

    return Result;
}

bool IsIdentifierToken(std::string_view Value)
{
    if (Value.empty() || !IsIdentifierStart(Value.front()))
    {
        return false;
    }

    return std::all_of(Value.begin() + 1, Value.end(), [](const char Ch) {
        return IsIdentifierContinue(Ch);
    });
}

std::optional<std::string> ParseIdentifierValue(std::string_view Expression)
{
    const std::string Value = Trim(Expression);
    if (!IsIdentifierToken(Value))
    {
        return std::nullopt;
    }
    return Value;
}

bool IsNumericLiteral(std::string_view Expression)
{
    const std::string Text = Trim(Expression);
    if (Text.empty())
    {
        return false;
    }

    std::size_t Consumed = 0;
    try
    {
        (void)std::stod(Text, &Consumed);
    }
    catch (const std::exception&)
    {
        return false;
    }

    return Consumed == Text.size();
}

std::optional<AnnotationPayload> ParseMarkerPayload(const MarkerKind Kind,
                                                    const std::string& MacroName,
                                                    std::string_view Arguments,
                                                    const fs::path& File,
                                                    const unsigned Line,
                                                    const unsigned Column,
                                                    std::vector<Diagnostic>& Diagnostics)
{
    AnnotationPayload Payload{};
    Payload.Kind = std::string(PayloadKindForMarker(Kind));

    if (Kind == MarkerKind::Generated)
    {
        if (!Trim(Arguments).empty())
        {
            Diagnostics.push_back(MakeDiagnostic(
                File, Line, Column, "SnGenerated() does not accept metadata arguments"));
            return std::nullopt;
        }
        return Payload;
    }

    bool Ok = true;
    auto SetValue = [&](const std::string& Key, std::string Value) {
        if (Payload.Values.contains(Key))
        {
            Diagnostics.push_back(MakeDiagnostic(
                File, Line, Column, "Duplicate metadata key '" + Key + "' on " + MacroName));
            Ok = false;
            return;
        }
        Payload.Values.emplace(Key, std::move(Value));
    };
    auto AddFlag = [&](const std::string& Flag) {
        Payload.Flags.insert(Flag);
    };

    for (const std::string& Fragment : SplitTopLevelArguments(Arguments))
    {
        if (Fragment.empty())
        {
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SnKey"))
        {
            if (const auto Parsed = ParseStringLiteralValue(*Value))
            {
                SetValue("key", *Parsed);
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(File, Line, Column, "SnKey(...) requires a string literal"));
                Ok = false;
            }
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SnDisplayName"))
        {
            if (const auto Parsed = ParseStringLiteralValue(*Value))
            {
                SetValue("display_name", *Parsed);
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(File, Line, Column, "SnDisplayName(...) requires a string literal"));
                Ok = false;
            }
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SnName"))
        {
            if (const auto Parsed = ParseStringLiteralValue(*Value))
            {
                SetValue("display_name", *Parsed);
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(File, Line, Column, "SnName(...) requires a string literal"));
                Ok = false;
            }
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SNAPI_DISPLAY_NAME"))
        {
            if (const auto Parsed = ParseStringLiteralValue(*Value))
            {
                SetValue("display_name", *Parsed);
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(File, Line, Column, "SNAPI_DISPLAY_NAME(...) requires a string literal"));
                Ok = false;
            }
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SnCategory"))
        {
            if (const auto Parsed = ParseStringLiteralValue(*Value))
            {
                SetValue("category", *Parsed);
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(File, Line, Column, "SnCategory(...) requires a string literal"));
                Ok = false;
            }
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SNAPI_CATEGORY"))
        {
            if (const auto Parsed = ParseStringLiteralValue(*Value))
            {
                SetValue("category", *Parsed);
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(File, Line, Column, "SNAPI_CATEGORY(...) requires a string literal"));
                Ok = false;
            }
            continue;
        }

        if (Fragment == "SnInterface" || Fragment == "SNAPI_INTERFACE")
        {
            AddFlag("interface");
            continue;
        }

        if (Fragment == "SnTemplate")
        {
            AddFlag("template");
            continue;
        }

        if (Fragment == "SnReplicated" || Fragment == "SNAPI_REPLICATED")
        {
            AddFlag("replicated");
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SnGetter"))
        {
            if (const auto Parsed = ParseIdentifierValue(*Value))
            {
                SetValue("getter", *Parsed);
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(File, Line, Column, "SnGetter(...) requires an identifier"));
                Ok = false;
            }
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SnConstGetter"))
        {
            if (const auto Parsed = ParseIdentifierValue(*Value))
            {
                SetValue("const_getter", *Parsed);
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(File, Line, Column, "SnConstGetter(...) requires an identifier"));
                Ok = false;
            }
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SnSetter"))
        {
            if (const auto Parsed = ParseIdentifierValue(*Value))
            {
                SetValue("setter", *Parsed);
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(File, Line, Column, "SnSetter(...) requires an identifier"));
                Ok = false;
            }
            continue;
        }

        if (Fragment == "SnSerialized" || Fragment == "SNAPI_SERIALIZED")
        {
            AddFlag("serialized");
            continue;
        }

        if (Fragment == "SnHidden" || Fragment == "SNAPI_HIDDEN")
        {
            AddFlag("hidden");
            continue;
        }

        if (Fragment == "SnReadOnly" || Fragment == "SNAPI_READ_ONLY")
        {
            AddFlag("read_only");
            continue;
        }

        if (Fragment == "SnAdvanced" || Fragment == "SNAPI_ADVANCED")
        {
            AddFlag("advanced");
            continue;
        }

        if (Fragment == "SnHeavyData" || Fragment == "SNAPI_HEAVY_DATA")
        {
            AddFlag("heavy_data");
            continue;
        }

        if (Fragment == "SnEditorAction" || Fragment == "SNAPI_EDITOR_ACTION")
        {
            AddFlag("editor_action");
            continue;
        }

        if (Fragment == "SNAPI_RPC_RELIABLE")
        {
            SetValue("rpc", "reliable");
            continue;
        }

        if (Fragment == "SNAPI_RPC_UNRELIABLE")
        {
            SetValue("rpc", "unreliable");
            continue;
        }

        if (Fragment == "SNAPI_NET_SERVER")
        {
            SetValue("net", "server");
            continue;
        }

        if (Fragment == "SNAPI_NET_CLIENT")
        {
            SetValue("net", "client");
            continue;
        }

        if (Fragment == "SNAPI_NET_MULTICAST")
        {
            SetValue("net", "multicast");
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SnRep"))
        {
            AddFlag("replicated");
            for (const std::string& Token : SplitTopLevelArguments(*Value))
            {
                if (Token == "SnReliable")
                {
                    SetValue("rep", "reliable");
                }
                else if (Token == "SnUnreliable")
                {
                    SetValue("rep", "unreliable");
                }
                else if (!Token.empty())
                {
                    Diagnostics.push_back(MakeDiagnostic(
                        File, Line, Column, "Unsupported SnRep(...) token '" + Token + "'"));
                    Ok = false;
                }
            }
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SnRpc"))
        {
            for (const std::string& Token : SplitTopLevelArguments(*Value))
            {
                if (Token == "SnReliable")
                {
                    SetValue("rpc", "reliable");
                }
                else if (Token == "SnUnreliable")
                {
                    SetValue("rpc", "unreliable");
                }
                else if (Token == "SnServer")
                {
                    SetValue("net", "server");
                }
                else if (Token == "SnClient")
                {
                    SetValue("net", "client");
                }
                else if (Token == "SnMulticast")
                {
                    SetValue("net", "multicast");
                }
                else if (!Token.empty())
                {
                    Diagnostics.push_back(MakeDiagnostic(
                        File, Line, Column, "Unsupported SnRpc(...) token '" + Token + "'"));
                    Ok = false;
                }
            }
            continue;
        }

        if (const auto Value = ExtractCallPayload(Fragment, "SnValue"))
        {
            for (const std::string& Token : SplitTopLevelArguments(*Value))
            {
                if (const auto MinValue = ExtractCallPayload(Token, "SnMin"))
                {
                    if (!IsNumericLiteral(*MinValue))
                    {
                        Diagnostics.push_back(MakeDiagnostic(
                            File, Line, Column, "SnMin(...) requires a numeric literal"));
                        Ok = false;
                    }
                    else
                    {
                        SetValue("min", Trim(*MinValue));
                    }
                }
                else if (const auto MaxValue = ExtractCallPayload(Token, "SnMax"))
                {
                    if (!IsNumericLiteral(*MaxValue))
                    {
                        Diagnostics.push_back(MakeDiagnostic(
                            File, Line, Column, "SnMax(...) requires a numeric literal"));
                        Ok = false;
                    }
                    else
                    {
                        SetValue("max", Trim(*MaxValue));
                    }
                }
                else if (const auto StepValue = ExtractCallPayload(Token, "SnStep"))
                {
                    if (!IsNumericLiteral(*StepValue))
                    {
                        Diagnostics.push_back(MakeDiagnostic(
                            File, Line, Column, "SnStep(...) requires a numeric literal"));
                        Ok = false;
                    }
                    else
                    {
                        SetValue("step", Trim(*StepValue));
                    }
                }
                else if (!Token.empty())
                {
                    Diagnostics.push_back(MakeDiagnostic(
                        File, Line, Column, "Unsupported SnValue(...) token '" + Token + "'"));
                    Ok = false;
                }
            }
            continue;
        }

        Diagnostics.push_back(MakeDiagnostic(
            File, Line, Column, "Unsupported metadata fragment '" + Fragment + "' on " + MacroName));
        Ok = false;
    }

    return Ok ? std::optional<AnnotationPayload>(std::move(Payload)) : std::nullopt;
}

std::unordered_map<std::string, HeaderMarkers> ScanReflectionMarkers(const std::vector<fs::path>& Headers,
                                                                     std::vector<Diagnostic>& Diagnostics)
{
    std::unordered_map<std::string, HeaderMarkers> Result{};
    for (const fs::path& Header : Headers)
    {
        std::ifstream Stream(Header);
        if (!Stream.is_open())
        {
            continue;
        }

        const std::string Contents((std::istreambuf_iterator<char>(Stream)), std::istreambuf_iterator<char>());
        std::size_t Pos = 0;
        unsigned Line = 1;
        unsigned Column = 1;
        HeaderMarkers FileMarkers{};
        FileMarkers.Source = Contents;

        while (Pos < Contents.size())
        {
            SkipWhitespaceAndComments(Contents, Pos, Line, Column);
            if (Pos >= Contents.size())
            {
                break;
            }

            if (!IsIdentifierStart(Contents[Pos]))
            {
                AdvanceSourcePosition(Contents, Pos, Line, Column);
                continue;
            }

            const unsigned TokenLine = Line;
            const unsigned TokenColumn = Column;
            const std::size_t TokenStart = Pos;
            while (Pos < Contents.size() && IsIdentifierContinue(Contents[Pos]))
            {
                AdvanceSourcePosition(Contents, Pos, Line, Column);
            }

            const std::string Identifier = Contents.substr(TokenStart, Pos - TokenStart);
            const auto Kind = MarkerKindFromMacroName(Identifier);
            if (!Kind)
            {
                continue;
            }

            const auto Arguments = ParseMacroArguments(Contents, Pos, Line, Column);
            if (!Arguments)
            {
                Diagnostics.push_back(MakeDiagnostic(
                    Header,
                    TokenLine,
                    TokenColumn,
                    "Malformed reflection marker '" + Identifier + "'"));
                continue;
            }

            const auto Payload = ParseMarkerPayload(*Kind, Identifier, *Arguments, Header, TokenLine, TokenColumn, Diagnostics);
            if (!Payload)
            {
                continue;
            }

            ReflectionMarker Marker{
                .File = Header,
                .Line = TokenLine,
                .Column = TokenColumn,
                .StartOffset = static_cast<unsigned>(TokenStart),
                .EndOffset = static_cast<unsigned>(Pos),
                .Kind = *Kind,
                .MacroName = Identifier,
                .Payload = *Payload,
            };

            switch (*Kind)
            {
            case MarkerKind::Type:
                FileMarkers.Type.push_back(std::move(Marker));
                break;
            case MarkerKind::Field:
                FileMarkers.Field.push_back(std::move(Marker));
                break;
            case MarkerKind::Function:
                FileMarkers.Function.push_back(std::move(Marker));
                break;
            case MarkerKind::EnumValue:
                FileMarkers.EnumValue.push_back(std::move(Marker));
                break;
            case MarkerKind::Generated:
                FileMarkers.Generated.push_back(std::move(Marker));
                break;
            }
        }

        Result.emplace(Header.generic_string(), std::move(FileMarkers));
    }

    return Result;
}

bool HeaderMarkersEmpty(const HeaderMarkers& Markers)
{
    return Markers.Type.empty() &&
           Markers.Field.empty() &&
           Markers.Function.empty() &&
           Markers.EnumValue.empty() &&
           Markers.Generated.empty();
}

std::unordered_map<std::string, HeaderScanInfo> BuildHeaderScanInfo(
    const std::vector<fs::path>& Headers,
    const std::unordered_map<std::string, HeaderMarkers>& Markers)
{
    std::vector<std::string> HeaderKeys{};
    HeaderKeys.reserve(Headers.size());
    for (const fs::path& Header : Headers)
    {
        HeaderKeys.push_back(NormalizePath(Header).generic_string());
    }
    std::sort(HeaderKeys.begin(), HeaderKeys.end());
    const std::uint64_t HeaderSetHash = HashStringVector(HeaderKeys);

    std::unordered_map<std::string, HeaderScanInfo> Result{};
    for (const fs::path& Header : Headers)
    {
        const fs::path NormalizedHeader = NormalizePath(Header);
        HeaderScanInfo Info{};
        Info.Header = NormalizedHeader;
        Info.HeaderSetHash = HeaderSetHash;

        if (const auto MarkersIt = Markers.find(NormalizedHeader.generic_string()); MarkersIt != Markers.end())
        {
            Info.SourceHash = HashStringView(MarkersIt->second.Source);
            Info.HasMarkers = !HeaderMarkersEmpty(MarkersIt->second);
            Info.IsTemplatePrimary = std::any_of(
                MarkersIt->second.Type.begin(),
                MarkersIt->second.Type.end(),
                [](const ReflectionMarker& Marker) { return Marker.Payload.Flags.contains("template"); });
        }
        else if (const auto Contents = LoadFileText(NormalizedHeader))
        {
            Info.SourceHash = HashStringView(*Contents);
        }

        Info.HasTypeExpressions = !ScanTypeExpressionsInFile(NormalizedHeader).empty();
        Result.emplace(NormalizedHeader.generic_string(), std::move(Info));
    }

    return Result;
}

std::uint64_t ComputeKnowledgeFingerprint(const RegistrationKnowledge& Knowledge)
{
    return CombinedHash(
        {kReflectionCacheSchemaVersion,
         HashStringSet(Knowledge.RegisteredTypeKeys),
         HashStringSet(Knowledge.HeaderVisibleTypeKeys)});
}

std::unordered_set<std::string> ComputeChangedKnowledgeKeys(const RegistrationKnowledge& OldKnowledge,
                                                            const RegistrationKnowledge& NewKnowledge)
{
    std::unordered_set<std::string> Result{};

    auto AppendChanged = [&Result](const std::unordered_set<std::string>& Left,
                                   const std::unordered_set<std::string>& Right) {
        for (const std::string& Key : Left)
        {
            if (!Right.contains(Key))
            {
                Result.insert(Key);
            }
        }
    };

    AppendChanged(OldKnowledge.RegisteredTypeKeys, NewKnowledge.RegisteredTypeKeys);
    AppendChanged(NewKnowledge.RegisteredTypeKeys, OldKnowledge.RegisteredTypeKeys);
    AppendChanged(OldKnowledge.HeaderVisibleTypeKeys, NewKnowledge.HeaderVisibleTypeKeys);
    AppendChanged(NewKnowledge.HeaderVisibleTypeKeys, OldKnowledge.HeaderVisibleTypeKeys);

    return Result;
}

bool HeaderDependsOnChangedKnowledge(const HeaderCacheEntry& Entry,
                                     const std::unordered_set<std::string>& ChangedKnowledgeKeys)
{
    if (ChangedKnowledgeKeys.empty())
    {
        return false;
    }

    return std::any_of(
        Entry.KnowledgeDependencies.begin(),
        Entry.KnowledgeDependencies.end(),
        [&ChangedKnowledgeKeys](const std::string& Key) { return ChangedKnowledgeKeys.contains(Key); });
}

std::uint64_t ComputeVersionedHeaderFingerprint(const std::uint64_t SchemaVersion,
                                                const HeaderScanInfo& HeaderInfo,
                                                const std::uint64_t CompileArgsHash)
{
    return CombinedHash(
        {SchemaVersion,
         CompileArgsHash,
         HeaderInfo.HeaderSetHash,
         HeaderInfo.SourceHash,
         HeaderInfo.HasMarkers ? 1ull : 0ull,
         HeaderInfo.HasTypeExpressions ? 1ull : 0ull,
         HeaderInfo.IsTemplatePrimary ? 1ull : 0ull});
}

std::uint64_t ComputeVersionedHeaderFingerprintWithKnowledge(const std::uint64_t SchemaVersion,
                                                             const HeaderScanInfo& HeaderInfo,
                                                             const std::uint64_t CompileArgsHash,
                                                             const std::uint64_t KnowledgeFingerprint)
{
    return CombinedHash(
        {SchemaVersion,
         CompileArgsHash,
         KnowledgeFingerprint,
         HeaderInfo.HeaderSetHash,
         HeaderInfo.SourceHash,
         HeaderInfo.HasMarkers ? 1ull : 0ull,
         HeaderInfo.HasTypeExpressions ? 1ull : 0ull,
         HeaderInfo.IsTemplatePrimary ? 1ull : 0ull});
}

std::uint64_t ComputeHeaderFingerprint(const HeaderScanInfo& HeaderInfo, const std::uint64_t CompileArgsHash)
{
    return ComputeVersionedHeaderFingerprint(kReflectionCacheSchemaVersion, HeaderInfo, CompileArgsHash);
}

std::vector<TypeNameCacheEntry> BuildTypeNameCacheEntries(const std::vector<TypeSpec>& Types)
{
    std::vector<TypeNameCacheEntry> Result{};
    for (const TypeSpec& Type : Types)
    {
        if (Type.QualifiedName.empty())
        {
            continue;
        }

        Result.push_back(TypeNameCacheEntry{
            .Header = NormalizePath(Type.Header),
            .TypeNameHeader = NormalizePath(Type.TypeNameHeader),
            .QualifiedName = Type.QualifiedName,
            .ReflectedName = Type.ReflectedName.empty()
                                 ? NormalizeTypeExpressionString(Type.QualifiedName)
                                 : Type.ReflectedName,
            .IsEnum = Type.IsEnum,
            .HasHeaderVisibleTypeName = Type.HasHeaderVisibleTypeName,
            .NeedsGeneratedTypeName =
                Type.NeedsGeneratedTypeName || (Type.IsEnum && !Type.HasHeaderVisibleTypeName),
        });
    }

    return Result;
}

bool SaveReflectionCache(const fs::path& Path, const ReflectionCache& Cache)
{
    std::error_code Error{};
    fs::create_directories(Path.parent_path(), Error);

    std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
    if (!Output.is_open())
    {
        return false;
    }

    Output << "schema " << Cache.SchemaVersion << '\n';
    Output << "registration " << Cache.RegistrationScanFingerprint << '\n';
    Output << "registration_args " << Cache.RegistrationCompileArgsHash << '\n';
    Output << "knowledge " << Cache.KnowledgeFingerprint << '\n';
    std::vector<std::string> RegisteredTypeKeys(
        Cache.Knowledge.RegisteredTypeKeys.begin(), Cache.Knowledge.RegisteredTypeKeys.end());
    std::sort(RegisteredTypeKeys.begin(), RegisteredTypeKeys.end());
    Output << "registered " << RegisteredTypeKeys.size() << '\n';
    for (const std::string& Key : RegisteredTypeKeys)
    {
        Output << "key " << std::quoted(Key) << '\n';
    }

    std::vector<std::string> HeaderVisibleTypeKeys(
        Cache.Knowledge.HeaderVisibleTypeKeys.begin(), Cache.Knowledge.HeaderVisibleTypeKeys.end());
    std::sort(HeaderVisibleTypeKeys.begin(), HeaderVisibleTypeKeys.end());
    Output << "visible " << HeaderVisibleTypeKeys.size() << '\n';
    for (const std::string& Key : HeaderVisibleTypeKeys)
    {
        Output << "key " << std::quoted(Key) << '\n';
    }

    std::vector<std::string> RegistrationFileKeys{};
    RegistrationFileKeys.reserve(Cache.RegistrationFiles.size());
    for (const auto& [FileKey, Entry] : Cache.RegistrationFiles)
    {
        (void)Entry;
        RegistrationFileKeys.push_back(FileKey);
    }
    std::sort(RegistrationFileKeys.begin(), RegistrationFileKeys.end());
    Output << "registration_files " << RegistrationFileKeys.size() << '\n';
    for (const std::string& FileKey : RegistrationFileKeys)
    {
        const RegistrationFileCacheEntry& Entry = Cache.RegistrationFiles.at(FileKey);
        Output << "regfile " << std::quoted(FileKey)
               << ' ' << Entry.SourceHash
               << ' ' << Entry.Dependencies.size()
               << ' ' << Entry.ResolvedTypeKeys.size()
               << '\n';

        for (const FileDependencySnapshot& Dependency : Entry.Dependencies)
        {
            Output << "dep "
                   << std::quoted(Dependency.Path.generic_string())
                   << ' ' << Dependency.Size
                   << ' ' << Dependency.WriteTime
                   << '\n';
        }

        std::vector<std::string> ResolvedTypeKeys(
            Entry.ResolvedTypeKeys.begin(), Entry.ResolvedTypeKeys.end());
        std::sort(ResolvedTypeKeys.begin(), ResolvedTypeKeys.end());
        for (const std::string& Key : ResolvedTypeKeys)
        {
            Output << "rkey " << std::quoted(Key) << '\n';
        }

        Output << "endregfile\n";
    }

    std::vector<std::string> HeaderKeys{};
    HeaderKeys.reserve(Cache.Headers.size());
    for (const auto& [HeaderKey, Entry] : Cache.Headers)
    {
        (void)Entry;
        HeaderKeys.push_back(HeaderKey);
    }
    std::sort(HeaderKeys.begin(), HeaderKeys.end());

    for (const std::string& HeaderKey : HeaderKeys)
    {
        const HeaderCacheEntry& Entry = Cache.Headers.at(HeaderKey);
        Output << "header " << std::quoted(HeaderKey)
               << ' ' << Entry.Fingerprint
               << ' ' << (Entry.HasMarkers ? 1 : 0)
               << ' ' << (Entry.HasTypeExpressions ? 1 : 0)
               << ' ' << (Entry.IsTemplatePrimary ? 1 : 0)
               << ' ' << Entry.TypeNameEntries.size()
               << ' ' << Entry.KnowledgeDependencies.size()
               << ' ' << Entry.TemplateCandidates.size()
               << '\n';
        for (const TypeNameCacheEntry& TypeEntry : Entry.TypeNameEntries)
        {
            Output << "type "
                   << std::quoted(TypeEntry.Header.generic_string())
                   << ' ' << std::quoted(TypeEntry.TypeNameHeader.generic_string())
                   << ' ' << std::quoted(TypeEntry.QualifiedName)
                   << ' ' << std::quoted(TypeEntry.ReflectedName)
                   << ' ' << (TypeEntry.IsEnum ? 1 : 0)
                   << ' ' << (TypeEntry.HasHeaderVisibleTypeName ? 1 : 0)
                   << ' ' << (TypeEntry.NeedsGeneratedTypeName ? 1 : 0)
                   << '\n';
        }
        std::vector<std::string> KnowledgeDependencies(
            Entry.KnowledgeDependencies.begin(), Entry.KnowledgeDependencies.end());
        std::sort(KnowledgeDependencies.begin(), KnowledgeDependencies.end());
        for (const std::string& Key : KnowledgeDependencies)
        {
            Output << "depkey " << std::quoted(Key) << '\n';
        }
        std::vector<TemplateSpecializationCandidate> TemplateCandidates = Entry.TemplateCandidates;
        std::sort(TemplateCandidates.begin(),
                  TemplateCandidates.end(),
                  [](const TemplateSpecializationCandidate& Left, const TemplateSpecializationCandidate& Right) {
                      if (Left.TemplateIndex != Right.TemplateIndex)
                      {
                          return Left.TemplateIndex < Right.TemplateIndex;
                      }
                      if (Left.QualifiedName != Right.QualifiedName)
                      {
                          return Left.QualifiedName < Right.QualifiedName;
                      }
                      if (Left.ReflectedName != Right.ReflectedName)
                      {
                          return Left.ReflectedName < Right.ReflectedName;
                      }
                      return Left.File.generic_string() < Right.File.generic_string();
                  });
        for (const TemplateSpecializationCandidate& Candidate : TemplateCandidates)
        {
            Output << "candidate "
                   << Candidate.TemplateIndex
                   << ' ' << std::quoted(Candidate.File.generic_string())
                   << ' ' << Candidate.Line
                   << ' ' << Candidate.Column
                   << ' ' << std::quoted(Candidate.QualifiedName)
                   << ' ' << std::quoted(Candidate.ReflectedName)
                   << ' ' << Candidate.TemplateArgumentKeys.size()
                   << ' ' << Candidate.Substitutions.size()
                   << '\n';
            for (const std::string& Key : Candidate.TemplateArgumentKeys)
            {
                Output << "arg " << std::quoted(Key) << '\n';
            }
            for (const auto& [Name, Value] : Candidate.Substitutions)
            {
                Output << "subst " << std::quoted(Name) << ' ' << std::quoted(Value) << '\n';
            }
            Output << "endcandidate\n";
        }
        Output << "endheader\n";
    }

    return static_cast<bool>(Output);
}

std::optional<ReflectionCache> LoadReflectionCache(const fs::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    if (!Input.is_open())
    {
        return std::nullopt;
    }

    ReflectionCache Cache{};
    std::string Token{};
    if (!(Input >> Token) || Token != "schema")
    {
        return std::nullopt;
    }
    if (!(Input >> Cache.SchemaVersion))
    {
        return std::nullopt;
    }
    if (Cache.SchemaVersion != 4 &&
        Cache.SchemaVersion != 5 &&
        Cache.SchemaVersion != 6 &&
        Cache.SchemaVersion != 9 &&
        Cache.SchemaVersion != kReflectionCacheSchemaVersion)
    {
        return std::nullopt;
    }
    if (!(Input >> Token) || Token != "registration")
    {
        return std::nullopt;
    }
    if (!(Input >> Cache.RegistrationScanFingerprint))
    {
        return std::nullopt;
    }
    if (Cache.SchemaVersion >= 5)
    {
        if (!(Input >> Token) || Token != "registration_args")
        {
            return std::nullopt;
        }
        if (!(Input >> Cache.RegistrationCompileArgsHash))
        {
            return std::nullopt;
        }
    }
    if (!(Input >> Token) || Token != "knowledge")
    {
        return std::nullopt;
    }
    if (!(Input >> Cache.KnowledgeFingerprint))
    {
        return std::nullopt;
    }
    std::size_t RegisteredCount = 0;
    if (!(Input >> Token) || Token != "registered" || !(Input >> RegisteredCount))
    {
        return std::nullopt;
    }
    for (std::size_t Index = 0; Index < RegisteredCount; ++Index)
    {
        std::string Key{};
        if (!(Input >> Token) || Token != "key" || !(Input >> std::quoted(Key)))
        {
            return std::nullopt;
        }
        Cache.Knowledge.RegisteredTypeKeys.insert(std::move(Key));
    }
    std::size_t VisibleCount = 0;
    if (!(Input >> Token) || Token != "visible" || !(Input >> VisibleCount))
    {
        return std::nullopt;
    }
    for (std::size_t Index = 0; Index < VisibleCount; ++Index)
    {
        std::string Key{};
        if (!(Input >> Token) || Token != "key" || !(Input >> std::quoted(Key)))
        {
            return std::nullopt;
        }
        Cache.Knowledge.HeaderVisibleTypeKeys.insert(std::move(Key));
    }

    if (Cache.SchemaVersion >= 5)
    {
        std::size_t RegistrationFileCount = 0;
        if (!(Input >> Token) || Token != "registration_files" || !(Input >> RegistrationFileCount))
        {
            return std::nullopt;
        }
        for (std::size_t FileIndex = 0; FileIndex < RegistrationFileCount; ++FileIndex)
        {
            if (!(Input >> Token) || Token != "regfile")
            {
                return std::nullopt;
            }

            std::string FileKey{};
            RegistrationFileCacheEntry Entry{};
            std::size_t DependencyCount = 0;
            std::size_t ResolvedKeyCount = 0;
            if (!(Input >> std::quoted(FileKey) >> Entry.SourceHash >> DependencyCount >> ResolvedKeyCount))
            {
                return std::nullopt;
            }

            Entry.Dependencies.reserve(DependencyCount);
            for (std::size_t DependencyIndex = 0; DependencyIndex < DependencyCount; ++DependencyIndex)
            {
                if (!(Input >> Token) || Token != "dep")
                {
                    return std::nullopt;
                }

                std::string DependencyPath{};
                FileDependencySnapshot Dependency{};
                if (!(Input >> std::quoted(DependencyPath) >> Dependency.Size >> Dependency.WriteTime))
                {
                    return std::nullopt;
                }
                Dependency.Path = NormalizePath(fs::path(DependencyPath));
                Entry.Dependencies.push_back(std::move(Dependency));
            }

            for (std::size_t KeyIndex = 0; KeyIndex < ResolvedKeyCount; ++KeyIndex)
            {
                std::string Key{};
                if (!(Input >> Token) || Token != "rkey" || !(Input >> std::quoted(Key)))
                {
                    return std::nullopt;
                }
                Entry.ResolvedTypeKeys.insert(std::move(Key));
            }

            if (!(Input >> Token) || Token != "endregfile")
            {
                return std::nullopt;
            }

            Cache.RegistrationFiles.emplace(NormalizePath(fs::path(FileKey)).generic_string(), std::move(Entry));
        }
    }

    while (Input >> Token)
    {
        if (Token != "header")
        {
            return std::nullopt;
        }

        std::string HeaderKey{};
        HeaderCacheEntry Entry{};
        int HasMarkers = 0;
        int HasTypeExpressions = 0;
        int IsTemplatePrimary = 0;
        std::size_t TypeNameCount = 0;
        std::size_t KnowledgeDependencyCount = 0;
        std::size_t TemplateCandidateCount = 0;
        if (!(Input >> std::quoted(HeaderKey)
              >> Entry.Fingerprint
              >> HasMarkers
              >> HasTypeExpressions
              >> IsTemplatePrimary
              >> TypeNameCount))
        {
            return std::nullopt;
        }
        if (Cache.SchemaVersion >= 6 && !(Input >> KnowledgeDependencyCount))
        {
            return std::nullopt;
        }
        if (Cache.SchemaVersion >= 7 && !(Input >> TemplateCandidateCount))
        {
            return std::nullopt;
        }
        Entry.HasMarkers = HasMarkers != 0;
        Entry.HasTypeExpressions = HasTypeExpressions != 0;
        Entry.IsTemplatePrimary = IsTemplatePrimary != 0;

        Entry.TypeNameEntries.reserve(TypeNameCount);
        for (std::size_t Index = 0; Index < TypeNameCount; ++Index)
        {
            if (!(Input >> Token) || Token != "type")
            {
                return std::nullopt;
            }

            std::string Header{};
            std::string TypeNameHeader{};
            int IsEnum = 0;
            int HasHeaderVisibleTypeName = 0;
            int NeedsGeneratedTypeName = 0;
            TypeNameCacheEntry TypeEntry{};
            if (!(Input >> std::quoted(Header)
                  >> std::quoted(TypeNameHeader)
                  >> std::quoted(TypeEntry.QualifiedName)
                  >> std::quoted(TypeEntry.ReflectedName)))
            {
                return std::nullopt;
            }
            if (Cache.SchemaVersion >= 9)
            {
                if (!(Input >> IsEnum >> HasHeaderVisibleTypeName >> NeedsGeneratedTypeName))
                {
                    return std::nullopt;
                }
            }
            else
            {
                if (!(Input >> NeedsGeneratedTypeName))
                {
                    return std::nullopt;
                }
            }
            TypeEntry.Header = NormalizePath(fs::path(Header));
            TypeEntry.TypeNameHeader =
                TypeNameHeader.empty() ? fs::path{} : NormalizePath(fs::path(TypeNameHeader));
            TypeEntry.IsEnum = IsEnum != 0;
            TypeEntry.HasHeaderVisibleTypeName = HasHeaderVisibleTypeName != 0;
            TypeEntry.NeedsGeneratedTypeName = NeedsGeneratedTypeName != 0;
            Entry.TypeNameEntries.push_back(std::move(TypeEntry));
        }
        for (std::size_t Index = 0; Index < KnowledgeDependencyCount; ++Index)
        {
            std::string Key{};
            if (!(Input >> Token) || Token != "depkey" || !(Input >> std::quoted(Key)))
            {
                return std::nullopt;
            }
            Entry.KnowledgeDependencies.insert(std::move(Key));
        }
        Entry.TemplateCandidates.reserve(TemplateCandidateCount);
        for (std::size_t Index = 0; Index < TemplateCandidateCount; ++Index)
        {
            if (!(Input >> Token) || Token != "candidate")
            {
                return std::nullopt;
            }

            std::string CandidateFile{};
            TemplateSpecializationCandidate Candidate{};
            std::size_t TemplateArgumentCount = 0;
            std::size_t SubstitutionCount = 0;
            if (!(Input >> Candidate.TemplateIndex
                  >> std::quoted(CandidateFile)
                  >> Candidate.Line
                  >> Candidate.Column
                  >> std::quoted(Candidate.QualifiedName)
                  >> std::quoted(Candidate.ReflectedName)
                  >> TemplateArgumentCount
                  >> SubstitutionCount))
            {
                return std::nullopt;
            }
            Candidate.File = NormalizePath(fs::path(CandidateFile));

            Candidate.TemplateArgumentKeys.reserve(TemplateArgumentCount);
            for (std::size_t ArgumentIndex = 0; ArgumentIndex < TemplateArgumentCount; ++ArgumentIndex)
            {
                std::string Key{};
                if (!(Input >> Token) || Token != "arg" || !(Input >> std::quoted(Key)))
                {
                    return std::nullopt;
                }
                Candidate.TemplateArgumentKeys.push_back(std::move(Key));
            }

            Candidate.Substitutions.reserve(SubstitutionCount);
            for (std::size_t SubstitutionIndex = 0; SubstitutionIndex < SubstitutionCount; ++SubstitutionIndex)
            {
                std::string Name{};
                std::string Value{};
                if (!(Input >> Token) || Token != "subst" || !(Input >> std::quoted(Name)) ||
                    !(Input >> std::quoted(Value)))
                {
                    return std::nullopt;
                }
                Candidate.Substitutions.emplace_back(std::move(Name), std::move(Value));
            }

            if (!(Input >> Token) || Token != "endcandidate")
            {
                return std::nullopt;
            }

            Entry.TemplateCandidates.push_back(std::move(Candidate));
        }

        if (!(Input >> Token) || Token != "endheader")
        {
            return std::nullopt;
        }

        Cache.Headers.emplace(NormalizePath(fs::path(HeaderKey)).generic_string(), std::move(Entry));
    }

    return Cache;
}

std::string JoinParagraphs(const std::vector<std::string>& Paragraphs)
{
    std::string Result{};
    for (const std::string& Paragraph : Paragraphs)
    {
        if (Paragraph.empty())
        {
            continue;
        }

        if (!Result.empty())
        {
            Result += "\n\n";
        }
        Result += Paragraph;
    }
    return Result;
}

std::string CommentNodeText(CXComment Comment);

std::optional<CXComment> FirstChildOfKind(const CXComment Comment, const CXCommentKind Kind)
{
    const unsigned Count = clang_Comment_getNumChildren(Comment);
    for (unsigned Index = 0; Index < Count; ++Index)
    {
        const CXComment Child = clang_Comment_getChild(Comment, Index);
        if (clang_Comment_getKind(Child) == Kind)
        {
            return Child;
        }
    }
    return std::nullopt;
}

std::string CommentParagraphText(const CXComment Comment)
{
    std::string Result{};
    const unsigned Count = clang_Comment_getNumChildren(Comment);
    for (unsigned Index = 0; Index < Count; ++Index)
    {
        Result += CommentNodeText(clang_Comment_getChild(Comment, Index));
    }
    return NormalizeParagraphText(Result);
}

std::string CommentNodeText(const CXComment Comment)
{
    switch (clang_Comment_getKind(Comment))
    {
    case CXComment_Text:
        return ToStringDispose(clang_TextComment_getText(Comment));
    case CXComment_InlineCommand:
    {
        std::string Result{};
        const unsigned ArgCount = clang_InlineCommandComment_getNumArgs(Comment);
        for (unsigned ArgIndex = 0; ArgIndex < ArgCount; ++ArgIndex)
        {
            if (!Result.empty())
            {
                Result.push_back(' ');
            }
            Result += ToStringDispose(clang_InlineCommandComment_getArgText(Comment, ArgIndex));
        }
        return Result;
    }
    case CXComment_Paragraph:
        return CommentParagraphText(Comment);
    case CXComment_HTMLStartTag:
    case CXComment_HTMLEndTag:
        return {};
    case CXComment_BlockCommand:
    {
        if (const auto Paragraph = FirstChildOfKind(Comment, CXComment_Paragraph))
        {
            return CommentParagraphText(*Paragraph);
        }
        return {};
    }
    case CXComment_VerbatimBlockLine:
        return ToStringDispose(clang_VerbatimBlockLineComment_getText(Comment));
    case CXComment_VerbatimLine:
        return ToStringDispose(clang_VerbatimLineComment_getText(Comment));
    default:
        return {};
    }
}

ParsedComment ExtractParsedComment(const CXCursor Cursor)
{
    ParsedComment Result{};
    const CXComment FullComment = clang_Cursor_getParsedComment(Cursor);
    if (clang_Comment_getKind(FullComment) == CXComment_Null)
    {
        return Result;
    }

    std::vector<std::string> Paragraphs{};
    const unsigned Count = clang_Comment_getNumChildren(FullComment);
    for (unsigned Index = 0; Index < Count; ++Index)
    {
        const CXComment Child = clang_Comment_getChild(FullComment, Index);
        switch (clang_Comment_getKind(Child))
        {
        case CXComment_Paragraph:
        {
            const std::string Text = CommentParagraphText(Child);
            if (!Text.empty())
            {
                Paragraphs.push_back(Text);
            }
            break;
        }
        case CXComment_BlockCommand:
        {
            if (const auto Paragraph = FirstChildOfKind(Child, CXComment_Paragraph))
            {
                const std::string Text = CommentParagraphText(*Paragraph);
                if (!Text.empty())
                {
                    Paragraphs.push_back(Text);
                }
            }
            break;
        }
        case CXComment_ParamCommand:
        {
            const std::string ParamName = Trim(ToStringDispose(clang_ParamCommandComment_getParamName(Child)));
            std::string ParamDoc{};
            if (const auto Paragraph = FirstChildOfKind(Child, CXComment_Paragraph))
            {
                ParamDoc = CommentParagraphText(*Paragraph);
            }
            if (!ParamName.empty() && !ParamDoc.empty())
            {
                Result.ParamDocs[ParamName] = std::move(ParamDoc);
            }
            break;
        }
        case CXComment_VerbatimBlockCommand:
        {
            std::string Text{};
            const unsigned LineCount = clang_Comment_getNumChildren(Child);
            for (unsigned LineIndex = 0; LineIndex < LineCount; ++LineIndex)
            {
                const std::string Line = CommentNodeText(clang_Comment_getChild(Child, LineIndex));
                if (Line.empty())
                {
                    continue;
                }
                if (!Text.empty())
                {
                    Text.push_back('\n');
                }
                Text += Line;
            }
            if (!Text.empty())
            {
                Paragraphs.push_back(Text);
            }
            break;
        }
        case CXComment_VerbatimLine:
        {
            const std::string Text = CommentNodeText(Child);
            if (!Text.empty())
            {
                Paragraphs.push_back(Text);
            }
            break;
        }
        default:
            break;
        }
    }

    Result.Doc = JoinParagraphs(Paragraphs);
    return Result;
}

fs::path CursorFilePath(const CXCursor Cursor)
{
    CXSourceLocation Location = clang_getCursorLocation(Cursor);
    CXFile File{};
    unsigned Line = 0;
    unsigned Column = 0;
    unsigned Offset = 0;
    clang_getFileLocation(Location, &File, &Line, &Column, &Offset);
    return NormalizePath(FilePathFromCXFile(File));
}

bool CursorIsFromTrackedHeaders(const CXCursor Cursor, const std::unordered_set<std::string>& HeaderKeys)
{
    const fs::path Path = CursorFilePath(Cursor);
    return !Path.empty() && HeaderKeys.contains(Path.generic_string());
}

Diagnostic MakeDiagnostic(const CXCursor Cursor, std::string Message)
{
    CXSourceLocation Location = clang_getCursorLocation(Cursor);
    CXFile File{};
    unsigned Line = 0;
    unsigned Column = 0;
    unsigned Offset = 0;
    clang_getFileLocation(Location, &File, &Line, &Column, &Offset);

    Diagnostic Result{};
    if (File)
    {
        Result.File = FilePathFromCXFile(File);
    }
    Result.Line = Line;
    Result.Column = Column;
    Result.Message = std::move(Message);
    return Result;
}

Diagnostic MakeDiagnostic(const fs::path& File, const unsigned Line, const unsigned Column, std::string Message)
{
    Diagnostic Result{};
    Result.File = NormalizePath(File);
    Result.Line = Line;
    Result.Column = Column;
    Result.Message = std::move(Message);
    return Result;
}

Diagnostic MakeWarning(const CXCursor Cursor, std::string Message)
{
    Diagnostic Result = MakeDiagnostic(Cursor, std::move(Message));
    Result.Severity = DiagnosticSeverity::Warning;
    return Result;
}

Diagnostic MakeWarning(const fs::path& File, const unsigned Line, const unsigned Column, std::string Message)
{
    Diagnostic Result = MakeDiagnostic(File, Line, Column, std::move(Message));
    Result.Severity = DiagnosticSeverity::Warning;
    return Result;
}

bool HasErrors(const std::vector<Diagnostic>& Diagnostics)
{
    return std::any_of(Diagnostics.begin(), Diagnostics.end(), [](const Diagnostic& Entry) {
        return Entry.Severity == DiagnosticSeverity::Error;
    });
}

std::string QualifiedNameForCursor(const CXCursor Cursor)
{
    std::vector<std::string> Parts{};
    CXCursor Current = Cursor;
    while (!clang_Cursor_isNull(Current))
    {
        const CXCursorKind Kind = clang_getCursorKind(Current);
        if (Kind == CXCursor_TranslationUnit)
        {
            break;
        }

        if (Kind == CXCursor_Namespace ||
            Kind == CXCursor_StructDecl ||
            Kind == CXCursor_ClassDecl ||
            Kind == CXCursor_EnumDecl ||
            Kind == CXCursor_ClassTemplate)
        {
            const std::string Spelling = ToStringDispose(clang_getCursorSpelling(Current));
            if (!Spelling.empty())
            {
                Parts.push_back(Spelling);
            }
        }

        Current = clang_getCursorSemanticParent(Current);
    }

    std::reverse(Parts.begin(), Parts.end());

    std::string Result{};
    for (const std::string& Part : Parts)
    {
        if (!Result.empty())
        {
            Result += "::";
        }
        Result += Part;
    }
    return Result;
}

bool CursorOrBasesMatchQualifiedName(const CXCursor Cursor,
                                     const std::string_view QualifiedName,
                                     std::unordered_set<std::string>& Visited)
{
    if (clang_Cursor_isNull(Cursor))
    {
        return false;
    }

    const std::string CursorName = NormalizeTypeExpressionString(QualifiedNameForCursor(Cursor));
    if (CursorName.empty())
    {
        return false;
    }
    if (CursorName == NormalizeTypeExpressionString(QualifiedName))
    {
        return true;
    }
    if (!Visited.insert(CursorName).second)
    {
        return false;
    }

    struct VisitState
    {
        std::string_view QualifiedName{};
        std::unordered_set<std::string>* Visited = nullptr;
        bool Match = false;
    } State{QualifiedName, &Visited, false};

    clang_visitChildren(
        Cursor,
        [](CXCursor Child, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const StatePtr = static_cast<VisitState*>(ClientData);
            if (StatePtr->Match || clang_getCursorKind(Child) != CXCursor_CXXBaseSpecifier)
            {
                return StatePtr->Match ? CXChildVisit_Break : CXChildVisit_Continue;
            }

            const CXCursor BaseDecl = clang_getTypeDeclaration(clang_getCanonicalType(clang_getCursorType(Child)));
            if (clang_Cursor_isNull(BaseDecl))
            {
                return CXChildVisit_Continue;
            }

            if (CursorOrBasesMatchQualifiedName(BaseDecl, StatePtr->QualifiedName, *StatePtr->Visited))
            {
                StatePtr->Match = true;
                return CXChildVisit_Break;
            }

            return CXChildVisit_Continue;
        },
        &State);

    return State.Match;
}

bool CursorIsDerivedFromQualifiedName(const CXCursor Cursor, const std::string_view QualifiedName)
{
    std::unordered_set<std::string> Visited{};
    return CursorOrBasesMatchQualifiedName(Cursor, QualifiedName, Visited);
}

bool IsSignedIntegerType(const CXType Type)
{
    switch (clang_getCanonicalType(Type).kind)
    {
    case CXType_SChar:
    case CXType_Short:
    case CXType_Int:
    case CXType_Long:
    case CXType_LongLong:
    case CXType_Int128:
    case CXType_Char_S:
    case CXType_WChar:
        return true;
    default:
        return false;
    }
}

std::string CppStringLiteral(std::string_view Value)
{
    std::string Result = "\"";
    for (const char Ch : Value)
    {
        switch (Ch)
        {
        case '\\':
            Result += "\\\\";
            break;
        case '"':
            Result += "\\\"";
            break;
        case '\n':
            Result += "\\n";
            break;
        case '\r':
            Result += "\\r";
            break;
        case '\t':
            Result += "\\t";
            break;
        default:
            Result.push_back(Ch);
            break;
        }
    }
    Result.push_back('"');
    return Result;
}

std::string PrettyPrintedTypeForCode(const CXType Type, const CXCursor Context)
{
    CXPrintingPolicy Policy = clang_getCursorPrintingPolicy(Context);
    clang_PrintingPolicy_setProperty(Policy, CXPrintingPolicy_SuppressTagKeyword, 1);
    clang_PrintingPolicy_setProperty(Policy, CXPrintingPolicy_SuppressScope, 0);
    clang_PrintingPolicy_setProperty(Policy, CXPrintingPolicy_SuppressUnwrittenScope, 0);
    clang_PrintingPolicy_setProperty(Policy, CXPrintingPolicy_FullyQualifiedName, 1);

    std::string Result = ToStringDispose(clang_getFullyQualifiedName(Type, Policy, 0));
    if (Result.empty())
    {
        Result = ToStringDispose(clang_getTypePrettyPrinted(Type, Policy));
    }
    clang_PrintingPolicy_dispose(Policy);

    if (Result.empty())
    {
        Result = ToStringDispose(clang_getTypeSpelling(Type));
    }

    return Result;
}

std::optional<std::string> FullyQualifiedTemplateSpecializationKey(const CXType Type, const CXCursor Context)
{
    const int ArgCount = clang_Type_getNumTemplateArguments(Type);
    if (ArgCount <= 0)
    {
        return std::nullopt;
    }

    CXCursor TypeDecl = clang_getTypeDeclaration(Type);
    if (clang_Cursor_isNull(TypeDecl))
    {
        return std::nullopt;
    }

    if (const CXCursor PrimaryTemplate = clang_getSpecializedCursorTemplate(TypeDecl);
        !clang_Cursor_isNull(PrimaryTemplate))
    {
        TypeDecl = PrimaryTemplate;
    }

    const std::string BaseName = QualifiedNameForCursor(TypeDecl);
    if (BaseName.empty())
    {
        return std::nullopt;
    }

    std::string Result = BaseName;
    Result += '<';
    for (int ArgIndex = 0; ArgIndex < ArgCount; ++ArgIndex)
    {
        if (ArgIndex > 0)
        {
            Result += ", ";
        }

        const CXType ArgType = clang_Type_getTemplateArgumentAsType(Type, static_cast<unsigned>(ArgIndex));
        if (ArgType.kind == CXType_Invalid)
        {
            return std::nullopt;
        }
        const std::string ArgExpr = PrettyPrintedTypeForCode(ArgType, Context);
        if (ArgExpr.empty())
        {
            return std::nullopt;
        }
        Result += ArgExpr;
    }
    Result += '>';
    return Result;
}

void AddRegisteredTypeKeyCandidates(std::unordered_set<std::string>& Out, const CXType Type, const CXCursor Context)
{
    if (Type.kind == CXType_Invalid)
    {
        return;
    }

    AddTypeKeyCandidate(Out, PrettyPrintedTypeForCode(Type, Context));
    if (const auto TemplateKey = FullyQualifiedTemplateSpecializationKey(Type, Context))
    {
        AddTypeKeyCandidate(Out, *TemplateKey);
    }

    const CXCursor TypeDecl = clang_getTypeDeclaration(Type);
    if (!clang_Cursor_isNull(TypeDecl))
    {
        AddTypeKeyCandidate(Out, QualifiedNameForCursor(TypeDecl));
        if (clang_getCursorKind(TypeDecl) == CXCursor_TypedefDecl)
        {
            AddRegisteredTypeKeyCandidates(Out, clang_getTypedefDeclUnderlyingType(TypeDecl), TypeDecl);
        }
    }

    const CXType Canonical = clang_getCanonicalType(Type);
    if (Canonical.kind != CXType_Invalid)
    {
        AddTypeKeyCandidate(Out, PrettyPrintedTypeForCode(Canonical, Context));
        if (const auto TemplateKey = FullyQualifiedTemplateSpecializationKey(Canonical, Context))
        {
            AddTypeKeyCandidate(Out, *TemplateKey);
        }
        const CXCursor CanonicalDecl = clang_getTypeDeclaration(Canonical);
        if (!clang_Cursor_isNull(CanonicalDecl))
        {
            AddTypeKeyCandidate(Out, QualifiedNameForCursor(CanonicalDecl));
        }
    }
}

std::string ReflectableTypeCondition(const CXType Type, const CXCursor Context)
{
    const std::string TypeExpr = PrettyPrintedTypeForCode(Type, Context);
    if (TypeExpr.empty())
    {
        return "false";
    }
    return "::SnAPI::GameFramework::THasReflectedTypeName<" + TypeExpr + ">::value";
}

bool IsRegisteredTypeKey(const CXType Type, const CXCursor Context, const RegistrationKnowledge& Knowledge)
{
    std::unordered_set<std::string> Candidates{};
    AddRegisteredTypeKeyCandidates(Candidates, Type, Context);
    for (const std::string& Candidate : Candidates)
    {
        if (Knowledge.RegisteredTypeKeys.contains(Candidate))
        {
            return true;
        }
    }
    return false;
}

bool IsTemplateDependentTypeImpl(const CXType Type,
                                 const CXCursor Context,
                                 const std::unordered_set<std::string>& TemplateParameters)
{
    if (Type.kind == CXType_Invalid)
    {
        return false;
    }

    const CXType Canonical = clang_getCanonicalType(Type);
    const CXType Effective = Canonical.kind != CXType_Invalid ? Canonical : Type;

    const auto IsTemplateParameterDecl = [](const CXType CandidateType) {
        const CXCursor Decl = clang_getTypeDeclaration(CandidateType);
        return !clang_Cursor_isNull(Decl) && clang_getCursorKind(Decl) == CXCursor_TemplateTypeParameter;
    };
    if (IsTemplateParameterDecl(Type) || IsTemplateParameterDecl(Effective))
    {
        return true;
    }

    if (TypeExpressionMentionsTemplateParameter(PrettyPrintedTypeForCode(Type, Context), TemplateParameters) ||
        TypeExpressionMentionsTemplateParameter(PrettyPrintedTypeForCode(Effective, Context), TemplateParameters))
    {
        return true;
    }

    const int TemplateArgCount = clang_Type_getNumTemplateArguments(Effective);
    for (int ArgIndex = 0; ArgIndex < TemplateArgCount; ++ArgIndex)
    {
        const CXType ArgType = clang_Type_getTemplateArgumentAsType(Effective, static_cast<unsigned>(ArgIndex));
        if (ArgType.kind != CXType_Invalid && IsTemplateDependentTypeImpl(ArgType, Context, TemplateParameters))
        {
            return true;
        }
    }

    switch (Effective.kind)
    {
    case CXType_Pointer:
        return IsTemplateDependentTypeImpl(clang_getPointeeType(Effective), Context, TemplateParameters);
    case CXType_LValueReference:
    case CXType_RValueReference:
        return IsTemplateDependentTypeImpl(clang_getPointeeType(Effective), Context, TemplateParameters);
    case CXType_ConstantArray:
    case CXType_IncompleteArray:
    case CXType_VariableArray:
    case CXType_DependentSizedArray:
        return IsTemplateDependentTypeImpl(clang_getArrayElementType(Effective), Context, TemplateParameters);
    default:
        break;
    }

    return false;
}

bool IsTemplateDependentType(const CXType Type, const CXCursor Context)
{
    const std::unordered_set<std::string> TemplateParameters = EnclosingTemplateParameterNames(Context);
    return IsTemplateDependentTypeImpl(Type, Context, TemplateParameters);
}

bool IsTypeReflectionCompatible(const CXType Type, const CXCursor Context, const RegistrationKnowledge& Knowledge)
{
    if (IsRegisteredTypeKey(Type, Context, Knowledge))
    {
        return true;
    }

    if (IsTemplateDependentType(Type, Context))
    {
        return true;
    }

    const CXType Canonical = clang_getCanonicalType(Type);
    switch (Canonical.kind)
    {
    case CXType_Pointer:
        return IsTypeReflectionCompatible(clang_getPointeeType(Canonical), Context, Knowledge);
    case CXType_LValueReference:
    case CXType_RValueReference:
        return IsTypeReflectionCompatible(clang_getPointeeType(Canonical), Context, Knowledge);
    case CXType_ConstantArray:
    case CXType_IncompleteArray:
    case CXType_VariableArray:
    case CXType_DependentSizedArray:
        return IsTypeReflectionCompatible(clang_getArrayElementType(Canonical), Context, Knowledge);
    default:
        break;
    }

    return false;
}

std::string CombineConditions(const std::vector<std::string>& Conditions)
{
    if (Conditions.empty())
    {
        return "true";
    }

    std::string Result{};
    for (const std::string& Condition : Conditions)
    {
        if (Condition.empty())
        {
            continue;
        }

        if (!Result.empty())
        {
            Result += " && ";
        }
        Result += Condition;
    }

    if (Result.empty())
    {
        return "true";
    }
    return Result;
}

const std::vector<ReflectionMarker>& MarkerVectorForKind(const HeaderMarkers& Markers, const MarkerKind Kind)
{
    switch (Kind)
    {
    case MarkerKind::Type:
        return Markers.Type;
    case MarkerKind::Field:
        return Markers.Field;
    case MarkerKind::Function:
        return Markers.Function;
    case MarkerKind::EnumValue:
        return Markers.EnumValue;
    case MarkerKind::Generated:
        return Markers.Generated;
    }

    return Markers.Type;
}

std::vector<unsigned char>& MarkerUsageVectorForKind(HeaderMarkerCursor& Cursor, const MarkerKind Kind)
{
    switch (Kind)
    {
    case MarkerKind::Type:
        return Cursor.UsedType;
    case MarkerKind::Field:
        return Cursor.UsedField;
    case MarkerKind::Function:
        return Cursor.UsedFunction;
    case MarkerKind::EnumValue:
        return Cursor.UsedEnumValue;
    case MarkerKind::Generated:
        return Cursor.UsedGenerated;
    }

    return Cursor.UsedType;
}

unsigned FirstOwnedChildOffset(const CXCursor Cursor)
{
    unsigned Result = 0;
    clang_visitChildren(
        Cursor,
        [](CXCursor Child, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const ResultPtr = static_cast<unsigned*>(ClientData);
            const CXCursorKind Kind = clang_getCursorKind(Child);
            switch (Kind)
            {
            case CXCursor_FieldDecl:
            case CXCursor_CXXMethod:
            case CXCursor_Constructor:
            case CXCursor_ClassDecl:
            case CXCursor_StructDecl:
            case CXCursor_EnumDecl:
            case CXCursor_ClassTemplate:
                break;
            default:
                return CXChildVisit_Continue;
            }

            const unsigned Offset = CursorExtentStartOffset(Child);
            if (*ResultPtr == 0 || (Offset != 0 && Offset < *ResultPtr))
            {
                *ResultPtr = Offset;
            }
            return CXChildVisit_Continue;
        },
        &Result);
    return Result;
}

const ReflectionMarker* MatchSourceMarker(const std::unordered_map<std::string, HeaderMarkers>& MarkerTable,
                                          std::unordered_map<std::string, HeaderMarkerCursor>& MarkerCursors,
                                          const CXCursor Cursor,
                                          const MarkerKind Kind,
                                          const bool Consume)
{
    const fs::path File = CursorFilePath(Cursor);
    if (File.empty())
    {
        return nullptr;
    }

    const auto MarkersIt = MarkerTable.find(File.generic_string());
    if (MarkersIt == MarkerTable.end())
    {
        return nullptr;
    }

    const auto CursorsIt = MarkerCursors.find(File.generic_string());
    if (CursorsIt == MarkerCursors.end())
    {
        return nullptr;
    }

    const std::vector<ReflectionMarker>& Markers = MarkerVectorForKind(MarkersIt->second, Kind);
    std::vector<unsigned char>& Usage = MarkerUsageVectorForKind(CursorsIt->second, Kind);
    if (Usage.size() != Markers.size())
    {
        Usage.assign(Markers.size(), 0);
    }

    const unsigned CursorOffset = CursorExtentStartOffset(Cursor);
    const std::string_view Source = MarkersIt->second.Source;

    const ReflectionMarker* Candidate = nullptr;
    std::size_t CandidateIndex = static_cast<std::size_t>(-1);
    for (std::size_t Index = 0; Index < Markers.size(); ++Index)
    {
        if (Usage[Index] != 0)
        {
            continue;
        }

        const ReflectionMarker& Marker = Markers[Index];
        if (Marker.EndOffset > CursorOffset)
        {
            break;
        }
        if (!ContainsOnlyTriviaBetween(Source, Marker.EndOffset, CursorOffset))
        {
            continue;
        }

        Candidate = &Marker;
        CandidateIndex = Index;
    }

    if (!Candidate)
    {
        return nullptr;
    }

    if (Consume)
    {
        Usage[CandidateIndex] = 1;
    }
    return Candidate;
}

const ReflectionMarker* MatchGeneratedMarker(const std::unordered_map<std::string, HeaderMarkers>& MarkerTable,
                                             std::unordered_map<std::string, HeaderMarkerCursor>& MarkerCursors,
                                             const CXCursor Cursor,
                                             const bool Consume)
{
    const fs::path File = CursorFilePath(Cursor);
    if (File.empty())
    {
        return nullptr;
    }

    const auto MarkersIt = MarkerTable.find(File.generic_string());
    if (MarkersIt == MarkerTable.end())
    {
        return nullptr;
    }

    const auto CursorsIt = MarkerCursors.find(File.generic_string());
    if (CursorsIt == MarkerCursors.end())
    {
        return nullptr;
    }

    const std::vector<ReflectionMarker>& Markers = MarkersIt->second.Generated;
    std::vector<unsigned char>& Usage = CursorsIt->second.UsedGenerated;
    if (Usage.size() != Markers.size())
    {
        Usage.assign(Markers.size(), 0);
    }

    const unsigned CursorBegin = CursorExtentStartOffset(Cursor);
    const unsigned CursorEnd = CursorExtentEndOffset(Cursor);
    const unsigned FirstChild = FirstOwnedChildOffset(Cursor);

    for (std::size_t Index = 0; Index < Markers.size(); ++Index)
    {
        const ReflectionMarker& Marker = Markers[Index];
        if (Marker.StartOffset < CursorBegin || Marker.EndOffset > CursorEnd)
        {
            continue;
        }
        if (FirstChild != 0 && Marker.StartOffset > FirstChild)
        {
            continue;
        }
        if (Consume)
        {
            Usage[Index] = 1;
        }
        return &Marker;
    }

    return nullptr;
}

bool ValidatePayload(const AnnotationPayload& Payload,
                     const std::initializer_list<std::string_view> AllowedKeys,
                     const std::initializer_list<std::string_view> AllowedFlags,
                     const CXCursor Cursor,
                     std::vector<Diagnostic>& Diagnostics)
{
    bool Ok = true;
    for (const auto& [Key, Value] : Payload.Values)
    {
        (void)Value;
        const bool Allowed = std::find(AllowedKeys.begin(), AllowedKeys.end(), Key) != AllowedKeys.end();
        if (!Allowed)
        {
            Diagnostics.push_back(MakeDiagnostic(Cursor, "Unsupported metadata key '" + Key + "' on " + Payload.Kind));
            Ok = false;
        }
    }

    for (const std::string& Flag : Payload.Flags)
    {
        const bool Allowed = std::find(AllowedFlags.begin(), AllowedFlags.end(), Flag) != AllowedFlags.end();
        if (!Allowed)
        {
            Diagnostics.push_back(MakeDiagnostic(Cursor, "Unsupported metadata flag '" + Flag + "' on " + Payload.Kind));
            Ok = false;
        }
    }

    return Ok;
}

std::string BuildFieldFlagsExpression(const AnnotationPayload& Payload)
{
    std::vector<std::string> Bits{};
    Bits.push_back("::SnAPI::GameFramework::EFieldFlagBits::Serialized");
    if (Payload.Flags.contains("replicated"))
    {
        Bits.push_back("::SnAPI::GameFramework::EFieldFlagBits::Replication");
    }
    if (const auto RepIt = Payload.Values.find("rep"); RepIt != Payload.Values.end())
    {
        if (RepIt->second == "reliable")
        {
            Bits.push_back("::SnAPI::GameFramework::EFieldFlagBits::ReplicationReliable");
        }
        else if (RepIt->second == "unreliable")
        {
            Bits.push_back("::SnAPI::GameFramework::EFieldFlagBits::ReplicationUnreliable");
        }
    }

    std::string Result{};
    for (const std::string& Bit : Bits)
    {
        if (!Result.empty())
        {
            Result += " | ";
        }
        Result += Bit;
    }
    return Result;
}

std::string BuildFieldEditorFlagsExpression(const AnnotationPayload& Payload)
{
    std::vector<std::string> Bits{};
    if (Payload.Flags.contains("hidden"))
    {
        Bits.push_back("::SnAPI::GameFramework::EFieldEditorFlagBits::Hidden");
    }
    if (Payload.Flags.contains("read_only"))
    {
        Bits.push_back("::SnAPI::GameFramework::EFieldEditorFlagBits::ReadOnly");
    }
    if (Payload.Flags.contains("advanced"))
    {
        Bits.push_back("::SnAPI::GameFramework::EFieldEditorFlagBits::Advanced");
    }
    if (Payload.Flags.contains("heavy_data"))
    {
        Bits.push_back("::SnAPI::GameFramework::EFieldEditorFlagBits::HeavyData");
    }

    if (Bits.empty())
    {
        return "{}";
    }

    std::string Result{};
    for (const std::string& Bit : Bits)
    {
        if (!Result.empty())
        {
            Result += " | ";
        }
        Result += Bit;
    }
    return Result;
}

std::string FieldPointerExpression(const std::string& OwnerQualifiedName, const CXCursor FieldCursor)
{
    return "&" + OwnerQualifiedName + "::" + ToStringDispose(clang_getCursorSpelling(FieldCursor));
}

std::string ComparableTypeKey(const CXType Type, const CXCursor Context)
{
    std::string Result = PrettyPrintedTypeForCode(Type, Context);
    Result = NormalizeTypeExpressionString(Result);

    for (;;)
    {
        if (Result.starts_with("const"))
        {
            Result.erase(0, 5);
            continue;
        }
        if (Result.starts_with("volatile"))
        {
            Result.erase(0, 8);
            continue;
        }
        break;
    }

    while (Result.ends_with("&"))
    {
        Result.pop_back();
    }

    while (Result.starts_with("::"))
    {
        Result.erase(0, 2);
    }

    return Result;
}

std::string DerivePropertyNameFromMethodName(const std::string& MethodName)
{
    auto StripPrefix = [&](std::string_view Prefix) -> std::optional<std::string> {
        if (!MethodName.starts_with(Prefix) || MethodName.size() <= Prefix.size())
        {
            return std::nullopt;
        }

        const char Next = MethodName[Prefix.size()];
        if (std::isupper(static_cast<unsigned char>(Next)) == 0)
        {
            return std::nullopt;
        }

        return MethodName.substr(Prefix.size());
    };

    if (const auto Value = StripPrefix("Get"))
    {
        return *Value;
    }
    if (const auto Value = StripPrefix("Set"))
    {
        return *Value;
    }
    if (const auto Value = StripPrefix("Edit"))
    {
        return *Value;
    }
    return MethodName;
}

std::string BuildMethodFlagsExpression(const AnnotationPayload& Payload, const bool IncludeRpcBits = true)
{
    std::vector<std::string> Bits{};
    if (Payload.Flags.contains("editor_action"))
    {
        Bits.push_back("::SnAPI::GameFramework::EMethodFlagBits::EditorAction");
    }

    const auto RpcIt = Payload.Values.find("rpc");
    if (IncludeRpcBits && RpcIt != Payload.Values.end())
    {
        if (RpcIt->second == "reliable")
        {
            Bits.push_back("::SnAPI::GameFramework::EMethodFlagBits::RpcReliable");
        }
        else if (RpcIt->second == "unreliable")
        {
            Bits.push_back("::SnAPI::GameFramework::EMethodFlagBits::RpcUnreliable");
        }
    }

    const auto NetIt = Payload.Values.find("net");
    if (IncludeRpcBits && NetIt != Payload.Values.end())
    {
        if (NetIt->second == "server")
        {
            Bits.push_back("::SnAPI::GameFramework::EMethodFlagBits::RpcNetServer");
        }
        else if (NetIt->second == "client")
        {
            Bits.push_back("::SnAPI::GameFramework::EMethodFlagBits::RpcNetClient");
        }
        else if (NetIt->second == "multicast")
        {
            Bits.push_back("::SnAPI::GameFramework::EMethodFlagBits::RpcNetMulticast");
        }
    }

    if (Bits.empty())
    {
        return "{}";
    }

    std::string Result{};
    for (const std::string& Bit : Bits)
    {
        if (!Result.empty())
        {
            Result += " | ";
        }
        Result += Bit;
    }
    return Result;
}

std::string BuildRpcReliabilityFlagsExpression(const AnnotationPayload& Payload)
{
    const auto RpcIt = Payload.Values.find("rpc");
    if (RpcIt == Payload.Values.end())
    {
        return "::SnAPI::GameFramework::EMethodFlagBits::RpcReliable";
    }

    if (RpcIt->second == "unreliable")
    {
        return "::SnAPI::GameFramework::EMethodFlagBits::RpcUnreliable";
    }

    return "::SnAPI::GameFramework::EMethodFlagBits::RpcReliable";
}

std::string SanitizeIdentifier(std::string_view Value)
{
    std::string Result{};
    Result.reserve(Value.size());
    for (const char Ch : Value)
    {
        if ((std::isalnum(static_cast<unsigned char>(Ch)) != 0) || Ch == '_')
        {
            Result.push_back(Ch);
        }
        else
        {
            Result.push_back('_');
        }
    }
    if (Result.empty() || (std::isdigit(static_cast<unsigned char>(Result.front())) != 0))
    {
        Result.insert(Result.begin(), '_');
    }
    return Result;
}

std::string MethodPointerExpression(const std::string& OwnerQualifiedName, const CXCursor MethodCursor)
{
    const std::string MethodName = ToStringDispose(clang_getCursorSpelling(MethodCursor));
    const int ExceptionKind = clang_getCursorExceptionSpecificationType(MethodCursor);
    const bool IsNoexcept = ExceptionKind == CXCursor_ExceptionSpecificationKind_BasicNoexcept ||
                            ExceptionKind == CXCursor_ExceptionSpecificationKind_NoThrow;

    const CXType MethodType = clang_getCursorType(MethodCursor);
    const std::string ReturnType = PrettyPrintedTypeForCode(clang_getResultType(MethodType), MethodCursor);
    const int ArgCount = clang_Cursor_getNumArguments(MethodCursor);

    std::string ParamList{};
    for (int Index = 0; Index < ArgCount; ++Index)
    {
        const CXCursor ArgCursor = clang_Cursor_getArgument(MethodCursor, static_cast<unsigned>(Index));
        if (Index > 0)
        {
            ParamList += ", ";
        }
        ParamList += PrettyPrintedTypeForCode(clang_getCursorType(ArgCursor), ArgCursor);
    }

    std::string PointerType = ReturnType + " (" + OwnerQualifiedName + "::*" + ")(" + ParamList + ")";
    if (clang_CXXMethod_isConst(MethodCursor) != 0)
    {
        PointerType += " const";
    }
    if (IsNoexcept)
    {
        PointerType += " noexcept";
    }

    return "static_cast<" + PointerType + ">(&" + OwnerQualifiedName + "::" + MethodName + ")";
}

bool IsPublicInstanceMethod(const CXCursor Cursor)
{
    return clang_getCursorKind(Cursor) == CXCursor_CXXMethod &&
           clang_getCXXAccessSpecifier(Cursor) == CX_CXXPublic &&
           clang_CXXMethod_isStatic(Cursor) == 0;
}

CXType UnderlyingFieldType(const CXType Type)
{
    const CXType Canonical = clang_getCanonicalType(Type);
    if (Canonical.kind == CXType_LValueReference || Canonical.kind == CXType_RValueReference)
    {
        return clang_getPointeeType(Canonical);
    }
    return Canonical;
}

bool AreComparableTypesEqual(const CXType Left, const CXCursor LeftContext, const CXType Right, const CXCursor RightContext)
{
    return ComparableTypeKey(UnderlyingFieldType(Left), LeftContext) ==
           ComparableTypeKey(UnderlyingFieldType(Right), RightContext);
}

std::vector<CXCursor> CollectOwnerMethodsByName(const CXCursor OwnerCursor, const std::string_view Name)
{
    struct VisitorState
    {
        std::string_view Name{};
        std::vector<CXCursor> Methods{};
    } State{Name, {}};

    clang_visitChildren(
        OwnerCursor,
        [](CXCursor Child, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const StatePtr = static_cast<VisitorState*>(ClientData);
            if (clang_getCursorKind(Child) != CXCursor_CXXMethod)
            {
                return CXChildVisit_Continue;
            }

            if (ToStringDispose(clang_getCursorSpelling(Child)) == StatePtr->Name)
            {
                StatePtr->Methods.push_back(Child);
            }
            return CXChildVisit_Continue;
        },
        &State);

    return State.Methods;
}

CXCursor ResolveGetterMethod(const CXCursor OwnerCursor,
                             const std::string_view Name,
                             const CXType FieldType,
                             const bool RequireConst)
{
    for (const CXCursor Method : CollectOwnerMethodsByName(OwnerCursor, Name))
    {
        if (!IsPublicInstanceMethod(Method) || clang_Cursor_getNumArguments(Method) != 0)
        {
            continue;
        }

        if (RequireConst && clang_CXXMethod_isConst(Method) == 0)
        {
            continue;
        }

        const CXType ReturnType = clang_getResultType(clang_getCursorType(Method));
        if (ReturnType.kind == CXType_Void || !AreComparableTypesEqual(ReturnType, Method, FieldType, OwnerCursor))
        {
            continue;
        }

        return Method;
    }

    return clang_getNullCursor();
}

CXCursor ResolveSetterMethod(const CXCursor OwnerCursor, const std::string_view Name, const CXType FieldType)
{
    for (const CXCursor Method : CollectOwnerMethodsByName(OwnerCursor, Name))
    {
        if (!IsPublicInstanceMethod(Method) || clang_Cursor_getNumArguments(Method) != 1)
        {
            continue;
        }

        const CXCursor ArgCursor = clang_Cursor_getArgument(Method, 0);
        const CXType ArgType = clang_getCursorType(ArgCursor);
        if (!AreComparableTypesEqual(ArgType, ArgCursor, FieldType, OwnerCursor))
        {
            continue;
        }

        return Method;
    }

    return clang_getNullCursor();
}

bool ConfigureFieldFromFieldDecl(const CXCursor Cursor,
                                 const std::string& OwnerQualifiedName,
                                 const AnnotationPayload& Payload,
                                 FieldSpec& Out,
                                 std::vector<Diagnostic>& Diagnostics)
{
    const CXCursor OwnerCursor = clang_getCursorSemanticParent(Cursor);
    const CXType FieldType = clang_getCursorType(Cursor);
    const std::string FieldName = ToStringDispose(clang_getCursorSpelling(Cursor));
    const bool HasGetter = Payload.Values.contains("getter");
    const bool HasConstGetter = Payload.Values.contains("const_getter");
    const bool HasSetter = Payload.Values.contains("setter");

    if (!HasGetter && !HasConstGetter && !HasSetter)
    {
        if (clang_getCXXAccessSpecifier(Cursor) != CX_CXXPublic)
        {
            Diagnostics.push_back(MakeDiagnostic(
                Cursor,
                "SnField on a non-public data member requires SnGetter(...), SnConstGetter(...), and/or SnSetter(...)"));
            return false;
        }

        Out.Name = Payload.Values.contains("key") ? Payload.Values.at("key") : FieldName;
        Out.ConditionExpr = ReflectableTypeCondition(FieldType, Cursor);
        Out.BuilderExpr = "Builder.Field(" + CppStringLiteral(Out.Name) + ", " +
            FieldPointerExpression(OwnerQualifiedName, Cursor) + ", " + Out.FlagsExpr + ", " + Out.EditorFlagsExpr + ");";
        return true;
    }

    if (HasConstGetter && !HasGetter)
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnConstGetter(...) requires SnGetter(...) on data members"));
        return false;
    }

    const std::string StableName = Payload.Values.contains("key") ? Payload.Values.at("key") : FieldName;
    Out.Name = StableName;
    Out.ConditionExpr = ReflectableTypeCondition(FieldType, Cursor);

    CXCursor GetterCursor = clang_getNullCursor();
    CXCursor ConstGetterCursor = clang_getNullCursor();
    CXCursor SetterCursor = clang_getNullCursor();

    if (HasGetter)
    {
        GetterCursor = ResolveGetterMethod(OwnerCursor, Payload.Values.at("getter"), FieldType, false);
        if (clang_Cursor_isNull(GetterCursor))
        {
            Diagnostics.push_back(MakeDiagnostic(
                Cursor,
                "Failed to resolve public getter '" + Payload.Values.at("getter") + "' for SnField '" + StableName + "'"));
            return false;
        }
    }

    if (HasConstGetter)
    {
        ConstGetterCursor = ResolveGetterMethod(OwnerCursor, Payload.Values.at("const_getter"), FieldType, true);
        if (clang_Cursor_isNull(ConstGetterCursor))
        {
            Diagnostics.push_back(MakeDiagnostic(
                Cursor,
                "Failed to resolve public const getter '" + Payload.Values.at("const_getter") +
                    "' for SnField '" + StableName + "'"));
            return false;
        }
    }

    if (HasSetter)
    {
        SetterCursor = ResolveSetterMethod(OwnerCursor, Payload.Values.at("setter"), FieldType);
        if (clang_Cursor_isNull(SetterCursor))
        {
            Diagnostics.push_back(MakeDiagnostic(
                Cursor,
                "Failed to resolve public setter '" + Payload.Values.at("setter") + "' for SnField '" + StableName + "'"));
            return false;
        }
    }

    if (!clang_Cursor_isNull(GetterCursor) && !clang_Cursor_isNull(ConstGetterCursor) && clang_Cursor_isNull(SetterCursor))
    {
        Out.BuilderExpr = "Builder.Field(" + CppStringLiteral(Out.Name) + ", " +
            MethodPointerExpression(OwnerQualifiedName, GetterCursor) + ", " +
            MethodPointerExpression(OwnerQualifiedName, ConstGetterCursor) + ", " + Out.FlagsExpr + ", " + Out.EditorFlagsExpr + ");";
        return true;
    }

    if (!clang_Cursor_isNull(GetterCursor) && !clang_Cursor_isNull(SetterCursor) && clang_Cursor_isNull(ConstGetterCursor))
    {
        Out.BuilderExpr = "Builder.Field(" + CppStringLiteral(Out.Name) + ", " +
            MethodPointerExpression(OwnerQualifiedName, GetterCursor) + ", " +
            MethodPointerExpression(OwnerQualifiedName, SetterCursor) + ", " + Out.FlagsExpr + ", " + Out.EditorFlagsExpr + ");";
        return true;
    }

    if (!clang_Cursor_isNull(GetterCursor) && clang_Cursor_isNull(ConstGetterCursor) && clang_Cursor_isNull(SetterCursor))
    {
        Out.BuilderExpr = "Builder.Field(" + CppStringLiteral(Out.Name) + ", " +
            MethodPointerExpression(OwnerQualifiedName, GetterCursor) + ", " + Out.FlagsExpr + ", " + Out.EditorFlagsExpr + ");";
        return true;
    }

    if (clang_Cursor_isNull(GetterCursor) && clang_Cursor_isNull(ConstGetterCursor) && !clang_Cursor_isNull(SetterCursor))
    {
        Out.BuilderExpr = "Builder.Field(" + CppStringLiteral(Out.Name) + ", " +
            MethodPointerExpression(OwnerQualifiedName, SetterCursor) + ", " + Out.FlagsExpr + ", " + Out.EditorFlagsExpr + ");";
        return true;
    }

    Diagnostics.push_back(MakeDiagnostic(
        Cursor,
        "Unsupported SnField accessor combination on '" + StableName +
            "'. Use direct member access, getter-only, setter-only, getter+setter, or getter+const-getter."));
    return false;
}

bool ConfigureFieldFromMethod(const CXCursor Cursor,
                              const std::string& OwnerQualifiedName,
                              const AnnotationPayload& Payload,
                              FieldSpec& Out,
                              std::vector<Diagnostic>& Diagnostics)
{
    if (!IsPublicInstanceMethod(Cursor))
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnField on methods requires a public non-static member function"));
        return false;
    }

    const CXCursor OwnerCursor = clang_getCursorSemanticParent(Cursor);
    const std::string MethodName = ToStringDispose(clang_getCursorSpelling(Cursor));
    const int ArgCount = clang_Cursor_getNumArguments(Cursor);
    const bool HasGetter = Payload.Values.contains("getter");
    const bool HasConstGetter = Payload.Values.contains("const_getter");
    const bool HasSetter = Payload.Values.contains("setter");

    CXCursor GetterCursor = clang_getNullCursor();
    CXCursor ConstGetterCursor = clang_getNullCursor();
    CXCursor SetterCursor = clang_getNullCursor();
    CXType FieldType{};

    if (ArgCount == 0)
    {
        const CXType ReturnType = clang_getResultType(clang_getCursorType(Cursor));
        if (ReturnType.kind == CXType_Void)
        {
            Diagnostics.push_back(MakeDiagnostic(Cursor, "SnField getter methods must not return void"));
            return false;
        }
        GetterCursor = Cursor;
        FieldType = ReturnType;
    }
    else if (ArgCount == 1)
    {
        SetterCursor = Cursor;
        FieldType = clang_getCursorType(clang_Cursor_getArgument(Cursor, 0));
    }
    else
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnField methods must be getter-like (0 args) or setter-like (1 arg)"));
        return false;
    }

    if (HasGetter)
    {
        GetterCursor = ResolveGetterMethod(OwnerCursor, Payload.Values.at("getter"), FieldType, false);
        if (clang_Cursor_isNull(GetterCursor))
        {
            Diagnostics.push_back(MakeDiagnostic(
                Cursor,
                "Failed to resolve public getter '" + Payload.Values.at("getter") + "' for SnField '" + MethodName + "'"));
            return false;
        }
    }

    if (HasConstGetter)
    {
        ConstGetterCursor = ResolveGetterMethod(OwnerCursor, Payload.Values.at("const_getter"), FieldType, true);
        if (clang_Cursor_isNull(ConstGetterCursor))
        {
            Diagnostics.push_back(MakeDiagnostic(
                Cursor,
                "Failed to resolve public const getter '" + Payload.Values.at("const_getter") +
                    "' for SnField '" + MethodName + "'"));
            return false;
        }
    }

    if (HasSetter)
    {
        SetterCursor = ResolveSetterMethod(OwnerCursor, Payload.Values.at("setter"), FieldType);
        if (clang_Cursor_isNull(SetterCursor))
        {
            Diagnostics.push_back(MakeDiagnostic(
                Cursor,
                "Failed to resolve public setter '" + Payload.Values.at("setter") + "' for SnField '" + MethodName + "'"));
            return false;
        }
    }

    Out.Name = Payload.Values.contains("key") ? Payload.Values.at("key") : DerivePropertyNameFromMethodName(MethodName);
    Out.ConditionExpr = ReflectableTypeCondition(FieldType, Cursor);

    if (!clang_Cursor_isNull(GetterCursor) && !clang_Cursor_isNull(ConstGetterCursor) && clang_Cursor_isNull(SetterCursor))
    {
        Out.BuilderExpr = "Builder.Field(" + CppStringLiteral(Out.Name) + ", " +
            MethodPointerExpression(OwnerQualifiedName, GetterCursor) + ", " +
            MethodPointerExpression(OwnerQualifiedName, ConstGetterCursor) + ", " + Out.FlagsExpr + ", " + Out.EditorFlagsExpr + ");";
        return true;
    }

    if (!clang_Cursor_isNull(GetterCursor) && !clang_Cursor_isNull(SetterCursor) && clang_Cursor_isNull(ConstGetterCursor))
    {
        Out.BuilderExpr = "Builder.Field(" + CppStringLiteral(Out.Name) + ", " +
            MethodPointerExpression(OwnerQualifiedName, GetterCursor) + ", " +
            MethodPointerExpression(OwnerQualifiedName, SetterCursor) + ", " + Out.FlagsExpr + ", " + Out.EditorFlagsExpr + ");";
        return true;
    }

    if (!clang_Cursor_isNull(GetterCursor) && clang_Cursor_isNull(ConstGetterCursor) && clang_Cursor_isNull(SetterCursor))
    {
        Out.BuilderExpr = "Builder.Field(" + CppStringLiteral(Out.Name) + ", " +
            MethodPointerExpression(OwnerQualifiedName, GetterCursor) + ", " + Out.FlagsExpr + ", " + Out.EditorFlagsExpr + ");";
        return true;
    }

    if (clang_Cursor_isNull(GetterCursor) && clang_Cursor_isNull(ConstGetterCursor) && !clang_Cursor_isNull(SetterCursor))
    {
        Out.BuilderExpr = "Builder.Field(" + CppStringLiteral(Out.Name) + ", " +
            MethodPointerExpression(OwnerQualifiedName, SetterCursor) + ", " + Out.FlagsExpr + ", " + Out.EditorFlagsExpr + ");";
        return true;
    }

    Diagnostics.push_back(MakeDiagnostic(
        Cursor,
        "Unsupported SnField accessor combination on method '" + MethodName +
            "'. Use getter-only, setter-only, getter+setter, or getter+const-getter."));
    return false;
}

bool PopulateFieldSpec(const CXCursor Cursor,
                       const std::string& OwnerQualifiedName,
                       HeaderParseContext& Context,
                       FieldSpec& Out,
                       std::vector<Diagnostic>& Diagnostics)
{
    (void)OwnerQualifiedName;
    const ReflectionMarker* const Marker = MatchSourceMarker(
        *Context.Markers, *Context.MarkerCursors, Cursor, MarkerKind::Field, true);
    if (!Marker)
    {
        return false;
    }

    const AnnotationPayload& Payload = Marker->Payload;

    if (clang_Cursor_getStorageClass(Cursor) == CX_SC_Static)
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnField does not support static members"));
        return false;
    }

    if (!ValidatePayload(
            Payload,
            {"key", "display_name", "category", "getter", "const_getter", "setter", "rep", "min", "max", "step"},
            {"replicated", "serialized", "hidden", "read_only", "advanced", "heavy_data"},
            Cursor,
            Diagnostics))
    {
        return false;
    }

    if (const auto RepIt = Payload.Values.find("rep"); RepIt != Payload.Values.end() &&
        RepIt->second != "reliable" && RepIt->second != "unreliable")
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "Unsupported rep value '" + RepIt->second + "' on snapi.field"));
        return false;
    }

    const ParsedComment Comment = ExtractParsedComment(Cursor);
    Out.Name = Payload.Values.contains("key") ? Payload.Values.at("key") : ToStringDispose(clang_getCursorSpelling(Cursor));
    Out.DisplayName = Payload.Values.contains("display_name") ? Payload.Values.at("display_name") : "";
    Out.Category = Payload.Values.contains("category") ? Payload.Values.at("category") : "";
    Out.Doc = Comment.Doc;
    Out.FlagsExpr = BuildFieldFlagsExpression(Payload);
    Out.EditorFlagsExpr = BuildFieldEditorFlagsExpression(Payload);
    Out.MinExpr = Payload.Values.contains("min") ? Payload.Values.at("min") : "";
    Out.MaxExpr = Payload.Values.contains("max") ? Payload.Values.at("max") : "";
    Out.StepExpr = Payload.Values.contains("step") ? Payload.Values.at("step") : "";

    CXType FieldType = clang_getCursorType(Cursor);
    if (clang_getCursorKind(Cursor) == CXCursor_CXXMethod)
    {
        FieldType = (clang_Cursor_getNumArguments(Cursor) == 0)
            ? clang_getResultType(clang_getCursorType(Cursor))
            : clang_getCursorType(clang_Cursor_getArgument(Cursor, 0));
    }
    if (!IsTypeReflectionCompatible(FieldType, Cursor, *Context.Knowledge))
    {
        Diagnostics.push_back(MakeWarning(
            Cursor,
            "Skipping SnField '" + Out.Name + "' because field type '" +
                PrettyPrintedTypeForCode(FieldType, Cursor) + "' is not registered for reflection"));
        return false;
    }

    if (clang_getCursorKind(Cursor) == CXCursor_FieldDecl)
    {
        return ConfigureFieldFromFieldDecl(Cursor, OwnerQualifiedName, Payload, Out, Diagnostics);
    }
    if (clang_getCursorKind(Cursor) == CXCursor_CXXMethod)
    {
        return ConfigureFieldFromMethod(Cursor, OwnerQualifiedName, Payload, Out, Diagnostics);
    }

    Diagnostics.push_back(MakeDiagnostic(Cursor, "SnField is only supported on data members and member functions"));
    return false;
}

bool PopulateMethodSpec(const CXCursor Cursor,
                        const std::string& OwnerQualifiedName,
                        TypeSpec& OwnerSpec,
                        HeaderParseContext& Context,
                        MethodSpec& Out,
                        std::vector<Diagnostic>& Diagnostics)
{
    const ReflectionMarker* const Marker = MatchSourceMarker(
        *Context.Markers, *Context.MarkerCursors, Cursor, MarkerKind::Function, true);
    if (!Marker)
    {
        return false;
    }

    const AnnotationPayload& Payload = Marker->Payload;

    if (clang_getCXXAccessSpecifier(Cursor) != CX_CXXPublic)
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnFunction is currently supported only on public member functions"));
        return false;
    }

    if (clang_CXXMethod_isStatic(Cursor) != 0)
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnFunction does not support static member functions"));
        return false;
    }

    if (!ValidatePayload(Payload, {"key", "display_name", "category", "rpc", "net"}, {"editor_action"}, Cursor, Diagnostics))
    {
        return false;
    }

    if (const auto RpcIt = Payload.Values.find("rpc");
        RpcIt != Payload.Values.end() && RpcIt->second != "reliable" && RpcIt->second != "unreliable")
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "Unsupported rpc value '" + RpcIt->second + "' on snapi.function"));
        return false;
    }

    if (const auto NetIt = Payload.Values.find("net");
        NetIt != Payload.Values.end() &&
        NetIt->second != "server" &&
        NetIt->second != "client" &&
        NetIt->second != "multicast")
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "Unsupported net value '" + NetIt->second + "' on snapi.function"));
        return false;
    }

    const ParsedComment Comment = ExtractParsedComment(Cursor);
    const std::string MethodIdentifier = ToStringDispose(clang_getCursorSpelling(Cursor));
    Out.Name = Payload.Values.contains("key") ? Payload.Values.at("key") : MethodIdentifier;
    Out.PointerExpr = MethodPointerExpression(OwnerQualifiedName, Cursor);
    Out.DisplayName = Payload.Values.contains("display_name") ? Payload.Values.at("display_name") : "";
    Out.Category = Payload.Values.contains("category") ? Payload.Values.at("category") : "";
    Out.Doc = Comment.Doc;

    const bool HasRpcMetadata = Payload.Values.contains("rpc") || Payload.Values.contains("net");
    if (HasRpcMetadata)
    {
        if (!(OwnerSpec.IsNodeLike || OwnerSpec.IsComponentLike))
        {
            Diagnostics.push_back(MakeDiagnostic(
                Cursor,
                "SnRpc(...) is only supported on methods declared in types derived from "
                "SnAPI::GameFramework::BaseNode or SnAPI::GameFramework::BaseComponent"));
            return false;
        }
        if (OwnerSpec.GeneratedLine == 0)
        {
            Diagnostics.push_back(MakeDiagnostic(
                Cursor,
                "SnRpc(...) requires the enclosing type to declare SnGenerated() so RPC trampolines can be injected"));
            return false;
        }
    }
    const bool UseGeneratedRpc = HasRpcMetadata;
    if (UseGeneratedRpc)
    {
        if (Payload.Values.find("net") == Payload.Values.end())
        {
            Diagnostics.push_back(MakeDiagnostic(Cursor, "SnRpc(...) requires one network target token"));
            return false;
        }
        if (clang_CXXMethod_isConst(Cursor) != 0)
        {
            Diagnostics.push_back(MakeDiagnostic(Cursor, "SnRpc(...) does not yet support const member functions"));
            return false;
        }
        if (clang_isCursorDefinition(Cursor) != 0)
        {
            Diagnostics.push_back(MakeDiagnostic(
                Cursor,
                "SnRpc(...) methods must be declarations only; implement the generated <MethodName>Impl() instead"));
            return false;
        }
    }

    Out.FlagsExpr = BuildMethodFlagsExpression(Payload, !UseGeneratedRpc);

    std::vector<std::string> CompatibilityIssues{};
    const CXType ResultType = clang_getResultType(clang_getCursorType(Cursor));
    if (UseGeneratedRpc && clang_getCanonicalType(ResultType).kind != CXType_Void)
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnRpc(...) is currently supported only on void member functions"));
        return false;
    }
    if (!IsTypeReflectionCompatible(ResultType, Cursor, *Context.Knowledge))
    {
        CompatibilityIssues.push_back("return type '" + PrettyPrintedTypeForCode(ResultType, Cursor) + "'");
    }

    const int ArgCount = clang_Cursor_getNumArguments(Cursor);
    Out.Params.reserve(static_cast<std::size_t>(std::max(ArgCount, 0)));
    std::vector<std::string> Conditions{};
    Conditions.reserve(static_cast<std::size_t>(std::max(ArgCount, 0)) + 1);
    Conditions.push_back(ReflectableTypeCondition(ResultType, Cursor));
    for (int Index = 0; Index < ArgCount; ++Index)
    {
        const CXCursor ArgCursor = clang_Cursor_getArgument(Cursor, static_cast<unsigned>(Index));
        const CXType ArgType = clang_getCursorType(ArgCursor);
        ParamSpec Param{};
        Param.Type = PrettyPrintedTypeForCode(ArgType, ArgCursor);
        Param.Name = ToStringDispose(clang_getCursorSpelling(ArgCursor));
        const auto DocIt = Comment.ParamDocs.find(Param.Name);
        if (DocIt != Comment.ParamDocs.end())
        {
            Param.Doc = DocIt->second;
        }
        Out.Params.push_back(std::move(Param));
        if (!IsTypeReflectionCompatible(ArgType, ArgCursor, *Context.Knowledge))
        {
            const std::string ParamName = Out.Params.back().Name.empty() ? ("#" + std::to_string(Index)) : ("'" + Out.Params.back().Name + "'");
            CompatibilityIssues.push_back("parameter " + ParamName + " type '" + Out.Params.back().Type + "'");
        }
        Conditions.push_back(ReflectableTypeCondition(ArgType, ArgCursor));
    }

    if (!CompatibilityIssues.empty())
    {
        std::string Message = "Skipping SnFunction '" + Out.Name + "' because ";
        for (std::size_t Index = 0; Index < CompatibilityIssues.size(); ++Index)
        {
            if (Index > 0)
            {
                Message += "; ";
            }
            Message += CompatibilityIssues[Index];
            Message += " is not registered for reflection";
        }
        Diagnostics.push_back(MakeWarning(Cursor, std::move(Message)));
        return false;
    }

    Out.ConditionExpr = CombineConditions(Conditions);

    if (UseGeneratedRpc)
    {
        GeneratedRpcSpec Rpc{};
        unsigned MethodLine = 0;
        (void)CursorExtentStartOffset(Cursor, &MethodLine, nullptr);
        Rpc.SourceLine = MethodLine;
        Rpc.PublicName = MethodIdentifier;
        Rpc.ImplName = MethodIdentifier + "Impl";
        Rpc.Params = Out.Params;
        Rpc.ReliabilityFlagsExpr = BuildRpcReliabilityFlagsExpression(Payload);

        const std::string SymbolStem = SanitizeIdentifier(MethodIdentifier) + "_" + std::to_string(Rpc.SourceLine);
        const std::string NetKind = Payload.Values.at("net");
        if (NetKind == "server")
        {
            Rpc.Kind = GeneratedRpcKind::Server;
            Rpc.ServerEntryName = "__SnRpc_" + SymbolStem + "_Server";
            Rpc.ServerEntryAccessor = "__SnGeneratedPtr_" + SymbolStem + "_Server";
        }
        else if (NetKind == "client")
        {
            Rpc.Kind = GeneratedRpcKind::Client;
            Rpc.ServerEntryName = "__SnRpc_" + SymbolStem + "_ServerIngress";
            Rpc.ServerEntryAccessor = "__SnGeneratedPtr_" + SymbolStem + "_ServerIngress";
            Rpc.ClientEntryName = "__SnRpc_" + SymbolStem + "_Client";
            Rpc.ClientEntryAccessor = "__SnGeneratedPtr_" + SymbolStem + "_Client";
        }
        else
        {
            Rpc.Kind = GeneratedRpcKind::Multicast;
            Rpc.ServerEntryName = "__SnRpc_" + SymbolStem + "_ServerIngress";
            Rpc.ServerEntryAccessor = "__SnGeneratedPtr_" + SymbolStem + "_ServerIngress";
            Rpc.ClientEntryName = "__SnRpc_" + SymbolStem + "_Multicast";
            Rpc.ClientEntryAccessor = "__SnGeneratedPtr_" + SymbolStem + "_Multicast";
        }

        OwnerSpec.GeneratedRpcs.push_back(std::move(Rpc));
    }

    return true;
}

std::optional<ConstructorSpec> DefaultConstructorSpecFor(const CXCursor Cursor)
{
    if (clang_getCXXAccessSpecifier(Cursor) != CX_CXXPublic || clang_CXXConstructor_isDefaultConstructor(Cursor) == 0)
    {
        return std::nullopt;
    }

    const ParsedComment Comment = ExtractParsedComment(Cursor);
    ConstructorSpec Spec{};
    Spec.Doc = Comment.Doc;
    return Spec;
}

bool CursorDeclaresNativeTypeNameMember(const CXCursor Cursor)
{
    bool Result = false;
    clang_visitChildren(
        Cursor,
        [](CXCursor Child, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const HasTypeName = static_cast<bool*>(ClientData);
            if (clang_getCursorKind(Child) == CXCursor_VarDecl &&
                ToStringDispose(clang_getCursorSpelling(Child)) == "kTypeName")
            {
                *HasTypeName = true;
                return CXChildVisit_Break;
            }

            return CXChildVisit_Continue;
        },
        &Result);
    return Result;
}

TypeSpec BuildRecordSpec(const fs::path& Header,
                         const CXCursor Cursor,
                         const AnnotationPayload& TypePayload,
                         const std::string_view QualifiedNameOverride,
                         const std::string_view ReflectedNameOverride,
                         HeaderParseContext& Context,
                         std::vector<Diagnostic>& Diagnostics)
{
    TypeSpec Spec{};
    Spec.Header = Header;
    Spec.DeclName = ToStringDispose(clang_getCursorSpelling(Cursor));
    Spec.QualifiedName = QualifiedNameOverride.empty() ? QualifiedNameForCursor(Cursor) : std::string(QualifiedNameOverride);
    Spec.ReflectedName = ReflectedNameOverride.empty()
                             ? NormalizeReflectedTypeNameString(Spec.QualifiedName)
                             : std::string(ReflectedNameOverride);
    Spec.IsInterface = false;
    Spec.HasNativeTypeNameMember = CursorDeclaresNativeTypeNameMember(Cursor);
    Spec.HasDefaultConstructor = false;
    Spec.IsNodeLike = CursorIsDerivedFromQualifiedName(Cursor, "SnAPI::GameFramework::BaseNode");
    Spec.IsComponentLike = CursorIsDerivedFromQualifiedName(Cursor, "SnAPI::GameFramework::BaseComponent");
    if (const ReflectionMarker* const GeneratedMarker =
            MatchGeneratedMarker(*Context.Markers, *Context.MarkerCursors, Cursor, true))
    {
        Spec.GeneratedLine = GeneratedMarker->Line;
    }

    if (!ValidatePayload(TypePayload, {"display_name", "category"}, {"interface", "template"}, Cursor, Diagnostics))
    {
        return Spec;
    }
    Spec.DisplayName = TypePayload.Values.contains("display_name") ? TypePayload.Values.at("display_name") : "";
    Spec.Category = TypePayload.Values.contains("category") ? TypePayload.Values.at("category") : "";
    Spec.IsInterface = TypePayload.Flags.contains("interface");
    Spec.HasHeaderVisibleTypeName = Context.Knowledge->HeaderVisibleTypeKeys.contains(Spec.ReflectedName);
    Spec.NeedsGeneratedTypeName =
        Spec.GeneratedLine == 0 &&
        !Spec.HasNativeTypeNameMember &&
        !Spec.HasHeaderVisibleTypeName;

    if (Spec.GeneratedLine != 0 && TypePayload.Flags.contains("template"))
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnGenerated() is not yet supported on reflection template primaries"));
        Spec.GeneratedLine = 0;
    }

    Spec.Doc = ExtractParsedComment(Cursor).Doc;

    struct BaseVisitorState
    {
        TypeSpec* Spec = nullptr;
        HeaderParseContext* Context = nullptr;
        std::vector<Diagnostic>* Diagnostics = nullptr;
    } BaseState{&Spec, &Context, &Diagnostics};

    clang_visitChildren(
        Cursor,
        [](CXCursor Child, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const State = static_cast<BaseVisitorState*>(ClientData);
            switch (clang_getCursorKind(Child))
            {
            case CXCursor_CXXBaseSpecifier:
            {
                if (clang_getCXXAccessSpecifier(Child) == CX_CXXPublic)
                {
                    BaseSpec Base{};
                    const CXType BaseType = clang_getCursorType(Child);
                    Base.TypeExpr = PrettyPrintedTypeForCode(BaseType, Child);
                    Base.ConditionExpr = ReflectableTypeCondition(BaseType, Child);
                    if (!Base.TypeExpr.empty())
                    {
                        if (!IsTypeReflectionCompatible(BaseType, Child, *State->Context->Knowledge))
                        {
                            State->Diagnostics->push_back(MakeWarning(
                                Child,
                                "Skipping reflected base '" + Base.TypeExpr +
                                    "' because it is not registered for reflection"));
                            break;
                        }
                        State->Spec->Bases.push_back(std::move(Base));
                    }
                }
                break;
            }
            default:
                break;
            }
            return CXChildVisit_Continue;
        },
        &BaseState);

    struct MemberVisitorState
    {
        const std::string* OwnerQualifiedName = nullptr;
        TypeSpec* Spec = nullptr;
        std::vector<Diagnostic>* Diagnostics = nullptr;
        HeaderParseContext* Context = nullptr;
    } MemberState{&Spec.QualifiedName, &Spec, &Diagnostics, &Context};

    clang_visitChildren(
        Cursor,
        [](CXCursor Child, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const State = static_cast<MemberVisitorState*>(ClientData);
            switch (clang_getCursorKind(Child))
            {
            case CXCursor_FieldDecl:
            {
                FieldSpec Field{};
                if (PopulateFieldSpec(Child, *State->OwnerQualifiedName, *State->Context, Field, *State->Diagnostics))
                {
                    State->Spec->Fields.push_back(std::move(Field));
                }
                break;
            }
            case CXCursor_CXXMethod:
            {
                FieldSpec Field{};
                if (PopulateFieldSpec(Child, *State->OwnerQualifiedName, *State->Context, Field, *State->Diagnostics))
                {
                    State->Spec->Fields.push_back(std::move(Field));
                }
                MethodSpec Method{};
                if (PopulateMethodSpec(Child,
                                       *State->OwnerQualifiedName,
                                       *State->Spec,
                                       *State->Context,
                                       Method,
                                       *State->Diagnostics))
                {
                    State->Spec->Methods.push_back(std::move(Method));
                }
                break;
            }
            case CXCursor_Constructor:
            {
                if (!State->Spec->DefaultConstructor)
                {
                    State->Spec->DefaultConstructor = DefaultConstructorSpecFor(Child);
                }
                break;
            }
            default:
                break;
            }
            return CXChildVisit_Continue;
        },
        &MemberState);

    return Spec;
}

TypeSpec BuildEnumSpec(const fs::path& Header,
                       const CXCursor Cursor,
                       const AnnotationPayload& TypePayload,
                       const std::string_view QualifiedNameOverride,
                       const std::string_view ReflectedNameOverride,
                       HeaderParseContext& Context,
                       std::vector<Diagnostic>& Diagnostics)
{
    TypeSpec Spec{};
    Spec.Header = Header;
    Spec.IsEnum = true;
    Spec.QualifiedName = QualifiedNameOverride.empty() ? QualifiedNameForCursor(Cursor) : std::string(QualifiedNameOverride);
    Spec.ReflectedName = ReflectedNameOverride.empty()
                             ? NormalizeReflectedTypeNameString(Spec.QualifiedName)
                             : std::string(ReflectedNameOverride);
    Spec.Doc = ExtractParsedComment(Cursor).Doc;

    if (!ValidatePayload(TypePayload, {"display_name", "category"}, {"template"}, Cursor, Diagnostics))
    {
        return Spec;
    }
    Spec.DisplayName = TypePayload.Values.contains("display_name") ? TypePayload.Values.at("display_name") : "";
    Spec.Category = TypePayload.Values.contains("category") ? TypePayload.Values.at("category") : "";
    Spec.HasHeaderVisibleTypeName = Context.Knowledge->HeaderVisibleTypeKeys.contains(Spec.ReflectedName);
    Spec.NeedsGeneratedTypeName = !Spec.HasHeaderVisibleTypeName;

    const CXType Underlying = clang_getEnumDeclIntegerType(Cursor);
    Spec.EnumIsSigned = IsSignedIntegerType(Underlying);

    struct EnumVisitorState
    {
        TypeSpec* Spec = nullptr;
        std::vector<Diagnostic>* Diagnostics = nullptr;
        HeaderParseContext* Context = nullptr;
    } EnumState{&Spec, &Diagnostics, &Context};
    clang_visitChildren(
        Cursor,
        [](CXCursor Child, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const State = static_cast<EnumVisitorState*>(ClientData);
            TypeSpec* const SpecPtr = State->Spec;
            std::vector<Diagnostic>* const DiagnosticsPtr = State->Diagnostics;
            if (clang_getCursorKind(Child) != CXCursor_EnumConstantDecl)
            {
                return CXChildVisit_Continue;
            }

            EnumValueSpec Value{};
            Value.Name = ToStringDispose(clang_getCursorSpelling(Child));
            Value.QualifiedValueExpr = SpecPtr->QualifiedName + "::" + Value.Name;
            Value.Doc = ExtractParsedComment(Child).Doc;
            if (const ReflectionMarker* const Marker = MatchSourceMarker(
                    *State->Context->Markers, *State->Context->MarkerCursors, Child, MarkerKind::EnumValue, false))
            {
                if (!ValidatePayload(Marker->Payload, {"key", "display_name"}, {}, Child, *DiagnosticsPtr))
                {
                    return CXChildVisit_Continue;
                }
                (void)MatchSourceMarker(
                    *State->Context->Markers, *State->Context->MarkerCursors, Child, MarkerKind::EnumValue, true);
                Value.Name = Marker->Payload.Values.contains("key")
                                 ? Marker->Payload.Values.at("key")
                                 : Value.Name;
                Value.DisplayName = Marker->Payload.Values.contains("display_name")
                                        ? Marker->Payload.Values.at("display_name")
                                        : "";
            }

            SpecPtr->EnumValues.push_back(std::move(Value));
            return CXChildVisit_Continue;
        },
        &EnumState);

    return Spec;
}

std::vector<std::string> TemplateParameterNames(const CXCursor Cursor)
{
    struct VisitorState
    {
        std::vector<std::string> Parameters{};
    } State{};

    clang_visitChildren(
        Cursor,
        [](CXCursor Child, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const StatePtr = static_cast<VisitorState*>(ClientData);
            if (clang_getCursorKind(Child) == CXCursor_TemplateTypeParameter)
            {
                const std::string Name = ToStringDispose(clang_getCursorSpelling(Child));
                if (!Name.empty())
                {
                    StatePtr->Parameters.push_back(Name);
                }
            }
            return CXChildVisit_Continue;
        },
        &State);

    return State.Parameters;
}

std::string ReplaceAllExact(std::string Value, const std::string_view Needle, const std::string_view Replacement)
{
    if (Needle.empty())
    {
        return Value;
    }

    std::size_t Pos = 0;
    while ((Pos = Value.find(Needle, Pos)) != std::string::npos)
    {
        Value.replace(Pos, Needle.size(), Replacement);
        Pos += Replacement.size();
    }
    return Value;
}

std::string SubstituteIdentifierTokens(std::string_view Value,
                                       const std::vector<std::pair<std::string, std::string>>& Replacements)
{
    if (Replacements.empty())
    {
        return std::string(Value);
    }

    std::string Result{};
    Result.reserve(Value.size());

    std::size_t Pos = 0;
    while (Pos < Value.size())
    {
        const char Ch = Value[Pos];
        if (Ch == '"' || Ch == '\'')
        {
            const char Quote = Ch;
            Result.push_back(Ch);
            ++Pos;
            while (Pos < Value.size())
            {
                const char Inner = Value[Pos++];
                Result.push_back(Inner);
                if (Inner == '\\' && Pos < Value.size())
                {
                    Result.push_back(Value[Pos++]);
                    continue;
                }
                if (Inner == Quote)
                {
                    break;
                }
            }
            continue;
        }

        if (!IsIdentifierStart(Ch))
        {
            Result.push_back(Ch);
            ++Pos;
            continue;
        }

        const std::size_t Start = Pos;
        ++Pos;
        while (Pos < Value.size() && IsIdentifierContinue(Value[Pos]))
        {
            ++Pos;
        }

        const std::string Token(Value.substr(Start, Pos - Start));
        const auto It = std::find_if(
            Replacements.begin(), Replacements.end(), [&Token](const std::pair<std::string, std::string>& Entry) {
                return Entry.first == Token;
            });
        if (It != Replacements.end())
        {
            Result += It->second;
        }
        else
        {
            Result += Token;
        }
    }

    return Result;
}

TypeSpec ExpandTemplatePrototype(const TypeSpec& Prototype,
                                 const std::string_view PrimaryQualifiedName,
                                 const TemplateSpecializationCandidate& Candidate)
{
    auto Expand = [&](const std::string& Value) -> std::string {
        std::string Result = ReplaceAllExact(Value, PrimaryQualifiedName, Candidate.QualifiedName);
        return SubstituteIdentifierTokens(Result, Candidate.Substitutions);
    };

    TypeSpec Result = Prototype;
    Result.QualifiedName = Candidate.QualifiedName;
    Result.ReflectedName = Candidate.ReflectedName;
    Result.TypeNameHeader = Candidate.File;

    for (BaseSpec& Base : Result.Bases)
    {
        Base.TypeExpr = Expand(Base.TypeExpr);
        Base.ConditionExpr = Expand(Base.ConditionExpr);
    }

    for (FieldSpec& Field : Result.Fields)
    {
        Field.ConditionExpr = Expand(Field.ConditionExpr);
        Field.BuilderExpr = Expand(Field.BuilderExpr);
        Field.MinExpr = Expand(Field.MinExpr);
        Field.MaxExpr = Expand(Field.MaxExpr);
        Field.StepExpr = Expand(Field.StepExpr);
    }

    for (MethodSpec& Method : Result.Methods)
    {
        Method.PointerExpr = Expand(Method.PointerExpr);
        Method.ConditionExpr = Expand(Method.ConditionExpr);
    }

    for (GeneratedRpcSpec& Rpc : Result.GeneratedRpcs)
    {
        for (ParamSpec& Param : Rpc.Params)
        {
            Param.Type = Expand(Param.Type);
        }
    }

    return Result;
}

void InitializeMarkerCursors(const std::unordered_map<std::string, HeaderMarkers>& Markers,
                             std::unordered_map<std::string, HeaderMarkerCursor>& Cursors)
{
    Cursors.clear();
    for (const auto& [FileKey, FileMarkers] : Markers)
    {
        HeaderMarkerCursor Cursor{};
        Cursor.UsedType.assign(FileMarkers.Type.size(), 0);
        Cursor.UsedField.assign(FileMarkers.Field.size(), 0);
        Cursor.UsedFunction.assign(FileMarkers.Function.size(), 0);
        Cursor.UsedEnumValue.assign(FileMarkers.EnumValue.size(), 0);
        Cursor.UsedGenerated.assign(FileMarkers.Generated.size(), 0);
        Cursors.emplace(FileKey, std::move(Cursor));
    }
}

std::vector<AnnotatedDeclaration> CollectAnnotatedDeclarations(
    CXTranslationUnit Unit,
    const std::unordered_set<std::string>& HeaderKeys,
    const std::unordered_map<std::string, HeaderMarkers>& Markers,
    std::unordered_map<std::string, HeaderMarkerCursor>& MarkerCursors,
    std::unordered_set<std::string>& OutConcreteKeys,
    std::vector<Diagnostic>& Diagnostics)
{
    struct VisitorState
    {
        const std::unordered_set<std::string>* HeaderKeys = nullptr;
        const std::unordered_map<std::string, HeaderMarkers>* Markers = nullptr;
        std::unordered_map<std::string, HeaderMarkerCursor>* MarkerCursors = nullptr;
        std::unordered_set<std::string>* OutConcreteKeys = nullptr;
        std::vector<Diagnostic>* Diagnostics = nullptr;
        std::vector<AnnotatedDeclaration> Declarations{};
    } State{&HeaderKeys, &Markers, &MarkerCursors, &OutConcreteKeys, &Diagnostics, {}};

    clang_visitChildren(
        clang_getTranslationUnitCursor(Unit),
        [](CXCursor Cursor, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const StatePtr = static_cast<VisitorState*>(ClientData);
            if (!CursorIsFromTrackedHeaders(Cursor, *StatePtr->HeaderKeys))
            {
                return CXChildVisit_Recurse;
            }

            const CXCursorKind Kind = clang_getCursorKind(Cursor);
            const bool IsRecordKind = Kind == CXCursor_StructDecl || Kind == CXCursor_ClassDecl;
            const bool IsSupportedKind = IsRecordKind || Kind == CXCursor_EnumDecl || Kind == CXCursor_ClassTemplate;
            if (!IsSupportedKind || clang_isCursorDefinition(Cursor) == 0)
            {
                return CXChildVisit_Recurse;
            }

            const ReflectionMarker* const Marker = MatchSourceMarker(
                *StatePtr->Markers, *StatePtr->MarkerCursors, Cursor, MarkerKind::Type, true);
            if (!Marker)
            {
                return CXChildVisit_Recurse;
            }

            const bool WantsTemplate = Marker->Payload.Flags.contains("template");
            if (Kind == CXCursor_ClassTemplate)
            {
                if (!WantsTemplate)
                {
                    StatePtr->Diagnostics->push_back(MakeDiagnostic(
                        Cursor,
                        "SnType on a class template requires SnTemplate so the generator knows to expand specializations"));
                    return CXChildVisit_Continue;
                }

                StatePtr->Declarations.push_back(AnnotatedDeclaration{
                    .Header = CursorFilePath(Cursor),
                    .Cursor = Cursor,
                    .Payload = Marker->Payload,
                    .IsTemplate = true,
                });
                return CXChildVisit_Recurse;
            }

            if (WantsTemplate)
            {
                StatePtr->Diagnostics->push_back(MakeDiagnostic(
                    Cursor,
                    "SnTemplate is only valid on class templates"));
                return CXChildVisit_Continue;
            }

            StatePtr->Declarations.push_back(AnnotatedDeclaration{
                .Header = CursorFilePath(Cursor),
                .Cursor = Cursor,
                .Payload = Marker->Payload,
                .IsTemplate = false,
            });

            const std::string Key = NormalizeTypeExpressionString(QualifiedNameForCursor(Cursor));
            if (!Key.empty())
            {
                StatePtr->OutConcreteKeys->insert(Key);
            }
            return CXChildVisit_Recurse;
        },
        &State);

    return State.Declarations;
}

struct TemplateExpressionProbeGroup
{
    std::string FileKey{};
    fs::path IncludedFile{};
    std::vector<std::pair<std::size_t, ScopedTypeExpression>> Expressions{};
};

struct TemplateExpressionProbeGroupResult
{
    bool Parsed = false;
    std::vector<TemplateSpecializationCandidate> Candidates{};
};

std::vector<TemplateExpressionProbeGroup> GroupTemplateProbeExpressions(
    const std::vector<ScopedTypeExpression>& Expressions)
{
    std::vector<TemplateExpressionProbeGroup> Groups{};
    std::unordered_map<std::string, std::size_t> GroupIndices{};
    GroupIndices.reserve(Expressions.size());

    for (std::size_t Index = 0; Index < Expressions.size(); ++Index)
    {
        const ScopedTypeExpression& Expression = Expressions[Index];
        const std::string FileKey = NormalizePathKey(Expression.File);
        const auto [It, Inserted] = GroupIndices.emplace(FileKey, Groups.size());
        if (Inserted)
        {
            Groups.push_back(TemplateExpressionProbeGroup{
                .FileKey = FileKey,
                .IncludedFile = Expression.File,
                .Expressions = {},
            });
        }

        Groups[It->second].Expressions.push_back({Index, Expression});
    }

    return Groups;
}

CXType NormalizeTemplateSpecializationType(const CXType Input)
{
    if (Input.kind == CXType_Invalid)
    {
        return Input;
    }

    CXType Result = clang_getCanonicalType(Input);
    if (Result.kind == CXType_Invalid)
    {
        Result = Input;
    }

    while (Result.kind == CXType_LValueReference || Result.kind == CXType_RValueReference)
    {
        Result = clang_getPointeeType(Result);
        if (Result.kind == CXType_Invalid)
        {
            return Result;
        }
    }

    Result = clang_getUnqualifiedType(Result);
    const CXType Canonical = clang_getCanonicalType(Result);
    if (Canonical.kind != CXType_Invalid)
    {
        Result = Canonical;
    }
    return Result;
}

std::string NormalizeTemplateCandidateQualifiedName(std::string Value)
{
    return StripTopLevelCvRefFromTypeExpression(std::move(Value));
}

void UpsertTemplateSpecializationCandidate(
    TemplateSpecializationCandidate Candidate,
    std::vector<TemplateSpecializationCandidate>& Candidates,
    std::unordered_map<std::string, std::size_t>& CandidateIndices)
{
    const std::string OriginalQualifiedName = Candidate.QualifiedName;
    const std::string OriginalDefaultReflectedName = NormalizeReflectedTypeNameString(OriginalQualifiedName);

    Candidate.QualifiedName = NormalizeTemplateCandidateQualifiedName(std::move(Candidate.QualifiedName));
    const std::string DefaultReflectedName = NormalizeReflectedTypeNameString(Candidate.QualifiedName);
    if (Candidate.ReflectedName.empty() || Candidate.ReflectedName == OriginalDefaultReflectedName)
    {
        Candidate.ReflectedName = DefaultReflectedName;
    }

    const std::string CandidateKey = NormalizeTypeExpressionString(Candidate.QualifiedName);
    if (CandidateKey.empty())
    {
        return;
    }

    const auto It = CandidateIndices.find(CandidateKey);
    if (It == CandidateIndices.end())
    {
        CandidateIndices.emplace(CandidateKey, Candidates.size());
        Candidates.push_back(std::move(Candidate));
        return;
    }

    TemplateSpecializationCandidate& Existing = Candidates[It->second];
    const std::string ExistingDefaultName = NormalizeReflectedTypeNameString(Existing.QualifiedName);
    const std::string CandidateDefaultName = NormalizeReflectedTypeNameString(Candidate.QualifiedName);
    if (!Candidate.ReflectedName.empty() &&
        Candidate.ReflectedName != CandidateDefaultName &&
        (Existing.ReflectedName.empty() || Existing.ReflectedName == ExistingDefaultName))
    {
        Existing.ReflectedName = Candidate.ReflectedName;
        Existing.File = Candidate.File;
        Existing.Line = Candidate.Line;
        Existing.Column = Candidate.Column;
    }
}

void CollectTemplateSpecializationsFromType(
    const CXType InputType,
    const CXCursor ContextCursor,
    const std::vector<AnnotatedDeclaration>& TemplateDeclarations,
    std::vector<TemplateSpecializationCandidate>& Candidates,
    std::unordered_map<std::string, std::size_t>& CandidateIndices,
    const std::optional<std::string_view>& ReflectedNameOverride = std::nullopt)
{
    const auto BuildCandidate =
        [&](const CXType Input, const std::optional<std::string_view>& Override) -> std::optional<TemplateSpecializationCandidate> {
        const auto QualifiedTypeExpr = [&](const CXType Type) -> std::string {
            if (const auto ExplicitKey = FullyQualifiedTemplateSpecializationKey(Type, ContextCursor))
            {
                return *ExplicitKey;
            }
            return PrettyPrintedTypeForCode(Type, ContextCursor);
        };

        if (Input.kind == CXType_Invalid)
        {
            return std::nullopt;
        }

        const CXType Effective = NormalizeTemplateSpecializationType(Input);
        const CXCursor TypeDecl = clang_getTypeDeclaration(Effective);
        if (clang_Cursor_isNull(TypeDecl))
        {
            return std::nullopt;
        }

        const CXCursor PrimaryTemplate = clang_getSpecializedCursorTemplate(TypeDecl);
        if (clang_Cursor_isNull(PrimaryTemplate))
        {
            return std::nullopt;
        }

        std::optional<std::size_t> TemplateIndex{};
        for (std::size_t Index = 0; Index < TemplateDeclarations.size(); ++Index)
        {
            if (!TemplateDeclarations[Index].IsTemplate)
            {
                continue;
            }

            if (clang_equalCursors(PrimaryTemplate, TemplateDeclarations[Index].Cursor) != 0)
            {
                TemplateIndex = Index;
                break;
            }

            const std::string TemplateQualifiedName = QualifiedNameForCursor(TemplateDeclarations[Index].Cursor);
            const std::string QualifiedName =
                NormalizeTemplateCandidateQualifiedName(QualifiedTypeExpr(Effective));
            if (!TemplateQualifiedName.empty() &&
                !QualifiedName.empty() &&
                QualifiedName.starts_with(TemplateQualifiedName) &&
                QualifiedName.size() > TemplateQualifiedName.size() &&
                QualifiedName[TemplateQualifiedName.size()] == '<')
            {
                TemplateIndex = Index;
                break;
            }
        }

        if (!TemplateIndex)
        {
            return std::nullopt;
        }

        const std::string QualifiedName =
            NormalizeTemplateCandidateQualifiedName(QualifiedTypeExpr(Effective));
        const std::string DefaultReflectedName = NormalizeReflectedTypeNameString(QualifiedName);
        if (QualifiedName.empty() || DefaultReflectedName.empty())
        {
            return std::nullopt;
        }

        TemplateSpecializationCandidate Candidate{};
        Candidate.TemplateIndex = *TemplateIndex;
        Candidate.File = CursorFilePath(ContextCursor);
        {
            CXFile File{};
            unsigned Line = 0;
            unsigned Column = 0;
            unsigned Offset = 0;
            clang_getFileLocation(clang_getCursorLocation(ContextCursor), &File, &Line, &Column, &Offset);
            (void)Offset;
            if (File)
            {
                Candidate.File = FilePathFromCXFile(File);
            }
            Candidate.Line = Line;
            Candidate.Column = Column;
        }
        Candidate.QualifiedName = QualifiedName;
        Candidate.ReflectedName =
            (Override && !Override->empty()) ? NormalizeReflectedTypeNameString(std::string(*Override)) : DefaultReflectedName;

        const std::vector<std::string> ParameterNames =
            TemplateParameterNames(TemplateDeclarations[*TemplateIndex].Cursor);
        const int ArgCount = clang_Type_getNumTemplateArguments(Effective);
        for (int ArgIndex = 0; ArgIndex < ArgCount; ++ArgIndex)
        {
            const CXType ArgType = clang_Type_getTemplateArgumentAsType(Effective, static_cast<unsigned>(ArgIndex));
            if (ArgType.kind == CXType_Invalid)
            {
                continue;
            }

            const std::string ArgQualifiedName = QualifiedTypeExpr(ArgType);
            const std::string ArgKey = NormalizeTypeExpressionString(ArgQualifiedName);
            Candidate.TemplateArgumentKeys.push_back(ArgKey);
            if (static_cast<std::size_t>(ArgIndex) < ParameterNames.size())
            {
                Candidate.Substitutions.emplace_back(ParameterNames[ArgIndex], ArgQualifiedName);
            }
        }

        return Candidate;
    };

    auto VisitType = [&](auto&& Self, const CXType Type, const bool AllowOverride) -> void {
        if (Type.kind == CXType_Invalid)
        {
            return;
        }

        const CXType Canonical = clang_getCanonicalType(Type);
        const CXType Effective = Canonical.kind != CXType_Invalid ? Canonical : Type;
        if (const auto Candidate = BuildCandidate(Effective, AllowOverride ? ReflectedNameOverride : std::nullopt))
        {
            UpsertTemplateSpecializationCandidate(*Candidate, Candidates, CandidateIndices);
        }

        const CXCursor TypeDecl = clang_getTypeDeclaration(Effective);
        if (!clang_Cursor_isNull(TypeDecl))
        {
            const CXCursorKind DeclKind = clang_getCursorKind(TypeDecl);
            if (DeclKind == CXCursor_TypedefDecl || DeclKind == CXCursor_TypeAliasDecl)
            {
                const CXType Underlying = clang_getTypedefDeclUnderlyingType(TypeDecl);
                if (Underlying.kind != CXType_Invalid)
                {
                    Self(Self, Underlying, AllowOverride);
                }
            }
        }

        const int TemplateArgCount = clang_Type_getNumTemplateArguments(Effective);
        for (int ArgIndex = 0; ArgIndex < TemplateArgCount; ++ArgIndex)
        {
            const CXType ArgType = clang_Type_getTemplateArgumentAsType(Effective, static_cast<unsigned>(ArgIndex));
            if (ArgType.kind != CXType_Invalid)
            {
                Self(Self, ArgType, false);
            }
        }

        const CXType Pointee = clang_getPointeeType(Effective);
        if (Pointee.kind != CXType_Invalid)
        {
            Self(Self, Pointee, false);
        }

        const CXType Element = clang_getArrayElementType(Effective);
        if (Element.kind != CXType_Invalid)
        {
            Self(Self, Element, false);
        }
    };

    VisitType(VisitType, InputType, true);
}

std::vector<TemplateSpecializationCandidate> CollectTemplateSpecializations(
    CXTranslationUnit Unit,
    const std::unordered_set<std::string>& HeaderKeys,
    const std::vector<AnnotatedDeclaration>& TemplateDeclarations)
{
    std::vector<TemplateSpecializationCandidate> Candidates{};
    std::unordered_map<std::string, std::size_t> CandidateIndices{};
    if (TemplateDeclarations.empty())
    {
        return Candidates;
    }

    struct VisitorState
    {
        const std::unordered_set<std::string>* HeaderKeys = nullptr;
        const std::vector<AnnotatedDeclaration>* TemplateDeclarations = nullptr;
        std::vector<TemplateSpecializationCandidate>* Candidates = nullptr;
        std::unordered_map<std::string, std::size_t>* CandidateIndices = nullptr;
    } State{&HeaderKeys, &TemplateDeclarations, &Candidates, &CandidateIndices};

    clang_visitChildren(
        clang_getTranslationUnitCursor(Unit),
        [](CXCursor Cursor, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const StatePtr = static_cast<VisitorState*>(ClientData);
            if (!CursorIsFromTrackedHeaders(Cursor, *StatePtr->HeaderKeys))
            {
                return CXChildVisit_Recurse;
            }

            switch (clang_getCursorKind(Cursor))
            {
            case CXCursor_FieldDecl:
            case CXCursor_VarDecl:
            case CXCursor_ParmDecl:
            case CXCursor_CXXBaseSpecifier:
                CollectTemplateSpecializationsFromType(clang_getCursorType(Cursor),
                                                       Cursor,
                                                       *StatePtr->TemplateDeclarations,
                                                       *StatePtr->Candidates,
                                                       *StatePtr->CandidateIndices);
                break;
            case CXCursor_CXXMethod:
            {
                const CXType MethodType = clang_getCursorType(Cursor);
                CollectTemplateSpecializationsFromType(clang_getResultType(MethodType),
                                                       Cursor,
                                                       *StatePtr->TemplateDeclarations,
                                                       *StatePtr->Candidates,
                                                       *StatePtr->CandidateIndices);
                const int ArgCount = clang_Cursor_getNumArguments(Cursor);
                for (int ArgIndex = 0; ArgIndex < ArgCount; ++ArgIndex)
                {
                    const CXCursor ArgCursor = clang_Cursor_getArgument(Cursor, static_cast<unsigned>(ArgIndex));
                    CollectTemplateSpecializationsFromType(clang_getCursorType(ArgCursor),
                                                           ArgCursor,
                                                           *StatePtr->TemplateDeclarations,
                                                           *StatePtr->Candidates,
                                                           *StatePtr->CandidateIndices);
                }
                break;
            }
            case CXCursor_TypedefDecl:
            case CXCursor_TypeAliasDecl:
                CollectTemplateSpecializationsFromType(clang_getTypedefDeclUnderlyingType(Cursor),
                                                       Cursor,
                                                       *StatePtr->TemplateDeclarations,
                                                       *StatePtr->Candidates,
                                                       *StatePtr->CandidateIndices);
                break;
            default:
                break;
            }

            return CXChildVisit_Recurse;
        },
        &State);

    return Candidates;
}

TemplateExpressionProbeGroupResult ResolveTemplateSpecializationsForExpressionGroup(
    const TemplateExpressionProbeGroup& Group,
    const std::vector<AnnotatedDeclaration>& TemplateDeclarations,
    const std::vector<const char*>& ArgPointers,
    const fs::path& BuildDir,
    CXIndex IndexHandle)
{
    TemplateExpressionProbeGroupResult Result{};
    if (Group.Expressions.empty())
    {
        return Result;
    }

    std::string ProbeSource = "// Generated template probe translation unit for SnAPI reflection.\n";
    std::unordered_set<std::string> ExtraIncludes{};
    for (const AnnotatedDeclaration& Declaration : TemplateDeclarations)
    {
        const std::string IncludePath = Declaration.Header.generic_string();
        if (NormalizePath(Declaration.Header) == NormalizePath(Group.IncludedFile))
        {
            continue;
        }
        if (ExtraIncludes.insert(IncludePath).second)
        {
            ProbeSource += "#include ";
            ProbeSource += CppStringLiteral(IncludePath);
            ProbeSource += "\n";
        }
    }
    ProbeSource += "#include ";
    ProbeSource += CppStringLiteral(Group.IncludedFile.generic_string());
    ProbeSource += "\n\n";
    for (const auto& [Index, Expression] : Group.Expressions)
    {
        const std::string AliasName = "__snapi_registered_type_" + std::to_string(Index);
        if (!Expression.Namespace.empty())
        {
            ProbeSource += "namespace ";
            ProbeSource += Expression.Namespace;
            ProbeSource += " {\n";
            ProbeSource += "typedef ";
            ProbeSource += Expression.Expression;
            ProbeSource += ' ';
            ProbeSource += AliasName;
            ProbeSource += ";\n}\n";
            continue;
        }

        ProbeSource += "typedef ";
        ProbeSource += Expression.Expression;
        ProbeSource += ' ';
        ProbeSource += AliasName;
        ProbeSource += ";\n";
    }
    const fs::path ProbePath = BuildDir / ("SnAPI.ReflectionGen.template_probe." +
                                          std::to_string(std::hash<std::string>{}(Group.FileKey)) + ".cpp");
    const std::string ProbePathString = ProbePath.string();

    CXUnsavedFile UnsavedFile{};
    UnsavedFile.Filename = ProbePathString.c_str();
    UnsavedFile.Contents = ProbeSource.c_str();
    UnsavedFile.Length = ProbeSource.size();

    CXTranslationUnit Unit = nullptr;
    const CXErrorCode ParseError = clang_parseTranslationUnit2FullArgv(
        IndexHandle,
        ProbePathString.c_str(),
        ArgPointers.data(),
        static_cast<int>(ArgPointers.size()),
        &UnsavedFile,
        1,
        CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing,
        &Unit);
    if (ParseError != CXError_Success || !Unit)
    {
        return Result;
    }

    struct VisitorState
    {
        const std::vector<AnnotatedDeclaration>* TemplateDeclarations = nullptr;
        const std::vector<std::pair<std::size_t, ScopedTypeExpression>>* Group = nullptr;
        std::vector<TemplateSpecializationCandidate>* Candidates = nullptr;
        std::unordered_map<std::string, std::size_t>* CandidateIndices = nullptr;
    };
    std::unordered_map<std::string, std::size_t> CandidateIndices{};
    VisitorState State{&TemplateDeclarations, &Group.Expressions, &Result.Candidates, &CandidateIndices};

    clang_visitChildren(
        clang_getTranslationUnitCursor(Unit),
        [](CXCursor Cursor, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const StatePtr = static_cast<VisitorState*>(ClientData);
            if (clang_getCursorKind(Cursor) != CXCursor_TypedefDecl)
            {
                return CXChildVisit_Recurse;
            }

            const std::string Name = ToStringDispose(clang_getCursorSpelling(Cursor));
            if (!Name.starts_with("__snapi_registered_type_"))
            {
                return CXChildVisit_Continue;
            }

            const auto MatchIt =
                std::find_if(StatePtr->Group->begin(), StatePtr->Group->end(), [&Name](const auto& Entry) {
                    return "__snapi_registered_type_" + std::to_string(Entry.first) == Name;
                });
            const std::optional<std::string_view> ReflectedNameOverride =
                (MatchIt != StatePtr->Group->end() && !MatchIt->second.DeclaredReflectedName.empty())
                ? std::optional<std::string_view>{MatchIt->second.DeclaredReflectedName}
                : std::nullopt;
            const CXType Underlying = clang_getTypedefDeclUnderlyingType(Cursor);
            CollectTemplateSpecializationsFromType(Underlying,
                                                   Cursor,
                                                   *StatePtr->TemplateDeclarations,
                                                   *StatePtr->Candidates,
                                                   *StatePtr->CandidateIndices,
                                                   ReflectedNameOverride);

            if (MatchIt != StatePtr->Group->end())
            {
                const CXType Effective = NormalizeTemplateSpecializationType(Underlying);
                const std::string CandidateKey =
                    NormalizeTypeExpressionString(
                        NormalizeTemplateCandidateQualifiedName(PrettyPrintedTypeForCode(Effective, Cursor)));
                if (const auto CandidateIt = StatePtr->CandidateIndices->find(CandidateKey);
                    CandidateIt != StatePtr->CandidateIndices->end())
                {
                    TemplateSpecializationCandidate& Candidate = (*StatePtr->Candidates)[CandidateIt->second];
                    Candidate.File = MatchIt->second.File;
                    Candidate.Line = MatchIt->second.Line;
                    Candidate.Column = MatchIt->second.Column;
                    if (ReflectedNameOverride && !ReflectedNameOverride->empty())
                    {
                        Candidate.ReflectedName =
                            NormalizeReflectedTypeNameString(std::string(*ReflectedNameOverride));
                    }
                }
            }
            return CXChildVisit_Continue;
        },
        &State);

    clang_disposeTranslationUnit(Unit);
    Result.Parsed = true;
    return Result;
}

void AppendTemplateSpecializationsFromExpressions(
    const std::vector<ScopedTypeExpression>& Expressions,
    const std::vector<std::string>& CompileArgs,
    const fs::path& BuildDir,
    const std::vector<AnnotatedDeclaration>& TemplateDeclarations,
    std::vector<TemplateSpecializationCandidate>& Candidates,
    std::unordered_map<std::string, std::size_t>& CandidateIndices)
{
    if (Expressions.empty() || TemplateDeclarations.empty())
    {
        return;
    }

    const std::vector<TemplateExpressionProbeGroup> Groups = GroupTemplateProbeExpressions(Expressions);
    std::vector<TemplateExpressionProbeGroupResult> GroupResults(Groups.size());

    std::vector<const char*> ArgPointers{};
    ArgPointers.reserve(CompileArgs.size());
    for (const std::string& Arg : CompileArgs)
    {
        ArgPointers.push_back(Arg.c_str());
    }

    const std::size_t WorkerCount = std::min<std::size_t>(
        Groups.size(),
        std::max(1u, std::thread::hardware_concurrency()));

    std::atomic<std::size_t> NextGroup = 0;
    std::vector<std::thread> Workers{};
    Workers.reserve(WorkerCount);
    for (std::size_t WorkerIndex = 0; WorkerIndex < WorkerCount; ++WorkerIndex)
    {
        Workers.emplace_back([&]() {
            CXIndex IndexHandle = clang_createIndex(0, 0);
            while (true)
            {
                const std::size_t GroupIndex = NextGroup.fetch_add(1);
                if (GroupIndex >= Groups.size())
                {
                    break;
                }

                GroupResults[GroupIndex] = ResolveTemplateSpecializationsForExpressionGroup(
                    Groups[GroupIndex], TemplateDeclarations, ArgPointers, BuildDir, IndexHandle);
            }
            clang_disposeIndex(IndexHandle);
        });
    }

    for (std::thread& Worker : Workers)
    {
        Worker.join();
    }

    for (TemplateExpressionProbeGroupResult& GroupResult : GroupResults)
    {
        if (!GroupResult.Parsed)
        {
            continue;
        }

        for (TemplateSpecializationCandidate& Candidate : GroupResult.Candidates)
        {
            UpsertTemplateSpecializationCandidate(std::move(Candidate), Candidates, CandidateIndices);
        }
    }
}

std::vector<TemplateSpecializationCandidate> ResolveTemplateSpecializations(
    const std::vector<TemplateSpecializationCandidate>& Candidates,
    RegistrationKnowledge& Knowledge,
    std::vector<Diagnostic>& Diagnostics)
{
    std::vector<TemplateSpecializationCandidate> Result{};
    std::vector<bool> Resolved(Candidates.size(), false);

    bool Progress = true;
    while (Progress)
    {
        Progress = false;
        for (std::size_t Index = 0; Index < Candidates.size(); ++Index)
        {
            if (Resolved[Index])
            {
                continue;
            }

            const TemplateSpecializationCandidate& Candidate = Candidates[Index];
            const bool ArgsReady = std::all_of(
                Candidate.TemplateArgumentKeys.begin(),
                Candidate.TemplateArgumentKeys.end(),
                [&Knowledge](const std::string& Key) { return Key.empty() || Knowledge.RegisteredTypeKeys.contains(Key); });
            if (!ArgsReady)
            {
                continue;
            }

            Knowledge.RegisteredTypeKeys.insert(Candidate.ReflectedName);
            Knowledge.RegisteredTypeKeys.insert(NormalizeTypeExpressionString(Candidate.QualifiedName));
            Result.push_back(Candidate);
            Resolved[Index] = true;
            Progress = true;
        }
    }

    for (std::size_t Index = 0; Index < Candidates.size(); ++Index)
    {
        if (Resolved[Index])
        {
            continue;
        }

        const TemplateSpecializationCandidate& Candidate = Candidates[Index];
        const auto MissingIt = std::find_if(
            Candidate.TemplateArgumentKeys.begin(),
            Candidate.TemplateArgumentKeys.end(),
            [&Knowledge](const std::string& Key) { return !Key.empty() && !Knowledge.RegisteredTypeKeys.contains(Key); });
        const std::string MissingKey =
            MissingIt != Candidate.TemplateArgumentKeys.end() ? *MissingIt : std::string("<unknown>");
        Diagnostics.push_back(MakeWarning(
            Candidate.File,
            Candidate.Line,
            Candidate.Column,
            "Skipping reflected template specialization '" + Candidate.ReflectedName +
                "' because template argument type '" + MissingKey + "' is not registered for reflection"));
    }

    return Result;
}

void CollectAnnotatedTypeKeys(CXTranslationUnit Unit,
                              const std::unordered_set<std::string>& HeaderKeys,
                              const std::unordered_map<std::string, HeaderMarkers>& Markers,
                              std::unordered_set<std::string>& OutKeys)
{
    struct VisitorState
    {
        const std::unordered_set<std::string>* HeaderKeys = nullptr;
        const std::unordered_map<std::string, HeaderMarkers>* Markers = nullptr;
        std::unordered_map<std::string, HeaderMarkerCursor> MarkerCursors{};
        std::unordered_set<std::string>* OutKeys = nullptr;
    } State{&HeaderKeys, &Markers, {}, &OutKeys};

    for (const auto& [FileKey, FileMarkers] : Markers)
    {
        (void)FileMarkers;
        State.MarkerCursors.emplace(FileKey, HeaderMarkerCursor{});
    }

    clang_visitChildren(
        clang_getTranslationUnitCursor(Unit),
        [](CXCursor Cursor, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const StatePtr = static_cast<VisitorState*>(ClientData);
            if (!CursorIsFromTrackedHeaders(Cursor, *StatePtr->HeaderKeys))
            {
                return CXChildVisit_Recurse;
            }

            const CXCursorKind Kind = clang_getCursorKind(Cursor);
            if ((Kind == CXCursor_StructDecl || Kind == CXCursor_ClassDecl || Kind == CXCursor_EnumDecl) &&
                clang_isCursorDefinition(Cursor) != 0)
            {
                if (!MatchSourceMarker(*StatePtr->Markers, StatePtr->MarkerCursors, Cursor, MarkerKind::Type, false))
                {
                    return CXChildVisit_Recurse;
                }

                (void)MatchSourceMarker(*StatePtr->Markers, StatePtr->MarkerCursors, Cursor, MarkerKind::Type, true);
                const std::string QualifiedName = QualifiedNameForCursor(Cursor);
                if (!QualifiedName.empty())
                {
                    StatePtr->OutKeys->insert(NormalizeTypeExpressionString(QualifiedName));
                }
                return CXChildVisit_Continue;
            }

            return CXChildVisit_Recurse;
        },
        &State);
}

CXChildVisitResult TranslationUnitVisitor(CXCursor Cursor, CXCursor Parent, CXClientData ClientData)
{
    (void)Parent;
    auto* const Context = static_cast<HeaderParseContext*>(ClientData);
    if (!CursorIsFromTrackedHeaders(Cursor, Context->HeaderKeys))
    {
        return CXChildVisit_Recurse;
    }

    const CXCursorKind Kind = clang_getCursorKind(Cursor);
    if ((Kind == CXCursor_StructDecl || Kind == CXCursor_ClassDecl || Kind == CXCursor_EnumDecl) &&
        !MatchSourceMarker(*Context->Markers, *Context->MarkerCursors, Cursor, MarkerKind::Type, false))
    {
        return CXChildVisit_Recurse;
    }

    if (Kind == CXCursor_StructDecl || Kind == CXCursor_ClassDecl)
    {
        if (clang_isCursorDefinition(Cursor) == 0)
        {
            return CXChildVisit_Continue;
        }

        const ReflectionMarker* const Marker = MatchSourceMarker(
            *Context->Markers, *Context->MarkerCursors, Cursor, MarkerKind::Type, true);
        if (!Marker)
        {
            return CXChildVisit_Continue;
        }

        TypeSpec Spec = BuildRecordSpec(
            CursorFilePath(Cursor), Cursor, Marker->Payload, {}, {}, *Context, *Context->Diagnostics);
        if (!Spec.QualifiedName.empty())
        {
            Context->Types.push_back(std::move(Spec));
        }
        return CXChildVisit_Continue;
    }

    if (Kind == CXCursor_EnumDecl)
    {
        if (clang_isCursorDefinition(Cursor) == 0)
        {
            return CXChildVisit_Continue;
        }

        const ReflectionMarker* const Marker = MatchSourceMarker(
            *Context->Markers, *Context->MarkerCursors, Cursor, MarkerKind::Type, true);
        if (!Marker)
        {
            return CXChildVisit_Continue;
        }

        TypeSpec Spec = BuildEnumSpec(
            CursorFilePath(Cursor), Cursor, Marker->Payload, {}, {}, *Context, *Context->Diagnostics);
        if (!Spec.QualifiedName.empty())
        {
            Context->Types.push_back(std::move(Spec));
        }
        return CXChildVisit_Continue;
    }

    return CXChildVisit_Recurse;
}

bool HeaderMayContainReflectionAnnotations(const fs::path& Header)
{
    static constexpr std::array<std::string_view, 8> Tokens{
        "SnType(",
        "SnField(",
        "SnFunction(",
        "SnEnumValue(",
        "SNAPI_TYPE(",
        "SNAPI_FIELD(",
        "SNAPI_FUNCTION(",
        "SNAPI_ENUM_VALUE(",
    };

    std::ifstream Stream(Header);
    if (!Stream.is_open())
    {
        return false;
    }

    std::string Contents((std::istreambuf_iterator<char>(Stream)), std::istreambuf_iterator<char>());
    return std::any_of(Tokens.begin(), Tokens.end(), [&Contents](const std::string_view Token) {
        return Contents.find(Token) != std::string::npos;
    });
}

std::vector<fs::path> FilterAnnotatedHeaders(const std::vector<fs::path>& Headers)
{
    std::vector<fs::path> Result{};
    Result.reserve(Headers.size());
    for (const fs::path& Header : Headers)
    {
        const fs::path NormalizedHeader = NormalizePath(Header);
        if (HeaderMayContainReflectionAnnotations(NormalizedHeader))
        {
            Result.push_back(NormalizedHeader);
        }
    }
    return Result;
}

bool IsPathWithin(const fs::path& Path, const fs::path& Root)
{
    const std::string NormalizedPath = NormalizePath(Path).generic_string();
    std::string NormalizedRoot = NormalizePath(Root).generic_string();
    if (!NormalizedRoot.ends_with('/'))
    {
        NormalizedRoot.push_back('/');
    }
    return NormalizedPath.starts_with(NormalizedRoot);
}

std::vector<fs::path> CollectRegistrationScanFiles(const fs::path& ProjectRoot, const fs::path& SeedSource)
{
    static constexpr std::array<std::string_view, 6> Extensions{
        ".h",
        ".hpp",
        ".inl",
        ".cpp",
        ".cc",
        ".cxx",
    };

    std::vector<fs::path> Roots{};
    if (!ProjectRoot.empty())
    {
        if (const fs::path IncludeRoot = ProjectRoot / "include"; fs::exists(IncludeRoot))
        {
            Roots.push_back(IncludeRoot);
        }
        if (const fs::path SourceRoot = ProjectRoot / "src"; fs::exists(SourceRoot))
        {
            Roots.push_back(SourceRoot);
        }
        if (IsPathWithin(SeedSource, ProjectRoot / "tests"))
        {
            if (const fs::path TestsRoot = ProjectRoot / "tests"; fs::exists(TestsRoot))
            {
                Roots.push_back(TestsRoot);
            }
        }
    }

    std::vector<fs::path> Result{};
    for (const fs::path& Root : Roots)
    {
        for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(Root))
        {
            if (!Entry.is_regular_file())
            {
                continue;
            }

            const std::string Extension = Entry.path().extension().string();
            if (std::find(Extensions.begin(), Extensions.end(), Extension) == Extensions.end())
            {
                continue;
            }

            Result.push_back(NormalizePath(Entry.path()));
        }
    }

    std::sort(Result.begin(), Result.end());
    return Result;
}

std::vector<fs::path> CollectHeaderRegistrationScanFiles(const fs::path& ProjectRoot, const fs::path& SeedSource)
{
    std::vector<fs::path> Files = CollectRegistrationScanFiles(ProjectRoot, SeedSource);
    Files.erase(
        std::remove_if(
            Files.begin(),
            Files.end(),
            [](const fs::path& File) {
                const std::string Extension = File.extension().string();
                return Extension != ".h" && Extension != ".hpp" && Extension != ".inl";
            }),
        Files.end());
    return Files;
}

std::string BuildRegistrationProbeSource(const fs::path& IncludedFile,
                                         const std::vector<std::pair<std::size_t, ScopedTypeExpression>>& Expressions)
{
    std::string Source = "// Generated registration probe translation unit for SnAPI reflection.\n";
    Source += "#include ";
    Source += CppStringLiteral(IncludedFile.generic_string());
    Source += "\n\n";

    for (const auto& [Index, Expression] : Expressions)
    {
        const std::string AliasName = "__snapi_registered_type_" + std::to_string(Index);
        if (!Expression.Namespace.empty())
        {
            Source += "namespace ";
            Source += Expression.Namespace;
            Source += " {\n";
            Source += "typedef ";
            Source += Expression.Expression;
            Source += ' ';
            Source += AliasName;
            Source += ";\n}\n";
            continue;
        }

        Source += "typedef ";
        Source += Expression.Expression;
        Source += ' ';
        Source += AliasName;
        Source += ";\n";
    }

    return Source;
}

struct RegistrationProbeGroup
{
    std::string FileKey{};
    fs::path IncludedFile{};
    std::vector<std::pair<std::size_t, ScopedTypeExpression>> Expressions{};
};

struct RegistrationProbeResolution
{
    bool Parsed = false;
    std::vector<FileDependencySnapshot> Dependencies{};
    std::unordered_set<std::string> ResolvedTypeKeys{};
};

struct RegistrationKnowledgeBuildResult
{
    RegistrationKnowledge Knowledge{};
    std::unordered_map<std::string, RegistrationFileCacheEntry> FileEntries{};
};

struct ParsedHeaderArtifacts
{
    std::vector<TypeSpec> Types{};
    std::unordered_map<std::string, std::unordered_set<std::string>> KnowledgeDependenciesByHeader{};
    std::unordered_map<std::string, std::vector<TemplateSpecializationCandidate>> TemplateCandidatesByHeader{};
};

std::vector<RegistrationProbeGroup> GroupRegistrationProbeExpressions(
    const std::vector<ScopedTypeExpression>& Expressions)
{
    std::vector<RegistrationProbeGroup> Groups{};
    std::unordered_map<std::string, std::size_t> GroupIndices{};
    GroupIndices.reserve(Expressions.size());

    for (std::size_t Index = 0; Index < Expressions.size(); ++Index)
    {
        const ScopedTypeExpression& Expression = Expressions[Index];
        const std::string FileKey = NormalizePathKey(Expression.File);
        const auto [It, Inserted] = GroupIndices.emplace(FileKey, Groups.size());
        if (Inserted)
        {
            Groups.push_back(RegistrationProbeGroup{
                .FileKey = FileKey,
                .IncludedFile = Expression.File,
                .Expressions = {},
            });
        }

        Groups[It->second].Expressions.push_back({Index, Expression});
    }

    return Groups;
}

std::vector<FileDependencySnapshot> CollectRegistrationDependencies(CXTranslationUnit Unit,
                                                                    const fs::path& IncludedFile)
{
    std::vector<FileDependencySnapshot> Result{};
    std::unordered_set<std::string> Seen{};
    AddDependencySnapshot(Result, Seen, IncludedFile);

    struct InclusionVisitorState
    {
        std::vector<FileDependencySnapshot>* Out = nullptr;
        std::unordered_set<std::string>* Seen = nullptr;
    } State{&Result, &Seen};

    clang_getInclusions(
        Unit,
        [](CXFile File, CXSourceLocation* InclusionStack, unsigned IncludeLen, CXClientData ClientData) {
            (void)InclusionStack;
            (void)IncludeLen;
            auto* const StatePtr = static_cast<InclusionVisitorState*>(ClientData);
            AddDependencySnapshot(*StatePtr->Out, *StatePtr->Seen, FilePathFromCXFile(File));
        },
        &State);

    std::sort(Result.begin(), Result.end(), [](const FileDependencySnapshot& Left, const FileDependencySnapshot& Right) {
        return Left.Path.generic_string() < Right.Path.generic_string();
    });
    return Result;
}

bool RegistrationFileCacheEntryIsValid(const RegistrationFileCacheEntry& Entry, const std::uint64_t SourceHash)
{
    if (Entry.SourceHash != SourceHash)
    {
        return false;
    }

    return std::all_of(
        Entry.Dependencies.begin(),
        Entry.Dependencies.end(),
        [](const FileDependencySnapshot& Snapshot) { return DependencySnapshotMatches(Snapshot); });
}

void AppendRegistrationKeys(RegistrationKnowledge& Knowledge,
                            const std::unordered_set<std::string>& Keys,
                            const bool HeaderVisible)
{
    Knowledge.RegisteredTypeKeys.insert(Keys.begin(), Keys.end());
    if (HeaderVisible)
    {
        Knowledge.HeaderVisibleTypeKeys.insert(Keys.begin(), Keys.end());
    }
}

RegistrationProbeResolution ResolveRegisteredTypeKeysForGroup(const RegistrationProbeGroup& Group,
                                                              const std::vector<const char*>& ArgPointers,
                                                              const fs::path& BuildDir,
                                                              CXIndex IndexHandle)
{
    if (Group.Expressions.empty())
    {
        return RegistrationProbeResolution{};
    }

    RegistrationProbeResolution Result{};
    const std::string ProbeSource = BuildRegistrationProbeSource(Group.IncludedFile, Group.Expressions);
    const fs::path ProbePath = BuildDir / ("SnAPI.ReflectionGen.registration_probe." +
                                          std::to_string(std::hash<std::string>{}(Group.FileKey)) + ".cpp");
    const std::string ProbePathString = ProbePath.string();

    CXUnsavedFile UnsavedFile{};
    UnsavedFile.Filename = ProbePathString.c_str();
    UnsavedFile.Contents = ProbeSource.c_str();
    UnsavedFile.Length = ProbeSource.size();

    CXTranslationUnit Unit = nullptr;
    const CXErrorCode ParseError = clang_parseTranslationUnit2FullArgv(
        IndexHandle,
        ProbePathString.c_str(),
        ArgPointers.data(),
        static_cast<int>(ArgPointers.size()),
        &UnsavedFile,
        1,
        CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing,
        &Unit);
    if (ParseError != CXError_Success || !Unit)
    {
        return Result;
    }

    struct VisitorState
    {
        std::unordered_set<std::string>* Out = nullptr;
        std::unordered_map<std::string, std::string> DeclaredNames{};
    } State{&Result.ResolvedTypeKeys, {}};

    for (const auto& [Index, Expression] : Group.Expressions)
    {
        if (!Expression.DeclaredReflectedName.empty())
        {
            State.DeclaredNames.emplace("__snapi_registered_type_" + std::to_string(Index), Expression.DeclaredReflectedName);
        }
    }

    clang_visitChildren(
        clang_getTranslationUnitCursor(Unit),
        [](CXCursor Cursor, CXCursor Parent, CXClientData ClientData) {
            (void)Parent;
            auto* const StatePtr = static_cast<VisitorState*>(ClientData);
            if (clang_getCursorKind(Cursor) == CXCursor_TypedefDecl)
            {
                const std::string Name = ToStringDispose(clang_getCursorSpelling(Cursor));
                if (Name.starts_with("__snapi_registered_type_"))
                {
                    const CXType Underlying = clang_getTypedefDeclUnderlyingType(Cursor);
                    AddRegisteredTypeKeyCandidates(*StatePtr->Out, Underlying, Cursor);
                    if (const auto NameIt = StatePtr->DeclaredNames.find(Name); NameIt != StatePtr->DeclaredNames.end())
                    {
                        StatePtr->Out->insert(NameIt->second);
                    }
                    return CXChildVisit_Continue;
                }
            }
            return CXChildVisit_Recurse;
        },
        &State);

    Result.Dependencies = CollectRegistrationDependencies(Unit, Group.IncludedFile);
    Result.Parsed = true;
    clang_disposeTranslationUnit(Unit);
    return Result;
}

RegistrationKnowledgeBuildResult BuildRegistrationKnowledgeFromScanData(
    const RegistrationScanData& ScanData,
    const std::vector<fs::path>& Headers,
    const std::vector<std::string>& CompileArgs,
    const std::uint64_t CompileArgsHash,
    const fs::path& BuildDir,
    const ReflectionCache* const ExistingCache)
{
    RegistrationKnowledgeBuildResult Result{};
    Result.Knowledge.RegisteredTypeKeys.insert(NormalizeTypeExpressionString("void"));
    Result.Knowledge.HeaderVisibleTypeKeys.insert(NormalizeTypeExpressionString("void"));

    const std::vector<fs::path> AnnotatedHeaders = FilterAnnotatedHeaders(Headers);
    if (!AnnotatedHeaders.empty())
    {
        std::vector<const char*> ArgPointers{};
        ArgPointers.reserve(CompileArgs.size());
        for (const std::string& Arg : CompileArgs)
        {
            ArgPointers.push_back(Arg.c_str());
        }

        std::unordered_set<std::string> HeaderKeys{};
        HeaderKeys.reserve(AnnotatedHeaders.size());
        for (const fs::path& Header : AnnotatedHeaders)
        {
            HeaderKeys.insert(Header.generic_string());
        }

        std::vector<Diagnostic> MarkerDiagnostics{};
        const std::unordered_map<std::string, HeaderMarkers> Markers =
            ScanReflectionMarkers(AnnotatedHeaders, MarkerDiagnostics);
        const fs::path ScanSourcePath = NormalizePath(BuildDir / "SnAPI.ReflectionGen.annotated_type_scan.cpp");
        const std::string ScanSourcePathString = ScanSourcePath.string();
        const std::string ScanSource = BuildScanTranslationUnitSource(AnnotatedHeaders);

        CXUnsavedFile UnsavedFile{};
        UnsavedFile.Filename = ScanSourcePathString.c_str();
        UnsavedFile.Contents = ScanSource.c_str();
        UnsavedFile.Length = ScanSource.size();

        CXIndex Index = clang_createIndex(0, 0);
        CXTranslationUnit Unit = nullptr;
        const CXErrorCode ParseError = clang_parseTranslationUnit2FullArgv(
            Index,
            ScanSourcePathString.c_str(),
            ArgPointers.data(),
            static_cast<int>(ArgPointers.size()),
            &UnsavedFile,
            1,
            CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing,
            &Unit);
        if (ParseError == CXError_Success && Unit != nullptr)
        {
            std::unordered_set<std::string> AnnotatedTypeKeys{};
            CollectAnnotatedTypeKeys(Unit, HeaderKeys, Markers, AnnotatedTypeKeys);
            AppendRegistrationKeys(Result.Knowledge, AnnotatedTypeKeys, false);
            clang_disposeTranslationUnit(Unit);
        }
        else if (Unit != nullptr)
        {
            clang_disposeTranslationUnit(Unit);
        }
        clang_disposeIndex(Index);
    }

    if (ScanData.Expressions.empty())
    {
        return Result;
    }

    const std::vector<RegistrationProbeGroup> Groups = GroupRegistrationProbeExpressions(ScanData.Expressions);
    Result.FileEntries.reserve(Groups.size());

    std::vector<const char*> ArgPointers{};
    ArgPointers.reserve(CompileArgs.size());
    for (const std::string& Arg : CompileArgs)
    {
        ArgPointers.push_back(Arg.c_str());
    }

    std::vector<RegistrationProbeResolution> GroupResults(Groups.size());
    std::vector<std::size_t> DirtyGroupIndices{};
    DirtyGroupIndices.reserve(Groups.size());

    const bool CanReuseRegistrationCache =
        ExistingCache != nullptr && ExistingCache->RegistrationCompileArgsHash == CompileArgsHash;
    for (std::size_t GroupIndex = 0; GroupIndex < Groups.size(); ++GroupIndex)
    {
        const RegistrationProbeGroup& Group = Groups[GroupIndex];
        const auto SourceHashIt = ScanData.FileSourceHashes.find(Group.FileKey);
        const std::uint64_t SourceHash = SourceHashIt != ScanData.FileSourceHashes.end() ? SourceHashIt->second : 0;

        bool Reused = false;
        if (CanReuseRegistrationCache)
        {
            if (const auto CacheIt = ExistingCache->RegistrationFiles.find(Group.FileKey);
                CacheIt != ExistingCache->RegistrationFiles.end() &&
                RegistrationFileCacheEntryIsValid(CacheIt->second, SourceHash))
            {
                Result.FileEntries.emplace(Group.FileKey, CacheIt->second);
                AppendRegistrationKeys(
                    Result.Knowledge,
                    CacheIt->second.ResolvedTypeKeys,
                    IsHeaderLikePath(fs::path(Group.FileKey)));
                Reused = true;
            }
        }

        if (!Reused)
        {
            DirtyGroupIndices.push_back(GroupIndex);
        }
    }

    if (DirtyGroupIndices.empty())
    {
        return Result;
    }

    const std::size_t WorkerCount = std::min<std::size_t>(
        DirtyGroupIndices.size(),
        std::max(1u, std::thread::hardware_concurrency()));

    std::atomic<std::size_t> NextGroup = 0;
    std::vector<std::thread> Workers{};
    Workers.reserve(WorkerCount);
    for (std::size_t WorkerIndex = 0; WorkerIndex < WorkerCount; ++WorkerIndex)
    {
        Workers.emplace_back([&]() {
            CXIndex IndexHandle = clang_createIndex(0, 0);
            while (true)
            {
                const std::size_t DirtyIndex = NextGroup.fetch_add(1);
                if (DirtyIndex >= DirtyGroupIndices.size())
                {
                    break;
                }

                const std::size_t GroupIndex = DirtyGroupIndices[DirtyIndex];
                GroupResults[GroupIndex] =
                    ResolveRegisteredTypeKeysForGroup(Groups[GroupIndex], ArgPointers, BuildDir, IndexHandle);
            }
            clang_disposeIndex(IndexHandle);
        });
    }

    for (std::thread& Worker : Workers)
    {
        Worker.join();
    }

    for (const std::size_t GroupIndex : DirtyGroupIndices)
    {
        const RegistrationProbeGroup& Group = Groups[GroupIndex];
        const RegistrationProbeResolution& Resolution = GroupResults[GroupIndex];
        if (!Resolution.Parsed)
        {
            continue;
        }

        RegistrationFileCacheEntry Entry{};
        if (const auto SourceHashIt = ScanData.FileSourceHashes.find(Group.FileKey);
            SourceHashIt != ScanData.FileSourceHashes.end())
        {
            Entry.SourceHash = SourceHashIt->second;
        }
        Entry.Dependencies = Resolution.Dependencies;
        Entry.ResolvedTypeKeys = Resolution.ResolvedTypeKeys;

        Result.FileEntries[Group.FileKey] = Entry;
        AppendRegistrationKeys(
            Result.Knowledge,
            Entry.ResolvedTypeKeys,
            IsHeaderLikePath(fs::path(Group.FileKey)));
    }

    return Result;
}

std::uint64_t HashScopedTypeExpressions(const std::vector<ScopedTypeExpression>& Expressions)
{
    std::uint64_t Seed = kFnvOffsetBasis;
    for (const ScopedTypeExpression& Expression : Expressions)
    {
        HashCombine(Seed, HashPathString(Expression.File));
        HashCombine(Seed, Expression.Line);
        HashCombine(Seed, Expression.Column);
        HashCombine(Seed, HashStringView(Expression.Namespace));
        HashCombine(Seed, HashStringView(Expression.Expression));
        HashCombine(Seed, HashStringView(Expression.DeclaredReflectedName));
    }
    return Seed;
}

RegistrationScanData BuildRegistrationScanData(const fs::path& ProjectRoot, const fs::path& SeedSource)
{
    const std::vector<fs::path> Files = CollectRegistrationScanFiles(ProjectRoot, SeedSource);

    RegistrationScanData Result{};
    for (const fs::path& File : Files)
    {
        std::uint64_t SourceHash = 0;
        std::vector<ScopedTypeExpression> FileExpressions = ScanTypeExpressionsInFile(File, &SourceHash);
        Result.FileSourceHashes.emplace(NormalizePathKey(File), SourceHash);
        if (IsHeaderLikePath(File))
        {
            Result.HeaderExpressions.insert(Result.HeaderExpressions.end(),
                                            FileExpressions.begin(),
                                            FileExpressions.end());
        }
        Result.Expressions.insert(Result.Expressions.end(),
                                  std::make_move_iterator(FileExpressions.begin()),
                                  std::make_move_iterator(FileExpressions.end()));
    }

    Result.Fingerprint = CombinedHash(
        {kReflectionCacheSchemaVersion,
         HashScopedTypeExpressions(Result.Expressions),
         HashScopedTypeExpressions(Result.HeaderExpressions)});
    return Result;
}

std::string BuildScanTranslationUnitSource(const std::vector<fs::path>& Headers)
{
    std::string Source = "// Generated scan translation unit for SnAPI reflection.\n";
    for (const fs::path& Header : Headers)
    {
        Source += "#include ";
        Source += CppStringLiteral(Header.generic_string());
        Source += "\n";
    }
    return Source;
}

bool MatchesGeneratedTypeNameHeaderArg(const std::string& Arg, const fs::path& GeneratedTypeNameHeader)
{
    if (GeneratedTypeNameHeader.empty() || Arg.empty())
    {
        return false;
    }

    const fs::path ExpectedPath = NormalizePath(GeneratedTypeNameHeader);
    const std::string ExpectedFileName = ExpectedPath.filename().string();

    if (Arg == ExpectedFileName)
    {
        return true;
    }

    if (Arg.find(".typenames.generated.hpp") == std::string::npos)
    {
        return false;
    }

    const fs::path CandidatePath = NormalizePath(fs::path(Arg));
    if (!CandidatePath.empty())
    {
        if (CandidatePath == ExpectedPath)
        {
            return true;
        }

        if (CandidatePath.filename() == ExpectedPath.filename())
        {
            return true;
        }
    }

    return Arg.ends_with(ExpectedFileName);
}

std::vector<std::string> LoadCompileArguments(const fs::path& BuildDir,
                                              const fs::path& SeedSource,
                                              const fs::path& GeneratedTypeNameHeader)
{
    CXCompilationDatabase_Error ErrorCode = CXCompilationDatabase_NoError;
    CXCompilationDatabase Database =
        clang_CompilationDatabase_fromDirectory(BuildDir.string().c_str(), &ErrorCode);
    if (ErrorCode != CXCompilationDatabase_NoError || !Database)
    {
        throw std::runtime_error("Failed to load compilation database from " + BuildDir.string());
    }

    std::vector<std::string> Result{};
    CXCompileCommands Commands =
        clang_CompilationDatabase_getCompileCommands(Database, SeedSource.string().c_str());
    if (!Commands || clang_CompileCommands_getSize(Commands) == 0)
    {
        clang_CompileCommands_dispose(Commands);
        clang_CompilationDatabase_dispose(Database);
        throw std::runtime_error("No compile command found for " + SeedSource.string());
    }

    const CXCompileCommand Command = clang_CompileCommands_getCommand(Commands, 0);
    const fs::path NormalizedSeedSource = NormalizePath(SeedSource);
    bool SkipNext = false;
    for (unsigned Index = 0; Index < clang_CompileCommand_getNumArgs(Command); ++Index)
    {
        std::string Arg = ToStringDispose(clang_CompileCommand_getArg(Command, Index));
        if (SkipNext)
        {
            SkipNext = false;
            continue;
        }

        if (Arg == "-c")
        {
            continue;
        }

        if (!Arg.empty() && Arg.front() != '-' && NormalizePath(fs::path(Arg)) == NormalizedSeedSource)
        {
            continue;
        }

        if (Arg == "-o" || Arg == "-MF" || Arg == "-MT" || Arg == "-MQ" || Arg == "-MJ")
        {
            SkipNext = true;
            continue;
        }

        if (Arg == "-include")
        {
            if (Index + 1 < clang_CompileCommand_getNumArgs(Command))
            {
                const std::string IncludedArg = ToStringDispose(clang_CompileCommand_getArg(Command, Index + 1));
                if (MatchesGeneratedTypeNameHeaderArg(IncludedArg, GeneratedTypeNameHeader))
                {
                    SkipNext = true;
                    continue;
                }
            }
        }

        if (Arg.starts_with("-o") || Arg.starts_with("-MF") || Arg.starts_with("-MT") ||
            Arg.starts_with("-MQ") || Arg.starts_with("-MJ"))
        {
            continue;
        }

        if (Arg.starts_with("/FI"))
        {
            const std::string IncludedArg = Arg.substr(3);
            if (MatchesGeneratedTypeNameHeaderArg(IncludedArg, GeneratedTypeNameHeader))
            {
                continue;
            }
        }

        if (Arg.starts_with("-include") &&
            Arg.size() > std::char_traits<char>::length("-include") &&
            MatchesGeneratedTypeNameHeaderArg(Arg.substr(std::char_traits<char>::length("-include")),
                                              GeneratedTypeNameHeader))
        {
            continue;
        }

        Result.push_back(std::move(Arg));
    }

    Result.push_back("-DSNAPI_REFLECTION_SCAN=1");
    Result.push_back("-fparse-all-comments");

    clang_CompileCommands_dispose(Commands);
    clang_CompilationDatabase_dispose(Database);
    return Result;
}

ParsedHeaderArtifacts ParseAnnotatedHeaders(const std::vector<fs::path>& Headers,
                                           const std::vector<fs::path>& ExpressionHeaders,
                                           const std::vector<std::string>& CompileArgs,
                                           const fs::path& BuildDir,
                                           const RegistrationKnowledge& BaseKnowledge,
                                           const std::unordered_map<std::string, std::vector<TemplateSpecializationCandidate>>*
                                               CachedTemplateCandidatesByHeader,
                                           std::vector<Diagnostic>& Diagnostics)
{
    ParsedHeaderArtifacts Result{};
    const std::vector<fs::path> AnnotatedHeaders = FilterAnnotatedHeaders(Headers);
    if (AnnotatedHeaders.empty())
    {
        return Result;
    }

    std::vector<const char*> ArgPointers{};
    ArgPointers.reserve(CompileArgs.size());
    for (const std::string& Arg : CompileArgs)
    {
        ArgPointers.push_back(Arg.c_str());
    }

    std::unordered_set<std::string> HeaderKeys{};
    HeaderKeys.reserve(AnnotatedHeaders.size());
    for (const fs::path& Header : AnnotatedHeaders)
    {
        HeaderKeys.insert(Header.generic_string());
    }

    const std::unordered_map<std::string, HeaderMarkers> Markers = ScanReflectionMarkers(AnnotatedHeaders, Diagnostics);

    const fs::path ScanSourcePath = NormalizePath(BuildDir / "SnAPI.ReflectionGen.scan.cpp");
    const std::string ScanSourcePathString = ScanSourcePath.string();
    const std::string ScanSource = BuildScanTranslationUnitSource(AnnotatedHeaders);
    CXUnsavedFile UnsavedFile{};
    UnsavedFile.Filename = ScanSourcePathString.c_str();
    UnsavedFile.Contents = ScanSource.c_str();
    UnsavedFile.Length = ScanSource.size();

    CXIndex Index = clang_createIndex(0, 0);
    CXTranslationUnit Unit = nullptr;
    const CXErrorCode ParseError = clang_parseTranslationUnit2FullArgv(
        Index,
        ScanSourcePath.string().c_str(),
        ArgPointers.data(),
        static_cast<int>(ArgPointers.size()),
        &UnsavedFile,
        1,
        CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing,
        &Unit);
    if (ParseError != CXError_Success || !Unit)
    {
        clang_disposeIndex(Index);
        Diagnostics.push_back(Diagnostic{DiagnosticSeverity::Error, ScanSourcePath, 0, 0, "Failed to parse scan translation unit"});
        return Result;
    }

    const unsigned DiagCount = clang_getNumDiagnostics(Unit);
    bool HasError = false;
    for (unsigned DiagIndex = 0; DiagIndex < DiagCount; ++DiagIndex)
    {
        CXDiagnostic Diag = clang_getDiagnostic(Unit, DiagIndex);
        const CXDiagnosticSeverity Severity = clang_getDiagnosticSeverity(Diag);
        if (Severity == CXDiagnostic_Error || Severity == CXDiagnostic_Fatal)
        {
            CXSourceLocation Location = clang_getDiagnosticLocation(Diag);
            CXFile File{};
            unsigned Line = 0;
            unsigned Column = 0;
            unsigned Offset = 0;
            clang_getFileLocation(Location, &File, &Line, &Column, &Offset);

            Diagnostic Out{};
            if (File)
            {
                Out.File = FilePathFromCXFile(File);
            }
            else
            {
                Out.File = ScanSourcePath;
            }
            Out.Line = Line;
            Out.Column = Column;
            Out.Message = ToStringDispose(clang_formatDiagnostic(Diag, clang_defaultDiagnosticDisplayOptions()));
            Diagnostics.push_back(std::move(Out));
            HasError = true;
        }
        clang_disposeDiagnostic(Diag);
    }

    if (!HasError)
    {
        RegistrationKnowledge Knowledge = BaseKnowledge;
        std::unordered_map<std::string, HeaderMarkerCursor> MarkerCursors{};
        InitializeMarkerCursors(Markers, MarkerCursors);

        std::unordered_set<std::string> ConcreteAnnotatedKeys{};
        const std::vector<AnnotatedDeclaration> Declarations = CollectAnnotatedDeclarations(
            Unit, HeaderKeys, Markers, MarkerCursors, ConcreteAnnotatedKeys, Diagnostics);
        Knowledge.RegisteredTypeKeys.insert(ConcreteAnnotatedKeys.begin(), ConcreteAnnotatedKeys.end());

        std::vector<AnnotatedDeclaration> TemplateDeclarations{};
        for (const AnnotatedDeclaration& Declaration : Declarations)
        {
            if (Declaration.IsTemplate)
            {
                TemplateDeclarations.push_back(Declaration);
            }
        }

        std::vector<TemplateSpecializationCandidate> TemplateCandidates =
            CollectTemplateSpecializations(Unit, HeaderKeys, TemplateDeclarations);
        std::unordered_map<std::string, std::size_t> TemplateCandidateIndices{};
        for (std::size_t Index = 0; Index < TemplateCandidates.size(); ++Index)
        {
            TemplateCandidateIndices.emplace(
                NormalizeTypeExpressionString(TemplateCandidates[Index].QualifiedName), Index);
        }

        bool ReusedCachedTemplateCandidates = false;
        if (CachedTemplateCandidatesByHeader && !TemplateDeclarations.empty())
        {
            ReusedCachedTemplateCandidates = true;
            std::unordered_set<std::string> SeenHeaders{};
            for (const AnnotatedDeclaration& Declaration : TemplateDeclarations)
            {
                const std::string HeaderKey = Declaration.Header.generic_string();
                if (!SeenHeaders.insert(HeaderKey).second)
                {
                    continue;
                }

                const auto CachedIt = CachedTemplateCandidatesByHeader->find(HeaderKey);
                if (CachedIt == CachedTemplateCandidatesByHeader->end())
                {
                    ReusedCachedTemplateCandidates = false;
                    break;
                }

                for (const TemplateSpecializationCandidate& Candidate : CachedIt->second)
                {
                    UpsertTemplateSpecializationCandidate(Candidate, TemplateCandidates, TemplateCandidateIndices);
                }
            }
        }

        if (!ReusedCachedTemplateCandidates)
        {
            std::vector<ScopedTypeExpression> HeaderExpressions{};
            for (const fs::path& Header : ExpressionHeaders)
            {
                std::vector<ScopedTypeExpression> FileExpressions = ScanTypeExpressionsInFile(Header);
                HeaderExpressions.insert(HeaderExpressions.end(),
                                         std::make_move_iterator(FileExpressions.begin()),
                                         std::make_move_iterator(FileExpressions.end()));
            }
            AppendTemplateSpecializationsFromExpressions(
                HeaderExpressions,
                CompileArgs,
                BuildDir,
                TemplateDeclarations,
                TemplateCandidates,
                TemplateCandidateIndices);
        }

        for (const TemplateSpecializationCandidate& Candidate : TemplateCandidates)
        {
            if (Candidate.TemplateIndex >= TemplateDeclarations.size())
            {
                continue;
            }

            Result.TemplateCandidatesByHeader[TemplateDeclarations[Candidate.TemplateIndex].Header.generic_string()]
                .push_back(Candidate);
            std::unordered_set<std::string>& Dependencies =
                Result.KnowledgeDependenciesByHeader[TemplateDeclarations[Candidate.TemplateIndex].Header.generic_string()];
            for (const std::string& Key : Candidate.TemplateArgumentKeys)
            {
                if (!Key.empty())
                {
                    Dependencies.insert(Key);
                }
            }
            if (!Candidate.ReflectedName.empty())
            {
                Dependencies.insert(Candidate.ReflectedName);
            }
        }

        const std::vector<TemplateSpecializationCandidate> ResolvedTemplateCandidates =
            ResolveTemplateSpecializations(TemplateCandidates, Knowledge, Diagnostics);

        std::vector<std::vector<TemplateSpecializationCandidate>> TemplateCandidatesByIndex(TemplateDeclarations.size());
        for (const TemplateSpecializationCandidate& Candidate : ResolvedTemplateCandidates)
        {
            if (Candidate.TemplateIndex < TemplateCandidatesByIndex.size())
            {
                TemplateCandidatesByIndex[Candidate.TemplateIndex].push_back(Candidate);
            }
        }

        HeaderParseContext Context{HeaderKeys, {}, &Diagnostics, &Knowledge, &Markers, &MarkerCursors};
        std::size_t TemplateDeclarationIndex = 0;
        for (const AnnotatedDeclaration& Declaration : Declarations)
        {
            const CXCursorKind Kind = clang_getCursorKind(Declaration.Cursor);
            if (Declaration.IsTemplate)
            {
                const std::size_t CandidateIndex = TemplateDeclarationIndex++;
                TypeSpec Prototype = BuildRecordSpec(
                    Declaration.Header, Declaration.Cursor, Declaration.Payload, {}, {}, Context, Diagnostics);
                if (!Prototype.QualifiedName.empty())
                {
                    if (CandidateIndex < TemplateCandidatesByIndex.size())
                    {
                        for (const TemplateSpecializationCandidate& Candidate : TemplateCandidatesByIndex[CandidateIndex])
                        {
                            TypeSpec Expanded = ExpandTemplatePrototype(Prototype, Prototype.QualifiedName, Candidate);
                            Expanded.NeedsGeneratedTypeName =
                                !BaseKnowledge.HeaderVisibleTypeKeys.contains(Candidate.ReflectedName);
                            Result.Types.push_back(std::move(Expanded));
                        }
                    }
                }
                continue;
            }

            if (Kind == CXCursor_EnumDecl)
            {
                TypeSpec Spec = BuildEnumSpec(
                    Declaration.Header, Declaration.Cursor, Declaration.Payload, {}, {}, Context, Diagnostics);
                if (!Spec.QualifiedName.empty())
                {
                    Result.Types.push_back(std::move(Spec));
                }
                continue;
            }

            TypeSpec Spec = BuildRecordSpec(
                Declaration.Header, Declaration.Cursor, Declaration.Payload, {}, {}, Context, Diagnostics);
            if (!Spec.QualifiedName.empty())
            {
                Result.Types.push_back(std::move(Spec));
            }
        }

        auto ReportUnmatched = [&](const std::vector<ReflectionMarker>& Entries,
                                   const std::vector<unsigned char>& Usage,
                                   const std::string_view MarkerLabel) {
            for (std::size_t Index = 0; Index < Entries.size(); ++Index)
            {
                if (Index < Usage.size() && Usage[Index] != 0)
                {
                    continue;
                }
                Diagnostics.push_back(MakeDiagnostic(
                    Entries[Index].File,
                    Entries[Index].Line,
                    Entries[Index].Column,
                    std::string(MarkerLabel) + " is not attached to a supported declaration"));
            }
        };

        for (const auto& [FileKey, FileMarkers] : Markers)
        {
            const auto CursorIt = MarkerCursors.find(FileKey);
            if (CursorIt == MarkerCursors.end())
            {
                continue;
            }

            ReportUnmatched(FileMarkers.Type, CursorIt->second.UsedType, "SnType");
            ReportUnmatched(FileMarkers.Field, CursorIt->second.UsedField, "SnField");
            ReportUnmatched(FileMarkers.Function, CursorIt->second.UsedFunction, "SnFunction");
            ReportUnmatched(FileMarkers.EnumValue, CursorIt->second.UsedEnumValue, "SnEnumValue");
            ReportUnmatched(FileMarkers.Generated, CursorIt->second.UsedGenerated, "SnGenerated");
        }
    }

    clang_disposeTranslationUnit(Unit);

    clang_disposeIndex(Index);
    return Result;
}

void WriteAssignment(std::ostream& Stream, const std::string& Target, const std::string& Value, std::string_view Indent = "    ")
{
    if (!Value.empty())
    {
        Stream << Indent << Target << " = " << CppStringLiteral(Value) << ";\n";
    }
}

std::string GeneratedTypeAliasName(const std::size_t Index)
{
    return "::SnAPI::GameFramework::GeneratedReflectionDetail::TGeneratedType_" + std::to_string(Index);
}

bool RequiresGeneratedTypeAlias(const TypeSpec& Spec)
{
    return Spec.QualifiedName.find('<') != std::string::npos;
}

std::string RegistrationTypeExpression(const TypeSpec& Spec, const std::size_t Index)
{
    return RequiresGeneratedTypeAlias(Spec) ? GeneratedTypeAliasName(Index) : Spec.QualifiedName;
}

std::string EffectiveParamName(const ParamSpec& Param, const std::size_t Index)
{
    return Param.Name.empty() ? ("Arg" + std::to_string(Index)) : Param.Name;
}

std::string ParameterDeclarationList(const std::vector<ParamSpec>& Params)
{
    std::string Result{};
    for (std::size_t Index = 0; Index < Params.size(); ++Index)
    {
        if (!Result.empty())
        {
            Result += ", ";
        }
        Result += Params[Index].Type;
        Result += ' ';
        Result += EffectiveParamName(Params[Index], Index);
    }
    return Result;
}

std::string ParameterArgumentList(const std::vector<ParamSpec>& Params)
{
    std::string Result{};
    for (std::size_t Index = 0; Index < Params.size(); ++Index)
    {
        if (!Result.empty())
        {
            Result += ", ";
        }
        Result += EffectiveParamName(Params[Index], Index);
    }
    return Result;
}

std::string VariantInitializerList(const std::vector<ParamSpec>& Params)
{
    std::string Result = "{";
    for (std::size_t Index = 0; Index < Params.size(); ++Index)
    {
        if (Index > 0)
        {
            Result += ", ";
        }
        Result += "::SnAPI::GameFramework::Variant::FromValue(";
        Result += EffectiveParamName(Params[Index], Index);
        Result += ')';
    }
    Result += '}';
    return Result;
}

void EmitRecordRegistration(std::ostream& Stream, const TypeSpec& Spec, const std::size_t Index)
{
    const std::string TypeExpr = RegistrationTypeExpression(Spec, Index);
    Stream << "SNAPI_REFLECT_METADATA(" << TypeExpr
           << ", ([]() -> ::SnAPI::GameFramework::TExpected<::SnAPI::GameFramework::TypeInfo*> {\n";
    Stream << "    using T = " << TypeExpr << ";\n";
    Stream << "    auto Builder = ::SnAPI::GameFramework::TTypeBuilder<T>(::SnAPI::GameFramework::ReflectedTypeName<T>().c_str());\n";
    if (Spec.IsInterface)
    {
        Stream << "    Builder.AsInterface();\n";
    }
    for (const BaseSpec& Base : Spec.Bases)
    {
        Stream << "    if constexpr (" << Base.ConditionExpr << ")\n";
        Stream << "    {\n";
        Stream << "        Builder.Base<" << Base.TypeExpr << ">();\n";
        Stream << "    }\n";
    }
    for (const FieldSpec& Field : Spec.Fields)
    {
        Stream << "    if constexpr (" << Field.ConditionExpr << ")\n";
        Stream << "    {\n";
        Stream << "        " << Field.BuilderExpr << "\n";
        Stream << "    }\n";
    }
    for (const MethodSpec& Method : Spec.Methods)
    {
        Stream << "    if constexpr (" << Method.ConditionExpr << ")\n";
        Stream << "    {\n";
        Stream << "        Builder.Method(" << CppStringLiteral(Method.Name) << ", " << Method.PointerExpr << ", " << Method.FlagsExpr << ");\n";
        Stream << "    }\n";
    }
    for (const GeneratedRpcSpec& Rpc : Spec.GeneratedRpcs)
    {
        Stream << "    Builder.Method("
               << CppStringLiteral(Rpc.ServerEntryName)
               << ", T::" << Rpc.ServerEntryAccessor << "(), "
               << "(" << Rpc.ReliabilityFlagsExpr
               << " | ::SnAPI::GameFramework::EMethodFlagBits::RpcNetServer"
               << " | ::SnAPI::GameFramework::EMethodFlagBits::HiddenGenerated));\n";
        if (!Rpc.ClientEntryName.empty())
        {
            Stream << "    Builder.Method("
                   << CppStringLiteral(Rpc.ClientEntryName)
                   << ", T::" << Rpc.ClientEntryAccessor << "(), "
                   << "(" << Rpc.ReliabilityFlagsExpr << " | "
                   << (Rpc.Kind == GeneratedRpcKind::Client
                           ? "::SnAPI::GameFramework::EMethodFlagBits::RpcNetClient"
                           : "::SnAPI::GameFramework::EMethodFlagBits::RpcNetMulticast")
                   << " | ::SnAPI::GameFramework::EMethodFlagBits::HiddenGenerated));\n";
        }
    }
    Stream << "    if constexpr (!std::is_abstract_v<T> && std::is_default_constructible_v<T>)\n";
    Stream << "    {\n";
    Stream << "        Builder.Constructor<>();\n";
    Stream << "    }\n";
    Stream << "    auto Result = Builder.Register();\n";
    Stream << "    if (!Result)\n";
    Stream << "    {\n";
    Stream << "        return Result;\n";
    Stream << "    }\n";
    Stream << "    auto* Info = *Result;\n";
    WriteAssignment(Stream, "Info->DisplayName", Spec.DisplayName);
    WriteAssignment(Stream, "Info->Category", Spec.Category);
    WriteAssignment(Stream, "Info->Doc", Spec.Doc);

    if (!Spec.Fields.empty())
    {
        Stream << "    [[maybe_unused]] std::size_t ReflectedFieldIndex = 0;\n";
        for (const FieldSpec& Field : Spec.Fields)
        {
            Stream << "    if constexpr (" << Field.ConditionExpr << ")\n";
            Stream << "    {\n";
            WriteAssignment(Stream, "Info->Fields[ReflectedFieldIndex].DisplayName", Field.DisplayName, "        ");
            WriteAssignment(Stream, "Info->Fields[ReflectedFieldIndex].Category", Field.Category, "        ");
            WriteAssignment(Stream, "Info->Fields[ReflectedFieldIndex].Doc", Field.Doc, "        ");
            if (!Field.MinExpr.empty())
            {
                Stream << "        Info->Fields[ReflectedFieldIndex].Value.Min = " << Field.MinExpr << ";\n";
            }
            if (!Field.MaxExpr.empty())
            {
                Stream << "        Info->Fields[ReflectedFieldIndex].Value.Max = " << Field.MaxExpr << ";\n";
            }
            if (!Field.StepExpr.empty())
            {
                Stream << "        Info->Fields[ReflectedFieldIndex].Value.Step = " << Field.StepExpr << ";\n";
            }
            Stream << "        ++ReflectedFieldIndex;\n";
            Stream << "    }\n";
        }
    }

    if (!Spec.Methods.empty())
    {
        Stream << "    [[maybe_unused]] std::size_t ReflectedMethodIndex = 0;\n";
        for (const MethodSpec& Method : Spec.Methods)
        {
            Stream << "    if constexpr (" << Method.ConditionExpr << ")\n";
            Stream << "    {\n";
            WriteAssignment(Stream, "Info->Methods[ReflectedMethodIndex].DisplayName", Method.DisplayName, "        ");
            WriteAssignment(Stream, "Info->Methods[ReflectedMethodIndex].Category", Method.Category, "        ");
            WriteAssignment(Stream, "Info->Methods[ReflectedMethodIndex].Doc", Method.Doc, "        ");
            for (std::size_t ParamIndex = 0; ParamIndex < Method.Params.size(); ++ParamIndex)
            {
                const ParamSpec& Param = Method.Params[ParamIndex];
                WriteAssignment(Stream,
                                "Info->Methods[ReflectedMethodIndex].Params[" + std::to_string(ParamIndex) + "].Name",
                                Param.Name,
                                "        ");
                WriteAssignment(Stream,
                                "Info->Methods[ReflectedMethodIndex].Params[" + std::to_string(ParamIndex) + "].Doc",
                                Param.Doc,
                                "        ");
            }
            Stream << "        ++ReflectedMethodIndex;\n";
            Stream << "    }\n";
        }
    }

    if (Spec.DefaultConstructor)
    {
        Stream << "    if constexpr (!std::is_abstract_v<T> && std::is_default_constructible_v<T>)\n";
        Stream << "    {\n";
        WriteAssignment(Stream, "Info->Constructors[0].Doc", Spec.DefaultConstructor->Doc, "        ");
        Stream << "    }\n";
    }
    Stream << "    return Result;\n";
    Stream << "})());\n\n";
}

void EmitEnumRegistration(std::ostream& Stream, const TypeSpec& Spec, const std::size_t Index)
{
    const std::string TypeExpr = RegistrationTypeExpression(Spec, Index);
    Stream << "SNAPI_REFLECT_METADATA(" << TypeExpr
           << ", ([]() -> ::SnAPI::GameFramework::TExpected<::SnAPI::GameFramework::TypeInfo*> {\n";
    Stream << "    using T = " << TypeExpr << ";\n";
    Stream << "    ::SnAPI::GameFramework::TypeInfo Info{};\n";
    Stream << "    Info.Id = ::SnAPI::GameFramework::TypeIdFromName(::SnAPI::GameFramework::ReflectedTypeName<T>());\n";
    Stream << "    Info.Name = ::SnAPI::GameFramework::ReflectedTypeName<T>();\n";
    Stream << "    Info.Size = sizeof(T);\n";
    Stream << "    Info.Align = alignof(T);\n";
    WriteAssignment(Stream, "Info.DisplayName", Spec.DisplayName);
    WriteAssignment(Stream, "Info.Category", Spec.Category);
    WriteAssignment(Stream, "Info.Doc", Spec.Doc);
    Stream << "    Info.RuntimeOps = &::SnAPI::GameFramework::GetTypeRuntimeOps<T>();\n";
    Stream << "    Info.IsEnum = true;\n";
    Stream << "    Info.EnumIsSigned = " << (Spec.EnumIsSigned ? "true" : "false") << ";\n";
    for (const EnumValueSpec& EnumValue : Spec.EnumValues)
    {
        Stream << "    Info.EnumValues.push_back(::SnAPI::GameFramework::EnumValueInfo{"
               << CppStringLiteral(EnumValue.Name) << ", "
               << CppStringLiteral(EnumValue.DisplayName) << ", "
               << CppStringLiteral(EnumValue.Doc) << ", "
               << "static_cast<std::uint64_t>(" << EnumValue.QualifiedValueExpr << ")});\n";
    }
    Stream << "    return ::SnAPI::GameFramework::TypeRegistry::Instance().Register(std::move(Info));\n";
    Stream << "})());\n\n";
}

std::string OwnerConnectionExpression(const TypeSpec& Spec)
{
    return Spec.IsComponentLike ? "OwnerConnectionId()" : "GetOwnerConnectionId()";
}

void EmitGeneratedRpcDefinitions(std::ostream& Stream, const TypeSpec& Spec)
{
    for (const GeneratedRpcSpec& Rpc : Spec.GeneratedRpcs)
    {
        const std::string Params = ParameterDeclarationList(Rpc.Params);
        const std::string Args = ParameterArgumentList(Rpc.Params);
        const std::string Variants = VariantInitializerList(Rpc.Params);

        Stream << "void " << Spec.QualifiedName << "::" << Rpc.PublicName << "(" << Params << ")\n";
        Stream << "{\n";
        switch (Rpc.Kind)
        {
        case GeneratedRpcKind::Server:
            Stream << "    if (CallRPC(" << CppStringLiteral(Rpc.ServerEntryName) << ", " << Variants << "))\n";
            Stream << "    {\n";
            Stream << "        return;\n";
            Stream << "    }\n";
            Stream << "    " << Rpc.ServerEntryName << "(" << Args << ");\n";
            break;
        case GeneratedRpcKind::Client:
            Stream << "    if (IsServer())\n";
            Stream << "    {\n";
            Stream << "        if (CallRPC(" << CppStringLiteral(Rpc.ClientEntryName) << ", " << Variants << "))\n";
            Stream << "        {\n";
            Stream << "            return;\n";
            Stream << "        }\n";
            Stream << "        if (" << OwnerConnectionExpression(Spec) << " == 0 || IsListenServer())\n";
            Stream << "        {\n";
            Stream << "            " << Rpc.ClientEntryName << "(" << Args << ");\n";
            Stream << "        }\n";
            Stream << "        return;\n";
            Stream << "    }\n";
            Stream << "    if (CallRPC(" << CppStringLiteral(Rpc.ServerEntryName) << ", " << Variants << "))\n";
            Stream << "    {\n";
            Stream << "        return;\n";
            Stream << "    }\n";
            Stream << "    " << Rpc.ServerEntryName << "(" << Args << ");\n";
            break;
        case GeneratedRpcKind::Multicast:
            Stream << "    if (IsServer())\n";
            Stream << "    {\n";
            Stream << "        if (CallRPC(" << CppStringLiteral(Rpc.ClientEntryName) << ", " << Variants << "))\n";
            Stream << "        {\n";
            Stream << "            return;\n";
            Stream << "        }\n";
            Stream << "        " << Rpc.ClientEntryName << "(" << Args << ");\n";
            Stream << "        return;\n";
            Stream << "    }\n";
            Stream << "    if (CallRPC(" << CppStringLiteral(Rpc.ServerEntryName) << ", " << Variants << "))\n";
            Stream << "    {\n";
            Stream << "        return;\n";
            Stream << "    }\n";
            Stream << "    " << Rpc.ServerEntryName << "(" << Args << ");\n";
            break;
        }
        Stream << "}\n\n";

        Stream << "void " << Spec.QualifiedName << "::" << Rpc.ServerEntryName << "(" << Params << ")\n";
        Stream << "{\n";
        Stream << "    if (!IsServer())\n";
        Stream << "    {\n";
        Stream << "        return;\n";
        Stream << "    }\n";
        switch (Rpc.Kind)
        {
        case GeneratedRpcKind::Server:
            Stream << "    " << Rpc.ImplName << "(" << Args << ");\n";
            break;
        case GeneratedRpcKind::Client:
            Stream << "    if (CallRPC(" << CppStringLiteral(Rpc.ClientEntryName) << ", " << Variants << "))\n";
            Stream << "    {\n";
            Stream << "        return;\n";
            Stream << "    }\n";
            Stream << "    if (" << OwnerConnectionExpression(Spec) << " == 0 || IsListenServer())\n";
            Stream << "    {\n";
            Stream << "        " << Rpc.ClientEntryName << "(" << Args << ");\n";
            Stream << "    }\n";
            break;
        case GeneratedRpcKind::Multicast:
            Stream << "    if (CallRPC(" << CppStringLiteral(Rpc.ClientEntryName) << ", " << Variants << "))\n";
            Stream << "    {\n";
            Stream << "        return;\n";
            Stream << "    }\n";
            Stream << "    " << Rpc.ClientEntryName << "(" << Args << ");\n";
            break;
        }
        Stream << "}\n\n";

        if (!Rpc.ClientEntryName.empty())
        {
            Stream << "void " << Spec.QualifiedName << "::" << Rpc.ClientEntryName << "(" << Params << ")\n";
            Stream << "{\n";
            Stream << "    " << Rpc.ImplName << "(" << Args << ");\n";
            Stream << "}\n\n";
        }
    }
}

std::string GeneratedHeaderMacroPrefix(const fs::path& RelativeHeader)
{
    return "SNAPI_GENERATED_" + SanitizeIdentifier(RelativeHeader.generic_string()) + "_";
}

fs::path GeneratedHeaderRelativePathFor(const fs::path& Header, const fs::path& HeaderRoot)
{
    std::error_code Error{};
    fs::path Relative = fs::relative(Header, HeaderRoot, Error);
    if (Error || Relative.empty())
    {
        Relative = Header.filename();
    }
    Relative.replace_extension(".generated.hpp");
    return Relative;
}

fs::path GeneratedSourceRelativePathFor(const fs::path& Header, const fs::path& HeaderRoot)
{
    std::error_code Error{};
    fs::path Relative = fs::relative(Header, HeaderRoot, Error);
    if (Error || Relative.empty())
    {
        Relative = Header.filename();
    }
    Relative += ".generated.cpp";
    return Relative;
}

std::string GeneratedSourceAnchorSymbolFor(const fs::path& Header, const fs::path& HeaderRoot)
{
    return "SnAPI_ReflectionAnchor_" + SanitizeIdentifier(GeneratedSourceRelativePathFor(Header, HeaderRoot).generic_string());
}

std::string GeneratedManifestLinkSymbolFor(const fs::path& OutputPath)
{
    return "SnAPI_LinkReflectionAnchors_" + SanitizeIdentifier(OutputPath.filename().generic_string());
}

fs::path GeneratedSourceOwnerHeaderFor(const TypeSpec& Type)
{
    if (!Type.TypeNameHeader.empty() && IsHeaderLikePath(Type.TypeNameHeader))
    {
        return NormalizePath(Type.TypeNameHeader);
    }
    return NormalizePath(Type.Header);
}

bool WriteBootstrapGeneratedSidecarHeaders(const fs::path& GeneratedIncludeDir,
                                           const fs::path& HeaderRoot,
                                           const std::vector<fs::path>& Headers)
{
    for (const fs::path& Header : Headers)
    {
        const fs::path Normalized = NormalizePath(Header);
        const fs::path Relative = GeneratedHeaderRelativePathFor(Normalized, HeaderRoot);
        const fs::path OutputPath = GeneratedIncludeDir / Relative;
        std::string Contents{};
        Contents += "// Bootstrap generated by SnAPI.GameFramework ReflectionGen. Do not edit.\n";
        Contents += "#pragma once\n\n";
        Contents += "#undef SnGenerated\n";
        Contents += "#define SnGenerated()\n";
        if (!EnsureFileExistsWithContents(OutputPath, Contents))
        {
            return false;
        }
    }

    return true;
}

bool WriteBootstrapGeneratedTypeNameHeader(const fs::path& OutputPath)
{
    return EnsureFileExistsWithContents(
        OutputPath,
        "// Bootstrap generated by SnAPI.GameFramework ReflectionGen. Do not edit.\n"
        "#pragma once\n");
}

bool IsHeaderLikePath(const fs::path& Path)
{
    const std::string Extension = Path.extension().string();
    return Extension == ".h" || Extension == ".hpp" || Extension == ".inl";
}

bool WriteGeneratedSidecarHeaders(const fs::path& GeneratedIncludeDir,
                                  const fs::path& HeaderRoot,
                                  const std::vector<fs::path>& Headers,
                                  const std::vector<TypeSpec>& Types)
{
    std::unordered_map<std::string, std::vector<const TypeSpec*>> TypesByHeader{};
    for (const TypeSpec& Type : Types)
    {
        if (Type.GeneratedLine == 0)
        {
            continue;
        }
        TypesByHeader[NormalizePath(Type.Header).generic_string()].push_back(&Type);
    }

    for (const fs::path& Header : Headers)
    {
        const fs::path Normalized = NormalizePath(Header);
        const fs::path Relative = GeneratedHeaderRelativePathFor(Normalized, HeaderRoot);
        const fs::path OutputPath = GeneratedIncludeDir / Relative;
        std::ostringstream Output{};
        Output << "// Generated by SnAPI.GameFramework ReflectionGen. Do not edit.\n";
        Output << "#pragma once\n\n";
        Output << "#ifndef SNAPI_DETAIL_CAT2\n";
        Output << "#define SNAPI_DETAIL_CAT2(a, b) a##b\n";
        Output << "#endif\n";
        Output << "#ifndef SNAPI_DETAIL_CAT\n";
        Output << "#define SNAPI_DETAIL_CAT(a, b) SNAPI_DETAIL_CAT2(a, b)\n";
        Output << "#endif\n";
        Output << "#undef SNAPI_DETAIL_CURRENT_GENERATED_PREFIX\n";
        Output << "#define SNAPI_DETAIL_CURRENT_GENERATED_PREFIX "
               << GeneratedHeaderMacroPrefix(Relative) << "\n";
        Output << "#undef SnGenerated\n";
        Output << "#define SnGenerated() SNAPI_DETAIL_CAT(SNAPI_DETAIL_CURRENT_GENERATED_PREFIX, __LINE__)\n\n";

        const auto It = TypesByHeader.find(Normalized.generic_string());
        if (It == TypesByHeader.end())
        {
            if (!WriteFileTextIfChanged(OutputPath, Output.str()))
            {
                return false;
            }
            continue;
        }

        for (const TypeSpec* const Type : It->second)
        {
            const std::string MacroName = GeneratedHeaderMacroPrefix(Relative) + std::to_string(Type->GeneratedLine);
            Output << "#define " << MacroName << " \\\n";
            Output << "private: \\\n";
            Output << "    using __SnGeneratedSelf = " << Type->DeclName << "; \\\n";
            Output << "public: \\\n";
            Output << "    static constexpr const char* kTypeName = "
                   << CppStringLiteral(Type->ReflectedName.empty()
                                           ? NormalizeTypeExpressionString(Type->QualifiedName)
                                           : Type->ReflectedName)
                   << ";";

            if (Type->GeneratedRpcs.empty())
            {
                Output << "\n\n";
                continue;
            }

            Output << " \\\n";
            Output << "protected: \\\n";
            for (const GeneratedRpcSpec& Rpc : Type->GeneratedRpcs)
            {
                Output << "    void " << Rpc.ImplName << "(" << ParameterDeclarationList(Rpc.Params) << "); \\\n";
            }
            Output << "private: \\\n";
            for (const GeneratedRpcSpec& Rpc : Type->GeneratedRpcs)
            {
                Output << "    void " << Rpc.ServerEntryName << "(" << ParameterDeclarationList(Rpc.Params) << "); \\\n";
                if (!Rpc.ClientEntryName.empty())
                {
                    Output << "    void " << Rpc.ClientEntryName << "(" << ParameterDeclarationList(Rpc.Params)
                           << "); \\\n";
                }
            }
            Output << "public: \\\n";
            for (std::size_t RpcIndex = 0; RpcIndex < Type->GeneratedRpcs.size(); ++RpcIndex)
            {
                const GeneratedRpcSpec& Rpc = Type->GeneratedRpcs[RpcIndex];
                Output << "    static constexpr auto " << Rpc.ServerEntryAccessor << "() { return &__SnGeneratedSelf::"
                       << Rpc.ServerEntryName << "; }";
                if (RpcIndex + 1 < Type->GeneratedRpcs.size() || !Rpc.ClientEntryName.empty())
                {
                    Output << " \\\n";
                }
                else
                {
                    Output << "\n";
                }
                if (!Rpc.ClientEntryName.empty())
                {
                    Output << "    static constexpr auto " << Rpc.ClientEntryAccessor << "() { return &__SnGeneratedSelf::"
                           << Rpc.ClientEntryName << "; }";
                    if (RpcIndex + 1 < Type->GeneratedRpcs.size())
                    {
                        Output << " \\\n";
                    }
                    else
                    {
                        Output << "\n";
                    }
                }
            }
            Output << "\n";
        }

        if (!WriteFileTextIfChanged(OutputPath, Output.str()))
        {
            return false;
        }
    }

    return true;
}

bool WriteGeneratedTypeNameHeaderEntries(const fs::path& OutputPath,
                                         const std::vector<TypeNameCacheEntry>& Entries)
{
    std::ostringstream Output{};
    Output << "// Generated by SnAPI.GameFramework ReflectionGen. Do not edit.\n";
    Output << "#pragma once\n\n";
    Output << "#include <type_traits>\n";
    Output << "#include \"TypeName.h\"\n";

    std::unordered_set<std::string> Includes{};
    std::unordered_set<std::string> SeenQualifiedNames{};
    std::vector<const TypeNameCacheEntry*> UniqueEntries{};
    UniqueEntries.reserve(Entries.size());
    for (const TypeNameCacheEntry& Entry : Entries)
    {
        if (Entry.QualifiedName.empty() || !SeenQualifiedNames.insert(Entry.QualifiedName).second)
        {
            continue;
        }
        UniqueEntries.push_back(&Entry);
        Includes.insert(Entry.Header.generic_string());
        const fs::path SupportHeader =
            (!Entry.TypeNameHeader.empty() && IsHeaderLikePath(Entry.TypeNameHeader)) ? Entry.TypeNameHeader : fs::path{};
        if (!SupportHeader.empty())
        {
            Includes.insert(SupportHeader.generic_string());
        }
    }

    std::vector<std::string> SortedIncludes(Includes.begin(), Includes.end());
    std::sort(SortedIncludes.begin(), SortedIncludes.end());
    for (const std::string& Include : SortedIncludes)
    {
        Output << "#include " << CppStringLiteral(Include) << "\n";
    }
    Output << "\n";

    Output << "namespace SnAPI::GameFramework\n";
    Output << "{\n";
    for (const TypeNameCacheEntry* const Entry : UniqueEntries)
    {
        if (!Entry->NeedsGeneratedTypeName)
        {
            continue;
        }

        Output << "template<>\n";
        Output << "struct TTypeName<" << Entry->QualifiedName << ">\n";
        Output << "{\n";
        Output << "    static constexpr const char* Value = "
               << CppStringLiteral(Entry->ReflectedName)
               << ";\n";
        Output << "};\n";
        Output << "template<>\n";
        Output << "struct THasDeclaredReflectedTypeName<" << Entry->QualifiedName << "> : std::true_type\n";
        Output << "{\n";
        Output << "};\n";
    }
    Output << "} // namespace SnAPI::GameFramework\n";
    return WriteFileTextIfChanged(OutputPath, Output.str());
}

bool WriteGeneratedTypeNameHeader(const fs::path& OutputPath, const std::vector<TypeSpec>& Types)
{
    return WriteGeneratedTypeNameHeaderEntries(OutputPath, BuildTypeNameCacheEntries(Types));
}

bool RequiresBroadOwnerIncludes(const fs::path& OwnerHeader, const std::vector<TypeSpec>& Types)
{
    const fs::path NormalizedOwner = NormalizePath(OwnerHeader);
    for (const TypeSpec& Type : Types)
    {
        if (!RequiresGeneratedTypeAlias(Type))
        {
            continue;
        }
        if (NormalizePath(Type.Header) == NormalizedOwner)
        {
            continue;
        }
        if (!Type.TypeNameHeader.empty() && NormalizePath(Type.TypeNameHeader) == NormalizedOwner)
        {
            return true;
        }
    }
    return false;
}

bool WriteEmptyGeneratedSourceFile(const fs::path& OutputPath,
                                   const fs::path& OwnerHeader,
                                   const fs::path& HeaderRoot)
{
    std::ostringstream Output{};
    Output << "// Generated by SnAPI.GameFramework ReflectionGen. Do not edit.\n\n";
    Output << "extern \"C\" void " << GeneratedSourceAnchorSymbolFor(OwnerHeader, HeaderRoot) << "()\n";
    Output << "{\n";
    Output << "}\n";
    return WriteFileTextIfChanged(OutputPath, Output.str());
}

bool WriteGeneratedSourceFile(const fs::path& OutputPath,
                              const fs::path& OwnerHeader,
                              const fs::path& HeaderRoot,
                              const std::vector<TypeSpec>& Types,
                              const std::vector<fs::path>& AllHeaders)
{
    std::ostringstream Output{};
    Output << "// Generated by SnAPI.GameFramework ReflectionGen. Do not edit.\n";
    Output << "#include <cstddef>\n";
    Output << "#include <type_traits>\n";
    Output << "#include \"Variant.h\"\n";
    Output << "#include \"TypeAutoRegistration.h\"\n";
    Output << "#include \"TypeName.h\"\n";
    Output << "#include \"TypeRegistry.h\"\n";
    Output << "\n";

    std::unordered_set<std::string> Includes{};
    Includes.insert(OwnerHeader.generic_string());
    if (RequiresBroadOwnerIncludes(OwnerHeader, Types))
    {
        for (const fs::path& Header : AllHeaders)
        {
            Includes.insert(NormalizePath(Header).generic_string());
        }
    }
    for (const TypeSpec& Type : Types)
    {
        Includes.insert(Type.Header.generic_string());
        const fs::path SupportHeader =
            (!Type.TypeNameHeader.empty() && IsHeaderLikePath(Type.TypeNameHeader)) ? Type.TypeNameHeader : fs::path{};
        if (!SupportHeader.empty())
        {
            Includes.insert(SupportHeader.generic_string());
        }
    }
    std::vector<std::string> SortedIncludes(Includes.begin(), Includes.end());
    std::sort(SortedIncludes.begin(), SortedIncludes.end());
    for (const std::string& Include : SortedIncludes)
    {
        Output << "#include " << CppStringLiteral(Include) << "\n";
    }
    Output << "\n";

    Output << "namespace SnAPI::GameFramework::GeneratedReflectionDetail\n";
    Output << "{\n";
    for (std::size_t Index = 0; Index < Types.size(); ++Index)
    {
        if (!RequiresGeneratedTypeAlias(Types[Index]))
        {
            continue;
        }
        Output << "using TGeneratedType_" << Index << " = " << Types[Index].QualifiedName << ";\n";
    }
    Output << "} // namespace SnAPI::GameFramework::GeneratedReflectionDetail\n\n";

    for (const TypeSpec& Type : Types)
    {
        EmitGeneratedRpcDefinitions(Output, Type);
    }

    for (std::size_t Index = 0; Index < Types.size(); ++Index)
    {
        const TypeSpec& Type = Types[Index];
        if (Type.IsEnum)
        {
            EmitEnumRegistration(Output, Type, Index);
        }
        else
        {
            EmitRecordRegistration(Output, Type, Index);
        }
    }

    Output << "extern \"C\" void " << GeneratedSourceAnchorSymbolFor(OwnerHeader, HeaderRoot) << "()\n";
    Output << "{\n";
    Output << "}\n";

    return WriteFileTextIfChanged(OutputPath, Output.str());
}

bool WriteGeneratedSources(const fs::path& GeneratedSourceDir,
                           const fs::path& HeaderRoot,
                           const std::vector<fs::path>& Headers,
                           const std::vector<TypeSpec>& Types)
{
    std::unordered_map<std::string, std::vector<TypeSpec>> TypesByOwner{};
    for (const TypeSpec& Type : Types)
    {
        const fs::path OwnerHeader = GeneratedSourceOwnerHeaderFor(Type);
        TypesByOwner[OwnerHeader.generic_string()].push_back(Type);
    }

    for (const fs::path& Header : Headers)
    {
        const fs::path Normalized = NormalizePath(Header);
        const fs::path Relative = GeneratedSourceRelativePathFor(Normalized, HeaderRoot);
        const fs::path OutputPath = GeneratedSourceDir / Relative;
        const auto It = TypesByOwner.find(Normalized.generic_string());
        if (It == TypesByOwner.end())
        {
            if (!WriteEmptyGeneratedSourceFile(OutputPath, Normalized, HeaderRoot))
            {
                return false;
            }
            continue;
        }

        if (!WriteGeneratedSourceFile(OutputPath, Normalized, HeaderRoot, It->second, Headers))
        {
            return false;
        }
    }

    return true;
}

bool PruneStaleGeneratedArtifacts(const fs::path& GeneratedIncludeDir,
                                  const fs::path& GeneratedSourceDir,
                                  const fs::path& GeneratedTypeNameHeader,
                                  const fs::path& HeaderRoot,
                                  const std::vector<fs::path>& Headers)
{
    std::unordered_set<std::string> ExpectedSources{};
    std::unordered_set<std::string> ExpectedHeaders{};
    ExpectedSources.reserve(Headers.size());
    ExpectedHeaders.reserve(Headers.size());

    for (const fs::path& Header : Headers)
    {
        const fs::path NormalizedHeader = NormalizePath(Header);
        if (!GeneratedSourceDir.empty())
        {
            ExpectedSources.insert(
                NormalizePath(GeneratedSourceDir / GeneratedSourceRelativePathFor(NormalizedHeader, HeaderRoot))
                    .generic_string());
        }
        if (!GeneratedIncludeDir.empty())
        {
            ExpectedHeaders.insert(
                NormalizePath(GeneratedIncludeDir / GeneratedHeaderRelativePathFor(NormalizedHeader, HeaderRoot))
                    .generic_string());
        }
    }

    const fs::path NormalizedTypeNameHeader =
        GeneratedTypeNameHeader.empty() ? fs::path{} : NormalizePath(GeneratedTypeNameHeader);

    auto PruneDirectory = [](const fs::path& Root,
                             const std::string_view ExtensionSuffix,
                             const std::unordered_set<std::string>& Expected,
                             const fs::path& ExcludedPath) {
        if (Root.empty() || !fs::exists(Root))
        {
            return true;
        }

        std::error_code IterateError{};
        for (fs::recursive_directory_iterator It(Root, IterateError), End; It != End; It.increment(IterateError))
        {
            if (IterateError)
            {
                return false;
            }

            if (!It->is_regular_file())
            {
                continue;
            }

            const fs::path Path = NormalizePath(It->path());
            if (!ExcludedPath.empty() && Path == ExcludedPath)
            {
                continue;
            }

            const std::string PathText = Path.generic_string();
            if (!PathText.ends_with(ExtensionSuffix))
            {
                continue;
            }

            if (Expected.contains(PathText))
            {
                continue;
            }

            std::error_code RemoveError{};
            fs::remove(Path, RemoveError);
            if (RemoveError)
            {
                return false;
            }
        }

        return true;
    };

    if (!PruneDirectory(GeneratedSourceDir, ".generated.cpp", ExpectedSources, {}))
    {
        return false;
    }

    if (!PruneDirectory(GeneratedIncludeDir, ".generated.hpp", ExpectedHeaders, NormalizedTypeNameHeader))
    {
        return false;
    }

    return true;
}

bool WriteGeneratedManifest(const fs::path& OutputPath,
                            const fs::path& GeneratedSourceDir,
                            const fs::path& HeaderRoot,
                            const std::vector<fs::path>& Headers)
{
    std::ostringstream Output{};
    Output << "// Generated by SnAPI.GameFramework ReflectionGen. Do not edit.\n";
    Output << "// This file forces per-header generated reflection sources to stay linked.\n";
    Output << "// Generated source dir: " << GeneratedSourceDir.generic_string() << "\n\n";
    for (const fs::path& Header : Headers)
    {
        const fs::path Normalized = NormalizePath(Header);
        Output << "extern \"C\" void " << GeneratedSourceAnchorSymbolFor(Normalized, HeaderRoot) << "();\n";
    }
    Output << "\n";
    Output << "extern \"C\" void " << GeneratedManifestLinkSymbolFor(OutputPath) << "()\n";
    Output << "{\n";
    for (const fs::path& Header : Headers)
    {
        const fs::path Normalized = NormalizePath(Header);
        Output << "    " << GeneratedSourceAnchorSymbolFor(Normalized, HeaderRoot) << "();\n";
    }
    Output << "}\n";
    return WriteFileTextIfChanged(OutputPath, Output.str());
}

fs::path ReflectionCachePathForOutput(const fs::path& OutputPath)
{
    fs::path CachePath = OutputPath;
    CachePath += ".cache";
    return CachePath;
}

std::vector<TypeNameCacheEntry> CollectCachedTypeNameEntries(const ReflectionCache& Cache,
                                                             const std::vector<fs::path>& Headers)
{
    std::vector<TypeNameCacheEntry> Result{};
    for (const fs::path& Header : Headers)
    {
        const auto It = Cache.Headers.find(NormalizePath(Header).generic_string());
        if (It == Cache.Headers.end())
        {
            continue;
        }
        Result.insert(Result.end(), It->second.TypeNameEntries.begin(), It->second.TypeNameEntries.end());
    }
    return Result;
}

struct Options
{
    fs::path Output{};
    fs::path BuildDir{};
    fs::path SeedSource{};
    fs::path ProjectRoot{};
    fs::path GeneratedIncludeDir{};
    fs::path GeneratedSourceDir{};
    fs::path GeneratedTypeNameHeader{};
    fs::path HeaderRoot{};
    std::vector<std::string> ExtraArgs{};
    std::vector<fs::path> Headers{};
};

Options ParseArguments(const int Argc, char** Argv)
{
    Options Result{};
    for (int Index = 1; Index < Argc; ++Index)
    {
        const std::string Arg = Argv[Index];
        if (Arg == "--output" && Index + 1 < Argc)
        {
            Result.Output = fs::path(Argv[++Index]);
            continue;
        }
        if (Arg == "--build-dir" && Index + 1 < Argc)
        {
            Result.BuildDir = fs::path(Argv[++Index]);
            continue;
        }
        if (Arg == "--seed-source" && Index + 1 < Argc)
        {
            Result.SeedSource = fs::path(Argv[++Index]);
            continue;
        }
        if (Arg == "--project-root" && Index + 1 < Argc)
        {
            Result.ProjectRoot = fs::path(Argv[++Index]);
            continue;
        }
        if (Arg == "--generated-include-dir" && Index + 1 < Argc)
        {
            Result.GeneratedIncludeDir = fs::path(Argv[++Index]);
            continue;
        }
        if (Arg == "--generated-source-dir" && Index + 1 < Argc)
        {
            Result.GeneratedSourceDir = fs::path(Argv[++Index]);
            continue;
        }
        if (Arg == "--generated-type-name-header" && Index + 1 < Argc)
        {
            Result.GeneratedTypeNameHeader = fs::path(Argv[++Index]);
            continue;
        }
        if (Arg == "--header-root" && Index + 1 < Argc)
        {
            Result.HeaderRoot = fs::path(Argv[++Index]);
            continue;
        }
        if (Arg == "--extra-arg" && Index + 1 < Argc)
        {
            Result.ExtraArgs.emplace_back(Argv[++Index]);
            continue;
        }

        Result.Headers.push_back(fs::path(Arg));
    }

    if (Result.Output.empty() || Result.BuildDir.empty() || Result.SeedSource.empty())
    {
        throw std::runtime_error(
            "Usage: ReflectionGen --output <file> --build-dir <dir> --seed-source <source> "
            "[--project-root <dir>] [--generated-include-dir <dir> --generated-source-dir <dir> "
            "--generated-type-name-header <file> "
            "--header-root <dir>] <headers...>");
    }

    const auto NormalizeOptionPath = [](fs::path& Path) {
        if (Path.empty())
        {
            return;
        }
        if (!Path.is_absolute())
        {
            Path = fs::absolute(Path);
        }
        Path = Path.lexically_normal();
    };

    NormalizeOptionPath(Result.Output);
    NormalizeOptionPath(Result.BuildDir);
    NormalizeOptionPath(Result.SeedSource);
    NormalizeOptionPath(Result.ProjectRoot);
    NormalizeOptionPath(Result.GeneratedIncludeDir);
    NormalizeOptionPath(Result.GeneratedSourceDir);
    NormalizeOptionPath(Result.GeneratedTypeNameHeader);
    NormalizeOptionPath(Result.HeaderRoot);
    for (fs::path& Header : Result.Headers)
    {
        NormalizeOptionPath(Header);
    }

    return Result;
}

void PrintDiagnostics(const std::vector<Diagnostic>& Diagnostics)
{
    for (const Diagnostic& DiagnosticEntry : Diagnostics)
    {
        std::cerr << DiagnosticEntry.File.string();
        if (DiagnosticEntry.Line != 0)
        {
            std::cerr << ':' << DiagnosticEntry.Line;
            if (DiagnosticEntry.Column != 0)
            {
                std::cerr << ':' << DiagnosticEntry.Column;
            }
        }
        std::cerr << ": " << (DiagnosticEntry.Severity == DiagnosticSeverity::Warning ? "warning: " : "")
                  << DiagnosticEntry.Message << '\n';
    }
}

bool ReflectionProfileEnabled()
{
    const char* const Value = std::getenv("SNAPI_REFLECTION_PROFILE");
    return Value != nullptr && Value[0] != '\0' && std::string_view(Value) != "0";
}

void PrintReflectionTiming(const std::string_view Label,
                           const std::chrono::steady_clock::duration Duration)
{
    const auto Milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(Duration).count();
    std::cerr << "[ReflectionGen] " << Label << ": " << Milliseconds << " ms\n";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        using Clock = std::chrono::steady_clock;

        const bool ProfileEnabled = ReflectionProfileEnabled();
        const auto StartedAt = Clock::now();
        const Options Options = ParseArguments(argc, argv);
        std::vector<Diagnostic> Diagnostics{};
        auto StageStartedAt = Clock::now();
        std::vector<std::string> CompileArgs =
            LoadCompileArguments(
                Options.BuildDir,
                Options.SeedSource.lexically_normal(),
                Options.GeneratedTypeNameHeader.lexically_normal());
        CompileArgs.insert(CompileArgs.end(), Options.ExtraArgs.begin(), Options.ExtraArgs.end());
        if (ProfileEnabled)
        {
            PrintReflectionTiming("load_compile_args", Clock::now() - StageStartedAt);
        }
        if (!Options.GeneratedIncludeDir.empty())
        {
            StageStartedAt = Clock::now();
            if (Options.HeaderRoot.empty())
            {
                std::cerr << "--generated-include-dir requires --header-root\n";
                return 1;
            }
            if (!WriteBootstrapGeneratedSidecarHeaders(
                    Options.GeneratedIncludeDir, Options.HeaderRoot, Options.Headers))
            {
                std::cerr << "Failed to prepare generated header bootstrap files in: " << Options.GeneratedIncludeDir
                          << '\n';
                return 1;
            }
            if (ProfileEnabled)
            {
                PrintReflectionTiming("bootstrap_generated_headers", Clock::now() - StageStartedAt);
            }
        }
        if (!Options.GeneratedTypeNameHeader.empty())
        {
            StageStartedAt = Clock::now();
            if (!WriteBootstrapGeneratedTypeNameHeader(Options.GeneratedTypeNameHeader))
            {
                std::cerr << "Failed to prepare generated type-name header bootstrap file: "
                          << Options.GeneratedTypeNameHeader << '\n';
                return 1;
            }
            if (ProfileEnabled)
            {
                PrintReflectionTiming("bootstrap_type_name_header", Clock::now() - StageStartedAt);
            }
        }

        StageStartedAt = Clock::now();
        const fs::path CachePath = ReflectionCachePathForOutput(Options.Output);
        const std::optional<ReflectionCache> ExistingCache = LoadReflectionCache(CachePath);
        ReflectionCache Cache = ExistingCache.value_or(ReflectionCache{});
        Cache.SchemaVersion = kReflectionCacheSchemaVersion;
        if (ProfileEnabled)
        {
            PrintReflectionTiming("load_cache", Clock::now() - StageStartedAt);
        }

        StageStartedAt = Clock::now();
        const RegistrationScanData RegistrationScan = BuildRegistrationScanData(Options.ProjectRoot, Options.SeedSource);
        if (ProfileEnabled)
        {
            PrintReflectionTiming("scan_registration_sources", Clock::now() - StageStartedAt);
        }

        const std::uint64_t CompileArgsHash = HashStringVector(CompileArgs);
        StageStartedAt = Clock::now();
        const RegistrationKnowledgeBuildResult KnowledgeBuild = BuildRegistrationKnowledgeFromScanData(
            RegistrationScan,
            Options.Headers,
            CompileArgs,
            CompileArgsHash,
            Options.BuildDir,
            ExistingCache ? &*ExistingCache : nullptr);
        const RegistrationKnowledge& Knowledge = KnowledgeBuild.Knowledge;
        Cache.RegistrationScanFingerprint = RegistrationScan.Fingerprint;
        Cache.RegistrationCompileArgsHash = CompileArgsHash;
        Cache.Knowledge = Knowledge;
        Cache.RegistrationFiles = KnowledgeBuild.FileEntries;
        Cache.KnowledgeFingerprint = ComputeKnowledgeFingerprint(Knowledge);
        if (ProfileEnabled)
        {
            PrintReflectionTiming("build_registration_knowledge", Clock::now() - StageStartedAt);
        }

        if (Options.GeneratedSourceDir.empty())
        {
            StageStartedAt = Clock::now();
            const ParsedHeaderArtifacts Parsed =
                ParseAnnotatedHeaders(
                    Options.Headers, Options.Headers, CompileArgs, Options.BuildDir, Knowledge, nullptr, Diagnostics);
            const std::vector<TypeSpec>& Types = Parsed.Types;
            if (ProfileEnabled)
            {
                PrintReflectionTiming("parse_annotated_headers", Clock::now() - StageStartedAt);
            }

            if (!Diagnostics.empty())
            {
                PrintDiagnostics(Diagnostics);
                if (HasErrors(Diagnostics))
                {
                    return 1;
                }
            }

            const fs::path LegacyHeaderRoot =
                Options.HeaderRoot.empty() ? Options.SeedSource.parent_path() : Options.HeaderRoot;
            StageStartedAt = Clock::now();
            if (!WriteGeneratedSourceFile(Options.Output, Options.SeedSource, LegacyHeaderRoot, Types, Options.Headers))
            {
                std::cerr << "Failed to write generated output: " << Options.Output << '\n';
                return 1;
            }
            if (ProfileEnabled)
            {
                PrintReflectionTiming("write_legacy_generated_source", Clock::now() - StageStartedAt);
            }
            if (!Options.GeneratedIncludeDir.empty())
            {
                StageStartedAt = Clock::now();
                if (!WriteGeneratedSidecarHeaders(Options.GeneratedIncludeDir, Options.HeaderRoot, Options.Headers, Types))
                {
                    std::cerr << "Failed to write generated headers into: " << Options.GeneratedIncludeDir << '\n';
                    return 1;
                }
                if (ProfileEnabled)
                {
                    PrintReflectionTiming("write_generated_headers", Clock::now() - StageStartedAt);
                }
            }
            if (!Options.GeneratedTypeNameHeader.empty())
            {
                StageStartedAt = Clock::now();
                if (!WriteGeneratedTypeNameHeader(Options.GeneratedTypeNameHeader, Types))
                {
                    std::cerr << "Failed to write generated type-name header: " << Options.GeneratedTypeNameHeader
                              << '\n';
                    return 1;
                }
                if (ProfileEnabled)
                {
                    PrintReflectionTiming("write_type_name_header", Clock::now() - StageStartedAt);
                }
            }
            if ((!Options.GeneratedSourceDir.empty() || !Options.GeneratedIncludeDir.empty()) &&
                !PruneStaleGeneratedArtifacts(
                    Options.GeneratedIncludeDir,
                    Options.GeneratedSourceDir,
                    Options.GeneratedTypeNameHeader,
                    Options.HeaderRoot,
                    Options.Headers))
            {
                std::cerr << "Failed to prune stale generated reflection artifacts\n";
                return 1;
            }

            if (ProfileEnabled)
            {
                PrintReflectionTiming("total", Clock::now() - StartedAt);
            }
            return 0;
        }

        const std::uint64_t KnowledgeFingerprint = Cache.KnowledgeFingerprint;
        StageStartedAt = Clock::now();
        const std::unordered_map<std::string, HeaderMarkers> HeaderMarkers =
            ScanReflectionMarkers(Options.Headers, Diagnostics);
        const std::unordered_map<std::string, HeaderScanInfo> HeaderInfoByPath =
            BuildHeaderScanInfo(Options.Headers, HeaderMarkers);
        if (ProfileEnabled)
        {
            PrintReflectionTiming("scan_headers", Clock::now() - StageStartedAt);
        }

        std::unordered_set<std::string> CurrentHeaderKeys{};
        CurrentHeaderKeys.reserve(Options.Headers.size());
        for (const fs::path& Header : Options.Headers)
        {
            CurrentHeaderKeys.insert(NormalizePath(Header).generic_string());
        }
        for (auto It = Cache.Headers.begin(); It != Cache.Headers.end();)
        {
            if (!CurrentHeaderKeys.contains(It->first))
            {
                It = Cache.Headers.erase(It);
            }
            else
            {
                ++It;
            }
        }

        const bool DirtyAll =
            !ExistingCache ||
            ExistingCache->SchemaVersion != kReflectionCacheSchemaVersion;
        const bool KnowledgeChanged = ExistingCache && ExistingCache->KnowledgeFingerprint != KnowledgeFingerprint;
        const bool HasKnowledgeDependencyCache =
            ExistingCache && ExistingCache->SchemaVersion >= 6;
        const std::unordered_set<std::string> ChangedKnowledgeKeys =
            KnowledgeChanged && ExistingCache
                ? ComputeChangedKnowledgeKeys(ExistingCache->Knowledge, Knowledge)
                : std::unordered_set<std::string>{};
        std::unordered_set<std::string> DirtyHeaderKeys{};
        std::unordered_set<std::string> DirtyExpressionSourceKeys{};
        std::unordered_set<std::string> KnowledgeOnlyDirtyHeaderKeys{};
        std::size_t DirtyForKnowledgeCount = 0;
        std::size_t DirtyForFingerprintCount = 0;
        std::size_t DirtyForOutputCount = 0;
        std::size_t DirtyForAllCount = 0;
        std::size_t LegacyFingerprintUpgradeCount = 0;

        auto MarkDirty = [&DirtyHeaderKeys](const std::string& HeaderKey) {
            DirtyHeaderKeys.insert(HeaderKey);
        };

        for (const fs::path& Header : Options.Headers)
        {
            const fs::path NormalizedHeader = NormalizePath(Header);
            const std::string HeaderKey = NormalizedHeader.generic_string();
            const HeaderScanInfo& HeaderInfo = HeaderInfoByPath.at(HeaderKey);
            const std::uint64_t Fingerprint = ComputeHeaderFingerprint(HeaderInfo, CompileArgsHash);

            bool MissingOutput = false;
            const fs::path GeneratedSourcePath =
                Options.GeneratedSourceDir / GeneratedSourceRelativePathFor(NormalizedHeader, Options.HeaderRoot);
            MissingOutput = MissingOutput || !fs::exists(GeneratedSourcePath);
            if (!Options.GeneratedIncludeDir.empty())
            {
                const fs::path GeneratedHeaderPath =
                    Options.GeneratedIncludeDir / GeneratedHeaderRelativePathFor(NormalizedHeader, Options.HeaderRoot);
                MissingOutput = MissingOutput || !fs::exists(GeneratedHeaderPath);
            }

            const auto CacheIt = Cache.Headers.find(HeaderKey);
            const bool FingerprintMatchesLegacy =
                ExistingCache &&
                CacheIt != Cache.Headers.end() &&
                (CacheIt->second.Fingerprint ==
                     ComputeVersionedHeaderFingerprint(
                         ExistingCache->SchemaVersion,
                         HeaderInfo,
                         CompileArgsHash) ||
                 CacheIt->second.Fingerprint ==
                     ComputeVersionedHeaderFingerprintWithKnowledge(
                         ExistingCache->SchemaVersion,
                         HeaderInfo,
                         CompileArgsHash,
                         ExistingCache->KnowledgeFingerprint));
            const bool NeedsLegacyFingerprintUpgrade =
                CacheIt != Cache.Headers.end() &&
                CacheIt->second.Fingerprint != Fingerprint &&
                FingerprintMatchesLegacy;
            bool DirtyForKnowledge = false;
            if (KnowledgeChanged && HeaderInfo.IsTemplatePrimary)
            {
                DirtyForKnowledge = true;
                if (HasKnowledgeDependencyCache &&
                    CacheIt != Cache.Headers.end() &&
                    !ChangedKnowledgeKeys.empty())
                {
                    DirtyForKnowledge = HeaderDependsOnChangedKnowledge(CacheIt->second, ChangedKnowledgeKeys);
                }
            }
            const bool DirtyForFingerprint =
                CacheIt == Cache.Headers.end() ||
                (CacheIt->second.Fingerprint != Fingerprint && !FingerprintMatchesLegacy);
            const bool DirtyForExpressionPropagation =
                HeaderInfo.HasTypeExpressions && (DirtyAll || DirtyForFingerprint);
            const bool DirtyForKnowledgeOnly =
                DirtyForKnowledge &&
                !DirtyAll &&
                !MissingOutput &&
                !DirtyForFingerprint &&
                CacheIt != Cache.Headers.end();
            if (DirtyAll ||
                DirtyForKnowledge ||
                MissingOutput ||
                DirtyForFingerprint)
            {
                DirtyForAllCount += DirtyAll ? 1u : 0u;
                DirtyForKnowledgeCount += DirtyForKnowledge ? 1u : 0u;
                DirtyForOutputCount += MissingOutput ? 1u : 0u;
                DirtyForFingerprintCount += DirtyForFingerprint ? 1u : 0u;
                MarkDirty(HeaderKey);
                if (DirtyForExpressionPropagation)
                {
                    DirtyExpressionSourceKeys.insert(HeaderKey);
                }
                if (DirtyForKnowledgeOnly)
                {
                    KnowledgeOnlyDirtyHeaderKeys.insert(HeaderKey);
                }
                continue;
            }

            if (NeedsLegacyFingerprintUpgrade)
            {
                CacheIt->second.Fingerprint = Fingerprint;
                ++LegacyFingerprintUpgradeCount;
            }
        }

        if (!DirtyExpressionSourceKeys.empty())
        {
            for (const auto& [HeaderKey, Info] : HeaderInfoByPath)
            {
                if (Info.IsTemplatePrimary)
                {
                    DirtyHeaderKeys.insert(HeaderKey);
                }
            }
        }

        bool NeedAllExpressionHeaders = false;
        for (const std::string& HeaderKey : DirtyHeaderKeys)
        {
            if (const auto InfoIt = HeaderInfoByPath.find(HeaderKey);
                InfoIt != HeaderInfoByPath.end() &&
                InfoIt->second.IsTemplatePrimary)
            {
                NeedAllExpressionHeaders = true;
                break;
            }
        }
        if (ProfileEnabled)
        {
            std::cerr << "[ReflectionGen] header_invalidation:"
                      << " knowledge_changed=" << (KnowledgeChanged ? 1 : 0)
                      << " existing_knowledge=" << (ExistingCache ? ExistingCache->KnowledgeFingerprint : 0)
                      << " current_knowledge=" << KnowledgeFingerprint
                      << " existing_registered=" << (ExistingCache ? ExistingCache->Knowledge.RegisteredTypeKeys.size() : 0)
                      << " current_registered=" << Knowledge.RegisteredTypeKeys.size()
                      << " existing_visible=" << (ExistingCache ? ExistingCache->Knowledge.HeaderVisibleTypeKeys.size() : 0)
                      << " current_visible=" << Knowledge.HeaderVisibleTypeKeys.size()
                      << " changed_keys=" << ChangedKnowledgeKeys.size()
                      << " dirty=" << DirtyHeaderKeys.size()
                      << " expression_sources=" << DirtyExpressionSourceKeys.size()
                      << " dirty_for_all=" << DirtyForAllCount
                      << " dirty_for_knowledge=" << DirtyForKnowledgeCount
                      << " dirty_for_output=" << DirtyForOutputCount
                      << " dirty_for_fingerprint=" << DirtyForFingerprintCount
                      << " legacy_upgrades=" << LegacyFingerprintUpgradeCount
                      << " need_all_expression_headers=" << (NeedAllExpressionHeaders ? 1 : 0)
                      << '\n';
        }

        std::vector<fs::path> DirtyHeaders{};
        DirtyHeaders.reserve(DirtyHeaderKeys.size());
        std::vector<fs::path> ExpressionHeaders{};
        ExpressionHeaders.reserve(Options.Headers.size());
        std::unordered_map<std::string, std::vector<TemplateSpecializationCandidate>> ReusableTemplateCandidatesByHeader{};
        for (const fs::path& Header : Options.Headers)
        {
            const fs::path NormalizedHeader = NormalizePath(Header);
            const std::string HeaderKey = NormalizedHeader.generic_string();
            if (DirtyHeaderKeys.contains(NormalizedHeader.generic_string()))
            {
                DirtyHeaders.push_back(NormalizedHeader);
            }
            if (DirtyHeaderKeys.contains(HeaderKey) ||
                (NeedAllExpressionHeaders &&
                 HeaderInfoByPath.contains(HeaderKey) &&
                 HeaderInfoByPath.at(HeaderKey).HasTypeExpressions))
            {
                ExpressionHeaders.push_back(NormalizedHeader);
            }
            if (ExistingCache &&
                ExistingCache->SchemaVersion >= 7 &&
                KnowledgeOnlyDirtyHeaderKeys.contains(HeaderKey) &&
                Cache.Headers.contains(HeaderKey))
            {
                ReusableTemplateCandidatesByHeader.emplace(HeaderKey, Cache.Headers.at(HeaderKey).TemplateCandidates);
            }
        }

        ParsedHeaderArtifacts ParsedDirty{};
        if (!DirtyHeaders.empty())
        {
            StageStartedAt = Clock::now();
            ParsedDirty = ParseAnnotatedHeaders(
                DirtyHeaders,
                ExpressionHeaders,
                CompileArgs,
                Options.BuildDir,
                Knowledge,
                ReusableTemplateCandidatesByHeader.empty() ? nullptr : &ReusableTemplateCandidatesByHeader,
                Diagnostics);
            if (ProfileEnabled)
            {
                PrintReflectionTiming("parse_dirty_headers", Clock::now() - StageStartedAt);
            }
        }
        std::vector<TypeSpec>& DirtyTypes = ParsedDirty.Types;

        if (!Diagnostics.empty())
        {
            PrintDiagnostics(Diagnostics);
            if (HasErrors(Diagnostics))
            {
                return 1;
            }
        }

        if (!DirtyHeaders.empty())
        {
            StageStartedAt = Clock::now();
            if (!WriteGeneratedSources(Options.GeneratedSourceDir, Options.HeaderRoot, DirtyHeaders, DirtyTypes))
            {
                std::cerr << "Failed to write generated sources into: " << Options.GeneratedSourceDir << '\n';
                return 1;
            }
            if (ProfileEnabled)
            {
                PrintReflectionTiming("write_generated_sources", Clock::now() - StageStartedAt);
            }
            if (!Options.GeneratedIncludeDir.empty())
            {
                StageStartedAt = Clock::now();
                if (!WriteGeneratedSidecarHeaders(
                        Options.GeneratedIncludeDir, Options.HeaderRoot, DirtyHeaders, DirtyTypes))
                {
                    std::cerr << "Failed to write generated headers into: " << Options.GeneratedIncludeDir << '\n';
                    return 1;
                }
                if (ProfileEnabled)
                {
                    PrintReflectionTiming("write_dirty_headers", Clock::now() - StageStartedAt);
                }
            }
        }

        std::unordered_map<std::string, std::vector<TypeSpec>> DirtyTypesByOwner{};
        for (const TypeSpec& Type : DirtyTypes)
        {
            const fs::path OwnerHeader = GeneratedSourceOwnerHeaderFor(Type);
            DirtyTypesByOwner[OwnerHeader.generic_string()].push_back(Type);
        }

        for (const fs::path& Header : DirtyHeaders)
        {
            const fs::path NormalizedHeader = NormalizePath(Header);
            const std::string HeaderKey = NormalizedHeader.generic_string();
            const HeaderScanInfo& HeaderInfo = HeaderInfoByPath.at(HeaderKey);
            HeaderCacheEntry Entry{};
            Entry.Fingerprint = ComputeHeaderFingerprint(HeaderInfo, CompileArgsHash);
            Entry.HasMarkers = HeaderInfo.HasMarkers;
            Entry.HasTypeExpressions = HeaderInfo.HasTypeExpressions;
            Entry.IsTemplatePrimary = HeaderInfo.IsTemplatePrimary;
            if (const auto DependenciesIt = ParsedDirty.KnowledgeDependenciesByHeader.find(HeaderKey);
                DependenciesIt != ParsedDirty.KnowledgeDependenciesByHeader.end())
            {
                Entry.KnowledgeDependencies = DependenciesIt->second;
            }
            if (const auto CandidatesIt = ParsedDirty.TemplateCandidatesByHeader.find(HeaderKey);
                CandidatesIt != ParsedDirty.TemplateCandidatesByHeader.end())
            {
                Entry.TemplateCandidates = CandidatesIt->second;
            }
            if (const auto TypesIt = DirtyTypesByOwner.find(HeaderKey); TypesIt != DirtyTypesByOwner.end())
            {
                Entry.TypeNameEntries = BuildTypeNameCacheEntries(TypesIt->second);
            }
            Cache.Headers[HeaderKey] = std::move(Entry);
        }

        Cache.KnowledgeFingerprint = KnowledgeFingerprint;

        StageStartedAt = Clock::now();
        if (!WriteGeneratedManifest(Options.Output, Options.GeneratedSourceDir, Options.HeaderRoot, Options.Headers))
        {
            std::cerr << "Failed to write generated manifest: " << Options.Output << '\n';
            return 1;
        }
        if (ProfileEnabled)
        {
            PrintReflectionTiming("write_manifest", Clock::now() - StageStartedAt);
        }
        if (!Options.GeneratedTypeNameHeader.empty())
        {
            const std::vector<TypeNameCacheEntry> CachedTypeNameEntries =
                CollectCachedTypeNameEntries(Cache, Options.Headers);
            StageStartedAt = Clock::now();
            if (!WriteGeneratedTypeNameHeaderEntries(Options.GeneratedTypeNameHeader, CachedTypeNameEntries))
            {
                std::cerr << "Failed to write generated type-name header: " << Options.GeneratedTypeNameHeader
                          << '\n';
                return 1;
            }
            if (ProfileEnabled)
            {
                PrintReflectionTiming("write_cached_type_name_header", Clock::now() - StageStartedAt);
            }
        }
        if ((!Options.GeneratedSourceDir.empty() || !Options.GeneratedIncludeDir.empty()) &&
            !PruneStaleGeneratedArtifacts(
                Options.GeneratedIncludeDir,
                Options.GeneratedSourceDir,
                Options.GeneratedTypeNameHeader,
                Options.HeaderRoot,
                Options.Headers))
        {
            std::cerr << "Failed to prune stale generated reflection artifacts\n";
            return 1;
        }
        StageStartedAt = Clock::now();
        if (!SaveReflectionCache(CachePath, Cache))
        {
            std::cerr << "Failed to write reflection cache: " << CachePath << '\n';
            return 1;
        }
        if (ProfileEnabled)
        {
            PrintReflectionTiming("save_cache", Clock::now() - StageStartedAt);
            PrintReflectionTiming("total", Clock::now() - StartedAt);
        }

        return 0;
    }
    catch (const std::exception& Ex)
    {
        std::cerr << Ex.what() << '\n';
        return 1;
    }
}
