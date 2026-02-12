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
 * @remarks
 * World-owned subsystem that encapsulates backend engine lifetime.
 * Initialization is explicit/lazy; callers can probe readiness via `IsInitialized`.
 *
 * Threading:
 * - Internal mutex guards engine pointer transitions.
 * - Typical usage is world-main-thread lifecycle and per-frame update calls.
 */
class AudioSystem final
{
public:
    /** @brief Construct an uninitialized audio system. */
    AudioSystem() = default;
    /** @brief Destructor; shuts down engine if initialized. */
    ~AudioSystem();
    /** @brief Non-copyable due to engine ownership/mutex state. */
    AudioSystem(const AudioSystem&) = delete;
    /** @brief Non-copyable due to engine ownership/mutex state. */
    AudioSystem& operator=(const AudioSystem&) = delete;
    /** @brief Movable; transfers engine ownership. */
    AudioSystem(AudioSystem&& Other) noexcept;
    /** @brief Move assign; transfers engine ownership safely. */
    AudioSystem& operator=(AudioSystem&& Other) noexcept;

    /**
     * @brief Initialize the shared audio engine.
     * @param Spec Device specification override.
     * @return True if initialization succeeds or is already initialized.
     * @remarks Allows caller-provided backend device/sample configuration.
     */
    bool Initialize();
    bool Initialize(const SnAPI::Audio::AudioDeviceSpec& Spec);

    /**
     * @brief Shut down the shared audio engine.
     * @remarks Safe to call repeatedly.
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
     * @remarks Borrowed pointer; do not store past subsystem lifetime changes.
     */
    SnAPI::Audio::AudioEngine* Engine();
    const SnAPI::Audio::AudioEngine* Engine() const;

    /**
     * @brief Update the audio system for this frame.
     * @param DeltaSeconds Time since last update.
     */
    void Update(float DeltaSeconds);

private:
    mutable std::mutex m_mutex; /**< @brief Guards engine pointer/lifecycle state transitions. */
    std::unique_ptr<SnAPI::Audio::AudioEngine> m_engine; /**< @brief Owned backend audio engine instance (null until initialized). */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_AUDIO
