#include "TypeRegistration.h"

#include "BaseNode.h"
#include "BuiltinTypes.h"
#include "Level.h"
#include "NodeGraph.h"
#include "Relevance.h"
#include "Serialization.h"
#include "ScriptComponent.h"
#include "TransformComponent.h"
#include "TypeAutoRegistration.h"
#if defined(SNAPI_GF_ENABLE_AUDIO)
#include "AudioListenerComponent.h"
#include "AudioSourceComponent.h"
#endif
#include "TypeRegistry.h"
#include "World.h"

namespace SnAPI::GameFramework
{

SNAPI_REFLECT_TYPE(BaseNode, (TTypeBuilder<BaseNode>(BaseNode::kTypeName)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(NodeGraph, (TTypeBuilder<NodeGraph>(NodeGraph::kTypeName)
    .Base<BaseNode>()
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(Level, (TTypeBuilder<Level>(Level::kTypeName)
    .Base<NodeGraph>()
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(World, (TTypeBuilder<World>(World::kTypeName)
    .Base<NodeGraph>()
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(TransformComponent, (TTypeBuilder<TransformComponent>(TransformComponent::kTypeName)
    .Field("Position", &TransformComponent::Position)
    .Field("Rotation", &TransformComponent::Rotation)
    .Field("Scale", &TransformComponent::Scale)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(RelevanceComponent, (TTypeBuilder<RelevanceComponent>(RelevanceComponent::kTypeName)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(ScriptComponent, (TTypeBuilder<ScriptComponent>(ScriptComponent::kTypeName)
    .Field("ScriptModule", &ScriptComponent::ScriptModule)
    .Field("ScriptType", &ScriptComponent::ScriptType)
    .Field("Instance", &ScriptComponent::Instance)
    .Constructor<>()
    .Register()));

#if defined(SNAPI_GF_ENABLE_AUDIO)

SNAPI_REFLECT_TYPE(AudioSourceComponent::Settings, (TTypeBuilder<AudioSourceComponent::Settings>(AudioSourceComponent::Settings::kTypeName)
    .Field("SoundPath", &AudioSourceComponent::Settings::SoundPath, EFieldFlagBits::Replication)
    .Field("Streaming", &AudioSourceComponent::Settings::Streaming)
    .Field("AutoPlay", &AudioSourceComponent::Settings::AutoPlay)
    .Field("Looping", &AudioSourceComponent::Settings::Looping)
    .Field("Volume", &AudioSourceComponent::Settings::Volume)
    .Field("SpatialGain", &AudioSourceComponent::Settings::SpatialGain)
    .Field("MinDistance", &AudioSourceComponent::Settings::MinDistance)
    .Field("MaxDistance", &AudioSourceComponent::Settings::MaxDistance)
    .Field("Rolloff", &AudioSourceComponent::Settings::Rolloff)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(AudioSourceComponent, (TTypeBuilder<AudioSourceComponent>(AudioSourceComponent::kTypeName)
    .Field("Settings",
           &AudioSourceComponent::EditSettings,
           &AudioSourceComponent::GetSettings,
           EFieldFlagBits::Replication)
    .Method("PlayServer",
            &AudioSourceComponent::PlayServer,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("PlayClient",
            &AudioSourceComponent::PlayClient,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetMulticast)
    .Method("StopServer",
            &AudioSourceComponent::StopServer,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("StopClient",
            &AudioSourceComponent::StopClient,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetMulticast)
    .Constructor<>()
    .Register()));

SNAPI_REFLECT_TYPE(AudioListenerComponent, (TTypeBuilder<AudioListenerComponent>(AudioListenerComponent::kTypeName)
    .Field("Active", &AudioListenerComponent::EditActive, &AudioListenerComponent::GetActive)
    .Method("SetActiveServer",
            &AudioListenerComponent::SetActiveServer,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetServer)
    .Method("SetActiveClient",
            &AudioListenerComponent::SetActiveClient,
            EMethodFlagBits::RpcReliable | EMethodFlagBits::RpcNetMulticast)
    .Constructor<>()
    .Register()));

#endif // SNAPI_GF_ENABLE_AUDIO

void RegisterBuiltinTypes()
{
    TypeInfo VoidInfo;
    VoidInfo.Id = TypeIdFromName("void");
    VoidInfo.Name = "void";
    VoidInfo.Size = 0;
    VoidInfo.Align = 0;
    TypeRegistry::Instance().Register(std::move(VoidInfo));

    auto RegisterPlain = [](const char* Name, size_t Size, size_t Align) {
        TypeInfo Info;
        Info.Id = TypeIdFromName(Name);
        Info.Name = Name;
        Info.Size = Size;
        Info.Align = Align;
        (void)TypeRegistry::Instance().Register(std::move(Info));
    };

    RegisterPlain(TTypeNameV<bool>, sizeof(bool), alignof(bool));
    RegisterPlain(TTypeNameV<int>, sizeof(int), alignof(int));
    RegisterPlain(TTypeNameV<unsigned int>, sizeof(unsigned int), alignof(unsigned int));
    RegisterPlain(TTypeNameV<std::uint64_t>, sizeof(std::uint64_t), alignof(std::uint64_t));
    RegisterPlain(TTypeNameV<float>, sizeof(float), alignof(float));
    RegisterPlain(TTypeNameV<double>, sizeof(double), alignof(double));
    RegisterPlain(TTypeNameV<std::string>, sizeof(std::string), alignof(std::string));
    RegisterPlain(TTypeNameV<std::vector<uint8_t>>, sizeof(std::vector<uint8_t>), alignof(std::vector<uint8_t>));
    RegisterPlain(TTypeNameV<Uuid>, sizeof(Uuid), alignof(Uuid));
    RegisterPlain(TTypeNameV<Vec3>, sizeof(Vec3), alignof(Vec3));
    RegisterPlain(TTypeNameV<NodeHandle>, sizeof(NodeHandle), alignof(NodeHandle));
    RegisterPlain(TTypeNameV<ComponentHandle>, sizeof(ComponentHandle), alignof(ComponentHandle));

    RegisterSerializationDefaults();
}

} // namespace SnAPI::GameFramework
