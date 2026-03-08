#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class ToneMapPass;
}

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-driven node that configures tone-mapping passes for one or more viewports.
 *
 * `ToneMapParamsNode` stores output-transform parameters for the renderer's tone-map pass and
 * uploads them when the targeted viewport already contains such a pass. The node is passive with
 * respect to renderer setup: it does not create the pass graph and does not guarantee that a tone
 * map pass exists. Instead, it retries until the renderer is ready.
 *
 * Core semantics:
 * - Negative viewport ids target all current render viewports.
 * - Non-negative ids target one viewport by renderer-assigned id.
 * - Feature flags are combined into the renderer's tone-map feature mask on upload.
 * - Values are sanitized before upload; for example, gamma is forced positive and several AgX
 *   controls are clamped into renderer-supported ranges.
 *
 * Ownership and lifetime:
 * - The node owns only serialized tone-map parameters.
 * - Tone-map passes are renderer-owned resources borrowed transiently during application.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning Editing values through `Edit*()` changes stored configuration immediately, but renderer
 * application is lazy and occurs only on the node's normal apply/retry hooks.
 *
 * @see RendererSystem
 * @see WorldRenderSettings
 */
class SNAPI_GAMEFRAMEWORK_API ToneMapParamsNode : public BaseNode
{
public:
    /** @brief Stable reflected type name used for serialization and asset lookup. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ToneMapParamsNode";

    /** @brief Construct an unnamed tone-map settings node with default output-transform values. */
    ToneMapParamsNode();
    /**
     * @brief Construct a named tone-map settings node.
     * @param Name Debug/editor-facing node name stored by the base node.
     */
    explicit ToneMapParamsNode(std::string Name);

    /** @brief Access the target viewport selector. @return Mutable viewport id; negative means all current viewports. */
    std::int64_t& EditViewportID();
    /** @brief Read the target viewport selector. @return Stored viewport id; negative means all current viewports. */
    const std::int64_t& GetViewportID() const;

    /** @brief Access the global exposure multiplier. @return Mutable non-negative exposure scalar. */
    float& EditExposure();
    /** @brief Read the global exposure multiplier. @return Stored exposure scalar. */
    const float& GetExposure() const;

    /** @brief Access the display gamma. @return Mutable gamma value; forced to a small positive minimum when applied. */
    float& EditGamma();
    /** @brief Read the display gamma. @return Stored gamma value. */
    const float& GetGamma() const;

    /** @brief Access the dithering strength. @return Mutable non-negative dither amplitude scalar. */
    float& EditDitherStrength();
    /** @brief Read the dithering strength. @return Stored dither amplitude scalar. */
    const float& GetDitherStrength() const;

    /** @brief Access the AgX exposure bias. @return Mutable exposure bias in stops. */
    float& EditAgXExposureBiasStops();
    /** @brief Read the AgX exposure bias. @return Stored exposure bias in stops. */
    const float& GetAgXExposureBiasStops() const;

    /** @brief Access AgX saturation. @return Mutable non-negative saturation multiplier. */
    float& EditAgXSaturation();
    /** @brief Read AgX saturation. @return Stored saturation multiplier. */
    const float& GetAgXSaturation() const;

    /** @brief Access AgX contrast. @return Mutable non-negative contrast multiplier. */
    float& EditAgXContrast();
    /** @brief Read AgX contrast. @return Stored contrast multiplier. */
    const float& GetAgXContrast() const;

    /** @brief Access the AgX pivot. @return Mutable pivot value clamped to [0, 1] before upload. */
    float& EditAgXPivot();
    /** @brief Read the AgX pivot. @return Stored pivot value. */
    const float& GetAgXPivot() const;

    /** @brief Access the AgX gamut threshold. @return Mutable threshold clamped to [0, 2] before upload. */
    float& EditAgXGamutThreshold();
    /** @brief Read the AgX gamut threshold. @return Stored threshold value. */
    const float& GetAgXGamutThreshold() const;

    /** @brief Access the AgX gamut knee. @return Mutable non-negative knee value. */
    float& EditAgXGamutKnee();
    /** @brief Read the AgX gamut knee. @return Stored knee value. */
    const float& GetAgXGamutKnee() const;

    /** @brief Access ACES saturation. @return Mutable non-negative saturation multiplier. */
    float& EditAcesSaturation();
    /** @brief Read ACES saturation. @return Stored saturation multiplier. */
    const float& GetAcesSaturation() const;

    /** @brief Access the ACES white point. @return Mutable non-negative white-point value in tone-map input units. */
    float& EditAcesWhitePoint();
    /** @brief Read the ACES white point. @return Stored white-point value. */
    const float& GetAcesWhitePoint() const;

    /** @brief Access the ACES feature toggle. @return Mutable flag enabling the ACES tone-map variant. */
    bool& EditEnableACES();
    /** @brief Read the ACES feature toggle. @return Stored ACES-enable flag. */
    const bool& GetEnableACES() const;

    /** @brief Access the AgX feature toggle. @return Mutable flag enabling the AgX tone-map variant. */
    bool& EditEnableAgX();
    /** @brief Read the AgX feature toggle. @return Stored AgX-enable flag. */
    const bool& GetEnableAgX() const;

    /** @brief Access the compare/debug feature toggle. @return Mutable flag enabling renderer-side compare mode. */
    bool& EditEnableCompare();
    /** @brief Read the compare/debug feature toggle. @return Stored compare-mode flag. */
    const bool& GetEnableCompare() const;

    /**
     * @brief Mark the node dirty and attempt an initial tone-map upload.
     * @remarks Safe before viewport readiness; missing passes simply cause future retries.
     */
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

    float m_exposure = 1.0f;
    float m_gamma = 2.2f;
    float m_ditherStrength = 1.0f;
    float m_agXExposureBiasStops = -0.5f;
    float m_agXSaturation = 1.05f;
    float m_agXContrast = 1.03f;
    float m_agXPivot = 0.5f;
    float m_agXGamutThreshold = 0.9f;
    float m_agXGamutKnee = 0.5f;
    float m_acesSaturation = 1.05f;
    float m_acesWhitePoint = 11.2f;
    bool m_enableACES = true;
    bool m_enableAgX = false;
    bool m_enableCompare = false;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
