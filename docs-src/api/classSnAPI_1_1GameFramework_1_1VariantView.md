# SnAPI::GameFramework::VariantView

Non-owning view into a Variant-like value.

## Private Members

<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::VariantView::m_type`

Reflected payload type id.
</div>
<div class="snapi-api-card" markdown="1">
### `const void* SnAPI::GameFramework::VariantView::m_ptr`

Non-owning payload pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::VariantView::m_isConst`

Constness gate for mutable borrowing.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::VariantView::VariantView()=default`

Construct an empty invalid view.
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::VariantView::VariantView(TypeId Type, const void *Ptr, bool IsConst)`

Construct explicit typed view.

**Parameters**

- `Type`: 
- `Ptr`: Raw payload pointer.
- `IsConst`:
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::VariantView::Type() const`

Get reflected payload type id for this view.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::VariantView::IsConst() const`

Check if mutable access is disallowed.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::VariantView::Borrowed() const`

Borrow const payload pointer.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::VariantView::BorrowedMutable()`

Borrow mutable payload pointer.

**Returns:** Mutable pointer when view is non-const, otherwise nullptr.
</div>
