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

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "IWorld.h"
#include "RendererSystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace SnAPI::GameFramework
{
namespace
{
[[nodiscard]] float FiniteOr(const float Value, const float Fallback)
{
    return std::isfinite(Value) ? Value : Fallback;
}

[[nodiscard]] double FiniteOr(const double Value, const double Fallback)
{
    return std::isfinite(Value) ? Value : Fallback;
}

[[nodiscard]] float ClampNonNegative(const float Value)
{
    return std::max(0.0f, FiniteOr(Value, 0.0f));
}

[[nodiscard]] float ClampPositive(const float Value, const float Minimum)
{
    return std::max(Minimum, FiniteOr(Value, Minimum));
}

[[nodiscard]] float ClampUnit(const float Value)
{
    return std::clamp(FiniteOr(Value, 0.0f), 0.0f, 1.0f);
}

[[nodiscard]] float ClampSignedUnitOpen(const float Value)
{
    return std::clamp(FiniteOr(Value, 0.0f), -0.99f, 0.99f);
}

[[nodiscard]] std::uint32_t ClampMinOne(const std::uint32_t Value)
{
    return std::max<std::uint32_t>(1u, Value);
}

[[nodiscard]] std::uint64_t ViewportSelectionKey(const std::int64_t ViewportID)
{
    return (ViewportID >= 0)
        ? static_cast<std::uint64_t>(ViewportID)
        : std::numeric_limits<std::uint64_t>::max();
}

[[nodiscard]] std::array<float, 3> ToFloat3(const Vec3& Value)
{
    return {
        FiniteOr(static_cast<float>(Value.x()), 0.0f),
        FiniteOr(static_cast<float>(Value.y()), 0.0f),
        FiniteOr(static_cast<float>(Value.z()), 0.0f)};
}

[[nodiscard]] std::array<float, 3> ToNonNegativeFloat3(const Vec3& Value)
{
    const auto Raw = ToFloat3(Value);
    return {
        std::max(0.0f, Raw[0]),
        std::max(0.0f, Raw[1]),
        std::max(0.0f, Raw[2])};
}

[[nodiscard]] std::array<float, 3> NormalizeOrFallback(
    const Vec3& Value,
    const std::array<float, 3>& Fallback)
{
    const auto Raw = ToFloat3(Value);
    const float LengthSq = Raw[0] * Raw[0] + Raw[1] * Raw[1] + Raw[2] * Raw[2];
    if (!std::isfinite(LengthSq) || LengthSq <= 1.0e-12f)
    {
        return Fallback;
    }

    const float InvLength = 1.0f / std::sqrt(LengthSq);
    return {Raw[0] * InvLength, Raw[1] * InvLength, Raw[2] * InvLength};
}

bool ResetRendererFeatureSettings(RendererSystem& Renderer, const std::int64_t ViewportID, const DeferredShadingParamsNode*)
{
    return Renderer.ApplyDeferredShadingFeatureSettings(ViewportID, RendererDeferredShadingFeatureSettings{});
}

bool ResetRendererFeatureSettings(RendererSystem& Renderer, const std::int64_t ViewportID, const SSAOParamsNode*)
{
    return Renderer.ApplySsaoFeatureSettings(ViewportID, RendererSsaoFeatureSettings{});
}

bool ResetRendererFeatureSettings(RendererSystem& Renderer, const std::int64_t ViewportID, const SSGIParamsNode*)
{
    return Renderer.ApplySsgiFeatureSettings(ViewportID, RendererSsgiFeatureSettings{});
}

bool ResetRendererFeatureSettings(RendererSystem& Renderer, const std::int64_t ViewportID, const SSRParamsNode*)
{
    return Renderer.ApplySsrFeatureSettings(ViewportID, RendererSsrFeatureSettings{});
}

bool ResetRendererFeatureSettings(RendererSystem& Renderer, const std::int64_t ViewportID, const TAAParamsNode*)
{
    return Renderer.ApplyTaaFeatureSettings(ViewportID, RendererTaaFeatureSettings{});
}

bool ResetRendererFeatureSettings(RendererSystem& Renderer, const std::int64_t ViewportID, const BloomParamsNode*)
{
    return Renderer.ApplyBloomFeatureSettings(ViewportID, RendererBloomFeatureSettings{});
}

bool ResetRendererFeatureSettings(RendererSystem& Renderer, const std::int64_t ViewportID, const AtmosphereParamsNode*)
{
    return Renderer.ApplyAtmosphereFeatureSettings(ViewportID, RendererAtmosphereFeatureSettings{});
}

bool ResetRendererFeatureSettings(RendererSystem& Renderer, const std::int64_t ViewportID, const AtmosphereCompositeParamsNode*)
{
    return Renderer.ApplyAtmosphereCompositeFeatureSettings(ViewportID, RendererAtmosphereCompositeFeatureSettings{});
}

bool ResetRendererFeatureSettings(RendererSystem& Renderer, const std::int64_t ViewportID, const HeightFogParamsNode*)
{
    return Renderer.ApplyHeightFogFeatureSettings(ViewportID, RendererHeightFogFeatureSettings{});
}

bool ResetRendererFeatureSettings(RendererSystem& Renderer, const std::int64_t ViewportID, const ToneMapParamsNode*)
{
    return Renderer.ApplyToneMapFeatureSettings(ViewportID, RendererToneMapFeatureSettings{});
}
} // namespace

#define SNAPI_GF_NODE_LIFECYCLE(ClassName) \
    ClassName::ClassName() { TypeKey(StaticTypeId<ClassName>()); } \
    ClassName::ClassName(std::string Name) : BaseNode(std::move(Name)) { TypeKey(StaticTypeId<ClassName>()); } \
    void ClassName::OnCreate() { m_applyPending = true; ApplyIfNeeded(); } \
    void ClassName::OnDestroy() \
    { \
        if (auto* WorldPtr = World()) \
        { \
            auto& Renderer = WorldPtr->Renderer(); \
            if (Renderer.IsInitialized()) \
            { \
                (void)ResetRendererFeatureSettings(Renderer, m_viewportID, this); \
            } \
        } \
        InvalidateApplyState(); \
    } \
    void ClassName::Tick(const float DeltaSeconds) { (void)DeltaSeconds; ApplyIfNeeded(); } \
    void ClassName::ApplyIfNeeded() \
    { \
        auto* WorldPtr = World(); \
        if (!WorldPtr) \
        { \
            InvalidateApplyState(); \
            return; \
        } \
        auto& Renderer = WorldPtr->Renderer(); \
        if (!Renderer.IsInitialized()) \
        { \
            InvalidateApplyState(); \
            return; \
        } \
        const std::uint64_t TargetViewportID = ViewportSelectionKey(m_viewportID); \
        const std::uint64_t FeatureRevision = Renderer.RenderViewportFeatureRevision(); \
        if (!m_applyPending && m_lastAppliedViewportID == TargetViewportID && m_lastAppliedFeatureRevision == FeatureRevision) \
        { \
            return; \
        } \
        (void)ApplyFeatureSettings(); \
    } \
    void ClassName::InvalidateApplyState() \
    { \
        m_applyPending = true; \
        m_lastAppliedFeatureRevision = 0; \
        m_lastAppliedViewportID = 0; \
    }

#if defined(WITH_EDITOR) && WITH_EDITOR
#define SNAPI_GF_NODE_EDITOR_LIFECYCLE(ClassName) \
    void ClassName::EditorTick(const float DeltaSeconds) { Tick(DeltaSeconds); } \
    void ClassName::EditorOnPropertyChanged(const std::string_view Name) \
    { \
        (void)Name; \
        m_applyPending = true; \
        ApplyIfNeeded(); \
    }
#else
#define SNAPI_GF_NODE_EDITOR_LIFECYCLE(ClassName)
#endif

#define SNAPI_GF_ACCESS(Type, ClassName, Name, Member) \
    Type& ClassName::Edit##Name() { m_applyPending = true; return Member; } \
    const Type& ClassName::Get##Name() const { return Member; }

#define SNAPI_GF_MARK_APPLIED(Renderer) \
    do \
    { \
        m_applyPending = false; \
        m_lastAppliedFeatureRevision = (Renderer).RenderViewportFeatureRevision(); \
        m_lastAppliedViewportID = ViewportSelectionKey(m_viewportID); \
        return true; \
    } while (false)

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

bool DeferredShadingParamsNode::ApplyFeatureSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidateApplyState();
        return false;
    }

    auto& Renderer = WorldPtr->Renderer();
    RendererDeferredShadingFeatureSettings Settings{};
    Settings.DebugMotionVectors = m_debugMotionVectors;
    Settings.DebugNormals = m_debugNormals;
    Settings.DebugAlbedo = m_debugAlbedo;
    Settings.DebugAO = m_debugAO;
    Settings.DebugRoughness = m_debugRoughness;
    Settings.DebugMetallic = m_debugMetallic;
    Settings.DebugDepth = m_debugDepth;
    Settings.DebugTextureCoords = m_debugTextureCoords;
    Settings.DebugDirectLighting = m_debugDirectLighting;
    Settings.DebugGI = m_debugGI;
    Settings.DebugSpecular = m_debugSpecular;
    Settings.DebugLighting = m_debugLighting;
    if (!Renderer.ApplyDeferredShadingFeatureSettings(m_viewportID, Settings))
    {
        InvalidateApplyState();
        return false;
    }
    SNAPI_GF_MARK_APPLIED(Renderer);
}

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

bool SSAOParamsNode::ApplyFeatureSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidateApplyState();
        return false;
    }

    const float FalloffStart = ClampNonNegative(m_falloffStart);
    RendererSsaoFeatureSettings Settings{};
    Settings.Radius = ClampNonNegative(m_radius);
    Settings.Bias = ClampNonNegative(m_bias);
    Settings.Intensity = ClampNonNegative(m_intensity);
    Settings.MaxDistance = ClampNonNegative(m_maxDistance);
    Settings.SliceCount = ClampMinOne(m_sliceCount);
    Settings.StepsPerSlice = ClampMinOne(m_stepsPerSlice);
    Settings.FalloffStart = FalloffStart;
    Settings.FalloffEnd = std::max(FalloffStart + 1.0e-3f, FiniteOr(m_falloffEnd, FalloffStart + 1.0e-3f));
    Settings.MaxPixelRadius = ClampNonNegative(m_maxPixelRadius);
    Settings.Thickness = ClampNonNegative(m_thickness);
    Settings.DenoiseBlurBeta = ClampNonNegative(m_denoiseBlurBeta);
    Settings.TemporalBlendFactor = ClampNonNegative(m_temporalBlendFactor);
    Settings.DisocclusionThreshold = ClampNonNegative(m_disocclusionThreshold);
    Settings.VelocityWeight = ClampNonNegative(m_velocityWeight);

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.ApplySsaoFeatureSettings(m_viewportID, Settings))
    {
        InvalidateApplyState();
        return false;
    }
    SNAPI_GF_MARK_APPLIED(Renderer);
}

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

bool SSGIParamsNode::ApplyFeatureSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidateApplyState();
        return false;
    }

    RendererSsgiFeatureSettings Settings{};
    Settings.Intensity = ClampNonNegative(m_intensity);
    Settings.MaxDistance = ClampNonNegative(m_maxDistance);
    Settings.Thickness = ClampNonNegative(m_thickness);
    Settings.SurfaceBias = ClampNonNegative(m_surfaceBias);
    Settings.MaxSteps = ClampMinOne(m_maxSteps);
    Settings.RayCount = ClampMinOne(m_rayCount);
    Settings.DepthSigma = ClampNonNegative(m_depthSigma);
    Settings.NormalSigma = ClampNonNegative(m_normalSigma);
    Settings.RadianceClamp = ClampNonNegative(m_radianceClamp);
    Settings.MaxPixelRadius = std::max(1.0f, FiniteOr(m_maxPixelRadius, 1.0f));
    Settings.StepExponent = std::max(0.25f, FiniteOr(m_stepExponent, 1.25f));
    Settings.TemporalBlendFactor = ClampUnit(m_temporalBlendFactor);
    Settings.DisocclusionThreshold = ClampNonNegative(m_disocclusionThreshold);
    Settings.ClampStrength = ClampNonNegative(m_clampStrength);
    Settings.VelocityWeight = ClampNonNegative(m_velocityWeight);
    Settings.LowLumaBoost = ClampNonNegative(m_lowLumaBoost);
    Settings.TemporalDebugMode = m_temporalDebugMode;

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.ApplySsgiFeatureSettings(m_viewportID, Settings))
    {
        InvalidateApplyState();
        return false;
    }
    SNAPI_GF_MARK_APPLIED(Renderer);
}

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

bool SSRParamsNode::ApplyFeatureSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidateApplyState();
        return false;
    }

    RendererSsrFeatureSettings Settings{};
    Settings.MaxDistance = ClampNonNegative(m_maxDistance);
    Settings.Thickness = ClampNonNegative(m_thickness);
    Settings.MaxRoughness = ClampUnit(m_maxRoughness);
    Settings.RoughnessThreshold = ClampNonNegative(m_roughnessThreshold);
    Settings.MaxSteps = ClampMinOne(m_maxSteps);
    Settings.MaxBinarySteps = ClampMinOne(m_maxBinarySteps);
    Settings.ScreenEdgeFade = ClampNonNegative(m_screenEdgeFade);
    Settings.ReflectionFade = ClampNonNegative(m_reflectionFade);
    Settings.TemporalBlendFactor = ClampUnit(m_temporalBlendFactor);
    Settings.ClampStrength = ClampNonNegative(m_clampStrength);
    Settings.MotionHistoryReset = ClampUnit(m_motionHistoryReset);
    Settings.TemporalDebugMode = m_temporalDebugMode;

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.ApplySsrFeatureSettings(m_viewportID, Settings))
    {
        InvalidateApplyState();
        return false;
    }
    SNAPI_GF_MARK_APPLIED(Renderer);
}

SNAPI_GF_NODE_LIFECYCLE(TAAParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(TAAParamsNode)
SNAPI_GF_ACCESS(std::int64_t, TAAParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, TAAParamsNode, BlendFactor, m_blendFactor)
SNAPI_GF_ACCESS(float, TAAParamsNode, MotionBlendFactor, m_motionBlendFactor)
SNAPI_GF_ACCESS(float, TAAParamsNode, ClampStrength, m_clampStrength)
SNAPI_GF_ACCESS(float, TAAParamsNode, Sharpen, m_sharpen)
SNAPI_GF_ACCESS(float, TAAParamsNode, JitterScale, m_jitterScale)

bool TAAParamsNode::ApplyFeatureSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidateApplyState();
        return false;
    }

    RendererTaaFeatureSettings Settings{};
    Settings.BlendFactor = ClampUnit(m_blendFactor);
    Settings.MotionBlendFactor = ClampUnit(m_motionBlendFactor);
    Settings.ClampStrength = ClampNonNegative(m_clampStrength);
    Settings.Sharpen = ClampNonNegative(m_sharpen);
    Settings.JitterScale = ClampNonNegative(m_jitterScale);

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.ApplyTaaFeatureSettings(m_viewportID, Settings))
    {
        InvalidateApplyState();
        return false;
    }
    SNAPI_GF_MARK_APPLIED(Renderer);
}

SNAPI_GF_NODE_LIFECYCLE(BloomParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(BloomParamsNode)
SNAPI_GF_ACCESS(std::int64_t, BloomParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, BloomParamsNode, Threshold, m_threshold)
SNAPI_GF_ACCESS(float, BloomParamsNode, Knee, m_knee)
SNAPI_GF_ACCESS(float, BloomParamsNode, Intensity, m_intensity)
SNAPI_GF_ACCESS(float, BloomParamsNode, Scatter, m_scatter)
SNAPI_GF_ACCESS(float, BloomParamsNode, Clamp, m_clamp)
SNAPI_GF_ACCESS(std::uint32_t, BloomParamsNode, MipCount, m_mipCount)

bool BloomParamsNode::ApplyFeatureSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidateApplyState();
        return false;
    }

    RendererBloomFeatureSettings Settings{};
    Settings.Threshold = ClampNonNegative(m_threshold);
    Settings.Knee = ClampNonNegative(m_knee);
    Settings.Intensity = ClampNonNegative(m_intensity);
    Settings.Scatter = ClampNonNegative(m_scatter);
    Settings.Clamp = ClampNonNegative(m_clamp);
    Settings.MipCount = ClampMinOne(m_mipCount);

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.ApplyBloomFeatureSettings(m_viewportID, Settings))
    {
        InvalidateApplyState();
        return false;
    }
    SNAPI_GF_MARK_APPLIED(Renderer);
}

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

bool AtmosphereParamsNode::ApplyFeatureSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidateApplyState();
        return false;
    }

    RendererAtmosphereFeatureSettings Settings{};
    Settings.WorldMode = m_worldMode;
    Settings.SunDirection = NormalizeOrFallback(m_sunDirection, Settings.SunDirection);
    Settings.SunColor = ToNonNegativeFloat3(m_sunColor);
    Settings.Exposure = ClampNonNegative(m_exposure);
    Settings.SunIntensity = ClampNonNegative(m_sunIntensity);
    Settings.RayleighScattering = ToNonNegativeFloat3(m_rayleighScattering);
    Settings.RayleighScaleHeight = ClampPositive(m_rayleighScaleHeight, 1.0f);
    Settings.MieScattering = ToNonNegativeFloat3(m_mieScattering);
    Settings.MieScaleHeight = ClampPositive(m_mieScaleHeight, 1.0f);
    Settings.MieAbsorption = ToNonNegativeFloat3(m_mieAbsorption);
    Settings.MieAnisotropyG = ClampSignedUnitOpen(m_mieAnisotropyG);
    Settings.PlanetRadiusMeters = ClampPositive(m_planetRadiusMeters, 1.0f);
    Settings.AtmosphereRadiusMeters = std::max(Settings.PlanetRadiusMeters + 1.0f, ClampPositive(m_atmosphereRadiusMeters, Settings.PlanetRadiusMeters + 1.0f));
    Settings.CameraGroundOffsetMeters = FiniteOr(m_cameraGroundOffsetMeters, 0.0f);
    Settings.MaxSunDistanceMeters = ClampPositive(m_maxSunDistanceMeters, 1.0f);
    Settings.ViewSampleCount = ClampMinOne(m_viewSampleCount);
    Settings.SunSampleCount = ClampMinOne(m_sunSampleCount);
    Settings.MultiScatterStrength = ClampNonNegative(m_multiScatterStrength);

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.ApplyAtmosphereFeatureSettings(m_viewportID, Settings))
    {
        InvalidateApplyState();
        return false;
    }
    SNAPI_GF_MARK_APPLIED(Renderer);
}

SNAPI_GF_NODE_LIFECYCLE(AtmosphereCompositeParamsNode)
SNAPI_GF_NODE_EDITOR_LIFECYCLE(AtmosphereCompositeParamsNode)
SNAPI_GF_ACCESS(std::int64_t, AtmosphereCompositeParamsNode, ViewportID, m_viewportID)
SNAPI_GF_ACCESS(float, AtmosphereCompositeParamsNode, DepthThreshold, m_depthThreshold)
SNAPI_GF_ACCESS(float, AtmosphereCompositeParamsNode, BlendWhenGeometry, m_blendWhenGeometry)
SNAPI_GF_ACCESS(float, AtmosphereCompositeParamsNode, BlendWhenSky, m_blendWhenSky)

bool AtmosphereCompositeParamsNode::ApplyFeatureSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidateApplyState();
        return false;
    }

    RendererAtmosphereCompositeFeatureSettings Settings{};
    Settings.DepthThreshold = ClampUnit(m_depthThreshold);
    Settings.BlendWhenGeometry = ClampUnit(m_blendWhenGeometry);
    Settings.BlendWhenSky = ClampUnit(m_blendWhenSky);

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.ApplyAtmosphereCompositeFeatureSettings(m_viewportID, Settings))
    {
        InvalidateApplyState();
        return false;
    }
    SNAPI_GF_MARK_APPLIED(Renderer);
}

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

bool HeightFogParamsNode::ApplyFeatureSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidateApplyState();
        return false;
    }

    const float SkyBlendStart = ClampNonNegative(m_skyBlendStartDistance);
    RendererHeightFogFeatureSettings Settings{};
    Settings.Density = ClampNonNegative(m_density);
    Settings.HeightFalloff = ClampNonNegative(m_heightFalloff);
    Settings.UseAbsoluteHeight = m_useAbsoluteHeight;
    Settings.HeightOffsetAbsoluteY = FiniteOr(m_heightOffsetAbsoluteY, 0.0);
    Settings.UseActiveCameraYAsRebaseOrigin = m_useActiveCameraYAsRebaseOrigin;
    Settings.RebaseOriginAbsoluteY = FiniteOr(m_rebaseOriginAbsoluteY, 0.0);
    Settings.HeightOffsetRebased = FiniteOr(m_heightOffsetRebased, 0.0f);
    Settings.StartDistance = ClampNonNegative(m_startDistance);
    Settings.FogColor = ToNonNegativeFloat3(m_fogColor);
    Settings.HorizonColor = ToNonNegativeFloat3(m_horizonColor);
    Settings.ZenithColor = ToNonNegativeFloat3(m_zenithColor);
    Settings.SkyBlendStartDistance = SkyBlendStart;
    Settings.SkyBlendEndDistance = std::max(SkyBlendStart + 1.0e-3f, FiniteOr(m_skyBlendEndDistance, SkyBlendStart + 1.0e-3f));
    Settings.SkyBlendStrength = ClampNonNegative(m_skyBlendStrength);
    Settings.TauDitherAmplitude = ClampNonNegative(m_tauDitherAmplitude);
    Settings.SunDirection = NormalizeOrFallback(m_sunDirection, Settings.SunDirection);
    Settings.SunAnisotropyG = ClampSignedUnitOpen(m_sunAnisotropyG);
    Settings.SunColor = ToNonNegativeFloat3(m_sunColor);
    Settings.SunInscatterIntensity = ClampNonNegative(m_sunInscatterIntensity);

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.ApplyHeightFogFeatureSettings(m_viewportID, Settings))
    {
        InvalidateApplyState();
        return false;
    }
    SNAPI_GF_MARK_APPLIED(Renderer);
}

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

bool ToneMapParamsNode::ApplyFeatureSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        InvalidateApplyState();
        return false;
    }

    RendererToneMapFeatureSettings Settings{};
    Settings.Exposure = ClampNonNegative(m_exposure);
    Settings.Gamma = ClampPositive(m_gamma, 1.0e-3f);
    Settings.DitherStrength = ClampNonNegative(m_ditherStrength);
    Settings.AgXExposureBiasStops = FiniteOr(m_agXExposureBiasStops, 0.0f);
    Settings.AgXSaturation = ClampNonNegative(m_agXSaturation);
    Settings.AgXContrast = ClampNonNegative(m_agXContrast);
    Settings.AgXPivot = ClampUnit(m_agXPivot);
    Settings.AgXGamutThreshold = std::clamp(FiniteOr(m_agXGamutThreshold, 0.0f), 0.0f, 2.0f);
    Settings.AgXGamutKnee = ClampNonNegative(m_agXGamutKnee);
    Settings.AcesSaturation = ClampNonNegative(m_acesSaturation);
    Settings.AcesWhitePoint = ClampNonNegative(m_acesWhitePoint);
    Settings.EnableACES = m_enableACES;
    Settings.EnableAgX = m_enableAgX;
    Settings.EnableCompare = m_enableCompare;

    auto& Renderer = WorldPtr->Renderer();
    if (!Renderer.ApplyToneMapFeatureSettings(m_viewportID, Settings))
    {
        InvalidateApplyState();
        return false;
    }
    SNAPI_GF_MARK_APPLIED(Renderer);
}

#undef SNAPI_GF_MARK_APPLIED
#undef SNAPI_GF_ACCESS
#undef SNAPI_GF_NODE_EDITOR_LIFECYCLE
#undef SNAPI_GF_NODE_LIFECYCLE

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
