#include "AtmosphereCompositeParamsNode.h"
#include "AtmosphereParamsNode.h"
#include "BloomParamsNode.h"
#include "DeferredShadingParamsNode.h"
#include "HeightFogParamsNode.h"
#include "SSAOParamsNode.h"
#include "SSGIParamsNode.h"
#include "SSRParamsNode.h"
#include "TAAParamsNode.h"
#include "ToneMapParamsNode.h"

#if defined(SNAPI_GF_ENABLE_RENDERER_NEW)

#include <utility>

namespace SnAPI::GameFramework
{

#define SNAPI_GF_NODE_LIFECYCLE(ClassName) \
    ClassName::ClassName() { TypeKey(StaticTypeId<ClassName>()); } \
    ClassName::ClassName(std::string Name) : BaseNode(std::move(Name)) { TypeKey(StaticTypeId<ClassName>()); } \
    void ClassName::OnCreate() { m_applyPending = false; } \
    void ClassName::OnDestroy() { InvalidatePassCache(); } \
    void ClassName::Tick(const float DeltaSeconds) { (void)DeltaSeconds; ApplyIfNeeded(); } \
    void ClassName::ApplyIfNeeded() { if (m_applyPending) { m_applyPending = !ApplyToPass(); } } \
    bool ClassName::ApplyToPass() { return false; } \
    void ClassName::InvalidatePassCache() { m_applyPending = true; m_lastAppliedPassGraphRevision = 0; m_lastAppliedViewportID = 0; }

#if defined(WITH_EDITOR) && WITH_EDITOR
#define SNAPI_GF_NODE_EDITOR_LIFECYCLE(ClassName) \
    void ClassName::EditorTick(const float DeltaSeconds) { Tick(DeltaSeconds); } \
    void ClassName::EditorOnPropertyChanged(const std::string_view Name) { (void)Name; InvalidatePassCache(); }
#else
#define SNAPI_GF_NODE_EDITOR_LIFECYCLE(ClassName)
#endif

#define SNAPI_GF_ACCESS(Type, ClassName, Name, Member) \
    Type& ClassName::Edit##Name() { m_applyPending = true; return Member; } \
    const Type& ClassName::Get##Name() const { return Member; }

SNAPI_GF_NODE_LIFECYCLE(DeferredShadingParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(DeferredShadingParamsNode)
SNAPI_GF_ACCESS(std::int64_t, DeferredShadingParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugMotionVectors, m_debugMotionVectors)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugNormals, m_debugNormals)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugAlbedo, m_debugAlbedo)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugAO, m_debugAO)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugRoughness, m_debugRoughness)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugMetallic, m_debugMetallic)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugDepth, m_debugDepth)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugTextureCoords, m_debugTextureCoords)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugDirectLighting, m_debugDirectLighting)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugGI, m_debugGI)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugSpecular, m_debugSpecular)
SNAPI_GF_ACCESS(bool, DeferredShadingParamsNode, DebugLighting, m_debugLighting)

SNAPI_GF_NODE_LIFECYCLE(SSAOParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(SSAOParamsNode)
SNAPI_GF_ACCESS(std::int64_t, SSAOParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, SSAOParamsNode, Radius, m_radius)
SNAPI_GF_ACCESS(float, SSAOParamsNode, Bias, m_bias)
SNAPI_GF_ACCESS(float, SSAOParamsNode, Intensity, m_intensity)
SNAPI_GF_ACCESS(float, SSAOParamsNode, MaxDistance, m_maxDistance)
SNAPI_GF_ACCESS(std::uint32_t, SSAOParamsNode, SliceCount, m_sliceCount)
SNAPI_GF_ACCESS(std::uint32_t, SSAOParamsNode, StepsPerSlice, m_stepsPerSlice)
SNAPI_GF_ACCESS(float, SSAOParamsNode, FalloffStart, m_falloffStart)
SNAPI_GF_ACCESS(float, SSAOParamsNode, FalloffEnd, m_falloffEnd)
SNAPI_GF_ACCESS(float, SSAOParamsNode, MaxPixelRadius, m_maxPixelRadius)
SNAPI_GF_ACCESS(float, SSAOParamsNode, Thickness, m_thickness)
SNAPI_GF_ACCESS(float, SSAOParamsNode, DenoiseBlurBeta, m_denoiseBlurBeta)
SNAPI_GF_ACCESS(float, SSAOParamsNode, TemporalBlendFactor, m_temporalBlendFactor)
SNAPI_GF_ACCESS(float, SSAOParamsNode, DisocclusionThreshold, m_disocclusionThreshold)
SNAPI_GF_ACCESS(float, SSAOParamsNode, VelocityWeight, m_velocityWeight)

SNAPI_GF_NODE_LIFECYCLE(SSGIParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(SSGIParamsNode)
SNAPI_GF_ACCESS(std::int64_t, SSGIParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, SSGIParamsNode, Intensity, m_intensity)
SNAPI_GF_ACCESS(float, SSGIParamsNode, MaxDistance, m_maxDistance)
SNAPI_GF_ACCESS(float, SSGIParamsNode, Thickness, m_thickness)
SNAPI_GF_ACCESS(float, SSGIParamsNode, SurfaceBias, m_surfaceBias)
SNAPI_GF_ACCESS(std::uint32_t, SSGIParamsNode, MaxSteps, m_maxSteps)
SNAPI_GF_ACCESS(std::uint32_t, SSGIParamsNode, RayCount, m_rayCount)
SNAPI_GF_ACCESS(float, SSGIParamsNode, DepthSigma, m_depthSigma)
SNAPI_GF_ACCESS(float, SSGIParamsNode, NormalSigma, m_normalSigma)
SNAPI_GF_ACCESS(float, SSGIParamsNode, RadianceClamp, m_radianceClamp)
SNAPI_GF_ACCESS(float, SSGIParamsNode, MaxPixelRadius, m_maxPixelRadius)
SNAPI_GF_ACCESS(float, SSGIParamsNode, StepExponent, m_stepExponent)
SNAPI_GF_ACCESS(float, SSGIParamsNode, TemporalBlendFactor, m_temporalBlendFactor)
SNAPI_GF_ACCESS(float, SSGIParamsNode, DisocclusionThreshold, m_disocclusionThreshold)
SNAPI_GF_ACCESS(float, SSGIParamsNode, ClampStrength, m_clampStrength)
SNAPI_GF_ACCESS(float, SSGIParamsNode, VelocityWeight, m_velocityWeight)
SNAPI_GF_ACCESS(float, SSGIParamsNode, LowLumaBoost, m_lowLumaBoost)
SNAPI_GF_ACCESS(std::uint32_t, SSGIParamsNode, TemporalDebugMode, m_temporalDebugMode)

SNAPI_GF_NODE_LIFECYCLE(SSRParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(SSRParamsNode)
SNAPI_GF_ACCESS(std::int64_t, SSRParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, SSRParamsNode, MaxDistance, m_maxDistance)
SNAPI_GF_ACCESS(float, SSRParamsNode, Thickness, m_thickness)
SNAPI_GF_ACCESS(float, SSRParamsNode, MaxRoughness, m_maxRoughness)
SNAPI_GF_ACCESS(float, SSRParamsNode, RoughnessThreshold, m_roughnessThreshold)
SNAPI_GF_ACCESS(std::uint32_t, SSRParamsNode, MaxSteps, m_maxSteps)
SNAPI_GF_ACCESS(std::uint32_t, SSRParamsNode, MaxBinarySteps, m_maxBinarySteps)
SNAPI_GF_ACCESS(float, SSRParamsNode, ScreenEdgeFade, m_screenEdgeFade)
SNAPI_GF_ACCESS(float, SSRParamsNode, ReflectionFade, m_reflectionFade)
SNAPI_GF_ACCESS(float, SSRParamsNode, TemporalBlendFactor, m_temporalBlendFactor)
SNAPI_GF_ACCESS(float, SSRParamsNode, ClampStrength, m_clampStrength)
SNAPI_GF_ACCESS(float, SSRParamsNode, MotionHistoryReset, m_motionHistoryReset)
SNAPI_GF_ACCESS(std::uint32_t, SSRParamsNode, TemporalDebugMode, m_temporalDebugMode)

SNAPI_GF_NODE_LIFECYCLE(TAAParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(TAAParamsNode)
SNAPI_GF_ACCESS(std::int64_t, TAAParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, TAAParamsNode, BlendFactor, m_blendFactor)
SNAPI_GF_ACCESS(float, TAAParamsNode, MotionBlendFactor, m_motionBlendFactor)
SNAPI_GF_ACCESS(float, TAAParamsNode, ClampStrength, m_clampStrength)
SNAPI_GF_ACCESS(float, TAAParamsNode, Sharpen, m_sharpen)
SNAPI_GF_ACCESS(float, TAAParamsNode, JitterScale, m_jitterScale)

SNAPI_GF_NODE_LIFECYCLE(BloomParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(BloomParamsNode)
SNAPI_GF_ACCESS(std::int64_t, BloomParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, BloomParamsNode, Threshold, m_threshold)
SNAPI_GF_ACCESS(float, BloomParamsNode, Knee, m_knee)
SNAPI_GF_ACCESS(float, BloomParamsNode, Intensity, m_intensity)
SNAPI_GF_ACCESS(float, BloomParamsNode, Scatter, m_scatter)
SNAPI_GF_ACCESS(float, BloomParamsNode, Clamp, m_clamp)
SNAPI_GF_ACCESS(std::uint32_t, BloomParamsNode, MipCount, m_mipCount)

SNAPI_GF_NODE_LIFECYCLE(AtmosphereParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(AtmosphereParamsNode)
SNAPI_GF_ACCESS(std::int64_t, AtmosphereParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(bool, AtmosphereParamsNode, WorldMode, m_worldMode)
SNAPI_GF_ACCESS(Vec3, AtmosphereParamsNode, SunDirection, m_sunDirection)
SNAPI_GF_ACCESS(Vec3, AtmosphereParamsNode, SunColor, m_sunColor)
SNAPI_GF_ACCESS(float, AtmosphereParamsNode, Exposure, m_exposure)
SNAPI_GF_ACCESS(float, AtmosphereParamsNode, SunIntensity, m_sunIntensity)
SNAPI_GF_ACCESS(Vec3, AtmosphereParamsNode, RayleighScattering, m_rayleighScattering)
SNAPI_GF_ACCESS(float, AtmosphereParamsNode, RayleighScaleHeight, m_rayleighScaleHeight)
SNAPI_GF_ACCESS(Vec3, AtmosphereParamsNode, MieScattering, m_mieScattering)
SNAPI_GF_ACCESS(float, AtmosphereParamsNode, MieScaleHeight, m_mieScaleHeight)
SNAPI_GF_ACCESS(Vec3, AtmosphereParamsNode, MieAbsorption, m_mieAbsorption)
SNAPI_GF_ACCESS(float, AtmosphereParamsNode, MieAnisotropyG, m_mieAnisotropyG)
SNAPI_GF_ACCESS(float, AtmosphereParamsNode, PlanetRadiusMeters, m_planetRadiusMeters)
SNAPI_GF_ACCESS(float, AtmosphereParamsNode, AtmosphereRadiusMeters, m_atmosphereRadiusMeters)
SNAPI_GF_ACCESS(float, AtmosphereParamsNode, CameraGroundOffsetMeters, m_cameraGroundOffsetMeters)
SNAPI_GF_ACCESS(float, AtmosphereParamsNode, MaxSunDistanceMeters, m_maxSunDistanceMeters)
SNAPI_GF_ACCESS(std::uint32_t, AtmosphereParamsNode, ViewSampleCount, m_viewSampleCount)
SNAPI_GF_ACCESS(std::uint32_t, AtmosphereParamsNode, SunSampleCount, m_sunSampleCount)
SNAPI_GF_ACCESS(float, AtmosphereParamsNode, MultiScatterStrength, m_multiScatterStrength)

SNAPI_GF_NODE_LIFECYCLE(AtmosphereCompositeParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(AtmosphereCompositeParamsNode)
SNAPI_GF_ACCESS(std::int64_t, AtmosphereCompositeParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, AtmosphereCompositeParamsNode, DepthThreshold, m_depthThreshold)
SNAPI_GF_ACCESS(float, AtmosphereCompositeParamsNode, BlendWhenGeometry, m_blendWhenGeometry)
SNAPI_GF_ACCESS(float, AtmosphereCompositeParamsNode, BlendWhenSky, m_blendWhenSky)

SNAPI_GF_NODE_LIFECYCLE(HeightFogParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(HeightFogParamsNode)
SNAPI_GF_ACCESS(std::int64_t, HeightFogParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, HeightFogParamsNode, Density, m_density)
SNAPI_GF_ACCESS(float, HeightFogParamsNode, HeightFalloff, m_heightFalloff)
SNAPI_GF_ACCESS(bool, HeightFogParamsNode, UseAbsoluteHeight, m_useAbsoluteHeight)
SNAPI_GF_ACCESS(double, HeightFogParamsNode, HeightOffsetAbsoluteY, m_heightOffsetAbsoluteY)
SNAPI_GF_ACCESS(bool, HeightFogParamsNode, UseActiveCameraYAsRebaseOrigin, m_useActiveCameraYAsRebaseOrigin)
SNAPI_GF_ACCESS(double, HeightFogParamsNode, RebaseOriginAbsoluteY, m_rebaseOriginAbsoluteY)
SNAPI_GF_ACCESS(float, HeightFogParamsNode, HeightOffsetRebased, m_heightOffsetRebased)
SNAPI_GF_ACCESS(float, HeightFogParamsNode, StartDistance, m_startDistance)
SNAPI_GF_ACCESS(Vec3, HeightFogParamsNode, FogColor, m_fogColor)
SNAPI_GF_ACCESS(Vec3, HeightFogParamsNode, HorizonColor, m_horizonColor)
SNAPI_GF_ACCESS(Vec3, HeightFogParamsNode, ZenithColor, m_zenithColor)
SNAPI_GF_ACCESS(float, HeightFogParamsNode, SkyBlendStartDistance, m_skyBlendStartDistance)
SNAPI_GF_ACCESS(float, HeightFogParamsNode, SkyBlendEndDistance, m_skyBlendEndDistance)
SNAPI_GF_ACCESS(float, HeightFogParamsNode, SkyBlendStrength, m_skyBlendStrength)
SNAPI_GF_ACCESS(float, HeightFogParamsNode, TauDitherAmplitude, m_tauDitherAmplitude)
SNAPI_GF_ACCESS(Vec3, HeightFogParamsNode, SunDirection, m_sunDirection)
SNAPI_GF_ACCESS(float, HeightFogParamsNode, SunAnisotropyG, m_sunAnisotropyG)
SNAPI_GF_ACCESS(Vec3, HeightFogParamsNode, SunColor, m_sunColor)
SNAPI_GF_ACCESS(float, HeightFogParamsNode, SunInscatterIntensity, m_sunInscatterIntensity)

SNAPI_GF_NODE_LIFECYCLE(ToneMapParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(ToneMapParamsNode)
SNAPI_GF_ACCESS(std::int64_t, ToneMapParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, Exposure, m_exposure)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, Gamma, m_gamma)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, DitherStrength, m_ditherStrength)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, AgXExposureBiasStops, m_agXExposureBiasStops)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, AgXSaturation, m_agXSaturation)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, AgXContrast, m_agXContrast)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, AgXPivot, m_agXPivot)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, AgXGamutThreshold, m_agXGamutThreshold)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, AgXGamutKnee, m_agXGamutKnee)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, AcesSaturation, m_acesSaturation)
SNAPI_GF_ACCESS(float, ToneMapParamsNode, AcesWhitePoint, m_acesWhitePoint)
SNAPI_GF_ACCESS(bool, ToneMapParamsNode, EnableACES, m_enableACES)
SNAPI_GF_ACCESS(bool, ToneMapParamsNode, EnableAgX, m_enableAgX)
SNAPI_GF_ACCESS(bool, ToneMapParamsNode, EnableCompare, m_enableCompare)

#undef SNAPI_GF_ACCESS
#undef SNAPI_GF_NODE_EDITOR_LIFECYCLE
#undef SNAPI_GF_NODE_LIFECYCLE

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER_NEW
