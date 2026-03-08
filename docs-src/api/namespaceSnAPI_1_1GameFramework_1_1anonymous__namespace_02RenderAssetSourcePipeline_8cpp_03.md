# SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}

## Contents

- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonValue
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::RenderAssetJsonImporter
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::RenderMaterialCooker
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::RenderMaterialInstanceCooker
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::RenderSkeletonCooker
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::RenderAnimationCooker
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::RenderStaticMeshCooker
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::RenderSkeletalMeshCooker

## Enumerations

<div class="snapi-api-card" markdown="1">
### `enum EJsonValueType`

**Values**

- `Null`
- `Bool`
- `Number`
- `String`
- `Array`
- `Object`
</div>
<div class="snapi-api-card" markdown="1">
### `enum ERenderSourceType`

**Values**

- `Unknown`
- `Material`
- `MaterialInstance`
- `StaticMesh`
- `SkeletalMesh`
</div>

## Variables

<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::EBulkSemantic SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::kBulkSemanticMeshStream`
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::EBulkSemantic SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::kBulkSemanticSkeletalAnimation`
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::ToLowerAscii(std::string_view Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::EndsWithInsensitive(const std::string &Value, std::string_view Suffix)`

**Parameters**

- `Value`: 
- `Suffix`:
</div>
<div class="snapi-api-card" markdown="1">
### `const JsonValue * SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::TryGetField(const JsonValue &Object, std::string_view Key)`

**Parameters**

- `Object`: 
- `Key`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::TryReadString(const JsonValue &Value, std::string &Out)`

**Parameters**

- `Value`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::TryReadBool(const JsonValue &Value, bool &Out)`

**Parameters**

- `Value`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::TryReadUnsigned(const JsonValue &Value, uint32_t &Out)`

**Parameters**

- `Value`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::TryReadSigned(const JsonValue &Value, int32_t &Out)`

**Parameters**

- `Value`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::TryReadFloat(const JsonValue &Value, float &Out)`

**Parameters**

- `Value`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::TryReadFloatArray(const JsonValue &Value, std::array< float, N > &Out)`

**Parameters**

- `Value`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::ParseAssetRefPayload(const JsonValue &Value, AssetRefPayload &OutRef)`

**Parameters**

- `Value`: 
- `OutRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional< EMeshStreamSemantic > SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::ParseStreamSemantic(const std::string &SemanticText)`

**Parameters**

- `SemanticText`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::ResolveUriRelativeToSource(std::string_view SourceUri, std::string Uri)`

**Parameters**

- `SourceUri`: 
- `Uri`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::ParseStaticMeshPayloadFields(const JsonValue &Root, StaticMeshPayload &Out)`

**Parameters**

- `Root`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::ParseStreamSourceArray(const JsonValue &Root, std::string_view SourceUri, std::vector< MeshStreamSourcePayload > &OutStreams)`

**Parameters**

- `Root`: 
- `SourceUri`: 
- `OutStreams`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::ParseMaterialPayload(const JsonValue &Root, MaterialPayload &Out)`

**Parameters**

- `Root`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::ParseMaterialInstancePayload(const JsonValue &Root, MaterialInstancePayload &Out)`

**Parameters**

- `Root`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional< std::string > SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::ReadOptionalStringField(const JsonValue &Root, std::string_view Key)`

**Parameters**

- `Root`: 
- `Key`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::DetermineLogicalName(const JsonValue &Root, const std::string &SourceUri)`

**Parameters**

- `Root`: 
- `SourceUri`:
</div>
<div class="snapi-api-card" markdown="1">
### `ERenderSourceType SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::DetermineSourceTypeFromPath(const std::string &SourceUri)`

**Parameters**

- `SourceUri`:
</div>
<div class="snapi-api-card" markdown="1">
### `ERenderSourceType SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::DetermineSourceType(const JsonValue &Root, const std::string &SourceUri)`

**Parameters**

- `Root`: 
- `SourceUri`:
</div>
