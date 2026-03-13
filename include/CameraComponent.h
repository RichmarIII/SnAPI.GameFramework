#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>
#include <string_view>

#include "BaseComponent.h"
#include "Math.h"

namespace SnAPI::Graphics
{
class CameraBase;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{

class RendererSystem;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Component that owns and drives a renderer camera.
 *
 * `CameraComponent` is the bridge between a game-world node and a renderer camera
 * instance. It owns one `SnAPI::Graphics::CameraBase`, keeps its projection settings
 * synchronized from `Settings`, and optionally derives camera pose from the owning
 * node's world transform plus configurable local offsets.
 *
 * Core semantics:
 * - exactly one renderer camera instance is owned per component while the component is alive
 * - `OnCreate()` lazily creates the renderer camera and applies current settings immediately
 * - when `Settings::SyncFromTransform` is enabled, pose is pulled from the owning node each update
 * - when `Settings::Active` is enabled, the component attempts to become the world's active renderer camera
 *
 * Ownership and lifetime:
 * - Owned primarily by the component.
 * - The renderer may retain a shared reference while this camera is active so backend-global camera state
 *   cannot dangle across deferred destroy or editor/world rebinding.
 * - `Camera()` returns a non-owning pointer into the shared camera object.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see RendererSystem
 * @see TransformComponent
 */
class CameraComponent : public BaseComponent, public ComponentCRTP<CameraComponent>
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::CameraComponent";

    /**
     * @brief Configurable camera parameters.
     *
     * Units:
     * - `NearClip` and `FarClip` are world units.
     * - `FovDegrees` is expressed in degrees.
     * - `LocalRotationOffsetEuler` is Euler rotation in radians.
     *
     * Semantics:
     * - invalid or degenerate values are clamped to safe renderer defaults before upload
     * - `Active` affects world-level renderer selection, not just this component's local state
     * - `AutoActivateForPlayer` is consumed by gameplay code such as `PawnBase` possession flow rather than by this component alone
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::CameraComponent::Settings";

        float NearClip = 0.01f; /**< @brief Near clipping plane. */
        float FarClip = 1000.0f; /**< @brief Far clipping plane (reserved by some pipelines). */
        float FovDegrees = 60.0f; /**< @brief Vertical field of view in degrees. */
        float Aspect = 16.0f / 9.0f; /**< @brief Camera aspect ratio. */
        bool Active = false; /**< @brief When true this camera is selected as world active camera. */
        bool SyncFromTransform = true; /**< @brief Pull camera pose from owner `TransformComponent`. */
        Vec3 LocalPositionOffset{}; /**< @brief Local translation offset applied after owner world transform. */
        Vec3 LocalRotationOffsetEuler{}; /**< @brief Local rotation offset (XYZ euler radians) applied after owner world rotation. */
        bool AutoActivateForPlayer = false; /**< @brief When true, will activate the camera for the player possessing the owned node */
    };

    ~CameraComponent();
    CameraComponent() = default;
    CameraComponent(const CameraComponent&) = delete;
    CameraComponent& operator=(const CameraComponent&) = delete;
    CameraComponent(CameraComponent&&) noexcept = default;
    CameraComponent& operator=(CameraComponent&&) noexcept = default;

    /** @brief Access settings (const). */
    const Settings& GetSettings() const
    {
        return m_settings;
    }

    /** @brief Access settings for mutation. */
    Settings& EditSettings()
    {
        return m_settings;
    }

    /** @brief Get renderer camera instance. @return Non-owning camera pointer, or `nullptr` before creation/after destruction. */
    SnAPI::Graphics::CameraBase* Camera();
    /** @brief Get renderer camera instance (const). @return Non-owning camera pointer, or `nullptr` before creation/after destruction. */
    const SnAPI::Graphics::CameraBase* Camera() const;
    /** @brief Access the shared renderer camera object retained by this component. */
    std::shared_ptr<SnAPI::Graphics::CameraBase> CameraShared() const;

    /** @brief Runtime active state helper. */
    bool IsActive() const
    {
        return m_settings.Active;
    }

    /**
     * @brief Enable or disable this camera as the world active camera.
     * @param Active New active state.
     * @remarks If disabling the currently active camera, the renderer active camera is cleared.
     */
    void SetActive(bool Active);

    /** @brief Ensure the renderer camera exists, apply current settings, and optionally activate it. */
    void OnCreate();
    /** @brief Release the owned renderer camera and clear renderer active-camera binding if needed. */
    void OnDestroy();
    /** @brief Variable-step camera update. @param DeltaSeconds Frame delta in seconds. */
    void Tick(float DeltaSeconds);
    /** @brief Late variable-step camera update. @param DeltaSeconds Frame delta in seconds. @remarks Uses the same update path as `Tick()`. */
    void LateTick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    /** @brief Editor-only update hook. @param DeltaSeconds Frame delta in seconds. */
    void EditorTick(float DeltaSeconds);
    /** @brief Editor-only property change hook. @param Name Changed reflected field name. */
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    RendererSystem* ResolveRendererSystem() const;
    void EnsureCamera();
    void ApplyCameraSettings() const;
    void SyncFromTransform() const;
    void UpdateCamera(float DeltaSeconds);

    Settings m_settings{}; /**< @brief Camera configuration. */
    std::shared_ptr<SnAPI::Graphics::CameraBase> m_camera{}; /**< @brief Shared renderer camera instance retained by the renderer while active. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
