#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"
#include "ReflectionAnnotations.h"

namespace SnAPI::Graphics
{
class DeferredShadingPass;
}

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Data-driven node that configures deferred-shading debug views for one or more viewports.
 *
 * `DeferredShadingParamsNode` exposes the renderer's deferred-material debug feature mask through
 * the world graph. The node is passive with respect to renderer setup: it does not create or own
 * deferred shading passes. Instead, it waits until matching passes exist and then applies the
 * authored feature mask.
 *
 * Core semantics:
 * - Negative viewport ids target all current render viewports.
 * - Non-negative ids target one renderer viewport by numeric id.
 * - Debug toggles map directly onto `SnAPI::Graphics::DeferredContract::Feature` bits.
 * - If multiple debug toggles are enabled simultaneously, shader-side precedence decides which
 *   view is shown.
 *
 * Ownership and lifetime:
 * - The node owns only its serialized settings.
 * - Deferred shading passes remain renderer-owned and may disappear across pass-graph rebuilds.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning `DebugTextureCoords` previews fullscreen UVs because the deferred pass does not store
 * mesh UVs in the G-buffer.
 *
 * @see RendererSystem
 * @see WorldRenderSettings
 */
SnType()
class SNAPI_GAMEFRAMEWORK_API DeferredShadingParamsNode : public BaseNode, public NodeCRTP<DeferredShadingParamsNode>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::DeferredShadingParamsNode";

    DeferredShadingParamsNode();
    explicit DeferredShadingParamsNode(std::string Name);

    SnField(SnKey("ViewportID"), SnConstGetter(GetViewportID))
    std::int64_t& EditViewportID();
    const std::int64_t& GetViewportID() const;

    SnField(SnKey("DebugMotionVectors"), SnConstGetter(GetDebugMotionVectors))
    bool& EditDebugMotionVectors();
    const bool& GetDebugMotionVectors() const;

    SnField(SnKey("DebugNormals"), SnConstGetter(GetDebugNormals))
    bool& EditDebugNormals();
    const bool& GetDebugNormals() const;

    SnField(SnKey("DebugAlbedo"), SnConstGetter(GetDebugAlbedo))
    bool& EditDebugAlbedo();
    const bool& GetDebugAlbedo() const;

    SnField(SnKey("DebugAO"), SnConstGetter(GetDebugAO))
    bool& EditDebugAO();
    const bool& GetDebugAO() const;

    SnField(SnKey("DebugRoughness"), SnConstGetter(GetDebugRoughness))
    bool& EditDebugRoughness();
    const bool& GetDebugRoughness() const;

    SnField(SnKey("DebugMetallic"), SnConstGetter(GetDebugMetallic))
    bool& EditDebugMetallic();
    const bool& GetDebugMetallic() const;

    SnField(SnKey("DebugDepth"), SnConstGetter(GetDebugDepth))
    bool& EditDebugDepth();
    const bool& GetDebugDepth() const;

    SnField(SnKey("DebugTextureCoords"), SnConstGetter(GetDebugTextureCoords))
    bool& EditDebugTextureCoords();
    const bool& GetDebugTextureCoords() const;

    SnField(SnKey("DebugDirectLighting"), SnConstGetter(GetDebugDirectLighting))
    bool& EditDebugDirectLighting();
    const bool& GetDebugDirectLighting() const;

    SnField(SnKey("DebugGI"), SnConstGetter(GetDebugGI))
    bool& EditDebugGI();
    const bool& GetDebugGI() const;

    SnField(SnKey("DebugSpecular"), SnConstGetter(GetDebugSpecular))
    bool& EditDebugSpecular();
    const bool& GetDebugSpecular() const;

    SnField(SnKey("DebugLighting"), SnConstGetter(GetDebugLighting))
    bool& EditDebugLighting();
    const bool& GetDebugLighting() const;

    void OnCreate();
    void OnDestroy();
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    void EditorTick(float DeltaSeconds);
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    void ApplyIfNeeded();
    bool ApplyToPass();
    void InvalidatePassCache();

    std::int64_t m_viewportID = -1;

    bool m_debugMotionVectors = false;
    bool m_debugNormals = false;
    bool m_debugAlbedo = false;
    bool m_debugAO = false;
    bool m_debugRoughness = false;
    bool m_debugMetallic = false;
    bool m_debugDepth = false;
    bool m_debugTextureCoords = false;
    bool m_debugDirectLighting = false;
    bool m_debugGI = false;
    bool m_debugSpecular = false;
    bool m_debugLighting = false;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
