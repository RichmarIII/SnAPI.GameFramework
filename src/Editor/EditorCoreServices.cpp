#include "Editor/EditorCoreServices.h"

#include "BaseNode.h"
#include "CameraComponent.h"
#include "UIRenderViewport.h"
#include "World.h"

#include <UIEvents.h>

#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "RigidBodyComponent.h"
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "ICamera.hpp"
#endif

#include <SnAPI/Math/LinearAlgebra.h>

#include <array>
#include <cmath>
#include <span>
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
    if (!SelectedNode.IsNull() && WorldPtr->NodePool().Borrowed(SelectedNode) != nullptr)
    {
        return;
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
            std::type_index(typeid(EditorCommandService))};
}

Result EditorLayoutService::Initialize(EditorServiceContext& Context)
{
    auto* ThemeService = Context.GetService<EditorThemeService>();
    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* SelectionService = Context.GetService<EditorSelectionService>();
    if (!ThemeService || !SceneService || !SelectionService)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Missing required editor services for layout"));
    }

    m_hasPendingSelectionRequest = false;
    m_pendingSelectionRequest = {};

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

    return Ok();
}

void EditorLayoutService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* CommandService = Context.GetService<EditorCommandService>();
    if (!SceneService || !SelectionService)
    {
        return;
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

    m_layout.Sync(Context.Runtime(), SceneService->ActiveCameraComponent(), &SelectionService->Model(), DeltaSeconds);
}

void EditorLayoutService::Shutdown(EditorServiceContext& Context)
{
    m_layout.SetHierarchySelectionHandler({});
    m_hasPendingSelectionRequest = false;
    m_pendingSelectionRequest = {};
    m_layout.Shutdown(&Context.Runtime());
}

UIRenderViewport* EditorLayoutService::GameViewportElement() const
{
    return m_layout.GameViewport();
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
            std::type_index(typeid(EditorCommandService))};
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

    const SnAPI::Math::Scalar NdcX = static_cast<SnAPI::Math::Scalar>((U * 2.0f) - 1.0f);
    const SnAPI::Math::Scalar NdcY = static_cast<SnAPI::Math::Scalar>(1.0f - (V * 2.0f));

    const SnAPI::Math::Vector4 ClipNear{
        NdcX, NdcY, static_cast<SnAPI::Math::Scalar>(1.0), static_cast<SnAPI::Math::Scalar>(1.0)};
    const SnAPI::Math::Vector4 ClipFar{
        NdcX, NdcY, static_cast<SnAPI::Math::Scalar>(0.0), static_cast<SnAPI::Math::Scalar>(1.0)};

    const SnAPI::Math::Matrix4 InverseViewProjection = Camera->ViewProjection().inverse();
    const SnAPI::Math::Vector4 WorldNear4 = InverseViewProjection * ClipNear;
    const SnAPI::Math::Vector4 WorldFar4 = InverseViewProjection * ClipFar;

    const auto NearW = static_cast<double>(WorldNear4.w());
    const auto FarW = static_cast<double>(WorldFar4.w());
    constexpr double kSmallNumber = 1.0e-8;
    if (std::fabs(NearW) <= kSmallNumber || std::fabs(FarW) <= kSmallNumber)
    {
        return false;
    }

    const SnAPI::Physics::Vec3 RayNear = (WorldNear4.template head<3>() / WorldNear4.w());
    const SnAPI::Physics::Vec3 RayFar = (WorldFar4.template head<3>() / WorldFar4.w());
    SnAPI::Physics::Vec3 RayDirection = RayFar - RayNear;
    const SnAPI::Math::Scalar DirectionLength = RayDirection.norm();
    if (!(DirectionLength > static_cast<SnAPI::Math::Scalar>(0.0)))
    {
        return false;
    }

    RayDirection /= DirectionLength;

    auto& Physics = WorldPtr->Physics();
    auto* Scene = Physics.Scene();
    if (!Scene)
    {
        return false;
    }

    SnAPI::Physics::RaycastRequest Request{};
    Request.Origin = Physics.WorldToPhysicsPosition(RayNear, false);
    Request.Direction = RayDirection;
    Request.Distance = static_cast<float>(100000.0);
    Request.Mode = SnAPI::Physics::EQueryMode::ClosestHit;

    std::array<SnAPI::Physics::RaycastHit, 1> Hits{};
    const std::uint32_t HitCount = Scene->Query().Raycast(Request, std::span<SnAPI::Physics::RaycastHit>(Hits));
    if (HitCount == 0 || !Hits[0].Body.IsValid())
    {
        return false;
    }

    bool Found = false;
    const SnAPI::Physics::BodyHandle HitBody = Hits[0].Body;
    WorldPtr->NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        if (Found)
        {
            return;
        }

        auto RigidBodyResult = Node.Component<RigidBodyComponent>();
        if (!RigidBodyResult || !RigidBodyResult->HasBody())
        {
            return;
        }

        if (RigidBodyResult->PhysicsBodyHandle() != HitBody)
        {
            return;
        }

        OutNode = Handle;
        Found = true;
    });

    return Found;
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

} // namespace SnAPI::GameFramework::Editor
