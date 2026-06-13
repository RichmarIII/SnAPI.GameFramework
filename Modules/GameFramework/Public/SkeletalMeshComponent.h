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
#include "RenderAssets/MaterialInstanceAsset.h"
#include "RenderAssets/SkeletalMeshAsset.h"
#include "Rendering/GameRenderMesh.h"
#include "Rendering/GameRenderObject.h"

namespace SnAPI::GameFramework
{

class RendererSystem;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Component that owns a skeletal mesh render object and drives rigid-animation playback.
 *
 * `SkeletalMeshComponent` is the animated counterpart to `StaticMeshComponent`. It owns one
 * renderer mesh object, retains it in the active renderer scene, synchronizes its transform
 * from the owning node, and optionally auto-plays rigid animations after load.
 *
 * Source semantics:
 * - `Settings::MeshAsset` is the authored/cooked asset path
 * - `Settings::MeshPath` is available for future primitive or generated mesh URI support, but this
 *   component currently requires `MeshAsset` for actual loading
 *
 * Animation semantics:
 * - `AutoPlayAnimations` is applied lazily after a mesh exists
 * - changing animation name or loop mode clears the applied-auto-play state and reapplies on the next update
 * - `StopAnimations()` stops active rigid animations and clears the auto-play-applied flag
 *
 * Ownership and lifetime:
 * - Renderer.New builds retain `GameRenderMesh` and `GameRenderObject` records.
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
     * - `RetainInScene` controls retained scene-object registration, not mesh loading
     * - `Visible` and `CastShadows` update renderer scene-object participation
     * - `AnimationName` empty means "play all available rigid animations" for auto-play and `PlayAllAnimations()`
     * - `MaterialInstanceOverrides` replace matching baked mesh material slots while leaving unspecified
     *   slots on the shared runtime mesh unchanged
     */
    SnType()
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletalMeshComponent::Settings";

        SnField(SnKey("MeshPath"), SnReplicated)
        std::string MeshPath{}; /**< @brief Optional mesh URI reserved for generated or primitive skeletal mesh sources. */
        SnField(SnKey("Visible"), SnReplicated)
        bool Visible = true; /**< @brief Toggle retained scene-object visibility. */
        SnField(SnKey("CastShadows"), SnReplicated)
        bool CastShadows = true; /**< @brief Toggle shadow-capable feature participation for the retained object. */
        SnField(SnKey("SyncFromTransform"))
        bool SyncFromTransform = true; /**< @brief Push owner transform to mesh local transform each tick. */
        SnField(SnKey("RetainInScene"))
        bool RetainInScene = true; /**< @brief Retain the loaded mesh as a Renderer.New scene object. */
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
    /** @brief Clear the current render mesh, retained scene object, and animation tracking state. */
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

    [[nodiscard]] const GameRenderMesh& RenderMesh() const
    {
        return m_renderMesh;
    }

    [[nodiscard]] const GameRenderObject& RenderObject() const
    {
        return m_renderObject;
    }

    /** @brief Attempt initial skeletal mesh/render-object creation from the current source settings. */
    void OnCreate();
    /** @brief Destroy the retained scene object and clear owned render resources. */
    void OnDestroy();
    /** @brief Per-frame transform, retained scene-object, auto-play, and animation update. @param DeltaSeconds Frame delta in seconds. */
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
    void SyncRenderObjectTransform();
    void UpdateRetainedSceneObject();

    Settings m_settings{}; /**< @brief Mesh/render/animation settings. */
    GameRenderMesh m_renderMesh{};
    GameRenderObject m_renderObject{};
    std::string m_loadedPath{}; /**< @brief Last successfully loaded path. */
    bool m_loadedFromAsset = false; /**< @brief True when current mesh originated from `Settings::MeshAsset`. */
    std::vector<TAssetRef<MaterialInstanceAsset>> m_loadedMeshMaterialInstances{}; /**< @brief Material instance asset refs baked into the currently loaded skeletal mesh asset. */
    std::string m_lastAutoPlayAnimation{}; /**< @brief Last animation name used for auto-play state tracking. */
    bool m_lastAutoPlayLoop = true; /**< @brief Last loop setting used for auto-play state tracking. */
    bool m_autoPlayApplied = false; /**< @brief True when auto-play has been applied for current settings. */
    bool m_lastVisible = true; /**< @brief Last applied visibility state. */
    bool m_lastCastShadows = true; /**< @brief Last applied cast-shadows state. */
    bool m_retainedSceneObjectStateInitialized = false; /**< @brief True after the retained object visibility/shadow state was evaluated. */
    std::string m_lastFailedPathLoadKey{}; /**< @brief Last source-path load key that failed; used to avoid per-frame retry loops. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
