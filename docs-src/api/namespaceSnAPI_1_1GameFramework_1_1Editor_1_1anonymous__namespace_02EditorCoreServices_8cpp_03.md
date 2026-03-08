# SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}

## Contents

- **Type:** SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::SelectNodeCommand

## Functions

<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::ApplySelection(EditorSelectionModel &Model, const NodeHandle &Node)`

**Parameters**

- `Model`: 
- `Node`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::IsFiniteFloat(const float Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::IsFiniteVec3(const Vec3 &Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `Vec3 SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::NormalizeOrAxis(const Vec3 &Value, const Vec3 &FallbackAxis)`

**Parameters**

- `Value`: 
- `FallbackAxis`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Math::Scalar SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::SnapValueToStep(const SnAPI::Math::Scalar Value, const SnAPI::Math::Scalar Step)`

**Parameters**

- `Value`: 
- `Step`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::Math::Scalar SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::ConsumeSnapRemainder(const SnAPI::Math::Scalar Delta, const SnAPI::Math::Scalar Step, SnAPI::Math::Scalar &InOutRemainder)`

**Parameters**

- `Delta`: 
- `Step`: 
- `InOutRemainder`:
</div>
<div class="snapi-api-card" markdown="1">
### `Quat SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::RotationFromTo(const Vec3 &From, const Vec3 &To)`

**Parameters**

- `From`: 
- `To`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::IsPointInsideRect(const SnAPI::UI::UIRect &Rect, const float X, const float Y)`

**Parameters**

- `Rect`: 
- `X`: 
- `Y`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::ComputeAssetListSignature(const std::vector< EditorAssetService::DiscoveredAsset > &Assets)`

**Parameters**

- `Assets`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::size_t SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::ComputeAssetDetailsSignature(const EditorLayout::ContentAssetDetails &Details)`

**Parameters**

- `Details`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::FormatBinaryByteSize(const std::uint64_t Bytes)`

**Parameters**

- `Bytes`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::ShortTypeLabel(std::string_view QualifiedName)`

**Parameters**

- `QualifiedName`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::CanPlaceAssetKind(const ::SnAPI::AssetPipeline::TypeId &AssetKind)`

**Parameters**

- `AssetKind`:
</div>
<div class="snapi-api-card" markdown="1">
### `BaseNode * SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::ResolveNodeFromHandle(const NodeHandle Handle, World &WorldRef)`

**Parameters**

- `Handle`: 
- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::InitializeCreatedNodeDefaults(IWorld &WorldRef, BaseNode &Node)`

**Parameters**

- `WorldRef`: 
- `Node`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::SetEditorCameraEnabledForPie(World &WorldRef, const bool Enabled)`

**Parameters**

- `WorldRef`: 
- `Enabled`:
</div>
<div class="snapi-api-card" markdown="1">
### `Result SnAPI::GameFramework::Editor::anonymous_namespace{EditorCoreServices.cpp}::ExecuteHierarchyAction(EditorServiceContext &Context, const EditorLayout::HierarchyActionRequest &Request)`

**Parameters**

- `Context`: 
- `Request`:
</div>
