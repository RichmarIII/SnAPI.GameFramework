#include "NodeAsset.h"

#include <new>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "AuthoredAssetCereal.h"
#include "NodeCast.h"
#include "TypeAutoRegistration.h"
#include "TypeAutoRegistry.h"
#include "TypeRegistry.h"
#include "WorldEcsRuntime.h"

namespace SnAPI::GameFramework
{
SNAPI_REFLECT_TYPE(
    NodeAsset,
    (TTypeBuilder<NodeAsset>(NodeAsset::kTypeName)
        .Base<IAsset>()
        .Field("Name", &NodeAsset::Name, EFieldFlagBits::Serialized)
        .Field("Nodes", &NodeAsset::Nodes, EFieldFlagBits::Serialized)
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    LevelAsset,
    (TTypeBuilder<LevelAsset>(LevelAsset::kTypeName)
        .Base<NodeAsset>()
        .Constructor<>()
        .Register()));

SNAPI_REFLECT_TYPE(
    WorldAsset,
    (TTypeBuilder<WorldAsset>(WorldAsset::kTypeName)
        .Base<NodeAsset>()
        .Constructor<>()
        .Register()));

namespace
{
using cereal::make_nvp;

class ScratchValue
{
public:
    explicit ScratchValue(const TypeInfo& Info)
        : m_info(&Info)
        , m_storage(::operator new(Info.Size, std::align_val_t(Info.Align)))
    {
    }

    ~ScratchValue()
    {
        Reset();
    }

    ScratchValue(const ScratchValue&) = delete;
    ScratchValue& operator=(const ScratchValue&) = delete;

    [[nodiscard]] void* Data() const
    {
        return m_storage;
    }

    void MarkConstructed()
    {
        m_constructed = true;
    }

    void Reset()
    {
        if (m_storage)
        {
            if (m_constructed && m_info && m_info->RuntimeOps && m_info->RuntimeOps->Destroy)
            {
                m_info->RuntimeOps->Destroy(m_storage);
            }
            ::operator delete(m_storage, std::align_val_t(m_info ? m_info->Align : alignof(std::max_align_t)));
            m_storage = nullptr;
        }
        m_constructed = false;
        m_info = nullptr;
    }

private:
    const TypeInfo* m_info = nullptr;
    void* m_storage = nullptr;
    bool m_constructed = false;
};

[[nodiscard]] const FieldInfo* FindFieldByName(const TypeId& OwnerType, std::string_view Name)
{
    for (const ReflectedFieldRef& Ref : TypeRegistry::Instance().CollectFields(OwnerType, true))
    {
        if (Ref.Field && Ref.Field->Name == Name)
        {
            return Ref.Field;
        }
    }
    return nullptr;
}

[[nodiscard]] BaseNode* ResolveChildNode(NodeHandle Handle, IWorld* WorldRef)
{
    if (WorldRef)
    {
        if (BaseNode* Node = WorldRef->BorrowedNode(Handle))
        {
            return Node;
        }
    }

    if (BaseNode* Node = Handle.Borrowed())
    {
        return Node;
    }

    if (!Handle.Id.is_nil())
    {
        return Handle.BorrowedSlowByUuid();
    }

    return nullptr;
}

[[nodiscard]] TExpected<Conduit::SerializedValue> CaptureFieldValue(const FieldInfo& Field, const void* Instance)
{
    if (Field.FieldType == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Reflected field type is missing"));
    }

    if (Field.ConstPointer)
    {
        const void* FieldPtr = Field.ConstPointer(Instance);
        if (!FieldPtr)
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Field pointer is null"));
        }

        Conduit::SerializedValue Value{};
        Value.Type = Field.FieldType;
        auto SerializeResult = SerializeReflectedValue(Field.FieldType, FieldPtr, Value.Bytes);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error());
        }
        return Value;
    }

    if (Field.ViewGetter)
    {
        auto ViewResult = Field.ViewGetter(const_cast<void*>(Instance));
        if (!ViewResult)
        {
            return std::unexpected(ViewResult.error());
        }
        if (!ViewResult->UnsafeBorrowed())
        {
            return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Field view payload is null"));
        }

        Conduit::SerializedValue Value{};
        Value.Type = Field.FieldType;
        auto SerializeResult = SerializeReflectedValue(Field.FieldType, ViewResult->UnsafeBorrowed(), Value.Bytes);
        if (!SerializeResult)
        {
            return std::unexpected(SerializeResult.error());
        }
        return Value;
    }

    if (Field.Getter)
    {
        auto GetterResult = Field.Getter(const_cast<void*>(Instance));
        if (!GetterResult)
        {
            return std::unexpected(GetterResult.error());
        }
        return Conduit::SerializedValue::FromVariant(*GetterResult);
    }

    return std::unexpected(MakeError(EErrorCode::NotFound, "Field does not expose a readable reflected value path"));
}

[[nodiscard]] bool CanAuthorField(const FieldInfo& Field)
{
    if (Field.IsConst)
    {
        return false;
    }

    if (Field.Setter || Field.RawSetter || Field.MutablePointer)
    {
        return true;
    }

    // Non-const view getters can still expose mutable backing storage.
    if (Field.ViewGetter)
    {
        return true;
    }

    return false;
}

Result AssignConstructedValue(const TypeInfo& ValueType, const void* SourceValue, void* DestValue)
{
    if (!DestValue)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Destination field storage is null"));
    }
    if (!ValueType.RuntimeOps)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Field type has no runtime ops"));
    }
    if (ValueType.RuntimeOps->CopyAssign)
    {
        ValueType.RuntimeOps->CopyAssign(SourceValue, DestValue);
        return Ok();
    }
    if (ValueType.RuntimeOps->MoveAssign)
    {
        ValueType.RuntimeOps->MoveAssign(const_cast<void*>(SourceValue), DestValue);
        return Ok();
    }
    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Field type is not assignable"));
}

Result ApplyFieldValue(const FieldInfo& Field, void* Instance, const Conduit::SerializedValue& Value)
{
    if (Value.Type == TypeId{} || Value.Bytes.empty())
    {
        return Ok();
    }

    if (Field.FieldType != Value.Type)
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Serialized field type does not match reflected field type"));
    }

    const TypeInfo* FieldTypeInfo = TypeRegistry::Instance().Find(Field.FieldType);
    if (!FieldTypeInfo || !FieldTypeInfo->RuntimeOps)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Reflected field type is not registered"));
    }

    ScratchValue Temporary(*FieldTypeInfo);
    auto ConstructResult = Value.ConstructInto(Temporary.Data());
    if (!ConstructResult)
    {
        return std::unexpected(ConstructResult.error());
    }
    Temporary.MarkConstructed();

    if (Field.RawSetter)
    {
        return Field.RawSetter(Instance, Temporary.Data());
    }

    if (Field.MutablePointer)
    {
        if (void* FieldPtr = Field.MutablePointer(Instance))
        {
            return AssignConstructedValue(*FieldTypeInfo, Temporary.Data(), FieldPtr);
        }
    }

    if (Field.ViewGetter)
    {
        auto ViewResult = Field.ViewGetter(Instance);
        if (ViewResult)
        {
            if (ViewResult->IsConst())
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Cannot mutate const reflected field"));
            }
            if (void* FieldPtr = ViewResult->UnsafeBorrowedMutable())
            {
                return AssignConstructedValue(*FieldTypeInfo, Temporary.Data(), FieldPtr);
            }
        }
    }

    if (Field.Getter)
    {
        auto GetterResult = Field.Getter(Instance);
        if (GetterResult)
        {
            if (GetterResult->StorageKind() == Variant::EStorageKind::BorrowedConst)
            {
                return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Cannot mutate const reflected field"));
            }
            if (void* FieldPtr = GetterResult->UnsafeBorrowedMutable())
            {
                return AssignConstructedValue(*FieldTypeInfo, Temporary.Data(), FieldPtr);
            }
        }
    }

    return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Field does not expose a writable reflected value path"));
}

TExpected<std::vector<NodeFieldAsset>> CaptureFields(const TypeId& OwnerType, const void* Instance)
{
    std::vector<NodeFieldAsset> Result{};
    for (const ReflectedFieldRef& Ref : TypeRegistry::Instance().CollectFields(OwnerType, true))
    {
        if (!Ref.Field)
        {
            continue;
        }
        if (!CanAuthorField(*Ref.Field))
        {
            continue;
        }

        auto ValueResult = CaptureFieldValue(*Ref.Field, Instance);
        if (!ValueResult)
        {
            return std::unexpected(ValueResult.error());
        }

        Result.push_back(NodeFieldAsset{
            .Name = Ref.Field->Name,
            .Value = std::move(*ValueResult),
        });
    }
    return Result;
}

TExpected<NodeComponentAsset> CaptureComponentAsset(const BaseNode& OwnerNode, const TypeId& ComponentType)
{
    IWorld* WorldRef = OwnerNode.World();
    if (!WorldRef)
    {
        return std::unexpected(MakeError(EErrorCode::NotReady, "Node is not bound to a world"));
    }
    if (OwnerNode.Handle().IsNull())
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node handle is invalid"));
    }

    NodeHandle OwnerHandle = OwnerNode.Handle();
    const void* ComponentPtr = WorldRef->BorrowedComponent(OwnerHandle, ComponentType);
    if (!ComponentPtr)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Component instance could not be resolved"));
    }

    NodeComponentAsset Asset{};
    Asset.Type = ComponentType;
    if (const auto* Component = static_cast<const BaseComponent*>(ComponentPtr))
    {
        Asset.Id = Component->Id();
    }

    auto FieldsResult = CaptureFields(ComponentType, ComponentPtr);
    if (!FieldsResult)
    {
        return std::unexpected(FieldsResult.error());
    }
    Asset.Fields = std::move(*FieldsResult);
    return Asset;
}

TExpected<NodeObjectAsset> CaptureNodeObject(const BaseNode& NodeRef)
{
    NodeObjectAsset Asset{};
    Asset.Id = NodeRef.Id();
    Asset.Type = NodeRef.TypeKey();
    Asset.Name = NodeRef.Name();
    Asset.Active = NodeRef.Active();

    auto FieldsResult = CaptureFields(NodeRef.TypeKey(), &NodeRef);
    if (!FieldsResult)
    {
        return std::unexpected(FieldsResult.error());
    }
    Asset.Fields = std::move(*FieldsResult);

    Asset.Components.reserve(NodeRef.ComponentTypes().size());
    for (const TypeId& ComponentType : NodeRef.ComponentTypes())
    {
        auto ComponentResult = CaptureComponentAsset(NodeRef, ComponentType);
        if (!ComponentResult)
        {
            return std::unexpected(ComponentResult.error());
        }
        Asset.Components.push_back(std::move(*ComponentResult));
    }

    Asset.Children.reserve(NodeRef.Children().size());
    for (const NodeHandle& ChildHandle : NodeRef.Children())
    {
        BaseNode* ChildNode = ResolveChildNode(ChildHandle, NodeRef.World());
        if (!ChildNode)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Child node could not be resolved during source asset capture"));
        }
        if (ChildNode->EditorTransient())
        {
            continue;
        }

        auto ChildResult = CaptureNodeObject(*ChildNode);
        if (!ChildResult)
        {
            return std::unexpected(ChildResult.error());
        }
        Asset.Children.push_back(std::move(*ChildResult));
    }

    return Asset;
}

Result ApplyFields(const TypeId& OwnerType, void* Instance, const std::vector<NodeFieldAsset>& Fields)
{
    for (const NodeFieldAsset& FieldValue : Fields)
    {
        const FieldInfo* Field = FindFieldByName(OwnerType, FieldValue.Name);
        if (!Field)
        {
            continue;
        }

        const Result ApplyResult = ApplyFieldValue(*Field, Instance, FieldValue.Value);
        if (!ApplyResult)
        {
            continue;
        }
    }
    return Ok();
}

TExpected<NodeHandle> MaterializeNodeObject(const NodeObjectAsset& Asset,
                                            World& WorldRef,
                                            const NodeHandle& Parent)
{
    if (Asset.Type == TypeId{})
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Node asset type is missing"));
    }

    const TypeInfo* NodeTypeInfo = TypeRegistry::Instance().Find(Asset.Type);
    if (!NodeTypeInfo)
    {
        return std::unexpected(MakeError(EErrorCode::NotFound, "Node asset type is not registered"));
    }
    if (!TypeRegistry::Instance().IsA(Asset.Type, StaticTypeId<BaseNode>()))
    {
        return std::unexpected(MakeError(EErrorCode::TypeMismatch, "Authored node asset references a non-node type"));
    }

    TExpected<NodeHandle> CreateResult = Asset.Id.is_nil()
        ? WorldRef.CreateNode(Asset.Type, Asset.Name)
        : WorldRef.CreateNodeWithId(Asset.Type, Asset.Name, Asset.Id);
    if (!CreateResult)
    {
        return std::unexpected(CreateResult.error());
    }

    NodeHandle Handle = *CreateResult;
    if (!Parent.IsNull())
    {
        NodeHandle ParentHandle = Parent;
        if (const Result AttachResult = WorldRef.AttachChild(ParentHandle, Handle); !AttachResult)
        {
            return std::unexpected(AttachResult.error());
        }
    }

    BaseNode* Node = Handle.Borrowed();
    if (!Node)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Materialized node could not be resolved"));
    }
    Node->Active(Asset.Active);

    (void)ApplyFields(Asset.Type, Node, Asset.Fields);

    for (const NodeComponentAsset& ComponentAsset : Asset.Components)
    {
        if (ComponentAsset.Type == TypeId{})
        {
            continue;
        }

        const TypeInfo* ComponentTypeInfo = TypeRegistry::Instance().Find(ComponentAsset.Type);
        if (!ComponentTypeInfo)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Node component asset type is not registered"));
        }

        TExpected<void*> CreateComponentResult = ComponentAsset.Id.is_nil()
            ? WorldRef.CreateComponent(Handle, ComponentAsset.Type)
            : WorldRef.CreateComponentWithId(Handle, ComponentAsset.Type, ComponentAsset.Id);
        if (!CreateComponentResult)
        {
            return std::unexpected(CreateComponentResult.error());
        }

        void* ComponentPtr = *CreateComponentResult;
        if (!ComponentPtr)
        {
            return std::unexpected(MakeError(EErrorCode::InternalError, "Materialized component could not be resolved"));
        }

        (void)ApplyFields(ComponentAsset.Type, ComponentPtr, ComponentAsset.Fields);
    }

    for (const NodeObjectAsset& Child : Asset.Children)
    {
        auto ChildResult = MaterializeNodeObject(Child, WorldRef, Handle);
        if (!ChildResult)
        {
            return std::unexpected(ChildResult.error());
        }
    }

    return Handle;
}

template<typename TValue>
TExpected<void> SerializeBinaryValue(const TValue& Value, std::vector<uint8_t>& OutBytes)
{
    try
    {
        std::ostringstream Output(std::ios::binary);
        cereal::BinaryOutputArchive Archive(Output);
        Archive(Value);
        const std::string Bytes = std::move(Output).str();
        OutBytes.assign(Bytes.begin(), Bytes.end());
        return Ok();
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

template<typename TValue>
TExpected<TValue> DeserializeBinaryValue(const uint8_t* Bytes, const size_t Size)
{
    if (!Bytes || Size == 0)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Serialized source asset bytes are empty"));
    }

    try
    {
        const std::string InputBytes(reinterpret_cast<const char*>(Bytes), Size);
        std::istringstream Input(InputBytes, std::ios::binary);
        cereal::BinaryInputArchive Archive(Input);
        TValue Value{};
        Archive(Value);
        return Value;
    }
    catch (const std::exception& Ex)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, Ex.what()));
    }
}

} // namespace

TExpected<NodeAsset> CaptureNodeAsset(const BaseNode& NodeRef)
{
    NodeAsset Asset{};
    Asset.Name = NodeRef.Name();
    auto RootResult = CaptureNodeObject(NodeRef);
    if (!RootResult)
    {
        return std::unexpected(RootResult.error());
    }
    Asset.Nodes.push_back(std::move(*RootResult));
    return Asset;
}

TExpected<LevelAsset> CaptureLevelAsset(const Level& LevelRef)
{
    LevelAsset Asset{};
    Asset.Name = LevelRef.Name();
    Asset.Nodes.reserve(LevelRef.Children().size());
    for (const NodeHandle& ChildHandle : LevelRef.Children())
    {
        BaseNode* ChildNode = ResolveChildNode(ChildHandle, LevelRef.World());
        if (!ChildNode)
        {
            return std::unexpected(MakeError(EErrorCode::NotFound, "Level child could not be resolved during source asset capture"));
        }
        if (ChildNode->EditorTransient())
        {
            continue;
        }

        auto ChildResult = CaptureNodeObject(*ChildNode);
        if (!ChildResult)
        {
            return std::unexpected(ChildResult.error());
        }
        Asset.Nodes.push_back(std::move(*ChildResult));
    }
    return Asset;
}

TExpected<WorldAsset> CaptureWorldAsset(const World& WorldRef)
{
    WorldAsset Asset{};
    Asset.Name = WorldRef.Name();
    const_cast<World&>(WorldRef).ForEachNode(
        [] (void* UserData, const NodeHandle&, BaseNode& Node) {
            auto* Out = static_cast<WorldAsset*>(UserData);
            if (!Node.Parent().IsNull() || Node.EditorTransient())
            {
                return;
            }

            auto CaptureResult = CaptureNodeObject(Node);
            if (CaptureResult)
            {
                Out->Nodes.push_back(std::move(*CaptureResult));
            }
        },
        &Asset);
    return Asset;
}

TExpected<NodePayload> CookNodeAsset(const NodeAsset& Asset)
{
    if (Asset.Nodes.size() != 1)
    {
        return std::unexpected(MakeError(EErrorCode::InvalidArgument, "Prefab assets must contain exactly one root node"));
    }

    World ScratchWorld("NodeAssetCookWorld");
    ScratchWorld.DeferNodeOnCreateCallbacks(true);
    ScopedComponentOnCreateSuppression SuppressOnCreate{};
    auto RootResult = MaterializeNodeObject(Asset.Nodes.front(), ScratchWorld, {});
    if (!RootResult)
    {
        return std::unexpected(RootResult.error());
    }

    BaseNode* RootNode = RootResult->Borrowed();
    if (!RootNode)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Cooked prefab root could not be resolved"));
    }

    return NodeSerializer::Serialize(*RootNode);
}

TExpected<LevelPayload> CookLevelAsset(const LevelAsset& Asset)
{
    World ScratchWorld("LevelAssetCookWorld");
    ScratchWorld.DeferNodeOnCreateCallbacks(true);
    ScopedComponentOnCreateSuppression SuppressOnCreate{};

    auto LevelHandleResult = ScratchWorld.CreateLevel(Asset.Name.empty() ? std::string("Level") : Asset.Name);
    if (!LevelHandleResult)
    {
        return std::unexpected(LevelHandleResult.error());
    }

    for (const NodeObjectAsset& NodeAssetValue : Asset.Nodes)
    {
        auto NodeResult = MaterializeNodeObject(NodeAssetValue, ScratchWorld, *LevelHandleResult);
        if (!NodeResult)
        {
            return std::unexpected(NodeResult.error());
        }
    }

    auto* LevelNode = NodeCast<Level>(LevelHandleResult->Borrowed());
    if (!LevelNode)
    {
        return std::unexpected(MakeError(EErrorCode::InternalError, "Cooked level root could not be resolved"));
    }

    return LevelSerializer::Serialize(*LevelNode);
}

TExpected<WorldPayload> CookWorldAsset(const WorldAsset& Asset)
{
    World ScratchWorld(Asset.Name.empty() ? std::string("World") : Asset.Name);
    ScratchWorld.DeferNodeOnCreateCallbacks(true);
    ScopedComponentOnCreateSuppression SuppressOnCreate{};

    for (const NodeObjectAsset& NodeAssetValue : Asset.Nodes)
    {
        auto NodeResult = MaterializeNodeObject(NodeAssetValue, ScratchWorld, {});
        if (!NodeResult)
        {
            return std::unexpected(NodeResult.error());
        }
    }

    return WorldSerializer::Serialize(ScratchWorld);
}

Result NodeAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetViaCerealJsonStream(*this, Output);
}

Result LevelAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetViaCerealJsonStream(*this, Output);
}

Result WorldAsset::Save(std::ostream& Output) const
{
    return Detail::SaveAuthoredAssetViaCerealJsonStream(*this, Output);
}

TExpected<void> SerializeNodeAsset(const NodeAsset& Asset, std::vector<uint8_t>& OutBytes)
{
    return SerializeBinaryValue(Asset, OutBytes);
}

TExpected<NodeAsset> DeserializeNodeAsset(const uint8_t* Bytes, size_t Size)
{
    return DeserializeBinaryValue<NodeAsset>(Bytes, Size);
}

TExpected<void> SerializeLevelAsset(const LevelAsset& Asset, std::vector<uint8_t>& OutBytes)
{
    return SerializeBinaryValue(Asset, OutBytes);
}

TExpected<LevelAsset> DeserializeLevelAsset(const uint8_t* Bytes, size_t Size)
{
    return DeserializeBinaryValue<LevelAsset>(Bytes, Size);
}

TExpected<void> SerializeWorldAsset(const WorldAsset& Asset, std::vector<uint8_t>& OutBytes)
{
    return SerializeBinaryValue(Asset, OutBytes);
}

TExpected<WorldAsset> DeserializeWorldAsset(const uint8_t* Bytes, size_t Size)
{
    return DeserializeBinaryValue<WorldAsset>(Bytes, Size);
}

} // namespace SnAPI::GameFramework
