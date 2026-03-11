#include "Editor/EditorTransformInteractionService.h"

#include "BaseNode.h"
#include "CameraComponent.h"
#include "Editor/EditorLayoutService.h"
#include "Editor/EditorPieService.h"
#include "Editor/EditorSceneService.h"
#include "Editor/EditorSelectionService.h"
#include "GameRuntime.h"
#include "InputSystem.h"
#include "SkeletalMeshComponent.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"
#include "UIRenderViewport.h"
#include "World.h"

#include <UIEvents.h>
#include <UIContext.h>
#include <UIElementBase.h>

#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <PrimitiveStreamSources.hpp>
#include "ICamera.hpp"
#include "IRenderObject.hpp"
#include "LinearAlgebra.hpp"
#include <MeshRenderObject.hpp>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

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

[[nodiscard]] Quat RotationFromTo(const Vec3& From, const Vec3& To)
{
    const Vec3 Source = NormalizeOrAxis(From, Vec3::UnitX());
    const Vec3 Target = NormalizeOrAxis(To, Vec3::UnitX());
    const SnAPI::Math::Scalar Dot =
        std::clamp(Source.dot(Target), static_cast<SnAPI::Math::Scalar>(-1.0), static_cast<SnAPI::Math::Scalar>(1.0));

    if (Dot >= static_cast<SnAPI::Math::Scalar>(1.0 - 1.0e-6))
    {
        return Quat::Identity();
    }

    if (Dot <= static_cast<SnAPI::Math::Scalar>(-1.0 + 1.0e-6))
    {
        const Vec3 SeedAxis = std::fabs(Source.dot(Vec3::UnitX())) < static_cast<SnAPI::Math::Scalar>(0.99)
            ? Vec3::UnitX()
            : Vec3::UnitY();
        const Vec3 OrthogonalAxis = NormalizeOrAxis(Source.cross(SeedAxis), Vec3::UnitZ());
        return Quat(SnAPI::Math::AngleAxis3D(std::numbers::pi_v<SnAPI::Math::Scalar>, OrthogonalAxis));
    }

    const Vec3 Axis = NormalizeOrAxis(Source.cross(Target), Vec3::UnitZ());
    const SnAPI::Math::Scalar Angle = std::acos(Dot);
    return Quat(SnAPI::Math::AngleAxis3D(Angle, Axis));
}

#if defined(SNAPI_GF_ENABLE_RENDERER)
class EditorTorusStreamSource final : public SnAPI::Graphics::PrimitiveStreamSourceBase
{
public:
    EditorTorusStreamSource(float MajorRadius, float MinorRadius, std::uint32_t MajorSegments, std::uint32_t MinorSegments)
        : SnAPI::Graphics::PrimitiveStreamSourceBase("EditorTorusStreamSource")
        , m_majorRadius(std::max(MajorRadius, 0.01f))
        , m_minorRadius(std::max(MinorRadius, 0.0025f))
        , m_majorSegments(std::max(MajorSegments, 8u))
        , m_minorSegments(std::max(MinorSegments, 4u))
    {
        RebuildGeometry();
    }

    void SetShape(float MajorRadius, float MinorRadius)
    {
        const float NextMajor = std::max(MajorRadius, 0.01f);
        const float NextMinor = std::max(MinorRadius, 0.0025f);
        if (std::fabs(m_majorRadius - NextMajor) <= 1.0e-6f
            && std::fabs(m_minorRadius - NextMinor) <= 1.0e-6f)
        {
            return;
        }

        m_majorRadius = NextMajor;
        m_minorRadius = NextMinor;
        RebuildGeometry();
    }

private:
    void RebuildGeometry()
    {
        const std::uint32_t RingSegments = std::max(m_majorSegments, 8u);
        const std::uint32_t TubeSegments = std::max(m_minorSegments, 4u);
        const std::uint32_t RingVertices = RingSegments + 1u;
        const std::uint32_t TubeVertices = TubeSegments + 1u;

        std::vector<SnAPI::Vector3DF> Positions{};
        std::vector<SnAPI::Vector3DF> Normals{};
        std::vector<SnAPI::Vector4DF> Tangents{};
        std::vector<SnAPI::Vector2DF> UV0{};
        std::vector<std::uint32_t> Indices{};

        Positions.reserve(static_cast<std::size_t>(RingVertices) * static_cast<std::size_t>(TubeVertices));
        Normals.reserve(Positions.capacity());
        Tangents.reserve(Positions.capacity());
        UV0.reserve(Positions.capacity());
        Indices.reserve(static_cast<std::size_t>(RingSegments) * static_cast<std::size_t>(TubeSegments) * 6u);

        const float InvRing = 1.0f / static_cast<float>(RingSegments);
        const float InvTube = 1.0f / static_cast<float>(TubeSegments);
        for (std::uint32_t Ring = 0; Ring <= RingSegments; ++Ring)
        {
            const float RingT = static_cast<float>(Ring) * InvRing;
            const float Phi = RingT * static_cast<float>(2.0 * std::numbers::pi_v<double>);
            const float CosPhi = std::cos(Phi);
            const float SinPhi = std::sin(Phi);

            for (std::uint32_t Tube = 0; Tube <= TubeSegments; ++Tube)
            {
                const float TubeT = static_cast<float>(Tube) * InvTube;
                const float Theta = TubeT * static_cast<float>(2.0 * std::numbers::pi_v<double>);
                const float CosTheta = std::cos(Theta);
                const float SinTheta = std::sin(Theta);

                const float RadiusAtTube = m_majorRadius + (m_minorRadius * CosTheta);
                const SnAPI::Vector3DF Position{
                    RadiusAtTube * CosPhi,
                    m_minorRadius * SinTheta,
                    RadiusAtTube * SinPhi};

                SnAPI::Vector3DF Normal{
                    CosTheta * CosPhi,
                    SinTheta,
                    CosTheta * SinPhi};
                if (Normal.squaredNorm() > 1.0e-10f)
                {
                    Normal.normalize();
                }
                else
                {
                    Normal = SnAPI::Vector3DF::UnitY();
                }

                SnAPI::Vector3DF Tangent3{
                    -RadiusAtTube * SinPhi,
                    0.0f,
                    RadiusAtTube * CosPhi};
                if (Tangent3.squaredNorm() > 1.0e-10f)
                {
                    Tangent3.normalize();
                }
                else
                {
                    Tangent3 = SnAPI::Vector3DF::UnitX();
                }

                Positions.push_back(Position);
                Normals.push_back(Normal);
                Tangents.emplace_back(Tangent3.x(), Tangent3.y(), Tangent3.z(), 1.0f);
                UV0.emplace_back(RingT, TubeT);
            }
        }

        for (std::uint32_t Ring = 0; Ring < RingSegments; ++Ring)
        {
            for (std::uint32_t Tube = 0; Tube < TubeSegments; ++Tube)
            {
                const std::uint32_t I0 = (Ring * TubeVertices) + Tube;
                const std::uint32_t I1 = I0 + 1u;
                const std::uint32_t I2 = ((Ring + 1u) * TubeVertices) + Tube;
                const std::uint32_t I3 = I2 + 1u;

                Indices.push_back(I0);
                Indices.push_back(I2);
                Indices.push_back(I1);

                Indices.push_back(I1);
                Indices.push_back(I2);
                Indices.push_back(I3);
            }
        }

        ReplaceGeometry(std::move(Positions),
                        std::move(Normals),
                        std::move(Tangents),
                        std::move(UV0),
                        std::move(Indices));
    }

    float m_majorRadius = 0.5f;
    float m_minorRadius = 0.08f;
    std::uint32_t m_majorSegments = 64u;
    std::uint32_t m_minorSegments = 14u;
};

[[nodiscard]] bool TryBuildViewportRay(const SnAPI::Graphics::ICamera& Camera,
                                       const SnAPI::UI::UIRect& ViewRect,
                                       const float ScreenX,
                                       const float ScreenY,
                                       Vec3& OutRayOrigin,
                                       Vec3& OutRayDirection)
{
    if (ViewRect.W <= 0.0f || ViewRect.H <= 0.0f || !std::isfinite(ScreenX) || !std::isfinite(ScreenY))
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
        static_cast<double>(Camera.Fov()) * (std::numbers::pi_v<double> / 180.0));
    const SnAPI::Math::Scalar TanHalfFov = static_cast<SnAPI::Math::Scalar>(
        std::tan(static_cast<double>(FovRadians) * 0.5));
    const SnAPI::Math::Scalar Aspect = static_cast<SnAPI::Math::Scalar>(Camera.Aspect());
    if (!std::isfinite(TanHalfFov) || !std::isfinite(Aspect)
        || !(TanHalfFov > static_cast<SnAPI::Math::Scalar>(0.0))
        || !(Aspect > static_cast<SnAPI::Math::Scalar>(0.0)))
    {
        return false;
    }

    const Vec3 Forward = NormalizeOrAxis(Camera.Forward().template cast<SnAPI::Math::Scalar>(), Vec3::UnitZ());
    const Vec3 Right = NormalizeOrAxis(Camera.Right().template cast<SnAPI::Math::Scalar>(), Vec3::UnitX());
    const Vec3 Up = NormalizeOrAxis(Camera.Up().template cast<SnAPI::Math::Scalar>(), Vec3::UnitY());

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
        std::max(static_cast<SnAPI::Math::Scalar>(Camera.Near()), static_cast<SnAPI::Math::Scalar>(0.001));
    const Vec3 RayOrigin = Camera.Position().template cast<SnAPI::Math::Scalar>() + (RayDirection * NearClip);
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
                                              const SnAPI::Graphics::ICamera& Camera)
{
    const Vec3 Axis = NormalizeOrAxis(AxisDirection, Vec3::UnitX());
    const Vec3 CameraForward = NormalizeOrAxis(Camera.Forward().template cast<SnAPI::Math::Scalar>(), Vec3::UnitZ());
    const Vec3 CameraRight = NormalizeOrAxis(Camera.Right().template cast<SnAPI::Math::Scalar>(), Vec3::UnitX());
    const Vec3 CameraUp = NormalizeOrAxis(Camera.Up().template cast<SnAPI::Math::Scalar>(), Vec3::UnitY());

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

[[nodiscard]] bool IsFiniteMatrix4(const SnAPI::Matrix4& Matrix)
{
    return Matrix.allFinite();
}
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(WITH_EDITOR) && WITH_EDITOR
[[nodiscard]] std::array<SnAPI::Math::Scalar, 3> ResolveFallbackAxisExtents(const std::array<Vec3, 3>& Axes,
                                                                             const Vec3& FallbackScale)
{
    const Vec3 ScaleAbs = FallbackScale.cwiseAbs();
    std::array<SnAPI::Math::Scalar, 3> Extents{
        static_cast<SnAPI::Math::Scalar>(0.05),
        static_cast<SnAPI::Math::Scalar>(0.05),
        static_cast<SnAPI::Math::Scalar>(0.05)};

    for (std::size_t AxisIndex = 0; AxisIndex < Axes.size(); ++AxisIndex)
    {
        const Vec3 Axis = NormalizeOrAxis(
            Axes[AxisIndex],
            AxisIndex == 0
                ? Vec3::UnitX()
                : (AxisIndex == 1 ? Vec3::UnitY() : Vec3::UnitZ()));
        const SnAPI::Math::Scalar Extent = static_cast<SnAPI::Math::Scalar>(0.5) * (
            std::fabs(Axis.x()) * ScaleAbs.x() +
            std::fabs(Axis.y()) * ScaleAbs.y() +
            std::fabs(Axis.z()) * ScaleAbs.z());
        if (std::isfinite(Extent) && Extent > static_cast<SnAPI::Math::Scalar>(0.0))
        {
            Extents[AxisIndex] = std::max(Extents[AxisIndex], Extent);
        }
    }

    return Extents;
}

[[nodiscard]] bool ComputeRenderObjectAxisExtentsFromPivot(const SnAPI::Graphics::IRenderObject& RenderObject,
                                                           const std::array<Vec3, 3>& Axes,
                                                           std::array<SnAPI::Math::Scalar, 3>& InOutMaxExtents)
{
    const auto& Source = RenderObject.VertexStreamSource();
    if (!Source)
    {
        return false;
    }

    const std::uint32_t SubMeshCount = Source->SubMeshCount();
    if (SubMeshCount == 0)
    {
        return false;
    }

    std::array<SnAPI::Vector3D, 3> AxisVectors{
        SnAPI::Vector3D{NormalizeOrAxis(Axes[0], Vec3::UnitX()).x(), NormalizeOrAxis(Axes[0], Vec3::UnitX()).y(), NormalizeOrAxis(Axes[0], Vec3::UnitX()).z()},
        SnAPI::Vector3D{NormalizeOrAxis(Axes[1], Vec3::UnitY()).x(), NormalizeOrAxis(Axes[1], Vec3::UnitY()).y(), NormalizeOrAxis(Axes[1], Vec3::UnitY()).z()},
        SnAPI::Vector3D{NormalizeOrAxis(Axes[2], Vec3::UnitZ()).x(), NormalizeOrAxis(Axes[2], Vec3::UnitZ()).y(), NormalizeOrAxis(Axes[2], Vec3::UnitZ()).z()}};

    bool HasBounds = false;
    for (std::uint32_t SubMeshIndex = 0; SubMeshIndex < SubMeshCount; ++SubMeshIndex)
    {
        SnAPI::Graphics::VertexSourceSubMesh SubMesh{};
        if (!Source->SubMesh(SubMeshIndex, SubMesh))
        {
            continue;
        }

        const SnAPI::Matrix4 WorldTransform = RenderObject.GlobalTransform(SubMeshIndex);
        if (!IsFiniteMatrix4(WorldTransform))
        {
            continue;
        }

        const SnAPI::Vector3D LocalMin = SubMesh.BoundingBoxMin.cast<double>();
        const SnAPI::Vector3D LocalMax = SubMesh.BoundingBoxMax.cast<double>();
        const SnAPI::Vector3D LocalCenter = (LocalMin + LocalMax) * 0.5;
        const SnAPI::Vector3D LocalExtent = (LocalMax - LocalMin) * 0.5;

        const auto LinearPart = WorldTransform.block<3, 3>(0, 0);
        const SnAPI::Vector3D CenterOffset = LinearPart * LocalCenter;
        const SnAPI::Vector3D AxisColumn0 = LinearPart.col(0);
        const SnAPI::Vector3D AxisColumn1 = LinearPart.col(1);
        const SnAPI::Vector3D AxisColumn2 = LinearPart.col(2);

        for (std::size_t AxisIndex = 0; AxisIndex < AxisVectors.size(); ++AxisIndex)
        {
            const SnAPI::Vector3D& Axis = AxisVectors[AxisIndex];
            const SnAPI::Math::Scalar Extent = static_cast<SnAPI::Math::Scalar>(
                std::fabs(Axis.dot(CenterOffset)) +
                (std::fabs(Axis.dot(AxisColumn0)) * LocalExtent.x()) +
                (std::fabs(Axis.dot(AxisColumn1)) * LocalExtent.y()) +
                (std::fabs(Axis.dot(AxisColumn2)) * LocalExtent.z()));
            if (!std::isfinite(Extent) || !(Extent > static_cast<SnAPI::Math::Scalar>(0.0)))
            {
                continue;
            }

            InOutMaxExtents[AxisIndex] = std::max(InOutMaxExtents[AxisIndex], Extent);
            HasBounds = true;
        }
    }

    return HasBounds;
}

[[nodiscard]] std::array<SnAPI::Math::Scalar, 3> ResolveSelectedObjectAxisExtents(BaseNode& Node,
                                                                                   const std::array<Vec3, 3>& Axes,
                                                                                   const Vec3& FallbackScale)
{
    auto Extents = ResolveFallbackAxisExtents(Axes, FallbackScale);

    auto StaticMeshResult = Node.Component<StaticMeshComponent>();
    if (StaticMeshResult && StaticMeshResult->RenderObject())
    {
        (void)ComputeRenderObjectAxisExtentsFromPivot(*StaticMeshResult->RenderObject(), Axes, Extents);
    }

    auto SkeletalMeshResult = Node.Component<SkeletalMeshComponent>();
    if (SkeletalMeshResult && SkeletalMeshResult->RenderObject())
    {
        (void)ComputeRenderObjectAxisExtentsFromPivot(*SkeletalMeshResult->RenderObject(), Axes, Extents);
    }

    for (auto& Extent : Extents)
    {
        if (!std::isfinite(Extent) || Extent <= static_cast<SnAPI::Math::Scalar>(0.0))
        {
            Extent = static_cast<SnAPI::Math::Scalar>(0.05);
        }
    }
    return Extents;
}

#endif

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

#if defined(SNAPI_GF_ENABLE_RENDERER)
void EditorTransformInteractionService::EnsureGizmoRenderObjects()
{
    if (m_gizmoAxisX && m_gizmoAxisY && m_gizmoAxisZ
        && m_gizmoAxisXAux && m_gizmoAxisYAux && m_gizmoAxisZAux)
    {
        return;
    }

    const auto BuildGizmoObject = []() -> std::shared_ptr<SnAPI::Graphics::IRenderObject>
    {
        auto RenderObject = std::make_shared<SnAPI::Graphics::MeshRenderObject>();
        if (!RenderObject)
        {
            return {};
        }

        RenderObject->TriangleCulling(false);
        RenderObject->SetCastsShadows(false);
        return RenderObject;
    };

    if (!m_gizmoAxisX)
    {
        m_gizmoAxisX = BuildGizmoObject();
    }
    if (!m_gizmoAxisY)
    {
        m_gizmoAxisY = BuildGizmoObject();
    }
    if (!m_gizmoAxisZ)
    {
        m_gizmoAxisZ = BuildGizmoObject();
    }
    if (!m_gizmoAxisXAux)
    {
        m_gizmoAxisXAux = BuildGizmoObject();
    }
    if (!m_gizmoAxisYAux)
    {
        m_gizmoAxisYAux = BuildGizmoObject();
    }
    if (!m_gizmoAxisZAux)
    {
        m_gizmoAxisZAux = BuildGizmoObject();
    }

    ConfigureGizmoGeometryForMode();
}

void EditorTransformInteractionService::ConfigureGizmoGeometryForMode()
{
    if (!m_gizmoAxisX || !m_gizmoAxisY || !m_gizmoAxisZ
        || !m_gizmoAxisXAux || !m_gizmoAxisYAux || !m_gizmoAxisZAux)
    {
        return;
    }

    const bool MissingSource =
        !m_gizmoAxisX->VertexStreamSource() || !m_gizmoAxisY->VertexStreamSource() || !m_gizmoAxisZ->VertexStreamSource()
        || !m_gizmoAxisXAux->VertexStreamSource() || !m_gizmoAxisYAux->VertexStreamSource() || !m_gizmoAxisZAux->VertexStreamSource();
    if (!MissingSource && m_gizmoGeometryMode == m_mode)
    {
        return;
    }

    const auto MakeUnitBox = []() -> std::shared_ptr<SnAPI::Graphics::IVertexStreamSource>
    {
        auto Source = std::make_shared<SnAPI::Graphics::BoxStreamSource>();
        if (Source)
        {
            Source->SetSize(1.0f, 1.0f, 1.0f);
        }
        return Source;
    };
    const auto MakeUnitCone = []() -> std::shared_ptr<SnAPI::Graphics::IVertexStreamSource>
    {
        return std::make_shared<SnAPI::Graphics::ConeStreamSource>(0.32f, 1.0f, 24u);
    };
    const auto MakeUnitHoop = []() -> std::shared_ptr<SnAPI::Graphics::IVertexStreamSource>
    {
        return std::make_shared<EditorTorusStreamSource>(0.5f, 0.07f, 56u, 12u);
    };

    switch (m_mode)
    {
    case EEditorTransformMode::Translate:
        m_gizmoAxisX->SetVertexStreamSource(MakeUnitBox());
        m_gizmoAxisY->SetVertexStreamSource(MakeUnitBox());
        m_gizmoAxisZ->SetVertexStreamSource(MakeUnitBox());
        m_gizmoAxisXAux->SetVertexStreamSource(MakeUnitCone());
        m_gizmoAxisYAux->SetVertexStreamSource(MakeUnitCone());
        m_gizmoAxisZAux->SetVertexStreamSource(MakeUnitCone());
        break;
    case EEditorTransformMode::Rotate:
        m_gizmoAxisX->SetVertexStreamSource(MakeUnitHoop());
        m_gizmoAxisY->SetVertexStreamSource(MakeUnitHoop());
        m_gizmoAxisZ->SetVertexStreamSource(MakeUnitHoop());
        // Keep aux objects valid; rotate mode only queues primary hoop objects.
        m_gizmoAxisXAux->SetVertexStreamSource(MakeUnitBox());
        m_gizmoAxisYAux->SetVertexStreamSource(MakeUnitBox());
        m_gizmoAxisZAux->SetVertexStreamSource(MakeUnitBox());
        break;
    case EEditorTransformMode::Scale:
        m_gizmoAxisX->SetVertexStreamSource(MakeUnitBox());
        m_gizmoAxisY->SetVertexStreamSource(MakeUnitBox());
        m_gizmoAxisZ->SetVertexStreamSource(MakeUnitBox());
        m_gizmoAxisXAux->SetVertexStreamSource(MakeUnitBox());
        m_gizmoAxisYAux->SetVertexStreamSource(MakeUnitBox());
        m_gizmoAxisZAux->SetVertexStreamSource(MakeUnitBox());
        break;
    default:
        break;
    }

    m_gizmoGeometryMode = m_mode;
}

void EditorTransformInteractionService::QueueTransformGizmos(EditorServiceContext& Context,
                                                             BaseNode* SelectedNode,
                                                             const NodeTransform& SelectedTransform,
                                                             SnAPI::Graphics::ICamera& Camera,
                                                             const std::uint64_t ViewportID)
{
    m_gizmoAxisXID = 0;
    m_gizmoAxisYID = 0;
    m_gizmoAxisZID = 0;
    m_gizmoAxisXAuxID = 0;
    m_gizmoAxisYAuxID = 0;
    m_gizmoAxisZAuxID = 0;

    if (ViewportID == 0)
    {
        return;
    }

    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return;
    }

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.IsInitialized())
    {
        return;
    }

    EnsureGizmoRenderObjects();
    ConfigureGizmoGeometryForMode();
    if (!m_gizmoAxisX || !m_gizmoAxisY || !m_gizmoAxisZ
        || !m_gizmoAxisXAux || !m_gizmoAxisYAux || !m_gizmoAxisZAux)
    {
        return;
    }

    Vec3 BasisX = Vec3::UnitX();
    Vec3 BasisY = Vec3::UnitY();
    Vec3 BasisZ = Vec3::UnitZ();
    switch (m_space)
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
        BasisX = NormalizeOrAxis(Camera.Right().template cast<SnAPI::Math::Scalar>(), Vec3::UnitX());
        BasisY = NormalizeOrAxis(Camera.Up().template cast<SnAPI::Math::Scalar>(), Vec3::UnitY());
        BasisZ = NormalizeOrAxis(Camera.Forward().template cast<SnAPI::Math::Scalar>(), Vec3::UnitZ());
        break;
    case EditorLayout::EGizmoSpace::World:
    default:
        break;
    }

    const std::array<Vec3, 3> AxisBasis{
        NormalizeOrAxis(BasisX, Vec3::UnitX()),
        NormalizeOrAxis(BasisY, Vec3::UnitY()),
        NormalizeOrAxis(BasisZ, Vec3::UnitZ())};
    const std::array<SnAPI::Math::Scalar, 3> AxisEdgeExtents = (SelectedNode != nullptr)
        ? ResolveSelectedObjectAxisExtents(*SelectedNode, AxisBasis, SelectedTransform.Scale)
        : ResolveFallbackAxisExtents(AxisBasis, SelectedTransform.Scale);
    const SnAPI::Math::Scalar SelectedObjectRadius = std::max({
        AxisEdgeExtents[0],
        AxisEdgeExtents[1],
        AxisEdgeExtents[2],
        static_cast<SnAPI::Math::Scalar>(0.1)});
    const auto CameraPosition = Camera.Position().template cast<SnAPI::Math::Scalar>();
    const SnAPI::Math::Scalar CameraDistance = std::max<SnAPI::Math::Scalar>(
        static_cast<SnAPI::Math::Scalar>(0.25),
        (SelectedTransform.Position - CameraPosition).norm());
    const SnAPI::Math::Scalar BoundsMargin = std::clamp(
        SelectedObjectRadius * static_cast<SnAPI::Math::Scalar>(0.010),
        static_cast<SnAPI::Math::Scalar>(0.004),
        static_cast<SnAPI::Math::Scalar>(0.03));
    const SnAPI::Math::Scalar AxisLength = std::max<SnAPI::Math::Scalar>(
        CameraDistance * static_cast<SnAPI::Math::Scalar>(0.08),
        std::max<SnAPI::Math::Scalar>(
            static_cast<SnAPI::Math::Scalar>(0.12),
            (SelectedObjectRadius * static_cast<SnAPI::Math::Scalar>(0.24))));
    const SnAPI::Math::Scalar AxisThickness = std::max<SnAPI::Math::Scalar>(
        static_cast<SnAPI::Math::Scalar>(0.016),
        AxisLength * static_cast<SnAPI::Math::Scalar>(0.085));

    const auto ToRendererVec3 = [](const Vec3& Value)
    {
        return SnAPI::Vector3D{
            static_cast<SnAPI::Vector3D::Scalar>(Value.x()),
            static_cast<SnAPI::Vector3D::Scalar>(Value.y()),
            static_cast<SnAPI::Vector3D::Scalar>(Value.z())};
    };

    const auto ToRendererQuat = [](const Quat& Value)
    {
        SnAPI::Quaternion Rotation = SnAPI::Quaternion::Identity();
        Rotation.x() = static_cast<SnAPI::Quaternion::Scalar>(Value.x());
        Rotation.y() = static_cast<SnAPI::Quaternion::Scalar>(Value.y());
        Rotation.z() = static_cast<SnAPI::Quaternion::Scalar>(Value.z());
        Rotation.w() = static_cast<SnAPI::Quaternion::Scalar>(Value.w());
        if (Rotation.squaredNorm() > 0.0)
        {
            Rotation.normalize();
        }
        else
        {
            Rotation = SnAPI::Quaternion::Identity();
        }
        return Rotation;
    };

    const auto AxisDirection = [&](const EActiveAxis Axis) -> Vec3
    {
        switch (Axis)
        {
        case EActiveAxis::X:
            return BasisX;
        case EActiveAxis::Y:
            return BasisY;
        case EActiveAxis::Z:
            return BasisZ;
        case EActiveAxis::None:
        default:
            return Vec3::UnitX();
        }
    };

    const auto AxisEdgeExtentForAxis = [&](const EActiveAxis Axis) -> SnAPI::Math::Scalar
    {
        switch (Axis)
        {
        case EActiveAxis::X:
            return AxisEdgeExtents[0];
        case EActiveAxis::Y:
            return AxisEdgeExtents[1];
        case EActiveAxis::Z:
            return AxisEdgeExtents[2];
        case EActiveAxis::None:
        default:
            return SelectedObjectRadius;
        }
    };

    const auto RingRadiusForAxis = [&](const EActiveAxis Axis) -> SnAPI::Math::Scalar
    {
        switch (Axis)
        {
        case EActiveAxis::X:
            return std::max(AxisEdgeExtents[1], AxisEdgeExtents[2]);
        case EActiveAxis::Y:
            return std::max(AxisEdgeExtents[0], AxisEdgeExtents[2]);
        case EActiveAxis::Z:
            return std::max(AxisEdgeExtents[0], AxisEdgeExtents[1]);
        case EActiveAxis::None:
        default:
            return SelectedObjectRadius;
        }
    };

    const auto BuildAlignedTransform = [&](const Vec3& Center,
                                           const Vec3& Direction,
                                           const Vec3& LocalScale) -> SnAPI::Matrix4
    {
        const Quat AxisRotation = RotationFromTo(Vec3::UnitY(), Direction);
        auto Transform = SnAPI::Transform3D::Identity();
        Transform.translate(ToRendererVec3(Center));
        Transform.rotate(ToRendererQuat(AxisRotation));
        Transform.scale(ToRendererVec3(LocalScale));
        return Transform.matrix();
    };

    const auto QueueGizmoObject = [&](const std::shared_ptr<SnAPI::Graphics::IRenderObject>& GizmoObject,
                                      std::uint32_t AxisTag,
                                      std::uint32_t& OutObjectID)
    {
        OutObjectID = 0;
        if (!GizmoObject)
        {
            return;
        }

        RendererSystem::EditorImmediateRenderMetadata Metadata{};
        Metadata.IsGizmo = (AxisTag != 0u);
        Metadata.AxisTag = AxisTag;

        const bool QueuedInEditorIDPass = Renderer.QueueEditorImmediateRenderObject(
            GizmoObject,
            ViewportID,
            SnAPI::Graphics::ERenderPassType::EditorID,
            Metadata);
        const bool QueuedInOverlayPass = Renderer.QueueEditorImmediateRenderObject(
            GizmoObject,
            ViewportID,
            SnAPI::Graphics::ERenderPassType::EditorOverlay,
            Metadata);
        if (!QueuedInEditorIDPass && !QueuedInOverlayPass)
        {
            return;
        }

        OutObjectID = Renderer.RenderObjectID(GizmoObject).value_or(0u);
    };

    const auto ApplyAxisTransform = [&](const EActiveAxis Axis,
                                        const std::shared_ptr<SnAPI::Graphics::IRenderObject>& PrimaryObject,
                                        const std::shared_ptr<SnAPI::Graphics::IRenderObject>& AuxObject)
    {
        const Vec3 Direction = NormalizeOrAxis(AxisDirection(Axis), Vec3::UnitY());
        const SnAPI::Math::Scalar AxisStartOffset = AxisEdgeExtentForAxis(Axis);

        if (m_mode == EEditorTransformMode::Rotate)
        {
            const SnAPI::Math::Scalar RingRadius = std::max<SnAPI::Math::Scalar>(
                RingRadiusForAxis(Axis) + BoundsMargin,
                CameraDistance * static_cast<SnAPI::Math::Scalar>(0.14));
            if (PrimaryObject)
            {
                PrimaryObject->SetWorldTransform(BuildAlignedTransform(
                    SelectedTransform.Position,
                    Direction,
                    Vec3{
                        static_cast<Vec3::Scalar>(RingRadius),
                        static_cast<Vec3::Scalar>(RingRadius * static_cast<SnAPI::Math::Scalar>(0.12)),
                        static_cast<Vec3::Scalar>(RingRadius)}));
            }
            return;
        }

        if (m_mode == EEditorTransformMode::Translate)
        {
            const SnAPI::Math::Scalar ShaftLength = AxisLength;
            const SnAPI::Math::Scalar HeadLength = std::max<SnAPI::Math::Scalar>(
                static_cast<SnAPI::Math::Scalar>(0.10),
                AxisLength * static_cast<SnAPI::Math::Scalar>(0.28));
            const SnAPI::Math::Scalar HeadRadius = std::max<SnAPI::Math::Scalar>(
                AxisThickness * static_cast<SnAPI::Math::Scalar>(2.5),
                HeadLength * static_cast<SnAPI::Math::Scalar>(0.20));

            const Vec3 ShaftCenter = SelectedTransform.Position + Direction * (AxisStartOffset + ShaftLength * static_cast<SnAPI::Math::Scalar>(0.5));
            const Vec3 HeadCenter = SelectedTransform.Position + Direction * (AxisStartOffset + ShaftLength + HeadLength * static_cast<SnAPI::Math::Scalar>(0.5));

            if (PrimaryObject)
            {
                PrimaryObject->SetWorldTransform(BuildAlignedTransform(
                    ShaftCenter,
                    Direction,
                    Vec3{
                        static_cast<Vec3::Scalar>(AxisThickness),
                        static_cast<Vec3::Scalar>(ShaftLength),
                        static_cast<Vec3::Scalar>(AxisThickness)}));
            }
            if (AuxObject)
            {
                AuxObject->SetWorldTransform(BuildAlignedTransform(
                    HeadCenter,
                    Direction,
                    Vec3{
                        static_cast<Vec3::Scalar>(HeadRadius * static_cast<SnAPI::Math::Scalar>(2.0)),
                        static_cast<Vec3::Scalar>(HeadLength),
                        static_cast<Vec3::Scalar>(HeadRadius * static_cast<SnAPI::Math::Scalar>(2.0))}));
            }
            return;
        }

        const SnAPI::Math::Scalar ShaftLength = AxisLength * static_cast<SnAPI::Math::Scalar>(0.62);
        const SnAPI::Math::Scalar HandleLength = std::max<SnAPI::Math::Scalar>(
            AxisThickness * static_cast<SnAPI::Math::Scalar>(2.6),
            AxisLength * static_cast<SnAPI::Math::Scalar>(0.18));
        const SnAPI::Math::Scalar HandleThickness = std::max<SnAPI::Math::Scalar>(
            AxisThickness * static_cast<SnAPI::Math::Scalar>(2.8),
            AxisLength * static_cast<SnAPI::Math::Scalar>(0.13));

        const Vec3 ShaftCenter = SelectedTransform.Position + Direction * (AxisStartOffset + ShaftLength * static_cast<SnAPI::Math::Scalar>(0.5));
        const Vec3 HandleCenter = SelectedTransform.Position + Direction * (AxisStartOffset + ShaftLength + HandleLength * static_cast<SnAPI::Math::Scalar>(0.5));
        if (PrimaryObject)
        {
            PrimaryObject->SetWorldTransform(BuildAlignedTransform(
                ShaftCenter,
                Direction,
                Vec3{
                    static_cast<Vec3::Scalar>(AxisThickness * static_cast<SnAPI::Math::Scalar>(0.9)),
                    static_cast<Vec3::Scalar>(ShaftLength),
                    static_cast<Vec3::Scalar>(AxisThickness * static_cast<SnAPI::Math::Scalar>(0.9))}));
        }
        if (AuxObject)
        {
            AuxObject->SetWorldTransform(BuildAlignedTransform(
                HandleCenter,
                Direction,
                Vec3{
                    static_cast<Vec3::Scalar>(HandleThickness * static_cast<SnAPI::Math::Scalar>(1.1)),
                    static_cast<Vec3::Scalar>(HandleLength),
                    static_cast<Vec3::Scalar>(HandleThickness * static_cast<SnAPI::Math::Scalar>(1.1))}));
        }
    };

    ApplyAxisTransform(EActiveAxis::X, m_gizmoAxisX, m_gizmoAxisXAux);
    ApplyAxisTransform(EActiveAxis::Y, m_gizmoAxisY, m_gizmoAxisYAux);
    ApplyAxisTransform(EActiveAxis::Z, m_gizmoAxisZ, m_gizmoAxisZAux);

    QueueGizmoObject(m_gizmoAxisX, 1u, m_gizmoAxisXID);
    QueueGizmoObject(m_gizmoAxisY, 2u, m_gizmoAxisYID);
    QueueGizmoObject(m_gizmoAxisZ, 3u, m_gizmoAxisZID);
    if (m_mode == EEditorTransformMode::Translate || m_mode == EEditorTransformMode::Scale)
    {
        QueueGizmoObject(m_gizmoAxisXAux, 1u, m_gizmoAxisXAuxID);
        QueueGizmoObject(m_gizmoAxisYAux, 2u, m_gizmoAxisYAuxID);
        QueueGizmoObject(m_gizmoAxisZAux, 3u, m_gizmoAxisZAuxID);
    }
}

EditorTransformInteractionService::EActiveAxis EditorTransformInteractionService::PickGizmoAxis(EditorServiceContext& Context,
                                                                                                 const float ScreenX,
                                                                                                 const float ScreenY,
                                                                                                 const SnAPI::UI::UIRect& ViewRect,
                                                                                                 const std::uint64_t ViewportID) const
{
    if (ViewportID == 0 || ViewRect.W <= 0.0f || ViewRect.H <= 0.0f || !std::isfinite(ScreenX) || !std::isfinite(ScreenY))
    {
        return EActiveAxis::None;
    }

    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return EActiveAxis::None;
    }

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.IsInitialized())
    {
        return EActiveAxis::None;
    }

    const float U = (ScreenX - ViewRect.X) / ViewRect.W;
    const float V = (ScreenY - ViewRect.Y) / ViewRect.H;
    if (!std::isfinite(U) || !std::isfinite(V))
    {
        return EActiveAxis::None;
    }

    const auto RenderObjectID = Renderer.ReadRenderViewportObjectID(ViewportID, U, V);
    if (!RenderObjectID.has_value() || *RenderObjectID == 0u)
    {
        return EActiveAxis::None;
    }

    if (m_gizmoAxisXID != 0 && *RenderObjectID == m_gizmoAxisXID)
    {
        return EActiveAxis::X;
    }
    if (m_gizmoAxisXAuxID != 0 && *RenderObjectID == m_gizmoAxisXAuxID)
    {
        return EActiveAxis::X;
    }
    if (m_gizmoAxisYID != 0 && *RenderObjectID == m_gizmoAxisYID)
    {
        return EActiveAxis::Y;
    }
    if (m_gizmoAxisYAuxID != 0 && *RenderObjectID == m_gizmoAxisYAuxID)
    {
        return EActiveAxis::Y;
    }
    if (m_gizmoAxisZID != 0 && *RenderObjectID == m_gizmoAxisZID)
    {
        return EActiveAxis::Z;
    }
    if (m_gizmoAxisZAuxID != 0 && *RenderObjectID == m_gizmoAxisZAuxID)
    {
        return EActiveAxis::Z;
    }

    return EActiveAxis::None;
}
#endif

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
#if defined(SNAPI_GF_ENABLE_RENDERER)
    m_gizmoAxisXID = 0;
    m_gizmoAxisYID = 0;
    m_gizmoAxisZID = 0;
    m_gizmoAxisXAuxID = 0;
    m_gizmoAxisYAuxID = 0;
    m_gizmoAxisZAuxID = 0;
    m_gizmoGeometryMode = EEditorTransformMode::Translate;
#endif
    return Ok();
}

void EditorTransformInteractionService::Tick(EditorServiceContext& Context, const float DeltaSeconds)
{
    (void)DeltaSeconds;

#if !defined(SNAPI_GF_ENABLE_INPUT) || !defined(SNAPI_GF_ENABLE_RENDERER) || !defined(SNAPI_GF_ENABLE_UI)
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

    if (auto* PieService = Context.GetService<EditorPieService>(); PieService && PieService->IsSessionActive())
    {
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
    const auto QueueGizmosForCurrentFrame = [&]()
    {
        if (ViewportID != 0)
        {
            QueueTransformGizmos(Context, Node, TransformWorld, *Camera, ViewportID);
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

    const float MouseX = Snapshot->Mouse().X;
    const float MouseY = Snapshot->Mouse().Y;
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
            m_activeAxis = PickGizmoAxis(Context, MouseX, MouseY, ViewRect, ViewportID);
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
#if defined(SNAPI_GF_ENABLE_RENDERER)
    m_gizmoAxisX.reset();
    m_gizmoAxisY.reset();
    m_gizmoAxisZ.reset();
    m_gizmoAxisXAux.reset();
    m_gizmoAxisYAux.reset();
    m_gizmoAxisZAux.reset();
    m_gizmoAxisXID = 0;
    m_gizmoAxisYID = 0;
    m_gizmoAxisZID = 0;
    m_gizmoAxisXAuxID = 0;
    m_gizmoAxisYAuxID = 0;
    m_gizmoAxisZAuxID = 0;
    m_gizmoGeometryMode = EEditorTransformMode::Translate;
#endif
}

} // namespace SnAPI::GameFramework::Editor
