#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>

#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::Graphics
{
template<typename Contract>
class TLightFor;
struct DirectionalLightContract;
using DirectionalLight = TLightFor<DirectionalLightContract>;
class LightManager;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{

class RendererSystem;

/**
 * @brief Component that owns and synchronizes a renderer directional light.
 */
class DirectionalLightComponent : public BaseComponent, public ComponentCRTP<DirectionalLightComponent>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::DirectionalLightComponent";

    /**
     * @brief Runtime directional-light settings.
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::DirectionalLightComponent::Settings";

        bool Enabled = true; /**< @brief Master enable for light registration/update. */
        Vec3 Direction{-0.5f, -1.0f, -0.3f}; /**< @brief Light direction in world space. */
        Vec3 Color{1.0f, 1.0f, 1.0f}; /**< @brief RGB light color. */
        float Intensity = 1.0f; /**< @brief Light intensity multiplier. */
        bool CastShadows = true; /**< @brief Shadow casting toggle. */
        unsigned int CascadeCount = 4u; /**< @brief Cascade count for directional CSM. */
        unsigned int ShadowMapSize = 2048u; /**< @brief Per-cascade shadow map resolution. */
        float ShadowBias = 0.005f; /**< @brief Receiver bias used in shadow sampling. */
        float ShadowFarDistance = 300.0f; /**< @brief Max camera distance covered by directional shadows. */
        float SoftnessFactor = 1.0f; /**< @brief Soft-shadow kernel scale. */
        bool SoftShadows = true; /**< @brief Enables PCF/soft-shadow sampling. */
        bool ContactHardening = false; /**< @brief Enables contact-hardening approximation. */
        bool CascadeBlending = true; /**< @brief Enables blend band between cascades. */
    };

    const Settings& GetSettings() const
    {
        return m_settings;
    }

    Settings& EditSettings()
    {
        return m_settings;
    }

    SnAPI::Graphics::DirectionalLight* Light();
    const SnAPI::Graphics::DirectionalLight* Light() const;

    void OnCreate();
    void OnDestroy();
    void Tick(float DeltaSeconds);

    /** @brief Non-virtual tick entry used by ECS runtime bridge. */
    void RuntimeTick(float DeltaSeconds);
    void OnCreateImpl(IWorld&) { OnCreate(); }
    void OnDestroyImpl(IWorld&) { OnDestroy(); }
    void TickImpl(IWorld&, float DeltaSeconds) { RuntimeTick(DeltaSeconds); }

private:
    RendererSystem* ResolveRendererSystem() const;
    void EnsureLightRegistered();
    void ApplyLightSettings();
    void ReleaseLight();
    void UpdateLight(float DeltaSeconds);

    Settings m_settings{}; /**< @brief Runtime light settings. */
    std::shared_ptr<SnAPI::Graphics::DirectionalLight> m_light{}; /**< @brief Owned/shared renderer directional light handle. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
