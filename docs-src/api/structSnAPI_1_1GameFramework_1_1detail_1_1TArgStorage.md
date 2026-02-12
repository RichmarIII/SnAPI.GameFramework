# SnAPI::GameFramework::detail::TArgStorage

Storage selection for reflected arguments.

## Public Types

<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::detail::TArgStorage< Arg >::Raw = std::remove_reference_t<Arg>`
</div>
<div class="snapi-api-card" markdown="1">
### `using SnAPI::GameFramework::detail::TArgStorage< Arg >::Type = std::conditional_t<std::is_lvalue_reference_v<Arg>, std::reference_wrapper<Raw>, std::remove_cvref_t<Arg>>`
</div>
