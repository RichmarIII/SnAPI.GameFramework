#include "BaseNode.h"

#include "ComponentStorage.h"
#include "IWorld.h"
#include "Profiling.h"
#include "TypeRegistry.h"
#include "Variant.h"
#include <cstdint>
#include <vector>
#if defined(SNAPI_GF_ENABLE_NETWORKING)
#include "NetworkSystem.h"
#endif

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
                                const std::string_view MethodName,
                                const std::span<const Variant> Args,
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

bool InvokeLocal(void* Instance, const MethodInfo& Method, const std::span<const Variant> Args)
{
    auto Result = Method.Invoke(Instance, Args);
    return Result.has_value();
}
} // namespace

RuntimeNodeHandle BaseNode::ResolveRuntimeNodeHandle() const
{
    if (!m_world || m_self.Id.is_nil())
    {
        return {};
    }

    if (!m_runtimeNode.IsNull())
    {
        if (m_world->EcsRuntime().Nodes().Resolve(m_runtimeNode))
        {
            return m_runtimeNode;
        }
    }

    auto RuntimeHandleResult = m_world->RuntimeNodeById(m_self.Id);
    if (!RuntimeHandleResult)
    {
        return {};
    }

    return RuntimeHandleResult.value();
}

RuntimeNodeHandle BaseNode::ResolveRuntimeNodeHandleAndCache()
{
    m_runtimeNode = ResolveRuntimeNodeHandle();
    return m_runtimeNode;
}

bool BaseNode::IsServer() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_world)
    {
        return m_world->Networking().IsServer();
    }
#endif
    return true;
}

bool BaseNode::IsClient() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_world)
    {
        return m_world->Networking().IsClient();
    }
#endif
    return false;
}

bool BaseNode::IsListenServer() const
{
#if defined(SNAPI_GF_ENABLE_NETWORKING)
    if (m_world)
    {
        return m_world->Networking().IsListenServer();
    }
#endif
    return false;
}

bool BaseNode::CallRPC(const std::string_view MethodName, std::initializer_list<Variant> Args)
{
    // GCC 15 with libstdc++ currently rejects direct span(pointer, size) construction here.
    // Materialize a contiguous buffer and pass it as a span to keep semantics unchanged.
    std::vector<Variant> ArgCopy{Args};
    return CallRPC(MethodName, std::span<const Variant>(ArgCopy.data(), ArgCopy.size()));
}

bool BaseNode::CallRPC(const std::string_view MethodName, const std::span<const Variant> Args)
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
