#include "RendererSystem.h"
#include "GameThreading.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "Profiling.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <utility>

#include <PCH.hpp>
#include <AtmospherePass.hpp>
#include <BloomPass.hpp>
#include <CompositePass.hpp>
#include <DeferredShadingPass.hpp>
#include <EnvironmentProbe.hpp>
#include <GBufferPass.hpp>
#include <HiZPass.hpp>
#include <ICamera.hpp>
#include <IGraphicsAPI.hpp>
#include <IRenderObject.hpp>
#include <LightManager.hpp>
#include <MeshManager.hpp>
#include <FontLibrary.hpp>
#include <PassVariants.hpp>
#include <PresentPass.hpp>
#include <SDLWindow.hpp>
#include <SSAOPass.hpp>
#include <SSRPass.hpp>
#include <ShadowPass.hpp>
#include <TLightFor.hpp>
#include <TMaterialFor.hpp>
#include <ToneMapPass.hpp>
#include <UIPass.hpp>
#include <VulkanGraphicsAPI.hpp>
#include <WindowBase.hpp>

#if defined(SNAPI_GF_ENABLE_UI)
#include <UIContext.h>
#include <UIRenderPackets.h>
#endif

namespace SnAPI::GameFramework
{
namespace
{
constexpr float kMinWindowExtent = 1.0f;
constexpr float kWindowSizeEpsilon = 0.5f;
constexpr std::size_t kMaxQueuedTextRequests = 256;
#if defined(SNAPI_GF_ENABLE_UI)
constexpr float kUiGlobalZBase = 100.0f;
constexpr float kUiGlobalZStep = 0.0001f;
#endif

float ClampWindowExtent(const float Value)
{
    return std::max(kMinWindowExtent, Value);
}

bool IsFontRenderable(SnAPI::Graphics::FontFace* Face)
{
    if (!Face || !Face->Valid())
    {
        return false;
    }

    if (Face->Atlas().GraphicsImage())
    {
        return true;
    }

    // Retry atlas generation in case the font was loaded before UI resources were fully ready.
    Face->GenerateAtlas();
    return Face->Atlas().GraphicsImage() != nullptr;
}

SnAPI::Graphics::WindowCreateInfo BuildWindowCreateInfo(const RendererBootstrapSettings& Settings)
{
    SnAPI::Graphics::WindowCreateInfo CreateInfo{};
    CreateInfo.Title = Settings.WindowTitle.empty() ? "SnAPI.GameFramework" : Settings.WindowTitle;
    CreateInfo.Size = {ClampWindowExtent(Settings.WindowWidth), ClampWindowExtent(Settings.WindowHeight)};
    CreateInfo.bFullScreen = Settings.FullScreen;
    CreateInfo.bResizable = Settings.Resizable;
    CreateInfo.bBorderless = Settings.Borderless;
    CreateInfo.bVisible = Settings.Visible;
    CreateInfo.bMaximized = Settings.Maximized;
    CreateInfo.bMinimized = Settings.Minimized;
    CreateInfo.bCloseable = Settings.Closeable;
    CreateInfo.bUseVulkan = true;
    CreateInfo.bUseOpenGL = false;
    CreateInfo.bAllowTransparency = Settings.AllowTransparency;
    return CreateInfo;
}
} // namespace

void RendererSystem::WindowDeleter::operator()(SnAPI::Graphics::WindowBase* Window) const
{
    delete Window;
}

void RendererSystem::LightManagerDeleter::operator()(SnAPI::Graphics::LightManager* Manager) const
{
    delete Manager;
}

RendererSystem::~RendererSystem()
{
    Shutdown();
}

RendererSystem::RendererSystem(RendererSystem&& Other) noexcept
{
    GameLockGuard Lock(Other.m_mutex);
    m_settings = std::move(Other.m_settings);
    m_graphics = Other.m_graphics;
    m_window = std::move(Other.m_window);
    m_lightManager = std::move(Other.m_lightManager);
    m_ssaoPass = Other.m_ssaoPass;
    m_ssrPass = Other.m_ssrPass;
    m_bloomPass = Other.m_bloomPass;
    m_gbufferPass = Other.m_gbufferPass;
    m_passGraphRegistered = Other.m_passGraphRegistered;
    m_defaultGBufferMaterial = std::move(Other.m_defaultGBufferMaterial);
    m_defaultShadowMaterial = std::move(Other.m_defaultShadowMaterial);
    m_defaultFont = Other.m_defaultFont;
    m_textQueue = std::move(Other.m_textQueue);
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiMaterial = std::move(Other.m_uiMaterial);
    m_uiFontMaterial = std::move(Other.m_uiFontMaterial);
    m_uiFallbackTexture = std::move(Other.m_uiFallbackTexture);
    m_uiFallbackMaterialInstance = std::move(Other.m_uiFallbackMaterialInstance);
    m_uiFontMaterialInstances = std::move(Other.m_uiFontMaterialInstances);
    m_uiTextures = std::move(Other.m_uiTextures);
    m_uiTextureMaterialInstances = std::move(Other.m_uiTextureMaterialInstances);
    m_uiPendingTextureUploads = std::move(Other.m_uiPendingTextureUploads);
    m_uiQueuedRects = std::move(Other.m_uiQueuedRects);
#endif
    m_lastWindowWidth = Other.m_lastWindowWidth;
    m_lastWindowHeight = Other.m_lastWindowHeight;
    m_hasWindowSizeSnapshot = Other.m_hasWindowSizeSnapshot;
    m_registeredRenderObjects = std::move(Other.m_registeredRenderObjects);
    m_initialized = Other.m_initialized;

    Other.m_graphics = nullptr;
    Other.ResetPassPointers();
    Other.m_passGraphRegistered = false;
    Other.m_defaultFont = nullptr;
    Other.m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    Other.m_uiMaterial.reset();
    Other.m_uiFontMaterial.reset();
    Other.m_uiFallbackTexture.reset();
    Other.m_uiFallbackMaterialInstance.reset();
    Other.m_uiFontMaterialInstances.clear();
    Other.m_uiTextures.clear();
    Other.m_uiTextureMaterialInstances.clear();
    Other.m_uiPendingTextureUploads.clear();
    Other.m_uiQueuedRects.clear();
#endif
    Other.m_lastWindowWidth = 0.0f;
    Other.m_lastWindowHeight = 0.0f;
    Other.m_hasWindowSizeSnapshot = false;
    Other.m_registeredRenderObjects.clear();
    Other.m_initialized = false;
}

RendererSystem& RendererSystem::operator=(RendererSystem&& Other) noexcept
{
    if (this == &Other)
    {
        return *this;
    }

    Shutdown();

    std::scoped_lock Lock(m_mutex, Other.m_mutex);
    m_settings = std::move(Other.m_settings);
    m_graphics = Other.m_graphics;
    m_window = std::move(Other.m_window);
    m_lightManager = std::move(Other.m_lightManager);
    m_ssaoPass = Other.m_ssaoPass;
    m_ssrPass = Other.m_ssrPass;
    m_bloomPass = Other.m_bloomPass;
    m_gbufferPass = Other.m_gbufferPass;
    m_passGraphRegistered = Other.m_passGraphRegistered;
    m_defaultGBufferMaterial = std::move(Other.m_defaultGBufferMaterial);
    m_defaultShadowMaterial = std::move(Other.m_defaultShadowMaterial);
    m_defaultFont = Other.m_defaultFont;
    m_textQueue = std::move(Other.m_textQueue);
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiMaterial = std::move(Other.m_uiMaterial);
    m_uiFontMaterial = std::move(Other.m_uiFontMaterial);
    m_uiFallbackTexture = std::move(Other.m_uiFallbackTexture);
    m_uiFallbackMaterialInstance = std::move(Other.m_uiFallbackMaterialInstance);
    m_uiFontMaterialInstances = std::move(Other.m_uiFontMaterialInstances);
    m_uiTextures = std::move(Other.m_uiTextures);
    m_uiTextureMaterialInstances = std::move(Other.m_uiTextureMaterialInstances);
    m_uiPendingTextureUploads = std::move(Other.m_uiPendingTextureUploads);
    m_uiQueuedRects = std::move(Other.m_uiQueuedRects);
#endif
    m_lastWindowWidth = Other.m_lastWindowWidth;
    m_lastWindowHeight = Other.m_lastWindowHeight;
    m_hasWindowSizeSnapshot = Other.m_hasWindowSizeSnapshot;
    m_registeredRenderObjects = std::move(Other.m_registeredRenderObjects);
    m_initialized = Other.m_initialized;

    Other.m_graphics = nullptr;
    Other.ResetPassPointers();
    Other.m_passGraphRegistered = false;
    Other.m_defaultFont = nullptr;
    Other.m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    Other.m_uiMaterial.reset();
    Other.m_uiFontMaterial.reset();
    Other.m_uiFallbackTexture.reset();
    Other.m_uiFallbackMaterialInstance.reset();
    Other.m_uiFontMaterialInstances.clear();
    Other.m_uiTextures.clear();
    Other.m_uiTextureMaterialInstances.clear();
    Other.m_uiPendingTextureUploads.clear();
    Other.m_uiQueuedRects.clear();
#endif
    Other.m_lastWindowWidth = 0.0f;
    Other.m_lastWindowHeight = 0.0f;
    Other.m_hasWindowSizeSnapshot = false;
    Other.m_registeredRenderObjects.clear();
    Other.m_initialized = false;
    return *this;
}

TaskHandle RendererSystem::EnqueueTask(WorkTask InTask, CompletionTask OnComplete)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    return m_taskQueue.EnqueueTask(std::move(InTask), std::move(OnComplete));
}

void RendererSystem::EnqueueThreadTask(std::function<void()> InTask)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    m_taskQueue.EnqueueThreadTask(std::move(InTask));
}

void RendererSystem::ExecuteQueuedTasks()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    m_taskQueue.ExecuteQueuedTasks(*this, m_mutex);
}

bool RendererSystem::Initialize()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    return Initialize(RendererBootstrapSettings{});
}

bool RendererSystem::Initialize(const RendererBootstrapSettings& Settings)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    if (m_initialized && m_graphics)
    {
        return true;
    }

    m_settings = Settings;

    auto ResetState = [this]() {
        m_defaultGBufferMaterial.reset();
        m_defaultShadowMaterial.reset();
        m_defaultFont = nullptr;
        m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
        m_uiMaterial.reset();
        m_uiFontMaterial.reset();
        m_uiFallbackTexture.reset();
        m_uiFallbackMaterialInstance.reset();
        m_uiFontMaterialInstances.clear();
        m_uiTextures.clear();
        m_uiTextureMaterialInstances.clear();
        m_uiPendingTextureUploads.clear();
        m_uiQueuedRects.clear();
#endif
        m_window.reset();
        m_lightManager.reset();
        ResetPassPointers();
        m_passGraphRegistered = false;
        m_lastWindowWidth = 0.0f;
        m_lastWindowHeight = 0.0f;
        m_hasWindowSizeSnapshot = false;
        m_registeredRenderObjects.clear();
    };

    ResetState();

    try
    {
        if (InitializeUnlocked())
        {
            return true;
        }
    }
    catch (const std::exception& Ex)
    {
        SNAPI_RENDERER_LOG_WARNING("Renderer initialization failed: %s", Ex.what());
    }

    ShutdownUnlocked();

    if (!m_settings.AutoFallbackOnOutOfMemory)
    {
        return false;
    }

    ApplyOutOfMemoryFallbackSettings();
    ResetState();

    try
    {
        if (InitializeUnlocked())
        {
            return true;
        }
    }
    catch (const std::exception& Ex)
    {
        SNAPI_RENDERER_LOG_ERROR("Renderer fallback initialization failed: %s", Ex.what());
    }

    ShutdownUnlocked();
    return false;
}

bool RendererSystem::InitializeUnlocked()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    m_defaultGBufferMaterial.reset();
    m_defaultShadowMaterial.reset();
    m_defaultFont = nullptr;
    m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiMaterial.reset();
    m_uiFontMaterial.reset();
    m_uiFallbackTexture.reset();
    m_uiFallbackMaterialInstance.reset();
    m_uiFontMaterialInstances.clear();
    m_uiTextures.clear();
    m_uiTextureMaterialInstances.clear();
    m_uiPendingTextureUploads.clear();
    m_uiQueuedRects.clear();
#endif
    m_window.reset();
    m_lightManager.reset();
    ResetPassPointers();
    m_passGraphRegistered = false;
    m_registeredRenderObjects.clear();

    if (!m_settings.CreateGraphicsApi)
    {
        m_graphics = nullptr;
        m_initialized = false;
        return true;
    }

    {
        SNAPI_GF_PROFILE_SCOPE("Renderer.CreateGraphicsAPI", "Rendering");
        m_graphics = SnAPI::Graphics::CreateGraphicsAPI<SnAPI::Graphics::VulkanGraphicsAPI>();
    }
    m_initialized = (m_graphics != nullptr);
    if (!m_initialized)
    {
        return false;
    }

    if (m_settings.CreateWindow && !CreateWindowResources())
    {
        ShutdownUnlocked();
        return false;
    }

    if (m_settings.CreateDefaultLighting && !EnsureDefaultLighting())
    {
        ShutdownUnlocked();
        return false;
    }

    if (m_settings.RegisterDefaultPassGraph && !RegisterDefaultPassGraph())
    {
        ShutdownUnlocked();
        return false;
    }

    if (m_settings.CreateDefaultEnvironmentProbe && !EnsureDefaultEnvironmentProbe())
    {
        ShutdownUnlocked();
        return false;
    }

    if (m_settings.CreateDefaultMaterials && !EnsureDefaultMaterials())
    {
        ShutdownUnlocked();
        return false;
    }

    if (m_settings.PreloadDefaultFont)
    {
        (void)EnsureDefaultFont();
    }

    return true;
}

void RendererSystem::ApplyOutOfMemoryFallbackSettings()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    const float FallbackWidth = ClampWindowExtent(m_settings.OutOfMemoryFallbackWindowWidth);
    const float FallbackHeight = ClampWindowExtent(m_settings.OutOfMemoryFallbackWindowHeight);

    m_settings.WindowWidth = std::min(m_settings.WindowWidth, FallbackWidth);
    m_settings.WindowHeight = std::min(m_settings.WindowHeight, FallbackHeight);

    if (m_settings.ForceWindowedOnOutOfMemory)
    {
        m_settings.FullScreen = false;
        m_settings.Borderless = false;
    }

    if (m_settings.DisableTransparencyOnOutOfMemory)
    {
        m_settings.AllowTransparency = false;
    }

    if (m_settings.DisableExpensivePassesOnOutOfMemory)
    {
        m_settings.EnableSsao = false;
        m_settings.EnableSsr = false;
        m_settings.EnableBloom = false;
        m_settings.EnableAtmosphere = false;
    }

    if (m_settings.DisableEnvironmentProbeOnOutOfMemory)
    {
        m_settings.CreateDefaultEnvironmentProbe = false;
    }
}

void RendererSystem::Shutdown()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    ShutdownUnlocked();
}

bool RendererSystem::IsInitialized() const
{
    GameLockGuard Lock(m_mutex);
    return m_initialized && m_graphics != nullptr;
}

SnAPI::Graphics::VulkanGraphicsAPI* RendererSystem::Graphics()
{
    GameLockGuard Lock(m_mutex);
    return m_graphics;
}

const SnAPI::Graphics::VulkanGraphicsAPI* RendererSystem::Graphics() const
{
    GameLockGuard Lock(m_mutex);
    return m_graphics;
}

SnAPI::Graphics::WindowBase* RendererSystem::Window()
{
    GameLockGuard Lock(m_mutex);
    return m_window.get();
}

const SnAPI::Graphics::WindowBase* RendererSystem::Window() const
{
    GameLockGuard Lock(m_mutex);
    return m_window.get();
}

bool RendererSystem::HasOpenWindow() const
{
    GameLockGuard Lock(m_mutex);
    return m_window && m_window->IsOpen();
}

bool RendererSystem::SetActiveCamera(SnAPI::Graphics::ICamera* Camera)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return false;
    }
    m_graphics->ActiveCamera(Camera);
    return true;
}

SnAPI::Graphics::ICamera* RendererSystem::ActiveCamera() const
{
    GameLockGuard Lock(m_mutex);
    return m_graphics ? m_graphics->ActiveCamera() : nullptr;
}

bool RendererSystem::SetViewPort(const SnAPI::Graphics::ViewportFit& ViewPort)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return false;
    }
    m_graphics->SetViewPort(ViewPort);
    return true;
}

bool RendererSystem::ClearViewPort()
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return false;
    }
    m_graphics->ClearViewPort();
    return true;
}

bool RendererSystem::SetPassViewPort(const SnAPI::Graphics::ERenderPassType PassType, const SnAPI::Graphics::ViewportFit& ViewPort)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return false;
    }
    m_graphics->SetPassViewPort(PassType, ViewPort);
    return true;
}

bool RendererSystem::ClearPassViewPort(const SnAPI::Graphics::ERenderPassType PassType)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return false;
    }
    m_graphics->ClearPassViewPort(PassType);
    return true;
}

bool RendererSystem::ClearPassViewPorts()
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return false;
    }
    m_graphics->ClearPassViewPorts();
    return true;
}

bool RendererSystem::RegisterRenderObject(const std::weak_ptr<SnAPI::Graphics::IRenderObject>& RenderObject)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    const auto SharedRenderObject = RenderObject.lock();
    if (!m_graphics || !SharedRenderObject)
    {
        return false;
    }

    m_graphics->RegisterRenderObject(SharedRenderObject);

    const auto* RenderObjectPtr = SharedRenderObject.get();
    const bool AlreadyTracked = std::ranges::any_of(m_registeredRenderObjects,
                                                    [RenderObjectPtr](const std::weak_ptr<SnAPI::Graphics::IRenderObject>& Existing) {
                                                        const auto ExistingShared = Existing.lock();
                                                        return ExistingShared && ExistingShared.get() == RenderObjectPtr;
                                                    });
    if (!AlreadyTracked)
    {
        m_registeredRenderObjects.emplace_back(SharedRenderObject);
    }
    return true;
}

bool RendererSystem::ApplyDefaultMaterials(SnAPI::Graphics::IRenderObject& RenderObject)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || !EnsureDefaultMaterials())
    {
        return false;
    }

    auto* Meshes = SnAPI::Graphics::MeshManager::Instance();
    if (!Meshes)
    {
        return false;
    }

    Meshes->PopulateMaterialInstances(RenderObject, m_defaultGBufferMaterial);
    Meshes->PopulateShadowMaterialInstances(RenderObject, m_defaultShadowMaterial);
    return true;
}

std::shared_ptr<SnAPI::Graphics::Material> RendererSystem::DefaultGBufferMaterial()
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || !EnsureDefaultMaterials())
    {
        return {};
    }
    return m_defaultGBufferMaterial;
}

std::shared_ptr<SnAPI::Graphics::Material> RendererSystem::DefaultShadowMaterial()
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || !EnsureDefaultMaterials())
    {
        return {};
    }
    return m_defaultShadowMaterial;
}

bool RendererSystem::ConfigureRenderObjectPasses(SnAPI::Graphics::IRenderObject& RenderObject, const bool Visible, const bool CastShadows) const
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return false;
    }

    if (auto* GBufferPass = m_graphics->GetRenderPass(SnAPI::Graphics::ERenderPassType::GBuffer))
    {
        RenderObject.EnablePass(GBufferPass->ID(), Visible);
    }
    if (auto* ShadowPass = m_graphics->GetRenderPass(SnAPI::Graphics::ERenderPassType::Shadow))
    {
        RenderObject.EnablePass(ShadowPass->ID(), Visible && CastShadows);
    }
    RenderObject.SetCastsShadows(CastShadows);
    return true;
}

bool RendererSystem::RecreateSwapChain()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    return RecreateSwapChainForCurrentWindowUnlocked();
}

bool RendererSystem::LoadDefaultFont(const std::string& FontPath, const std::uint32_t FontSize)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return false;
    }

    auto* Library = SnAPI::Graphics::FontLibrary::Instance();
    if (!Library)
    {
        return false;
    }

    auto* Face = Library->FontFace(FontPath, FontSize);
    if (!IsFontRenderable(Face))
    {
        return false;
    }

    m_defaultFont = Face;
    return true;
}

bool RendererSystem::QueueText(std::string Text, const float X, const float Y)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || Text.empty())
    {
        return false;
    }

    m_textQueue.push_back(TextRequest{std::move(Text), X, Y});
    if (m_textQueue.size() > kMaxQueuedTextRequests)
    {
        m_textQueue.erase(m_textQueue.begin());
    }
    return true;
}

bool RendererSystem::HasDefaultFont() const
{
    GameLockGuard Lock(m_mutex);
    return IsFontRenderable(m_defaultFont);
}

SnAPI::Graphics::FontFace* RendererSystem::EnsureDefaultFontFace()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    if ((!m_defaultFont || !IsFontRenderable(m_defaultFont)) && !EnsureDefaultFont())
    {
        return nullptr;
    }
    return m_defaultFont;
}

#if defined(SNAPI_GF_ENABLE_UI)
bool RendererSystem::QueueUiRenderPackets(SnAPI::UI::UIContext& Context, const SnAPI::UI::RenderPacketList& Packets)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return false;
    }

    const auto PacketSpan = Packets.Packets();
    std::size_t TotalInstances = 0;
    for (const auto& Packet : PacketSpan)
    {
        TotalInstances += Packet.InstanceCount();
    }

    m_uiQueuedRects.clear();
    m_uiQueuedRects.reserve(TotalInstances);

    float GlobalZ = kUiGlobalZBase;

    const auto ApplyUiColor = [](QueuedUiRect& OutRect, const SnAPI::UI::Color& ColorValue) {
        constexpr float kInv = 1.0f / 255.0f;
        OutRect.R = static_cast<float>(ColorValue.R) * kInv;
        OutRect.G = static_cast<float>(ColorValue.G) * kInv;
        OutRect.B = static_cast<float>(ColorValue.B) * kInv;
        OutRect.A = static_cast<float>(ColorValue.A) * kInv;
    };

    const auto ApplyUiBorderColor = [](QueuedUiRect& OutRect, const SnAPI::UI::Color& ColorValue) {
        constexpr float kInv = 1.0f / 255.0f;
        OutRect.BorderR = static_cast<float>(ColorValue.R) * kInv;
        OutRect.BorderG = static_cast<float>(ColorValue.G) * kInv;
        OutRect.BorderB = static_cast<float>(ColorValue.B) * kInv;
        OutRect.BorderA = static_cast<float>(ColorValue.A) * kInv;
    };

    const auto NormalizeUiScissorMode = [](const SnAPI::UI::EScissorMode Mode) {
        switch (Mode)
        {
        case SnAPI::UI::EScissorMode::None:
        case SnAPI::UI::EScissorMode::Rect:
        case SnAPI::UI::EScissorMode::ClipAll:
            return Mode;
        default:
            return SnAPI::UI::EScissorMode::None;
        }
    };

    const auto ApplyUiScissor = [&](QueuedUiRect& OutRect, const SnAPI::UI::EScissorMode Mode, const SnAPI::UI::ScissorRect& Scissor) -> bool {
        OutRect.HasScissor = false;
        OutRect.ScissorMinX = 0.0f;
        OutRect.ScissorMinY = 0.0f;
        OutRect.ScissorMaxX = 0.0f;
        OutRect.ScissorMaxY = 0.0f;

        switch (NormalizeUiScissorMode(Mode))
        {
        case SnAPI::UI::EScissorMode::None:
            return true;
        case SnAPI::UI::EScissorMode::ClipAll:
            return false;
        case SnAPI::UI::EScissorMode::Rect:
            break;
        }

        if (Scissor.W <= 0 || Scissor.H <= 0)
        {
            return false;
        }

        OutRect.HasScissor = true;
        OutRect.ScissorMinX = static_cast<float>(Scissor.X);
        OutRect.ScissorMinY = static_cast<float>(Scissor.Y);
        OutRect.ScissorMaxX = static_cast<float>(Scissor.X + Scissor.W);
        OutRect.ScissorMaxY = static_cast<float>(Scissor.Y + Scissor.H);
        return true;
    };

    const auto QueueImageTextureUploadIfNeeded = [&](const std::uint32_t TextureIdValue) {
        if (TextureIdValue == 0 || m_uiTextures.contains(TextureIdValue) || m_uiPendingTextureUploads.contains(TextureIdValue))
        {
            return;
        }

        const auto* Image = Context.GetImageData(SnAPI::UI::TextureId{TextureIdValue});
        if (!Image || !Image->Valid || Image->Width <= 0 || Image->Height <= 0 || Image->Pixels.empty())
        {
            return;
        }

        auto& Pending = m_uiPendingTextureUploads[TextureIdValue];
        Pending.Width = static_cast<std::uint32_t>(Image->Width);
        Pending.Height = static_cast<std::uint32_t>(Image->Height);
        Pending.Pixels = Image->Pixels;
    };

    for (const auto& Packet : PacketSpan)
    {
        if (const auto* Rects = std::get_if<SnAPI::UI::RectInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Rects)
            {
                QueuedUiRect Rect{};
                Rect.X = Instance.X;
                Rect.Y = Instance.Y;
                Rect.W = Instance.W;
                Rect.H = Instance.H;
                Rect.CornerRadius = std::max(0.0f, Instance.CornerRadius);
                Rect.BorderThickness = std::max(0.0f, Instance.BorderThickness);
                Rect.GlobalZ = GlobalZ;
                ApplyUiColor(Rect, Instance.Fill);
                ApplyUiBorderColor(Rect, Instance.Border);
                if (!ApplyUiScissor(Rect, Instance.ScissorMode, Instance.Scissor))
                {
                    continue;
                }

                m_uiQueuedRects.emplace_back(Rect);
                GlobalZ += kUiGlobalZStep;
            }
            continue;
        }

        if (const auto* Images = std::get_if<SnAPI::UI::ImageInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Images)
            {
                QueuedUiRect Rect{};
                Rect.X = Instance.X;
                Rect.Y = Instance.Y;
                Rect.W = Instance.W;
                Rect.H = Instance.H;
                Rect.U0 = Instance.U0;
                Rect.V0 = Instance.V0;
                Rect.U1 = Instance.U1;
                Rect.V1 = Instance.V1;
                Rect.TextureId = Instance.Texture.Value;
                Rect.GlobalZ = GlobalZ;
                ApplyUiColor(Rect, Instance.Tint);
                if (!ApplyUiScissor(Rect, Instance.ScissorMode, Instance.Scissor))
                {
                    continue;
                }

                m_uiQueuedRects.emplace_back(Rect);
                GlobalZ += kUiGlobalZStep;
                QueueImageTextureUploadIfNeeded(Rect.TextureId);
            }
            continue;
        }

        if (const auto* Glyphs = std::get_if<SnAPI::UI::GlyphInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Glyphs)
            {
                QueuedUiRect Rect{};
                Rect.X = Instance.X;
                Rect.Y = Instance.Y;
                Rect.W = Instance.W;
                Rect.H = Instance.H;
                Rect.U0 = Instance.U0;
                Rect.V0 = Instance.V0;
                Rect.U1 = Instance.U1;
                Rect.V1 = Instance.V1;
                Rect.UseFontAtlas = true;
                Rect.GlobalZ = GlobalZ;
                ApplyUiColor(Rect, Instance.GlyphColor);
                if (!ApplyUiScissor(Rect, Instance.ScissorMode, Instance.Scissor))
                {
                    continue;
                }

                m_uiQueuedRects.emplace_back(Rect);
                GlobalZ += kUiGlobalZStep;
            }
        }
    }

    if (m_uiQueuedRects.size() > 131072)
    {
        m_uiQueuedRects.resize(131072);
    }

    return true;
}
#endif

void RendererSystem::EndFrame()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    TaskDispatcherScope DispatcherScope(*this);
    ExecuteQueuedTasks();
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return;
    }

    if (m_window && m_window->IsOpen())
    {
        if (m_settings.AutoHandleSwapChainResize)
        {
            SNAPI_GF_PROFILE_SCOPE("Renderer.HandleResize", "Rendering");
            (void)HandleWindowResizeIfNeeded();
        }

        bool BeganFrame = false;
        {
            SNAPI_GF_PROFILE_SCOPE("Renderer.BeginFrame", "Rendering");
            BeganFrame = m_graphics->BeginFrame(m_window.get());
        }

        if (BeganFrame)
        {
#if defined(SNAPI_GF_ENABLE_UI)
            {
                SNAPI_GF_PROFILE_SCOPE("Renderer.FlushQueuedUi", "Rendering");
                FlushQueuedUiPackets();
            }
#endif
            {
                SNAPI_GF_PROFILE_SCOPE("Renderer.FlushQueuedText", "Rendering");
                FlushQueuedText();
            }
            {
                SNAPI_GF_PROFILE_SCOPE("Renderer.EndFrame.Present", "Rendering");
                m_graphics->EndFrame(m_window.get());
            }
        }
    }

    if (auto* Camera = m_graphics->ActiveCamera())
    {
        SNAPI_GF_PROFILE_SCOPE("Renderer.SaveCameraFrameState", "Rendering");
        Camera->SaveFrameState();
    }

    SNAPI_GF_PROFILE_SCOPE("Renderer.SaveRenderObjectFrameState", "Rendering");
    for (auto It = m_registeredRenderObjects.begin(); It != m_registeredRenderObjects.end();)
    {
        if (auto RenderObject = It->lock())
        {
            RenderObject->SaveFrameState();
            ++It;
        }
        else
        {
            It = m_registeredRenderObjects.erase(It);
        }
    }
}

void RendererSystem::ShutdownUnlocked()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_graphics && m_window)
    {
        m_graphics->DestroyResourcesForWindow(m_window.get());
    }

    if (m_graphics)
    {
        m_graphics->ActiveCamera(nullptr);
        SnAPI::Graphics::DestroyGraphicsAPI();
    }

    m_graphics = nullptr;
    m_window.reset();
    m_lightManager.reset();
    ResetPassPointers();
    m_passGraphRegistered = false;
    m_defaultGBufferMaterial.reset();
    m_defaultShadowMaterial.reset();
    m_defaultFont = nullptr;
    m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiMaterial.reset();
    m_uiFontMaterial.reset();
    m_uiFallbackTexture.reset();
    m_uiFallbackMaterialInstance.reset();
    m_uiFontMaterialInstances.clear();
    m_uiTextures.clear();
    m_uiTextureMaterialInstances.clear();
    m_uiPendingTextureUploads.clear();
    m_uiQueuedRects.clear();
#endif
    m_lastWindowWidth = 0.0f;
    m_lastWindowHeight = 0.0f;
    m_hasWindowSizeSnapshot = false;
    m_registeredRenderObjects.clear();
    m_initialized = false;
}

bool RendererSystem::EnsureDefaultMaterials()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_defaultGBufferMaterial && m_defaultShadowMaterial)
    {
        return true;
    }
    if (!m_graphics)
    {
        return false;
    }

    auto GBufferMaterial = std::make_shared<SnAPI::Graphics::GBufferMaterial>("DefaultGBufferMaterial");
    GBufferMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlbedoMap, true);
    GBufferMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::NormalMap, false);
    GBufferMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::MetalnessMap, false);
    GBufferMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::RoughnessMap, false);
    GBufferMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::OcclusionMap, false);
    GBufferMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::DoubleSided, false);
    GBufferMaterial->BakeCompileTimeParams();

    auto ShadowMaterial = std::make_shared<SnAPI::Graphics::ShadowMaterial>("DefaultShadowMaterial");
    ShadowMaterial->BakeCompileTimeParams();

    m_defaultGBufferMaterial = std::move(GBufferMaterial);
    m_defaultShadowMaterial = std::move(ShadowMaterial);
    return true;
}

bool RendererSystem::EnsureDefaultLighting()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_lightManager)
    {
        return true;
    }

    m_lightManager.reset(new Graphics::LightManager());
    const auto Sun = m_lightManager->CreateDirectionalLight();
    if (!Sun)
    {
        m_lightManager.reset();
        return false;
    }

    Sun->SetDirection(SnAPI::Vector3DF(-0.5f, -1.0f, -0.3f).normalized());
    Sun->SetColor(SnAPI::ColorF::White());
    Sun->SetIntensity(1.0f);
    Sun->SetCastsShadows(true);
    Sun->SetCascadeCount(4);
    Sun->SetShadowMapSize(2048);
    Sun->SetShadowBias(0.005f);

    return true;
}

bool RendererSystem::EnsureDefaultEnvironmentProbe()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!m_graphics)
    {
        return false;
    }

    if (!m_settings.CreateDefaultEnvironmentProbe)
    {
        return true;
    }

    if (!m_graphics->EnvironmentProbes().empty())
    {
        return true;
    }

    auto Probe = std::make_unique<SnAPI::Graphics::EnvironmentProbe>();
    if (!Probe)
    {
        return false;
    }

    Probe->Position(SnAPI::Vector3D{
        static_cast<SnAPI::Vector3D::Scalar>(m_settings.DefaultEnvironmentProbeX),
        static_cast<SnAPI::Vector3D::Scalar>(m_settings.DefaultEnvironmentProbeY),
        static_cast<SnAPI::Vector3D::Scalar>(m_settings.DefaultEnvironmentProbeZ)});

    m_graphics->RegisterEnvironmentProbe(std::move(Probe));
    return true;
}

bool RendererSystem::EnsureDefaultFont()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!m_graphics)
    {
        return false;
    }

    if (IsFontRenderable(m_defaultFont))
    {
        return true;
    }

    auto* Library = SnAPI::Graphics::FontLibrary::Instance();
    if (!Library)
    {
        return false;
    }

    namespace fs = std::filesystem;
    const std::array<std::string, 4> FallbackPaths{
        m_settings.DefaultFontPath,
        "/usr/share/fonts/TTF/Arial.TTF",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf"};

    for (const auto& Candidate : FallbackPaths)
    {
        if (Candidate.empty())
        {
            continue;
        }

        std::error_code Ec;
        if (!fs::exists(Candidate, Ec) || Ec)
        {
            continue;
        }

        auto* Face = Library->FontFace(Candidate, m_settings.DefaultFontSize);
        if (IsFontRenderable(Face))
        {
            m_defaultFont = Face;
            return true;
        }
    }

    return false;
}

bool RendererSystem::HandleWindowResizeIfNeeded()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!m_graphics || !m_window || !m_window->IsOpen())
    {
        return false;
    }

    const auto WindowSize = m_window->Size();
    const float Width = WindowSize.x();
    const float Height = WindowSize.y();

    if (Width <= 0.0f || Height <= 0.0f)
    {
        return false;
    }

    if (!m_hasWindowSizeSnapshot)
    {
        m_lastWindowWidth = Width;
        m_lastWindowHeight = Height;
        m_hasWindowSizeSnapshot = true;
        return false;
    }

    if (std::fabs(Width - m_lastWindowWidth) <= kWindowSizeEpsilon
        && std::fabs(Height - m_lastWindowHeight) <= kWindowSizeEpsilon)
    {
        return false;
    }

    return RecreateSwapChainForCurrentWindowUnlocked();
}

void RendererSystem::FlushQueuedText()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_textQueue.empty())
    {
        return;
    }

    {
        SNAPI_GF_PROFILE_SCOPE("Renderer.FlushQueuedText.ResolveDefaultFont", "Rendering");
        if ((!m_defaultFont || !IsFontRenderable(m_defaultFont)) && !EnsureDefaultFont())
        {
            SNAPI_GF_PROFILE_SCOPE("Renderer.FlushQueuedText.FontUnavailableTrimQueue", "Rendering");
            // Keep only the latest request while font resources are unavailable.
            if (m_textQueue.size() > 1)
            {
                auto LastRequest = std::move(m_textQueue.back());
                m_textQueue.clear();
                m_textQueue.push_back(std::move(LastRequest));
            }
            return;
        }
    }

    {
        SNAPI_GF_PROFILE_SCOPE("Renderer.FlushQueuedText.DrawBatch", "Rendering");
        for (const auto& Request : m_textQueue)
        {
            SNAPI_GF_PROFILE_SCOPE("Renderer.FlushQueuedText.DrawText", "Rendering");
            m_graphics->DrawText(Request.Text.c_str(),
                                 *m_defaultFont,
                                 SnAPI::Point2D{
                                     static_cast<SnAPI::Point2D::Scalar>(Request.X),
                                     static_cast<SnAPI::Point2D::Scalar>(Request.Y)});
        }
    }

    {
        SNAPI_GF_PROFILE_SCOPE("Renderer.FlushQueuedText.ClearQueue", "Rendering");
        m_textQueue.clear();
    }
}

#if defined(SNAPI_GF_ENABLE_UI)
bool RendererSystem::EnsureUiMaterialResources()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!m_graphics)
    {
        return false;
    }

    if (!m_uiMaterial)
    {
        auto UiMaterial = std::make_shared<SnAPI::Graphics::UIMaterial>("DefaultUIMaterial");
        UiMaterial->BakeCompileTimeParams();
        m_uiMaterial = std::move(UiMaterial);
    }
    if (!m_uiFontMaterial)
    {
        auto UiFontMaterial = std::make_shared<SnAPI::Graphics::UIMaterial>("UIFontMaterial");
        UiFontMaterial->BakeCompileTimeParams();
        m_uiFontMaterial = std::move(UiFontMaterial);
    }

    if (!m_uiFallbackTexture)
    {
        SnAPI::Graphics::ImageCreateInfo ImageCI =
            SnAPI::Graphics::ImageCreateInfo::DataDefault(SnAPI::Size2DU{1u, 1u}, SnAPI::Graphics::ETextureFormat::R8G8B8A8_Unorm);
        ImageCI.Data = {255, 255, 255, 255};
        m_uiFallbackTexture = std::shared_ptr<SnAPI::Graphics::IGPUImage>(m_graphics->CreateImage2D(ImageCI).release());
        if (!m_uiFallbackTexture)
        {
            return false;
        }
    }

    if (!m_uiFallbackMaterialInstance)
    {
        m_uiFallbackMaterialInstance = m_uiMaterial->CreateMaterialInstance();
        if (!m_uiFallbackMaterialInstance)
        {
            return false;
        }
        m_uiFallbackMaterialInstance->Texture("Material_Texture", m_uiFallbackTexture.get());
    }

    return true;
}

std::shared_ptr<SnAPI::Graphics::MaterialInstance> RendererSystem::ResolveUiMaterialForTexture(const std::uint32_t TextureId)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!EnsureUiMaterialResources())
    {
        return {};
    }

    if (TextureId == 0)
    {
        return m_uiFallbackMaterialInstance;
    }

    if (const auto MaterialIt = m_uiTextureMaterialInstances.find(TextureId); MaterialIt != m_uiTextureMaterialInstances.end())
    {
        return MaterialIt->second;
    }

    auto PendingIt = m_uiPendingTextureUploads.find(TextureId);
    if (PendingIt == m_uiPendingTextureUploads.end())
    {
        return m_uiFallbackMaterialInstance;
    }

    auto& Pending = PendingIt->second;
    if (Pending.Width == 0 || Pending.Height == 0 || Pending.Pixels.empty())
    {
        return m_uiFallbackMaterialInstance;
    }

    SnAPI::Graphics::ImageCreateInfo ImageCI = SnAPI::Graphics::ImageCreateInfo::DataDefault(
        SnAPI::Size2DU{Pending.Width, Pending.Height}, SnAPI::Graphics::ETextureFormat::R8G8B8A8_Unorm);
    ImageCI.Data = Pending.Pixels;
    auto Texture = std::shared_ptr<SnAPI::Graphics::IGPUImage>(m_graphics->CreateImage2D(ImageCI).release());
    if (!Texture)
    {
        return m_uiFallbackMaterialInstance;
    }

    auto MaterialInstance = m_uiMaterial->CreateMaterialInstance();
    if (!MaterialInstance)
    {
        return m_uiFallbackMaterialInstance;
    }

    MaterialInstance->Texture("Material_Texture", Texture.get());
    m_uiTextures[TextureId] = std::move(Texture);
    m_uiTextureMaterialInstances[TextureId] = MaterialInstance;
    m_uiPendingTextureUploads.erase(PendingIt);
    return MaterialInstance;
}

std::shared_ptr<SnAPI::Graphics::MaterialInstance> RendererSystem::ResolveUiFontMaterialInstance()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!EnsureUiMaterialResources())
    {
        return {};
    }

    // World::EndFrame primes default font + packet writer metrics before BeginFrame.
    // Avoid loading/regenerating font resources while the render command buffer is open.
    if (!m_defaultFont || !IsFontRenderable(m_defaultFont))
    {
        return m_uiFallbackMaterialInstance;
    }

    auto* AtlasTexture = m_defaultFont->Atlas().GraphicsImage();
    if (!AtlasTexture)
    {
        return m_uiFallbackMaterialInstance;
    }

    if (const auto It = m_uiFontMaterialInstances.find(AtlasTexture); It != m_uiFontMaterialInstances.end())
    {
        return It->second;
    }

    if (!m_uiFontMaterial)
    {
        return m_uiFallbackMaterialInstance;
    }

    auto MaterialInstance = m_uiFontMaterial->CreateMaterialInstance();
    if (!MaterialInstance)
    {
        return m_uiFallbackMaterialInstance;
    }

    MaterialInstance->Texture("Material_Texture", AtlasTexture);
    m_uiFontMaterialInstances.emplace(AtlasTexture, MaterialInstance);

    return MaterialInstance;
}

void RendererSystem::FlushQueuedUiPackets()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_uiQueuedRects.empty())
    {
        return;
    }
    if (!m_graphics || !EnsureUiMaterialResources())
    {
        m_uiQueuedRects.clear();
        return;
    }

    for (const auto& Entry : m_uiQueuedRects)
    {
        SnAPI::Graphics::GpuData::InstancedTexturedRect Rect{};
        Rect.Rect.Pivot = SnAPI::Vector2DF{0.0f, 0.0f};
        Rect.Rect.PosSize = SnAPI::Vector4DF{Entry.X, Entry.Y, Entry.W, Entry.H};
        Rect.Rect.Color = SnAPI::ColorF{Entry.R, Entry.G, Entry.B, Entry.A};
        Rect.Rect.CornerRadius = Entry.CornerRadius;
        Rect.Rect.BorderThickness = Entry.BorderThickness;
        Rect.Rect.PixelSize = SnAPI::Vector2DF{Entry.W, Entry.H};
        Rect.Rect.BorderColor = SnAPI::ColorF{Entry.BorderR, Entry.BorderG, Entry.BorderB, Entry.BorderA};
        Rect.Rect.GlobalZ = Entry.GlobalZ;
        Rect.SubArea = SnAPI::Graphics::GpuData::TextureArea{
            Entry.U0,
            Entry.V0,
            Entry.U1 - Entry.U0,
            Entry.V1 - Entry.V0};

        if (Entry.HasScissor)
        {
            Rect.Rect.Scissor = SnAPI::Area2DF{
                Entry.ScissorMinX,
                Entry.ScissorMinY,
                Entry.ScissorMaxX,
                Entry.ScissorMaxY};
        }
        else
        {
            Rect.Rect.Scissor = SnAPI::Area2DF{};
        }

        std::shared_ptr<SnAPI::Graphics::MaterialInstance> MaterialInstance{};
        if (Entry.UseFontAtlas)
        {
            MaterialInstance = ResolveUiFontMaterialInstance();
        }
        else
        {
            MaterialInstance = ResolveUiMaterialForTexture(Entry.TextureId);
        }

        if (!MaterialInstance)
        {
            MaterialInstance = m_uiFallbackMaterialInstance;
        }

        m_graphics->DrawTexturedRectangle(Rect, MaterialInstance, true);
    }

    m_uiQueuedRects.clear();
}
#endif

bool RendererSystem::RecreateSwapChainForCurrentWindowUnlocked()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!m_graphics || !m_window || !m_window->IsOpen())
    {
        return false;
    }

    auto TryRecreate = [this]() -> bool {
        try
        {
            m_graphics->RecreateSwapChainForWindow(m_window.get());
            return true;
        }
        catch (const std::exception& Ex)
        {
            SNAPI_RENDERER_LOG_WARNING("Swapchain recreation failed: %s", Ex.what());
            return false;
        }
    };

    if (!TryRecreate())
    {
        if (!m_settings.AutoFallbackOnOutOfMemory)
        {
            return false;
        }

        const float FallbackWidth = ClampWindowExtent(std::min(m_window->Size().x(), m_settings.OutOfMemoryFallbackWindowWidth));
        const float FallbackHeight = ClampWindowExtent(std::min(m_window->Size().y(), m_settings.OutOfMemoryFallbackWindowHeight));

        if (m_settings.ForceWindowedOnOutOfMemory)
        {
            m_window->FullScreen(false);
        }
        if (m_settings.DisableTransparencyOnOutOfMemory)
        {
            m_window->AllowTransparency(false);
        }
        m_window->Size({FallbackWidth, FallbackHeight});

        if (!TryRecreate())
        {
            return false;
        }
    }

    const auto WindowSize = m_window->Size();
    m_lastWindowWidth = WindowSize.x();
    m_lastWindowHeight = WindowSize.y();
    m_hasWindowSizeSnapshot = (m_lastWindowWidth > 0.0f && m_lastWindowHeight > 0.0f);
    return true;
}

bool RendererSystem::CreateWindowResources()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_window)
    {
        return true;
    }
    if (!m_graphics)
    {
        return false;
    }

    auto Window = std::unique_ptr<SnAPI::Graphics::WindowBase, WindowDeleter>(new SnAPI::Graphics::SDLWindow());
    Window->Create(BuildWindowCreateInfo(m_settings));
    m_graphics->InitializeResourcesForWindow(Window.get());
    const auto WindowSize = Window->Size();
    m_lastWindowWidth = WindowSize.x();
    m_lastWindowHeight = WindowSize.y();
    m_hasWindowSizeSnapshot = (m_lastWindowWidth > 0.0f && m_lastWindowHeight > 0.0f);
    m_window = std::move(Window);
    return true;
}

bool RendererSystem::RegisterDefaultPassGraph()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_passGraphRegistered)
    {
        return true;
    }
    if (!m_graphics)
    {
        return false;
    }
    if (m_settings.CreateDefaultLighting && !EnsureDefaultLighting())
    {
        return false;
    }

    using namespace SnAPI::Graphics;

    auto GBufferPassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "GBuffer Pass"},
        {AutoGeneratedPass::PropertyNames::MaterialsShadingModel.data(), "GBufferShadingModel"},
        {GBufferPass::PropertyNames::ShadersCullProgram.data(), "TriangleCulling"},
        {GBufferPass::PropertyNames::ShadersGenDrawIndirectProgram.data(), "GenDrawIndirect"},
        {GBufferPass::PropertyNames::ShadersReductionProgram.data(), "TriangleReduction"},
        {GBufferPass::PropertyNames::ShadersInstanceCullProgram.data(), "InstanceCulling"},
        {AutoGeneratedPass::PropertyNames::PassDepthConfig.data(), DepthConfig{.WriteDepth = true, .DepthTest = true, .WriteResourceName = "GBuffer_Depth"}}};

    auto ShadingPassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "Shading Pass"},
        {FullScreenPass::PropertyNames::MaterialsShadingModel.data(), "DeferredShadingShadingModel"},
        {FullScreenPass::PropertyNames::MaterialsModule.data(), "DefaultDeferredMaterial"},
        {AutoGeneratedPass::PropertyNames::PassDepthConfig.data(),
         DepthConfig{.WriteDepth = false, .SampleDepth = true, .DepthTest = false, .ReadResourceName = "GBuffer_Depth", .ReadLayout = EImageLayout::DepthStencilReadOnlyOptimal}}};

    auto ToneMapPassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "ToneMap Pass"},
        {FullScreenPass::PropertyNames::MaterialsShadingModel.data(), "PostProcessShadingModel"},
        {FullScreenPass::PropertyNames::MaterialsModule.data(), "ToneMapMaterial"},
    };

    auto UIPassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "UI Pass"},
        {AutoGeneratedPass::PropertyNames::MaterialsShadingModel.data(), "UIShadingModel"},
        {AutoGeneratedPass::PropertyNames::MaterialsModule.data(), "DefaultUIMaterial"},
    };

    auto CompositePassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "Composite Pass"},
        {FullScreenPass::PropertyNames::MaterialsShadingModel.data(), "PostProcessShadingModel"},
        {FullScreenPass::PropertyNames::MaterialsModule.data(), "CompositeMaterial"},
    };

    auto AtmosphereCompositePassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "Atmosphere Composite Pass"},
        {FullScreenPass::PropertyNames::MaterialsShadingModel.data(), "PostProcessShadingModel"},
        {FullScreenPass::PropertyNames::MaterialsModule.data(), "AtmosCompositeMaterial"},
        {AutoGeneratedPass::PropertyNames::PassDepthConfig.data(),
         DepthConfig{.WriteDepth = false, .SampleDepth = true, .DepthTest = false, .ReadResourceName = "GBuffer_Depth", .ReadLayout = EImageLayout::DepthStencilReadOnlyOptimal}}};

    auto PresentPassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "Present Pass"},
        {FullScreenPass::PropertyNames::MaterialsShadingModel.data(), "PostProcessShadingModel"},
        {FullScreenPass::PropertyNames::MaterialsModule.data(), "PassThroughMaterial"},
    };

    auto AtmospherePassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "Atmosphere Pass"},
        {FullScreenPass::PropertyNames::MaterialsShadingModel.data(), "PostProcessShadingModel"},
        {FullScreenPass::PropertyNames::MaterialsModule.data(), "AtmosphereMaterial"},
    };

    auto SSAOPassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "SSAO Pass"},
        {AutoGeneratedPass::PropertyNames::PassDepthConfig.data(),
         DepthConfig{.WriteDepth = false, .SampleDepth = true, .DepthTest = false, .ReadResourceName = "GBuffer_Depth", .ReadLayout = EImageLayout::DepthStencilReadOnlyOptimal}}};

    auto Shadow = std::make_unique<ShadowPass>();
    Shadow->SetLightManager(m_lightManager.get());
    m_graphics->RegisterPass(std::move(Shadow));

    m_gbufferPass = static_cast<GBufferPass*>(m_graphics->RegisterPass(std::make_unique<GBufferPass>(std::move(GBufferPassProperties))));

    if (m_settings.EnableSsao)
    {
        auto SSAO = std::make_unique<SSAOPass>(std::move(SSAOPassProperties));
        SSAO->SetIntensity(1.0f);
        SSAO->SetRadius(3.0f);
        SSAO->SetBias(0.025f);
        SSAO->SetDenoiseBlurBeta(1.5f);
        SSAO->SetSliceCount(3);
        SSAO->SetStepsPerSlice(6);
        SSAO->SetTemporalBlendFactor(0.01f);
        SSAO->SetDisocclusionThreshold(0.02f);
        m_ssaoPass = static_cast<SSAOPass*>(m_graphics->RegisterPass(std::move(SSAO)));
    }

    auto HiZPassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "HiZ Pass"},
        {AutoGeneratedPass::PropertyNames::PassDepthConfig.data(),
         DepthConfig{.WriteDepth = false, .SampleDepth = true, .DepthTest = false, .ReadResourceName = "GBuffer_Depth", .ReadLayout = EImageLayout::DepthStencilReadOnlyOptimal}}};
    m_graphics->RegisterPass(std::make_unique<HiZPass>(std::move(HiZPassProperties)));

    if (m_settings.EnableSsr)
    {
        auto SSRPassProperties = PassProperties{
            {AutoGeneratedPass::PropertyNames::PassName.data(), "SSR Pass"},
            {AutoGeneratedPass::PropertyNames::PassDepthConfig.data(),
             DepthConfig{.WriteDepth = false, .SampleDepth = true, .DepthTest = false, .ReadResourceName = "GBuffer_Depth", .ReadLayout = EImageLayout::DepthStencilReadOnlyOptimal}}};

        auto SSR = std::make_unique<SSRPass>(std::move(SSRPassProperties));
        SSR->SetMaxRoughness(0.8f);
        SSR->SetRoughnessThreshold(0.2f);
        SSR->SetMaxSteps(32);
        SSR->SetThickness(0.015f);
        SSR->SetMaxDistance(0.25);
        m_ssrPass = static_cast<SSRPass*>(m_graphics->RegisterPass(std::move(SSR)));

        auto SSRCompositePassProperties = PassProperties{
            {AutoGeneratedPass::PropertyNames::PassName.data(), "SSR Composite Pass"},
            {FullScreenPass::PropertyNames::MaterialsShadingModel.data(), "PostProcessShadingModel"},
            {FullScreenPass::PropertyNames::MaterialsModule.data(), "SSRCompositeMaterial"},
        };
        m_graphics->RegisterPass(std::make_unique<CompositePass>(std::move(SSRCompositePassProperties)));
    }

    auto DeferredShading = std::make_unique<DeferredShadingPass>(std::move(ShadingPassProperties));
    DeferredShading->SetLightManager(m_lightManager.get());
    m_graphics->RegisterPass(std::move(DeferredShading));

    m_graphics->RegisterPass(std::make_unique<ToneMapPass>(std::move(ToneMapPassProperties)));
    m_graphics->RegisterPass(std::make_unique<CompositePass>(std::move(CompositePassProperties)));
    m_graphics->RegisterPass(std::make_unique<UIPass>(std::move(UIPassProperties)));
    m_graphics->RegisterPass(std::make_unique<PresentPass>(std::move(PresentPassProperties)));

    if (m_settings.EnableAtmosphere)
    {
        m_graphics->RegisterPass(std::make_unique<AtmospherePass>(std::move(AtmospherePassProperties)));
        m_graphics->RegisterPass(std::make_unique<CompositePass>(std::move(AtmosphereCompositePassProperties)));
    }

    if (m_settings.EnableBloom)
    {
        auto BloomPassProperties = PassProperties{
            {AutoGeneratedPass::PropertyNames::PassName.data(), "Bloom Pass"},
        };
        auto Bloom = std::make_unique<BloomPass>(std::move(BloomPassProperties));
        Bloom->SetMipCount(5);
        m_bloomPass = static_cast<BloomPass*>(m_graphics->RegisterPass(std::move(Bloom)));
    }

    m_graphics->RequestFrameGraphRebuild();
    m_passGraphRegistered = true;
    return true;
}

void RendererSystem::ResetPassPointers()
{
    m_ssaoPass = nullptr;
    m_ssrPass = nullptr;
    m_bloomPass = nullptr;
    m_gbufferPass = nullptr;
}

    const Graphics::LightManager* RendererSystem::LightManager()const
{
    return m_lightManager.get();
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
