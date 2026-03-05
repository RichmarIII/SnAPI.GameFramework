#include "WorldRenderSettings.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "IWorld.h"
#include "NodeCast.h"

namespace SnAPI::GameFramework
{
namespace
{
template<typename TNodeType>
void EnsureReferencedNode(IWorld& WorldRef,
                          const NodeHandle Parent,
                          const TAssetRef<TNodeType>& Asset,
                          NodeHandle& InOutSpawned)
{
    if (Asset.IsNull())
    {
        if (!InOutSpawned.IsNull())
        {
            (void)WorldRef.DestroyNode(InOutSpawned);
            InOutSpawned = {};
        }
        return;
    }

    if (!InOutSpawned.IsNull() && InOutSpawned.Borrowed() != nullptr)
    {
        if (auto* ExistingNode = InOutSpawned.Borrowed())
        {
            if (auto* TypedNode = NodeCast<TNodeType>(ExistingNode))
            {
                TypedNode->OnCreate();
            }
        }
        return;
    }

    InOutSpawned = {};
    if (auto SpawnResult = Asset.Instantiate(WorldRef, Parent, true); SpawnResult)
    {
        InOutSpawned = *SpawnResult;
    }
}
} // namespace

WorldRenderSettings::WorldRenderSettings()
{
    TypeKey(StaticTypeId<WorldRenderSettings>());
}

WorldRenderSettings::WorldRenderSettings(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<WorldRenderSettings>());
}

TAssetRef<SSAOParamsNode>& WorldRenderSettings::EditSSAOParams() { return m_ssaoParams; }
const TAssetRef<SSAOParamsNode>& WorldRenderSettings::GetSSAOParams() const { return m_ssaoParams; }

TAssetRef<SSRParamsNode>& WorldRenderSettings::EditSSRParams() { return m_ssrParams; }
const TAssetRef<SSRParamsNode>& WorldRenderSettings::GetSSRParams() const { return m_ssrParams; }

TAssetRef<BloomParamsNode>& WorldRenderSettings::EditBloomParams() { return m_bloomParams; }
const TAssetRef<BloomParamsNode>& WorldRenderSettings::GetBloomParams() const { return m_bloomParams; }

TAssetRef<AtmosphereParamsNode>& WorldRenderSettings::EditAtmosphereParams() { return m_atmosphereParams; }
const TAssetRef<AtmosphereParamsNode>& WorldRenderSettings::GetAtmosphereParams() const { return m_atmosphereParams; }

TAssetRef<AtmosphereCompositeParamsNode>& WorldRenderSettings::EditAtmosphereCompositeParams()
{
    return m_atmosphereCompositeParams;
}
const TAssetRef<AtmosphereCompositeParamsNode>& WorldRenderSettings::GetAtmosphereCompositeParams() const
{
    return m_atmosphereCompositeParams;
}

TAssetRef<HeightFogParamsNode>& WorldRenderSettings::EditHeightFogParams() { return m_heightFogParams; }
const TAssetRef<HeightFogParamsNode>& WorldRenderSettings::GetHeightFogParams() const { return m_heightFogParams; }

TAssetRef<ToneMapParamsNode>& WorldRenderSettings::EditToneMapParams() { return m_toneMapParams; }
const TAssetRef<ToneMapParamsNode>& WorldRenderSettings::GetToneMapParams() const { return m_toneMapParams; }

void WorldRenderSettings::OnCreate()
{
    ApplyReferencedSettings();
}

void WorldRenderSettings::Tick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
}

#if defined(WITH_EDITOR) && WITH_EDITOR
void WorldRenderSettings::EditorTick(const float DeltaSeconds)
{
    (void)DeltaSeconds;
}

void WorldRenderSettings::EditorOnPropertyChanged(const std::string_view Name)
{
    (void)Name;
    ApplyReferencedSettings();
}
#endif

void WorldRenderSettings::ApplyReferencedSettings()
{
    auto* WorldPtr = World();
    if (!WorldPtr)
    {
        return;
    }

    const NodeHandle ParentHandle = Handle();
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_ssaoParams, m_spawnedSSAO);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_ssrParams, m_spawnedSSR);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_bloomParams, m_spawnedBloom);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_atmosphereParams, m_spawnedAtmosphere);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_atmosphereCompositeParams, m_spawnedAtmosphereComposite);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_heightFogParams, m_spawnedHeightFog);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_toneMapParams, m_spawnedToneMap);
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
