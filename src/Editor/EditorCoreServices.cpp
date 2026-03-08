#include "Editor/EditorCoreServices.h"

#include "AssetRef.h"
#include "BaseNode.h"
#include "AssetPipelineIds.h"
#include "CameraComponent.h"
#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_RENDERER)
#include "Editor/EditorCameraComponent.h"
#endif
#include "InputSystem.h"
#include "Level.h"
#include "NodeCast.h"
#include "PawnBase.h"
#include "RenderAssetRuntime.h"
#include "PlayerStart.h"
#include "SkeletalMeshComponent.h"
#include "Serialization.h"
#include "StaticMeshComponent.h"
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
#include <TextureCompressorIds.h>

#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
#endif

#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "RigidBodyComponent.h"
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
#include <PrimitiveStreamSources.hpp>
#include "ICamera.hpp"
#include "IRenderObject.hpp"
#include "LinearAlgebra.hpp"
#include <MeshRenderObject.hpp>
#include "WindowBase.hpp"
#endif

#include <SnAPI/Math/LinearAlgebra.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <numbers>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "GameRuntime.h"

#if defined(SNAPI_GF_ENABLE_RENDERER) && __has_include(<SDL3/SDL.h>)
#include <SDL3/SDL.h>
#define SNAPI_GF_EDITOR_HAS_SDL3 1
#else
#define SNAPI_GF_EDITOR_HAS_SDL3 0
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

[[nodiscard]] BaseNode* ResolveNodeFromHandle(const NodeHandle Handle, World& WorldRef)
{
    if (Handle.IsNull())
    {
        return nullptr;
    }

    if (auto* Node = Handle.Borrowed())
    {
        return Node;
    }

    if (auto* Node = Handle.BorrowedSlowByUuid())
    {
        return Node;
    }

    if (const auto HandleResult = WorldRef.NodeHandleById(Handle.Id); HandleResult.has_value())
    {
        return WorldRef.NodePool().Borrowed(*HandleResult);
    }

    return nullptr;
}

void InitializeCreatedNodeDefaults(IWorld& WorldRef, BaseNode& Node)
{
    if (NodeCast<PlayerStart>(&Node) != nullptr)
    {
        (void)WorldRef.RequestNodeOnCreate(Node.Handle());
    }

    if (NodeCast<PawnBase>(&Node) != nullptr)
    {
        (void)WorldRef.RequestNodeOnCreate(Node.Handle());
    }
}

void SetEditorCameraEnabledForPie(World& WorldRef, const bool Enabled)
{
#if defined(SNAPI_GF_ENABLE_INPUT) && defined(SNAPI_GF_ENABLE_RENDERER)
    WorldRef.NodePool().ForEach([Enabled](const NodeHandle&, BaseNode& Node) {
        auto EditorCamera = Node.Component<EditorCameraComponent>();
        if (!EditorCamera)
        {
            return;
        }

        EditorCamera->EditSettings().Enabled = Enabled;

        auto Camera = Node.Component<CameraComponent>();
        if (!Camera)
        {
            return;
        }

        Camera->SetActive(Enabled);
    });
#else
    (void)WorldRef;
    (void)Enabled;
#endif
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

        auto DestroyResult = WorldPtr->DestroyNode(TargetNode->Handle());
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

        auto RemoveResult = WorldPtr->RemoveComponentByType(TargetNode->Handle(), Request.Type);
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
            auto AttachResult = WorldPtr->AttachChild(ParentNode->Handle(), *CreateResult);
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

    auto CreateComponentResult = WorldPtr->CreateComponent(TargetNode->Handle(), Type->Id);
    if (!CreateComponentResult)
    {
        return std::unexpected(CreateComponentResult.error());
    }

    return Ok();
}

#if defined(SNAPI_GF_ENABLE_PHYSICS)
[[nodiscard]] std::optional<NodeHandle> ResolveNodeHandleByPhysicsBody(World& WorldRef,
                                                                        const SnAPI::Physics::BodyHandle& TargetBody)
{
    std::optional<NodeHandle> ResolvedHandle{};
    WorldRef.NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
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
    WorldRef.NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
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

Result EditorSceneService::EnsureEditorCamera(EditorServiceContext& Context)
{
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }
    return m_scene.EnsureEditorCamera(*WorldPtr);
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

std::string_view EditorPieService::Name() const
{
    return "EditorPieService";
}

Result EditorPieService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_state = EState::Stopped;
    m_editorSnapshot.reset();
    m_editorWorldKind = EWorldKind::Editor;
    m_editorExecutionProfile = WorldExecutionProfile::Editor();
    return Ok();
}

void EditorPieService::Shutdown(EditorServiceContext& Context)
{
    (void)StopSession(Context);
    m_state = EState::Stopped;
    m_editorSnapshot.reset();
}

Result EditorPieService::Play(EditorServiceContext& Context)
{
    if (m_state == EState::Playing)
    {
        return Ok();
    }

    if (m_state == EState::Paused)
    {
        return ResumeSession(Context);
    }

    return StartSession(Context);
}

Result EditorPieService::Pause(EditorServiceContext& Context)
{
    if (m_state != EState::Playing)
    {
        return Ok();
    }

    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    WorldPtr->SetWorldKind(EWorldKind::PIE);
    WorldPtr->SetExecutionProfile(PausedExecutionProfile());
    m_state = EState::Paused;
    return Ok();
}

Result EditorPieService::Stop(EditorServiceContext& Context)
{
    if (m_state == EState::Stopped)
    {
        return Ok();
    }

    return StopSession(Context);
}

Result EditorPieService::StartSession(EditorServiceContext& Context)
{
    auto& Runtime = Context.Runtime();
    Runtime.StopGameplayHost();

    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    auto SnapshotResult = WorldSerializer::Serialize(*WorldPtr);
    if (!SnapshotResult)
    {
        return std::unexpected(SnapshotResult.error());
    }

    m_editorSnapshot = std::move(*SnapshotResult);
    m_editorWorldKind = WorldPtr->Kind();
    m_editorExecutionProfile = WorldPtr->ExecutionProfile();

    WorldPtr->SetWorldKind(EWorldKind::PIE);
    WorldPtr->SetExecutionProfile(WorldExecutionProfile::PIE());

#if defined(SNAPI_GF_ENABLE_RENDERER)
    (void)WorldPtr->Renderer().SetActiveCamera(nullptr);
#endif

    TDeserializeOptions PieOptions{};
    PieOptions.RegenerateObjectIds = true;
    auto PieLoadResult = WorldSerializer::Deserialize(*m_editorSnapshot, *WorldPtr, PieOptions);
    if (!PieLoadResult)
    {
        TDeserializeOptions RestoreOptions{};
        RestoreOptions.RegenerateObjectIds = false;
        (void)WorldSerializer::Deserialize(*m_editorSnapshot, *WorldPtr, RestoreOptions);
        WorldPtr->SetWorldKind(m_editorWorldKind);
        WorldPtr->SetExecutionProfile(m_editorExecutionProfile);
        return std::unexpected(PieLoadResult.error());
    }

    SetEditorCameraEnabledForPie(*WorldPtr, false);

    if (Runtime.Settings().Gameplay.has_value())
    {
        auto StartGameplayResult = Runtime.StartGameplayHost();
        if (!StartGameplayResult)
        {
            TDeserializeOptions RestoreOptions{};
            RestoreOptions.RegenerateObjectIds = false;
            (void)WorldSerializer::Deserialize(*m_editorSnapshot, *WorldPtr, RestoreOptions);
            WorldPtr->SetWorldKind(m_editorWorldKind);
            WorldPtr->SetExecutionProfile(m_editorExecutionProfile);
            return std::unexpected(StartGameplayResult.error());
        }
    }

    m_state = EState::Playing;
    return Ok();
}

Result EditorPieService::ResumeSession(EditorServiceContext& Context)
{
    auto& Runtime = Context.Runtime();
    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    WorldPtr->SetWorldKind(EWorldKind::PIE);
    WorldPtr->SetExecutionProfile(WorldExecutionProfile::PIE());
    SetEditorCameraEnabledForPie(*WorldPtr, false);

    if (Runtime.Settings().Gameplay.has_value())
    {
        auto StartGameplayResult = Runtime.StartGameplayHost();
        if (!StartGameplayResult)
        {
            return std::unexpected(StartGameplayResult.error());
        }
    }

    m_state = EState::Playing;
    return Ok();
}

Result EditorPieService::StopSession(EditorServiceContext& Context)
{
    auto& Runtime = Context.Runtime();
    Runtime.StopGameplayHost();

    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        m_state = EState::Stopped;
        m_editorSnapshot.reset();
        return std::unexpected(MakeError(EErrorCode::NotReady, "Runtime world is not available"));
    }

    if (!m_editorSnapshot.has_value())
    {
        WorldPtr->SetWorldKind(m_editorWorldKind);
        WorldPtr->SetExecutionProfile(m_editorExecutionProfile);
        SetEditorCameraEnabledForPie(*WorldPtr, true);
        m_state = EState::Stopped;
        return Ok();
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    (void)WorldPtr->Renderer().SetActiveCamera(nullptr);
#endif

    TDeserializeOptions RestoreOptions{};
    RestoreOptions.RegenerateObjectIds = false;
    auto RestoreResult = WorldSerializer::Deserialize(*m_editorSnapshot, *WorldPtr, RestoreOptions);
    if (!RestoreResult)
    {
        return std::unexpected(RestoreResult.error());
    }

    WorldPtr->SetWorldKind(m_editorWorldKind);
    WorldPtr->SetExecutionProfile(m_editorExecutionProfile);
    m_editorSnapshot.reset();
    m_state = EState::Stopped;
    return Ok();
}

WorldExecutionProfile EditorPieService::PausedExecutionProfile()
{
    auto Profile = WorldExecutionProfile::PIE();
    Profile.RunGameplay = false;
    Profile.TickPhysicsSimulation = false;
    Profile.TickAudio = false;
    Profile.PumpNetworking = false;
    return Profile;
}

struct EditorAssetIconService::TextureBinding
{
    ::SnAPI::AssetPipeline::AssetId AssetId{};
    const SnAPI::UI::UIContext* Context = nullptr;
    std::uint32_t TextureId = 0;
    std::uint32_t TextureWidth = 0;
    std::uint32_t TextureHeight = 0;
    ::SnAPI::AssetPipeline::AssetHandle<RuntimeTextureAsset> RuntimeTexture{};
};

EditorAssetIconService::~EditorAssetIconService() = default;

std::string_view EditorAssetIconService::Name() const
{
    return "EditorAssetIconService";
}

std::vector<std::type_index> EditorAssetIconService::Dependencies() const
{
    return {std::type_index(typeid(EditorAssetService))};
}

Result EditorAssetIconService::Initialize(EditorServiceContext& Context)
{
    (void)Context;
    m_boundContext = nullptr;
    m_textureBindingsByAssetKey.clear();
    m_nextTextureId = 0x70000000u;
    ++m_revision;
    return Ok();
}

void EditorAssetIconService::Shutdown(EditorServiceContext& Context)
{
    ResetAllBindings(Context);
    m_boundContext = nullptr;
}

void EditorAssetIconService::Synchronize(EditorServiceContext& Context,
                                         const std::vector<EditorAssetService::DiscoveredAsset>& Assets,
                                         const SnAPI::UI::UIContext* UiContext)
{
    if (UiContext != m_boundContext)
    {
        ResetAllBindings(Context);
        m_boundContext = UiContext;
    }

    if (m_textureBindingsByAssetKey.empty())
    {
        return;
    }

    std::unordered_map<std::string, const EditorAssetService::DiscoveredAsset*> TextureAssetsByKey{};
    TextureAssetsByKey.reserve(Assets.size());
    for (const auto& Asset : Assets)
    {
        if (Asset.AssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture)
        {
            TextureAssetsByKey.emplace(Asset.Key, &Asset);
        }
    }

    std::vector<std::string> KeysToRemove{};
    KeysToRemove.reserve(m_textureBindingsByAssetKey.size());

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto* WorldPtr = Context.Runtime().WorldPtr();
#endif

    for (auto& [AssetKey, Binding] : m_textureBindingsByAssetKey)
    {
        const auto AssetIt = TextureAssetsByKey.find(AssetKey);
        if (AssetIt == TextureAssetsByKey.end() || !Binding || Binding->TextureId == 0)
        {
            KeysToRemove.push_back(AssetKey);
            continue;
        }
        if (UiContext == nullptr || Binding->Context != UiContext)
        {
            KeysToRemove.push_back(AssetKey);
            continue;
        }

#if defined(SNAPI_GF_ENABLE_RENDERER)
        const auto& Asset = *AssetIt->second;

        TAssetRef<RuntimeTextureAsset> TextureRef{};
        TextureRef.EditAssetName() = Asset.Name;
        TextureRef.EditAssetId() = Asset.AssetId.ToString();
        if (auto TextureResult = TextureRef.GetShared<RuntimeTextureAsset>(); TextureResult && TextureResult->Get())
        {
            Binding->AssetId = Asset.AssetId;
            Binding->RuntimeTexture = *TextureResult;
        }

        auto* Image = Binding->RuntimeTexture.Get();
        if (!WorldPtr || !Image ||
            !WorldPtr->Renderer().RegisterExternalImageUiTexture(*UiContext, Binding->TextureId, Image, true))
        {
            KeysToRemove.push_back(AssetKey);
        }
#else
        (void)AssetIt;
#endif
    }

    for (const auto& AssetKey : KeysToRemove)
    {
        RemoveBinding(Context, AssetKey);
    }
}

void EditorAssetIconService::InvalidateAsset(EditorServiceContext& Context, std::string_view AssetKey)
{
    if (AssetKey.empty())
    {
        return;
    }
    RemoveBinding(Context, AssetKey);
}

EditorAssetIconService::AssetIconMetadata EditorAssetIconService::ResolveAssetIcon(
    EditorServiceContext& Context,
    const EditorAssetService::DiscoveredAsset& Asset,
    const SnAPI::UI::UIContext* UiContext)
{
    AssetIconMetadata Metadata = BuildFallbackIcon(Asset);
    if (!UiContext || Asset.AssetKind != TextureCompressorPlugin::AssetKind_CompressedTexture)
    {
        return Metadata;
    }

    if (UiContext != m_boundContext)
    {
        ResetAllBindings(Context);
        m_boundContext = UiContext;
    }

    if (const auto ExistingIt = m_textureBindingsByAssetKey.find(Asset.Key);
        ExistingIt != m_textureBindingsByAssetKey.end())
    {
        const TextureBinding& Existing = *ExistingIt->second;
        if (Existing.AssetId == Asset.AssetId && Existing.Context == UiContext && Existing.TextureId != 0)
        {
            Metadata.TextureId = Existing.TextureId;
            Metadata.TextureWidth = Existing.TextureWidth;
            Metadata.TextureHeight = Existing.TextureHeight;
            return Metadata;
        }
        RemoveBinding(Context, Asset.Key);
    }

    TAssetRef<RuntimeTextureAsset> TextureRef{};
    TextureRef.EditAssetName() = Asset.Name;
    TextureRef.EditAssetId() = Asset.AssetId.ToString();
    auto TextureResult = TextureRef.GetShared<RuntimeTextureAsset>();
    if (!TextureResult || !TextureResult->Get())
    {
        return Metadata;
    }

    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (!WorldPtr)
    {
        return Metadata;
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    const std::uint32_t TextureId = AllocateTextureId();
    if (TextureId == 0)
    {
        return Metadata;
    }

    if (!WorldPtr->Renderer().RegisterExternalImageUiTexture(*UiContext, TextureId, TextureResult->Get(), true))
    {
        return Metadata;
    }

    auto Binding = std::make_shared<TextureBinding>();
    const auto Extent = TextureResult->Get()->Extent();
    Binding->AssetId = Asset.AssetId;
    Binding->Context = UiContext;
    Binding->TextureId = TextureId;
    Binding->TextureWidth = Extent.x();
    Binding->TextureHeight = Extent.y();
    Binding->RuntimeTexture = *TextureResult;
    m_textureBindingsByAssetKey[Asset.Key] = std::move(Binding);
    Metadata.TextureId = TextureId;
    Metadata.TextureWidth = Extent.x();
    Metadata.TextureHeight = Extent.y();
    ++m_revision;
#endif
    return Metadata;
}

EditorAssetIconService::AssetIconMetadata EditorAssetIconService::BuildFallbackIcon(
    const EditorAssetService::DiscoveredAsset& Asset) const
{
    AssetIconMetadata Metadata{};
    if (Asset.AssetKind == TextureCompressorPlugin::AssetKind_CompressedTexture)
    {
        Metadata.IconSource = "editor://Assets/sphere.svg";
    }
    else if (Asset.AssetKind == AssetKindMaterial())
    {
        Metadata.IconSource = "editor://Assets/component.svg";
    }
    else if (Asset.AssetKind == AssetKindMaterialInstance())
    {
        Metadata.IconSource = "editor://Assets/box.svg";
    }
    else if (Asset.AssetKind == AssetKindStaticMesh() ||
             Asset.AssetKind == AssetKindSkeletalMesh())
    {
        Metadata.IconSource = "editor://Assets/box.svg";
    }
    else if (Asset.AssetKind == AssetKindLevel())
    {
        Metadata.IconSource = "editor://Assets/level.svg";
    }
    else if (Asset.AssetKind == AssetKindWorld())
    {
        Metadata.IconSource = "editor://Assets/world.svg";
    }
    else
    {
        Metadata.IconSource = "editor://Assets/component.svg";
    }
    return Metadata;
}

std::uint32_t EditorAssetIconService::AllocateTextureId()
{
    // Reserve a high-id range for editor-owned external-image bindings.
    if (m_nextTextureId == 0u)
    {
        m_nextTextureId = 0x70000000u;
    }
    return m_nextTextureId++;
}

void EditorAssetIconService::RemoveBinding(EditorServiceContext& Context, std::string_view AssetKey)
{
    const auto It = m_textureBindingsByAssetKey.find(std::string(AssetKey));
    if (It == m_textureBindingsByAssetKey.end())
    {
        return;
    }

#if defined(SNAPI_GF_ENABLE_RENDERER)
    auto* WorldPtr = Context.Runtime().WorldPtr();
    if (WorldPtr && It->second && It->second->Context && It->second->TextureId != 0)
    {
        (void)WorldPtr->Renderer().UnregisterExternalImageUiTexture(*It->second->Context, It->second->TextureId);
    }
#endif

    m_textureBindingsByAssetKey.erase(It);
    ++m_revision;
}

void EditorAssetIconService::ResetAllBindings(EditorServiceContext& Context)
{
    if (m_textureBindingsByAssetKey.empty())
    {
        return;
    }

    std::vector<std::string> Keys{};
    Keys.reserve(m_textureBindingsByAssetKey.size());
    for (const auto& [AssetKey, _] : m_textureBindingsByAssetKey)
    {
        Keys.push_back(AssetKey);
    }
    for (const auto& AssetKey : Keys)
    {
        RemoveBinding(Context, AssetKey);
    }
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
            std::type_index(typeid(EditorPieService)),
            std::type_index(typeid(EditorRootViewportService)),
            std::type_index(typeid(EditorCommandService)),
            std::type_index(typeid(EditorAssetService)),
            std::type_index(typeid(EditorAssetIconService))};
}

Result EditorLayoutService::Initialize(EditorServiceContext& Context)
{
    auto* ThemeService = Context.GetService<EditorThemeService>();
    auto* SceneService = Context.GetService<EditorSceneService>();
    auto* SelectionService = Context.GetService<EditorSelectionService>();
    auto* PieService = Context.GetService<EditorPieService>();
    auto* AssetService = Context.GetService<EditorAssetService>();
    auto* IconService = Context.GetService<EditorAssetIconService>();
    if (!ThemeService || !SceneService || !SelectionService || !PieService || !AssetService || !IconService)
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
    m_layoutRebuildRequested = false;
    m_assetListSignature = 0;
    m_assetDetailsSignature = 0;
    m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
    m_assetInspectorIconRevision = std::numeric_limits<std::uint64_t>::max();

    const Result BuildResult = m_layout.Build(Context.Runtime(),
                                              ThemeService->Theme(),
                                              SceneService->ActiveCameraComponent(),
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
    if (!SceneService || !SelectionService || !PieService || !AssetService)
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
                Request.StartupLevelPack,
                Request.DefaultRenderSettingsAssetId);
        }

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
        CameraComponent* ActiveCamera = SceneService->ActiveCameraComponent();
        ApplyAssetBrowserState(Context);
        m_layout.Sync(Context.Runtime(), ActiveCamera, &SelectionService->Model(), DeltaSeconds);
        return;
    }

    if (m_hasPendingAssetSelection)
    {
        m_hasPendingAssetSelection = false;

        if (!m_pendingAssetSelectionKey.empty())
        {
            if (AssetService->SelectAssetByKey(m_pendingAssetSelectionKey) && m_pendingAssetSelectionDoubleClick)
            {
                auto OpenEditorResult = AssetService->OpenAssetEditorByKey(m_pendingAssetSelectionKey);
                if (!OpenEditorResult)
                {
                    (void)AssetService->OpenSelectedAssetPreview();
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

    if (m_hasPendingAssetCreateRequest)
    {
        m_hasPendingAssetCreateRequest = false;
        if (!PieService->IsSessionActive() && m_pendingAssetCreateRequest.Type != TypeId{})
        {
            (void)AssetService->CreateRuntimeNodeAssetByType(Context,
                                                             m_pendingAssetCreateRequest.Type,
                                                             m_pendingAssetCreateRequest.Name,
                                                             m_pendingAssetCreateRequest.FolderPath);
        }
        m_pendingAssetCreateRequest = {};
    }

    if (m_hasPendingAssetImportRequest)
    {
        m_hasPendingAssetImportRequest = false;
        if (!PieService->IsSessionActive() && !m_pendingAssetImportRequest.SourcePath.empty())
        {
            (void)AssetService->ImportSourceAsset(Context,
                                                  m_pendingAssetImportRequest.SourcePath,
                                                  m_pendingAssetImportRequest.FolderPath,
                                                  m_pendingAssetImportRequest.BuildOptions,
                                                  m_pendingAssetImportRequest.ImportSettings);
        }
        m_pendingAssetImportRequest = {};
    }

    if (m_hasPendingAssetInspectorSaveRequest)
    {
        m_hasPendingAssetInspectorSaveRequest = false;
        (void)AssetService->SaveActiveAssetEditor();
    }

    if (m_hasPendingAssetInspectorReimportRequest)
    {
        m_hasPendingAssetInspectorReimportRequest = false;
        if (!PieService->IsSessionActive())
        {
            (void)AssetService->ReimportActiveAsset(Context);
        }
    }

    if (m_hasPendingAssetInspectorCloseRequest)
    {
        m_hasPendingAssetInspectorCloseRequest = false;
        AssetService->CloseAssetEditor();
    }

    if (m_hasPendingAssetInspectorNodeSelectionRequest)
    {
        m_hasPendingAssetInspectorNodeSelectionRequest = false;
        (void)AssetService->SelectAssetEditorNode(m_pendingAssetInspectorNodeSelection);
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
            (void)AssetService->AddAssetEditorNode(Request.TargetNode, Request.Type);
            break;
        case EditorLayout::EHierarchyAction::AddComponentType:
            (void)AssetService->AddAssetEditorComponent(Request.TargetNode, Request.Type);
            break;
        case EditorLayout::EHierarchyAction::RemoveComponentType:
            (void)AssetService->RemoveAssetEditorComponent(Request.TargetNode, Request.Type);
            break;
        case EditorLayout::EHierarchyAction::DeleteNode:
            (void)AssetService->DeleteAssetEditorNode(Request.TargetNode);
            break;
        default:
            break;
        }
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
        if (!PieService->IsSessionActive())
        {
            (void)ExecuteHierarchyAction(Context, Request);
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
    CameraComponent* ActiveCamera = SceneService->ActiveCameraComponent();
    AssetService->TickAssetEditorSession(DeltaSeconds);
    ApplyAssetBrowserState(Context);
    m_layout.Sync(Context.Runtime(), ActiveCamera, &SelectionService->Model(), DeltaSeconds);
}

void EditorLayoutService::ApplyAssetBrowserState(EditorServiceContext& Context)
{
    auto* AssetService = Context.GetService<EditorAssetService>();
    auto* IconService = Context.GetService<EditorAssetIconService>();
    if (!AssetService || !IconService)
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
        ProjectState.StartupLevelPack = CurrentProject.StartupLevelPack;
        ProjectState.DefaultRenderSettingsAssetId = CurrentProject.DefaultRenderSettingsAssetId;
        m_layout.SetProjectState(std::move(ProjectState));
    }

    const auto& Assets = AssetService->Assets();
    const SnAPI::UI::UIContext* LayoutContext = m_layout.Context();
    IconService->Synchronize(Context, Assets, LayoutContext);

    std::size_t AssetSignature = ComputeAssetListSignature(Assets);
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
            Entry.IsDirty = Asset.IsDirty;
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
    if (const auto* SelectedAsset = AssetService->SelectedAsset())
    {
        Details.Name = SelectedAsset->Name;
        Details.Type = SelectedAsset->TypeLabel;
        Details.Variant = SelectedAsset->Variant.empty() ? std::string("default") : SelectedAsset->Variant;
        Details.AssetId = SelectedAsset->Key;
        Details.IsRuntime = SelectedAsset->IsRuntime;
        Details.IsDirty = SelectedAsset->IsDirty;
        Details.CanPlace = CanPlaceAssetKind(SelectedAsset->AssetKind);
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

    SceneService->Tick(Context, 0.0f);
    CameraComponent* ActiveCamera = SceneService->ActiveCameraComponent();
    const Result BuildResult = m_layout.Build(Context.Runtime(),
                                              ThemeService->Theme(),
                                              ActiveCamera,
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
    m_layoutRebuildRequested = false;
    m_assetListSignature = 0;
    m_assetDetailsSignature = 0;
    m_assetInspectorSessionRevision = std::numeric_limits<std::uint64_t>::max();
    m_assetInspectorIconRevision = std::numeric_limits<std::uint64_t>::max();
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
    HudPanel.Width().Set(SnAPI::UI::Sizing::Auto());
    HudPanel.Height().Set(SnAPI::UI::Sizing::Auto());
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
    HudStats.Width().Set(SnAPI::UI::Sizing::Auto());
    HudStats.Height().Set(SnAPI::UI::Sizing::Auto());
    HudStats.Gap().Set(12.0f);
    HudStats.Background().Set(SnAPI::UI::Color::Transparent());
    HudStats.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudFrameLabelBuilder = HudStatsBuilder.Add(SnAPI::UI::UIText("Frame: -- ms"));
    auto& HudFrameLabel = HudFrameLabelBuilder.Element();
    HudFrameLabel.Width().Set(SnAPI::UI::Sizing::Auto());
    HudFrameLabel.TextColor().Set(SnAPI::UI::Color{206, 212, 221, 255});
    HudFrameLabel.HAlign().Set(SnAPI::UI::EAlignment::Start);
    HudFrameLabel.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    HudFrameLabel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudFpsLabelBuilder = HudStatsBuilder.Add(SnAPI::UI::UIText("FPS: --"));
    auto& HudFpsLabel = HudFpsLabelBuilder.Element();
    HudFpsLabel.Width().Set(SnAPI::UI::Sizing::Auto());
    HudFpsLabel.TextColor().Set(SnAPI::UI::Color{223, 227, 234, 255});
    HudFpsLabel.HAlign().Set(SnAPI::UI::EAlignment::Start);
    HudFpsLabel.Wrapping().Set(SnAPI::UI::ETextWrapping::Truncate);
    HudFpsLabel.Properties().SetProperty(SnAPI::UI::UIElementBase::VisibilityKey, SnAPI::UI::EVisibility::HitTestInvisible);

    auto HudGraphBuilder = HudPanelBuilder.Add(SnAPI::UI::UIRealtimeGraph("Frame Time / FPS"));
    auto& HudGraph = HudGraphBuilder.Element();
    HudGraph.Width().Set(SnAPI::UI::Sizing::Auto());
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
