# SnGfVariantHandle

Owning opaque handle to a heap-allocated `Variant`.

The handle is a C ABI token for a `Variant` allocated by the runtime. Consumers must treat the pointer as opaque and must release ownership with `sn_gf_variant_destroy()`.

Ownership and lifetime:
- The handle owns the pointed-to `Variant`.
- Passing the handle by value does not duplicate ownership.
- `Ptr == NULL` represents an empty handle.

## Public Members

<div class="snapi-api-card" markdown="1">
### `void* SnGfVariantHandle::Ptr`

Opaque pointer to runtime-owned `Variant` storage.
</div>
