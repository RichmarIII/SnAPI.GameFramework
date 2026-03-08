#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class TAAPass;
}

namespace SnAPI::GameFramework
{
class SNAPI_GAMEFRAMEWORK_API TAAParamsNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TAAParamsNode";

    TAAParamsNode();
    explicit TAAParamsNode(std::string Name);

    std::int64_t& EditViewportID();
    const std::int64_t& GetViewportID() const;

    float& EditBlendFactor();
    const float& GetBlendFactor() const;

    float& EditMotionBlendFactor();
    const float& GetMotionBlendFactor() const;

    float& EditClampStrength();
    const float& GetClampStrength() const;

    float& EditSharpen();
    const float& GetSharpen() const;

    void OnCreate();
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    void EditorTick(float DeltaSeconds);
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    void ApplyIfNeeded();
    bool ApplyToPass();
    void InvalidatePassCache();

    std::int64_t m_viewportID = -1;
    float m_blendFactor = 0.06f;
    float m_motionBlendFactor = 0.18f;
    float m_clampStrength = 0.10f;
    float m_sharpen = 0.0f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
