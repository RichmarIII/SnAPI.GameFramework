#include "TransformComponent.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "BaseNode.h"
#include "IWorld.h"
#include "Level.h"
#include "WorldEcsRuntime.h"

namespace SnAPI::GameFramework
{
namespace
{
constexpr std::size_t kMaxTransformHierarchyDepth = 1024;
constexpr Vec3::Scalar kMinScaleMagnitude = static_cast<Vec3::Scalar>(1.0e-6);

bool BuildAncestorChain(BaseNode& LeafNode, std::vector<BaseNode*>& OutChain);
NodeTransform IdentityNodeTransform();

Quat NormalizeQuatOrIdentity(const Quat& Rotation)
{
    Quat Out = Rotation;
    if (Out.squaredNorm() > static_cast<Quat::Scalar>(0))
    {
        Out.normalize();
    }
    else
    {
        Out = Quat::Identity();
    }
    return Out;
}

Vec3 SafeScaleDivide(const Vec3& Numerator, const Vec3& Denominator)
{
    Vec3 Out{};

    const auto DivideAxis = [&](const Vec3::Scalar Value, const Vec3::Scalar Divisor) -> Vec3::Scalar {
        if (std::abs(Divisor) <= kMinScaleMagnitude)
        {
            return static_cast<Vec3::Scalar>(0);
        }
        return Value / Divisor;
    };

    Out.x() = DivideAxis(Numerator.x(), Denominator.x());
    Out.y() = DivideAxis(Numerator.y(), Denominator.y());
    Out.z() = DivideAxis(Numerator.z(), Denominator.z());
    return Out;
}

bool ReadLocalTransformFromComponent(BaseNode& Node, NodeTransform& OutTransform)
{
    auto TransformResult = Node.Component<TransformComponent>();
    if (!TransformResult)
    {
        return false;
    }

    OutTransform.Position = TransformResult->Position;
    OutTransform.Rotation = NormalizeQuatOrIdentity(TransformResult->Rotation);
    OutTransform.Scale = TransformResult->Scale;
    return true;
}

RuntimeNodeTransform ToRuntimeTransform(const NodeTransform& Transform)
{
    RuntimeNodeTransform Out{};
    Out.Position = Transform.Position;
    Out.Rotation = Transform.Rotation;
    Out.Scale = Transform.Scale;
    return Out;
}

NodeTransform ToNodeTransform(const RuntimeNodeTransform& Transform)
{
    NodeTransform Out{};
    Out.Position = Transform.Position;
    Out.Rotation = Transform.Rotation;
    Out.Scale = Transform.Scale;
    return Out;
}

bool ResolveRuntimeNode(BaseNode& Node, IWorld*& OutWorld, RuntimeNodeHandle& OutHandle)
{
    OutWorld = Node.World();
    OutHandle = {};
    if (!OutWorld)
    {
        return false;
    }

    auto& NodeRuntime = OutWorld->EcsRuntime().Nodes();
    RuntimeNodeHandle CachedHandle = Node.RuntimeNode();
    if (!CachedHandle.IsNull() && NodeRuntime.Resolve(CachedHandle))
    {
        OutHandle = CachedHandle;
        return true;
    }

    auto RuntimeHandleResult = OutWorld->RuntimeNodeById(Node.Id());
    if (!RuntimeHandleResult)
    {
        Node.RuntimeNode({});
        return false;
    }

    OutHandle = RuntimeHandleResult.value();
    Node.RuntimeNode(OutHandle);
    return !OutHandle.IsNull();
}

void SyncNodeLocalTransformToRuntime(BaseNode& Node, IWorld& WorldRef, const RuntimeNodeHandle Handle)
{
    NodeTransform Local = IdentityNodeTransform();
    auto& NodeRuntime = WorldRef.EcsRuntime().Nodes();
    if (ReadLocalTransformFromComponent(Node, Local))
    {
        (void)NodeRuntime.SetLocalTransform(Handle, ToRuntimeTransform(Local));
    }
    else
    {
        (void)NodeRuntime.ClearLocalTransform(Handle);
    }
}

bool SyncAncestorChainToRuntime(BaseNode& Node,
                                IWorld& WorldRef,
                                RuntimeNodeHandle& OutLeafHandle,
                                std::vector<BaseNode*>& ScratchChain)
{
    OutLeafHandle = {};
    if (!BuildAncestorChain(Node, ScratchChain))
    {
        return false;
    }

    IWorld* SyncWorld = nullptr;
    if (!ResolveRuntimeNode(Node, SyncWorld, OutLeafHandle))
    {
        return false;
    }

    for (BaseNode* ChainNode : ScratchChain)
    {
        if (!ChainNode)
        {
            continue;
        }

        IWorld* ChainWorld = nullptr;
        RuntimeNodeHandle ChainRuntimeHandle{};
        if (!ResolveRuntimeNode(*ChainNode, ChainWorld, ChainRuntimeHandle))
        {
            return false;
        }
        if (ChainWorld != &WorldRef)
        {
            return false;
        }

        SyncNodeLocalTransformToRuntime(*ChainNode, WorldRef, ChainRuntimeHandle);
    }
    return true;
}

BaseNode* ResolveHierarchyParent(BaseNode& Node)
{
    NodeHandle ParentHandle = Node.Parent();
    if (!ParentHandle.IsNull())
    {
        return ParentHandle.Borrowed();
    }

    IWorld* WorldRef = nullptr;
    RuntimeNodeHandle RuntimeHandle{};
    if (ResolveRuntimeNode(Node, WorldRef, RuntimeHandle) && WorldRef)
    {
        const RuntimeNodeHandle ParentRuntime = WorldRef->RuntimeParent(RuntimeHandle);
        if (!ParentRuntime.IsNull())
        {
            auto ParentHandleResult = WorldRef->NodeHandleById(ParentRuntime.Id);
            if (ParentHandleResult)
            {
                return ParentHandleResult->Borrowed();
            }
        }
    }

    return nullptr;
}

bool BuildAncestorChain(BaseNode& LeafNode, std::vector<BaseNode*>& OutChain)
{
    OutChain.clear();
    OutChain.push_back(&LeafNode);

    BaseNode* Current = &LeafNode;
    for (std::size_t Depth = 0; Depth < kMaxTransformHierarchyDepth; ++Depth)
    {
        BaseNode* ParentNode = ResolveHierarchyParent(*Current);
        if (!ParentNode)
        {
            return true;
        }

        if (std::find(OutChain.begin(), OutChain.end(), ParentNode) != OutChain.end())
        {
            return false;
        }

        OutChain.push_back(ParentNode);
        Current = ParentNode;
    }

    return false;
}

NodeTransform IdentityNodeTransform()
{
    return {};
}

NodeTransform NormalizeTransformRotation(const NodeTransform& Value)
{
    NodeTransform Out = Value;
    Out.Rotation = NormalizeQuatOrIdentity(Out.Rotation);
    return Out;
}

} // namespace

NodeTransform TransformComponent::ComposeNodeTransform(const NodeTransform& ParentWorld, const NodeTransform& Local)
{
    const NodeTransform NormalizedParent = NormalizeTransformRotation(ParentWorld);
    const NodeTransform NormalizedLocal = NormalizeTransformRotation(Local);

    NodeTransform Out = IdentityNodeTransform();
    Out.Position = NormalizedParent.Position
                 + (NormalizedParent.Rotation * NormalizedParent.Scale.cwiseProduct(NormalizedLocal.Position));
    Out.Rotation = NormalizeQuatOrIdentity(NormalizedParent.Rotation * NormalizedLocal.Rotation);
    Out.Scale = NormalizedParent.Scale.cwiseProduct(NormalizedLocal.Scale);
    return Out;
}

NodeTransform TransformComponent::LocalNodeTransformFromWorld(const NodeTransform& ParentWorld, const NodeTransform& World)
{
    const NodeTransform NormalizedParent = NormalizeTransformRotation(ParentWorld);
    const NodeTransform NormalizedWorld = NormalizeTransformRotation(World);

    const Quat ParentInverse = NormalizedParent.Rotation.conjugate();
    const Vec3 ParentSpacePosition = ParentInverse * (NormalizedWorld.Position - NormalizedParent.Position);

    NodeTransform Out = IdentityNodeTransform();
    Out.Position = SafeScaleDivide(ParentSpacePosition, NormalizedParent.Scale);
    Out.Rotation = NormalizeQuatOrIdentity(ParentInverse * NormalizedWorld.Rotation);
    Out.Scale = SafeScaleDivide(NormalizedWorld.Scale, NormalizedParent.Scale);
    return Out;
}

bool TransformComponent::TryGetNodeLocalTransform(BaseNode& Node, NodeTransform& OutTransform)
{
    OutTransform = IdentityNodeTransform();
    const bool HasLocal = ReadLocalTransformFromComponent(Node, OutTransform);

    IWorld* WorldRef = nullptr;
    RuntimeNodeHandle RuntimeHandle{};
    if (ResolveRuntimeNode(Node, WorldRef, RuntimeHandle) && WorldRef)
    {
        SyncNodeLocalTransformToRuntime(Node, *WorldRef, RuntimeHandle);
    }
    return HasLocal;
}

bool TransformComponent::TryGetNodeWorldTransform(BaseNode& Node, NodeTransform& OutTransform)
{
    OutTransform = IdentityNodeTransform();

    std::vector<BaseNode*> Chain{};
    Chain.reserve(16);

    IWorld* WorldRef = nullptr;
    RuntimeNodeHandle RuntimeHandle{};
    if (ResolveRuntimeNode(Node, WorldRef, RuntimeHandle) && WorldRef)
    {
        RuntimeNodeHandle SyncedLeafHandle{};
        if (SyncAncestorChainToRuntime(Node, *WorldRef, SyncedLeafHandle, Chain))
        {
            RuntimeNodeTransform RuntimeWorld{};
            if (WorldRef->EcsRuntime().Nodes().TryGetWorldTransform(SyncedLeafHandle, RuntimeWorld))
            {
                OutTransform = ToNodeTransform(RuntimeWorld);
                return true;
            }
        }
    }

    if (!BuildAncestorChain(Node, Chain))
    {
        return false;
    }

    bool HasTransformInHierarchy = false;
    for (auto It = Chain.rbegin(); It != Chain.rend(); ++It)
    {
        NodeTransform Local = IdentityNodeTransform();
        if (!ReadLocalTransformFromComponent(**It, Local))
        {
            continue;
        }

        OutTransform = TransformComponent::ComposeNodeTransform(OutTransform, Local);
        HasTransformInHierarchy = true;
    }

    return HasTransformInHierarchy;
}

bool TransformComponent::TryGetNodeParentWorldTransform(BaseNode& Node, NodeTransform& OutTransform)
{
    OutTransform = IdentityNodeTransform();

    std::vector<BaseNode*> Chain{};
    Chain.reserve(16);

    IWorld* WorldRef = nullptr;
    RuntimeNodeHandle RuntimeHandle{};
    if (ResolveRuntimeNode(Node, WorldRef, RuntimeHandle) && WorldRef)
    {
        RuntimeNodeHandle SyncedLeafHandle{};
        if (SyncAncestorChainToRuntime(Node, *WorldRef, SyncedLeafHandle, Chain))
        {
            RuntimeNodeTransform RuntimeParentWorld{};
            if (WorldRef->EcsRuntime().Nodes().TryGetParentWorldTransform(SyncedLeafHandle, RuntimeParentWorld))
            {
                OutTransform = ToNodeTransform(RuntimeParentWorld);
                return true;
            }
        }
    }

    BaseNode* ParentNode = ResolveHierarchyParent(Node);
    if (!ParentNode)
    {
        return false;
    }

    return TransformComponent::TryGetNodeWorldTransform(*ParentNode, OutTransform);
}

bool TransformComponent::TrySetNodeWorldTransform(BaseNode& Node,
                                                  const NodeTransform& WorldTransform,
                                                  const bool CreateIfMissing)
{
    const NodeTransform NormalizedWorldTransform = NormalizeTransformRotation(WorldTransform);

    auto TransformResult = Node.Component<TransformComponent>();
    if (!TransformResult)
    {
        if (!CreateIfMissing)
        {
            return false;
        }

        auto AddedResult = Node.Add<TransformComponent>();
        if (!AddedResult)
        {
            return false;
        }
        TransformResult = AddedResult;
    }

    IWorld* WorldRef = nullptr;
    RuntimeNodeHandle RuntimeHandle{};
    std::vector<BaseNode*> Chain{};
    Chain.reserve(16);
    if (ResolveRuntimeNode(Node, WorldRef, RuntimeHandle) && WorldRef)
    {
        RuntimeNodeHandle SyncedLeafHandle{};
        if (SyncAncestorChainToRuntime(Node, *WorldRef, SyncedLeafHandle, Chain))
        {
            auto& NodeRuntime = WorldRef->EcsRuntime().Nodes();
            if (NodeRuntime.TrySetWorldTransform(SyncedLeafHandle, ToRuntimeTransform(NormalizedWorldTransform)))
            {
                RuntimeNodeTransform RuntimeLocal{};
                if (NodeRuntime.TryGetLocalTransform(SyncedLeafHandle, RuntimeLocal))
                {
                    const NodeTransform LocalTransform = ToNodeTransform(RuntimeLocal);
                    TransformResult->Position = LocalTransform.Position;
                    TransformResult->Rotation = LocalTransform.Rotation;
                    TransformResult->Scale = LocalTransform.Scale;
                    return true;
                }
            }
        }
    }

    NodeTransform ParentWorld = IdentityNodeTransform();
    const bool HasParentWorldTransform = TransformComponent::TryGetNodeParentWorldTransform(Node, ParentWorld);

    const NodeTransform LocalTransform = HasParentWorldTransform
        ? TransformComponent::LocalNodeTransformFromWorld(ParentWorld, NormalizedWorldTransform)
        : NormalizedWorldTransform;

    TransformResult->Position = LocalTransform.Position;
    TransformResult->Rotation = LocalTransform.Rotation;
    TransformResult->Scale = LocalTransform.Scale;
    return true;
}

bool TransformComponent::TrySetNodeWorldPose(BaseNode& Node,
                                             const Vec3& WorldPosition,
                                             const Quat& WorldRotation,
                                             const bool CreateIfMissing)
{
    Vec3 LocalScale{1.0f, 1.0f, 1.0f};
    if (auto ExistingTransform = Node.Component<TransformComponent>())
    {
        LocalScale = ExistingTransform->Scale;
    }

    NodeTransform ParentWorld = IdentityNodeTransform();
    const bool HasParentWorldTransform = TransformComponent::TryGetNodeParentWorldTransform(Node, ParentWorld);

    NodeTransform DesiredWorldTransform = IdentityNodeTransform();
    DesiredWorldTransform.Position = WorldPosition;
    DesiredWorldTransform.Rotation = NormalizeQuatOrIdentity(WorldRotation);
    DesiredWorldTransform.Scale = HasParentWorldTransform
        ? ParentWorld.Scale.cwiseProduct(LocalScale)
        : LocalScale;

    return TransformComponent::TrySetNodeWorldTransform(Node, DesiredWorldTransform, CreateIfMissing);
}

} // namespace SnAPI::GameFramework
