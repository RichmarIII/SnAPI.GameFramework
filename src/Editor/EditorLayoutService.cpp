#include "Editor/EditorLayoutService.h"

#include "AssetPipelineIds.h"
#include "BaseNode.h"
#include "CameraComponent.h"
#include "Conduit/Editor/Service.h"
#include "Editor/EditorAssetIconService.h"
#include "Editor/EditorAssetService.h"
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
#include "Serialization.h"
#include "TypeRegistry.h"
#include "UIRenderViewport.h"
#include "World.h"

#include <UIContext.h>

#include <algorithm>
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
void ApplySelection(EditorSelectionModel& Model, const NodeHandle& Node)
{
    if (Node.IsNull())
    {
        Model.Clear();
        return;
    }

    (void)Model.SelectNode(Node);
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
    const std::size_t Delimiter = QualifiedName.rfind("::");
    if (Delimiter == std::string_view::npos)
    {
        return std::string(QualifiedName);
    }

    return std::string(QualifiedName.substr(Delimiter + 2));
}

[[nodiscard]] bool CanPlaceAssetKind(const ::SnAPI::AssetPipeline::TypeId& AssetKind)
{
    return AssetKind == AssetKindNode() ||
           AssetKind == AssetKindLevel() ||
           AssetKind == AssetKindWorld();
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
    auto* IconService = Context.GetService<EditorAssetIconService>();
    auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
    if (!ThemeService || !SceneService || !SelectionService || !PieService || !AssetService || !IconService || !ConduitService)
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
    m_hasPendingAssetSelection = false;
    m_pendingAssetSelectionDoubleClick = false;
    m_pendingAssetSelectionKey.clear();
    m_hasPendingAssetPlaceRequest = false;
    m_pendingAssetPlaceKey.clear();
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
    m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
    m_assetInspectorIconRevision = std::numeric_limits<std::uint64_t>::max();
    m_conduitWorkspaceRevision = std::numeric_limits<std::uint64_t>::max();

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
    m_layout.SetConduitNodeMoveHandler(SnAPI::UI::TDelegate<void(const Uuid&, float, float)>::Bind(
        [this](const Uuid& NodeId, const float X, const float Y) {
            m_pendingConduitNodeMoveId = NodeId;
            m_pendingConduitNodeMoveX = X;
            m_pendingConduitNodeMoveY = Y;
            m_hasPendingConduitNodeMoveRequest = true;
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
    auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
    if (!SceneService || !SelectionService || !PieService || !AssetService || !ConduitService)
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
            ProjectResult = AssetService->CreateProject(Context, Request.ProjectName, Request.ProjectDirectory);
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
            (void)SceneService->EnsureEditorCamera(Context);
            SelectionService->Model().Clear();
            if (CommandService)
            {
                CommandService->ClearHistory();
            }
            QueueLayoutRebuild();
        }
    }

    const bool HasProjectLoaded = AssetService->CurrentProject().IsLoaded;
    const bool RequireProjectSelection = !HasProjectLoaded && !m_hasPendingProjectActionRequest;
    m_layout.SetProjectSelectionRequired(RequireProjectSelection);
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

    if (m_hasPendingConduitNodeMoveRequest)
    {
        m_hasPendingConduitNodeMoveRequest = false;
        (void)ConduitService->MoveNode(m_pendingConduitNodeMoveId, m_pendingConduitNodeMoveX, m_pendingConduitNodeMoveY);
        m_pendingConduitNodeMoveId = {};
        m_pendingConduitNodeMoveX = 0.0f;
        m_pendingConduitNodeMoveY = 0.0f;
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

void EditorLayoutService::ApplyAssetBrowserState(EditorServiceContext& Context)
{
    auto* AssetService = Context.GetService<EditorAssetService>();
    auto* IconService = Context.GetService<EditorAssetIconService>();
    auto* ConduitService = Context.GetService<Conduit::Editor::ConduitEditorService>();
    if (!AssetService || !IconService || !ConduitService)
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
        HashCombine(std::hash<std::uint64_t>{}(Document.Revision()));
    }
    for (const auto& Document : ConduitService->ClassDocuments())
    {
        HashCombine(std::hash<std::string>{}(Document.AssetKey()));
        HashCombine(static_cast<std::size_t>(Document.IsDirty() ? 1u : 0u));
        HashCombine(std::hash<std::uint64_t>{}(Document.Revision()));
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

    if (!AssetService->StatusMessage().empty())
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
                    .Kind = Pin.Kind,
                    .IsInput = Pin.IsInput,
                    .IsExec = Pin.IsExec,
                });
            }

            WorkspaceState.CanvasNodes.push_back(EditorLayout::ConduitWorkspaceState::CanvasNode{
                .Id = Node.Id,
                .Title = Node.Title,
                .Detail = Node.Detail,
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
        }
        else if (ConduitView.WarningCount > 0)
        {
            WorkspaceState.Status = "Compiled with " + std::to_string(ConduitView.WarningCount) + " warning(s).";
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
    if (!ThemeService || !SceneService || !SelectionService)
    {
        m_layoutRebuildRequested = false;
        return;
    }

    m_layout.Shutdown(&Context.Runtime());
    m_assetListSignature = 0;
    m_assetDetailsSignature = 0;
    m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
    m_assetInspectorIconRevision = std::numeric_limits<std::uint64_t>::max();
    m_conduitWorkspaceRevision = std::numeric_limits<std::uint64_t>::max();
    m_hasPendingHierarchyActionRequest = false;
    m_pendingHierarchyActionRequest = {};
    m_hasPendingToolbarAction = false;
    m_pendingToolbarAction = EditorLayout::EToolbarAction::Play;
    m_hasPendingProjectActionRequest = false;
    m_pendingProjectActionRequest = {};
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
    m_layout.SetConduitNodeMoveHandler(SnAPI::UI::TDelegate<void(const Uuid&, float, float)>::Bind(
        [this](const Uuid& NodeId, const float X, const float Y) {
            m_pendingConduitNodeMoveId = NodeId;
            m_pendingConduitNodeMoveX = X;
            m_pendingConduitNodeMoveY = Y;
            m_hasPendingConduitNodeMoveRequest = true;
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
    m_layout.SetContentAssetSaveHandler({});
    m_layout.SetContentAssetDeleteHandler({});
    m_layout.SetContentAssetRenameHandler({});
    m_layout.SetContentAssetRefreshHandler({});
    m_layout.SetContentAssetCreateHandler({});
    m_layout.SetContentAssetImportHandler({});
    m_layout.SetContentAssetInspectorSaveHandler({});
    m_layout.SetContentAssetInspectorReimportHandler({});
    m_layout.SetContentAssetInspectorCloseHandler({});
    m_layout.SetContentAssetInspectorNodeSelectionHandler({});
    m_layout.SetContentAssetInspectorHierarchyActionHandler({});
    m_layout.SetConduitVariableSelectionHandler({});
    m_layout.SetConduitVariableCreateHandler({});
    m_layout.SetConduitVariableRemoveHandler({});
    m_layout.SetConduitVariableRenameHandler({});
    m_layout.SetConduitVariableTypeHandler({});
    m_layout.SetConduitVariableDefaultBoolHandler({});
    m_layout.SetConduitVariableDefaultTextHandler({});
    m_layout.SetConduitVariableDefaultEnumHandler({});
    m_layout.SetConduitVariableClearDefaultHandler({});
    m_layout.SetConduitVariableCommitDefaultHandler({});
    m_layout.SetConduitVariableResetDefaultHandler({});
    m_layout.SetConduitNodeSelectionHandler({});
    m_layout.SetConduitNodeCreateHandler({});
    m_layout.SetConduitNodeRemoveHandler({});
    m_layout.SetConduitNodeMoveHandler({});
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
    m_hasPendingSelectionRequest = false;
    m_pendingSelectionRequest = {};
    m_hasPendingHierarchyActionRequest = false;
    m_pendingHierarchyActionRequest = {};
    m_hasPendingToolbarAction = false;
    m_pendingToolbarAction = EditorLayout::EToolbarAction::Play;
    m_hasPendingProjectActionRequest = false;
    m_pendingProjectActionRequest = {};
    m_hasPendingAssetSelection = false;
    m_pendingAssetSelectionDoubleClick = false;
    m_pendingAssetSelectionKey.clear();
    m_hasPendingAssetPlaceRequest = false;
    m_pendingAssetPlaceKey.clear();
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
    m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
    m_assetInspectorIconRevision = std::numeric_limits<std::uint64_t>::max();
    m_conduitWorkspaceRevision = std::numeric_limits<std::uint64_t>::max();
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
