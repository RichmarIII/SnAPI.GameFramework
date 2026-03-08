# SnAPI::GameFramework::WorldNodeRuntime

Dense hierarchy runtime that owns runtime nodes, parent/child links, and cached transforms.

`WorldNodeRuntime` is the node-side half of the ECS runtime refactor. It centralizes:
- runtime node identity and metadata
- parent/child hierarchy links
- root tracking
- local and cached world transforms

Core semantics:
- Handles are generation-safe and become invalid once a node slot is reused.
- Root membership is maintained automatically by attach/detach operations.
- World transforms are cached and recomputed lazily when a subtree is marked dirty.
- Destroying a node destroys its entire subtree iteratively in child-first order.

Threading:
- Main-thread only.

## Contents

- **Type:** SnAPI::GameFramework::WorldNodeRuntime::HierarchyEntry

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::WorldNodeRuntime::Handle = RuntimeNodeHandle`
</div>

## Private Members

<div class="snapi-api-card" markdown="1">
### `TDenseRuntimeStorage<RuntimeNodeRecord> SnAPI::GameFramework::WorldNodeRuntime::m_nodes`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<HierarchyEntry> SnAPI::GameFramework::WorldNodeRuntime::m_hierarchyBySlot`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<Handle> SnAPI::GameFramework::WorldNodeRuntime::m_roots`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<Handle> SnAPI::GameFramework::WorldNodeRuntime::m_dirtyTraversalScratch`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::pair<Handle, bool> > SnAPI::GameFramework::WorldNodeRuntime::m_destroyTraversalScratch`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::WorldNodeRuntime::CreateNode(IWorld &WorldRef, std::string Name, const TypeId &Type)`

Create a runtime node with a generated UUID.

**Parameters**

- `WorldRef`: Owning world passed to runtime lifecycle hooks.
- `Name`: Debug/editor-facing node name.
- `Type`: Reflected runtime node type.

**Returns:** Handle to the new node, or an error on failure.
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::WorldNodeRuntime::CreateNodeWithId(IWorld &WorldRef, const Uuid &Id, std::string Name, const TypeId &Type)`

Create a runtime node with an explicit UUID.

Newly created nodes start as roots with no parent and no explicit local transform.

**Parameters**

- `WorldRef`: Owning world passed to runtime lifecycle hooks.
- `Id`: Stable node identity.
- `Name`: Debug/editor-facing node name.
- `Type`: Reflected runtime node type.

**Returns:** Handle to the new node, or an error when creation fails.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::WorldNodeRuntime::DestroyNode(IWorld &WorldRef, const Handle NodeHandle)`

Destroy a runtime node and all descendants.

**Parameters**

- `WorldRef`: Owning world passed to runtime lifecycle hooks.
- `NodeHandle`: 

**Returns:** Success or an error when the handle is invalid.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::WorldNodeRuntime::AttachChild(const Handle ParentHandle, const Handle ChildHandle)`

Attach a child node under a parent node.

**Parameters**

- `ParentHandle`: Parent runtime node.
- `ChildHandle`: Child runtime node.

**Returns:** Success or an error when handles are invalid, the child already has a parent, or the operation would create a cycle.
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::WorldNodeRuntime::DetachChild(const Handle ChildHandle)`

Detach a node from its current parent, promoting it to a root.

**Parameters**

- `ChildHandle`: Child runtime node to detach.

**Returns:** Success or an error when the handle is invalid.
</div>
<div class="snapi-api-card" markdown="1">
### `RuntimeNodeRecord * SnAPI::GameFramework::WorldNodeRuntime::Resolve(const Handle NodeHandle)`

Resolve a runtime node handle to borrowed mutable node metadata.

**Parameters**

- `NodeHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `const RuntimeNodeRecord * SnAPI::GameFramework::WorldNodeRuntime::Resolve(const Handle NodeHandle) const`

Const overload of `Resolve(const Handle)`.

**Parameters**

- `NodeHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `TExpected< Handle > SnAPI::GameFramework::WorldNodeRuntime::HandleById(const Uuid &Id) const`

Rebuild a current runtime node handle from a UUID.

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `Handle SnAPI::GameFramework::WorldNodeRuntime::Parent(const Handle ChildHandle) const`

Get the parent handle of a runtime node.

**Parameters**

- `ChildHandle`: Child runtime node.

**Returns:** Parent handle when the link is valid, otherwise a null handle.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< Handle > SnAPI::GameFramework::WorldNodeRuntime::Children(const Handle ParentHandle) const`

Collect the current live children of a parent node.

**Parameters**

- `ParentHandle`: Parent runtime node.

**Returns:** Vector of live child handles in stored child order.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldNodeRuntime::ForEachChild(const Handle ParentHandle, TVisitor &&Visitor) const`

Visit each live child handle of a parent node.

**Parameters**

- `ParentHandle`: Parent runtime node.
- `Visitor`: Callback invoked for each currently live child.
</div>
<div class="snapi-api-card" markdown="1">
### `const std::vector< Handle > & SnAPI::GameFramework::WorldNodeRuntime::Roots() const`

Borrow the current root-handle list.
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::WorldNodeRuntime::Size() const`

Get the number of live runtime nodes.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldNodeRuntime::SetLocalTransform(const Handle NodeHandle, const RuntimeNodeTransform &LocalTransform)`

Assign an explicit local transform to a runtime node.

The input rotation is normalized before storage. The entire subtree is marked dirty so cached world transforms will be recomputed lazily.

**Parameters**

- `NodeHandle`: 
- `LocalTransform`: Local transform relative to the parent.

**Returns:** `true` when the node exists and the transform was stored.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldNodeRuntime::ClearLocalTransform(const Handle NodeHandle)`

Remove a node's explicit local transform.

Clearing a local transform means the node contributes no authored transform of its own; world transform queries may then inherit only ancestor transforms.

**Parameters**

- `NodeHandle`: 

**Returns:** `true` when the node exists.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldNodeRuntime::TryGetLocalTransform(const Handle NodeHandle, RuntimeNodeTransform &OutTransform) const`

Read the stored local transform for a node.

**Parameters**

- `NodeHandle`: 
- `OutTransform`: Receives the local transform on success. Reset to identity on entry.

**Returns:** `true` when the node has an explicit local transform.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldNodeRuntime::TryGetWorldTransform(const Handle NodeHandle, RuntimeNodeTransform &OutTransform)`

Compute or fetch the cached world transform for a node.

Returns `false` both for invalid nodes and for valid nodes that neither define a local transform nor inherit one from an ancestor.

**Parameters**

- `NodeHandle`: 
- `OutTransform`: Receives the computed world transform. Reset to identity on entry.

**Returns:** `true` when the node has a world transform to report.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldNodeRuntime::TryGetParentWorldTransform(const Handle NodeHandle, RuntimeNodeTransform &OutTransform)`

Compute the world transform of a node's parent.

**Parameters**

- `NodeHandle`: 
- `OutTransform`: Receives the parent world transform.

**Returns:** `true` when the node has a parent and that parent has a world transform.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldNodeRuntime::TrySetWorldTransform(const Handle NodeHandle, const RuntimeNodeTransform &WorldTransform)`

Set a node's world transform by converting it into local space relative to the parent.

Parent scale is inverted with a safety threshold, so near-zero parent scale axes collapse to `0` rather than producing infinities.

**Parameters**

- `NodeHandle`: 
- `WorldTransform`: Desired world-space transform.

**Returns:** `true` when the node exists and the local transform was updated.
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldNodeRuntime::Clear(IWorld &WorldRef)`

Destroy all runtime nodes and reset the hierarchy runtime to empty.

**Parameters**

- `WorldRef`: Owning world passed to runtime lifecycle hooks.
</div>

## Private Static Func

<div class="snapi-api-card" markdown="1">
### `static void SnAPI::GameFramework::WorldNodeRuntime::RemoveChildLink(HierarchyEntry &ParentEntry, const Handle ChildHandle)`

**Parameters**

- `ParentEntry`: 
- `ChildHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `static RuntimeNodeTransform SnAPI::GameFramework::WorldNodeRuntime::IdentityTransform()`
</div>
<div class="snapi-api-card" markdown="1">
### `static RuntimeNodeTransform SnAPI::GameFramework::WorldNodeRuntime::NormalizeTransformRotation(const RuntimeNodeTransform &InTransform)`

**Parameters**

- `InTransform`:
</div>
<div class="snapi-api-card" markdown="1">
### `static Quat SnAPI::GameFramework::WorldNodeRuntime::NormalizeQuatOrIdentity(const Quat &Rotation)`

**Parameters**

- `Rotation`:
</div>
<div class="snapi-api-card" markdown="1">
### `static Vec3 SnAPI::GameFramework::WorldNodeRuntime::SafeScaleDivide(const Vec3 &Numerator, const Vec3 &Denominator)`

**Parameters**

- `Numerator`: 
- `Denominator`:
</div>
<div class="snapi-api-card" markdown="1">
### `static RuntimeNodeTransform SnAPI::GameFramework::WorldNodeRuntime::ComposeTransform(const RuntimeNodeTransform &ParentWorld, const RuntimeNodeTransform &Local)`

**Parameters**

- `ParentWorld`: 
- `Local`:
</div>
<div class="snapi-api-card" markdown="1">
### `static RuntimeNodeTransform SnAPI::GameFramework::WorldNodeRuntime::LocalTransformFromWorld(const RuntimeNodeTransform &ParentWorld, const RuntimeNodeTransform &World)`

**Parameters**

- `ParentWorld`: 
- `World`:
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldNodeRuntime::EnsureHierarchySlot(const uint32_t SlotIndex)`

**Parameters**

- `SlotIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `HierarchyEntry * SnAPI::GameFramework::WorldNodeRuntime::EntryForHandle(const Handle NodeHandle)`

**Parameters**

- `NodeHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `const HierarchyEntry * SnAPI::GameFramework::WorldNodeRuntime::EntryForHandle(const Handle NodeHandle) const`

**Parameters**

- `NodeHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldNodeRuntime::AddRootIfMissing(const Handle NodeHandle)`

**Parameters**

- `NodeHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldNodeRuntime::RemoveRootIfPresent(const Handle NodeHandle)`

**Parameters**

- `NodeHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::WorldNodeRuntime::MarkSubtreeDirty(const Handle NodeHandle)`

**Parameters**

- `NodeHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::WorldNodeRuntime::ComputeWorldTransform(const Handle NodeHandle, RuntimeNodeTransform &OutTransform, bool &OutHasTransform)`

**Parameters**

- `NodeHandle`: 
- `OutTransform`: 
- `OutHasTransform`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::WorldNodeRuntime::DestroyNodeIterative(IWorld &WorldRef, const Handle RootHandle)`

**Parameters**

- `WorldRef`: 
- `RootHandle`:
</div>
