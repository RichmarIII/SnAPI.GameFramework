#pragma once

#if defined(SNAPI_GF_ENABLE_UI) && defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <optional>
#include <string>

#include "Export.h"

#include <UIDelegates.h>
#include <UIElementBase.h>

namespace SnAPI::Graphics
{
class ICamera;
} // namespace SnAPI::Graphics

namespace SnAPI::UI
{
struct PointerEvent;
class UIImage;
} // namespace SnAPI::UI

namespace SnAPI::GameFramework
{

class GameRuntime;
enum class ERenderViewportPassGraphPreset : uint8_t;

/**
 * @ingroup SnAPI_GameFramework
 * @brief UI element that owns a renderer viewport, a child UI context, and an image presenter.
 *
 * `UIRenderViewport` is the bridge between the UI tree and the renderer's virtual-viewport
 * system. It behaves like a normal UI element in layout, but lazily creates a renderer
 * viewport, a render-target swapchain, a dedicated child `UISystem` context, and an internal
 * `UIImage` used to present the viewport output back into the parent UI tree.
 *
 * Why this abstraction exists:
 * - to embed live 3D or off-screen rendering inside arbitrary UI layouts
 * - to give each embedded viewport its own child UI context for overlays and input routing
 * - to keep renderer/UI binding logic out of higher-level editor and game layout code
 *
 * Core semantics:
 * - owned resources are created lazily from `Arrange()` and `Paint()` through `SyncViewport()`
 * - the viewport is not guaranteed to exist immediately after the element is constructed or attached
 * - the element owns exactly one child UI context and one renderer viewport while active
 * - the final rendered image is exposed to UI through an internally managed texture id and `UIImage`
 * - viewport/context binding is established through `GameRuntime::BindViewportWithUI(...)`
 *
 * Ownership and lifetime:
 * - Owned by its parent `UIContext`.
 * - Owns the renderer viewport, render-target swapchain, child UI context, and presenter image it creates.
 * - `GameRuntime*` and camera pointers are borrowed only.
 * - Owned resource ids become invalid after `OnDestroyed()` or when `SetGameRuntime(...)` replaces the runtime.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @warning
 * Viewport creation is layout-driven and lazy. Code that depends on the viewport existing
 * must not assume it is ready during unrelated object `OnCreate` paths unless bootstrap has
 * already forced at least one UI/layout/render synchronization pass.
 *
 * @see GameRuntime
 * @see UISystem
 */
class SNAPI_GAMEFRAMEWORK_API UIRenderViewport final : public SnAPI::UI::UIElementBase
{
public:
    using PointerEventHandler = SnAPI::UI::TDelegate<void(const SnAPI::UI::PointerEvent&, std::uint32_t, bool)>;
    using PropertyKey = SnAPI::UI::PropertyKey;
    template<typename TValue>
    using TPropertyRef = SnAPI::UI::TPropertyRef<TValue>;

    SNAPI_PROPERTY_INV(std::string, ViewportName, SnAPI::UI::EInvalidation::Layout);
    SNAPI_PROPERTY_INV(bool, Enabled, SnAPI::UI::EInvalidation::Layout);
    SNAPI_PROPERTY_INV(float, RenderScale, SnAPI::UI::EInvalidation::Layout);
    SNAPI_PROPERTY_INV(std::int32_t, ViewportIndex, SnAPI::UI::EInvalidation::Layout);
    SNAPI_PROPERTY_INV(ERenderViewportPassGraphPreset, PassGraphPreset, SnAPI::UI::EInvalidation::Layout);
    SNAPI_PROPERTY_INV(bool, AutoRegisterPassGraph, SnAPI::UI::EInvalidation::Layout);

    SNAPI_PROPERTY_INV(SnAPI::UI::Color, BackgroundColor, SnAPI::UI::EInvalidation::Paint);
    SNAPI_PROPERTY_INV(SnAPI::UI::Color, BorderColor, SnAPI::UI::EInvalidation::Paint);
    SNAPI_PROPERTY_INV(float, BorderThickness, SnAPI::UI::EInvalidation::Paint);
    SNAPI_PROPERTY_INV(float, CornerRadius, SnAPI::UI::EInvalidation::Paint);

    /** @brief Construct the element with default viewport styling and behavior properties. */
    UIRenderViewport();
    ~UIRenderViewport() override = default;

    /**
     * @brief Initialize the element within a UI context.
     * @param Context Owning UI context.
     * @param Id Element id assigned by the UI system.
     * @post Ensures the internal presenter image exists.
     */
    void Initialize(SnAPI::UI::UIContext* Context, SnAPI::UI::ElementId Id);

    /**
     * @brief Attach the runtime used for viewport/UI binding and renderer access.
     * @param Runtime Borrowed runtime pointer, or `nullptr` to detach.
     * @warning Releasing or replacing the runtime destroys all owned viewport resources first.
     */
    void SetGameRuntime(GameRuntime* Runtime);
    /** @brief Access the borrowed runtime pointer currently used by this element. @return Runtime pointer or `nullptr`. */
    GameRuntime* GetGameRuntime() const { return m_runtime; }

    /**
     * @brief Set the camera rendered by the owned viewport.
     * @param Camera Borrowed camera pointer, or `nullptr`.
     * @remarks Also updates the camera aspect ratio when the viewport has a valid render extent.
     */
    void SetViewportCamera(SnAPI::Graphics::ICamera* Camera);
    /** @brief Access the borrowed camera pointer currently used by the viewport. @return Camera pointer or `nullptr`. */
    SnAPI::Graphics::ICamera* GetViewportCamera() const { return m_camera; }
    /**
     * @brief Install a pointer-event callback invoked for pointer move/down/up routing.
     * @param Handler Delegate invoked with the routed event, routed event type id, and hit-test result.
     */
    void SetPointerEventHandler(PointerEventHandler Handler);
    /** @brief Remove the currently installed pointer-event callback. */
    void ClearPointerEventHandler();

    /** @brief Access the owned renderer viewport id. @return Viewport id or `0` when no viewport exists yet. */
    std::uint64_t OwnedViewportId() const { return m_ownedViewportId; }
    /** @brief Access the owned child UI context id. @return Context id or `0` when no child context exists yet. */
    std::uint64_t OwnedContextId() const { return m_ownedContextId; }

    /** @brief Measure the desired UI size for layout. @param Constraints Parent constraints. @param OutDesired Receives desired size. */
    void Measure(const SnAPI::UI::UIConstraints& Constraints, SnAPI::UI::UISize& OutDesired) override;
    /** @brief Arrange the element and opportunistically synchronize owned viewport resources. @param FinalRect Final layout rect. */
    void Arrange(const SnAPI::UI::UIRect& FinalRect) override;
    /** @brief Paint the element contents. @param Context Paint context. @remarks Also triggers lazy viewport synchronization. */
    void Paint(SnAPI::UI::UIPaintContext& Context) const override;
    /** @brief Route UI events relevant to the viewport shell. @param Context Routed-event context. */
    void OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context) override;
    /** @brief Notification that keyboard focus changed. @param Focused New focus state. */
    void OnFocusChanged(bool Focused) override;
    /** @brief Release all owned renderer/UI resources before final destruction. */
    void OnDestroyed() override;

private:
    void EnsureImagePresenter();
    void SyncViewport();
    void ReleaseOwnedResources();
    static std::uint32_t ComputeRenderExtent(float LogicalSize, float RenderScale);

    GameRuntime* m_runtime = nullptr;
    SnAPI::Graphics::ICamera* m_camera = nullptr;
    std::uint64_t m_ownedViewportId = 0;
    std::uint64_t m_ownedSwapChainId = 0;
    std::uint64_t m_ownedContextId = 0;
    SnAPI::UI::ElementId m_presenterImageId{};
    SnAPI::UI::TextureId m_presentedTextureId{};
    bool m_bindingEstablished = false;
    std::uint32_t m_appliedRenderWidth = 0;
    std::uint32_t m_appliedRenderHeight = 0;
    std::uint32_t m_pendingRenderWidth = 0;
    std::uint32_t m_pendingRenderHeight = 0;
    bool m_hasPendingRenderExtentResize = false;
    std::optional<ERenderViewportPassGraphPreset> m_registeredPassGraphPreset{};
    PointerEventHandler m_pointerEventHandler{};
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI && SNAPI_GF_ENABLE_RENDERER
