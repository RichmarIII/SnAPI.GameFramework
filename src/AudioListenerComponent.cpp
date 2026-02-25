#include "AudioListenerComponent.h"

#if defined(SNAPI_GF_ENABLE_AUDIO)

#include "Profiling.h"

#include "AudioSystem.h"
#include "BaseNode.h"
#include "Level.h"
#include "TransformComponent.h"
#include "Variant.h"
#include "World.h"

#include <AudioEngine.h>
#include <Types.h>

namespace SnAPI::GameFramework
{
namespace
{
SnAPI::Audio::Vector3F ToAudioVector(const Vec3& Value)
{
    return SnAPI::Audio::Vector3F(Value.x(), Value.y(), Value.z());
}

SnAPI::Audio::QuaternionF ToAudioQuaternion(const Quat& Rotation)
{
    SnAPI::Audio::QuaternionF Out = SnAPI::Audio::QuaternionF::Identity();
    Out.x() = static_cast<SnAPI::Audio::QuaternionF::Scalar>(Rotation.x());
    Out.y() = static_cast<SnAPI::Audio::QuaternionF::Scalar>(Rotation.y());
    Out.z() = static_cast<SnAPI::Audio::QuaternionF::Scalar>(Rotation.z());
    Out.w() = static_cast<SnAPI::Audio::QuaternionF::Scalar>(Rotation.w());
    if (Out.squaredNorm() > 0.0f)
    {
        Out.normalize();
    }
    else
    {
        Out = SnAPI::Audio::QuaternionF::Identity();
    }
    return Out;
}
} // namespace

AudioSystem* AudioListenerComponent::ResolveAudioSystem() const
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
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
    if (!WorldPtr->ShouldTickAudio())
    {
        return nullptr;
    }
    return &WorldPtr->Audio();
}

void AudioListenerComponent::OnCreate()
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    if (auto* Audio = ResolveAudioSystem())
    {
        Audio->Initialize();
    }
}

void AudioListenerComponent::SetActive(bool ActiveValue)
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    if (CallRPC("SetActiveServer", {Variant::FromValue(ActiveValue)}))
    {
        return;
    }
    SetActiveClient(ActiveValue);
}

void AudioListenerComponent::SetActiveServer(bool ActiveValue)
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    m_active = ActiveValue;
    if (CallRPC("SetActiveClient", {Variant::FromValue(ActiveValue)}))
    {
        return;
    }
    SetActiveClient(ActiveValue);
}

void AudioListenerComponent::SetActiveClient(bool ActiveValue)
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    m_active = ActiveValue;
}

void AudioListenerComponent::Tick(float DeltaSeconds)
{
    RuntimeTick(DeltaSeconds);
}

void AudioListenerComponent::RuntimeTick(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
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
    Quat Rotation = Quat::Identity();
    if (auto* OwnerNode = this->OwnerNode())
    {
        NodeTransform WorldTransform{};
        if (TransformComponent::TryGetNodeWorldTransform(*OwnerNode, WorldTransform))
        {
            Position = WorldTransform.Position;
            Rotation = WorldTransform.Rotation;
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
