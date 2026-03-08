#pragma once

#include "BaseComponent.h"
#include "Math.h"
#include <string_view>

namespace SnAPI::GameFramework
{

#if defined(SNAPI_GF_ENABLE_AUDIO)

class AudioSystem;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Component that drives the world's shared audio listener transform.
 *
 * `AudioListenerComponent` maps the owning node's world transform into the single listener state
 * exposed by `AudioSystem` / `SnAPI.Audio`. It does not own a listener object of its own; instead,
 * each tick it updates the shared listener transform when active.
 *
 * Core semantics:
 * - Position and orientation are read from the owning node's world transform.
 * - Velocity is derived from the difference between the current and previous published positions.
 * - If no transform can be resolved, the listener falls back to origin with identity rotation.
 * - The low-level audio engine frame is advanced elsewhere by the world/audio system.
 *
 * Ownership and lifetime:
 * - The component owns only its active flag and velocity cache.
 * - The shared audio listener is owned by `AudioSystem`.
 * - No stable listener handle is exposed because the backend models a single listener.
 *
 * Threading model:
 * - Main-thread only.
 *
 * Networking semantics:
 * - `SetActive()` is the gameplay-facing, role-aware entry point.
 * - `SetActiveServer()` is the authoritative RPC endpoint.
 * - `SetActiveClient()` applies local state on the receiving peer.
 * - Direct mutation through `Active(bool)` or `EditActive()` is local-only and does not perform RPC.
 *
 * @warning Multiple active listener components in the same world are not arbitrated. The last one to
 * tick wins because every active instance writes into the same backend listener state.
 *
 * @note Disabling the component stops future listener updates but does not explicitly clear the audio
 * engine's current listener transform.
 *
 * @see AudioSystem
 * @see AudioSourceComponent
 */
class AudioListenerComponent : public BaseComponent, public ComponentCRTP<AudioListenerComponent>
{
public:
    /** @brief Stable reflected type name used for serialization registration. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AudioListenerComponent";

    /**
     * @brief Check whether this component currently writes listener updates.
     * @return `true` when `Tick()` will attempt to publish listener state.
     */
    bool Active() const
    {
        return m_active;
    }
    /**
     * @brief Locally enable or disable listener updates.
     * @param Active New local active state.
     * @warning This is a local state mutation only. Use `SetActive()` for gameplay/network-aware toggles.
     */
    void Active(bool Active)
    {
        m_active = Active;
    }

    /**
     * @brief Read the serialized active flag.
     * @return Borrowed reference to the stored active flag.
     */
    const bool& GetActive() const
    {
        return m_active;
    }

    /**
     * @brief Mutate the serialized active flag directly.
     * @return Borrowed reference to the stored active flag.
     * @warning This mutates local state only and does not propagate through RPC.
     */
    bool& EditActive()
    {
        return m_active;
    }

    /**
     * @brief Prepare audio-side dependencies for later listener updates.
     * @remarks Initializes the audio system on demand when audio ticking is enabled for the world.
     */
    void OnCreate();
    /**
     * @brief Publish the current listener transform when active.
     * @param DeltaSeconds Variable-step frame delta in seconds.
     *
     * Position and orientation come from the owning node's world transform. Velocity is estimated
     * from position delta over @p DeltaSeconds when a previous sample exists.
     */
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /**
     * @brief React to editor-side property edits.
     * @param Name Name of the changed reflected property.
     * @remarks A transition to active immediately pushes one listener update; other fields are ignored.
     */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

    /**
     * @brief Gameplay-facing, role-aware active-state setter.
     * @param ActiveValue New active state.
     *
     * Semantics:
     * - Client: attempts `SetActiveServer` RPC and returns if the RPC was accepted.
     * - Authority/offline: falls through to local application.
     */
    void SetActive(bool ActiveValue);
    /**
     * @brief Authoritative RPC endpoint for `SetActive()`.
     * @param ActiveValue New active state.
     * @remarks Applies server state first, then attempts to fan out through `SetActiveClient()`.
     */
    void SetActiveServer(bool ActiveValue);
    /**
     * @brief Local/client endpoint for `SetActive()`.
     * @param ActiveValue New active state.
     * @remarks Applies only local listener activity state.
     */
    void SetActiveClient(bool ActiveValue);

private:
    /** @brief Resolve world audio subsystem, if available. */
    AudioSystem* ResolveAudioSystem() const;
    bool m_active = true; /**< @brief Local listener activation gate. */
    Vec3 m_lastPosition{}; /**< @brief Last listener position pushed to backend (change detection). */
    bool m_hasLastPosition = false; /**< @brief True once listener position cache has been initialized. */
};

#endif // SNAPI_GF_ENABLE_AUDIO

} // namespace SnAPI::GameFramework
