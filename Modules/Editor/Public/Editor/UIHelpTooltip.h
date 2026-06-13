#pragma once

#include "Editor/EditorExport.h"

#include <string>

#include <UIBadge.h>
#include <UITooltip.h>

namespace SnAPI::GameFramework::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Lightweight hover badge that renders a detailed tooltip bubble for form fields.
 *
 * `UIHelpTooltip` is used throughout the editor build/project/module dialogs to attach
 * detailed hover help without forcing fixed-size explanatory text blocks into the form
 * layout. The badge behaves like a compact info chip:
 * - it measures like a normal `UIBadge`
 * - it tracks pointer hover directly
 * - it paints a floating `UITooltip` anchored to the badge while hovered
 *
 * Core semantics:
 * - the tooltip only appears while the badge itself is hovered
 * - the tooltip bubble is clamped to the active UI viewport
 * - wheel input over the tooltip scrolls long help text
 *
 * Threading model:
 * - Main-thread only.
 */
class SNAPI_GAMEFRAMEWORK_EDITOR_API UIHelpTooltip final : public SnAPI::UI::UIBadge
{
public:
    using PropertyKey = SnAPI::UI::PropertyKey;
    template<typename TValue>
    using TPropertyRef = SnAPI::UI::TPropertyRef<TValue>;

    /** @brief Detailed help text shown inside the hover tooltip bubble. */
    SNAPI_PROPERTY_INV(std::string, TooltipText, SnAPI::UI::EInvalidation::Layout);

    /**
     * @brief Construct one compact help badge with editor-friendly defaults.
     */
    UIHelpTooltip();

    /**
     * @brief Initialize the badge and its internal tooltip state.
     * @param Context Borrowed owning UI context.
     * @param Id Assigned element id.
     */
    void Initialize(SnAPI::UI::UIContext* Context, SnAPI::UI::ElementId Id);

    /**
     * @brief Paint the badge and any active tooltip bubble.
     * @param Context Borrowed paint context.
     */
    void Paint(SnAPI::UI::UIPaintContext& Context) const override;

    /**
     * @brief Track hover state and tooltip scrolling.
     * @param Context Borrowed routed-event context.
     */
    void OnRoutedEvent(SnAPI::UI::RoutedEventContext& Context) override;

private:
    /**
     * @brief Return the current viewport rectangle used to clamp the tooltip bubble.
     * @return Viewport rectangle in screen coordinates.
     */
    [[nodiscard]] SnAPI::UI::UIRect ResolveViewportRect() const;

    /**
     * @brief Synchronize the internal tooltip from the current badge state.
     */
    void SyncTooltipState() const;

    mutable SnAPI::UI::UITooltip m_tooltip{}; /**< @brief Floating tooltip bubble painted while the badge is hovered. */
};

} // namespace SnAPI::GameFramework::Editor
