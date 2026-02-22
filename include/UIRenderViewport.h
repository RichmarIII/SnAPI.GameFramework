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
} // namespace SnAPI::UI

namespace SnAPI::GameFramework
{

class GameRuntime;
enum class ERenderViewportPassGraphPreset : uint8_t;

/**
 * @brief External UI element that owns a renderer viewport and a child UI context.
 *
 * @remarks
 * - The element creates one child `UISystem` context under its parent context.
 * - The element creates one renderer viewport.
 * - Binding is established through `GameRuntime::BindViewportWithUI(...)`.
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

    UIRenderViewport();
    ~UIRenderViewport() override = default;

    void Initialize(SnAPI::UI::UIContext* Context, SnAPI::UI::ElementId Id);

    void SetGameRuntime(GameRuntime* Runtime);
    GameRuntime* GetGameRuntime() const { return m_runtime; }

    void SetViewportCamera(SnAPI::Graphics::ICamera* Camera);
    SnAPI::Graphics::ICamera* GetViewportCamera() const { return m_camera; }
    void SetPointerEventHandler(PointerEventHandler Handler);
    void ClearPointerEventHandler();

    std::uint64_t OwnedViewportId() const { return m_ownedViewportId; }
    std::uint64_t OwnedContextId() const { return m_ownedContextId; }

    void Measure(const SnAPI::UI::UIConstraints& Constraints, SnAPI::UI::UISize& OutDesired) override;
    void Arrange(const SnAPI::UI::UIRect& FinalRect) override;
    void Paint(SnAPI::UI::UIPaintContext& Context) const override;
    void OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context) override;
    void OnFocusChanged(bool Focused) override;

private:
    void SyncViewport();
    void ReleaseOwnedResources();
    static std::uint32_t ComputeRenderExtent(float LogicalSize, float RenderScale);

    GameRuntime* m_runtime = nullptr;
    SnAPI::Graphics::ICamera* m_camera = nullptr;
    std::uint64_t m_ownedViewportId = 0;
    std::uint64_t m_ownedContextId = 0;
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
