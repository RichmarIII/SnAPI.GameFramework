#include "AudioSystem.h"

#if defined(SNAPI_GF_ENABLE_AUDIO)

#include "Profiling.h"

#include <AudioDeviceSpec.h>
#include <AudioEngine.h>
#include <MiniaudioDevice.h>

namespace SnAPI::GameFramework
{
AudioSystem::~AudioSystem()
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    Shutdown();
}

AudioSystem::AudioSystem(AudioSystem&& Other) noexcept
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    std::lock_guard<std::mutex> Lock(Other.m_mutex);
    m_engine = std::move(Other.m_engine);
}

AudioSystem& AudioSystem::operator=(AudioSystem&& Other) noexcept
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    if (this == &Other)
    {
        return *this;
    }
    std::scoped_lock Lock(m_mutex, Other.m_mutex);
    m_engine = std::move(Other.m_engine);
    return *this;
}

bool AudioSystem::Initialize(const SnAPI::Audio::AudioDeviceSpec& Spec)
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    std::lock_guard<std::mutex> Lock(m_mutex);
    if (m_engine && m_engine->IsInitialized())
    {
        return true;
    }
    if (!m_engine)
    {
        auto Factory = std::make_unique<SnAPI::Audio::Backend::MiniaudioDeviceFactory>();
        m_engine = std::make_unique<SnAPI::Audio::AudioEngine>(std::move(Factory));
    }
    return m_engine->Initialize(Spec);
}

bool AudioSystem::Initialize()
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    const SnAPI::Audio::AudioDeviceSpec Spec{};
    return Initialize(Spec);
}

void AudioSystem::Shutdown()
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    std::lock_guard<std::mutex> Lock(m_mutex);
    if (m_engine)
    {
        m_engine->Shutdown();
    }
}

bool AudioSystem::IsInitialized() const
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    std::lock_guard<std::mutex> Lock(m_mutex);
    return m_engine && m_engine->IsInitialized();
}

SnAPI::Audio::AudioEngine* AudioSystem::Engine()
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    std::lock_guard<std::mutex> Lock(m_mutex);
    return m_engine.get();
}

const SnAPI::Audio::AudioEngine* AudioSystem::Engine() const
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    std::lock_guard<std::mutex> Lock(m_mutex);
    return m_engine.get();
}

void AudioSystem::Update(float DeltaSeconds)
{
    SNAPI_GF_PROFILE_FUNCTION("Audio");
    (void)DeltaSeconds;
    SnAPI::Audio::AudioEngine* EnginePtr = nullptr;
    {
        std::lock_guard<std::mutex> Lock(m_mutex);
        EnginePtr = m_engine.get();
    }
    if (EnginePtr && EnginePtr->IsInitialized())
    {
        EnginePtr->Update(DeltaSeconds);
    }
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_AUDIO
