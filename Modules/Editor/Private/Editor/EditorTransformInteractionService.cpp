#include "Editor/EditorTransformInteractionService.h"

#include "BaseNode.h"
#include "CameraComponent.h"
#include "Editor/EditorCameraComponent.h"
#include "Editor/EditorLayoutService.h"
#include "Editor/EditorPieService.h"
#include "Editor/EditorSceneService.h"
#include "Editor/EditorSelectionService.h"
#include "GameRuntime.h"
#include "InputSystem.h"
#include "RendererSystem.h"
#include "SkeletalMeshComponent.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"
#include "UIRenderViewport.h"
#include "World.h"

#include "Rendering/GameRenderCamera.h"

#include <UIEvents.h>
#include <UIContext.h>
#include <UIElementBase.h>

#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
#endif


#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string_view>
#include <utility>
#include <vector>

namespace SnAPI::GameFramework::Editor
{
namespace
{
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

[[nodiscard]] SnAPI::Math::Scalar SnapValueToStep(const SnAPI::Math::Scalar Value, const SnAPI::Math::Scalar Step)
{
    if (!(Step > static_cast<SnAPI::Math::Scalar>(0.0)) || !std::isfinite(Value) || !std::isfinite(Step))
    {
        return Value;
    }
    return std::round(Value / Step) * Step;
}

[[nodiscard]] SnAPI::Math::Scalar ConsumeSnapRemainder(const SnAPI::Math::Scalar Delta,
                                                       const SnAPI::Math::Scalar Step,
                                                       SnAPI::Math::Scalar& InOutRemainder)
{
    if (!(Step > static_cast<SnAPI::Math::Scalar>(0.0)))
    {
        return Delta;
    }

    InOutRemainder += Delta;
    const auto StepCount = static_cast<long long>(std::trunc(InOutRemainder / Step));
    if (StepCount == 0)
    {
        return static_cast<SnAPI::Math::Scalar>(0.0);
    }

    const SnAPI::Math::Scalar Quantized = static_cast<SnAPI::Math::Scalar>(StepCount) * Step;
    InOutRemainder -= Quantized;
    return Quantized;
}


[[nodiscard]] bool TryBuildViewportRay(const GameRenderCamera& Camera,
                                       const SnAPI::UI::UIRect& ViewRect,
                                       const float ScreenX,
                                       const float ScreenY,
                                       Vec3& OutRayOrigin,
                                       Vec3& OutRayDirection)
{
    if (!Camera.Valid() || ViewRect.W <= 0.0f || ViewRect.H <= 0.0f
        || !std::isfinite(ScreenX) || !std::isfinite(ScreenY))
    {
        return false;
    }

    const float U = (ScreenX - ViewRect.X) / ViewRect.W;
    const float V = (ScreenY - ViewRect.Y) / ViewRect.H;
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

    const Vec3 Forward = NormalizeOrAxis(Camera.Forward(), Vec3::UnitZ());
    const Vec3 Right = NormalizeOrAxis(Camera.Right(), Vec3::UnitX());
    const Vec3 Up = NormalizeOrAxis(Camera.Up(), Vec3::UnitY());

    Vec3 RayDirection = Forward +
        (Right * (NormalizedX * Aspect * TanHalfFov)) +
        (Up * (NormalizedY * TanHalfFov));
    const auto DirectionLengthSquared = RayDirection.squaredNorm();
    if (!(DirectionLengthSquared > static_cast<SnAPI::Math::Scalar>(1.0e-10)))
    {
        return false;
    }

    RayDirection /= std::sqrt(DirectionLengthSquared);
    const SnAPI::Math::Scalar NearClip =
        std::max(static_cast<SnAPI::Math::Scalar>(Camera.NearClip()), static_cast<SnAPI::Math::Scalar>(0.001));
    const Vec3 RayOrigin = Camera.Position() + (RayDirection * NearClip);
    if (!IsFiniteVec3(RayOrigin) || !IsFiniteVec3(RayDirection))
    {
        return false;
    }

    OutRayOrigin = RayOrigin;
    OutRayDirection = RayDirection;
    return true;
}

[[nodiscard]] bool TryIntersectRayPlane(const Vec3& RayOrigin,
                                        const Vec3& RayDirection,
                                        const Vec3& PlanePoint,
                                        const Vec3& PlaneNormal,
                                        Vec3& OutIntersection)
{
    const Vec3 Normal = NormalizeOrAxis(PlaneNormal, Vec3::UnitZ());
    const SnAPI::Math::Scalar Denominator = RayDirection.dot(Normal);
    if (!std::isfinite(Denominator)
        || std::fabs(Denominator) <= static_cast<SnAPI::Math::Scalar>(1.0e-6))
    {
        return false;
    }

    SnAPI::Math::Scalar T = (PlanePoint - RayOrigin).dot(Normal) / Denominator;
    if (!std::isfinite(T))
    {
        return false;
    }

    if (T < static_cast<SnAPI::Math::Scalar>(0.0))
    {
        T = static_cast<SnAPI::Math::Scalar>(0.0);
    }

    const Vec3 Intersection = RayOrigin + (RayDirection * T);
    if (!IsFiniteVec3(Intersection))
    {
        return false;
    }

    OutIntersection = Intersection;
    return true;
}

[[nodiscard]] Vec3 ResolveAxisMovePlaneNormal(const Vec3& AxisDirection,
                                              const GameRenderCamera& Camera)
{
    const Vec3 Axis = NormalizeOrAxis(AxisDirection, Vec3::UnitX());
    const Vec3 CameraForward = NormalizeOrAxis(Camera.Forward(), Vec3::UnitZ());
    const Vec3 CameraRight = NormalizeOrAxis(Camera.Right(), Vec3::UnitX());
    const Vec3 CameraUp = NormalizeOrAxis(Camera.Up(), Vec3::UnitY());

    const auto RejectAxis = [&Axis](const Vec3& Value) -> Vec3
    {
        return Value - (Axis * Value.dot(Axis));
    };

    Vec3 PlaneNormal = RejectAxis(CameraForward);
    if (!(PlaneNormal.squaredNorm() > static_cast<SnAPI::Math::Scalar>(1.0e-8)))
    {
        PlaneNormal = RejectAxis(CameraRight);
    }
    if (!(PlaneNormal.squaredNorm() > static_cast<SnAPI::Math::Scalar>(1.0e-8)))
    {
        PlaneNormal = RejectAxis(CameraUp);
    }
    if (!(PlaneNormal.squaredNorm() > static_cast<SnAPI::Math::Scalar>(1.0e-8)))
    {
        const Vec3 Seed = std::fabs(Axis.dot(Vec3::UnitY())) < static_cast<SnAPI::Math::Scalar>(0.95)
            ? Vec3::UnitY()
            : Vec3::UnitX();
        PlaneNormal = RejectAxis(Seed);
    }

    return NormalizeOrAxis(PlaneNormal, Vec3::UnitZ());
}

void ResolveNativeGizmoFrame(const NodeTransform& SelectedTransform,
                             const GameRenderCamera& Camera,
                             const EditorLayout::EGizmoSpace Space,
                             std::array<Vec3, 3>& OutAxisBasis,
                             SnAPI::Math::Scalar& OutAxisLength,
                             SnAPI::Math::Scalar& OutAxisThickness,
                             SnAPI::Math::Scalar& OutRingRadius)
{
    Vec3 BasisX = Vec3::UnitX();
    Vec3 BasisY = Vec3::UnitY();
    Vec3 BasisZ = Vec3::UnitZ();
    switch (Space)
    {
    case EditorLayout::EGizmoSpace::Object:
        {
            const auto RotationMatrix = SelectedTransform.Rotation.toRotationMatrix();
            BasisX = NormalizeOrAxis(RotationMatrix * Vec3::UnitX(), Vec3::UnitX());
            BasisY = NormalizeOrAxis(RotationMatrix * Vec3::UnitY(), Vec3::UnitY());
            BasisZ = NormalizeOrAxis(RotationMatrix * Vec3::UnitZ(), Vec3::UnitZ());
        }
        break;
    case EditorLayout::EGizmoSpace::Camera:
        BasisX = NormalizeOrAxis(Camera.Right(), Vec3::UnitX());
        BasisY = NormalizeOrAxis(Camera.Up(), Vec3::UnitY());
        BasisZ = NormalizeOrAxis(Camera.Forward(), Vec3::UnitZ());
        break;
    case EditorLayout::EGizmoSpace::World:
    default:
        break;
    }

    OutAxisBasis = {
        NormalizeOrAxis(BasisX, Vec3::UnitX()),
        NormalizeOrAxis(BasisY, Vec3::UnitY()),
        NormalizeOrAxis(BasisZ, Vec3::UnitZ())};

    const SnAPI::Math::Scalar CameraDistance = std::max<SnAPI::Math::Scalar>(
        static_cast<SnAPI::Math::Scalar>(0.25),
        (SelectedTransform.Position - Camera.Position()).norm());
    const SnAPI::Math::Scalar GizmoScaleMultiplier = static_cast<SnAPI::Math::Scalar>(3.0);
    OutAxisLength = std::max<SnAPI::Math::Scalar>(
        CameraDistance * static_cast<SnAPI::Math::Scalar>(0.08) * GizmoScaleMultiplier,
        static_cast<SnAPI::Math::Scalar>(0.12) * GizmoScaleMultiplier);
    OutAxisThickness = std::max<SnAPI::Math::Scalar>(
        static_cast<SnAPI::Math::Scalar>(0.016) * GizmoScaleMultiplier,
        OutAxisLength * static_cast<SnAPI::Math::Scalar>(0.085));
    OutRingRadius = std::max<SnAPI::Math::Scalar>(
        OutAxisLength * static_cast<SnAPI::Math::Scalar>(0.9),
        CameraDistance * static_cast<SnAPI::Math::Scalar>(0.14) * GizmoScaleMultiplier);
}

[[nodiscard]] SnAPI::Math::Scalar RaySegmentDistanceSquared(const Vec3& RayOrigin,
                                                            const Vec3& RayDirection,
                                                            const Vec3& SegmentStart,
                                                            const Vec3& SegmentEnd,
                                                            SnAPI::Math::Scalar& OutRayT)
{
    const Vec3 Segment = SegmentEnd - SegmentStart;
    const Vec3 Offset = RayOrigin - SegmentStart;
    const SnAPI::Math::Scalar A = RayDirection.dot(RayDirection);
    const SnAPI::Math::Scalar B = RayDirection.dot(Segment);
    const SnAPI::Math::Scalar C = Segment.dot(Segment);
    const SnAPI::Math::Scalar D = RayDirection.dot(Offset);
    const SnAPI::Math::Scalar E = Segment.dot(Offset);
    const SnAPI::Math::Scalar Denominator = (A * C) - (B * B);

    SnAPI::Math::Scalar RayT = static_cast<SnAPI::Math::Scalar>(0.0);
    SnAPI::Math::Scalar SegmentT = static_cast<SnAPI::Math::Scalar>(0.0);
    if (std::fabs(Denominator) > static_cast<SnAPI::Math::Scalar>(1.0e-8))
    {
        RayT = ((B * E) - (C * D)) / Denominator;
        SegmentT = ((A * E) - (B * D)) / Denominator;
    }
    else if (C > static_cast<SnAPI::Math::Scalar>(1.0e-8))
    {
        SegmentT = E / C;
    }

    RayT = std::max<SnAPI::Math::Scalar>(RayT, static_cast<SnAPI::Math::Scalar>(0.0));
    SegmentT = std::clamp(
        SegmentT,
        static_cast<SnAPI::Math::Scalar>(0.0),
        static_cast<SnAPI::Math::Scalar>(1.0));

    const Vec3 RayPoint = RayOrigin + (RayDirection * RayT);
    const Vec3 SegmentPoint = SegmentStart + (Segment * SegmentT);
    OutRayT = RayT;
    return (RayPoint - SegmentPoint).squaredNorm();
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


} // namespace

std::string_view EditorTransformInteractionService::Name() const
{
    return "EditorTransformInteractionService";
}

std::vector<std::type_index> EditorTransformInteractionService::Dependencies() const
{
    return {std::type_index(typeid(EditorSceneService)),
            std::type_index(typeid(EditorSelectionService)),
            std::type_index(typeid(EditorPieService)),
            std::type_index(typeid(EditorLayoutService))};
}

void EditorTransformInteractionService::UpdateNativeTransformGizmos(EditorServiceContext& Context,
                                                                    BaseNode* SelectedNode,
                                                                    const NodeTransform& SelectedTransform,
                                                                    const GameRenderCamera& Camera,
                                                                    const std::uint64_t ViewportID)
{
    if (ViewportID == 0)
    {
        return;
    }

    auto* NativeWorldPtr = Context.Runtime().WorldPtr();
    if (!NativeWorldPtr)
    {
        return;
    }

    auto& NativeRenderer = NativeWorldPtr->Renderer();
    if (!NativeRenderer.IsInitialized())
    {
        return;
    }

    std::array<Vec3, 3> NativeAxisBasis{};
    SnAPI::Math::Scalar NativeAxisLength = static_cast<SnAPI::Math::Scalar>(0.0);
    SnAPI::Math::Scalar NativeAxisThickness = static_cast<SnAPI::Math::Scalar>(0.0);
    SnAPI::Math::Scalar NativeRingRadius = static_cast<SnAPI::Math::Scalar>(0.0);
    ResolveNativeGizmoFrame(
        SelectedTransform,
        Camera,
        m_space,
        NativeAxisBasis,
        NativeAxisLength,
        NativeAxisThickness,
        NativeRingRadius);

    const auto NativeAxisColor = [this](const EActiveAxis Axis) -> std::array<float, 4>
    {
        if (Axis == m_activeAxis)
        {
            return {1.0f, 0.88f, 0.14f, 1.0f};
        }

        switch (Axis)
        {
        case EActiveAxis::X:
            return {1.0f, 0.16f, 0.10f, 1.0f};
        case EActiveAxis::Y:
            return {0.14f, 0.92f, 0.26f, 1.0f};
        case EActiveAxis::Z:
            return {0.20f, 0.42f, 1.0f, 1.0f};
        case EActiveAxis::None:
        default:
            return {1.0f, 1.0f, 1.0f, 1.0f};
        }
    };

    const auto QueueNativeLine = [&](const Vec3& Start,
                                     const Vec3& End,
                                     const std::array<float, 4>& Color,
                                     const float Thickness)
    {
        (void)NativeRenderer.QueueDebugLine(GameRenderDebugLine{
            .StartWorld = Start,
            .EndWorld = End,
            .ColorLinear = Color,
            .ThicknessPixels = Thickness,
            .DepthTest = false});
    };

    const auto QueueNativeAxis = [&](const EActiveAxis Axis, const Vec3& Direction)
    {
        const Vec3 UnitDirection = NormalizeOrAxis(Direction, Vec3::UnitY());
        const std::array<float, 4> Color = NativeAxisColor(Axis);
        const float PrimaryThickness = m_mode == EEditorTransformMode::Scale ? 5.0f : 4.0f;
        const Vec3 Start = SelectedTransform.Position;
        const Vec3 End = SelectedTransform.Position
            + (UnitDirection * NativeAxisLength * static_cast<SnAPI::Math::Scalar>(1.35));
        QueueNativeLine(Start, End, Color, PrimaryThickness);

        const Vec3 CameraRight = NormalizeOrAxis(Camera.Right(), Vec3::UnitX());
        const Vec3 CameraUp = NormalizeOrAxis(Camera.Up(), Vec3::UnitY());
        Vec3 HeadSide = UnitDirection.cross(CameraRight);
        if (!(HeadSide.squaredNorm() > static_cast<SnAPI::Math::Scalar>(1.0e-8)))
        {
            HeadSide = UnitDirection.cross(CameraUp);
        }
        HeadSide = NormalizeOrAxis(HeadSide, Vec3::UnitZ());

        const SnAPI::Math::Scalar HeadLength = std::max<SnAPI::Math::Scalar>(
            NativeAxisLength * static_cast<SnAPI::Math::Scalar>(0.18),
            NativeAxisThickness * static_cast<SnAPI::Math::Scalar>(2.5));
        const SnAPI::Math::Scalar HeadSpread = std::max<SnAPI::Math::Scalar>(
            NativeAxisLength * static_cast<SnAPI::Math::Scalar>(0.06),
            NativeAxisThickness * static_cast<SnAPI::Math::Scalar>(1.6));
        const Vec3 HeadBase = End - (UnitDirection * HeadLength);
        QueueNativeLine(End, HeadBase + (HeadSide * HeadSpread), Color, PrimaryThickness);
        QueueNativeLine(End, HeadBase - (HeadSide * HeadSpread), Color, PrimaryThickness);

        if (m_mode == EEditorTransformMode::Scale)
        {
            const SnAPI::Math::Scalar Tick = std::max<SnAPI::Math::Scalar>(
                HeadSpread,
                NativeAxisLength * static_cast<SnAPI::Math::Scalar>(0.08));
            QueueNativeLine(End - (HeadSide * Tick), End + (HeadSide * Tick), Color, PrimaryThickness);
        }
    };

    if (m_mode == EEditorTransformMode::Rotate)
    {
        const std::array<std::pair<EActiveAxis, std::pair<Vec3, Vec3>>, 3> Rings{
            std::pair{EActiveAxis::X, std::pair{NativeAxisBasis[1], NativeAxisBasis[2]}},
            std::pair{EActiveAxis::Y, std::pair{NativeAxisBasis[2], NativeAxisBasis[0]}},
            std::pair{EActiveAxis::Z, std::pair{NativeAxisBasis[0], NativeAxisBasis[1]}}};
        constexpr std::size_t SegmentCount = 72u;
        for (const auto& Ring : Rings)
        {
            const std::array<float, 4> Color = NativeAxisColor(Ring.first);
            const Vec3 U = NormalizeOrAxis(Ring.second.first, Vec3::UnitX());
            const Vec3 V = NormalizeOrAxis(Ring.second.second, Vec3::UnitY());
            for (std::size_t Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const double T0 = static_cast<double>(Segment) / static_cast<double>(SegmentCount);
                const double T1 = static_cast<double>(Segment + 1u) / static_cast<double>(SegmentCount);
                const double A0 = T0 * 2.0 * std::numbers::pi_v<double>;
                const double A1 = T1 * 2.0 * std::numbers::pi_v<double>;
                const Vec3 P0 = SelectedTransform.Position
                    + (U * static_cast<SnAPI::Math::Scalar>(std::cos(A0) * static_cast<double>(NativeRingRadius)))
                    + (V * static_cast<SnAPI::Math::Scalar>(std::sin(A0) * static_cast<double>(NativeRingRadius)));
                const Vec3 P1 = SelectedTransform.Position
                    + (U * static_cast<SnAPI::Math::Scalar>(std::cos(A1) * static_cast<double>(NativeRingRadius)))
                    + (V * static_cast<SnAPI::Math::Scalar>(std::sin(A1) * static_cast<double>(NativeRingRadius)));
                QueueNativeLine(P0, P1, Color, 3.0f);
            }
        }
    }
    else
    {
        QueueNativeAxis(EActiveAxis::X, NativeAxisBasis[0]);
        QueueNativeAxis(EActiveAxis::Y, NativeAxisBasis[1]);
        QueueNativeAxis(EActiveAxis::Z, NativeAxisBasis[2]);
    }

    (void)Context;
    (void)SelectedNode;
}

EditorTransformInteractionService::EActiveAxis EditorTransformInteractionService::PickNativeGizmoAxis(
    EditorServiceContext& Context,
    const NodeTransform& SelectedTransform,
    const GameRenderCamera& Camera,
    const float ScreenX,
    const float ScreenY,
    const SnAPI::UI::UIRect& ViewRect,
    const std::uint64_t ViewportID) const
{
    (void)Context;

    if (ViewportID == 0 || ViewRect.W <= 0.0f || ViewRect.H <= 0.0f
        || !std::isfinite(ScreenX) || !std::isfinite(ScreenY))
    {
        return EActiveAxis::None;
    }

    Vec3 RayOrigin = Vec3::Zero();
    Vec3 RayDirection = Vec3::Zero();
    if (!TryBuildViewportRay(Camera, ViewRect, ScreenX, ScreenY, RayOrigin, RayDirection))
    {
        return EActiveAxis::None;
    }

    std::array<Vec3, 3> AxisBasis{};
    SnAPI::Math::Scalar AxisLength = static_cast<SnAPI::Math::Scalar>(0.0);
    SnAPI::Math::Scalar AxisThickness = static_cast<SnAPI::Math::Scalar>(0.0);
    SnAPI::Math::Scalar RingRadius = static_cast<SnAPI::Math::Scalar>(0.0);
    ResolveNativeGizmoFrame(SelectedTransform, Camera, m_space, AxisBasis, AxisLength, AxisThickness, RingRadius);

    EActiveAxis BestAxis = EActiveAxis::None;
    SnAPI::Math::Scalar BestRayT = std::numeric_limits<SnAPI::Math::Scalar>::max();
    SnAPI::Math::Scalar BestScore = std::numeric_limits<SnAPI::Math::Scalar>::max();
    const auto Consider = [&](const EActiveAxis Axis,
                              const SnAPI::Math::Scalar RayT,
                              const SnAPI::Math::Scalar Score)
    {
        if (!std::isfinite(RayT) || !std::isfinite(Score))
        {
            return;
        }

        if (RayT < BestRayT || (std::fabs(RayT - BestRayT) <= static_cast<SnAPI::Math::Scalar>(1.0e-5) && Score < BestScore))
        {
            BestRayT = RayT;
            BestScore = Score;
            BestAxis = Axis;
        }
    };

    if (m_mode == EEditorTransformMode::Rotate)
    {
        const SnAPI::Math::Scalar PickTolerance = std::max<SnAPI::Math::Scalar>(
            RingRadius * static_cast<SnAPI::Math::Scalar>(0.16),
            AxisThickness * static_cast<SnAPI::Math::Scalar>(2.5));
        for (std::size_t AxisIndex = 0; AxisIndex < AxisBasis.size(); ++AxisIndex)
        {
            Vec3 HitPoint = Vec3::Zero();
            if (!TryIntersectRayPlane(
                    RayOrigin,
                    RayDirection,
                    SelectedTransform.Position,
                    AxisBasis[AxisIndex],
                    HitPoint))
            {
                continue;
            }

            const SnAPI::Math::Scalar Radius = (HitPoint - SelectedTransform.Position).norm();
            const SnAPI::Math::Scalar Delta = std::fabs(Radius - RingRadius);
            if (!std::isfinite(Delta) || Delta > PickTolerance)
            {
                continue;
            }

            const SnAPI::Math::Scalar RayT = (HitPoint - RayOrigin).dot(RayDirection);
            Consider(static_cast<EActiveAxis>(AxisIndex + 1u), RayT, Delta);
        }
        return BestAxis;
    }

    const SnAPI::Math::Scalar PickRadius = std::max<SnAPI::Math::Scalar>(
        AxisThickness * static_cast<SnAPI::Math::Scalar>(4.0),
        AxisLength * static_cast<SnAPI::Math::Scalar>(0.08));
    const SnAPI::Math::Scalar PickRadiusSquared = PickRadius * PickRadius;
    for (std::size_t AxisIndex = 0; AxisIndex < AxisBasis.size(); ++AxisIndex)
    {
        const Vec3 Direction = NormalizeOrAxis(AxisBasis[AxisIndex], Vec3::UnitX());
        SnAPI::Math::Scalar AxisReach = AxisLength;
        if (m_mode == EEditorTransformMode::Translate)
        {
            AxisReach += std::max<SnAPI::Math::Scalar>(
                static_cast<SnAPI::Math::Scalar>(0.10),
                AxisLength * static_cast<SnAPI::Math::Scalar>(0.28));
        }
        else
        {
            AxisReach = AxisLength * static_cast<SnAPI::Math::Scalar>(0.80);
        }

        SnAPI::Math::Scalar RayT = static_cast<SnAPI::Math::Scalar>(0.0);
        const SnAPI::Math::Scalar DistanceSquared = RaySegmentDistanceSquared(
            RayOrigin,
            RayDirection,
            SelectedTransform.Position,
            SelectedTransform.Position + (Direction * AxisReach),
            RayT);
        if (!std::isfinite(DistanceSquared) || DistanceSquared > PickRadiusSquared)
        {
            continue;
        }

        Consider(static_cast<EActiveAxis>(AxisIndex + 1u), RayT, DistanceSquared);
    }

    return BestAxis;
}


Result EditorTransformInteractionService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_mode = EEditorTransformMode::Translate;
    m_space = EditorLayout::EGizmoSpace::World;
    m_snapEnabled = false;
    m_moveSnapStep = static_cast<SnAPI::Math::Scalar>(1.0);
    m_rotateSnapDegrees = static_cast<SnAPI::Math::Scalar>(15.0);
    m_scaleSnapStep = static_cast<SnAPI::Math::Scalar>(0.5);
    m_dragging = false;
    m_activeAxis = EActiveAxis::None;
    m_lastMouseX = 0.0f;
    m_lastMouseY = 0.0f;
    m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
    m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
    m_freeMovePlaneActive = false;
    m_freeMovePlaneNormal = Vec3::UnitZ();
    m_freeMoveNodeStart = Vec3::Zero();
    m_freeMoveHitStart = Vec3::Zero();
    m_axisMovePlaneActive = false;
    m_axisMovePlaneNormal = Vec3::UnitZ();
    m_axisMoveAxisDirection = Vec3::UnitX();
    m_axisMoveNodeStart = Vec3::Zero();
    m_axisMoveHitStart = Vec3::Zero();
    return Ok();
}

void EditorTransformInteractionService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;

#if !defined(SNAPI_GF_ENABLE_INPUT) || !defined(SNAPI_GF_ENABLE_UI) || !defined(SNAPI_GF_ENABLE_RENDERER)
    (void)Context;
    m_dragging = false;
    m_activeAxis = EActiveAxis::None;
    m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
    m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
    m_freeMovePlaneActive = false;
    m_axisMovePlaneActive = false;
    return;
#else
    const auto ResetTranslateDragPlanes = [&]()
    {
        m_freeMovePlaneActive = false;
        m_axisMovePlaneActive = false;
    };
    const auto ClearNativeGizmosForCurrentFrame = []() {};

    if (auto* PieService = Context.GetService<EditorPieService>(); PieService && PieService->IsSessionActive())
    {
        ClearNativeGizmosForCurrentFrame();
        m_dragging = false;
        m_activeAxis = EActiveAxis::None;
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        ResetTranslateDragPlanes();
        return;
    }

    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        ClearNativeGizmosForCurrentFrame();
        m_dragging = false;
        m_activeAxis = EActiveAxis::None;
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        ResetTranslateDragPlanes();
        return;
    }

    if (m_mode != EEditorTransformMode::Translate
        && m_mode != EEditorTransformMode::Rotate
        && m_mode != EEditorTransformMode::Scale)
    {
        m_mode = EEditorTransformMode::Translate;
    }

    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* LayoutService = Context.GetService<EditorLayoutService>();
    if (!SelectionService || !SceneService || !LayoutService)
    {
        ClearNativeGizmosForCurrentFrame();
        m_dragging = false;
        m_activeAxis = EActiveAxis::None;
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        ResetTranslateDragPlanes();
        return;
    }

    const NodeHandle Selected = SelectionService->Model().SelectedNode();
    if (Selected.IsNull())
    {
        ClearNativeGizmosForCurrentFrame();
        m_dragging = false;
        m_activeAxis = EActiveAxis::None;
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        ResetTranslateDragPlanes();
        return;
    }

    BaseNode* Node = SelectionService->Model().ResolveSelectedNode(*WorldPtr);
    if (!Node)
    {
        ClearNativeGizmosForCurrentFrame();
        m_dragging = false;
        m_activeAxis = EActiveAxis::None;
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        ResetTranslateDragPlanes();
        return;
    }

    auto TransformResult = Node->Component<TransformComponent>();
    if (!TransformResult)
    {
        ClearNativeGizmosForCurrentFrame();
        m_dragging = false;
        m_activeAxis = EActiveAxis::None;
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        ResetTranslateDragPlanes();
        return;
    }

    auto* Camera = SceneService->ActiveRenderCamera();
    auto* ViewportElement = LayoutService->GameViewportElement();
    if (!Camera || !ViewportElement)
    {
        ClearNativeGizmosForCurrentFrame();
        m_dragging = false;
        m_activeAxis = EActiveAxis::None;
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        ResetTranslateDragPlanes();
        return;
    }

    m_space = LayoutService->GizmoSpace();
    m_snapEnabled = LayoutService->GizmoSnappingEnabled();
    const double MoveSnapStep = LayoutService->MoveSnapStep();
    const double RotateSnapStepDegrees = LayoutService->RotateSnapStepDegrees();
    const double ScaleSnapStep = LayoutService->ScaleSnapStep();
    m_moveSnapStep = (std::isfinite(MoveSnapStep) && MoveSnapStep > 0.0)
        ? static_cast<SnAPI::Math::Scalar>(MoveSnapStep)
        : static_cast<SnAPI::Math::Scalar>(1.0);
    m_rotateSnapDegrees = (std::isfinite(RotateSnapStepDegrees) && RotateSnapStepDegrees > 0.0)
        ? static_cast<SnAPI::Math::Scalar>(RotateSnapStepDegrees)
        : static_cast<SnAPI::Math::Scalar>(15.0);
    m_scaleSnapStep = (std::isfinite(ScaleSnapStep) && ScaleSnapStep > 0.0)
        ? static_cast<SnAPI::Math::Scalar>(ScaleSnapStep)
        : static_cast<SnAPI::Math::Scalar>(0.5);

    NodeTransform TransformWorld{};
    if (!TransformComponent::TryGetNodeWorldTransform(*Node, TransformWorld))
    {
        TransformWorld.Position = TransformResult->Position;
        TransformWorld.Rotation = TransformResult->Rotation;
        TransformWorld.Scale = TransformResult->Scale;
    }

    if (!IsFiniteVec3(TransformWorld.Position))
    {
        TransformWorld.Position = Vec3::Zero();
    }
    if (!IsFiniteVec3(TransformWorld.Scale))
    {
        TransformWorld.Scale = Vec3::Ones();
    }
    if (!std::isfinite(TransformWorld.Rotation.x()) || !std::isfinite(TransformWorld.Rotation.y()) ||
        !std::isfinite(TransformWorld.Rotation.z()) || !std::isfinite(TransformWorld.Rotation.w()) ||
        !(TransformWorld.Rotation.squaredNorm() > static_cast<Quat::Scalar>(0.0)))
    {
        TransformWorld.Rotation = Quat::Identity();
    }

    const SnAPI::UI::UIRect ViewRect = ViewportElement->LayoutRect();
    const std::uint64_t ViewportID = ViewportElement->OwnedViewportId();
    const bool HideGizmoForActiveEditorCamera = [&]() -> bool
    {
        if (!Node->Component<EditorCameraComponent>())
        {
            return false;
        }

        auto* ActiveCameraComponent = SceneService->ActiveCameraComponent();
        if (!ActiveCameraComponent || ActiveCameraComponent->Owner().IsNull())
        {
            return false;
        }

        return ActiveCameraComponent->Owner() == Node->Handle();
    }();
    const auto QueueGizmosForCurrentFrame = [&]()
    {
        if (ViewportID != 0 && !HideGizmoForActiveEditorCamera)
        {
            UpdateNativeTransformGizmos(Context, Node, TransformWorld, *Camera, ViewportID);
        }
        else
        {
            ClearNativeGizmosForCurrentFrame();
        }
    };

    if (!WorldPtr->Input().IsInitialized())
    {
        QueueGizmosForCurrentFrame();
        m_dragging = false;
        m_activeAxis = EActiveAxis::None;
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        ResetTranslateDragPlanes();
        return;
    }

    const auto* Snapshot = WorldPtr->Input().Snapshot();
    if (!Snapshot || !Snapshot->IsWindowFocused())
    {
        QueueGizmosForCurrentFrame();
        m_dragging = false;
        m_activeAxis = EActiveAxis::None;
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        ResetTranslateDragPlanes();
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

    const auto MousePosition = WorldPtr->UI().MapInputPointToScreenPoint(
        SnAPI::UI::UIPoint{Snapshot->Mouse().X, Snapshot->Mouse().Y});
    const float MouseX = MousePosition.X;
    const float MouseY = MousePosition.Y;
    const bool PointerInside = IsPointInsideRect(ViewRect, MouseX, MouseY);

    const bool LeftDown = Snapshot->MouseButtonDown(SnAPI::Input::EMouseButton::Left);
    const bool LeftPressed = Snapshot->MouseButtonPressed(SnAPI::Input::EMouseButton::Left);
    const bool AllowTransform = PointerInside && LeftDown && !RightDown;
    if (!AllowTransform)
    {
        QueueGizmosForCurrentFrame();
        m_dragging = false;
        m_activeAxis = EActiveAxis::None;
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        ResetTranslateDragPlanes();
        return;
    }

    if (!m_dragging || LeftPressed)
    {
        m_dragging = true;
        if (LeftPressed)
        {
            m_activeAxis = PickNativeGizmoAxis(Context, TransformWorld, *Camera, MouseX, MouseY, ViewRect, ViewportID);
        }
        ResetTranslateDragPlanes();
        if (m_mode == EEditorTransformMode::Translate && m_activeAxis == EActiveAxis::None)
        {
            Vec3 RayOrigin = Vec3::Zero();
            Vec3 RayDirection = Vec3::Zero();
            Vec3 GrabPoint = Vec3::Zero();
            const Vec3 PlaneNormal = NormalizeOrAxis(Camera->Forward().template cast<SnAPI::Math::Scalar>(), Vec3::UnitZ());
            if (TryBuildViewportRay(*Camera, ViewRect, MouseX, MouseY, RayOrigin, RayDirection)
                && TryIntersectRayPlane(RayOrigin, RayDirection, TransformWorld.Position, PlaneNormal, GrabPoint))
            {
                m_freeMovePlaneActive = true;
                m_freeMovePlaneNormal = PlaneNormal;
                m_freeMoveNodeStart = TransformWorld.Position;
                m_freeMoveHitStart = GrabPoint;
            }
        }
        else if (m_mode == EEditorTransformMode::Translate)
        {
            const auto ResolveAxisDirectionForDragStart = [&]() -> Vec3
            {
                switch (m_space)
                {
                case EditorLayout::EGizmoSpace::Object:
                    {
                        const auto RotationMatrix = TransformWorld.Rotation.toRotationMatrix();
                        switch (m_activeAxis)
                        {
                        case EActiveAxis::X:
                            return NormalizeOrAxis(RotationMatrix * Vec3::UnitX(), Vec3::UnitX());
                        case EActiveAxis::Y:
                            return NormalizeOrAxis(RotationMatrix * Vec3::UnitY(), Vec3::UnitY());
                        case EActiveAxis::Z:
                            return NormalizeOrAxis(RotationMatrix * Vec3::UnitZ(), Vec3::UnitZ());
                        case EActiveAxis::None:
                        default:
                            break;
                        }
                    }
                    break;
                case EditorLayout::EGizmoSpace::Camera:
                    switch (m_activeAxis)
                    {
                    case EActiveAxis::X:
                        return NormalizeOrAxis(Camera->Right().template cast<SnAPI::Math::Scalar>(), Vec3::UnitX());
                    case EActiveAxis::Y:
                        return NormalizeOrAxis(Camera->Up().template cast<SnAPI::Math::Scalar>(), Vec3::UnitY());
                    case EActiveAxis::Z:
                        return NormalizeOrAxis(Camera->Forward().template cast<SnAPI::Math::Scalar>(), Vec3::UnitZ());
                    case EActiveAxis::None:
                    default:
                        break;
                    }
                    break;
                case EditorLayout::EGizmoSpace::World:
                default:
                    switch (m_activeAxis)
                    {
                    case EActiveAxis::X:
                        return Vec3::UnitX();
                    case EActiveAxis::Y:
                        return Vec3::UnitY();
                    case EActiveAxis::Z:
                        return Vec3::UnitZ();
                    case EActiveAxis::None:
                    default:
                        break;
                    }
                    break;
                }
                return Vec3::UnitX();
            };

            const Vec3 AxisDirection = NormalizeOrAxis(ResolveAxisDirectionForDragStart(), Vec3::UnitX());
            const Vec3 PlaneNormal = ResolveAxisMovePlaneNormal(AxisDirection, *Camera);

            Vec3 RayOrigin = Vec3::Zero();
            Vec3 RayDirection = Vec3::Zero();
            Vec3 GrabPoint = Vec3::Zero();
            if (TryBuildViewportRay(*Camera, ViewRect, MouseX, MouseY, RayOrigin, RayDirection)
                && TryIntersectRayPlane(RayOrigin, RayDirection, TransformWorld.Position, PlaneNormal, GrabPoint))
            {
                m_axisMovePlaneActive = true;
                m_axisMovePlaneNormal = PlaneNormal;
                m_axisMoveAxisDirection = AxisDirection;
                m_axisMoveNodeStart = TransformWorld.Position;
                m_axisMoveHitStart = GrabPoint;
            }
        }
        m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
        m_lastMouseX = MouseX;
        m_lastMouseY = MouseY;
        QueueGizmosForCurrentFrame();
        return;
    }

    const float Dx = MouseX - m_lastMouseX;
    const float Dy = MouseY - m_lastMouseY;
    m_lastMouseX = MouseX;
    m_lastMouseY = MouseY;
    if (!IsFiniteFloat(Dx) || !IsFiniteFloat(Dy))
    {
        QueueGizmosForCurrentFrame();
        return;
    }

    const bool Fast = Snapshot->KeyDown(SnAPI::Input::EKey::LeftShift) ||
                      Snapshot->KeyDown(SnAPI::Input::EKey::RightShift);
    const float SpeedMultiplier = Fast ? 2.0f : 1.0f;

    Vec3 BasisX = Vec3::UnitX();
    Vec3 BasisY = Vec3::UnitY();
    Vec3 BasisZ = Vec3::UnitZ();
    switch (m_space)
    {
    case EditorLayout::EGizmoSpace::Object:
        {
            const auto RotationMatrix = TransformWorld.Rotation.toRotationMatrix();
            BasisX = NormalizeOrAxis(RotationMatrix * Vec3::UnitX(), Vec3::UnitX());
            BasisY = NormalizeOrAxis(RotationMatrix * Vec3::UnitY(), Vec3::UnitY());
            BasisZ = NormalizeOrAxis(RotationMatrix * Vec3::UnitZ(), Vec3::UnitZ());
        }
        break;
    case EditorLayout::EGizmoSpace::Camera:
        BasisX = NormalizeOrAxis(Camera->Right().template cast<SnAPI::Math::Scalar>(), Vec3::UnitX());
        BasisY = NormalizeOrAxis(Camera->Up().template cast<SnAPI::Math::Scalar>(), Vec3::UnitY());
        BasisZ = NormalizeOrAxis(Camera->Forward().template cast<SnAPI::Math::Scalar>(), Vec3::UnitZ());
        break;
    case EditorLayout::EGizmoSpace::World:
    default:
        break;
    }

    const auto AxisVector = [&]() -> Vec3
    {
        switch (m_activeAxis)
        {
        case EActiveAxis::X:
            return BasisX;
        case EActiveAxis::Y:
            return BasisY;
        case EActiveAxis::Z:
            return BasisZ;
        case EActiveAxis::None:
        default:
            return Vec3::Zero();
        }
    };

    switch (m_mode)
    {
    case EEditorTransformMode::Translate:
        {
            Vec3 Right = NormalizeOrAxis(Camera->Right().template cast<SnAPI::Math::Scalar>(), Vec3::UnitX());
            Vec3 Up = NormalizeOrAxis(Camera->Up().template cast<SnAPI::Math::Scalar>(), Vec3::UnitY());
            const Vec3 CameraPos = Camera->Position().template cast<SnAPI::Math::Scalar>();
            const SnAPI::Math::Scalar Distance = std::max<SnAPI::Math::Scalar>(
                static_cast<SnAPI::Math::Scalar>(0.25),
                (TransformWorld.Position - CameraPos).norm());
            const SnAPI::Math::Scalar PixelScale = Distance * static_cast<SnAPI::Math::Scalar>(0.0015 * SpeedMultiplier);
            if (m_activeAxis == EActiveAxis::None)
            {
                bool UsedPlaneDrag = false;
                if (m_freeMovePlaneActive)
                {
                    Vec3 RayOrigin = Vec3::Zero();
                    Vec3 RayDirection = Vec3::Zero();
                    Vec3 HitPoint = Vec3::Zero();
                    if (TryBuildViewportRay(*Camera, ViewRect, MouseX, MouseY, RayOrigin, RayDirection)
                        && TryIntersectRayPlane(RayOrigin, RayDirection, m_freeMoveNodeStart, m_freeMovePlaneNormal, HitPoint))
                    {
                        TransformWorld.Position = m_freeMoveNodeStart + (HitPoint - m_freeMoveHitStart);
                        UsedPlaneDrag = true;
                    }
                }

                if (!UsedPlaneDrag)
                {
                    TransformWorld.Position += (Right * static_cast<SnAPI::Math::Scalar>(Dx) * PixelScale) +
                                               (Up * static_cast<SnAPI::Math::Scalar>(-Dy) * PixelScale);
                }
            }
            else
            {
                m_freeMovePlaneActive = false;
                bool UsedAxisPlaneDrag = false;
                const Vec3 Axis = NormalizeOrAxis(
                    m_axisMovePlaneActive ? m_axisMoveAxisDirection : AxisVector(),
                    Vec3::UnitX());
                if (m_axisMovePlaneActive)
                {
                    Vec3 RayOrigin = Vec3::Zero();
                    Vec3 RayDirection = Vec3::Zero();
                    Vec3 HitPoint = Vec3::Zero();
                    if (TryBuildViewportRay(*Camera, ViewRect, MouseX, MouseY, RayOrigin, RayDirection)
                        && TryIntersectRayPlane(RayOrigin, RayDirection, m_axisMoveNodeStart, m_axisMovePlaneNormal, HitPoint))
                    {
                        const SnAPI::Math::Scalar AxisOffset = (HitPoint - m_axisMoveHitStart).dot(m_axisMoveAxisDirection);
                        if (std::isfinite(AxisOffset))
                        {
                            TransformWorld.Position = m_axisMoveNodeStart + (m_axisMoveAxisDirection * AxisOffset);
                            UsedAxisPlaneDrag = true;
                        }
                    }
                }

                if (!UsedAxisPlaneDrag)
                {
                    const SnAPI::Math::Scalar AxisPixelDelta =
                        (Axis.dot(Right) * static_cast<SnAPI::Math::Scalar>(Dx)) +
                        (Axis.dot(Up) * static_cast<SnAPI::Math::Scalar>(-Dy));
                    TransformWorld.Position += Axis * (AxisPixelDelta * PixelScale);
                }
            }

            if (m_snapEnabled && m_moveSnapStep > static_cast<SnAPI::Math::Scalar>(0.0))
            {
                if (m_activeAxis == EActiveAxis::None)
                {
                    TransformWorld.Position.x() = SnapValueToStep(TransformWorld.Position.x(), m_moveSnapStep);
                    TransformWorld.Position.y() = SnapValueToStep(TransformWorld.Position.y(), m_moveSnapStep);
                    TransformWorld.Position.z() = SnapValueToStep(TransformWorld.Position.z(), m_moveSnapStep);
                }
                else
                {
                    const Vec3 Axis = NormalizeOrAxis(
                        m_axisMovePlaneActive ? m_axisMoveAxisDirection : AxisVector(),
                        Vec3::UnitX());
                    const SnAPI::Math::Scalar AxisCoord = TransformWorld.Position.dot(Axis);
                    const SnAPI::Math::Scalar SnappedCoord = SnapValueToStep(AxisCoord, m_moveSnapStep);
                    TransformWorld.Position += Axis * (SnappedCoord - AxisCoord);
                }
            }
        }
        break;
    case EEditorTransformMode::Rotate:
        {
            ResetTranslateDragPlanes();
            if (m_activeAxis != EActiveAxis::None)
            {
                const Vec3 Axis = NormalizeOrAxis(AxisVector(), Vec3::UnitX());
                const SnAPI::Math::Scalar DegreesPerPixel = static_cast<SnAPI::Math::Scalar>(0.35 * SpeedMultiplier);
                SnAPI::Math::Scalar RotationRadians = SnAPI::Math::SLinearAlgebra::DegreesToRadians(
                    static_cast<SnAPI::Math::Scalar>(Dx - Dy) * DegreesPerPixel);
                if (m_snapEnabled)
                {
                    const SnAPI::Math::Scalar RotationStepRadians = SnAPI::Math::SLinearAlgebra::DegreesToRadians(m_rotateSnapDegrees);
                    RotationRadians = ConsumeSnapRemainder(RotationRadians, RotationStepRadians, m_rotateSnapRemainderPrimary);
                }
                if (!std::isfinite(RotationRadians) ||
                    std::fabs(RotationRadians) <= static_cast<SnAPI::Math::Scalar>(0.0))
                {
                    break;
                }
                const Quat AxisQuat(SnAPI::Math::AngleAxis3D(RotationRadians, Axis));
                TransformWorld.Rotation = (AxisQuat * TransformWorld.Rotation).normalized();
            }
            else
            {
                const SnAPI::Math::Scalar DegreesPerPixel = static_cast<SnAPI::Math::Scalar>(0.25 * SpeedMultiplier);
                SnAPI::Math::Scalar YawRadians = SnAPI::Math::SLinearAlgebra::DegreesToRadians(
                    static_cast<SnAPI::Math::Scalar>(Dx) * DegreesPerPixel);
                SnAPI::Math::Scalar PitchRadians = SnAPI::Math::SLinearAlgebra::DegreesToRadians(
                    static_cast<SnAPI::Math::Scalar>(-Dy) * DegreesPerPixel);
                if (m_snapEnabled)
                {
                    const SnAPI::Math::Scalar RotationStepRadians = SnAPI::Math::SLinearAlgebra::DegreesToRadians(m_rotateSnapDegrees);
                    YawRadians = ConsumeSnapRemainder(YawRadians, RotationStepRadians, m_rotateSnapRemainderPrimary);
                    PitchRadians = ConsumeSnapRemainder(PitchRadians, RotationStepRadians, m_rotateSnapRemainderSecondary);
                }

                const Quat YawQuat(SnAPI::Math::AngleAxis3D(YawRadians, BasisY));
                const Quat PitchQuat(SnAPI::Math::AngleAxis3D(PitchRadians, BasisX));

                TransformWorld.Rotation = (YawQuat * PitchQuat * TransformWorld.Rotation).normalized();
            }
        }
        break;
    case EEditorTransformMode::Scale:
        {
            ResetTranslateDragPlanes();
            const SnAPI::Math::Scalar Delta = static_cast<SnAPI::Math::Scalar>((Dx - Dy) * 0.01f * SpeedMultiplier);
            const SnAPI::Math::Scalar ScaleFactor = std::max<SnAPI::Math::Scalar>(
                static_cast<SnAPI::Math::Scalar>(0.01),
                static_cast<SnAPI::Math::Scalar>(1.0) + Delta);

            if (m_activeAxis == EActiveAxis::None)
            {
                TransformWorld.Scale *= ScaleFactor;
            }
            else
            {
                switch (m_activeAxis)
                {
                case EActiveAxis::X:
                    TransformWorld.Scale.x() *= ScaleFactor;
                    break;
                case EActiveAxis::Y:
                    TransformWorld.Scale.y() *= ScaleFactor;
                    break;
                case EActiveAxis::Z:
                    TransformWorld.Scale.z() *= ScaleFactor;
                    break;
                case EActiveAxis::None:
                default:
                    break;
                }
            }

            if (m_snapEnabled && m_scaleSnapStep > static_cast<SnAPI::Math::Scalar>(0.0))
            {
                if (m_activeAxis == EActiveAxis::None)
                {
                    TransformWorld.Scale.x() = SnapValueToStep(TransformWorld.Scale.x(), m_scaleSnapStep);
                    TransformWorld.Scale.y() = SnapValueToStep(TransformWorld.Scale.y(), m_scaleSnapStep);
                    TransformWorld.Scale.z() = SnapValueToStep(TransformWorld.Scale.z(), m_scaleSnapStep);
                }
                else
                {
                    switch (m_activeAxis)
                    {
                    case EActiveAxis::X:
                        TransformWorld.Scale.x() = SnapValueToStep(TransformWorld.Scale.x(), m_scaleSnapStep);
                        break;
                    case EActiveAxis::Y:
                        TransformWorld.Scale.y() = SnapValueToStep(TransformWorld.Scale.y(), m_scaleSnapStep);
                        break;
                    case EActiveAxis::Z:
                        TransformWorld.Scale.z() = SnapValueToStep(TransformWorld.Scale.z(), m_scaleSnapStep);
                        break;
                    case EActiveAxis::None:
                    default:
                        break;
                    }
                }
            }

            TransformWorld.Scale.x() =
                std::max<SnAPI::Math::Scalar>(TransformWorld.Scale.x(), static_cast<SnAPI::Math::Scalar>(0.001));
            TransformWorld.Scale.y() =
                std::max<SnAPI::Math::Scalar>(TransformWorld.Scale.y(), static_cast<SnAPI::Math::Scalar>(0.001));
            TransformWorld.Scale.z() =
                std::max<SnAPI::Math::Scalar>(TransformWorld.Scale.z(), static_cast<SnAPI::Math::Scalar>(0.001));
        }
        break;
    default:
        break;
    }

    (void)TransformComponent::TrySetNodeWorldTransform(*Node, TransformWorld, true);
    QueueGizmosForCurrentFrame();
#endif
}

void EditorTransformInteractionService::Shutdown(EditorServiceContext& Context)
{
    (void)Context;
    m_dragging = false;
    m_activeAxis = EActiveAxis::None;
    m_rotateSnapRemainderPrimary = static_cast<SnAPI::Math::Scalar>(0.0);
    m_rotateSnapRemainderSecondary = static_cast<SnAPI::Math::Scalar>(0.0);
    m_freeMovePlaneActive = false;
    m_freeMovePlaneNormal = Vec3::UnitZ();
    m_freeMoveNodeStart = Vec3::Zero();
    m_freeMoveHitStart = Vec3::Zero();
    m_axisMovePlaneActive = false;
    m_axisMovePlaneNormal = Vec3::UnitZ();
    m_axisMoveAxisDirection = Vec3::UnitX();
    m_axisMoveNodeStart = Vec3::Zero();
    m_axisMoveHitStart = Vec3::Zero();
}

} // namespace SnAPI::GameFramework::Editor
