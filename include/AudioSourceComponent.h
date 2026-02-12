#pragma once

#include <string>

#include "IComponent.h"
#include "Math.h"

#if defined(SNAPI_GF_ENABLE_AUDIO)
#include <AudioEngine.h>
#endif

namespace SnAPI::GameFramework
{

#if defined(SNAPI_GF_ENABLE_AUDIO)

class AudioSystem;

/**
 * @brief Component that drives a SnAPI.Audio emitter.
 * @remarks Uses the owning node's TransformComponent when present.
 */
class AudioSourceComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AudioSourceComponent";

    /**
     * @brief Configurable settings for the audio source.
     * @remarks Stored as a nested data container to group runtime tweaks.
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::AudioSourceSettings";

        std::string SoundPath; /**< @brief Path or URI to the audio asset. */
        bool Streaming = false; /**< @brief Stream audio instead of loading resident. */
        bool AutoPlay = false; /**< @brief Start playback once the sound is loaded. */
        bool Looping = false; /**< @brief Loop playback. */
        float Volume = 1.0f; /**< @brief Gain applied after spatialization. */
        float SpatialGain = 1.0f; /**< @brief Gain applied before distance attenuation. */
        float MinDistance = 1.0f; /**< @brief Distance where attenuation begins. */
        float MaxDistance = 50.0f; /**< @brief Distance where attenuation maxes out. */
        float Rolloff = 1.0f; /**< @brief Attenuation curve exponent. */
    };

    /**
     * @brief Access settings (const).
     * @return Settings reference.
     */
    const Settings& GetSettings() const
    {
        return m_settings;
    }
    /**
     * @brief Access settings for modification.
     * @return Settings reference.
     */
    Settings& EditSettings()
    {
        return m_settings;
    }

    void OnCreate() override;
    void OnDestroy() override;
    void Tick(float DeltaSeconds) override;

    void Play();
    void Stop();
    bool IsPlaying() const;
    bool IsLoaded() const;
    bool LoadSound(const std::string& Path, bool StreamingMode);
    void UnloadSound();

protected:
    Settings m_settings{};

private:
    AudioSystem* ResolveAudioSystem() const;
    void EnsureEmitter();
    void UpdateEmitterTransform(float DeltaSeconds);
    void RefreshPlaybackState();

    SnAPI::Audio::SoundHandle m_sound{};
    SnAPI::Audio::EmitterHandle m_emitter{};
    std::string m_loadedPath{};
    bool m_loadedStreaming = false;
    bool m_playRequested = false;
    float m_lastVolume = 1.0f;
    bool m_lastLooping = false;
    Vec3 m_lastPosition{};
    bool m_hasLastPosition = false;
};

#endif // SNAPI_GF_ENABLE_AUDIO

} // namespace SnAPI::GameFramework
