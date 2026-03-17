#include <algorithm>
#include <array>

#include <catch2/catch_test_macros.hpp>

#include "GameFramework.hpp"
#include "GeneratedReflectionFixture.h"
#include "TypeRegistration.h"

using namespace SnAPI::GameFramework;

void SnAPI::GameFramework::Tests::GeneratedRpcNode::JumpImpl(const int Delta)
{
    Counter += Delta;
}

void SnAPI::GameFramework::Tests::GeneratedRpcNode::ShowDamageImpl(const int Delta)
{
    Counter += Delta * 10;
}

/**
 * @brief Minimal base type used to validate field/method registration.
 * @remarks Exercised by reflection inheritance and invocation tests.
 */
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

/**
 * @brief Derived test type used to validate inherited metadata traversal.
 */
struct TestDerived : public TestBase
{
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TestDerived";
    int m_extra = 0;

    int Sum(int Add) const
    {
        return m_value + m_extra + Add;
    }
};

namespace
{

[[nodiscard]] bool HasReflectedMethod(const TypeId& Type, const std::string_view Name)
{
    const auto Methods = TypeRegistry::Instance().CollectMethods(Type, true);
    return std::any_of(Methods.begin(), Methods.end(), [Name](const ReflectedMethodRef& Entry) {
        return Entry.Method != nullptr && Entry.Method->Name == Name;
    });
}

[[nodiscard]] bool HasReflectedField(const TypeInfo& Type, const std::string_view Name)
{
    return std::any_of(Type.Fields.begin(), Type.Fields.end(), [Name](const FieldInfo& Entry) {
        return Entry.Name == Name;
    });
}

[[nodiscard]] const FieldInfo* FindReflectedField(const TypeInfo& Type, const std::string_view Name)
{
    const auto It = std::find_if(Type.Fields.begin(), Type.Fields.end(), [Name](const FieldInfo& Entry) {
        return Entry.Name == Name;
    });
    return It != Type.Fields.end() ? &(*It) : nullptr;
}

} // namespace

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

/**
 * @brief Reflection flags test type for field/method flag assertions.
 */
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

TEST_CASE("Generated reflection codegen captures docs and parameter metadata")
{
    RegisterBuiltinTypes();

    auto FixtureTypeResult = StaticType<Tests::GeneratedReflectionFixture>();
    REQUIRE(FixtureTypeResult);

    const TypeInfo* FixtureInfo = TypeRegistry::Instance().Find(StaticTypeId<Tests::GeneratedReflectionFixture>());
    REQUIRE(FixtureInfo != nullptr);
    CHECK(FixtureInfo->DisplayName == "Generated Fixture");
    CHECK(FixtureInfo->Category == "Tests|Generated");
    CHECK(FixtureInfo->Doc == "Annotated fixture type used to validate libclang-driven reflection generation.");

    REQUIRE(FixtureInfo->Fields.size() == 4);
    CHECK(HasReflectedField(*FixtureInfo, "Value"));
    CHECK(HasReflectedField(*FixtureInfo, "HiddenValue"));
    CHECK(HasReflectedField(*FixtureInfo, "PreviewValue"));
    CHECK(HasReflectedField(*FixtureInfo, "PreviewBytes"));
    CHECK_FALSE(HasReflectedField(*FixtureInfo, "Unsupported"));
    const FieldInfo* ValueField = FindReflectedField(*FixtureInfo, "Value");
    REQUIRE(ValueField != nullptr);
    CHECK(ValueField->DisplayName == "Value");
    CHECK(ValueField->Category == "Tests|Generated|Fields");
    CHECK(ValueField->Doc == "Current value carried by the generated fixture.");
    CHECK(ValueField->Flags.Has(EFieldFlagBits::Replication));
    CHECK(ValueField->Flags.Has(EFieldFlagBits::Serialized));
    CHECK(ValueField->Flags.Has(EFieldFlagBits::ReplicationUnreliable));
    CHECK(ValueField->EditorFlags.Has(EFieldEditorFlagBits::Advanced));
    REQUIRE(ValueField->Value.Min.has_value());
    REQUIRE(ValueField->Value.Max.has_value());
    REQUIRE(ValueField->Value.Step.has_value());
    CHECK(*ValueField->Value.Min == -16.0);
    CHECK(*ValueField->Value.Max == 16.0);
    CHECK(*ValueField->Value.Step == 1.0);

    const FieldInfo* HiddenField = FindReflectedField(*FixtureInfo, "HiddenValue");
    REQUIRE(HiddenField != nullptr);
    CHECK(HiddenField->DisplayName == "Hidden Value");
    CHECK(HiddenField->Category == "Tests|Generated|Fields");
    CHECK(HiddenField->Doc == "Hidden value exposed through `EditHiddenValue()` / `GetHiddenValue()`.");
    CHECK(HiddenField->Flags.Has(EFieldFlagBits::Serialized));
    CHECK(HiddenField->EditorFlags.Has(EFieldEditorFlagBits::Hidden));

    const FieldInfo* PreviewField = FindReflectedField(*FixtureInfo, "PreviewValue");
    REQUIRE(PreviewField != nullptr);
    CHECK(PreviewField->DisplayName == "Preview Value");
    CHECK(PreviewField->Category == "Tests|Generated|Fields");
    CHECK(PreviewField->Doc ==
          "Compute a preview value derived from the current fixture state.\n\nTwice the current stored value.");
    CHECK(PreviewField->Flags.Has(EFieldFlagBits::Serialized));
    CHECK(PreviewField->EditorFlags.Has(EFieldEditorFlagBits::ReadOnly));

    const FieldInfo* PreviewBytesField = FindReflectedField(*FixtureInfo, "PreviewBytes");
    REQUIRE(PreviewBytesField != nullptr);
    CHECK(PreviewBytesField->DisplayName == "Preview Bytes");
    CHECK(PreviewBytesField->Category == "Tests|Generated|Fields");
    CHECK(PreviewBytesField->Doc == "Opaque preview bytes used to validate heavy-data field metadata.");
    CHECK(PreviewBytesField->Flags.Has(EFieldFlagBits::Serialized));
    CHECK(PreviewBytesField->EditorFlags.Has(EFieldEditorFlagBits::HeavyData));

    REQUIRE(FixtureInfo->Methods.size() == 1);
    CHECK(HasReflectedMethod(StaticTypeId<Tests::GeneratedReflectionFixture>(), "AddValue"));
    CHECK_FALSE(HasReflectedMethod(StaticTypeId<Tests::GeneratedReflectionFixture>(), "UnsupportedCall"));
    CHECK(FixtureInfo->Methods[0].DisplayName == "Add Value");
    CHECK(FixtureInfo->Methods[0].Category == "Tests|Generated|Methods");
    CHECK(FixtureInfo->Methods[0].Doc == "Add a delta to the fixture value.");
    CHECK_FALSE(FixtureInfo->Methods[0].Flags.Has(EMethodFlagBits::RpcReliable));
    CHECK_FALSE(FixtureInfo->Methods[0].Flags.Has(EMethodFlagBits::RpcNetServer));
    REQUIRE(FixtureInfo->Methods[0].Params.size() == 1);
    CHECK(FixtureInfo->Methods[0].Params[0].Name == "Delta");
    CHECK(FixtureInfo->Methods[0].Params[0].Doc == "Signed amount to add to the current value.");

    REQUIRE(FixtureInfo->Constructors.size() == 1);

    auto EnumTypeResult = StaticType<Tests::GeneratedReflectionMode>();
    REQUIRE(EnumTypeResult);

    const TypeInfo* EnumInfo = TypeRegistry::Instance().Find(StaticTypeId<Tests::GeneratedReflectionMode>());
    REQUIRE(EnumInfo != nullptr);
    CHECK(EnumInfo->IsEnum);
    CHECK(EnumInfo->DisplayName == "Generated Mode");
    CHECK(EnumInfo->Category == "Tests|Generated");
    CHECK(EnumInfo->Doc == "Example generated enum used to validate reflection codegen.");
    REQUIRE(EnumInfo->EnumValues.size() == 2);
    CHECK(EnumInfo->EnumValues[0].DisplayName == "Idle");
    CHECK(EnumInfo->EnumValues[0].Doc == "Idle state for the generated enum fixture.");
    CHECK(EnumInfo->EnumValues[1].DisplayName == "Active");
    CHECK(EnumInfo->EnumValues[1].Doc == "Active state for the generated enum fixture.");

    static constexpr std::string_view kGeneratedTemplateBoxTypeName =
        "SnAPI::GameFramework::Tests::GeneratedTemplateBox<SnAPI::GameFramework::Tests::GeneratedReflectionFixture>";

    const TypeInfo* TemplateBoxInfo =
        TypeRegistry::Instance().Find(TypeIdFromName(kGeneratedTemplateBoxTypeName));
    REQUIRE(TemplateBoxInfo != nullptr);
    CHECK(TemplateBoxInfo->DisplayName == "Generated Template Box");
    CHECK(TemplateBoxInfo->Category == "Tests|Generated|Template");
    CHECK(TemplateBoxInfo->Doc == "Annotated primary template used to validate reflected specialization expansion.");
    REQUIRE(TemplateBoxInfo->Fields.size() == 1);
    CHECK(TemplateBoxInfo->Fields[0].Name == "Label");
    CHECK(TemplateBoxInfo->Fields[0].Doc == "Stored label for the generated template box.");
    REQUIRE(TemplateBoxInfo->Methods.size() == 1);
    CHECK(TemplateBoxInfo->Methods[0].Name == "ReadLabel");
    CHECK(TemplateBoxInfo->Methods[0].DisplayName == "Read Label");
    CHECK(TemplateBoxInfo->Methods[0].Doc == "Read the stored label.");

    const TypeInfo* TemplateHostInfo = TypeRegistry::Instance().Find(StaticTypeId<Tests::GeneratedTemplateHost>());
    REQUIRE(TemplateHostInfo != nullptr);
    REQUIRE(TemplateHostInfo->Fields.size() == 1);
    CHECK(TemplateHostInfo->Fields[0].Name == "Box");
    CHECK(TemplateHostInfo->Fields[0].FieldType == TypeIdFromName(kGeneratedTemplateBoxTypeName));

    const TypeInfo* RpcNodeInfo = TypeRegistry::Instance().Find(StaticTypeId<Tests::GeneratedRpcNode>());
    REQUIRE(RpcNodeInfo != nullptr);
    CHECK(RpcNodeInfo->DisplayName == "Generated RPC Node");
    CHECK(RpcNodeInfo->Category == "Tests|Generated|RPC");
    CHECK(RpcNodeInfo->Methods.size() == 5);

    const auto RpcNodeMethods = TypeRegistry::Instance().CollectMethods(StaticTypeId<Tests::GeneratedRpcNode>(), true);
    REQUIRE(RpcNodeMethods.size() == 2);
    CHECK(std::ranges::any_of(RpcNodeMethods, [](const ReflectedMethodRef& Entry) {
        return Entry.Method && Entry.Method->Name == "Jump";
    }));
    CHECK(std::ranges::any_of(RpcNodeMethods, [](const ReflectedMethodRef& Entry) {
        return Entry.Method && Entry.Method->Name == "ShowDamage";
    }));

    Tests::GeneratedRpcNode RpcNode{};
    RpcNode.Jump(3);
    CHECK(RpcNode.Counter == 3);
    RpcNode.ShowDamage(2);
    CHECK(RpcNode.Counter == 23);
}

TEST_CASE("Reflected template families expose editor metadata automatically")
{
    RegisterBuiltinTypes();

    const TypeInfo* FieldFlagsInfo = TypeRegistry::Instance().Find(StaticTypeId<FieldFlags>());
    REQUIRE(FieldFlagsInfo != nullptr);
    CHECK(FieldFlagsInfo->EditorValueFamily == EEditorValueFamily::Flags);
    CHECK(FieldFlagsInfo->EditorValueTargetType == StaticTypeId<EFieldFlagBits>());

    const TypeInfo* FieldEditorFlagsInfo = TypeRegistry::Instance().Find(StaticTypeId<FieldEditorFlags>());
    REQUIRE(FieldEditorFlagsInfo != nullptr);
    CHECK(FieldEditorFlagsInfo->EditorValueFamily == EEditorValueFamily::Flags);
    CHECK(FieldEditorFlagsInfo->EditorValueTargetType == StaticTypeId<EFieldEditorFlagBits>());

    const TypeInfo* MethodFlagsInfo = TypeRegistry::Instance().Find(StaticTypeId<MethodFlags>());
    REQUIRE(MethodFlagsInfo != nullptr);
    CHECK(MethodFlagsInfo->EditorValueFamily == EEditorValueFamily::Flags);
    CHECK(MethodFlagsInfo->EditorValueTargetType == StaticTypeId<EMethodFlagBits>());

    const TypeInfo* SubClassInfo = TypeRegistry::Instance().Find(StaticTypeId<TSubClassOf<PawnBase>>());
    REQUIRE(SubClassInfo != nullptr);
    CHECK(SubClassInfo->EditorValueFamily == EEditorValueFamily::SubClassOf);
    CHECK(SubClassInfo->EditorValueTargetType == StaticTypeId<PawnBase>());
    REQUIRE(SubClassInfo->EditorValueAdapter.PopulateOptions != nullptr);
    REQUIRE(SubClassInfo->EditorValueAdapter.ReadSelectionLabel != nullptr);
    REQUIRE(SubClassInfo->EditorValueAdapter.WriteSelection != nullptr);

    const TypeInfo* AssetRefInfo = TypeRegistry::Instance().Find(StaticTypeId<TAssetRef<PawnBase>>());
    REQUIRE(AssetRefInfo != nullptr);
    CHECK(AssetRefInfo->EditorValueFamily == EEditorValueFamily::AssetRef);
    CHECK(AssetRefInfo->EditorValueTargetType == StaticTypeId<PawnBase>());
    REQUIRE(AssetRefInfo->EditorValueAdapter.PopulateOptions != nullptr);
    REQUIRE(AssetRefInfo->EditorValueAdapter.ReadSelectionLabel != nullptr);
    REQUIRE(AssetRefInfo->EditorValueAdapter.WriteSelection != nullptr);
}

TEST_CASE("Builtins expose enum metadata for editor-facing engine enums")
{
    RegisterBuiltinTypes();

    const TypeInfo* WorldKindInfo = TypeRegistry::Instance().Find(StaticTypeId<EWorldKind>());
    REQUIRE(WorldKindInfo != nullptr);
    CHECK(WorldKindInfo->IsEnum);
    CHECK(WorldKindInfo->EnumValues.size() == 3);

    const TypeInfo* FieldFlagBitsInfo = TypeRegistry::Instance().Find(StaticTypeId<EFieldFlagBits>());
    REQUIRE(FieldFlagBitsInfo != nullptr);
    CHECK(FieldFlagBitsInfo->IsEnum);

    const TypeInfo* FieldEditorFlagBitsInfo = TypeRegistry::Instance().Find(StaticTypeId<EFieldEditorFlagBits>());
    REQUIRE(FieldEditorFlagBitsInfo != nullptr);
    CHECK(FieldEditorFlagBitsInfo->IsEnum);

    const TypeInfo* MethodFlagBitsInfo = TypeRegistry::Instance().Find(StaticTypeId<EMethodFlagBits>());
    REQUIRE(MethodFlagBitsInfo != nullptr);
    CHECK(MethodFlagBitsInfo->IsEnum);

#if defined(SNAPI_GF_ENABLE_PHYSICS)
    const TypeInfo* CollisionBitsInfo = TypeRegistry::Instance().Find(StaticTypeId<ECollisionFilterBits>());
    REQUIRE(CollisionBitsInfo != nullptr);
    CHECK(CollisionBitsInfo->IsEnum);

    const TypeInfo* ShapeTypeInfo = TypeRegistry::Instance().Find(StaticTypeId<SnAPI::Physics::EShapeType>());
    REQUIRE(ShapeTypeInfo != nullptr);
    CHECK(ShapeTypeInfo->IsEnum);
#endif

#if defined(SNAPI_GF_ENABLE_INPUT)
    const TypeInfo* InputBackendInfo = TypeRegistry::Instance().Find(StaticTypeId<SnAPI::Input::EInputBackend>());
    REQUIRE(InputBackendInfo != nullptr);
    CHECK(InputBackendInfo->IsEnum);
#endif

#if defined(SNAPI_GF_ENABLE_NETWORKING)
    const TypeInfo* SessionRoleInfo = TypeRegistry::Instance().Find(StaticTypeId<SnAPI::Networking::ESessionRole>());
    REQUIRE(SessionRoleInfo != nullptr);
    CHECK(SessionRoleInfo->IsEnum);
#endif

#if defined(SNAPI_GF_ENABLE_RENDERER)
    const TypeInfo* ViewportPresetInfo = TypeRegistry::Instance().Find(StaticTypeId<ERenderViewportPassGraphPreset>());
    REQUIRE(ViewportPresetInfo != nullptr);
    CHECK(ViewportPresetInfo->IsEnum);
#endif
}

TEST_CASE("TypeId is a distinct reflected type from Uuid")
{
    RegisterBuiltinTypes();

    CHECK(StaticTypeId<TypeId>() != StaticTypeId<Uuid>());

    const TypeInfo* TypeIdInfo = TypeRegistry::Instance().Find(StaticTypeId<TypeId>());
    REQUIRE(TypeIdInfo != nullptr);
    CHECK(TypeIdInfo->Id == StaticTypeId<TypeId>());
    CHECK(TypeIdInfo->Name == TTypeNameV<TypeId>);

    const TypeInfo* UuidInfo = TypeRegistry::Instance().Find(StaticTypeId<Uuid>());
    REQUIRE(UuidInfo != nullptr);
    CHECK(UuidInfo->Id == StaticTypeId<Uuid>());
    CHECK(UuidInfo->Name == TTypeNameV<Uuid>);
}

TEST_CASE("Framework reflection exposes Conduit-facing node, component, and system methods")
{
    RegisterBuiltinTypes();

    CHECK(HasReflectedMethod(StaticTypeId<BaseNode>(), "Handle"));
    CHECK(HasReflectedMethod(StaticTypeId<BaseNode>(), "Parent"));
    CHECK(HasReflectedMethod(StaticTypeId<BaseNode>(), "PendingDestroy"));
    CHECK(HasReflectedMethod(StaticTypeId<BaseNode>(), "SetActive"));
    CHECK(HasReflectedMethod(StaticTypeId<BaseNode>(), "SetReplicated"));
    CHECK(HasReflectedMethod(StaticTypeId<BaseNode>(), "IsServer"));
    CHECK(HasReflectedMethod(StaticTypeId<BaseNode>(), "World"));

    CHECK(HasReflectedMethod(StaticTypeId<TransformComponent>(), "Owner"));
    CHECK(HasReflectedMethod(StaticTypeId<TransformComponent>(), "OwnerNode"));
    CHECK(HasReflectedMethod(StaticTypeId<TransformComponent>(), "World"));
    CHECK(HasReflectedMethod(StaticTypeId<TransformComponent>(), "Handle"));
    CHECK(HasReflectedMethod(StaticTypeId<TransformComponent>(), "SetActive"));
    CHECK(HasReflectedMethod(StaticTypeId<TransformComponent>(), "SetReplicated"));

    CHECK(HasReflectedMethod(StaticTypeId<IWorld>(), "FixedTickEnabled"));
    CHECK(HasReflectedMethod(StaticTypeId<IWorld>(), "FixedTickDeltaSeconds"));
    CHECK(HasReflectedMethod(StaticTypeId<IWorld>(), "FixedTickInterpolationAlpha"));

#if defined(SNAPI_GF_ENABLE_INPUT)
    CHECK(HasReflectedMethod(StaticTypeId<InputSystem>(), "Settings"));
#endif
#if defined(SNAPI_GF_ENABLE_UI)
    CHECK(HasReflectedMethod(StaticTypeId<UISystem>(), "Settings"));
#endif
#if defined(SNAPI_GF_ENABLE_AUDIO)
    CHECK(HasReflectedMethod(StaticTypeId<AudioSystem>(), "Update"));
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    CHECK(HasReflectedMethod(StaticTypeId<PhysicsSystem>(), "Settings"));
    CHECK(HasReflectedMethod(StaticTypeId<PhysicsSystem>(), "WorldToPhysicsPosition"));
    CHECK(HasReflectedMethod(StaticTypeId<PhysicsSystem>(), "PhysicsToWorldPosition"));
    CHECK(HasReflectedMethod(StaticTypeId<PhysicsSystem>(), "EnsureFloatingOriginNear"));
    CHECK(HasReflectedMethod(StaticTypeId<PhysicsSystem>(), "FloatingOriginWorld"));
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
    CHECK(HasReflectedMethod(StaticTypeId<RendererSystem>(), "Settings"));
    CHECK(HasReflectedMethod(StaticTypeId<RendererSystem>(), "UseDefaultRenderViewport"));
    CHECK(HasReflectedMethod(StaticTypeId<RendererSystem>(), "RegisterRenderViewportPassGraph"));
    CHECK(HasReflectedMethod(StaticTypeId<RendererSystem>(), "QueueText"));
    CHECK(HasReflectedMethod(StaticTypeId<RendererSystem>(), "HasDefaultFont"));
#endif
}

TEST_CASE("Framework bootstrap settings types are reflected for Conduit chaining")
{
    RegisterBuiltinTypes();

#if defined(SNAPI_GF_ENABLE_INPUT)
    {
        const TypeInfo* Info = TypeRegistry::Instance().Find(StaticTypeId<InputBootstrapSettings>());
        REQUIRE(Info != nullptr);
        CHECK(Info->Fields.size() >= 4);
    }
#endif
#if defined(SNAPI_GF_ENABLE_UI)
    {
        const TypeInfo* Info = TypeRegistry::Instance().Find(StaticTypeId<UIBootstrapSettings>());
        REQUIRE(Info != nullptr);
        CHECK(Info->Fields.size() >= 2);
    }
#endif
#if defined(SNAPI_GF_ENABLE_PHYSICS)
    {
        const TypeInfo* Info = TypeRegistry::Instance().Find(StaticTypeId<PhysicsBootstrapSettings>());
        REQUIRE(Info != nullptr);
        CHECK(Info->Fields.size() >= 8);
    }
#endif
#if defined(SNAPI_GF_ENABLE_RENDERER)
    {
        const TypeInfo* Info = TypeRegistry::Instance().Find(StaticTypeId<RendererBootstrapSettings>());
        REQUIRE(Info != nullptr);
        CHECK(Info->Fields.size() >= 10);
    }
#endif
}

TEST_CASE("PrettyReflectedTypeName strips namespaces recursively for template spellings")
{
    CHECK(PrettyReflectedTypeName("SnAPI::GameFramework::Vec3") == "Vec3");
    CHECK(PrettyReflectedTypeName("SnAPI::GameFramework::TSubClassOf<SnAPI::GameFramework::PawnBase>") == "TSubClassOf<PawnBase>");
    CHECK(PrettyReflectedTypeName("const SnAPI::GameFramework::TAssetRef<SnAPI::GameFramework::Vec3>*")
          == "const TAssetRef<Vec3>*");
}

#if defined(SNAPI_GF_ENABLE_AUDIO)
TEST_CASE("AudioSourceComponent exposes reflected RPC endpoints")
{
    RegisterBuiltinTypes();

    auto* Info = TypeRegistry::Instance().Find(StaticTypeId<AudioSourceComponent>());
    REQUIRE(Info);

    bool HasPlay = false;
    bool HasStop = false;
    bool HasLegacyPlayServer = false;
    bool HasLegacyPlayClient = false;
    bool HasLegacyStopServer = false;
    bool HasLegacyStopClient = false;
    bool HasHiddenAudioServerRpc = false;
    bool HasHiddenAudioMulticastRpc = false;
    bool HasSetActive = false;
    bool HasLegacySetActiveServer = false;
    bool HasLegacySetActiveClient = false;
    bool HasHiddenListenerServerRpc = false;
    bool HasHiddenListenerMulticastRpc = false;

    for (const auto& Method : Info->Methods)
    {
        if (Method.Name == "Play")
        {
            HasPlay = true;
        }
        else if (Method.Name == "Stop")
        {
            HasStop = true;
        }
        else if (Method.Name == "PlayServer")
        {
            HasLegacyPlayServer = true;
        }
        else if (Method.Name == "PlayClient")
        {
            HasLegacyPlayClient = true;
        }
        else if (Method.Name == "StopServer")
        {
            HasLegacyStopServer = true;
        }
        else if (Method.Name == "StopClient")
        {
            HasLegacyStopClient = true;
        }

        if (Method.Flags.Has(EMethodFlagBits::HiddenGenerated))
        {
            HasHiddenAudioServerRpc |= Method.Flags.Has(EMethodFlagBits::RpcReliable)
                && Method.Flags.Has(EMethodFlagBits::RpcNetServer);
            HasHiddenAudioMulticastRpc |= Method.Flags.Has(EMethodFlagBits::RpcReliable)
                && Method.Flags.Has(EMethodFlagBits::RpcNetMulticast);
        }
    }

    REQUIRE(HasPlay);
    REQUIRE(HasStop);
    REQUIRE_FALSE(HasLegacyPlayServer);
    REQUIRE_FALSE(HasLegacyPlayClient);
    REQUIRE_FALSE(HasLegacyStopServer);
    REQUIRE_FALSE(HasLegacyStopClient);
    REQUIRE(HasHiddenAudioServerRpc);
    REQUIRE(HasHiddenAudioMulticastRpc);

    auto* ListenerInfo = TypeRegistry::Instance().Find(StaticTypeId<AudioListenerComponent>());
    REQUIRE(ListenerInfo);
    for (const auto& Method : ListenerInfo->Methods)
    {
        if (Method.Name == "SetActive")
        {
            HasSetActive = true;
        }
        else if (Method.Name == "SetActiveServer")
        {
            HasLegacySetActiveServer = true;
        }
        else if (Method.Name == "SetActiveClient")
        {
            HasLegacySetActiveClient = true;
        }

        if (Method.Flags.Has(EMethodFlagBits::HiddenGenerated))
        {
            HasHiddenListenerServerRpc |= Method.Flags.Has(EMethodFlagBits::RpcReliable)
                && Method.Flags.Has(EMethodFlagBits::RpcNetServer);
            HasHiddenListenerMulticastRpc |= Method.Flags.Has(EMethodFlagBits::RpcReliable)
                && Method.Flags.Has(EMethodFlagBits::RpcNetMulticast);
        }
    }

    REQUIRE(HasSetActive);
    REQUIRE_FALSE(HasLegacySetActiveServer);
    REQUIRE_FALSE(HasLegacySetActiveClient);
    REQUIRE(HasHiddenListenerServerRpc);
    REQUIRE(HasHiddenListenerMulticastRpc);
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

#if defined(SNAPI_GF_ENABLE_RENDERER)
TEST_CASE("Renderer components expose replicated settings fields")
{
    RegisterBuiltinTypes();

    auto* CameraInfo = TypeRegistry::Instance().Find(StaticTypeId<CameraComponent>());
    REQUIRE(CameraInfo);
    bool CameraHasReplicatedSettings = false;
    for (const auto& Field : CameraInfo->Fields)
    {
        if (Field.Name == "Settings")
        {
            CameraHasReplicatedSettings = Field.Flags.Has(EFieldFlagBits::Replication);
            break;
        }
    }
    REQUIRE(CameraHasReplicatedSettings);

    auto* StaticMeshInfo = TypeRegistry::Instance().Find(StaticTypeId<StaticMeshComponent>());
    REQUIRE(StaticMeshInfo);
    bool StaticMeshHasReplicatedSettings = false;
    for (const auto& Field : StaticMeshInfo->Fields)
    {
        if (Field.Name == "Settings")
        {
            StaticMeshHasReplicatedSettings = Field.Flags.Has(EFieldFlagBits::Replication);
            break;
        }
    }
    REQUIRE(StaticMeshHasReplicatedSettings);

    auto* SkeletalMeshInfo = TypeRegistry::Instance().Find(StaticTypeId<SkeletalMeshComponent>());
    REQUIRE(SkeletalMeshInfo);
    bool SkeletalHasReplicatedSettings = false;
    for (const auto& Field : SkeletalMeshInfo->Fields)
    {
        if (Field.Name == "Settings")
        {
            SkeletalHasReplicatedSettings = Field.Flags.Has(EFieldFlagBits::Replication);
            break;
        }
    }
    REQUIRE(SkeletalHasReplicatedSettings);
}
#endif
