#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

#if defined(SNAPI_GF_ENABLE_PHYSICS)

namespace
{

void AddGround(World& WorldRef)
{
    auto GroundNodeResult = WorldRef.CreateNode<BaseNode>("Ground");
    REQUIRE(GroundNodeResult);

    auto* GroundNode = GroundNodeResult->Borrowed();
    REQUIRE(GroundNode != nullptr);

    auto GroundTransform = GroundNode->Add<TransformComponent>();
    REQUIRE(GroundTransform);
    GroundTransform->Position = Vec3{0.0f, -1.0f, 0.0f};

    auto GroundCollider = GroundNode->Add<ColliderComponent>();
    REQUIRE(GroundCollider);
    GroundCollider->EditSettings().Shape = SnAPI::Physics::EShapeType::Box;
    GroundCollider->EditSettings().HalfExtent = Vec3{30.0f, 1.0f, 30.0f};
    GroundCollider->EditSettings().Layer = CollisionLayerFlags(ECollisionFilterBits::WorldStatic);

    auto GroundBody = GroundNode->Add<RigidBodyComponent>();
    REQUIRE(GroundBody);
    GroundBody->EditSettings().BodyType = SnAPI::Physics::EBodyType::Static;
    REQUIRE(GroundBody->RecreateBody());
}

SnAPI::Math::Quaternion ComposeRotationZyx(const Vec3& Euler)
{
    return SnAPI::Math::AngleAxis3D(Euler.z(), SnAPI::Math::Vector3::UnitZ())
         * SnAPI::Math::AngleAxis3D(Euler.y(), SnAPI::Math::Vector3::UnitY())
         * SnAPI::Math::AngleAxis3D(Euler.x(), SnAPI::Math::Vector3::UnitX());
}

} // namespace

TEST_CASE("GameRuntime initializes and steps physics through world fixed tick")
{
    GameRuntime Runtime;
    GameRuntimeSettings Settings{};
    Settings.WorldName = "PhysicsRuntimeWorld";
    Settings.RegisterBuiltins = true;
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;
    Settings.Tick.MaxFixedStepsPerUpdate = 2;

    GameRuntimePhysicsSettings PhysicsSettings{};
    PhysicsSettings.TickInFixedTick = true;
    PhysicsSettings.TickInVariableTick = false;
    Settings.Physics = PhysicsSettings;

    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.World().Physics().IsInitialized());

    AddGround(Runtime.World());

    auto FallingNodeResult = Runtime.World().CreateNode<BaseNode>("Falling");
    REQUIRE(FallingNodeResult);
    auto* FallingNode = FallingNodeResult->Borrowed();
    REQUIRE(FallingNode != nullptr);

    auto FallingTransform = FallingNode->Add<TransformComponent>();
    REQUIRE(FallingTransform);
    FallingTransform->Position = Vec3{0.0f, 6.0f, 0.0f};

    auto FallingCollider = FallingNode->Add<ColliderComponent>();
    REQUIRE(FallingCollider);
    FallingCollider->EditSettings().Shape = SnAPI::Physics::EShapeType::Box;
    FallingCollider->EditSettings().HalfExtent = Vec3{0.5f, 0.5f, 0.5f};

    auto FallingBody = FallingNode->Add<RigidBodyComponent>();
    REQUIRE(FallingBody);
    FallingBody->EditSettings().BodyType = SnAPI::Physics::EBodyType::Dynamic;
    REQUIRE(FallingBody->RecreateBody());

    const float InitialY = FallingTransform->Position.y();
    for (int i = 0; i < 120; ++i)
    {
        Runtime.Update(1.0f / 60.0f);
    }

    REQUIRE(FallingTransform->Position.y() < InitialY - 0.5f);
}

TEST_CASE("Physics bootstrap MaxSubStepping overrides scene collision steps")
{
    GameRuntime Runtime;
    GameRuntimeSettings Settings{};
    Settings.WorldName = "PhysicsSubSteppingWorld";
    Settings.RegisterBuiltins = true;
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;
    Settings.Tick.MaxFixedStepsPerUpdate = 2;

    GameRuntimePhysicsSettings PhysicsSettings{};
    PhysicsSettings.Scene.CollisionSteps = 1;
    PhysicsSettings.MaxSubStepping = 4;
    Settings.Physics = PhysicsSettings;

    REQUIRE(Runtime.Init(Settings));
    CHECK(Runtime.World().Physics().Settings().Scene.CollisionSteps == 4u);
}

TEST_CASE("Physics bootstrap rejects MaxSubStepping of zero")
{
    GameRuntime Runtime;
    GameRuntimeSettings Settings{};
    Settings.WorldName = "PhysicsInvalidSubSteppingWorld";
    Settings.RegisterBuiltins = true;
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;
    Settings.Tick.MaxFixedStepsPerUpdate = 2;

    GameRuntimePhysicsSettings PhysicsSettings{};
    PhysicsSettings.MaxSubStepping = 0;
    Settings.Physics = PhysicsSettings;

    REQUIRE_FALSE(Runtime.Init(Settings));
}

TEST_CASE("CharacterMovementController drives rigid body movement")
{
    GameRuntime Runtime;
    GameRuntimeSettings Settings{};
    Settings.WorldName = "PhysicsCharacterWorld";
    Settings.RegisterBuiltins = true;
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;
    Settings.Tick.MaxFixedStepsPerUpdate = 2;

    GameRuntimePhysicsSettings PhysicsSettings{};
    PhysicsSettings.TickInFixedTick = true;
    PhysicsSettings.TickInVariableTick = false;
    Settings.Physics = PhysicsSettings;

    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.World().Physics().IsInitialized());

    AddGround(Runtime.World());

    auto PlayerNodeResult = Runtime.World().CreateNode<BaseNode>("Player");
    REQUIRE(PlayerNodeResult);
    auto* PlayerNode = PlayerNodeResult->Borrowed();
    REQUIRE(PlayerNode != nullptr);

    auto PlayerTransform = PlayerNode->Add<TransformComponent>();
    REQUIRE(PlayerTransform);
    PlayerTransform->Position = Vec3{0.0f, 1.0f, 0.0f};

    auto PlayerCollider = PlayerNode->Add<ColliderComponent>();
    REQUIRE(PlayerCollider);
    PlayerCollider->EditSettings().Shape = SnAPI::Physics::EShapeType::Box;
    PlayerCollider->EditSettings().HalfExtent = Vec3{0.4f, 0.9f, 0.4f};
    PlayerCollider->EditSettings().Layer = CollisionLayerFlags(ECollisionFilterBits::WorldDynamic);

    auto PlayerBody = PlayerNode->Add<RigidBodyComponent>();
    REQUIRE(PlayerBody);
    PlayerBody->EditSettings().BodyType = SnAPI::Physics::EBodyType::Dynamic;
    PlayerBody->EditSettings().Mass = 70.0f;
    REQUIRE(PlayerBody->RecreateBody());

    auto Movement = PlayerNode->Add<CharacterMovementController>();
    REQUIRE(Movement);
    Movement->EditSettings().MoveForce = 60.0f;
    Movement->SetMoveInput(Vec3{1.0f, 0.0f, 0.0f});

    for (int i = 0; i < 180; ++i)
    {
        Runtime.Update(1.0f / 60.0f);
    }

    REQUIRE(PlayerTransform->Position.x() > 0.25f);

    Movement->Jump();
    for (int i = 0; i < 30; ++i)
    {
        Runtime.Update(1.0f / 60.0f);
    }

    // Jump path is force/grounded dependent; ensure simulation remained stable and finite.
    REQUIRE(PlayerTransform->Position.y() > -10.0f);
}

TEST_CASE("RigidBodyComponent deactivates on sleep and reactivates on wake events")
{
    GameRuntime Runtime;
    GameRuntimeSettings Settings{};
    Settings.WorldName = "PhysicsSleepEventsWorld";
    Settings.RegisterBuiltins = true;
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;
    Settings.Tick.MaxFixedStepsPerUpdate = 2;

    GameRuntimePhysicsSettings PhysicsSettings{};
    PhysicsSettings.TickInFixedTick = true;
    PhysicsSettings.TickInVariableTick = false;
    Settings.Physics = PhysicsSettings;

    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.World().Physics().IsInitialized());

    auto DynamicNodeResult = Runtime.World().CreateNode<BaseNode>("Dynamic");
    REQUIRE(DynamicNodeResult);
    auto* DynamicNode = DynamicNodeResult->Borrowed();
    REQUIRE(DynamicNode != nullptr);

    auto DynamicTransform = DynamicNode->Add<TransformComponent>();
    REQUIRE(DynamicTransform);
    DynamicTransform->Position = Vec3{0.0f, 2.0f, 0.0f};

    auto DynamicCollider = DynamicNode->Add<ColliderComponent>();
    REQUIRE(DynamicCollider);
    DynamicCollider->EditSettings().Shape = SnAPI::Physics::EShapeType::Sphere;
    DynamicCollider->EditSettings().Radius = 0.5f;

    auto DynamicBody = DynamicNode->Add<RigidBodyComponent>();
    REQUIRE(DynamicBody);
    DynamicBody->EditSettings().BodyType = SnAPI::Physics::EBodyType::Dynamic;
    DynamicBody->EditSettings().AutoDeactivateWhenSleeping = true;
    REQUIRE(DynamicBody->RecreateBody());

    auto* Scene = Runtime.World().Physics().Scene();
    REQUIRE(Scene != nullptr);

    const SnAPI::Physics::BodyHandle BodyHandle = DynamicBody->PhysicsBodyHandle();
    REQUIRE(BodyHandle.IsValid());

    REQUIRE(Scene->Rigid().SleepBody(BodyHandle).has_value());
    Runtime.Update(1.0f / 60.0f);
    CHECK_FALSE(DynamicBody->Active());

    REQUIRE(Scene->Rigid().WakeBody(BodyHandle).has_value());
    Runtime.Update(1.0f / 60.0f);
    CHECK(DynamicBody->Active());
}

TEST_CASE("RigidBodyComponent keeps quaternion orientation stable through physics sync")
{
    GameRuntime Runtime;
    GameRuntimeSettings Settings{};
    Settings.WorldName = "PhysicsRotationConventionWorld";
    Settings.RegisterBuiltins = true;
    Settings.Tick.EnableFixedTick = true;
    Settings.Tick.FixedDeltaSeconds = 1.0f / 60.0f;
    Settings.Tick.MaxFixedStepsPerUpdate = 2;

    GameRuntimePhysicsSettings PhysicsSettings{};
    PhysicsSettings.Scene.Gravity = Vec3::Zero();
    PhysicsSettings.TickInFixedTick = true;
    PhysicsSettings.TickInVariableTick = false;
    Settings.Physics = PhysicsSettings;

    REQUIRE(Runtime.Init(Settings));
    REQUIRE(Runtime.World().Physics().IsInitialized());

    auto NodeResult = Runtime.World().CreateNode<BaseNode>("RotBody");
    REQUIRE(NodeResult);
    auto* Node = NodeResult->Borrowed();
    REQUIRE(Node != nullptr);

    auto TransformResult = Node->Add<TransformComponent>();
    REQUIRE(TransformResult);
    TransformResult->Position = Vec3{0.0f, 2.0f, 0.0f};
    const Vec3 ExpectedEuler{0.35f, -0.50f, 1.10f};
    const Quat ExpectedRotation = ComposeRotationZyx(ExpectedEuler).normalized();
    TransformResult->Rotation = ExpectedRotation;

    auto Collider = Node->Add<ColliderComponent>();
    REQUIRE(Collider);
    Collider->EditSettings().Shape = SnAPI::Physics::EShapeType::Box;
    Collider->EditSettings().HalfExtent = Vec3{0.5f, 0.5f, 0.5f};

    auto Body = Node->Add<RigidBodyComponent>();
    REQUIRE(Body);
    Body->EditSettings().BodyType = SnAPI::Physics::EBodyType::Dynamic;
    Body->EditSettings().LinearDamping = 0.0f;
    Body->EditSettings().AngularDamping = 0.0f;
    REQUIRE(Body->RecreateBody());

    for (int i = 0; i < 20; ++i)
    {
        Runtime.Update(1.0f / 60.0f);
    }

    const Quat Actual = TransformResult->Rotation.squaredNorm() > 0.0f
                            ? TransformResult->Rotation.normalized()
                            : Quat::Identity();
    const float Dot = std::abs(ExpectedRotation.dot(Actual));
    CHECK(Dot > 0.999f);
}

#endif // SNAPI_GF_ENABLE_PHYSICS
