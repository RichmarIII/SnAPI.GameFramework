#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(WITH_EDITOR) && WITH_EDITOR

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <GBufferPass.hpp>
#include <TMaterialFor.hpp>

namespace SnAPI::Graphics
{
class IRenderObject;
}

namespace SnAPI::GameFramework::Editor
{
/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Associate temporary editor-immediate metadata with one render object for one pass type.
 *
 * The editor uses this registry to annotate render objects that are queued through the renderer's
 * immediate editor path. The metadata is consumed later by editor-specific passes when they need
 * to distinguish ordinary selected-mesh highlighting from transform gizmo sub-parts.
 *
 * Core semantics:
 * - Metadata is keyed by the render-object pointer and render-pass type.
 * - Passing `IsGizmo == false` or `AxisTag == 0` removes any existing metadata for that key.
 * - Metadata is process-local and intended for per-frame editor rendering, not persistent storage.
 *
 * Threading model:
 * - Internally synchronized.
 *
 * @param RenderObject Borrowed render-object pointer used as the metadata key.
 * @param PassType Editor render pass that will consume the metadata.
 * @param IsGizmo `true` when the render object represents a gizmo primitive rather than ordinary scene geometry.
 * @param AxisTag Non-zero axis tag used for picking and overlay shading. `1`, `2`, and `3` conventionally map to X/Y/Z.
 */
void SetEditorImmediateRenderObjectMetadata(const SnAPI::Graphics::IRenderObject* RenderObject,
                                            SnAPI::Graphics::ERenderPassType PassType,
                                            bool IsGizmo,
                                            std::uint32_t AxisTag);
/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Remove editor-immediate metadata for one render object and pass type.
 * @param RenderObject Borrowed render-object pointer used as the metadata key.
 * @param PassType Editor render pass whose metadata entry should be removed.
 */
void RemoveEditorImmediateRenderObjectMetadata(const SnAPI::Graphics::IRenderObject* RenderObject,
                                               SnAPI::Graphics::ERenderPassType PassType);
/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Clear all registered editor-immediate metadata entries.
 * @remarks Intended for broad editor teardown or frame-scope reset paths.
 */
void ClearEditorImmediateRenderObjectMetadata();
/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Query the registered gizmo axis tag for one render object and pass type.
 * @param RenderObject Borrowed render-object pointer used as the metadata key.
 * @param PassType Editor render pass to query.
 * @return Registered axis tag, or `0` when no metadata exists for the key.
 */
[[nodiscard]] std::uint32_t EditorImmediateAxisTag(const SnAPI::Graphics::IRenderObject* RenderObject,
                                                   SnAPI::Graphics::ERenderPassType PassType);
/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Query whether one render object is currently tagged as an editor gizmo for a given pass.
 * @param RenderObject Borrowed render-object pointer used as the metadata key.
 * @param PassType Editor render pass to query.
 * @return `true` when a non-zero axis tag is registered for the key.
 */
[[nodiscard]] bool IsEditorImmediateGizmoRenderObject(const SnAPI::Graphics::IRenderObject* RenderObject,
                                                      SnAPI::Graphics::ERenderPassType PassType);

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Material contract used by the editor object-id pass.
 *
 * This contract defines the compile-time material interface used by `GFEditorIDPass`.
 * The pass writes per-object ids and gizmo flags into the editor id buffer so viewport picking
 * can resolve scene nodes and transform gizmo axes.
 */
struct GFEditorIDContract
{
    static constexpr SnAPI::Graphics::MaterialDomain Domain = SnAPI::Graphics::MaterialDomain::GBuffer;
    static constexpr std::string_view ShadingModelModuleName = "GFEditorIDShadingModel";
    static constexpr std::string_view HookInterfaceName = "IGFEditorIDHooks";

    /**
     * @brief Compile-time feature flags supported by the contract.
     */
    enum class Feature : std::uint32_t
    {
        None = 0,
        Instancing = 1u << 0
    };

    /**
     * @brief Runtime parameter block type.
     * @remarks The current editor-id material path uses only push constants, so the block is empty.
     */
    struct ParamBlock
    {
    };

    /**
     * @brief Map one feature flag to its shader preprocessor define.
     * @param Value Feature flag to translate.
     * @return Define name, or an empty string view when no define is associated.
     */
    static constexpr std::string_view PreprocessorNameForFeature(const Feature Value)
    {
        switch (Value)
        {
        case Feature::Instancing:
            return "WITH_INSTANCING";
        case Feature::None:
        default:
            return "";
        }
    }

    static inline constexpr std::array<Feature, 1> AllFeatures = {Feature::Instancing};
    static constexpr std::string_view ParamBlockName = "";
};

constexpr GFEditorIDContract::Feature operator|(const GFEditorIDContract::Feature Left,
                                                const GFEditorIDContract::Feature Right)
{
    return static_cast<GFEditorIDContract::Feature>(
        static_cast<std::uint32_t>(Left) | static_cast<std::uint32_t>(Right));
}

constexpr GFEditorIDContract::Feature operator&(const GFEditorIDContract::Feature Left,
                                                const GFEditorIDContract::Feature Right)
{
    return static_cast<GFEditorIDContract::Feature>(
        static_cast<std::uint32_t>(Left) & static_cast<std::uint32_t>(Right));
}

constexpr GFEditorIDContract::Feature operator~(const GFEditorIDContract::Feature Value)
{
    return static_cast<GFEditorIDContract::Feature>(~static_cast<std::uint32_t>(Value));
}

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Material contract used by the editor overlay pass.
 *
 * This contract drives the translucent overlay material used to highlight selected geometry and
 * render transform gizmos on top of the normal scene.
 */
struct GFEditorOverlayContract
{
    static constexpr SnAPI::Graphics::MaterialDomain Domain = SnAPI::Graphics::MaterialDomain::GBuffer;
    static constexpr std::string_view ShadingModelModuleName = "GFEditorOverlayShadingModel";
    static constexpr std::string_view HookInterfaceName = "IGFEditorOverlayHooks";

    /**
     * @brief Compile-time feature flags supported by the contract.
     */
    enum class Feature : std::uint32_t
    {
        None = 0,
        Instancing = 1u << 0
    };

    /**
     * @brief Runtime parameter block type.
     * @remarks The current overlay material path uses only push constants, so the block is empty.
     */
    struct ParamBlock
    {
    };

    /**
     * @brief Map one feature flag to its shader preprocessor define.
     * @param Value Feature flag to translate.
     * @return Define name, or an empty string view when no define is associated.
     */
    static constexpr std::string_view PreprocessorNameForFeature(const Feature Value)
    {
        switch (Value)
        {
        case Feature::Instancing:
            return "WITH_INSTANCING";
        case Feature::None:
        default:
            return "";
        }
    }

    static inline constexpr std::array<Feature, 1> AllFeatures = {Feature::Instancing};
    static constexpr std::string_view ParamBlockName = "";
};

constexpr GFEditorOverlayContract::Feature operator|(const GFEditorOverlayContract::Feature Left,
                                                     const GFEditorOverlayContract::Feature Right)
{
    return static_cast<GFEditorOverlayContract::Feature>(
        static_cast<std::uint32_t>(Left) | static_cast<std::uint32_t>(Right));
}

constexpr GFEditorOverlayContract::Feature operator&(const GFEditorOverlayContract::Feature Left,
                                                     const GFEditorOverlayContract::Feature Right)
{
    return static_cast<GFEditorOverlayContract::Feature>(
        static_cast<std::uint32_t>(Left) & static_cast<std::uint32_t>(Right));
}

constexpr GFEditorOverlayContract::Feature operator~(const GFEditorOverlayContract::Feature Value)
{
    return static_cast<GFEditorOverlayContract::Feature>(~static_cast<std::uint32_t>(Value));
}

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Material wrapper used by `GFEditorIDPass`.
 *
 * The wrapper bakes editor-id shader defines and pipeline state into a reusable material type
 * specialized for writing object-id data into the editor picking buffer.
 */
class GFEditorIDMaterial final : public SnAPI::Graphics::TMaterialFor<GFEditorIDContract>
{
public:
    using SnAPI::Graphics::TMaterialFor<GFEditorIDContract>::TMaterialFor;

protected:
    void OnBakeCompileTimeParams() override
    {
        for (const auto FeatureFlag : ContractType::AllFeatures)
        {
            if (const auto DefineName = ContractType::PreprocessorNameForFeature(FeatureFlag); !DefineName.empty())
            {
                SetDefine(DefineName, HasFeature(FeatureFlag));
            }
        }

        PipelineState().eVertexInputRate =
            HasFeature(Feature::Instancing) ? SnAPI::Graphics::EVertexInputRate::Instance : SnAPI::Graphics::EVertexInputRate::Vertex;
        PipelineState().DepthTest = true;
        PipelineState().DepthWrite = true;
    }
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Material wrapper used by `GFEditorOverlayPass`.
 *
 * The wrapper bakes overlay-specific pipeline state for translucent editor highlighting and gizmo rendering.
 */
class GFEditorOverlayMaterial final : public SnAPI::Graphics::TMaterialFor<GFEditorOverlayContract>
{
public:
    using SnAPI::Graphics::TMaterialFor<GFEditorOverlayContract>::TMaterialFor;

protected:
    void OnBakeCompileTimeParams() override
    {
        for (const auto FeatureFlag : ContractType::AllFeatures)
        {
            if (const auto DefineName = ContractType::PreprocessorNameForFeature(FeatureFlag); !DefineName.empty())
            {
                SetDefine(DefineName, HasFeature(FeatureFlag));
            }
        }

        PipelineState().AlphaBlend = true;
        PipelineState().ColorBlend = false;
        PipelineState().DepthTest = true;
        PipelineState().DepthWrite = true;
        PipelineState().eCullMode = SnAPI::Graphics::ECullMode::Back;
        PipelineState().eVertexInputRate =
            HasFeature(Feature::Instancing) ? SnAPI::Graphics::EVertexInputRate::Instance : SnAPI::Graphics::EVertexInputRate::Vertex;
    }
};

using SharedGFEditorIDMaterialPtr = std::shared_ptr<GFEditorIDMaterial>;
using SharedGFEditorOverlayMaterialPtr = std::shared_ptr<GFEditorOverlayMaterial>;

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief G-buffer-derived pass that writes editor object-id data.
 *
 * `GFEditorIDPass` renders scene geometry and editor gizmos using a dedicated material that encodes
 * renderer object ids and gizmo flags into the editor id target. The pass is primarily consumed by
 * viewport picking and transform-axis selection.
 *
 * Core semantics:
 * - Uses the active camera's view-without-rotation matrices to match the engine's editor-id shader expectations.
 * - Falls back to the default editor id material module when no explicit module is configured.
 * - Ignores unsupported shading-model overrides and forces the contract's required shading model.
 *
 * Ownership and lifetime:
 * - The pass owns its editor material and material instance.
 * - Render objects supplied to `RenderScene()` remain external and borrowed for the duration of the call only.
 */
class GFEditorIDPass final : public SnAPI::Graphics::GBufferPass
{
public:
    struct PropertyNames : SnAPI::Graphics::GBufferPass::PropertyNames
    {
        static constexpr std::string_view MaterialsModule = "Materials.Module";
    };

    /**
     * @brief Construct the pass with an optional property map.
     * @param Properties Initial pass properties. Missing material-module properties are defaulted.
     */
    explicit GFEditorIDPass(SnAPI::Graphics::PassProperties&& Properties = {});
    ~GFEditorIDPass() override = default;

    [[nodiscard]] SnAPI::Graphics::ERenderPassType PassType() const override
    {
        return SnAPI::Graphics::ERenderPassType::EditorID;
    }

    /**
     * @brief Build or rebuild the pass shader program and material instance.
     * @remarks
     * Rebuild occurs only when the configured material module or required shading model changes.
     */
    void CreateShaderProgram() override;

protected:
    /**
     * @brief Render the supplied scene objects into the editor id target.
     * @param Context Active pass context.
     * @param RenderObjects Borrowed render objects collected for this pass.
     * @remarks
     * Only objects that provide the required G-buffer streams are rendered. The pass also encodes
     * per-object gizmo metadata when it is registered through the editor-immediate metadata API.
     */
    void RenderScene(const SnAPI::Graphics::PassContext& Context,
                     const std::vector<std::shared_ptr<SnAPI::Graphics::IRenderObject>>& RenderObjects) override;

private:
    SharedGFEditorIDMaterialPtr m_editorIDMaterial{};
    SnAPI::Graphics::SharedMaterialInstancePtr m_editorIDMaterialInstance{};
};

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief G-buffer-derived pass that renders translucent editor overlays.
 *
 * `GFEditorOverlayPass` renders selected meshes and transform gizmos using editor-specific overlay
 * shading. The pass is responsible for visual highlighting, while axis-tag metadata allows shader
 * logic to color-code or otherwise distinguish gizmo axes.
 */
class GFEditorOverlayPass final : public SnAPI::Graphics::GBufferPass
{
public:
    struct PropertyNames : SnAPI::Graphics::GBufferPass::PropertyNames
    {
        static constexpr std::string_view MaterialsModule = "Materials.Module";
    };

    /**
     * @brief Construct the pass with an optional property map.
     * @param Properties Initial pass properties. Missing material-module properties are defaulted.
     */
    explicit GFEditorOverlayPass(SnAPI::Graphics::PassProperties&& Properties = {});
    ~GFEditorOverlayPass() override = default;

    [[nodiscard]] SnAPI::Graphics::ERenderPassType PassType() const override
    {
        return SnAPI::Graphics::ERenderPassType::EditorOverlay;
    }

    /**
     * @brief Build or rebuild the pass shader program and material instance.
     * @remarks
     * Rebuild occurs only when the configured material module or required shading model changes.
     */
    void CreateShaderProgram() override;

protected:
    /**
     * @brief Render the supplied scene objects into the editor overlay target.
     * @param Context Active pass context.
     * @param RenderObjects Borrowed render objects collected for this pass.
     * @remarks
     * The pass uses the editor overlay material for every eligible render object and pushes
     * axis-tag metadata so gizmo axes can be shaded differently from ordinary selected meshes.
     */
    void RenderScene(const SnAPI::Graphics::PassContext& Context,
                     const std::vector<std::shared_ptr<SnAPI::Graphics::IRenderObject>>& RenderObjects) override;

private:
    SharedGFEditorOverlayMaterialPtr m_editorOverlayMaterial{};
    SnAPI::Graphics::SharedMaterialInstancePtr m_editorOverlayMaterialInstance{};
};

} // namespace SnAPI::GameFramework::Editor

#endif
