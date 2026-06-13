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
#include "StaticMeshComponent.h"
#include "SkeletalMeshComponent.h"
#include "TransformComponent.h"
#include "UIRenderViewport.h"
#include "World.h"

#include "Rendering/GameRenderCamera.h"
#include "Rendering/GameRenderMesh.h"
#include "Rendering/GameRenderObject.h"

#include <UIContext.h>
#include <UIEvents.h>

#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "RigidBodyComponent.h"
#endif


#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

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
struct RendererNewPickRay
{
    Vec3 Origin{Vec3::Zero()};
    Vec3 Direction{Vec3{0.0, 0.0, -1.0}};
};

[[nodiscard]] bool IsFiniteVec3(const Vec3& Value)
{
    return std::isfinite(Value.x()) && std::isfinite(Value.y()) && std::isfinite(Value.z());
}

[[nodiscard]] Vec3 NormalizeOrAxis(const Vec3& Value, const Vec3& FallbackAxis)
{
    const auto LengthSquared = Value.squaredNorm();
    if (!(LengthSquared > static_cast<Vec3::Scalar>(1.0e-10)))
    {
        return FallbackAxis;
    }
    return Value / std::sqrt(LengthSquared);
}

[[nodiscard]] bool TryBuildRendererNewViewportRay(const GameRenderCamera& Camera,
                                                  const SnAPI::UI::UIRect& ViewRect,
                                                  const SnAPI::UI::UIPoint& ScreenPoint,
                                                  RendererNewPickRay& OutRay)
{
    if (!Camera.Valid() || ViewRect.W <= 0.0f || ViewRect.H <= 0.0f || !ViewRect.Contains(ScreenPoint))
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
        static_cast<double>(Camera.FovDegrees()) * (std::numbers::pi_v<double> / 180.0));
    const SnAPI::Math::Scalar TanHalfFov = static_cast<SnAPI::Math::Scalar>(
        std::tan(static_cast<double>(FovRadians) * 0.5));
    const SnAPI::Math::Scalar Aspect = static_cast<SnAPI::Math::Scalar>(Camera.Aspect());
    if (!std::isfinite(TanHalfFov) || !std::isfinite(Aspect)
        || !(TanHalfFov > static_cast<SnAPI::Math::Scalar>(0.0))
        || !(Aspect > static_cast<SnAPI::Math::Scalar>(0.0)))
    {
        return false;
    }

    const Vec3 Forward = NormalizeOrAxis(Camera.Forward(), Vec3{0.0, 0.0, -1.0});
    const Vec3 Right = NormalizeOrAxis(Camera.Right(), Vec3::UnitX());
    const Vec3 Up = NormalizeOrAxis(Camera.Up(), Vec3::UnitY());
    Vec3 RayDirection = Forward
        + (Right * (NormalizedX * Aspect * TanHalfFov))
        + (Up * (NormalizedY * TanHalfFov));
    RayDirection = NormalizeOrAxis(RayDirection, Forward);

    const Vec3 RayOrigin = Camera.Position()
        + (RayDirection * std::max(static_cast<SnAPI::Math::Scalar>(Camera.NearClip()),
                                   static_cast<SnAPI::Math::Scalar>(0.001)));
    if (!IsFiniteVec3(RayOrigin) || !IsFiniteVec3(RayDirection))
    {
        return false;
    }

    OutRay.Origin = RayOrigin;
    OutRay.Direction = RayDirection;
    return true;
}

[[nodiscard]] Vec3 TransformPoint(const SnAPI::Math::Matrix4& WorldFromLocal, const Vec3& LocalPoint)
{
    const Vec4 HomogeneousLocal{
        static_cast<Vec4::Scalar>(LocalPoint.x()),
        static_cast<Vec4::Scalar>(LocalPoint.y()),
        static_cast<Vec4::Scalar>(LocalPoint.z()),
        static_cast<Vec4::Scalar>(1.0)};
    const Vec4 HomogeneousWorld = WorldFromLocal * HomogeneousLocal;
    return Vec3{
        static_cast<Vec3::Scalar>(HomogeneousWorld.x()),
        static_cast<Vec3::Scalar>(HomogeneousWorld.y()),
        static_cast<Vec3::Scalar>(HomogeneousWorld.z())};
}

[[nodiscard]] double MaxAxisScale(const SnAPI::Math::Matrix4& WorldFromLocal)
{
    const Vec3 AxisX{
        static_cast<Vec3::Scalar>(WorldFromLocal(0, 0)),
        static_cast<Vec3::Scalar>(WorldFromLocal(1, 0)),
        static_cast<Vec3::Scalar>(WorldFromLocal(2, 0))};
    const Vec3 AxisY{
        static_cast<Vec3::Scalar>(WorldFromLocal(0, 1)),
        static_cast<Vec3::Scalar>(WorldFromLocal(1, 1)),
        static_cast<Vec3::Scalar>(WorldFromLocal(2, 1))};
    const Vec3 AxisZ{
        static_cast<Vec3::Scalar>(WorldFromLocal(0, 2)),
        static_cast<Vec3::Scalar>(WorldFromLocal(1, 2)),
        static_cast<Vec3::Scalar>(WorldFromLocal(2, 2))};
    return std::max({static_cast<double>(AxisX.norm()),
                     static_cast<double>(AxisY.norm()),
                     static_cast<double>(AxisZ.norm()),
                     1.0e-5});
}

[[nodiscard]] bool TryIntersectRendererNewObjectBounds(const GameRenderMesh& Mesh,
                                                       const GameRenderObject& Object,
                                                       const RendererNewPickRay& Ray,
                                                       double& OutDistance)
{
    if (!Mesh.Valid() || !Mesh.HasLocalBounds() || !Object.Valid() || !Object.Visible())
    {
        return false;
    }

    const Vec3 Center = TransformPoint(Object.WorldFromLocal(), Mesh.LocalBoundsCenter());
    const double Radius = std::max(Mesh.LocalBoundsRadius() * MaxAxisScale(Object.WorldFromLocal()), 1.0e-5);
    if (!IsFiniteVec3(Center) || !std::isfinite(Radius) || !(Radius > 0.0))
    {
        return false;
    }

    const Vec3 Offset = Ray.Origin - Center;
    const double B = static_cast<double>(Offset.dot(Ray.Direction));
    const double C = static_cast<double>(Offset.dot(Offset)) - (Radius * Radius);
    const double Discriminant = (B * B) - C;
    if (Discriminant < 0.0)
    {
        return false;
    }

    const double SqrtDiscriminant = std::sqrt(Discriminant);
    double Distance = -B - SqrtDiscriminant;
    if (Distance < 0.0)
    {
        Distance = -B + SqrtDiscriminant;
    }
    if (!std::isfinite(Distance) || Distance < 0.0)
    {
        return false;
    }

    OutDistance = Distance;
    return true;
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
    (void)Context;
    return;
}

void EditorSelectionInteractionService::SetPieMouseCapture(EditorServiceContext& Context, const bool CaptureEnabled)
{
    (void)Context;
    m_pieMouseCaptureEnabled = false;
    (void)CaptureEnabled;
    return;
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
        EditorAssetService::AssetPlacementRequest PlacementRequest{};
        PlacementRequest.UseScreenPoint = true;
        PlacementRequest.ScreenPositionX = Event.Position.X;
        PlacementRequest.ScreenPositionY = Event.Position.Y;
        if (const Result InstantiateResult = AssetService->InstantiateArmedAsset(Context, PlacementRequest); InstantiateResult)
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

    (void)Context;
    (void)ScreenPoint;
    return false;
}

bool EditorSelectionInteractionService::TryResolvePickedNodeRendererId(EditorServiceContext& Context,
                                                                       const SnAPI::UI::UIPoint& ScreenPoint,
                                                                       NodeHandle& OutNode) const
{
    OutNode = {};

#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(WITH_EDITOR) && WITH_EDITOR
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return false;
    }

    auto* LayoutService = Context.GetService<EditorLayoutService>();
    auto* SceneService = Context.GetService<EditorSceneService>();
    if (!LayoutService || !SceneService)
    {
        return false;
    }

    auto* Viewport = LayoutService->GameViewportElement();
    auto* Camera = SceneService->ActiveRenderCamera();
    if (!Viewport || !Camera)
    {
        return false;
    }

    RendererNewPickRay Ray{};
    if (!TryBuildRendererNewViewportRay(*Camera, Viewport->LayoutRect(), ScreenPoint, Ray))
    {
        return false;
    }

    double BestDistance = std::numeric_limits<double>::max();
    NodeHandle BestNode{};
    WorldPtr->ForEachNode([&](const NodeHandle& Handle, BaseNode& Node) {
        if (Node.EditorTransient())
        {
            return;
        }

        const auto TryCandidate = [&](const GameRenderMesh& Mesh, const GameRenderObject& Object) {
            double Distance = 0.0;
            if (!TryIntersectRendererNewObjectBounds(Mesh, Object, Ray, Distance) || Distance >= BestDistance)
            {
                return;
            }

            const NodeHandle Owner = Object.OwnerNode();
            BestNode = Owner.IsNull() ? Handle : Owner;
            BestDistance = Distance;
        };

        if (auto StaticMeshResult = Node.Component<StaticMeshComponent>(); StaticMeshResult)
        {
            TryCandidate(StaticMeshResult->RenderMesh(), StaticMeshResult->RenderObject());
        }
        if (auto SkeletalMeshResult = Node.Component<SkeletalMeshComponent>(); SkeletalMeshResult)
        {
            TryCandidate(SkeletalMeshResult->RenderMesh(), SkeletalMeshResult->RenderObject());
        }
    });

    if (BestNode.IsNull())
    {
        return false;
    }

    OutNode = BestNode;
    return true;
#else
    (void)Context;
    (void)ScreenPoint;
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
