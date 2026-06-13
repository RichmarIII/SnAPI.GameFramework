#include "Rendering/GameRenderOutput.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>
#include <utility>

namespace SnAPI::GameFramework
{
void GameRenderOutput::ConfigureTarget(
    const std::uint64_t id,
    std::string name,
    const SnAPI::Renderer::TextureHandle texture,
    const SnAPI::Renderer::RenderTargetHandle target,
    const SnAPI::Renderer::Extent2D extent,
    const bool enabled)
{
    m_Id = id;
    m_Name = std::move(name);
    m_Texture = texture;
    m_Target = target;
    m_Surface = {};
    Resize(extent);
    m_Enabled = enabled;
}

void GameRenderOutput::ConfigureSurface(
    const std::uint64_t id,
    std::string name,
    const SnAPI::Renderer::SurfaceHandle surface,
    const SnAPI::Renderer::Extent2D extent,
    const bool enabled)
{
    m_Id = id;
    m_Name = std::move(name);
    m_Texture = {};
    m_Target = {};
    m_Surface = surface;
    Resize(extent);
    m_Enabled = enabled;
}

void GameRenderOutput::Resize(const SnAPI::Renderer::Extent2D extent) noexcept
{
    m_Extent = SnAPI::Renderer::Extent2D{
        .Width = std::max(1u, extent.Width),
        .Height = std::max(1u, extent.Height)};
}

void GameRenderOutput::Enabled(const bool enabled) noexcept
{
    m_Enabled = enabled;
}

void GameRenderOutput::ClearRenderTarget() noexcept
{
    m_Texture = {};
    m_Target = {};
}

void GameRenderOutput::ClearSurface() noexcept
{
    m_Surface = {};
}

void GameRenderOutput::ClearOverlay() noexcept
{
    m_Overlay = {};
}

void GameRenderOutput::Camera(std::shared_ptr<GameRenderCamera> camera) noexcept
{
    m_Camera = std::move(camera);
}

std::uint64_t GameRenderOutput::Id() const noexcept
{
    return m_Id;
}

std::string_view GameRenderOutput::Name() const noexcept
{
    return m_Name;
}

SnAPI::Renderer::TextureHandle GameRenderOutput::Texture() const noexcept
{
    return m_Texture;
}

SnAPI::Renderer::RenderTargetHandle GameRenderOutput::Target() const noexcept
{
    return m_Target;
}

SnAPI::Renderer::SurfaceHandle GameRenderOutput::Surface() const noexcept
{
    return m_Surface;
}

SnAPI::Renderer::Extent2D GameRenderOutput::Extent() const noexcept
{
    return m_Extent;
}

bool GameRenderOutput::Enabled() const noexcept
{
    return m_Enabled;
}

bool GameRenderOutput::HasTarget() const noexcept
{
    return m_Texture.Valid() && m_Target.Valid();
}

bool GameRenderOutput::HasSurface() const noexcept
{
    return m_Surface.Valid();
}

bool GameRenderOutput::HasOverlay() const noexcept
{
    return !m_Overlay.Empty();
}

std::shared_ptr<GameRenderCamera> GameRenderOutput::Camera() const noexcept
{
    return m_Camera;
}

SnAPI::Renderer::RenderOverlayPacket& GameRenderOutput::Overlay() noexcept
{
    return m_Overlay;
}

const SnAPI::Renderer::RenderOverlayPacket& GameRenderOutput::Overlay() const noexcept
{
    return m_Overlay;
}
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
