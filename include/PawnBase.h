#pragma once

#include <string>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::GameFramework
{

/**
 * @ingroup SnAPI_GameFramework
 * @brief Default pawn node used by gameplay-host spawning and auto-possession flow.
 *
 * `PawnBase` is the stock spawnable pawn type used by `GameplayHost` when no more specific
 * pawn type is selected by the game, game mode, or `PlayerStart` asset reference. Its
 * `OnCreate()` path is intentionally idempotent and ensures a baseline set of transform,
 * movement, input, and rendering-related components exist when the relevant subsystems are enabled.
 *
 * Semantics:
 * - the node is replicated by default
 * - `OnCreate()` ensures default components rather than assuming a pre-authored prefab
 * - possession toggles the pawn camera active state when a camera component is present
 *
 * @see GameplayHost
 * @see PlayerStart
 */
class SNAPI_GAMEFRAMEWORK_API PawnBase : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::PawnBase";

    PawnBase();
    explicit PawnBase(std::string Name);

    /**
     * @brief Lifecycle hook used by gameplay spawn flow to ensure default pawn components exist.
     * @remarks Safe to call repeatedly; existing components are preserved.
     */
    void OnCreate();

    /** @brief Notification that a player began possessing this pawn. @param PlayerHandle Possessing player handle. */
    void OnPossess(const NodeHandle& PlayerHandle);
    /** @brief Notification that a player stopped possessing this pawn. @param PlayerHandle Player handle that released possession. */
    void OnUnpossess(const NodeHandle& PlayerHandle);

private:
    void EnsureDefaultComponents();
};

} // namespace SnAPI::GameFramework
