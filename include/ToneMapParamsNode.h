#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class ToneMapPass;
}

namespace SnAPI::GameFramework
{

class SNAPI_GAMEFRAMEWORK_API ToneMapParamsNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::ToneMapParamsNode";

    ToneMapParamsNode();
    explicit ToneMapParamsNode(std::string Name);

    std::int64_t& EditViewportID();
    const std::int64_t& GetViewportID() const;

    float& EditExposure();
    const float& GetExposure() const;

    float& EditGamma();
    const float& GetGamma() const;

    float& EditDitherStrength();
    const float& GetDitherStrength() const;

    float& EditAgXExposureBiasStops();
    const float& GetAgXExposureBiasStops() const;

    float& EditAgXSaturation();
    const float& GetAgXSaturation() const;

    float& EditAgXContrast();
    const float& GetAgXContrast() const;

    float& EditAgXPivot();
    const float& GetAgXPivot() const;

    float& EditAgXGamutThreshold();
    const float& GetAgXGamutThreshold() const;

    float& EditAgXGamutKnee();
    const float& GetAgXGamutKnee() const;

    float& EditAcesSaturation();
    const float& GetAcesSaturation() const;

    float& EditAcesWhitePoint();
    const float& GetAcesWhitePoint() const;

    bool& EditEnableACES();
    const bool& GetEnableACES() const;

    bool& EditEnableAgX();
    const bool& GetEnableAgX() const;

    bool& EditEnableCompare();
    const bool& GetEnableCompare() const;

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

    float m_exposure = 1.0f;
    float m_gamma = 2.2f;
    float m_ditherStrength = 1.0f;
    float m_agXExposureBiasStops = -0.5f;
    float m_agXSaturation = 1.05f;
    float m_agXContrast = 1.03f;
    float m_agXPivot = 0.5f;
    float m_agXGamutThreshold = 0.9f;
    float m_agXGamutKnee = 0.5f;
    float m_acesSaturation = 1.05f;
    float m_acesWhitePoint = 11.2f;
    bool m_enableACES = true;
    bool m_enableAgX = false;
    bool m_enableCompare = false;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
