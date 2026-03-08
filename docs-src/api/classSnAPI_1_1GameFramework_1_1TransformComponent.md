# SnAPI::GameFramework::TransformComponent

Basic transform component storing local position, rotation, and scale.

`TransformComponent` is the canonical spatial state component for scene-graph nodes. In addition to storing local transform values, it provides static helpers for composing, querying, and writing world transforms across parent chains and nested graph boundaries.

Core semantics:
- component fields represent local space relative to the node's parent
- world-transform helpers traverse full parent chains and can cross nested-graph ownership boundaries
- when ECS runtime transform data is available, helpers synchronize component data into the runtime and query it for authoritative world transforms
- write helpers can create the component on demand when `CreateIfMissing` is enabled

Threading model:
- Main-thread only.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::TransformComponent::kTypeName`

Stable type name for reflection.
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::TransformComponent::Position`

Local position.
</div>
<div class="snapi-api-card" markdown="1">
### `& SnAPI::GameFramework::TransformComponent::Rotation`

Local rotation as quaternion.
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::TransformComponent::Scale`

Local scale.
</div>

## Public Static Functions

<div class="snapi-api-card" markdown="1">
### `NodeTransform SnAPI::GameFramework::TransformComponent::ComposeNodeTransform(const NodeTransform &ParentWorld, const NodeTransform &Local)`

Compose a child local transform onto a parent world transform.

**Parameters**

- `ParentWorld`: Parent transform in world space.
- `Local`: Child transform in parent-local space.

**Returns:** Composed child transform in world space.
</div>
<div class="snapi-api-card" markdown="1">
### `NodeTransform SnAPI::GameFramework::TransformComponent::LocalNodeTransformFromWorld(const NodeTransform &ParentWorld, const NodeTransform &World)`

Convert a world transform into parent-local space.

**Parameters**

- `ParentWorld`: Parent transform in world space.
- `World`: 

**Returns:** Child transform expressed in parent-local space.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TransformComponent::TryGetNodeLocalTransform(BaseNode &Node, NodeTransform &OutTransform)`

Read a node's local transform component.

**Parameters**

- `Node`: Node to query.
- `OutTransform`: Receives local transform if present.

**Returns:** True when node has `TransformComponent`.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TransformComponent::TryGetNodeWorldTransform(BaseNode &Node, NodeTransform &OutTransform)`

Resolve a node's world transform by walking its full parent chain.

**Parameters**

- `Node`: Node to query.
- `OutTransform`: Receives world transform.

**Returns:** True when at least one `TransformComponent` exists in the traversed hierarchy.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TransformComponent::TryGetNodeParentWorldTransform(BaseNode &Node, NodeTransform &OutTransform)`

Resolve world transform for a node's parent chain (excluding the node itself).

**Parameters**

- `Node`: Node whose parent world transform should be computed.
- `OutTransform`: Receives parent world transform.

**Returns:** True when at least one ancestor in the parent chain has `TransformComponent`.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TransformComponent::TrySetNodeWorldTransform(BaseNode &Node, const NodeTransform &WorldTransform, bool CreateIfMissing=false)`

Write a node's local transform so that its resulting world transform matches the input.

**Parameters**

- `Node`: Node to update.
- `WorldTransform`: Desired world transform.
- `CreateIfMissing`: When true, creates `TransformComponent` if missing.

**Returns:** True when local transform was written.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::TransformComponent::TrySetNodeWorldPose(BaseNode &Node, const Vec3 &WorldPosition, const Quat &WorldRotation, bool CreateIfMissing=true)`

Write a node's local pose (position + rotation) from desired world-space values.

**Parameters**

- `Node`: Node to update.
- `WorldPosition`: Desired world-space position.
- `WorldRotation`: Desired world-space rotation.
- `CreateIfMissing`: When true, creates `TransformComponent` if missing.

**Returns:** True when local transform was written.
</div>
