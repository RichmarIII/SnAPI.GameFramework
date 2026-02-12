#pragma once

#include "IComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

#if defined(SNAPI_GF_ENABLE_AUDIO)

class AudioSystem;

/**
 * @brief Component that drives the shared SnAPI.Audio listener.
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
    /** @brief Enable or disable updates for this listener. */
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

    void OnCreate() override;
    void Tick(float DeltaSeconds) override;

private:
    AudioSystem* ResolveAudioSystem() const;
    bool m_active = true;
    Vec3 m_lastPosition{};
    bool m_hasLastPosition = false;
};

#endif // SNAPI_GF_ENABLE_AUDIO

} // namespace SnAPI::GameFramework
