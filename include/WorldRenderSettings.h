#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <string>
#include <string_view>

#include "AssetRef.h"
#include "AtmosphereCompositeParamsNode.h"
#include "AtmosphereParamsNode.h"
#include "BaseNode.h"
#include "BloomParamsNode.h"
#include "Export.h"
#include "HeightFogParamsNode.h"
#include "SSAOParamsNode.h"
#include "SSGIParamsNode.h"
#include "SSRParamsNode.h"
#include "TAAParamsNode.h"
#include "ToneMapParamsNode.h"
#include "ReflectionAnnotations.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-driven node that materializes referenced render-parameter nodes under itself.
 *
 * `WorldRenderSettings` is a convenience container for world-scoped post-processing and
 * atmosphere configuration. Instead of requiring authors to place every parameter node
 * manually, this node stores asset references and ensures matching parameter nodes exist
 * as children when the settings node is created or edited.
 *
 * Core semantics:
 * - each asset reference corresponds to at most one spawned child node owned by the world
 * - null asset references destroy any previously spawned child for that slot
 * - existing spawned nodes are reused when still alive and of the expected type
 * - reused nodes have `OnCreate` requested again so they can reapply their settings safely
 *
 * Ownership and lifetime:
 * - The asset references are data only; the referenced assets are not owned by this node.
 * - Spawned child nodes are owned by the world graph under this node.
 * - Spawned handles are internal implementation state and may be recreated as references change.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning
 * Applying referenced settings depends on world/renderer readiness. This node should be
 * created only after the relevant renderer/viewports exist, or its `OnCreate` work must
 * be deferred by the caller/bootstrap sequence.
 *
 * @see RendererSystem
 * @see SSAOParamsNode
 * @see SSGIParamsNode
 * @see SSRParamsNode
 * @see TAAParamsNode
 * @see BloomParamsNode
 */
SnType()
class SNAPI_GAMEFRAMEWORK_API WorldRenderSettings : public BaseNode, public NodeCRTP<WorldRenderSettings>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::WorldRenderSettings";

    WorldRenderSettings();
    explicit WorldRenderSettings(std::string Name);

    /** @brief Access the referenced SSAO parameter asset. @return Mutable asset reference. */
    SnField(SnKey("SSAOParams"), SnConstGetter(GetSSAOParams))
    TAssetRef<SSAOParamsNode>& EditSSAOParams();
    /** @brief Access the referenced SSAO parameter asset. @return Const asset reference. */
    const TAssetRef<SSAOParamsNode>& GetSSAOParams() const;

    /** @brief Access the referenced SSGI parameter asset. @return Mutable asset reference. */
    SnField(SnKey("SSGIParams"), SnConstGetter(GetSSGIParams))
    TAssetRef<SSGIParamsNode>& EditSSGIParams();
    /** @brief Access the referenced SSGI parameter asset. @return Const asset reference. */
    const TAssetRef<SSGIParamsNode>& GetSSGIParams() const;

    /** @brief Access the referenced SSR parameter asset. @return Mutable asset reference. */
    SnField(SnKey("SSRParams"), SnConstGetter(GetSSRParams))
    TAssetRef<SSRParamsNode>& EditSSRParams();
    /** @brief Access the referenced SSR parameter asset. @return Const asset reference. */
    const TAssetRef<SSRParamsNode>& GetSSRParams() const;

    /** @brief Access the referenced TAA parameter asset. @return Mutable asset reference. */
    SnField(SnKey("TAAParams"), SnConstGetter(GetTAAParams))
    TAssetRef<TAAParamsNode>& EditTAAParams();
    /** @brief Access the referenced TAA parameter asset. @return Const asset reference. */
    const TAssetRef<TAAParamsNode>& GetTAAParams() const;

    /** @brief Access the referenced bloom parameter asset. @return Mutable asset reference. */
    SnField(SnKey("BloomParams"), SnConstGetter(GetBloomParams))
    TAssetRef<BloomParamsNode>& EditBloomParams();
    /** @brief Access the referenced bloom parameter asset. @return Const asset reference. */
    const TAssetRef<BloomParamsNode>& GetBloomParams() const;

    /** @brief Access the referenced atmosphere parameter asset. @return Mutable asset reference. */
    SnField(SnKey("AtmosphereParams"), SnConstGetter(GetAtmosphereParams))
    TAssetRef<AtmosphereParamsNode>& EditAtmosphereParams();
    /** @brief Access the referenced atmosphere parameter asset. @return Const asset reference. */
    const TAssetRef<AtmosphereParamsNode>& GetAtmosphereParams() const;

    /** @brief Access the referenced atmosphere-composite parameter asset. @return Mutable asset reference. */
    SnField(SnKey("AtmosphereCompositeParams"), SnConstGetter(GetAtmosphereCompositeParams))
    TAssetRef<AtmosphereCompositeParamsNode>& EditAtmosphereCompositeParams();
    /** @brief Access the referenced atmosphere-composite parameter asset. @return Const asset reference. */
    const TAssetRef<AtmosphereCompositeParamsNode>& GetAtmosphereCompositeParams() const;

    /** @brief Access the referenced height-fog parameter asset. @return Mutable asset reference. */
    SnField(SnKey("HeightFogParams"), SnConstGetter(GetHeightFogParams))
    TAssetRef<HeightFogParamsNode>& EditHeightFogParams();
    /** @brief Access the referenced height-fog parameter asset. @return Const asset reference. */
    const TAssetRef<HeightFogParamsNode>& GetHeightFogParams() const;

    /** @brief Access the referenced tone-map parameter asset. @return Mutable asset reference. */
    SnField(SnKey("ToneMapParams"), SnConstGetter(GetToneMapParams))
    TAssetRef<ToneMapParamsNode>& EditToneMapParams();
    /** @brief Access the referenced tone-map parameter asset. @return Const asset reference. */
    const TAssetRef<ToneMapParamsNode>& GetToneMapParams() const;

    /**
     * @brief Materialize or refresh referenced settings nodes.
     * @remarks Called by normal node-creation flow and safe to invoke repeatedly.
     */
    void OnCreate();
    /**
     * @brief Per-frame tick hook.
     * @param DeltaSeconds Frame delta time in seconds.
     * @remarks Currently a no-op placeholder kept for node contract symmetry.
     */
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /**
     * @brief Editor-only per-frame tick hook.
     * @param DeltaSeconds Frame delta time in seconds.
     * @remarks Currently a no-op placeholder.
     */
    void EditorTick(float DeltaSeconds);
    /**
     * @brief Editor-only callback invoked after a reflected property changes.
     * @param Name Name of the changed property.
     * @remarks Reapplies referenced settings immediately.
     */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    void ApplyReferencedSettings();

    TAssetRef<SSAOParamsNode> m_ssaoParams{};
    TAssetRef<SSGIParamsNode> m_ssgiParams{};
    TAssetRef<SSRParamsNode> m_ssrParams{};
    TAssetRef<TAAParamsNode> m_taaParams{};
    TAssetRef<BloomParamsNode> m_bloomParams{};
    TAssetRef<AtmosphereParamsNode> m_atmosphereParams{};
    TAssetRef<AtmosphereCompositeParamsNode> m_atmosphereCompositeParams{};
    TAssetRef<HeightFogParamsNode> m_heightFogParams{};
    TAssetRef<ToneMapParamsNode> m_toneMapParams{};

    NodeHandle m_spawnedSSAO{};
    NodeHandle m_spawnedSSGI{};
    NodeHandle m_spawnedSSR{};
    NodeHandle m_spawnedTAA{};
    NodeHandle m_spawnedBloom{};
    NodeHandle m_spawnedAtmosphere{};
    NodeHandle m_spawnedAtmosphereComposite{};
    NodeHandle m_spawnedHeightFog{};
    NodeHandle m_spawnedToneMap{};
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
