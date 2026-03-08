#pragma once

#include <string>
#include <string_view>

#include "BaseComponent.h"
#include "Math.h"

#if defined(SNAPI_GF_ENABLE_AUDIO)
#include <AudioEngine.h>
#endif

namespace SnAPI::GameFramework
{

#if defined(SNAPI_GF_ENABLE_AUDIO)

class AudioSystem;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Component that drives a world-owned audio emitter and optional replicated playback.
 *
 * `AudioSourceComponent` is the gameplay-facing wrapper around a `SnAPI.Audio` emitter. It manages
 * emitter creation, sound loading, playback requests, and spatial transform updates based on the
 * owning node's world position.
 *
 * Core semantics:
 * - The component lazily creates an emitter when audio is available.
 * - Sound loading is path-driven and reloads automatically when `Settings::SoundPath` or
 *   `Settings::Streaming` changes.
 * - Playback can be deferred: if `Play()` is requested before the sound or audio engine is ready,
 *   the request is stored and fulfilled later when loading succeeds.
 * - Spatial updates publish position and derived velocity only; emitter orientation is not used.
 *
 * Ownership and lifetime:
 * - The component owns its emitter handle and the currently loaded sound handle.
 * - The actual emitter and sound resources are owned by the world audio engine/backend.
 * - Loaded sounds remain valid until explicitly unloaded, replaced, or the component is torn down.
 *
 * Threading model:
 * - Main-thread only.
 *
 * Networking semantics:
 * - `Play()` and `Stop()` are gameplay-facing, role-aware entry points.
 * - `PlayServer()` / `StopServer()` are authoritative endpoints that fan out to client endpoints.
 * - `PlayClient()` / `StopClient()` perform local backend work.
 * - Dedicated servers skip local playback and stop operations even when RPCs are received.
 *
 * Error semantics:
 * - Most failures are soft failures. Missing audio readiness, unresolved paths, or backend exceptions
 *   simply result in `false` returns or no-ops.
 * - Backend exceptions are swallowed by the implementation to keep gameplay flow alive.
 *
 * @see AudioSystem
 * @see AudioListenerComponent
 */
class AudioSourceComponent : public BaseComponent, public ComponentCRTP<AudioSourceComponent>
{
public:
    /** @brief Stable reflected type name used for serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AudioSourceComponent";

    /**
     * @ingroup SnAPI_GameFramework
     * @brief Configurable settings for the audio source.
     *
     * The settings describe what to load, how to spatialize it, and how playback should behave.
     * Mutating the settings does not immediately rebind the backend; changes are applied during the
     * normal tick/refresh path or explicit editor property-change handling.
     */
    struct Settings
    {
        /** @brief Stable reflected type name used for serialization registration. */
        static constexpr const char* kTypeName = "SnAPI::GameFramework::AudioSourceSettings";

        std::string SoundPath; /**< @brief Logical asset path or URI resolved through `PathResolver` before loading. */
        bool Streaming = false; /**< @brief Select streamed decoding when `true`; otherwise load a resident sample. */
        bool AutoPlay = false; /**< @brief Request playback from `OnCreate()` and from relevant editor property edits when `true`. */
        bool Looping = false; /**< @brief Forwarded to the backend emitter's looping flag. */
        float Volume = 1.0f; /**< @brief Non-spatial gain multiplier applied directly to the emitter. */
        float SpatialGain = 1.0f; /**< @brief Spatial gain scalar written into the emitter transform. */
        float MinDistance = 1.0f; /**< @brief Near attenuation distance in world units. */
        float MaxDistance = 50.0f; /**< @brief Far attenuation distance in world units. */
        float Rolloff = 1.0f; /**< @brief Backend rolloff/attenuation curve control. */
    };

    /**
     * @brief Read the current source settings.
     * @return Borrowed reference to the stored settings.
     */
    const Settings& GetSettings() const
    {
        return m_settings;
    }
    /**
     * @brief Mutate the current source settings.
     * @return Borrowed reference to the stored settings.
     * @remarks Changes are applied lazily during `Tick()`, `RefreshPlaybackState()`, or editor property handling.
     */
    Settings& EditSettings()
    {
        return m_settings;
    }

    /**
     * @brief Create runtime audio state on demand and honor autoplay.
     * @remarks Ensures an emitter exists if audio is available, then requests playback when autoplay is enabled.
     */
    void OnCreate();
    /**
     * @brief Clear local runtime audio state during teardown.
     * @remarks Avoids world/network dispatch during graph shutdown and invalidates cached handles locally.
     */
    void OnDestroy();
    /**
     * @brief Maintain emitter allocation, loaded sound state, and spatial transform.
     * @param DeltaSeconds Variable-step frame delta in seconds.
     * @remarks Position-derived velocity is computed from @p DeltaSeconds when a previous sample exists.
     */
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /**
     * @brief React to editor property edits for the source settings.
     * @param Name Name of the changed reflected property.
     * @remarks Relevant settings changes can trigger deferred autoplay, immediate reload checks, and transform refresh.
     */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

    /**
     * @brief Request playback of the configured sound.
     *
     * Semantics:
     * - Client: attempts `PlayServer()` RPC and returns if accepted.
     * - Authority/offline: falls through to local playback.
     * - If the sound or audio engine is not ready yet, playback remains pending through an internal flag.
     */
    void Play();
    /**
     * @brief Request playback stop.
     * @remarks Mirrors the role-routing semantics of `Play()`.
     */
    void Stop();

    /**
     * @brief Authoritative RPC endpoint for `Play()`.
     * @remarks Marks playback as pending locally, then attempts to fan out through `PlayClient()`.
     */
    void PlayServer();
    /**
     * @brief Local/client endpoint for `Play()`.
     * @remarks Performs actual backend playback on client and listen-server peers.
     */
    void PlayClient();
    /**
     * @brief Authoritative RPC endpoint for `Stop()`.
     * @remarks Clears pending playback state, then attempts to fan out through `StopClient()`.
     */
    void StopServer();
    /**
     * @brief Local/client endpoint for `Stop()`.
     * @remarks Performs actual backend stop on client and listen-server peers.
     */
    void StopClient();

    /**
     * @brief Check whether the current emitter reports active playback.
     * @return `true` when the backend emitter exists and reports that it is playing.
     */
    bool IsPlaying() const;
    /**
     * @brief Check whether a valid sound resource handle is currently loaded.
     * @return `true` when the current sound handle is valid.
     */
    bool IsLoaded() const;
    /**
     * @brief Load sound data for this source.
     * @param Path Logical asset path or URI resolved through `PathResolver`.
     * @param StreamingMode Choose streamed or resident loading.
     * @return `true` on successful load and handle assignment.
     *
     * Semantics:
     * - Resolves @p Path to a filesystem path before calling the backend.
     * - Unloads any currently loaded sound first.
     * - If playback was pending, a successful load also starts playback immediately.
     */
    bool LoadSound(const std::string& Path, bool StreamingMode);
    /**
     * @brief Unload the currently loaded sound resource, if any.
     * @remarks Clears load bookkeeping and invalidates the sound handle even if backend unload fails or is unavailable.
     */
    void UnloadSound();

protected:
    Settings m_settings{}; /**< @brief Editable source configuration used by tick/playback logic. */

private:
    /** @brief Resolve world audio subsystem for this component instance. */
    AudioSystem* ResolveAudioSystem() const;
    /** @brief Lazily create/validate emitter handle. */
    void EnsureEmitter();
    /** @brief Push owner transform into audio emitter state. */
    void UpdateEmitterTransform(float DeltaSeconds);
    /** @brief Apply settings deltas (volume/looping/load state) to backend emitter. */
    void RefreshPlaybackState();

    SnAPI::Audio::SoundHandle m_sound{}; /**< @brief Active sound resource handle currently bound/loaded. */
    SnAPI::Audio::EmitterHandle m_emitter{}; /**< @brief Backend emitter handle owned by world audio engine. */
    std::string m_loadedPath{}; /**< @brief Path of the currently loaded sound (for change detection). */
    bool m_loadedStreaming = false; /**< @brief Streaming mode used by current load (for change detection). */
    bool m_playRequested = false; /**< @brief Deferred play intent used while waiting for load/engine readiness. */
    float m_lastVolume = 1.0f; /**< @brief Last applied volume cache to avoid redundant backend calls. */
    bool m_lastLooping = false; /**< @brief Last applied loop-state cache to avoid redundant backend calls. */
    Vec3 m_lastPosition{}; /**< @brief Last emitted spatial position cache. */
    bool m_hasLastPosition = false; /**< @brief True once position cache has been initialized. */
};

#endif // SNAPI_GF_ENABLE_AUDIO

} // namespace SnAPI::GameFramework
