# SnAPI::GameFramework::Error

Error payload for TExpected results.

## Public Members

<div class="snapi-api-card" markdown="1">
### `EErrorCode SnAPI::GameFramework::Error::Code`

Error category.
</div>
<div class="snapi-api-card" markdown="1">
### `std::string SnAPI::GameFramework::Error::Message`

Human-readable diagnostic message.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Error::Error()=default`

Construct a success error value.

**Notes**

- A success Error evaluates to false.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Error::Error(EErrorCode InCode, std::string InMessage)`

Construct an error with code and message.

**Parameters**

- `InCode`: Error category.
- `InMessage`: Diagnostic message.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::Error::operator bool() const noexcept`

Boolean conversion for quick success checks.

**Notes**

- This intentionally inverts the typical "success" meaning.
</div>
