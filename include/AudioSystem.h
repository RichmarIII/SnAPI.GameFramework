#pragma once

#if defined(SNAPI_GF_ENABLE_AUDIO)

#include <functional>
#include <memory>
#include "GameThreading.h"
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
 * - Internal state is game-thread owned (`GameMutex` assertion-only guard).
 * - Cross-thread callers should use `EnqueueTask`, which is the only path
 *   that takes a real mutex lock.
 */
class AudioSystem final : public ITaskDispatcher
{
public:
    using WorkTask = std::function<void(AudioSystem&)>;
    using CompletionTask = std::function<void(const TaskHandle&)>;

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

    /**
     * @brief Enqueue work on the audio system thread.
     * @param InTask Work callback executed on audio-thread affinity.
     * @param OnComplete Optional completion callback marshaled to caller dispatcher.
     * @return Task handle for wait/cancel polling.
     */
    TaskHandle EnqueueTask(WorkTask InTask, CompletionTask OnComplete = {});

    /**
     * @brief Enqueue a generic thread task for dispatcher marshalling.
     * @param InTask Callback to execute on this system thread.
     */
    void EnqueueThreadTask(std::function<void()> InTask) override;

    /**
     * @brief Execute all queued tasks on the audio thread.
     */
    void ExecuteQueuedTasks();

private:
    mutable GameMutex m_mutex; /**< @brief Audio-system thread affinity guard. */
    TSystemTaskQueue<AudioSystem> m_taskQueue{}; /**< @brief Cross-thread task handoff queue (real lock only on enqueue). */
    std::unique_ptr<SnAPI::Audio::AudioEngine> m_engine; /**< @brief Owned backend audio engine instance (null until initialized). */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_AUDIO
