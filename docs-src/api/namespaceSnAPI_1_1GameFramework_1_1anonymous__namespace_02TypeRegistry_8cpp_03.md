# SnAPI::GameFramework::anonymous_namespace{TypeRegistry.cpp}

## Functions

<div class="snapi-api-card" markdown="1">
### `bool SnAPI::GameFramework::anonymous_namespace{TypeRegistry.cpp}::IsAUnlocked(const std::unordered_map< TypeId, TypeInfo, UuidHash > &Types, const TypeId &Type, const TypeId &Base)`

**Parameters**

- `Types`: 
- `Type`: 
- `Base`:
</div>
<div class="snapi-api-card" markdown="1">
### `void SnAPI::GameFramework::anonymous_namespace{TypeRegistry.cpp}::BuildLineageUnlocked(const std::unordered_map< TypeId, TypeInfo, UuidHash > &Types, const TypeId &Type, std::unordered_set< TypeId, UuidHash > &Visited, std::vector< const TypeInfo * > &OutLineage)`

**Parameters**

- `Types`: 
- `Type`: 
- `Visited`: 
- `OutLineage`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< ReflectedFieldRef > SnAPI::GameFramework::anonymous_namespace{TypeRegistry.cpp}::CollectFieldsUnlocked(const std::unordered_map< TypeId, TypeInfo, UuidHash > &Types, const TypeId &Type, const bool IncludeBaseTypes)`

**Parameters**

- `Types`: 
- `Type`: 
- `IncludeBaseTypes`:
</div>
<div class="snapi-api-card" markdown="1">
### `std::vector< ReflectedMethodRef > SnAPI::GameFramework::anonymous_namespace{TypeRegistry.cpp}::CollectMethodsUnlocked(const std::unordered_map< TypeId, TypeInfo, UuidHash > &Types, const TypeId &Type, const bool IncludeBaseTypes)`

**Parameters**

- `Types`: 
- `Type`: 
- `IncludeBaseTypes`:
</div>
