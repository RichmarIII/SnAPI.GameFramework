#pragma once

#if defined(SNAPI_GF_ENABLE_UI)

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

#include <UIHandles.h>

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Runtime node that owns a root-UI real-time frame graph.
 *
 * `FrameGraphNode` lets authored levels opt into a frame-time/FPS overlay without hardcoding
 * game-specific UI into runtime startup. When the node is live in a world with UI enabled, it
 * creates one `UIRealtimeGraph` in the root context and feeds frame-time/FPS samples from node
 * tick updates.
 *
 * Core semantics:
 * - The graph is created lazily against the root UI context.
 * - Settings edits rebuild the graph so authored changes are reflected immediately.
 * - Destroying the node removes the owned graph element from the UI tree.
 *
 * Threading model:
 * - Main-thread only.
 */
class SNAPI_GAMEFRAMEWORK_API FrameGraphNode final : public BaseNode, public NodeCRTP<FrameGraphNode>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::FrameGraphNode";

    FrameGraphNode();
    explicit FrameGraphNode(std::string Name);

    std::string& EditTitle();
    const std::string& GetTitle() const;

    bool& EditShowLegend();
    const bool& GetShowLegend() const;

    std::uint32_t& EditValuePrecision();
    const std::uint32_t& GetValuePrecision() const;

    std::uint32_t& EditSampleCapacity();
    const std::uint32_t& GetSampleCapacity() const;

    float& EditFrameTimeMaxSeconds();
    const float& GetFrameTimeMaxSeconds() const;

    float& EditFpsMax();
    const float& GetFpsMax() const;

    void OnCreate();
    void OnDestroy();
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    void EditorTick(float DeltaSeconds);
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    void ResetGraphState();
    void DestroyGraph();
    void EnsureGraph();
    void PushSamples(float DeltaSeconds);

    static constexpr std::uint32_t kInvalidSeries = std::numeric_limits<std::uint32_t>::max();

    std::string m_title{"Frame Graph"};
    bool m_showLegend = true;
    std::uint32_t m_valuePrecision = 4u;
    std::uint32_t m_sampleCapacity = 180u;
    float m_frameTimeMaxSeconds = 0.1f;
    float m_fpsMax = 120.0f;

    SnAPI::UI::ElementId m_graphElementId{};
    std::uint64_t m_graphContextId = 0;
    std::uint32_t m_frameTimeSeries = kInvalidSeries;
    std::uint32_t m_fpsSeries = kInvalidSeries;
    bool m_graphNeedsRebuild = true;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_UI
