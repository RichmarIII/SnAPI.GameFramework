#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstddef>
#include <cstdint>
#include <array>
#include "GameThreading.h"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SnAPI::Graphics
{
class ICamera;
class IRenderObject;
class Material;
class MaterialInstance;
class VulkanGraphicsAPI;
struct ViewportFit;
enum class ERenderPassType;
struct WindowBase;
class LightManager;
class SSAOPass;
class SSRPass;
class BloomPass;
class GBufferPass;
class FontFace;
class IHighLevelPass;
struct IGPUImage;
} // namespace SnAPI::Graphics

#if defined(SNAPI_GF_ENABLE_UI)
namespace SnAPI::UI
{
class UIContext;
class RenderPacketList;
} // namespace SnAPI::UI
#endif

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
 * @brief Built-in pass graph presets for virtual render viewports.
 */
enum class ERenderViewportPassGraphPreset : uint8_t
{
    None = 0, /**< @brief Do not auto-register any passes. */
    UiPresentOnly, /**< @brief Register only UI + Present passes (editor shell style viewport). */
    DefaultWorld /**< @brief Register default world stack (shadow/gbuffer/deferred/post/ui/present + optional effects). */
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
     * @brief Set default virtual render viewport for the renderer.
     * @param ViewPort Top-left pixel viewport rectangle in window space.
     * @return True when renderer is initialized and viewport was applied.
     */
    bool SetViewPort(const SnAPI::Graphics::ViewportFit& ViewPort);

    /**
     * @brief Reset default virtual render viewport to full-window behavior.
     * @return True when renderer is initialized and viewport was reset.
     */
    bool ClearViewPort();

    /**
     * @brief Enable or disable renderer default viewport runtime (ID = `DefaultRenderViewportID()`).
     * @param Enabled True to enable/create default viewport; false to disable/remove it.
     * @return True when renderer is initialized and state was applied.
     */
    bool UseDefaultRenderViewport(bool Enabled = true);

    /**
     * @brief Query whether the renderer default viewport runtime is currently active.
     * @return True when default viewport runtime exists and is enabled for use.
     */
    [[nodiscard]] bool IsUsingDefaultRenderViewport() const;

    /**
     * @brief Set a pass-specific viewport override.
     * @param PassType Render pass type to override.
     * @param ViewPort Top-left pixel viewport rectangle for that pass.
     * @return True when renderer is initialized and override was applied.
     */
    bool SetPassViewPort(SnAPI::Graphics::ERenderPassType PassType, const SnAPI::Graphics::ViewportFit& ViewPort);

    /**
     * @brief Clear a pass-specific viewport override.
     * @param PassType Render pass type to clear.
     * @return True when renderer is initialized and override was cleared.
     */
    bool ClearPassViewPort(SnAPI::Graphics::ERenderPassType PassType);

    /**
     * @brief Clear all pass-specific viewport overrides.
     * @return True when renderer is initialized and overrides were cleared.
     */
    bool ClearPassViewPorts();

    /**
     * @brief Create a new virtual render viewport.
     * @param Name Debug/display name for viewport runtime.
     * @param X Output rect X (window-space).
     * @param Y Output rect Y (window-space).
     * @param Width Output rect width (window-space).
     * @param Height Output rect height (window-space).
     * @param RenderWidth Local render extent width.
     * @param RenderHeight Local render extent height.
     * @param Camera Optional viewport camera override (nullptr = use active camera).
     * @param Enabled Whether the viewport is enabled.
     * @param OutViewportID Created viewport ID on success.
     * @return True when viewport was created.
     */
    bool CreateRenderViewport(std::string Name,
                              float X,
                              float Y,
                              float Width,
                              float Height,
                              std::uint32_t RenderWidth,
                              std::uint32_t RenderHeight,
                              SnAPI::Graphics::ICamera* Camera,
                              bool Enabled,
                              std::uint64_t& OutViewportID);

    /**
     * @brief Update an existing virtual render viewport configuration.
     * @return True when viewport config was updated.
     */
    bool UpdateRenderViewport(std::uint64_t ViewportID,
                              std::string Name,
                              float X,
                              float Y,
                              float Width,
                              float Height,
                              std::uint32_t RenderWidth,
                              std::uint32_t RenderHeight,
                              SnAPI::Graphics::ICamera* Camera,
                              bool Enabled);

    /**
     * @brief Destroy a virtual render viewport.
     * @param ViewportID Viewport identifier.
     * @return True when viewport was destroyed.
     */
    bool DestroyRenderViewport(std::uint64_t ViewportID);

    /**
     * @brief Check whether a render viewport currently exists.
     * @param ViewportID Target viewport identifier.
     * @return True when viewport exists.
     */
    [[nodiscard]] bool HasRenderViewport(std::uint64_t ViewportID) const;

    /**
     * @brief Set draw/composition index for a render viewport.
     * @param ViewportID Target viewport identifier.
     * @param Index Zero-based render order index (lower renders first).
     * @return True when viewport exists and index was applied.
     */
    bool SetRenderViewportIndex(std::uint64_t ViewportID, std::size_t Index);

    /**
     * @brief Query draw/composition index for a render viewport.
     * @param ViewportID Target viewport identifier.
     * @return Zero-based index when viewport exists.
     */
    [[nodiscard]] std::optional<std::size_t> RenderViewportIndex(std::uint64_t ViewportID) const;

    /**
     * @brief Register a built-in pass graph preset for a viewport.
     * @param ViewportID Target viewport identifier.
     * @param Preset Pass-graph preset.
     * @return True when registration succeeded or was already applied with same preset.
     */
    bool RegisterRenderViewportPassGraph(std::uint64_t ViewportID, ERenderViewportPassGraphPreset Preset);

    /**
     * @brief Set global DAG input-name remaps for one virtual viewport.
     * @param ViewportID Target viewport identifier.
     * @param Overrides Mapping pairs {FromName, ToName}.
     */
    bool SetRenderViewportGlobalInputNameOverrides(std::uint64_t ViewportID, std::vector<std::pair<std::string, std::string>> Overrides);

    /**
     * @brief Set global DAG output-name remaps for one virtual viewport.
     * @param ViewportID Target viewport identifier.
     * @param Overrides Mapping pairs {FromName, ToName}.
     */
    bool SetRenderViewportGlobalOutputNameOverrides(std::uint64_t ViewportID, std::vector<std::pair<std::string, std::string>> Overrides);

    /**
     * @brief Set per-pass DAG input-name remaps for one virtual viewport.
     * @param ViewportID Target viewport identifier.
     * @param Pass Target pass pointer.
     * @param Overrides Mapping pairs {FromName, ToName}.
     */
    bool SetRenderViewportPassInputNameOverrides(std::uint64_t ViewportID,
                                                 const SnAPI::Graphics::IHighLevelPass* Pass,
                                                 std::vector<std::pair<std::string, std::string>> Overrides);

    /**
     * @brief Set per-pass DAG output-name remaps for one virtual viewport.
     * @param ViewportID Target viewport identifier.
     * @param Pass Target pass pointer.
     * @param Overrides Mapping pairs {FromName, ToName}.
     */
    bool SetRenderViewportPassOutputNameOverrides(std::uint64_t ViewportID,
                                                  const SnAPI::Graphics::IHighLevelPass* Pass,
                                                  std::vector<std::pair<std::string, std::string>> Overrides);

    /**
     * @brief Clear per-pass DAG name remaps for one virtual viewport.
     * @param ViewportID Target viewport identifier.
     * @param Pass Target pass pointer.
     */
    bool ClearRenderViewportPassNameOverrides(std::uint64_t ViewportID, const SnAPI::Graphics::IHighLevelPass* Pass);

    /**
     * @brief Clear all DAG name remaps for one virtual viewport.
     * @param ViewportID Target viewport identifier.
     */
    bool ClearRenderViewportNameOverrides(std::uint64_t ViewportID);

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
    bool ConfigureRenderObjectPasses(SnAPI::Graphics::IRenderObject& RenderObject, bool Visible, bool CastShadows) const;

    /**
     * @brief Monotonic revision for render-viewport pass graph topology changes.
     * @return Current pass-graph revision value.
     * @remarks
     * Components can cache this value to know when viewport pass graphs were added and
     * pass enable masks should be re-applied to existing render objects.
     */
    std::uint64_t RenderViewportPassGraphRevision() const;

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
     * @brief Ensure a renderable default font exists and return it.
     * @return Non-owning font pointer, or nullptr when default font cannot be resolved.
     */
    SnAPI::Graphics::FontFace* EnsureDefaultFontFace();

#if defined(SNAPI_GF_ENABLE_UI)
    /**
     * @brief Queue one frame of UI render packets for renderer submission.
     * @param ViewportID Target render viewport id that should consume these packets.
     * @param Context UI context used to resolve packet texture ids into image payloads.
     * @param Packets Frame packet list generated by `UISystem::BuildRenderPackets`.
     * @return True when packets were accepted for the next renderer `EndFrame` submit.
     * @remarks
     * This performs CPU-side translation from SnAPI.UI packet formats into renderer
     * instanced-rectangle draw records and caches texture upload payloads for
     * deferred GPU creation during `EndFrame` after `BeginFrame` is active.
     */
    bool QueueUiRenderPackets(std::uint64_t ViewportID, SnAPI::UI::UIContext& Context, const SnAPI::UI::RenderPacketList& Packets);

    /**
     * @brief Queue one frame of UI render packets for renderer default viewport submission.
     * @param Context UI context used to resolve packet texture ids into image payloads.
     * @param Packets Frame packet list generated by `UISystem::BuildRenderPackets`.
     * @return True when packets were accepted.
     */
    bool QueueUiRenderPackets(SnAPI::UI::UIContext& Context, const SnAPI::UI::RenderPacketList& Packets);
#endif

    /**
     * @brief End-of-frame renderer maintenance and frame submission.
     * @remarks
     * If a renderer window is owned, runs `BeginFrame/EndFrame`.
     * Always saves active camera previous-frame state for temporal effects.
     */
    void EndFrame();

    /**
     * @brief Gets the Light Manager
     *
     * @return The LightManager
     */
    const Graphics::LightManager* LightManager()const;

private:
#if defined(SNAPI_GF_ENABLE_UI)
    struct QueuedUiRect;
#endif

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
#if defined(SNAPI_GF_ENABLE_UI)
    bool EnsureUiMaterialResources();
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> ResolveUiMaterialForTexture(const SnAPI::UI::UIContext& Context,
                                                                                    std::uint32_t TextureId);
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> ResolveUiMaterialForGradient(const QueuedUiRect& Entry);
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> ResolveUiFontMaterialInstance(std::uint64_t AtlasTextureHandle);
    void FlushQueuedUiPackets();
#endif
    bool CreateWindowResources();
    bool RegisterDefaultPassGraph();
    bool RegisterRenderViewportPassGraphUnlocked(std::uint64_t ViewportID, ERenderViewportPassGraphPreset Preset, bool TrackDefaultPassPointers);
    void ResetPassPointers();

    struct TextRequest
    {
        std::string Text{};
        float X = 0.0f;
        float Y = 0.0f;
    };

#if defined(SNAPI_GF_ENABLE_UI)
    struct UiTextureCacheKey
    {
        const SnAPI::UI::UIContext* Context = nullptr;
        std::uint32_t TextureId = 0;

        friend bool operator==(const UiTextureCacheKey& Left, const UiTextureCacheKey& Right) = default;
    };

    struct UiTextureCacheKeyHasher
    {
        std::size_t operator()(const UiTextureCacheKey& Key) const noexcept
        {
            const std::size_t ContextHash = std::hash<const void*>{}(static_cast<const void*>(Key.Context));
            const std::size_t TextureHash = std::hash<std::uint32_t>{}(Key.TextureId);
            return ContextHash ^ (TextureHash + 0x9e3779b97f4a7c15ull + (ContextHash << 6u) + (ContextHash >> 2u));
        }
    };

    struct QueuedUiRect
    {
        enum class EPrimitiveKind : std::uint8_t
        {
            Rectangle = 0,
            Triangle = 1,
            Circle = 2,
            Shadow = 3
        };

        static constexpr std::size_t MaxGradientStops = 10;

        std::uint64_t ViewportID = 0;
        const SnAPI::UI::UIContext* Context = nullptr;
        float X = 0.0f;
        float Y = 0.0f;
        float W = 0.0f;
        float H = 0.0f;
        float CornerRadius = 0.0f;
        float BorderThickness = 0.0f;
        float U0 = 0.0f;
        float V0 = 0.0f;
        float U1 = 1.0f;
        float V1 = 1.0f;
        float R = 1.0f;
        float G = 1.0f;
        float B = 1.0f;
        float A = 1.0f;
        float BorderR = 0.0f;
        float BorderG = 0.0f;
        float BorderB = 0.0f;
        float BorderA = 0.0f;
        float ScissorMinX = 0.0f;
        float ScissorMinY = 0.0f;
        float ScissorMaxX = 0.0f;
        float ScissorMaxY = 0.0f;
        bool HasScissor = false;
        EPrimitiveKind PrimitiveKind = EPrimitiveKind::Rectangle;
        std::uint32_t TextureId = 0;
        std::uint64_t FontAtlasTextureHandle = 0;
        bool UseFontAtlas = false;
        bool UseGradient = false;
        std::array<float, 4> ShapeData0{};
        std::array<float, 4> ShapeData1{};
        float GradientStartX = 0.0f;
        float GradientStartY = 0.0f;
        float GradientEndX = 1.0f;
        float GradientEndY = 0.0f;
        std::uint8_t GradientStopCount = 0;
        std::array<float, MaxGradientStops> GradientStops{};
        std::array<std::uint32_t, MaxGradientStops> GradientColors{};
        float GlobalZ = 0.0f;
    };

    struct UiGradientCacheKey
    {
        float StartX = 0.0f;
        float StartY = 0.0f;
        float EndX = 1.0f;
        float EndY = 0.0f;
        std::uint8_t StopCount = 0;
        std::array<float, QueuedUiRect::MaxGradientStops> Stops{};
        std::array<std::uint32_t, QueuedUiRect::MaxGradientStops> Colors{};

        friend bool operator==(const UiGradientCacheKey& Left, const UiGradientCacheKey& Right) = default;
    };

    struct UiGradientCacheKeyHasher
    {
        std::size_t operator()(const UiGradientCacheKey& Key) const noexcept
        {
            std::size_t Seed = 0;
            const auto Mix = [&Seed](const std::size_t Value) {
                Seed ^= Value + 0x9e3779b97f4a7c15ull + (Seed << 6u) + (Seed >> 2u);
            };

            Mix(std::hash<float>{}(Key.StartX));
            Mix(std::hash<float>{}(Key.StartY));
            Mix(std::hash<float>{}(Key.EndX));
            Mix(std::hash<float>{}(Key.EndY));
            Mix(std::hash<std::uint8_t>{}(Key.StopCount));

            for (std::size_t Index = 0; Index < static_cast<std::size_t>(Key.StopCount); ++Index)
            {
                Mix(std::hash<float>{}(Key.Stops[Index]));
                Mix(std::hash<std::uint32_t>{}(Key.Colors[Index]));
            }

            return Seed;
        }
    };

    struct PendingUiTextureUpload
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        bool HasTransparency = true;
        std::vector<std::uint8_t> Pixels{};
    };
#endif

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
    bool m_defaultFontFallbacksConfigured = false; /**< @brief True once fallback face chain is attached to the default font. */
    std::vector<TextRequest> m_textQueue{}; /**< @brief Pending text draw requests flushed in EndFrame. */
#if defined(SNAPI_GF_ENABLE_UI)
    std::shared_ptr<SnAPI::Graphics::Material> m_uiMaterial{}; /**< @brief Shared UI material used to create texture-bound UI material instances. */
    std::shared_ptr<SnAPI::Graphics::Material> m_uiFontMaterial{}; /**< @brief Shared UI font material used for glyph coverage sampling. */
    std::shared_ptr<SnAPI::Graphics::Material> m_uiTriangleMaterial{}; /**< @brief Shared UI triangle material used for vector triangle masking. */
    std::shared_ptr<SnAPI::Graphics::Material> m_uiCircleMaterial{}; /**< @brief Shared UI circle material used for vector circle fills. */
    std::shared_ptr<SnAPI::Graphics::Material> m_uiShadowMaterial{}; /**< @brief Shared UI shadow material used for procedural drop-shadow rendering. */
    std::shared_ptr<SnAPI::Graphics::IGPUImage> m_uiFallbackTexture{}; /**< @brief White 1x1 fallback texture used for rects and missing images. */
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> m_uiFallbackMaterialInstance{}; /**< @brief Material instance bound to fallback white texture. */
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> m_uiTriangleMaterialInstance{}; /**< @brief Reused immutable triangle material instance. */
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> m_uiCircleMaterialInstance{}; /**< @brief Reused immutable circle material instance. */
    std::shared_ptr<SnAPI::Graphics::MaterialInstance> m_uiShadowMaterialInstance{}; /**< @brief Reused immutable shadow material instance. */
    std::unordered_map<SnAPI::Graphics::IGPUImage*, std::shared_ptr<SnAPI::Graphics::MaterialInstance>> m_uiFontMaterialInstances{}; /**< @brief Cached immutable UI material instances keyed by font atlas texture pointer. */
    std::unordered_map<UiTextureCacheKey, std::shared_ptr<SnAPI::Graphics::IGPUImage>, UiTextureCacheKeyHasher> m_uiTextures{}; /**< @brief UI GPU images keyed by (UIContext, texture-id) to avoid cross-context id collisions. */
    std::unordered_map<UiTextureCacheKey, bool, UiTextureCacheKeyHasher> m_uiTextureHasTransparency{}; /**< @brief UI texture transparency hint keyed by (UIContext, texture-id); UI defaults this to true to avoid CPU alpha scans. */
    std::unordered_map<UiTextureCacheKey, std::shared_ptr<SnAPI::Graphics::MaterialInstance>, UiTextureCacheKeyHasher> m_uiTextureMaterialInstances{}; /**< @brief UI texture material instances keyed by (UIContext, texture-id). */
    std::unordered_map<UiGradientCacheKey, std::shared_ptr<SnAPI::Graphics::IGPUImage>, UiGradientCacheKeyHasher> m_uiGradientTextures{}; /**< @brief Cached generated gradient textures keyed by gradient definition. */
    std::unordered_map<UiGradientCacheKey, std::shared_ptr<SnAPI::Graphics::MaterialInstance>, UiGradientCacheKeyHasher> m_uiGradientMaterialInstances{}; /**< @brief Cached material instances for generated gradient textures. */
    std::unordered_map<UiTextureCacheKey, PendingUiTextureUpload, UiTextureCacheKeyHasher> m_uiPendingTextureUploads{}; /**< @brief Deferred CPU-side UI image payloads keyed by (UIContext, texture-id). */
    std::vector<QueuedUiRect> m_uiQueuedRects{}; /**< @brief Per-frame translated UI rectangles awaiting renderer draw submission. */
    bool m_uiPacketsQueuedThisFrame = false; /**< @brief True once at least one UI context queued packets for the current frame. */
#endif
    float m_lastWindowWidth = 0.0f; /**< @brief Last known window width used for resize detection. */
    float m_lastWindowHeight = 0.0f; /**< @brief Last known window height used for resize detection. */
    bool m_hasWindowSizeSnapshot = false; /**< @brief True after first window-size sample. */
    float m_pendingSwapChainWidth = 0.0f; /**< @brief Latest observed window width waiting for swapchain recreation. */
    float m_pendingSwapChainHeight = 0.0f; /**< @brief Latest observed window height waiting for swapchain recreation. */
    bool m_hasPendingSwapChainResize = false; /**< @brief True while window size has diverged and resize is being coalesced. */
    std::uint32_t m_pendingSwapChainStableFrames = 0; /**< @brief Consecutive frames where pending swapchain target stayed unchanged. */
    std::vector<std::weak_ptr<SnAPI::Graphics::IRenderObject>> m_registeredRenderObjects{}; /**< @brief Registered render objects that need end-of-frame state snapshots. */
    std::unordered_map<std::uint64_t, ERenderViewportPassGraphPreset> m_registeredViewportPassGraphs{}; /**< @brief Tracks preset assignment per viewport to prevent duplicate pass registration. */
    std::uint64_t m_renderViewportPassGraphRevision = 1; /**< @brief Monotonic revision incremented when viewport pass-graph topology changes. */
    bool m_initialized = false; /**< @brief True when backend lifecycle is active through this subsystem. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
