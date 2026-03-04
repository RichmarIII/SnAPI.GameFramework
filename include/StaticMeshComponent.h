#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

#include "AssetRef.h"
#include "BaseComponent.h"
#include "RenderAssetRuntime.h"

namespace SnAPI::Graphics
{
class MaterialInstance;
class IRenderObject;
class IVertexStreamSource;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{

class RendererSystem;

/**
 * @brief Component that loads and registers a static mesh with the renderer.
 */
class StaticMeshComponent : public BaseComponent, public ComponentCRTP<StaticMeshComponent>
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

        std::string MeshPath{}; /**< @brief Optional mesh source path token (primitives or source-path fallback). */
        bool Visible = true; /**< @brief Toggle visibility in primary geometry pass. */
        bool CastShadows = true; /**< @brief Toggle participation in shadow pass. */
        bool SyncFromTransform = true; /**< @brief Push owner transform to mesh local transform each tick. */
        bool RegisterWithRenderer = true; /**< @brief Register loaded mesh in renderer draw list. */
        TAssetRef<StaticMeshAssetRuntime> MeshAsset{}; /**< @brief Preferred runtime asset reference for cooked static meshes (appended for payload compatibility). */
        std::vector<TAssetRef<MaterialInstanceAssetRuntime>> MaterialInstanceOverrides{}; /**< @brief Optional per-material-slot overrides applied on top of mesh-default material instances. */
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

    /**
     * @brief Override the render object geometry source with a procedural vertex stream.
     * @remarks
     * When set, this takes precedence over `Settings::MeshPath`. Clearing the source falls
     * back to mesh-path loading behavior.
     */
    void SetVertexStreamSource(std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> StreamSource);

    /** @brief Get the currently assigned procedural vertex stream source override. */
    [[nodiscard]] const std::shared_ptr<SnAPI::Graphics::IVertexStreamSource>& GetVertexStreamSource() const
    {
        return m_streamSource;
    }

    [[nodiscard]] const std::shared_ptr<SnAPI::Graphics::IRenderObject>& RenderObject() const
    {
        return m_renderObject;
    }

    void OnCreate();
    void OnDestroy();
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    void EditorTick(float DeltaSeconds);
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    RendererSystem* ResolveRendererSystem() const;
    bool EnsureMeshLoaded();
    void SyncRenderObjectTransform(SnAPI::Graphics::IRenderObject& RenderObject) const;
    void ApplyConfiguredMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject);
    void ApplySharedMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject) const;
    void ApplyRenderObjectState(SnAPI::Graphics::IRenderObject& RenderObject);

    Settings m_settings{}; /**< @brief Mesh/render settings. */
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_renderObject{}; /**< @brief Per-instance render object state. */
    std::string m_loadedPath{}; /**< @brief Last successfully loaded path. */
    bool m_loadedFromAsset = false; /**< @brief True when current mesh originated from `Settings::MeshAsset`. */
    std::vector<TAssetRef<MaterialInstanceAssetRuntime>> m_loadedMeshMaterialInstances{}; /**< @brief Material instance refs baked into currently loaded mesh asset. */
    bool m_registered = false; /**< @brief True when current mesh has been registered with renderer. */
    bool m_passStateInitialized = false; /**< @brief True after initial pass visibility/shadow state push. */
    bool m_lastVisible = true; /**< @brief Last applied visibility state. */
    bool m_lastCastShadows = true; /**< @brief Last applied cast-shadows state. */
    std::uint64_t m_lastPassGraphRevision = 0; /**< @brief Last renderer pass-graph revision applied to this render object. */
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> m_sharedGBufferInstance{}; /**< @brief Optional shared GBuffer material instance override. */
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> m_sharedShadowInstance{}; /**< @brief Optional shared shadow material instance override. */
    std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> m_streamSource{}; /**< @brief Optional procedural stream source override. */
    std::weak_ptr<SnAPI::Graphics::IVertexStreamSource> m_loadedStreamSource{}; /**< @brief Last procedural source used to build current render object. */
    std::string m_lastFailedPathLoadKey{}; /**< @brief Last source-path load key that failed; used to avoid per-frame retry loops. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
