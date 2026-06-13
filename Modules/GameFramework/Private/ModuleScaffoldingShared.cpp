#include "ModuleScaffoldingShared.h"

#include "ProjectBuildGenerationShared.h"

namespace SnAPI::GameFramework::Detail
{

bool UsesEditorModuleTemplate(const EProjectModuleType ModuleType)
{
    return ModuleType == EProjectModuleType::Editor;
}

bool IsSupportedScaffoldModuleType(const EProjectModuleType ModuleType)
{
    return ModuleType != EProjectModuleType::Program;
}

bool DefaultLoadInEditor(const EProjectModuleType ModuleType)
{
    switch (ModuleType)
    {
    case EProjectModuleType::Runtime:
    case EProjectModuleType::Editor:
    case EProjectModuleType::Shared:
    case EProjectModuleType::Developer:
        return true;
    case EProjectModuleType::Test:
    case EProjectModuleType::Program:
        return false;
    }

    return false;
}

bool DefaultLoadInRuntime(const EProjectModuleType ModuleType)
{
    switch (ModuleType)
    {
    case EProjectModuleType::Runtime:
    case EProjectModuleType::Shared:
        return true;
    case EProjectModuleType::Editor:
    case EProjectModuleType::Developer:
    case EProjectModuleType::Test:
    case EProjectModuleType::Program:
        return false;
    }

    return false;
}

TExpected<ModuleLayout> BuildModuleLayout(const ModuleScaffoldOptions& Options)
{
    auto ModuleNameResult = NormalizeIdentifier(Options.ModuleName, "Module name");
    if (!ModuleNameResult)
    {
        return std::unexpected(ModuleNameResult.error());
    }

    const std::string RequestedNamespace = Options.NamespaceRoot.empty() ? *ModuleNameResult : Options.NamespaceRoot;
    auto NamespaceResult = NormalizeIdentifier(RequestedNamespace, "Module namespace");
    if (!NamespaceResult)
    {
        return std::unexpected(NamespaceResult.error());
    }

    std::string ModuleRootField = Options.ModuleRootField.empty()
        ? BuildDefaultModuleRootField(Options.CodeRootField, *ModuleNameResult)
        : Options.ModuleRootField;
    ModuleRootField =
        ProjectDescriptorService::ToProjectRelativePathField(ModuleRootField, Options.WorkspaceRootDirectory);

    ModuleLayout Layout{};
    Layout.ModuleName = std::move(*ModuleNameResult);
    Layout.NamespaceRoot = std::move(*NamespaceResult);
    Layout.ModuleClassName = BuildPascalCaseStem(Layout.ModuleName) + "Module";
    Layout.ModuleType = Options.ModuleType;
    Layout.GenerateGameplayBootstrap =
        Options.GenerateGameplayBootstrap && Options.ModuleType == EProjectModuleType::Runtime;
    Layout.ModuleRootField = std::move(ModuleRootField);

    if (Layout.GenerateGameplayBootstrap)
    {
        const std::string TypeStem = BuildPascalCaseStem(Layout.ModuleName);
        Layout.GameClassName = TypeStem + "Game";
        Layout.GameModeClassName = TypeStem + "GameMode";
    }

    const std::filesystem::path ModuleRootPath(Layout.ModuleRootField);
    Layout.ModuleRootDirectory =
        ModuleRootPath.is_absolute() ? ModuleRootPath : (Options.WorkspaceRootDirectory / ModuleRootPath);
    Layout.IncludeDirectory = Layout.ModuleRootDirectory / "Public";
    Layout.PublicHeaderDirectory = Layout.IncludeDirectory / Layout.ModuleName;
    Layout.SourceDirectory = Layout.ModuleRootDirectory / "Private";
    Layout.ModuleRootCMakePath = Layout.ModuleRootDirectory / "CMakeLists.txt";
    Layout.CMakeFragmentPath = Layout.ModuleRootDirectory / (Layout.ModuleName + ".CMakeLists.txt");
    Layout.ModuleHeaderPath = Layout.PublicHeaderDirectory / (Layout.ModuleName + "Module.h");
    Layout.ModuleSourcePath = Layout.SourceDirectory / (Layout.ModuleName + "Module.cpp");
    if (Layout.GenerateGameplayBootstrap)
    {
        Layout.GameHeaderPath = Layout.PublicHeaderDirectory / (Layout.ModuleName + "Game.h");
        Layout.GameSourcePath = Layout.SourceDirectory / (Layout.ModuleName + "Game.cpp");
        Layout.GameModeHeaderPath = Layout.PublicHeaderDirectory / (Layout.ModuleName + "GameMode.h");
        Layout.GameModeSourcePath = Layout.SourceDirectory / (Layout.ModuleName + "GameMode.cpp");
    }
    return Layout;
}

ProjectModuleDescriptor BuildModuleDescriptor(const ModuleScaffoldOptions& Options, const ModuleLayout& Layout)
{
    ProjectModuleDescriptor Module{};
    Module.Name = Layout.ModuleName;
    Module.Type = Options.ModuleType;
    Module.Root = Layout.ModuleRootField;
    Module.Platforms = Options.Platforms;
    Module.PreprocessorDefinitions = Options.PreprocessorDefinitions;
    Module.UseReflectionGen = Options.UseReflectionGen;
    Module.UseSWIG = Options.UseSWIG;
    Module.LoadInEditor = Options.LoadInEditor.value_or(DefaultLoadInEditor(Options.ModuleType));
    Module.LoadInRuntime = Options.LoadInRuntime.value_or(DefaultLoadInRuntime(Options.ModuleType));
    Module.PublicDependencies = Options.PublicDependencies;
    Module.PrivateDependencies = Options.PrivateDependencies;

    if (Options.ModuleType == EProjectModuleType::Editor)
    {
        EnsureDependency(Module.PrivateDependencies, "SnAPI.GameFramework");
    }
    else
    {
        EnsureDependency(Module.PublicDependencies, "SnAPI.GameFramework");
    }

    return Module;
}

Result EnsureModuleFilesDoNotExist(const ModuleLayout& Layout)
{
    std::vector<std::filesystem::path> Files = {
        Layout.ModuleRootCMakePath,
        Layout.CMakeFragmentPath,
        Layout.ModuleHeaderPath,
        Layout.ModuleSourcePath,
    };
    if (Layout.GenerateGameplayBootstrap)
    {
        Files.push_back(Layout.GameHeaderPath);
        Files.push_back(Layout.GameSourcePath);
        Files.push_back(Layout.GameModeHeaderPath);
        Files.push_back(Layout.GameModeSourcePath);
    }

    for (const std::filesystem::path& File : Files)
    {
        std::error_code Error{};
        if (std::filesystem::exists(File, Error) && !Error)
        {
            return std::unexpected(
                MakeError(EErrorCode::AlreadyExists, "Generated module file already exists: " + File.string()));
        }
    }

    return Ok();
}

namespace
{

/**
 * @brief Build the generated starter game header text for one runtime module.
 * @param Layout Generated runtime-module layout.
 * @return Header text.
 */
[[nodiscard]] std::string BuildStarterGameHeader(const ModuleLayout& Layout)
{
    return std::string("#pragma once\n\n"
                       "#include \"IGame.h\"\n\n"
                       "namespace ") +
        Layout.NamespaceRoot + "\n{\n\n" + "class " + Layout.GameClassName +
        " final : public SnAPI::GameFramework::IGame\n"
        "{\n"
        "public:\n"
        "    [[nodiscard]] std::string_view Name() const override;\n"
        "    SnAPI::GameFramework::Result Initialize(SnAPI::GameFramework::GameplayHost& Host) override;\n"
        "    std::unique_ptr<SnAPI::GameFramework::IGameMode> CreateInitialGameMode(\n"
        "        SnAPI::GameFramework::GameplayHost& Host) override;\n"
        "    void Tick(SnAPI::GameFramework::GameplayHost& Host, float DeltaSeconds) override;\n"
        "    void Shutdown(SnAPI::GameFramework::GameplayHost& Host) override;\n"
        "};\n\n"
        "} // namespace " +
        Layout.NamespaceRoot + "\n";
}

/**
 * @brief Build the generated starter game source text for one runtime module.
 * @param Layout Generated runtime-module layout.
 * @return Source text.
 */
[[nodiscard]] std::string BuildStarterGameSource(const ModuleLayout& Layout)
{
    return std::string("#include \"") + Layout.ModuleName + "/" + Layout.ModuleName + "Game.h\"\n" +
        "#include \"" + Layout.ModuleName + "/" + Layout.ModuleName + "GameMode.h\"\n\n" + "namespace " +
        Layout.NamespaceRoot + "\n{\n\n" + "std::string_view " + Layout.GameClassName +
        "::Name() const\n"
        "{\n"
        "    return \"" +
        Layout.GameClassName +
        "\";\n"
        "}\n\n"
        "SnAPI::GameFramework::Result " +
        Layout.GameClassName +
        "::Initialize(SnAPI::GameFramework::GameplayHost& Host)\n"
        "{\n"
        "    (void)Host;\n"
        "    return SnAPI::GameFramework::Ok();\n"
        "}\n\n"
        "std::unique_ptr<SnAPI::GameFramework::IGameMode> " +
        Layout.GameClassName +
        "::CreateInitialGameMode(SnAPI::GameFramework::GameplayHost& Host)\n"
        "{\n"
        "    (void)Host;\n"
        "    return std::make_unique<" +
        Layout.GameModeClassName +
        ">();\n"
        "}\n\n"
        "void " +
        Layout.GameClassName +
        "::Tick(SnAPI::GameFramework::GameplayHost& Host, const float DeltaSeconds)\n"
        "{\n"
        "    (void)Host;\n"
        "    (void)DeltaSeconds;\n"
        "}\n\n"
        "void " +
        Layout.GameClassName +
        "::Shutdown(SnAPI::GameFramework::GameplayHost& Host)\n"
        "{\n"
        "    (void)Host;\n"
        "}\n\n"
        "} // namespace " +
        Layout.NamespaceRoot + "\n";
}

/**
 * @brief Build the generated starter game-mode header text for one runtime module.
 * @param Layout Generated runtime-module layout.
 * @return Header text.
 */
[[nodiscard]] std::string BuildStarterGameModeHeader(const ModuleLayout& Layout)
{
    return std::string("#pragma once\n\n"
                       "#include \"IGameMode.h\"\n\n"
                       "namespace ") +
        Layout.NamespaceRoot + "\n{\n\n" + "class " + Layout.GameModeClassName +
        " final : public SnAPI::GameFramework::IGameMode\n"
        "{\n"
        "public:\n"
        "    [[nodiscard]] std::string_view Name() const override;\n"
        "    SnAPI::GameFramework::Result Initialize(SnAPI::GameFramework::GameplayHost& Host) override;\n"
        "    void Tick(SnAPI::GameFramework::GameplayHost& Host, float DeltaSeconds) override;\n"
        "    void Shutdown(SnAPI::GameFramework::GameplayHost& Host) override;\n"
        "};\n\n"
        "} // namespace " +
        Layout.NamespaceRoot + "\n";
}

/**
 * @brief Build the generated starter game-mode source text for one runtime module.
 * @param Layout Generated runtime-module layout.
 * @return Source text.
 */
[[nodiscard]] std::string BuildStarterGameModeSource(const ModuleLayout& Layout)
{
    return std::string("#include \"") + Layout.ModuleName + "/" + Layout.ModuleName + "GameMode.h\"\n\n" +
        "namespace " + Layout.NamespaceRoot + "\n{\n\n" + "std::string_view " + Layout.GameModeClassName +
        "::Name() const\n"
        "{\n"
        "    return \"" +
        Layout.GameModeClassName +
        "\";\n"
        "}\n\n"
        "SnAPI::GameFramework::Result " +
        Layout.GameModeClassName +
        "::Initialize(SnAPI::GameFramework::GameplayHost& Host)\n"
        "{\n"
        "    (void)Host;\n"
        "    return SnAPI::GameFramework::Ok();\n"
        "}\n\n"
        "void " +
        Layout.GameModeClassName +
        "::Tick(SnAPI::GameFramework::GameplayHost& Host, const float DeltaSeconds)\n"
        "{\n"
        "    (void)Host;\n"
        "    (void)DeltaSeconds;\n"
        "}\n\n"
        "void " +
        Layout.GameModeClassName +
        "::Shutdown(SnAPI::GameFramework::GameplayHost& Host)\n"
        "{\n"
        "    (void)Host;\n"
        "}\n\n"
        "} // namespace " +
        Layout.NamespaceRoot + "\n";
}

/**
 * @brief Return the type-specific starter hook name for one scaffolded module type.
 * @param ModuleType Authored module role.
 * @return Hook function name, or empty when the module does not emit an extra hook.
 */
[[nodiscard]] std::string_view ModuleHookName(const EProjectModuleType ModuleType)
{
    switch (ModuleType)
    {
    case EProjectModuleType::Runtime:
        return "RegisterRuntimeServices";
    case EProjectModuleType::Shared:
        return "RegisterSharedServices";
    case EProjectModuleType::Developer:
        return "RegisterDeveloperTools";
    case EProjectModuleType::Test:
        return "RegisterTestScaffolding";
    case EProjectModuleType::Editor:
        return "RegisterEditorServices";
    case EProjectModuleType::Program:
        return {};
    }

    return {};
}

/**
 * @brief Return one short type-specific starter-hook description.
 * @param ModuleType Authored module role.
 * @return Human-readable description.
 */
[[nodiscard]] std::string_view ModuleHookDescription(const EProjectModuleType ModuleType)
{
    switch (ModuleType)
    {
    case EProjectModuleType::Runtime:
        return "Hook for runtime-facing services, registries, or gameplay systems.";
    case EProjectModuleType::Shared:
        return "Hook for shared services that are linked into editor and runtime hosts.";
    case EProjectModuleType::Developer:
        return "Hook for developer-only diagnostics, tools, or instrumentation.";
    case EProjectModuleType::Test:
        return "Hook for test registration and fixture bootstrap code.";
    case EProjectModuleType::Editor:
        return "Hook for editor-only services and tooling registration.";
    case EProjectModuleType::Program:
        return {};
    }

    return {};
}

/**
 * @brief Build the generated library-style module header text.
 * @param Layout Generated module layout.
 * @return Header text.
 */
[[nodiscard]] std::string BuildModuleHeader(const ModuleLayout& Layout)
{
    std::string Text = "#pragma once\n\n";
    if (UsesEditorModuleTemplate(Layout.ModuleType))
    {
        Text += "#include \"Editor/IEditorService.h\"\n\n";
    }
    Text += "#include <string_view>\n\n";
    Text += "namespace " + Layout.NamespaceRoot + "\n{\n\n";
    Text += "class " + Layout.ModuleClassName + " final\n{\npublic:\n";
    Text += "    [[nodiscard]] static std::string_view Name();\n";

    const std::string_view HookName = ModuleHookName(Layout.ModuleType);
    const std::string_view HookDescription = ModuleHookDescription(Layout.ModuleType);
    if (!HookName.empty())
    {
        if (UsesEditorModuleTemplate(Layout.ModuleType))
        {
            Text += "    /** @brief ";
            Text += HookDescription;
            Text += " */\n";
            Text += "    static void ";
            Text += HookName;
            Text += "(SnAPI::GameFramework::Editor::EditorServiceContext& Context);\n";
        }
        else
        {
            Text += "    /** @brief ";
            Text += HookDescription;
            Text += " */\n";
            Text += "    static void ";
            Text += HookName;
            Text += "();\n";
        }
    }

    Text += "};\n\n} // namespace " + Layout.NamespaceRoot + "\n";
    return Text;
}

/**
 * @brief Build the generated library-style module source text.
 * @param Layout Generated module layout.
 * @return Source text.
 */
[[nodiscard]] std::string BuildModuleSource(const ModuleLayout& Layout)
{
    std::string Text = std::string("#include \"") + Layout.ModuleName + "/" + Layout.ModuleName +
        "Module.h\"\n\n" + "namespace " + Layout.NamespaceRoot + "\n{\n\n" + "std::string_view " +
        Layout.ModuleClassName +
        "::Name()\n"
        "{\n"
        "    return \"" +
        Layout.ModuleName +
        "\";\n"
        "}\n";

    const std::string_view HookName = ModuleHookName(Layout.ModuleType);
    if (UsesEditorModuleTemplate(Layout.ModuleType))
    {
        Text += "\nvoid " + Layout.ModuleClassName +
            "::" + std::string(HookName) + "(SnAPI::GameFramework::Editor::EditorServiceContext& Context)\n"
            "{\n"
            "    (void)Context;\n"
            "}\n";
    }
    else if (!HookName.empty())
    {
        Text += "\nvoid " + Layout.ModuleClassName +
            "::" + std::string(HookName) + "()\n"
            "{\n"
            "}\n";
    }

    Text += "\n} // namespace " + Layout.NamespaceRoot + "\n";
    return Text;
}

/**
 * @brief Build the generated module CMake fragment.
 * @param Module Descriptor entry that drives dependency/flag emission.
 * @param Layout Generated module layout.
 * @return CMake fragment text.
 */
[[nodiscard]] std::string BuildModuleCMakeFragment(const ProjectModuleDescriptor& Module, const ModuleLayout& Layout)
{
    std::string Text = std::string("# Auto-generated by SnAPI Build System. Safe to regenerate.\n\n") +
        "add_library(" + Layout.ModuleName +
        "\n"
        "    Private/" +
        Layout.ModuleName +
        "Module.cpp\n"
        "";
    if (Layout.GenerateGameplayBootstrap)
    {
        Text += "    Private/" + Layout.ModuleName + "Game.cpp\n"
                "    Private/" + Layout.ModuleName + "GameMode.cpp\n";
    }
    Text += ")\n\n"
            "target_include_directories(" +
        Layout.ModuleName +
        " PUBLIC\n"
        "    ${CMAKE_CURRENT_SOURCE_DIR}/Public\n"
        ")\n";

    if (!Module.PublicDependencies.empty())
    {
        Text += "\n"
                "target_link_libraries(" +
            Layout.ModuleName + " PUBLIC\n";
        for (const std::string& Dependency : Module.PublicDependencies)
        {
            Text += "    " + Dependency + "\n";
        }
        Text += ")\n";
    }

    if (!Module.PrivateDependencies.empty())
    {
        Text += "\n"
                "target_link_libraries(" +
            Layout.ModuleName + " PRIVATE\n";
        for (const std::string& Dependency : Module.PrivateDependencies)
        {
            Text += "    " + Dependency + "\n";
        }
        Text += ")\n";
    }

    if (!Module.PreprocessorDefinitions.empty())
    {
        Text += "\n"
                "target_compile_definitions(" +
            Layout.ModuleName + " PRIVATE\n";
        for (const std::string& Definition : Module.PreprocessorDefinitions)
        {
            Text += "    " + Definition + "\n";
        }
        Text += ")\n";
    }

    return Text;
}

/**
 * @brief Build the module-root `CMakeLists.txt` wrapper.
 * @param Layout Generated module layout.
 * @return CMake wrapper text.
 */
[[nodiscard]] std::string BuildModuleRootCMake(const ModuleLayout& Layout)
{
    return std::string("# Minimal wrapper for the generated ") + Layout.ModuleName +
        " module fragment.\n"
        "include(\"${CMAKE_CURRENT_LIST_DIR}/" +
        Layout.ModuleName + ".CMakeLists.txt\")\n";
}

} // namespace

Result WriteModuleFiles(const ProjectModuleDescriptor& Module, const ModuleLayout& Layout,
                        std::vector<std::filesystem::path>* OutGeneratedFiles)
{
    if (Result DirectoryResult = EnsureDirectory(Layout.PublicHeaderDirectory); !DirectoryResult)
    {
        return DirectoryResult;
    }
    if (Result DirectoryResult = EnsureDirectory(Layout.SourceDirectory); !DirectoryResult)
    {
        return DirectoryResult;
    }

    struct GeneratedTextFile
    {
        std::filesystem::path Path{};
        std::string Text{};
    };

    const std::vector<GeneratedTextFile> FilesToWrite = {
        {Layout.ModuleHeaderPath, BuildModuleHeader(Layout)},
        {Layout.ModuleSourcePath, BuildModuleSource(Layout)},
        {Layout.ModuleRootCMakePath, BuildModuleRootCMake(Layout)},
        {Layout.CMakeFragmentPath, BuildModuleCMakeFragment(Module, Layout)},
    };

    std::vector<GeneratedTextFile> EffectiveFilesToWrite = FilesToWrite;
    if (Layout.GenerateGameplayBootstrap)
    {
        EffectiveFilesToWrite.push_back({Layout.GameHeaderPath, BuildStarterGameHeader(Layout)});
        EffectiveFilesToWrite.push_back({Layout.GameSourcePath, BuildStarterGameSource(Layout)});
        EffectiveFilesToWrite.push_back({Layout.GameModeHeaderPath, BuildStarterGameModeHeader(Layout)});
        EffectiveFilesToWrite.push_back({Layout.GameModeSourcePath, BuildStarterGameModeSource(Layout)});
    }

    for (const GeneratedTextFile& File : EffectiveFilesToWrite)
    {
        if (Result WriteResult = WriteTextFile(File.Path, File.Text); !WriteResult)
        {
            return WriteResult;
        }

        if (OutGeneratedFiles != nullptr)
        {
            OutGeneratedFiles->push_back(File.Path);
        }
    }

    return Ok();
}

} // namespace SnAPI::GameFramework::Detail
