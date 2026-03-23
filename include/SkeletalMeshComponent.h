#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>
#include <string>
#include <string_view>
#include <cstdint>

#include "AssetRef.h"
#include "BaseComponent.h"
#include "BuiltinTypes.h"
#include "ReflectionAnnotations.h"

namespace SnAPI::Graphics
{
class MeshRenderObject;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{

class RendererSystem;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Component that owns a skeletal mesh render object and drives rigid-animation playback.
 *
 * `SkeletalMeshComponent` is the animated counterpart to `StaticMeshComponent`. It owns one
 * renderer mesh render-object, registers it into the appropriate renderer passes, synchronizes
 * its transform from the owning node, and optionally auto-plays rigid animations after load.
 *
 * Source semantics:
 * - `Settings::MeshAsset` is the reliable runtime load path and the preferred way to use this component
 * - `Settings::MeshPath` is retained for compatibility and participates in change detection, but the
 *   current implementation is asset-centric and does not guarantee direct source-path loading by itself
 *
 * Animation semantics:
 * - `AutoPlayAnimations` is applied lazily after a mesh exists
 * - changing animation name or loop mode clears the applied-auto-play state and reapplies on the next update
 * - `StopAnimations()` stops active rigid animations and clears the auto-play-applied flag
 *
 * Ownership and lifetime:
 * - The component owns the `MeshRenderObject` shared pointer it creates.
 * - The renderer borrows that object while it is registered in passes.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see RendererSystem
 * @see TransformComponent
 */
SnType()
class SkeletalMeshComponent : public BaseComponent, public ComponentCRTP<SkeletalMeshComponent>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletalMeshComponent";

    /**
     * @brief Runtime mesh/render/animation settings.
     *
     * Semantics:
     * - `RegisterWithRenderer` controls pass registration, not object creation
     * - `Visible` and `CastShadows` affect renderer pass membership
     * - `AnimationName` empty means "play all available rigid animations" for auto-play and `PlayAllAnimations()`
     * - `MaterialInstanceOverrides` replace matching baked mesh material slots while leaving unspecified
     *   slots on the shared runtime mesh unchanged
     */
    SnType()
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletalMeshComponent::Settings";

        SnField(SnKey("MeshPath"), SnReplicated)
        std::string MeshPath{}; /**< @brief Compatibility mesh identifier tracked for change detection; direct source-path loading is not the primary supported path in the current implementation. */
        SnField(SnKey("Visible"), SnReplicated)
        bool Visible = true; /**< @brief Toggle visibility in primary geometry pass. */
        SnField(SnKey("CastShadows"), SnReplicated)
        bool CastShadows = true; /**< @brief Toggle participation in shadow pass. */
        SnField(SnKey("SyncFromTransform"))
        bool SyncFromTransform = true; /**< @brief Push owner transform to mesh local transform each tick. */
        SnField(SnKey("RegisterWithRenderer"))
        bool RegisterWithRenderer = true; /**< @brief Register loaded mesh in renderer draw list. */
        SnField(SnKey("AutoPlayAnimations"))
        bool AutoPlayAnimations = true; /**< @brief Auto-play animation after load. */
        SnField(SnKey("LoopAnimations"))
        bool LoopAnimations = true; /**< @brief Loop animation playback. */
        SnField(SnKey("AnimationName"))
        std::string AnimationName{}; /**< @brief Optional named rigid animation; empty = play all. */
        SnField(SnKey("MeshAsset"), SnReplicated)
        SkeletalMeshAssetRef MeshAsset{}; /**< @brief Preferred authored skeletal-mesh asset reference. */
        SnField(SnKey("MaterialInstanceOverrides"), SnReplicated)
        std::vector<TAssetRef<MaterialInstanceAsset>> MaterialInstanceOverrides{}; /**< @brief Optional per-material-slot overrides applied on top of mesh-default material instances. */
    };

    /** @brief Access settings (const). */
    const Settings& GetSettings() const
    {
        return m_settings;
    }

    /** @brief Access settings for mutation. */
    SnField(SnKey("Settings"), SnReplicated, SnConstGetter(GetSettings))
    Settings& EditSettings()
    {
        return m_settings;
    }

    /** @brief Explicitly clear cached load state and rebuild from current source settings. @return `true` when a render object is available after reload. */
    SnFunction(SnKey("ReloadMesh"))
    bool ReloadMesh();
    /** @brief Clear the current render object, stop its registration, and reset animation tracking state. */
    void ClearMesh();

    /** @brief Play one rigid animation by name on the loaded mesh. @param Name Animation name. @param Loop Whether playback should loop. @param StartTime Initial playback time in seconds. @return `true` when a render object exists and the request was applied. */
    SnFunction(SnKey("PlayAnimation"))
    bool PlayAnimation(const std::string& Name, bool Loop = true, float StartTime = 0.0f);
    /** @brief Play all rigid animations on the loaded mesh. @param Loop Whether playback should loop. @param StartTime Initial playback time in seconds. @return `true` when a render object exists and the request was applied. */
    SnFunction(SnKey("PlayAllAnimations"))
    bool PlayAllAnimations(bool Loop = true, float StartTime = 0.0f);
    /** @brief Stop all rigid animations on the loaded mesh and clear auto-play-applied state. */
    SnFunction(SnKey("StopAnimations"))
    void StopAnimations();

    /** @brief Access the component-owned renderer object handle. @return Borrowed reference to the shared-pointer handle, which may be empty. */
    [[nodiscard]] const std::shared_ptr<SnAPI::Graphics::MeshRenderObject>& RenderObject() const
    {
        return m_renderObject;
    }

    /** @brief Attempt initial skeletal mesh/render-object creation from the current source settings. */
    void OnCreate();
    /** @brief Remove the render object from renderer passes and clear owned state. */
    void OnDestroy();
    /** @brief Per-frame transform, pass-state, auto-play, and animation update. @param DeltaSeconds Frame delta in seconds. */
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /** @brief Editor-only update hook. @param DeltaSeconds Frame delta in seconds. */
    void EditorTick(float DeltaSeconds);
    /** @brief Editor-only property change hook. @param Name Changed reflected field name. */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    RendererSystem* ResolveRendererSystem() const;
    bool EnsureMeshLoaded();
    void SyncRenderObjectTransform(SnAPI::Graphics::MeshRenderObject& RenderObject) const;
    void ApplyConfiguredMaterialInstances(SnAPI::Graphics::MeshRenderObject& RenderObject);
    void ApplyRenderObjectState(SnAPI::Graphics::MeshRenderObject& RenderObject);
    void ApplyAutoPlay(SnAPI::Graphics::MeshRenderObject& RenderObject);

    Settings m_settings{}; /**< @brief Mesh/render/animation settings. */
    std::shared_ptr<SnAPI::Graphics::MeshRenderObject> m_renderObject{}; /**< @brief Per-instance render object state. */
    std::string m_loadedPath{}; /**< @brief Last successfully loaded path. */
    bool m_loadedFromAsset = false; /**< @brief True when current mesh originated from `Settings::MeshAsset`. */
    std::vector<TAssetRef<MaterialInstanceAsset>> m_loadedMeshMaterialInstances{}; /**< @brief Material instance asset refs baked into the currently loaded skeletal mesh asset. */
    std::string m_lastAutoPlayAnimation{}; /**< @brief Last animation name used for auto-play state tracking. */
    bool m_lastAutoPlayLoop = true; /**< @brief Last loop setting used for auto-play state tracking. */
    bool m_autoPlayApplied = false; /**< @brief True when auto-play has been applied for current settings. */
    bool m_registered = false; /**< @brief True when current mesh has been registered with renderer. */
    bool m_passStateInitialized = false; /**< @brief True after initial pass visibility/shadow state push. */
    bool m_lastVisible = true; /**< @brief Last applied visibility state. */
    bool m_lastCastShadows = true; /**< @brief Last applied cast-shadows state. */
    std::uint64_t m_lastPassGraphRevision = 0; /**< @brief Last renderer pass-graph revision applied to this render object. */
    std::string m_lastFailedPathLoadKey{}; /**< @brief Last source-path load key that failed; used to avoid per-frame retry loops. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
