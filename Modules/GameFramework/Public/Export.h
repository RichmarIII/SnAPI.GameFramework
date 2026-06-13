#pragma once

/**
 * @file Export.h
 * @ingroup SnAPI_GameFramework
 * @brief Shared-library visibility macro definitions for the GameFramework module.
 *
 * This header centralizes symbol-visibility decoration so public API types and functions
 * can be marked once with `SNAPI_GAMEFRAMEWORK_API` and then compile correctly for:
 * - Windows DLL export
 * - Windows DLL import
 * - ELF/Mach-O default visibility
 * - static-library or hidden-visibility builds
 */

/**
 * @def SNAPI_GAMEFRAMEWORK_API
 * @ingroup SnAPI_GameFramework
 * @brief Visibility decoration for public GameFramework symbols.
 *
 * Usage guidance:
 * - Apply this macro to classes, functions, and global variables that are part of the
 *   module's public ABI.
 * - Do not apply it to purely internal symbols.
 *
 * Build semantics:
 * - `SNAPI_GAMEFRAMEWORK_BUILD_DLL` exports symbols while building the module itself.
 * - `SNAPI_GAMEFRAMEWORK_USE_DLL` imports symbols when consuming the DLL on Windows.
 * - On GCC/Clang platforms, the macro maps to default symbol visibility when available.
 * - In static-library style builds, the macro expands to nothing.
 */
#if defined(_WIN32)
    #if defined(SNAPI_GAMEFRAMEWORK_BUILD_DLL)
        #define SNAPI_GAMEFRAMEWORK_API __declspec(dllexport)
    #elif defined(SNAPI_GAMEFRAMEWORK_USE_DLL)
        #define SNAPI_GAMEFRAMEWORK_API __declspec(dllimport)
    #else
        #define SNAPI_GAMEFRAMEWORK_API
    #endif
#else
    #if defined(__GNUC__) || defined(__clang__)
        #define SNAPI_GAMEFRAMEWORK_API __attribute__((visibility("default")))
    #else
        #define SNAPI_GAMEFRAMEWORK_API
    #endif
#endif
