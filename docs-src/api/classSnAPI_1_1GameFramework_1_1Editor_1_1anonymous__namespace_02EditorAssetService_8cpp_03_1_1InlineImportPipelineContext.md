# SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext

## Private Members

<div class="snapi-api-card" markdown="1">
### `const ::SnAPI::AssetPipeline::PayloadRegistry& SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::m_registry`
</div>
<div class="snapi-api-card" markdown="1">
### `const std::unordered_map<std::string, std::string>& SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::m_options`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::string>& SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::m_infos`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::string>& SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::m_warnings`
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<std::string>& SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::m_errors`
</div>
<div class="snapi-api-card" markdown="1">
### `std::mutex SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::m_logMutex`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::InlineImportPipelineContext(const ::SnAPI::AssetPipeline::PayloadRegistry &Registry, const std::unordered_map< std::string, std::string > &Options, std::vector< std::string > &Infos, std::vector< std::string > &Warnings, std::vector< std::string > &Errors)`

**Parameters**

- `Registry`: 
- `Options`: 
- `Infos`: 
- `Warnings`: 
- `Errors`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::LogInfo(const char *Fmt,...) override`

**Parameters**

- `Fmt`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::LogWarn(const char *Fmt,...) override`

**Parameters**

- `Fmt`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::LogError(const char *Fmt,...) override`

**Parameters**

- `Fmt`:
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::ReadAllBytes(const std::string &Uri, std::vector< uint8_t > &Out) override`

**Parameters**

- `Uri`: 
- `Out`:
</div>
<div class="snapi-api-card" markdown="1">
### `uint64_t SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::HashBytes64(const void *Data, const std::size_t Size) override`

**Parameters**

- `Data`: 
- `Size`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::HashBytes128(const void *Data, const std::size_t Size, uint64_t &OutHi, uint64_t &OutLo) override`

**Parameters**

- `Data`: 
- `Size`: 
- `OutHi`: 
- `OutLo`:
</div>
<div class="snapi-api-card" markdown="1">
### `::SnAPI::AssetPipeline::AssetId SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::MakeDeterministicAssetId(std::string_view LogicalName, std::string_view VariantKey) override`

**Parameters**

- `LogicalName`: 
- `VariantKey`:
</div>
<div class="snapi-api-card" markdown="1">
### `const ::SnAPI::AssetPipeline::IPayloadSerializer * SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::FindSerializer(const ::SnAPI::AssetPipeline::TypeId Id) const override`

**Parameters**

- `Id`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Editor::anonymous_namespace{EditorAssetService.cpp}::InlineImportPipelineContext::GetOption(std::string_view Key, std::string_view Default={}) const override`

**Parameters**

- `Key`: 
- `Default`:
</div>
