#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"

using namespace SnAPI::GameFramework;

namespace
{

struct CopyableVariantPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::CopyableVariantPayload";
    int Value = 0;
};

struct MoveOnlyVariantPayload
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::Tests::MoveOnlyVariantPayload";
    int Value = 0;

    MoveOnlyVariantPayload() = default;
    explicit MoveOnlyVariantPayload(const int InValue)
        : Value(InValue)
    {
    }

    MoveOnlyVariantPayload(const MoveOnlyVariantPayload&) = delete;
    MoveOnlyVariantPayload& operator=(const MoveOnlyVariantPayload&) = delete;
    MoveOnlyVariantPayload(MoveOnlyVariantPayload&&) = default;
    MoveOnlyVariantPayload& operator=(MoveOnlyVariantPayload&&) = default;
};

} // namespace

TEST_CASE("Owned variant copies detach on mutable access")
{
    Variant A = Variant::FromValue(CopyableVariantPayload{3});
    Variant B = A;

    REQUIRE(A.StorageKind() == Variant::EStorageKind::Owned);
    REQUIRE(B.StorageKind() == Variant::EStorageKind::Owned);

    auto ARef = A.AsRef<CopyableVariantPayload>();
    REQUIRE(ARef);
    ARef->get().Value = 9;

    auto AConst = A.AsConstRef<CopyableVariantPayload>();
    auto BConst = B.AsConstRef<CopyableVariantPayload>();
    REQUIRE(AConst);
    REQUIRE(BConst);
    REQUIRE(AConst->get().Value == 9);
    REQUIRE(BConst->get().Value == 3);
}

TEST_CASE("Borrowed variant copies continue to alias the source object")
{
    CopyableVariantPayload Payload{4};
    Variant A = Variant::FromRef(Payload);
    Variant B = A;

    REQUIRE(A.StorageKind() == Variant::EStorageKind::BorrowedMutable);
    REQUIRE(B.StorageKind() == Variant::EStorageKind::BorrowedMutable);

    auto BRef = B.AsRef<CopyableVariantPayload>();
    REQUIRE(BRef);
    BRef->get().Value = 11;

    auto AConst = A.AsConstRef<CopyableVariantPayload>();
    REQUIRE(AConst);
    REQUIRE(Payload.Value == 11);
    REQUIRE(AConst->get().Value == 11);
}

TEST_CASE("Shared move-only owned variants reject mutable detachment")
{
    Variant A = Variant::FromValue(MoveOnlyVariantPayload{7});
    Variant B = A;

    auto MutableRef = A.AsRef<MoveOnlyVariantPayload>();
    REQUIRE_FALSE(MutableRef);
    REQUIRE(MutableRef.error().Code == EErrorCode::InvalidArgument);

    auto BConst = B.AsConstRef<MoveOnlyVariantPayload>();
    REQUIRE(BConst);
    REQUIRE(BConst->get().Value == 7);
}
