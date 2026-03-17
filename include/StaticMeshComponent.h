#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

#include "AssetRef.h"
#include "BaseComponent.h"
#include "BuiltinTypes.h"
#include "RenderAssetPayloads.h"
#include "ReflectionAnnotations.h"

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
 * @ingroup SnAPI_GameFramework
 * @brief Component that builds and registers a static render object with the renderer.
 *
 * `StaticMeshComponent` owns one renderer mesh render-object and keeps it synchronized
 * with the owning node's transform and visibility/shadow participation state. It is the
 * standard bridge from GameFramework nodes into the renderer geometry passes for rigid,
 * non-animated meshes.
 *
 * Mesh source priority:
 * - a procedural vertex-stream source set through `SetVertexStreamSource(...)` takes highest priority
 * - otherwise `Settings::MeshAsset` is the preferred runtime/cooked asset path
 * - otherwise certain primitive tokens in `Settings::MeshPath` are supported
 *
 * Current implementation note:
 * - `MeshPath` reliably supports built-in primitive tokens such as `primitive://box`
 * - non-primitive filesystem/content-path strings participate in change detection/editor compatibility,
 *   but do not currently guarantee direct source-path loading on their own
 *
 * Ownership and lifetime:
 * - The component owns the `IRenderObject` shared pointer it creates.
 * - The renderer only borrows the render object while it is registered in passes.
 * - `RenderObject()` returns a borrowed reference to the component-owned shared pointer.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see RendererSystem
 * @see TransformComponent
 */
SnType()
class StaticMeshComponent : public BaseComponent, public ComponentCRTP<StaticMeshComponent>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::StaticMeshComponent";

    /**
     * @brief Runtime mesh/render settings.
     *
     * Semantics:
     * - `RegisterWithRenderer` controls pass registration, not object creation
     * - `Visible` and `CastShadows` feed renderer pass membership through `RendererSystem::ConfigureRenderObjectPasses(...)`
     * - `MaterialInstanceOverrides` replace matching baked material slots while leaving unspecified slots unchanged
     */
    SnType()
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::StaticMeshComponent::Settings";

        SnField(SnKey("MeshPath"), SnReplicated)
        std::string MeshPath{}; /**< @brief Optional compatibility mesh token. Primitive tokens are supported directly; non-primitive paths are currently best treated as metadata/change keys rather than guaranteed load sources. */
        SnField(SnKey("Visible"), SnReplicated)
        bool Visible = true; /**< @brief Toggle visibility in primary geometry pass. */
        SnField(SnKey("CastShadows"), SnReplicated)
        bool CastShadows = true; /**< @brief Toggle participation in shadow pass. */
        SnField(SnKey("SyncFromTransform"))
        bool SyncFromTransform = true; /**< @brief Push owner transform to mesh local transform each tick. */
        SnField(SnKey("RegisterWithRenderer"))
        bool RegisterWithRenderer = true; /**< @brief Register loaded mesh in renderer draw list. */
        SnField(SnKey("MeshAsset"), SnReplicated)
        StaticMeshAssetRef MeshAsset{}; /**< @brief Preferred authored static-mesh asset reference. */
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
    /** @brief Clear the current render object and remove it from all renderer passes. */
    void ClearMesh();

    /**
     * @brief Override mesh submesh material instances with shared instances.
     * @param GBufferInstance Shared material instance applied to all geometry-pass submeshes.
     * @param ShadowInstance Optional shared material instance applied to all shadow-pass submeshes.
     * @remarks Useful when many objects should intentionally share one descriptor/material-state set.
     */
    void SetSharedMaterialInstances(std::shared_ptr<SnAPI::Graphics::MaterialInstance> GBufferInstance,
                                    std::shared_ptr<SnAPI::Graphics::MaterialInstance> ShadowInstance = {});

    /**
     * @brief Override the render-object geometry source with a procedural vertex stream.
     * @param StreamSource Shared procedural stream source, or null to clear the override.
     * @remarks
     * When set, this takes precedence over asset and mesh-path loading.
     * Changing the source clears the current render object so it can be rebuilt lazily.
     */
    void SetVertexStreamSource(std::shared_ptr<SnAPI::Graphics::IVertexStreamSource> StreamSource);

    /** @brief Get the currently assigned procedural vertex stream source override. */
    [[nodiscard]] const std::shared_ptr<SnAPI::Graphics::IVertexStreamSource>& GetVertexStreamSource() const
    {
        return m_streamSource;
    }

    /** @brief Access the component-owned renderer object handle. @return Borrowed reference to the shared-pointer handle, which may be empty. */
    [[nodiscard]] const std::shared_ptr<SnAPI::Graphics::IRenderObject>& RenderObject() const
    {
        return m_renderObject;
    }

    /** @brief Attempt initial mesh/render-object creation from the current source settings. */
    void OnCreate();
    /** @brief Remove the render object from renderer passes and clear owned state. */
    void OnDestroy();
    /** @brief Per-frame transform/pass-state synchronization. @param DeltaSeconds Frame delta in seconds. */
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
    void SyncRenderObjectTransform(SnAPI::Graphics::IRenderObject& RenderObject) const;
    void ApplyConfiguredMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject);
    void ApplySharedMaterialInstances(SnAPI::Graphics::IRenderObject& RenderObject) const;
    void ApplyRenderObjectState(SnAPI::Graphics::IRenderObject& RenderObject);

    Settings m_settings{}; /**< @brief Mesh/render settings. */
    std::shared_ptr<SnAPI::Graphics::IRenderObject> m_renderObject{}; /**< @brief Per-instance render object state. */
    std::string m_loadedPath{}; /**< @brief Last successfully loaded path. */
    bool m_loadedFromAsset = false; /**< @brief True when current mesh originated from `Settings::MeshAsset`. */
    std::vector<TAssetRef<MaterialInstanceAsset>> m_loadedMeshMaterialInstances{}; /**< @brief Material instance asset refs baked into currently loaded mesh asset. */
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
