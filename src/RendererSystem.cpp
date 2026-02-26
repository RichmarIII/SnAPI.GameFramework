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
#include <limits>
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
#include <SDL3/SDL.h>
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

#include "SnAPI/Math/LinearAlgebra.h"

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
constexpr std::uint32_t kPendingSwapChainStableFrameThreshold = 2u;
constexpr float kViewportConfigFloatEpsilon = 0.001f;
constexpr std::size_t kMaxQueuedTextRequests = 256;
#if defined(SNAPI_GF_ENABLE_UI)
constexpr float kUiGlobalZBase = 1.0f;
// Keep UI depth values in a compact range to preserve separation after the UI shader's
// reverse-Z remap (which is non-linear in GlobalZ). Large GlobalZ growth collapses depth.
constexpr float kUiGlobalZStep = 0.01f;
constexpr std::uint32_t kUiGradientTextureSize = 128u;

[[nodiscard]] constexpr std::uint32_t PackUiColorRgba8(const SnAPI::UI::Color& Value)
{
    return (static_cast<std::uint32_t>(Value.R) << 24u) |
           (static_cast<std::uint32_t>(Value.G) << 16u) |
           (static_cast<std::uint32_t>(Value.B) << 8u) |
           static_cast<std::uint32_t>(Value.A);
}

[[nodiscard]] constexpr float ClampUnit(const float Value)
{
    return std::clamp(Value, 0.0f, 1.0f);
}
#endif

float ClampWindowExtent(const float Value)
{
    return std::max(kMinWindowExtent, Value);
}

bool NearlyEqual(const float Left, const float Right)
{
    return std::fabs(Left - Right) <= kViewportConfigFloatEpsilon;
}

[[nodiscard]] bool IsPrimaryMouseButtonDown()
{
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    const auto Buttons = SDL_GetGlobalMouseState(&MouseX, &MouseY);
    (void)MouseX;
    (void)MouseY;
    return (Buttons & SDL_BUTTON_LMASK) != 0u;
}

bool AreViewportRectsEquivalent(const SnAPI::Graphics::ViewportFit& Left, const SnAPI::Graphics::ViewportFit& Right)
{
    return NearlyEqual(Left.X, Right.X) &&
           NearlyEqual(Left.Y, Right.Y) &&
           NearlyEqual(Left.Width, Right.Width) &&
           NearlyEqual(Left.Height, Right.Height);
}

bool AreRenderViewportConfigsEquivalent(const SnAPI::Graphics::RenderViewportConfig& Left,
                                        const SnAPI::Graphics::RenderViewportConfig& Right)
{
    return Left.Name == Right.Name &&
           Left.RenderExtent.x() == Right.RenderExtent.x() &&
           Left.RenderExtent.y() == Right.RenderExtent.y() &&
           AreViewportRectsEquivalent(Left.OutputRect, Right.OutputRect) &&
           Left.Enabled == Right.Enabled &&
           Left.TargetSwapChainID == Right.TargetSwapChainID &&
           Left.FinalColorResourceName == Right.FinalColorResourceName &&
           Left.FinalDepthResourceName == Right.FinalDepthResourceName &&
           Left.pCamera == Right.pCamera;
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
    m_defaultFontFallbacksConfigured = Other.m_defaultFontFallbacksConfigured;
    m_textQueue = std::move(Other.m_textQueue);
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiMaterial = std::move(Other.m_uiMaterial);
    m_uiFontMaterial = std::move(Other.m_uiFontMaterial);
    m_uiTriangleMaterial = std::move(Other.m_uiTriangleMaterial);
    m_uiCircleMaterial = std::move(Other.m_uiCircleMaterial);
    m_uiShadowMaterial = std::move(Other.m_uiShadowMaterial);
    m_uiFallbackTexture = std::move(Other.m_uiFallbackTexture);
    m_uiFallbackMaterialInstance = std::move(Other.m_uiFallbackMaterialInstance);
    m_uiTriangleMaterialInstance = std::move(Other.m_uiTriangleMaterialInstance);
    m_uiCircleMaterialInstance = std::move(Other.m_uiCircleMaterialInstance);
    m_uiShadowMaterialInstance = std::move(Other.m_uiShadowMaterialInstance);
    m_uiFontMaterialInstances = std::move(Other.m_uiFontMaterialInstances);
    m_uiTextures = std::move(Other.m_uiTextures);
    m_uiTextureHasTransparency = std::move(Other.m_uiTextureHasTransparency);
    m_uiTextureMaterialInstances = std::move(Other.m_uiTextureMaterialInstances);
    m_uiGradientTextures = std::move(Other.m_uiGradientTextures);
    m_uiGradientMaterialInstances = std::move(Other.m_uiGradientMaterialInstances);
    m_uiPendingTextureUploads = std::move(Other.m_uiPendingTextureUploads);
    m_uiQueuedRects = std::move(Other.m_uiQueuedRects);
    m_uiPacketsQueuedThisFrame = Other.m_uiPacketsQueuedThisFrame;
#endif
    m_lastWindowWidth = Other.m_lastWindowWidth;
    m_lastWindowHeight = Other.m_lastWindowHeight;
    m_hasWindowSizeSnapshot = Other.m_hasWindowSizeSnapshot;
    m_pendingSwapChainWidth = Other.m_pendingSwapChainWidth;
    m_pendingSwapChainHeight = Other.m_pendingSwapChainHeight;
    m_hasPendingSwapChainResize = Other.m_hasPendingSwapChainResize;
    m_pendingSwapChainStableFrames = Other.m_pendingSwapChainStableFrames;
    m_registeredRenderObjects = std::move(Other.m_registeredRenderObjects);
    m_registeredViewportPassGraphs = std::move(Other.m_registeredViewportPassGraphs);
    m_renderViewportPassGraphRevision = Other.m_renderViewportPassGraphRevision;
    m_initialized = Other.m_initialized;

    Other.m_graphics = nullptr;
    Other.ResetPassPointers();
    Other.m_passGraphRegistered = false;
    Other.m_defaultFont = nullptr;
    Other.m_defaultFontFallbacksConfigured = false;
    Other.m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    Other.m_uiMaterial.reset();
    Other.m_uiFontMaterial.reset();
    Other.m_uiTriangleMaterial.reset();
    Other.m_uiCircleMaterial.reset();
    Other.m_uiShadowMaterial.reset();
    Other.m_uiFallbackTexture.reset();
    Other.m_uiFallbackMaterialInstance.reset();
    Other.m_uiTriangleMaterialInstance.reset();
    Other.m_uiCircleMaterialInstance.reset();
    Other.m_uiShadowMaterialInstance.reset();
    Other.m_uiFontMaterialInstances.clear();
    Other.m_uiTextures.clear();
    Other.m_uiTextureHasTransparency.clear();
    Other.m_uiTextureMaterialInstances.clear();
    Other.m_uiGradientTextures.clear();
    Other.m_uiGradientMaterialInstances.clear();
    Other.m_uiPendingTextureUploads.clear();
    Other.m_uiQueuedRects.clear();
    Other.m_uiPacketsQueuedThisFrame = false;
#endif
    Other.m_lastWindowWidth = 0.0f;
    Other.m_lastWindowHeight = 0.0f;
    Other.m_hasWindowSizeSnapshot = false;
    Other.m_pendingSwapChainWidth = 0.0f;
    Other.m_pendingSwapChainHeight = 0.0f;
    Other.m_hasPendingSwapChainResize = false;
    Other.m_pendingSwapChainStableFrames = 0;
    Other.m_registeredRenderObjects.clear();
    Other.m_registeredViewportPassGraphs.clear();
    Other.m_renderViewportPassGraphRevision = 1;
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
    m_defaultFontFallbacksConfigured = Other.m_defaultFontFallbacksConfigured;
    m_textQueue = std::move(Other.m_textQueue);
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiMaterial = std::move(Other.m_uiMaterial);
    m_uiFontMaterial = std::move(Other.m_uiFontMaterial);
    m_uiTriangleMaterial = std::move(Other.m_uiTriangleMaterial);
    m_uiCircleMaterial = std::move(Other.m_uiCircleMaterial);
    m_uiShadowMaterial = std::move(Other.m_uiShadowMaterial);
    m_uiFallbackTexture = std::move(Other.m_uiFallbackTexture);
    m_uiFallbackMaterialInstance = std::move(Other.m_uiFallbackMaterialInstance);
    m_uiTriangleMaterialInstance = std::move(Other.m_uiTriangleMaterialInstance);
    m_uiCircleMaterialInstance = std::move(Other.m_uiCircleMaterialInstance);
    m_uiShadowMaterialInstance = std::move(Other.m_uiShadowMaterialInstance);
    m_uiFontMaterialInstances = std::move(Other.m_uiFontMaterialInstances);
    m_uiTextures = std::move(Other.m_uiTextures);
    m_uiTextureHasTransparency = std::move(Other.m_uiTextureHasTransparency);
    m_uiTextureMaterialInstances = std::move(Other.m_uiTextureMaterialInstances);
    m_uiGradientTextures = std::move(Other.m_uiGradientTextures);
    m_uiGradientMaterialInstances = std::move(Other.m_uiGradientMaterialInstances);
    m_uiPendingTextureUploads = std::move(Other.m_uiPendingTextureUploads);
    m_uiQueuedRects = std::move(Other.m_uiQueuedRects);
    m_uiPacketsQueuedThisFrame = Other.m_uiPacketsQueuedThisFrame;
#endif
    m_lastWindowWidth = Other.m_lastWindowWidth;
    m_lastWindowHeight = Other.m_lastWindowHeight;
    m_hasWindowSizeSnapshot = Other.m_hasWindowSizeSnapshot;
    m_pendingSwapChainWidth = Other.m_pendingSwapChainWidth;
    m_pendingSwapChainHeight = Other.m_pendingSwapChainHeight;
    m_hasPendingSwapChainResize = Other.m_hasPendingSwapChainResize;
    m_pendingSwapChainStableFrames = Other.m_pendingSwapChainStableFrames;
    m_registeredRenderObjects = std::move(Other.m_registeredRenderObjects);
    m_registeredViewportPassGraphs = std::move(Other.m_registeredViewportPassGraphs);
    m_renderViewportPassGraphRevision = Other.m_renderViewportPassGraphRevision;
    m_initialized = Other.m_initialized;

    Other.m_graphics = nullptr;
    Other.ResetPassPointers();
    Other.m_passGraphRegistered = false;
    Other.m_defaultFont = nullptr;
    Other.m_defaultFontFallbacksConfigured = false;
    Other.m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    Other.m_uiMaterial.reset();
    Other.m_uiFontMaterial.reset();
    Other.m_uiTriangleMaterial.reset();
    Other.m_uiCircleMaterial.reset();
    Other.m_uiShadowMaterial.reset();
    Other.m_uiFallbackTexture.reset();
    Other.m_uiFallbackMaterialInstance.reset();
    Other.m_uiTriangleMaterialInstance.reset();
    Other.m_uiCircleMaterialInstance.reset();
    Other.m_uiShadowMaterialInstance.reset();
    Other.m_uiFontMaterialInstances.clear();
    Other.m_uiTextures.clear();
    Other.m_uiTextureHasTransparency.clear();
    Other.m_uiTextureMaterialInstances.clear();
    Other.m_uiGradientTextures.clear();
    Other.m_uiGradientMaterialInstances.clear();
    Other.m_uiPendingTextureUploads.clear();
    Other.m_uiQueuedRects.clear();
    Other.m_uiPacketsQueuedThisFrame = false;
#endif
    Other.m_lastWindowWidth = 0.0f;
    Other.m_lastWindowHeight = 0.0f;
    Other.m_hasWindowSizeSnapshot = false;
    Other.m_pendingSwapChainWidth = 0.0f;
    Other.m_pendingSwapChainHeight = 0.0f;
    Other.m_hasPendingSwapChainResize = false;
    Other.m_pendingSwapChainStableFrames = 0;
    Other.m_registeredRenderObjects.clear();
    Other.m_registeredViewportPassGraphs.clear();
    Other.m_renderViewportPassGraphRevision = 1;
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
        m_defaultFontFallbacksConfigured = false;
        m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
        m_uiMaterial.reset();
        m_uiFontMaterial.reset();
        m_uiTriangleMaterial.reset();
        m_uiCircleMaterial.reset();
        m_uiShadowMaterial.reset();
        m_uiFallbackTexture.reset();
        m_uiFallbackMaterialInstance.reset();
        m_uiTriangleMaterialInstance.reset();
        m_uiCircleMaterialInstance.reset();
        m_uiShadowMaterialInstance.reset();
        m_uiFontMaterialInstances.clear();
        m_uiTextures.clear();
        m_uiTextureHasTransparency.clear();
        m_uiTextureMaterialInstances.clear();
        m_uiGradientTextures.clear();
        m_uiGradientMaterialInstances.clear();
        m_uiPendingTextureUploads.clear();
        m_uiQueuedRects.clear();
        m_uiPacketsQueuedThisFrame = false;
#endif
        m_window.reset();
        m_lightManager.reset();
        ResetPassPointers();
        m_passGraphRegistered = false;
        m_lastWindowWidth = 0.0f;
        m_lastWindowHeight = 0.0f;
        m_hasWindowSizeSnapshot = false;
        m_pendingSwapChainWidth = 0.0f;
        m_pendingSwapChainHeight = 0.0f;
        m_hasPendingSwapChainResize = false;
        m_pendingSwapChainStableFrames = 0;
        m_registeredRenderObjects.clear();
        m_registeredViewportPassGraphs.clear();
        m_renderViewportPassGraphRevision = 1;
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
    m_defaultFontFallbacksConfigured = false;
    m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiMaterial.reset();
    m_uiFontMaterial.reset();
    m_uiTriangleMaterial.reset();
    m_uiCircleMaterial.reset();
    m_uiShadowMaterial.reset();
    m_uiFallbackTexture.reset();
    m_uiFallbackMaterialInstance.reset();
    m_uiTriangleMaterialInstance.reset();
    m_uiCircleMaterialInstance.reset();
    m_uiShadowMaterialInstance.reset();
    m_uiFontMaterialInstances.clear();
    m_uiTextures.clear();
    m_uiTextureHasTransparency.clear();
    m_uiTextureMaterialInstances.clear();
    m_uiGradientTextures.clear();
    m_uiGradientMaterialInstances.clear();
    m_uiPendingTextureUploads.clear();
    m_uiQueuedRects.clear();
    m_uiPacketsQueuedThisFrame = false;
#endif
    m_window.reset();
    m_lightManager.reset();
    ResetPassPointers();
    m_passGraphRegistered = false;
    m_registeredRenderObjects.clear();
    m_registeredViewportPassGraphs.clear();
    m_renderViewportPassGraphRevision = 1;
    m_pendingSwapChainWidth = 0.0f;
    m_pendingSwapChainHeight = 0.0f;
    m_hasPendingSwapChainResize = false;
    m_pendingSwapChainStableFrames = 0;

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

    // GameFramework owns swapchain resize coalescing when auto-resize is enabled.
    m_graphics->SetAutoSwapChainRecreateOnInvalidation(!m_settings.AutoHandleSwapChainResize);

    if (m_settings.CreateWindow && !CreateWindowResources())
    {
        ShutdownUnlocked();
        return false;
    }

    if (!EnsureLightManagerInternal())
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

bool RendererSystem::UseDefaultRenderViewport(const bool Enabled)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics)
    {
        return false;
    }

    m_graphics->UseDefaultViewport(Enabled);
    return true;
}

bool RendererSystem::IsUsingDefaultRenderViewport() const
{
    GameLockGuard Lock(m_mutex);
    return m_graphics ? m_graphics->IsUsingDefaultViewport() : false;
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

bool RendererSystem::CreateRenderViewport(std::string Name,
                                          const float X,
                                          const float Y,
                                          const float Width,
                                          const float Height,
                                          const std::uint32_t RenderWidth,
                                          const std::uint32_t RenderHeight,
                                          SnAPI::Graphics::ICamera* Camera,
                                          const bool Enabled,
                                          std::uint64_t& OutViewportID)
{
    GameLockGuard Lock(m_mutex);
    OutViewportID = 0;
    if (!m_graphics)
    {
        return false;
    }

    const float ClampedW = std::max(kMinWindowExtent, Width);
    const float ClampedH = std::max(kMinWindowExtent, Height);
    const std::uint32_t FinalRenderWidth = RenderWidth > 0 ? RenderWidth : static_cast<std::uint32_t>(std::round(ClampedW));
    const std::uint32_t FinalRenderHeight = RenderHeight > 0 ? RenderHeight : static_cast<std::uint32_t>(std::round(ClampedH));

    SnAPI::Graphics::RenderViewportConfig Config{};
    Config.Name = Name.empty() ? "Viewport" : std::move(Name);
    Config.OutputRect = SnAPI::Graphics::ViewportFit{
        .X = X,
        .Y = Y,
        .Width = ClampedW,
        .Height = ClampedH,
    };
    Config.RenderExtent = SnAPI::Math::Size2DU{
        std::max<std::uint32_t>(1u, FinalRenderWidth),
        std::max<std::uint32_t>(1u, FinalRenderHeight)};
    Config.Enabled = Enabled;
    Config.pCamera = Camera;

    const auto ViewportID = m_graphics->CreateRenderViewport(Config);
    if (ViewportID == 0)
    {
        return false;
    }

    OutViewportID = static_cast<std::uint64_t>(ViewportID);
    return true;
}

bool RendererSystem::UpdateRenderViewport(const std::uint64_t ViewportID,
                                          std::string Name,
                                          const float X,
                                          const float Y,
                                          const float Width,
                                          const float Height,
                                          const std::uint32_t RenderWidth,
                                          const std::uint32_t RenderHeight,
                                          SnAPI::Graphics::ICamera* Camera,
                                          const bool Enabled)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0)
    {
        return false;
    }

    const float ClampedW = std::max(kMinWindowExtent, Width);
    const float ClampedH = std::max(kMinWindowExtent, Height);
    const std::uint32_t FinalRenderWidth = RenderWidth > 0 ? RenderWidth : static_cast<std::uint32_t>(std::round(ClampedW));
    const std::uint32_t FinalRenderHeight = RenderHeight > 0 ? RenderHeight : static_cast<std::uint32_t>(std::round(ClampedH));

    SnAPI::Graphics::RenderViewportConfig Config{};
    Config.Name = Name.empty() ? "Viewport" : std::move(Name);
    Config.OutputRect = SnAPI::Graphics::ViewportFit{
        .X = X,
        .Y = Y,
        .Width = ClampedW,
        .Height = ClampedH,
    };
    Config.RenderExtent = SnAPI::Math::Size2DU{
        std::max<std::uint32_t>(1u, FinalRenderWidth),
        std::max<std::uint32_t>(1u, FinalRenderHeight)};
    Config.Enabled = Enabled;
    Config.pCamera = Camera;

    const auto RendererViewportID = static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID);
    const auto Existing = m_graphics->GetRenderViewportConfig(RendererViewportID);
    if (Existing.has_value())
    {
        Config.TargetSwapChainID = Existing->TargetSwapChainID;
        Config.FinalColorResourceName = Existing->FinalColorResourceName;
        Config.FinalDepthResourceName = Existing->FinalDepthResourceName;
    }

    if (Existing.has_value() && AreRenderViewportConfigsEquivalent(*Existing, Config))
    {
        return true;
    }

    return m_graphics->SetRenderViewportConfig(RendererViewportID, Config);
}

bool RendererSystem::DestroyRenderViewport(const std::uint64_t ViewportID)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0)
    {
        return false;
    }

    const auto RendererViewportID = static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID);
    if (RendererViewportID == m_graphics->DefaultRenderViewportID())
    {
        return false;
    }

    const bool Destroyed = m_graphics->DestroyRenderViewport(RendererViewportID);
    if (Destroyed || !m_graphics->GetRenderViewportConfig(RendererViewportID).has_value())
    {
        m_registeredViewportPassGraphs.erase(ViewportID);
    }
    return Destroyed;
}

bool RendererSystem::HasRenderViewport(const std::uint64_t ViewportID) const
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0)
    {
        return false;
    }

    const auto RendererViewportID = static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID);
    return m_graphics->GetRenderViewportConfig(RendererViewportID).has_value();
}

bool RendererSystem::SetRenderViewportIndex(const std::uint64_t ViewportID, const std::size_t Index)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0)
    {
        return false;
    }

    return m_graphics->SetRenderViewportIndex(static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID), Index);
}

std::optional<std::size_t> RendererSystem::RenderViewportIndex(const std::uint64_t ViewportID) const
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0)
    {
        return std::nullopt;
    }

    return m_graphics->GetRenderViewportIndex(static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID));
}

bool RendererSystem::RegisterRenderViewportPassGraph(const std::uint64_t ViewportID, const ERenderViewportPassGraphPreset Preset)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0)
    {
        return false;
    }

    const bool TrackDefault = static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID) == m_graphics->DefaultRenderViewportID();
    return RegisterRenderViewportPassGraphUnlocked(ViewportID, Preset, TrackDefault);
}

bool RendererSystem::SetRenderViewportGlobalInputNameOverrides(const std::uint64_t ViewportID,
                                                               std::vector<std::pair<std::string, std::string>> Overrides)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0)
    {
        return false;
    }

    return m_graphics->SetRenderViewportGlobalInputNameOverrides(static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID), std::move(Overrides));
}

bool RendererSystem::SetRenderViewportGlobalOutputNameOverrides(const std::uint64_t ViewportID,
                                                                std::vector<std::pair<std::string, std::string>> Overrides)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0)
    {
        return false;
    }

    return m_graphics->SetRenderViewportGlobalOutputNameOverrides(static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID), std::move(Overrides));
}

bool RendererSystem::SetRenderViewportPassInputNameOverrides(const std::uint64_t ViewportID,
                                                             const SnAPI::Graphics::IHighLevelPass* Pass,
                                                             std::vector<std::pair<std::string, std::string>> Overrides)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0 || !Pass)
    {
        return false;
    }

    return m_graphics->SetRenderViewportPassInputNameOverrides(static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID), Pass, std::move(Overrides));
}

bool RendererSystem::SetRenderViewportPassOutputNameOverrides(const std::uint64_t ViewportID,
                                                              const SnAPI::Graphics::IHighLevelPass* Pass,
                                                              std::vector<std::pair<std::string, std::string>> Overrides)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0 || !Pass)
    {
        return false;
    }

    return m_graphics->SetRenderViewportPassOutputNameOverrides(static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID), Pass, std::move(Overrides));
}

bool RendererSystem::ClearRenderViewportPassNameOverrides(const std::uint64_t ViewportID, const SnAPI::Graphics::IHighLevelPass* Pass)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0 || !Pass)
    {
        return false;
    }

    return m_graphics->ClearRenderViewportPassNameOverrides(static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID), Pass);
}

bool RendererSystem::ClearRenderViewportNameOverrides(const std::uint64_t ViewportID)
{
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0)
    {
        return false;
    }

    return m_graphics->ClearRenderViewportNameOverrides(static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID));
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

    bool ConfiguredAnyPass = false;
    const auto ViewportIds = m_graphics->RenderViewportIDs();
    for (const auto ViewportId : ViewportIds)
    {
        const auto PresetIt = m_registeredViewportPassGraphs.find(static_cast<std::uint64_t>(ViewportId));
        if (PresetIt == m_registeredViewportPassGraphs.end())
        {
            // Only configure scene-object visibility for viewports explicitly registered
            // through RendererSystem pass-graph presets. This avoids accidental routing
            // into stale/foreign fullscreen viewports.
            continue;
        }

        if (PresetIt->second == ERenderViewportPassGraphPreset::UiPresentOnly ||
            PresetIt->second == ERenderViewportPassGraphPreset::None)
        {
            continue;
        }

        if (auto* GBufferPass = m_graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::GBuffer))
        {
            RenderObject.EnablePass(GBufferPass->ID(), Visible);
            ConfiguredAnyPass = true;
        }
        if (auto* ShadowPass = m_graphics->GetRenderPass(ViewportId, SnAPI::Graphics::ERenderPassType::Shadow))
        {
            RenderObject.EnablePass(ShadowPass->ID(), Visible && CastShadows);
            ConfiguredAnyPass = true;
        }
    }

    RenderObject.SetCastsShadows(CastShadows);
    return ConfiguredAnyPass;
}

std::uint64_t RendererSystem::RenderViewportPassGraphRevision() const
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    return m_renderViewportPassGraphRevision;
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
    m_defaultFontFallbacksConfigured = false;
    EnsureDefaultFont();
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
bool RendererSystem::QueueUiRenderPackets(const std::uint64_t ViewportID,
                                          SnAPI::UI::UIContext& Context,
                                          const SnAPI::UI::RenderPacketList& Packets)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    GameLockGuard Lock(m_mutex);
    if (!m_graphics || ViewportID == 0)
    {
        return false;
    }
    if (!m_graphics->GetRenderViewportConfig(static_cast<SnAPI::Graphics::RenderViewportID>(ViewportID)).has_value())
    {
        return false;
    }

    const auto PacketSpan = Packets.Packets();
    const auto ContextScreenRect = Context.GetScreenRect();
    const float ContextOffsetX = ContextScreenRect.X;
    const float ContextOffsetY = ContextScreenRect.Y;
    std::size_t TotalInstances = 0;
    for (const auto& Packet : PacketSpan)
    {
        TotalInstances += Packet.InstanceCount();
    }

    if (!m_uiPacketsQueuedThisFrame)
    {
        m_uiQueuedRects.clear();
        m_uiPacketsQueuedThisFrame = true;
    }

    if (m_uiQueuedRects.capacity() < (m_uiQueuedRects.size() + TotalInstances))
    {
        m_uiQueuedRects.reserve(m_uiQueuedRects.size() + TotalInstances);
    }

    float GlobalZ = kUiGlobalZBase + static_cast<float>(m_uiQueuedRects.size()) * kUiGlobalZStep;

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

    const auto ApplyUiGradient = [](QueuedUiRect& OutRect, const SnAPI::UI::LinearGradient& Gradient) {
        OutRect.UseGradient = true;
        OutRect.TextureId = 0;
        OutRect.UseFontAtlas = false;
        OutRect.R = 1.0f;
        OutRect.G = 1.0f;
        OutRect.B = 1.0f;
        OutRect.A = 1.0f;
        OutRect.U0 = 0.0f;
        OutRect.V0 = 0.0f;
        OutRect.U1 = 1.0f;
        OutRect.V1 = 1.0f;

        OutRect.GradientStopCount = static_cast<std::uint8_t>(
            std::min<std::size_t>(QueuedUiRect::MaxGradientStops, static_cast<std::size_t>(Gradient.StopCount)));

        if (OutRect.GradientStopCount == 0)
        {
            OutRect.UseGradient = false;
            OutRect.R = 0.0f;
            OutRect.G = 0.0f;
            OutRect.B = 0.0f;
            OutRect.A = 0.0f;
            return;
        }

        OutRect.GradientStartX = Gradient.StartX;
        OutRect.GradientStartY = Gradient.StartY;
        OutRect.GradientEndX = Gradient.EndX;
        OutRect.GradientEndY = Gradient.EndY;

        for (std::size_t Index = 0; Index < QueuedUiRect::MaxGradientStops; ++Index)
        {
            if (Index < OutRect.GradientStopCount)
            {
                OutRect.GradientStops[Index] = Gradient.Stops[Index].Position;
                OutRect.GradientColors[Index] = PackUiColorRgba8(Gradient.Stops[Index].StopColor);
            }
            else
            {
                OutRect.GradientStops[Index] = 0.0f;
                OutRect.GradientColors[Index] = 0u;
            }
        }
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
        OutRect.ScissorMinX = static_cast<float>(Scissor.X) - ContextOffsetX;
        OutRect.ScissorMinY = static_cast<float>(Scissor.Y) - ContextOffsetY;
        OutRect.ScissorMaxX = static_cast<float>(Scissor.X + Scissor.W) - ContextOffsetX;
        OutRect.ScissorMaxY = static_cast<float>(Scissor.Y + Scissor.H) - ContextOffsetY;
        return true;
    };

    const auto QueueImageTextureUploadIfNeeded = [&](const std::uint32_t TextureIdValue) {
        if (TextureIdValue == 0)
        {
            return;
        }

        const UiTextureCacheKey TextureCacheKey{&Context, TextureIdValue};
        const bool HasOpacityMetadata = m_uiTextureHasTransparency.contains(TextureCacheKey);
        if (m_uiTextures.contains(TextureCacheKey) && HasOpacityMetadata)
        {
            return;
        }

        const auto* Image = Context.GetImageData(SnAPI::UI::TextureId{TextureIdValue});
        if (!Image || !Image->Valid || Image->Width <= 0 || Image->Height <= 0 || Image->Pixels.empty())
        {
            return;
        }

        // UI path assumes textures may be translucent; avoid per-pixel alpha scans on CPU.
        constexpr bool HasTransparency = true;
        m_uiTextureHasTransparency[TextureCacheKey] = HasTransparency;

        if (m_uiPendingTextureUploads.contains(TextureCacheKey))
        {
            return;
        }

        auto& Pending = m_uiPendingTextureUploads[TextureCacheKey];
        Pending.Width = static_cast<std::uint32_t>(Image->Width);
        Pending.Height = static_cast<std::uint32_t>(Image->Height);
        Pending.HasTransparency = HasTransparency;
        Pending.Pixels = Image->Pixels;
    };

    for (const auto& Packet : PacketSpan)
    {
        if (const auto* Rects = std::get_if<SnAPI::UI::RectInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Rects)
            {
                QueuedUiRect Rect{};
                Rect.ViewportID = ViewportID;
                Rect.Context = &Context;
                Rect.X = Instance.X - ContextOffsetX;
                Rect.Y = Instance.Y - ContextOffsetY;
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

        if (const auto* Triangles = std::get_if<SnAPI::UI::TriangleInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Triangles)
            {
                const float MinX = std::min({Instance.X0, Instance.X1, Instance.X2});
                const float MaxX = std::max({Instance.X0, Instance.X1, Instance.X2});
                const float MinY = std::min({Instance.Y0, Instance.Y1, Instance.Y2});
                const float MaxY = std::max({Instance.Y0, Instance.Y1, Instance.Y2});
                const float Width = std::max(0.0f, MaxX - MinX);
                const float Height = std::max(0.0f, MaxY - MinY);

                if (Width <= 0.0f || Height <= 0.0f)
                {
                    continue;
                }

                QueuedUiRect Rect{};
                Rect.ViewportID = ViewportID;
                Rect.Context = &Context;
                Rect.X = MinX - ContextOffsetX;
                Rect.Y = MinY - ContextOffsetY;
                Rect.W = Width;
                Rect.H = Height;
                Rect.PrimitiveKind = QueuedUiRect::EPrimitiveKind::Triangle;
                Rect.GlobalZ = GlobalZ;
                ApplyUiColor(Rect, Instance.Fill);

                const float InvWidth = 1.0f / Width;
                const float InvHeight = 1.0f / Height;
                Rect.ShapeData0 = {
                    (Instance.X0 - MinX) * InvWidth,
                    (Instance.Y0 - MinY) * InvHeight,
                    (Instance.X1 - MinX) * InvWidth,
                    (Instance.Y1 - MinY) * InvHeight};
                Rect.ShapeData1 = {
                    (Instance.X2 - MinX) * InvWidth,
                    (Instance.Y2 - MinY) * InvHeight,
                    0.0f,
                    0.0f};

                if (!ApplyUiScissor(Rect, Instance.ScissorMode, Instance.Scissor))
                {
                    continue;
                }

                m_uiQueuedRects.emplace_back(Rect);
                GlobalZ += kUiGlobalZStep;
            }
            continue;
        }

        if (const auto* Circles = std::get_if<SnAPI::UI::CircleInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Circles)
            {
                const float Radius = std::max(0.0f, Instance.Radius);
                if (Radius <= 0.0f)
                {
                    continue;
                }

                QueuedUiRect Rect{};
                Rect.ViewportID = ViewportID;
                Rect.Context = &Context;
                Rect.X = (Instance.CenterX - Radius) - ContextOffsetX;
                Rect.Y = (Instance.CenterY - Radius) - ContextOffsetY;
                Rect.W = Radius * 2.0f;
                Rect.H = Radius * 2.0f;
                Rect.CornerRadius = Radius;
                Rect.BorderThickness = std::max(0.0f, Instance.BorderThickness);
                Rect.PrimitiveKind = QueuedUiRect::EPrimitiveKind::Circle;
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
                Rect.ViewportID = ViewportID;
                Rect.Context = &Context;
                Rect.X = Instance.X - ContextOffsetX;
                Rect.Y = Instance.Y - ContextOffsetY;
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

        if (const auto* Gradients = std::get_if<SnAPI::UI::GradientInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Gradients)
            {
                QueuedUiRect Rect{};
                Rect.ViewportID = ViewportID;
                Rect.Context = &Context;
                Rect.X = Instance.X - ContextOffsetX;
                Rect.Y = Instance.Y - ContextOffsetY;
                Rect.W = Instance.W;
                Rect.H = Instance.H;
                Rect.CornerRadius = std::max(0.0f, Instance.CornerRadius);
                Rect.BorderThickness = std::max(0.0f, Instance.BorderThickness);
                Rect.GlobalZ = GlobalZ;
                ApplyUiBorderColor(Rect, Instance.Border);
                ApplyUiGradient(Rect, Instance.Gradient);
                if (!ApplyUiScissor(Rect, Instance.ScissorMode, Instance.Scissor))
                {
                    continue;
                }

                m_uiQueuedRects.emplace_back(Rect);
                GlobalZ += kUiGlobalZStep;
            }
            continue;
        }

        if (const auto* Shadows = std::get_if<SnAPI::UI::ShadowInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Shadows)
            {
                QueuedUiRect Rect{};
                Rect.ViewportID = ViewportID;
                Rect.Context = &Context;
                Rect.X = Instance.X - ContextOffsetX;
                Rect.Y = Instance.Y - ContextOffsetY;
                Rect.W = Instance.W;
                Rect.H = Instance.H;
                Rect.PrimitiveKind = QueuedUiRect::EPrimitiveKind::Shadow;
                Rect.GlobalZ = GlobalZ;
                ApplyUiColor(Rect, Instance.ShadowColor);
                Rect.ShapeData0 = {
                    std::max(0.0f, Instance.Expansion),
                    std::max(0.0f, Instance.CornerRadius),
                    std::max(0.0f, Instance.Spread),
                    std::max(0.0f, Instance.Blur)};
                if (!ApplyUiScissor(Rect, Instance.ScissorMode, Instance.Scissor))
                {
                    continue;
                }

                m_uiQueuedRects.emplace_back(Rect);
                GlobalZ += kUiGlobalZStep;
            }
            continue;
        }

        if (const auto* Glyphs = std::get_if<SnAPI::UI::GlyphInstanceSpan>(&Packet.Instances))
        {
            for (const auto& Instance : *Glyphs)
            {
                QueuedUiRect Rect{};
                Rect.ViewportID = ViewportID;
                Rect.Context = &Context;
                Rect.X = Instance.X - ContextOffsetX;
                Rect.Y = Instance.Y - ContextOffsetY;
                Rect.W = Instance.W;
                Rect.H = Instance.H;
                Rect.U0 = Instance.U0;
                Rect.V0 = Instance.V0;
                Rect.U1 = Instance.U1;
                Rect.V1 = Instance.V1;
                Rect.FontAtlasTextureHandle = Instance.AtlasTextureHandle;
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

bool RendererSystem::QueueUiRenderPackets(SnAPI::UI::UIContext& Context, const SnAPI::UI::RenderPacketList& Packets)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    std::uint64_t ViewportID = 0;
    {
        GameLockGuard Lock(m_mutex);
        if (!m_graphics)
        {
            return false;
        }

        if (m_graphics->IsUsingDefaultViewport())
        {
            ViewportID = static_cast<std::uint64_t>(m_graphics->DefaultRenderViewportID());
        }
        else
        {
            const auto Viewports = m_graphics->RenderViewportIDs();
            if (!Viewports.empty())
            {
                ViewportID = static_cast<std::uint64_t>(Viewports.front());
            }
        }
    }

    if (ViewportID == 0)
    {
        return false;
    }

    return QueueUiRenderPackets(ViewportID, Context, Packets);
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
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiPacketsQueuedThisFrame = false;
#endif
}

void RendererSystem::ShutdownUnlocked()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_graphics)
    {
        m_graphics->ActiveCamera(nullptr);
    }

    // Release subsystem-owned references to GPU objects before tearing down the graphics API.
    m_lightManager.reset();
    ResetPassPointers();
    m_passGraphRegistered = false;
    m_defaultGBufferMaterial.reset();
    m_defaultShadowMaterial.reset();
    m_defaultFont = nullptr;
    m_defaultFontFallbacksConfigured = false;
    m_textQueue.clear();
#if defined(SNAPI_GF_ENABLE_UI)
    m_uiMaterial.reset();
    m_uiFontMaterial.reset();
    m_uiTriangleMaterial.reset();
    m_uiCircleMaterial.reset();
    m_uiShadowMaterial.reset();
    m_uiFallbackTexture.reset();
    m_uiFallbackMaterialInstance.reset();
    m_uiTriangleMaterialInstance.reset();
    m_uiCircleMaterialInstance.reset();
    m_uiShadowMaterialInstance.reset();
    m_uiFontMaterialInstances.clear();
    m_uiTextures.clear();
    m_uiTextureHasTransparency.clear();
    m_uiTextureMaterialInstances.clear();
    m_uiGradientTextures.clear();
    m_uiGradientMaterialInstances.clear();
    m_uiPendingTextureUploads.clear();
    m_uiQueuedRects.clear();
    m_uiPacketsQueuedThisFrame = false;
#endif
    m_lastWindowWidth = 0.0f;
    m_lastWindowHeight = 0.0f;
    m_hasWindowSizeSnapshot = false;
    m_pendingSwapChainWidth = 0.0f;
    m_pendingSwapChainHeight = 0.0f;
    m_hasPendingSwapChainResize = false;
    m_pendingSwapChainStableFrames = 0;
    m_registeredRenderObjects.clear();

    if (m_graphics)
    {
        SnAPI::Graphics::DestroyGraphicsAPI();
    }

    m_graphics = nullptr;
    m_window.reset();
    m_registeredViewportPassGraphs.clear();
    m_renderViewportPassGraphRevision = 1;
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
    GBufferMaterial->SetFeature(SnAPI::Graphics::GBufferContract::Feature::AlbedoMap, false);
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

bool RendererSystem::EnsureLightManagerInternal()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (m_lightManager)
    {
        return true;
    }
    if (!m_graphics || !m_initialized)
    {
        return false;
    }

    m_lightManager.reset(new Graphics::LightManager());
    return m_lightManager != nullptr;
}

bool RendererSystem::EnsureDefaultLighting()
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!EnsureLightManagerInternal())
    {
        return false;
    }
    if (!m_lightManager)
    {
        return false;
    }
    if (!m_lightManager->GetDirectionalLights().empty())
    {
        return true;
    }

    const auto Sun = m_lightManager->CreateDirectionalLight();
    if (!Sun)
    {
        return false;
    }

    Sun->SetDirection(SnAPI::Vector3DF(-0.5f, -1.0f, -0.3f).normalized());
    Sun->SetColor(SnAPI::ColorF::White());
    Sun->SetIntensity(1.0f);
    Sun->SetCastsShadows(true);
    Sun->SetFeature(SnAPI::Graphics::DirectionalLightContract::Feature::CascadeBlending, true);
    Sun->SetCascadeCount(4);
    Sun->SetShadowMapSize(2048);
    Sun->SetShadowBias(0.005f);
    Sun->SetShadowFarDistance(300.0f);

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

    auto* Library = SnAPI::Graphics::FontLibrary::Instance();
    if (!Library)
    {
        return false;
    }

    namespace fs = std::filesystem;
    const auto ConfigureFallbackFaces = [this, Library](SnAPI::Graphics::FontFace* PrimaryFace) {
        if (!PrimaryFace || m_defaultFontFallbacksConfigured)
        {
            return;
        }

        PrimaryFace->ClearFallbackFaces();

        const std::array<std::string, 11> FallbackFacePaths{
            // General Latin fallback
            m_settings.DefaultFontPath,
            "/usr/share/fonts/TTF/Arial.TTF",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/segoeui.ttf",
            // CJK fallback
            "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
            "C:/Windows/Fonts/msgothic.ttc",
            "C:/Windows/Fonts/simsun.ttc",
            // Emoji fallback
            "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
            "C:/Windows/Fonts/seguiemj.ttf"};

        for (const auto& Candidate : FallbackFacePaths)
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

            auto* FallbackFace = Library->FontFace(Candidate, m_settings.DefaultFontSize);
            if (IsFontRenderable(FallbackFace))
            {
                PrimaryFace->AddFallbackFace(FallbackFace);
            }
        }

        m_defaultFontFallbacksConfigured = true;
    };

    if (IsFontRenderable(m_defaultFont))
    {
        ConfigureFallbackFaces(m_defaultFont);
        return true;
    }

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
            m_defaultFontFallbacksConfigured = false;
            ConfigureFallbackFaces(m_defaultFont);
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

    const bool RecreateRequestedByGraphics = m_graphics->ConsumeSwapChainRecreateRequest(m_window.get());

    if (!m_hasWindowSizeSnapshot)
    {
        m_lastWindowWidth = Width;
        m_lastWindowHeight = Height;
        m_hasWindowSizeSnapshot = true;
        m_pendingSwapChainWidth = Width;
        m_pendingSwapChainHeight = Height;
        m_hasPendingSwapChainResize = RecreateRequestedByGraphics;
        m_pendingSwapChainStableFrames = 0;
        return false;
    }

    const bool MatchesCurrentSwapChain =
        std::fabs(Width - m_lastWindowWidth) <= kWindowSizeEpsilon
        && std::fabs(Height - m_lastWindowHeight) <= kWindowSizeEpsilon;
    if (MatchesCurrentSwapChain && !RecreateRequestedByGraphics)
    {
        m_pendingSwapChainWidth = Width;
        m_pendingSwapChainHeight = Height;
        m_hasPendingSwapChainResize = false;
        m_pendingSwapChainStableFrames = 0;
        return false;
    }

    const bool PendingTargetChanged =
        !m_hasPendingSwapChainResize
        || std::fabs(Width - m_pendingSwapChainWidth) > kWindowSizeEpsilon
        || std::fabs(Height - m_pendingSwapChainHeight) > kWindowSizeEpsilon;

    const bool NeedPendingResizeTracking = !MatchesCurrentSwapChain
                                           || (RecreateRequestedByGraphics && !m_hasPendingSwapChainResize);
    if (NeedPendingResizeTracking && PendingTargetChanged)
    {
        m_pendingSwapChainWidth = Width;
        m_pendingSwapChainHeight = Height;
        m_hasPendingSwapChainResize = true;
        m_pendingSwapChainStableFrames = 0;
        // Window border drag can change size continuously even when button state
        // is not visible to the app (platform-dependent). Never recreate on the
        // same frame a new resize delta arrives.
        return false;
    }

    if (!m_hasPendingSwapChainResize)
    {
        m_pendingSwapChainStableFrames = 0;
        return false;
    }

    if (m_pendingSwapChainStableFrames < std::numeric_limits<std::uint32_t>::max())
    {
        ++m_pendingSwapChainStableFrames;
    }
    if (m_pendingSwapChainStableFrames < kPendingSwapChainStableFrameThreshold)
    {
        return false;
    }

    if (IsPrimaryMouseButtonDown())
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
    if (!m_uiTriangleMaterial)
    {
        auto UiTriangleMaterial = std::make_shared<SnAPI::Graphics::UIMaterial>("UITriangleMaterial");
        UiTriangleMaterial->BakeCompileTimeParams();
        m_uiTriangleMaterial = std::move(UiTriangleMaterial);
    }
    if (!m_uiCircleMaterial)
    {
        auto UiCircleMaterial = std::make_shared<SnAPI::Graphics::UIMaterial>("UICircleMaterial");
        UiCircleMaterial->BakeCompileTimeParams();
        m_uiCircleMaterial = std::move(UiCircleMaterial);
    }
    if (!m_uiShadowMaterial)
    {
        auto UiShadowMaterial = std::make_shared<SnAPI::Graphics::UIMaterial>("UIShadowMaterial");
        UiShadowMaterial->BakeCompileTimeParams();
        m_uiShadowMaterial = std::move(UiShadowMaterial);
    }

    if (!m_uiFallbackTexture)
    {
        SnAPI::Graphics::ImageCreateInfo ImageCI =
            SnAPI::Graphics::ImageCreateInfo::VisualDefault(SnAPI::Size2DU{1u, 1u}, SnAPI::Graphics::ETextureFormat::R8G8B8A8_Unorm, 1);
        if (ImageCI.SamplerCreateInfo)
        {
            auto& Sampler = *ImageCI.SamplerCreateInfo;
            Sampler.AddressModeU = SnAPI::Graphics::EImageSamplerAddressMode::ClampToEdge;
            Sampler.AddressModeV = SnAPI::Graphics::EImageSamplerAddressMode::ClampToEdge;
            Sampler.AddressModeW = SnAPI::Graphics::EImageSamplerAddressMode::ClampToEdge;
            Sampler.MipmapMode = SnAPI::Graphics::EImageSamplerMipmapMode::Nearest;
            Sampler.MinLod = 0.0f;
            Sampler.MaxLod = 0.0f;
        }
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

    if (!m_uiTriangleMaterialInstance)
    {
        if (!m_uiTriangleMaterial)
        {
            return false;
        }
        m_uiTriangleMaterialInstance = m_uiTriangleMaterial->CreateMaterialInstance();
        if (!m_uiTriangleMaterialInstance)
        {
            return false;
        }
    }

    if (!m_uiCircleMaterialInstance)
    {
        if (!m_uiCircleMaterial)
        {
            return false;
        }
        m_uiCircleMaterialInstance = m_uiCircleMaterial->CreateMaterialInstance();
        if (!m_uiCircleMaterialInstance)
        {
            return false;
        }
    }

    if (!m_uiShadowMaterialInstance)
    {
        if (!m_uiShadowMaterial)
        {
            return false;
        }
        m_uiShadowMaterialInstance = m_uiShadowMaterial->CreateMaterialInstance();
        if (!m_uiShadowMaterialInstance)
        {
            return false;
        }
    }

    return true;
}

std::shared_ptr<SnAPI::Graphics::MaterialInstance> RendererSystem::ResolveUiMaterialForTexture(const SnAPI::UI::UIContext& Context,
                                                                                                const std::uint32_t TextureId)
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

    const UiTextureCacheKey TextureCacheKey{&Context, TextureId};

    if (const auto MaterialIt = m_uiTextureMaterialInstances.find(TextureCacheKey);
        MaterialIt != m_uiTextureMaterialInstances.end())
    {
        return MaterialIt->second;
    }

    auto PendingIt = m_uiPendingTextureUploads.find(TextureCacheKey);
    if (PendingIt == m_uiPendingTextureUploads.end())
    {
        return m_uiFallbackMaterialInstance;
    }

    auto& Pending = PendingIt->second;
    if (Pending.Width == 0 || Pending.Height == 0 || Pending.Pixels.empty())
    {
        return m_uiFallbackMaterialInstance;
    }

    SnAPI::Graphics::ImageCreateInfo ImageCI = SnAPI::Graphics::ImageCreateInfo::VisualDefault(
        SnAPI::Size2DU{Pending.Width, Pending.Height}, SnAPI::Graphics::ETextureFormat::R8G8B8A8_Unorm, 1);
    if (ImageCI.SamplerCreateInfo)
    {
        auto& Sampler = *ImageCI.SamplerCreateInfo;
        Sampler.AddressModeU = SnAPI::Graphics::EImageSamplerAddressMode::ClampToEdge;
        Sampler.AddressModeV = SnAPI::Graphics::EImageSamplerAddressMode::ClampToEdge;
        Sampler.AddressModeW = SnAPI::Graphics::EImageSamplerAddressMode::ClampToEdge;
        Sampler.MipmapMode = SnAPI::Graphics::EImageSamplerMipmapMode::Nearest;
        Sampler.MinLod = 0.0f;
        Sampler.MaxLod = 0.0f;
    }
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
    m_uiTextures[TextureCacheKey] = std::move(Texture);
    m_uiTextureHasTransparency[TextureCacheKey] = Pending.HasTransparency;
    m_uiTextureMaterialInstances[TextureCacheKey] = MaterialInstance;
    m_uiPendingTextureUploads.erase(PendingIt);
    return MaterialInstance;
}

std::shared_ptr<SnAPI::Graphics::MaterialInstance> RendererSystem::ResolveUiMaterialForGradient(const QueuedUiRect& Entry)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!EnsureUiMaterialResources())
    {
        return {};
    }

    if (!Entry.UseGradient)
    {
        return m_uiFallbackMaterialInstance;
    }

    const std::size_t StopCount = std::min<std::size_t>(QueuedUiRect::MaxGradientStops, Entry.GradientStopCount);
    if (StopCount == 0)
    {
        return m_uiFallbackMaterialInstance;
    }

    std::array<float, QueuedUiRect::MaxGradientStops> StopPositions{};
    std::array<std::uint32_t, QueuedUiRect::MaxGradientStops> StopColors{};
    for (std::size_t Index = 0; Index < StopCount; ++Index)
    {
        StopPositions[Index] = ClampUnit(Entry.GradientStops[Index]);
        StopColors[Index] = Entry.GradientColors[Index];
    }

    // Canonicalize stop ordering for deterministic cache keys.
    for (std::size_t Outer = 1; Outer < StopCount; ++Outer)
    {
        std::size_t Inner = Outer;
        while (Inner > 0 && StopPositions[Inner] < StopPositions[Inner - 1])
        {
            std::swap(StopPositions[Inner], StopPositions[Inner - 1]);
            std::swap(StopColors[Inner], StopColors[Inner - 1]);
            --Inner;
        }
    }

    UiGradientCacheKey CacheKey{};
    CacheKey.StartX = Entry.GradientStartX;
    CacheKey.StartY = Entry.GradientStartY;
    CacheKey.EndX = Entry.GradientEndX;
    CacheKey.EndY = Entry.GradientEndY;
    CacheKey.StopCount = static_cast<std::uint8_t>(StopCount);
    for (std::size_t Index = 0; Index < StopCount; ++Index)
    {
        CacheKey.Stops[Index] = StopPositions[Index];
        CacheKey.Colors[Index] = StopColors[Index];
    }

    if (const auto It = m_uiGradientMaterialInstances.find(CacheKey); It != m_uiGradientMaterialInstances.end())
    {
        return It->second;
    }

    const auto DecodeChannel = [](const std::uint32_t Packed, const int Shift) -> float {
        constexpr float kInv = 1.0f / 255.0f;
        return static_cast<float>((Packed >> Shift) & 0xffu) * kInv;
    };

    const auto SampleGradient = [&](const float T) {
        const float ClampedT = ClampUnit(T);

        if (StopCount == 1 || ClampedT <= StopPositions[0])
        {
            return StopColors[0];
        }
        if (ClampedT >= StopPositions[StopCount - 1])
        {
            return StopColors[StopCount - 1];
        }

        std::size_t SegmentIndex = 0;
        for (std::size_t Index = 0; Index + 1 < StopCount; ++Index)
        {
            if (ClampedT <= StopPositions[Index + 1])
            {
                SegmentIndex = Index;
                break;
            }
        }

        const float LeftPos = StopPositions[SegmentIndex];
        const float RightPos = StopPositions[SegmentIndex + 1];
        const std::uint32_t LeftColor = StopColors[SegmentIndex];
        const std::uint32_t RightColor = StopColors[SegmentIndex + 1];

        const float Denom = std::max(1e-6f, RightPos - LeftPos);
        const float Alpha = ClampUnit((ClampedT - LeftPos) / Denom);
        const float InvAlpha = 1.0f - Alpha;

        const auto BlendChannel = [&](const int Shift) -> std::uint8_t {
            const float Left = DecodeChannel(LeftColor, Shift);
            const float Right = DecodeChannel(RightColor, Shift);
            const float Mixed = std::clamp(Left * InvAlpha + Right * Alpha, 0.0f, 1.0f);
            return static_cast<std::uint8_t>(std::round(Mixed * 255.0f));
        };

        return (static_cast<std::uint32_t>(BlendChannel(24)) << 24u) |
               (static_cast<std::uint32_t>(BlendChannel(16)) << 16u) |
               (static_cast<std::uint32_t>(BlendChannel(8)) << 8u) |
               static_cast<std::uint32_t>(BlendChannel(0));
    };

    std::vector<std::uint8_t> PixelData{};
    PixelData.resize(static_cast<std::size_t>(kUiGradientTextureSize) *
                     static_cast<std::size_t>(kUiGradientTextureSize) * 4u);

    const float StartX = Entry.GradientStartX;
    const float StartY = Entry.GradientStartY;
    const float EndX = Entry.GradientEndX;
    const float EndY = Entry.GradientEndY;
    const float DeltaX = EndX - StartX;
    const float DeltaY = EndY - StartY;
    const float GradientLengthSq = DeltaX * DeltaX + DeltaY * DeltaY;

    for (std::uint32_t Y = 0; Y < kUiGradientTextureSize; ++Y)
    {
        for (std::uint32_t X = 0; X < kUiGradientTextureSize; ++X)
        {
            const float U = (static_cast<float>(X) + 0.5f) / static_cast<float>(kUiGradientTextureSize);
            const float V = (static_cast<float>(Y) + 0.5f) / static_cast<float>(kUiGradientTextureSize);
            float T = 0.0f;
            if (GradientLengthSq > 1e-6f)
            {
                T = ((U - StartX) * DeltaX + (V - StartY) * DeltaY) / GradientLengthSq;
            }

            const std::uint32_t Packed = SampleGradient(T);
            const std::size_t PixelIndex =
                (static_cast<std::size_t>(Y) * static_cast<std::size_t>(kUiGradientTextureSize) + static_cast<std::size_t>(X)) * 4u;
            PixelData[PixelIndex + 0] = static_cast<std::uint8_t>((Packed >> 24u) & 0xffu);
            PixelData[PixelIndex + 1] = static_cast<std::uint8_t>((Packed >> 16u) & 0xffu);
            PixelData[PixelIndex + 2] = static_cast<std::uint8_t>((Packed >> 8u) & 0xffu);
            PixelData[PixelIndex + 3] = static_cast<std::uint8_t>(Packed & 0xffu);
        }
    }

    SnAPI::Graphics::ImageCreateInfo ImageCI = SnAPI::Graphics::ImageCreateInfo::VisualDefault(
        SnAPI::Size2DU{kUiGradientTextureSize, kUiGradientTextureSize},
        SnAPI::Graphics::ETextureFormat::R8G8B8A8_Unorm,
        1);
    if (ImageCI.SamplerCreateInfo)
    {
        auto& Sampler = *ImageCI.SamplerCreateInfo;
        Sampler.AddressModeU = SnAPI::Graphics::EImageSamplerAddressMode::ClampToEdge;
        Sampler.AddressModeV = SnAPI::Graphics::EImageSamplerAddressMode::ClampToEdge;
        Sampler.AddressModeW = SnAPI::Graphics::EImageSamplerAddressMode::ClampToEdge;
        Sampler.MipmapMode = SnAPI::Graphics::EImageSamplerMipmapMode::Nearest;
        Sampler.MinLod = 0.0f;
        Sampler.MaxLod = 0.0f;
    }
    ImageCI.Data = std::move(PixelData);
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
    m_uiGradientTextures.emplace(CacheKey, std::move(Texture));
    m_uiGradientMaterialInstances.emplace(CacheKey, MaterialInstance);
    return MaterialInstance;
}

std::shared_ptr<SnAPI::Graphics::MaterialInstance> RendererSystem::ResolveUiFontMaterialInstance(const std::uint64_t AtlasTextureHandle)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!EnsureUiMaterialResources())
    {
        return {};
    }

    if (!m_uiFontMaterial)
    {
        return m_uiFallbackMaterialInstance;
    }

    auto* AtlasTexture = reinterpret_cast<SnAPI::Graphics::IGPUImage*>(
        static_cast<std::uintptr_t>(AtlasTextureHandle));
    if (!AtlasTexture)
    {
        return m_uiFallbackMaterialInstance;
    }

    if (const auto It = m_uiFontMaterialInstances.find(AtlasTexture); It != m_uiFontMaterialInstances.end())
    {
        return It->second;
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
        m_uiPacketsQueuedThisFrame = false;
        return;
    }

    constexpr float kOpaqueAlphaEpsilon = 1e-5f;
    const auto IsNearlyOpaque = [](const float Alpha) {
        return std::fabs(Alpha - 1.0f) <= kOpaqueAlphaEpsilon;
    };
    const auto GradientHasTransparency = [&](const QueuedUiRect& Entry) {
        const std::size_t StopCount = std::min<std::size_t>(QueuedUiRect::MaxGradientStops, static_cast<std::size_t>(Entry.GradientStopCount));
        if (StopCount == 0)
        {
            return false;
        }

        for (std::size_t StopIndex = 0; StopIndex < StopCount; ++StopIndex)
        {
            const std::uint32_t PackedColor = Entry.GradientColors[StopIndex];
            const std::uint8_t Alpha = static_cast<std::uint8_t>(PackedColor & 0xffu);
            if (Alpha < 255u)
            {
                return true;
            }
        }
        return false;
    };
    const auto HasUiTransparency = [&](const QueuedUiRect& Entry) {
        // These procedural primitives rely on edge coverage in the shader,
        // so they must be alpha blended to avoid writing partial-alpha edges
        // directly into the target when "opaque" batching is used.
        if (Entry.PrimitiveKind == QueuedUiRect::EPrimitiveKind::Shadow ||
            Entry.PrimitiveKind == QueuedUiRect::EPrimitiveKind::Triangle ||
            Entry.PrimitiveKind == QueuedUiRect::EPrimitiveKind::Circle)
        {
            return true;
        }

        if (Entry.UseFontAtlas)
        {
            return true;
        }

        // Rounded clipping and visible borders are AA'd in UIShadingModel and
        // therefore produce fractional coverage at edges.
        if (Entry.CornerRadius > kOpaqueAlphaEpsilon)
        {
            return true;
        }

        if (Entry.BorderThickness > kOpaqueAlphaEpsilon &&
            Entry.BorderA > kOpaqueAlphaEpsilon)
        {
            return true;
        }

        if (!IsNearlyOpaque(Entry.A))
        {
            return true;
        }

        if (Entry.UseGradient)
        {
            return GradientHasTransparency(Entry);
        }

        if (Entry.TextureId != 0)
        {
            const UiTextureCacheKey Key{Entry.Context, Entry.TextureId};
            if (const auto It = m_uiTextureHasTransparency.find(Key); It != m_uiTextureHasTransparency.end())
            {
                return It->second;
            }

            // No metadata yet; be conservative until the image is analyzed.
            return true;
        }

        // Default to opaque path so layered UI surfaces do not get over-accumulated in OIT.
        return false;
    };

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
        Rect.Rect.ShapeData0 = SnAPI::Vector4DF{Entry.ShapeData0[0], Entry.ShapeData0[1], Entry.ShapeData0[2], Entry.ShapeData0[3]};
        Rect.Rect.ShapeData1 = SnAPI::Vector4DF{Entry.ShapeData1[0], Entry.ShapeData1[1], Entry.ShapeData1[2], Entry.ShapeData1[3]};
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
        if (Entry.PrimitiveKind == QueuedUiRect::EPrimitiveKind::Shadow)
        {
            MaterialInstance = m_uiShadowMaterialInstance;
        }
        else if (Entry.PrimitiveKind == QueuedUiRect::EPrimitiveKind::Triangle)
        {
            MaterialInstance = m_uiTriangleMaterialInstance;
        }
        else if (Entry.PrimitiveKind == QueuedUiRect::EPrimitiveKind::Circle)
        {
            MaterialInstance = m_uiCircleMaterialInstance;
        }
        else if (Entry.UseFontAtlas)
        {
            MaterialInstance = ResolveUiFontMaterialInstance(Entry.FontAtlasTextureHandle);
        }
        else if (Entry.UseGradient)
        {
            MaterialInstance = ResolveUiMaterialForGradient(Entry);
        }
        else
        {
            if (Entry.Context)
            {
                MaterialInstance = ResolveUiMaterialForTexture(*Entry.Context, Entry.TextureId);
            }
            else
            {
                MaterialInstance = m_uiFallbackMaterialInstance;
            }
        }

        if (!MaterialInstance)
        {
            MaterialInstance = m_uiFallbackMaterialInstance;
        }

        m_graphics->DrawTexturedRectangleForViewport(
            static_cast<SnAPI::Graphics::RenderViewportID>(Entry.ViewportID),
            Rect,
            MaterialInstance,
            HasUiTransparency(Entry),
            false);
    }

    m_uiQueuedRects.clear();
    m_uiPacketsQueuedThisFrame = false;
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
    m_pendingSwapChainWidth = m_lastWindowWidth;
    m_pendingSwapChainHeight = m_lastWindowHeight;
    m_hasPendingSwapChainResize = false;
    m_pendingSwapChainStableFrames = 0;
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
    m_pendingSwapChainWidth = m_lastWindowWidth;
    m_pendingSwapChainHeight = m_lastWindowHeight;
    m_hasPendingSwapChainResize = false;
    m_pendingSwapChainStableFrames = 0;
    m_window = std::move(Window);
    return true;
}

bool RendererSystem::RegisterRenderViewportPassGraphUnlocked(const std::uint64_t ViewportID,
                                                             const ERenderViewportPassGraphPreset Preset,
                                                             const bool TrackDefaultPassPointers)
{
    SNAPI_GF_PROFILE_FUNCTION("Rendering");
    if (!m_graphics || ViewportID == 0)
    {
        return false;
    }

    using namespace SnAPI::Graphics;
    const auto RendererViewportID = static_cast<RenderViewportID>(ViewportID);
    const bool ViewportExists = m_graphics->GetRenderViewportConfig(RendererViewportID).has_value();
    if (!ViewportExists)
    {
        SNAPI_RENDERER_LOG_WARNING("Cannot register pass graph preset: render viewport %llu does not exist.",
                                   static_cast<unsigned long long>(ViewportID));
        return false;
    }

    const auto RefreshTrackedPassPointers = [this, RendererViewportID, TrackDefaultPassPointers]() {
        if (!TrackDefaultPassPointers || !m_graphics)
        {
            return;
        }

        ResetPassPointers();
        m_gbufferPass = static_cast<GBufferPass*>(m_graphics->GetRenderPass(RendererViewportID, ERenderPassType::GBuffer));
        m_ssaoPass = static_cast<SSAOPass*>(m_graphics->GetRenderPass(RendererViewportID, ERenderPassType::SSAO));
        m_ssrPass = static_cast<SSRPass*>(m_graphics->GetRenderPass(RendererViewportID, ERenderPassType::SSR));
        m_bloomPass = static_cast<BloomPass*>(m_graphics->GetRenderPass(RendererViewportID, ERenderPassType::Bloom));
    };

    const auto SetViewportFinalColorResource = [this, RendererViewportID](std::string FinalColorResourceName) {
        if (!m_graphics)
        {
            return;
        }

        const auto ExistingConfig = m_graphics->GetRenderViewportConfig(RendererViewportID);
        if (!ExistingConfig.has_value() || ExistingConfig->FinalColorResourceName == FinalColorResourceName)
        {
            return;
        }

        auto UpdatedConfig = *ExistingConfig;
        UpdatedConfig.FinalColorResourceName = std::move(FinalColorResourceName);
        if (!m_graphics->SetRenderViewportConfig(RendererViewportID, UpdatedConfig))
        {
            SNAPI_RENDERER_LOG_WARNING("Failed to set final color resource for viewport %llu.",
                                       static_cast<unsigned long long>(RendererViewportID));
        }
    };

    if (const auto ExistingIt = m_registeredViewportPassGraphs.find(ViewportID);
        ExistingIt != m_registeredViewportPassGraphs.end())
    {
        if (ExistingIt->second == Preset)
        {
            RefreshTrackedPassPointers();
            if (TrackDefaultPassPointers && Preset != ERenderViewportPassGraphPreset::None)
            {
                m_passGraphRegistered = true;
            }
            return true;
        }

        SNAPI_RENDERER_LOG_WARNING("Render viewport %llu already has pass graph preset %u; refusing to replace with %u.",
                                   static_cast<unsigned long long>(ViewportID),
                                   static_cast<unsigned>(ExistingIt->second),
                                   static_cast<unsigned>(Preset));
        return false;
    }

    if (Preset == ERenderViewportPassGraphPreset::None)
    {
        m_registeredViewportPassGraphs.emplace(ViewportID, Preset);
        return true;
    }

    if (Preset == ERenderViewportPassGraphPreset::UiPresentOnly)
    {
        auto UIPassProperties = PassProperties{
            {AutoGeneratedPass::PropertyNames::PassName.data(), "UI Pass"},
            {AutoGeneratedPass::PropertyNames::MaterialsShadingModel.data(), "UIShadingModel"},
            {AutoGeneratedPass::PropertyNames::MaterialsModule.data(), "DefaultUIMaterial"},
            {AutoGeneratedPass::PropertyNames::PassDepthConfig.data(),
             DepthConfig{
                 .WriteDepth = true,
                 .DepthTest = true,
                 .ClearDepth = 0.0f,
                 .WriteResourceName = "UI_Depth",
                 .DepthCompareOp = ECompareOp::Greater}},
        };

        m_graphics->RegisterPass(RendererViewportID, std::make_unique<UIPass>(std::move(UIPassProperties)));
        SetViewportFinalColorResource("UI_Opaque");

        m_registeredViewportPassGraphs.emplace(ViewportID, Preset);
        m_graphics->RequestFrameGraphRebuild(RendererViewportID);
        ++m_renderViewportPassGraphRevision;
        RefreshTrackedPassPointers();
        if (TrackDefaultPassPointers)
        {
            m_passGraphRegistered = true;
        }
        return true;
    }

    if (Preset != ERenderViewportPassGraphPreset::DefaultWorld)
    {
        SNAPI_RENDERER_LOG_WARNING("Unsupported viewport pass graph preset value %u.", static_cast<unsigned>(Preset));
        return false;
    }

    if (!EnsureLightManagerInternal())
    {
        return false;
    }

    if (m_settings.CreateDefaultLighting && !EnsureDefaultLighting())
    {
        return false;
    }

    auto* ActiveLightManager = m_lightManager.get();
    SNAPI_RENDERER_DEBUG_ASSERT_MSG(ActiveLightManager, "Light manager is null");

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
        {AutoGeneratedPass::PropertyNames::PassDepthConfig.data(),
         DepthConfig{
             .WriteDepth = true,
             .DepthTest = true,
             .ClearDepth = 0.0f,
             .WriteResourceName = "UI_Depth",
             .DepthCompareOp = ECompareOp::Greater}},
    };

    auto CompositePassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "Composite Pass"},
        {FullScreenPass::PropertyNames::MaterialsShadingModel.data(), "PostProcessShadingModel"},
        {FullScreenPass::PropertyNames::MaterialsModule.data(), "CompositeMaterial"},
        // CompositeMaterial expects UI_Out; source it directly from UIPass UI_Opaque.
        {AutoGeneratedPass::PropertyNames::PassInputResourceNameOverrides.data(),
         ResourceNameMappings{{"UI_Out", "UI_Opaque"}}},
    };

    auto AtmosphereCompositePassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "Atmosphere Composite Pass"},
        {FullScreenPass::PropertyNames::MaterialsShadingModel.data(), "PostProcessShadingModel"},
        {FullScreenPass::PropertyNames::MaterialsModule.data(), "AtmosCompositeMaterial"},
        {AutoGeneratedPass::PropertyNames::PassDepthConfig.data(),
         DepthConfig{.WriteDepth = false, .SampleDepth = true, .DepthTest = false, .ReadResourceName = "GBuffer_Depth", .ReadLayout = EImageLayout::DepthStencilReadOnlyOptimal}}};

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
    Shadow->SetLightManager(ActiveLightManager);
    m_graphics->RegisterPass(RendererViewportID, std::move(Shadow));

    auto* RegisteredGBufferPass = static_cast<GBufferPass*>(
        m_graphics->RegisterPass(RendererViewportID, std::make_unique<GBufferPass>(std::move(GBufferPassProperties))));
    if (TrackDefaultPassPointers)
    {
        m_gbufferPass = RegisteredGBufferPass;
    }

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
        auto* RegisteredSSAOPass = static_cast<SSAOPass*>(m_graphics->RegisterPass(RendererViewportID, std::move(SSAO)));
        if (TrackDefaultPassPointers)
        {
            m_ssaoPass = RegisteredSSAOPass;
        }
    }

    auto HiZPassProperties = PassProperties{
        {AutoGeneratedPass::PropertyNames::PassName.data(), "HiZ Pass"},
        {AutoGeneratedPass::PropertyNames::PassDepthConfig.data(),
         DepthConfig{.WriteDepth = false, .SampleDepth = true, .DepthTest = false, .ReadResourceName = "GBuffer_Depth", .ReadLayout = EImageLayout::DepthStencilReadOnlyOptimal}}};
    m_graphics->RegisterPass(RendererViewportID, std::make_unique<HiZPass>(std::move(HiZPassProperties)));

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
        auto* RegisteredSSRPass = static_cast<SSRPass*>(m_graphics->RegisterPass(RendererViewportID, std::move(SSR)));
        if (TrackDefaultPassPointers)
        {
            m_ssrPass = RegisteredSSRPass;
        }

        auto SSRCompositePassProperties = PassProperties{
            {AutoGeneratedPass::PropertyNames::PassName.data(), "SSR Composite Pass"},
            {FullScreenPass::PropertyNames::MaterialsShadingModel.data(), "PostProcessShadingModel"},
            {FullScreenPass::PropertyNames::MaterialsModule.data(), "SSRCompositeMaterial"},
        };
        m_graphics->RegisterPass(RendererViewportID, std::make_unique<CompositePass>(std::move(SSRCompositePassProperties)));
    }

    auto DeferredShading = std::make_unique<DeferredShadingPass>(std::move(ShadingPassProperties));
    DeferredShading->SetLightManager(ActiveLightManager);
    m_graphics->RegisterPass(RendererViewportID, std::move(DeferredShading));

    m_graphics->RegisterPass(RendererViewportID, std::make_unique<ToneMapPass>(std::move(ToneMapPassProperties)));
    m_graphics->RegisterPass(RendererViewportID, std::make_unique<CompositePass>(std::move(CompositePassProperties)));
    m_graphics->RegisterPass(RendererViewportID, std::make_unique<UIPass>(std::move(UIPassProperties)));

    if (m_settings.EnableAtmosphere)
    {
        m_graphics->RegisterPass(RendererViewportID, std::make_unique<AtmospherePass>(std::move(AtmospherePassProperties)));
        m_graphics->RegisterPass(RendererViewportID, std::make_unique<CompositePass>(std::move(AtmosphereCompositePassProperties)));
    }

    if (m_settings.EnableBloom)
    {
        auto BloomPassProperties = PassProperties{
            {AutoGeneratedPass::PropertyNames::PassName.data(), "Bloom Pass"},
        };
        auto Bloom = std::make_unique<BloomPass>(std::move(BloomPassProperties));
        Bloom->SetMipCount(5);
        auto* RegisteredBloomPass = static_cast<BloomPass*>(m_graphics->RegisterPass(RendererViewportID, std::move(Bloom)));
        if (TrackDefaultPassPointers)
        {
            m_bloomPass = RegisteredBloomPass;
        }
    }

    std::string FinalColorResourceName = "Composite_Out";
    if (m_settings.EnableAtmosphere)
    {
        FinalColorResourceName = "AtmosComposite_Out";
    }
    if (m_settings.EnableBloom)
    {
        FinalColorResourceName = "BloomComposite_Out";
    }
    SetViewportFinalColorResource(std::move(FinalColorResourceName));

    m_registeredViewportPassGraphs.emplace(ViewportID, Preset);
    m_graphics->RequestFrameGraphRebuild(RendererViewportID);
    ++m_renderViewportPassGraphRevision;
    RefreshTrackedPassPointers();
    if (TrackDefaultPassPointers)
    {
        m_passGraphRegistered = true;
    }
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

    m_graphics->UseDefaultViewport(true);
    const auto DefaultViewportID = static_cast<std::uint64_t>(m_graphics->DefaultRenderViewportID());
    return RegisterRenderViewportPassGraphUnlocked(DefaultViewportID, ERenderViewportPassGraphPreset::DefaultWorld, true);
}

void RendererSystem::ResetPassPointers()
{
    m_ssaoPass = nullptr;
    m_ssrPass = nullptr;
    m_bloomPass = nullptr;
    m_gbufferPass = nullptr;
}

Graphics::LightManager* RendererSystem::LightManager()
{
    return m_lightManager.get();
}

const Graphics::LightManager* RendererSystem::LightManager() const
{
    return m_lightManager.get();
}

Graphics::LightManager* RendererSystem::EnsureLightManager()
{
    return EnsureLightManagerInternal() ? m_lightManager.get() : nullptr;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
