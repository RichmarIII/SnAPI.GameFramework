#pragma once

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include <string>
#include <string_view>

#include "AssetRef.h"
#include "AtmosphereCompositeParamsNode.h"
#include "AtmosphereParamsNode.h"
#include "BaseNode.h"
#include "BloomParamsNode.h"
#include "Export.h"
#include "HeightFogParamsNode.h"
#include "SSAOParamsNode.h"
#include "SSRParamsNode.h"
#include "ToneMapParamsNode.h"

namespace SnAPI::GameFramework
{

class SNAPI_GAMEFRAMEWORK_API WorldRenderSettings : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::WorldRenderSettings";

    WorldRenderSettings();
    explicit WorldRenderSettings(std::string Name);

    TAssetRef<SSAOParamsNode>& EditSSAOParams();
    const TAssetRef<SSAOParamsNode>& GetSSAOParams() const;

    TAssetRef<SSRParamsNode>& EditSSRParams();
    const TAssetRef<SSRParamsNode>& GetSSRParams() const;

    TAssetRef<BloomParamsNode>& EditBloomParams();
    const TAssetRef<BloomParamsNode>& GetBloomParams() const;

    TAssetRef<AtmosphereParamsNode>& EditAtmosphereParams();
    const TAssetRef<AtmosphereParamsNode>& GetAtmosphereParams() const;

    TAssetRef<AtmosphereCompositeParamsNode>& EditAtmosphereCompositeParams();
    const TAssetRef<AtmosphereCompositeParamsNode>& GetAtmosphereCompositeParams() const;

    TAssetRef<HeightFogParamsNode>& EditHeightFogParams();
    const TAssetRef<HeightFogParamsNode>& GetHeightFogParams() const;

    TAssetRef<ToneMapParamsNode>& EditToneMapParams();
    const TAssetRef<ToneMapParamsNode>& GetToneMapParams() const;

    void OnCreate();
    void Tick(float DeltaSeconds);
#if defined(WITH_EDITOR) && WITH_EDITOR
    void EditorTick(float DeltaSeconds);
    void EditorOnPropertyChanged(std::string_view Name);
#endif

private:
    void ApplyReferencedSettings();

    TAssetRef<SSAOParamsNode> m_ssaoParams{};
    TAssetRef<SSRParamsNode> m_ssrParams{};
    TAssetRef<BloomParamsNode> m_bloomParams{};
    TAssetRef<AtmosphereParamsNode> m_atmosphereParams{};
    TAssetRef<AtmosphereCompositeParamsNode> m_atmosphereCompositeParams{};
    TAssetRef<HeightFogParamsNode> m_heightFogParams{};
    TAssetRef<ToneMapParamsNode> m_toneMapParams{};

    NodeHandle m_spawnedSSAO{};
    NodeHandle m_spawnedSSR{};
    NodeHandle m_spawnedBloom{};
    NodeHandle m_spawnedAtmosphere{};
    NodeHandle m_spawnedAtmosphereComposite{};
    NodeHandle m_spawnedHeightFog{};
    NodeHandle m_spawnedToneMap{};
};

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
