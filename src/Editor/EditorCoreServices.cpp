#include "Editor/EditorCoreServices.h"

#include "BaseNode.h"
#include "CameraComponent.h"
#include "InputSystem.h"
#include "Level.h"
#include "NodeGraph.h"
#include "Serialization.h"
#include "TransformComponent.h"
#include "TypeRegistry.h"
#include "UIRenderViewport.h"
#include "World.h"

#include <UIEvents.h>
#include <UIContext.h>
#include <UIElementBase.h>
#include <UIPanel.h>
#include <UIRealtimeGraph.h>
#include <UISizing.h>
#include <UIText.h>

#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "RigidBodyComponent.h"
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "ICamera.hpp"
#endif

#include <SnAPI/Math/LinearAlgebra.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "GameRuntime.h"

namespace SnAPI::GameFramework::Editor
{
namespace
{
void ApplySelection(EditorSelectionModel& Model, const NodeHandle Node)
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
    SelectNodeCommand(const NodeHandle Previous, const NodeHandle Next)
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

[[nodiscard]] bool IsFiniteFloat(const float Value)
{
    return std::isfinite(Value);
}

[[nodiscard]] bool IsFiniteVec3(const Vec3& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z());
}

[[nodiscard]] Vec3 NormalizeOrAxis(const Vec3& Value, const Vec3& FallbackAxis)
{
    const auto LengthSquared = Value.squaredNorm();
    if (!(LengthSquared > static_cast<Vec3::Scalar>(1.0e-8)))
    {
        return FallbackAxis;
    }
    return Value / std::sqrt(LengthSquared);
}

[[nodiscard]] bool IsPointInsideRect(const SnAPI::UI::UIRect& Rect, const float X, const float Y)
{
    if (!std::isfinite(Rect.X) || !std::isfinite(Rect.Y) || !std::isfinite(Rect.W) || !std::isfinite(Rect.H))
    {
        return false;
    }

    if (Rect.W <= 0.0f || Rect.H <= 0.0f || !std::isfinite(X) || !std::isfinite(Y))
    {
        return false;
    }

    return X >= Rect.X && X <= (Rect.X + Rect.W) && Y >= Rect.Y && Y <= (Rect.Y + Rect.H);
}

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

[[nodiscard]] std::string ShortTypeLabel(std::string_view QualifiedName)
{
    const std::size_t Delimiter = QualifiedName.rfind("::");
    if (Delimiter == std::string_view::npos)
    {
        return std::string(QualifiedName);
    }

    return std::string(QualifiedName.substr(Delimiter + 2));
}

[[nodiscard]] BaseNode* ResolveNodeFromHandle(const NodeHandle Handle, World& WorldRef)
{
    if (Handle.IsNull())
    {
        return &WorldRef;
    }

    if (auto* Node = Handle.Borrowed())
    {
        return Node;
    }

    if (auto* Node = Handle.BorrowedSlowByUuid())
    {
        return Node;
    }

    if (const auto HandleResult = WorldRef.NodeHandleByIdSlow(Handle.Id); HandleResult.has_value())
    {
        return WorldRef.NodePool().Borrowed(*HandleResult);
    }

    return nullptr;
}

[[nodiscard]] Result ExecuteHierarchyAction(EditorServiceContext& Context,
                                            const EditorLayout::HierarchyActionRequest& Request)
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

        NodeGraph* OwnerGraph = TargetNode->OwnerGraph();
        if (!OwnerGraph)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Target node is not owned by a graph"));
        }

        auto DestroyResult = OwnerGraph->DestroyNode(TargetNode->Handle());
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

        NodeGraph* OwnerGraph = TargetNode->OwnerGraph();
        if (!OwnerGraph)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Target node is not owned by a graph"));
        }

        auto RemoveResult = OwnerGraph->RemoveComponentByType(TargetNode->Handle(), Request.Type);
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

        NodeGraph* OwnerGraph = Request.TargetIsWorldRoot ? static_cast<NodeGraph*>(WorldPtr) : ParentNode->OwnerGraph();
        if (!OwnerGraph)
        {
            return std::unexpected(MakeError(EErrorCode::NotReady, "Target node is not owned by a graph"));
        }

        std::string NodeName = ShortTypeLabel(Type->Name);
        if (NodeName.empty())
        {
            NodeName = "Node";
        }

        auto CreateResult = OwnerGraph->CreateNode(Type->Id, NodeName);
        if (!CreateResult)
        {
            return std::unexpected(CreateResult.error());
        }

        if (!Request.TargetIsWorldRoot)
        {
            auto AttachResult = OwnerGraph->AttachChild(ParentNode->Handle(), *CreateResult);
            if (!AttachResult)
            {
                return std::unexpected(AttachResult.error());
            }
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

    NodeGraph* OwnerGraph = TargetNode->OwnerGraph();
    if (!OwnerGraph)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Target node is not owned by a graph"));
    }

    auto CreateComponentResult = ComponentSerializationRegistry::Instance().Create(*OwnerGraph, TargetNode->Handle(), Type->Id);
    if (!CreateComponentResult)
    {
        return std::unexpected(CreateComponentResult.error());
    }

    return Ok();
}

#if defined(SNAPI_GF_ENABLE_PHYSICS)
[[nodiscard]] std::optional<NodeHandle> ResolveNodeHandleByPhysicsBodyRecursive(
    NodeGraph& Graph,
    const SnAPI::Physics::BodyHandle& TargetBody,
    std::unordered_set<const NodeGraph*>& VisitedGraphs)
{
    if (!VisitedGraphs.insert(&Graph).second)
    {
        return std::nullopt;
    }

    std::optional<NodeHandle> ResolvedHandle{};
    Graph.NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (ResolvedHandle.has_value())
        {
            return;
        }

        auto RigidBodyResult = Node.Component<RigidBodyComponent>();
        if (RigidBodyResult && RigidBodyResult->HasBody() && RigidBodyResult->PhysicsBodyHandle() == TargetBody)
        {
            ResolvedHandle = Handle;
            return;
        }

        if (auto* NestedGraph = dynamic_cast<NodeGraph*>(&Node))
        {
            if (auto NestedResult = ResolveNodeHandleByPhysicsBodyRecursive(*NestedGraph, TargetBody, VisitedGraphs))
            {
                ResolvedHandle = *NestedResult;
            }
        }
    });

    return ResolvedHandle;
}
#endif
} // namespace

std::string_view EditorCommandService::Name() const
{
    return "EditorCommandService";
}

int EditorCommandService::Priority() const
{
    return -1000;
}

Result EditorCommandService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    ClearHistory();
    return Ok();
}

void EditorCommandService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    ClearHistory();
}

Result EditorCommandService::Execute(EditorServiceContext& Context, std::unique_ptr<IEditorCommand> Command)
{
    if (!Command)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Editor command must not be null"));
    }

    if (const Result ExecuteResult = Command->Execute(Context); !ExecuteResult)
    {
        return ExecuteResult;
    }

    m_redoStack.clear();
    if (m_undoStack.size() >= m_maxHistory)
    {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_undoStack.emplace_back(std::move(Command));
    return Ok();
}

Result EditorCommandService::Undo(EditorServiceContext& Context)
{
    if (m_undoStack.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "No editor command available to undo"));
    }

    std::unique_ptr<IEditorCommand> Command = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    if (const Result UndoResult = Command->Undo(Context); !UndoResult)
    {
        m_undoStack.emplace_back(std::move(Command));
        return UndoResult;
    }

    m_redoStack.emplace_back(std::move(Command));
    return Ok();
}

Result EditorCommandService::Redo(EditorServiceContext& Context)
{
    if (m_redoStack.empty())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "No editor command available to redo"));
    }

    std::unique_ptr<IEditorCommand> Command = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    if (const Result RedoResult = Command->Execute(Context); !RedoResult)
    {
        m_redoStack.emplace_back(std::move(Command));
        return RedoResult;
    }

    m_undoStack.emplace_back(std::move(Command));
    return Ok();
}

void EditorCommandService::ClearHistory()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

std::string_view EditorThemeService::Name() const
{
    return "EditorThemeService";
}

Result EditorThemeService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_theme.Initialize();
    return Ok();
}

void EditorThemeService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
}

std::string_view EditorSceneService::Name() const
{
    return "EditorSceneService";
}

Result EditorSceneService::Initialize(EditorServiceContext& Context)
{
    return m_scene.Initialize(Context.Runtime());
}

void EditorSceneService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;
    if (auto* WorldPtr = Context.Runtime().WorldPtr())
    {
        m_scene.SyncActiveCamera(*WorldPtr);
    }
}

void EditorSceneService::Shutdown(EditorServiceContext& Context)
{
    m_scene.Shutdown(&Context.Runtime());
}

CameraComponent* EditorSceneService::ActiveCameraComponent() const
{
    return m_scene.ActiveCameraComponent();
}

SnAPI::Graphics::ICamera* EditorSceneService::ActiveRenderCamera() const
{
    return m_scene.ActiveRenderCamera();
}

std::string_view EditorRootViewportService::Name() const
{
    return "EditorRootViewportService";
}

Result EditorRootViewportService::Initialize(EditorServiceContext& Context)
{
    return m_binding.Initialize(Context.Runtime(), "Editor.RootViewport");
}

void EditorRootViewportService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;
    (void)m_binding.SyncToWindow(Context.Runtime());
}

void EditorRootViewportService::Shutdown(EditorServiceContext& Context)
{
    m_binding.Shutdown(&Context.Runtime());
}

std::string_view EditorSelectionService::Name() const
{
    return "EditorSelectionService";
}

std::vector<std::type_index> EditorSelectionService::Dependencies() const
{
    return {std::type_index(typeid(EditorSceneService))};
}

Result EditorSelectionService::Initialize(EditorServiceContext& Context)
{
    m_selection.Clear();
    auto* SceneService = Context.GetService<EditorSceneService>();
    EnsureSelectionValid(Context, SceneService != nullptr ? SceneService->ActiveCameraComponent() : nullptr);
    return Ok();
}

void EditorSelectionService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;
    auto* SceneService = Context.GetService<EditorSceneService>();
    EnsureSelectionValid(Context, SceneService != nullptr ? SceneService->ActiveCameraComponent() : nullptr);
}

void EditorSelectionService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    m_selection.Clear();
}

void EditorSelectionService::EnsureSelectionValid(EditorServiceContext& Context, CameraComponent* ActiveCamera)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        m_selection.Clear();
        return;
    }

    const NodeHandle SelectedNode = m_selection.SelectedNode();
    if (!SelectedNode.IsNull())
    {
        if (auto* Resolved = m_selection.ResolveSelectedNode(*WorldPtr))
        {
            const NodeHandle ResolvedHandle = Resolved->Handle();
            if (!ResolvedHandle.IsNull() && ResolvedHandle != SelectedNode)
            {
                (void)m_selection.SelectNode(ResolvedHandle);
            }
            return;
        }
    }

    if (ActiveCamera && !ActiveCamera->Owner().IsNull())
    {
        (void)m_selection.SelectNode(ActiveCamera->Owner());
        return;
    }

    m_selection.Clear();
}

std::string_view EditorLayoutService::Name() const
{
    return "EditorLayoutService";
}

std::vector<std::type_index> EditorLayoutService::Dependencies() const
{
    return {std::type_index(typeid(EditorThemeService)),
            std::type_index(typeid(EditorSceneService)),
            std::type_index(typeid(EditorSelectionService)),
            std::type_index(typeid(EditorRootViewportService)),
            std::type_index(typeid(EditorCommandService)),
            std::type_index(typeid(EditorAssetService))};
}

Result EditorLayoutService::Initialize(EditorServiceContext& Context)
{
    auto* ThemeService = Context.GetService<EditorThemeService>();
    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* AssetService = Context.GetService<EditorAssetService>();
    if (!ThemeService || !SceneService || !SelectionService || !AssetService)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Missing required editor services for layout"));
    }

    m_hasPendingSelectionRequest = false;
    m_pendingSelectionRequest = {};
    m_hasPendingHierarchyActionRequest = false;
    m_pendingHierarchyActionRequest = {};
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
    m_layoutRebuildRequested = false;
    m_assetListSignature = 0;
    m_assetDetailsSignature = 0;

    const Result BuildResult = m_layout.Build(Context.Runtime(),
                                              ThemeService->Theme(),
                                              SceneService->ActiveCameraComponent(),
                                              &SelectionService->Model());
    if (!BuildResult)
    {
        return BuildResult;
    }

    m_layout.SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(NodeHandle)>::Bind([this](const NodeHandle Handle) {
        m_pendingSelectionRequest = Handle;
        m_hasPendingSelectionRequest = true;
    }));
    m_layout.SetHierarchyActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::HierarchyActionRequest&)>::Bind(
            [this](const EditorLayout::HierarchyActionRequest& Request) {
                m_pendingHierarchyActionRequest = Request;
                m_hasPendingHierarchyActionRequest = true;
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

    ApplyAssetBrowserState(Context);
    return Ok();
}

void EditorLayoutService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* CommandService = Context.GetService<EditorCommandService>();
    auto* AssetService = Context.GetService<EditorAssetService>();
    if (!SceneService || !SelectionService || !AssetService)
    {
        return;
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

    if (m_hasPendingAssetSelection)
    {
        m_hasPendingAssetSelection = false;

        if (!m_pendingAssetSelectionKey.empty())
        {
            if (AssetService->SelectAssetByKey(m_pendingAssetSelectionKey) && m_pendingAssetSelectionDoubleClick)
            {
                (void)AssetService->OpenSelectedAssetPreview();
            }
        }

        m_pendingAssetSelectionKey.clear();
        m_pendingAssetSelectionDoubleClick = false;
    }

    if (m_hasPendingAssetPlaceRequest)
    {
        m_hasPendingAssetPlaceRequest = false;

        if (!m_pendingAssetPlaceKey.empty())
        {
            (void)AssetService->ArmPlacementByKey(m_pendingAssetPlaceKey);
        }

        m_pendingAssetPlaceKey.clear();
    }

    if (m_hasPendingAssetSaveRequest)
    {
        m_hasPendingAssetSaveRequest = false;

        if (!m_pendingAssetSaveKey.empty())
        {
            (void)AssetService->SaveAssetByKey(m_pendingAssetSaveKey);
        }

        m_pendingAssetSaveKey.clear();
    }

    if (m_hasPendingAssetRenameRequest)
    {
        m_hasPendingAssetRenameRequest = false;

        if (!m_pendingAssetRenameKey.empty())
        {
            (void)AssetService->RenameAssetByKey(m_pendingAssetRenameKey, m_pendingAssetRenameValue);
        }

        m_pendingAssetRenameKey.clear();
        m_pendingAssetRenameValue.clear();
    }

    if (m_hasPendingAssetDeleteRequest)
    {
        m_hasPendingAssetDeleteRequest = false;

        if (!m_pendingAssetDeleteKey.empty())
        {
            (void)AssetService->DeleteAssetByKey(m_pendingAssetDeleteKey);
        }

        m_pendingAssetDeleteKey.clear();
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
        const EditorLayout::HierarchyActionRequest Request = m_pendingHierarchyActionRequest;
        m_hasPendingHierarchyActionRequest = false;
        m_pendingHierarchyActionRequest = {};
        (void)ExecuteHierarchyAction(Context, Request);
    }

    ApplyAssetBrowserState(Context);
    m_layout.Sync(Context.Runtime(), SceneService->ActiveCameraComponent(), &SelectionService->Model(), DeltaSeconds);
}

void EditorLayoutService::ApplyAssetBrowserState(EditorServiceContext& Context)
{
    auto* AssetService = Context.GetService<EditorAssetService>();
    if (!AssetService)
    {
        return;
    }

    const auto& Assets = AssetService->Assets();
    const std::size_t AssetSignature = ComputeAssetListSignature(Assets);
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
            Entry.IsDirty = Asset.IsDirty;
            Entries.emplace_back(std::move(Entry));
        }

        m_layout.SetContentAssets(std::move(Entries));
        m_assetListSignature = AssetSignature;
    }

    EditorLayout::ContentAssetDetails Details{};
    if (const auto* SelectedAsset = AssetService->SelectedAsset())
    {
        Details.Name = SelectedAsset->Name;
        Details.Type = SelectedAsset->TypeLabel;
        Details.Variant = SelectedAsset->Variant.empty() ? std::string("default") : SelectedAsset->Variant;
        Details.AssetId = SelectedAsset->Key;
        Details.IsRuntime = SelectedAsset->IsRuntime;
        Details.IsDirty = SelectedAsset->IsDirty;
        Details.CanPlace = true;
        Details.CanSave = SelectedAsset->CanSave && (!SelectedAsset->IsRuntime || SelectedAsset->IsDirty);
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
    m_hasPendingHierarchyActionRequest = false;
    m_pendingHierarchyActionRequest = {};

    const Result BuildResult = m_layout.Build(Context.Runtime(),
                                              ThemeService->Theme(),
                                              SceneService->ActiveCameraComponent(),
                                              &SelectionService->Model());
    if (!BuildResult)
    {
        m_layoutRebuildRequested = false;
        return;
    }

    m_layout.SetHierarchySelectionHandler(SnAPI::UI::TDelegate<void(NodeHandle)>::Bind([this](const NodeHandle Handle) {
        m_pendingSelectionRequest = Handle;
        m_hasPendingSelectionRequest = true;
    }));
    m_layout.SetHierarchyActionHandler(
        SnAPI::UI::TDelegate<void(const EditorLayout::HierarchyActionRequest&)>::Bind(
            [this](const EditorLayout::HierarchyActionRequest& Request) {
                m_pendingHierarchyActionRequest = Request;
                m_hasPendingHierarchyActionRequest = true;
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
    m_layout.SetHierarchySelectionHandler({});
    m_layout.SetHierarchyActionHandler({});
    m_hasPendingSelectionRequest = false;
    m_pendingSelectionRequest = {};
    m_hasPendingHierarchyActionRequest = false;
    m_pendingHierarchyActionRequest = {};
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
    m_layoutRebuildRequested = false;
    m_assetListSignature = 0;
    m_assetDetailsSignature = 0;
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

std::string_view EditorGameViewportOverlayService::Name() const
{
    return "EditorGameViewportOverlayService";
}

std::vector<std::type_index> EditorGameViewportOverlayService::Dependencies() const
{
    return {std::type_index(typeid(EditorLayoutService))};
}

Result EditorGameViewportOverlayService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    ResetOverlayState();
    return Ok();
}

void EditorGameViewportOverlayService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
#if !defined(SNAPI_GF_ENABLE_UI) || !defined(SNAPI_GF_ENABLE_RENDERER)
    (void)Context;
    (void)DeltaSeconds;
    ResetOverlayState();
    return;
#else
    auto* LayoutService = Context.GetService<EditorLayoutService>();
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!LayoutService || !WorldPtr || !WorldPtr->UI().IsInitialized())
    {
        return;
    }

    auto* Viewport = LayoutService->GameViewportElement();
    if (!Viewport)
    {
        return;
    }

    const std::uint64_t OverlayContextId = Viewport->OwnedContextId();
    if (OverlayContextId == 0)
    {
        return;
    }

    if (m_overlayContextId != OverlayContextId)
    {
        ResetOverlayState();
        m_overlayContextId = OverlayContextId;
    }

    auto* OverlayContext = WorldPtr->UI().Context(m_overlayContextId);
    if (!OverlayContext)
    {
        return;
    }

    if (!EnsureOverlayElements(*OverlayContext))
    {
        return;
    }

    UpdateOverlayVisibility(*OverlayContext, LayoutService->GameViewportTabIndex());
    UpdateOverlaySamples(*OverlayContext, DeltaSeconds);
#endif
}

void EditorGameViewportOverlayService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    ResetOverlayState();
}

void EditorGameViewportOverlayService::ResetOverlayState()
{
    m_overlayContextId = 0;

    m_hudPanel = {};
    m_hudGraph = {};
    m_hudFrameLabel = {};
    m_hudFpsLabel = {};
    m_hudFrameSeries = std::numeric_limits<std::uint32_t>::max();
    m_hudFpsSeries = std::numeric_limits<std::uint32_t>::max();

    m_profilerPanel = {};
    m_profilerGraph = {};
    m_profilerFrameLabel = {};
    m_profilerFpsLabel = {};
    m_profilerFrameSeries = std::numeric_limits<std::uint32_t>::max();
    m_profilerFpsSeries = std::numeric_limits<std::uint32_t>::max();
}

bool EditorGameViewportOverlayService::EnsureOverlayElements(SnAPI::UI::UIContext& OverlayContext)
{
#if !defined(SNAPI_GF_ENABLE_UI)
    (void)OverlayContext;
    return false;
#else
    const auto ExistingHudGraph = dynamic_cast<SnAPI::UI::UIRealtimeGraph*>(&OverlayContext.GetElement(m_hudGraph));
    const auto ExistingHudFrameLabel = dynamic_cast<SnAPI::UI::UIText*>(&OverlayContext.GetElement(m_hudFrameLabel));
    const auto ExistingHudFpsLabel = dynamic_cast<SnAPI::UI::UIText*>(&OverlayContext.GetElement(m_hudFpsLabel));
    if (ExistingHudGraph && ExistingHudFrameLabel && ExistingHudFpsLabel)
    {
        return true;
    }

    m_hudPanel = {};
    m_hudGraph = {};
    m_hudFrameLabel = {};
    m_hudFpsLabel = {};
    m_hudFrameSeries = std::numeric_limits<std::uint32_t>::max();
    m_hudFpsSeries = std::numeric_limits<std::uint32_t>::max();

    m_profilerPanel = {};
    m_profilerGraph = {};
    m_profilerFrameLabel = {};
    m_profilerFpsLabel = {};
    m_profilerFrameSeries = std::numeric_limits<std::uint32_t>::max();
    m_profilerFpsSeries = std::numeric_limits<std::uint32_t>::max();

    auto HudPanelBuilder = OverlayContext.Root().Add(SnAPI::UI::UIPanel("Editor.GameViewportOverlay.HUD"));
    auto& HudPanel = HudPanelBuilder.Element();
    HudPanel.Direction().Set(SnAPI::UI::ELayoutDirection::Vertical);
    HudPanel.Width().Set(SnAPI::UI::Sizing::Fixed(252.0f));
    HudPanel.Height().Set(SnAPI::UI::Sizing::Fixed(126.0f));
    HudPanel.HAlign().Set(SnAPI::UI::EAlignment::End);
    HudPanel.VAlign().Set(SnAPI::UI::EAlignment::End);
    HudPanel.ElementMargin().Set(SnAPI::UI::Margin{12.0f, 12.0f, 12.0f, 12.0f});
    HudPanel.Padding().Set(6.0f);
    HudPanel.Gap().Set(3.0f);
    HudPanel.Background().Set(SnAPI::UI::Color{20, 22, 27, 214});
    HudPanel.BorderColor().Set(SnAPI::UI::Color{87, 93, 104, 220});
    HudPanel.BorderThickness().Set(1.0f);
    HudPanel.CornerRadius().Set(6.0f);
    HudPanel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudStatsBuilder = HudPanelBuilder.Add(SnAPI::UI::UIPanel("Editor.GameViewportOverlay.HUD.Stats"));
    auto& HudStats = HudStatsBuilder.Element();
    HudStats.Direction().Set(SnAPI::UI::ELayoutDirection::Horizontal);
    HudStats.Width().Set(SnAPI::UI::Sizing::Fill());
    HudStats.Height().Set(SnAPI::UI::Sizing::Auto());
    HudStats.Gap().Set(12.0f);
    HudStats.Background().Set(SnAPI::UI::Color::Transparent());
    HudStats.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudFrameLabelBuilder = HudStatsBuilder.Add(SnAPI::UI::UIText("Frame: -- ms"));
    auto& HudFrameLabel = HudFrameLabelBuilder.Element();
    HudFrameLabel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    HudFrameLabel.TextColor().Set(SnAPI::UI::Color{206, 212, 221, 255});
    HudFrameLabel.HAlign().Set(SnAPI::UI::EAlignment::Start);
    HudFrameLabel.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    HudFrameLabel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudFpsLabelBuilder = HudStatsBuilder.Add(SnAPI::UI::UIText("FPS: --"));
    auto& HudFpsLabel = HudFpsLabelBuilder.Element();
    HudFpsLabel.Width().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    HudFpsLabel.TextColor().Set(SnAPI::UI::Color{223, 227, 234, 255});
    HudFpsLabel.HAlign().Set(SnAPI::UI::EAlignment::Start);
    HudFpsLabel.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    HudFpsLabel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudGraphBuilder = HudPanelBuilder.Add(SnAPI::UI::UIRealtimeGraph("Frame Time / FPS"));
    auto& HudGraph = HudGraphBuilder.Element();
    HudGraph.Width().Set(SnAPI::UI::Sizing::Fill());
    HudGraph.Height().Set(SnAPI::UI::Sizing::Ratio(1.0f));
    HudGraph.SampleCapacity().Set(220u);
    HudGraph.AutoRange().Set(true);
    HudGraph.ShowLegend().Set(false);
    HudGraph.GridLinesX().Set(8u);
    HudGraph.GridLinesY().Set(4u);
    HudGraph.ContentPadding().Set(6.0f);
    HudGraph.LineThickness().Set(1.6f);
    HudGraph.ValuePrecision().Set(1u);
    HudGraph.BackgroundColor().Set(SnAPI::UI::Color{19, 21, 25, 224});
    HudGraph.PlotBackgroundColor().Set(SnAPI::UI::Color{24, 27, 33, 230});
    HudGraph.BorderColor().Set(SnAPI::UI::Color{84, 90, 101, 216});
    HudGraph.GridColor().Set(SnAPI::UI::Color{92, 99, 110, 76});
    HudGraph.AxisColor().Set(SnAPI::UI::Color{130, 137, 149, 152});
    HudGraph.TitleColor().Set(SnAPI::UI::Color{228, 231, 237, 255});
    HudGraph.LegendTextColor().Set(SnAPI::UI::Color{186, 192, 202, 255});
    HudGraph.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    const std::uint32_t HudFrameSeries = HudGraph.AddSeries("Frame ms", SnAPI::UI::Color{184, 191, 201, 255});
    const std::uint32_t HudFpsSeries = HudGraph.AddSeries("FPS", SnAPI::UI::Color{223, 228, 235, 255});
    if (HudFrameSeries != SnAPI::UI::UIRealtimeGraph::InvalidSeries)
    {
        (void)HudGraph.SetSeriesRange(HudFrameSeries, 0.0f, 33.34f);
    }
    if (HudFpsSeries != SnAPI::UI::UIRealtimeGraph::InvalidSeries)
    {
        (void)HudGraph.SetSeriesRange(HudFpsSeries, 0.0f, 240.0f);
    }

    m_hudPanel = HudPanelBuilder.Handle().Id;
    m_hudGraph = HudGraphBuilder.Handle().Id;
    m_hudFrameLabel = HudFrameLabelBuilder.Handle().Id;
    m_hudFpsLabel = HudFpsLabelBuilder.Handle().Id;
    m_hudFrameSeries = HudFrameSeries;
    m_hudFpsSeries = HudFpsSeries;

    m_profilerPanel = {};
    m_profilerGraph = {};
    m_profilerFrameLabel = {};
    m_profilerFpsLabel = {};
    m_profilerFrameSeries = std::numeric_limits<std::uint32_t>::max();
    m_profilerFpsSeries = std::numeric_limits<std::uint32_t>::max();
    return true;
#endif
}

void EditorGameViewportOverlayService::UpdateOverlayVisibility(SnAPI::UI::UIContext& OverlayContext,
                                                               const int32_t ActiveTabIndex)
{
#if !defined(SNAPI_GF_ENABLE_UI)
    (void)OverlayContext;
    (void)ActiveTabIndex;
#else
    (void)ActiveTabIndex;
    constexpr SnAPI::UI::EVisibility HudVisibility = SnAPI::UI::EVisibility::HitTestInvisible;
    constexpr SnAPI::UI::EVisibility ProfilerVisibility = SnAPI::UI::EVisibility::Collapsed;

    if (m_hudPanel.Value != 0)
    {
        if (auto* HudPanel = dynamic_cast<SnAPI::UI::UIPanel*>(&OverlayContext.GetElement(m_hudPanel)))
        {
            HudPanel->Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, HudVisibility);
        }
    }

    if (m_profilerPanel.Value != 0)
    {
        if (auto* ProfilerPanel = dynamic_cast<SnAPI::UI::UIPanel*>(&OverlayContext.GetElement(m_profilerPanel)))
        {
            ProfilerPanel->Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, ProfilerVisibility);
        }
    }
#endif
}

void EditorGameViewportOverlayService::UpdateOverlaySamples(SnAPI::UI::UIContext& OverlayContext, const float DeltaSeconds)
{
#if !defined(SNAPI_GF_ENABLE_UI)
    (void)OverlayContext;
    (void)DeltaSeconds;
#else
    if (!std::isfinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
    {
        return;
    }

    const float FrameMs = std::clamp(DeltaSeconds * 1000.0f, 0.0f, 500.0f);
    const float FramesPerSecond = std::clamp(1.0f / DeltaSeconds, 0.0f, 2000.0f);

    auto PushGraphSamples = [FrameMs, FramesPerSecond](SnAPI::UI::UIContext& Context,
                                                       const SnAPI::UI::ElementId GraphId,
                                                       const std::uint32_t FrameSeries,
                                                       const std::uint32_t FpsSeries) {
        auto* Graph = dynamic_cast<SnAPI::UI::UIRealtimeGraph*>(&Context.GetElement(GraphId));
        if (!Graph || FrameSeries == std::numeric_limits<std::uint32_t>::max())
        {
            return;
        }
        (void)Graph->PushSample(FrameSeries, FrameMs);
        if (FpsSeries != std::numeric_limits<std::uint32_t>::max())
        {
            (void)Graph->PushSample(FpsSeries, FramesPerSecond);
        }
    };

    auto UpdateLabel = [FrameMs, FramesPerSecond](SnAPI::UI::UIContext& Context,
                                                  const SnAPI::UI::ElementId FrameLabelId,
                                                  const SnAPI::UI::ElementId FpsLabelId) {
        if (auto* FrameLabel = dynamic_cast<SnAPI::UI::UIText*>(&Context.GetElement(FrameLabelId)))
        {
            char Buffer[64]{};
            std::snprintf(Buffer, sizeof(Buffer), "Frame: %.2f ms", FrameMs);
            FrameLabel->Text().Set(std::string(Buffer));
        }
        if (auto* FpsLabel = dynamic_cast<SnAPI::UI::UIText*>(&Context.GetElement(FpsLabelId)))
        {
            char Buffer[64]{};
            std::snprintf(Buffer, sizeof(Buffer), "FPS: %.1f", FramesPerSecond);
            FpsLabel->Text().Set(std::string(Buffer));
        }
    };

    PushGraphSamples(OverlayContext, m_hudGraph, m_hudFrameSeries, m_hudFpsSeries);
    UpdateLabel(OverlayContext, m_hudFrameLabel, m_hudFpsLabel);
#endif
}

std::string_view EditorSelectionInteractionService::Name() const
{
    return "EditorSelectionInteractionService";
}

std::vector<std::type_index> EditorSelectionInteractionService::Dependencies() const
{
    return {std::type_index(typeid(EditorSceneService)),
            std::type_index(typeid(EditorSelectionService)),
            std::type_index(typeid(EditorLayoutService)),
            std::type_index(typeid(EditorCommandService)),
            std::type_index(typeid(EditorAssetService))};
}

Result EditorSelectionInteractionService::Initialize(EditorServiceContext& Context)
{
    m_host = &Context.Host();
    m_pointerPressedInside = false;
    m_pointerDragged = false;
    m_pointerPressPosition = {};
    RebindViewportHandler(Context);
    return Ok();
}

void EditorSelectionInteractionService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;
    m_host = &Context.Host();
    RebindViewportHandler(Context);
}

void EditorSelectionInteractionService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
#if defined(SNAPI_GF_ENABLE_UI) && defined(SNAPI_GF_ENABLE_RENDERER)
    if (m_boundViewport)
    {
        m_boundViewport->ClearPointerEventHandler();
    }
#endif

    m_boundViewport = nullptr;
    m_pointerPressedInside = false;
    m_pointerDragged = false;
    m_pointerPressPosition = {};
    m_host = nullptr;
}

void EditorSelectionInteractionService::RebindViewportHandler(EditorServiceContext& Context)
{
#if !defined(SNAPI_GF_ENABLE_UI) || !defined(SNAPI_GF_ENABLE_RENDERER)
    (void)Context;
    return;
#else
    auto* LayoutService = Context.GetService<EditorLayoutService>();
    auto* NextViewport = LayoutService ? LayoutService->GameViewportElement() : nullptr;
    if (m_boundViewport == NextViewport)
    {
        return;
    }

    if (m_boundViewport)
    {
        m_boundViewport->ClearPointerEventHandler();
    }

    m_boundViewport = NextViewport;
    m_pointerPressedInside = false;
    m_pointerDragged = false;
    m_pointerPressPosition = {};

    if (!m_boundViewport)
    {
        return;
    }

    m_boundViewport->SetPointerEventHandler(
        SnAPI::UI::TDelegate<void(const SnAPI::UI::PointerEvent&, std::uint32_t, bool)>::Bind(
            [this](const SnAPI::UI::PointerEvent& Event, const std::uint32_t RoutedTypeId, const bool ContainsPointer) {
                if (!m_host)
                {
                    return;
                }

                EditorServiceContext EventContext(*m_host);
                HandleViewportPointerEvent(EventContext, Event, RoutedTypeId, ContainsPointer);
            }));
#endif
}

void EditorSelectionInteractionService::HandleViewportPointerEvent(EditorServiceContext& Context,
                                                                   const SnAPI::UI::PointerEvent& Event,
                                                                   const std::uint32_t RoutedTypeId,
                                                                   const bool ContainsPointer)
{
#if !defined(SNAPI_GF_ENABLE_UI)
    (void)Context;
    (void)Event;
    (void)RoutedTypeId;
    (void)ContainsPointer;
    return;
#else
    constexpr float kDragThresholdPixels = 3.0f;
    constexpr float kDragThresholdSquared = kDragThresholdPixels * kDragThresholdPixels;

    if (RoutedTypeId == SnAPI::UI::RoutedEventTypes::PointerDown.Id)
    {
        if (Event.LeftDown && ContainsPointer)
        {
            m_pointerPressedInside = true;
            m_pointerDragged = false;
            m_pointerPressPosition = Event.Position;
        }
        return;
    }

    if (RoutedTypeId == SnAPI::UI::RoutedEventTypes::PointerMove.Id)
    {
        if (m_pointerPressedInside && Event.LeftDown)
        {
            const float Dx = Event.Position.X - m_pointerPressPosition.X;
            const float Dy = Event.Position.Y - m_pointerPressPosition.Y;
            const float DistanceSquared = (Dx * Dx) + (Dy * Dy);
            if (DistanceSquared > kDragThresholdSquared)
            {
                m_pointerDragged = true;
            }
        }
        return;
    }

    if (RoutedTypeId != SnAPI::UI::RoutedEventTypes::PointerUp.Id)
    {
        return;
    }

    const bool ShouldPick = m_pointerPressedInside && !m_pointerDragged && ContainsPointer;
    m_pointerPressedInside = false;
    m_pointerDragged = false;

    if (!ShouldPick)
    {
        return;
    }

    auto* AssetService = Context.GetService<EditorAssetService>();
    if (AssetService && AssetService->IsPlacementArmed())
    {
        if (const Result InstantiateResult = AssetService->InstantiateArmedAsset(Context); InstantiateResult)
        {
            return;
        }
    }

    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* CommandService = Context.GetService<EditorCommandService>();
    if (!SelectionService || !CommandService)
    {
        return;
    }

    const NodeHandle Previous = SelectionService->Model().SelectedNode();
    NodeHandle Next{};
    (void)TryResolvePickedNode(Context, Event.Position, Next);

    if (Previous == Next)
    {
        return;
    }

    (void)CommandService->Execute(Context, std::make_unique<SelectNodeCommand>(Previous, Next));
#endif
}

bool EditorSelectionInteractionService::TryResolvePickedNode(EditorServiceContext& Context,
                                                             const SnAPI::UI::UIPoint& ScreenPoint,
                                                             NodeHandle& OutNode) const
{
    OutNode = {};

    switch (m_backend)
    {
    case EEditorPickingBackend::PhysicsRaycast:
        return TryResolvePickedNodePhysics(Context, ScreenPoint, OutNode);
    case EEditorPickingBackend::ActiveCameraOwner:
        return TryResolvePickedNodeActiveCamera(Context, OutNode);
    case EEditorPickingBackend::RendererIdBuffer:
        return false;
    case EEditorPickingBackend::Auto:
    default:
        if (TryResolvePickedNodePhysics(Context, ScreenPoint, OutNode))
        {
            return true;
        }
        return TryResolvePickedNodeActiveCamera(Context, OutNode);
    }
}

bool EditorSelectionInteractionService::TryResolvePickedNodePhysics(EditorServiceContext& Context,
                                                                    const SnAPI::UI::UIPoint& ScreenPoint,
                                                                    NodeHandle& OutNode) const
{
    OutNode = {};

#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI) || !defined(SNAPI_GF_ENABLE_PHYSICS)
    (void)Context;
    (void)ScreenPoint;
    return false;
#else
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return false;
    }

    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* LayoutService = Context.GetService<EditorLayoutService>();
    if (!SceneService || !LayoutService)
    {
        return false;
    }

    auto* Viewport = LayoutService->GameViewportElement();
    auto* Camera = SceneService->ActiveRenderCamera();
    if (!Viewport || !Camera)
    {
        return false;
    }

    const SnAPI::UI::UIRect ViewRect = Viewport->LayoutRect();
    if (ViewRect.W <= 0.0f || ViewRect.H <= 0.0f || !ViewRect.Contains(ScreenPoint))
    {
        return false;
    }

    const float U = (ScreenPoint.X - ViewRect.X) / ViewRect.W;
    const float V = (ScreenPoint.Y - ViewRect.Y) / ViewRect.H;
    if (!std::isfinite(U) || !std::isfinite(V))
    {
        return false;
    }

    const SnAPI::Math::Scalar NormalizedX = static_cast<SnAPI::Math::Scalar>((U * 2.0f) - 1.0f);
    const SnAPI::Math::Scalar NormalizedY = static_cast<SnAPI::Math::Scalar>(1.0f - (V * 2.0f));

    const SnAPI::Math::Scalar FovRadians = static_cast<SnAPI::Math::Scalar>(
        static_cast<double>(Camera->Fov()) * (std::numbers::pi_v<double> / 180.0));
    const SnAPI::Math::Scalar TanHalfFov = static_cast<SnAPI::Math::Scalar>(
        std::tan(static_cast<double>(FovRadians) * 0.5));
    const SnAPI::Math::Scalar Aspect = static_cast<SnAPI::Math::Scalar>(Camera->Aspect());
    if (!std::isfinite(TanHalfFov) || !std::isfinite(Aspect) ||
        !(TanHalfFov > static_cast<SnAPI::Math::Scalar>(0.0)) ||
        !(Aspect > static_cast<SnAPI::Math::Scalar>(0.0)))
    {
        return false;
    }

    SnAPI::Physics::Vec3 Forward = Camera->Forward().template cast<SnAPI::Math::Scalar>();
    SnAPI::Physics::Vec3 Right = Camera->Right().template cast<SnAPI::Math::Scalar>();
    SnAPI::Physics::Vec3 Up = Camera->Up().template cast<SnAPI::Math::Scalar>();

    const SnAPI::Math::Scalar ForwardLength = Forward.norm();
    const SnAPI::Math::Scalar RightLength = Right.norm();
    const SnAPI::Math::Scalar UpLength = Up.norm();
    constexpr SnAPI::Math::Scalar kSmallNumber = static_cast<SnAPI::Math::Scalar>(1.0e-8);
    if (!(ForwardLength > kSmallNumber) || !(RightLength > kSmallNumber) || !(UpLength > kSmallNumber))
    {
        return false;
    }

    Forward /= ForwardLength;
    Right /= RightLength;
    Up /= UpLength;

    SnAPI::Physics::Vec3 RayDirection = Forward +
        (Right * (NormalizedX * Aspect * TanHalfFov)) +
        (Up * (NormalizedY * TanHalfFov));

    const SnAPI::Math::Scalar DirectionLength = RayDirection.norm();
    if (!(DirectionLength > kSmallNumber))
    {
        return false;
    }
    RayDirection /= DirectionLength;

    const SnAPI::Math::Scalar NearClip =
        std::max(static_cast<SnAPI::Math::Scalar>(Camera->Near()), static_cast<SnAPI::Math::Scalar>(0.001));
    const SnAPI::Physics::Vec3 CameraPosition = Camera->Position().template cast<SnAPI::Math::Scalar>();
    const SnAPI::Physics::Vec3 RayOrigin = CameraPosition + (RayDirection * NearClip);

    if (!WorldPtr->ShouldAllowPhysicsQueries())
    {
        return false;
    }

    auto& Physics = WorldPtr->Physics();
    auto* Scene = Physics.Scene();
    if (!Scene)
    {
        return false;
    }

    SnAPI::Physics::RaycastRequest Request{};
    Request.Origin = Physics.WorldToPhysicsPosition(RayOrigin, false);
    Request.Direction = RayDirection;
    Request.Distance = static_cast<float>(100000.0);
    Request.Mode = SnAPI::Physics::EQueryMode::ClosestHit;

    std::array<SnAPI::Physics::RaycastHit, 1> Hits{};
    const std::uint32_t HitCount = Scene->Query().Raycast(Request, std::span<SnAPI::Physics::RaycastHit>(Hits));
    if (HitCount == 0 || !Hits[0].Body.IsValid())
    {
        return false;
    }

    const SnAPI::Physics::BodyHandle HitBody = Hits[0].Body;
    std::unordered_set<const NodeGraph*> VisitedGraphs{};
    if (auto Resolved = ResolveNodeHandleByPhysicsBodyRecursive(*WorldPtr, HitBody, VisitedGraphs))
    {
        OutNode = *Resolved;
        return true;
    }

    return false;
#endif
}

bool EditorSelectionInteractionService::TryResolvePickedNodeActiveCamera(EditorServiceContext& Context,
                                                                         NodeHandle& OutNode) const
{
    OutNode = {};
    auto* SceneService = Context.GetService<EditorSceneService>();
    if (!SceneService)
    {
        return false;
    }

    auto* Camera = SceneService->ActiveCameraComponent();
    if (!Camera || Camera->Owner().IsNull())
    {
        return false;
    }

    OutNode = Camera->Owner();
    return true;
}

std::string_view EditorTransformInteractionService::Name() const
{
    return "EditorTransformInteractionService";
}

std::vector<std::type_index> EditorTransformInteractionService::Dependencies() const
{
    return {std::type_index(typeid(EditorSceneService)),
            std::type_index(typeid(EditorSelectionService)),
            std::type_index(typeid(EditorLayoutService))};
}

Result EditorTransformInteractionService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_mode = EEditorTransformMode::Translate;
    m_dragging = false;
    m_lastMouseX = 0.0f;
    m_lastMouseY = 0.0f;
    return Ok();
}

void EditorTransformInteractionService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;

#if !defined(SNAPI_GF_ENABLE_INPUT) || !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
    (void)Context;
    m_dragging = false;
    return;
#else
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr || !WorldPtr->Input().IsInitialized())
    {
        m_dragging = false;
        return;
    }

    const auto* Snapshot = WorldPtr->Input().Snapshot();
    if (!Snapshot || !Snapshot->IsWindowFocused())
    {
        m_dragging = false;
        return;
    }

    const bool RightDown = Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Right);
    if (!RightDown)
    {
        if (Snapshot->KeyPressed(SnAPI::Input::EKey::W))
        {
            m_mode = EEditorTransformMode::Translate;
        }
        else if (Snapshot->KeyPressed(SnAPI::Input::EKey::E))
        {
            m_mode = EEditorTransformMode::Rotate;
        }
        else if (Snapshot->KeyPressed(SnAPI::Input::EKey::R))
        {
            m_mode = EEditorTransformMode::Scale;
        }
    }

    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* LayoutService = Context.GetService<EditorLayoutService>();
    if (!SelectionService || !SceneService || !LayoutService)
    {
        m_dragging = false;
        return;
    }

    const NodeHandle Selected = SelectionService->Model().SelectedNode();
    if (Selected.IsNull())
    {
        m_dragging = false;
        return;
    }

    BaseNode* Node = SelectionService->Model().ResolveSelectedNode(*WorldPtr);
    if (!Node)
    {
        m_dragging = false;
        return;
    }

    auto TransformResult = Node->Component<TransformComponent>();
    if (!TransformResult)
    {
        m_dragging = false;
        return;
    }

    auto* Camera = SceneService->ActiveRenderCamera();
    auto* ViewportElement = LayoutService->GameViewportElement();
    if (!Camera || !ViewportElement)
    {
        m_dragging = false;
        return;
    }

    const SnAPI::UI::UIRect ViewRect = ViewportElement->LayoutRect();
    const float MouseX = Snapshot->Mouse().X;
    const float MouseY = Snapshot->Mouse().Y;
    const bool PointerInside = IsPointInsideRect(ViewRect, MouseX, MouseY);

    const bool LeftDown = Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Left);
    const bool LeftPressed = Snapshot->MouseButtonPressed(SnAPI::Input::EMouseButton::Left);
    const bool AllowTransform = PointerInside && LeftDown && !RightDown;
    if (!AllowTransform)
    {
        m_dragging = false;
        return;
    }

    if (!m_dragging || LeftPressed)
    {
        m_dragging = true;
        m_lastMouseX = MouseX;
        m_lastMouseY = MouseY;
        return;
    }

    const float Dx = MouseX - m_lastMouseX;
    const float Dy = MouseY - m_lastMouseY;
    m_lastMouseX = MouseX;
    m_lastMouseY = MouseY;
    if (!IsFiniteFloat(Dx) || !IsFiniteFloat(Dy))
    {
        return;
    }

    constexpr float kTransformDragThresholdPixels = 3.0f;
    if (std::fabs(Dx) < kTransformDragThresholdPixels && std::fabs(Dy) < kTransformDragThresholdPixels)
    {
        return;
    }

    auto& Transform = *TransformResult;
    if (!IsFiniteVec3(Transform.Position))
    {
        Transform.Position = Vec3::Zero();
    }
    if (!IsFiniteVec3(Transform.Scale))
    {
        Transform.Scale = Vec3::Ones();
    }
    if (!std::isfinite(Transform.Rotation.x()) || !std::isfinite(Transform.Rotation.y()) ||
        !std::isfinite(Transform.Rotation.z()) || !std::isfinite(Transform.Rotation.w()) ||
        !(Transform.Rotation.squaredNorm() > static_cast<Quat::Scalar>(0.0)))
    {
        Transform.Rotation = Quat::Identity();
    }

    const bool Fast = Snapshot->KeyDown(SnAPI::Input::EKey::LeftShift) ||
                      Snapshot->KeyDown(SnAPI::Input::EKey::RightShift);
    const float SpeedMultiplier = Fast ? 2.0f : 1.0f;

    switch (m_mode)
    {
    case EEditorTransformMode::Translate:
        {
            Vec3 Right = NormalizeOrAxis(Camera->Right().template cast<SnAPI::Math::Scalar>(), Vec3::UnitX());
            Vec3 Up = NormalizeOrAxis(Camera->Up().template cast<SnAPI::Math::Scalar>(), Vec3::UnitY());
            const Vec3 CameraPos = Camera->Position().template cast<SnAPI::Math::Scalar>();
            const SnAPI::Math::Scalar Distance = std::max<SnAPI::Math::Scalar>(
                static_cast<SnAPI::Math::Scalar>(0.25),
                (Transform.Position - CameraPos).norm());
            const SnAPI::Math::Scalar PixelScale = Distance * static_cast<SnAPI::Math::Scalar>(0.0015 * SpeedMultiplier);
            Transform.Position += (Right * static_cast<SnAPI::Math::Scalar>(Dx) * PixelScale) +
                                  (Up * static_cast<SnAPI::Math::Scalar>(-Dy) * PixelScale);
        }
        break;
    case EEditorTransformMode::Rotate:
        {
            const SnAPI::Math::Scalar DegreesPerPixel = static_cast<SnAPI::Math::Scalar>(0.25 * SpeedMultiplier);
            const SnAPI::Math::Scalar YawRadians = SnAPI::Math::SLinearAlgebra::DegreesToRadians(
                static_cast<SnAPI::Math::Scalar>(Dx) * DegreesPerPixel);
            const SnAPI::Math::Scalar PitchRadians = SnAPI::Math::SLinearAlgebra::DegreesToRadians(
                static_cast<SnAPI::Math::Scalar>(-Dy) * DegreesPerPixel);

            const Quat YawQuat(SnAPI::Math::AngleAxis3D(YawRadians, Vec3::UnitY()));
            const Vec3 PitchAxis = NormalizeOrAxis(
                Camera->Right().template cast<SnAPI::Math::Scalar>(),
                Vec3::UnitX());
            const Quat PitchQuat(SnAPI::Math::AngleAxis3D(PitchRadians, PitchAxis));

            Transform.Rotation = (YawQuat * PitchQuat * Transform.Rotation).normalized();
        }
        break;
    case EEditorTransformMode::Scale:
        {
            const SnAPI::Math::Scalar Delta = static_cast<SnAPI::Math::Scalar>((Dx - Dy) * 0.01f * SpeedMultiplier);
            const SnAPI::Math::Scalar ScaleFactor = std::max<SnAPI::Math::Scalar>(
                static_cast<SnAPI::Math::Scalar>(0.01),
                static_cast<SnAPI::Math::Scalar>(1.0) + Delta);
            Transform.Scale *= ScaleFactor;
            Transform.Scale.x() = std::max<SnAPI::Math::Scalar>(Transform.Scale.x(), static_cast<SnAPI::Math::Scalar>(0.001));
            Transform.Scale.y() = std::max<SnAPI::Math::Scalar>(Transform.Scale.y(), static_cast<SnAPI::Math::Scalar>(0.001));
            Transform.Scale.z() = std::max<SnAPI::Math::Scalar>(Transform.Scale.z(), static_cast<SnAPI::Math::Scalar>(0.001));
        }
        break;
    default:
        break;
    }
#endif
}

void EditorTransformInteractionService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    m_dragging = false;
}

} // namespace SnAPI::GameFramework::Editor
