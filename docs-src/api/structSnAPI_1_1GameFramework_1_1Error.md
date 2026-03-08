# SnAPI::GameFramework::Error

Error payload stored by `TExpected` and `Result`.

`Error` is the standard failure object returned by the framework. It intentionally stays lightweight: one categorical code plus one descriptive message. This keeps API contracts predictable and easy to surface through editor UI, logs, and tests without forcing callers into exception-based handling.

Semantics:
- A default-constructed `Error` represents success.
- `operator bool()` returns `true` for failure, not success.
- `Message` is intended for diagnostics and may change over time; do not parse it for logic.

Ownership and lifetime:
- `Message` is owned by the `Error` instance.
- Copies are independent and safe to store.

Threading:
- Plain value type. Safe to copy across threads.

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
