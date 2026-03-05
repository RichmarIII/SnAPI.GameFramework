#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"
#include "Math.h"

namespace SnAPI::Graphics
{
class HeightFogPass;
}

namespace SnAPI::GameFramework
{

class SNAPI_GAMEFRAMEWORK_API HeightFogParamsNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::HeightFogParamsNode";

    HeightFogParamsNode();
    explicit HeightFogParamsNode(std::string Name);

    std::int64_t& EditViewportID();
    const std::int64_t& GetViewportID() const;

    float& EditDensity();
    const float& GetDensity() const;

    float& EditHeightFalloff();
    const float& GetHeightFalloff() const;

    bool& EditUseAbsoluteHeight();
    const bool& GetUseAbsoluteHeight() const;

    double& EditHeightOffsetAbsoluteY();
    const double& GetHeightOffsetAbsoluteY() const;

    bool& EditUseActiveCameraYAsRebaseOrigin();
    const bool& GetUseActiveCameraYAsRebaseOrigin() const;

    double& EditRebaseOriginAbsoluteY();
    const double& GetRebaseOriginAbsoluteY() const;

    float& EditHeightOffsetRebased();
    const float& GetHeightOffsetRebased() const;

    float& EditStartDistance();
    const float& GetStartDistance() const;

    Vec3& EditFogColor();
    const Vec3& GetFogColor() const;

    Vec3& EditHorizonColor();
    const Vec3& GetHorizonColor() const;

    Vec3& EditZenithColor();
    const Vec3& GetZenithColor() const;

    float& EditSkyBlendStartDistance();
    const float& GetSkyBlendStartDistance() const;

    float& EditSkyBlendEndDistance();
    const float& GetSkyBlendEndDistance() const;

    float& EditSkyBlendStrength();
    const float& GetSkyBlendStrength() const;

    float& EditTauDitherAmplitude();
    const float& GetTauDitherAmplitude() const;

    Vec3& EditSunDirection();
    const Vec3& GetSunDirection() const;

    float& EditSunAnisotropyG();
    const float& GetSunAnisotropyG() const;

    Vec3& EditSunColor();
    const Vec3& GetSunColor() const;

    float& EditSunInscatterIntensity();
    const float& GetSunInscatterIntensity() const;

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

    float m_density = 0.004f;
    float m_heightFalloff = 0.008f;
    bool m_useAbsoluteHeight = true;
    double m_heightOffsetAbsoluteY = 0.0;
    bool m_useActiveCameraYAsRebaseOrigin = true;
    double m_rebaseOriginAbsoluteY = 0.0;
    float m_heightOffsetRebased = 0.0f;
    float m_startDistance = 10.0f;

    Vec3 m_fogColor{static_cast<Scalar>(0.64), static_cast<Scalar>(0.70), static_cast<Scalar>(0.76)};
    Vec3 m_horizonColor{static_cast<Scalar>(0.69), static_cast<Scalar>(0.72), static_cast<Scalar>(0.75)};
    Vec3 m_zenithColor{static_cast<Scalar>(0.47), static_cast<Scalar>(0.57), static_cast<Scalar>(0.69)};

    float m_skyBlendStartDistance = 100.0f;
    float m_skyBlendEndDistance = 1200.0f;
    float m_skyBlendStrength = 1.0f;
    float m_tauDitherAmplitude = 0.005f;

    Vec3 m_sunDirection{static_cast<Scalar>(0.0), static_cast<Scalar>(-1.0), static_cast<Scalar>(0.0)};
    float m_sunAnisotropyG = 0.7f;
    Vec3 m_sunColor{static_cast<Scalar>(1.0), static_cast<Scalar>(0.95), static_cast<Scalar>(0.85)};
    float m_sunInscatterIntensity = 0.05f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
