#include "INode.h"

#include "IWorld.h"
#include "TypeRegistry.h"
#include "Variant.h"

#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetworkSystem.h"
#endif

#include <cstdint>

namespace SnAPI::GameFramework
{
namespace
{
constexpr std::uint8_t kRpcTargetNode = 0;

bool IsRpcMethod(const MethodInfo& Method)
{
    return Method.Flags.Has(EMethodFlagBits::RpcNetServer)
        || Method.Flags.Has(EMethodFlagBits::RpcNetClient)
        || Method.Flags.Has(EMethodFlagBits::RpcNetMulticast);
}

const MethodInfo* FindRpcMethod(const TypeId& Type,
                                std::string_view MethodName,
                                std::span<const Variant> Args,
                                TypeId& OutOwnerType)
{
    const auto* Info = TypeRegistry::Instance().Find(Type);
    if (!Info)
    {
        return nullptr;
    }

    for (const auto& Method : Info->Methods)
    {
        if (Method.Name != MethodName || !IsRpcMethod(Method))
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
            if (Method.ParamTypes[i] != Args[i].Type())
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
        if (const auto* Method = FindRpcMethod(Base, MethodName, Args, OutOwnerType))
        {
            return Method;
        }
    }

    return nullptr;
}

bool InvokeLocal(void* Instance, const MethodInfo& Method, std::span<const Variant> Args)
{
    auto Result = Method.Invoke(Instance, Args);
    return Result.has_value();
}

} // namespace

bool INode::CallRPC(std::string_view MethodName, std::initializer_list<Variant> Args)
{
    return CallRPC(MethodName, std::span<const Variant>(Args.begin(), Args.size()));
}

bool INode::CallRPC(std::string_view MethodName, std::span<const Variant> Args)
{
    TypeId MethodOwner{};
    const MethodInfo* Method = FindRpcMethod(TypeKey(), MethodName, Args, MethodOwner);
    if (!Method)
    {
        return false;
    }

    if (Method->Flags.Has(EMethodFlagBits::RpcNetServer))
    {
        if (IsServer())
        {
            return InvokeLocal(this, *Method, Args);
        }

#if defined(SNAPI_GF_ENABLE_NETWORKING)
        auto* WorldPtr = World();
        if (!WorldPtr)
        {
            return false;
        }

        auto& Network = WorldPtr->Networking();
        auto* Bridge = Network.RpcBridge();
        if (!Network.Session() || !Network.Rpc() || !Bridge)
        {
            return false;
        }

        const auto Connection = Network.PrimaryConnection();
        if (!Connection)
        {
            return false;
        }

        return Bridge->Call(*Connection,
                            kRpcTargetNode,
                            Id(),
                            TypeKey(),
                            MethodOwner,
                            *Method,
                            Args)
            != 0;
#else
        return false;
#endif
    }

    if (Method->Flags.Has(EMethodFlagBits::RpcNetClient))
    {
        if (IsServer())
        {
#if defined(SNAPI_GF_ENABLE_NETWORKING)
            auto* WorldPtr = World();
            auto* Network = WorldPtr ? &WorldPtr->Networking() : nullptr;
            auto* Bridge = Network ? Network->RpcBridge() : nullptr;
            if (Network && Network->Session() && Network->Rpc() && Bridge)
            {
                const auto Connection = Network->PrimaryConnection();
                if (Connection)
                {
                    return Bridge->Call(*Connection,
                                        kRpcTargetNode,
                                        Id(),
                                        TypeKey(),
                                        MethodOwner,
                                        *Method,
                                        Args)
                        != 0;
                }

                if (!IsListenServer())
                {
                    return false;
                }
            }
#endif
            return InvokeLocal(this, *Method, Args);
        }

        if (IsClient())
        {
            return InvokeLocal(this, *Method, Args);
        }

        return false;
    }

    if (Method->Flags.Has(EMethodFlagBits::RpcNetMulticast))
    {
        if (IsServer())
        {
#if defined(SNAPI_GF_ENABLE_NETWORKING)
            auto* WorldPtr = World();
            auto* Network = WorldPtr ? &WorldPtr->Networking() : nullptr;
            auto* Bridge = Network ? Network->RpcBridge() : nullptr;
            if (Network && Network->Session() && Network->Rpc() && Bridge)
            {
                return Bridge->Call(0,
                                    kRpcTargetNode,
                                    Id(),
                                    TypeKey(),
                                    MethodOwner,
                                    *Method,
                                    Args)
                    != 0;
            }
#endif
            return InvokeLocal(this, *Method, Args);
        }

        if (IsClient())
        {
            return InvokeLocal(this, *Method, Args);
        }

        return false;
    }

    return false;
}

} // namespace SnAPI::GameFramework
