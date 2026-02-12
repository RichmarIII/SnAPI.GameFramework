#pragma once

#if defined(SNAPI_GF_ENABLE_AUDIO)

#include <memory>
#include <mutex>

#include <AudioEngine.h>

namespace SnAPI::Audio
{
struct AudioDeviceSpec;
} // namespace SnAPI::Audio

namespace SnAPI::GameFramework
{

/**
 * @brief Shared audio system wrapper for SnAPI.Audio.
 * @remarks Provides lazy initialization and per-frame updates.
 */
class AudioSystem final
{
public:
    AudioSystem() = default;
    ~AudioSystem();
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
    AudioSystem(AudioSystem&& Other) noexcept;
    AudioSystem& operator=(AudioSystem&& Other) noexcept;

    /**
     * @brief Initialize the shared audio engine.
     * @param Spec Device specification override.
     * @return True if initialization succeeds or is already initialized.
     */
    bool Initialize();
    bool Initialize(const SnAPI::Audio::AudioDeviceSpec& Spec);

    /**
     * @brief Shut down the shared audio engine.
     */
    void Shutdown();

    /**
     * @brief Check whether the audio engine is initialized.
     * @return True if initialized.
     */
    bool IsInitialized() const;

    /**
     * @brief Access the shared audio engine.
     * @return Pointer to AudioEngine or nullptr.
     */
    SnAPI::Audio::AudioEngine* Engine();
    const SnAPI::Audio::AudioEngine* Engine() const;

    /**
     * @brief Update the audio system for this frame.
     * @param DeltaSeconds Time since last update.
     */
    void Update(float DeltaSeconds);

private:
    mutable std::mutex m_mutex;
    std::unique_ptr<SnAPI::Audio::AudioEngine> m_engine;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_AUDIO
