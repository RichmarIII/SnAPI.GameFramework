#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"
#include "ReflectionAnnotations.h"


namespace SnAPI::GameFramework
{
SnType()
class SNAPI_GAMEFRAMEWORK_API TAAParamsNode : public BaseNode, public NodeCRTP<TAAParamsNode>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TAAParamsNode";

    TAAParamsNode();
    explicit TAAParamsNode(std::string Name);

    SnField(SnKey("ViewportID"), SnConstGetter(GetViewportID))
    std::int64_t& EditViewportID();
    const std::int64_t& GetViewportID() const;

    SnField(SnKey("BlendFactor"), SnConstGetter(GetBlendFactor))
    float& EditBlendFactor();
    const float& GetBlendFactor() const;

    SnField(SnKey("MotionBlendFactor"), SnConstGetter(GetMotionBlendFactor))
    float& EditMotionBlendFactor();
    const float& GetMotionBlendFactor() const;

    SnField(SnKey("ClampStrength"), SnConstGetter(GetClampStrength))
    float& EditClampStrength();
    const float& GetClampStrength() const;

    SnField(SnKey("Sharpen"), SnConstGetter(GetSharpen))
    float& EditSharpen();
    const float& GetSharpen() const;

    SnField(SnKey("JitterScale"), SnConstGetter(GetJitterScale))
    float& EditJitterScale();
    const float& GetJitterScale() const;

    void OnCreate();
    void OnDestroy();
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    void EditorTick(float DeltaSeconds);
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    void ApplyIfNeeded();
    bool ApplyFeatureSettings();
    void InvalidateApplyState();

    std::int64_t m_viewportID = -1;
    float m_blendFactor = 0.06f;
    float m_motionBlendFactor = 0.18f;
    float m_clampStrength = 0.10f;
    float m_sharpen = 0.0f;
    float m_jitterScale = 1.0f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedFeatureRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};
} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
