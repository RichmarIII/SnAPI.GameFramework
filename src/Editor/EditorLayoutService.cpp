#include "Editor/EditorLayoutService.h"

#include "AssetPipelineIds.h"
#include "BaseNode.h"
#include "CameraComponent.h"
#include "Conduit/Editor/Service.h"
#include "Editor/EditorAssetIconService.h"
#include "Editor/EditorAssetService.h"
#include "Editor/EditorBuildService.h"
#include "Editor/EditorCommandService.h"
#include "Editor/EditorPieService.h"
#include "Editor/EditorRootViewportService.h"
#include "Editor/EditorSceneService.h"
#include "Editor/EditorSelectionService.h"
#include "Editor/EditorThemeService.h"
#include "GameRuntime.h"
#include "Level.h"
#include "NodeCast.h"
#include "PawnBase.h"
#include "PlayerStart.h"
#include "ProjectDescriptor.h"
#include "Serialization.h"
#include "TypeRegistry.h"
#include "UIRenderViewport.h"
#include "World.h"

#include <TextureCompressorIds.h>

#include <UIContext.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace SnAPI::GameFramework::Editor
{
namespace
{
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

void ApplySelection(EditorSelectionModel& Model, const NodeHandle& Node)
{
    if (Node.IsNull())
    {
        Model.Clear();
        return;
    }

    (void)Model.SelectNode(Node);
}

void LogConduitCompileOutput(const Conduit::Editor::CompileOutput& Output)
{
    if (Output.Diagnostics.empty())
    {
        std::printf("[SnAPI][ConduitCompile] Compiled successfully with no diagnostics.\n");
        std::fflush(stdout);
        return;
    }

    for (const auto& Diagnostic : Output.Diagnostics)
    {
        const char* SeverityLabel = "Info";
        switch (Diagnostic.Severity)
        {
            case Conduit::Editor::ECompileDiagnosticSeverity::Warning:
                SeverityLabel = "Warning";
                break;
            case Conduit::Editor::ECompileDiagnosticSeverity::Error:
                SeverityLabel = "Error";
                break;
            case Conduit::Editor::ECompileDiagnosticSeverity::Info:
            default:
                SeverityLabel = "Info";
                break;
        }

        std::printf("[SnAPI][ConduitCompile][%s] %s\n", SeverityLabel, Diagnostic.Message.c_str());
    }
    std::fflush(stdout);
}

class SelectNodeCommand final : public IEditorCommand
{
public:
    SelectNodeCommand(const NodeHandle& Previous, const NodeHandle& Next)
        : m_previous(Previous)
        , m_next(Next)
    {
    }

    [[nodiscard]] std::string_view Name() const override
    {
        return "SelectNodeCommand";
    }

    Result Execute(EditorServiceContext& Context) override
    {
        auto* SelectionService = Context.GetService<EditorSelectionService>();
        if (!SelectionService)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Selection service is not available"));
        }

        ApplySelection(SelectionService->Model(), m_next);
        return Ok();
    }

    Result Undo(EditorServiceContext& Context) override
    {
        auto* SelectionService = Context.GetService<EditorSelectionService>();
        if (!SelectionService)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Selection service is not available"));
        }

        ApplySelection(SelectionService->Model(), m_previous);
        return Ok();
    }

private:
    NodeHandle m_previous{};
    NodeHandle m_next{};
};

[[nodiscard]] std::size_t ComputeAssetListSignature(const std::vector<EditorAssetService::DiscoveredAsset>& Assets)
{
    std::size_t Seed = Assets.size();
    const auto HashCombine = [&Seed](const std::size_t Value) {
        Seed ^= Value + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
    };

    for (const auto& Asset : Assets)
    {
        HashCombine(std::hash<std::string>{}(Asset.Key));
        HashCombine(std::hash<std::string>{}(Asset.Name));
        HashCombine(std::hash<std::string>{}(Asset.TypeLabel));
        HashCombine(std::hash<std::string>{}(Asset.Variant));
        HashCombine(static_cast<std::size_t>(Asset.IsRuntime ? 1u : 0u));
        HashCombine(static_cast<std::size_t>(Asset.IsDirty ? 1u : 0u));
        HashCombine(static_cast<std::size_t>(Asset.CanSave ? 1u : 0u));
    }

    return Seed;
}

[[nodiscard]] std::size_t ComputeAssetDetailsSignature(const EditorLayout::ContentAssetDetails& Details)
{
    std::size_t Seed = 0;
    const auto HashCombine = [&Seed](const std::size_t Value) {
        Seed ^= Value + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
    };

    HashCombine(std::hash<std::string>{}(Details.Name));
    HashCombine(std::hash<std::string>{}(Details.Type));
    HashCombine(std::hash<std::string>{}(Details.Variant));
    HashCombine(std::hash<std::string>{}(Details.AssetId));
    HashCombine(std::hash<std::string>{}(Details.Status));
    HashCombine(static_cast<std::size_t>(Details.IsRuntime ? 1u : 0u));
    HashCombine(static_cast<std::size_t>(Details.IsDirty ? 1u : 0u));
    HashCombine(static_cast<std::size_t>(Details.CanPlace ? 1u : 0u));
    HashCombine(static_cast<std::size_t>(Details.CanSave ? 1u : 0u));
    return Seed;
}

[[nodiscard]] std::string FormatBinaryByteSize(const std::uint64_t Bytes)
{
    constexpr std::array<std::string_view, 5> Units{"B", "KB", "MB", "GB", "TB"};
    double Value = static_cast<double>(Bytes);
    std::size_t UnitIndex = 0;
    while (Value >= 1024.0 && UnitIndex + 1u < Units.size())
    {
        Value /= 1024.0;
        ++UnitIndex;
    }

    std::ostringstream Stream{};
    if (UnitIndex == 0)
    {
        Stream << static_cast<std::uint64_t>(Value) << ' ' << Units[UnitIndex];
    }
    else
    {
        Stream << std::fixed << std::setprecision(Value >= 100.0 ? 1 : 2) << Value << ' ' << Units[UnitIndex];
    }
    return Stream.str();
}

[[nodiscard]] std::string ShortTypeLabel(std::string_view QualifiedName)
{
    return PrettyReflectedTypeName(QualifiedName);
}

[[nodiscard]] bool CanPlaceAssetKind(const ::SnAPI::AssetPipeline::TypeId& AssetKind)
{
    return AssetKind == AssetKindNode() ||
           AssetKind == AssetKindLevel() ||
           AssetKind == AssetKindWorld() ||
           AssetKind == AssetKindStaticMesh() ||
           AssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture;
}

[[nodiscard]] BaseNode* ResolveNodeFromHandle(NodeHandle& InOutHandle, World& WorldRef)
{
    if (InOutHandle.IsNull())
    {
        return nullptr;
    }

    if (auto* Node = WorldRef.BorrowedNode(InOutHandle))
    {
        return Node;
    }

    if (auto* Node = InOutHandle.BorrowedSlowByUuid())
    {
        InOutHandle = Node->Handle();
        return Node;
    }

    if (const auto HandleResult = WorldRef.NodeHandleById(InOutHandle.Id); HandleResult.has_value())
    {
        InOutHandle = *HandleResult;
        return WorldRef.BorrowedNode(InOutHandle);
    }

    return nullptr;
}

void InitializeCreatedNodeDefaults(IWorld& WorldRef, BaseNode& Node)
{
    NodeHandle NodeHandleValue = Node.Handle();
    (void)WorldRef.RequestNodeOnCreate(NodeHandleValue);
}

[[nodiscard]] const char* EditorErrorCodeLabel(const EErrorCode Code)
{
    switch (Code)
    {
    case EErrorCode::None:
        return "None";
    case EErrorCode::NotFound:
        return "NotFound";
    case EErrorCode::InvalidArgument:
        return "InvalidArgument";
    case EErrorCode::TypeMismatch:
        return "TypeMismatch";
    case EErrorCode::OutOfRange:
        return "OutOfRange";
    case EErrorCode::AlreadyExists:
        return "AlreadyExists";
    case EErrorCode::NotReady:
        return "NotReady";
    case EErrorCode::InternalError:
        return "InternalError";
    default:
        return "Unknown";
    }
}

template<typename TExpectedLike>
void ReportEditorExpectedFailure(std::string_view Operation, const TExpectedLike& ResultValue)
{
#if defined(WITH_EDITOR) && WITH_EDITOR
    if (ResultValue)
    {
        return;
    }

    const Error& ErrorValue = ResultValue.error();
    std::fprintf(stdout,
                 "[SnAPI][EditorError][%s] %.*s: %s\n",
                 EditorErrorCodeLabel(ErrorValue.Code),
                 static_cast<int>(Operation.size()),
                 Operation.data(),
                 ErrorValue.Message.c_str());
    std::fflush(stdout);
#else
    (void)Operation;
    (void)ResultValue;
#endif
}

[[nodiscard]] std::string_view BuildConfigurationLabel(const EBuildConfiguration Configuration)
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
    default:
        return "Development";
    }
}

[[nodiscard]] std::string_view BuildStatusLabel(const EBuildExecutionStatus Status)
{
    switch (Status)
    {
    case EBuildExecutionStatus::Succeeded:
        return "Succeeded";
    case EBuildExecutionStatus::Failed:
        return "Failed";
    case EBuildExecutionStatus::Cancelled:
        return "Cancelled";
    default:
        return "Unknown";
    }
}

[[nodiscard]] std::string_view HistoryStateLabel(const EBuildHistoryEntryState State)
{
    switch (State)
    {
    case EBuildHistoryEntryState::Complete:
        return "Complete";
    case EBuildHistoryEntryState::Incomplete:
        return "Incomplete";
    default:
        return "Unknown";
    }
}

template<typename TValue>
void HashCombine(std::size_t& Seed, const TValue& Value)
{
    Seed ^= std::hash<TValue>{}(Value) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
}

[[nodiscard]] std::string DescribeOutputSummary(const BuildExecutionReport& Report)
{
    if (!Report.ArchiveFilePath.empty())
    {
        return "Archive: " + Report.ArchiveFilePath.string();
    }
    if (!Report.PackageDirectoryPath.empty())
    {
        return "Package: " + Report.PackageDirectoryPath.string();
    }
    if (!Report.PackageOutputRootDirectory.empty())
    {
        return "Output Root: " + Report.PackageOutputRootDirectory.string();
    }
    if (!Report.StageDirectory.empty())
    {
        return "Stage: " + Report.StageDirectory.string();
    }
    return {};
}

[[nodiscard]] std::string DescribePlanSummary(const EditorBuildPlan& Plan)
{
    return "Planned build " + Plan.Graph.BuildId + " with " + std::to_string(Plan.Graph.Nodes.size()) + " nodes for " +
           Plan.Request.Project.Descriptor.Project.Name + ".";
}

[[nodiscard]] std::string DescribeBuildSummary(const BuildExecutionReport& Report)
{
    std::string Summary = "Build " + Report.BuildId + " " + std::string(BuildStatusLabel(Report.Status)) +
                          " with " + std::to_string(Report.NodeRecords.size()) + " node records.";
    if (!Report.RequestHash.empty())
    {
        Summary += "\nRequest: " + Report.RequestHash;
    }
    if (Report.Status != EBuildExecutionStatus::Succeeded)
    {
        const auto FailedNodeIt = std::find_if(
            Report.NodeRecords.begin(),
            Report.NodeRecords.end(),
            [](const BuildNodeExecutionRecord& Record) { return Record.Status == EBuildNodeExecutionStatus::Failed; });
        if (FailedNodeIt != Report.NodeRecords.end() && !FailedNodeIt->Message.empty())
        {
            Summary += "\nReason: Node '" + FailedNodeIt->Name + "' failed: " + FailedNodeIt->Message;
        }
        else
        {
            const auto ValidationIssueIt =
                std::find_if(Report.ValidationIssues.begin(),
                             Report.ValidationIssues.end(),
                             [](const BuildValidationIssue& Issue)
                             { return Issue.Severity == EBuildValidationSeverity::Error; });
            if (ValidationIssueIt != Report.ValidationIssues.end())
            {
                Summary += "\nReason: " + ValidationIssueIt->RuleId + ": " + ValidationIssueIt->Message;
            }
            else if (Report.Status == EBuildExecutionStatus::Cancelled)
            {
                Summary += "\nReason: Build cancelled before completion.";
            }
        }
    }
    return Summary;
}

[[nodiscard]] Result BuildActionResultFromExecutionReport(const TExpected<BuildExecutionReport>& ReportValue,
                                                          const std::string_view StatusMessage)
{
    if (!ReportValue)
    {
        return std::unexpected(ReportValue.error());
    }

    if (ReportValue->Status == EBuildExecutionStatus::Succeeded)
    {
        return Ok();
    }

    const std::string Message =
        !StatusMessage.empty()
            ? std::string(StatusMessage)
            : "Build " + ReportValue->BuildId + " completed with status " +
                  std::string(BuildStatusLabel(ReportValue->Status)) + ".";
    return std::unexpected(MakeError(ReportValue->Status == EBuildExecutionStatus::Cancelled ? EErrorCode::NotReady
                                                                                             : EErrorCode::InternalError,
                                     Message));
}

[[nodiscard]] std::string DescribeHistorySummary(const BuildHistoryEntry& Entry)
{
    std::string Summary = std::string(HistoryStateLabel(Entry.State));
    if (Entry.State == EBuildHistoryEntryState::Complete)
    {
        Summary += " / " + std::string(BuildStatusLabel(Entry.Status));
    }
    if (!Entry.StartedAtUtc.empty())
    {
        Summary += " / Started: " + Entry.StartedAtUtc;
    }
    if (Entry.OutputFileCount > 0u)
    {
        Summary += " / Outputs: " + std::to_string(Entry.OutputFileCount);
    }
    return Summary;
}

[[nodiscard]] std::string BuildDiscoveredAssetField(const EditorAssetService::DiscoveredAsset& Asset,
                                                    const std::string_view AssetRootDirectory)
{
    if (!Asset.SourceFilePath.empty() && !AssetRootDirectory.empty())
    {
        return ProjectDescriptorService::ToProjectRelativePathField(Asset.SourceFilePath, AssetRootDirectory);
    }

    if (!Asset.Name.empty())
    {
        return Asset.Name;
    }

    return Asset.Key;
}

[[nodiscard]] bool IsLevelAssetField(const std::string_view AssetField)
{
    if (AssetField.empty())
    {
        return false;
    }

    return std::filesystem::path(std::string(AssetField)).extension() == ".level";
}

[[nodiscard]] std::size_t ComputeBuildPanelStateSignature(const EditorLayout::BuildPanelState& State)
{
    std::size_t Signature = 0u;
    HashCombine(Signature, State.ProjectLoaded);
    HashCombine(Signature, State.ProjectName);
    HashCombine(Signature, State.ProjectFilePath);
    HashCombine(Signature, State.AssetRootDirectory);
    HashCombine(Signature, State.BuildInProgress);
    HashCombine(Signature, State.StatusMessage);
    HashCombine(Signature, State.LastPlanSummary);
    HashCombine(Signature, State.LastBuildId);
    HashCombine(Signature, State.LastBuildSummary);
    HashCombine(Signature, State.LastBuildOutputSummary);
    HashCombine(Signature, State.HistoryComparisonSummary);
    HashCombine(Signature, State.ConsoleLogRevision);
    HashCombine(Signature, State.Profiles.size());
    for (const auto& Profile : State.Profiles)
    {
        HashCombine(Signature, Profile.Name);
        HashCombine(Signature, Profile.Label);
        HashCombine(Signature, Profile.Summary);
        HashCombine(Signature, Profile.Platform);
        HashCombine(Signature, Profile.Configuration);
        HashCombine(Signature, Profile.ExecutionEnvironment);
        HashCombine(Signature, Profile.SelectedLevels.size());
        for (const auto& Value : Profile.SelectedLevels)
        {
            HashCombine(Signature, Value);
        }
        HashCombine(Signature, Profile.ExplicitAssets.size());
        for (const auto& Value : Profile.ExplicitAssets)
        {
            HashCombine(Signature, Value);
        }
        HashCombine(Signature, Profile.IncludeFolders.size());
        for (const auto& Value : Profile.IncludeFolders)
        {
            HashCombine(Signature, Value);
        }
        HashCombine(Signature, Profile.ExcludeFolders.size());
        for (const auto& Value : Profile.ExcludeFolders)
        {
            HashCombine(Signature, Value);
        }
        HashCombine(Signature, Profile.IncludeAssetLabels.size());
        for (const auto& Value : Profile.IncludeAssetLabels)
        {
            HashCombine(Signature, Value);
        }
        HashCombine(Signature, Profile.ExcludeAssetLabels.size());
        for (const auto& Value : Profile.ExcludeAssetLabels)
        {
            HashCombine(Signature, Value);
        }
        HashCombine(Signature, Profile.IncludeAssetKinds.size());
        for (const auto& Value : Profile.IncludeAssetKinds)
        {
            HashCombine(Signature, Value);
        }
        HashCombine(Signature, Profile.ExcludeAssetKinds.size());
        for (const auto& Value : Profile.ExcludeAssetKinds)
        {
            HashCombine(Signature, Value);
        }
        HashCombine(Signature, static_cast<std::size_t>(Profile.DependencyPolicy));
        HashCombine(Signature, static_cast<std::size_t>(Profile.ChunkStrategy));
        HashCombine(Signature, Profile.AllowExplicitOverrideExcludes);
        HashCombine(Signature, Profile.ArchiveEnabled);
        HashCombine(Signature, Profile.ArchiveFormat);
        HashCombine(Signature, Profile.IsDefault);
        HashCombine(Signature, Profile.IsAdHoc);
    }
    HashCombine(Signature, State.AvailableLevels.size());
    for (const auto& Entry : State.AvailableLevels)
    {
        HashCombine(Signature, Entry);
    }
    HashCombine(Signature, State.AvailableAssets.size());
    for (const auto& Entry : State.AvailableAssets)
    {
        HashCombine(Signature, Entry);
    }
    HashCombine(Signature, State.AvailableAssetKinds.size());
    for (const auto& Entry : State.AvailableAssetKinds)
    {
        HashCombine(Signature, Entry);
    }
    HashCombine(Signature, State.HistoryEntries.size());
    for (const auto& Entry : State.HistoryEntries)
    {
        HashCombine(Signature, Entry.BuildId);
        HashCombine(Signature, Entry.Label);
        HashCombine(Signature, Entry.Summary);
        HashCombine(Signature, Entry.RequestHash);
        HashCombine(Signature, Entry.StartedAtUtc);
        HashCombine(Signature, Entry.FinishedAtUtc);
        HashCombine(Signature, Entry.IsComplete);
        HashCombine(Signature, Entry.IsLatest);
    }
    return Signature;
}

[[nodiscard]] Result ExecuteHierarchyAction(EditorServiceContext& Context,
                                            EditorLayout::HierarchyActionRequest& Request)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "World is not available"));
    }

    if (Request.Action == EditorLayout::EHierarchyAction::CreatePrefab)
    {
        auto* AssetService = Context.GetService<EditorAssetService>();
        if (!AssetService)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Asset service is not available"));
        }
        if (Request.TargetNode.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Prefab creation requires a target node"));
        }
        return AssetService->CreateRuntimePrefabFromNode(Context, Request.TargetNode);
    }

    if (Request.Action == EditorLayout::EHierarchyAction::DeleteNode)
    {
        if (Request.TargetNode.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Delete node requires a target node"));
        }

        BaseNode* TargetNode = ResolveNodeFromHandle(Request.TargetNode, *WorldPtr);
        if (!TargetNode)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Target node not found"));
        }

        if (TypeRegistry::Instance().IsA(TargetNode->TypeKey(), StaticTypeId<World>()))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "World cannot be deleted"));
        }

        NodeHandle TargetHandle = TargetNode->Handle();
        auto DestroyResult = WorldPtr->DestroyNode(TargetHandle);
        if (!DestroyResult)
        {
            return std::unexpected(DestroyResult.error());
        }
        return Ok();
    }

    if (Request.Action == EditorLayout::EHierarchyAction::RemoveComponentType)
    {
        if (Request.TargetNode.IsNull())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Remove component requires a target node"));
        }
        if (Request.Type == TypeId{})
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Remove component requires a component type"));
        }

        BaseNode* TargetNode = ResolveNodeFromHandle(Request.TargetNode, *WorldPtr);
        if (!TargetNode)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Target node not found"));
        }

        NodeHandle TargetHandle = TargetNode->Handle();
        auto RemoveResult = WorldPtr->RemoveComponentByType(TargetHandle, Request.Type);
        if (!RemoveResult)
        {
            return std::unexpected(RemoveResult.error());
        }
        return Ok();
    }

    const TypeInfo* Type = TypeRegistry::Instance().Find(Request.Type);
    if (!Type)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Requested type is not registered"));
    }

    if (Request.Action == EditorLayout::EHierarchyAction::AddNodeType)
    {
        if (!TypeRegistry::Instance().IsA(Type->Id, StaticTypeId<BaseNode>()))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Requested type is not a node type"));
        }
        if (TypeRegistry::Instance().IsA(Type->Id, StaticTypeId<World>()))
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "World nodes cannot be created from hierarchy"));
        }
        if (TypeRegistry::Instance().IsA(Type->Id, StaticTypeId<Level>()) && !Request.TargetIsWorldRoot)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Levels can only be added under the world root"));
        }

        BaseNode* ParentNode = nullptr;
        if (!Request.TargetIsWorldRoot)
        {
            ParentNode = ResolveNodeFromHandle(Request.TargetNode, *WorldPtr);
            if (!ParentNode)
            {
                return std::unexpected(MakeError(EErrorCode::NotFound, "Target node not found"));
            }
        }

        std::string NodeName = ShortTypeLabel(Type->Name);
        if (NodeName.empty())
        {
            NodeName = "Node";
        }

        auto CreateResult = WorldPtr->CreateNode(Type->Id, NodeName);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }

        if (!Request.TargetIsWorldRoot)
        {
            NodeHandle ParentHandle = ParentNode->Handle();
            auto AttachResult = WorldPtr->AttachChild(ParentHandle, *CreateResult);
            if (!AttachResult)
            {
                return std::unexpected(AttachResult.error());
            }
        }

        if (BaseNode* CreatedNode = CreateResult->Borrowed())
        {
            InitializeCreatedNodeDefaults(*WorldPtr, *CreatedNode);
        }
        return Ok();
    }

    if (Request.Action != EditorLayout::EHierarchyAction::AddComponentType)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported hierarchy action"));
    }

    if (!ComponentSerializationRegistry::Instance().Has(Type->Id))
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Requested type is not a component type"));
    }
    if (Request.TargetNode.IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Target node is required for component creation"));
    }

    BaseNode* TargetNode = ResolveNodeFromHandle(Request.TargetNode, *WorldPtr);
    if (!TargetNode)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Target node not found"));
    }

    NodeHandle TargetHandle = TargetNode->Handle();
    auto CreateComponentResult = WorldPtr->CreateComponent(TargetHandle, Type->Id);
    if (!CreateComponentResult)
    {
        return std::unexpected(CreateComponentResult.error());
    }

    return Ok();
}

} // namespace

std::string_view EditorLayoutService::Name() const
{
    return "EditorLayoutService";
}

std::vector<std::type_index> EditorLayoutService::Dependencies() const
{
    return {std::type_index(typeid(EditorThemeService)),
            std::type_index(typeid(EditorSceneService)),
            std::type_index(typeid(EditorSelectionService)),
            std::type_index(typeid(EditorPieService)),
            std::type_index(typeid(EditorRootViewportService)),
            std::type_index(typeid(EditorCommandService)),
            std::type_index(typeid(EditorAssetService)),
            std::type_index(typeid(EditorBuildService)),
            std::type_index(typeid(EditorAssetIconService)),
            std::type_index(typeid(Conduit::Editor::ConduitEditorService))};
}

Result EditorLayoutService::Initialize(EditorServiceContext& Context)
{
    auto* ThemeService = Context.GetService<EditorThemeService>();
    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* PieService = Context.GetService<EditorPieService>();
    auto* AssetService = Context.GetService<EditorAssetService>();
    auto* BuildService = Context.GetService<EditorBuildService>();
    auto* IconService = Context.GetService<EditorAssetIconService>();
    auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
    if (!ThemeService || !SceneService || !SelectionService || !PieService || !AssetService || !BuildService || !IconService ||
        !ConduitService)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Missing required editor services for layout"));
    }

    m_hasPendingSelectionRequest = false;
    m_pendingSelectionRequest = {};
    m_hasPendingHierarchyActionRequest = false;
    m_pendingHierarchyActionRequest = {};
    m_hasPendingToolbarAction = false;
    m_pendingToolbarAction = EditorLayout::EToolbarAction::Play;
    m_hasPendingProjectActionRequest = false;
    m_pendingProjectActionRequest = {};
    m_hasPendingBuildActionRequest = false;
    m_pendingBuildActionRequest = {};
    m_hasPendingAssetSelection = false;
    m_pendingAssetSelectionDoubleClick = false;
    m_pendingAssetSelectionKey.clear();
    m_hasPendingAssetPlaceRequest = false;
    m_pendingAssetPlaceKey.clear();
    m_hasPendingAssetDropRequest = false;
    m_pendingAssetDropRequest = {};
    m_hasPendingAssetSaveRequest = false;
    m_pendingAssetSaveKey.clear();
    m_hasPendingAssetDeleteRequest = false;
    m_pendingAssetDeleteKey.clear();
    m_hasPendingAssetRenameRequest = false;
    m_pendingAssetRenameKey.clear();
    m_pendingAssetRenameValue.clear();
    m_hasPendingAssetRefreshRequest = false;
    m_hasPendingAssetCreateRequest = false;
    m_pendingAssetCreateRequest = {};
    m_hasPendingAssetImportRequest = false;
    m_pendingAssetImportRequest = {};
    m_hasPendingAssetInspectorSaveRequest = false;
    m_hasPendingAssetInspectorReimportRequest = false;
    m_hasPendingAssetInspectorCloseRequest = false;
    m_hasPendingConduitVariableSelectionRequest = false;
    m_pendingConduitVariableSelection = {};
    m_hasPendingConduitVariableCreateRequest = false;
    m_pendingConduitVariableCreateName.clear();
    m_pendingConduitVariableCreateType = {};
    m_hasPendingConduitVariableRemoveRequest = false;
    m_hasPendingConduitVariableRenameRequest = false;
    m_pendingConduitVariableRenameValue.clear();
    m_hasPendingConduitVariableTypeRequest = false;
    m_pendingConduitVariableType = {};
    m_hasPendingConduitGraphSelfTypeRequest = false;
    m_pendingConduitGraphSelfType = {};
    m_hasPendingConduitVariableDefaultBoolRequest = false;
    m_pendingConduitVariableDefaultBool = false;
    m_hasPendingConduitVariableDefaultTextRequest = false;
    m_pendingConduitVariableDefaultText.clear();
    m_hasPendingConduitVariableDefaultEnumRequest = false;
    m_pendingConduitVariableDefaultEnum.clear();
    m_hasPendingConduitVariableClearDefaultRequest = false;
    m_hasPendingConduitVariableCommitDefaultRequest = false;
    m_hasPendingConduitVariableResetDefaultRequest = false;
    m_hasPendingConduitNodeSelectionRequest = false;
    m_pendingConduitNodeSelection = {};
    m_hasPendingConduitNodeCreateRequest = false;
    m_pendingConduitNodeCreateStableId.clear();
    m_hasPendingConduitNodeRemoveRequest = false;
    m_hasPendingConduitNodeDefaultBoolRequest = false;
    m_pendingConduitNodeDefaultPinKey.clear();
    m_pendingConduitNodeDefaultBool = false;
    m_hasPendingConduitNodeDefaultTextRequest = false;
    m_pendingConduitNodeDefaultTextPinKey.clear();
    m_pendingConduitNodeDefaultText.clear();
    m_hasPendingConduitNodeDefaultEnumRequest = false;
    m_pendingConduitNodeDefaultEnumPinKey.clear();
    m_pendingConduitNodeDefaultEnum.clear();
    m_hasPendingConduitNodeDefaultClearRequest = false;
    m_pendingConduitNodeDefaultClearPinKey.clear();
    m_hasPendingConduitNodeMoveRequest = false;
    m_pendingConduitNodeMoveId = {};
    m_pendingConduitNodeMoveX = 0.0f;
    m_pendingConduitNodeMoveY = 0.0f;
    m_hasPendingConduitSpawnMenuOpenRequest = false;
    m_pendingConduitSpawnMenuOpenRequest = {};
    m_hasPendingConduitSpawnMenuSelectionRequest = false;
    m_pendingConduitSpawnMenuSelectionRequest = {};
    m_pendingConduitSpawnSelectionEntry = {};
    m_hasPendingConduitPinConnectRequest = false;
    m_pendingConduitConnectSourceNode = {};
    m_pendingConduitConnectSourcePin.clear();
    m_pendingConduitConnectTargetNode = {};
    m_pendingConduitConnectTargetPin.clear();
    m_hasPendingConduitCompileRequest = false;
    m_hasPendingConduitNodePrimaryTextRequest = false;
    m_pendingConduitNodePrimaryText.clear();
    m_hasPendingConduitNodeSecondaryTextRequest = false;
    m_pendingConduitNodeSecondaryText.clear();
    m_hasPendingConduitViewportRequest = false;
    m_pendingConduitViewportPanX = 0.0f;
    m_pendingConduitViewportPanY = 0.0f;
    m_pendingConduitViewportZoom = 1.0f;
    m_hasPendingConduitClassNameRequest = false;
    m_pendingConduitClassName.clear();
    m_hasPendingConduitClassHostTypeRequest = false;
    m_pendingConduitClassHostType = {};
    m_hasPendingConduitClassGraphRequest = false;
    m_pendingConduitClassGraph.clear();
    m_hasPendingConduitNodeMoveRequest = false;
    m_pendingConduitNodeMoveId = {};
    m_pendingConduitNodeMoveX = 0.0f;
    m_pendingConduitNodeMoveY = 0.0f;
    m_hasPendingConduitNodePrimaryTextRequest = false;
    m_pendingConduitNodePrimaryText.clear();
    m_hasPendingConduitNodeSecondaryTextRequest = false;
    m_pendingConduitNodeSecondaryText.clear();
    m_hasPendingConduitViewportRequest = false;
    m_pendingConduitViewportPanX = 0.0f;
    m_pendingConduitViewportPanY = 0.0f;
    m_pendingConduitViewportZoom = 1.0f;
    m_hasPendingConduitClassNameRequest = false;
    m_pendingConduitClassName.clear();
    m_hasPendingConduitClassHostTypeRequest = false;
    m_pendingConduitClassHostType = {};
    m_hasPendingConduitClassGraphRequest = false;
    m_pendingConduitClassGraph.clear();
    m_layoutRebuildRequested = false;
    m_assetListSignature = 0;
    m_assetDetailsSignature = 0;
    m_buildPanelStateSignature = 0;
    m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
    m_assetInspectorIconRevision = std::numeric_limits<std::uint64_t>::max();
    m_conduitWorkspaceRevision = std::numeric_limits<std::uint64_t>::max();
    m_conduitCanvasRevision = std::numeric_limits<std::uint64_t>::max();
    m_buildPanelHistoryDirty = true;
    m_buildPanelHistoryProjectFilePath.clear();
    m_buildPanelComparisonSummary.clear();
    m_cachedBuildHistory.clear();

    const Result BuildResult = m_layout.Build(Context.Runtime(),
                                              ThemeService->Theme(),
                                              SceneService->ActiveCameraHandle(),
                                              &SelectionService->Model());
    if (!BuildResult)
    {
        return BuildResult;
    }

    m_layout.SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)>::Bind([this](const NodeHandle& Handle) {
        m_pendingSelectionRequest = Handle;
        m_hasPendingSelectionRequest = true;
    }));
    m_layout.SetHierarchyActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::HierarchyActionRequest&)>::Bind(
            [this](const EditorLayout::HierarchyActionRequest& Request) {
                m_pendingHierarchyActionRequest = Request;
                m_hasPendingHierarchyActionRequest = true;
            }));
    m_layout.SetToolbarActionHandler(SnAPI::UI::TDelegate<void(EditorLayout::EToolbarAction)>::Bind(
        [this](const EditorLayout::EToolbarAction Action) {
            m_pendingToolbarAction = Action;
            m_hasPendingToolbarAction = true;
        }));
    m_layout.SetProjectActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::ProjectActionRequest&)>::Bind(
            [this](const EditorLayout::ProjectActionRequest& Request) {
                m_pendingProjectActionRequest = Request;
                m_hasPendingProjectActionRequest = true;
                // Prevent same-frame required-project logic from reopening the chooser while a request is queued.
                m_layout.SetProjectSelectionRequired(false);
            }));
    m_layout.SetPluginActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::PluginActionRequest&)>::Bind(
            [this](const EditorLayout::PluginActionRequest& Request) {
                m_pendingPluginActionRequest = Request;
                m_hasPendingPluginActionRequest = true;
            }));
    m_layout.SetModuleActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::ModuleActionRequest&)>::Bind(
            [this](const EditorLayout::ModuleActionRequest& Request) {
                m_pendingModuleActionRequest = Request;
                m_hasPendingModuleActionRequest = true;
            }));
    m_layout.SetBuildActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::BuildActionRequest&)>::Bind(
            [this](const EditorLayout::BuildActionRequest& Request) {
                m_pendingBuildActionRequest = Request;
                m_hasPendingBuildActionRequest = true;
            }));
    m_layout.SetContentAssetSelectionHandler(
        SnAPI::UI::TDelegate<void(const std::string&, bool)>::Bind([this](const std::string& AssetKey, const bool IsDoubleClick) {
            m_pendingAssetSelectionKey = AssetKey;
            m_pendingAssetSelectionDoubleClick = IsDoubleClick;
            m_hasPendingAssetSelection = true;
        }));
    m_layout.SetContentAssetPlaceHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& AssetKey) {
        m_pendingAssetPlaceKey = AssetKey;
        m_hasPendingAssetPlaceRequest = true;
    }));
    m_layout.SetContentAssetDropHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::ContentAssetDropRequest&)>::Bind(
            [this](const EditorLayout::ContentAssetDropRequest& Request) {
                m_pendingAssetDropRequest = Request;
                m_hasPendingAssetDropRequest = true;
            }));
    m_layout.SetContentAssetSaveHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& AssetKey) {
        m_pendingAssetSaveKey = AssetKey;
        m_hasPendingAssetSaveRequest = true;
    }));
    m_layout.SetContentAssetDeleteHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& AssetKey) {
        m_pendingAssetDeleteKey = AssetKey;
        m_hasPendingAssetDeleteRequest = true;
    }));
    m_layout.SetContentAssetRenameHandler(
        SnAPI::UI::TDelegate<void(const std::string&, const std::string&)>::Bind(
            [this](const std::string& AssetKey, const std::string& NewName) {
                m_pendingAssetRenameKey = AssetKey;
                m_pendingAssetRenameValue = NewName;
                m_hasPendingAssetRenameRequest = true;
            }));
    m_layout.SetContentAssetRefreshHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingAssetRefreshRequest = true;
    }));
    m_layout.SetContentAssetCreateHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::ContentAssetCreateRequest&)>::Bind(
            [this](const EditorLayout::ContentAssetCreateRequest& Request) {
                m_pendingAssetCreateRequest = Request;
                m_hasPendingAssetCreateRequest = true;
            }));
    m_layout.SetContentAssetImportHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::ContentAssetImportRequest&)>::Bind(
            [this](const EditorLayout::ContentAssetImportRequest& Request) {
                m_pendingAssetImportRequest = Request;
                m_hasPendingAssetImportRequest = true;
            }));
    m_layout.SetContentAssetInspectorSaveHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingAssetInspectorSaveRequest = true;
    }));
    m_layout.SetContentAssetInspectorReimportHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingAssetInspectorReimportRequest = true;
    }));
    m_layout.SetContentAssetInspectorCloseHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingAssetInspectorCloseRequest = true;
    }));
    m_layout.SetContentAssetInspectorRuntimeMutatedHandler(
        SnAPI::UI::TDelegate<void(const TypeId&, void*)>::Bind(
            [AssetService](const TypeId& RootType, void* RootInstance) {
                if (AssetService)
                {
                    AssetService->NotifyActiveAssetEditorRuntimeMutated(RootType, RootInstance);
                }
            }));
    m_layout.SetContentAssetInspectorImportMutatedHandler(
        SnAPI::UI::TDelegate<void(const TypeId&, void*)>::Bind(
            [AssetService](const TypeId& RootType, void* RootInstance) {
                if (AssetService)
                {
                    AssetService->NotifyActiveAssetEditorImportSettingsMutated(RootType, RootInstance);
                }
            }));
    m_layout.SetContentAssetInspectorNodeSelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)>::Bind(
        [this](const NodeHandle& Handle) {
            m_pendingAssetInspectorNodeSelection = Handle;
            m_hasPendingAssetInspectorNodeSelectionRequest = true;
        }));
    m_layout.SetContentAssetInspectorHierarchyActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::HierarchyActionRequest&)>::Bind(
            [this](const EditorLayout::HierarchyActionRequest& Request) {
                m_pendingAssetInspectorHierarchyActionRequest = Request;
                m_hasPendingAssetInspectorHierarchyActionRequest = true;
            }));
    m_layout.SetConduitVariableSelectionHandler(SnAPI::UI::TDelegate<void(const Uuid&)>::Bind([this](const Uuid& VariableId) {
        m_pendingConduitVariableSelection = VariableId;
        m_hasPendingConduitVariableSelectionRequest = true;
    }));
    m_layout.SetConduitVariableCreateHandler(
        SnAPI::UI::TDelegate<void(const std::string&, const TypeId&)>::Bind(
            [this](const std::string& Name, const TypeId& Type) {
                m_pendingConduitVariableCreateName = Name;
                m_pendingConduitVariableCreateType = Type;
                m_hasPendingConduitVariableCreateRequest = true;
            }));
    m_layout.SetConduitVariableRemoveHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitVariableRemoveRequest = true;
    }));
    m_layout.SetConduitVariableRenameHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Name) {
        m_pendingConduitVariableRenameValue = Name;
        m_hasPendingConduitVariableRenameRequest = true;
    }));
    m_layout.SetConduitVariableTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)>::Bind([this](const TypeId& Type) {
        m_pendingConduitVariableType = Type;
        m_hasPendingConduitVariableTypeRequest = true;
    }));
    m_layout.SetConduitGraphSelfTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)>::Bind([this](const TypeId& Type) {
        m_pendingConduitGraphSelfType = Type;
        m_hasPendingConduitGraphSelfTypeRequest = true;
    }));
    m_layout.SetConduitVariableDefaultBoolHandler(SnAPI::UI::TDelegate<void(bool)>::Bind([this](const bool Value) {
        m_pendingConduitVariableDefaultBool = Value;
        m_hasPendingConduitVariableDefaultBoolRequest = true;
    }));
    m_layout.SetConduitVariableDefaultTextHandler(
        SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_pendingConduitVariableDefaultText = Value;
            m_hasPendingConduitVariableDefaultTextRequest = true;
        }));
    m_layout.SetConduitVariableDefaultEnumHandler(
        SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_pendingConduitVariableDefaultEnum = Value;
            m_hasPendingConduitVariableDefaultEnumRequest = true;
        }));
    m_layout.SetConduitVariableClearDefaultHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitVariableClearDefaultRequest = true;
    }));
    m_layout.SetConduitVariableCommitDefaultHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitVariableCommitDefaultRequest = true;
    }));
    m_layout.SetConduitVariableResetDefaultHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitVariableResetDefaultRequest = true;
    }));
    m_layout.SetConduitNodeSelectionHandler(SnAPI::UI::TDelegate<void(const Uuid&)>::Bind([this](const Uuid& NodeId) {
        m_pendingConduitNodeSelection = NodeId;
        m_hasPendingConduitNodeSelectionRequest = true;
    }));
    m_layout.SetConduitNodeCreateHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& StableId) {
        m_pendingConduitNodeCreateStableId = StableId;
        m_hasPendingConduitNodeCreateRequest = true;
    }));
    m_layout.SetConduitNodeRemoveHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitNodeRemoveRequest = true;
    }));
    m_layout.SetConduitNodeDefaultBoolHandler(
        SnAPI::UI::TDelegate<void(const std::string&, bool)>::Bind([this](const std::string& PinKey, const bool Value) {
            m_pendingConduitNodeDefaultPinKey = PinKey;
            m_pendingConduitNodeDefaultBool = Value;
            m_hasPendingConduitNodeDefaultBoolRequest = true;
        }));
    m_layout.SetConduitNodeDefaultTextHandler(
        SnAPI::UI::TDelegate<void(const std::string&, const std::string&)>::Bind(
            [this](const std::string& PinKey, const std::string& Value) {
                m_pendingConduitNodeDefaultTextPinKey = PinKey;
                m_pendingConduitNodeDefaultText = Value;
                m_hasPendingConduitNodeDefaultTextRequest = true;
            }));
    m_layout.SetConduitNodeDefaultEnumHandler(
        SnAPI::UI::TDelegate<void(const std::string&, const std::string&)>::Bind(
            [this](const std::string& PinKey, const std::string& Value) {
                m_pendingConduitNodeDefaultEnumPinKey = PinKey;
                m_pendingConduitNodeDefaultEnum = Value;
                m_hasPendingConduitNodeDefaultEnumRequest = true;
            }));
    m_layout.SetConduitNodeDefaultClearHandler(
        SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& PinKey) {
            m_pendingConduitNodeDefaultClearPinKey = PinKey;
            m_hasPendingConduitNodeDefaultClearRequest = true;
        }));
    m_layout.SetConduitNodeMoveHandler(SnAPI::UI::TDelegate<void(const Uuid&, float, float)>::Bind(
        [this](const Uuid& NodeId, const float X, const float Y) {
            m_pendingConduitNodeMoveId = NodeId;
            m_pendingConduitNodeMoveX = X;
            m_pendingConduitNodeMoveY = Y;
            m_hasPendingConduitNodeMoveRequest = true;
        }));
    m_layout.SetConduitSpawnMenuRequestHandler(
        SnAPI::UI::TDelegate<void(const Conduit::Editor::GraphSpawnMenuRequest&)>::Bind(
            [this](const Conduit::Editor::GraphSpawnMenuRequest& Request) {
                m_pendingConduitSpawnMenuOpenRequest = Request;
                m_hasPendingConduitSpawnMenuOpenRequest = true;
            }));
    m_layout.SetConduitSpawnMenuSelectionHandler(
        SnAPI::UI::TDelegate<void(const Conduit::Editor::GraphSpawnMenuRequest&,
                                  const Conduit::Editor::SpawnMenuEntryView&)>::Bind(
            [this](const Conduit::Editor::GraphSpawnMenuRequest& Request,
                   const Conduit::Editor::SpawnMenuEntryView& Entry) {
                m_pendingConduitSpawnMenuSelectionRequest = Request;
                m_pendingConduitSpawnSelectionEntry = Entry;
                m_hasPendingConduitSpawnMenuSelectionRequest = true;
            }));
    m_layout.SetConduitPinConnectedHandler(
        SnAPI::UI::TDelegate<void(const Uuid&, const std::string&, const Uuid&, const std::string&)>::Bind(
            [this](const Uuid& SourceNodeId,
                   const std::string& SourcePin,
                   const Uuid& TargetNodeId,
                   const std::string& TargetPin) {
                m_pendingConduitConnectSourceNode = SourceNodeId;
                m_pendingConduitConnectSourcePin = SourcePin;
                m_pendingConduitConnectTargetNode = TargetNodeId;
                m_pendingConduitConnectTargetPin = TargetPin;
                m_hasPendingConduitPinConnectRequest = true;
            }));
    m_layout.SetConduitCompileHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitCompileRequest = true;
    }));
    m_layout.SetConduitNodePrimaryTextHandler(
        SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_pendingConduitNodePrimaryText = Value;
            m_hasPendingConduitNodePrimaryTextRequest = true;
        }));
    m_layout.SetConduitNodeSecondaryTextHandler(
        SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_pendingConduitNodeSecondaryText = Value;
            m_hasPendingConduitNodeSecondaryTextRequest = true;
        }));
    m_layout.SetConduitViewportHandler(SnAPI::UI::TDelegate<void(float, float, float)>::Bind(
        [this](const float PanX, const float PanY, const float Zoom) {
            m_pendingConduitViewportPanX = PanX;
            m_pendingConduitViewportPanY = PanY;
            m_pendingConduitViewportZoom = Zoom;
            m_hasPendingConduitViewportRequest = true;
        }));
    m_layout.SetConduitClassNameHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Name) {
        m_pendingConduitClassName = Name;
        m_hasPendingConduitClassNameRequest = true;
    }));
    m_layout.SetConduitClassHostTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)>::Bind([this](const TypeId& Type) {
        m_pendingConduitClassHostType = Type;
        m_hasPendingConduitClassHostTypeRequest = true;
    }));
    m_layout.SetConduitClassGraphHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& AssetKey) {
        m_pendingConduitClassGraph = AssetKey;
        m_hasPendingConduitClassGraphRequest = true;
    }));

    m_layout.SetProjectSelectionRequired(!AssetService->CurrentProject().IsLoaded && !m_hasPendingProjectActionRequest);
    ApplyAssetBrowserState(Context);
    return Ok();
}

void EditorLayoutService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* PieService = Context.GetService<EditorPieService>();
    auto* CommandService = Context.GetService<EditorCommandService>();
    auto* AssetService = Context.GetService<EditorAssetService>();
    auto* BuildService = Context.GetService<EditorBuildService>();
    auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
    if (!SceneService || !SelectionService || !PieService || !AssetService || !BuildService || !ConduitService)
    {
        return;
    }

    if (m_layoutRebuildRequested)
    {
        RebuildLayout(Context);
    }

    if (m_hasPendingAssetRefreshRequest)
    {
        m_hasPendingAssetRefreshRequest = false;
        const Result RefreshResult = AssetService->RefreshDiscovery();
        if (!RefreshResult)
        {
            // Keep rendering and expose error through status text.
        }
    }

    if (m_hasPendingProjectActionRequest)
    {
        m_hasPendingProjectActionRequest = false;
        const EditorLayout::ProjectActionRequest Request = m_pendingProjectActionRequest;
        m_pendingProjectActionRequest = {};

        Result ProjectResult = Ok();
        if (Request.Action == EditorLayout::EProjectAction::CreateNew)
        {
            ProjectResult = AssetService->CreateProject(Context, Request.CreateRequest, true);
        }
        else if (Request.Action == EditorLayout::EProjectAction::OpenExisting)
        {
            ProjectResult = AssetService->LoadProject(Context, Request.ProjectFilePath);
        }
        else if (Request.Action == EditorLayout::EProjectAction::SaveSettings)
        {
            ProjectResult = AssetService->SaveProjectSettings(
                Context,
                Request.ProjectName,
                Request.StartupLevelAsset,
                Request.DefaultRenderSettingsAssetId);
        }

        ReportEditorExpectedFailure("Project action", ProjectResult);

        if (ProjectResult &&
            (Request.Action == EditorLayout::EProjectAction::CreateNew ||
             Request.Action == EditorLayout::EProjectAction::OpenExisting))
        {
            m_layout.RememberRecentProject(Request);
            (void)SceneService->EnsureEditorCamera(Context);
            SelectionService->Model().Clear();
            if (CommandService)
            {
                CommandService->ClearHistory();
            }
            m_buildPanelHistoryDirty = true;
            m_buildPanelHistoryProjectFilePath.clear();
            m_buildPanelComparisonSummary.clear();
            m_cachedBuildHistory.clear();
            m_buildPanelStateSignature = 0;
            QueueLayoutRebuild();
        }
    }

    if (m_hasPendingPluginActionRequest)
    {
        m_hasPendingPluginActionRequest = false;
        const EditorLayout::PluginActionRequest Request = m_pendingPluginActionRequest;
        m_pendingPluginActionRequest = {};

        Result PluginResult = Ok();
        if (Request.Action == EditorLayout::EPluginAction::CreateNew)
        {
            PluginResult = AssetService->CreatePlugin(Context, Request.CreateRequest);
        }
        else
        {
            PluginResult = std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported editor plugin action"));
        }

        ReportEditorExpectedFailure("Plugin action", PluginResult);
    }

    if (m_hasPendingModuleActionRequest)
    {
        m_hasPendingModuleActionRequest = false;
        const EditorLayout::ModuleActionRequest Request = m_pendingModuleActionRequest;
        m_pendingModuleActionRequest = {};

        Result ModuleResult = Ok();
        if (Request.Action == EditorLayout::EModuleAction::CreateProjectModule)
        {
            ModuleResult = AssetService->CreateProjectModule(Context, Request.ProjectRequest);
        }
        else if (Request.Action == EditorLayout::EModuleAction::CreatePluginModule)
        {
            ModuleResult = AssetService->CreatePluginModule(Context, Request.PluginRequest);
        }
        else
        {
            ModuleResult = std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported editor module action"));
        }

        ReportEditorExpectedFailure("Module action", ModuleResult);
    }

    if (m_hasPendingBuildActionRequest)
    {
        m_hasPendingBuildActionRequest = false;
        const EditorLayout::BuildActionRequest Request = m_pendingBuildActionRequest;
        m_pendingBuildActionRequest = {};

        Result BuildActionResult = Ok();
        switch (Request.Action)
        {
        case EditorLayout::EBuildAction::PlanProject:
        {
            m_buildPanelComparisonSummary.clear();
            BuildActionResult = BuildService->QueuePlanActiveProject(Context, Request.Request);
            break;
        }
        case EditorLayout::EBuildAction::PackageProject:
        {
            m_buildPanelComparisonSummary.clear();
            if (PieService->IsSessionActive())
            {
                BuildActionResult =
                    std::unexpected(MakeError(EErrorCode::NotReady, "Cannot package the project while PIE is active"));
            }
            else
            {
                BuildExecutionOptions ExecutionOptions{};
                ExecutionOptions.CodeBuild.Enabled = true;
                ExecutionOptions.AssetCook.Enabled = true;
                ExecutionOptions.PackageOutput = Request.PackageOutput;
                BuildActionResult =
                    BuildService->QueuePackageActiveProject(Context, Request.Request, {}, ExecutionOptions);
            }
            break;
        }
        case EditorLayout::EBuildAction::RetryBuild:
        {
            m_buildPanelComparisonSummary.clear();
            BuildExecutionOptions ExecutionOptions{};
            ExecutionOptions.CodeBuild.Enabled = true;
            ExecutionOptions.AssetCook.Enabled = true;
            BuildActionResult = BuildService->QueueRetryBuild(Context, Request.SourceBuildId, {}, ExecutionOptions);
            break;
        }
        case EditorLayout::EBuildAction::RebuildAll:
        {
            m_buildPanelComparisonSummary.clear();
            BuildExecutionOptions ExecutionOptions{};
            ExecutionOptions.CodeBuild.Enabled = true;
            ExecutionOptions.AssetCook.Enabled = true;
            ExecutionOptions.ResumeSucceededNodes = false;
            BuildActionResult = BuildService->QueueRetryBuild(Context, Request.SourceBuildId, {}, ExecutionOptions);
            break;
        }
        case EditorLayout::EBuildAction::RefreshHistory:
        {
            m_buildPanelComparisonSummary.clear();
            BuildActionResult = Ok();
            break;
        }
        case EditorLayout::EBuildAction::CompareHistory:
        {
            auto Comparison = BuildService->CompareHistory(Context, Request.SourceBuildId, Request.CompareBuildId);
            ReportEditorExpectedFailure("Compare build history", Comparison);
            if (Comparison)
            {
                std::ostringstream Builder{};
                Builder << "Compared " << Comparison->LeftBuildId << " -> " << Comparison->RightBuildId
                        << ". Request hash match: " << (Comparison->SameRequestHash ? "yes" : "no")
                        << ". Status match: " << (Comparison->SameStatus ? "yes" : "no")
                        << ". Added outputs: " << Comparison->AddedOutputFiles.size()
                        << ". Removed outputs: " << Comparison->RemovedOutputFiles.size()
                        << ". Node deltas: " << Comparison->NodeDeltas.size() << ".";
                m_buildPanelComparisonSummary = Builder.str();
                BuildActionResult = Ok();
            }
            else
            {
                BuildActionResult = std::unexpected(Comparison.error());
            }
            break;
        }
        default:
            BuildActionResult = std::unexpected(MakeError(EErrorCode::InvalidArgument, "Unsupported editor build action"));
            break;
        }

        m_buildPanelHistoryDirty = true;
        ApplyBuildPanelState(Context, true);
        ReportEditorExpectedFailure("Build action", BuildActionResult);
    }

    const bool HasProjectLoaded = AssetService->CurrentProject().IsLoaded;
    const bool RequireProjectSelection = !HasProjectLoaded && !m_hasPendingProjectActionRequest;
    m_layout.SetProjectSelectionRequired(RequireProjectSelection);
    ApplyBuildPanelState(Context);
    if (!HasProjectLoaded)
    {
        SceneService->Tick(Context, 0.0f);
        ApplyAssetBrowserState(Context);
        m_layout.Sync(Context.Runtime(), SceneService->ActiveCameraHandle(), &SelectionService->Model(), DeltaSeconds);
        return;
    }

    if (m_hasPendingAssetSelection)
    {
        m_hasPendingAssetSelection = false;

        if (!m_pendingAssetSelectionKey.empty())
        {
            if (AssetService->SelectAssetByKey(m_pendingAssetSelectionKey) && m_pendingAssetSelectionDoubleClick)
            {
                auto OpenEditorResult = AssetService->OpenAssetEditorByKey(Context, m_pendingAssetSelectionKey);
                if (!OpenEditorResult)
                {
                    ReportEditorExpectedFailure("Open asset editor", OpenEditorResult);
                    auto PreviewResult = AssetService->OpenSelectedAssetPreview();
                    ReportEditorExpectedFailure("Open asset preview", PreviewResult);
                }
            }
        }

        m_pendingAssetSelectionKey.clear();
        m_pendingAssetSelectionDoubleClick = false;
    }

    if (m_hasPendingAssetPlaceRequest)
    {
        m_hasPendingAssetPlaceRequest = false;

        if (!PieService->IsSessionActive() && !m_pendingAssetPlaceKey.empty())
        {
            auto PlaceResult = AssetService->ArmPlacementByKey(m_pendingAssetPlaceKey);
            ReportEditorExpectedFailure("Arm asset placement", PlaceResult);
        }

        m_pendingAssetPlaceKey.clear();
    }

    if (m_hasPendingAssetDropRequest)
    {
        m_hasPendingAssetDropRequest = false;

        if (!PieService->IsSessionActive() && !m_pendingAssetDropRequest.AssetKey.empty())
        {
            EditorAssetService::AssetPlacementRequest Request{};
            Request.Parent = m_pendingAssetDropRequest.TargetNode;
            Request.UseScreenPoint = (m_pendingAssetDropRequest.Target == EditorLayout::EContentAssetDropTarget::Viewport);
            Request.ScreenPositionX = m_pendingAssetDropRequest.ScreenPosition.X;
            Request.ScreenPositionY = m_pendingAssetDropRequest.ScreenPosition.Y;

            auto DropResult = AssetService->InstantiateAssetByKey(Context, m_pendingAssetDropRequest.AssetKey, Request);
            ReportEditorExpectedFailure("Drop asset", DropResult);
        }

        m_pendingAssetDropRequest = {};
    }

    if (m_hasPendingAssetSaveRequest)
    {
        m_hasPendingAssetSaveRequest = false;

        if (!m_pendingAssetSaveKey.empty())
        {
            auto SaveResult = AssetService->SaveAssetByKey(Context, m_pendingAssetSaveKey);
            ReportEditorExpectedFailure("Save asset", SaveResult);
        }

        m_pendingAssetSaveKey.clear();
    }

    if (m_hasPendingAssetRenameRequest)
    {
        m_hasPendingAssetRenameRequest = false;

        if (!m_pendingAssetRenameKey.empty())
        {
            auto RenameResult = AssetService->RenameAssetByKey(m_pendingAssetRenameKey, m_pendingAssetRenameValue);
            ReportEditorExpectedFailure("Rename asset", RenameResult);
        }

        m_pendingAssetRenameKey.clear();
        m_pendingAssetRenameValue.clear();
    }

    if (m_hasPendingAssetDeleteRequest)
    {
        m_hasPendingAssetDeleteRequest = false;

        if (!m_pendingAssetDeleteKey.empty())
        {
            auto DeleteResult = AssetService->DeleteAssetByKey(m_pendingAssetDeleteKey);
            ReportEditorExpectedFailure("Delete asset", DeleteResult);
        }

        m_pendingAssetDeleteKey.clear();
    }

    if (m_hasPendingAssetCreateRequest)
    {
        m_hasPendingAssetCreateRequest = false;
        if (!PieService->IsSessionActive() && m_pendingAssetCreateRequest.Type != TypeId{})
        {
            if (TypeRegistry::Instance().IsA(m_pendingAssetCreateRequest.Type, StaticTypeId<BaseNode>()))
            {
                auto CreateResult = AssetService->CreatePrefabSourceAssetByNodeType(Context,
                                                                                   m_pendingAssetCreateRequest.Type,
                                                                                   m_pendingAssetCreateRequest.Name,
                                                                                   m_pendingAssetCreateRequest.FolderPath);
                ReportEditorExpectedFailure("Create prefab asset", CreateResult);
            }
            else
            {
                auto CreateResult = AssetService->CreateSourceAssetByType(Context,
                                                                          m_pendingAssetCreateRequest.Type,
                                                                          m_pendingAssetCreateRequest.Name,
                                                                          m_pendingAssetCreateRequest.FolderPath);
                ReportEditorExpectedFailure("Create source asset", CreateResult);
            }
        }
        m_pendingAssetCreateRequest = {};
    }

    if (m_hasPendingAssetImportRequest)
    {
        m_hasPendingAssetImportRequest = false;
        if (!PieService->IsSessionActive() && !m_pendingAssetImportRequest.SourcePath.empty())
        {
            auto ImportResult = AssetService->ImportSourceAsset(Context,
                                                                m_pendingAssetImportRequest.SourcePath,
                                                                m_pendingAssetImportRequest.FolderPath,
                                                                m_pendingAssetImportRequest.BuildOptions,
                                                                m_pendingAssetImportRequest.ImportSettings);
            ReportEditorExpectedFailure("Import source asset", ImportResult);
        }
        m_pendingAssetImportRequest = {};
    }

    if (m_hasPendingAssetInspectorSaveRequest)
    {
        m_hasPendingAssetInspectorSaveRequest = false;
        auto SaveResult = AssetService->SaveActiveAssetEditor(Context);
        ReportEditorExpectedFailure("Save active asset editor", SaveResult);
    }

    if (m_hasPendingAssetInspectorReimportRequest)
    {
        m_hasPendingAssetInspectorReimportRequest = false;
        if (!PieService->IsSessionActive())
        {
            auto ReimportResult = AssetService->ReimportActiveAsset(Context);
            ReportEditorExpectedFailure("Reimport active asset", ReimportResult);
        }
    }

    if (m_hasPendingAssetInspectorCloseRequest)
    {
        m_hasPendingAssetInspectorCloseRequest = false;
        AssetService->CloseAssetEditor(Context);
    }

    if (m_hasPendingAssetInspectorNodeSelectionRequest)
    {
        m_hasPendingAssetInspectorNodeSelectionRequest = false;
        auto SelectResult = AssetService->SelectAssetEditorNode(m_pendingAssetInspectorNodeSelection);
        ReportEditorExpectedFailure("Select asset editor node", SelectResult);
        m_pendingAssetInspectorNodeSelection = {};
    }

    if (m_hasPendingAssetInspectorHierarchyActionRequest)
    {
        m_hasPendingAssetInspectorHierarchyActionRequest = false;
        const EditorLayout::HierarchyActionRequest Request = m_pendingAssetInspectorHierarchyActionRequest;
        m_pendingAssetInspectorHierarchyActionRequest = {};

        switch (Request.Action)
        {
        case EditorLayout::EHierarchyAction::AddNodeType:
            ReportEditorExpectedFailure("Add asset editor node",
                                        AssetService->AddAssetEditorNode(Request.TargetNode, Request.Type));
            break;
        case EditorLayout::EHierarchyAction::AddComponentType:
            ReportEditorExpectedFailure("Add asset editor component",
                                        AssetService->AddAssetEditorComponent(Request.TargetNode, Request.Type));
            break;
        case EditorLayout::EHierarchyAction::RemoveComponentType:
            ReportEditorExpectedFailure("Remove asset editor component",
                                        AssetService->RemoveAssetEditorComponent(Request.TargetNode, Request.Type));
            break;
        case EditorLayout::EHierarchyAction::DeleteNode:
            ReportEditorExpectedFailure("Delete asset editor node",
                                        AssetService->DeleteAssetEditorNode(Request.TargetNode));
            break;
        default:
            break;
        }
    }

    if (m_hasPendingConduitVariableRenameRequest)
    {
        m_hasPendingConduitVariableRenameRequest = false;
        if (!m_pendingConduitVariableRenameValue.empty())
        {
            (void)ConduitService->RenameSelectedVariable(m_pendingConduitVariableRenameValue);
        }
        m_pendingConduitVariableRenameValue.clear();
    }

    if (m_hasPendingConduitVariableTypeRequest)
    {
        m_hasPendingConduitVariableTypeRequest = false;
        if (m_pendingConduitVariableType != TypeId{})
        {
            (void)ConduitService->SetSelectedVariableType(m_pendingConduitVariableType);
        }
        m_pendingConduitVariableType = {};
    }

    if (m_hasPendingConduitGraphSelfTypeRequest)
    {
        m_hasPendingConduitGraphSelfTypeRequest = false;
        (void)ConduitService->SetActiveGraphSelfType(m_pendingConduitGraphSelfType);
        m_pendingConduitGraphSelfType = {};
    }

    if (m_hasPendingConduitVariableDefaultBoolRequest)
    {
        m_hasPendingConduitVariableDefaultBoolRequest = false;
        (void)ConduitService->SetSelectedVariableDefaultBool(m_pendingConduitVariableDefaultBool);
    }

    if (m_hasPendingConduitVariableDefaultTextRequest)
    {
        m_hasPendingConduitVariableDefaultTextRequest = false;
        (void)ConduitService->SetSelectedVariableDefaultText(m_pendingConduitVariableDefaultText);
        m_pendingConduitVariableDefaultText.clear();
    }

    if (m_hasPendingConduitVariableDefaultEnumRequest)
    {
        m_hasPendingConduitVariableDefaultEnumRequest = false;
        if (!m_pendingConduitVariableDefaultEnum.empty())
        {
            (void)ConduitService->SetSelectedVariableDefaultEnum(m_pendingConduitVariableDefaultEnum);
        }
        m_pendingConduitVariableDefaultEnum.clear();
    }

    if (m_hasPendingConduitVariableClearDefaultRequest)
    {
        m_hasPendingConduitVariableClearDefaultRequest = false;
        (void)ConduitService->ClearSelectedVariableDefault();
    }

    if (m_hasPendingConduitVariableCommitDefaultRequest)
    {
        m_hasPendingConduitVariableCommitDefaultRequest = false;
        (void)ConduitService->CommitSelectedVariableComplexDefault();
    }

    if (m_hasPendingConduitVariableResetDefaultRequest)
    {
        m_hasPendingConduitVariableResetDefaultRequest = false;
        (void)ConduitService->ResetSelectedVariableDefaultEditor();
    }

    if (m_hasPendingConduitNodePrimaryTextRequest)
    {
        m_hasPendingConduitNodePrimaryTextRequest = false;
        (void)ConduitService->SetSelectedNodePrimaryText(m_pendingConduitNodePrimaryText);
        m_pendingConduitNodePrimaryText.clear();
    }

    if (m_hasPendingConduitNodeSecondaryTextRequest)
    {
        m_hasPendingConduitNodeSecondaryTextRequest = false;
        (void)ConduitService->SetSelectedNodeSecondaryText(m_pendingConduitNodeSecondaryText);
        m_pendingConduitNodeSecondaryText.clear();
    }

    if (m_hasPendingConduitViewportRequest)
    {
        m_hasPendingConduitViewportRequest = false;
        (void)ConduitService->SetViewport(
            m_pendingConduitViewportPanX,
            m_pendingConduitViewportPanY,
            m_pendingConduitViewportZoom);
    }

    if (m_hasPendingConduitClassNameRequest)
    {
        m_hasPendingConduitClassNameRequest = false;
        (void)ConduitService->RenameActiveClass(m_pendingConduitClassName);
        m_pendingConduitClassName.clear();
    }

    if (m_hasPendingConduitVariableSelectionRequest)
    {
        m_hasPendingConduitVariableSelectionRequest = false;
        (void)ConduitService->SelectVariable(m_pendingConduitVariableSelection);
        m_pendingConduitVariableSelection = {};
    }

    if (m_hasPendingConduitVariableCreateRequest)
    {
        m_hasPendingConduitVariableCreateRequest = false;
        if (!m_pendingConduitVariableCreateName.empty() && m_pendingConduitVariableCreateType != TypeId{})
        {
            (void)ConduitService->CreateVariable(m_pendingConduitVariableCreateName, m_pendingConduitVariableCreateType);
        }
        m_pendingConduitVariableCreateName.clear();
        m_pendingConduitVariableCreateType = {};
    }

    if (m_hasPendingConduitVariableRemoveRequest)
    {
        m_hasPendingConduitVariableRemoveRequest = false;
        (void)ConduitService->RemoveSelectedVariable();
    }

    if (m_hasPendingConduitNodeSelectionRequest)
    {
        m_hasPendingConduitNodeSelectionRequest = false;
        (void)ConduitService->SelectNode(m_pendingConduitNodeSelection);
        m_pendingConduitNodeSelection = {};
    }

    if (m_hasPendingConduitNodeCreateRequest)
    {
        m_hasPendingConduitNodeCreateRequest = false;
        if (!m_pendingConduitNodeCreateStableId.empty())
        {
            (void)ConduitService->SpawnNode(m_pendingConduitNodeCreateStableId);
        }
        m_pendingConduitNodeCreateStableId.clear();
    }

    if (m_hasPendingConduitNodeRemoveRequest)
    {
        m_hasPendingConduitNodeRemoveRequest = false;
        (void)ConduitService->RemoveSelectedNode();
    }

    if (m_hasPendingConduitNodeDefaultBoolRequest)
    {
        m_hasPendingConduitNodeDefaultBoolRequest = false;
        if (!m_pendingConduitNodeDefaultPinKey.empty())
        {
            (void)ConduitService->SetSelectedNodeInputDefaultBool(
                m_pendingConduitNodeDefaultPinKey,
                m_pendingConduitNodeDefaultBool);
        }
        m_pendingConduitNodeDefaultPinKey.clear();
    }

    if (m_hasPendingConduitNodeDefaultTextRequest)
    {
        m_hasPendingConduitNodeDefaultTextRequest = false;
        if (!m_pendingConduitNodeDefaultTextPinKey.empty())
        {
            (void)ConduitService->SetSelectedNodeInputDefaultText(
                m_pendingConduitNodeDefaultTextPinKey,
                m_pendingConduitNodeDefaultText);
        }
        m_pendingConduitNodeDefaultTextPinKey.clear();
        m_pendingConduitNodeDefaultText.clear();
    }

    if (m_hasPendingConduitNodeDefaultEnumRequest)
    {
        m_hasPendingConduitNodeDefaultEnumRequest = false;
        if (!m_pendingConduitNodeDefaultEnumPinKey.empty() &&
            !m_pendingConduitNodeDefaultEnum.empty())
        {
            (void)ConduitService->SetSelectedNodeInputDefaultEnum(
                m_pendingConduitNodeDefaultEnumPinKey,
                m_pendingConduitNodeDefaultEnum);
        }
        m_pendingConduitNodeDefaultEnumPinKey.clear();
        m_pendingConduitNodeDefaultEnum.clear();
    }

    if (m_hasPendingConduitNodeDefaultClearRequest)
    {
        m_hasPendingConduitNodeDefaultClearRequest = false;
        if (!m_pendingConduitNodeDefaultClearPinKey.empty())
        {
            (void)ConduitService->ClearSelectedNodeInputDefault(
                m_pendingConduitNodeDefaultClearPinKey);
        }
        m_pendingConduitNodeDefaultClearPinKey.clear();
    }

    if (m_hasPendingConduitNodeMoveRequest)
    {
        m_hasPendingConduitNodeMoveRequest = false;
        (void)ConduitService->MoveNode(m_pendingConduitNodeMoveId, m_pendingConduitNodeMoveX, m_pendingConduitNodeMoveY);
        m_pendingConduitNodeMoveId = {};
        m_pendingConduitNodeMoveX = 0.0f;
        m_pendingConduitNodeMoveY = 0.0f;
    }

    if (m_hasPendingConduitSpawnMenuOpenRequest)
    {
        m_hasPendingConduitSpawnMenuOpenRequest = false;
        m_layout.OpenConduitSpawnMenu(
            m_pendingConduitSpawnMenuOpenRequest,
            ConduitService->BuildSpawnMenuEntries(m_pendingConduitSpawnMenuOpenRequest));
        m_pendingConduitSpawnMenuOpenRequest = {};
    }

    if (m_hasPendingConduitSpawnMenuSelectionRequest)
    {
        m_hasPendingConduitSpawnMenuSelectionRequest = false;
        auto SpawnResult = ConduitService->SpawnNode(m_pendingConduitSpawnSelectionEntry.StableId,
                                                     m_pendingConduitSpawnMenuSelectionRequest.GraphX,
                                                     m_pendingConduitSpawnMenuSelectionRequest.GraphY);
        ReportEditorExpectedFailure("Spawn Conduit node", SpawnResult);
        if (SpawnResult &&
            m_pendingConduitSpawnMenuSelectionRequest.FromPinDrag &&
            !m_pendingConduitSpawnMenuSelectionRequest.SourcePin.empty() &&
            !m_pendingConduitSpawnSelectionEntry.TargetPin.empty())
        {
            const Result ConnectResult = ConduitService->ConnectPins(m_pendingConduitSpawnMenuSelectionRequest.SourceNodeId,
                                                                     m_pendingConduitSpawnMenuSelectionRequest.SourcePin,
                                                                     (*SpawnResult)->Id,
                                                                     m_pendingConduitSpawnSelectionEntry.TargetPin);
            ReportEditorExpectedFailure("Connect Conduit pins", ConnectResult);
        }
        m_pendingConduitSpawnMenuSelectionRequest = {};
        m_pendingConduitSpawnSelectionEntry = {};
    }

    if (m_hasPendingConduitPinConnectRequest)
    {
        m_hasPendingConduitPinConnectRequest = false;
        const Result ConnectResult = ConduitService->ConnectPins(m_pendingConduitConnectSourceNode,
                                                                 m_pendingConduitConnectSourcePin,
                                                                 m_pendingConduitConnectTargetNode,
                                                                 m_pendingConduitConnectTargetPin);
        ReportEditorExpectedFailure("Connect Conduit pins", ConnectResult);
        m_pendingConduitConnectSourceNode = {};
        m_pendingConduitConnectSourcePin.clear();
        m_pendingConduitConnectTargetNode = {};
        m_pendingConduitConnectTargetPin.clear();
    }

    if (m_hasPendingConduitCompileRequest)
    {
        m_hasPendingConduitCompileRequest = false;
        Result CompileActionResult = Ok();
        if (const auto* ActiveDocument = ConduitService->ActiveDocument())
        {
            auto CompileResult = ConduitService->CompileDocument(ActiveDocument->AssetKey());
            if (!CompileResult)
            {
                CompileActionResult = std::unexpected(CompileResult.error());
            }
            else if (*CompileResult != nullptr)
            {
                LogConduitCompileOutput(**CompileResult);
            }
        }
        else
        {
            CompileActionResult = std::unexpected(
                MakeError(EErrorCode::NotFound, "No active Conduit graph document is open"));
        }
        ReportEditorExpectedFailure("Compile Conduit graph", CompileActionResult);
    }

    if (m_hasPendingConduitClassHostTypeRequest)
    {
        m_hasPendingConduitClassHostTypeRequest = false;
        if (m_pendingConduitClassHostType != TypeId{})
        {
            (void)ConduitService->SetActiveClassHostType(m_pendingConduitClassHostType);
        }
        m_pendingConduitClassHostType = {};
    }

    if (m_hasPendingConduitClassGraphRequest)
    {
        m_hasPendingConduitClassGraphRequest = false;
        (void)ConduitService->SetActiveClassGraph(m_pendingConduitClassGraph);
        m_pendingConduitClassGraph.clear();
    }

    if (m_hasPendingSelectionRequest)
    {
        const NodeHandle Previous = SelectionService->Model().SelectedNode();
        const NodeHandle Next = m_pendingSelectionRequest;
        m_hasPendingSelectionRequest = false;
        m_pendingSelectionRequest = {};

        if (Previous != Next)
        {
            if (CommandService)
            {
                (void)CommandService->Execute(Context, std::make_unique<SelectNodeCommand>(Previous, Next));
            }
            else
            {
                ApplySelection(SelectionService->Model(), Next);
            }
        }
    }

    if (m_hasPendingHierarchyActionRequest)
    {
        EditorLayout::HierarchyActionRequest Request = m_pendingHierarchyActionRequest;
        m_hasPendingHierarchyActionRequest = false;
        m_pendingHierarchyActionRequest = {};
        if (!PieService->IsSessionActive())
        {
            auto ActionResult = ExecuteHierarchyAction(Context, Request);
            ReportEditorExpectedFailure("Execute hierarchy action", ActionResult);
        }
    }

    if (m_hasPendingToolbarAction)
    {
        const EditorLayout::EToolbarAction Action = m_pendingToolbarAction;
        m_hasPendingToolbarAction = false;
        bool WorldReloaded = false;
        Result ActionResult = Ok();
        switch (Action)
        {
        case EditorLayout::EToolbarAction::Play:
        {
            const auto PreviousState = PieService->State();
            ActionResult = PieService->Play(Context);
            WorldReloaded = (PreviousState == EditorPieService::EState::Stopped && ActionResult.has_value());
            break;
        }
        case EditorLayout::EToolbarAction::Pause:
            ActionResult = PieService->Pause(Context);
            break;
        case EditorLayout::EToolbarAction::Stop:
        {
            const auto PreviousState = PieService->State();
            ActionResult = PieService->Stop(Context);
            WorldReloaded = (PreviousState != EditorPieService::EState::Stopped && ActionResult.has_value());
            break;
        }
        case EditorLayout::EToolbarAction::JoinLocalPlayer2:
        {
            if (!PieService->IsSessionActive())
            {
                break;
            }

            GameplayHost* Host = Context.Runtime().Gameplay();
            if (!Host)
            {
                ActionResult = std::unexpected(MakeError(
                    EErrorCode::NotReady,
                    "Gameplay host is not available while PIE is running"));
                break;
            }

            ActionResult = Host->RequestJoinPlayer("LocalPlayer2", 1u, true);
            break;
        }
        default:
            break;
        }

        if (ActionResult && WorldReloaded)
        {
            SelectionService->Model().Clear();
            if (CommandService)
            {
                CommandService->ClearHistory();
            }
        }
    }

    SceneService->Tick(Context, 0.0f);
    AssetService->TickAssetEditorSession(DeltaSeconds);
    ApplyAssetBrowserState(Context);
    m_layout.Sync(Context.Runtime(), SceneService->ActiveCameraHandle(), &SelectionService->Model(), DeltaSeconds);
}

void EditorLayoutService::ApplyBuildPanelState(EditorServiceContext& Context, const bool ForceHistoryReload)
{
    auto* AssetService = Context.GetService<EditorAssetService>();
    auto* BuildService = Context.GetService<EditorBuildService>();
    if (!AssetService || !BuildService)
    {
        return;
    }

    EditorLayout::BuildPanelState State{};
    const auto& CurrentProject = AssetService->CurrentProject();
    State.ProjectLoaded = CurrentProject.IsLoaded;
    State.ProjectName = CurrentProject.Name;
    State.ProjectFilePath = CurrentProject.ProjectFilePath;
    State.AssetRootDirectory = CurrentProject.AssetRootDirectory;
    State.BuildInProgress = BuildService->IsBusy();
    State.StatusMessage = BuildService->StatusMessage();
    if (BuildService->ConsoleLogProjectFilePath().lexically_normal() ==
        std::filesystem::path(CurrentProject.ProjectFilePath).lexically_normal())
    {
        State.ConsoleLogRevision = BuildService->ConsoleLogRevision();
        State.ConsoleLogText = BuildService->ConsoleLogText();
    }

    if (!CurrentProject.IsLoaded || CurrentProject.ProjectFilePath.empty())
    {
        m_cachedBuildHistory.clear();
        m_buildPanelHistoryDirty = true;
        m_buildPanelHistoryProjectFilePath.clear();
        m_buildPanelComparisonSummary.clear();
        State.HistoryComparisonSummary.clear();

        const std::size_t Signature = ComputeBuildPanelStateSignature(State);
        if (Signature != m_buildPanelStateSignature)
        {
            m_layout.SetBuildPanelState(std::move(State));
            m_buildPanelStateSignature = Signature;
        }
        return;
    }

    const bool ProjectChanged = (m_buildPanelHistoryProjectFilePath != CurrentProject.ProjectFilePath);
    if (ProjectChanged)
    {
        m_cachedBuildHistory.clear();
        m_buildPanelComparisonSummary.clear();
        m_buildPanelHistoryDirty = true;
    }

    auto Profiles = BuildService->ListProfiles(Context);
    ReportEditorExpectedFailure("List build profiles", Profiles);
    if (Profiles)
    {
        State.Profiles.reserve(Profiles->size());
        for (const auto& Profile : *Profiles)
        {
            EditorLayout::BuildProfileEntry Entry{};
            Entry.Name = Profile.Name;
            Entry.Label = Profile.Label;
            Entry.Summary = Profile.Summary;
            Entry.Platform = Profile.Platform;
            Entry.Configuration = Profile.Configuration;
            Entry.ExecutionEnvironment = Profile.ExecutionEnvironment;
            Entry.SelectedLevels = Profile.SelectedLevels;
            Entry.ExplicitAssets = Profile.ExplicitAssets;
            Entry.IncludeFolders = Profile.IncludeFolders;
            Entry.ExcludeFolders = Profile.ExcludeFolders;
            Entry.IncludeAssetLabels = Profile.IncludeAssetLabels;
            Entry.ExcludeAssetLabels = Profile.ExcludeAssetLabels;
            Entry.IncludeAssetKinds = Profile.IncludeAssetKinds;
            Entry.ExcludeAssetKinds = Profile.ExcludeAssetKinds;
            Entry.DependencyPolicy = Profile.DependencyPolicy;
            Entry.ChunkStrategy = Profile.ChunkStrategy;
            Entry.AllowExplicitOverrideExcludes = Profile.AllowExplicitOverrideExcludes;
            Entry.ArchiveEnabled = Profile.ArchiveEnabled;
            Entry.ArchiveFormat = Profile.ArchiveFormat;
            Entry.IsDefault = Profile.IsDefault;
            Entry.IsAdHoc = Profile.IsAdHoc;
            State.Profiles.emplace_back(std::move(Entry));
        }
    }

    std::unordered_set<std::string> AddedLevels{};
    std::unordered_set<std::string> AddedAssets{};
    std::unordered_set<std::string> AddedKinds{};
    for (const EditorAssetService::DiscoveredAsset& Asset : AssetService->Assets())
    {
        const std::string AssetField = TrimCopy(BuildDiscoveredAssetField(Asset, CurrentProject.AssetRootDirectory));
        if (!AssetField.empty() && AddedAssets.insert(AssetField).second)
        {
            State.AvailableAssets.push_back(AssetField);
            if (IsLevelAssetField(AssetField) && AddedLevels.insert(AssetField).second)
            {
                State.AvailableLevels.push_back(AssetField);
            }
        }

        const std::string KindLabel = TrimCopy(Asset.TypeLabel.empty() ? Asset.Variant : Asset.TypeLabel);
        if (!KindLabel.empty() && AddedKinds.insert(KindLabel).second)
        {
            State.AvailableAssetKinds.push_back(KindLabel);
        }
    }
    std::sort(State.AvailableLevels.begin(), State.AvailableLevels.end());
    std::sort(State.AvailableAssets.begin(), State.AvailableAssets.end());
    std::sort(State.AvailableAssetKinds.begin(), State.AvailableAssetKinds.end());

    if (const auto& LastPlan = BuildService->LastPlan(); LastPlan.has_value())
    {
        State.LastPlanSummary = DescribePlanSummary(*LastPlan);
    }

    if (const auto& LastReport = BuildService->LastReport(); LastReport.has_value())
    {
        State.LastBuildId = LastReport->BuildId;
        State.LastBuildSummary = DescribeBuildSummary(*LastReport);
        State.LastBuildOutputSummary = DescribeOutputSummary(*LastReport);
    }

    if (BuildService->ConsumeHistoryRefreshRequested())
    {
        m_buildPanelHistoryDirty = true;
    }

    if (ForceHistoryReload || ProjectChanged || m_buildPanelHistoryDirty ||
        (m_layout.IsBuildModalOpen() && m_cachedBuildHistory.empty()))
    {
        auto History = BuildService->ListHistory(Context);
        ReportEditorExpectedFailure("List build history", History);
        if (History)
        {
            m_cachedBuildHistory = *History;
            m_buildPanelHistoryDirty = false;
            m_buildPanelHistoryProjectFilePath = CurrentProject.ProjectFilePath;
        }
    }

    State.HistoryEntries.reserve(m_cachedBuildHistory.size());
    for (std::size_t Index = 0; Index < m_cachedBuildHistory.size(); ++Index)
    {
        const BuildHistoryEntry& Entry = m_cachedBuildHistory[Index];
        EditorLayout::BuildHistoryEntryView View{};
        View.BuildId = Entry.BuildId;
        View.Label = Entry.BuildId + " [" + std::string(HistoryStateLabel(Entry.State));
        if (Entry.State == EBuildHistoryEntryState::Complete)
        {
            View.Label += " / " + std::string(BuildStatusLabel(Entry.Status));
        }
        View.Label += "]";
        View.Summary = DescribeHistorySummary(Entry);
        View.RequestHash = Entry.RequestHash;
        View.StartedAtUtc = Entry.StartedAtUtc;
        View.FinishedAtUtc = Entry.FinishedAtUtc;
        View.IsComplete = Entry.State == EBuildHistoryEntryState::Complete;
        View.IsLatest = (Index == 0u);
        State.HistoryEntries.emplace_back(std::move(View));
    }
    State.HistoryComparisonSummary = m_buildPanelComparisonSummary;

    const std::size_t Signature = ComputeBuildPanelStateSignature(State);
    if (Signature != m_buildPanelStateSignature)
    {
        m_layout.SetBuildPanelState(std::move(State));
        m_buildPanelStateSignature = Signature;
    }
}

void EditorLayoutService::ApplyAssetBrowserState(EditorServiceContext& Context)
{
    auto* AssetService = Context.GetService<EditorAssetService>();
    auto* BuildService = Context.GetService<EditorBuildService>();
    auto* IconService = Context.GetService<EditorAssetIconService>();
    auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
    if (!AssetService || !BuildService || !IconService || !ConduitService)
    {
        return;
    }

    {
        const auto& CurrentProject = AssetService->CurrentProject();
        EditorLayout::ProjectState ProjectState{};
        ProjectState.IsLoaded = CurrentProject.IsLoaded;
        ProjectState.Name = CurrentProject.Name;
        ProjectState.ProjectFilePath = CurrentProject.ProjectFilePath;
        ProjectState.ProjectRootDirectory = CurrentProject.ProjectRootDirectory;
        ProjectState.AssetRootDirectory = CurrentProject.AssetRootDirectory;
        ProjectState.StartupLevelAsset = CurrentProject.StartupLevelAsset;
        ProjectState.DefaultRenderSettingsAssetId = CurrentProject.DefaultRenderSettingsAssetId;
        m_layout.SetProjectState(std::move(ProjectState));
    }

    const auto& Assets = AssetService->Assets();
    const SnAPI::UI::UIContext* LayoutContext = m_layout.Context();
    IconService->Synchronize(Context, Assets, LayoutContext);

    std::size_t AssetSignature = ComputeAssetListSignature(Assets);
    std::size_t ConduitDocumentSignature = ConduitService->Documents().size() ^ (ConduitService->ClassDocuments().size() << 1);
    const auto HashCombine = [&ConduitDocumentSignature](const std::size_t Value) {
        ConduitDocumentSignature ^= Value + 0x9e3779b9 + (ConduitDocumentSignature << 6) + (ConduitDocumentSignature >> 2);
    };
    for (const auto& Document : ConduitService->Documents())
    {
        HashCombine(std::hash<std::string>{}(Document.AssetKey()));
        HashCombine(static_cast<std::size_t>(Document.IsDirty() ? 1u : 0u));
    }
    for (const auto& Document : ConduitService->ClassDocuments())
    {
        HashCombine(std::hash<std::string>{}(Document.AssetKey()));
        HashCombine(static_cast<std::size_t>(Document.IsDirty() ? 1u : 0u));
    }
    AssetSignature ^= ConduitDocumentSignature + 0x9e3779b9 + (AssetSignature << 6) + (AssetSignature >> 2);
    const std::uint64_t IconRevision = IconService->Revision();
    AssetSignature ^= std::hash<std::uint64_t>{}(IconRevision) + 0x9e3779b9 + (AssetSignature << 6) + (AssetSignature >> 2);
    if (AssetSignature != m_assetListSignature)
    {
        std::vector<EditorLayout::ContentAssetEntry> Entries{};
        Entries.reserve(Assets.size());
        for (const auto& Asset : Assets)
        {
            EditorLayout::ContentAssetEntry Entry{};
            Entry.Key = Asset.Key;
            Entry.Name = Asset.Name;
            Entry.Type = Asset.TypeLabel;
            Entry.Variant = Asset.Variant;
            Entry.IsRuntime = Asset.IsRuntime;
            Entry.IsDirty = Asset.IsDirty || ConduitService->IsDocumentDirty(Asset.Key);
            const auto IconMetadata = IconService->ResolveAssetIcon(Context, Asset, LayoutContext);
            Entry.IconSource = std::move(IconMetadata.IconSource);
            Entry.IconTextureId = IconMetadata.TextureId;
            Entry.IconWidth = IconMetadata.TextureWidth;
            Entry.IconHeight = IconMetadata.TextureHeight;
            Entries.emplace_back(std::move(Entry));
        }

        m_layout.SetContentAssets(std::move(Entries));
        m_assetListSignature = AssetSignature;
    }

    EditorLayout::ContentAssetDetails Details{};
    const Conduit::Editor::GraphDocument* SelectedConduitDocument = nullptr;
    const Conduit::Editor::ClassDocument* SelectedConduitClassDocument = nullptr;
    if (const auto* SelectedAsset = AssetService->SelectedAsset())
    {
        SelectedConduitDocument = ConduitService->FindDocument(SelectedAsset->Key);
        SelectedConduitClassDocument = ConduitService->FindClassDocument(SelectedAsset->Key);
        Details.Name = SelectedAsset->Name;
        Details.Type = SelectedAsset->TypeLabel;
        Details.Variant = SelectedAsset->Variant.empty() ? std::string("default") : SelectedAsset->Variant;
        Details.AssetId = SelectedAsset->Key;
        Details.IsRuntime = SelectedAsset->IsRuntime;
        Details.IsDirty = SelectedAsset->IsDirty ||
                          (SelectedConduitDocument && SelectedConduitDocument->IsDirty()) ||
                          (SelectedConduitClassDocument && SelectedConduitClassDocument->IsDirty());
        Details.CanPlace = CanPlaceAssetKind(SelectedAsset->AssetKind);
        Details.CanSave = SelectedAsset->CanSave && (!SelectedAsset->IsRuntime || Details.IsDirty);
    }
    else
    {
        Details.IsRuntime = false;
        Details.IsDirty = false;
        Details.CanPlace = false;
        Details.CanSave = false;
    }

    if (!BuildService->StatusMessage().empty())
    {
        Details.Status = BuildService->StatusMessage();
    }
    else if (!AssetService->StatusMessage().empty())
    {
        Details.Status = AssetService->StatusMessage();
    }
    else if (!AssetService->PreviewSummary().empty())
    {
        Details.Status = AssetService->PreviewSummary();
    }
    else if (AssetService->IsPlacementArmed())
    {
        Details.Status = "Placement armed: click inside viewport to instantiate.";
    }
    else if (SelectedConduitDocument)
    {
        if (const auto& LastCompile = SelectedConduitDocument->LastCompile(); LastCompile.has_value())
        {
            std::size_t WarningCount = 0;
            std::size_t ErrorCount = 0;
            for (const auto& Diagnostic : LastCompile->Diagnostics)
            {
                if (Diagnostic.Severity == Conduit::Editor::ECompileDiagnosticSeverity::Warning)
                {
                    ++WarningCount;
                }
                else if (Diagnostic.Severity == Conduit::Editor::ECompileDiagnosticSeverity::Error)
                {
                    ++ErrorCount;
                }
            }

            if (ErrorCount > 0)
            {
                Details.Status = "Conduit compile failed with " + std::to_string(ErrorCount) + " error(s).";
            }
            else if (WarningCount > 0)
            {
                Details.Status = "Conduit compiled with " + std::to_string(WarningCount) + " warning(s).";
            }
            else
            {
                Details.Status = "Conduit compiled successfully.";
            }
        }
        else if (SelectedConduitDocument->IsDirty())
        {
            Details.Status = "Conduit graph has unsaved edits.";
        }
        else
        {
            Details.Status = "Conduit graph ready.";
        }
    }
    else if (SelectedConduitClassDocument)
    {
        if (SelectedConduitClassDocument->IsDirty())
        {
            Details.Status = "Conduit class has unsaved edits.";
        }
        else
        {
            Details.Status = "Conduit class ready.";
        }
    }
    else
    {
        Details.Status = "Ready";
    }

    const std::size_t DetailsSignature = ComputeAssetDetailsSignature(Details);
    if (DetailsSignature != m_assetDetailsSignature)
    {
        m_layout.SetContentAssetDetails(std::move(Details));
        m_assetDetailsSignature = DetailsSignature;
    }

    const Conduit::Editor::ConduitEditorService::WorkspaceView ConduitView = ConduitService->ActiveWorkspaceView();
    if (ConduitView.Revision != m_conduitWorkspaceRevision)
    {
        EditorLayout::ConduitWorkspaceState WorkspaceState{};
        switch (ConduitView.Kind)
        {
        case Conduit::Editor::EWorkspaceDocumentKind::Graph:
            WorkspaceState.Kind = EditorLayout::ConduitWorkspaceState::EDocumentKind::Graph;
            break;
        case Conduit::Editor::EWorkspaceDocumentKind::Class:
            WorkspaceState.Kind = EditorLayout::ConduitWorkspaceState::EDocumentKind::Class;
            break;
        case Conduit::Editor::EWorkspaceDocumentKind::None:
        default:
            WorkspaceState.Kind = EditorLayout::ConduitWorkspaceState::EDocumentKind::None;
            break;
        }
        WorkspaceState.Open = ConduitView.Open;
        WorkspaceState.AssetKey = ConduitView.AssetKey;
        WorkspaceState.Title = ConduitView.Title;
        WorkspaceState.SelfTypeLabel = ConduitView.SelfTypeLabel;
        WorkspaceState.HostTypeLabel = ConduitView.HostTypeLabel;
        WorkspaceState.GraphAssetLabel = ConduitView.GraphAssetLabel;
        WorkspaceState.SlotCount = ConduitView.SlotCount;
        WorkspaceState.VariableCount = ConduitView.VariableCount;
        WorkspaceState.NodeCount = ConduitView.NodeCount;
        WorkspaceState.IsDirty = ConduitView.IsDirty;
        WorkspaceState.CompileSucceeded = ConduitView.CompileSucceeded;
        WorkspaceState.WarningCount = ConduitView.WarningCount;
        WorkspaceState.ErrorCount = ConduitView.ErrorCount;
        const auto VariableEntries = ConduitService->ActiveVariableEntries();
        WorkspaceState.VariableEntries.reserve(VariableEntries.size());
        for (const auto& Entry : VariableEntries)
        {
            WorkspaceState.VariableEntries.push_back(EditorLayout::ConduitWorkspaceState::VariableEntry{
                .Id = Entry.Id,
                .Name = Entry.Name,
                .TypeLabel = Entry.TypeLabel,
                .HasDefault = Entry.HasDefault,
                .Selected = Entry.Selected,
            });
        }
        const auto PaletteEntries = ConduitService->ActivePaletteEntries();
        WorkspaceState.PaletteEntries.reserve(PaletteEntries.size());
        for (const auto& Entry : PaletteEntries)
        {
            WorkspaceState.PaletteEntries.push_back(EditorLayout::ConduitWorkspaceState::PaletteEntry{
                .StableId = Entry.StableId,
                .DisplayName = Entry.DisplayName,
                .Category = Entry.Category,
                .Tooltip = Entry.Tooltip,
                .RequiresSpecialization = Entry.RequiresSpecialization,
            });
        }
        const auto NodeEntries = ConduitService->ActiveNodeEntries();
        WorkspaceState.NodeEntries.reserve(NodeEntries.size());
        for (const auto& Entry : NodeEntries)
        {
            WorkspaceState.NodeEntries.push_back(EditorLayout::ConduitWorkspaceState::NodeEntry{
                .Id = Entry.Id,
                .Title = Entry.Title,
                .Detail = Entry.Detail,
                .Selected = Entry.Selected,
            });
        }
        const auto CanvasView = ConduitService->ActiveCanvasView();
        WorkspaceState.CanvasPanX = CanvasView.Viewport.PanX;
        WorkspaceState.CanvasPanY = CanvasView.Viewport.PanY;
        WorkspaceState.CanvasZoom = CanvasView.Viewport.Zoom;
        WorkspaceState.CanvasNodes.reserve(CanvasView.Nodes.size());
        for (const auto& Node : CanvasView.Nodes)
        {
            std::vector<EditorLayout::ConduitWorkspaceState::CanvasNode::Pin> InputPins{};
            InputPins.reserve(Node.InputPins.size());
            for (const auto& Pin : Node.InputPins)
            {
                InputPins.push_back(EditorLayout::ConduitWorkspaceState::CanvasNode::Pin{
                    .Name = Pin.Name,
                    .TypeLabel = Pin.TypeLabel,
                    .Tooltip = Pin.Tooltip,
                    .Kind = Pin.Kind,
                    .IsInput = Pin.IsInput,
                    .IsExec = Pin.IsExec,
                });
            }

            std::vector<EditorLayout::ConduitWorkspaceState::CanvasNode::Pin> OutputPins{};
            OutputPins.reserve(Node.OutputPins.size());
            for (const auto& Pin : Node.OutputPins)
            {
                OutputPins.push_back(EditorLayout::ConduitWorkspaceState::CanvasNode::Pin{
                    .Name = Pin.Name,
                    .TypeLabel = Pin.TypeLabel,
                    .Tooltip = Pin.Tooltip,
                    .Kind = Pin.Kind,
                    .IsInput = Pin.IsInput,
                    .IsExec = Pin.IsExec,
                });
            }

            WorkspaceState.CanvasNodes.push_back(EditorLayout::ConduitWorkspaceState::CanvasNode{
                .Id = Node.Id,
                .Title = Node.Title,
                .Detail = Node.Detail,
                .Tooltip = Node.Tooltip,
                .X = Node.X,
                .Y = Node.Y,
                .Width = Node.Width,
                .IsCollapsed = Node.IsCollapsed,
                .Selected = Node.Selected,
                .InputPins = std::move(InputPins),
                .OutputPins = std::move(OutputPins),
            });
        }
        WorkspaceState.CanvasComments.reserve(CanvasView.Comments.size());
        for (const auto& Comment : CanvasView.Comments)
        {
            WorkspaceState.CanvasComments.push_back(EditorLayout::ConduitWorkspaceState::CanvasComment{
                .Id = Comment.Id,
                .Title = Comment.Title,
                .X = Comment.X,
                .Y = Comment.Y,
                .Width = Comment.Width,
                .Height = Comment.Height,
                .ColorRgba = Comment.ColorRgba,
                .Selected = Comment.Selected,
            });
        }
        WorkspaceState.CanvasWires.reserve(CanvasView.Wires.size());
        for (const auto& Wire : CanvasView.Wires)
        {
            WorkspaceState.CanvasWires.push_back(EditorLayout::ConduitWorkspaceState::CanvasWire{
                .SourceNodeId = Wire.SourceNodeId,
                .SourcePin = Wire.SourcePin,
                .TargetNodeId = Wire.TargetNodeId,
                .TargetPin = Wire.TargetPin,
                .Kind = Wire.Kind,
                .IsExec = Wire.IsExec,
            });
        }
        const auto VariableTypes = ConduitService->AvailableVariableTypes();
        WorkspaceState.VariableTypeOptions.reserve(VariableTypes.size());
        for (const auto& Entry : VariableTypes)
        {
            WorkspaceState.VariableTypeOptions.push_back(EditorLayout::ConduitWorkspaceState::VariableTypeOption{
                .Type = Entry.Type,
                .Label = Entry.Label,
            });
        }
        const auto GraphSelfTypes = ConduitService->AvailableGraphSelfTypes();
        WorkspaceState.GraphSelfTypeOptions.reserve(GraphSelfTypes.size());
        for (const auto& Entry : GraphSelfTypes)
        {
            WorkspaceState.GraphSelfTypeOptions.push_back(EditorLayout::ConduitWorkspaceState::GraphSelfTypeOption{
                .Type = Entry.Type,
                .Label = Entry.Label,
            });
        }
        if (const auto* ActiveGraphDocument = ConduitService->ActiveDocument())
        {
            WorkspaceState.SelectedSelfType = ActiveGraphDocument->Asset().SelfType;
        }
        const auto ClassHostTypes = ConduitService->AvailableClassHostTypes();
        WorkspaceState.ClassHostTypeOptions.reserve(ClassHostTypes.size());
        for (const auto& Entry : ClassHostTypes)
        {
            WorkspaceState.ClassHostTypeOptions.push_back(EditorLayout::ConduitWorkspaceState::ClassHostTypeOption{
                .Type = Entry.Type,
                .Label = Entry.Label,
            });
        }
        const auto ClassGraphs = ConduitService->AvailableClassGraphAssets();
        WorkspaceState.ClassGraphOptions.reserve(ClassGraphs.size());
        for (const auto& Entry : ClassGraphs)
        {
            WorkspaceState.ClassGraphOptions.push_back(EditorLayout::ConduitWorkspaceState::ClassGraphOption{
                .AssetKey = Entry.AssetKey,
                .Label = Entry.Label,
            });
        }
        const auto InspectorView = ConduitService->ActiveVariableInspectorView();
        WorkspaceState.SelectedVariable.HasSelection = InspectorView.HasSelection;
        WorkspaceState.SelectedVariable.VariableId = InspectorView.VariableId;
        WorkspaceState.SelectedVariable.Name = InspectorView.Name;
        WorkspaceState.SelectedVariable.Type = InspectorView.Type;
        WorkspaceState.SelectedVariable.TypeLabel = InspectorView.TypeLabel;
        WorkspaceState.SelectedVariable.HasDefault = InspectorView.HasDefault;
        WorkspaceState.SelectedVariable.BoolValue = InspectorView.BoolValue;
        WorkspaceState.SelectedVariable.TextValue = InspectorView.TextValue;
        WorkspaceState.SelectedVariable.EnumOptions = InspectorView.EnumOptions;
        WorkspaceState.SelectedVariable.SelectedEnumIndex = InspectorView.SelectedEnumIndex;
        WorkspaceState.SelectedVariable.ComplexObject = InspectorView.ComplexObject;
        WorkspaceState.SelectedVariable.ComplexType = InspectorView.ComplexType;
        switch (InspectorView.DefaultEditorKind)
        {
        case Conduit::Editor::EVariableDefaultEditorKind::Bool:
            WorkspaceState.SelectedVariable.DefaultEditorKind =
                EditorLayout::ConduitWorkspaceState::EVariableDefaultEditorKind::Bool;
            break;
        case Conduit::Editor::EVariableDefaultEditorKind::Text:
            WorkspaceState.SelectedVariable.DefaultEditorKind =
                EditorLayout::ConduitWorkspaceState::EVariableDefaultEditorKind::Text;
            break;
        case Conduit::Editor::EVariableDefaultEditorKind::Enum:
            WorkspaceState.SelectedVariable.DefaultEditorKind =
                EditorLayout::ConduitWorkspaceState::EVariableDefaultEditorKind::Enum;
            break;
        case Conduit::Editor::EVariableDefaultEditorKind::Complex:
            WorkspaceState.SelectedVariable.DefaultEditorKind =
                EditorLayout::ConduitWorkspaceState::EVariableDefaultEditorKind::Complex;
            break;
        case Conduit::Editor::EVariableDefaultEditorKind::None:
        default:
            WorkspaceState.SelectedVariable.DefaultEditorKind =
                EditorLayout::ConduitWorkspaceState::EVariableDefaultEditorKind::None;
            break;
        }
        const auto ClassInspector = ConduitService->ActiveClassInspectorView();
        WorkspaceState.SelectedClass.HasSelection = ClassInspector.HasSelection;
        WorkspaceState.SelectedClass.Name = ClassInspector.Name;
        WorkspaceState.SelectedClass.HostType = ClassInspector.HostType;
        WorkspaceState.SelectedClass.HostTypeLabel = ClassInspector.HostTypeLabel;
        WorkspaceState.SelectedClass.GraphAssetKey = ClassInspector.GraphAssetKey;
        WorkspaceState.SelectedClass.GraphAssetLabel = ClassInspector.GraphAssetLabel;
        const auto NodeInspector = ConduitService->ActiveNodeInspectorView();
        WorkspaceState.SelectedNode.HasSelection = NodeInspector.HasSelection;
        WorkspaceState.SelectedNode.NodeId = NodeInspector.NodeId;
        WorkspaceState.SelectedNode.Title = NodeInspector.Title;
        WorkspaceState.SelectedNode.Detail = NodeInspector.Detail;
        WorkspaceState.SelectedNode.CanEditPrimaryText = NodeInspector.CanEditPrimaryText;
        WorkspaceState.SelectedNode.PrimaryTextLabel = NodeInspector.PrimaryTextLabel;
        WorkspaceState.SelectedNode.PrimaryTextValue = NodeInspector.PrimaryTextValue;
        WorkspaceState.SelectedNode.CanEditSecondaryText = NodeInspector.CanEditSecondaryText;
        WorkspaceState.SelectedNode.SecondaryTextLabel = NodeInspector.SecondaryTextLabel;
        WorkspaceState.SelectedNode.SecondaryTextValue = NodeInspector.SecondaryTextValue;
        WorkspaceState.SelectedNode.InputDefaults.clear();
        WorkspaceState.SelectedNode.InputDefaults.reserve(NodeInspector.InputDefaults.size());
        for (const auto& Entry : NodeInspector.InputDefaults)
        {
            EditorLayout::ConduitWorkspaceState::NodeInspector::InputDefault DefaultEntry{};
            DefaultEntry.PinKey = Entry.PinKey;
            DefaultEntry.DisplayName = Entry.DisplayName;
            DefaultEntry.Type = Entry.Type;
            DefaultEntry.TypeLabel = Entry.TypeLabel;
            DefaultEntry.Tooltip = Entry.Tooltip;
            DefaultEntry.Connected = Entry.Connected;
            DefaultEntry.HasDefault = Entry.HasDefault;
            DefaultEntry.DefaultEditorKind =
                static_cast<EditorLayout::ConduitWorkspaceState::EVariableDefaultEditorKind>(
                    Entry.DefaultEditorKind);
            DefaultEntry.BoolValue = Entry.BoolValue;
            DefaultEntry.TextValue = Entry.TextValue;
            DefaultEntry.EnumOptions = Entry.EnumOptions;
            DefaultEntry.SelectedEnumIndex = Entry.SelectedEnumIndex;
            WorkspaceState.SelectedNode.InputDefaults.emplace_back(std::move(DefaultEntry));
        }
        WorkspaceState.Revision = ConduitView.Revision;

        if (!ConduitView.Open)
        {
            WorkspaceState.Status = "No Conduit document open.";
        }
        else if (ConduitView.Kind == Conduit::Editor::EWorkspaceDocumentKind::Class)
        {
            WorkspaceState.Status = ConduitView.IsDirty
                ? std::string("Class modified. Save when ready.")
                : std::string("Class open.");
        }
        else if (!ConduitView.HasCompile)
        {
            WorkspaceState.Status = ConduitView.IsDirty
                ? std::string("Graph modified. Compile and save when ready.")
                : std::string("Graph open. No compile cached yet.");
        }
        else if (ConduitView.ErrorCount > 0)
        {
            WorkspaceState.Status = "Compile failed with " + std::to_string(ConduitView.ErrorCount) + " error(s).";
            if (const auto* ActiveGraphDocument = ConduitService->ActiveDocument();
                ActiveGraphDocument && ActiveGraphDocument->LastCompile().has_value())
            {
                const auto& Diagnostics = ActiveGraphDocument->LastCompile()->Diagnostics;
                const auto It = std::find_if(Diagnostics.begin(), Diagnostics.end(), [](const auto& Diagnostic) {
                    return Diagnostic.Severity == Conduit::Editor::ECompileDiagnosticSeverity::Error &&
                           !Diagnostic.Message.empty();
                });
                if (It != Diagnostics.end())
                {
                    WorkspaceState.Status += " " + It->Message;
                }
            }
        }
        else if (ConduitView.WarningCount > 0)
        {
            WorkspaceState.Status = "Compiled with " + std::to_string(ConduitView.WarningCount) + " warning(s).";
            if (const auto* ActiveGraphDocument = ConduitService->ActiveDocument();
                ActiveGraphDocument && ActiveGraphDocument->LastCompile().has_value())
            {
                const auto& Diagnostics = ActiveGraphDocument->LastCompile()->Diagnostics;
                const auto It = std::find_if(Diagnostics.begin(), Diagnostics.end(), [](const auto& Diagnostic) {
                    return Diagnostic.Severity == Conduit::Editor::ECompileDiagnosticSeverity::Warning &&
                           !Diagnostic.Message.empty();
                });
                if (It != Diagnostics.end())
                {
                    WorkspaceState.Status += " " + It->Message;
                }
            }
        }
        else if (ConduitView.CompileSucceeded)
        {
            WorkspaceState.Status = "Compiled successfully.";
        }
        else
        {
            WorkspaceState.Status = "Graph open.";
        }

        m_layout.SetConduitWorkspaceState(std::move(WorkspaceState));
        m_conduitWorkspaceRevision = ConduitView.Revision;
        m_conduitCanvasRevision = ConduitView.CanvasRevision;
    }
    else if (ConduitView.Kind == Conduit::Editor::EWorkspaceDocumentKind::Graph &&
             ConduitView.CanvasRevision != m_conduitCanvasRevision)
    {
        m_layout.SetConduitCanvasView(ConduitService->ActiveCanvasView());
        m_conduitCanvasRevision = ConduitView.CanvasRevision;
    }

    const std::uint64_t InspectorRevision = AssetService->AssetEditorSessionRevision();
    if (InspectorRevision != m_assetInspectorSessionRevision ||
        IconRevision != m_assetInspectorIconRevision)
    {
        const EditorAssetService::AssetEditorSessionView SessionView = AssetService->AssetEditorSession();
        if (InspectorRevision != m_assetInspectorSessionRevision && SessionView.IsOpen && !SessionView.AssetKey.empty())
        {
            IconService->InvalidateAsset(Context, SessionView.AssetKey);
        }
        EditorLayout::ContentAssetInspectorState InspectorState{};
        InspectorState.Open = SessionView.IsOpen;
        InspectorState.AssetKey = SessionView.AssetKey;
        InspectorState.Title = SessionView.Title;
        InspectorState.TargetType = SessionView.TargetType;
        InspectorState.TargetObject = SessionView.TargetObject;
        InspectorState.ImportSettingsType = SessionView.ImportSettingsType;
        InspectorState.ImportSettingsObject = SessionView.ImportSettingsObject;
        InspectorState.SelectedNode = SessionView.SelectedNode;
        InspectorState.CanEditHierarchy = SessionView.CanEditHierarchy;
        InspectorState.HasImportSettings = SessionView.HasImportSettings;
        InspectorState.RuntimeDirty = SessionView.RuntimeDirty;
        InspectorState.ImportSettingsDirty = SessionView.ImportSettingsDirty;
        InspectorState.Nodes.reserve(SessionView.Nodes.size());
        for (const auto& Entry : SessionView.Nodes)
        {
            EditorLayout::ContentAssetInspectorState::NodeEntry NodeEntry{};
            NodeEntry.Handle = Entry.Handle;
            NodeEntry.Depth = Entry.Depth;
            NodeEntry.Label = Entry.Label;
            InspectorState.Nodes.emplace_back(std::move(NodeEntry));
        }
        InspectorState.IsDirty = SessionView.IsDirty;
        InspectorState.CanSave = SessionView.CanSave;
        InspectorState.CanReimport = SessionView.CanReimport;
        InspectorState.SessionRevision = InspectorRevision;
        if (SessionView.IsOpen)
        {
            const auto AssetIt = std::find_if(Assets.begin(), Assets.end(), [&SessionView](const EditorAssetService::DiscoveredAsset& Asset) {
                return Asset.Key == SessionView.AssetKey;
            });
            if (AssetIt != Assets.end())
            {
                const auto IconMetadata = IconService->ResolveAssetIcon(Context, *AssetIt, LayoutContext);
                InspectorState.PreviewIconSource = std::move(IconMetadata.IconSource);
                InspectorState.PreviewTextureId = IconMetadata.TextureId;
                InspectorState.PreviewWidth = IconMetadata.TextureWidth;
                InspectorState.PreviewHeight = IconMetadata.TextureHeight;
            }
        }
        if (SessionView.IsOpen && SessionView.HasTexturePreviewStats)
        {
            InspectorState.PreviewWidth = SessionView.TexturePreviewWidth;
            InspectorState.PreviewHeight = SessionView.TexturePreviewHeight;
            InspectorState.PreviewStatsPrimary =
                std::to_string(SessionView.TexturePreviewWidth) + " x " +
                std::to_string(SessionView.TexturePreviewHeight) + " | Target: " +
                (SessionView.TexturePreviewTarget.empty() ? std::string("Unknown") : SessionView.TexturePreviewTarget) +
                " | Format: " +
                (SessionView.TexturePreviewFormat.empty() ? std::string("Unknown") : SessionView.TexturePreviewFormat) +
                " | Mips: " + std::to_string(SessionView.TexturePreviewMipCount);
            InspectorState.PreviewStatsSecondary =
                "GPU Size: " + FormatBinaryByteSize(SessionView.TexturePreviewGpuSizeBytes);
        }
        if (SessionView.IsOpen)
        {
            if (SessionView.RuntimeDirty && SessionView.ImportSettingsDirty)
            {
                InspectorState.Status = "Runtime and import settings changed. Save to persist settings, then Reimport to apply import changes.";
            }
            else if (SessionView.RuntimeDirty)
            {
                InspectorState.Status = "Runtime settings changed. Click Save to persist.";
            }
            else if (SessionView.ImportSettingsDirty)
            {
                InspectorState.Status = "Import settings changed. Save to persist and Reimport to apply.";
            }
            else
            {
                InspectorState.Status = "No pending edits.";
            }
        }
        m_layout.SetContentAssetInspectorState(std::move(InspectorState));
        m_assetInspectorSessionRevision = InspectorRevision;
        m_assetInspectorIconRevision = IconRevision;
    }
}

void EditorLayoutService::RebuildLayout(EditorServiceContext& Context)
{
    auto* ThemeService = Context.GetService<EditorThemeService>();
    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* AssetService = Context.GetService<EditorAssetService>();
    if (!ThemeService || !SceneService || !SelectionService || !AssetService)
    {
        m_layoutRebuildRequested = false;
        return;
    }

    m_layout.Shutdown(&Context.Runtime());
    m_assetListSignature = 0;
    m_assetDetailsSignature = 0;
    m_buildPanelStateSignature = 0;
    m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
    m_assetInspectorIconRevision = std::numeric_limits<std::uint64_t>::max();
    m_conduitWorkspaceRevision = std::numeric_limits<std::uint64_t>::max();
    m_conduitCanvasRevision = std::numeric_limits<std::uint64_t>::max();
    m_hasPendingHierarchyActionRequest = false;
    m_pendingHierarchyActionRequest = {};
    m_hasPendingToolbarAction = false;
    m_pendingToolbarAction = EditorLayout::EToolbarAction::Play;
    m_hasPendingProjectActionRequest = false;
    m_pendingProjectActionRequest = {};
    m_hasPendingPluginActionRequest = false;
    m_pendingPluginActionRequest = {};
    m_hasPendingModuleActionRequest = false;
    m_pendingModuleActionRequest = {};
    m_hasPendingAssetCreateRequest = false;
    m_pendingAssetCreateRequest = {};
    m_hasPendingAssetImportRequest = false;
    m_pendingAssetImportRequest = {};
    m_hasPendingAssetInspectorSaveRequest = false;
    m_hasPendingAssetInspectorReimportRequest = false;
    m_hasPendingAssetInspectorCloseRequest = false;
    m_hasPendingAssetInspectorNodeSelectionRequest = false;
    m_pendingAssetInspectorNodeSelection = {};
    m_hasPendingAssetInspectorHierarchyActionRequest = false;
    m_pendingAssetInspectorHierarchyActionRequest = {};
    m_hasPendingConduitVariableSelectionRequest = false;
    m_pendingConduitVariableSelection = {};
    m_hasPendingConduitVariableCreateRequest = false;
    m_pendingConduitVariableCreateName.clear();
    m_pendingConduitVariableCreateType = {};
    m_hasPendingConduitVariableRemoveRequest = false;
    m_hasPendingConduitVariableRenameRequest = false;
    m_pendingConduitVariableRenameValue.clear();
    m_hasPendingConduitVariableTypeRequest = false;
    m_pendingConduitVariableType = {};
    m_hasPendingConduitGraphSelfTypeRequest = false;
    m_pendingConduitGraphSelfType = {};
    m_hasPendingConduitVariableDefaultBoolRequest = false;
    m_pendingConduitVariableDefaultBool = false;
    m_hasPendingConduitVariableDefaultTextRequest = false;
    m_pendingConduitVariableDefaultText.clear();
    m_hasPendingConduitVariableDefaultEnumRequest = false;
    m_pendingConduitVariableDefaultEnum.clear();
    m_hasPendingConduitVariableClearDefaultRequest = false;
    m_hasPendingConduitVariableCommitDefaultRequest = false;
    m_hasPendingConduitVariableResetDefaultRequest = false;
    m_hasPendingConduitNodeSelectionRequest = false;
    m_pendingConduitNodeSelection = {};
    m_hasPendingConduitNodeCreateRequest = false;
    m_pendingConduitNodeCreateStableId.clear();
    m_hasPendingConduitNodeRemoveRequest = false;
    m_hasPendingConduitNodeDefaultBoolRequest = false;
    m_pendingConduitNodeDefaultPinKey.clear();
    m_pendingConduitNodeDefaultBool = false;
    m_hasPendingConduitNodeDefaultTextRequest = false;
    m_pendingConduitNodeDefaultTextPinKey.clear();
    m_pendingConduitNodeDefaultText.clear();
    m_hasPendingConduitNodeDefaultEnumRequest = false;
    m_pendingConduitNodeDefaultEnumPinKey.clear();
    m_pendingConduitNodeDefaultEnum.clear();
    m_hasPendingConduitNodeDefaultClearRequest = false;
    m_pendingConduitNodeDefaultClearPinKey.clear();
    m_hasPendingConduitSpawnMenuOpenRequest = false;
    m_pendingConduitSpawnMenuOpenRequest = {};
    m_hasPendingConduitSpawnMenuSelectionRequest = false;
    m_pendingConduitSpawnMenuSelectionRequest = {};
    m_pendingConduitSpawnSelectionEntry = {};
    m_hasPendingConduitPinConnectRequest = false;
    m_pendingConduitConnectSourceNode = {};
    m_pendingConduitConnectSourcePin.clear();
    m_pendingConduitConnectTargetNode = {};
    m_pendingConduitConnectTargetPin.clear();
    m_hasPendingConduitCompileRequest = false;

    SceneService->Tick(Context, 0.0f);
    const Result BuildResult = m_layout.Build(Context.Runtime(),
                                              ThemeService->Theme(),
                                              SceneService->ActiveCameraHandle(),
                                              &SelectionService->Model());
    if (!BuildResult)
    {
        m_layoutRebuildRequested = false;
        return;
    }

    m_layout.SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)>::Bind([this](const NodeHandle& Handle) {
        m_pendingSelectionRequest = Handle;
        m_hasPendingSelectionRequest = true;
    }));
    m_layout.SetHierarchyActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::HierarchyActionRequest&)>::Bind(
            [this](const EditorLayout::HierarchyActionRequest& Request) {
                m_pendingHierarchyActionRequest = Request;
                m_hasPendingHierarchyActionRequest = true;
            }));
    m_layout.SetToolbarActionHandler(SnAPI::UI::TDelegate<void(EditorLayout::EToolbarAction)>::Bind(
        [this](const EditorLayout::EToolbarAction Action) {
            m_pendingToolbarAction = Action;
            m_hasPendingToolbarAction = true;
        }));
    m_layout.SetProjectActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::ProjectActionRequest&)>::Bind(
            [this](const EditorLayout::ProjectActionRequest& Request) {
                m_pendingProjectActionRequest = Request;
                m_hasPendingProjectActionRequest = true;
                // Prevent same-frame required-project logic from reopening the chooser while a request is queued.
                m_layout.SetProjectSelectionRequired(false);
            }));
    m_layout.SetPluginActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::PluginActionRequest&)>::Bind(
            [this](const EditorLayout::PluginActionRequest& Request) {
                m_pendingPluginActionRequest = Request;
                m_hasPendingPluginActionRequest = true;
            }));
    m_layout.SetModuleActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::ModuleActionRequest&)>::Bind(
            [this](const EditorLayout::ModuleActionRequest& Request) {
                m_pendingModuleActionRequest = Request;
                m_hasPendingModuleActionRequest = true;
            }));
    m_layout.SetBuildActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::BuildActionRequest&)>::Bind(
            [this](const EditorLayout::BuildActionRequest& Request) {
                m_pendingBuildActionRequest = Request;
                m_hasPendingBuildActionRequest = true;
            }));
    m_layout.SetContentAssetSelectionHandler(
        SnAPI::UI::TDelegate<void(const std::string&, bool)>::Bind([this](const std::string& AssetKey, const bool IsDoubleClick) {
            m_pendingAssetSelectionKey = AssetKey;
            m_pendingAssetSelectionDoubleClick = IsDoubleClick;
            m_hasPendingAssetSelection = true;
        }));
    m_layout.SetContentAssetPlaceHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& AssetKey) {
        m_pendingAssetPlaceKey = AssetKey;
        m_hasPendingAssetPlaceRequest = true;
    }));
    m_layout.SetContentAssetDropHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::ContentAssetDropRequest&)>::Bind(
            [this](const EditorLayout::ContentAssetDropRequest& Request) {
                m_pendingAssetDropRequest = Request;
                m_hasPendingAssetDropRequest = true;
            }));
    m_layout.SetContentAssetSaveHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& AssetKey) {
        m_pendingAssetSaveKey = AssetKey;
        m_hasPendingAssetSaveRequest = true;
    }));
    m_layout.SetContentAssetDeleteHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& AssetKey) {
        m_pendingAssetDeleteKey = AssetKey;
        m_hasPendingAssetDeleteRequest = true;
    }));
    m_layout.SetContentAssetRenameHandler(
        SnAPI::UI::TDelegate<void(const std::string&, const std::string&)>::Bind(
            [this](const std::string& AssetKey, const std::string& NewName) {
                m_pendingAssetRenameKey = AssetKey;
                m_pendingAssetRenameValue = NewName;
                m_hasPendingAssetRenameRequest = true;
            }));
    m_layout.SetContentAssetRefreshHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingAssetRefreshRequest = true;
    }));
    m_layout.SetContentAssetCreateHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::ContentAssetCreateRequest&)>::Bind(
            [this](const EditorLayout::ContentAssetCreateRequest& Request) {
                m_pendingAssetCreateRequest = Request;
                m_hasPendingAssetCreateRequest = true;
            }));
    m_layout.SetContentAssetImportHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::ContentAssetImportRequest&)>::Bind(
            [this](const EditorLayout::ContentAssetImportRequest& Request) {
                m_pendingAssetImportRequest = Request;
                m_hasPendingAssetImportRequest = true;
            }));
    m_layout.SetContentAssetInspectorSaveHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingAssetInspectorSaveRequest = true;
    }));
    m_layout.SetContentAssetInspectorReimportHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingAssetInspectorReimportRequest = true;
    }));
    m_layout.SetContentAssetInspectorCloseHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingAssetInspectorCloseRequest = true;
    }));
    m_layout.SetContentAssetInspectorRuntimeMutatedHandler(
        SnAPI::UI::TDelegate<void(const TypeId&, void*)>::Bind(
            [AssetService](const TypeId& RootType, void* RootInstance) {
                if (AssetService)
                {
                    AssetService->NotifyActiveAssetEditorRuntimeMutated(RootType, RootInstance);
                }
            }));
    m_layout.SetContentAssetInspectorImportMutatedHandler(
        SnAPI::UI::TDelegate<void(const TypeId&, void*)>::Bind(
            [AssetService](const TypeId& RootType, void* RootInstance) {
                if (AssetService)
                {
                    AssetService->NotifyActiveAssetEditorImportSettingsMutated(RootType, RootInstance);
                }
            }));
    m_layout.SetContentAssetInspectorNodeSelectionHandler(SnAPI::UI::TDelegate<void(const NodeHandle&)>::Bind(
        [this](const NodeHandle& Handle) {
            m_pendingAssetInspectorNodeSelection = Handle;
            m_hasPendingAssetInspectorNodeSelectionRequest = true;
        }));
    m_layout.SetContentAssetInspectorHierarchyActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::HierarchyActionRequest&)>::Bind(
            [this](const EditorLayout::HierarchyActionRequest& Request) {
                m_pendingAssetInspectorHierarchyActionRequest = Request;
                m_hasPendingAssetInspectorHierarchyActionRequest = true;
            }));
    m_layout.SetConduitVariableSelectionHandler(SnAPI::UI::TDelegate<void(const Uuid&)>::Bind([this](const Uuid& VariableId) {
        m_pendingConduitVariableSelection = VariableId;
        m_hasPendingConduitVariableSelectionRequest = true;
    }));
    m_layout.SetConduitVariableCreateHandler(
        SnAPI::UI::TDelegate<void(const std::string&, const TypeId&)>::Bind(
            [this](const std::string& Name, const TypeId& Type) {
                m_pendingConduitVariableCreateName = Name;
                m_pendingConduitVariableCreateType = Type;
                m_hasPendingConduitVariableCreateRequest = true;
            }));
    m_layout.SetConduitVariableRemoveHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitVariableRemoveRequest = true;
    }));
    m_layout.SetConduitVariableRenameHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Name) {
        m_pendingConduitVariableRenameValue = Name;
        m_hasPendingConduitVariableRenameRequest = true;
    }));
    m_layout.SetConduitVariableTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)>::Bind([this](const TypeId& Type) {
        m_pendingConduitVariableType = Type;
        m_hasPendingConduitVariableTypeRequest = true;
    }));
    m_layout.SetConduitGraphSelfTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)>::Bind([this](const TypeId& Type) {
        m_pendingConduitGraphSelfType = Type;
        m_hasPendingConduitGraphSelfTypeRequest = true;
    }));
    m_layout.SetConduitVariableDefaultBoolHandler(SnAPI::UI::TDelegate<void(bool)>::Bind([this](const bool Value) {
        m_pendingConduitVariableDefaultBool = Value;
        m_hasPendingConduitVariableDefaultBoolRequest = true;
    }));
    m_layout.SetConduitVariableDefaultTextHandler(
        SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_pendingConduitVariableDefaultText = Value;
            m_hasPendingConduitVariableDefaultTextRequest = true;
        }));
    m_layout.SetConduitVariableDefaultEnumHandler(
        SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_pendingConduitVariableDefaultEnum = Value;
            m_hasPendingConduitVariableDefaultEnumRequest = true;
        }));
    m_layout.SetConduitVariableClearDefaultHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitVariableClearDefaultRequest = true;
    }));
    m_layout.SetConduitVariableCommitDefaultHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitVariableCommitDefaultRequest = true;
    }));
    m_layout.SetConduitVariableResetDefaultHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitVariableResetDefaultRequest = true;
    }));
    m_layout.SetConduitNodeSelectionHandler(SnAPI::UI::TDelegate<void(const Uuid&)>::Bind([this](const Uuid& NodeId) {
        m_pendingConduitNodeSelection = NodeId;
        m_hasPendingConduitNodeSelectionRequest = true;
    }));
    m_layout.SetConduitNodeCreateHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& StableId) {
        m_pendingConduitNodeCreateStableId = StableId;
        m_hasPendingConduitNodeCreateRequest = true;
    }));
    m_layout.SetConduitNodeRemoveHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitNodeRemoveRequest = true;
    }));
    m_layout.SetConduitNodeDefaultBoolHandler(
        SnAPI::UI::TDelegate<void(const std::string&, bool)>::Bind([this](const std::string& PinKey, const bool Value) {
            m_pendingConduitNodeDefaultPinKey = PinKey;
            m_pendingConduitNodeDefaultBool = Value;
            m_hasPendingConduitNodeDefaultBoolRequest = true;
        }));
    m_layout.SetConduitNodeDefaultTextHandler(
        SnAPI::UI::TDelegate<void(const std::string&, const std::string&)>::Bind(
            [this](const std::string& PinKey, const std::string& Value) {
                m_pendingConduitNodeDefaultTextPinKey = PinKey;
                m_pendingConduitNodeDefaultText = Value;
                m_hasPendingConduitNodeDefaultTextRequest = true;
            }));
    m_layout.SetConduitNodeDefaultEnumHandler(
        SnAPI::UI::TDelegate<void(const std::string&, const std::string&)>::Bind(
            [this](const std::string& PinKey, const std::string& Value) {
                m_pendingConduitNodeDefaultEnumPinKey = PinKey;
                m_pendingConduitNodeDefaultEnum = Value;
                m_hasPendingConduitNodeDefaultEnumRequest = true;
            }));
    m_layout.SetConduitNodeDefaultClearHandler(
        SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& PinKey) {
            m_pendingConduitNodeDefaultClearPinKey = PinKey;
            m_hasPendingConduitNodeDefaultClearRequest = true;
        }));
    m_layout.SetConduitNodeMoveHandler(SnAPI::UI::TDelegate<void(const Uuid&, float, float)>::Bind(
        [this](const Uuid& NodeId, const float X, const float Y) {
            m_pendingConduitNodeMoveId = NodeId;
            m_pendingConduitNodeMoveX = X;
            m_pendingConduitNodeMoveY = Y;
            m_hasPendingConduitNodeMoveRequest = true;
        }));
    m_layout.SetConduitSpawnMenuRequestHandler(
        SnAPI::UI::TDelegate<void(const Conduit::Editor::GraphSpawnMenuRequest&)>::Bind(
            [this](const Conduit::Editor::GraphSpawnMenuRequest& Request) {
                m_pendingConduitSpawnMenuOpenRequest = Request;
                m_hasPendingConduitSpawnMenuOpenRequest = true;
            }));
    m_layout.SetConduitSpawnMenuSelectionHandler(
        SnAPI::UI::TDelegate<void(const Conduit::Editor::GraphSpawnMenuRequest&,
                                  const Conduit::Editor::SpawnMenuEntryView&)>::Bind(
            [this](const Conduit::Editor::GraphSpawnMenuRequest& Request,
                   const Conduit::Editor::SpawnMenuEntryView& Entry) {
                m_pendingConduitSpawnMenuSelectionRequest = Request;
                m_pendingConduitSpawnSelectionEntry = Entry;
                m_hasPendingConduitSpawnMenuSelectionRequest = true;
            }));
    m_layout.SetConduitPinConnectedHandler(
        SnAPI::UI::TDelegate<void(const Uuid&, const std::string&, const Uuid&, const std::string&)>::Bind(
            [this](const Uuid& SourceNodeId,
                   const std::string& SourcePin,
                   const Uuid& TargetNodeId,
                   const std::string& TargetPin) {
                m_pendingConduitConnectSourceNode = SourceNodeId;
                m_pendingConduitConnectSourcePin = SourcePin;
                m_pendingConduitConnectTargetNode = TargetNodeId;
                m_pendingConduitConnectTargetPin = TargetPin;
                m_hasPendingConduitPinConnectRequest = true;
            }));
    m_layout.SetConduitCompileHandler(SnAPI::UI::TDelegate<void()>::Bind([this]() {
        m_hasPendingConduitCompileRequest = true;
    }));
    m_layout.SetConduitNodePrimaryTextHandler(
        SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_pendingConduitNodePrimaryText = Value;
            m_hasPendingConduitNodePrimaryTextRequest = true;
        }));
    m_layout.SetConduitNodeSecondaryTextHandler(
        SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Value) {
            m_pendingConduitNodeSecondaryText = Value;
            m_hasPendingConduitNodeSecondaryTextRequest = true;
        }));
    m_layout.SetConduitViewportHandler(SnAPI::UI::TDelegate<void(float, float, float)>::Bind(
        [this](const float PanX, const float PanY, const float Zoom) {
            m_pendingConduitViewportPanX = PanX;
            m_pendingConduitViewportPanY = PanY;
            m_pendingConduitViewportZoom = Zoom;
            m_hasPendingConduitViewportRequest = true;
        }));
    m_layout.SetConduitClassNameHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& Name) {
        m_pendingConduitClassName = Name;
        m_hasPendingConduitClassNameRequest = true;
    }));
    m_layout.SetConduitClassHostTypeHandler(SnAPI::UI::TDelegate<void(const TypeId&)>::Bind([this](const TypeId& Type) {
        m_pendingConduitClassHostType = Type;
        m_hasPendingConduitClassHostTypeRequest = true;
    }));
    m_layout.SetConduitClassGraphHandler(SnAPI::UI::TDelegate<void(const std::string&)>::Bind([this](const std::string& AssetKey) {
        m_pendingConduitClassGraph = AssetKey;
        m_hasPendingConduitClassGraphRequest = true;
    }));

    if (auto* AssetService = Context.GetService<EditorAssetService>())
    {
        m_layout.SetProjectSelectionRequired(!AssetService->CurrentProject().IsLoaded && !m_hasPendingProjectActionRequest);
    }
    ApplyAssetBrowserState(Context);
    m_layoutRebuildRequested = false;
}

void EditorLayoutService::Shutdown(EditorServiceContext& Context)
{
    m_layout.SetContentAssetSelectionHandler({});
    m_layout.SetContentAssetPlaceHandler({});
    m_layout.SetContentAssetDropHandler({});
    m_layout.SetContentAssetSaveHandler({});
    m_layout.SetContentAssetDeleteHandler({});
    m_layout.SetContentAssetRenameHandler({});
    m_layout.SetContentAssetRefreshHandler({});
    m_layout.SetContentAssetCreateHandler({});
    m_layout.SetContentAssetImportHandler({});
    m_layout.SetContentAssetInspectorSaveHandler({});
    m_layout.SetContentAssetInspectorReimportHandler({});
    m_layout.SetContentAssetInspectorCloseHandler({});
    m_layout.SetContentAssetInspectorRuntimeMutatedHandler({});
    m_layout.SetContentAssetInspectorImportMutatedHandler({});
    m_layout.SetContentAssetInspectorNodeSelectionHandler({});
    m_layout.SetContentAssetInspectorHierarchyActionHandler({});
    m_layout.SetConduitVariableSelectionHandler({});
    m_layout.SetConduitVariableCreateHandler({});
    m_layout.SetConduitVariableRemoveHandler({});
    m_layout.SetConduitVariableRenameHandler({});
    m_layout.SetConduitVariableTypeHandler({});
    m_layout.SetConduitGraphSelfTypeHandler({});
    m_layout.SetConduitVariableDefaultBoolHandler({});
    m_layout.SetConduitVariableDefaultTextHandler({});
    m_layout.SetConduitVariableDefaultEnumHandler({});
    m_layout.SetConduitVariableClearDefaultHandler({});
    m_layout.SetConduitVariableCommitDefaultHandler({});
    m_layout.SetConduitVariableResetDefaultHandler({});
    m_layout.SetConduitNodeSelectionHandler({});
    m_layout.SetConduitNodeCreateHandler({});
    m_layout.SetConduitNodeRemoveHandler({});
    m_layout.SetConduitNodeDefaultBoolHandler({});
    m_layout.SetConduitNodeDefaultTextHandler({});
    m_layout.SetConduitNodeDefaultEnumHandler({});
    m_layout.SetConduitNodeDefaultClearHandler({});
    m_layout.SetConduitNodeMoveHandler({});
    m_layout.SetConduitSpawnMenuRequestHandler({});
    m_layout.SetConduitSpawnMenuSelectionHandler({});
    m_layout.SetConduitPinConnectedHandler({});
    m_layout.SetConduitCompileHandler({});
    m_layout.SetConduitNodePrimaryTextHandler({});
    m_layout.SetConduitNodeSecondaryTextHandler({});
    m_layout.SetConduitViewportHandler({});
    m_layout.SetConduitClassNameHandler({});
    m_layout.SetConduitClassHostTypeHandler({});
    m_layout.SetConduitClassGraphHandler({});
    m_layout.SetHierarchySelectionHandler({});
    m_layout.SetHierarchyActionHandler({});
    m_layout.SetToolbarActionHandler({});
    m_layout.SetProjectActionHandler({});
    m_layout.SetPluginActionHandler({});
    m_layout.SetModuleActionHandler({});
    m_layout.SetBuildActionHandler({});
    m_hasPendingSelectionRequest = false;
    m_pendingSelectionRequest = {};
    m_hasPendingHierarchyActionRequest = false;
    m_pendingHierarchyActionRequest = {};
    m_hasPendingToolbarAction = false;
    m_pendingToolbarAction = EditorLayout::EToolbarAction::Play;
    m_hasPendingProjectActionRequest = false;
    m_pendingProjectActionRequest = {};
    m_hasPendingPluginActionRequest = false;
    m_pendingPluginActionRequest = {};
    m_hasPendingModuleActionRequest = false;
    m_pendingModuleActionRequest = {};
    m_hasPendingBuildActionRequest = false;
    m_pendingBuildActionRequest = {};
    m_hasPendingAssetSelection = false;
    m_pendingAssetSelectionDoubleClick = false;
    m_pendingAssetSelectionKey.clear();
    m_hasPendingAssetPlaceRequest = false;
    m_pendingAssetPlaceKey.clear();
    m_hasPendingAssetDropRequest = false;
    m_pendingAssetDropRequest = {};
    m_hasPendingAssetSaveRequest = false;
    m_pendingAssetSaveKey.clear();
    m_hasPendingAssetDeleteRequest = false;
    m_pendingAssetDeleteKey.clear();
    m_hasPendingAssetRenameRequest = false;
    m_pendingAssetRenameKey.clear();
    m_pendingAssetRenameValue.clear();
    m_hasPendingAssetRefreshRequest = false;
    m_hasPendingAssetCreateRequest = false;
    m_pendingAssetCreateRequest = {};
    m_hasPendingAssetImportRequest = false;
    m_pendingAssetImportRequest = {};
    m_hasPendingAssetInspectorSaveRequest = false;
    m_hasPendingAssetInspectorReimportRequest = false;
    m_hasPendingAssetInspectorCloseRequest = false;
    m_hasPendingAssetInspectorNodeSelectionRequest = false;
    m_pendingAssetInspectorNodeSelection = {};
    m_hasPendingAssetInspectorHierarchyActionRequest = false;
    m_pendingAssetInspectorHierarchyActionRequest = {};
    m_hasPendingConduitVariableSelectionRequest = false;
    m_pendingConduitVariableSelection = {};
    m_hasPendingConduitVariableCreateRequest = false;
    m_pendingConduitVariableCreateName.clear();
    m_pendingConduitVariableCreateType = {};
    m_hasPendingConduitVariableRemoveRequest = false;
    m_hasPendingConduitVariableRenameRequest = false;
    m_pendingConduitVariableRenameValue.clear();
    m_hasPendingConduitVariableTypeRequest = false;
    m_pendingConduitVariableType = {};
    m_hasPendingConduitGraphSelfTypeRequest = false;
    m_pendingConduitGraphSelfType = {};
    m_hasPendingConduitVariableDefaultBoolRequest = false;
    m_pendingConduitVariableDefaultBool = false;
    m_hasPendingConduitVariableDefaultTextRequest = false;
    m_pendingConduitVariableDefaultText.clear();
    m_hasPendingConduitVariableDefaultEnumRequest = false;
    m_pendingConduitVariableDefaultEnum.clear();
    m_hasPendingConduitVariableClearDefaultRequest = false;
    m_hasPendingConduitVariableCommitDefaultRequest = false;
    m_hasPendingConduitVariableResetDefaultRequest = false;
    m_hasPendingConduitNodeSelectionRequest = false;
    m_pendingConduitNodeSelection = {};
    m_hasPendingConduitNodeCreateRequest = false;
    m_pendingConduitNodeCreateStableId.clear();
    m_hasPendingConduitNodeRemoveRequest = false;
    m_hasPendingConduitNodeDefaultBoolRequest = false;
    m_pendingConduitNodeDefaultPinKey.clear();
    m_pendingConduitNodeDefaultBool = false;
    m_hasPendingConduitNodeDefaultTextRequest = false;
    m_pendingConduitNodeDefaultTextPinKey.clear();
    m_pendingConduitNodeDefaultText.clear();
    m_hasPendingConduitNodeDefaultEnumRequest = false;
    m_pendingConduitNodeDefaultEnumPinKey.clear();
    m_pendingConduitNodeDefaultEnum.clear();
    m_hasPendingConduitNodeDefaultClearRequest = false;
    m_pendingConduitNodeDefaultClearPinKey.clear();
    m_hasPendingConduitNodeMoveRequest = false;
    m_pendingConduitNodeMoveId = {};
    m_pendingConduitNodeMoveX = 0.0f;
    m_pendingConduitNodeMoveY = 0.0f;
    m_hasPendingConduitSpawnMenuOpenRequest = false;
    m_pendingConduitSpawnMenuOpenRequest = {};
    m_hasPendingConduitSpawnMenuSelectionRequest = false;
    m_pendingConduitSpawnMenuSelectionRequest = {};
    m_pendingConduitSpawnSelectionEntry = {};
    m_hasPendingConduitPinConnectRequest = false;
    m_pendingConduitConnectSourceNode = {};
    m_pendingConduitConnectSourcePin.clear();
    m_pendingConduitConnectTargetNode = {};
    m_pendingConduitConnectTargetPin.clear();
    m_hasPendingConduitCompileRequest = false;
    m_hasPendingConduitNodePrimaryTextRequest = false;
    m_pendingConduitNodePrimaryText.clear();
    m_hasPendingConduitNodeSecondaryTextRequest = false;
    m_pendingConduitNodeSecondaryText.clear();
    m_hasPendingConduitViewportRequest = false;
    m_pendingConduitViewportPanX = 0.0f;
    m_pendingConduitViewportPanY = 0.0f;
    m_pendingConduitViewportZoom = 1.0f;
    m_hasPendingConduitClassNameRequest = false;
    m_pendingConduitClassName.clear();
    m_hasPendingConduitClassHostTypeRequest = false;
    m_pendingConduitClassHostType = {};
    m_hasPendingConduitClassGraphRequest = false;
    m_pendingConduitClassGraph.clear();
    m_layoutRebuildRequested = false;
    m_assetListSignature = 0;
    m_assetDetailsSignature = 0;
    m_buildPanelStateSignature = 0;
    m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
    m_assetInspectorIconRevision = std::numeric_limits<std::uint64_t>::max();
    m_conduitWorkspaceRevision = std::numeric_limits<std::uint64_t>::max();
    m_conduitCanvasRevision = std::numeric_limits<std::uint64_t>::max();
    m_buildPanelHistoryDirty = true;
    m_buildPanelHistoryProjectFilePath.clear();
    m_buildPanelComparisonSummary.clear();
    m_cachedBuildHistory.clear();
    m_layout.Shutdown(&Context.Runtime());
}

UIRenderViewport* EditorLayoutService::GameViewportElement() const
{
    return m_layout.GameViewport();
}

int32_t EditorLayoutService::GameViewportTabIndex() const
{
    return m_layout.GameViewportTabIndex();
}

EditorLayout::EGizmoSpace EditorLayoutService::GizmoSpace() const
{
    return m_layout.GizmoSpace();
}

bool EditorLayoutService::GizmoSnappingEnabled() const
{
    return m_layout.GizmoSnappingEnabled();
}

double EditorLayoutService::MoveSnapStep() const
{
    return m_layout.MoveSnapStep();
}

double EditorLayoutService::RotateSnapStepDegrees() const
{
    return m_layout.RotateSnapStepDegrees();
}

double EditorLayoutService::ScaleSnapStep() const
{
    return m_layout.ScaleSnapStep();
}


} // namespace SnAPI::GameFramework::Editor
