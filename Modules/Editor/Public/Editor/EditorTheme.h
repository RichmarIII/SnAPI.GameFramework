#pragma once

#include <UITheme.h>

namespace SnAPI::GameFramework::Editor
{

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @brief Theme for editor shell widgets and chrome.
 *
 * `EditorTheme` centralizes the default visual styling used by the editor module's UI.
 * The theme exists so editor widgets can share one consistent palette, typography choice,
 * spacing language, and widget-state styling instead of each service defining ad-hoc values.
 *
 * Ownership and lifetime:
 * - Same as `SnAPI::UI::Theme`; the caller owns the theme object and registers it with the UI layer.
 *
 * Threading model:
 * - Main-thread only.
 *
 * @see SnAPI::UI::Theme
 */
class EditorTheme final : public SnAPI::UI::Theme
{
public:
    /**
     * @brief Construct the editor theme object.
     * @remarks The constructor does not fully populate theme state; call `Initialize()` before use.
     */
    EditorTheme();
    /**
     * @brief Populate theme colors, fonts, and widget style rules.
     * @remarks Safe to call when rebuilding theme state, though callers typically invoke it once.
     */
    void Initialize() override;
};

} // namespace SnAPI::GameFramework::Editor
