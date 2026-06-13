#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>

#include "Device/Surface.h"

namespace SnAPI::GameFramework
{

class RendererSystem;

class GameRenderWindow final
{
public:
    [[nodiscard]] bool Valid() const noexcept;
    [[nodiscard]] bool Open() const noexcept;
    [[nodiscard]] std::uint32_t Width() const noexcept;
    [[nodiscard]] std::uint32_t Height() const noexcept;
    [[nodiscard]] SnAPI::Renderer::PlatformWindowHandle NativeHandle() const noexcept;
    [[nodiscard]] const SnAPI::Renderer::RenderNativeSurfaceInfo& NativeSurface() const noexcept;

private:
    friend class RendererSystem;

    void Configure(std::uint32_t width,
                   std::uint32_t height,
                   SnAPI::Renderer::PlatformWindowHandle nativeHandle,
                   SnAPI::Renderer::RenderNativeSurfaceInfo nativeSurface,
                   bool open) noexcept;
    void Resize(std::uint32_t width, std::uint32_t height) noexcept;
    void Open(bool open) noexcept;
    void Reset() noexcept;

    std::uint32_t m_Width{0u};
    std::uint32_t m_Height{0u};
    SnAPI::Renderer::PlatformWindowHandle m_NativeHandle{0u};
    SnAPI::Renderer::RenderNativeSurfaceInfo m_NativeSurface{};
    bool m_Open{false};
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
