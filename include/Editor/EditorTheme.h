#pragma once

#include <UITheme.h>

namespace SnAPI::GameFramework::Editor
{

/**
 * @brief Theme for SnAPI.GameFramework.Editor shell widgets.
 */
class EditorTheme final : public SnAPI::UI::Theme
{
public:
    EditorTheme();
    void Initialize() override;
};

} // namespace SnAPI::GameFramework::Editor
