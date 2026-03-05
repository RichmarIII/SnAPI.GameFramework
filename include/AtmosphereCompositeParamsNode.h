#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <cstdint>
#include <string>
#include <string_view>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::Graphics
{
class AtmosphereCompositePass;
}

namespace SnAPI::GameFramework
{

class SNAPI_GAMEFRAMEWORK_API AtmosphereCompositeParamsNode : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::AtmosphereCompositeParamsNode";

    AtmosphereCompositeParamsNode();
    explicit AtmosphereCompositeParamsNode(std::string Name);

    std::int64_t& EditViewportID();
    const std::int64_t& GetViewportID() const;

    float& EditDepthThreshold();
    const float& GetDepthThreshold() const;

    float& EditBlendWhenGeometry();
    const float& GetBlendWhenGeometry() const;

    float& EditBlendWhenSky();
    const float& GetBlendWhenSky() const;

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

    float m_depthThreshold = 0.0f;
    float m_blendWhenGeometry = 0.0f;
    float m_blendWhenSky = 1.0f;

    bool m_applyPending = true;
    std::uint64_t m_lastAppliedPassGraphRevision = 0;
    std::uint64_t m_lastAppliedViewportID = 0;
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
