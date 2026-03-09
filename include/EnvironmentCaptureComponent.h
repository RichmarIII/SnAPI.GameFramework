#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "BaseComponent.h"
#include "Export.h"
#include <LinearAlgebra.hpp>

namespace SnAPI::Graphics
{
class IEnvironmentProbe;
class VulkanGraphicsAPI;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{
class RendererSystem;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Component that owns one renderer-side environment probe capture instance.
 *
 * `EnvironmentCaptureComponent` is the authoring bridge between a scene node and the renderer-native
 * `IEnvironmentProbe` capture system. It registers exactly one probe with the renderer, keeps the
 * probe position synchronized to the owning node transform, and issues explicit or realtime capture
 * requests against a selected render viewport.
 *
 * Core semantics:
 * - the component owns probe lifetime logically, while the renderer owns the registered probe object
 * - `Bake()` queues an explicit capture request using the current settings
 * - `Realtime=true` requests a new capture once the previous capture has completed
 * - `ViewportID=-1` means "use the renderer's current default/primary world viewport"
 *
 * @warning
 * The current renderer lighting path still consumes one global active environment probe. New probes are
 * registered at the front of the renderer probe list so the most recently authored probe takes priority.
 *
 * @see EnvironmentProbeNode
 * @see SnAPI::Graphics::IEnvironmentProbe
 */
class SNAPI_GAMEFRAMEWORK_API EnvironmentCaptureComponent final : public BaseComponent, public ComponentCRTP<EnvironmentCaptureComponent>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::EnvironmentCaptureComponent";

    /**
     * @brief Runtime/editor settings for one environment-probe capture component.
     *
     * Semantics:
     * - `ViewportID=-1` resolves to the renderer's current primary/default viewport
     * - `FaceSize` is applied symmetrically to all cube faces
     * - `FacesPerFrame=6` captures the full cube in one frame; lower values amortize runtime cost
     * - `Realtime` only re-requests capture once the current job has finished
     * - `CaptureResourceNameOverride` optionally redirects capture away from the viewport's default final resource
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::EnvironmentCaptureComponent::Settings";

        std::int64_t ViewportID = -1; /**< @brief Source viewport id, or `-1` to use the renderer's current primary/default viewport. */
        unsigned int FaceSize = 256u; /**< @brief Cube-face width/height in pixels. */
        unsigned int FacesPerFrame = 6u; /**< @brief Number of faces captured per frame while a capture job is active. */
        bool Realtime = false; /**< @brief Requeue captures continuously once each job completes. */
        std::string CaptureResourceNameOverride{}; /**< @brief Optional graph resource name to capture instead of the viewport's configured final color resource. */
        SnAPI::Vector3D ProjectionExtents{SnAPI::Vector3D(5.0, 5.0, 5.0)}; /**< @brief Half-extents used for parallax-correct local specular projection. */
        SnAPI::Vector3D InfluenceExtents{SnAPI::Vector3D(7.5, 7.5, 7.5)}; /**< @brief Half-extents used for deferred local-probe blending weights. */
        float Priority = 1.0f; /**< @brief Relative probe priority when more than four probes overlap one pixel. */
    };

    [[nodiscard]] Settings& EditSettings();
    [[nodiscard]] const Settings& GetSettings() const;

    [[nodiscard]] std::string GetCaptureStateText() const;
    [[nodiscard]] unsigned int GetCapturedFaces() const;

    void Bake();
    void CancelCapture();

    void OnCreate();
    void OnDestroy();
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    void EditorTick(float DeltaSeconds);
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    [[nodiscard]] RendererSystem* ResolveRendererSystem() const;
    [[nodiscard]] SnAPI::Graphics::VulkanGraphicsAPI* ResolveGraphics() const;
    [[nodiscard]] bool EnsureProbeRegistered();
    void UnregisterProbe();
    void SyncProbePosition();
    void SyncProbeSettings();
    [[nodiscard]] bool RequestCapture(bool Force);
    [[nodiscard]] std::optional<std::uint64_t> ResolveSourceViewportID(
        const SnAPI::Graphics::VulkanGraphicsAPI& Graphics) const;
    [[nodiscard]] bool ProbeRegisteredWithGraphics(const SnAPI::Graphics::VulkanGraphicsAPI& Graphics) const;

    Settings m_settings{};
    SnAPI::Graphics::IEnvironmentProbe* m_probe = nullptr;
    bool m_bBakeRequested = false;
};
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
