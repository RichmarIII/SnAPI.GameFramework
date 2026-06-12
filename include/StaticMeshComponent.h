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
#include "RenderAssets/MaterialInstanceAsset.h"
#include "RenderAssets/StaticMeshAsset.h"
#include "ReflectionAnnotations.h"
#include "Rendering/GameRenderMesh.h"
#include "Rendering/GameRenderObject.h"

namespace SnAPI::GameFramework
{

class RendererSystem;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Component that builds and registers a static mesh instance with the renderer.
 *
 * `StaticMeshComponent` owns one renderer mesh record plus one retained renderer scene
 * object, and keeps that object synchronized with the owning node transform and
 * visibility/shadow state. It is the standard bridge from GameFramework nodes into
 * renderer scene geometry for rigid, non-animated meshes.
 *
 * Mesh source priority:
 * - `Settings::MeshAsset` is the authored/cooked asset path
 * - `Settings::MeshPath` accepts built-in primitive URIs such as `primitive://box`
 * - non-primitive filesystem/content strings are intentionally not loaded directly by this component
 *
 * Ownership and lifetime:
 * - Renderer.New builds retain `GameRenderMesh` and `GameRenderObject` records.
 * - Accessors return borrowed references to the component-owned renderer record.
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
     * - `RetainInScene` controls retained scene-object registration, not mesh loading
     * - `Visible` and `CastShadows` update renderer scene-object participation
     * - `MaterialInstanceOverrides` replace matching baked material slots while leaving unspecified slots unchanged
     */
    SnType()
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::StaticMeshComponent::Settings";

        SnField(SnKey("MeshPath"), SnReplicated)
        std::string MeshPath{}; /**< @brief Optional built-in primitive mesh URI such as `primitive://box`. */
        SnField(SnKey("Visible"), SnReplicated)
        bool Visible = true; /**< @brief Toggle retained scene-object visibility. */
        SnField(SnKey("CastShadows"), SnReplicated)
        bool CastShadows = true; /**< @brief Toggle shadow-capable feature participation for the retained object. */
        SnField(SnKey("SyncFromTransform"))
        bool SyncFromTransform = true; /**< @brief Push owner transform to mesh local transform each tick. */
        SnField(SnKey("RetainInScene"))
        bool RetainInScene = true; /**< @brief Retain the loaded mesh as a Renderer.New scene object. */
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
    /** @brief Clear the current render mesh and retained scene object. */
    void ClearMesh();

    [[nodiscard]] const GameRenderMesh& RenderMesh() const
    {
        return m_renderMesh;
    }

    [[nodiscard]] const GameRenderObject& RenderObject() const
    {
        return m_renderObject;
    }

    /** @brief Attempt initial mesh/render-object creation from the current source settings. */
    void OnCreate();
    /** @brief Destroy the retained scene object and clear owned render resources. */
    void OnDestroy();
    /** @brief Per-frame transform and retained scene-object synchronization. @param DeltaSeconds Frame delta in seconds. */
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

    Settings m_settings{}; /**< @brief Mesh/render settings. */
    GameRenderMesh m_renderMesh{};
    GameRenderObject m_renderObject{};
    std::string m_loadedPath{}; /**< @brief Last successfully loaded path. */
    bool m_loadedFromAsset = false; /**< @brief True when current mesh originated from `Settings::MeshAsset`. */
    std::vector<TAssetRef<MaterialInstanceAsset>> m_loadedMeshMaterialInstances{}; /**< @brief Material instance asset refs baked into currently loaded mesh asset. */
    bool m_lastVisible = true; /**< @brief Last applied visibility state. */
    bool m_lastCastShadows = true; /**< @brief Last applied cast-shadows state. */
    bool m_retainedSceneObjectStateInitialized = false; /**< @brief True after the retained object visibility/shadow state was evaluated. */
    std::string m_lastFailedPathLoadKey{}; /**< @brief Last source-path load key that failed; used to avoid per-frame retry loops. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
