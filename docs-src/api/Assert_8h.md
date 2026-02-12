# File `Assert.h`

## Contents

- **Namespace:** SnAPI
- **Namespace:** SnAPI::GameFramework
- **Namespace:** SnAPI::GameFramework::detail

## Macros

<div class="snapi-api-card" markdown="1">
### `DEBUG_ASSERT`

Debug-only assertion with formatted diagnostic message.

**Parameters**

- `condition`: Expression that must evaluate to true.
- `fmt`: std::format-style format string.
- `...`: 

**Notes**

- Compiled out when NDEBUG is defined.
</div>
