# SnAPI::GameFramework::VariantView

Non-owning typed view into external payload storage.

`VariantView` is the zero-allocation counterpart to `Variant` used on hot reflective traversal paths such as serialization and replication.

Ownership and lifetime:
- `VariantView` never owns storage.
- The caller must guarantee that the referenced payload remains alive for the lifetime of the view.

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

Construct an explicit typed view.

**Parameters**

- `Type`: 
- `Ptr`: Raw payload pointer.
- `IsConst`:
</div>
<div class="snapi-api-card" markdown="1">
### `const TypeId & SnAPI::GameFramework::VariantView::Type() const`

Get the reflected payload type id for this view.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::VariantView::IsConst() const`

Check whether mutable access is disallowed.
</div>
<div class="snapi-api-card" markdown="1">
### `const void * SnAPI::GameFramework::VariantView::Borrowed() const`

Borrow the payload pointer as const.
</div>
<div class="snapi-api-card" markdown="1">
### `void * SnAPI::GameFramework::VariantView::BorrowedMutable()`

Borrow the payload pointer as mutable.

**Returns:** Mutable pointer when the view is non-const, otherwise `nullptr`.
</div>
