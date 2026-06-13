#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "Math.h"

namespace SnAPI::Renderer
{
struct RenderView;
} // namespace SnAPI::Renderer

namespace SnAPI::GameFramework
{
class GameRenderCamera final
{
public:
    GameRenderCamera();

    void Configure(float nearClip, float farClip, float fovDegrees, float aspect, const Vec3& position, const Quat& rotation);
    void ApplyToView(SnAPI::Renderer::RenderView& view) const;
    void Reset();

    [[nodiscard]] bool Valid() const noexcept;
    [[nodiscard]] float NearClip() const noexcept;
    [[nodiscard]] float FarClip() const noexcept;
    [[nodiscard]] float FovDegrees() const noexcept;
    [[nodiscard]] float Aspect() const noexcept;
    [[nodiscard]] const Vec3& Position() const noexcept;
    [[nodiscard]] const Quat& Rotation() const noexcept;
    [[nodiscard]] Vec3 Forward() const noexcept;
    [[nodiscard]] Vec3 Right() const noexcept;
    [[nodiscard]] Vec3 Up() const noexcept;

private:
    float m_NearClip{0.01f};
    float m_FarClip{1000.0f};
    float m_FovDegrees{60.0f};
    float m_Aspect{16.0f / 9.0f};
    Vec3 m_Position{Vec3::Zero()};
    Quat m_Rotation{Quat::Identity()};
    float m_PreviousNearClip{0.01f};
    float m_PreviousFovDegrees{60.0f};
    float m_PreviousAspect{16.0f / 9.0f};
    Vec3 m_PreviousPosition{Vec3::Zero()};
    Quat m_PreviousRotation{Quat::Identity()};
    bool m_Valid{false};
    bool m_HasPreviousFrame{false};
};
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
