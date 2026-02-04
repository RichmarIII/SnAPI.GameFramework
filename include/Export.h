#pragma once

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
