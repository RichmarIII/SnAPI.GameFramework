#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Export.h"
#include "BaseComponent.h"
#include "IWorld.h"
#include "StaticTypeId.h"
#include "TypeName.h"
#include "TypeRegistry.h"
#include "Variant.h"

#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include <Services/RpcService.h>
#endif

namespace SnAPI::GameFramework
{

#if defined(SNAPI_GF_ENABLE_NETWORKING)

using SnAPI::Networking::NetConnectionHandle;

/**
 * @brief Accessors for the currently executing reflected RPC invocation context.
 * @remarks
 * Context is thread-local and only valid while an incoming RPC is being invoked.
 */
namespace NetRpcInvocationContext
{
SNAPI_GAMEFRAMEWORK_API std::optional<NetConnectionHandle> CurrentConnection();
}

/**
 * @brief Status codes for reflection RPC responses.
 * @remarks Encodes bridge-level resolution/invoke failures in transport-neutral form.
 */
enum class ERpcReflectionStatus : std::uint8_t
{
    Success = 0,
    TargetNotFound = 1,
    MethodNotFound = 2,
    DecodeFailed = 3,
    EncodeFailed = 4,
    InvokeFailed = 5,
};

/**
 * @brief Reflection RPC request payload.
 * @remarks Compact transport object used by `NetRpcBridge` + `RpcService`.
 */
struct NetRpcRequest
{
    std::uint8_t TargetKind = 0; /**< @brief 0 = node, 1 = component. */
    Uuid TargetId{}; /**< @brief Target object UUID. */
    TypeId TargetType{}; /**< @brief Reflected type id for the target. */
    SnAPI::Networking::MethodId MethodIdValue = 0; /**< @brief Reflected method id. */
    std::vector<SnAPI::Networking::Byte> Payload; /**< @brief Serialized arguments. */
};

/**
 * @brief Reflection RPC response payload.
 * @remarks Contains status plus optional serialized return value payload.
 */
struct NetRpcResponse
{
    ERpcReflectionStatus Status = ERpcReflectionStatus::Success; /**< @brief Result status. */
    std::vector<SnAPI::Networking::Byte> Payload; /**< @brief Serialized return value. */
};

/**
 * @brief Codec for reflection RPC request/response payloads.
 * @remarks Converts between in-memory request/response structures and byte streams.
 */
struct NetRpcCodec
{
    bool EncodeRequest(SnAPI::Networking::NetByteWriter& Writer, const NetRpcRequest& RequestValue) const;
    bool DecodeRequest(SnAPI::Networking::NetByteReader& Reader, NetRpcRequest& RequestValue) const;
    bool EncodeResponse(SnAPI::Networking::NetByteWriter& Writer, const NetRpcResponse& ResponseValue) const;
    bool DecodeResponse(SnAPI::Networking::NetByteReader& Reader, NetRpcResponse& ResponseValue) const;
};

/**
 * @brief RPC interface used by the bridge to route reflection calls.
 * @remarks Abstract target for server/client/multicast dispatch entrypoints.
 */
struct INetReflectionRpc
{
    virtual ~INetReflectionRpc() = default;
    virtual NetRpcResponse InvokeServer(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) = 0;
    virtual NetRpcResponse InvokeClient(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) = 0;
    virtual NetRpcResponse InvokeMulticast(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) = 0;
};

/**
 * @brief Reflection-driven RPC bridge for Nodes and Components.
 * @remarks
 * Maps reflected method metadata to runtime RPC dispatch.
 *
 * Responsibilities:
 * - register reflected RPC-capable methods and deterministic method ids
 * - encode argument variants via codec registry
 * - resolve target object and method by UUID/type/method id on receive
 * - invoke reflected methods and encode return payloads
 */
class SNAPI_GAMEFRAMEWORK_API NetRpcBridge final : public INetReflectionRpc
{
public:
    /** @brief Completion callback signature for asynchronous RPC call results. */
    using CompletionFn = std::function<void(const TExpected<Variant>& Result)>;

    /**
     * @brief Construct bridge for an optional world graph context.
     * @param WorldRef World used for target resolution.
     */
    explicit NetRpcBridge(IWorld* WorldRef = nullptr);

    /** @brief Set target world used for node/component lookup on invoke. */
    void World(IWorld* WorldRef);
    /** @brief Get target world used for invoke routing. */
    IWorld* World() const;

    /**
     * @brief Bind bridge to RpcService target id.
     * @param Service RPC service instance.
     * @param TargetIdValue target id namespace/channel.
     * @return True on successful bind.
     */
    bool Bind(SnAPI::Networking::RpcService& Service, SnAPI::Networking::RpcTargetId TargetIdValue = 1);

    /** @brief Register one reflected type for RPC method mapping. */
    void RegisterType(const TypeId& Type);
    /** @brief Register all currently present graph node/component types for RPC mapping. */
    void RegisterGraphTypes();

    /**
     * @brief Invoke reflected RPC targeting a node instance.
     * @param Handle Connection handle (0 for multicast/broadcast semantics where supported).
     * @param Target Node target object.
     * @param MethodName Reflected method name.
     * @param Args Variant-packed arguments.
     * @param Completion Optional completion callback for return payload.
     * @param Options Transport RPC options (reliability/channel/timeouts).
     * @return RpcId assigned by underlying RpcService.
     */
    SnAPI::Networking::RpcId Call(NetConnectionHandle Handle,
                                  const BaseNode& Target,
                                  std::string_view MethodName,
                                  std::span<const Variant> Args,
                                  CompletionFn Completion = {},
                                  SnAPI::Networking::RpcCallOptions Options = {});

    /**
     * @brief Invoke reflected RPC targeting a component instance.
     * @param Handle Connection handle (0 for multicast/broadcast semantics where supported).
     * @param Target Component target object.
     * @param TargetType Reflected concrete component type id.
     * @param MethodName Reflected method name.
     * @param Args Variant-packed arguments.
     * @param Completion Optional completion callback for return payload.
     * @param Options Transport RPC options.
     * @return RpcId assigned by underlying RpcService.
     */
    SnAPI::Networking::RpcId Call(NetConnectionHandle Handle,
                                  const BaseComponent& Target,
                                  const TypeId& TargetType,
                                  std::string_view MethodName,
                                  std::span<const Variant> Args,
                                  CompletionFn Completion = {},
                                  SnAPI::Networking::RpcCallOptions Options = {});

    template<typename T>
    SnAPI::Networking::RpcId Call(NetConnectionHandle Handle,
                                  const T& Target,
                                  std::string_view MethodName,
                                  std::span<const Variant> Args,
                                  CompletionFn Completion = {},
                                  SnAPI::Networking::RpcCallOptions Options = {})
    {
        static_assert(std::is_base_of_v<BaseComponent, T>, "T must derive from BaseComponent");
        return Call(Handle,
                    static_cast<const BaseComponent&>(Target),
                    StaticTypeId<T>(),
                    MethodName,
                    Args,
                    std::move(Completion),
                    Options);
    }

    /**
     * @brief Low-level call path with explicit target metadata.
     * @param Handle Connection handle.
     * @param TargetKind 0=node, 1=component.
     * @param TargetId UUID of target object.
     * @param TargetType Reflected concrete target type id.
     * @param MethodOwnerType Reflected owner type id that declares the method.
     * @param Method Reflected method metadata.
     * @param Args Variant-packed arguments.
     * @param Completion Optional completion callback.
     * @param Options Transport options.
     * @return RpcId assigned by underlying RpcService.
     */
    SnAPI::Networking::RpcId Call(NetConnectionHandle Handle,
                                  std::uint8_t TargetKind,
                                  const Uuid& TargetId,
                                  const TypeId& TargetType,
                                  const TypeId& MethodOwnerType,
                                  const MethodInfo& Method,
                                  std::span<const Variant> Args,
                                  CompletionFn Completion = {},
                                  SnAPI::Networking::RpcCallOptions Options = {});

    // INetReflectionRpc
    /** @brief Server-target dispatcher implementation for incoming request payloads. */
    NetRpcResponse InvokeServer(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) override;
    /** @brief Client-target dispatcher implementation for incoming request payloads. */
    NetRpcResponse InvokeClient(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) override;
    /** @brief Multicast-target dispatcher implementation for incoming request payloads. */
    NetRpcResponse InvokeMulticast(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) override;

private:
    struct RpcMethodEntry
    {
        TypeId OwnerType{}; /**< @brief Reflected owner type where the method is declared. */
        MethodInfo Method{}; /**< @brief Reflected method metadata including invoke callback and flags. */
        SnAPI::Networking::MethodId MethodIdValue = 0; /**< @brief Deterministic networking method id used on wire. */
    };

    /**
     * @brief Resolve reflected method metadata by name and compatible argument signature.
     * @param Type Target reflected type id.
     * @param Name Requested method name.
     * @param Args Argument payload used for overload compatibility check.
     * @param OutOwnerType Filled with owner type id that declares selected method.
     * @return Pointer to resolved method metadata or nullptr.
     */
    const MethodInfo* FindRpcMethod(const TypeId& Type,
                                    std::string_view Name,
                                    std::span<const Variant> Args,
                                    TypeId& OutOwnerType) const;

    /** @brief Decode + execute one incoming request against local graph object state. */
    NetRpcResponse HandleRequest(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue);

    /** @brief Shared internal call implementation used by all public `Call` overloads. */
    SnAPI::Networking::RpcId CallInternal(NetConnectionHandle Handle,
                                          std::uint8_t TargetKind,
                                          const Uuid& TargetId,
                                          const TypeId& TargetType,
                                          const RpcMethodEntry& Entry,
                                          std::span<const Variant> Args,
                                          CompletionFn Completion,
                                          SnAPI::Networking::RpcCallOptions Options);

    IWorld* m_world = nullptr; /**< @brief Non-owning world context for target UUID resolution. */
    SnAPI::Networking::RpcService* m_rpc = nullptr; /**< @brief Non-owning bound RpcService pointer. */
    SnAPI::Networking::RpcTargetId m_targetId = 1; /**< @brief Bound target id namespace/channel. */
    std::unordered_map<SnAPI::Networking::MethodId, RpcMethodEntry> m_methods{}; /**< @brief MethodId -> reflected method mapping table. */
};

#endif // SNAPI_GF_ENABLE_NETWORKING

} // namespace SnAPI::GameFramework
