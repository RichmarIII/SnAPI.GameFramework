#pragma once

#include <unordered_map>

#include "Expected.h"
#include "GameThreading.h"
#include "StaticTypeId.h"
#include "Uuid.h"
#include "WorldEcsRuntime.h"

namespace SnAPI::GameFramework
{

class BaseNode;

/**
 * @ingroup SnAPI_GameFramework
 * @brief Lazy factory table that bridges reflected node `TypeId` values to dense node storage creation.
 *
 * Reflection metadata knows a node's `TypeId`, but dense node storage still needs a compile-time
 * `TNode` to instantiate `TDenseRuntimeStorage<TNode>`. This registry closes that gap without
 * introducing a second runtime node representation: each registered node type contributes one
 * callback that ensures its typed storage exists inside `WorldEcsRuntime`.
 */
class NodeStorageFactoryRegistry
{
public:
    using EnsureStorageFn = void(*)(WorldEcsRuntime&);

    [[nodiscard]] static NodeStorageFactoryRegistry& Instance()
    {
        static NodeStorageFactoryRegistry Registry;
        return Registry;
    }

    template<typename TNode>
    void Register()
    {
        static_assert(std::is_base_of_v<BaseNode, TNode>, "Node storage factories require BaseNode-derived types");
        static_assert(DenseRuntimeNodeType<TNode>,
                      "Dense ECS node types must inherit NodeCRTP<Derived>, be move-only, and be noexcept movable");

        Register(StaticTypeId<TNode>(), [](WorldEcsRuntime& Runtime) {
            (void)Runtime.NodeStorage<TNode>();
        });
    }

    void Register(const TypeId& Type, EnsureStorageFn Fn)
    {
        if (!Fn)
        {
            return;
        }

        GameLockGuard Lock(m_mutex);
        auto [It, Inserted] = m_factories.emplace(Type, Fn);
        if (!Inserted)
        {
            DEBUG_ASSERT(It->second == Fn, "Duplicate node storage factory registration");
            It->second = Fn;
        }
    }

    [[nodiscard]] Result EnsureStorage(const TypeId& Type, WorldEcsRuntime& Runtime) const
    {
        EnsureStorageFn Fn = nullptr;
        {
            GameLockGuard Lock(m_mutex);
            if (auto It = m_factories.find(Type); It != m_factories.end())
            {
                Fn = It->second;
            }
        }

        if (!Fn)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Node storage factory was not registered"));
        }

        Fn(Runtime);
        return Ok();
    }

private:
    mutable GameMutex m_mutex{};
    std::unordered_map<TypeId, EnsureStorageFn, UuidHash> m_factories{};
};

} // namespace SnAPI::GameFramework
