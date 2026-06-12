#include "Rendering/GameRenderWindow.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <algorithm>

namespace SnAPI::GameFramework
{

bool GameRenderWindow::Valid() const noexcept
{
    return m_NativeHandle != 0u;
}

bool GameRenderWindow::Open() const noexcept
{
    return m_Open;
}

std::uint32_t GameRenderWindow::Width() const noexcept
{
    return m_Width;
}

std::uint32_t GameRenderWindow::Height() const noexcept
{
    return m_Height;
}

SnAPI::Renderer::PlatformWindowHandle GameRenderWindow::NativeHandle() const noexcept
{
    return m_NativeHandle;
}

const SnAPI::Renderer::RenderNativeSurfaceInfo& GameRenderWindow::NativeSurface() const noexcept
{
    return m_NativeSurface;
}

void GameRenderWindow::Configure(
    const std::uint32_t width,
    const std::uint32_t height,
    const SnAPI::Renderer::PlatformWindowHandle nativeHandle,
    SnAPI::Renderer::RenderNativeSurfaceInfo nativeSurface,
    const bool open) noexcept
{
    m_NativeHandle = nativeHandle;
    m_NativeSurface = nativeSurface;
    Resize(width, height);
    m_Open = open;
}

void GameRenderWindow::Resize(const std::uint32_t width, const std::uint32_t height) noexcept
{
    m_Width = std::max(1u, width);
    m_Height = std::max(1u, height);
}

void GameRenderWindow::Open(const bool open) noexcept
{
    m_Open = open;
}

void GameRenderWindow::Reset() noexcept
{
    m_Width = 0u;
    m_Height = 0u;
    m_NativeHandle = 0u;
    m_NativeSurface = {};
    m_Open = false;
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
