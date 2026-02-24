#include <catch2/catch_test_macros.hpp>

#include "ComponentStorage.h"
#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

TEST_CASE("Component storage resolves owner UUID-only handles")
{
    NodeGraph Graph{};
    auto OwnerResult = Graph.CreateNode<BaseNode>("StorageFallbackOwner");
    REQUIRE(OwnerResult.has_value());

    TComponentStorage<TransformComponent> Storage{};
    auto AddResult = Storage.Add(*OwnerResult);
    REQUIRE(AddResult);

    const NodeHandle OwnerUuidOnly{OwnerResult->Id};

    REQUIRE(Storage.Has(OwnerUuidOnly));
    REQUIRE(Storage.Borrowed(OwnerUuidOnly) != nullptr);

    Storage.Remove(OwnerUuidOnly);
    Storage.EndFrame();

    REQUIRE_FALSE(Storage.Has(OwnerUuidOnly));
    REQUIRE(Storage.Borrowed(OwnerUuidOnly) == nullptr);
}

