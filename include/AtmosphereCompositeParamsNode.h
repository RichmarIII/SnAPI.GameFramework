#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class AtmosphereCompositePass;
}

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-driven node that tunes atmosphere-composite passes for one or more viewports.
 *
 * `AtmosphereCompositeParamsNode` configures post-lighting composite passes that blend atmosphere
 * results into the final scene. Unlike the primary atmosphere node, this class may apply to multiple
 * composite passes per viewport because a pass graph can contain several composite stages.
 *
 * Core semantics:
 * - Negative viewport ids target all active render viewports.
 * - Non-negative ids target one viewport by renderer-assigned id.
 * - Every composite pass of type `AtmosphereCompositePass` found in the selected viewport receives
 *   the same stored settings.
 * - Blend weights are clamped to the unit interval before upload.
 *
 * Ownership and lifetime:
 * - The node owns only its serialized blend parameters.
 * - Composite passes remain renderer-owned and can be recreated when pass graphs change.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see RendererSystem
 * @see AtmosphereParamsNode
 * @see WorldRenderSettings
 */
class SNAPI_GAMEFRAMEWORK_API AtmosphereCompositeParamsNode : public BaseNode
{
public:
    /** @brief Stable reflected type name used for serialization and asset lookup. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AtmosphereCompositeParamsNode";

    /** @brief Construct an unnamed atmosphere-composite settings node with default blend values. */
    AtmosphereCompositeParamsNode();
    /**
     * @brief Construct a named atmosphere-composite settings node.
     * @param Name Debug/editor-facing node name stored by the base node.
     */
    explicit AtmosphereCompositeParamsNode(std::string Name);

    /** @brief Access the target viewport selector. @return Mutable viewport id; negative means all current viewports. */
    std::int64_t& EditViewportID();
    /** @brief Read the target viewport selector. @return Stored viewport id; negative means all current viewports. */
    const std::int64_t& GetViewportID() const;

    /** @brief Access the composite depth threshold. @return Mutable threshold value clamped to [0, 1] before upload. */
    float& EditDepthThreshold();
    /** @brief Read the composite depth threshold. @return Stored threshold value. */
    const float& GetDepthThreshold() const;

    /** @brief Access the atmosphere blend factor used when scene geometry is present. @return Mutable blend weight clamped to [0, 1]. */
    float& EditBlendWhenGeometry();
    /** @brief Read the geometry blend factor. @return Stored blend weight. */
    const float& GetBlendWhenGeometry() const;

    /** @brief Access the atmosphere blend factor used when sky/background is visible. @return Mutable blend weight clamped to [0, 1]. */
    float& EditBlendWhenSky();
    /** @brief Read the sky blend factor. @return Stored blend weight. */
    const float& GetBlendWhenSky() const;

    /**
     * @brief Mark the node dirty and attempt to upload current composite settings.
     * @remarks Missing renderer state is treated as deferred readiness.
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

    float m_depthThreshold = 0.0f;
    float m_blendWhenGeometry = 0.0f;
    float m_blendWhenSky = 1.0f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
