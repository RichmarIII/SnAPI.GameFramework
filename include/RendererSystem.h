#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstddef>
#include <cstdint>
#include <array>
#include <filesystem>
#include "GameThreading.h"
#include "Uuid.h"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SnAPI/Math/LinearAlgebra.h>

#include "TypeName.h"
#include "ReflectionAnnotations.h"
#include "Rendering/GameRenderCamera.h"
#include "Rendering/GameRenderDebugLine.h"
#include "Rendering/GameRenderLight.h"
#include "Rendering/GameRenderMesh.h"
#include "Rendering/GameRenderObject.h"
#include "Rendering/GameRenderWindow.h"

namespace SnAPI::Renderer
{
struct DirectionalLightDesc;
struct PrimitiveMeshData;
} // namespace SnAPI::Renderer

#if defined(SNAPI_GF_ENABLE_UI)
namespace SnAPI::UI
{
class IFontMetrics;
class UIContext;
class RenderPacketList;
} // namespace SnAPI::UI
#endif

namespace SnAPI::GameFramework
{

struct RuntimeMeshData;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Bootstrap settings for world-owned renderer integration.
 *
 * `RendererBootstrapSettings` defines how `RendererSystem` bootstraps the renderer backend,
 * optional window resources, default feature profiles, default scene helpers, and convenience
 * assets such as fallback materials and the default font.
 *
 * Core semantics:
 * - `CreateRendererRuntime` controls whether the Renderer.New runtime and backend are created at all
 * - `CreateWindow` only affects the subsystem-owned platform window path
 * - default pass/light/font/material toggles are convenience bootstrap steps, not permanent feature locks
 * - out-of-memory fallback flags mutate the effective settings used for a second initialization attempt
 *
 * Window-state note:
 * - Window creation flags are best-effort platform requests
 * - the current implementation also maximizes the created window after creation, so final platform state should be treated as implementation-defined rather than guaranteed solely by the flag values
 *
 * Units:
 * - window sizes are in platform window pixels
 * - `DefaultFontSize` is in font pixels
 * - default environment probe coordinates are in world units
 *
 * @see RendererSystem
 */
SnType()
struct RendererBootstrapSettings
{
    SnField(SnKey("CreateRendererRuntime"))
    bool CreateRendererRuntime = true; /**< @brief Create and initialize the Renderer.New runtime and backend on initialize. */
    SnField(SnKey("CreateWindow"))
    bool CreateWindow = true; /**< @brief Create an SDL window and initialize renderer resources for it. */
    SnField(SnKey("WindowTitle"))
    std::string WindowTitle = "SnAPI.GameFramework"; /**< @brief Main renderer window title. */
    SnField(SnKey("WindowWidth"))
    float WindowWidth = 1280.0f; /**< @brief Main renderer window width. */
    SnField(SnKey("WindowHeight"))
    float WindowHeight = 720.0f; /**< @brief Main renderer window height. */
    SnField(SnKey("FullScreen"))
    bool FullScreen = false; /**< @brief Start window in fullscreen mode. */
    SnField(SnKey("Resizable"))
    bool Resizable = true; /**< @brief Allow window resizing. */
    SnField(SnKey("Borderless"))
    bool Borderless = false; /**< @brief Use borderless window mode. */
    SnField(SnKey("Visible"))
    bool Visible = true; /**< @brief Start window visible. */
    SnField(SnKey("Maximized"))
    bool Maximized = false; /**< @brief Start window maximized. */
    SnField(SnKey("Minimized"))
    bool Minimized = false; /**< @brief Start window minimized. */
    SnField(SnKey("Closeable"))
    bool Closeable = true; /**< @brief Allow platform close actions. */
    SnField(SnKey("AllowTransparency"))
    bool AllowTransparency = true; /**< @brief Enable transparent compositor support when available. */
    SnField(SnKey("CreateDefaultLighting"))
    bool CreateDefaultLighting = false; /**< @brief Create a default directional light used by shadow/deferred passes. */
    SnField(SnKey("ApplyDefaultFeatureProfile"))
    bool ApplyDefaultFeatureProfile = true; /**< @brief Apply the default renderer feature profile (shadow/gbuffer/deferred/present). */
    SnField(SnKey("EnableSsao"))
    bool EnableSsao = true; /**< @brief Register the Renderer.New ambient-occlusion feature chain in the default feature profile. */
    SnField(SnKey("EnableSsgi"))
    bool EnableSsgi = true; /**< @brief Register SSGI trace/filter/composite passes in default feature profile. */
    SnField(SnKey("EnableSsr"))
    bool EnableSsr = true; /**< @brief Register SSR + composite passes in default feature profile. */
    SnField(SnKey("EnableTaa"))
    bool EnableTaa = true; /**< @brief Register full-resolution temporal anti-aliasing in default feature profile. */
    SnField(SnKey("EnableBloom"))
    bool EnableBloom = true; /**< @brief Register bloom pass in default feature profile. */
    SnField(SnKey("EnableAtmosphere"))
    bool EnableAtmosphere = true; /**< @brief Register atmosphere + composite passes in default feature profile. */
    SnField(SnKey("EnableHeightFog"))
    bool EnableHeightFog = true; /**< @brief Register analytic height fog pass in default feature profile. */
    SnField(SnKey("AtmosphereWorldMode"))
    bool AtmosphereWorldMode = false; /**< @brief Enable planet-scale atmosphere coordinates (`WORLD=1`); false uses regular-scene mode. */
    SnField(SnKey("AutoHandleSurfaceResize"))
    bool AutoHandleSurfaceResize = true; /**< @brief Detect window-size changes and recreate surface automatically. */
    SnField(SnKey("AutoFallbackOnOutOfMemory"))
    bool AutoFallbackOnOutOfMemory = true; /**< @brief Retry renderer init with reduced settings when device-memory allocation fails. */
    SnField(SnKey("OutOfMemoryFallbackWindowWidth"))
    float OutOfMemoryFallbackWindowWidth = 1920.0f; /**< @brief Maximum retry width used during out-of-memory fallback. */
    SnField(SnKey("OutOfMemoryFallbackWindowHeight"))
    float OutOfMemoryFallbackWindowHeight = 1080.0f; /**< @brief Maximum retry height used during out-of-memory fallback. */
    SnField(SnKey("ForceWindowedOnOutOfMemory"))
    bool ForceWindowedOnOutOfMemory = true; /**< @brief Force windowed mode during out-of-memory fallback. */
    SnField(SnKey("DisableTransparencyOnOutOfMemory"))
    bool DisableTransparencyOnOutOfMemory = true; /**< @brief Disable transparent window mode during out-of-memory fallback. */
    SnField(SnKey("DisableExpensivePassesOnOutOfMemory"))
    bool DisableExpensivePassesOnOutOfMemory = true; /**< @brief Disable SSAO/SSGI/SSR/Bloom/Atmosphere during out-of-memory fallback. */
    SnField(SnKey("DisableEnvironmentProbeOnOutOfMemory"))
    bool DisableEnvironmentProbeOnOutOfMemory = true; /**< @brief Disable default environment probe during out-of-memory fallback. */
    SnField(SnKey("CreateDefaultEnvironmentProbe"))
    bool CreateDefaultEnvironmentProbe = true; /**< @brief Register a default environment probe for scene capture-based IBL. */
    SnField(SnKey("DefaultEnvironmentProbeX"))
    float DefaultEnvironmentProbeX = 0.0f; /**< @brief Default environment probe world X position. */
    SnField(SnKey("DefaultEnvironmentProbeY"))
    float DefaultEnvironmentProbeY = 0.0f; /**< @brief Default environment probe world Y position. */
    SnField(SnKey("DefaultEnvironmentProbeZ"))
    float DefaultEnvironmentProbeZ = 0.0f; /**< @brief Default environment probe world Z position. */
    SnField(SnKey("PreloadDefaultFont"))
    bool PreloadDefaultFont = true; /**< @brief Attempt to load a default UI font so `QueueText` works out of the box. */
    SnField(SnKey("DefaultFontPath"))
    std::string DefaultFontPath = "/usr/share/fonts/TTF/Arial.TTF"; /**< @brief Optional default font path; fallback list is used when unavailable. */
    SnField(SnKey("DefaultFontSize"))
    std::uint32_t DefaultFontSize = 24; /**< @brief Default font pixel size. */
    SnField(SnKey("CreateDefaultMaterials"))
    bool CreateDefaultMaterials = true; /**< @brief Build default GBuffer + Shadow materials for mesh components. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Built-in feature profile presets for virtual render viewports.
 *
 * Each preset describes a named, one-time feature selection recipe for a virtual
 * render viewport. Presets are intentionally coarse-grained so gameplay/editor code
 * can ask for a standard viewport topology without selecting renderer passes manually.
 *
 * Semantics:
 * - one preset may be registered per viewport through `RendererSystem::ApplyRenderViewportFeatureProfile(...)`
 * - registering the same preset again is idempotent
 * - attempting to replace one preset with a different preset on the same viewport is rejected
 *
 * @see RendererSystem::ApplyRenderViewportFeatureProfile
 */
SnType()
enum class EGameRenderFeatureProfile : uint8_t
{
    None = 0, /**< @brief Do not apply a render feature profile. */
    DefaultWorld, /**< @brief Render the default world stack (shadow/gbuffer/deferred/post/ui/present + optional effects). */
#if defined(WITH_EDITOR) && WITH_EDITOR
    EditorWorld /**< @brief Render the editor world stack (default world + editor id/overlay passes). */
#endif
};
SNAPI_DEFINE_TYPE_NAME(EGameRenderFeatureProfile, "SnAPI::GameFramework::EGameRenderFeatureProfile")

/**
 * @ingroup SnAPI_GameFramework
 * @brief Deferred shading debug-view settings retained by the renderer facade.
 */
struct RendererDeferredShadingFeatureSettings
{
    bool DebugMotionVectors{false};
    bool DebugNormals{false};
    bool DebugAlbedo{false};
    bool DebugAO{false};
    bool DebugRoughness{false};
    bool DebugMetallic{false};
    bool DebugDepth{false};
    bool DebugTextureCoords{false};
    bool DebugDirectLighting{false};
    bool DebugGI{false};
    bool DebugSpecular{false};
    bool DebugLighting{false};
};

struct RendererSsaoFeatureSettings
{
    float Radius{0.5f};
    float Bias{0.025f};
    float Intensity{1.0f};
    float MaxDistance{4.0f};
    std::uint32_t SliceCount{3u};
    std::uint32_t StepsPerSlice{3u};
    float FalloffStart{0.0f};
    float FalloffEnd{1.0f};
    float MaxPixelRadius{80.0f};
    float Thickness{0.25f};
    float DenoiseBlurBeta{8.0f};
    float TemporalBlendFactor{0.08f};
    float DisocclusionThreshold{0.02f};
    float VelocityWeight{12.0f};
};

struct RendererSsgiFeatureSettings
{
    float Intensity{0.85f};
    float MaxDistance{6.0f};
    float Thickness{0.2f};
    float SurfaceBias{0.05f};
    std::uint32_t MaxSteps{16u};
    std::uint32_t RayCount{4u};
    float DepthSigma{64.0f};
    float NormalSigma{32.0f};
    float RadianceClamp{2.5f};
    float MaxPixelRadius{96.0f};
    float StepExponent{1.25f};
    float TemporalBlendFactor{0.08f};
    float DisocclusionThreshold{0.02f};
    float ClampStrength{0.10f};
    float VelocityWeight{12.0f};
    float LowLumaBoost{0.08f};
    std::uint32_t TemporalDebugMode{0u};
};

struct RendererSsrFeatureSettings
{
    float MaxDistance{0.25f};
    float Thickness{0.015f};
    float MaxRoughness{0.8f};
    float RoughnessThreshold{0.2f};
    std::uint32_t MaxSteps{32u};
    std::uint32_t MaxBinarySteps{8u};
    float ScreenEdgeFade{0.1f};
    float ReflectionFade{0.8f};
    float TemporalBlendFactor{0.10f};
    float ClampStrength{0.10f};
    float MotionHistoryReset{0.25f};
    std::uint32_t TemporalDebugMode{0u};
};

struct RendererTaaFeatureSettings
{
    float BlendFactor{0.06f};
    float MotionBlendFactor{0.18f};
    float ClampStrength{0.10f};
    float Sharpen{0.0f};
    float JitterScale{1.0f};
};

struct RendererBloomFeatureSettings
{
    float Threshold{1.1f};
    float Knee{0.5f};
    float Intensity{0.8f};
    float Scatter{0.6f};
    float Clamp{10.0f};
    std::uint32_t MipCount{5u};
};

struct RendererAtmosphereFeatureSettings
{
    bool WorldMode{false};
    std::array<float, 3> SunDirection{0.70710677f, 0.70710677f, 0.0f};
    std::array<float, 3> SunColor{1.0f, 1.0f, 1.0f};
    float Exposure{8.0f};
    float SunIntensity{1.0f};
    std::array<float, 3> RayleighScattering{5.8e-6f, 13.5e-6f, 33.1e-6f};
    float RayleighScaleHeight{8000.0f};
    std::array<float, 3> MieScattering{21.0e-6f, 21.0e-6f, 21.0e-6f};
    float MieScaleHeight{1200.0f};
    std::array<float, 3> MieAbsorption{0.0f, 0.0f, 0.0f};
    float MieAnisotropyG{0.76f};
    float PlanetRadiusMeters{6360.0e3f};
    float AtmosphereRadiusMeters{6420.0e3f};
    float CameraGroundOffsetMeters{100.0f};
    float MaxSunDistanceMeters{120.0e3f};
    std::uint32_t ViewSampleCount{4u};
    std::uint32_t SunSampleCount{4u};
    float MultiScatterStrength{2.0e-6f};
};

struct RendererAtmosphereCompositeFeatureSettings
{
    float DepthThreshold{0.0f};
    float BlendWhenGeometry{0.0f};
    float BlendWhenSky{1.0f};
};

struct RendererHeightFogFeatureSettings
{
    float Density{0.004f};
    float HeightFalloff{0.008f};
    bool UseAbsoluteHeight{true};
    double HeightOffsetAbsoluteY{0.0};
    bool UseActiveCameraYAsRebaseOrigin{true};
    double RebaseOriginAbsoluteY{0.0};
    float HeightOffsetRebased{0.0f};
    float StartDistance{10.0f};
    std::array<float, 3> FogColor{0.64f, 0.70f, 0.76f};
    std::array<float, 3> HorizonColor{0.69f, 0.72f, 0.75f};
    std::array<float, 3> ZenithColor{0.47f, 0.57f, 0.69f};
    float SkyBlendStartDistance{100.0f};
    float SkyBlendEndDistance{1200.0f};
    float SkyBlendStrength{1.0f};
    float TauDitherAmplitude{0.005f};
    std::array<float, 3> SunDirection{0.0f, -1.0f, 0.0f};
    float SunAnisotropyG{0.7f};
    std::array<float, 3> SunColor{1.0f, 0.95f, 0.85f};
    float SunInscatterIntensity{0.05f};
};

struct RendererToneMapFeatureSettings
{
    float Exposure{1.0f};
    float Gamma{2.2f};
    float DitherStrength{1.0f};
    float AgXExposureBiasStops{-0.5f};
    float AgXSaturation{1.05f};
    float AgXContrast{1.03f};
    float AgXPivot{0.5f};
    float AgXGamutThreshold{0.9f};
    float AgXGamutKnee{0.5f};
    float AcesSaturation{1.05f};
    float AcesWhitePoint{11.2f};
    bool EnableACES{true};
    bool EnableAgX{false};
    bool EnableCompare{false};
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief World-owned rendering facade for windows, scene outputs, UI, and renderer resources.
 *
 * `RendererSystem` is the GameFramework-owned facade for rendering. It exposes renderer
 * lifecycle, world window ownership, virtual render viewports, scene-object registration,
 * UI packet translation, default materials/fonts, and light-manager
 * access behind one subsystem that `World` can own and tick.
 *
 * Why this abstraction exists:
 * - to align renderer bootstrap/shutdown with world/runtime lifetime
 * - to hide backend/runtime details behind a world-oriented contract
 * - to centralize the glue between scene components, UI, and renderer output management
 *
 * Core semantics:
 * - `Initialize(...)` copies the bootstrap settings and may perform a second attempt with reduced settings after out-of-memory failure
 * - virtual render viewports, renderer scene records, UI textures, and queued text are all owned/tracked here
 * - `EndFrame()` is the point where queued UI/text work is flushed and frame submission occurs
 * - `RenderViewportFeatureRevision()` increments when viewport feature-profile topology changes so components can reapply feature routing safely
 *
 * Ownership and lifetime:
 * - Owned by `World`.
 * - Owns the subsystem-created window, cached UI resources, and tracked viewport metadata.
 * - Native GameFramework render records become invalid after `Shutdown()`.
 *
 * Threading model:
 * - Main-thread oriented.
 * - Cross-thread work should be marshaled through `EnqueueTask(...)`.
 *
 * @warning
 * Although the API is presented as a world subsystem, the renderer backend itself is not
 * world-isolated. Multiple simultaneously initialized worlds should not assume independent
 * graphics backends or isolated global renderer state.
 *
 * @see World
 * @see RendererBootstrapSettings
 * @see EGameRenderFeatureProfile
 */
SnType()
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
     * @brief Initialize the renderer using default bootstrap settings.
     * @return `true` when bootstrap completed successfully.
     * @remarks
     * A successful return means the bootstrap sequence completed. `IsInitialized()` becomes
     * `true` only when a graphics backend was actually created; if `CreateRendererRuntime` is
     * disabled in the effective settings, initialization may still return success while the
     * subsystem remains intentionally non-ready for rendering work.
     */
    bool Initialize();

    /**
     * @brief Initialize the renderer with explicit bootstrap settings.
     * @param Settings Renderer bootstrap settings copied into the subsystem.
     * @return `true` when bootstrap completed successfully.
     * @warning Replaces any previously owned renderer-side state for this subsystem.
     */
    bool Initialize(const RendererBootstrapSettings& Settings);

    /**
     * @brief Shutdown renderer resources owned through this subsystem.
     * @remarks Invalidates all borrowed pointers previously returned by this subsystem.
     */
    void Shutdown();

    /**
     * @brief Check whether a live graphics backend is available.
     * @return `true` when the subsystem currently has a usable graphics backend pointer.
     */
    SnFunction(SnKey("IsInitialized"))
    bool IsInitialized() const;

    /**
     * @brief Access the active renderer bootstrap settings snapshot.
     * @return Borrowed reference to the subsystem-owned settings copy.
     */
    SnFunction(SnKey("Settings"))
    const RendererBootstrapSettings& Settings() const
    {
        return m_settings;
    }


    /**
     * @brief Snapshot the primary Renderer.New window state.
     */
    [[nodiscard]] GameRenderWindow MainWindow() const;

    /**
     * @brief Check whether a renderer window exists and is currently open.
     */
    SnFunction(SnKey("HasOpenWindow"))
    bool HasOpenWindow() const;

    bool SetActiveCamera(const std::shared_ptr<GameRenderCamera>& Camera);
    bool SetActiveCamera(GameRenderCamera* Camera);
    [[nodiscard]] GameRenderCamera* ActiveCamera() const;
    [[nodiscard]] std::shared_ptr<GameRenderCamera> ActiveCameraShared() const;

    /**
     * @brief Configure project shader search root for runtime Slang compilation.
     * @param AssetRoot Project asset root directory (for example `<Project>/Assets`).
     * @return True when shader search paths were updated.
     * @remarks
     * This sets/refreshes a custom shader search path at `<AssetRoot>/Shaders` with recursive lookup.
     */
    bool SetProjectShaderSearchRoot(const std::filesystem::path& AssetRoot);


    /**
     * @brief Enable or disable renderer default viewport runtime (ID = `DefaultRenderViewportID()`).
     * @param Enabled True to enable/create default viewport; false to disable/remove it.
     * @return True when renderer is initialized and state was applied.
     */
    SnFunction(SnKey("UseDefaultRenderViewport"))
    bool UseDefaultRenderViewport(bool Enabled = true);

    /**
     * @brief Query whether the renderer default viewport runtime is currently active.
     * @return True when default viewport runtime exists and is enabled for use.
     */
    SnFunction(SnKey("IsUsingDefaultRenderViewport"))
    [[nodiscard]] bool IsUsingDefaultRenderViewport() const;


    /**
     * @brief Create a new virtual render viewport.
     * @param Name Debug/display name for the viewport runtime. Empty names fall back to `"Viewport"`.
     * @param X Output rect X in window-space units.
     * @param Y Output rect Y in window-space units.
     * @param Width Output rect width in window-space units.
     * @param Height Output rect height in window-space units.
     * @param RenderWidth Internal render extent width in pixels. `0` falls back to rounded output width.
     * @param RenderHeight Internal render extent height in pixels. `0` falls back to rounded output height.
     * @param Camera Optional borrowed viewport camera override. `nullptr` means "use the renderer active camera".
     * @param Enabled Whether the viewport should be enabled immediately.
     * @param OutViewportID Receives the created viewport id on success and `0` on failure.
     * @return `true` when the viewport was created.
     * @remarks Output rect size and render extent are clamped to at least one pixel/unit internally.
     */
    bool CreateRenderViewport(std::string Name,
                              float X,
                              float Y,
                              float Width,
                              float Height,
                              std::uint32_t RenderWidth,
                              std::uint32_t RenderHeight,
                              const std::shared_ptr<GameRenderCamera>& Camera,
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
                              const std::shared_ptr<GameRenderCamera>& Camera,
                              bool Enabled);

    /**
     * @brief Destroy a virtual render viewport.
     * @param ViewportID Viewport identifier.
     * @return `true` when the viewport was destroyed.
     * @warning The renderer default viewport cannot be destroyed through this API.
     */
    SnFunction(SnKey("DestroyRenderViewport"))
    bool DestroyRenderViewport(std::uint64_t ViewportID);

    /**
     * @brief Check whether a render viewport currently exists.
     * @param ViewportID Target viewport identifier.
     * @return True when viewport exists.
     */
    SnFunction(SnKey("HasRenderViewport"))
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
     * @brief Apply a built-in feature profile preset for a viewport.
     * @param ViewportID Target viewport identifier.
     * @param Preset Feature-profile preset.
     * @return `true` when registration succeeded or the same preset had already been applied.
     * @remarks
     * A viewport can only have one tracked preset assignment. Re-registering the same preset is
     * idempotent; attempting to replace an existing different preset is rejected. Successful new
     * registrations increment `RenderViewportFeatureRevision()`.
     */
    SnFunction(SnKey("ApplyRenderViewportFeatureProfile"))
    bool ApplyRenderViewportFeatureProfile(std::uint64_t ViewportID, EGameRenderFeatureProfile Preset);


    /**
     * @brief Register one static mesh resource with Renderer.New from compiled GameFramework mesh data.
     * @param MeshData Cooked mesh payload to upload.
     * @param OutMesh Destination GameFramework mesh record.
     * @param DebugName Optional diagnostic name.
     * @return True when the mesh and backing Renderer.New buffers were created.
     */
    [[nodiscard]] bool CreateStaticRenderMesh(
        const RuntimeMeshData& MeshData,
        GameRenderMesh& OutMesh,
        std::string_view DebugName = {});

    /**
     * @brief Register one static mesh resource with Renderer.New from Renderer.New primitive mesh data.
     * @param MeshData Primitive mesh payload to upload.
     * @param OutMesh Destination GameFramework mesh record.
     * @param DebugName Optional diagnostic name.
     * @return True when the mesh and backing Renderer.New buffers were created.
     */
    [[nodiscard]] bool CreateStaticRenderMesh(
        SnAPI::Renderer::PrimitiveMeshData MeshData,
        GameRenderMesh& OutMesh,
        std::string_view DebugName = {});

    /**
     * @brief Destroy one Renderer.New mesh resource owned through a GameFramework mesh record.
     * @param Mesh Mesh record to destroy and reset.
     * @return True when a live resource was destroyed.
     */
    bool DestroyRenderMesh(GameRenderMesh& Mesh);

    /**
     * @brief Create one retained Renderer.New scene object from a registered mesh.
     * @param Mesh Registered mesh resource.
     * @param OutObject Destination object record.
     * @param WorldFromLocal Initial object transform.
     * @param CastShadows Whether the object should participate in shadow-capable feature profiles.
     * @param DebugName Optional diagnostic name.
     * @return True when the retained object was created.
     */
    [[nodiscard]] bool CreateStaticRenderObject(
        const GameRenderMesh& Mesh,
        GameRenderObject& OutObject,
        const SnAPI::Math::Matrix4& WorldFromLocal,
        bool CastShadows = true,
        std::string_view DebugName = {});

    /**
     * @brief Create one retained Renderer.New scene object with explicit feature-channel routing.
     * @param Mesh Registered mesh resource.
     * @param OutObject Destination object record.
     * @param WorldFromLocal Initial object transform.
     * @param FeatureChannels Renderer.New feature channels used by built-in profiles.
     * @param CastShadows Whether the GameFramework object record should report shadow participation.
     * @param DebugName Optional diagnostic name.
     * @return True when the retained object was created.
     */
    [[nodiscard]] bool CreateStaticRenderObject(
        const GameRenderMesh& Mesh,
        GameRenderObject& OutObject,
        const SnAPI::Math::Matrix4& WorldFromLocal,
        SnAPI::Renderer::RenderFeatureChannelMask FeatureChannels,
        bool CastShadows,
        std::string_view DebugName = {});

    /**
     * @brief Destroy one retained Renderer.New scene object.
     * @param Object Object record to destroy and reset.
     * @return True when a live object was destroyed.
     */
    bool DestroyRenderObject(GameRenderObject& Object);

    /**
     * @brief Update the transform for one retained Renderer.New scene object.
     * @param Object Object record to update.
     * @param WorldFromLocal New world-from-local transform.
     * @return True when the renderer accepted the transform update.
     */
    bool SetRenderObjectTransform(GameRenderObject& Object, const SnAPI::Math::Matrix4& WorldFromLocal);

    /**
     * @brief Queue one world-space debug line for the next Renderer.New frame.
     * @param Line Debug line in world coordinates.
     * @return True when the line was accepted for the next frame.
     */
    [[nodiscard]] bool QueueDebugLine(const GameRenderDebugLine& Line);

    [[nodiscard]] bool CreateDirectionalRenderLight(
        const SnAPI::Renderer::DirectionalLightDesc& Desc,
        GameRenderLight& OutLight,
        std::string_view DebugName = {});
    bool SetDirectionalRenderLight(GameRenderLight& Light, const SnAPI::Renderer::DirectionalLightDesc& Desc);
    bool DestroyRenderLight(GameRenderLight& Light);


    /**
     * @brief Monotonic revision for render-viewport feature profile topology changes.
     * @return Current feature-profile revision value.
     * @remarks
     * Components can cache this value to know when viewport feature profiles were added and
     * feature participation should be re-applied to existing render objects.
     */
    SnFunction(SnKey("RenderViewportFeatureRevision"))
    std::uint64_t RenderViewportFeatureRevision() const;

    [[nodiscard]] std::uint64_t RendererFeatureSettingsRevision() const;
    bool ApplyDeferredShadingFeatureSettings(std::int64_t ViewportID, const RendererDeferredShadingFeatureSettings& Settings);
    bool ApplySsaoFeatureSettings(std::int64_t ViewportID, const RendererSsaoFeatureSettings& Settings);
    bool ApplySsgiFeatureSettings(std::int64_t ViewportID, const RendererSsgiFeatureSettings& Settings);
    bool ApplySsrFeatureSettings(std::int64_t ViewportID, const RendererSsrFeatureSettings& Settings);
    bool ApplyTaaFeatureSettings(std::int64_t ViewportID, const RendererTaaFeatureSettings& Settings);
    bool ApplyBloomFeatureSettings(std::int64_t ViewportID, const RendererBloomFeatureSettings& Settings);
    bool ApplyAtmosphereFeatureSettings(std::int64_t ViewportID, const RendererAtmosphereFeatureSettings& Settings);
    bool ApplyAtmosphereCompositeFeatureSettings(std::int64_t ViewportID, const RendererAtmosphereCompositeFeatureSettings& Settings);
    bool ApplyHeightFogFeatureSettings(std::int64_t ViewportID, const RendererHeightFogFeatureSettings& Settings);
    bool ApplyToneMapFeatureSettings(std::int64_t ViewportID, const RendererToneMapFeatureSettings& Settings);

    SnFunction(SnKey("SetDefaultTaaJitterScale"))
    void SetDefaultTaaJitterScale(float Value);
    SnFunction(SnKey("SetViewportTaaJitterScale"))
    void SetViewportTaaJitterScale(std::uint64_t ViewportID, float Value);


    /**
     * @brief Load and set the default font used by `QueueText`.
     * @param FontPath Font file path.
     * @param FontSize Font pixel size.
     * @return `true` if a renderable font was resolved and stored as the default font.
     */
    SnFunction(SnKey("LoadDefaultFont"))
    bool LoadDefaultFont(const std::string& FontPath, std::uint32_t FontSize = 24);

    /**
     * @brief Queue screen-space text for rendering during the next `EndFrame()` submit.
     * @param Text UTF-8 text to draw. Empty text is ignored.
     * @param X Screen-space X position.
     * @param Y Screen-space Y position.
     * @return `true` if the text request was queued.
     * @remarks Uses the default font configured through settings or `LoadDefaultFont(...)`.
     */
    SnFunction(SnKey("QueueText"))
    bool QueueText(std::string Text, float X = 0.0f, float Y = 0.0f);

    /**
     * @brief Check whether a default font is currently available.
     */
    SnFunction(SnKey("HasDefaultFont"))
    bool HasDefaultFont() const;


#if defined(SNAPI_GF_ENABLE_UI)
    SnAPI::UI::IFontMetrics* EnsureDefaultUiFontMetrics();

    /**
     * @brief Queue one frame of UI render packets for renderer submission.
     * @param ViewportID Target render viewport id that should consume these packets.
     * @param Context UI context used to resolve packet texture ids into image payloads.
     * @param Packets Frame packet list generated by `UISystem::BuildRenderPackets`.
     * @return `true` when packets were accepted for the next renderer `EndFrame()` submit.
     * @remarks
     * This performs CPU-side translation from SnAPI.UI packet formats into renderer
     * instanced-rectangle draw records and caches texture upload payloads for
     * deferred GPU creation during `EndFrame()` after `BeginFrame()` is active.
     * Packet coordinates are interpreted relative to the supplied context screen rect.
     */
    bool QueueUiRenderPackets(std::uint64_t ViewportID, SnAPI::UI::UIContext& Context, const SnAPI::UI::RenderPacketList& Packets);

    /**
     * @brief Queue one frame of UI render packets for renderer default viewport submission.
     * @param Context UI context used to resolve packet texture ids into image payloads.
     * @param Packets Frame packet list generated by `UISystem::BuildRenderPackets`.
     * @return `true` when packets were accepted.
     * @remarks Uses the renderer default viewport when enabled, otherwise the first currently known viewport.
     */
    bool QueueUiRenderPackets(SnAPI::UI::UIContext& Context, const SnAPI::UI::RenderPacketList& Packets);

    /**
     * @brief Register an external viewport-backed UI texture binding for one context-local texture id.
     * @param Context UI context that owns the texture id.
     * @param TextureId Context-local texture id.
     * @param SourceViewportID Render viewport id that provides the sampled image.
     * @param HasTransparency True when sampled output should be alpha blended.
     * @return `true` when the binding was accepted.
     * @remarks Replaces any existing external-image binding for the same `(Context, TextureId)` pair.
     */
    bool RegisterExternalViewportUiTexture(const SnAPI::UI::UIContext& Context,
                                           std::uint32_t TextureId,
                                           std::uint64_t SourceViewportID,
                                           bool HasTransparency);


    /**
     * @brief Remove one external viewport-backed UI texture binding.
     * @param Context UI context that owns the texture id.
     * @param TextureId Context-local texture id.
     * @return True when a binding existed and was removed.
     */
    bool UnregisterExternalViewportUiTexture(const SnAPI::UI::UIContext& Context, std::uint32_t TextureId);

#endif

    /**
     * @brief Run end-of-frame renderer maintenance and frame submission.
     * @remarks
     * Executes queued subsystem tasks, optionally coalesces surface resize handling,
     * begins/presents a frame when a live window exists, flushes queued UI/text work,
     * clears editor-immediate submissions, and saves previous-frame camera/render-object state.
     */
    void EndFrame();


private:
#if defined(SNAPI_GF_ENABLE_UI)
    struct QueuedUiRect;
#endif
    struct RendererNewRuntimeState;
    struct RendererNewRuntimeStateDeleter
    {
        void operator()(RendererNewRuntimeState* State) const;
    };

    bool InitializeUnlocked();
    void ApplyOutOfMemoryFallbackSettings();

    void ShutdownUnlocked();
    bool EnsureDefaultLighting();
    bool EnsureDefaultEnvironmentProbe();
    bool EnsureDefaultFont();
    bool HandleWindowResizeIfNeeded();
    void FlushQueuedText();
#if defined(SNAPI_GF_ENABLE_UI)
    void FlushQueuedUiPackets();
#endif
    bool CreateWindowResources();
    bool EnsureRendererNewViewportTarget(std::uint64_t ViewportID, std::uint32_t RenderWidth, std::uint32_t RenderHeight);
    void DestroyRendererNewViewportTarget(std::uint64_t ViewportID);
    bool EnsureRendererNewDefaultTextFontUnlocked();
    void WarmRendererNewQueuedOverlayTextUnlocked();
    void ClearRendererNewQueuedOverlaysUnlocked();
    void FlushRendererNewDebugLinesUnlocked();
    void ApplyRendererNewFeatureSettingsUnlocked(std::uint64_t ViewportID);
    bool ApplyDefaultFeatureProfile();
    bool ApplyRenderViewportFeatureProfileUnlocked(std::uint64_t ViewportID, EGameRenderFeatureProfile Preset, bool TrackDefaultPassPointers);
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

    struct UiExternalTextureBinding
    {
        std::uint64_t SourceViewportID = 0;
        bool HasTransparency = true;
    };

#endif

    mutable GameMutex m_mutex{}; /**< @brief Renderer-system thread affinity guard. */
    TSystemTaskQueue<RendererSystem> m_taskQueue{}; /**< @brief Cross-thread task handoff queue (real lock only on enqueue). */
    RendererBootstrapSettings m_settings{}; /**< @brief Active bootstrap settings snapshot. */
    std::unique_ptr<RendererNewRuntimeState, RendererNewRuntimeStateDeleter> m_rendererNew{}; /**< @brief Private renderer runtime state for this integration. */
    std::vector<GameRenderDebugLine> m_rendererNewDebugLines{}; /**< @brief Per-frame world-space debug line submissions flushed into Renderer.New. */
    GameRenderWindow m_mainWindow{};
    std::shared_ptr<GameRenderCamera> m_activeCamera{}; /**< @brief Strongly retained active Renderer.New camera for this world-facing renderer facade. */
    bool m_defaultFontFallbacksConfigured = false; /**< @brief True once fallback face chain is attached to the default font. */
    std::vector<TextRequest> m_textQueue{}; /**< @brief Pending text draw requests flushed in EndFrame. */
#if defined(SNAPI_GF_ENABLE_UI)
    std::unordered_map<UiTextureCacheKey, UiExternalTextureBinding, UiTextureCacheKeyHasher> m_uiExternalTextureBindings{}; /**< @brief External viewport-backed texture bindings keyed by (UIContext, texture-id). */
    std::unordered_map<UiTextureCacheKey, PendingUiTextureUpload, UiTextureCacheKeyHasher> m_uiPendingTextureUploads{}; /**< @brief Deferred CPU-side UI image payloads keyed by (UIContext, texture-id). */
    std::vector<QueuedUiRect> m_uiQueuedRects{}; /**< @brief Per-frame translated UI rectangles awaiting renderer draw submission. */
    bool m_uiPacketsQueuedThisFrame = false; /**< @brief True once at least one UI context queued packets for the current frame. */
#endif
    std::unordered_map<std::uint64_t, EGameRenderFeatureProfile> m_renderViewportFeatureProfiles{}; /**< @brief Tracks feature profile assignment per viewport. */
    std::uint64_t m_renderViewportFeatureRevision = 1; /**< @brief Monotonic revision incremented when viewport feature-profile topology changes. */
    float m_defaultTaaJitterScale = 1.0f; /**< @brief Default projection jitter amplitude scale applied to TAA-enabled world viewports. */
    std::unordered_map<std::uint64_t, float> m_viewportTaaJitterScales{}; /**< @brief Optional per-viewport TAA jitter-scale overrides keyed by viewport id. */
    std::uint64_t m_taaFrameIndex = 0; /**< @brief Monotonic TAA jitter sample index advanced on rendered frames. */
    bool m_initialized = false; /**< @brief True when backend lifecycle is active through this subsystem. */
};

SNAPI_DEFINE_TYPE_NAME(RendererSystem, "SnAPI::GameFramework::RendererSystem")
SNAPI_DEFINE_TYPE_NAME(RendererBootstrapSettings, "SnAPI::GameFramework::RendererBootstrapSettings")

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
