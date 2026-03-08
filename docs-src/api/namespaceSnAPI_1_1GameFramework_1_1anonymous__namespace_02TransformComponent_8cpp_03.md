# SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}

## Variables

<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::kMaxTransformHierarchyDepth`
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3::Scalar SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::kMinScaleMagnitude`
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::BuildAncestorChain(BaseNode &LeafNode, std::vector< BaseNode * > &OutChain)`

**Parameters**

- `LeafNode`: 
- `OutChain`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeTransform SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::IdentityNodeTransform()`
</div>
<div class="snapi-api-card" markdown="1">
### `Quat SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::NormalizeQuatOrIdentity(const Quat &Rotation)`

**Parameters**

- `Rotation`:
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::SafeScaleDivide(const Vec3 &Numerator, const Vec3 &Denominator)`

**Parameters**

- `Numerator`: 
- `Denominator`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::ReadLocalTransformFromComponent(BaseNode &Node, NodeTransform &OutTransform)`

**Parameters**

- `Node`: 
- `OutTransform`:
</div>
<div class="snapi-api-card" markdown="1">
### `RuntimeNodeTransform SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::ToRuntimeTransform(const NodeTransform &Transform)`

**Parameters**

- `Transform`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeTransform SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::ToNodeTransform(const RuntimeNodeTransform &Transform)`

**Parameters**

- `Transform`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::ResolveRuntimeNode(BaseNode &Node, IWorld *&OutWorld, RuntimeNodeHandle &OutHandle)`

**Parameters**

- `Node`: 
- `OutWorld`: 
- `OutHandle`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::SyncNodeLocalTransformToRuntime(BaseNode &Node, IWorld &WorldRef, const RuntimeNodeHandle Handle)`

**Parameters**

- `Node`: 
- `WorldRef`: 
- `Handle`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::SyncAncestorChainToRuntime(BaseNode &Node, IWorld &WorldRef, RuntimeNodeHandle &OutLeafHandle, std::vector< BaseNode * > &ScratchChain)`

**Parameters**

- `Node`: 
- `WorldRef`: 
- `OutLeafHandle`: 
- `ScratchChain`:
</div>
<div class="snapi-api-card" markdown="1">
### `BaseNode * SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::ResolveHierarchyParent(BaseNode &Node)`

**Parameters**

- `Node`:
</div>
<div class="snapi-api-card" markdown="1">
### `NodeTransform SnAPI::GameFramework::anonymous_namespace{TransformComponent.cpp}::NormalizeTransformRotation(const NodeTransform &Value)`

**Parameters**

- `Value`:
</div>
