#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class BloomPass;
}

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-driven node that configures bloom passes for one or more render viewports.
 *
 * `BloomParamsNode` stores the renderer-facing bloom settings that authors want applied to the
 * selected viewport or viewports. The node itself is not a renderer resource. It is a world node
 * that retries until the renderer exposes a bloom pass for the targeted viewport, then uploads the
 * current parameters.
 *
 * Core semantics:
 * - A negative viewport id targets all currently known render viewports.
 * - A non-negative viewport id targets one renderer viewport with the same numeric id.
 * - The node notices pass-graph rebuilds and reapplies automatically after the bloom pass is recreated.
 * - Numeric values are sanitized before upload; for example, negative values are clamped away and
 *   mip counts are forced to at least one.
 *
 * Ownership and lifetime:
 * - The node owns only its serialized bloom settings.
 * - Bloom passes are owned by `RendererSystem` and may come and go with viewport lifetime.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning Editing fields through `Edit*()` mutates stored data immediately, but the actual renderer
 * upload is lazy and depends on the normal apply/retry hooks.
 *
 * @see RendererSystem
 * @see WorldRenderSettings
 */
class SNAPI_GAMEFRAMEWORK_API BloomParamsNode : public BaseNode, public NodeCRTP<BloomParamsNode>
{
public:
    /** @brief Stable reflected type name used for serialization and asset lookup. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::BloomParamsNode";

    /** @brief Construct an unnamed bloom settings node with default renderer tuning values. */
    BloomParamsNode();
    /**
     * @brief Construct a named bloom settings node.
     * @param Name Debug/editor-facing node name stored by the base node.
     */
    explicit BloomParamsNode(std::string Name);

    /** @brief Access the target viewport selector. @return Mutable viewport id; negative means all current viewports. */
    std::int64_t& EditViewportID();
    /** @brief Read the target viewport selector. @return Stored viewport id; negative means all current viewports. */
    const std::int64_t& GetViewportID() const;

    /** @brief Access the HDR brightness threshold. @return Mutable non-negative threshold value in bloom input color units. */
    float& EditThreshold();
    /** @brief Read the HDR brightness threshold. @return Stored threshold value. */
    const float& GetThreshold() const;

    /** @brief Access the soft-knee width around the bloom threshold. @return Mutable non-negative knee value. */
    float& EditKnee();
    /** @brief Read the soft-knee width around the bloom threshold. @return Stored knee value. */
    const float& GetKnee() const;

    /** @brief Access the bloom intensity multiplier. @return Mutable non-negative intensity scalar. */
    float& EditIntensity();
    /** @brief Read the bloom intensity multiplier. @return Stored intensity scalar. */
    const float& GetIntensity() const;

    /** @brief Access the bloom energy spread across mip levels. @return Mutable non-negative scatter value. */
    float& EditScatter();
    /** @brief Read the bloom energy spread across mip levels. @return Stored scatter value. */
    const float& GetScatter() const;

    /** @brief Access the pre-bloom brightness clamp. @return Mutable non-negative clamp value in bloom input color units. */
    float& EditClamp();
    /** @brief Read the pre-bloom brightness clamp. @return Stored clamp value. */
    const float& GetClamp() const;

    /** @brief Access the number of bloom mip levels. @return Mutable mip count; values below 1 are clamped to 1. */
    std::uint32_t& EditMipCount();
    /** @brief Read the number of bloom mip levels. @return Stored mip count. */
    const std::uint32_t& GetMipCount() const;

    /**
     * @brief Mark the node dirty and attempt to apply the current bloom settings.
     * @remarks Safe before viewport readiness; the node will retry until a bloom pass exists.
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

    float m_threshold = 1.1f;
    float m_knee = 0.5f;
    float m_intensity = 0.8f;
    float m_scatter = 0.6f;
    float m_clamp = 10.0f;
    std::uint32_t m_mipCount = 5;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
