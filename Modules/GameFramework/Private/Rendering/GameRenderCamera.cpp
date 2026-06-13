#include "Rendering/GameRenderCamera.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "RenderSpaceConventions.h"
#include "Scene/RenderView.h"

#include <SnAPI/Math/Types.h>

#include <algorithm>
#include <cmath>

namespace SnAPI::GameFramework
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;

[[nodiscard]] bool IsFiniteVec3(const Vec3& value)
{
    return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z());
}

[[nodiscard]] bool IsFiniteQuat(const Quat& value)
{
    return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z()) && std::isfinite(value.w());
}

[[nodiscard]] Quat NormalizeOrIdentity(Quat rotation)
{
    if (!IsFiniteQuat(rotation) || rotation.squaredNorm() <= static_cast<Quat::Scalar>(0))
    {
        return Quat::Identity();
    }

    rotation.normalize();
    return rotation;
}

[[nodiscard]] SnAPI::Renderer::UnjitteredClipFromViewTransform BuildClipFromView(
    const float fovDegrees,
    const float aspect,
    const float nearClip)
{
    const float fovRadians = fovDegrees * (kPi / 180.0f);
    return SnAPI::Renderer::RenderSpaceConventions::PerspectiveClipFromViewReverseZInfinite(
        static_cast<SnAPI::Renderer::Scalar>(fovRadians),
        static_cast<SnAPI::Renderer::Scalar>(aspect),
        static_cast<SnAPI::Renderer::Scalar>(nearClip));
}

[[nodiscard]] SnAPI::Renderer::ViewFromWorldTransform BuildViewFromWorld(const Vec3& position, const Quat& rotation)
{
    const auto worldFromView = SnAPI::Math::MakeTransform(position, rotation);
    return SnAPI::Renderer::ViewFromWorldTransform(worldFromView.matrix().inverse());
}
} // namespace

GameRenderCamera::GameRenderCamera() = default;

void GameRenderCamera::Configure(
    const float nearClip,
    const float farClip,
    const float fovDegrees,
    const float aspect,
    const Vec3& position,
    const Quat& rotation)
{
    if (m_Valid)
    {
        m_PreviousNearClip = m_NearClip;
        m_PreviousFovDegrees = m_FovDegrees;
        m_PreviousAspect = m_Aspect;
        m_PreviousPosition = m_Position;
        m_PreviousRotation = m_Rotation;
        m_HasPreviousFrame = true;
    }

    m_NearClip = std::max(0.0001f, std::isfinite(nearClip) ? nearClip : 0.01f);
    m_FarClip = std::max(m_NearClip + 0.001f, std::isfinite(farClip) ? farClip : 1000.0f);
    m_FovDegrees = std::clamp(std::isfinite(fovDegrees) ? fovDegrees : 60.0f, 1.0f, 179.0f);
    m_Aspect = std::max(0.001f, std::isfinite(aspect) ? aspect : (16.0f / 9.0f));
    m_Position = IsFiniteVec3(position) ? position : Vec3::Zero();
    m_Rotation = NormalizeOrIdentity(rotation);
    m_Valid = true;
}

void GameRenderCamera::ApplyToView(SnAPI::Renderer::RenderView& view) const
{
    if (!m_Valid)
    {
        return;
    }

    const auto clipFromView = BuildClipFromView(m_FovDegrees, m_Aspect, m_NearClip);
    const auto viewFromWorld = BuildViewFromWorld(m_Position, m_Rotation);
    const auto clipFromWorld =
        SnAPI::Renderer::RenderSpaceConventions::ComposeClipFromWorld(clipFromView, viewFromWorld);
    const auto previousClipFromView = BuildClipFromView(
        m_HasPreviousFrame ? m_PreviousFovDegrees : m_FovDegrees,
        m_HasPreviousFrame ? m_PreviousAspect : m_Aspect,
        m_HasPreviousFrame ? m_PreviousNearClip : m_NearClip);
    const auto previousViewFromWorld = BuildViewFromWorld(
        m_HasPreviousFrame ? m_PreviousPosition : m_Position,
        m_HasPreviousFrame ? m_PreviousRotation : m_Rotation);
    const auto previousClipFromWorld =
        SnAPI::Renderer::RenderSpaceConventions::ComposeClipFromWorld(previousClipFromView, previousViewFromWorld);
    const auto jitteredClipFromView = SnAPI::Renderer::RenderSpaceConventions::ApplyProjectionJitterPixels(
        clipFromView,
        static_cast<SnAPI::Renderer::Scalar>(view.JitterPixels[0]),
        static_cast<SnAPI::Renderer::Scalar>(view.JitterPixels[1]),
        view.InternalRenderWidth(),
        view.InternalRenderHeight());
    const auto jitteredClipFromWorld =
        SnAPI::Renderer::RenderSpaceConventions::ComposeClipFromWorld(jitteredClipFromView, viewFromWorld);

    view.NearPlane = static_cast<double>(m_NearClip);
    view.CameraWorldPosition = SnAPI::Renderer::Point3{m_Position.x(), m_Position.y(), m_Position.z()};
    view.HasExplicitClipFromViewUnjittered = true;
    view.ClipFromViewUnjittered = clipFromView;
    view.HasExplicitViewFromWorld = true;
    view.ViewFromWorld = viewFromWorld;
    view.ClipFromWorldUnjittered = clipFromWorld;
    view.PreviousClipFromWorldUnjittered = previousClipFromWorld;
    view.ClipFromWorldJittered = jitteredClipFromWorld;
    view.PreviousClipFromWorldJittered =
        SnAPI::Renderer::JitteredClipFromWorldTransform(view.PreviousClipFromWorldUnjittered.Raw());
    view.TemporalCameraStateValid = m_HasPreviousFrame;
}

void GameRenderCamera::Reset()
{
    *this = GameRenderCamera{};
}

bool GameRenderCamera::Valid() const noexcept
{
    return m_Valid;
}

float GameRenderCamera::NearClip() const noexcept
{
    return m_NearClip;
}

float GameRenderCamera::FarClip() const noexcept
{
    return m_FarClip;
}

float GameRenderCamera::FovDegrees() const noexcept
{
    return m_FovDegrees;
}

float GameRenderCamera::Aspect() const noexcept
{
    return m_Aspect;
}

const Vec3& GameRenderCamera::Position() const noexcept
{
    return m_Position;
}

const Quat& GameRenderCamera::Rotation() const noexcept
{
    return m_Rotation;
}

Vec3 GameRenderCamera::Forward() const noexcept
{
    return (m_Rotation * Vec3{0.0, 0.0, -1.0}).normalized();
}

Vec3 GameRenderCamera::Right() const noexcept
{
    return (m_Rotation * Vec3{1.0, 0.0, 0.0}).normalized();
}

Vec3 GameRenderCamera::Up() const noexcept
{
    return (m_Rotation * Vec3{0.0, 1.0, 0.0}).normalized();
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
