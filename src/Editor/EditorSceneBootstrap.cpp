#include "Editor/EditorSceneBootstrap.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "BaseNode.h"
#include "CameraBase.hpp"
#include "CameraComponent.h"
#if defined(SNAPI_GF_ENABLE_INPUT)
#include "Editor/EditorCameraComponent.h"
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
#include "ColliderComponent.h"
#include "RigidBodyComponent.h"
#endif
#include "GameRuntime.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"
#include "World.h"

#include <BoxStreamSource.hpp>
#include <ConeStreamSource.hpp>
#include <PyramidStreamSource.hpp>
#include <SphereStreamSource.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace SnAPI::GameFramework::Editor
{
namespace
{
enum class EPrimitiveKind : std::uint8_t
{
    Box,
    Cone,
    Pyramid,
    Sphere,
};

struct PrimitiveSpawnSpec
{
    std::string Name{};
    Vec3 Position{};
    Quat Rotation = Quat::Identity();
    Vec3 Scale{1.0f, 1.0f, 1.0f};

    EPrimitiveKind VisualKind = EPrimitiveKind::Box;
    Vec3 VisualSize{1.0f, 1.0f, 1.0f};
    float VisualRadius = 0.5f;
    float VisualHeight = 1.0f;
    std::uint32_t VisualSegments = 24u;

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    bool PhysicsEnabled = true;
    SnAPI::Physics::EBodyType BodyType = SnAPI::Physics::EBodyType::Static;
    SnAPI::Physics::EShapeType ColliderShape = SnAPI::Physics::EShapeType::Box;
    Vec3 ColliderHalfExtent{0.5f, 0.5f, 0.5f};
    float ColliderRadius = 0.5f;
    float ColliderHalfHeight = 0.5f;
    float Mass = 1.0f;
    float Friction = 0.8f;
    float Restitution = 0.05f;
    bool EnableCcd = true;
    Vec3 InitialLinearVelocity{};
#endif
};

SnAPI::Vector3DF ToVector3DF(const Vec3& Value)
{
    return SnAPI::Vector3DF{
        static_cast<float>(Value.x()),
        static_cast<float>(Value.y()),
        static_cast<float>(Value.z())};
}

SnAPI::Graphics::SharedVertexStreamSourcePtr CreatePrimitiveSource(const PrimitiveSpawnSpec& Spec)
{
    using namespace SnAPI::Graphics;

    switch (Spec.VisualKind)
    {
    case EPrimitiveKind::Box:
    {
        auto Source = std::make_shared<BoxStreamSource>();
        Source->SetSize(ToVector3DF(Spec.VisualSize));
        return Source;
    }
    case EPrimitiveKind::Cone:
    {
        auto Source = std::make_shared<ConeStreamSource>();
        Source->SetRadius(static_cast<float>(Spec.VisualRadius));
        Source->SetHeight(static_cast<float>(Spec.VisualHeight));
        Source->SetRadialSegments(Spec.VisualSegments);
        return Source;
    }
    case EPrimitiveKind::Pyramid:
    {
        auto Source = std::make_shared<PyramidStreamSource>();
        Source->SetSize(ToVector3DF(Spec.VisualSize));
        return Source;
    }
    case EPrimitiveKind::Sphere:
    {
        auto Source = std::make_shared<SphereStreamSource>();
        Source->SetRadius(static_cast<float>(Spec.VisualRadius));
        const std::uint32_t Longitude = std::max<std::uint32_t>(Spec.VisualSegments, 8u);
        Source->SetSegments(Longitude, std::max<std::uint32_t>(Longitude / 2u, 6u));
        return Source;
    }
    default:
        break;
    }

    return {};
}

bool ConfigureTransform(BaseNode& Node, const PrimitiveSpawnSpec& Spec)
{
    auto TransformResult = Node.Add<TransformComponent>();
    if (!TransformResult)
    {
        return false;
    }

    auto& Transform = *TransformResult;
    Transform.Position = Spec.Position;
    Transform.Rotation = Spec.Rotation;
    Transform.Scale = Spec.Scale;
    return true;
}

bool ConfigureVisual(BaseNode& Node, const PrimitiveSpawnSpec& Spec)
{
    auto MeshResult = Node.Add<StaticMeshComponent>();
    if (!MeshResult)
    {
        return false;
    }

    auto& Mesh = *MeshResult;
    auto& Settings = Mesh.EditSettings();
    Settings.MeshPath.clear();
    Settings.Visible = true;
    Settings.CastShadows = true;
    Settings.SyncFromTransform = true;
    Settings.RegisterWithRenderer = true;
    Mesh.SetVertexStreamSource(CreatePrimitiveSource(Spec));
    return true;
}

#if defined(SNAPI_GF_ENABLE_PHYSICS)
bool ConfigurePhysics(BaseNode& Node, const PrimitiveSpawnSpec& Spec)
{
    if (!Spec.PhysicsEnabled)
    {
        return true;
    }

    auto ColliderResult = Node.Add<ColliderComponent>();
    if (!ColliderResult)
    {
        return false;
    }

    auto& Collider = *ColliderResult;
    auto& ColliderSettings = Collider.EditSettings();
    ColliderSettings.Shape = Spec.ColliderShape;
    ColliderSettings.HalfExtent = Spec.ColliderHalfExtent;
    ColliderSettings.Radius = Spec.ColliderRadius;
    ColliderSettings.HalfHeight = Spec.ColliderHalfHeight;
    ColliderSettings.Density = std::max<float>(Spec.Mass, 0.01f);
    ColliderSettings.Friction = Spec.Friction;
    ColliderSettings.Restitution = Spec.Restitution;
    ColliderSettings.Layer = Spec.BodyType == SnAPI::Physics::EBodyType::Static
                                 ? CollisionLayerFlags(ECollisionFilterBits::WorldStatic)
                                 : CollisionLayerFlags(ECollisionFilterBits::WorldDynamic);
    ColliderSettings.Mask = kCollisionMaskAll;
    ColliderSettings.IsTrigger = false;

    RigidBodyComponent::Settings BodySettings{};
    BodySettings.BodyType = Spec.BodyType;
    BodySettings.Mass = std::max<float>(Spec.Mass, 0.01f);
    BodySettings.EnableCcd = Spec.EnableCcd;
    BodySettings.StartActive = true;
    BodySettings.InitialLinearVelocity = Spec.InitialLinearVelocity;
    BodySettings.InitialAngularVelocity = Vec3{};
    BodySettings.SyncFromPhysics = Spec.BodyType == SnAPI::Physics::EBodyType::Dynamic;
    BodySettings.SyncToPhysics = Spec.BodyType != SnAPI::Physics::EBodyType::Dynamic;
    BodySettings.EnableRenderInterpolation = true;
    BodySettings.AutoDeactivateWhenSleeping = true;

    auto RigidBodyResult = Node.Add<RigidBodyComponent>(BodySettings);
    if (!RigidBodyResult)
    {
        return false;
    }

    return true;
}
#endif

bool SpawnPrimitive(World& WorldRef, const PrimitiveSpawnSpec& Spec, std::vector<NodeHandle>& OutNodes)
{
    auto NodeResult = WorldRef.CreateNode(Spec.Name);
    if (!NodeResult)
    {
        return false;
    }

    NodeHandle Handle = NodeResult.value();
    BaseNode* Node = Handle.Borrowed();
    if (!Node)
    {
        (void)WorldRef.DestroyNode(Handle);
        return false;
    }

    bool Success = ConfigureTransform(*Node, Spec) && ConfigureVisual(*Node, Spec);
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    Success = Success && ConfigurePhysics(*Node, Spec);
#endif
    if (!Success)
    {
        (void)WorldRef.DestroyNode(Handle);
        return false;
    }

    OutNodes.push_back(Handle);
    return true;
}

Quat PitchDegrees(const float Degrees)
{
    return Quat(SnAPI::Math::AngleAxis3D(SnAPI::Math::SLinearAlgebra::DegreesToRadians(static_cast<SnAPI::Math::Scalar>(Degrees)),
                                         SnAPI::Math::Vector3::UnitX()));
}

void BuildPlatformingScene(World& WorldRef, std::vector<NodeHandle>& OutNodes)
{
    std::vector<PrimitiveSpawnSpec> Specs{};
    Specs.reserve(16);

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Platform.Ground",
        .Position = Vec3(0.0f, -1.0f, 0.0f),
        .VisualKind = EPrimitiveKind::Box,
        .VisualSize = Vec3(48.0f, 2.0f, 48.0f),
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .PhysicsEnabled = true,
        .BodyType = SnAPI::Physics::EBodyType::Static,
        .ColliderShape = SnAPI::Physics::EShapeType::Box,
        .ColliderHalfExtent = Vec3(24.0f, 1.0f, 24.0f),
        .Mass = 10000.0f,
        .Friction = 0.95f,
        .Restitution = 0.02f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Platform.StepA",
        .Position = Vec3(0.0f, 1.0f, 0.0f),
        .VisualKind = EPrimitiveKind::Box,
        .VisualSize = Vec3(6.0f, 1.0f, 6.0f),
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Static,
        .ColliderShape = SnAPI::Physics::EShapeType::Box,
        .ColliderHalfExtent = Vec3(3.0f, 0.5f, 3.0f),
        .Mass = 2000.0f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Platform.StepB",
        .Position = Vec3(6.0f, 2.3f, -1.0f),
        .VisualKind = EPrimitiveKind::Box,
        .VisualSize = Vec3(5.0f, 1.0f, 5.0f),
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Static,
        .ColliderShape = SnAPI::Physics::EShapeType::Box,
        .ColliderHalfExtent = Vec3(2.5f, 0.5f, 2.5f),
        .Mass = 2000.0f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Platform.StepC",
        .Position = Vec3(12.0f, 3.7f, 1.4f),
        .VisualKind = EPrimitiveKind::Box,
        .VisualSize = Vec3(4.2f, 1.0f, 4.2f),
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Static,
        .ColliderShape = SnAPI::Physics::EShapeType::Box,
        .ColliderHalfExtent = Vec3(2.1f, 0.5f, 2.1f),
        .Mass = 1600.0f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Platform.StepD",
        .Position = Vec3(17.5f, 5.1f, -1.3f),
        .VisualKind = EPrimitiveKind::Box,
        .VisualSize = Vec3(3.6f, 1.0f, 3.6f),
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Static,
        .ColliderShape = SnAPI::Physics::EShapeType::Box,
        .ColliderHalfExtent = Vec3(1.8f, 0.5f, 1.8f),
        .Mass = 1400.0f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Platform.TiltBridge",
        .Position = Vec3(11.5f, 4.6f, -4.2f),
        .Rotation = PitchDegrees(-16.0f),
        .VisualKind = EPrimitiveKind::Box,
        .VisualSize = Vec3(8.0f, 0.8f, 3.2f),
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Static,
        .ColliderShape = SnAPI::Physics::EShapeType::Box,
        .ColliderHalfExtent = Vec3(4.0f, 0.4f, 1.6f),
        .Mass = 1800.0f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Platform.GoalPyramid",
        .Position = Vec3(24.0f, 8.1f, 0.9f),
        .VisualKind = EPrimitiveKind::Pyramid,
        .VisualSize = Vec3(4.5f, 3.8f, 4.5f),
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Static,
        .ColliderShape = SnAPI::Physics::EShapeType::Box,
        .ColliderHalfExtent = Vec3(2.25f, 1.9f, 2.25f),
        .Mass = 2400.0f,
        .Friction = 0.9f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Obstacle.ConeA",
        .Position = Vec3(7.2f, 3.1f, 2.3f),
        .VisualKind = EPrimitiveKind::Cone,
        .VisualRadius = 0.7f,
        .VisualHeight = 1.5f,
        .VisualSegments = 24u,
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Static,
        .ColliderShape = SnAPI::Physics::EShapeType::Capsule,
        .ColliderRadius = 0.5f,
        .ColliderHalfHeight = 0.35f,
        .Mass = 350.0f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Obstacle.ConeB",
        .Position = Vec3(13.8f, 4.5f, 2.6f),
        .VisualKind = EPrimitiveKind::Cone,
        .VisualRadius = 0.8f,
        .VisualHeight = 1.8f,
        .VisualSegments = 28u,
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Static,
        .ColliderShape = SnAPI::Physics::EShapeType::Capsule,
        .ColliderRadius = 0.55f,
        .ColliderHalfHeight = 0.45f,
        .Mass = 400.0f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Dynamic.CrateA",
        .Position = Vec3(2.0f, 8.0f, 0.5f),
        .VisualKind = EPrimitiveKind::Box,
        .VisualSize = Vec3(1.2f, 1.2f, 1.2f),
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Dynamic,
        .ColliderShape = SnAPI::Physics::EShapeType::Box,
        .ColliderHalfExtent = Vec3(0.6f, 0.6f, 0.6f),
        .Mass = 1.2f,
        .Friction = 0.75f,
        .Restitution = 0.12f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Dynamic.CrateB",
        .Position = Vec3(8.8f, 10.5f, -0.6f),
        .VisualKind = EPrimitiveKind::Box,
        .VisualSize = Vec3(1.4f, 1.4f, 1.4f),
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Dynamic,
        .ColliderShape = SnAPI::Physics::EShapeType::Box,
        .ColliderHalfExtent = Vec3(0.7f, 0.7f, 0.7f),
        .Mass = 1.6f,
        .Friction = 0.72f,
        .Restitution = 0.1f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Dynamic.BouncerSphere",
        .Position = Vec3(14.6f, 11.8f, 0.2f),
        .VisualKind = EPrimitiveKind::Sphere,
        .VisualRadius = 0.75f,
        .VisualSegments = 36u,
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Dynamic,
        .ColliderShape = SnAPI::Physics::EShapeType::Sphere,
        .ColliderRadius = 0.75f,
        .Mass = 0.9f,
        .Friction = 0.45f,
        .Restitution = 0.58f,
        .InitialLinearVelocity = Vec3(1.25f, 0.0f, 0.0f),
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Dynamic.FallingCone",
        .Position = Vec3(18.6f, 13.0f, -0.8f),
        .VisualKind = EPrimitiveKind::Cone,
        .VisualRadius = 0.65f,
        .VisualHeight = 1.6f,
        .VisualSegments = 26u,
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Dynamic,
        .ColliderShape = SnAPI::Physics::EShapeType::Capsule,
        .ColliderRadius = 0.45f,
        .ColliderHalfHeight = 0.55f,
        .Mass = 0.8f,
        .Friction = 0.5f,
        .Restitution = 0.2f,
#endif
    });

    Specs.push_back(PrimitiveSpawnSpec{
        .Name = "Dynamic.TopPyramid",
        .Position = Vec3(22.3f, 14.4f, 0.4f),
        .VisualKind = EPrimitiveKind::Pyramid,
        .VisualSize = Vec3(1.6f, 2.0f, 1.6f),
#if defined(SNAPI_GF_ENABLE_PHYSICS)
        .BodyType = SnAPI::Physics::EBodyType::Dynamic,
        .ColliderShape = SnAPI::Physics::EShapeType::Box,
        .ColliderHalfExtent = Vec3(0.8f, 1.0f, 0.8f),
        .Mass = 0.95f,
        .Friction = 0.55f,
        .Restitution = 0.22f,
#endif
    });

    for (const PrimitiveSpawnSpec& Spec : Specs)
    {
        (void)SpawnPrimitive(WorldRef, Spec, OutNodes);
    }
}
} // namespace

Result EditorSceneBootstrap::Initialize(GameRuntime& Runtime)
{
    auto* WorldPtr = Runtime.WorldPtr();
    if (!WorldPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Game runtime is not initialized"));
    }

    if (!WorldPtr->Renderer().IsInitialized())
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Renderer system is not initialized"));
    }

    if (!m_cameraNode.IsNull() && m_cameraNode.Borrowed() == nullptr)
    {
        m_cameraNode = {};
        m_cameraComponent = nullptr;
    }
    m_sceneNodes.erase(std::remove_if(m_sceneNodes.begin(), m_sceneNodes.end(),
                                      [](const NodeHandle& Handle) { return Handle.IsNull() || Handle.Borrowed() == nullptr; }),
                       m_sceneNodes.end());

    if (!m_cameraNode.IsNull() && !m_sceneNodes.empty())
    {
        SyncActiveCamera(*WorldPtr);
        if (m_cameraComponent)
        {
            return Ok();
        }
    }

    for (auto It = m_sceneNodes.rbegin(); It != m_sceneNodes.rend(); ++It)
    {
        if (!It->IsNull())
        {
            (void)WorldPtr->DestroyNode(*It);
        }
    }
    m_sceneNodes.clear();

    if (!m_cameraNode.IsNull())
    {
        (void)WorldPtr->DestroyNode(m_cameraNode);
        m_cameraNode = {};
    }
    m_cameraComponent = nullptr;

    auto CameraNodeResult = WorldPtr->CreateNode("EditorCamera");
    if (!CameraNodeResult)
    {
        return std::unexpected(CameraNodeResult.error());
    }

    auto* CameraNode = CameraNodeResult->Borrowed();
    if (!CameraNode)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Failed to borrow editor camera node"));
    }

    if (auto Transform = CameraNode->Add<TransformComponent>())
    {
        Transform->Position = Vec3(0.0f, 6.0f, 24.0f);
        Transform->Rotation =
            SnAPI::Math::AngleAxis3D(SnAPI::Math::SLinearAlgebra::DegreesToRadians(-18.0f), SnAPI::Math::Vector3::UnitX());
    }

    auto CameraResult = CameraNode->Add<CameraComponent>();
    if (!CameraResult)
    {
        return std::unexpected(CameraResult.error());
    }

    auto* Camera = &*CameraResult;
    auto& Settings = Camera->EditSettings();
    Settings.Active = true;
    Settings.SyncFromTransform = true;
    Settings.FovDegrees = 60.0f;
    Settings.NearClip = 0.05f;
    Settings.FarClip = 5000.0f;
    Settings.Aspect = 16.0f / 9.0f;
    Camera->SetActive(true);

#if defined(SNAPI_GF_ENABLE_INPUT)
    auto EditorCameraResult = CameraNode->Add<EditorCameraComponent>();
    if (!EditorCameraResult)
    {
        return std::unexpected(EditorCameraResult.error());
    }

    auto& FlySettings = EditorCameraResult->EditSettings();
    FlySettings.Enabled = true;
    FlySettings.RequireInputFocus = true;
    FlySettings.RequireRightMouseButton = true;
    FlySettings.RequirePointerInsideViewport = true;
    FlySettings.MoveSpeed = 14.0f;
    FlySettings.FastMoveMultiplier = 2.0f;
    FlySettings.LookSensitivity = 0.12f;
    FlySettings.InvertY = false;
#endif

    m_cameraNode = CameraNodeResult.value();
    m_cameraComponent = Camera;

    BuildPlatformingScene(*WorldPtr, m_sceneNodes);

    SyncActiveCamera(*WorldPtr);
    return Ok();
}

void EditorSceneBootstrap::Shutdown(GameRuntime* Runtime)
{
    if (Runtime)
    {
        if (auto* WorldPtr = Runtime->WorldPtr())
        {
            for (auto It = m_sceneNodes.rbegin(); It != m_sceneNodes.rend(); ++It)
            {
                if (!It->IsNull())
                {
                    (void)WorldPtr->DestroyNode(*It);
                }
            }

            if (!m_cameraNode.IsNull())
            {
                (void)WorldPtr->DestroyNode(m_cameraNode);
            }
        }
    }

    m_cameraNode = {};
    m_sceneNodes.clear();
    m_cameraComponent = nullptr;
}

void EditorSceneBootstrap::SyncActiveCamera(World& WorldRef)
{
    if (CameraComponent* Active = ResolveActiveCameraComponent(WorldRef))
    {
        m_cameraComponent = Active;
        return;
    }

    if (!m_cameraNode.IsNull())
    {
        if (auto* CameraNode = m_cameraNode.Borrowed())
        {
            if (auto Camera = CameraNode->Component<CameraComponent>())
            {
                m_cameraComponent = &*Camera;
            }
        }
    }
}

CameraComponent* EditorSceneBootstrap::ActiveCameraComponent() const
{
    return m_cameraComponent;
}

SnAPI::Graphics::ICamera* EditorSceneBootstrap::ActiveRenderCamera() const
{
    if (!m_cameraComponent)
    {
        return nullptr;
    }

    return m_cameraComponent->Camera();
}

CameraComponent* EditorSceneBootstrap::ResolveActiveCameraComponent(World& WorldRef) const
{
    auto* ActiveCamera = WorldRef.Renderer().ActiveCamera();
    if (!ActiveCamera)
    {
        return nullptr;
    }

    CameraComponent* MatchedCamera = nullptr;
    WorldRef.NodePool().ForEach([&](const NodeHandle&, BaseNode& Node) {
        if (MatchedCamera != nullptr || !Node.Has<CameraComponent>())
        {
            return;
        }

        auto CameraResult = Node.Component<CameraComponent>();
        if (!CameraResult)
        {
            return;
        }

        auto* Component = &*CameraResult;
        if (Component->Camera() == ActiveCamera)
        {
            MatchedCamera = Component;
        }
    });

    return MatchedCamera;
}

} // namespace SnAPI::GameFramework::Editor

#else

#include "GameRuntime.h"

namespace SnAPI::GameFramework::Editor
{

Result EditorSceneBootstrap::Initialize(GameRuntime& Runtime)
{
    (void)Runtime;
    return Ok();
}

void EditorSceneBootstrap::Shutdown(GameRuntime* Runtime)
{
    (void)Runtime;
    m_cameraNode = {};
    m_sceneNodes.clear();
    m_cameraComponent = nullptr;
}

void EditorSceneBootstrap::SyncActiveCamera(World& WorldRef)
{
    (void)WorldRef;
}

CameraComponent* EditorSceneBootstrap::ActiveCameraComponent() const
{
    return nullptr;
}

SnAPI::Graphics::ICamera* EditorSceneBootstrap::ActiveRenderCamera() const
{
    return nullptr;
}

CameraComponent* EditorSceneBootstrap::ResolveActiveCameraComponent(World& WorldRef) const
{
    (void)WorldRef;
    return nullptr;
}

} // namespace SnAPI::GameFramework::Editor

#endif
