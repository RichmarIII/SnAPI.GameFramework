# SnAPI::GameFramework::Editor::TextureAssetEditorPayload

Editor-facing view of cooked texture payload metadata.

This structure is used for inspector presentation and dirty-state tracking of texture assets. In the current editor flow, most texture knobs are reimport settings rather than direct runtime payload edits, so this payload mainly exposes preview metadata and derived state.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::Editor::TextureAssetEditorPayload::kTypeName`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `ETextureCompressionTarget SnAPI::GameFramework::Editor::TextureAssetEditorPayload::Target`

Compression family reported by the cooked payload.
</div>
<div class="snapi-api-card" markdown="1">
### `ETextureCompressionFormat SnAPI::GameFramework::Editor::TextureAssetEditorPayload::Format`

Exact cooked format reported by the payload when known.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::Editor::TextureAssetEditorPayload::Quality`

Normalized cooker quality hint recorded for preview.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::Editor::TextureAssetEditorPayload::Width`

Base-level texture width in texels.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::Editor::TextureAssetEditorPayload::Height`

Base-level texture height in texels.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::Editor::TextureAssetEditorPayload::MipCount`

Number of mip levels present in the cooked payload.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::TextureAssetEditorPayload::SRGB`

`true` when the cooked payload is interpreted as sRGB data.
</div>
