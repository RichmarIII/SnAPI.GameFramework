#include <array>

#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"
#include "TypeRegistration.h"

using namespace SnAPI::GameFramework;

struct TestBase
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TestBase";
    int m_value = 0;

    int Value() const
    {
        return m_value;
    }

    void Value(int InValue)
    {
        m_value = InValue;
    }
};

struct TestDerived : public TestBase
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TestDerived";
    int m_extra = 0;

    int Sum(int Add) const
    {
        return m_value + m_extra + Add;
    }
};

TEST_CASE("Reflection registers types and supports inheritance")
{
    RegisterBuiltinTypes();

    (void)TTypeBuilder<TestBase>(TestBase::kTypeName)
        .Field("Value", &TestBase::m_value)
        .Method("Value", static_cast<int (TestBase::*)() const>(&TestBase::Value))
        .Method("Value", static_cast<void (TestBase::*)(int)>(&TestBase::Value))
        .Constructor<>()
        .Register();

    (void)TTypeBuilder<TestDerived>(TestDerived::kTypeName)
        .Base<TestBase>()
        .Field("Extra", &TestDerived::m_extra)
        .Method("Sum", &TestDerived::Sum)
        .Constructor<>()
        .Register();

    const auto BaseId = TypeIdFromName(TestBase::kTypeName);
    const auto DerivedId = TypeIdFromName(TestDerived::kTypeName);
    REQUIRE(TypeRegistry::Instance().IsA(DerivedId, BaseId));

    auto* DerivedInfo = TypeRegistry::Instance().Find(DerivedId);
    REQUIRE(DerivedInfo);
    REQUIRE_FALSE(DerivedInfo->Fields.empty());

    TestDerived Instance;
    Instance.m_value = 3;
    Instance.m_extra = 7;

    auto FieldResult = DerivedInfo->Fields[0].Getter(&Instance);
    REQUIRE(FieldResult);
    REQUIRE(FieldResult->AsConstRef<int>().value() == 7);

    std::array<Variant, 1> Args{Variant::FromValue(5)};
    auto MethodHandle = DerivedInfo->Methods[0].Invoke(&Instance, Args);
    REQUIRE(MethodHandle);
    REQUIRE(MethodHandle->AsConstRef<int>().value() == 15);

    auto CtorResult = DerivedInfo->Constructors[0].Construct({});
    REQUIRE(CtorResult);
    auto Ptr = std::static_pointer_cast<TestDerived>(CtorResult.value());
    REQUIRE(Ptr != nullptr);
}

struct FlaggedType
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::FlaggedType";
    int Replicated = 0;

    void RpcCall(int) {}
};

TEST_CASE("Reflection records field and method flags")
{
    RegisterBuiltinTypes();

    (void)TTypeBuilder<FlaggedType>(FlaggedType::kTypeName)
        .Field("Replicated", &FlaggedType::Replicated, EFieldFlagBits::Replication)
        .Method("RpcCall", &FlaggedType::RpcCall,
                EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
        .Constructor<>()
        .Register();

    auto* Info = TypeRegistry::Instance().Find(TypeIdFromName(FlaggedType::kTypeName));
    REQUIRE(Info);
    REQUIRE(Info->Fields.size() == 1);
    REQUIRE(Info->Methods.size() == 1);
    REQUIRE(Info->Fields[0].Flags.Has(EFieldFlagBits::Replication));
    REQUIRE(Info->Methods[0].Flags.Has(EMethodFlagBits::RpcReliable));
    REQUIRE(Info->Methods[0].Flags.Has(EMethodFlagBits::RpcNetServer));
}
