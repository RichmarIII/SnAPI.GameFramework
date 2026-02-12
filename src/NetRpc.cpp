#include "NetRpc.h"

#if defined(SNAPI_GF_ENABLE_NETWORKING)

#include "ObjectRegistry.h"
#include "Serialization.h"

#include <NetHash.h>

#include <cereal/archives/binary.hpp>

#include <array>
#include <cstring>
#include <limits>
#include <ostream>
#include <streambuf>

SNAPI_DECLARE_RPC_EX(SnAPI::GameFramework::INetReflectionRpc,
                     InvokeServer,
                     "SnAPI.GameFramework.ReflectionRpc.Server",
                     SnAPI::GameFramework::NetRpcCodec,
                     SnAPI::Networking::ERpcDirection::ToServer)

SNAPI_DECLARE_RPC_EX(SnAPI::GameFramework::INetReflectionRpc,
                     InvokeClient,
                     "SnAPI.GameFramework.ReflectionRpc.Client",
                     SnAPI::GameFramework::NetRpcCodec,
                     SnAPI::Networking::ERpcDirection::ToClient)

SNAPI_DECLARE_RPC_EX(SnAPI::GameFramework::INetReflectionRpc,
                     InvokeMulticast,
                     "SnAPI.GameFramework.ReflectionRpc.Multicast",
                     SnAPI::GameFramework::NetRpcCodec,
                     SnAPI::Networking::ERpcDirection::ToMulticast)

namespace SnAPI::GameFramework
{
using SnAPI::Networking::NetConnectionHandle;

namespace
{
using SnAPI::Networking::Byte;
using SnAPI::Networking::ByteSpan;
using SnAPI::Networking::ConstByteSpan;
using SnAPI::Networking::MethodId;
using SnAPI::Networking::NetByteReader;
using SnAPI::Networking::NetByteWriter;
using SnAPI::Networking::RpcCallOptions;
using SnAPI::Networking::RpcId;
using SnAPI::Networking::RpcTargetId;
using SnAPI::Networking::TRpcCallResult;

enum class ENetObjectKind : std::uint8_t
{
    Node = 0,
    Component = 1,
};

class VectorWriteStreambuf final : public std::streambuf
{
public:
    explicit VectorWriteStreambuf(std::vector<uint8_t>& Buffer)
        : m_buffer(Buffer)
    {
    }

protected:
    int_type overflow(int_type Ch) override
    {
        if (traits_type::eq_int_type(Ch, traits_type::eof()))
        {
            return traits_type::eof();
        }
        m_buffer.push_back(static_cast<uint8_t>(Ch));
        return Ch;
    }

    std::streamsize xsputn(const char* Data, std::streamsize Count) override
    {
        if (!Data || Count <= 0)
        {
            return 0;
        }
        const auto Size = static_cast<size_t>(Count);
        const size_t Offset = m_buffer.size();
        m_buffer.resize(Offset + Size);
        std::memcpy(m_buffer.data() + Offset, Data, Size);
        return Count;
    }

private:
    std::vector<uint8_t>& m_buffer;
};

class MemoryReadStreambuf final : public std::streambuf
{
public:
    MemoryReadStreambuf(const uint8_t* Data, size_t Size)
    {
        if (!Data || Size == 0)
        {
            setg(nullptr, nullptr, nullptr);
            return;
        }
        char* Begin = const_cast<char*>(reinterpret_cast<const char*>(Data));
        setg(Begin, Begin, Begin + static_cast<std::streamsize>(Size));
    }
};

bool WriteUuid(NetByteWriter& Writer, const Uuid& Id)
{
    const auto& Bytes = Id.as_bytes();
    std::array<Byte, 16> Data{};
    for (size_t i = 0; i < Data.size(); ++i)
    {
        Data[i] = static_cast<Byte>(std::to_integer<uint8_t>(Bytes[i]));
    }
    return Writer.WriteBytes(ConstByteSpan(Data.data(), Data.size()));
}

bool ReadUuid(NetByteReader& Reader, Uuid& Out)
{
    std::array<Byte, 16> Data{};
    if (!Reader.ReadBytes(ByteSpan(Data.data(), Data.size())))
    {
        return false;
    }
    Out = Uuid(Data);
    return true;
}

bool EncodePayload(NetByteWriter& Writer, const std::vector<Byte>& Payload)
{
    if (Payload.size() > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const auto PayloadBytes = static_cast<std::uint32_t>(Payload.size());
    return Writer.WriteU32(PayloadBytes)
        && Writer.WriteBytes(ConstByteSpan(Payload.data(), Payload.size()));
}

bool DecodePayload(NetByteReader& Reader, std::vector<Byte>& OutPayload)
{
    std::uint32_t PayloadBytes = 0;
    if (!Reader.ReadU32(PayloadBytes))
    {
        return false;
    }
    if (Reader.Remaining() < PayloadBytes)
    {
        return false;
    }
    OutPayload.resize(PayloadBytes);
    if (PayloadBytes == 0)
    {
        return true;
    }
    return Reader.ReadBytes(ByteSpan(OutPayload.data(), OutPayload.size()));
}

MethodId BuildMethodId(const TypeInfo& Owner, const MethodInfo& Method)
{
    std::string Key;
    Key.reserve(Owner.Name.size() + Method.Name.size() + 32);
    Key.append(Owner.Name);
    Key.append("::");
    Key.append(Method.Name);
    Key.push_back('(');
    for (size_t i = 0; i < Method.ParamTypes.size(); ++i)
    {
        const auto* ParamInfo = TypeRegistry::Instance().Find(Method.ParamTypes[i]);
        const std::string ParamName = ParamInfo ? ParamInfo->Name : ToString(Method.ParamTypes[i]);
        Key.append(ParamName);
        if (i + 1 < Method.ParamTypes.size())
        {
            Key.push_back(',');
        }
    }
    Key.push_back(')');
    return SnAPI::Networking::NetHash::Hash32(Key);
}

bool IsRpcMethod(const MethodInfo& Method)
{
    return Method.Flags.Has(EMethodFlagBits::RpcNetServer)
        || Method.Flags.Has(EMethodFlagBits::RpcNetClient)
        || Method.Flags.Has(EMethodFlagBits::RpcNetMulticast);
}

SnAPI::Networking::ERpcDirection DirectionFor(const MethodInfo& Method)
{
    if (Method.Flags.Has(EMethodFlagBits::RpcNetClient))
    {
        return SnAPI::Networking::ERpcDirection::ToClient;
    }
    if (Method.Flags.Has(EMethodFlagBits::RpcNetMulticast))
    {
        return SnAPI::Networking::ERpcDirection::ToMulticast;
    }
    return SnAPI::Networking::ERpcDirection::ToServer;
}

bool ReliableFor(const MethodInfo& Method, bool DefaultReliable = true)
{
    if (Method.Flags.Has(EMethodFlagBits::RpcUnreliable))
    {
        return false;
    }
    if (Method.Flags.Has(EMethodFlagBits::RpcReliable))
    {
        return true;
    }
    return DefaultReliable;
}

TExpected<void> EncodeArgs(std::span<const TypeId> Types,
                           std::span<const Variant> Args,
                           std::vector<uint8_t>& OutBytes,
                           const TSerializationContext& Context)
{
    if (Types.size() != Args.size())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Argument count mismatch"));
    }
    OutBytes.clear();
    VectorWriteStreambuf Buffer(OutBytes);
    std::ostream Os(&Buffer);
    cereal::BinaryOutputArchive Archive(Os);
    auto& Registry = ValueCodecRegistry::Instance();
    for (size_t i = 0; i < Types.size(); ++i)
    {
        if (Args[i].Type() != Types[i])
        {
            return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Argument type mismatch"));
        }
        auto EncodeResult = Registry.Encode(Types[i], Args[i].Borrowed(), Archive, Context);
        if (!EncodeResult)
        {
            return EncodeResult;
        }
    }
    return Ok();
}

TExpected<std::vector<Variant>> DecodeArgs(std::span<const TypeId> Types,
                                           ConstByteSpan Data,
                                           const TSerializationContext& Context)
{
    MemoryReadStreambuf Buffer(Data.data(), Data.size());
    std::istream Is(&Buffer);
    cereal::BinaryInputArchive Archive(Is);
    std::vector<Variant> Args;
    Args.reserve(Types.size());
    auto& Registry = ValueCodecRegistry::Instance();
    for (const auto& Type : Types)
    {
        auto DecodeResult = Registry.Decode(Type, Archive, Context);
        if (!DecodeResult)
        {
            return std::unexpected(DecodeResult.error());
        }
        Args.push_back(std::move(DecodeResult.value()));
    }
    return Args;
}

TExpected<void> EncodeReturnValue(const TypeId& ReturnType,
                                  const Variant& Value,
                                  std::vector<uint8_t>& OutBytes,
                                  const TSerializationContext& Context)
{
    if (ReturnType == StaticTypeId<void>())
    {
        OutBytes.clear();
        return Ok();
    }
    OutBytes.clear();
    VectorWriteStreambuf Buffer(OutBytes);
    std::ostream Os(&Buffer);
    cereal::BinaryOutputArchive Archive(Os);
    auto& Registry = ValueCodecRegistry::Instance();
    return Registry.Encode(ReturnType, Value.Borrowed(), Archive, Context);
}

TExpected<Variant> DecodeReturnValue(const TypeId& ReturnType,
                                     ConstByteSpan Data,
                                     const TSerializationContext& Context)
{
    if (ReturnType == StaticTypeId<void>())
    {
        return Variant::Void();
    }
    MemoryReadStreambuf Buffer(Data.data(), Data.size());
    std::istream Is(&Buffer);
    cereal::BinaryInputArchive Archive(Is);
    return ValueCodecRegistry::Instance().Decode(ReturnType, Archive, Context);
}

} // namespace

bool NetRpcCodec::EncodeRequest(NetByteWriter& Writer, const NetRpcRequest& RequestValue) const
{
    return Writer.WriteU8(RequestValue.TargetKind)
        && WriteUuid(Writer, RequestValue.TargetId)
        && WriteUuid(Writer, RequestValue.TargetType)
        && Writer.WriteU32(RequestValue.MethodIdValue)
        && EncodePayload(Writer, RequestValue.Payload);
}

bool NetRpcCodec::DecodeRequest(NetByteReader& Reader, NetRpcRequest& RequestValue) const
{
    if (!Reader.ReadU8(RequestValue.TargetKind))
    {
        return false;
    }
    return ReadUuid(Reader, RequestValue.TargetId)
        && ReadUuid(Reader, RequestValue.TargetType)
        && Reader.ReadU32(RequestValue.MethodIdValue)
        && DecodePayload(Reader, RequestValue.Payload);
}

bool NetRpcCodec::EncodeResponse(NetByteWriter& Writer, const NetRpcResponse& ResponseValue) const
{
    return Writer.WriteU8(static_cast<std::uint8_t>(ResponseValue.Status))
        && EncodePayload(Writer, ResponseValue.Payload);
}

bool NetRpcCodec::DecodeResponse(NetByteReader& Reader, NetRpcResponse& ResponseValue) const
{
    std::uint8_t StatusValue = 0;
    if (!Reader.ReadU8(StatusValue))
    {
        return false;
    }
    ResponseValue.Status = static_cast<ERpcReflectionStatus>(StatusValue);
    return DecodePayload(Reader, ResponseValue.Payload);
}

NetRpcBridge::NetRpcBridge(NodeGraph* Graph)
    : m_graph(Graph)
{
}

void NetRpcBridge::Graph(NodeGraph* Graph)
{
    m_graph = Graph;
}

NodeGraph* NetRpcBridge::Graph() const
{
    return m_graph;
}

bool NetRpcBridge::Bind(SnAPI::Networking::RpcService& Service, RpcTargetId TargetIdValue)
{
    m_rpc = &Service;
    m_targetId = TargetIdValue;
    if (!m_rpc->RegisterMethod<&INetReflectionRpc::InvokeServer>(SnAPI::Networking::EDispatchThread::Net))
    {
        return false;
    }
    if (!m_rpc->RegisterMethod<&INetReflectionRpc::InvokeClient>(SnAPI::Networking::EDispatchThread::Net))
    {
        return false;
    }
    if (!m_rpc->RegisterMethod<&INetReflectionRpc::InvokeMulticast>(SnAPI::Networking::EDispatchThread::Net))
    {
        return false;
    }
    if (!m_rpc->RegisterTarget<INetReflectionRpc>(m_targetId, this))
    {
        return false;
    }
    RegisterGraphTypes();
    return true;
}

void NetRpcBridge::RegisterType(const TypeId& Type)
{
    const auto* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return;
    }
    for (const auto& Base : Info->BaseTypes)
    {
        RegisterType(Base);
    }
    for (const auto& Method : Info->Methods)
    {
        if (!IsRpcMethod(Method))
        {
            continue;
        }
        RpcMethodEntry Entry;
        Entry.OwnerType = Type;
        Entry.Method = Method;
        Entry.MethodIdValue = BuildMethodId(*Info, Method);
        m_methods.emplace(Entry.MethodIdValue, Entry);
    }
}

void NetRpcBridge::RegisterGraphTypes()
{
    if (!m_graph)
    {
        return;
    }
    std::unordered_map<TypeId, bool, UuidHash> Registered;
    m_graph->NodePool().ForEach([&](const NodeHandle& Handle, BaseNode& Node) {
        (void)Handle;
        if (!Registered.emplace(Node.TypeKey(), true).second)
        {
            return;
        }
        RegisterType(Node.TypeKey());
        for (const auto& Type : Node.ComponentTypes())
        {
            if (Registered.emplace(Type, true).second)
            {
                RegisterType(Type);
            }
        }
    });
}

SnAPI::Networking::RpcId NetRpcBridge::Call(NetConnectionHandle Handle,
                                            const BaseNode& Target,
                                            std::string_view MethodName,
                                            std::span<const Variant> Args,
                                            CompletionFn Completion,
                                            RpcCallOptions Options)
{
    TypeId OwnerType{};
    const MethodInfo* Method = FindRpcMethod(Target.TypeKey(), MethodName, Args, OwnerType);
    if (!Method)
    {
        if (Completion)
        {
            Completion(std::unexpected(MakeError(EErrorCode::NotFound, "RPC method not found")));
        }
        return 0;
    }
    return Call(Handle,
                static_cast<std::uint8_t>(ENetObjectKind::Node),
                Target.Id(),
                Target.TypeKey(),
                OwnerType,
                *Method,
                Args,
                std::move(Completion),
                Options);
}

SnAPI::Networking::RpcId NetRpcBridge::Call(NetConnectionHandle Handle,
                                            const IComponent& Target,
                                            const TypeId& TargetType,
                                            std::string_view MethodName,
                                            std::span<const Variant> Args,
                                            CompletionFn Completion,
                                            RpcCallOptions Options)
{
    TypeId OwnerType{};
    const MethodInfo* Method = FindRpcMethod(TargetType, MethodName, Args, OwnerType);
    if (!Method)
    {
        if (Completion)
        {
            Completion(std::unexpected(MakeError(EErrorCode::NotFound, "RPC method not found")));
        }
        return 0;
    }
    return Call(Handle,
                static_cast<std::uint8_t>(ENetObjectKind::Component),
                Target.Id(),
                TargetType,
                OwnerType,
                *Method,
                Args,
                std::move(Completion),
                Options);
}

SnAPI::Networking::RpcId NetRpcBridge::Call(NetConnectionHandle Handle,
                                            std::uint8_t TargetKind,
                                            const Uuid& TargetId,
                                            const TypeId& TargetType,
                                            const TypeId& MethodOwnerType,
                                            const MethodInfo& Method,
                                            std::span<const Variant> Args,
                                            CompletionFn Completion,
                                            RpcCallOptions Options)
{
    const auto* Info = TypeRegistry::Instance().Find(MethodOwnerType);
    if (!Info)
    {
        if (Completion)
        {
            Completion(std::unexpected(MakeError(EErrorCode::NotFound, "Target type not registered")));
        }
        return 0;
    }

    RpcMethodEntry Entry;
    Entry.OwnerType = MethodOwnerType;
    Entry.Method = Method;
    Entry.MethodIdValue = BuildMethodId(*Info, Method);

    return CallInternal(Handle,
                        TargetKind,
                        TargetId,
                        TargetType,
                        Entry,
                        Args,
                        std::move(Completion),
                        Options);
}

NetRpcResponse NetRpcBridge::InvokeServer(NetConnectionHandle Handle, const NetRpcRequest& RequestValue)
{
    return HandleRequest(Handle, RequestValue);
}

NetRpcResponse NetRpcBridge::InvokeClient(NetConnectionHandle Handle, const NetRpcRequest& RequestValue)
{
    return HandleRequest(Handle, RequestValue);
}

NetRpcResponse NetRpcBridge::InvokeMulticast(NetConnectionHandle Handle, const NetRpcRequest& RequestValue)
{
    return HandleRequest(Handle, RequestValue);
}

const MethodInfo* NetRpcBridge::FindRpcMethod(const TypeId& Type,
                                              std::string_view Name,
                                              std::span<const Variant> Args,
                                              TypeId& OutOwnerType) const
{
    const auto* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return nullptr;
    }
    for (const auto& Method : Info->Methods)
    {
        if (Method.Name != Name || !IsRpcMethod(Method))
        {
            continue;
        }
        if (Method.ParamTypes.size() != Args.size())
        {
            continue;
        }
        bool Match = true;
        for (size_t i = 0; i < Args.size(); ++i)
        {
            if (Args[i].Type() != Method.ParamTypes[i])
            {
                Match = false;
                break;
            }
        }
        if (Match)
        {
            OutOwnerType = Type;
            return &Method;
        }
    }
    for (const auto& Base : Info->BaseTypes)
    {
        if (const auto* Method = FindRpcMethod(Base, Name, Args, OutOwnerType))
        {
            return Method;
        }
    }
    return nullptr;
}

NetRpcResponse NetRpcBridge::HandleRequest(NetConnectionHandle,
                                           const NetRpcRequest& RequestValue)
{
    NetRpcResponse Response{};
    if (!m_graph)
    {
        Response.Status = ERpcReflectionStatus::TargetNotFound;
        return Response;
    }

    auto MethodIt = m_methods.find(RequestValue.MethodIdValue);
    if (MethodIt == m_methods.end() && !RequestValue.TargetType.is_nil())
    {
        RegisterType(RequestValue.TargetType);
        MethodIt = m_methods.find(RequestValue.MethodIdValue);
    }
    if (MethodIt == m_methods.end())
    {
        Response.Status = ERpcReflectionStatus::MethodNotFound;
        return Response;
    }

    const auto& Entry = MethodIt->second;
    void* Instance = nullptr;
    if (RequestValue.TargetKind == static_cast<std::uint8_t>(ENetObjectKind::Node))
    {
        auto* Node = ObjectRegistry::Instance().Resolve<BaseNode>(RequestValue.TargetId);
        Instance = Node;
    }
    else if (RequestValue.TargetKind == static_cast<std::uint8_t>(ENetObjectKind::Component))
    {
        auto* Component = ObjectRegistry::Instance().Resolve<IComponent>(RequestValue.TargetId);
        Instance = Component;
    }

    if (!Instance)
    {
        Response.Status = ERpcReflectionStatus::TargetNotFound;
        return Response;
    }

    TSerializationContext Context;
    Context.Graph = m_graph;
    auto ArgsResult = DecodeArgs(Entry.Method.ParamTypes,
                                 ConstByteSpan(RequestValue.Payload.data(), RequestValue.Payload.size()),
                                 Context);
    if (!ArgsResult)
    {
        Response.Status = ERpcReflectionStatus::DecodeFailed;
        return Response;
    }

    auto InvokeResult = Entry.Method.Invoke(Instance, ArgsResult.value());
    if (!InvokeResult)
    {
        Response.Status = ERpcReflectionStatus::InvokeFailed;
        return Response;
    }

    auto EncodeResult = EncodeReturnValue(Entry.Method.ReturnType, InvokeResult.value(), Response.Payload, Context);
    if (!EncodeResult)
    {
        Response.Status = ERpcReflectionStatus::EncodeFailed;
        return Response;
    }

    Response.Status = ERpcReflectionStatus::Success;
    return Response;
}

RpcId NetRpcBridge::CallInternal(NetConnectionHandle Handle,
                                 std::uint8_t TargetKind,
                                 const Uuid& TargetId,
                                 const TypeId& TargetType,
                                 const RpcMethodEntry& Entry,
                                 std::span<const Variant> Args,
                                 CompletionFn Completion,
                                 RpcCallOptions Options)
{
    if (!m_rpc)
    {
        if (Completion)
        {
            Completion(std::unexpected(MakeError(EErrorCode::InvalidArgument, "RpcService not bound")));
        }
        return 0;
    }

    if (!IsRpcMethod(Entry.Method))
    {
        if (Completion)
        {
            Completion(std::unexpected(MakeError(EErrorCode::InvalidArgument, "Method not flagged for RPC")));
        }
        return 0;
    }

    TSerializationContext Context;
    Context.Graph = m_graph;

    NetRpcRequest Request{};
    Request.TargetKind = TargetKind;
    Request.TargetId = TargetId;
    Request.TargetType = TargetType;
    Request.MethodIdValue = Entry.MethodIdValue;

    auto EncodeResult = EncodeArgs(Entry.Method.ParamTypes, Args, Request.Payload, Context);
    if (!EncodeResult)
    {
        if (Completion)
        {
            Completion(std::unexpected(EncodeResult.error()));
        }
        return 0;
    }

    Options.Reliable = ReliableFor(Entry.Method, Options.Reliable);

    const auto CompletionWrapper = [Completion = std::move(Completion),
                                    ReturnType = Entry.Method.ReturnType,
                                    Context](const TRpcCallResult<NetRpcResponse>& Result) mutable {
        if (!Completion)
        {
            return;
        }
        if (Result.Status != SnAPI::Networking::ERpcCallStatus::Success || !Result.ResponseValue)
        {
            Completion(std::unexpected(MakeError(EErrorCode::InternalError, "RPC transport failed")));
            return;
        }
        const auto& Response = *Result.ResponseValue;
        if (Response.Status != ERpcReflectionStatus::Success)
        {
            Completion(std::unexpected(MakeError(EErrorCode::InternalError, "RPC invoke failed")));
            return;
        }
        auto DecodeResult = DecodeReturnValue(ReturnType,
                                              ConstByteSpan(Response.Payload.data(), Response.Payload.size()),
                                              Context);
        if (!DecodeResult)
        {
            Completion(std::unexpected(DecodeResult.error()));
            return;
        }
        Completion(DecodeResult.value());
    };

    const auto Direction = DirectionFor(Entry.Method);
    if (Direction == SnAPI::Networking::ERpcDirection::ToServer)
    {
        return m_rpc->Call<&INetReflectionRpc::InvokeServer>(Handle,
                                                             m_targetId,
                                                             Request,
                                                             CompletionWrapper,
                                                             Options);
    }
    if (Direction == SnAPI::Networking::ERpcDirection::ToClient)
    {
        return m_rpc->Call<&INetReflectionRpc::InvokeClient>(Handle,
                                                             m_targetId,
                                                             Request,
                                                             CompletionWrapper,
                                                             Options);
    }
    return m_rpc->Call<&INetReflectionRpc::InvokeMulticast>(Handle,
                                                            m_targetId,
                                                            Request,
                                                            CompletionWrapper,
                                                            Options);
}

} // namespace SnAPI::GameFramework

#endif // SNAPI_GF_ENABLE_NETWORKING
