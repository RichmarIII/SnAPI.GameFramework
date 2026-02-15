#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>
#include <string>

#include "IComponent.h"

namespace SnAPI::Graphics
{
struct Mesh;
class MaterialInstance;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{

class RendererSystem;

/**
 * @brief Component that loads and registers a static mesh with the renderer.
 */
class StaticMeshComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::StaticMeshComponent";

    /**
     * @brief Runtime mesh/render settings.
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::StaticMeshComponent::Settings";

        std::string MeshPath{}; /**< @brief Mesh asset path resolved by `MeshManager`. */
        bool Visible = true; /**< @brief Toggle visibility in primary geometry pass. */
        bool CastShadows = true; /**< @brief Toggle participation in shadow pass. */
        bool SyncFromTransform = true; /**< @brief Push owner transform to mesh local transform each tick. */
        bool RegisterWithRenderer = true; /**< @brief Register loaded mesh in renderer draw list. */
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

    /**
     * @brief Override mesh submesh material instances with shared instances.
     * @remarks Useful for stress/perf scenarios where many objects intentionally share one descriptor state.
     */
    void SetSharedMaterialInstances(std::shared_ptr<SnAPI::Graphics::MaterialInstance> GBufferInstance,
                                    std::shared_ptr<SnAPI::Graphics::MaterialInstance> ShadowInstance = {});

    void OnCreate() override;
    void OnDestroy() override;
    void Tick(float DeltaSeconds) override;

private:
    RendererSystem* ResolveRendererSystem() const;
    bool EnsureMeshLoaded();
    void SyncMeshTransform(SnAPI::Graphics::Mesh& Mesh) const;
    void ApplySharedMaterialInstances(SnAPI::Graphics::Mesh& Mesh) const;
    void ApplyMeshRenderingState(SnAPI::Graphics::Mesh& Mesh);

    Settings m_settings{}; /**< @brief Mesh/render settings. */
    std::shared_ptr<SnAPI::Graphics::Mesh> m_mesh{}; /**< @brief Component-owned mesh instance. */
    std::string m_loadedPath{}; /**< @brief Last successfully loaded path. */
    bool m_registered = false; /**< @brief True when current mesh has been registered with renderer. */
    bool m_passStateInitialized = false; /**< @brief True after initial pass visibility/shadow state push. */
    bool m_lastVisible = true; /**< @brief Last applied visibility state. */
    bool m_lastCastShadows = true; /**< @brief Last applied cast-shadows state. */
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> m_sharedGBufferInstance{}; /**< @brief Optional shared GBuffer material instance override. */
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> m_sharedShadowInstance{}; /**< @brief Optional shared shadow material instance override. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
