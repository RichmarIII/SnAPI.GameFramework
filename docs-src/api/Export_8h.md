# File `Export.h`

Shared-library visibility macro definitions for the GameFramework module.

This header centralizes symbol-visibility decoration so public API types and functions can be marked once with `SNAPI_GAMEFRAMEWORK_API` and then compile correctly for:
- Windows DLL export
- Windows DLL import
- ELF/Mach-O default visibility
- static-library or hidden-visibility builds
