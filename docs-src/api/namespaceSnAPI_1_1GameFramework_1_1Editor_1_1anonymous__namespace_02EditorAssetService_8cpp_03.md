# SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}

## Contents

- **Type:** SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::AssetImportMetadataEntryDisk
- **Type:** SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::AssetImportMetadataDatabaseDisk
- **Type:** SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext
- **Type:** SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::DefaultShapePackSpec
- **Type:** SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::RuntimeWorldCounts

## Type Aliases

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::EImportProfile = EAssetImportProfile`
</div>

## Variables

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kDefaultProjectFileName`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kDefaultProjectAssetRoot`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kDefaultProjectStartupLevelPack`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kEditorStarterLevelTemplatePackFileName`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kEditorStarterScriptFileName`
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kProjectConfigVersion`
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kMaterialPayloadSchemaVersion`
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kMaterialInstancePayloadSchemaVersion`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kDefaultMaterialShaderModule`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kDefaultMaterialShadingModel`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kAssetImportMetadataDirectoryName`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kAssetImportMetadataFileName`
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kAssetImportMetadataVersion`
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::Uuid SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kAssetIdNamespace`
</div>
<div class="snapi-api-card" markdown="1">
### `std::array<std::string_view, 11> SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kAssimpManagedBuildOptionKeys`
</div>
<div class="snapi-api-card" markdown="1">
### `std::array<std::string_view, 6> SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::kTextureManagedBuildOptionKeys`
</div>

## Functions

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ToLowerCopy(std::string_view Text)`

**Parameters**

- `Text`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::FormatLogMessage(const char *Prefix, const char *Fmt, va_list Args)`

**Parameters**

- `Prefix`: 
- `Fmt`: 
- `Args`:
</div>
<div class="snapi-api-card" markdown="1">
### `Editor::ETextureCompressionTarget SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ToEditorTextureTarget(const TextureCompressorPlugin::ECompressionTarget Target)`

**Parameters**

- `Target`:
</div>
<div class="snapi-api-card" markdown="1">
### `TextureCompressorPlugin::ECompressionTarget SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ToCookedTextureTarget(const Editor::ETextureCompressionTarget Target)`

**Parameters**

- `Target`:
</div>
<div class="snapi-api-card" markdown="1">
### `Editor::ETextureCompressionFormat SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ToEditorTextureFormat(const TextureCompressorPlugin::ECompressedFormat Format)`

**Parameters**

- `Format`:
</div>
<div class="snapi-api-card" markdown="1">
### `TextureCompressorPlugin::ECompressedFormat SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ToCookedTextureFormat(const Editor::ETextureCompressionFormat Format)`

**Parameters**

- `Format`:
</div>
<div class="snapi-api-card" markdown="1">
### `TextureCompressorPlugin::ECompressionTarget SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ResolveCookedCompressionTarget(const TextureCompressorPlugin::TextureCompressorCookedInfo &Cooked)`

**Parameters**

- `Cooked`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::CompressionTargetName(const TextureCompressorPlugin::ECompressionTarget Target)`

**Parameters**

- `Target`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::uint64_t SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ComputeTextureGpuSizeBytes(const TextureCompressorPlugin::TextureCompressorCookedInfo &Cooked)`

**Parameters**

- `Cooked`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::OptionValueOr(const std::unordered_map< std::string, std::string > &Options, const std::string_view Key, std::string_view Default={})`

**Parameters**

- `Options`: 
- `Key`: 
- `Default`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ParseBoolOption(const std::string_view Text, const bool DefaultValue)`

**Parameters**

- `Text`: 
- `DefaultValue`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::optional< int32_t > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ParseIntOption(const std::string_view Text)`

**Parameters**

- `Text`:
</div>
<div class="snapi-api-card" markdown="1">
### `TextureCompressorPlugin::ECompressedFormat SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ParseTextureFormatOption(std::string_view Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ImportProfileToString(const EImportProfile Profile)`

**Parameters**

- `Profile`:
</div>
<div class="snapi-api-card" markdown="1">
### `EImportProfile SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ImportProfileFromString(std::string_view ProfileText)`

**Parameters**

- `ProfileText`:
</div>
<div class="snapi-api-card" markdown="1">
### `EImportProfile SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ImportProfileFromImporterName(std::string_view ImporterName)`

**Parameters**

- `ImporterName`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map< std::string, std::string > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::BuildOptionsFromAssimpImportSettings(const Editor::AssimpImportSettings &Settings)`

**Parameters**

- `Settings`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::unordered_map< std::string, std::string > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::BuildOptionsFromTextureImportSettings(const Editor::TextureImportSettings &Settings)`

**Parameters**

- `Settings`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::RemoveManagedBuildOptions(std::unordered_map< std::string, std::string > &BuildOptions, const std::array< std::string_view, N > &Keys)`

**Parameters**

- `BuildOptions`: 
- `Keys`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::FillAssimpImportSettingsFromTyped(const AssimpImporterSettings &Typed, Editor::AssimpImportSettings &Out)`

**Parameters**

- `Typed`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::FillTextureImportSettingsFromTyped(const TextureCompressorPlugin::TextureCompressorImportSettings &Typed, Editor::TextureImportSettings &Out)`

**Parameters**

- `Typed`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::AssetImportSettingsPtr SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::BuildTypedImportSettingsForImporter(const ::SnAPI::AssetPipeline::IAssetImporter &Importer, const std::unordered_map< std::string, std::string > &BuildOptions)`

**Parameters**

- `Importer`: 
- `BuildOptions`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::PopulateTextureEditorPayloadFromCooked(const TextureCompressorPlugin::TextureCompressorCookedInfo &Cooked, Editor::TextureAssetEditorPayload &Out)`

**Parameters**

- `Cooked`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ApplyTextureEditorPayloadToCooked(const Editor::TextureAssetEditorPayload &EditorPayload, TextureCompressorPlugin::TextureCompressorCookedInfo &InOutCooked)`

**Parameters**

- `EditorPayload`: 
- `InOutCooked`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::PopulateStaticMeshEditorPayloadFromCooked(const StaticMeshPayload &Cooked, Editor::StaticMeshAssetEditorPayload &Out)`

**Parameters**

- `Cooked`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ApplyStaticMeshEditorPayloadToCooked(const Editor::StaticMeshAssetEditorPayload &EditorPayload, StaticMeshPayload &InOutCooked)`

**Parameters**

- `EditorPayload`: 
- `InOutCooked`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::TrimCopy(std::string Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::HasUriScheme(const std::string_view Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ResolveAppDataRootPath()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::EditorTemplateAssetDirectory()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ResolveEditorAssetSourceDirectory()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ResolveEditorScriptTemplateSource()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< void, std::string > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::CopyDirectoryContentsRecursive(const std::filesystem::path &SourceDirectory, const std::filesystem::path &DestinationDirectory)`

**Parameters**

- `SourceDirectory`: 
- `DestinationDirectory`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ResolveRendererShaderSourceDirectory()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::EditorDefaultShapeAssetDirectory()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::JsonEscape(std::string_view Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< std::string, std::string > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::JsonParseString(const std::string &Text, std::size_t &Position)`

**Parameters**

- `Text`: 
- `Position`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::JsonTryReadStringField(const std::string &Text, std::string_view Key, std::string &OutValue)`

**Parameters**

- `Text`: 
- `Key`: 
- `OutValue`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::JsonTryReadUnsignedField(const std::string &Text, std::string_view Key, uint32_t &OutValue)`

**Parameters**

- `Text`: 
- `Key`: 
- `OutValue`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::NormalizeProjectPathField(const std::string_view RawValue)`

**Parameters**

- `RawValue`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ToProjectRelativePathField(const std::string_view RawValue, const std::filesystem::path &BaseRoot)`

**Parameters**

- `RawValue`: 
- `BaseRoot`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< void, std::string > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::WriteProjectConfigFile(const std::filesystem::path &ProjectFilePath, const std::string_view Name, const std::string_view AssetRoot, const std::string_view StartupLevelPack, const std::string_view DefaultRenderSettingsAssetId)`

**Parameters**

- `ProjectFilePath`: 
- `Name`: 
- `AssetRoot`: 
- `StartupLevelPack`: 
- `DefaultRenderSettingsAssetId`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::array< DefaultShapePackSpec, 4 > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::DefaultShapePackSpecs()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected<::SnAPI::AssetPipeline::AssetPackEntry, std::string > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::BuildDefaultShapePackEntry(const DefaultShapePackSpec &Spec)`

**Parameters**

- `Spec`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected<::SnAPI::AssetPipeline::AssetPackEntry, std::string > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::BuildDefaultShapePackEntryFromRuntimeWorld(const DefaultShapePackSpec &Spec, IWorld &RuntimeWorld)`

**Parameters**

- `Spec`: 
- `RuntimeWorld`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< std::size_t, std::string > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::EnsureDefaultShapePacks(const std::filesystem::path &PackDirectory, IWorld *RuntimeWorld)`

**Parameters**

- `PackDirectory`: 
- `RuntimeWorld`:
</div>
<div class="snapi-api-card" markdown="1">
### `RuntimeWorldCounts SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::CountRuntimeWorldObjects(World &WorldRef)`

**Parameters**

- `WorldRef`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::AppendUniquePath(std::vector< std::string > &Paths, std::unordered_set< std::string > &SeenPaths, const std::filesystem::path &InputPath)`

**Parameters**

- `Paths`: 
- `SeenPaths`: 
- `InputPath`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::NormalizeAssetLogicalName(std::string_view RawName)`

**Parameters**

- `RawName`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ShortTypeName(std::string_view QualifiedTypeName)`

**Parameters**

- `QualifiedTypeName`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::LeafLogicalName(std::string Value)`

**Parameters**

- `Value`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InitializeCreatedNodeDefaults(IWorld &WorldRef, BaseNode &Node)`

**Parameters**

- `WorldRef`: 
- `Node`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::MakeUniqueLogicalName(::SnAPI::AssetPipeline::AssetManager &AssetManagerRef, const std::string &Prefix, std::string BaseName)`

**Parameters**

- `AssetManagerRef`: 
- `Prefix`: 
- `BaseName`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::filesystem::path SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::ResolveImportAssetRootDirectory(const EditorAssetService::ProjectInfo &Project)`

**Parameters**

- `Project`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::SanitizePackFileStem(std::string_view Raw)`

**Parameters**

- `Raw`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter > > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::CreateEditorImporters()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker > > SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::CreateEditorCookers()`
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::IAssetImporter * SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::FindMatchingImporter(const ::SnAPI::AssetPipeline::SourceRef &Source, const std::vector< std::unique_ptr<::SnAPI::AssetPipeline::IAssetImporter > > &Importers)`

**Parameters**

- `Source`: 
- `Importers`:
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::IAssetCooker * SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::FindMatchingCooker(const ::SnAPI::AssetPipeline::TypeId &AssetKind, const ::SnAPI::AssetPipeline::TypeId &IntermediateType, const std::vector< std::unique_ptr<::SnAPI::AssetPipeline::IAssetCooker > > &Cookers)`

**Parameters**

- `AssetKind`: 
- `IntermediateType`: 
- `Cookers`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::IsTextureImporter(const ::SnAPI::AssetPipeline::IAssetImporter &Importer)`

**Parameters**

- `Importer`:
</div>
