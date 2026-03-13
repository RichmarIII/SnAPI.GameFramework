#include "Editor/EditorSelectionInteractionService.h"

#include "BaseNode.h"
#include "CameraComponent.h"
#include "Editor/EditorAssetService.h"
#include "Editor/EditorCommandService.h"
#include "Editor/EditorLayoutService.h"
#include "Editor/EditorPieService.h"
#include "Editor/EditorSceneService.h"
#include "Editor/EditorSelectionService.h"
#include "GameRuntime.h"
#include "RenderAssetRuntime.h"
#include "StaticMeshComponent.h"
#include "SkeletalMeshComponent.h"
#include "TransformComponent.h"
#include "UIRenderViewport.h"
#include "World.h"

#include <UIContext.h>
#include <UIEvents.h>

#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "RigidBodyComponent.h"
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include "ICamera.hpp"
#include "IRenderObject.hpp"
#include "WindowBase.hpp"
#include <MeshRenderObject.hpp>
#endif

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

#if defined(SNAPI_GF_ENABLE_PHYSICS)
[[nodiscard]] std::optional<NodeHandle> ResolveNodeHandleByPhysicsBody(World& WorldRef,
                                                                        const SnAPI::Physics::BodyHandle& TargetBody)
{
    std::optional<NodeHandle> ResolvedHandle{};
    WorldRef.ForEachNode([&](const NodeHandle& Handle, BaseNode& Node) {
        if (ResolvedHandle.has_value())
        {
            return;
        }
        if (Node.EditorTransient())
        {
            return;
        }

        auto RigidBodyResult = Node.Component<RigidBodyComponent>();
        if (RigidBodyResult && RigidBodyResult->HasBody() && RigidBodyResult->PhysicsBodyHandle() == TargetBody)
        {
            ResolvedHandle = Handle;
        }
    });

    return ResolvedHandle;
}
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(WITH_EDITOR) && WITH_EDITOR
[[nodiscard]] std::optional<NodeHandle> ResolveNodeHandleByRenderObject(World& WorldRef,
                                                                         const SnAPI::Graphics::IRenderObject* TargetRenderObject)
{
    if (!TargetRenderObject)
    {
        return std::nullopt;
    }

    std::optional<NodeHandle> ResolvedHandle{};
    WorldRef.ForEachNode([&](const NodeHandle& Handle, BaseNode& Node) {
        if (ResolvedHandle.has_value() || Node.EditorTransient())
        {
            return;
        }

        auto StaticMeshResult = Node.Component<StaticMeshComponent>();
        if (StaticMeshResult && StaticMeshResult->RenderObject()
            && StaticMeshResult->RenderObject().get() == TargetRenderObject)
        {
            ResolvedHandle = Handle;
            return;
        }

        auto SkeletalMeshResult = Node.Component<SkeletalMeshComponent>();
        if (SkeletalMeshResult && SkeletalMeshResult->RenderObject()
            && static_cast<const SnAPI::Graphics::IRenderObject*>(SkeletalMeshResult->RenderObject().get()) == TargetRenderObject)
        {
            ResolvedHandle = Handle;
        }
    });

    return ResolvedHandle;
}
#endif
} // namespace

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
            std::type_index(typeid(EditorPieService)),
            std::type_index(typeid(EditorAssetService))};
}

Result EditorSelectionInteractionService::Initialize(EditorServiceContext& Context)
{
    m_host = &Context.Host();
    m_pointerPressedInside = false;
    m_pointerDragged = false;
    m_pointerPressPosition = {};
    m_pieMouseCaptureEnabled = false;
    RebindViewportHandler(Context);
    return Ok();
}

void EditorSelectionInteractionService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;
    m_host = &Context.Host();
    RebindViewportHandler(Context);
    UpdatePieMouseCaptureState(Context);
    QueueSelectedNodeEditorOverlay(Context);
}

void EditorSelectionInteractionService::Shutdown(EditorServiceContext& Context)
{
    SetPieMouseCapture(Context, false);
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
    m_pieMouseCaptureEnabled = false;
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

void EditorSelectionInteractionService::UpdatePieMouseCaptureState(EditorServiceContext& Context)
{
    auto* PieService = Context.GetService<EditorPieService>();
    const bool PieActive = PieService && PieService->IsSessionActive();
    if (!PieActive)
    {
        if (m_pieMouseCaptureEnabled)
        {
            SetPieMouseCapture(Context, false);
        }
        return;
    }

#if defined(SNAPI_GF_ENABLE_INPUT)
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr || !WorldPtr->Input().IsInitialized())
    {
        return;
    }

    const auto* Snapshot = WorldPtr->Input().Snapshot();
    if (!Snapshot)
    {
        return;
    }

    if (!Snapshot->IsWindowFocused() ||
        Snapshot->KeyPressed(SnAPI::Input::EKey::Escape))
    {
        SetPieMouseCapture(Context, false);
    }
#else
    (void)Context;
#endif
}

void EditorSelectionInteractionService::QueueSelectedNodeEditorOverlay(EditorServiceContext& Context) const
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(WITH_EDITOR) || !WITH_EDITOR
    (void)Context;
    return;
#else
    if (auto* PieService = Context.GetService<EditorPieService>(); PieService && PieService->IsSessionActive())
    {
        return;
    }

    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return;
    }

    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* LayoutService = Context.GetService<EditorLayoutService>();
    if (!SelectionService || !LayoutService)
    {
        return;
    }

    BaseNode* SelectedNode = SelectionService->Model().ResolveSelectedNode(*WorldPtr);
    if (!SelectedNode || SelectedNode->EditorTransient())
    {
        return;
    }

    auto* Viewport = LayoutService->GameViewportElement();
    if (!Viewport)
    {
        return;
    }

    const std::uint64_t ViewportID = Viewport->OwnedViewportId();
    if (ViewportID == 0)
    {
        return;
    }

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.IsInitialized())
    {
        return;
    }

    if (auto StaticMeshResult = SelectedNode->Component<StaticMeshComponent>();
        StaticMeshResult && StaticMeshResult->RenderObject())
    {
        (void)Renderer.QueueEditorImmediateRenderObject(StaticMeshResult->RenderObject(),
                                                        ViewportID,
                                                        SnAPI::Graphics::ERenderPassType::EditorOverlay);
    }

    if (auto SkeletalMeshResult = SelectedNode->Component<SkeletalMeshComponent>();
        SkeletalMeshResult && SkeletalMeshResult->RenderObject())
    {
        (void)Renderer.QueueEditorImmediateRenderObject(SkeletalMeshResult->RenderObject(),
                                                        ViewportID,
                                                        SnAPI::Graphics::ERenderPassType::EditorOverlay);
    }
#endif
}

void EditorSelectionInteractionService::SetPieMouseCapture(EditorServiceContext& Context, const bool CaptureEnabled)
{
#if !defined(SNAPI_GF_ENABLE_RENDERER) || !SNAPI_GF_EDITOR_HAS_SDL3
    (void)Context;
    m_pieMouseCaptureEnabled = false;
    (void)CaptureEnabled;
    return;
#else
    if (m_pieMouseCaptureEnabled == CaptureEnabled)
    {
        return;
    }

    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr || !WorldPtr->Renderer().IsInitialized())
    {
        m_pieMouseCaptureEnabled = false;
        return;
    }

    auto* Window = WorldPtr->Renderer().Window();
    if (!Window)
    {
        m_pieMouseCaptureEnabled = false;
        return;
    }

    auto* NativeWindow = reinterpret_cast<SDL_Window*>(Window->Handle());
    if (!NativeWindow)
    {
        m_pieMouseCaptureEnabled = false;
        return;
    }

    if (CaptureEnabled)
    {
        const bool RelativeEnabled = SDL_SetWindowRelativeMouseMode(NativeWindow, true);
        const bool GrabEnabled = SDL_SetWindowMouseGrab(NativeWindow, true);
        if (RelativeEnabled && GrabEnabled)
        {
            (void)SDL_CaptureMouse(true);
            (void)SDL_HideCursor();
            m_pieMouseCaptureEnabled = true;
            return;
        }
    }

    (void)SDL_SetWindowRelativeMouseMode(NativeWindow, false);
    (void)SDL_SetWindowMouseGrab(NativeWindow, false);
    (void)SDL_CaptureMouse(false);
    (void)SDL_ShowCursor();
    m_pieMouseCaptureEnabled = false;
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
    auto* PieService = Context.GetService<EditorPieService>();
    if (PieService && PieService->IsSessionActive())
    {
        if (RoutedTypeId == SnAPI::UI::RoutedEventTypes::PointerDown.Id &&
            ContainsPointer &&
            (Event.LeftDown || Event.RightDown || Event.MiddleDown))
        {
            SetPieMouseCapture(Context, true);
        }
        m_pointerPressedInside = false;
        m_pointerDragged = false;
        return;
    }

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
    const bool ResolvedNode = TryResolvePickedNode(Context, Event.Position, Next);
    if (!ResolvedNode)
    {
#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(WITH_EDITOR) && WITH_EDITOR
        auto* WorldPtr = Context.Runtime().WorldPtr();
        auto* LayoutService = Context.GetService<EditorLayoutService>();
        auto* Viewport = LayoutService ? LayoutService->GameViewportElement() : nullptr;
        if (WorldPtr && Viewport && WorldPtr->Renderer().IsInitialized())
        {
            const SnAPI::UI::UIRect ViewRect = Viewport->LayoutRect();
            if (ViewRect.W > 0.0f && ViewRect.H > 0.0f && ViewRect.Contains(Event.Position))
            {
                const float U = (Event.Position.X - ViewRect.X) / ViewRect.W;
                const float V = (Event.Position.Y - ViewRect.Y) / ViewRect.H;
                if (std::isfinite(U) && std::isfinite(V))
                {
                    const auto HitRenderObjectID =
                        WorldPtr->Renderer().ReadRenderViewportObjectID(Viewport->OwnedViewportId(), U, V);
                    if (HitRenderObjectID.has_value() && *HitRenderObjectID != 0u)
                    {
                        return;
                    }
                }
            }
        }
#endif
    }

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
        return TryResolvePickedNodeRendererId(Context, ScreenPoint, OutNode);
    case EEditorPickingBackend::Auto:
    default:
        if (TryResolvePickedNodeRendererId(Context, ScreenPoint, OutNode))
        {
            return true;
        }
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
    if (auto Resolved = ResolveNodeHandleByPhysicsBody(*WorldPtr, HitBody))
    {
        OutNode = *Resolved;
        return true;
    }

    return false;
#endif
}

bool EditorSelectionInteractionService::TryResolvePickedNodeRendererId(EditorServiceContext& Context,
                                                                       const SnAPI::UI::UIPoint& ScreenPoint,
                                                                       NodeHandle& OutNode) const
{
    OutNode = {};

#if !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(WITH_EDITOR) || !WITH_EDITOR
    (void)Context;
    (void)ScreenPoint;
    return false;
#else
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return false;
    }

    auto* LayoutService = Context.GetService<EditorLayoutService>();
    if (!LayoutService)
    {
        return false;
    }

    auto* Viewport = LayoutService->GameViewportElement();
    if (!Viewport)
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

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.IsInitialized())
    {
        return false;
    }

    const std::uint64_t ViewportID = Viewport->OwnedViewportId();
    if (ViewportID == 0)
    {
        return false;
    }

    const auto RenderObjectID = Renderer.ReadRenderViewportObjectID(ViewportID, U, V);
    if (!RenderObjectID.has_value() || *RenderObjectID == 0)
    {
        return false;
    }

    const auto RenderObject = Renderer.ResolveRenderObjectByID(*RenderObjectID);
    if (!RenderObject)
    {
        return false;
    }

    if (auto Resolved = ResolveNodeHandleByRenderObject(*WorldPtr, RenderObject.get()))
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


} // namespace SnAPI::GameFramework::Editor
