#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class SSGIPass;
}

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-driven node that configures screen-space global illumination passes for one or more viewports.
 *
 * `SSGIParamsNode` stores SSGI trace, denoise, and temporal accumulation parameters inside the
 * world graph and pushes them into renderer-owned `SSGIPass` instances when those passes are
 * available. The node is passive: it does not create renderer viewports or register pass graphs.
 * Its responsibility is to retry application until matching `SSGIPass` objects already exist.
 *
 * Viewport selection semantics:
 * - A negative viewport id applies to every current renderer viewport.
 * - A non-negative viewport id targets exactly one renderer viewport with the same numeric id.
 * - Pass-graph rebuilds are detected through the renderer revision counter, so the node reapplies
 *   settings after viewport recreation or preset changes.
 *
 * Ownership and lifetime:
 * - The node owns only its stored parameter values.
 * - Matching SSGI passes are owned by the renderer and may disappear when viewport graphs change.
 * - No pass pointer is retained across frames.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning Mutable `Edit*()` accessors update stored configuration immediately, but renderer state
 * changes happen lazily on the node's normal apply/retry hooks.
 *
 * @see RendererSystem
 * @see WorldRenderSettings
 */
class SNAPI_GAMEFRAMEWORK_API SSGIParamsNode : public BaseNode
{
public:
    /** @brief Stable reflected type name used for serialization and asset lookup. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SSGIParamsNode";

    /** @brief Construct an unnamed SSGI settings node with default renderer tuning values. */
    SSGIParamsNode();
    /**
     * @brief Construct a named SSGI settings node.
     * @param Name Debug/editor-facing node name stored by the base node.
     */
    explicit SSGIParamsNode(std::string Name);

    /** @brief Access the target viewport selector. @return Mutable viewport id; negative means all current viewports. */
    std::int64_t& EditViewportID();
    /** @brief Read the target viewport selector. @return Stored viewport id; negative means all current viewports. */
    const std::int64_t& GetViewportID() const;

    /** @brief Access the global diffuse GI intensity. @return Mutable non-negative scalar. */
    float& EditIntensity();
    /** @brief Read the global diffuse GI intensity. @return Stored intensity scalar. */
    const float& GetIntensity() const;

    /** @brief Access the maximum diffuse trace distance. @return Mutable non-negative distance in renderer scene units. */
    float& EditMaxDistance();
    /** @brief Read the maximum diffuse trace distance. @return Stored distance in renderer scene units. */
    const float& GetMaxDistance() const;

    /** @brief Access the intersection acceptance thickness. @return Mutable non-negative thickness value. */
    float& EditThickness();
    /** @brief Read the intersection acceptance thickness. @return Stored thickness value. */
    const float& GetThickness() const;

    /** @brief Access the normal-direction origin bias. @return Mutable non-negative bias value. */
    float& EditSurfaceBias();
    /** @brief Read the normal-direction origin bias. @return Stored bias value. */
    const float& GetSurfaceBias() const;

    /** @brief Access the maximum screen-space march steps. @return Mutable step count; values below 1 are clamped to 1 on upload. */
    std::uint32_t& EditMaxSteps();
    /** @brief Read the maximum screen-space march steps. @return Stored step count. */
    const std::uint32_t& GetMaxSteps() const;

    /** @brief Access the cosine-hemisphere ray count per pixel. @return Mutable ray count; values below 1 are clamped to 1 on upload. */
    std::uint32_t& EditRayCount();
    /** @brief Read the cosine-hemisphere ray count per pixel. @return Stored ray count. */
    const std::uint32_t& GetRayCount() const;

    /** @brief Access the bilateral depth weight falloff. @return Mutable non-negative sigma scalar. */
    float& EditDepthSigma();
    /** @brief Read the bilateral depth weight falloff. @return Stored sigma scalar. */
    const float& GetDepthSigma() const;

    /** @brief Access the bilateral normal weight falloff. @return Mutable non-negative sigma scalar. */
    float& EditNormalSigma();
    /** @brief Read the bilateral normal weight falloff. @return Stored sigma scalar. */
    const float& GetNormalSigma() const;

    /** @brief Access the luminance clamp applied to sampled radiance. @return Mutable non-negative clamp scalar. */
    float& EditRadianceClamp();
    /** @brief Read the luminance clamp applied to sampled radiance. @return Stored clamp scalar. */
    const float& GetRadianceClamp() const;

    /** @brief Access the maximum projected screen-space ray radius. @return Mutable radius in pixels; values below 1 are clamped on upload. */
    float& EditMaxPixelRadius();
    /** @brief Read the maximum projected screen-space ray radius. @return Stored radius in pixels. */
    const float& GetMaxPixelRadius() const;

    /** @brief Access the trace step exponent. @return Mutable exponent; values below 0.25 are clamped on upload. */
    float& EditStepExponent();
    /** @brief Read the trace step exponent. @return Stored exponent. */
    const float& GetStepExponent() const;

    /** @brief Access the temporal accumulation blend factor. @return Mutable blend scalar clamped to [0, 1] on upload. */
    float& EditTemporalBlendFactor();
    /** @brief Read the temporal accumulation blend factor. @return Stored blend scalar. */
    const float& GetTemporalBlendFactor() const;

    /** @brief Access the temporal disocclusion rejection threshold. @return Mutable non-negative threshold value. */
    float& EditDisocclusionThreshold();
    /** @brief Read the temporal disocclusion rejection threshold. @return Stored threshold value. */
    const float& GetDisocclusionThreshold() const;

    /** @brief Access the history clamp expansion strength. @return Mutable non-negative clamp scalar. */
    float& EditClampStrength();
    /** @brief Read the history clamp expansion strength. @return Stored clamp scalar. */
    const float& GetClampStrength() const;

    /** @brief Access the motion-vector weighting for temporal rejection. @return Mutable non-negative velocity weight. */
    float& EditVelocityWeight();
    /** @brief Read the motion-vector weighting for temporal rejection. @return Stored velocity weight. */
    const float& GetVelocityWeight() const;

    /** @brief Access the low-luminance temporal responsiveness boost. @return Mutable non-negative boost scalar. */
    float& EditLowLumaBoost();
    /** @brief Read the low-luminance temporal responsiveness boost. @return Stored boost scalar. */
    const float& GetLowLumaBoost() const;

    /** @brief Access the temporal debug visualization mode. @return Mutable debug selector where 0 = final result. */
    std::uint32_t& EditTemporalDebugMode();
    /** @brief Read the temporal debug visualization mode. @return Stored debug selector. */
    const std::uint32_t& GetTemporalDebugMode() const;

    /** @brief Mark the node dirty and attempt an immediate apply. */
    void OnCreate();
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

    float m_intensity = 0.85f;
    float m_maxDistance = 6.0f;
    float m_thickness = 0.2f;
    float m_surfaceBias = 0.05f;
    std::uint32_t m_maxSteps = 16;
    std::uint32_t m_rayCount = 4;
    float m_depthSigma = 64.0f;
    float m_normalSigma = 32.0f;
    float m_radianceClamp = 2.5f;
    float m_maxPixelRadius = 96.0f;
    float m_stepExponent = 1.25f;
    float m_temporalBlendFactor = 0.08f;
    float m_disocclusionThreshold = 0.02f;
    float m_clampStrength = 0.10f;
    float m_velocityWeight = 12.0f;
    float m_lowLumaBoost = 0.08f;
    std::uint32_t m_temporalDebugMode = 0;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
