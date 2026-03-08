# SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::string_view SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::m_text`
</div>
<div class="snapi-api-card" markdown="1">
### `size_t SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::m_pos`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::JsonParser(std::string_view Text)`

**Parameters**

- `Text`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< JsonValue, std::string > SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::ParseRoot()`
</div>

## Private Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::IsEof() const`
</div>
<div class="snapi-api-card" markdown="1">
### `char SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::Peek() const`
</div>
<div class="snapi-api-card" markdown="1">
### `char SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::Take()`
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::SkipWhitespace()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< JsonValue, std::string > SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::ParseValue()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< JsonValue, std::string > SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::ParseObject()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< JsonValue, std::string > SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::ParseArray()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< std::string, std::string > SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::ParseString()`
</div>
<div class="snapi-api-card" markdown="1">
### `std::expected< JsonValue, std::string > SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::ParseNumber()`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{RenderAssetSourcePipeline.cpp}::JsonParser::ConsumeKeyword(std::string_view Keyword)`

**Parameters**

- `Keyword`:
</div>
