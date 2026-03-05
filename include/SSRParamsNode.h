#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class SSRPass;
}

namespace SnAPI::GameFramework
{

class SNAPI_GAMEFRAMEWORK_API SSRParamsNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::SSRParamsNode";

    SSRParamsNode();
    explicit SSRParamsNode(std::string Name);

    std::int64_t& EditViewportID();
    const std::int64_t& GetViewportID() const;

    float& EditMaxDistance();
    const float& GetMaxDistance() const;

    float& EditThickness();
    const float& GetThickness() const;

    float& EditMaxRoughness();
    const float& GetMaxRoughness() const;

    float& EditRoughnessThreshold();
    const float& GetRoughnessThreshold() const;

    std::uint32_t& EditMaxSteps();
    const std::uint32_t& GetMaxSteps() const;

    std::uint32_t& EditMaxBinarySteps();
    const std::uint32_t& GetMaxBinarySteps() const;

    float& EditScreenEdgeFade();
    const float& GetScreenEdgeFade() const;

    float& EditReflectionFade();
    const float& GetReflectionFade() const;

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

    float m_maxDistance = 0.25f;
    float m_thickness = 0.015f;
    float m_maxRoughness = 0.8f;
    float m_roughnessThreshold = 0.2f;
    std::uint32_t m_maxSteps = 32;
    std::uint32_t m_maxBinarySteps = 8;
    float m_screenEdgeFade = 0.1f;
    float m_reflectionFade = 0.8f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
