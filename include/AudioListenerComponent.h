#pragma once

#include "IComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

#if defined(SNAPI_GF_ENABLE_AUDIO)

class AudioSystem;

/**
 * @brief Component that drives the shared SnAPI.Audio listener.
 * @remarks
 * Uses owning node transform as listener pose source and pushes updates into
 * world audio system listener state each tick when active.
 * The actual audio engine frame update is performed by `World`.
 *
 * Networking-enabled behavior mirrors AudioSource:
 * - Gameplay entry (`SetActive`) routes by role.
 * - Server endpoint fans out to client endpoint.
 */
class AudioListenerComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AudioListenerComponent";

    /** @brief Check whether this listener updates the audio system. */
    bool Active() const
    {
        return m_active;
    }
    /** @brief Enable or disable listener pose updates and activation state. */
    void Active(bool Active)
    {
        m_active = Active;
    }

    /** @brief Access active flag storage (const) for reflection/serialization. */
    const bool& GetActive() const
    {
        return m_active;
    }

    /** @brief Access active flag storage for reflection/serialization. */
    bool& EditActive()
    {
        return m_active;
    }

    /** @brief Lifecycle hook after creation; prepares listener-side runtime state. */
    void OnCreate() override;
    /**
     * @brief Per-frame update.
     * @param DeltaSeconds Frame delta seconds.
     * @remarks If active, synchronizes listener transform into audio engine.
     */
    void Tick(float DeltaSeconds) override;

    /**
     * @brief Gameplay-facing setter.
     * @param ActiveValue New active state.
     * @remarks Role-aware ergonomic entry point (client->server RPC, server->multicast fan-out).
     */
    void SetActive(bool ActiveValue);
    /**
     * @brief RPC server endpoint for SetActive().
     * @remarks Authoritative path, forwards to multicast client endpoint.
     */
    void SetActiveServer(bool ActiveValue);
    /**
     * @brief RPC client/multicast endpoint for SetActive().
     * @remarks Applies local listener active state.
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
