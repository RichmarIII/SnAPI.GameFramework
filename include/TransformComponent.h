#pragma once

#include "IComponent.h"
#include "Math.h"

namespace SnAPI::GameFramework
{

class BaseNode;

/**
 * @brief Plain transform value used for hierarchy/world-space calculations.
 */
struct NodeTransform
{
    Vec3 Position{}; /**< @brief Position in local or world space depending on context. */
    Quat Rotation = Quat::Identity(); /**< @brief Rotation in local or world space depending on context. */
    Vec3 Scale{1.0f, 1.0f, 1.0f}; /**< @brief Scale in local or world space depending on context. */
};

/**
 * @brief Basic transform component (position, quaternion rotation, scale).
 * @remarks Minimal spatial state component used by examples and built-in systems.
 */
class TransformComponent : public IComponent
{
public:
    /** @brief Stable type name for reflection. */
    static constexpr const char* kTypeName = "SnAPI::GameFramework::TransformComponent";

    Vec3 Position{}; /**< @brief Local position. */
    Quat Rotation = Quat::Identity(); /**< @brief Local rotation as quaternion. */
    Vec3 Scale{1.0f, 1.0f, 1.0f}; /**< @brief Local scale. */

    /**
     * @brief Compose a child local transform onto a parent world transform.
     * @param ParentWorld Parent transform in world space.
     * @param Local Child transform in parent-local space.
     * @return Composed child transform in world space.
     */
    [[nodiscard]] static NodeTransform ComposeNodeTransform(const NodeTransform& ParentWorld, const NodeTransform& Local);

    /**
     * @brief Convert a world transform into parent-local space.
     * @param ParentWorld Parent transform in world space.
     * @param World Child transform in world space.
     * @return Child transform expressed in parent-local space.
     */
    [[nodiscard]] static NodeTransform LocalNodeTransformFromWorld(const NodeTransform& ParentWorld, const NodeTransform& World);

    /**
     * @brief Read a node's local transform component.
     * @param Node Node to query.
     * @param OutTransform Receives local transform if present.
     * @return True when node has `TransformComponent`.
     */
    static bool TryGetNodeLocalTransform(BaseNode& Node, NodeTransform& OutTransform);

    /**
     * @brief Resolve a node's world transform by walking its full parent chain.
     * @param Node Node to query.
     * @param OutTransform Receives world transform.
     * @return True when at least one `TransformComponent` exists in the traversed hierarchy.
     * @remarks
     * Parents without `TransformComponent` are still traversed so ancestor transforms are not skipped.
     * Traversal also crosses nested-graph boundaries (prefab/level graph root -> owning graph node).
     */
    static bool TryGetNodeWorldTransform(BaseNode& Node, NodeTransform& OutTransform);

    /**
     * @brief Resolve world transform for a node's parent chain (excluding the node itself).
     * @param Node Node whose parent world transform should be computed.
     * @param OutTransform Receives parent world transform.
     * @return True when at least one ancestor in the parent chain has `TransformComponent`.
     * @remarks Includes nested-graph ownership boundaries for graph-root nodes.
     */
    static bool TryGetNodeParentWorldTransform(BaseNode& Node, NodeTransform& OutTransform);

    /**
     * @brief Write a node's local transform so that its resulting world transform matches the input.
     * @param Node Node to update.
     * @param WorldTransform Desired world transform.
     * @param CreateIfMissing When true, creates `TransformComponent` if missing.
     * @return True when local transform was written.
     */
    static bool TrySetNodeWorldTransform(BaseNode& Node, const NodeTransform& WorldTransform, bool CreateIfMissing = false);

    /**
     * @brief Write a node's local pose (position + rotation) from desired world-space values.
     * @param Node Node to update.
     * @param WorldPosition Desired world-space position.
     * @param WorldRotation Desired world-space rotation.
     * @param CreateIfMissing When true, creates `TransformComponent` if missing.
     * @return True when local transform was written.
     * @remarks Preserves the node's existing local scale when present.
     */
    static bool TrySetNodeWorldPose(BaseNode& Node,
                                    const Vec3& WorldPosition,
                                    const Quat& WorldRotation,
                                    bool CreateIfMissing = true);
};

} // namespace SnAPI::GameFramework
