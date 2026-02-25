#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>
#include <string>
#include <cstdint>

#include "BaseComponent.h"

namespace SnAPI::Graphics
{
class MeshRenderObject;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{

class RendererSystem;

/**
 * @brief Component that loads an animated mesh and updates rigid-part animations.
 */
class SkeletalMeshComponent : public BaseComponent, public ComponentCRTP<SkeletalMeshComponent>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletalMeshComponent";

    /**
     * @brief Runtime mesh/render/animation settings.
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::SkeletalMeshComponent::Settings";

        std::string MeshPath{}; /**< @brief Mesh asset path resolved by `MeshManager`. */
        bool Visible = true; /**< @brief Toggle visibility in primary geometry pass. */
        bool CastShadows = true; /**< @brief Toggle participation in shadow pass. */
        bool SyncFromTransform = true; /**< @brief Push owner transform to mesh local transform each tick. */
        bool RegisterWithRenderer = true; /**< @brief Register loaded mesh in renderer draw list. */
        bool AutoPlayAnimations = true; /**< @brief Auto-play animation after load. */
        bool LoopAnimations = true; /**< @brief Loop animation playback. */
        std::string AnimationName{}; /**< @brief Optional named rigid animation; empty = play all. */
    };

    /** @brief Access settings (const). */
    const Settings& GetSettings() const
    {
        return m_settings;
    }

    /** @brief Access settings for mutation. */
    Settings& EditSettings()
    {
        return m_settings;
    }

    /** @brief Explicitly reload mesh from current settings path. */
    bool ReloadMesh();
    /** @brief Clear currently loaded mesh reference. */
    void ClearMesh();

    /** @brief Play one rigid animation by name on loaded mesh. */
    bool PlayAnimation(const std::string& Name, bool Loop = true, float StartTime = 0.0f);
    /** @brief Play all rigid animations on loaded mesh. */
    bool PlayAllAnimations(bool Loop = true, float StartTime = 0.0f);
    /** @brief Stop all rigid animations on loaded mesh. */
    void StopAnimations();

    void OnCreate();
    void OnDestroy();
    void Tick(float DeltaSeconds);

    /** @brief Non-virtual tick entry used by ECS runtime bridge. */
    void RuntimeTick(float DeltaSeconds);
    void OnCreateImpl(IWorld&) { OnCreate(); }
    void OnDestroyImpl(IWorld&) { OnDestroy(); }
    void TickImpl(IWorld&, float DeltaSeconds) { RuntimeTick(DeltaSeconds); }

private:
    RendererSystem* ResolveRendererSystem() const;
    bool EnsureMeshLoaded();
    void SyncRenderObjectTransform(SnAPI::Graphics::MeshRenderObject& RenderObject) const;
    void ApplyRenderObjectState(SnAPI::Graphics::MeshRenderObject& RenderObject);
    void ApplyAutoPlay(SnAPI::Graphics::MeshRenderObject& RenderObject);

    Settings m_settings{}; /**< @brief Mesh/render/animation settings. */
    std::shared_ptr<SnAPI::Graphics::MeshRenderObject> m_renderObject{}; /**< @brief Per-instance render object state. */
    std::string m_loadedPath{}; /**< @brief Last successfully loaded path. */
    std::string m_lastAutoPlayAnimation{}; /**< @brief Last animation name used for auto-play state tracking. */
    bool m_lastAutoPlayLoop = true; /**< @brief Last loop setting used for auto-play state tracking. */
    bool m_autoPlayApplied = false; /**< @brief True when auto-play has been applied for current settings. */
    bool m_registered = false; /**< @brief True when current mesh has been registered with renderer. */
    bool m_passStateInitialized = false; /**< @brief True after initial pass visibility/shadow state push. */
    bool m_lastVisible = true; /**< @brief Last applied visibility state. */
    bool m_lastCastShadows = true; /**< @brief Last applied cast-shadows state. */
    std::uint64_t m_lastPassGraphRevision = 0; /**< @brief Last renderer pass-graph revision applied to this render object. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
