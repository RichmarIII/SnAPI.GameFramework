#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include "GameThreading.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace SnAPI::Graphics
{
class ICamera;
class IRenderObject;
class Material;
class MaterialInstance;
class VulkanGraphicsAPI;
struct WindowBase;
class LightManager;
class SSAOPass;
class SSRPass;
class BloomPass;
class GBufferPass;
class FontFace;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{

/**
 * @brief Bootstrap settings for world-owned renderer integration.
 */
struct RendererBootstrapSettings
{
    bool CreateGraphicsApi = true; /**< @brief Create and initialize VulkanGraphicsAPI singleton on initialize. */
    bool CreateWindow = true; /**< @brief Create an SDL window and initialize renderer resources for it. */
    std::string WindowTitle = "SnAPI.GameFramework"; /**< @brief Main renderer window title. */
    float WindowWidth = 1280.0f; /**< @brief Main renderer window width. */
    float WindowHeight = 720.0f; /**< @brief Main renderer window height. */
    bool FullScreen = false; /**< @brief Start window in fullscreen mode. */
    bool Resizable = true; /**< @brief Allow window resizing. */
    bool Borderless = false; /**< @brief Use borderless window mode. */
    bool Visible = true; /**< @brief Start window visible. */
    bool Maximized = false; /**< @brief Start window maximized. */
    bool Minimized = false; /**< @brief Start window minimized. */
    bool Closeable = true; /**< @brief Allow platform close actions. */
    bool AllowTransparency = true; /**< @brief Enable transparent compositor support when available. */
    bool CreateDefaultLighting = true; /**< @brief Create a default directional light used by shadow/deferred passes. */
    bool RegisterDefaultPassGraph = true; /**< @brief Register the default renderer pass DAG (shadow/gbuffer/deferred/present). */
    bool EnableSsao = true; /**< @brief Register SSAO pass chain in default pass graph. */
    bool EnableSsr = true; /**< @brief Register SSR + composite passes in default pass graph. */
    bool EnableBloom = true; /**< @brief Register bloom pass in default pass graph. */
    bool EnableAtmosphere = true; /**< @brief Register atmosphere + composite passes in default pass graph. */
    bool AutoHandleSwapChainResize = true; /**< @brief Detect window-size changes and recreate swapchain automatically. */
    bool AutoFallbackOnOutOfMemory = true; /**< @brief Retry renderer init with reduced settings when device-memory allocation fails. */
    float OutOfMemoryFallbackWindowWidth = 1920.0f; /**< @brief Maximum retry width used during out-of-memory fallback. */
    float OutOfMemoryFallbackWindowHeight = 1080.0f; /**< @brief Maximum retry height used during out-of-memory fallback. */
    bool ForceWindowedOnOutOfMemory = true; /**< @brief Force windowed mode during out-of-memory fallback. */
    bool DisableTransparencyOnOutOfMemory = true; /**< @brief Disable transparent window mode during out-of-memory fallback. */
    bool DisableExpensivePassesOnOutOfMemory = true; /**< @brief Disable SSAO/SSR/Bloom/Atmosphere during out-of-memory fallback. */
    bool DisableEnvironmentProbeOnOutOfMemory = true; /**< @brief Disable default environment probe during out-of-memory fallback. */
    bool CreateDefaultEnvironmentProbe = true; /**< @brief Register a default environment probe for scene capture-based IBL. */
    float DefaultEnvironmentProbeX = 0.0f; /**< @brief Default environment probe world X position. */
    float DefaultEnvironmentProbeY = 0.0f; /**< @brief Default environment probe world Y position. */
    float DefaultEnvironmentProbeZ = 0.0f; /**< @brief Default environment probe world Z position. */
    bool PreloadDefaultFont = true; /**< @brief Attempt to load a default UI font so `QueueText` works out of the box. */
    std::string DefaultFontPath = "/usr/share/fonts/TTF/Arial.TTF"; /**< @brief Optional default font path; fallback list is used when unavailable. */
    std::uint32_t DefaultFontSize = 24; /**< @brief Default font pixel size. */
    bool CreateDefaultMaterials = true; /**< @brief Build default GBuffer + Shadow materials for mesh components. */
};

/**
 * @brief World-owned adapter over SnAPI.Renderer runtime globals.
 * @remarks
 * This subsystem owns high-level renderer lifecycle for GameFramework worlds
 * while reusing SnAPI.Renderer's singleton-style backend.
 */
class RendererSystem final : public ITaskDispatcher
{
public:
    using WorkTask = std::function<void(RendererSystem&)>;
    using CompletionTask = std::function<void(const TaskHandle&)>;

    /** @brief Construct an uninitialized renderer system. */
    RendererSystem() = default;
    /** @brief Destructor; releases renderer resources when initialized. */
    ~RendererSystem();

    RendererSystem(const RendererSystem&) = delete;
    RendererSystem& operator=(const RendererSystem&) = delete;

    RendererSystem(RendererSystem&& Other) noexcept;
    RendererSystem& operator=(RendererSystem&& Other) noexcept;

    /**
     * @brief Enqueue work on the renderer system thread.
     * @param InTask Work callback executed on renderer-thread affinity.
     * @param OnComplete Optional completion callback marshaled to caller dispatcher.
     * @return Task handle for wait/cancel polling.
     */
    TaskHandle EnqueueTask(WorkTask InTask, CompletionTask OnComplete = {});

    /**
     * @brief Enqueue a generic thread task for dispatcher marshalling.
     * @param InTask Callback to execute on this system thread.
     */
    void EnqueueThreadTask(std::function<void()> InTask) override;

    /**
     * @brief Execute all queued tasks on the renderer thread.
     */
    void ExecuteQueuedTasks();

    /**
     * @brief Initialize renderer using default bootstrap settings.
     * @return True if initialization succeeds or is already initialized.
     */
    bool Initialize();

    /**
     * @brief Initialize renderer with explicit bootstrap settings.
     * @param Settings Renderer bootstrap settings.
     * @return True if initialization succeeds or is already initialized.
     */
    bool Initialize(const RendererBootstrapSettings& Settings);

    /**
     * @brief Shutdown renderer resources owned through this subsystem.
     */
    void Shutdown();

    /**
     * @brief Check whether renderer backend is available.
     */
    bool IsInitialized() const;

    /**
     * @brief Access active graphics backend.
     * @return VulkanGraphicsAPI pointer or nullptr.
     */
    SnAPI::Graphics::VulkanGraphicsAPI* Graphics();
    const SnAPI::Graphics::VulkanGraphicsAPI* Graphics() const;

    /**
     * @brief Access the primary renderer window created by this system.
     * @return Window pointer or nullptr when no window is owned.
     */
    SnAPI::Graphics::WindowBase* Window();
    const SnAPI::Graphics::WindowBase* Window() const;

    /**
     * @brief Check whether a renderer window exists and is currently open.
     */
    bool HasOpenWindow() const;

    /**
     * @brief Set active camera used by renderer.
     * @param Camera Camera pointer (nullable).
     * @return True if renderer is initialized and assignment was applied.
     */
    bool SetActiveCamera(SnAPI::Graphics::ICamera* Camera);

    /**
     * @brief Access active renderer camera.
     * @return Camera pointer or nullptr.
     */
    SnAPI::Graphics::ICamera* ActiveCamera() const;

    /**
     * @brief Register a render object for renderer draw submission.
     * @param RenderObject Weak render object reference.
     * @return True when render-object registration request was submitted.
     */
    bool RegisterRenderObject(const std::weak_ptr<SnAPI::Graphics::IRenderObject>& RenderObject);

    /**
     * @brief Populate default material instances for a render object.
     * @param RenderObject Render object to update.
     * @return True when default materials were assigned.
     */
    bool ApplyDefaultMaterials(SnAPI::Graphics::IRenderObject& RenderObject);

    /**
     * @brief Access the lazily-created default GBuffer material.
     * @return Shared default GBuffer material or nullptr when unavailable.
     */
    std::shared_ptr<SnAPI::Graphics::Material> DefaultGBufferMaterial();

    /**
     * @brief Access the lazily-created default shadow material.
     * @return Shared default shadow material or nullptr when unavailable.
     */
    std::shared_ptr<SnAPI::Graphics::Material> DefaultShadowMaterial();

    /**
     * @brief Configure standard world pass visibility for a render object.
     * @param RenderObject Render object to configure.
     * @param Visible True to enable gbuffer rendering.
     * @param CastShadows True to enable shadow pass rendering.
     * @return True when renderer was initialized and pass state was applied.
     */
    bool ConfigureRenderObjectPasses(SnAPI::Graphics::IRenderObject& RenderObject, bool Visible, bool CastShadows);

    /**
     * @brief Force swapchain recreation for the owned window.
     * @return True when request was applied.
     */
    bool RecreateSwapChain();

    /**
     * @brief Load and set default font used by `QueueText`.
     * @param FontPath Font file path.
     * @param FontSize Font pixel size.
     * @return True if font was loaded.
     */
    bool LoadDefaultFont(const std::string& FontPath, std::uint32_t FontSize = 24);

    /**
     * @brief Queue screen-space text for rendering during next frame submit.
     * @remarks Uses default font configured through settings or `LoadDefaultFont`.
     * @return True if text was queued.
     */
    bool QueueText(std::string Text, float X = 0.0f, float Y = 0.0f);

    /**
     * @brief Check whether a default font is currently available.
     */
    bool HasDefaultFont() const;

    /**
     * @brief End-of-frame renderer maintenance and frame submission.
     * @remarks
     * If a renderer window is owned, runs `BeginFrame/EndFrame`.
     * Always saves active camera previous-frame state for temporal effects.
     */
    void EndFrame();

private:
    bool InitializeUnlocked();
    void ApplyOutOfMemoryFallbackSettings();
    bool RecreateSwapChainForCurrentWindowUnlocked();
    struct WindowDeleter
    {
        void operator()(SnAPI::Graphics::WindowBase* Window) const;
    };

    struct LightManagerDeleter
    {
        void operator()(SnAPI::Graphics::LightManager* Manager) const;
    };

    void ShutdownUnlocked();
    bool EnsureDefaultMaterials();
    bool EnsureDefaultLighting();
    bool EnsureDefaultEnvironmentProbe();
    bool EnsureDefaultFont();
    bool HandleWindowResizeIfNeeded();
    void FlushQueuedText();
    bool CreateWindowResources();
    bool RegisterDefaultPassGraph();
    void ResetPassPointers();

    struct TextRequest
    {
        std::string Text{};
        float X = 0.0f;
        float Y = 0.0f;
    };

    mutable GameMutex m_mutex{}; /**< @brief Renderer-system thread affinity guard. */
    TSystemTaskQueue<RendererSystem> m_taskQueue{}; /**< @brief Cross-thread task handoff queue (real lock only on enqueue). */
    RendererBootstrapSettings m_settings{}; /**< @brief Active bootstrap settings snapshot. */
    SnAPI::Graphics::VulkanGraphicsAPI* m_graphics = nullptr; /**< @brief Non-owning pointer to active renderer singleton instance. */
    std::unique_ptr<SnAPI::Graphics::WindowBase, WindowDeleter> m_window{}; /**< @brief Optional world-owned renderer window. */
    std::unique_ptr<SnAPI::Graphics::LightManager, LightManagerDeleter> m_lightManager{}; /**< @brief Optional world-owned light manager for default pass graph. */
    SnAPI::Graphics::SSAOPass* m_ssaoPass = nullptr; /**< @brief Non-owning pointer to default SSAO pass when registered. */
    SnAPI::Graphics::SSRPass* m_ssrPass = nullptr; /**< @brief Non-owning pointer to default SSR pass when registered. */
    SnAPI::Graphics::BloomPass* m_bloomPass = nullptr; /**< @brief Non-owning pointer to default bloom pass when registered. */
    SnAPI::Graphics::GBufferPass* m_gbufferPass = nullptr; /**< @brief Non-owning pointer to default GBuffer pass when registered. */
    bool m_passGraphRegistered = false; /**< @brief True once default pass DAG has been registered. */
    std::shared_ptr<SnAPI::Graphics::Material> m_defaultGBufferMaterial{}; /**< @brief Default material assigned by mesh components. */
    std::shared_ptr<SnAPI::Graphics::Material> m_defaultShadowMaterial{}; /**< @brief Default shadow material assigned by mesh components. */
    SnAPI::Graphics::FontFace* m_defaultFont = nullptr; /**< @brief Non-owning default font pointer managed by FontLibrary cache. */
    std::vector<TextRequest> m_textQueue{}; /**< @brief Pending text draw requests flushed in EndFrame. */
    float m_lastWindowWidth = 0.0f; /**< @brief Last known window width used for resize detection. */
    float m_lastWindowHeight = 0.0f; /**< @brief Last known window height used for resize detection. */
    bool m_hasWindowSizeSnapshot = false; /**< @brief True after first window-size sample. */
    std::vector<std::weak_ptr<SnAPI::Graphics::IRenderObject>> m_registeredRenderObjects{}; /**< @brief Registered render objects that need end-of-frame state snapshots. */
    bool m_initialized = false; /**< @brief True when backend lifecycle is active through this subsystem. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
