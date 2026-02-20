#pragma once

#include <cstdint>
#include <string>

#include "BaseNode.h"
#include "Export.h"

#if defined(SNAPI_GF_ENABLE_INPUT)
#include <Input.h>
#endif

namespace SnAPI::GameFramework
{

/**
 * @brief Replicable player-ownership node used for local/splitscreen gameplay flow.
 * @remarks
 * `LocalPlayer` is world-level state that maps player identity, optional input
 * assignment, and currently possessed node.
 *
 * Networking:
 * - Possession changes are server-authoritative.
 * - Clients request possession via reflected RPC server endpoints.
 * - Possession state is replicated through reflected fields.
 */
class SNAPI_GAMEFRAMEWORK_API LocalPlayer : public BaseNode
{
public:
    static constexpr const char* kTypeName = "SnAPI::GameFramework::LocalPlayer";

    LocalPlayer();
    explicit LocalPlayer(std::string Name);

    unsigned int& EditPlayerIndex();
    const unsigned int& GetPlayerIndex() const;

    NodeHandle& EditPossessedNode();
    const NodeHandle& GetPossessedNode() const;

    bool& EditAcceptInput();
    const bool& GetAcceptInput() const;

    std::uint64_t& EditOwnerConnectionId();
    const std::uint64_t& GetOwnerConnectionId() const;

#if defined(SNAPI_GF_ENABLE_INPUT)
    SnAPI::Input::DeviceId& EditAssignedInputDevice();
    const SnAPI::Input::DeviceId& GetAssignedInputDevice() const;

    bool& EditUseAssignedInputDevice();
    const bool& GetUseAssignedInputDevice() const;
#endif

    /**
     * @brief Request possession of target node.
     * @remarks Clients forward to `ServerRequestPossess`; server executes directly.
     */
    void RequestPossess(NodeHandle Target);

    /**
     * @brief Request possession clear.
     * @remarks Clients forward to `ServerRequestUnpossess`; server executes directly.
     */
    void RequestUnpossess();

    /**
     * @brief Server-authoritative possession RPC endpoint.
     */
    void ServerRequestPossess(NodeHandle Target);

    /**
     * @brief Server-authoritative unpossession RPC endpoint.
     */
    void ServerRequestUnpossess();

private:
    bool CanPossessTarget(NodeHandle Target) const;

    unsigned int m_playerIndex = 0;
    NodeHandle m_possessedNode{};
    bool m_acceptInput = true;
    std::uint64_t m_ownerConnectionId = 0;

#if defined(SNAPI_GF_ENABLE_INPUT)
    SnAPI::Input::DeviceId m_assignedInputDevice{};
    bool m_useAssignedInputDevice = false;
#endif
};

} // namespace SnAPI::GameFramework
