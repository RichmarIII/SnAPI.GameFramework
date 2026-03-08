# SnAPI::GameFramework::NonPolymorphicRuntimeType

Compile-time contract for types that may live in the dense ECS runtime.

Runtime objects are intentionally constrained:
- they must be class types
- they must be non-polymorphic
- they must expose a static reflected name through `TTypeNameV<T>`

The main design goal is hot-path storage and ticking without vtable dispatch or heap indirection per object.
