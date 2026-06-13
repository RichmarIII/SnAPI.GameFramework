#include "AssetRef.h"

#include <mutex>
#include <utility>

namespace SnAPI::GameFramework
{
namespace
{
struct AssetManagerResolverState
{
    std::mutex Mutex{};
    TAssetManagerResolver Resolver{};

    static AssetManagerResolverState& Instance()
    {
        static AssetManagerResolverState State{};
        return State;
    }

    AssetManagerResolverState(const AssetManagerResolverState&) = delete;
    AssetManagerResolverState& operator=(const AssetManagerResolverState&) = delete;

private:
    AssetManagerResolverState() = default;
};
} // namespace

void SetDefaultAssetManagerResolver(TAssetManagerResolver Resolver)
{
    auto& State = AssetManagerResolverState::Instance();
    std::scoped_lock Lock(State.Mutex);
    State.Resolver = std::move(Resolver);
}

void ClearDefaultAssetManagerResolver()
{
    auto& State = AssetManagerResolverState::Instance();
    std::scoped_lock Lock(State.Mutex);
    State.Resolver = {};
}

::SnAPI::AssetPipeline::AssetManager* ResolveDefaultAssetManager()
{
    auto& State = AssetManagerResolverState::Instance();
    std::scoped_lock Lock(State.Mutex);
    if (!State.Resolver)
    {
        return nullptr;
    }

    return State.Resolver();
}

} // namespace SnAPI::GameFramework
