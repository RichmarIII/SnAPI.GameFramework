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
 * @brief Owns the root editor render viewport and binds it to the root UI context.
 *
 * `EditorViewportBinding` is the small stateful adapter that keeps the editor shell's root
 * render viewport synchronized with the runtime window and the root UI context.
 *
 * Core semantics:
 * - Initialization creates an explicit renderer viewport instead of using the renderer's
 *   implicit default viewport.
 * - The created viewport is bound to the UI root context and assigned the `UiPresentOnly`
 *   pass-graph preset.
 * - `SyncToWindow()` preserves explicit-viewport mode, recreates missing bindings when
 *   possible, and keeps logical UI size and render extent in sync with the current window.
 * - Render-extent resize is intentionally deferred while the left mouse button is held to
 *   avoid resizing the render target during active drag operations.
 *
 * Ownership and lifetime:
 * - The class stores only ids and cached size state.
 * - The underlying viewport and UI binding are owned by the runtime subsystems, not by this object.
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
     * @brief Create and bind the root editor viewport.
     * @param Runtime Initialized runtime that owns renderer and UI systems.
     * @param ViewportName Optional logical viewport name. Empty input keeps the current/default name.
     * @return Success or an error.
     * @pre Renderer and UI subsystems must both be initialized.
     * @post On success, `ViewportId()` and `ContextId()` reference a live explicit binding.
     */
    Result Initialize(GameRuntime& Runtime, std::string ViewportName);
    /**
     * @brief Tear down the current viewport binding if it exists.
     * @param Runtime Optional runtime used to unbind and destroy the live viewport. May be null during late teardown.
     * @remarks Safe to call repeatedly.
     */
    void Shutdown(GameRuntime* Runtime);

    /**
     * @brief Synchronize the bound viewport with the current window and UI state.
     * @param Runtime Initialized runtime.
     * @return `true` when the binding remains valid and the requested sync work succeeded, otherwise `false`.
     * @remarks
     * This function may recreate a missing viewport, rebind the UI context, and resize both
     * the logical viewport rect and the render extent when required.
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

    std::string m_viewportName{"Editor.RootViewport"};
    std::uint64_t m_viewportId = 0;
    std::uint64_t m_rootContextId = 0;
    float m_lastWidth = 0.0f;
    float m_lastHeight = 0.0f;
    std::uint32_t m_appliedRenderWidth = 0;
    std::uint32_t m_appliedRenderHeight = 0;
    std::uint32_t m_pendingRenderWidth = 0;
    std::uint32_t m_pendingRenderHeight = 0;
    bool m_hasPendingRenderExtentResize = false;
};

} // namespace SnAPI::GameFramework::Editor
