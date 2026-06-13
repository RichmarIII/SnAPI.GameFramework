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
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Keeps the root editor UI context bound to the renderer surface viewport.
 *
 * `EditorViewportBinding` is the small stateful adapter that keeps the editor shell's root
 * UI context synchronized with the runtime window and bound to the renderer's default
 * surface viewport.
 *
 * Core semantics:
 * - Initialization enables the default renderer viewport (`1`) and binds it to the UI root context.
 * - Embedded game/editor viewports remain explicit offscreen render viewports owned by `UIRenderViewport`.
 * - `SyncToWindow()` recreates the root binding when needed and keeps the logical UI size
 *   synchronized with the current renderer window size.
 *
 * Ownership and lifetime:
 * - The class stores only ids and cached UI size state.
 * - The default renderer viewport and UI binding are owned by the runtime subsystems.
 * - Cached ids become invalid after `Shutdown()` or runtime teardown.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see GameRuntime
 * @see RendererSystem
 * @see UISystem
 */
class EditorViewportBinding final
{
public:
    /**
     * @brief Bind the root editor UI context to the renderer surface viewport.
     * @param Runtime Initialized runtime that owns renderer and UI systems.
     * @param ViewportName Reserved diagnostic name for callers that still pass one.
     * @return Success or an error.
     * @pre Renderer and UI subsystems must both be initialized.
     * @post On success, `ViewportId()` is the renderer default viewport id and `ContextId()` references the root UI context.
     */
    Result Initialize(GameRuntime& Runtime, std::string ViewportName);
    /**
     * @brief Clear cached binding state.
     * @param Runtime Reserved runtime pointer for service lifecycle symmetry.
     * @remarks Safe to call repeatedly.
     */
    void Shutdown(GameRuntime* Runtime);

    /**
     * @brief Synchronize the bound viewport with the current window and UI state.
     * @param Runtime Initialized runtime.
     * @return `true` when the binding remains valid and the requested sync work succeeded, otherwise `false`.
     * @remarks
     * This function may re-enable the default viewport, rebind the UI context, and resize
     * the logical root UI rect when required.
     */
    [[nodiscard]] bool SyncToWindow(GameRuntime& Runtime);
    /**
     * @brief Query whether both the viewport id and root UI context id are known.
     * @return `true` when initialization has established a live binding.
     */
    [[nodiscard]] bool IsInitialized() const { return m_viewportId != 0 && m_rootContextId != 0; }

    /**
     * @brief Access the bound renderer viewport id.
     * @return Viewport id, or `0` when uninitialized.
     */
    [[nodiscard]] std::uint64_t ViewportId() const { return m_viewportId; }
    /**
     * @brief Access the bound root UI context id.
     * @return Context id, or `0` when uninitialized.
     */
    [[nodiscard]] std::uint64_t ContextId() const { return m_rootContextId; }

private:
    [[nodiscard]] bool ResolveViewportSize(GameRuntime& Runtime, float& OutWidth, float& OutHeight) const;
    [[nodiscard]] bool EnsureUiBinding(GameRuntime& Runtime) const;

    std::uint64_t m_viewportId = 0;
    std::uint64_t m_rootContextId = 0;
    float m_lastWidth = 0.0f;
    float m_lastHeight = 0.0f;
};

} // namespace SnAPI::GameFramework::Editor
