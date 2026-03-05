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
class AtmospherePass;
}

namespace SnAPI::GameFramework
{

class SNAPI_GAMEFRAMEWORK_API AtmosphereParamsNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AtmosphereParamsNode";

    AtmosphereParamsNode();
    explicit AtmosphereParamsNode(std::string Name);

    std::int64_t& EditViewportID();
    const std::int64_t& GetViewportID() const;

    bool& EditWorldMode();
    const bool& GetWorldMode() const;

    Vec3& EditSunDirection();
    const Vec3& GetSunDirection() const;

    Vec3& EditSunColor();
    const Vec3& GetSunColor() const;

    float& EditExposure();
    const float& GetExposure() const;

    float& EditSunIntensity();
    const float& GetSunIntensity() const;

    Vec3& EditRayleighScattering();
    const Vec3& GetRayleighScattering() const;

    float& EditRayleighScaleHeight();
    const float& GetRayleighScaleHeight() const;

    Vec3& EditMieScattering();
    const Vec3& GetMieScattering() const;

    float& EditMieScaleHeight();
    const float& GetMieScaleHeight() const;

    Vec3& EditMieAbsorption();
    const Vec3& GetMieAbsorption() const;

    float& EditMieAnisotropyG();
    const float& GetMieAnisotropyG() const;

    float& EditPlanetRadiusMeters();
    const float& GetPlanetRadiusMeters() const;

    float& EditAtmosphereRadiusMeters();
    const float& GetAtmosphereRadiusMeters() const;

    float& EditCameraGroundOffsetMeters();
    const float& GetCameraGroundOffsetMeters() const;

    float& EditMaxSunDistanceMeters();
    const float& GetMaxSunDistanceMeters() const;

    std::uint32_t& EditViewSampleCount();
    const std::uint32_t& GetViewSampleCount() const;

    std::uint32_t& EditSunSampleCount();
    const std::uint32_t& GetSunSampleCount() const;

    float& EditMultiScatterStrength();
    const float& GetMultiScatterStrength() const;

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

    bool m_worldMode = false;
    Vec3 m_sunDirection{static_cast<Scalar>(0.70710677), static_cast<Scalar>(0.70710677), static_cast<Scalar>(0.0)};
    Vec3 m_sunColor{static_cast<Scalar>(1.0), static_cast<Scalar>(1.0), static_cast<Scalar>(1.0)};
    float m_exposure = 8.0f;
    float m_sunIntensity = 1.0f;

    Vec3 m_rayleighScattering{static_cast<Scalar>(5.8e-6), static_cast<Scalar>(13.5e-6), static_cast<Scalar>(33.1e-6)};
    float m_rayleighScaleHeight = 8000.0f;
    Vec3 m_mieScattering{static_cast<Scalar>(21.0e-6), static_cast<Scalar>(21.0e-6), static_cast<Scalar>(21.0e-6)};
    float m_mieScaleHeight = 1200.0f;
    Vec3 m_mieAbsorption{static_cast<Scalar>(0.0), static_cast<Scalar>(0.0), static_cast<Scalar>(0.0)};
    float m_mieAnisotropyG = 0.76f;

    float m_planetRadiusMeters = 6360.0e3f;
    float m_atmosphereRadiusMeters = 6420.0e3f;
    float m_cameraGroundOffsetMeters = 100.0f;
    float m_maxSunDistanceMeters = 120.0e3f;

    std::uint32_t m_viewSampleCount = 4;
    std::uint32_t m_sunSampleCount = 4;
    float m_multiScatterStrength = 2.0e-6f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
