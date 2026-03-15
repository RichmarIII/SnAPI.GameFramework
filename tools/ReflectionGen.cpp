#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/Documentation.h>
#include <clang-c/Index.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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
};

struct ReflectionMarker
{
    fs::path File{};
    unsigned Line = 0;
    unsigned Column = 0;
    MarkerKind Kind = MarkerKind::Type;
    std::string MacroName{};
    AnnotationPayload Payload{};
};

struct HeaderMarkers
{
    std::vector<ReflectionMarker> Type{};
    std::vector<ReflectionMarker> Field{};
    std::vector<ReflectionMarker> Function{};
    std::vector<ReflectionMarker> EnumValue{};
};

struct HeaderMarkerCursor
{
    std::size_t NextType = 0;
    std::size_t NextField = 0;
    std::size_t NextFunction = 0;
    std::size_t NextEnumValue = 0;
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
    std::string MinExpr{};
    std::string MaxExpr{};
    std::string StepExpr{};
    std::string ConditionExpr{};
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
    std::string QualifiedName{};
    std::string DisplayName{};
    std::string Category{};
    std::string Doc{};
    bool IsEnum = false;
    bool EnumIsSigned = false;
    bool IsInterface = false;
    bool HasDefaultConstructor = false;
    std::optional<ConstructorSpec> DefaultConstructor{};
    std::vector<BaseSpec> Bases{};
    std::vector<FieldSpec> Fields{};
    std::vector<MethodSpec> Methods{};
    std::vector<EnumValueSpec> EnumValues{};
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
};

struct RegistrationKnowledge
{
    std::unordered_set<std::string> RegisteredTypeKeys{};
};

Diagnostic MakeDiagnostic(const CXCursor Cursor, std::string Message);
Diagnostic MakeDiagnostic(const fs::path& File, unsigned Line, unsigned Column, std::string Message);
Diagnostic MakeWarning(const CXCursor Cursor, std::string Message);
Diagnostic MakeWarning(const fs::path& File, unsigned Line, unsigned Column, std::string Message);

std::string ToStringDispose(CXString Value)
{
    const char* const Text = clang_getCString(Value);
    std::string Result = Text ? Text : "";
    clang_disposeString(Value);
    return Result;
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
    return fs::path(Path).lexically_normal();
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

std::vector<ScopedTypeExpression> ScanTypeExpressionsInFile(const fs::path& File)
{
    std::ifstream Stream(File);
    if (!Stream.is_open())
    {
        return {};
    }

    const std::string Contents((std::istreambuf_iterator<char>(Stream)), std::istreambuf_iterator<char>());

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
                });
            };

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

        if (Fragment == "SnReplicated" || Fragment == "SNAPI_REPLICATED")
        {
            AddFlag("replicated");
            continue;
        }

        if (Fragment == "SnSerialized" || Fragment == "SNAPI_SERIALIZED")
        {
            AddFlag("serialized");
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
            }
        }

        Result.emplace(Header.generic_string(), std::move(FileMarkers));
    }

    return Result;
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

std::string ReflectableTypeCondition(const CXType Type, const CXCursor Context)
{
    const std::string TypeExpr = PrettyPrintedTypeForCode(Type, Context);
    if (TypeExpr.empty())
    {
        return "false";
    }
    return "::SnAPI::GameFramework::CHasReflectedTypeName<" + TypeExpr + ">";
}

bool IsRegisteredTypeKey(const CXType Type, const CXCursor Context, const RegistrationKnowledge& Knowledge)
{
    const std::string OriginalKey = NormalizeTypeExpressionString(PrettyPrintedTypeForCode(Type, Context));
    if (!OriginalKey.empty() && Knowledge.RegisteredTypeKeys.contains(OriginalKey))
    {
        return true;
    }

    const CXType Canonical = clang_getCanonicalType(Type);
    const std::string CanonicalKey = NormalizeTypeExpressionString(PrettyPrintedTypeForCode(Canonical, Context));
    return !CanonicalKey.empty() && Knowledge.RegisteredTypeKeys.contains(CanonicalKey);
}

bool IsTypeReflectionCompatible(const CXType Type, const CXCursor Context, const RegistrationKnowledge& Knowledge)
{
    if (IsRegisteredTypeKey(Type, Context, Knowledge))
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
    }

    return Markers.Type;
}

std::size_t& NextMarkerIndexForKind(HeaderMarkerCursor& Cursor, const MarkerKind Kind)
{
    switch (Kind)
    {
    case MarkerKind::Type:
        return Cursor.NextType;
    case MarkerKind::Field:
        return Cursor.NextField;
    case MarkerKind::Function:
        return Cursor.NextFunction;
    case MarkerKind::EnumValue:
        return Cursor.NextEnumValue;
    }

    return Cursor.NextType;
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

    CXSourceLocation Location = clang_getCursorLocation(Cursor);
    CXFile IgnoredFile{};
    unsigned Line = 0;
    unsigned Column = 0;
    unsigned Offset = 0;
    clang_getFileLocation(Location, &IgnoredFile, &Line, &Column, &Offset);

    const std::vector<ReflectionMarker>& Markers = MarkerVectorForKind(MarkersIt->second, Kind);
    std::size_t& NextIndex = NextMarkerIndexForKind(CursorsIt->second, Kind);
    if (NextIndex >= Markers.size())
    {
        return nullptr;
    }

    const ReflectionMarker& Marker = Markers[NextIndex];
    if (Marker.Line < Line || (Marker.Line == Line && Marker.Column <= Column))
    {
        if (Consume)
        {
            ++NextIndex;
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
    if (Payload.Flags.contains("replicated"))
    {
        Bits.push_back("::SnAPI::GameFramework::EFieldFlagBits::Replication");
    }
    if (Payload.Flags.contains("serialized"))
    {
        Bits.push_back("::SnAPI::GameFramework::EFieldFlagBits::Serialized");
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

std::string BuildMethodFlagsExpression(const AnnotationPayload& Payload)
{
    std::vector<std::string> Bits{};
    if (Payload.Flags.contains("editor_action"))
    {
        Bits.push_back("::SnAPI::GameFramework::EMethodFlagBits::EditorAction");
    }

    const auto RpcIt = Payload.Values.find("rpc");
    if (RpcIt != Payload.Values.end())
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
    if (NetIt != Payload.Values.end())
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

std::string MethodPointerExpression(const std::string& OwnerQualifiedName, const CXCursor MethodCursor)
{
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

    return "static_cast<" + PointerType + ">(&" + OwnerQualifiedName + "::" +
           ToStringDispose(clang_getCursorSpelling(MethodCursor)) + ")";
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

    if (clang_getCXXAccessSpecifier(Cursor) != CX_CXXPublic)
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnField is currently supported only on public data members"));
        return false;
    }

    if (clang_Cursor_getStorageClass(Cursor) == CX_SC_Static)
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnField does not support static data members"));
        return false;
    }

    if (!ValidatePayload(
            Payload,
            {"display_name", "category", "rep", "min", "max", "step"},
            {"replicated", "serialized"},
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
    Out.Name = ToStringDispose(clang_getCursorSpelling(Cursor));
    Out.DisplayName = Payload.Values.contains("display_name") ? Payload.Values.at("display_name") : "";
    Out.Category = Payload.Values.contains("category") ? Payload.Values.at("category") : "";
    Out.Doc = Comment.Doc;
    Out.FlagsExpr = BuildFieldFlagsExpression(Payload);
    Out.MinExpr = Payload.Values.contains("min") ? Payload.Values.at("min") : "";
    Out.MaxExpr = Payload.Values.contains("max") ? Payload.Values.at("max") : "";
    Out.StepExpr = Payload.Values.contains("step") ? Payload.Values.at("step") : "";

    const CXType FieldType = clang_getCursorType(Cursor);
    if (!IsTypeReflectionCompatible(FieldType, Cursor, *Context.Knowledge))
    {
        Diagnostics.push_back(MakeWarning(
            Cursor,
            "Skipping SnField '" + Out.Name + "' because field type '" +
                PrettyPrintedTypeForCode(FieldType, Cursor) + "' is not registered for reflection"));
        return false;
    }

    Out.ConditionExpr = ReflectableTypeCondition(clang_getCursorType(Cursor), Cursor);
    return true;
}

bool PopulateMethodSpec(const CXCursor Cursor,
                        const std::string& OwnerQualifiedName,
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

    const int ExceptionKind = clang_getCursorExceptionSpecificationType(Cursor);
    if (ExceptionKind == CXCursor_ExceptionSpecificationKind_BasicNoexcept ||
        ExceptionKind == CXCursor_ExceptionSpecificationKind_ComputedNoexcept ||
        ExceptionKind == CXCursor_ExceptionSpecificationKind_NoThrow)
    {
        Diagnostics.push_back(MakeDiagnostic(Cursor, "SnFunction does not yet support noexcept methods"));
        return false;
    }

    if (!ValidatePayload(Payload, {"display_name", "category", "rpc", "net"}, {"editor_action"}, Cursor, Diagnostics))
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
    Out.Name = ToStringDispose(clang_getCursorSpelling(Cursor));
    Out.PointerExpr = MethodPointerExpression(OwnerQualifiedName, Cursor);
    Out.DisplayName = Payload.Values.contains("display_name") ? Payload.Values.at("display_name") : "";
    Out.Category = Payload.Values.contains("category") ? Payload.Values.at("category") : "";
    Out.Doc = Comment.Doc;
    Out.FlagsExpr = BuildMethodFlagsExpression(Payload);

    std::vector<std::string> CompatibilityIssues{};
    const CXType ResultType = clang_getResultType(clang_getCursorType(Cursor));
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

TypeSpec BuildRecordSpec(const fs::path& Header,
                         const CXCursor Cursor,
                         const AnnotationPayload& TypePayload,
                         HeaderParseContext& Context,
                         std::vector<Diagnostic>& Diagnostics)
{
    TypeSpec Spec{};
    Spec.Header = Header;
    Spec.QualifiedName = QualifiedNameForCursor(Cursor);
    Spec.IsInterface = false;
    Spec.HasDefaultConstructor = false;

    if (!ValidatePayload(TypePayload, {"display_name", "category"}, {"interface"}, Cursor, Diagnostics))
    {
        return Spec;
    }
    Spec.DisplayName = TypePayload.Values.contains("display_name") ? TypePayload.Values.at("display_name") : "";
    Spec.Category = TypePayload.Values.contains("category") ? TypePayload.Values.at("category") : "";
    Spec.IsInterface = TypePayload.Flags.contains("interface");

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
                MethodSpec Method{};
                if (PopulateMethodSpec(Child, *State->OwnerQualifiedName, *State->Context, Method, *State->Diagnostics))
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
                       HeaderParseContext& Context,
                       std::vector<Diagnostic>& Diagnostics)
{
    TypeSpec Spec{};
    Spec.Header = Header;
    Spec.IsEnum = true;
    Spec.QualifiedName = QualifiedNameForCursor(Cursor);
    Spec.Doc = ExtractParsedComment(Cursor).Doc;

    if (!ValidatePayload(TypePayload, {"display_name", "category"}, {}, Cursor, Diagnostics))
    {
        return Spec;
    }
    Spec.DisplayName = TypePayload.Values.contains("display_name") ? TypePayload.Values.at("display_name") : "";
    Spec.Category = TypePayload.Values.contains("category") ? TypePayload.Values.at("category") : "";

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
                if (!ValidatePayload(Marker->Payload, {"display_name"}, {}, Child, *DiagnosticsPtr))
                {
                    return CXChildVisit_Continue;
                }
                (void)MatchSourceMarker(
                    *State->Context->Markers, *State->Context->MarkerCursors, Child, MarkerKind::EnumValue, true);
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

        TypeSpec Spec = BuildRecordSpec(CursorFilePath(Cursor), Cursor, Marker->Payload, *Context, *Context->Diagnostics);
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

        TypeSpec Spec = BuildEnumSpec(CursorFilePath(Cursor), Cursor, Marker->Payload, *Context, *Context->Diagnostics);
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

    return Result;
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

std::unordered_set<std::string> ResolveRegisteredTypeKeys(const std::vector<ScopedTypeExpression>& Expressions,
                                                          const std::vector<std::string>& CompileArgs,
                                                          const fs::path& BuildDir)
{
    std::unordered_set<std::string> Result{};
    if (Expressions.empty())
    {
        return Result;
    }

    std::unordered_map<std::string, std::vector<std::pair<std::size_t, ScopedTypeExpression>>> Grouped{};
    for (std::size_t Index = 0; Index < Expressions.size(); ++Index)
    {
        const ScopedTypeExpression& Expression = Expressions[Index];
        Grouped[NormalizePathKey(Expression.File)].push_back({Index, Expression});
    }

    std::vector<const char*> ArgPointers{};
    ArgPointers.reserve(CompileArgs.size());
    for (const std::string& Arg : CompileArgs)
    {
        ArgPointers.push_back(Arg.c_str());
    }

    for (const auto& [FileKey, Group] : Grouped)
    {
        const fs::path IncludedFile = Group.front().second.File;
        const std::string ProbeSource = BuildRegistrationProbeSource(IncludedFile, Group);
        const fs::path ProbePath = BuildDir / ("SnAPI.ReflectionGen.registration_probe." +
                                              std::to_string(std::hash<std::string>{}(FileKey)) + ".cpp");
        const std::string ProbePathString = ProbePath.string();

        CXUnsavedFile UnsavedFile{};
        UnsavedFile.Filename = ProbePathString.c_str();
        UnsavedFile.Contents = ProbeSource.c_str();
        UnsavedFile.Length = ProbeSource.size();

        CXIndex IndexHandle = clang_createIndex(0, 0);
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
            clang_disposeIndex(IndexHandle);
            continue;
        }

        struct VisitorState
        {
            std::unordered_set<std::string>* Out = nullptr;
        } State{&Result};

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
                        const std::string Key = NormalizeTypeExpressionString(PrettyPrintedTypeForCode(Underlying, Cursor));
                        if (!Key.empty())
                        {
                            StatePtr->Out->insert(Key);
                        }
                        return CXChildVisit_Continue;
                    }
                }
                return CXChildVisit_Recurse;
            },
            &State);

        clang_disposeTranslationUnit(Unit);
        clang_disposeIndex(IndexHandle);
    }

    Result.insert(NormalizeTypeExpressionString("void"));
    return Result;
}

RegistrationKnowledge BuildRegistrationKnowledge(const fs::path& ProjectRoot,
                                                const fs::path& SeedSource,
                                                const std::vector<std::string>& CompileArgs,
                                                const fs::path& BuildDir)
{
    RegistrationKnowledge Knowledge{};
    const std::vector<fs::path> Files = CollectRegistrationScanFiles(ProjectRoot, SeedSource);

    std::vector<ScopedTypeExpression> Expressions{};
    for (const fs::path& File : Files)
    {
        std::vector<ScopedTypeExpression> FileExpressions = ScanTypeExpressionsInFile(File);
        Expressions.insert(Expressions.end(),
                           std::make_move_iterator(FileExpressions.begin()),
                           std::make_move_iterator(FileExpressions.end()));
    }

    Knowledge.RegisteredTypeKeys = ResolveRegisteredTypeKeys(Expressions, CompileArgs, BuildDir);
    return Knowledge;
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

std::vector<std::string> LoadCompileArguments(const fs::path& BuildDir, const fs::path& SeedSource)
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

        if (Arg.starts_with("-o") || Arg.starts_with("-MF") || Arg.starts_with("-MT") ||
            Arg.starts_with("-MQ") || Arg.starts_with("-MJ"))
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

std::vector<TypeSpec> ParseAnnotatedHeaders(const std::vector<fs::path>& Headers,
                                           const std::vector<std::string>& CompileArgs,
                                           const fs::path& BuildDir,
                                           const RegistrationKnowledge& BaseKnowledge,
                                           std::vector<Diagnostic>& Diagnostics)
{
    std::vector<TypeSpec> Result{};
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
        CollectAnnotatedTypeKeys(Unit, HeaderKeys, Markers, Knowledge.RegisteredTypeKeys);

        std::unordered_map<std::string, HeaderMarkerCursor> MarkerCursors{};
        for (const auto& [FileKey, FileMarkers] : Markers)
        {
            (void)FileMarkers;
            MarkerCursors.emplace(FileKey, HeaderMarkerCursor{});
        }

        HeaderParseContext Context{std::move(HeaderKeys), {}, &Diagnostics, &Knowledge, &Markers, &MarkerCursors};
        clang_visitChildren(clang_getTranslationUnitCursor(Unit), &TranslationUnitVisitor, &Context);
        Result.insert(Result.end(),
                      std::make_move_iterator(Context.Types.begin()),
                      std::make_move_iterator(Context.Types.end()));

        auto ReportUnmatched = [&](const std::vector<ReflectionMarker>& Entries,
                                   const std::size_t NextIndex,
                                   const std::string_view MarkerLabel) {
            for (std::size_t Index = NextIndex; Index < Entries.size(); ++Index)
            {
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

            ReportUnmatched(FileMarkers.Type, CursorIt->second.NextType, "SnType");
            ReportUnmatched(FileMarkers.Field, CursorIt->second.NextField, "SnField");
            ReportUnmatched(FileMarkers.Function, CursorIt->second.NextFunction, "SnFunction");
            ReportUnmatched(FileMarkers.EnumValue, CursorIt->second.NextEnumValue, "SnEnumValue");
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

void EmitRecordRegistration(std::ostream& Stream, const TypeSpec& Spec)
{
    Stream << "SNAPI_REFLECT_METADATA(" << Spec.QualifiedName
           << ", ([]() -> ::SnAPI::GameFramework::TExpected<::SnAPI::GameFramework::TypeInfo*> {\n";
    Stream << "    using T = " << Spec.QualifiedName << ";\n";
    Stream << "    auto Builder = ::SnAPI::GameFramework::TTypeBuilder<T>(::SnAPI::GameFramework::TTypeNameV<T>);\n";
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
        Stream << "        Builder.Field(" << CppStringLiteral(Field.Name) << ", &T::" << Field.Name << ", " << Field.FlagsExpr << ");\n";
        Stream << "    }\n";
    }
    for (const MethodSpec& Method : Spec.Methods)
    {
        Stream << "    if constexpr (" << Method.ConditionExpr << ")\n";
        Stream << "    {\n";
        Stream << "        Builder.Method(" << CppStringLiteral(Method.Name) << ", " << Method.PointerExpr << ", " << Method.FlagsExpr << ");\n";
        Stream << "    }\n";
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

void EmitEnumRegistration(std::ostream& Stream, const TypeSpec& Spec)
{
    Stream << "SNAPI_REFLECT_METADATA(" << Spec.QualifiedName
           << ", ([]() -> ::SnAPI::GameFramework::TExpected<::SnAPI::GameFramework::TypeInfo*> {\n";
    Stream << "    using T = " << Spec.QualifiedName << ";\n";
    Stream << "    ::SnAPI::GameFramework::TypeInfo Info{};\n";
    Stream << "    Info.Id = ::SnAPI::GameFramework::TypeIdFromName(::SnAPI::GameFramework::TTypeNameV<T>);\n";
    Stream << "    Info.Name = ::SnAPI::GameFramework::TTypeNameV<T>;\n";
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

bool WriteGeneratedFile(const fs::path& OutputPath, const std::vector<TypeSpec>& Types)
{
    std::ofstream Output(OutputPath);
    if (!Output.is_open())
    {
        return false;
    }

    Output << "// Generated by SnAPI.GameFramework ReflectionGen. Do not edit.\n";
    Output << "#include <cstddef>\n";
    Output << "#include <type_traits>\n";
    Output << "#include \"TypeAutoRegistration.h\"\n";
    Output << "#include \"TypeName.h\"\n";
    Output << "#include \"TypeRegistry.h\"\n";
    Output << "\n";

    std::unordered_set<std::string> Includes{};
    for (const TypeSpec& Type : Types)
    {
        Includes.insert(Type.Header.generic_string());
    }
    std::vector<std::string> SortedIncludes(Includes.begin(), Includes.end());
    std::sort(SortedIncludes.begin(), SortedIncludes.end());
    for (const std::string& Include : SortedIncludes)
    {
        Output << "#include " << CppStringLiteral(Include) << "\n";
    }
    Output << "\n";

    for (const TypeSpec& Type : Types)
    {
        if (Type.IsEnum)
        {
            EmitEnumRegistration(Output, Type);
        }
        else
        {
            EmitRecordRegistration(Output, Type);
        }
    }

    return true;
}

struct Options
{
    fs::path Output{};
    fs::path BuildDir{};
    fs::path SeedSource{};
    fs::path ProjectRoot{};
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
        if (Arg == "--extra-arg" && Index + 1 < Argc)
        {
            Result.ExtraArgs.emplace_back(Argv[++Index]);
            continue;
        }

        Result.Headers.push_back(fs::path(Arg));
    }

    if (Result.Output.empty() || Result.BuildDir.empty() || Result.SeedSource.empty())
    {
        throw std::runtime_error("Usage: ReflectionGen --output <file> --build-dir <dir> --seed-source <source> [--project-root <dir>] <headers...>");
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

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options Options = ParseArguments(argc, argv);
        std::vector<Diagnostic> Diagnostics{};
        std::vector<std::string> CompileArgs =
            LoadCompileArguments(Options.BuildDir, Options.SeedSource.lexically_normal());
        CompileArgs.insert(CompileArgs.end(), Options.ExtraArgs.begin(), Options.ExtraArgs.end());
        const RegistrationKnowledge Knowledge =
            BuildRegistrationKnowledge(Options.ProjectRoot, Options.SeedSource, CompileArgs, Options.BuildDir);
        const std::vector<TypeSpec> Types =
            ParseAnnotatedHeaders(Options.Headers, CompileArgs, Options.BuildDir, Knowledge, Diagnostics);

        if (!Diagnostics.empty())
        {
            PrintDiagnostics(Diagnostics);
            if (HasErrors(Diagnostics))
            {
                return 1;
            }
        }

        if (!WriteGeneratedFile(Options.Output, Types))
        {
            std::cerr << "Failed to write generated output: " << Options.Output << '\n';
            return 1;
        }

        return 0;
    }
    catch (const std::exception& Ex)
    {
        std::cerr << Ex.what() << '\n';
        return 1;
    }
}
