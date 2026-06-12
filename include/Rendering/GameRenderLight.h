#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <memory>
#include <string>

#include "Types/Handles.h"

namespace SnAPI::Renderer
{
struct DirectionalLightDesc;
} // namespace SnAPI::Renderer

namespace SnAPI::GameFramework
{

class RendererSystem;

class GameRenderLight final
{
public:
    [[nodiscard]] bool Valid() const noexcept;
    void Reset() noexcept;

    [[nodiscard]] SnAPI::Renderer::LightHandle Light() const noexcept;
    [[nodiscard]] const SnAPI::Renderer::DirectionalLightDesc* DirectionalDesc() const noexcept;
    [[nodiscard]] const std::string& DebugName() const noexcept;

private:
    friend class RendererSystem;

    SnAPI::Renderer::LightHandle m_Light{};
    std::shared_ptr<SnAPI::Renderer::DirectionalLightDesc> m_DirectionalDesc{};
    std::string m_DebugName{};
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
