#include "Editor/EditorRenderPasses.h"

#if defined(SNAPI_GF_ENABLE_RENDERER) && defined(WITH_EDITOR) && WITH_EDITOR

#if defined(SNAPI_GF_ENABLE_LEGACY_RENDERER)

#include <ICamera.hpp>
#include <IGraphicsAPI.hpp>
#include <IRenderObject.hpp>
#include <MaterialInstance.hpp>
#include <PCH.hpp>
#include <TVertexDataFor.hpp>
#include <VertexFactory.hpp>
#include <VulkanGraphicsAPI.hpp>

#include <mutex>
#include <unordered_map>

namespace SnAPI::GameFramework::Editor
{
namespace
{
struct GFEditorIDSourceCacheKey
{
    std::uint64_t SourceID{};
    std::uint64_t SourceRevision{};

    [[nodiscard]] bool operator==(const GFEditorIDSourceCacheKey& Rhs) const = default;
};

struct GFEditorIDSourceCacheKeyHasher
{
    [[nodiscard]] std::size_t operator()(const GFEditorIDSourceCacheKey& Key) const noexcept
    {
        std::size_t Seed = std::hash<std::uint64_t>{}(Key.SourceID);
        SnAPI::HashCombine(Seed, Key.SourceRevision);
        return Seed;
    }
};

struct GFEditorIDCameraPerObject
{
    SnAPI::Matrix4F Model{SnAPI::Matrix4F::Identity()};
    SnAPI::Matrix4F PrevModel{SnAPI::Matrix4F::Identity()};
    std::uint32_t InstanceOffset{0};
    std::uint32_t ObjectID{0};
    std::uint32_t Flags{0};
    std::uint32_t Pad{};
};

static_assert(sizeof(GFEditorIDCameraPerObject) == ((sizeof(SnAPI::Matrix4F) * 2) + 16),
              "GameFramework editor ID push constant layout mismatch");

enum class EEditorIDObjectFlags : std::uint32_t
{
    None = 0u,
    Gizmo = 1u << 0u,
};

struct GFEditorOverlaySourceCacheKey
{
    std::uint64_t SourceID{};
    std::uint64_t SourceRevision{};

    [[nodiscard]] bool operator==(const GFEditorOverlaySourceCacheKey& Rhs) const = default;
};

struct GFEditorOverlaySourceCacheKeyHasher
{
    [[nodiscard]] std::size_t operator()(const GFEditorOverlaySourceCacheKey& Key) const noexcept
    {
        std::size_t Seed = std::hash<std::uint64_t>{}(Key.SourceID);
        SnAPI::HashCombine(Seed, Key.SourceRevision);
        return Seed;
    }
};

struct GFEditorOverlayCameraPerObject
{
    SnAPI::Matrix4F Model{SnAPI::Matrix4F::Identity()};
    SnAPI::Matrix4F PrevModel{SnAPI::Matrix4F::Identity()};
    std::uint32_t InstanceOffset{0};
    std::uint32_t AxisTag{0};
    std::uint32_t Pad[2]{};
};

static_assert(sizeof(GFEditorOverlayCameraPerObject) == ((sizeof(SnAPI::Matrix4F) * 2) + 16),
              "GameFramework editor overlay push constant layout mismatch");

struct EditorImmediateMetadataKey
{
    const SnAPI::Graphics::IRenderObject* RenderObject{};
    SnAPI::Graphics::ERenderPassType PassType{SnAPI::Graphics::ERenderPassType::UnKnown};

    [[nodiscard]] bool operator==(const EditorImmediateMetadataKey& Other) const = default;
};

struct EditorImmediateMetadataKeyHasher
{
    [[nodiscard]] std::size_t operator()(const EditorImmediateMetadataKey& Value) const noexcept
    {
        std::size_t Seed = std::hash<const void*>{}(Value.RenderObject);
        SnAPI::HashCombine(Seed, static_cast<std::uint32_t>(Value.PassType));
        return Seed;
    }
};

using EditorImmediateMetadataMap = std::unordered_map<EditorImmediateMetadataKey, std::uint32_t, EditorImmediateMetadataKeyHasher>;

[[nodiscard]] EditorImmediateMetadataMap& EditorGizmoRegistry()
{
    static EditorImmediateMetadataMap Registry{};
    return Registry;
}

[[nodiscard]] std::mutex& EditorGizmoRegistryMutex()
{
    static std::mutex Mutex{};
    return Mutex;
}
}

void SetEditorImmediateRenderObjectMetadata(const SnAPI::Graphics::IRenderObject* RenderObject,
                                            const SnAPI::Graphics::ERenderPassType PassType,
                                            const bool IsGizmo,
                                            const std::uint32_t AxisTag)
{
    if (!RenderObject || PassType == SnAPI::Graphics::ERenderPassType::UnKnown)
    {
        return;
    }

    std::scoped_lock Lock(EditorGizmoRegistryMutex());
    const EditorImmediateMetadataKey Key{RenderObject, PassType};
    if (!IsGizmo || AxisTag == 0u)
    {
        EditorGizmoRegistry().erase(Key);
        return;
    }
    EditorGizmoRegistry()[Key] = AxisTag;
}

void RemoveEditorImmediateRenderObjectMetadata(const SnAPI::Graphics::IRenderObject* RenderObject,
                                               const SnAPI::Graphics::ERenderPassType PassType)
{
    if (!RenderObject || PassType == SnAPI::Graphics::ERenderPassType::UnKnown)
    {
        return;
    }

    std::scoped_lock Lock(EditorGizmoRegistryMutex());
    const EditorImmediateMetadataKey Key{RenderObject, PassType};
    EditorGizmoRegistry().erase(Key);
}

void ClearEditorImmediateRenderObjectMetadata()
{
    std::scoped_lock Lock(EditorGizmoRegistryMutex());
    EditorGizmoRegistry().clear();
}

std::uint32_t EditorImmediateAxisTag(const SnAPI::Graphics::IRenderObject* RenderObject,
                                     const SnAPI::Graphics::ERenderPassType PassType)
{
    if (!RenderObject || PassType == SnAPI::Graphics::ERenderPassType::UnKnown)
    {
        return 0u;
    }

    std::scoped_lock Lock(EditorGizmoRegistryMutex());
    const EditorImmediateMetadataKey Key{RenderObject, PassType};
    if (const auto It = EditorGizmoRegistry().find(Key); It != EditorGizmoRegistry().end())
    {
        return It->second;
    }
    return 0u;
}

bool IsEditorImmediateGizmoRenderObject(const SnAPI::Graphics::IRenderObject* RenderObject,
                                        const SnAPI::Graphics::ERenderPassType PassType)
{
    return EditorImmediateAxisTag(RenderObject, PassType) != 0u;
}

GFEditorIDPass::GFEditorIDPass(SnAPI::Graphics::PassProperties&& Properties)
    : SnAPI::Graphics::GBufferPass(std::move(Properties))
{
    Property(PropertyNames::MaterialsShadingModel, std::string{GFEditorIDContract::ShadingModelModuleName});

    if (!Property(PropertyNames::MaterialsModule).has_value())
    {
        Property(PropertyNames::MaterialsModule, std::string{"GFDefaultEditorIDMaterial"});
    }
}

void GFEditorIDPass::CreateShaderProgram()
{
    const std::string RequiredShadingModel{GFEditorIDContract::ShadingModelModuleName};
    const std::string ConfiguredShadingModel = std::get<std::string>(
        Property(PropertyNames::MaterialsShadingModel).value_or(RequiredShadingModel));
    if (ConfiguredShadingModel != RequiredShadingModel)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorIDPass: shading model override '%s' is not supported; using '%s'",
                                   ConfiguredShadingModel.c_str(),
                                   RequiredShadingModel.c_str());
        Property(PropertyNames::MaterialsShadingModel, RequiredShadingModel);
    }

    SnAPI::Graphics::GBufferPass::CreateShaderProgram();

    const std::string Module = std::get<std::string>(
        Property(PropertyNames::MaterialsModule).value_or(std::string{"GFDefaultEditorIDMaterial"}));

    const bool NeedsRebuild =
        !m_editorIDMaterialInstance
        || !m_editorIDMaterial
        || m_editorIDMaterial->ShaderModuleName() != Module
        || m_editorIDMaterial->ShadingModelName() != RequiredShadingModel;
    if (!NeedsRebuild)
    {
        return;
    }

    m_editorIDMaterial = std::make_shared<GFEditorIDMaterial>(Module);
    m_editorIDMaterial->BakeAndCompile();
    if (!m_editorIDMaterial || !m_editorIDMaterial->ShaderVariant())
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorIDPass: failed to build editor id material variant");
        m_editorIDMaterial.reset();
        m_editorIDMaterialInstance.reset();
        return;
    }

    m_editorIDMaterialInstance = m_editorIDMaterial->CreateMaterialInstance();
    if (!m_editorIDMaterialInstance)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorIDPass: failed to create material instance");
        m_editorIDMaterial.reset();
    }
}

void GFEditorIDPass::RenderScene(
    const SnAPI::Graphics::PassContext& Context,
    const std::vector<std::shared_ptr<SnAPI::Graphics::IRenderObject>>& RenderObjects)
{
    SNAPI_RENDERER_PROFILE_SCOPE("Renderer.GFEditorID.RenderScene");
    if (!m_editorIDMaterialInstance)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorIDPass: missing material instance, skipping draw");
        return;
    }

    const auto* CommandBuffer = static_cast<const SnAPI::Graphics::VulkanCommandBuffer*>(Context.GfxCommandBuffer);
    auto* pCommandBuffer = const_cast<SnAPI::Graphics::VulkanCommandBuffer*>(CommandBuffer);
    SNAPI_RENDERER_DEBUG_ASSERT_MSG(pCommandBuffer, "Command buffer is null");

    const auto* ActiveCamera = Context.pActiveCamera;
    if (!ActiveCamera)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorIDPass: no active camera, skipping scene rendering");
        return;
    }

    const auto Variant = m_editorIDMaterialInstance->ShaderVariant().lock();
    if (!Variant)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorIDPass: material variant is null, skipping draw");
        return;
    }

    const auto ViewNoRotation = ActiveCamera->ViewNoRotation();
    const auto PrevViewNoRotation = ActiveCamera->PreviousViewNoRotation();

    auto* VertexFactory = SnAPI::Graphics::VertexFactory::Instance();
    SNAPI_RENDERER_DEBUG_ASSERT_MSG(VertexFactory, "VertexFactory is null");

    auto* GraphicsAPI = static_cast<SnAPI::Graphics::VulkanGraphicsAPI*>(SnAPI::Graphics::IGraphicsAPI::Instance());
    SNAPI_RENDERER_DEBUG_ASSERT_MSG(GraphicsAPI, "Graphics API is null");
    if (!CurrentCameraPerFrameUBO())
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorIDPass: camera per-frame UBO is null, skipping draw");
        return;
    }
    if (!m_RenderPass)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorIDPass: render pass is null, skipping draw");
        return;
    }

    std::unordered_map<GFEditorIDSourceCacheKey,
                       std::shared_ptr<SnAPI::Graphics::TVertexDataFor<SnAPI::Graphics::GBufferVertexContract>>,
                       GFEditorIDSourceCacheKeyHasher>
        VertexDataCache{};

    const auto ResolveVertexData =
        [&](const SnAPI::Graphics::SharedVertexStreamSourcePtr& Source)
        -> std::shared_ptr<SnAPI::Graphics::TVertexDataFor<SnAPI::Graphics::GBufferVertexContract>>
    {
        if (!Source)
        {
            return {};
        }

        const GFEditorIDSourceCacheKey Key{
            .SourceID = Source->SourceID(),
            .SourceRevision = Source->SourceRevision(),
        };

        if (const auto It = VertexDataCache.find(Key); It != VertexDataCache.end())
        {
            return It->second;
        }

        auto VertexData = VertexFactory->GetOrCreateVertexData<SnAPI::Graphics::GBufferVertexContract>(Source);
        if (VertexData)
        {
            VertexDataCache.emplace(Key, VertexData);
        }
        return VertexData;
    };

    SnAPI::Graphics::Pipeline* CurrentPipeline = nullptr;

    for (std::size_t MeshIndex = 0; MeshIndex < RenderObjects.size(); ++MeshIndex)
    {
        const auto& RenderObject = RenderObjects[MeshIndex];
        if (!RenderObject)
        {
            continue;
        }

        const auto& Source = RenderObject->VertexStreamSource();
        if (!Source)
        {
            continue;
        }
        if (!RenderObject->ProvidesStreams(SnAPI::Graphics::GBufferVertexContract::RequiredStreams))
        {
            continue;
        }

        const auto VertexData = ResolveVertexData(Source);
        if (!VertexData)
        {
            continue;
        }

        pCommandBuffer->BindVertexBuffers(VertexData->GetBindingBuffers(), VertexData->GetBindingOffsets());

        for (std::size_t SubMeshIndex = 0; SubMeshIndex < Source->SubMeshCount(); ++SubMeshIndex)
        {
            if (MeshIndex < m_InstanceVisibility.size()
                && SubMeshIndex < m_InstanceVisibility[MeshIndex].size()
                && !m_InstanceVisibility[MeshIndex][SubMeshIndex])
            {
                continue;
            }

            auto* Pipeline = Variant->GetOrCreatePipeline(m_RenderPass.get(), VertexInputLayout(), m_editorIDMaterial->PipelineState());
            if (!Pipeline)
            {
                continue;
            }

            if (CurrentPipeline != Pipeline)
            {
                Pipeline->Bind(pCommandBuffer, SnAPI::Graphics::EPipelineBindPoint::Graphics);
                CurrentPipeline = Pipeline;
            }

            const auto RigidPartTransform = RenderObject->GlobalTransform(static_cast<std::uint32_t>(SubMeshIndex));
            const auto PrevRigidPartTransform = RenderObject->PreviousGlobalTransform(static_cast<std::uint32_t>(SubMeshIndex));

            GFEditorIDCameraPerObject PushConstantData{};
            PushConstantData.Model = (ViewNoRotation * RigidPartTransform).cast<float>();
            PushConstantData.PrevModel = (PrevViewNoRotation * PrevRigidPartTransform).cast<float>();
            PushConstantData.InstanceOffset = 0u;
            PushConstantData.ObjectID = GraphicsAPI->RenderObjectID(RenderObject.get()).value_or(0u);
            const std::uint32_t AxisTag =
                EditorImmediateAxisTag(RenderObject.get(), SnAPI::Graphics::ERenderPassType::EditorID);
            PushConstantData.Flags = (AxisTag != 0u)
                ? static_cast<std::uint32_t>(EEditorIDObjectFlags::Gizmo)
                : static_cast<std::uint32_t>(EEditorIDObjectFlags::None);

            m_editorIDMaterialInstance->Buffer("GFEditorID_CameraData", CurrentCameraPerFrameUBO());
            m_editorIDMaterialInstance->PushConstant(&PushConstantData, sizeof(GFEditorIDCameraPerObject), 0);
            m_editorIDMaterialInstance->Commit();
            m_editorIDMaterialInstance->Bind(pCommandBuffer, Variant->PipelineLayout());

            SnAPI::Graphics::VertexSourceSubMesh SubMesh{};
            if (!Source->SubMesh(static_cast<std::uint32_t>(SubMeshIndex), SubMesh))
            {
                continue;
            }

            pCommandBuffer->BindIndexBuffer(VertexData->IndexBuffer().get(), 0, SnAPI::Graphics::EIndexType::UInt32);
            pCommandBuffer->DrawIndexed(SubMesh.IndexCount, 1, SubMesh.IndexOffset, 0, 0);
        }
    }
}

GFEditorOverlayPass::GFEditorOverlayPass(SnAPI::Graphics::PassProperties&& Properties)
    : SnAPI::Graphics::GBufferPass(std::move(Properties))
{
    Property(PropertyNames::MaterialsShadingModel, std::string{GFEditorOverlayContract::ShadingModelModuleName});

    if (!Property(PropertyNames::MaterialsModule).has_value())
    {
        Property(PropertyNames::MaterialsModule, std::string{"GFDefaultEditorOverlayMaterial"});
    }
}

void GFEditorOverlayPass::CreateShaderProgram()
{
    const std::string RequiredShadingModel{GFEditorOverlayContract::ShadingModelModuleName};
    const std::string ConfiguredShadingModel = std::get<std::string>(
        Property(PropertyNames::MaterialsShadingModel).value_or(RequiredShadingModel));
    if (ConfiguredShadingModel != RequiredShadingModel)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorOverlayPass: shading model override '%s' is not supported; using '%s'",
                                   ConfiguredShadingModel.c_str(),
                                   RequiredShadingModel.c_str());
        Property(PropertyNames::MaterialsShadingModel, RequiredShadingModel);
    }

    SnAPI::Graphics::GBufferPass::CreateShaderProgram();

    const std::string Module = std::get<std::string>(
        Property(PropertyNames::MaterialsModule).value_or(std::string{"GFDefaultEditorOverlayMaterial"}));

    const bool NeedsRebuild =
        !m_editorOverlayMaterialInstance
        || !m_editorOverlayMaterial
        || !m_editorOverlayGizmoMaterialInstance
        || !m_editorOverlayGizmoMaterial
        || m_editorOverlayMaterial->ShaderModuleName() != Module
        || m_editorOverlayMaterial->ShadingModelName() != RequiredShadingModel
        || m_editorOverlayGizmoMaterial->ShaderModuleName() != Module
        || m_editorOverlayGizmoMaterial->ShadingModelName() != RequiredShadingModel;
    if (!NeedsRebuild)
    {
        return;
    }

    m_editorOverlayMaterial = std::make_shared<GFEditorOverlayMaterial>(Module);
    m_editorOverlayMaterial->SetFeature(GFEditorOverlayContract::Feature::SceneDepth, true);
    m_editorOverlayMaterial->BakeAndCompile();
    m_editorOverlayGizmoMaterial = std::make_shared<GFEditorOverlayMaterial>(Module);
    m_editorOverlayGizmoMaterial->SetFeature(GFEditorOverlayContract::Feature::SceneDepth, false);
    m_editorOverlayGizmoMaterial->BakeAndCompile();
    if (!m_editorOverlayMaterial || !m_editorOverlayMaterial->ShaderVariant()
        || !m_editorOverlayGizmoMaterial || !m_editorOverlayGizmoMaterial->ShaderVariant())
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorOverlayPass: failed to build editor overlay material variant");
        m_editorOverlayMaterial.reset();
        m_editorOverlayGizmoMaterial.reset();
        m_editorOverlayMaterialInstance.reset();
        m_editorOverlayGizmoMaterialInstance.reset();
        return;
    }

    m_editorOverlayMaterialInstance = m_editorOverlayMaterial->CreateMaterialInstance();
    m_editorOverlayGizmoMaterialInstance = m_editorOverlayGizmoMaterial->CreateMaterialInstance();
    if (!m_editorOverlayMaterialInstance || !m_editorOverlayGizmoMaterialInstance)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorOverlayPass: failed to create material instance");
        m_editorOverlayMaterial.reset();
        m_editorOverlayGizmoMaterial.reset();
        m_editorOverlayMaterialInstance.reset();
        m_editorOverlayGizmoMaterialInstance.reset();
    }
}

std::vector<SnAPI::Graphics::PassResource>
GFEditorOverlayPass::DescribeReadResources(const SnAPI::Graphics::PassContext& Context) const
{
    auto Resources = SnAPI::Graphics::GBufferPass::DescribeReadResources(Context);

    const auto Depth = std::get<SnAPI::Graphics::DepthConfig>(
        Property(PropertyNames::PassDepthConfig).value_or(SnAPI::Graphics::DepthConfig{}));
    if (!Depth.SampleDepth || Depth.ReadResourceName.empty())
    {
        return Resources;
    }

    const bool AlreadyDeclared = std::ranges::any_of(
        Resources,
        [&Depth](const SnAPI::Graphics::PassResource& Resource)
        {
            return Resource.Name == Depth.ReadResourceName;
        });
    if (AlreadyDeclared)
    {
        return Resources;
    }

    SnAPI::Graphics::PassResource Resource{};
    Resource.Name = Depth.ReadResourceName;
    Resource.InitialLayout = Depth.ReadLayout;
    Resource.DstStageMask = SnAPI::Graphics::EPipelineStageFlagBits::FragmentShader;
    Resource.DstAccessMask = SnAPI::Graphics::EAccessFlagBits::ShaderRead;
    Resources.push_back(std::move(Resource));
    return Resources;
}

void GFEditorOverlayPass::RenderScene(
    const SnAPI::Graphics::PassContext& Context,
    const std::vector<std::shared_ptr<SnAPI::Graphics::IRenderObject>>& RenderObjects)
{
    SNAPI_RENDERER_PROFILE_SCOPE("Renderer.GFEditorOverlay.RenderScene");
    if (!m_editorOverlayMaterialInstance || !m_editorOverlayGizmoMaterialInstance)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorOverlayPass: missing material instance, skipping draw");
        return;
    }

    const auto* CommandBuffer = static_cast<const SnAPI::Graphics::VulkanCommandBuffer*>(Context.GfxCommandBuffer);
    auto* pCommandBuffer = const_cast<SnAPI::Graphics::VulkanCommandBuffer*>(CommandBuffer);
    SNAPI_RENDERER_DEBUG_ASSERT_MSG(pCommandBuffer, "Command buffer is null");

    const auto* ActiveCamera = Context.pActiveCamera;
    if (!ActiveCamera)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorOverlayPass: no active camera, skipping scene rendering");
        return;
    }

    const auto OverlayVariant = m_editorOverlayMaterialInstance->ShaderVariant().lock();
    const auto GizmoVariant = m_editorOverlayGizmoMaterialInstance->ShaderVariant().lock();
    if (!OverlayVariant || !GizmoVariant)
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorOverlayPass: material variant is null, skipping draw");
        return;
    }

    const auto ViewNoRotation = ActiveCamera->ViewNoRotation();
    const auto PrevViewNoRotation = ActiveCamera->PreviousViewNoRotation();
    if (!CurrentCameraPerFrameUBO())
    {
        SNAPI_RENDERER_LOG_WARNING("GFEditorOverlayPass: camera per-frame UBO is null, skipping draw");
        return;
    }

    auto* VertexFactory = SnAPI::Graphics::VertexFactory::Instance();
    SNAPI_RENDERER_DEBUG_ASSERT_MSG(VertexFactory, "VertexFactory is null");

    for (const auto& Resource : ReadResources())
    {
        if (auto* pImage = std::get_if<const SnAPI::Graphics::IGPUImage*>(&Resource.Resource))
        {
            if (!*pImage)
            {
                continue;
            }

            const std::string& BindingName = Resource.BindingName.empty() ? Resource.Name : Resource.BindingName;
            const auto Layout =
                (Resource.InitialLayout == SnAPI::Graphics::EImageLayout::Undefined)
                    ? SnAPI::Graphics::EImageLayout::ShaderReadOnlyOptimal
                    : Resource.InitialLayout;
            m_editorOverlayMaterialInstance->Texture(BindingName, const_cast<SnAPI::Graphics::IGPUImage*>(*pImage), Layout);
            m_editorOverlayGizmoMaterialInstance->Texture(BindingName, const_cast<SnAPI::Graphics::IGPUImage*>(*pImage), Layout);
        }
    }

    std::unordered_map<GFEditorOverlaySourceCacheKey,
                       std::shared_ptr<SnAPI::Graphics::TVertexDataFor<SnAPI::Graphics::GBufferVertexContract>>,
                       GFEditorOverlaySourceCacheKeyHasher>
        VertexDataCache{};

    const auto ResolveVertexData =
        [&](const SnAPI::Graphics::SharedVertexStreamSourcePtr& Source)
        -> std::shared_ptr<SnAPI::Graphics::TVertexDataFor<SnAPI::Graphics::GBufferVertexContract>>
    {
        if (!Source)
        {
            return {};
        }

        const GFEditorOverlaySourceCacheKey Key{
            .SourceID = Source->SourceID(),
            .SourceRevision = Source->SourceRevision(),
        };

        if (const auto It = VertexDataCache.find(Key); It != VertexDataCache.end())
        {
            return It->second;
        }

        auto VertexData = VertexFactory->GetOrCreateVertexData<SnAPI::Graphics::GBufferVertexContract>(Source);
        if (VertexData)
        {
            VertexDataCache.emplace(Key, VertexData);
        }
        return VertexData;
    };

    SnAPI::Graphics::Pipeline* CurrentPipeline = nullptr;

    for (std::size_t MeshIndex = 0; MeshIndex < RenderObjects.size(); ++MeshIndex)
    {
        const auto& RenderObject = RenderObjects[MeshIndex];
        if (!RenderObject)
        {
            continue;
        }

        const auto& Source = RenderObject->VertexStreamSource();
        if (!Source)
        {
            continue;
        }
        if (!RenderObject->ProvidesStreams(SnAPI::Graphics::GBufferVertexContract::RequiredStreams))
        {
            continue;
        }

        const auto VertexData = ResolveVertexData(Source);
        if (!VertexData)
        {
            continue;
        }

        pCommandBuffer->BindVertexBuffers(VertexData->GetBindingBuffers(), VertexData->GetBindingOffsets());

        for (std::size_t SubMeshIndex = 0; SubMeshIndex < Source->SubMeshCount(); ++SubMeshIndex)
        {
            if (MeshIndex < m_InstanceVisibility.size()
                && SubMeshIndex < m_InstanceVisibility[MeshIndex].size()
                && !m_InstanceVisibility[MeshIndex][SubMeshIndex])
            {
                continue;
            }

            const std::uint32_t AxisTag =
                EditorImmediateAxisTag(RenderObject.get(), SnAPI::Graphics::ERenderPassType::EditorOverlay);
            const bool IsGizmo = AxisTag != 0u;

            auto& ActiveMaterial = IsGizmo ? m_editorOverlayGizmoMaterial : m_editorOverlayMaterial;
            auto& ActiveMaterialInstance = IsGizmo ? m_editorOverlayGizmoMaterialInstance : m_editorOverlayMaterialInstance;
            auto& ActiveVariant = IsGizmo ? GizmoVariant : OverlayVariant;

            auto* Pipeline = ActiveVariant->GetOrCreatePipeline(m_RenderPass.get(), VertexInputLayout(), ActiveMaterial->PipelineState());
            if (!Pipeline)
            {
                continue;
            }

            if (CurrentPipeline != Pipeline)
            {
                Pipeline->Bind(pCommandBuffer, SnAPI::Graphics::EPipelineBindPoint::Graphics);
                CurrentPipeline = Pipeline;
            }

            const auto RigidPartTransform = RenderObject->GlobalTransform(static_cast<std::uint32_t>(SubMeshIndex));
            const auto PrevRigidPartTransform = RenderObject->PreviousGlobalTransform(static_cast<std::uint32_t>(SubMeshIndex));

            GFEditorOverlayCameraPerObject PushConstantData{};
            PushConstantData.Model = (ViewNoRotation * RigidPartTransform).cast<float>();
            PushConstantData.PrevModel = (PrevViewNoRotation * PrevRigidPartTransform).cast<float>();
            PushConstantData.InstanceOffset = 0u;
            PushConstantData.AxisTag = AxisTag;

            ActiveMaterialInstance->Buffer("GFEditorOverlay_CameraData", CurrentCameraPerFrameUBO());
            ActiveMaterialInstance->PushConstant(&PushConstantData, sizeof(GFEditorOverlayCameraPerObject), 0);
            ActiveMaterialInstance->Commit();
            ActiveMaterialInstance->Bind(pCommandBuffer, ActiveVariant->PipelineLayout());

            SnAPI::Graphics::VertexSourceSubMesh SubMesh{};
            if (!Source->SubMesh(static_cast<std::uint32_t>(SubMeshIndex), SubMesh))
            {
                continue;
            }

            pCommandBuffer->BindIndexBuffer(VertexData->IndexBuffer().get(), 0, SnAPI::Graphics::EIndexType::UInt32);
            pCommandBuffer->DrawIndexed(SubMesh.IndexCount, 1, SubMesh.IndexOffset, 0, 0);
        }
    }
}

} // namespace SnAPI::GameFramework::Editor

#else

namespace SnAPI::GameFramework::Editor
{
void SetEditorImmediateRenderObjectMetadata(const SnAPI::Graphics::IRenderObject* RenderObject,
                                            SnAPI::Graphics::ERenderPassType PassType,
                                            bool IsGizmo,
                                            std::uint32_t AxisTag)
{
    (void)RenderObject;
    (void)PassType;
    (void)IsGizmo;
    (void)AxisTag;
}

void RemoveEditorImmediateRenderObjectMetadata(const SnAPI::Graphics::IRenderObject* RenderObject,
                                               SnAPI::Graphics::ERenderPassType PassType)
{
    (void)RenderObject;
    (void)PassType;
}

void ClearEditorImmediateRenderObjectMetadata()
{
}

std::uint32_t EditorImmediateAxisTag(const SnAPI::Graphics::IRenderObject* RenderObject,
                                     SnAPI::Graphics::ERenderPassType PassType)
{
    (void)RenderObject;
    (void)PassType;
    return 0u;
}

bool IsEditorImmediateGizmoRenderObject(const SnAPI::Graphics::IRenderObject* RenderObject,
                                        SnAPI::Graphics::ERenderPassType PassType)
{
    (void)RenderObject;
    (void)PassType;
    return false;
}
} // namespace SnAPI::GameFramework::Editor

#endif

#endif
