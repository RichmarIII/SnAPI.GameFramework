#include "WorldRenderSettings.h"

#if defined(SNAPI_GF_ENABLE_RENDERER)

#include "IWorld.h"
#include "NodeCast.h"

#include <vector>

namespace SnAPI::GameFramework
{
namespace
{
template<typename TNodeType>
std::vector<NodeHandle> FindReferencedChildNodes(IWorld& WorldRef, NodeHandle& InOutParent)
{
    std::vector<NodeHandle> Matches{};
    BaseNode* ParentNode = WorldRef.BorrowedNode(InOutParent);
    if (!ParentNode)
    {
        return Matches;
    }

    Matches.reserve(ParentNode->Children().size());
    for (const NodeHandle& ChildRef : ParentNode->Children())
    {
        NodeHandle ChildHandle = ChildRef;
        BaseNode* ChildNode = WorldRef.BorrowedNode(ChildHandle);
        if (!ChildNode)
        {
            continue;
        }

        if (NodeCast<TNodeType>(ChildNode) == nullptr)
        {
            continue;
        }

        Matches.push_back(ChildHandle);
    }

    return Matches;
}

template<typename TNodeType>
void EnsureReferencedNode(IWorld& WorldRef,
                          NodeHandle& InOutParent,
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

        std::vector<NodeHandle> ExistingChildren = FindReferencedChildNodes<TNodeType>(WorldRef, InOutParent);
        for (NodeHandle& ExistingChild : ExistingChildren)
        {
            (void)WorldRef.DestroyNode(ExistingChild);
        }
        return;
    }

    if (!InOutSpawned.IsNull() && WorldRef.BorrowedNode(InOutSpawned) != nullptr)
    {
        if (auto* ExistingNode = WorldRef.BorrowedNode(InOutSpawned); NodeCast<TNodeType>(ExistingNode) != nullptr)
        {
            ExistingNode->EditorTransient(true);
            (void)WorldRef.RequestNodeOnCreate(InOutSpawned);
            return;
        }
    }

    std::vector<NodeHandle> ExistingChildren = FindReferencedChildNodes<TNodeType>(WorldRef, InOutParent);
    if (!ExistingChildren.empty())
    {
        InOutSpawned = ExistingChildren.front();
        if (BaseNode* ExistingNode = WorldRef.BorrowedNode(InOutSpawned))
        {
            ExistingNode->EditorTransient(true);
        }

        for (std::size_t Index = 1; Index < ExistingChildren.size(); ++Index)
        {
            NodeHandle Duplicate = ExistingChildren[Index];
            (void)WorldRef.DestroyNode(Duplicate);
        }

        (void)WorldRef.RequestNodeOnCreate(InOutSpawned);
        return;
    }

    InOutSpawned = {};
    if (auto SpawnResult = Asset.Instantiate(WorldRef, InOutParent, true); SpawnResult)
    {
        InOutSpawned = *SpawnResult;
        if (BaseNode* SpawnedNode = WorldRef.BorrowedNode(InOutSpawned))
        {
            SpawnedNode->EditorTransient(true);
        }
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

TAssetRef<SSGIParamsNode>& WorldRenderSettings::EditSSGIParams() { return m_ssgiParams; }
const TAssetRef<SSGIParamsNode>& WorldRenderSettings::GetSSGIParams() const { return m_ssgiParams; }

TAssetRef<SSRParamsNode>& WorldRenderSettings::EditSSRParams() { return m_ssrParams; }
const TAssetRef<SSRParamsNode>& WorldRenderSettings::GetSSRParams() const { return m_ssrParams; }

TAssetRef<TAAParamsNode>& WorldRenderSettings::EditTAAParams() { return m_taaParams; }
const TAssetRef<TAAParamsNode>& WorldRenderSettings::GetTAAParams() const { return m_taaParams; }

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

    NodeHandle ParentHandle = Handle();
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_ssaoParams, m_spawnedSSAO);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_ssgiParams, m_spawnedSSGI);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_ssrParams, m_spawnedSSR);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_taaParams, m_spawnedTAA);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_bloomParams, m_spawnedBloom);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_atmosphereParams, m_spawnedAtmosphere);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_atmosphereCompositeParams, m_spawnedAtmosphereComposite);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_heightFogParams, m_spawnedHeightFog);
    EnsureReferencedNode(*WorldPtr, ParentHandle, m_toneMapParams, m_spawnedToneMap);
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_RENDERER
