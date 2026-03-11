#pragma once

#include "Editor/EditorExport.h"
#include "Editor/IEditorService.h"

#include <UIHandles.h>

#include <cstdint>
#include <limits>

namespace SnAPI::UI
{
class UIContext;
}

namespace SnAPI::GameFramework::Editor
{

class SNAPI_GAMEFRAMEWORK_EDITOR_API EditorGameViewportOverlayService final : public IEditorService
{
public:
    [[nodiscard]] std::string_view Name() const override;
    [[nodiscard]] std::vector<std::type_index> Dependencies() const override;
    Result Initialize(EditorServiceContext& Context) override;
    void Tick(EditorServiceContext& Context, float DeltaSeconds) override;
    void Shutdown(EditorServiceContext& Context) override;

private:
    void ResetOverlayState();
    bool EnsureOverlayElements(SnAPI::UI::UIContext& OverlayContext);
    void UpdateOverlayVisibility(SnAPI::UI::UIContext& OverlayContext, int32_t ActiveTabIndex);
    void UpdateOverlaySamples(SnAPI::UI::UIContext& OverlayContext, float DeltaSeconds);

    std::uint64_t m_overlayContextId = 0;
    SnAPI::UI::ElementId m_hudPanel{};
    SnAPI::UI::ElementId m_hudGraph{};
    SnAPI::UI::ElementId m_hudFrameLabel{};
    SnAPI::UI::ElementId m_hudFpsLabel{};
    std::uint32_t m_hudFrameSeries = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t m_hudFpsSeries = std::numeric_limits<std::uint32_t>::max();

    SnAPI::UI::ElementId m_profilerPanel{};
    SnAPI::UI::ElementId m_profilerGraph{};
    SnAPI::UI::ElementId m_profilerFrameLabel{};
    SnAPI::UI::ElementId m_profilerFpsLabel{};
    std::uint32_t m_profilerFrameSeries = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t m_profilerFpsSeries = std::numeric_limits<std::uint32_t>::max();
};

} // namespace SnAPI::GameFramework::Editor
