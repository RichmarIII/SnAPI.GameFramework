#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"
#include "Math.h"
#include "ReflectionAnnotations.h"


namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-driven node that configures atmospheric scattering feature settings for one or more viewports.
 *
 * `AtmosphereParamsNode` stores the physically-inspired atmosphere parameters that should be pushed
 * into renderer-owned atmosphere feature state. It does not create the feature state or the viewport; it
 * waits for those resources to exist, sanitizes its stored state, then uploads the current values.
 *
 * Viewport selection semantics:
 * - Negative viewport ids target all current renderer viewports.
 * - Non-negative ids target exactly one renderer viewport with the same numeric id.
 * - Recreated feature profiles are detected automatically through the renderer's feature-profile revision.
 *
 * Parameter semantics:
 * - Direction vectors are interpreted in world space and normalized before upload.
 * - Scattering and absorption coefficients are non-negative and sanitized if non-finite.
 * - Planet and atmosphere radii are expressed in meters.
 * - Sample counts are integer quality controls for the view and sun integration loops.
 *
 * Ownership and lifetime:
 * - The node owns only serialized atmosphere configuration.
 * - The renderer owns the actual atmosphere feature resources and may recreate them at any time.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning Editing fields through `Edit*()` changes stored values immediately, but renderer-side
 * state changes remain lazy and depend on the normal apply/retry hooks.
 *
 * @see RendererSystem
 * @see WorldRenderSettings
 */
SnType()
class SNAPI_GAMEFRAMEWORK_API AtmosphereParamsNode : public BaseNode, public NodeCRTP<AtmosphereParamsNode>
{
public:
    /** @brief Stable reflected type name used for serialization and asset lookup. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AtmosphereParamsNode";

    /** @brief Construct an unnamed atmosphere settings node with default Earth-like tuning values. */
    AtmosphereParamsNode();
    /**
     * @brief Construct a named atmosphere settings node.
     * @param Name Debug/editor-facing node name stored by the base node.
     */
    explicit AtmosphereParamsNode(std::string Name);

    /** @brief Access the target viewport selector. @return Mutable viewport id; negative means all current viewports. */
    SnField(SnKey("ViewportID"), SnConstGetter(GetViewportID))
    std::int64_t& EditViewportID();
    /** @brief Read the target viewport selector. @return Stored viewport id; negative means all current viewports. */
    const std::int64_t& GetViewportID() const;

    /** @brief Access the world-atmosphere feature toggle. @return Mutable flag enabling world-mode atmosphere coordinates. */
    SnField(SnKey("WorldMode"), SnConstGetter(GetWorldMode))
    bool& EditWorldMode();
    /** @brief Read the world-atmosphere feature toggle. @return Stored world-mode flag. */
    const bool& GetWorldMode() const;

    /** @brief Access the primary sun direction. @return Mutable world-space direction vector; normalized before upload. */
    SnField(SnKey("SunDirection"), SnConstGetter(GetSunDirection))
    Vec3& EditSunDirection();
    /** @brief Read the primary sun direction. @return Stored world-space direction vector. */
    const Vec3& GetSunDirection() const;

    /** @brief Access the sun spectral color multiplier. @return Mutable non-negative RGB value. */
    SnField(SnKey("SunColor"), SnConstGetter(GetSunColor))
    Vec3& EditSunColor();
    /** @brief Read the sun spectral color multiplier. @return Stored RGB value. */
    const Vec3& GetSunColor() const;

    /** @brief Access the final atmosphere exposure. @return Mutable non-negative exposure scalar. */
    SnField(SnKey("Exposure"), SnConstGetter(GetExposure))
    float& EditExposure();
    /** @brief Read the final atmosphere exposure. @return Stored exposure scalar. */
    const float& GetExposure() const;

    /** @brief Access the sun radiance multiplier. @return Mutable non-negative intensity scalar. */
    SnField(SnKey("SunIntensity"), SnConstGetter(GetSunIntensity))
    float& EditSunIntensity();
    /** @brief Read the sun radiance multiplier. @return Stored intensity scalar. */
    const float& GetSunIntensity() const;

    /** @brief Access Rayleigh scattering coefficients. @return Mutable non-negative RGB scattering coefficients. */
    SnField(SnKey("RayleighScattering"), SnConstGetter(GetRayleighScattering))
    Vec3& EditRayleighScattering();
    /** @brief Read Rayleigh scattering coefficients. @return Stored RGB scattering coefficients. */
    const Vec3& GetRayleighScattering() const;

    /** @brief Access the Rayleigh scale height. @return Mutable scale height in meters; sanitized to stay positive. */
    SnField(SnKey("RayleighScaleHeight"), SnConstGetter(GetRayleighScaleHeight))
    float& EditRayleighScaleHeight();
    /** @brief Read the Rayleigh scale height. @return Stored scale height in meters. */
    const float& GetRayleighScaleHeight() const;

    /** @brief Access Mie scattering coefficients. @return Mutable non-negative RGB scattering coefficients. */
    SnField(SnKey("MieScattering"), SnConstGetter(GetMieScattering))
    Vec3& EditMieScattering();
    /** @brief Read Mie scattering coefficients. @return Stored RGB scattering coefficients. */
    const Vec3& GetMieScattering() const;

    /** @brief Access the Mie scale height. @return Mutable scale height in meters; sanitized to stay positive. */
    SnField(SnKey("MieScaleHeight"), SnConstGetter(GetMieScaleHeight))
    float& EditMieScaleHeight();
    /** @brief Read the Mie scale height. @return Stored scale height in meters. */
    const float& GetMieScaleHeight() const;

    /** @brief Access Mie absorption coefficients. @return Mutable non-negative RGB absorption coefficients. */
    SnField(SnKey("MieAbsorption"), SnConstGetter(GetMieAbsorption))
    Vec3& EditMieAbsorption();
    /** @brief Read Mie absorption coefficients. @return Stored RGB absorption coefficients. */
    const Vec3& GetMieAbsorption() const;

    /** @brief Access the Mie anisotropy parameter. @return Mutable forward-scattering `g` term. */
    SnField(SnKey("MieAnisotropyG"), SnConstGetter(GetMieAnisotropyG))
    float& EditMieAnisotropyG();
    /** @brief Read the Mie anisotropy parameter. @return Stored `g` term. */
    const float& GetMieAnisotropyG() const;

    /** @brief Access the planet radius. @return Mutable radius in meters; sanitized to a finite positive value. */
    SnField(SnKey("PlanetRadiusMeters"), SnConstGetter(GetPlanetRadiusMeters))
    float& EditPlanetRadiusMeters();
    /** @brief Read the planet radius. @return Stored radius in meters. */
    const float& GetPlanetRadiusMeters() const;

    /** @brief Access the atmosphere outer radius. @return Mutable radius in meters; forced above planet radius when applied. */
    SnField(SnKey("AtmosphereRadiusMeters"), SnConstGetter(GetAtmosphereRadiusMeters))
    float& EditAtmosphereRadiusMeters();
    /** @brief Read the atmosphere outer radius. @return Stored radius in meters. */
    const float& GetAtmosphereRadiusMeters() const;

    /** @brief Access the camera offset above ground. @return Mutable offset in meters. */
    SnField(SnKey("CameraGroundOffsetMeters"), SnConstGetter(GetCameraGroundOffsetMeters))
    float& EditCameraGroundOffsetMeters();
    /** @brief Read the camera offset above ground. @return Stored offset in meters. */
    const float& GetCameraGroundOffsetMeters() const;

    /** @brief Access the maximum sun-distance parameter. @return Mutable distance in meters. */
    SnField(SnKey("MaxSunDistanceMeters"), SnConstGetter(GetMaxSunDistanceMeters))
    float& EditMaxSunDistanceMeters();
    /** @brief Read the maximum sun-distance parameter. @return Stored distance in meters. */
    const float& GetMaxSunDistanceMeters() const;

    /** @brief Access the number of primary view samples. @return Mutable sample count controlling atmospheric integration quality. */
    SnField(SnKey("ViewSampleCount"), SnConstGetter(GetViewSampleCount))
    std::uint32_t& EditViewSampleCount();
    /** @brief Read the number of primary view samples. @return Stored sample count. */
    const std::uint32_t& GetViewSampleCount() const;

    /** @brief Access the number of sun-light samples. @return Mutable sample count controlling lighting quality. */
    SnField(SnKey("SunSampleCount"), SnConstGetter(GetSunSampleCount))
    std::uint32_t& EditSunSampleCount();
    /** @brief Read the number of sun-light samples. @return Stored sample count. */
    const std::uint32_t& GetSunSampleCount() const;

    /** @brief Access the multi-scattering strength term. @return Mutable non-negative scattering strength scalar. */
    SnField(SnKey("MultiScatterStrength"), SnConstGetter(GetMultiScatterStrength))
    float& EditMultiScatterStrength();
    /** @brief Read the multi-scattering strength term. @return Stored scattering strength scalar. */
    const float& GetMultiScatterStrength() const;

    /**
     * @brief Mark the node dirty and attempt an initial atmosphere upload.
     * @remarks Safe before renderer readiness; missing renderer feature state simply cause later retries.
     */
    void OnCreate();
    void OnDestroy();
    /** @brief Retry feature-setting application when needed. @param DeltaSeconds Variable-step frame delta in seconds. Currently unused. */
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /** @brief Editor-only retry hook. @param DeltaSeconds Variable-step editor frame delta in seconds. Currently unused. */
    void EditorTick(float DeltaSeconds);
    /** @brief Mark the node dirty after reflected editor edits. @param Name Name of the changed property. Currently unused. */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    void ApplyIfNeeded();
    bool ApplyFeatureSettings();
    void InvalidateApplyState();

    std::int64_t m_viewportID = -1;

    bool m_worldMode = false;
    Vec3 m_sunDirection{static_cast<Scalar>(0.70710677), static_cast<Scalar>(0.70710677), static_cast<Scalar>(0.0)};
    Vec3 m_sunColor{static_cast<Scalar>(1.0), static_cast<Scalar>(1.0), static_cast<Scalar>(1.0)};
    float m_exposure = 8.0f;
    float m_sunIntensity = 1.0f;

    Vec3 m_rayleighScattering{static_cast<Scalar>(5.8e-6), static_cast<Scalar>(13.5e-6), static_cast<Scalar>(33.1e-6)};
    float m_rayleighScaleHeight = 8000.0f;
    Vec3 m_mieScattering{static_cast<Scalar>(21.0e-6), static_cast<Scalar>(21.0e-6), static_cast<Scalar>(21.0e-6)};
    float m_mieScaleHeight = 1200.0f;
    Vec3 m_mieAbsorption{static_cast<Scalar>(0.0), static_cast<Scalar>(0.0), static_cast<Scalar>(0.0)};
    float m_mieAnisotropyG = 0.76f;

    float m_planetRadiusMeters = 6360.0e3f;
    float m_atmosphereRadiusMeters = 6420.0e3f;
    float m_cameraGroundOffsetMeters = 100.0f;
    float m_maxSunDistanceMeters = 120.0e3f;

    std::uint32_t m_viewSampleCount = 4;
    std::uint32_t m_sunSampleCount = 4;
    float m_multiScatterStrength = 2.0e-6f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedFeatureRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
