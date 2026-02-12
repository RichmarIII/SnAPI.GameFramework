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

#if defined(SNAPI_GF_ENABLE_AUDIO)
TEST_CASE("AudioSourceComponent exposes reflected RPC endpoints")
{
    RegisterBuiltinTypes();

    auto* Info = TypeRegistry::Instance().Find(StaticTypeId<AudioSourceComponent>());
    REQUIRE(Info);

    bool HasPlayServer = false;
    bool HasPlayClient = false;
    bool HasStopServer = false;
    bool HasStopClient = false;
    bool HasSetActiveServer = false;
    bool HasSetActiveClient = false;

    for (const auto& Method : Info->Methods)
    {
        if (Method.Name == "PlayServer")
        {
            HasPlayServer = Method.Flags.Has(EMethodFlagBits::RpcReliable)
                && Method.Flags.Has(EMethodFlagBits::RpcNetServer);
        }
        else if (Method.Name == "PlayClient")
        {
            HasPlayClient = Method.Flags.Has(EMethodFlagBits::RpcReliable)
                && Method.Flags.Has(EMethodFlagBits::RpcNetMulticast);
        }
        else if (Method.Name == "StopServer")
        {
            HasStopServer = Method.Flags.Has(EMethodFlagBits::RpcReliable)
                && Method.Flags.Has(EMethodFlagBits::RpcNetServer);
        }
        else if (Method.Name == "StopClient")
        {
            HasStopClient = Method.Flags.Has(EMethodFlagBits::RpcReliable)
                && Method.Flags.Has(EMethodFlagBits::RpcNetMulticast);
        }
    }

    REQUIRE(HasPlayServer);
    REQUIRE(HasPlayClient);
    REQUIRE(HasStopServer);
    REQUIRE(HasStopClient);

    auto* ListenerInfo = TypeRegistry::Instance().Find(StaticTypeId<AudioListenerComponent>());
    REQUIRE(ListenerInfo);
    for (const auto& Method : ListenerInfo->Methods)
    {
        if (Method.Name == "SetActiveServer")
        {
            HasSetActiveServer = Method.Flags.Has(EMethodFlagBits::RpcReliable)
                && Method.Flags.Has(EMethodFlagBits::RpcNetServer);
        }
        else if (Method.Name == "SetActiveClient")
        {
            HasSetActiveClient = Method.Flags.Has(EMethodFlagBits::RpcReliable)
                && Method.Flags.Has(EMethodFlagBits::RpcNetMulticast);
        }
    }

    REQUIRE(HasSetActiveServer);
    REQUIRE(HasSetActiveClient);
}

TEST_CASE("AudioSourceComponent settings fields are marked for replication")
{
    RegisterBuiltinTypes();

    auto* AudioInfo = TypeRegistry::Instance().Find(StaticTypeId<AudioSourceComponent>());
    REQUIRE(AudioInfo);

    bool HasReplicatedSettingsField = false;
    for (const auto& Field : AudioInfo->Fields)
    {
        if (Field.Name == "Settings")
        {
            HasReplicatedSettingsField = Field.Flags.Has(EFieldFlagBits::Replication);
            break;
        }
    }
    REQUIRE(HasReplicatedSettingsField);

    auto* SettingsInfo = TypeRegistry::Instance().Find(StaticTypeId<AudioSourceComponent::Settings>());
    REQUIRE(SettingsInfo);

    bool HasReplicatedSoundPath = false;
    bool HasReplicatedStreaming = false;
    for (const auto& Field : SettingsInfo->Fields)
    {
        if (Field.Name == "SoundPath")
        {
            HasReplicatedSoundPath = Field.Flags.Has(EFieldFlagBits::Replication);
        }
        else if (Field.Name == "Streaming")
        {
            HasReplicatedStreaming = Field.Flags.Has(EFieldFlagBits::Replication);
        }
    }
    REQUIRE(HasReplicatedSoundPath);
    REQUIRE_FALSE(HasReplicatedStreaming);
}
#endif
