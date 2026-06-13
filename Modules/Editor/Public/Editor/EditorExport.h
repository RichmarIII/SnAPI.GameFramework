#pragma once

/**
 * @ingroup SnAPI_GameFramework_Editor
 * @def SNAPI_GAMEFRAMEWORK_EDITOR_API
 * @brief Symbol visibility macro for the editor shared-library boundary.
 *
 * This macro decorates public editor types and functions that must remain visible when
 * `SnAPI.GameFramework.Editor` is built as a shared library. It expands to:
 * - `__declspec(dllexport)` while building the DLL on Windows
 * - `__declspec(dllimport)` while consuming the DLL on Windows
 * - default symbol visibility attributes on GCC/Clang
 * - an empty definition for static-library style builds
 *
 * The macro has no runtime cost. Its only purpose is ABI visibility across compilation units.
 */
#if defined(_WIN32)
    #if defined(SNAPI_GAMEFRAMEWORK_EDITOR_BUILD_DLL)
        #define SNAPI_GAMEFRAMEWORK_EDITOR_API __declspec(dllexport)
    #elif defined(SNAPI_GAMEFRAMEWORK_EDITOR_USE_DLL)
        #define SNAPI_GAMEFRAMEWORK_EDITOR_API __declspec(dllimport)
    #else
        #define SNAPI_GAMEFRAMEWORK_EDITOR_API
    #endif
#else
    #if defined(__GNUC__) || defined(__clang__)
        #define SNAPI_GAMEFRAMEWORK_EDITOR_API __attribute__((visibility("default")))
    #else
        #define SNAPI_GAMEFRAMEWORK_EDITOR_API
    #endif
#endif
