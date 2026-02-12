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
#if defined(SNAPI_GF_ENABLE_NETWORKING)
class NetworkSystem;
#endif

/**
 * @brief Component that drives a SnAPI.Audio emitter.
 * @remarks
 * Provides gameplay-facing audio controls with optional network-aware dispatch.
 *
 * Runtime behavior:
 * - Maintains a world-audio emitter handle for this component.
 * - Applies source transform from owning node transform when available.
 * - Lazily loads/unloads sound assets based on settings and playback requests.
 *
 * Networking behavior (when enabled):
 * - `Play()` / `Stop()` are ergonomic entry points that branch on role and forward
 *   through reflected RPC endpoints (`PlayServer`/`PlayClient`, `StopServer`/`StopClient`).
 * - Dedicated server instances skip local audio emission in client endpoints.
 */
class AudioSourceComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AudioSourceComponent";

    /**
     * @brief Configurable settings for the audio source.
     * @remarks
     * Nested struct is reflected/serialized. Replication behavior depends on per-field flags
     * and codec availability (default built-ins replicate selected nested fields).
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::AudioSourceSettings";

        std::string SoundPath; /**< @brief Asset path/URI resolved by AudioEngine loaders. */
        bool Streaming = false; /**< @brief `true` uses stream loader; `false` loads resident sample data. */
        bool AutoPlay = false; /**< @brief If true, `OnCreate` requests immediate playback. */
        bool Looping = false; /**< @brief Loop mode forwarded to emitter playback state. */
        float Volume = 1.0f; /**< @brief Post-spatialization gain scalar. */
        float SpatialGain = 1.0f; /**< @brief Pre-attenuation spatial gain scalar. */
        float MinDistance = 1.0f; /**< @brief Near attenuation boundary used by spatial model. */
        float MaxDistance = 50.0f; /**< @brief Far attenuation boundary used by spatial model. */
        float Rolloff = 1.0f; /**< @brief Distance falloff exponent/curve control. */
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
     * @remarks Caller edits are applied during tick/refresh path; no implicit immediate reload.
     */
    Settings& EditSettings()
    {
        return m_settings;
    }

    /**
     * @brief Lifecycle hook after component creation.
     * @remarks Ensures emitter allocation and honors `Settings::AutoPlay`.
     */
    void OnCreate() override;
    /**
     * @brief Lifecycle hook before destruction.
     * @remarks Clears local audio state without world/network virtual dispatch during teardown.
     */
    void OnDestroy() override;
    /**
     * @brief Per-frame maintenance tick.
     * @param DeltaSeconds Frame delta time.
     * @remarks Keeps emitter parameters and transform synchronized with current settings/owner state.
     */
    void Tick(float DeltaSeconds) override;

    /**
     * @brief Start playback.
     * @remarks
     * Gameplay entry point.
     * - Client: forwards to server RPC endpoint.
     * - Server: executes server path then fan-out to clients.
     * - Offline/local: executes client playback path directly.
     */
    void Play();
    /**
     * @brief Stop playback.
     * @remarks Mirrors role-routing semantics of `Play()`.
     */
    void Stop();

    /**
     * @brief RPC server endpoint for Play().
     * @remarks Authoritative path that triggers multicast `PlayClient`.
     */
    void PlayServer();
    /**
     * @brief RPC client/multicast endpoint for Play().
     * @remarks Performs actual local audio emission on client/listen-server peers.
     */
    void PlayClient();
    /**
     * @brief RPC server endpoint for Stop().
     * @remarks Authoritative path that triggers multicast `StopClient`.
     */
    void StopServer();
    /**
     * @brief RPC client/multicast endpoint for Stop().
     * @remarks Performs actual local emitter stop on client/listen-server peers.
     */
    void StopClient();

    /**
     * @brief Check whether emitter is currently playing.
     * @return True when backend emitter reports active playback.
     */
    bool IsPlaying() const;
    /**
     * @brief Check whether a valid sound resource is loaded.
     * @return True when a sound handle is currently valid.
     */
    bool IsLoaded() const;
    /**
     * @brief Load sound data for this source.
     * @param Path Asset path/URI to load.
     * @param StreamingMode Streaming/resident selection.
     * @return True on successful load and handle assignment.
     * @remarks Replaces any currently loaded sound.
     */
    bool LoadSound(const std::string& Path, bool StreamingMode);
    /**
     * @brief Unload currently loaded sound resource.
     * @remarks Clears loaded-path bookkeeping and invalidates sound handle.
     */
    void UnloadSound();

protected:
    Settings m_settings{}; /**< @brief Editable source configuration used by tick/playback logic. */

private:
    /** @brief Resolve world audio subsystem for this component instance. */
    AudioSystem* ResolveAudioSystem() const;
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    /** @brief Resolve world networking subsystem for role/RPC dispatch decisions. */
    NetworkSystem* ResolveNetworkSystem() const;
#endif
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
