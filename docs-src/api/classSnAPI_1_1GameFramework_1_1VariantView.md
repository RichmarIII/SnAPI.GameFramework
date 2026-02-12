# SnAPI::GameFramework::VariantView

Non-owning view into a Variant-like value.

## Private Members

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::VariantView::m_type`
</div>
<div class="snapi-api-card" markdown="1">
### `const void* SnAPI::GameFramework::VariantView::m_ptr`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::VariantView::m_isConst`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::VariantView::VariantView()=default`
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::VariantView::VariantView(TypeId Type, const void *Ptr, bool IsConst)`

**Parameters**

- `Type`: 
- `Ptr`: 
- `IsConst`:
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::VariantView::Type() const`
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::VariantView::IsConst() const`
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::VariantView::Borrowed() const`
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::VariantView::BorrowedMutable()`
</div>
