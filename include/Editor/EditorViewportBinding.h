#pragma once

#include <cstdint>
#include <string>

#include "Expected.h"

namespace SnAPI::GameFramework
{
class GameRuntime;
} // namespace SnAPI::GameFramework

namespace SnAPI::GameFramework::Editor
{

/**
 * @brief Owns the root editor render viewport and binds it to the root UI context.
 */
class EditorViewportBinding final
{
public:
    Result Initialize(GameRuntime& Runtime, std::string ViewportName);
    void Shutdown(GameRuntime* Runtime);

    [[nodiscard]] bool SyncToWindow(GameRuntime& Runtime);
    [[nodiscard]] bool IsInitialized() const { return m_viewportId != 0 && m_rootContextId != 0; }

    [[nodiscard]] std::uint64_t ViewportId() const { return m_viewportId; }
    [[nodiscard]] std::uint64_t ContextId() const { return m_rootContextId; }

private:
    [[nodiscard]] bool ResolveViewportSize(GameRuntime& Runtime, float& OutWidth, float& OutHeight) const;
    [[nodiscard]] bool EnsureUiBinding(GameRuntime& Runtime) const;

    std::string m_viewportName{"Editor.RootViewport"};
    std::uint64_t m_viewportId = 0;
    std::uint64_t m_rootContextId = 0;
    float m_lastWidth = 0.0f;
    float m_lastHeight = 0.0f;
};

} // namespace SnAPI::GameFramework::Editor
