#include "AssetRef.h"

#include <mutex>
#include <utility>

namespace SnAPI::GameFramework
{
namespace
{
std::mutex GAssetManagerResolverMutex{};
TAssetManagerResolver GAssetManagerResolver{};
} // namespace

void SetDefaultAssetManagerResolver(TAssetManagerResolver Resolver)
{
    std::scoped_lock Lock(GAssetManagerResolverMutex);
    GAssetManagerResolver = std::move(Resolver);
}

void ClearDefaultAssetManagerResolver()
{
    std::scoped_lock Lock(GAssetManagerResolverMutex);
    GAssetManagerResolver = {};
}

::SnAPI::AssetPipeline::AssetManager* ResolveDefaultAssetManager()
{
    std::scoped_lock Lock(GAssetManagerResolverMutex);
    if (!GAssetManagerResolver)
    {
        return nullptr;
    }

    return GAssetManagerResolver();
}

} // namespace SnAPI::GameFramework
