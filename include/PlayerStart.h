#pragma once

#include <string>

#include "AssetRef.h"
#include "BaseNode.h"
#include "Export.h"
#include "PawnBase.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Spawn marker node used by gameplay-host pawn spawning.
 *
 * `PlayerStart` marks a candidate spawn location for newly joined players. It may also
 * carry an optional pawn asset reference that overrides the default spawn path for players
 * using this start. The gameplay host validates that a selected player start belongs to the
 * same world, is active, and is not pending destruction before using it.
 *
 * @see GameplayHost
 * @see PawnBase
 */
class SNAPI_GAMEFRAMEWORK_API PlayerStart : public BaseNode, public NodeCRTP<PlayerStart>
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::PlayerStart";

    PlayerStart();
    explicit PlayerStart(std::string Name);

    /**
     * @brief Lifecycle hook used to ensure required PlayerStart components exist.
     * @remarks Safe to call repeatedly; existing components are preserved.
     */
    void OnCreate();

    /** @brief Access the optional pawn asset instantiated when players spawn from this start. @return Mutable asset reference. */
    TAssetRef<PawnBase>& EditSpawnPawnAsset();
    /** @brief Access the optional pawn asset instantiated when players spawn from this start. @return Const asset reference. */
    const TAssetRef<PawnBase>& GetSpawnPawnAsset() const;

private:
    void EnsureDefaultComponents();

    TAssetRef<PawnBase> m_spawnPawnAsset{};
};

} // namespace SnAPI::GameFramework
