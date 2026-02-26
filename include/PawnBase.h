#pragma once

#include <string>

#include "BaseNode.h"
#include "Export.h"

namespace SnAPI::GameFramework
{

/**
 * @brief Base gameplay pawn node that installs default movement components on spawn.
 */
class SNAPI_GAMEFRAMEWORK_API PawnBase : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::PawnBase";

    PawnBase();
    explicit PawnBase(std::string Name);

    /**
     * @brief Lifecycle hook used by gameplay spawn flow to ensure default pawn components exist.
     */
    void OnCreateImpl(IWorld& WorldRef);

    void OnPossess(const NodeHandle& PlayerHandle);
    void OnUnpossess(const NodeHandle& PlayerHandle);

private:
    void EnsureDefaultComponents();
};

} // namespace SnAPI::GameFramework
