#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Export.h"
#include "IComponent.h"
#include "NodeGraph.h"
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
 * @brief Status codes for reflection RPC responses.
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
 */
struct NetRpcResponse
{
    ERpcReflectionStatus Status = ERpcReflectionStatus::Success; /**< @brief Result status. */
    std::vector<SnAPI::Networking::Byte> Payload; /**< @brief Serialized return value. */
};

/**
 * @brief Codec for reflection RPC request/response payloads.
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
 */
class SNAPI_GAMEFRAMEWORK_API NetRpcBridge final : public INetReflectionRpc
{
public:
    using CompletionFn = std::function<void(const TExpected<Variant>& Result)>;

    explicit NetRpcBridge(NodeGraph* Graph = nullptr);

    void Graph(NodeGraph* Graph);
    NodeGraph* Graph() const;

    bool Bind(SnAPI::Networking::RpcService& Service, SnAPI::Networking::RpcTargetId TargetIdValue = 1);

    void RegisterType(const TypeId& Type);
    void RegisterGraphTypes();

    SnAPI::Networking::RpcId Call(NetConnectionHandle Handle,
                                  const BaseNode& Target,
                                  std::string_view MethodName,
                                  std::span<const Variant> Args,
                                  CompletionFn Completion = {},
                                  SnAPI::Networking::RpcCallOptions Options = {});

    SnAPI::Networking::RpcId Call(NetConnectionHandle Handle,
                                  const IComponent& Target,
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
        static_assert(std::is_base_of_v<IComponent, T>, "T must derive from IComponent");
        return Call(Handle,
                    static_cast<const IComponent&>(Target),
                    StaticTypeId<T>(),
                    MethodName,
                    Args,
                    std::move(Completion),
                    Options);
    }

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
    NetRpcResponse InvokeServer(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) override;
    NetRpcResponse InvokeClient(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) override;
    NetRpcResponse InvokeMulticast(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) override;

private:
    struct RpcMethodEntry
    {
        TypeId OwnerType{};
        MethodInfo Method{};
        SnAPI::Networking::MethodId MethodIdValue = 0;
    };

    const MethodInfo* FindRpcMethod(const TypeId& Type,
                                    std::string_view Name,
                                    std::span<const Variant> Args,
                                    TypeId& OutOwnerType) const;

    NetRpcResponse HandleRequest(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue);

    SnAPI::Networking::RpcId CallInternal(NetConnectionHandle Handle,
                                          std::uint8_t TargetKind,
                                          const Uuid& TargetId,
                                          const TypeId& TargetType,
                                          const RpcMethodEntry& Entry,
                                          std::span<const Variant> Args,
                                          CompletionFn Completion,
                                          SnAPI::Networking::RpcCallOptions Options);

    NodeGraph* m_graph = nullptr;
    SnAPI::Networking::RpcService* m_rpc = nullptr;
    SnAPI::Networking::RpcTargetId m_targetId = 1;
    std::unordered_map<SnAPI::Networking::MethodId, RpcMethodEntry> m_methods{};
};

#endif // SNAPI_GF_ENABLE_NETWORKING

} // namespace SnAPI::GameFramework
