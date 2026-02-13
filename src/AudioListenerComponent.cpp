#include "AudioListenerComponent.h"

#if defined(SNAPI_GF_ENABLE_AUDIO)

#include "AudioSystem.h"
#include "BaseNode.h"
#include "NodeGraph.h"
#include "TransformComponent.h"
#include "Variant.h"
#include "World.h"

#include <AudioEngine.h>
#include <Types.h>
#include <Eigen/Geometry>

namespace SnAPI::GameFramework
{
namespace
{
SnAPI::Audio::Vector3F ToAudioVector(const Vec3& Value)
{
    return SnAPI::Audio::Vector3F(Value.X, Value.Y, Value.Z);
}

SnAPI::Audio::QuaternionF ToAudioQuaternion(const Vec3& EulerRadians)
{
    const Eigen::AngleAxisf Pitch(EulerRadians.X, SnAPI::Audio::Vector3F::UnitX());
    const Eigen::AngleAxisf Yaw(EulerRadians.Y, SnAPI::Audio::Vector3F::UnitY());
    const Eigen::AngleAxisf Roll(EulerRadians.Z, SnAPI::Audio::Vector3F::UnitZ());
    return Yaw * Pitch * Roll;
}
} // namespace

AudioSystem* AudioListenerComponent::ResolveAudioSystem() const
{
    auto* OwnerNode = this->OwnerNode();
    if (!OwnerNode)
    {
        return nullptr;
    }
    auto* WorldPtr = OwnerNode->World();
    if (!WorldPtr)
    {
        return nullptr;
    }
    return &WorldPtr->Audio();
}

void AudioListenerComponent::OnCreate()
{
    if (auto* Audio = ResolveAudioSystem())
    {
        Audio->Initialize();
    }
}

void AudioListenerComponent::SetActive(bool ActiveValue)
{
    if (CallRPC("SetActiveServer", {Variant::FromValue(ActiveValue)}))
    {
        return;
    }
    SetActiveClient(ActiveValue);
}

void AudioListenerComponent::SetActiveServer(bool ActiveValue)
{
    m_active = ActiveValue;
    if (CallRPC("SetActiveClient", {Variant::FromValue(ActiveValue)}))
    {
        return;
    }
    SetActiveClient(ActiveValue);
}

void AudioListenerComponent::SetActiveClient(bool ActiveValue)
{
    m_active = ActiveValue;
}

void AudioListenerComponent::Tick(float DeltaSeconds)
{
    if (!m_active)
    {
        return;
    }

    auto* Audio = ResolveAudioSystem();
    if (!Audio || !Audio->Initialize())
    {
        return;
    }

    auto* Engine = Audio->Engine();
    if (!Engine)
    {
        return;
    }

    Vec3 Position{};
    Vec3 Rotation{};
    if (auto* OwnerNode = this->OwnerNode())
    {
        if (auto TransformResult = OwnerNode->Component<TransformComponent>())
        {
            Position = TransformResult->Position;
            Rotation = TransformResult->Rotation;
        }
    }

    Vec3 Velocity{};
    if (m_hasLastPosition && DeltaSeconds > 0.0f)
    {
        Velocity = (Position - m_lastPosition) * (1.0f / DeltaSeconds);
    }
    m_lastPosition = Position;
    m_hasLastPosition = true;

    SnAPI::Audio::ListenerTransform Listener{};
    Listener.Position = ToAudioVector(Position);
    Listener.Rotation = ToAudioQuaternion(Rotation);
    Listener.Velocity = ToAudioVector(Velocity);

    Engine->SetListenerTransform(Listener);
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_AUDIO
