#include "PlayerStart.h"

#include "BaseNode.inl"
#include "PawnBase.h"
#include "TransformComponent.h"

namespace SnAPI::GameFramework
{
PlayerStart::PlayerStart()
{
    TypeKey(StaticTypeId<PlayerStart>());
}

PlayerStart::PlayerStart(std::string Name)
    : BaseNode(std::move(Name))
{
    TypeKey(StaticTypeId<PlayerStart>());
}

void PlayerStart::OnCreateImpl(IWorld& WorldRef)
{
    EnsureDefaultComponents();
    (void)WorldRef;
}

TAssetRef<PawnBase>& PlayerStart::EditSpawnPawnAsset()
{
    return m_spawnPawnAsset;
}

const TAssetRef<PawnBase>& PlayerStart::GetSpawnPawnAsset() const
{
    return m_spawnPawnAsset;
}

void PlayerStart::EnsureDefaultComponents()
{
    if (!Has<TransformComponent>())
    {
        (void)Add<TransformComponent>();
    }
}

} // namespace SnAPI::GameFramework
