#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class SSAOPass;
}

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-driven node that configures SSAO render passes for one or more viewports.
 *
 * `SSAOParamsNode` stores screen-space ambient occlusion tuning values in the world graph and
 * pushes them into renderer-owned SSAO passes when those passes are available. The node does
 * not create viewports, swapchains, or pass graphs. Instead, it retries application until the
 * selected render viewport already exposes an SSAO pass.
 *
 * Viewport selection semantics:
 * - A negative viewport id targets every render viewport currently known to the renderer.
 * - A non-negative viewport id targets exactly one renderer viewport with the same numeric id.
 * - New or rebuilt viewports are picked up automatically because the node tracks the renderer's
 *   pass-graph revision and retries after topology changes.
 *
 * Core semantics:
 * - Missing renderer state is treated as "not ready yet", not as a hard failure.
 * - Uploaded values are sanitized before they reach the renderer.
 * - The node never owns the underlying SSAO pass and does not keep a stable pass pointer.
 *
 * Ownership and lifetime:
 * - The node owns only its serialized parameter state.
 * - Matching SSAO passes are owned by `RendererSystem` / the graphics backend.
 * - Applied state remains in the renderer until another system overwrites the same pass settings
 *   or the pass graph is recreated.
 *
 * Threading model:
 * - Main-thread only.
 *
 * Performance notes:
 * - Tick cost is low after successful application because the node exits early while the cached
 *   viewport selection and pass-graph revision remain unchanged.
 * - Applying to "all viewports" scales linearly with the number of active render viewports.
 *
 * @warning Mutable `Edit*()` accessors update stored configuration immediately, but renderer
 * application is lazy. Settings are pushed during `OnCreate()`, `Tick()`, editor property-change
 * notifications, or later retry points when the target pass graph becomes available.
 *
 * @see RendererSystem
 * @see WorldRenderSettings
 */
class SNAPI_GAMEFRAMEWORK_API SSAOParamsNode : public BaseNode
{
public:
    /** @brief Stable reflected type name used for serialization and asset lookup. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SSAOParamsNode";

    /** @brief Construct an unnamed SSAO settings node with default renderer tuning values. */
    SSAOParamsNode();
    /**
     * @brief Construct a named SSAO settings node.
     * @param Name Debug/editor-facing node name stored by the base node.
     */
    explicit SSAOParamsNode(std::string Name);

    /**
     * @brief Access the target viewport selector.
     * @return Mutable viewport id. Negative values mean "apply to all current viewports".
     */
    std::int64_t& EditViewportID();
    /**
     * @brief Read the target viewport selector.
     * @return Stored viewport id. Negative values mean "apply to all current viewports".
     */
    const std::int64_t& GetViewportID() const;

    /**
     * @brief Access the SSAO sample radius.
     * @return Mutable radius in renderer scene units. Negative values are clamped to zero when applied.
     */
    float& EditRadius();
    /**
     * @brief Read the SSAO sample radius.
     * @return Stored radius in renderer scene units.
     */
    const float& GetRadius() const;

    /**
     * @brief Access the depth bias used to suppress self-occlusion.
     * @return Mutable non-negative bias value.
     */
    float& EditBias();
    /**
     * @brief Read the depth bias used to suppress self-occlusion.
     * @return Stored bias value.
     */
    const float& GetBias() const;

    /**
     * @brief Access the occlusion intensity multiplier.
     * @return Mutable non-negative intensity scalar.
     */
    float& EditIntensity();
    /**
     * @brief Read the occlusion intensity multiplier.
     * @return Stored intensity scalar.
     */
    const float& GetIntensity() const;

    /**
     * @brief Access the maximum occlusion distance.
     * @return Mutable non-negative distance in renderer scene units.
     */
    float& EditMaxDistance();
    /**
     * @brief Read the maximum occlusion distance.
     * @return Stored maximum distance in renderer scene units.
     */
    const float& GetMaxDistance() const;

    /**
     * @brief Access the number of azimuth slices used by the SSAO kernel.
     * @return Mutable slice count. Values below 1 are clamped to 1 when applied.
     */
    std::uint32_t& EditSliceCount();
    /**
     * @brief Read the number of azimuth slices used by the SSAO kernel.
     * @return Stored slice count.
     */
    const std::uint32_t& GetSliceCount() const;

    /**
     * @brief Access the number of samples traced per slice.
     * @return Mutable step count. Values below 1 are clamped to 1 when applied.
     */
    std::uint32_t& EditStepsPerSlice();
    /**
     * @brief Read the number of samples traced per slice.
     * @return Stored step count.
     */
    const std::uint32_t& GetStepsPerSlice() const;

    /**
     * @brief Access the normalized falloff-start position.
     * @return Mutable non-negative falloff start. The end value is forced above this value at apply time.
     */
    float& EditFalloffStart();
    /**
     * @brief Read the normalized falloff-start position.
     * @return Stored falloff-start value.
     */
    const float& GetFalloffStart() const;

    /**
     * @brief Access the normalized falloff-end position.
     * @return Mutable falloff end. The renderer receives at least `FalloffStart + 1e-3`.
     */
    float& EditFalloffEnd();
    /**
     * @brief Read the normalized falloff-end position.
     * @return Stored falloff-end value.
     */
    const float& GetFalloffEnd() const;

    /**
     * @brief Access the maximum radius in pixels used by the pass.
     * @return Mutable non-negative screen-space radius in pixels.
     */
    float& EditMaxPixelRadius();
    /**
     * @brief Read the maximum radius in pixels used by the pass.
     * @return Stored screen-space radius in pixels.
     */
    const float& GetMaxPixelRadius() const;

    /**
     * @brief Access the thickness tolerance used by the SSAO resolver.
     * @return Mutable non-negative thickness value in renderer-defined depth units.
     */
    float& EditThickness();
    /**
     * @brief Read the thickness tolerance used by the SSAO resolver.
     * @return Stored thickness value.
     */
    const float& GetThickness() const;

    /**
     * @brief Access the denoise blur beta.
     * @return Mutable non-negative filter-shaping parameter.
     */
    float& EditDenoiseBlurBeta();
    /**
     * @brief Read the denoise blur beta.
     * @return Stored filter-shaping parameter.
     */
    const float& GetDenoiseBlurBeta() const;

    /**
     * @brief Access the temporal accumulation blend factor.
     * @return Mutable non-negative blend scalar. Lower values favor longer temporal history.
     */
    float& EditTemporalBlendFactor();
    /**
     * @brief Read the temporal accumulation blend factor.
     * @return Stored blend scalar.
     */
    const float& GetTemporalBlendFactor() const;

    /**
     * @brief Access the temporal disocclusion rejection threshold.
     * @return Mutable non-negative threshold value.
     */
    float& EditDisocclusionThreshold();
    /**
     * @brief Read the temporal disocclusion rejection threshold.
     * @return Stored threshold value.
     */
    const float& GetDisocclusionThreshold() const;

    /**
     * @brief Access the motion-vector weighting used by temporal accumulation.
     * @return Mutable non-negative velocity weight.
     */
    float& EditVelocityWeight();
    /**
     * @brief Read the motion-vector weighting used by temporal accumulation.
     * @return Stored velocity weight.
     */
    const float& GetVelocityWeight() const;

    /**
     * @brief Mark the node dirty and attempt an immediate initial apply.
     *
     * Safe to call before the renderer or target viewport is ready. If no matching SSAO pass
     * exists yet, the node simply remains dirty and retries later.
     */
    void OnCreate();
    /**
     * @brief Retry pass application when needed.
     * @param DeltaSeconds Variable-step frame delta in seconds. Currently unused.
     */
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /**
     * @brief Editor-only retry hook.
     * @param DeltaSeconds Variable-step editor frame delta in seconds. Currently unused.
     */
    void EditorTick(float DeltaSeconds);
    /**
     * @brief Mark the node dirty after an editor property edit and retry immediately.
     * @param Name Name of the changed reflected property. Currently unused.
     */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    void ApplyIfNeeded();
    bool ApplyToPass();
    void InvalidatePassCache();

    std::int64_t m_viewportID = -1;

    float m_radius = 3.0f;
    float m_bias = 0.025f;
    float m_intensity = 1.0f;
    float m_maxDistance = 1000000.0f;
    std::uint32_t m_sliceCount = 3;
    std::uint32_t m_stepsPerSlice = 6;
    float m_falloffStart = 0.9f;
    float m_falloffEnd = 1.0f;
    float m_maxPixelRadius = 128.0f;
    float m_thickness = 0.5f;
    float m_denoiseBlurBeta = 1.5f;
    float m_temporalBlendFactor = 0.01f;
    float m_disocclusionThreshold = 0.02f;
    float m_velocityWeight = 10.0f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
