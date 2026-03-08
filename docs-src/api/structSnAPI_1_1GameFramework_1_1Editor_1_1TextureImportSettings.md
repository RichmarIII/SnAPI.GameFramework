# SnAPI::GameFramework::Editor::TextureImportSettings

Reflected settings for texture compression imports.

These values control how source textures are cooked into compressed runtime payloads. As with other import settings, edits are metadata only until a reimport occurs.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::Editor::TextureImportSettings::kTypeName`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `ETextureCompressionTarget SnAPI::GameFramework::Editor::TextureImportSettings::Target`

Compression family preferred for the cooked output.
</div>
<div class="snapi-api-card" markdown="1">
### `ETextureCompressionFormat SnAPI::GameFramework::Editor::TextureImportSettings::Format`

Exact compressed format override, or `Auto` for cooker-selected output.
</div>
<div class="snapi-api-card" markdown="1">
### `float SnAPI::GameFramework::Editor::TextureImportSettings::Quality`

Normalized quality hint in the `[0, 1]` range used by the texture cooker.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::TextureImportSettings::ForceSrgb`

Force sRGB color-space handling during cook.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::TextureImportSettings::ForceLinear`

Force linear color-space handling during cook.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::Editor::TextureImportSettings::ForceNormalMap`

Treat the source as a normal map even if heuristics disagree.
</div>
<div class="snapi-api-card" markdown="1">
### `uint32_t SnAPI::GameFramework::Editor::TextureImportSettings::MaxMips`

Maximum number of mip levels to keep; `0` means cooker default or full chain.
</div>
