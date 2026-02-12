# SnAPI::GameFramework::ConstructorInfo

Reflection metadata for a constructor.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::vector<TypeId> SnAPI::GameFramework::ConstructorInfo::ParamTypes`

Parameter type ids.
</div>
<div class="snapi-api-card" markdown="1">
### `std::function<TExpected<std::shared_ptr<void> >(std::span<const Variant> Args)> SnAPI::GameFramework::ConstructorInfo::Construct`

Construction callback.
</div>
