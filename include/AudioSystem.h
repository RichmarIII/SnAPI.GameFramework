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
 * @ingroup SnAPI_GameFramework
 * @brief World-owned audio engine wrapper for SnAPI.Audio.
 *
 * `AudioSystem` owns the single shared audio engine used by a `World`. It gives
 * gameplay and component code one stable place to initialize audio output, query
 * readiness, and run per-frame backend maintenance without exposing engine lifetime
 * as a process-global concern.
 *
 * Core semantics:
 * - initialization is explicit and idempotent
 * - the engine object is lazily allocated on first initialize call
 * - `Update()` is the frame hook that lets the backend process pending work
 * - `Engine()` returns a borrowed pointer that is only valid while the subsystem remains alive and initialized
 *
 * Ownership and lifetime:
 * - Owned by `World`.
 * - Owns the backend `AudioEngine` instance.
 * - Returned engine pointers are non-owning.
 *
 * Threading model:
 * - Main-thread oriented.
 * - Cross-thread callers should marshal through `EnqueueTask(...)`.
 *
 * @see World
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
     * @brief Initialize the shared audio engine with default device settings.
     * @return `true` if initialization succeeds or was already complete.
     */
    bool Initialize();
    /**
     * @brief Initialize the shared audio engine with an explicit device specification.
     * @param Spec Device specification override forwarded to the backend.
     * @return `true` if initialization succeeds or was already complete.
     * @remarks Allows caller-provided backend device/sample configuration.
     */
    bool Initialize(const SnAPI::Audio::AudioDeviceSpec& Spec);

    /**
     * @brief Shut down the shared audio engine.
     * @remarks Safe to call repeatedly. Borrowed pointers from `Engine()` become invalid after shutdown.
     */
    void Shutdown();

    /**
     * @brief Check whether the audio engine is initialized.
     * @return True if initialized.
     */
    bool IsInitialized() const;

    /**
     * @brief Access the shared audio engine.
     * @return Non-owning pointer to `AudioEngine` or `nullptr`.
     * @warning Do not retain the pointer across `Shutdown()` or subsystem destruction.
     */
    SnAPI::Audio::AudioEngine* Engine();
    /**
     * @brief Access the shared audio engine (const).
     * @return Non-owning pointer to `AudioEngine` or `nullptr`.
     * @warning Do not retain the pointer across `Shutdown()` or subsystem destruction.
     */
    const SnAPI::Audio::AudioEngine* Engine() const;

    /**
     * @brief Update the audio system for this frame.
     * @param DeltaSeconds Time since last update in seconds.
     * @remarks No-op when the engine is absent or not initialized.
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
