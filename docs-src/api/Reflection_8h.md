# File `Reflection.h`

Umbrella header for the core runtime reflection facilities used by GameFramework.

Including this header brings in the fundamental reflection surface:
- `TypeRegistry` for reflected type metadata lookup
- `TTypeBuilder` for registration/build-time metadata declaration
- `Variant` and related view types for erased runtime values

This header is intended as the "just give me reflection" include for users who do not want to track the lower-level reflection headers individually.
