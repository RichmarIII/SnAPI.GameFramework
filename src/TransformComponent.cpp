#include "TransformComponent.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "NodeGraph.h"

namespace SnAPI::GameFramework
{
namespace
{
constexpr std::size_t kMaxTransformHierarchyDepth = 1024;
constexpr Vec3::Scalar kMinScaleMagnitude = static_cast<Vec3::Scalar>(1.0e-6);

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
    if (!Node.OwnerGraph())
    {
        return false;
    }

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

BaseNode* ResolveHierarchyParent(BaseNode& Node)
{
    NodeHandle ParentHandle = Node.Parent();
    if (!ParentHandle.IsNull())
    {
        return ParentHandle.Borrowed();
    }

    auto* OwnerGraph = Node.OwnerGraph();
    if (!OwnerGraph)
    {
        return nullptr;
    }

    auto* OwnerGraphNode = static_cast<BaseNode*>(OwnerGraph);
    if (OwnerGraphNode == &Node)
    {
        return nullptr;
    }

    return OwnerGraphNode;
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
    return ReadLocalTransformFromComponent(Node, OutTransform);
}

bool TransformComponent::TryGetNodeWorldTransform(BaseNode& Node, NodeTransform& OutTransform)
{
    OutTransform = IdentityNodeTransform();

    std::vector<BaseNode*> Chain{};
    Chain.reserve(16);
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

    NodeTransform ParentWorld = IdentityNodeTransform();
    const bool HasParentWorldTransform = TransformComponent::TryGetNodeParentWorldTransform(Node, ParentWorld);

    const NodeTransform NormalizedWorldTransform = NormalizeTransformRotation(WorldTransform);
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
