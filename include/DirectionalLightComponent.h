#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>
#include <string_view>

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
 * @ingroup SnAPI_GameFramework
 * @brief Component that owns and synchronizes a renderer directional light.
 *
 * `DirectionalLightComponent` owns one renderer directional-light object and keeps it
 * registered with the world's `RendererSystem` light manager while the component is enabled.
 * The component is data-driven: every update pushes the current `Settings` state into the
 * renderer light rather than requiring gameplay code to touch the renderer directly.
 *
 * Core semantics:
 * - `Settings::Enabled` controls whether a renderer light should exist at all
 * - disabling releases the renderer light registration and owned shared handle
 * - enabling re-registers the light if the renderer/light manager is available
 * - color, intensity, shadow, and cascade settings are clamped to safe non-negative ranges before upload
 *
 * Ownership and lifetime:
 * - The component owns a shared handle to the renderer light it creates.
 * - `Light()` returns a non-owning pointer that becomes invalid after `OnDestroy()` or `ReleaseLight()`.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see RendererSystem
 */
class DirectionalLightComponent : public BaseComponent, public ComponentCRTP<DirectionalLightComponent>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::DirectionalLightComponent";

    /**
     * @brief Runtime directional-light settings.
     *
     * Units:
     * - `Direction` is a world-space direction vector.
     * - `ShadowFarDistance` is in world units.
     * - `ShadowBias` and `SoftnessFactor` are unitless tuning values.
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

    /** @brief Access the owned renderer light. @return Non-owning light pointer or `nullptr` when not registered. */
    SnAPI::Graphics::DirectionalLight* Light();
    /** @brief Access the owned renderer light (const). @return Non-owning light pointer or `nullptr` when not registered. */
    const SnAPI::Graphics::DirectionalLight* Light() const;

    /** @brief Register the light if enabled and apply current settings. */
    void OnCreate();
    /** @brief Remove the light from the renderer and release the owned handle. */
    void OnDestroy();
    /** @brief Variable-step update hook. @param DeltaSeconds Frame delta in seconds. */
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /** @brief Editor-only update hook. @param DeltaSeconds Frame delta in seconds. */
    void EditorTick(float DeltaSeconds);
    /** @brief Editor-only property change hook. @param Name Changed reflected field name. */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

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
