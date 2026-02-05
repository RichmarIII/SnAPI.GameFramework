#include "TypeRegistration.h"

#include "BaseNode.h"
#include "BuiltinTypes.h"
#include "Level.h"
#include "NodeGraph.h"
#include "Relevance.h"
#include "Serialization.h"
#include "ScriptComponent.h"
#include "TransformComponent.h"
#include "TypeBuilder.h"
#include "TypeRegistry.h"
#include "World.h"

namespace SnAPI::GameFramework
{

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

    (void)TTypeBuilder<BaseNode>(BaseNode::kTypeName)
        .Constructor<>()
        .Register();
    (void)TTypeBuilder<NodeGraph>(NodeGraph::kTypeName)
        .Base<BaseNode>()
        .Constructor<>()
        .Register();
    (void)TTypeBuilder<Level>(Level::kTypeName)
        .Base<NodeGraph>()
        .Constructor<>()
        .Register();
    (void)TTypeBuilder<World>(World::kTypeName)
        .Base<NodeGraph>()
        .Constructor<>()
        .Register();
    (void)TTypeBuilder<TransformComponent>(TransformComponent::kTypeName)
        .Field("Position", &TransformComponent::Position)
        .Field("Rotation", &TransformComponent::Rotation)
        .Field("Scale", &TransformComponent::Scale)
        .Constructor<>()
        .Register();
    (void)TTypeBuilder<RelevanceComponent>(RelevanceComponent::kTypeName)
        .Constructor<>()
        .Register();
    (void)TTypeBuilder<ScriptComponent>(ScriptComponent::kTypeName)
        .Field("ScriptModule", &ScriptComponent::ScriptModule)
        .Field("ScriptType", &ScriptComponent::ScriptType)
        .Field("Instance", &ScriptComponent::Instance)
        .Constructor<>()
        .Register();

    RegisterSerializationDefaults();
}

} // namespace SnAPI::GameFramework
