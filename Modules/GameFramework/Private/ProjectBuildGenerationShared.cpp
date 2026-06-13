#include "ProjectBuildGenerationShared.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace SnAPI::GameFramework::Detail
{
    namespace
    {

        /**
         * @brief Return `true` when the character can start one generated identifier.
         * @param Character Candidate character.
         * @return `true` for ASCII letters and underscore.
         */
        [[nodiscard]] bool IsIdentifierStart(const char Character)
        {
            const unsigned char Value = static_cast<unsigned char>(Character);
            return std::isalpha(Value) != 0 || Character == '_';
        }

        /**
         * @brief Return `true` when the character can continue one generated identifier.
         * @param Character Candidate character.
         * @return `true` for ASCII letters, digits, and underscore.
         */
        [[nodiscard]] bool IsIdentifierContinue(const char Character)
        {
            const unsigned char Value = static_cast<unsigned char>(Character);
            return std::isalnum(Value) != 0 || Character == '_';
        }

        /**
         * @brief Convert one stored path field into a quoted CMake path expression.
         * @param StoredPath Descriptor path field.
         * @return Quoted CMake path expression suitable for generated includes.
         */
        [[nodiscard]] std::string BuildProjectRelativeCMakePathExpression(const std::string_view StoredPath,
                                                                          const std::string_view RootVariableName)
        {
            const std::filesystem::path PathValue(StoredPath);
            if (PathValue.is_absolute())
            {
                return "\"" + PathValue.lexically_normal().generic_string() + "\"";
            }

            return "\"${" + std::string(RootVariableName) + "}/" + PathValue.lexically_normal().generic_string() + "\"";
        }

    } // namespace

    std::string TrimCopy(const std::string_view Text)
    {
        std::size_t Begin = 0u;
        std::size_t End = Text.size();
        while (Begin < End && std::isspace(static_cast<unsigned char>(Text[Begin])) != 0)
        {
            ++Begin;
        }
        while (End > Begin && std::isspace(static_cast<unsigned char>(Text[End - 1u])) != 0)
        {
            --End;
        }
        return std::string(Text.substr(Begin, End - Begin));
    }

    TExpected<std::string> NormalizeIdentifier(const std::string_view RawValue, const std::string_view FieldName)
    {
        const std::string Value = TrimCopy(RawValue);
        if (Value.empty())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, std::string(FieldName) + " cannot be empty"));
        }

        if (!IsIdentifierStart(Value.front()))
        {
            return std::unexpected(
                MakeError(EErrorCode::InvalidArgument,
                          std::string(FieldName) + " must start with an ASCII letter or underscore"));
        }

        for (const char Character : Value)
        {
            if (!IsIdentifierContinue(Character))
            {
                return std::unexpected(
                    MakeError(EErrorCode::InvalidArgument,
                              std::string(FieldName) + " may contain only ASCII letters, digits, and underscore"));
            }
        }

        return Value;
    }

    std::string BuildPascalCaseStem(const std::string_view Identifier)
    {
        std::string Result{};
        Result.reserve(Identifier.size());

        bool CapitalizeNext = true;
        for (const char Character : Identifier)
        {
            if (Character == '_')
            {
                CapitalizeNext = true;
                continue;
            }

            const unsigned char Value = static_cast<unsigned char>(Character);
            Result.push_back(static_cast<char>(CapitalizeNext ? std::toupper(Value) : Character));
            CapitalizeNext = false;
        }

        return Result;
    }

    std::string BuildDefaultModuleRootField(const std::string_view CodeRoot, const std::string_view ModuleName)
    {
        return (std::filesystem::path(CodeRoot) / std::filesystem::path(ModuleName))
            .lexically_normal()
            .generic_string();
    }

    ProjectModuleDescriptor* FindModuleDescriptor(ProjectDescriptor& Descriptor, const std::string_view ModuleName)
    {
        for (ProjectModuleDescriptor& Module : Descriptor.Modules)
        {
            if (Module.Name == ModuleName)
            {
                return &Module;
            }
        }

        return nullptr;
    }

    const ProjectModuleDescriptor* FindModuleDescriptor(const ProjectDescriptor& Descriptor,
                                                        const std::string_view ModuleName)
    {
        for (const ProjectModuleDescriptor& Module : Descriptor.Modules)
        {
            if (Module.Name == ModuleName)
            {
                return &Module;
            }
        }

        return nullptr;
    }

    void EnsureDependency(std::vector<std::string>& Dependencies, const std::string_view DependencyName)
    {
        if (DependencyName.empty())
        {
            return;
        }

        if (std::find(Dependencies.begin(), Dependencies.end(), DependencyName) == Dependencies.end())
        {
            Dependencies.push_back(std::string(DependencyName));
        }
    }

    Result EnsureDirectory(const std::filesystem::path& Directory)
    {
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

    Result WriteTextFile(const std::filesystem::path& DestinationPath, const std::string& Text)
    {
        if (Result DirectoryResult = EnsureDirectory(DestinationPath.parent_path()); !DirectoryResult)
        {
            return DirectoryResult;
        }

        std::ofstream Output(DestinationPath, std::ios::binary | std::ios::trunc);
        if (!Output.is_open())
        {
            return std::unexpected(
                MakeError(EErrorCode::InternalError,
                          "Failed to open generated file '" + DestinationPath.string() + "' for write"));
        }

        Output.write(Text.data(), static_cast<std::streamsize>(Text.size()));
        if (!Output.good())
        {
            return std::unexpected(MakeError(EErrorCode::InternalError,
                                             "Failed to write generated file '" + DestinationPath.string() + "'"));
        }

        return Ok();
    }

    ProjectBuildIntegrationLayout BuildProjectBuildIntegrationLayout(const ProjectDescriptor& Descriptor,
                                                                     const std::filesystem::path& ProjectRoot)
    {
        return BuildWorkspaceBuildIntegrationLayout(
            Descriptor.Paths,
            ProjectRoot,
            BuildIntegrationSettings{
                .RootVariableName = "SNAPI_PROJECT_ROOT_DIR",
                .GeneratedModulesFileName = "ProjectModules.cmake",
                .HostDisplayName = "project",
            });
    }

    ProjectBuildIntegrationLayout BuildWorkspaceBuildIntegrationLayout(const ProjectDescriptorPaths& Paths,
                                                                      const std::filesystem::path& WorkspaceRoot,
                                                                      const BuildIntegrationSettings& Settings)
    {
        ProjectBuildIntegrationLayout Layout{};
        Layout.GeneratedBuildDirectory =
            WorkspaceRoot / std::filesystem::path(Paths.IntermediateRoot) / "Build" / "Generated";
        Layout.ProjectCodeRootCMakePath = WorkspaceRoot / std::filesystem::path(Paths.CodeRoot) / "CMakeLists.txt";
        Layout.GeneratedProjectModulesCMakePath = Layout.GeneratedBuildDirectory / Settings.GeneratedModulesFileName;
        return Layout;
    }

    std::string BuildProjectCodeRootCMake()
    {
        return BuildWorkspaceCodeRootCMake(BuildIntegrationSettings{
            .RootVariableName = "SNAPI_PROJECT_ROOT_DIR",
            .GeneratedModulesFileName = "ProjectModules.cmake",
            .HostDisplayName = "project",
        });
    }

    std::string BuildWorkspaceCodeRootCMake(const BuildIntegrationSettings& Settings)
    {
        return std::string("# Minimal bridge for SnAPI ") + Settings.HostDisplayName +
            " modules.\n"
            "set(" + Settings.RootVariableName +
            " \"${CMAKE_CURRENT_LIST_DIR}/..\")\n"
            "include(\"${" + Settings.RootVariableName + "}/Intermediate/Build/Generated/" +
            Settings.GeneratedModulesFileName +
            "\" OPTIONAL)\n"
            "unset(" + Settings.RootVariableName + ")\n";
    }

    std::string BuildGeneratedProjectModulesCMake(const ProjectDescriptor& Descriptor)
    {
        return BuildGeneratedWorkspaceModulesCMake(
            Descriptor.Modules,
            BuildIntegrationSettings{
                .RootVariableName = "SNAPI_PROJECT_ROOT_DIR",
                .GeneratedModulesFileName = "ProjectModules.cmake",
                .HostDisplayName = "project",
            });
    }

    std::string BuildGeneratedWorkspaceModulesCMake(const std::vector<ProjectModuleDescriptor>& Modules,
                                                    const BuildIntegrationSettings& Settings)
    {
        std::string Text = "# Auto-generated by SnAPI Build System. Safe to regenerate.\n\n";

        for (const ProjectModuleDescriptor& Module : Modules)
        {
            if (Module.Root.empty())
            {
                continue;
            }

            Text += "add_subdirectory("
                + BuildProjectRelativeCMakePathExpression(Module.Root, Settings.RootVariableName) + ")\n";
        }

        if (!Modules.empty())
        {
            Text += "\n";
        }

        for (const ProjectModuleDescriptor& Module : Modules)
        {
            if (Module.Name.empty())
            {
                continue;
            }

            if (Module.LoadInEditor)
            {
                Text += "if(TARGET SnAPI.GameFramework.Editor)\n";
                Text += "    target_link_libraries(SnAPI.GameFramework.Editor PRIVATE " + Module.Name + ")\n";
                Text += "endif()\n";
            }

            if (Module.LoadInRuntime)
            {
                Text += "if(TARGET SnAPI.GameFramework.Runtime)\n";
                Text += "    target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE " + Module.Name + ")\n";
                Text += "endif()\n";
            }
        }

        if (Modules.empty())
        {
            Text += "# No " + Settings.HostDisplayName + " modules are currently declared.\n";
        }

        return Text;
    }

    Result WriteProjectBuildIntegrationFiles(const ProjectDescriptor& Descriptor,
                                             const ProjectBuildIntegrationLayout& Layout,
                                             std::vector<std::filesystem::path>* OutGeneratedFiles)
    {
        return WriteWorkspaceBuildIntegrationFiles(
            Descriptor.Modules,
            Layout,
            BuildIntegrationSettings{
                .RootVariableName = "SNAPI_PROJECT_ROOT_DIR",
                .GeneratedModulesFileName = "ProjectModules.cmake",
                .HostDisplayName = "project",
            },
            OutGeneratedFiles);
    }

    Result WriteWorkspaceBuildIntegrationFiles(const std::vector<ProjectModuleDescriptor>& Modules,
                                               const ProjectBuildIntegrationLayout& Layout,
                                               const BuildIntegrationSettings& Settings,
                                               std::vector<std::filesystem::path>* OutGeneratedFiles)
    {
        struct GeneratedTextFile
        {
            std::filesystem::path Path{};
            std::string Text{};
        };

        const std::vector<GeneratedTextFile> FilesToWrite = {
            {Layout.ProjectCodeRootCMakePath, BuildWorkspaceCodeRootCMake(Settings)},
            {Layout.GeneratedProjectModulesCMakePath, BuildGeneratedWorkspaceModulesCMake(Modules, Settings)},
        };

        for (const GeneratedTextFile& File : FilesToWrite)
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
