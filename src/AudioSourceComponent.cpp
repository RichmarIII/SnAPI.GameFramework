#include "AudioSourceComponent.h"

#if defined(SNAPI_GF_ENABLE_AUDIO)

#include "AudioSystem.h"
#include "BaseNode.h"
#include "NodeGraph.h"
#include "TransformComponent.h"
#include "World.h"

#include <Types.h>
#include <exception>

namespace SnAPI::GameFramework
{
namespace
{
SnAPI::Audio::Vector3F ToAudioVector(const Vec3& Value)
{
    return SnAPI::Audio::Vector3F(Value.X, Value.Y, Value.Z);
}
} // namespace

AudioSystem* AudioSourceComponent::ResolveAudioSystem() const
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

void AudioSourceComponent::OnCreate()
{
    EnsureEmitter();
    if (m_settings.AutoPlay)
    {
        Play();
    }
}

void AudioSourceComponent::OnDestroy()
{
    // Component teardown must avoid World()/Networking() virtual dispatch during graph shutdown.
    m_playRequested = false;
    m_sound = {};
    m_loadedPath.clear();
    m_loadedStreaming = false;
    m_emitter = {};
    m_hasLastPosition = false;
}

void AudioSourceComponent::Tick(float DeltaSeconds)
{
    EnsureEmitter();
    RefreshPlaybackState();
    UpdateEmitterTransform(DeltaSeconds);
}

void AudioSourceComponent::Play()
{
    if (CallRPC("PlayServer"))
    {
        return;
    }
    PlayClient();
}

void AudioSourceComponent::PlayServer()
{
    m_playRequested = true;
    if (CallRPC("PlayClient"))
    {
        return;
    }
    PlayClient();
}

void AudioSourceComponent::PlayClient()
{
    m_playRequested = true;
    if (IsServer() && !IsListenServer())
    {
        return;
    }
    if (m_settings.SoundPath.empty())
    {
        return;
    }

    EnsureEmitter();
    if (!IsLoaded() || m_loadedPath != m_settings.SoundPath || m_loadedStreaming != m_settings.Streaming)
    {
        if (!LoadSound(m_settings.SoundPath, m_settings.Streaming))
        {
            return;
        }
    }

    if (auto* Audio = ResolveAudioSystem())
    {
        if (auto* Engine = Audio->Engine(); Engine && m_emitter.IsValid() && m_sound.IsValid())
        {
            Engine->SetLooping(m_emitter, m_settings.Looping);
            Engine->Play(m_emitter, m_sound);
            m_playRequested = false;
        }
    }
}

void AudioSourceComponent::Stop()
{
    if (CallRPC("StopServer"))
    {
        return;
    }
    StopClient();
}

void AudioSourceComponent::StopServer()
{
    m_playRequested = false;
    if (CallRPC("StopClient"))
    {
        return;
    }
    StopClient();
}

void AudioSourceComponent::StopClient()
{
    m_playRequested = false;
    if (IsServer() && !IsListenServer())
    {
        return;
    }
    if (auto* Audio = ResolveAudioSystem())
    {
        try
        {
            if (auto* Engine = Audio->Engine(); Engine && m_emitter.IsValid())
            {
                Engine->Stop(m_emitter);
            }
        }
        catch (const std::exception&)
        {
        }
        catch (...)
        {
        }
    }
    m_playRequested = false;
}

bool AudioSourceComponent::IsPlaying() const
{
    const auto* Emitter = m_emitter.TryGet();
    return Emitter && Emitter->IsPlaying();
}

bool AudioSourceComponent::IsLoaded() const
{
    return m_sound.IsValid();
}

bool AudioSourceComponent::LoadSound(const std::string& Path, bool StreamingMode)
{
    if (Path.empty())
    {
        return false;
    }

    EnsureEmitter();
    auto* Audio = ResolveAudioSystem();
    auto* Engine = Audio ? Audio->Engine() : nullptr;
    if (!Engine)
    {
        return false;
    }

    UnloadSound();

    try
    {
        m_sound = StreamingMode
            ? Engine->LoadSoundStreaming(Path)
            : Engine->LoadSoundResident(Path);
    }
    catch (const std::exception&)
    {
        m_sound = {};
        return false;
    }
    catch (...)
    {
        m_sound = {};
        return false;
    }

    m_loadedPath = Path;
    m_loadedStreaming = StreamingMode;

    if (!m_sound.IsValid())
    {
        return false;
    }

    if (m_playRequested)
    {
        Engine->SetLooping(m_emitter, m_settings.Looping);
        Engine->Play(m_emitter, m_sound);
        m_playRequested = false;
    }

    return true;
}

void AudioSourceComponent::UnloadSound()
{
    auto* Audio = ResolveAudioSystem();
    auto* Engine = Audio ? Audio->Engine() : nullptr;
    if (Engine && m_sound.IsValid())
    {
        Engine->UnloadSound(m_sound);
    }
    m_sound = {};
    m_loadedPath.clear();
    m_loadedStreaming = false;
}

void AudioSourceComponent::EnsureEmitter()
{
    try
    {
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

        if (!m_emitter.IsValid())
        {
            m_emitter = Engine->CreateEmitter();
            m_lastVolume = m_settings.Volume;
            m_lastLooping = m_settings.Looping;
            if (auto* Emitter = m_emitter.TryGet())
            {
                Emitter->SetGain(m_settings.Volume);
            }
            Engine->SetLooping(m_emitter, m_settings.Looping);
        }
    }
    catch (const std::exception&)
    {
    }
    catch (...)
    {
    }
}

void AudioSourceComponent::RefreshPlaybackState()
{
    if (!m_emitter.IsValid())
    {
        return;
    }

    if (m_loadedPath != m_settings.SoundPath || m_loadedStreaming != m_settings.Streaming)
    {
        if (!m_settings.SoundPath.empty())
        {
            LoadSound(m_settings.SoundPath, m_settings.Streaming);
        }
        else
        {
            UnloadSound();
        }
    }

    if (m_lastLooping != m_settings.Looping)
    {
        if (auto* Audio = ResolveAudioSystem())
        {
            if (auto* Engine = Audio->Engine())
            {
                Engine->SetLooping(m_emitter, m_settings.Looping);
            }
        }
        m_lastLooping = m_settings.Looping;
    }

    if (m_lastVolume != m_settings.Volume)
    {
        if (auto* Emitter = m_emitter.TryGet())
        {
            Emitter->SetGain(m_settings.Volume);
        }
        m_lastVolume = m_settings.Volume;
    }
}

void AudioSourceComponent::UpdateEmitterTransform(float DeltaSeconds)
{
    auto* Audio = ResolveAudioSystem();
    auto* Engine = Audio ? Audio->Engine() : nullptr;
    if (!Engine || !m_emitter.IsValid())
    {
        return;
    }

    Vec3 Position{};
    if (auto* OwnerNode = Owner().Borrowed())
    {
        if (auto TransformResult = OwnerNode->Component<TransformComponent>())
        {
            Position = TransformResult->Position;
        }
    }

    Vec3 Velocity{};
    if (m_hasLastPosition && DeltaSeconds > 0.0f)
    {
        Velocity = (Position - m_lastPosition) * (1.0f / DeltaSeconds);
    }

    m_lastPosition = Position;
    m_hasLastPosition = true;

    SnAPI::Audio::EmitterTransform Transform{};
    Transform.Position = ToAudioVector(Position);
    Transform.Velocity = ToAudioVector(Velocity);
    Transform.Gain = m_settings.SpatialGain;
    Transform.MinDistance = m_settings.MinDistance;
    Transform.MaxDistance = m_settings.MaxDistance;
    Transform.Rolloff = m_settings.Rolloff;

    Engine->SetEmitterTransform(m_emitter, Transform);
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_AUDIO
