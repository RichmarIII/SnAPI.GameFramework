#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"
#include "Math.h"
#include "ReflectionAnnotations.h"

namespace SnAPI::Graphics
{
class HeightFogPass;
}

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-driven node that configures height-fog passes for one or more render viewports.
 *
 * `HeightFogParamsNode` is the world-facing contract for volumetric-style height fog tuning. The
 * node stores fog parameters, waits for a compatible `HeightFogPass` to appear in the selected
 * viewport, and then uploads the sanitized values. It may apply to multiple fullscreen passes per
 * viewport because the renderer can host several fullscreen stages and only some of them are height
 * fog passes.
 *
 * Height semantics:
 * - Absolute-height mode interprets offsets and rebase origins in world-space meters.
 * - Rebased mode interprets `HeightOffsetRebased` in the renderer's rebased local frame.
 * - When `UseActiveCameraYAsRebaseOrigin` is true, the renderer's active camera Y position overrides
 *   the stored rebase origin while uploading absolute-height settings.
 *
 * Viewport selection semantics:
 * - Negative viewport ids target all current render viewports.
 * - Non-negative ids target one viewport by renderer id.
 * - Pass-graph rebuilds trigger automatic reapplication through the cached revision check.
 *
 * Ownership and lifetime:
 * - The node owns only serialized fog parameters.
 * - Matching fog passes and the active camera are borrowed from `RendererSystem`.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning Mutable `Edit*()` accessors update stored values immediately, but renderer-side state
 * changes happen lazily during the node's normal apply/retry hooks.
 *
 * @see RendererSystem
 * @see WorldRenderSettings
 */
SnType()
class SNAPI_GAMEFRAMEWORK_API HeightFogParamsNode : public BaseNode, public NodeCRTP<HeightFogParamsNode>
{
public:
    /** @brief Stable reflected type name used for serialization and asset lookup. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::HeightFogParamsNode";

    /** @brief Construct an unnamed height-fog settings node with default atmospheric-fog tuning values. */
    HeightFogParamsNode();
    /**
     * @brief Construct a named height-fog settings node.
     * @param Name Debug/editor-facing node name stored by the base node.
     */
    explicit HeightFogParamsNode(std::string Name);

    /** @brief Access the target viewport selector. @return Mutable viewport id; negative means all current viewports. */
    SnField(SnKey("ViewportID"), SnConstGetter(GetViewportID))
    std::int64_t& EditViewportID();
    /** @brief Read the target viewport selector. @return Stored viewport id; negative means all current viewports. */
    const std::int64_t& GetViewportID() const;

    /** @brief Access fog density. @return Mutable non-negative density scalar. */
    SnField(SnKey("Density"), SnConstGetter(GetDensity))
    float& EditDensity();
    /** @brief Read fog density. @return Stored density scalar. */
    const float& GetDensity() const;

    /** @brief Access vertical density falloff. @return Mutable non-negative falloff scalar. */
    SnField(SnKey("HeightFalloff"), SnConstGetter(GetHeightFalloff))
    float& EditHeightFalloff();
    /** @brief Read vertical density falloff. @return Stored falloff scalar. */
    const float& GetHeightFalloff() const;

    /** @brief Access the height-mode selector. @return Mutable flag choosing absolute-height vs rebased-height mode. */
    SnField(SnKey("UseAbsoluteHeight"), SnConstGetter(GetUseAbsoluteHeight))
    bool& EditUseAbsoluteHeight();
    /** @brief Read the height-mode selector. @return Stored absolute-height flag. */
    const bool& GetUseAbsoluteHeight() const;

    /** @brief Access the absolute fog height offset. @return Mutable world-space Y offset in meters. */
    SnField(SnKey("HeightOffsetAbsoluteY"), SnConstGetter(GetHeightOffsetAbsoluteY))
    double& EditHeightOffsetAbsoluteY();
    /** @brief Read the absolute fog height offset. @return Stored world-space Y offset in meters. */
    const double& GetHeightOffsetAbsoluteY() const;

    /** @brief Access the camera-driven rebase-origin toggle. @return Mutable flag selecting active-camera Y as the rebase origin. */
    SnField(SnKey("UseActiveCameraYAsRebaseOrigin"), SnConstGetter(GetUseActiveCameraYAsRebaseOrigin))
    bool& EditUseActiveCameraYAsRebaseOrigin();
    /** @brief Read the camera-driven rebase-origin toggle. @return Stored flag selecting active-camera Y as the rebase origin. */
    const bool& GetUseActiveCameraYAsRebaseOrigin() const;

    /** @brief Access the explicit absolute rebase origin. @return Mutable world-space Y coordinate in meters. */
    SnField(SnKey("RebaseOriginAbsoluteY"), SnConstGetter(GetRebaseOriginAbsoluteY))
    double& EditRebaseOriginAbsoluteY();
    /** @brief Read the explicit absolute rebase origin. @return Stored world-space Y coordinate in meters. */
    const double& GetRebaseOriginAbsoluteY() const;

    /** @brief Access the rebased fog height offset. @return Mutable local-space height offset in rebased renderer units. */
    SnField(SnKey("HeightOffsetRebased"), SnConstGetter(GetHeightOffsetRebased))
    float& EditHeightOffsetRebased();
    /** @brief Read the rebased fog height offset. @return Stored local-space height offset. */
    const float& GetHeightOffsetRebased() const;

    /** @brief Access the fog start distance from the camera. @return Mutable non-negative distance in renderer scene units. */
    SnField(SnKey("StartDistance"), SnConstGetter(GetStartDistance))
    float& EditStartDistance();
    /** @brief Read the fog start distance from the camera. @return Stored start distance. */
    const float& GetStartDistance() const;

    /** @brief Access the base fog color. @return Mutable RGB fog color. */
    SnField(SnKey("FogColor"), SnConstGetter(GetFogColor))
    Vec3& EditFogColor();
    /** @brief Read the base fog color. @return Stored RGB fog color. */
    const Vec3& GetFogColor() const;

    /** @brief Access the horizon tint used for sky blending. @return Mutable RGB horizon color. */
    SnField(SnKey("HorizonColor"), SnConstGetter(GetHorizonColor))
    Vec3& EditHorizonColor();
    /** @brief Read the horizon tint used for sky blending. @return Stored RGB horizon color. */
    const Vec3& GetHorizonColor() const;

    /** @brief Access the zenith tint used for sky blending. @return Mutable RGB zenith color. */
    SnField(SnKey("ZenithColor"), SnConstGetter(GetZenithColor))
    Vec3& EditZenithColor();
    /** @brief Read the zenith tint used for sky blending. @return Stored RGB zenith color. */
    const Vec3& GetZenithColor() const;

    /** @brief Access the sky-blend start distance. @return Mutable non-negative distance in renderer scene units. */
    SnField(SnKey("SkyBlendStartDistance"), SnConstGetter(GetSkyBlendStartDistance))
    float& EditSkyBlendStartDistance();
    /** @brief Read the sky-blend start distance. @return Stored start distance. */
    const float& GetSkyBlendStartDistance() const;

    /** @brief Access the sky-blend end distance. @return Mutable distance forced above the start distance when applied. */
    SnField(SnKey("SkyBlendEndDistance"), SnConstGetter(GetSkyBlendEndDistance))
    float& EditSkyBlendEndDistance();
    /** @brief Read the sky-blend end distance. @return Stored end distance. */
    const float& GetSkyBlendEndDistance() const;

    /** @brief Access the sky-blend strength. @return Mutable non-negative blend-strength scalar. */
    SnField(SnKey("SkyBlendStrength"), SnConstGetter(GetSkyBlendStrength))
    float& EditSkyBlendStrength();
    /** @brief Read the sky-blend strength. @return Stored blend-strength scalar. */
    const float& GetSkyBlendStrength() const;

    /** @brief Access the tau dithering amplitude. @return Mutable non-negative dither amplitude scalar. */
    SnField(SnKey("TauDitherAmplitude"), SnConstGetter(GetTauDitherAmplitude))
    float& EditTauDitherAmplitude();
    /** @brief Read the tau dithering amplitude. @return Stored dither amplitude scalar. */
    const float& GetTauDitherAmplitude() const;

    /** @brief Access the dominant sun direction. @return Mutable world-space direction vector; normalized before upload. */
    SnField(SnKey("SunDirection"), SnConstGetter(GetSunDirection))
    Vec3& EditSunDirection();
    /** @brief Read the dominant sun direction. @return Stored world-space direction vector. */
    const Vec3& GetSunDirection() const;

    /** @brief Access the anisotropy factor for sun inscattering. @return Mutable phase-function `g` term. */
    SnField(SnKey("SunAnisotropyG"), SnConstGetter(GetSunAnisotropyG))
    float& EditSunAnisotropyG();
    /** @brief Read the anisotropy factor for sun inscattering. @return Stored phase-function `g` term. */
    const float& GetSunAnisotropyG() const;

    /** @brief Access the sun color used for inscattering. @return Mutable RGB color multiplier. */
    SnField(SnKey("SunColor"), SnConstGetter(GetSunColor))
    Vec3& EditSunColor();
    /** @brief Read the sun color used for inscattering. @return Stored RGB color multiplier. */
    const Vec3& GetSunColor() const;

    /** @brief Access the sun-inscattering intensity. @return Mutable non-negative intensity scalar. */
    SnField(SnKey("SunInscatterIntensity"), SnConstGetter(GetSunInscatterIntensity))
    float& EditSunInscatterIntensity();
    /** @brief Read the sun-inscattering intensity. @return Stored intensity scalar. */
    const float& GetSunInscatterIntensity() const;

    /**
     * @brief Mark the node dirty and attempt an initial fog upload.
     * @remarks Safe before renderer readiness; missing passes simply cause future retries.
     */
    void OnCreate();
    void OnDestroy();
    /** @brief Retry pass application when needed. @param DeltaSeconds Variable-step frame delta in seconds. Currently unused. */
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /** @brief Editor-only retry hook. @param DeltaSeconds Variable-step editor frame delta in seconds. Currently unused. */
    void EditorTick(float DeltaSeconds);
    /** @brief Mark the node dirty after reflected editor edits. @param Name Name of the changed property. Currently unused. */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    void ApplyIfNeeded();
    bool ApplyToPass();
    void InvalidatePassCache();

    std::int64_t m_viewportID = -1;

    float m_density = 0.004f;
    float m_heightFalloff = 0.008f;
    bool m_useAbsoluteHeight = true;
    double m_heightOffsetAbsoluteY = 0.0;
    bool m_useActiveCameraYAsRebaseOrigin = true;
    double m_rebaseOriginAbsoluteY = 0.0;
    float m_heightOffsetRebased = 0.0f;
    float m_startDistance = 10.0f;

    Vec3 m_fogColor{static_cast<Scalar>(0.64), static_cast<Scalar>(0.70), static_cast<Scalar>(0.76)};
    Vec3 m_horizonColor{static_cast<Scalar>(0.69), static_cast<Scalar>(0.72), static_cast<Scalar>(0.75)};
    Vec3 m_zenithColor{static_cast<Scalar>(0.47), static_cast<Scalar>(0.57), static_cast<Scalar>(0.69)};

    float m_skyBlendStartDistance = 100.0f;
    float m_skyBlendEndDistance = 1200.0f;
    float m_skyBlendStrength = 1.0f;
    float m_tauDitherAmplitude = 0.005f;

    Vec3 m_sunDirection{static_cast<Scalar>(0.0), static_cast<Scalar>(-1.0), static_cast<Scalar>(0.0)};
    float m_sunAnisotropyG = 0.7f;
    Vec3 m_sunColor{static_cast<Scalar>(1.0), static_cast<Scalar>(0.95), static_cast<Scalar>(0.85)};
    float m_sunInscatterIntensity = 0.05f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
