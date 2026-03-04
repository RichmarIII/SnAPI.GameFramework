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
void SetEditorImmediateRenderObjectMetadata(const SnAPI::Graphics::IRenderObject* RenderObject,
                                            SnAPI::Graphics::ERenderPassType PassType,
                                            bool IsGizmo,
                                            std::uint32_t AxisTag);
void RemoveEditorImmediateRenderObjectMetadata(const SnAPI::Graphics::IRenderObject* RenderObject,
                                               SnAPI::Graphics::ERenderPassType PassType);
void ClearEditorImmediateRenderObjectMetadata();
[[nodiscard]] std::uint32_t EditorImmediateAxisTag(const SnAPI::Graphics::IRenderObject* RenderObject,
                                                   SnAPI::Graphics::ERenderPassType PassType);
[[nodiscard]] bool IsEditorImmediateGizmoRenderObject(const SnAPI::Graphics::IRenderObject* RenderObject,
                                                      SnAPI::Graphics::ERenderPassType PassType);

struct GFEditorIDContract
{
    static constexpr SnAPI::Graphics::MaterialDomain Domain = SnAPI::Graphics::MaterialDomain::GBuffer;
    static constexpr std::string_view ShadingModelModuleName = "GFEditorIDShadingModel";
    static constexpr std::string_view HookInterfaceName = "IGFEditorIDHooks";

    enum class Feature : std::uint32_t
    {
        None = 0,
        Instancing = 1u << 0
    };

    struct ParamBlock
    {
    };

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

struct GFEditorOverlayContract
{
    static constexpr SnAPI::Graphics::MaterialDomain Domain = SnAPI::Graphics::MaterialDomain::GBuffer;
    static constexpr std::string_view ShadingModelModuleName = "GFEditorOverlayShadingModel";
    static constexpr std::string_view HookInterfaceName = "IGFEditorOverlayHooks";

    enum class Feature : std::uint32_t
    {
        None = 0,
        Instancing = 1u << 0
    };

    struct ParamBlock
    {
    };

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

class GFEditorIDPass final : public SnAPI::Graphics::GBufferPass
{
public:
    struct PropertyNames : SnAPI::Graphics::GBufferPass::PropertyNames
    {
        static constexpr std::string_view MaterialsModule = "Materials.Module";
    };

    explicit GFEditorIDPass(SnAPI::Graphics::PassProperties&& Properties = {});
    ~GFEditorIDPass() override = default;

    [[nodiscard]] SnAPI::Graphics::ERenderPassType PassType() const override
    {
        return SnAPI::Graphics::ERenderPassType::EditorID;
    }

    void CreateShaderProgram() override;

protected:
    void RenderScene(const SnAPI::Graphics::PassContext& Context,
                     const std::vector<std::shared_ptr<SnAPI::Graphics::IRenderObject>>& RenderObjects) override;

private:
    SharedGFEditorIDMaterialPtr m_editorIDMaterial{};
    SnAPI::Graphics::SharedMaterialInstancePtr m_editorIDMaterialInstance{};
};

class GFEditorOverlayPass final : public SnAPI::Graphics::GBufferPass
{
public:
    struct PropertyNames : SnAPI::Graphics::GBufferPass::PropertyNames
    {
        static constexpr std::string_view MaterialsModule = "Materials.Module";
    };

    explicit GFEditorOverlayPass(SnAPI::Graphics::PassProperties&& Properties = {});
    ~GFEditorOverlayPass() override = default;

    [[nodiscard]] SnAPI::Graphics::ERenderPassType PassType() const override
    {
        return SnAPI::Graphics::ERenderPassType::EditorOverlay;
    }

    void CreateShaderProgram() override;

protected:
    void RenderScene(const SnAPI::Graphics::PassContext& Context,
                     const std::vector<std::shared_ptr<SnAPI::Graphics::IRenderObject>>& RenderObjects) override;

private:
    SharedGFEditorOverlayMaterialPtr m_editorOverlayMaterial{};
    SnAPI::Graphics::SharedMaterialInstancePtr m_editorOverlayMaterialInstance{};
};

} // namespace SnAPI::GameFramework::Editor

#endif
