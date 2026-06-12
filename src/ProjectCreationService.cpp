#include "ProjectCreationService.h"

#include "AuthoredAssetJson.h"
#include "NodeAsset.h"
#include "ProjectBuildGenerationShared.h"

#include <algorithm>
#include <fstream>
#include <string_view>

namespace SnAPI::GameFramework
{
    namespace
    {

        /**
         * @brief Track the filesystem layout for one generated starter runtime module.
         */
        struct StarterRuntimeModuleLayout
        {
            std::string ModuleName{};
            std::string NamespaceRoot{};
            std::string GameClassName{};
            std::string GameModeClassName{};
            std::string ModuleClassName{};
            std::string ModuleRootField{};
            std::filesystem::path ModuleRootDirectory{};
            std::filesystem::path IncludeDirectory{};
            std::filesystem::path PublicHeaderDirectory{};
            std::filesystem::path SourceDirectory{};
            std::filesystem::path ModuleRootCMakePath{};
            std::filesystem::path CMakeFragmentPath{};
            std::filesystem::path GameHeaderPath{};
            std::filesystem::path GameSourcePath{};
            std::filesystem::path GameModeHeaderPath{};
            std::filesystem::path GameModeSourcePath{};
            std::filesystem::path ModuleHeaderPath{};
            std::filesystem::path ModuleSourcePath{};
            std::filesystem::path DefaultGameConfigPath{};
        };

        /**
         * @brief Track the filesystem layout for one generated starter editor module.
         */
        struct StarterEditorModuleLayout
        {
            std::string ModuleName{};
            std::string NamespaceRoot{};
            std::string ModuleClassName{};
            std::string ModuleRootField{};
            std::string RuntimeDependencyModuleName{};
            std::filesystem::path ModuleRootDirectory{};
            std::filesystem::path IncludeDirectory{};
            std::filesystem::path PublicHeaderDirectory{};
            std::filesystem::path SourceDirectory{};
            std::filesystem::path ModuleRootCMakePath{};
            std::filesystem::path CMakeFragmentPath{};
            std::filesystem::path ModuleHeaderPath{};
            std::filesystem::path ModuleSourcePath{};
        };

        /**
         * @brief Build the resolved starter runtime-module layout for one request.
         * @param Request Original project-creation request.
         * @param Descriptor Normalized descriptor seed.
         * @param ProjectRoot Resolved project root directory.
         * @return Layout or an identifier/path error.
         */
        [[nodiscard]] TExpected<StarterRuntimeModuleLayout>
        BuildStarterRuntimeModuleLayout(const ProjectCreationRequest& Request, const ProjectDescriptor& Descriptor,
                                        const std::filesystem::path& ProjectRoot)
        {
            const std::string RequestedModuleName =
                Request.Code.RuntimeModuleName.empty() ? Request.ProjectName : Request.Code.RuntimeModuleName;
            auto ModuleNameResult = Detail::NormalizeIdentifier(RequestedModuleName, "Starter runtime module name");
            if (!ModuleNameResult)
            {
                return std::unexpected(ModuleNameResult.error());
            }

            const std::string RequestedNamespace =
                Request.Code.NamespaceRoot.empty() ? *ModuleNameResult : Request.Code.NamespaceRoot;
            auto NamespaceResult = Detail::NormalizeIdentifier(RequestedNamespace, "Starter runtime namespace");
            if (!NamespaceResult)
            {
                return std::unexpected(NamespaceResult.error());
            }

            StarterRuntimeModuleLayout Layout{};
            Layout.ModuleName = std::move(*ModuleNameResult);
            Layout.NamespaceRoot = std::move(*NamespaceResult);
            const std::string TypeStem = Detail::BuildPascalCaseStem(Layout.ModuleName);
            Layout.GameClassName = TypeStem;
            Layout.GameModeClassName = TypeStem + "Mode";
            Layout.ModuleClassName = TypeStem + "Module";

            std::string ModuleRootField =
                Detail::BuildDefaultModuleRootField(Descriptor.Paths.CodeRoot, Layout.ModuleName);
            for (const ProjectModuleDescriptor& Module : Descriptor.Modules)
            {
                if (Module.Name == Layout.ModuleName && !Module.Root.empty())
                {
                    ModuleRootField = Module.Root;
                    break;
                }
            }

            Layout.ModuleRootField = ProjectDescriptorService::ToProjectRelativePathField(ModuleRootField, ProjectRoot);

            const std::filesystem::path ModuleRootPath(Layout.ModuleRootField);
            Layout.ModuleRootDirectory = ModuleRootPath.is_absolute() ? ModuleRootPath : (ProjectRoot / ModuleRootPath);
            Layout.IncludeDirectory = Layout.ModuleRootDirectory / "include";
            Layout.PublicHeaderDirectory = Layout.IncludeDirectory / Layout.ModuleName;
            Layout.SourceDirectory = Layout.ModuleRootDirectory / "src";
            Layout.ModuleRootCMakePath = Layout.ModuleRootDirectory / "CMakeLists.txt";
            Layout.CMakeFragmentPath = Layout.ModuleRootDirectory / (Layout.ModuleName + ".CMakeLists.txt");
            Layout.GameHeaderPath = Layout.PublicHeaderDirectory / (Layout.ModuleName + "Game.h");
            Layout.GameSourcePath = Layout.SourceDirectory / (Layout.ModuleName + "Game.cpp");
            Layout.GameModeHeaderPath = Layout.PublicHeaderDirectory / (Layout.ModuleName + "GameMode.h");
            Layout.GameModeSourcePath = Layout.SourceDirectory / (Layout.ModuleName + "GameMode.cpp");
            Layout.ModuleHeaderPath = Layout.PublicHeaderDirectory / (Layout.ModuleName + "Module.h");
            Layout.ModuleSourcePath = Layout.SourceDirectory / (Layout.ModuleName + "Module.cpp");
            Layout.DefaultGameConfigPath =
                ProjectRoot / std::filesystem::path(Descriptor.Paths.ConfigRoot) / "DefaultGame.json";
            return Layout;
        }

        /**
         * @brief Build the resolved starter editor-module layout for one request.
         * @param Request Original project-creation request.
         * @param Descriptor Normalized descriptor seed.
         * @param ProjectRoot Resolved project root directory.
         * @return Layout or an identifier/path error.
         */
        [[nodiscard]] TExpected<StarterEditorModuleLayout>
        BuildStarterEditorModuleLayout(const ProjectCreationRequest& Request, const ProjectDescriptor& Descriptor,
                                       const std::filesystem::path& ProjectRoot)
        {
            const std::string RuntimeModuleName =
                Request.Code.RuntimeModuleName.empty() ? Request.ProjectName : Request.Code.RuntimeModuleName;
            const std::string RequestedEditorModuleName =
                Request.Code.EditorModuleName.empty() ? (RuntimeModuleName + "Editor") : Request.Code.EditorModuleName;
            auto ModuleNameResult =
                Detail::NormalizeIdentifier(RequestedEditorModuleName, "Starter editor module name");
            if (!ModuleNameResult)
            {
                return std::unexpected(ModuleNameResult.error());
            }

            const std::string RequestedNamespace =
                Request.Code.NamespaceRoot.empty() ? RuntimeModuleName : Request.Code.NamespaceRoot;
            auto NamespaceResult = Detail::NormalizeIdentifier(RequestedNamespace, "Starter editor namespace");
            if (!NamespaceResult)
            {
                return std::unexpected(NamespaceResult.error());
            }

            StarterEditorModuleLayout Layout{};
            Layout.ModuleName = std::move(*ModuleNameResult);
            Layout.NamespaceRoot = std::move(*NamespaceResult);
            Layout.ModuleClassName = Detail::BuildPascalCaseStem(Layout.ModuleName) + "Module";

            auto RuntimeDependencyResult =
                Detail::NormalizeIdentifier(RuntimeModuleName, "Starter runtime module name");
            if (RuntimeDependencyResult)
            {
                Layout.RuntimeDependencyModuleName = std::move(*RuntimeDependencyResult);
            }

            std::string ModuleRootField =
                Detail::BuildDefaultModuleRootField(Descriptor.Paths.CodeRoot, Layout.ModuleName);
            for (const ProjectModuleDescriptor& Module : Descriptor.Modules)
            {
                if (Module.Name == Layout.ModuleName && !Module.Root.empty())
                {
                    ModuleRootField = Module.Root;
                    break;
                }
            }

            Layout.ModuleRootField = ProjectDescriptorService::ToProjectRelativePathField(ModuleRootField, ProjectRoot);

            const std::filesystem::path ModuleRootPath(Layout.ModuleRootField);
            Layout.ModuleRootDirectory = ModuleRootPath.is_absolute() ? ModuleRootPath : (ProjectRoot / ModuleRootPath);
            Layout.IncludeDirectory = Layout.ModuleRootDirectory / "include";
            Layout.PublicHeaderDirectory = Layout.IncludeDirectory / Layout.ModuleName;
            Layout.SourceDirectory = Layout.ModuleRootDirectory / "src";
            Layout.ModuleRootCMakePath = Layout.ModuleRootDirectory / "CMakeLists.txt";
            Layout.CMakeFragmentPath = Layout.ModuleRootDirectory / (Layout.ModuleName + ".CMakeLists.txt");
            Layout.ModuleHeaderPath = Layout.PublicHeaderDirectory / (Layout.ModuleName + "Module.h");
            Layout.ModuleSourcePath = Layout.SourceDirectory / (Layout.ModuleName + "Module.cpp");
            return Layout;
        }

        /**
         * @brief Return one fully qualified generated type name.
         * @param NamespaceRoot Generated namespace root.
         * @param TypeName Generated type name.
         * @return `<NamespaceRoot>::<TypeName>`.
         */
        [[nodiscard]] std::string MakeQualifiedTypeName(const std::string_view NamespaceRoot,
                                                        const std::string_view TypeName)
        {
            return std::string(NamespaceRoot) + "::" + std::string(TypeName);
        }

        /**
         * @brief Ensure one descriptor includes the starter runtime-module declaration and startup classes.
         * @param Descriptor Descriptor to update.
         * @param Layout Generated starter module layout.
         */
        void ApplyStarterRuntimeModuleDefaults(ProjectDescriptor& Descriptor, const StarterRuntimeModuleLayout& Layout)
        {
            if (Descriptor.Startup.DefaultGameClass.empty())
            {
                Descriptor.Startup.DefaultGameClass = MakeQualifiedTypeName(Layout.NamespaceRoot, Layout.GameClassName);
            }
            if (Descriptor.Startup.DefaultGameModeClass.empty())
            {
                Descriptor.Startup.DefaultGameModeClass =
                    MakeQualifiedTypeName(Layout.NamespaceRoot, Layout.GameModeClassName);
            }

            ProjectModuleDescriptor* Module = Detail::FindModuleDescriptor(Descriptor, Layout.ModuleName);
            if (Module == nullptr)
            {
                Descriptor.Modules.push_back(ProjectModuleDescriptor{
                    .Name = Layout.ModuleName,
                    .Type = EProjectModuleType::Runtime,
                    .Root = Layout.ModuleRootField,
                    .PublicDependencies = {"SnAPI.GameFramework"},
                    .LoadInEditor = true,
                    .LoadInRuntime = true,
                });
                return;
            }

            if (Module->Root.empty())
            {
                Module->Root = Layout.ModuleRootField;
            }
            Detail::EnsureDependency(Module->PublicDependencies, "SnAPI.GameFramework");
        }

        /**
         * @brief Ensure one descriptor includes the starter editor-module declaration.
         * @param Descriptor Descriptor to update.
         * @param Layout Generated starter editor-module layout.
         */
        void ApplyStarterEditorModuleDefaults(ProjectDescriptor& Descriptor, const StarterEditorModuleLayout& Layout)
        {
            ProjectModuleDescriptor* Module = Detail::FindModuleDescriptor(Descriptor, Layout.ModuleName);
            if (Module == nullptr)
            {
                ProjectModuleDescriptor NewModule{
                    .Name = Layout.ModuleName,
                    .Type = EProjectModuleType::Editor,
                    .Root = Layout.ModuleRootField,
                    .LoadInEditor = true,
                    .LoadInRuntime = false,
                };
                Detail::EnsureDependency(NewModule.PrivateDependencies, Layout.RuntimeDependencyModuleName);
                Detail::EnsureDependency(NewModule.PrivateDependencies, "SnAPI.GameFramework");
                Descriptor.Modules.push_back(std::move(NewModule));
                return;
            }

            Module->Type = EProjectModuleType::Editor;
            Module->LoadInEditor = true;
            Module->LoadInRuntime = false;
            if (Module->Root.empty())
            {
                Module->Root = Layout.ModuleRootField;
            }
            Detail::EnsureDependency(Module->PrivateDependencies, Layout.RuntimeDependencyModuleName);
            Detail::EnsureDependency(Module->PrivateDependencies, "SnAPI.GameFramework");
        }

        /**
         * @brief Build the generated starter game header text.
         * @param Layout Generated starter runtime-module layout.
         * @return Header text.
         */
        [[nodiscard]] std::string BuildStarterGameHeader(const StarterRuntimeModuleLayout& Layout)
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
         * @brief Build the generated starter game source text.
         * @param Layout Generated starter runtime-module layout.
         * @return Source text.
         */
        [[nodiscard]] std::string BuildStarterGameSource(const StarterRuntimeModuleLayout& Layout)
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
         * @brief Build the generated starter game-mode header text.
         * @param Layout Generated starter runtime-module layout.
         * @return Header text.
         */
        [[nodiscard]] std::string BuildStarterGameModeHeader(const StarterRuntimeModuleLayout& Layout)
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
         * @brief Build the generated starter game-mode source text.
         * @param Layout Generated starter runtime-module layout.
         * @return Source text.
         */
        [[nodiscard]] std::string BuildStarterGameModeSource(const StarterRuntimeModuleLayout& Layout)
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
         * @brief Build the generated starter module header text.
         * @param Layout Generated starter runtime-module layout.
         * @return Header text.
         */
        [[nodiscard]] std::string BuildStarterModuleHeader(const StarterRuntimeModuleLayout& Layout)
        {
            return std::string("#pragma once\n\n"
                               "#include <string_view>\n\n"
                               "namespace ") +
                Layout.NamespaceRoot + "\n{\n\n" +
                "/**\n"
                " * @brief Lightweight starter module identity type for the generated project runtime module.\n"
                " *\n"
                " * The current build-system milestones link project modules directly into the editor and runtime "
                "hosts,\n"
                " * so the starter module type acts as a stable home for module-local identity and future bootstrap "
                "hooks.\n"
                " */\n"
                "class " +
                Layout.ModuleClassName +
                " final\n"
                "{\n"
                "public:\n"
                "    [[nodiscard]] static std::string_view Name();\n"
                "};\n\n"
                "} // namespace " +
                Layout.NamespaceRoot + "\n";
        }

        /**
         * @brief Build the generated starter module source text.
         * @param Layout Generated starter runtime-module layout.
         * @return Source text.
         */
        [[nodiscard]] std::string BuildStarterModuleSource(const StarterRuntimeModuleLayout& Layout)
        {
            return std::string("#include \"") + Layout.ModuleName + "/" + Layout.ModuleName + "Module.h\"\n\n" +
                "namespace " + Layout.NamespaceRoot + "\n{\n\n" + "std::string_view " + Layout.ModuleClassName +
                "::Name()\n"
                "{\n"
                "    return \"" +
                Layout.ModuleName +
                "\";\n"
                "}\n\n"
                "} // namespace " +
                Layout.NamespaceRoot + "\n";
        }

        /**
         * @brief Build the generated starter module CMake fragment.
         * @param Layout Generated starter runtime-module layout.
         * @return CMake fragment text.
         */
        [[nodiscard]] std::string BuildStarterModuleCMakeFragment(const StarterRuntimeModuleLayout& Layout)
        {
            return std::string("# Auto-generated by SnAPI Build System. Safe to regenerate.\n\n") + "add_library(" +
                Layout.ModuleName +
                "\n"
                "    src/" +
                Layout.ModuleName +
                "Game.cpp\n"
                "    src/" +
                Layout.ModuleName +
                "GameMode.cpp\n"
                "    src/" +
                Layout.ModuleName +
                "Module.cpp\n"
                ")\n\n"
                "target_include_directories(" +
                Layout.ModuleName +
                " PUBLIC\n"
                "    ${CMAKE_CURRENT_SOURCE_DIR}/include\n"
                ")\n\n"
                "target_link_libraries(" +
                Layout.ModuleName +
                " PUBLIC\n"
                "    SnAPI.GameFramework\n"
                ")\n";
        }

        /**
         * @brief Build the starter module-root `CMakeLists.txt` wrapper.
         * @param Layout Generated starter runtime-module layout.
         * @return CMake wrapper text.
         */
        [[nodiscard]] std::string BuildStarterModuleRootCMake(const StarterRuntimeModuleLayout& Layout)
        {
            return std::string("# Minimal wrapper for the generated ") + Layout.ModuleName +
                " module fragment.\n"
                "include(\"${CMAKE_CURRENT_LIST_DIR}/" +
                Layout.ModuleName + ".CMakeLists.txt\")\n";
        }

        /**
         * @brief Build the starter editor module header text.
         * @param Layout Generated starter editor-module layout.
         * @return Header text.
         */
        [[nodiscard]] std::string BuildStarterEditorModuleHeader(const StarterEditorModuleLayout& Layout)
        {
            return std::string("#pragma once\n\n"
                               "#include \"Editor/IEditorService.h\"\n\n"
                               "#include <string_view>\n\n"
                               "namespace ") +
                Layout.NamespaceRoot + "\n{\n\n" + "class " + Layout.ModuleClassName +
                " final\n"
                "{\n"
                "public:\n"
                "    [[nodiscard]] static std::string_view Name();\n"
                "    static void RegisterEditorServices(SnAPI::GameFramework::Editor::EditorServiceContext& Context);\n"
                "};\n\n"
                "} // namespace " +
                Layout.NamespaceRoot + "\n";
        }

        /**
         * @brief Build the starter editor module source text.
         * @param Layout Generated starter editor-module layout.
         * @return Source text.
         */
        [[nodiscard]] std::string BuildStarterEditorModuleSource(const StarterEditorModuleLayout& Layout)
        {
            return std::string("#include \"") + Layout.ModuleName + "/" + Layout.ModuleName + "Module.h\"\n\n" +
                "namespace " + Layout.NamespaceRoot + "\n{\n\n" + "std::string_view " + Layout.ModuleClassName +
                "::Name()\n"
                "{\n"
                "    return \"" +
                Layout.ModuleName +
                "\";\n"
                "}\n\n"
                "void " +
                Layout.ModuleClassName +
                "::RegisterEditorServices(SnAPI::GameFramework::Editor::EditorServiceContext& Context)\n"
                "{\n"
                "    (void)Context;\n"
                "}\n\n"
                "} // namespace " +
                Layout.NamespaceRoot + "\n";
        }

        /**
         * @brief Build the starter editor module CMake fragment.
         * @param Layout Generated starter editor-module layout.
         * @return CMake fragment text.
         */
        [[nodiscard]] std::string BuildStarterEditorModuleCMakeFragment(const StarterEditorModuleLayout& Layout)
        {
            std::string Text = std::string("# Auto-generated by SnAPI Build System. Safe to regenerate.\n\n") +
                "add_library(" + Layout.ModuleName +
                "\n"
                "    src/" +
                Layout.ModuleName +
                "Module.cpp\n"
                ")\n\n"
                "target_include_directories(" +
                Layout.ModuleName +
                " PUBLIC\n"
                "    ${CMAKE_CURRENT_SOURCE_DIR}/include\n"
                ")\n\n"
                "target_link_libraries(" +
                Layout.ModuleName + " PRIVATE\n";

            if (!Layout.RuntimeDependencyModuleName.empty() && Layout.RuntimeDependencyModuleName != Layout.ModuleName)
            {
                Text += "    " + Layout.RuntimeDependencyModuleName + "\n";
            }

            Text += "    SnAPI.GameFramework\n"
                    ")\n";
            return Text;
        }

        /**
         * @brief Build the starter editor module-root `CMakeLists.txt` wrapper.
         * @param Layout Generated starter editor-module layout.
         * @return CMake wrapper text.
         */
        [[nodiscard]] std::string BuildStarterEditorModuleRootCMake(const StarterEditorModuleLayout& Layout)
        {
            return std::string("# Minimal wrapper for the generated ") + Layout.ModuleName +
                " module fragment.\n"
                "include(\"${CMAKE_CURRENT_LIST_DIR}/" +
                Layout.ModuleName + ".CMakeLists.txt\")\n";
        }

        /**
         * @brief Build the project `Code/CMakeLists.txt` bridge file.
         * @return CMake wrapper text.
         */
        /**
         * @brief Build the generated starter runtime-config JSON text.
         * @param Descriptor Normalized project descriptor.
         * @param Layout Generated starter runtime-module layout.
         * @return JSON text.
         */
        [[nodiscard]] std::string BuildDefaultGameConfigText(const ProjectDescriptor& Descriptor,
                                                             const StarterRuntimeModuleLayout& Layout)
        {
            nlohmann::ordered_json Root = nlohmann::ordered_json::object();
            Root["ProjectName"] = Descriptor.Project.Name;
            Root["StartupLevelAsset"] = Descriptor.Startup.StartupLevelAsset;
            Root["DefaultGameClass"] = Descriptor.Startup.DefaultGameClass.empty()
                ? MakeQualifiedTypeName(Layout.NamespaceRoot, Layout.GameClassName)
                : Descriptor.Startup.DefaultGameClass;
            Root["DefaultGameModeClass"] = Descriptor.Startup.DefaultGameModeClass.empty()
                ? MakeQualifiedTypeName(Layout.NamespaceRoot, Layout.GameModeClassName)
                : Descriptor.Startup.DefaultGameModeClass;
            return Root.dump(2) + "\n";
        }

        /**
         * @brief Materialize all generated starter runtime-module files.
         * @param Descriptor Normalized descriptor used for config generation.
         * @param Layout Generated starter runtime-module layout.
         * @param OutGeneratedFiles Optional flat list populated with written files.
         * @return Success or the first file-write error.
         */
        [[nodiscard]] Result
        WriteStarterRuntimeModuleFiles(const ProjectDescriptor& Descriptor, const StarterRuntimeModuleLayout& Layout,
                                       std::vector<std::filesystem::path>* OutGeneratedFiles = nullptr)
        {
            if (Result DirectoryResult = Detail::EnsureDirectory(Layout.PublicHeaderDirectory); !DirectoryResult)
            {
                return DirectoryResult;
            }
            if (Result DirectoryResult = Detail::EnsureDirectory(Layout.SourceDirectory); !DirectoryResult)
            {
                return DirectoryResult;
            }

            struct GeneratedTextFile
            {
                std::filesystem::path Path{};
                std::string Text{};
            };

            const std::vector<GeneratedTextFile> FilesToWrite = {
                {Layout.GameHeaderPath, BuildStarterGameHeader(Layout)},
                {Layout.GameSourcePath, BuildStarterGameSource(Layout)},
                {Layout.GameModeHeaderPath, BuildStarterGameModeHeader(Layout)},
                {Layout.GameModeSourcePath, BuildStarterGameModeSource(Layout)},
                {Layout.ModuleHeaderPath, BuildStarterModuleHeader(Layout)},
                {Layout.ModuleSourcePath, BuildStarterModuleSource(Layout)},
                {Layout.ModuleRootCMakePath, BuildStarterModuleRootCMake(Layout)},
                {Layout.CMakeFragmentPath, BuildStarterModuleCMakeFragment(Layout)},
                {Layout.DefaultGameConfigPath, BuildDefaultGameConfigText(Descriptor, Layout)},
            };

            for (const GeneratedTextFile& File : FilesToWrite)
            {
                if (Result WriteResult = Detail::WriteTextFile(File.Path, File.Text); !WriteResult)
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

        /**
         * @brief Materialize all generated starter editor-module files.
         * @param Layout Generated starter editor-module layout.
         * @param OutGeneratedFiles Optional flat list populated with written files.
         * @return Success or the first file-write error.
         */
        [[nodiscard]] Result
        WriteStarterEditorModuleFiles(const StarterEditorModuleLayout& Layout,
                                      std::vector<std::filesystem::path>* OutGeneratedFiles = nullptr)
        {
            if (Result DirectoryResult = Detail::EnsureDirectory(Layout.PublicHeaderDirectory); !DirectoryResult)
            {
                return DirectoryResult;
            }
            if (Result DirectoryResult = Detail::EnsureDirectory(Layout.SourceDirectory); !DirectoryResult)
            {
                return DirectoryResult;
            }

            struct GeneratedTextFile
            {
                std::filesystem::path Path{};
                std::string Text{};
            };

            const std::vector<GeneratedTextFile> FilesToWrite = {
                {Layout.ModuleHeaderPath, BuildStarterEditorModuleHeader(Layout)},
                {Layout.ModuleSourcePath, BuildStarterEditorModuleSource(Layout)},
                {Layout.ModuleRootCMakePath, BuildStarterEditorModuleRootCMake(Layout)},
                {Layout.CMakeFragmentPath, BuildStarterEditorModuleCMakeFragment(Layout)},
            };

            for (const GeneratedTextFile& File : FilesToWrite)
            {
                if (Result WriteResult = Detail::WriteTextFile(File.Path, File.Text); !WriteResult)
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

        /**
         * @brief Normalize one caller-supplied project-file name.
         * @param ProjectFileName Raw request field.
         * @return Relative descriptor file path or an error.
         */
        [[nodiscard]] TExpected<std::filesystem::path>
        NormalizeProjectFileName(const std::filesystem::path& ProjectFileName)
        {
            std::filesystem::path Normalized = ProjectFileName;
            if (Normalized.empty())
            {
                Normalized = std::filesystem::path(ProjectDescriptorService::kDefaultProjectFileName);
            }

            if (Normalized.is_absolute())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument,
                                                 "Project descriptor path must be relative to the project root"));
            }

            return Normalized.lexically_normal();
        }

        /**
         * @brief Resolve one parent directory to an absolute normalized path.
         * @param ParentDirectory Raw request directory.
         * @return Absolute parent directory or an error.
         */
        [[nodiscard]] TExpected<std::filesystem::path>
        ResolveParentDirectory(const std::filesystem::path& ParentDirectory)
        {
            if (ParentDirectory.empty())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument, "Project parent directory cannot be empty"));
            }

            std::error_code Error{};
            std::filesystem::path AbsoluteParent = ParentDirectory;
            if (!AbsoluteParent.is_absolute())
            {
                AbsoluteParent = std::filesystem::absolute(AbsoluteParent, Error);
                if (Error)
                {
                    return std::unexpected(MakeError(EErrorCode::InternalError,
                                                     "Failed to resolve project parent directory: " + Error.message()));
                }
            }

            return AbsoluteParent.lexically_normal();
        }

        /**
         * @brief Copy one directory tree into another directory, preserving relative layout.
         * @param SourceDirectory Existing source directory.
         * @param DestinationDirectory Destination directory root.
         * @return Success or an error string.
         */
        [[nodiscard]] std::expected<void, std::string>
        CopyDirectoryContentsRecursive(const std::filesystem::path& SourceDirectory,
                                       const std::filesystem::path& DestinationDirectory)
        {
            std::error_code Error{};
            if (!std::filesystem::exists(SourceDirectory, Error) || Error)
            {
                return std::unexpected("Source directory does not exist: " + SourceDirectory.string());
            }

            std::filesystem::create_directories(DestinationDirectory, Error);
            if (Error)
            {
                return std::unexpected("Failed to create directory '" + DestinationDirectory.string() +
                                       "': " + Error.message());
            }

            for (std::filesystem::recursive_directory_iterator It(SourceDirectory, Error), End; !Error && It != End;
                 ++It)
            {
                const std::filesystem::path RelativePath =
                    std::filesystem::relative(It->path(), SourceDirectory, Error);
                if (Error)
                {
                    return std::unexpected("Failed to compute relative path while copying '" + It->path().string() +
                                           "'");
                }

                const std::filesystem::path DestinationPath = DestinationDirectory / RelativePath;
                if (It->is_directory())
                {
                    std::filesystem::create_directories(DestinationPath, Error);
                    if (Error)
                    {
                        return std::unexpected("Failed to create directory '" + DestinationPath.string() +
                                               "': " + Error.message());
                    }
                    continue;
                }

                std::filesystem::create_directories(DestinationPath.parent_path(), Error);
                if (Error)
                {
                    return std::unexpected("Failed to create parent directory '" +
                                           DestinationPath.parent_path().string() + "': " + Error.message());
                }

                Error.clear();
                std::filesystem::copy_file(It->path(), DestinationPath,
                                           std::filesystem::copy_options::overwrite_existing, Error);
                if (Error)
                {
                    return std::unexpected("Failed to copy file '" + It->path().string() + "' to '" +
                                           DestinationPath.string() + "': " + Error.message());
                }
            }

            if (Error)
            {
                return std::unexpected("Failed to enumerate directory '" + SourceDirectory.string() +
                                       "': " + Error.message());
            }

            return {};
        }

        /**
         * @brief Copy one optional template file into the project workspace.
         * @param SourcePath Template source path. Empty disables the copy.
         * @param DestinationPath Destination file path.
         * @return Success or an error.
         */
        [[nodiscard]] Result CopyOptionalTemplateFile(const std::filesystem::path& SourcePath,
                                                      const std::filesystem::path& DestinationPath)
        {
            if (SourcePath.empty())
            {
                return Ok();
            }

            std::error_code Error{};
            if (!std::filesystem::exists(SourcePath, Error) || Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::NotFound, "Template file does not exist: " + SourcePath.string()));
            }

            if (Result DirectoryResult = Detail::EnsureDirectory(DestinationPath.parent_path()); !DirectoryResult)
            {
                return DirectoryResult;
            }

            Error.clear();
            std::filesystem::copy_file(SourcePath, DestinationPath, std::filesystem::copy_options::overwrite_existing,
                                       Error);
            if (Error)
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError,
                              "Failed to copy template file '" + SourcePath.string() + "': " + Error.message()));
            }

            return Ok();
        }

        /**
         * @brief Write one generated default starter-level source asset.
         * @param DestinationPath Startup-level file path to write.
         * @return Success or an error.
         */
        [[nodiscard]] Result WriteDefaultStarterLevel(const std::filesystem::path& DestinationPath)
        {
            LevelAsset StarterLevel{};
            auto JsonResult = SerializeAuthoredAssetToJson(StarterLevel);
            if (!JsonResult)
            {
                return std::unexpected(JsonResult.error());
            }

            if (Result DirectoryResult = Detail::EnsureDirectory(DestinationPath.parent_path()); !DirectoryResult)
            {
                return DirectoryResult;
            }

            std::ofstream Output(DestinationPath, std::ios::binary | std::ios::trunc);
            if (!Output.is_open())
            {
                return std::unexpected(
                    MakeError(EErrorCode::InternalError, "Failed to open starter level asset for write"));
            }

            Output.write(JsonResult->data(), static_cast<std::streamsize>(JsonResult->size()));
            if (!Output.good())
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to write starter level asset"));
            }

            return Ok();
        }

        /**
         * @brief Normalize one descriptor seed for workspace creation defaults.
         * @param ProjectName Stable project name supplied by the request.
         * @param Descriptor Seed descriptor to normalize in place.
         */
        void NormalizeDescriptorDefaults(const std::string& ProjectName, ProjectDescriptor& Descriptor)
        {
            Descriptor.Format.SchemaVersion = ProjectDescriptorService::kCurrentSchemaVersion;
            if (Descriptor.Format.MinimumToolVersion.empty())
            {
                Descriptor.Format.MinimumToolVersion =
                    std::string(ProjectDescriptorService::kDefaultMinimumToolVersion);
            }

            if (Descriptor.Project.Name.empty())
            {
                Descriptor.Project.Name = ProjectName;
            }
            if (Descriptor.Project.DisplayName.empty())
            {
                Descriptor.Project.DisplayName = Descriptor.Project.Name;
            }

            if (Descriptor.Paths.AssetRoot.empty())
            {
                Descriptor.Paths.AssetRoot = std::string(ProjectDescriptorService::kDefaultAssetRoot);
            }
            if (Descriptor.Paths.CodeRoot.empty())
            {
                Descriptor.Paths.CodeRoot = std::string(ProjectDescriptorService::kDefaultCodeRoot);
            }
            if (Descriptor.Paths.ConfigRoot.empty())
            {
                Descriptor.Paths.ConfigRoot = std::string(ProjectDescriptorService::kDefaultConfigRoot);
            }
            if (Descriptor.Paths.IntermediateRoot.empty())
            {
                Descriptor.Paths.IntermediateRoot = std::string(ProjectDescriptorService::kDefaultIntermediateRoot);
            }
            if (Descriptor.Paths.SavedRoot.empty())
            {
                Descriptor.Paths.SavedRoot = std::string(ProjectDescriptorService::kDefaultSavedRoot);
            }

            if (Descriptor.Startup.StartupLevelAsset.empty())
            {
                Descriptor.Startup.StartupLevelAsset = std::string(ProjectDescriptorService::kDefaultStartupLevelAsset);
            }
        }

    } // namespace

    TExpected<ProjectDescriptor> ProjectCreationService::BuildDefaultDescriptor(std::string_view ProjectName)
    {
        const std::string Name = Detail::TrimCopy(ProjectName);
        if (Name.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project name cannot be empty"));
        }

        ProjectDescriptor Descriptor{};
        NormalizeDescriptorDefaults(Name, Descriptor);
        return Descriptor;
    }

    Result ProjectCreationService::CreateProject(const ProjectCreationRequest& Request,
                                                 ProjectCreationResult* OutResult)
    {
        const std::string Name = Detail::TrimCopy(Request.ProjectName);
        if (Name.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Project name cannot be empty"));
        }

        auto ParentDirectoryResult = ResolveParentDirectory(Request.ParentDirectory);
        if (!ParentDirectoryResult)
        {
            return std::unexpected(ParentDirectoryResult.error());
        }

        auto ProjectFileNameResult = NormalizeProjectFileName(Request.ProjectFileName);
        if (!ProjectFileNameResult)
        {
            return std::unexpected(ProjectFileNameResult.error());
        }

        ProjectDescriptor Descriptor = Request.Descriptor;
        NormalizeDescriptorDefaults(Name, Descriptor);

        const std::filesystem::path ProjectRoot = ParentDirectoryResult.value() / Name;
        StarterRuntimeModuleLayout RuntimeModuleLayout{};
        bool WriteStarterRuntimeModule = false;
        if (Request.Code.CreateStarterRuntimeModule)
        {
            auto RuntimeModuleLayoutResult = BuildStarterRuntimeModuleLayout(Request, Descriptor, ProjectRoot);
            if (!RuntimeModuleLayoutResult)
            {
                return std::unexpected(RuntimeModuleLayoutResult.error());
            }

            RuntimeModuleLayout = std::move(*RuntimeModuleLayoutResult);
            ApplyStarterRuntimeModuleDefaults(Descriptor, RuntimeModuleLayout);
            WriteStarterRuntimeModule = true;
        }

        StarterEditorModuleLayout EditorModuleLayout{};
        bool WriteStarterEditorModule = false;
        if (Request.Code.CreateStarterEditorModule)
        {
            auto EditorModuleLayoutResult = BuildStarterEditorModuleLayout(Request, Descriptor, ProjectRoot);
            if (!EditorModuleLayoutResult)
            {
                return std::unexpected(EditorModuleLayoutResult.error());
            }

            EditorModuleLayout = std::move(*EditorModuleLayoutResult);
            ApplyStarterEditorModuleDefaults(Descriptor, EditorModuleLayout);
            WriteStarterEditorModule = true;
        }

        const Detail::ProjectBuildIntegrationLayout ProjectBuildLayout =
            Detail::BuildProjectBuildIntegrationLayout(Descriptor, ProjectRoot);

        const std::filesystem::path ProjectFilePath = ProjectRoot / ProjectFileNameResult.value();
        const std::filesystem::path AssetRootDirectory =
            ProjectRoot / std::filesystem::path(Descriptor.Paths.AssetRoot);
        const std::filesystem::path StartupLevelAssetPath =
            AssetRootDirectory / std::filesystem::path(Descriptor.Startup.StartupLevelAsset);
        const std::filesystem::path StarterScriptPath = Request.Templates.StarterScriptTemplateSourcePath.empty()
            ? std::filesystem::path{}
            : (AssetRootDirectory / Request.Templates.StarterScriptTemplateSourcePath.filename());
        const std::filesystem::path ShaderDirectory = Request.Templates.ShaderTemplateDirectory.empty()
            ? std::filesystem::path{}
            : (AssetRootDirectory / "Shaders");

        if (Result DirectoryResult = Detail::EnsureDirectory(ProjectRoot); !DirectoryResult)
        {
            return DirectoryResult;
        }
        if (Result DirectoryResult = Detail::EnsureDirectory(AssetRootDirectory); !DirectoryResult)
        {
            return DirectoryResult;
        }
        if (Result DirectoryResult =
                Detail::EnsureDirectory(ProjectRoot / std::filesystem::path(Descriptor.Paths.CodeRoot));
            !DirectoryResult)
        {
            return DirectoryResult;
        }
        if (Result DirectoryResult =
                Detail::EnsureDirectory(ProjectRoot / std::filesystem::path(Descriptor.Paths.ConfigRoot));
            !DirectoryResult)
        {
            return DirectoryResult;
        }
        if (Result DirectoryResult =
                Detail::EnsureDirectory(ProjectRoot / std::filesystem::path(Descriptor.Paths.IntermediateRoot));
            !DirectoryResult)
        {
            return DirectoryResult;
        }
        if (Result DirectoryResult =
                Detail::EnsureDirectory(ProjectRoot / std::filesystem::path(Descriptor.Paths.SavedRoot));
            !DirectoryResult)
        {
            return DirectoryResult;
        }
        if (WriteStarterRuntimeModule)
        {
            if (Result DirectoryResult = Detail::EnsureDirectory(RuntimeModuleLayout.ModuleRootDirectory);
                !DirectoryResult)
            {
                return DirectoryResult;
            }
        }
        if (WriteStarterEditorModule)
        {
            if (Result DirectoryResult = Detail::EnsureDirectory(EditorModuleLayout.ModuleRootDirectory);
                !DirectoryResult)
            {
                return DirectoryResult;
            }
        }

        if (!Request.Templates.StarterLevelTemplateSourcePath.empty())
        {
            if (Result CopyResult =
                    CopyOptionalTemplateFile(Request.Templates.StarterLevelTemplateSourcePath, StartupLevelAssetPath);
                !CopyResult)
            {
                return CopyResult;
            }
        }
        else if (Result StarterLevelResult = WriteDefaultStarterLevel(StartupLevelAssetPath); !StarterLevelResult)
        {
            return StarterLevelResult;
        }

        if (Result ScriptResult =
                CopyOptionalTemplateFile(Request.Templates.StarterScriptTemplateSourcePath, StarterScriptPath);
            !ScriptResult)
        {
            return ScriptResult;
        }

        if (!Request.Templates.ShaderTemplateDirectory.empty())
        {
            auto CopyResult =
                CopyDirectoryContentsRecursive(Request.Templates.ShaderTemplateDirectory, ShaderDirectory);
            if (!CopyResult)
            {
                return std::unexpected(MakeError(EErrorCode::InternalError, CopyResult.error()));
            }
        }

        std::vector<std::filesystem::path> GeneratedFiles{};
        if (WriteStarterRuntimeModule)
        {
            if (Result WriteCodeResult =
                    WriteStarterRuntimeModuleFiles(Descriptor, RuntimeModuleLayout, &GeneratedFiles);
                !WriteCodeResult)
            {
                return WriteCodeResult;
            }
        }
        if (WriteStarterEditorModule)
        {
            if (Result WriteEditorCodeResult = WriteStarterEditorModuleFiles(EditorModuleLayout, &GeneratedFiles);
                !WriteEditorCodeResult)
            {
                return WriteEditorCodeResult;
            }
        }
        if (Result WriteBuildIntegrationResult =
                Detail::WriteProjectBuildIntegrationFiles(Descriptor, ProjectBuildLayout, &GeneratedFiles);
            !WriteBuildIntegrationResult)
        {
            return WriteBuildIntegrationResult;
        }

        if (Result SaveResult = ProjectDescriptorService::Save(Descriptor, ProjectFilePath.string()); !SaveResult)
        {
            return SaveResult;
        }

        if (OutResult)
        {
            auto ResolvedProjectResult = ProjectDescriptorService::LoadResolved(ProjectFilePath.string());
            if (!ResolvedProjectResult)
            {
                return std::unexpected(ResolvedProjectResult.error());
            }

            OutResult->Project = std::move(*ResolvedProjectResult);
            OutResult->StartupLevelAssetPath = StartupLevelAssetPath;
            OutResult->StarterScriptPath = StarterScriptPath;
            OutResult->ShaderDirectory = ShaderDirectory;
            OutResult->ProjectCodeRootCMakePath = ProjectBuildLayout.ProjectCodeRootCMakePath;
            OutResult->GeneratedProjectModulesCMakePath = ProjectBuildLayout.GeneratedProjectModulesCMakePath;
            OutResult->RuntimeModuleDirectory =
                WriteStarterRuntimeModule ? RuntimeModuleLayout.ModuleRootDirectory : std::filesystem::path{};
            OutResult->RuntimeModuleRootCMakePath =
                WriteStarterRuntimeModule ? RuntimeModuleLayout.ModuleRootCMakePath : std::filesystem::path{};
            OutResult->RuntimeModuleCMakePath =
                WriteStarterRuntimeModule ? RuntimeModuleLayout.CMakeFragmentPath : std::filesystem::path{};
            OutResult->EditorModuleDirectory =
                WriteStarterEditorModule ? EditorModuleLayout.ModuleRootDirectory : std::filesystem::path{};
            OutResult->EditorModuleRootCMakePath =
                WriteStarterEditorModule ? EditorModuleLayout.ModuleRootCMakePath : std::filesystem::path{};
            OutResult->EditorModuleCMakePath =
                WriteStarterEditorModule ? EditorModuleLayout.CMakeFragmentPath : std::filesystem::path{};
            OutResult->DefaultGameConfigPath =
                WriteStarterRuntimeModule ? RuntimeModuleLayout.DefaultGameConfigPath : std::filesystem::path{};
            OutResult->GeneratedFiles = std::move(GeneratedFiles);
        }

        return Ok();
    }

} // namespace SnAPI::GameFramework
