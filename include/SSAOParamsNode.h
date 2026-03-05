#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class SSAOPass;
}

namespace SnAPI::GameFramework
{

class SNAPI_GAMEFRAMEWORK_API SSAOParamsNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SSAOParamsNode";

    SSAOParamsNode();
    explicit SSAOParamsNode(std::string Name);

    std::int64_t& EditViewportID();
    const std::int64_t& GetViewportID() const;

    float& EditRadius();
    const float& GetRadius() const;

    float& EditBias();
    const float& GetBias() const;

    float& EditIntensity();
    const float& GetIntensity() const;

    float& EditMaxDistance();
    const float& GetMaxDistance() const;

    std::uint32_t& EditSliceCount();
    const std::uint32_t& GetSliceCount() const;

    std::uint32_t& EditStepsPerSlice();
    const std::uint32_t& GetStepsPerSlice() const;

    float& EditFalloffStart();
    const float& GetFalloffStart() const;

    float& EditFalloffEnd();
    const float& GetFalloffEnd() const;

    float& EditMaxPixelRadius();
    const float& GetMaxPixelRadius() const;

    float& EditThickness();
    const float& GetThickness() const;

    float& EditDenoiseBlurBeta();
    const float& GetDenoiseBlurBeta() const;

    float& EditTemporalBlendFactor();
    const float& GetTemporalBlendFactor() const;

    float& EditDisocclusionThreshold();
    const float& GetDisocclusionThreshold() const;

    float& EditVelocityWeight();
    const float& GetVelocityWeight() const;

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

    float m_radius = 3.0f;
    float m_bias = 0.025f;
    float m_intensity = 1.0f;
    float m_maxDistance = 1000000.0f;
    std::uint32_t m_sliceCount = 3;
    std::uint32_t m_stepsPerSlice = 6;
    float m_falloffStart = 0.9f;
    float m_falloffEnd = 1.0f;
    float m_maxPixelRadius = 128.0f;
    float m_thickness = 0.5f;
    float m_denoiseBlurBeta = 1.5f;
    float m_temporalBlendFactor = 0.01f;
    float m_disocclusionThreshold = 0.02f;
    float m_velocityWeight = 10.0f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
