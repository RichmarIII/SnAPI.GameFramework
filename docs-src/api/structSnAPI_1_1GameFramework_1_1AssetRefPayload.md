# SnAPI::GameFramework::AssetRefPayload

Serializable asset-reference payload used inside cooked render-asset payloads.

## Public Static Members

<div class="snapi-api-card" markdown="1">
### `const char* SnAPI::GameFramework::AssetRefPayload::kTypeName`
</div>

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::AssetRefPayload::AssetName`

Asset catalog name used as a human-readable and fallback identifier.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::AssetRefPayload::AssetId`

Canonical asset-id string used for stable cooked references.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::AssetRefPayload::operator==(const AssetRefPayload &) const =default`
</div>
