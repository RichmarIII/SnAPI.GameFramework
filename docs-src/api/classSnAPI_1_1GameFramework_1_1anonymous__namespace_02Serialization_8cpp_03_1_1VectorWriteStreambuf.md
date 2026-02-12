# SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::VectorWriteStreambuf

Streambuf that appends cereal output bytes directly into a vector.

## Private Members

<div class="snapi-api-card" markdown="1">
### `std::vector<uint8_t>& SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::VectorWriteStreambuf::m_buffer`

Destination byte vector reference.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::VectorWriteStreambuf::VectorWriteStreambuf(std::vector< uint8_t > &Buffer)`

**Parameters**

- `Buffer`:
</div>

## Protected Functions

<div class="snapi-api-card" markdown="1">
### `int_type SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::VectorWriteStreambuf::overflow(int_type Ch) override`

**Parameters**

- `Ch`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::streamsize SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::VectorWriteStreambuf::xsputn(const char *Data, std::streamsize Count) override`

**Parameters**

- `Data`: 
- `Count`:
</div>
