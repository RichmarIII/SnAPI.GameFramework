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
 * @ingroup SnAPI_GameFramework
 * @brief Accessors for ambient information about the currently executing inbound reflected RPC.
 *
 * The bridge installs this context for the duration of inbound request handling. It is
 * intended for gameplay code that needs to inspect the connection that initiated an RPC
 * without threading an explicit connection parameter through every reflected method.
 *
 * Lifetime and threading:
 * - The context is thread-local.
 * - It is only set while a reflected RPC is actively being invoked.
 * - Outside that call window the accessors return no value.
 */
namespace NetRpcInvocationContext
{
/**
 * @brief Return the connection currently associated with inbound reflected RPC execution.
 * @return Connection handle for the active inbound RPC, or `std::nullopt` when no
 *         reflected RPC is being executed on the current thread.
 *
 * @note This is read-only ambient context. The returned handle does not extend the
 * lifetime of the underlying network connection object.
 */
SNAPI_GAMEFRAMEWORK_API std::optional<NetConnectionHandle> CurrentConnection();
} // namespace NetRpcInvocationContext

/**
 * @ingroup SnAPI_GameFramework
 * @brief Transport-neutral result codes produced by the reflection RPC bridge.
 *
 * These status values describe failures at the bridge layer after transport delivery has
 * already succeeded. They distinguish "the packet arrived, but the bridge could not
 * resolve or execute the request" from lower-level transport failures handled by
 * `RpcService`.
 */
enum class ERpcReflectionStatus : std::uint8_t
{
    Success = 0, /**< @brief The request was decoded, invoked, and any return value was encoded successfully. */
    TargetNotFound = 1, /**< @brief The target Node or Component UUID could not be resolved in the bound World. */
    MethodNotFound = 2, /**< @brief The method id was unknown or the reflected method was not registered for RPC. */
    DecodeFailed = 3, /**< @brief The request argument payload could not be decoded into the reflected parameter types. */
    EncodeFailed = 4, /**< @brief The reflected return value could not be serialized for the response payload. */
    InvokeFailed = 5, /**< @brief The method threw, returned an error, or otherwise failed during invocation. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief In-memory request payload routed through `RpcService`.
 *
 * The request carries enough metadata for the receiving bridge to resolve a target object,
 * identify the reflected method, and decode the serialized arguments.
 *
 * Ownership and lifetime:
 * - The struct owns its payload bytes.
 * - It is a transient transport object, not a long-lived gameplay object.
 *
 * @see NetRpcBridge, NetRpcCodec, NetRpcResponse
 */
struct NetRpcRequest
{
    std::uint8_t TargetKind = 0; /**< @brief Target discriminator: `0` for Node, `1` for Component. */
    Uuid TargetId{}; /**< @brief Stable UUID of the target object instance. */
    TypeId TargetType{}; /**< @brief Reflected concrete type of the target instance. Used for lazy registration on the receiver. */
    SnAPI::Networking::MethodId MethodIdValue = 0; /**< @brief Deterministic hashed method identifier derived from owner name, method name, and parameter types. */
    std::vector<SnAPI::Networking::Byte> Payload; /**< @brief Serialized argument bytes encoded in reflected parameter order. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Response payload emitted by the reflected RPC bridge.
 *
 * A successful response carries a serialized return value payload. A failed response
 * carries only an `ERpcReflectionStatus`.
 */
struct NetRpcResponse
{
    ERpcReflectionStatus Status = ERpcReflectionStatus::Success; /**< @brief Bridge-level result code for the request. */
    std::vector<SnAPI::Networking::Byte> Payload; /**< @brief Serialized reflected return value. Empty when the method returns nothing or when the request failed. */
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Stateless serializer for `NetRpcRequest` and `NetRpcResponse`.
 *
 * The codec provides the wire-level representation used by `RpcService`. It does not own
 * any transport resources and can be copied freely.
 *
 * Error semantics:
 * - All functions fail by returning `false`.
 * - Failure indicates malformed input, insufficient output capacity in the underlying
 *   writer, or a payload too large to encode.
 */
struct NetRpcCodec
{
    /**
     * @brief Encode an RPC request into a network byte stream.
     * @param Writer Destination writer.
     * @param RequestValue Request to serialize.
     * @return `true` on success, `false` on encoding failure.
     */
    bool EncodeRequest(SnAPI::Networking::NetByteWriter& Writer, const NetRpcRequest& RequestValue) const;

    /**
     * @brief Decode an RPC request from a network byte stream.
     * @param Reader Source reader positioned at the request payload.
     * @param RequestValue Destination request object.
     * @return `true` on success, `false` when the stream is malformed or truncated.
     */
    bool DecodeRequest(SnAPI::Networking::NetByteReader& Reader, NetRpcRequest& RequestValue) const;

    /**
     * @brief Encode an RPC response into a network byte stream.
     * @param Writer Destination writer.
     * @param ResponseValue Response to serialize.
     * @return `true` on success, `false` on encoding failure.
     */
    bool EncodeResponse(SnAPI::Networking::NetByteWriter& Writer, const NetRpcResponse& ResponseValue) const;

    /**
     * @brief Decode an RPC response from a network byte stream.
     * @param Reader Source reader positioned at the response payload.
     * @param ResponseValue Destination response object.
     * @return `true` on success, `false` when the stream is malformed or truncated.
     */
    bool DecodeResponse(SnAPI::Networking::NetByteReader& Reader, NetRpcResponse& ResponseValue) const;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Abstract dispatch surface used by `RpcService` to reach the reflection bridge.
 *
 * The transport layer binds one or more well-known RPC routes to this interface. The
 * concrete bridge implementation is then free to decode the request, resolve the target
 * object, and invoke the reflected method.
 *
 * Threading:
 * - The default `NetRpcBridge::Bind()` path registers these entry points on
 *   `EDispatchThread::Net`.
 * - Implementations therefore must either be thread-safe for direct invocation or must
 *   marshal work back to the game thread before touching unsafe game state.
 */
struct INetReflectionRpc
{
    virtual ~INetReflectionRpc() = default;

    /**
     * @brief Handle a request whose reflected direction is "to server".
     * @param Handle Connection associated with the inbound or outbound transport flow.
     * @param RequestValue Decoded reflected RPC request.
     * @return Bridge-level response payload.
     */
    virtual NetRpcResponse InvokeServer(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) = 0;

    /**
     * @brief Handle a request whose reflected direction is "to client".
     * @param Handle Connection associated with the inbound or outbound transport flow.
     * @param RequestValue Decoded reflected RPC request.
     * @return Bridge-level response payload.
     */
    virtual NetRpcResponse InvokeClient(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) = 0;

    /**
     * @brief Handle a request whose reflected direction is "to multicast".
     * @param Handle Connection associated with the inbound or outbound transport flow.
     *        Some transports may use `0` for broadcast-style submission.
     * @param RequestValue Decoded reflected RPC request.
     * @return Bridge-level response payload.
     */
    virtual NetRpcResponse InvokeMulticast(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) = 0;
};

/**
 * @ingroup SnAPI_GameFramework
 * @brief Reflection-driven RPC bridge for Node and Component methods.
 *
 * `NetRpcBridge` is the network-facing companion to the reflection system. It lets users
 * mark reflected methods with RPC flags, then invoke those methods by name on Nodes and
 * Components without hand-writing per-type transport glue.
 *
 * Design intent:
 * - keep gameplay code focused on reflected methods rather than transport packet structs
 * - derive stable wire method ids from reflected signatures
 * - reuse the same value codecs used by serialization and replication
 *
 * Core semantics:
 * - Method dispatch is exact-type and exact-signature. Argument `Variant` types must
 *   match the reflected parameter types exactly.
 * - The bridge caches RPC-capable methods by deterministic `MethodId`.
 * - `RegisterGraphTypes()` scans only the types currently present in the World. New types
 *   can be added later by calling `RegisterType()` or by letting the receiver lazily
 *   register `RequestValue.TargetType` when handling an inbound request.
 * - Binding is one-way: once attached to an `RpcService`, the bridge cannot be rebound to
 *   a different service or a different target id without constructing a new bridge.
 *
 * Ownership and lifetime:
 * - The bridge does not own the World or the `RpcService`.
 * - Completion callbacks are copied into the underlying transport call and may outlive
 *   the stack frame that initiated the call.
 *
 * Threading:
 * - Outbound `Call()` helpers are not internally synchronized.
 * - The default inbound dispatch path executes on the networking service's net thread.
 *   Reflected gameplay methods therefore run on that thread unless the surrounding
 *   integration explicitly marshals them elsewhere.
 *
 * Error semantics:
 * - Outbound call setup failures return `RpcId{0}` and, when provided, invoke the
 *   completion callback with an error.
 * - Inbound failures are encoded in `NetRpcResponse::Status`.
 *
 * @warning The Component overload requires the concrete reflected component type. Passing
 * a base type can cause method lookup to fail even when the object derives from that base.
 * @see NetReplicationBridge, NetRpcCodec, NetRpcInvocationContext, RpcService
 */
class SNAPI_GAMEFRAMEWORK_API NetRpcBridge final : public INetReflectionRpc
{
public:
    /**
     * @brief Completion callback signature for outbound RPC calls.
     *
     * The callback receives either the decoded reflected return value or an error that
     * explains why the transport or bridge-level invoke failed.
     */
    using CompletionFn = std::function<void(const TExpected<Variant>& Result)>;

    /**
     * @brief Construct a bridge with an optional World binding.
     *
     * @param WorldRef Non-owning World used to resolve target UUIDs during inbound
     *        request handling. May be null when the bridge is configured before a World
     *        exists, but inbound request handling will then fail with
     *        `ERpcReflectionStatus::TargetNotFound`.
     */
    explicit NetRpcBridge(IWorld* WorldRef = nullptr);

    /**
     * @brief Replace the World used for target lookup.
     * @param WorldRef Non-owning World pointer. May be null to disable target resolution.
     *
     * @note Changing the World does not clear the cached method table because that cache
     * is keyed by type, not by object instance.
     */
    void World(IWorld* WorldRef);

    /**
     * @brief Return the currently bound World pointer.
     * @return Non-owning World pointer used for inbound target resolution, or null.
     */
    IWorld* World() const;

    /**
     * @brief Bind the bridge to one `RpcService` target id.
     *
     * Binding registers the reflected server/client/multicast entry points with the
     * service, then registers this bridge as the target object for @p TargetIdValue.
     *
     * @param Service RPC service instance. The bridge does not take ownership.
     * @param TargetIdValue Transport target namespace/channel used to route reflected RPCs.
     * @return `true` on success. Returns `false` when registration fails or when the
     *         bridge is already bound incompatibly.
     *
     * @post On success, outbound `Call()` helpers can use the bound service.
     * @warning Rebinding rules are strict:
     * - binding again to the same service and the same target id is allowed and refreshes
     *   graph type registration
     * - binding to the same service with a different target id fails
     * - binding to a different service after a successful bind also fails
     */
    bool Bind(SnAPI::Networking::RpcService& Service, SnAPI::Networking::RpcTargetId TargetIdValue = 1);

    /**
     * @brief Register one reflected type and its RPC-capable base methods.
     *
     * The bridge walks the type's base hierarchy recursively and caches every reflected
     * method flagged for network RPC use.
     *
     * @param Type Reflected type to scan.
     *
     * @note Unknown types are ignored.
     */
    void RegisterType(const TypeId& Type);

    /**
     * @brief Register all Node and Component types currently present in the bound World.
     *
     * This is a convenience bootstrap step. It does not subscribe to future type
     * additions; call it again or use `RegisterType()` when new reflected gameplay types
     * are introduced after the initial scan.
     */
    void RegisterGraphTypes();

    /**
     * @brief Invoke a reflected RPC on a Node instance by method name.
     *
     * The bridge resolves the reflected method using the Node's concrete runtime type and
     * the exact `Variant` argument types provided in @p Args. It then derives transport
     * routing and reliability from the reflected method flags and submits the call through
     * the bound `RpcService`.
     *
     * @param Handle Connection handle used by the underlying transport. For multicast
     *        flows this may be `0` when the transport API treats the call as broadcast.
     * @param Target Target Node instance. The bridge only borrows the object for the
     *        duration of call setup.
     * @param MethodName Reflected method name.
     * @param Args Arguments packed as `Variant`s in reflected parameter order. Types must
     *        match exactly.
     * @param Completion Optional completion callback invoked when the response arrives or
     *        when setup fails immediately.
     * @param Options Transport options. Reliability may be overridden by reflected method
     *        flags.
     * @return Transport `RpcId`, or `0` when setup failed before the call was submitted.
     */
    SnAPI::Networking::RpcId Call(NetConnectionHandle Handle,
                                  const BaseNode& Target,
                                  std::string_view MethodName,
                                  std::span<const Variant> Args,
                                  CompletionFn Completion = {},
                                  SnAPI::Networking::RpcCallOptions Options = {});

    /**
     * @brief Invoke a reflected RPC on a Component instance with an explicit concrete type.
     *
     * Components are frequently referenced through a base `BaseComponent&`. The bridge
     * therefore needs the reflected concrete component type to locate the correct method
     * metadata.
     *
     * @param Handle Connection handle used by the underlying transport.
     * @param Target Target Component instance. Borrowed for the duration of call setup.
     * @param TargetType Reflected concrete type that should be used for method lookup and
     *        for target metadata on the wire.
     * @param MethodName Reflected method name.
     * @param Args Arguments packed as `Variant`s in reflected parameter order. Types must
     *        match exactly.
     * @param Completion Optional completion callback invoked when the response arrives or
     *        when setup fails immediately.
     * @param Options Transport options. Reliability may be overridden by reflected method
     *        flags.
     * @return Transport `RpcId`, or `0` when setup failed before the call was submitted.
     */
    SnAPI::Networking::RpcId Call(NetConnectionHandle Handle,
                                  const BaseComponent& Target,
                                  const TypeId& TargetType,
                                  std::string_view MethodName,
                                  std::span<const Variant> Args,
                                  CompletionFn Completion = {},
                                  SnAPI::Networking::RpcCallOptions Options = {});

    /**
     * @brief Convenience overload for Component-derived types.
     *
     * This overload supplies `StaticTypeId<T>()` as the reflected concrete component
     * type. Use it when the static type of @p Target already matches the reflected type
     * that declares the RPC method.
     *
     * @tparam T Concrete Component type. Must derive from `BaseComponent`.
     * @param Handle Connection handle used by the underlying transport.
     * @param Target Target Component instance.
     * @param MethodName Reflected method name.
     * @param Args Arguments packed as `Variant`s in reflected parameter order.
     * @param Completion Optional completion callback invoked when the response arrives or
     *        when setup fails immediately.
     * @param Options Transport call options.
     * @return Transport `RpcId`, or `0` when setup failed.
     */
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
     * @brief Submit a reflected RPC when the caller already has the method metadata.
     *
     * This overload avoids name-based method lookup and is primarily useful for code that
     * has already resolved the reflected method through another path.
     *
     * @param Handle Connection handle used by the underlying transport.
     * @param TargetKind Target discriminator: `0` for Node, `1` for Component.
     * @param TargetId UUID of the target object instance.
     * @param TargetType Reflected concrete target type transmitted on the wire.
     * @param MethodOwnerType Reflected type that actually declares @p Method.
     * @param Method Reflected method metadata to invoke.
     * @param Args Arguments packed as `Variant`s in reflected parameter order.
     * @param Completion Optional completion callback invoked when the response arrives or
     *        when setup fails immediately.
     * @param Options Transport call options. Reliability may be overridden by reflected
     *        method flags.
     * @return Transport `RpcId`, or `0` when setup failed before the call was submitted.
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
    /** @brief Decode and execute an inbound "to server" reflected RPC request. */
    NetRpcResponse InvokeServer(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) override;
    /** @brief Decode and execute an inbound "to client" reflected RPC request. */
    NetRpcResponse InvokeClient(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) override;
    /** @brief Decode and execute an inbound multicast reflected RPC request. */
    NetRpcResponse InvokeMulticast(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue) override;

private:
    struct RpcMethodEntry
    {
        TypeId OwnerType{}; /**< @brief Reflected declaring type used when encoding and decoding the RPC signature. */
        MethodInfo Method{}; /**< @brief Reflected method metadata, including flags and invoke callback. */
        SnAPI::Networking::MethodId MethodIdValue = 0; /**< @brief Deterministic transport method id derived from the reflected signature. */
    };

    /**
     * @brief Resolve an RPC-capable reflected method by name and exact argument signature.
     * @param Type Reflected type to search.
     * @param Name Method name to find.
     * @param Args Candidate arguments used for exact reflected parameter-type matching.
     * @param OutOwnerType Filled with the reflected declaring type when a method is found.
     * @return Pointer to reflected method metadata, or `nullptr` when no compatible RPC
     *         method exists.
     */
    const MethodInfo* FindRpcMethod(const TypeId& Type,
                                    std::string_view Name,
                                    std::span<const Variant> Args,
                                    TypeId& OutOwnerType) const;

    /**
     * @brief Shared inbound execution path for all reflected RPC directions.
     * @param Handle Connection associated with the inbound request.
     * @param RequestValue Decoded reflected RPC request.
     * @return Reflected RPC response describing success or bridge-level failure.
     */
    NetRpcResponse HandleRequest(SnAPI::Networking::NetConnectionHandle Handle, const NetRpcRequest& RequestValue);

    /**
     * @brief Shared outbound call submission path used by all public `Call()` overloads.
     * @param Handle Connection handle used by the underlying transport.
     * @param TargetKind Target discriminator: `0` for Node, `1` for Component.
     * @param TargetId UUID of the target object instance.
     * @param TargetType Reflected concrete target type written into the request.
     * @param Entry Resolved method metadata and deterministic wire id.
     * @param Args Variant-packed arguments in reflected parameter order.
     * @param Completion Completion callback to forward to the transport.
     * @param Options Transport options, possibly rewritten for reflected reliability.
     * @return Transport `RpcId`, or `0` when setup failed.
     */
    SnAPI::Networking::RpcId CallInternal(NetConnectionHandle Handle,
                                          std::uint8_t TargetKind,
                                          const Uuid& TargetId,
                                          const TypeId& TargetType,
                                          const RpcMethodEntry& Entry,
                                          std::span<const Variant> Args,
                                          CompletionFn Completion,
                                          SnAPI::Networking::RpcCallOptions Options);

    IWorld* m_world = nullptr; /**< @brief Non-owning World used to resolve inbound target UUIDs to live Node and Component instances. */
    SnAPI::Networking::RpcService* m_rpc = nullptr; /**< @brief Non-owning transport service used for outbound submission and inbound target registration. */
    SnAPI::Networking::RpcTargetId m_targetId = 1; /**< @brief Transport target id bound through `Bind()`. */
    std::unordered_map<SnAPI::Networking::MethodId, RpcMethodEntry> m_methods{}; /**< @brief Deterministic wire method id to reflected method metadata cache. */
};

#endif // SNAPI_GF_ENABLE_NETWORKING

} // namespace SnAPI::GameFramework
