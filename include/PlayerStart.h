#pragma once

#include <string>

#include "AssetRef.h"
#include "BaseNode.h"
#include "Export.h"
#include "PawnBase.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Spawn marker node used by gameplay host pawn-spawn flow.
 */
class SNAPI_GAMEFRAMEWORK_API PlayerStart : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::PlayerStart";

    PlayerStart();
    explicit PlayerStart(std::string Name);

    /**
     * @brief Lifecycle hook used to ensure required PlayerStart components exist.
     */
    void OnCreateImpl(IWorld& WorldRef);

    TAssetRef<PawnBase>& EditSpawnPawnAsset();
    const TAssetRef<PawnBase>& GetSpawnPawnAsset() const;

private:
    void EnsureDefaultComponents();

    TAssetRef<PawnBase> m_spawnPawnAsset{};
};

} // namespace SnAPI::GameFramework
