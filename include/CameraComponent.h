#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>

#include "IComponent.h"

namespace SnAPI::Graphics
{
class CameraBase;
} // namespace SnAPI::Graphics

namespace SnAPI::GameFramework
{

class RendererSystem;

/**
 * @brief Component that owns and drives a renderer camera.
 * @remarks
 * Uses owning node `TransformComponent` as pose source when enabled and can
 * become the world's active renderer camera.
 */
class CameraComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::CameraComponent";

    /**
     * @brief Configurable camera parameters.
     */
    struct Settings
    {
        static constexpr const char* kTypeName = "SnAPI::GameFramework::CameraComponent::Settings";

        float NearClip = 0.01f; /**< @brief Near clipping plane. */
        float FarClip = 1000.0f; /**< @brief Far clipping plane (reserved by some pipelines). */
        float FovDegrees = 60.0f; /**< @brief Vertical field of view in degrees. */
        float Aspect = 16.0f / 9.0f; /**< @brief Camera aspect ratio. */
        bool Active = true; /**< @brief When true this camera is selected as world active camera. */
        bool SyncFromTransform = true; /**< @brief Pull camera pose from owner `TransformComponent`. */
    };

    ~CameraComponent() override;

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

    /** @brief Get renderer camera instance (nullable before creation). */
    SnAPI::Graphics::CameraBase* Camera();
    /** @brief Get renderer camera instance (nullable before creation). */
    const SnAPI::Graphics::CameraBase* Camera() const;

    /** @brief Runtime active state helper. */
    bool IsActive() const
    {
        return m_settings.Active;
    }

    /** @brief Enable/disable this camera as world active camera. */
    void SetActive(bool Active);

    void OnCreate() override;
    void OnDestroy() override;
    void Tick(float DeltaSeconds) override;
    void LateTick(float DeltaSeconds) override;

private:
    struct CameraDeleter
    {
        void operator()(SnAPI::Graphics::CameraBase* Camera) const;
    };

    RendererSystem* ResolveRendererSystem() const;
    void EnsureCamera();
    void ApplyCameraSettings();
    void SyncFromTransform();

    Settings m_settings{}; /**< @brief Camera configuration. */
    std::unique_ptr<SnAPI::Graphics::CameraBase, CameraDeleter> m_camera{}; /**< @brief Owned renderer camera instance. */
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
