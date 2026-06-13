#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "Resources/ResourceDesc.h"
#include "Scene/RenderOverlayPacket.h"
#include "Types/Handles.h"

namespace SnAPI::GameFramework
{
class GameRenderCamera;

class GameRenderOutput final
{
public:
    GameRenderOutput() = default;

    void ConfigureTarget(
        std::uint64_t id,
        std::string name,
        SnAPI::Renderer::TextureHandle texture,
        SnAPI::Renderer::RenderTargetHandle target,
        SnAPI::Renderer::Extent2D extent,
        bool enabled);

    void ConfigureSurface(
        std::uint64_t id,
        std::string name,
        SnAPI::Renderer::SurfaceHandle surface,
        SnAPI::Renderer::Extent2D extent,
        bool enabled);

    void Resize(SnAPI::Renderer::Extent2D extent) noexcept;
    void Enabled(bool enabled) noexcept;
    void ClearRenderTarget() noexcept;
    void ClearSurface() noexcept;
    void ClearOverlay() noexcept;
    void Camera(std::shared_ptr<GameRenderCamera> camera) noexcept;

    [[nodiscard]] std::uint64_t Id() const noexcept;
    [[nodiscard]] std::string_view Name() const noexcept;
    [[nodiscard]] SnAPI::Renderer::TextureHandle Texture() const noexcept;
    [[nodiscard]] SnAPI::Renderer::RenderTargetHandle Target() const noexcept;
    [[nodiscard]] SnAPI::Renderer::SurfaceHandle Surface() const noexcept;
    [[nodiscard]] SnAPI::Renderer::Extent2D Extent() const noexcept;
    [[nodiscard]] bool Enabled() const noexcept;
    [[nodiscard]] bool HasTarget() const noexcept;
    [[nodiscard]] bool HasSurface() const noexcept;
    [[nodiscard]] bool HasOverlay() const noexcept;
    [[nodiscard]] std::shared_ptr<GameRenderCamera> Camera() const noexcept;
    [[nodiscard]] SnAPI::Renderer::RenderOverlayPacket& Overlay() noexcept;
    [[nodiscard]] const SnAPI::Renderer::RenderOverlayPacket& Overlay() const noexcept;

private:
    std::uint64_t m_Id{0u};
    std::string m_Name{};
    SnAPI::Renderer::TextureHandle m_Texture{};
    SnAPI::Renderer::RenderTargetHandle m_Target{};
    SnAPI::Renderer::SurfaceHandle m_Surface{};
    SnAPI::Renderer::Extent2D m_Extent{.Width = 1u, .Height = 1u};
    SnAPI::Renderer::RenderOverlayPacket m_Overlay{};
    std::shared_ptr<GameRenderCamera> m_Camera{};
    bool m_Enabled{true};
};
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
