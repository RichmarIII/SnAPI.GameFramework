# SnAPI::GameFramework::MeshStreamSourcePayload

Source-side mesh stream payload before cooking into bulk-data chunks.

## Public Members

<div class="snapi-api-card" markdown="1">
### `EMeshStreamSemantic SnAPI::GameFramework::MeshStreamSourcePayload::Semantic`

Meaning of the source stream.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::MeshStreamSourcePayload::SubIndex`

Substream index used when one semantic appears multiple times.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::MeshStreamSourcePayload::Uri`

Optional source URI for traceability or deferred loading.
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector<uint8_t> SnAPI::GameFramework::MeshStreamSourcePayload::Bytes`

Inline raw stream bytes when source data is embedded.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::MeshStreamSourcePayload::ElementCount`

Number of logical elements in the stream.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::MeshStreamSourcePayload::StrideBytes`

Per-element stride in bytes.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::MeshStreamSourcePayload::Compress`

`true` when cooker stages should attempt to compress the stored byte payload.
</div>
