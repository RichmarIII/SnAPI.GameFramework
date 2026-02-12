# SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::TypeVisitGuard

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::unordered_map<TypeId, bool, UuidHash>& SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::TypeVisitGuard::Visited`

Shared visited-type set for recursion/cycle detection.
</div>
<div class="snapi-api-card" markdown="1">
### `TypeId SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::TypeVisitGuard::Type`

Type currently being traversed.
</div>
<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::TypeVisitGuard::Inserted`

True when this guard inserted new visited entry.
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::TypeVisitGuard::TypeVisitGuard(std::unordered_map< TypeId, bool, UuidHash > &InVisited, const TypeId &InType)`

**Parameters**

- `InVisited`: 
- `InType`:
</div>
<div class="snapi-api-card" markdown="1">
### `SnAPI::GameFramework::anonymous_namespace{Serialization.cpp}::TypeVisitGuard::~TypeVisitGuard()`
</div>
