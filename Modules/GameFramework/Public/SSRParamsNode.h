#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"
#include "ReflectionAnnotations.h"


namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-driven node that configures screen-space reflection feature settings for one or more viewports.
 *
 * `SSRParamsNode` exposes serialized reflection settings through the world graph and applies them
 * to renderer-owned SSR feature state when the targeted viewport is available. The node is intentionally passive: it does
 * not create render viewports or add SSR to a feature profile. Its job is to wait until the selected viewport
 * can accept SSR settings, then push sanitized values into the renderer facade.
 *
 * Viewport selection semantics:
 * - A negative viewport id means "apply to every current renderer viewport".
 * - A non-negative viewport id targets exactly one renderer viewport with the same numeric id.
 * - Feature-profile rebuilds are detected automatically through the renderer revision counter, so the
 *   node reapplies settings after viewport recreation or preset changes.
 *
 * Ownership and lifetime:
 * - The node owns only its stored parameter values.
 * - Renderer feature resources are owned by the renderer and may be recreated when viewport profiles change.
 * - No renderer-internal pointer is retained across frames.
 *
 * Threading model:
 * - Main-thread only.
 *
 * Performance notes:
 * - Successful steady-state ticks are cheap because the node caches the last viewport selection
 *   and feature-profile revision that accepted the settings.
 * - Applying to all viewports scales linearly with the number of active render viewports.
 *
 * @warning Mutable `Edit*()` accessors only change stored configuration. Renderer state updates
 * are lazy and happen on the node's normal apply/retry hooks.
 *
 * @see RendererSystem
 * @see WorldRenderSettings
 */
SnType()
class SNAPI_GAMEFRAMEWORK_API SSRParamsNode : public BaseNode, public NodeCRTP<SSRParamsNode>
{
public:
    /** @brief Stable reflected type name used for serialization and asset lookup. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SSRParamsNode";

    /** @brief Construct an unnamed SSR settings node with default renderer tuning values. */
    SSRParamsNode();
    /**
     * @brief Construct a named SSR settings node.
     * @param Name Debug/editor-facing node name stored by the base node.
     */
    explicit SSRParamsNode(std::string Name);

    /** @brief Access the target viewport selector. @return Mutable viewport id; negative means all current viewports. */
    SnField(SnKey("ViewportID"), SnConstGetter(GetViewportID))
    std::int64_t& EditViewportID();
    /** @brief Read the target viewport selector. @return Stored viewport id; negative means all current viewports. */
    const std::int64_t& GetViewportID() const;

    /** @brief Access the maximum reflection ray distance. @return Mutable distance in renderer-defined scene units. */
    SnField(SnKey("MaxDistance"), SnConstGetter(GetMaxDistance))
    float& EditMaxDistance();
    /** @brief Read the maximum reflection ray distance. @return Stored ray distance. */
    const float& GetMaxDistance() const;

    /** @brief Access the intersection thickness tolerance. @return Mutable non-negative thickness value. */
    SnField(SnKey("Thickness"), SnConstGetter(GetThickness))
    float& EditThickness();
    /** @brief Read the intersection thickness tolerance. @return Stored thickness value. */
    const float& GetThickness() const;

    /** @brief Access the roughness ceiling accepted by SSR. @return Mutable roughness limit, typically interpreted in [0, 1]. */
    SnField(SnKey("MaxRoughness"), SnConstGetter(GetMaxRoughness))
    float& EditMaxRoughness();
    /** @brief Read the roughness ceiling accepted by SSR. @return Stored roughness limit. */
    const float& GetMaxRoughness() const;

    /** @brief Access the roughness threshold used by the feature state. @return Mutable roughness threshold, clamped non-negative on upload. */
    SnField(SnKey("RoughnessThreshold"), SnConstGetter(GetRoughnessThreshold))
    float& EditRoughnessThreshold();
    /** @brief Read the roughness threshold used by the feature state. @return Stored roughness threshold. */
    const float& GetRoughnessThreshold() const;

    /** @brief Access the maximum number of forward march steps. @return Mutable step count; values below 1 are clamped to 1. */
    SnField(SnKey("MaxSteps"), SnConstGetter(GetMaxSteps))
    std::uint32_t& EditMaxSteps();
    /** @brief Read the maximum number of forward march steps. @return Stored step count. */
    const std::uint32_t& GetMaxSteps() const;

    /** @brief Access the maximum number of binary-search refinement steps. @return Mutable step count; values below 1 are clamped to 1. */
    SnField(SnKey("MaxBinarySteps"), SnConstGetter(GetMaxBinarySteps))
    std::uint32_t& EditMaxBinarySteps();
    /** @brief Read the maximum number of binary-search refinement steps. @return Stored step count. */
    const std::uint32_t& GetMaxBinarySteps() const;

    /** @brief Access the fade applied near screen borders. @return Mutable non-negative fade scalar. */
    SnField(SnKey("ScreenEdgeFade"), SnConstGetter(GetScreenEdgeFade))
    float& EditScreenEdgeFade();
    /** @brief Read the fade applied near screen borders. @return Stored fade scalar. */
    const float& GetScreenEdgeFade() const;

    /** @brief Access the global reflection fade multiplier. @return Mutable non-negative fade scalar. */
    SnField(SnKey("ReflectionFade"), SnConstGetter(GetReflectionFade))
    float& EditReflectionFade();
    /** @brief Read the global reflection fade multiplier. @return Stored fade scalar. */
    const float& GetReflectionFade() const;

    /** @brief Access the temporal accumulation blend factor. @return Mutable blend scalar clamped to [0, 1] on upload. */
    SnField(SnKey("TemporalBlendFactor"), SnConstGetter(GetTemporalBlendFactor))
    float& EditTemporalBlendFactor();
    /** @brief Read the temporal accumulation blend factor. @return Stored blend scalar. */
    const float& GetTemporalBlendFactor() const;

    /** @brief Access the history clamp expansion strength. @return Mutable non-negative clamp scalar. */
    SnField(SnKey("ClampStrength"), SnConstGetter(GetClampStrength))
    float& EditClampStrength();
    /** @brief Read the history clamp expansion strength. @return Stored clamp scalar. */
    const float& GetClampStrength() const;

    /**
     * @brief Access the motion-driven history reset strength.
     * @return Mutable scalar clamped to [0, 1] on upload.
     * @remarks `0` ignores motion for history reset. `1` fully resets SSR history on detected motion.
     */
    SnField(SnKey("MotionHistoryReset"), SnConstGetter(GetMotionHistoryReset))
    float& EditMotionHistoryReset();
    /** @brief Read the motion-driven history reset strength. @return Stored reset scalar in [0, 1]. */
    const float& GetMotionHistoryReset() const;

    /** @brief Access the temporal debug visualization mode. @return Mutable debug selector where 0 = final result. */
    SnField(SnKey("TemporalDebugMode"), SnConstGetter(GetTemporalDebugMode))
    std::uint32_t& EditTemporalDebugMode();
    /** @brief Read the temporal debug visualization mode. @return Stored debug selector. */
    const std::uint32_t& GetTemporalDebugMode() const;

    /**
     * @brief Mark the node dirty and attempt an immediate apply.
     * @remarks Missing renderer feature state is treated as deferred readiness, not an error.
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

    float m_maxDistance = 0.25f;
    float m_thickness = 0.015f;
    float m_maxRoughness = 0.8f;
    float m_roughnessThreshold = 0.2f;
    std::uint32_t m_maxSteps = 32;
    std::uint32_t m_maxBinarySteps = 8;
    float m_screenEdgeFade = 0.1f;
    float m_reflectionFade = 0.8f;
    float m_temporalBlendFactor = 0.10f;
    float m_clampStrength = 0.10f;
    float m_motionHistoryReset = 0.25f;
    std::uint32_t m_temporalDebugMode = 0;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedFeatureRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
