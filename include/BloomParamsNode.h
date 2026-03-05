#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class BloomPass;
}

namespace SnAPI::GameFramework
{

class SNAPI_GAMEFRAMEWORK_API BloomParamsNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::BloomParamsNode";

    BloomParamsNode();
    explicit BloomParamsNode(std::string Name);

    std::int64_t& EditViewportID();
    const std::int64_t& GetViewportID() const;

    float& EditThreshold();
    const float& GetThreshold() const;

    float& EditKnee();
    const float& GetKnee() const;

    float& EditIntensity();
    const float& GetIntensity() const;

    float& EditScatter();
    const float& GetScatter() const;

    float& EditClamp();
    const float& GetClamp() const;

    std::uint32_t& EditMipCount();
    const std::uint32_t& GetMipCount() const;

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

    float m_threshold = 1.1f;
    float m_knee = 0.5f;
    float m_intensity = 0.8f;
    float m_scatter = 0.6f;
    float m_clamp = 10.0f;
    std::uint32_t m_mipCount = 5;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
