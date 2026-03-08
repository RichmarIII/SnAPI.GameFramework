# SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}

## Contents

- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::MaterialImportOutputs
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::EmbeddedTextureImportOutputs
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::MeshImportBuffers
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::RenderAssetAssimpImporter
- **Type:** SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::IPipelineContext

## Variables

<div class="snapi-api-card" markdown="1">
### `std::array<std::string_view, 14> SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::kSupportedModelExtensions`
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::ToLowerAscii(std::string_view Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::EndsWithInsensitive(const std::string &Value, std::string_view Suffix)`

**Parameters**

- `Value`: 
- `Suffix`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::HasSupportedModelExtension(const std::string &Uri)`

**Parameters**

- `Uri`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::ParseBool(std::string Value, const bool DefaultValue)`

**Parameters**

- `Value`: 
- `DefaultValue`:
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::ParseUInt(const std::string &Value, const uint32_t DefaultValue)`

**Parameters**

- `Value`: 
- `DefaultValue`:
</div>
<div class="snapi-api-card" markdown="1">
### `AssimpImporterSettings SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::ReadAssimpImportSettings(const ::SnAPI::AssetPipeline::IAssetImportSettings *ImportSettings, IPipelineContext &Ctx)`

**Parameters**

- `ImportSettings`: 
- `Ctx`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::BuildImportVariantKey(const MeshImportSettingsPayload &Settings)`

**Parameters**

- `Settings`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::SanitizeName(std::string_view Name, const uint32_t FallbackIndex)`

**Parameters**

- `Name`: 
- `FallbackIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::MakeScopedLogicalName(std::string_view BaseLogicalName, std::string_view Scope, std::string_view Name, const uint32_t Index)`

**Parameters**

- `BaseLogicalName`: 
- `Scope`: 
- `Name`: 
- `Index`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::AppendDependencyUnique(std::vector< SourceRef > &Dependencies, std::unordered_set< std::string > &SeenUris, const std::string &Uri)`

**Parameters**

- `Dependencies`: 
- `SeenUris`: 
- `Uri`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::ResolveUriRelativeToSource(std::string_view SourceUri, const std::string &Uri)`

**Parameters**

- `SourceUri`: 
- `Uri`:
</div>
<div class="snapi-api-card" markdown="1">
### `AssetRefPayload SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::MakeAssetRef(const ImportedItem &Item)`

**Parameters**

- `Item`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::AppendValueBytes(std::vector< uint8_t > &Bytes, const TValue &Value)`

**Parameters**

- `Bytes`: 
- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::AppendArrayBytes(std::vector< uint8_t > &Bytes, const std::array< TValue, N > &Values)`

**Parameters**

- `Bytes`: 
- `Values`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::array< float, 16 > SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::MatrixToArray(const aiMatrix4x4 &Matrix)`

**Parameters**

- `Matrix`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::array< float, 16 > SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::IdentityMatrixArray()`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::IsFiniteVec3(const aiVector3D &Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::IsFiniteQuat(const aiQuaternion &Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< aiNode * > SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::BuildNodeList(aiNode *Root)`

**Parameters**

- `Root`:
</div>
<div class="snapi-api-card" markdown="1">
### `aiNode * SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::FindNodeByName(aiNode *Root, const std::string &Name)`

**Parameters**

- `Root`: 
- `Name`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::InsertBoneInfluence(std::array< uint32_t, 4 > &BoneIndices, std::array< float, 4 > &BoneWeights, const uint32_t BoneIndex, const float Weight)`

**Parameters**

- `BoneIndices`: 
- `BoneWeights`: 
- `BoneIndex`: 
- `Weight`:
</div>
<div class="snapi-api-card" markdown="1">
### `aiVector3D SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::SampleVectorKeys(const aiVectorKey *Keys, const uint32_t KeyCount, const double Time)`

**Parameters**

- `Keys`: 
- `KeyCount`: 
- `Time`:
</div>
<div class="snapi-api-card" markdown="1">
### `aiQuaternion SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::SampleQuatKeys(const aiQuatKey *Keys, const uint32_t KeyCount, const double Time)`

**Parameters**

- `Keys`: 
- `KeyCount`: 
- `Time`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::UpdateNonTrivialAlphaFlag(TextureCompressorPlugin::ImageIntermediate &Out)`

**Parameters**

- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional< uint32_t > SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::ParseEmbeddedTextureIndex(const std::string &Token)`

**Parameters**

- `Token`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::ResolveEmbeddedTextureRef(const std::string &Token, const EmbeddedTextureImportOutputs &EmbeddedTextures, AssetRefPayload &OutRef)`

**Parameters**

- `Token`: 
- `EmbeddedTextures`: 
- `OutRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::DecodeRawAssimpTexture(const aiTexture &Texture, TextureCompressorPlugin::ImageIntermediate &Out)`

**Parameters**

- `Texture`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::DecodeAssimpEmbeddedTexture(const aiTexture &Texture, TextureCompressorPlugin::ImageIntermediate &Out, IPipelineContext &Ctx)`

**Parameters**

- `Texture`: 
- `Out`: 
- `Ctx`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::BuildEmbeddedTextureLabel(const aiTexture &Texture, const uint32_t TextureIndex)`

**Parameters**

- `Texture`: 
- `TextureIndex`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::BuildEmbeddedTextureItems(const SourceRef &Source, const aiScene &Scene, std::string_view BaseLogicalName, std::string_view VariantKey, std::vector< ImportedItem > &GeneratedItems, EmbeddedTextureImportOutputs &OutEmbeddedTextures, IPipelineContext &Ctx)`

**Parameters**

- `Source`: 
- `Scene`: 
- `BaseLogicalName`: 
- `VariantKey`: 
- `GeneratedItems`: 
- `OutEmbeddedTextures`: 
- `Ctx`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetAssimpImporter.cpp}::BuildAssimpItems(const SourceRef &Source, const aiScene &Scene, const AssimpImporterSettings &ImportConfig, std::vector< ImportedItem > &OutItems, IPipelineContext &Ctx)`

**Parameters**

- `Source`: 
- `Scene`: 
- `ImportConfig`: 
- `OutItems`: 
- `Ctx`:
</div>
