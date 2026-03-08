#pragma once

/**
 * @file Profiling.h
 * @ingroup SnAPI_GameFramework
 * @brief Optional profiler-integration macros for instrumenting GameFramework code.
 *
 * These macros wrap the profiler backend behind compile-time feature gates so call sites
 * can stay uniform whether profiling support is enabled or compiled out.
 *
 * Disabled-build semantics:
 * - Every macro becomes a no-op expression.
 * - Call sites do not need additional `#if` guards.
 */

/**
 * @def SNAPI_GF_PROFILE_SCOPE
 * @ingroup SnAPI_GameFramework
 * @brief Instrument a named scope in the active profiler backend.
 * @param Name Scope label.
 * @param Category Profiler category or track identifier.
 */

/**
 * @def SNAPI_GF_PROFILE_FUNCTION
 * @ingroup SnAPI_GameFramework
 * @brief Instrument the current function scope using `__func__` as the scope name.
 * @param Category Profiler category or track identifier.
 */

/**
 * @def SNAPI_GF_PROFILE_SET_THREAD_NAME
 * @ingroup SnAPI_GameFramework
 * @brief Publish a human-readable name for the current thread to the profiler backend.
 * @param Name Thread label.
 */

/**
 * @def SNAPI_GF_PROFILE_BEGIN_FRAME_AUTO
 * @ingroup SnAPI_GameFramework
 * @brief Begin a profiler frame using the backend's automatic frame index.
 */

/**
 * @def SNAPI_GF_PROFILE_BEGIN_FRAME
 * @ingroup SnAPI_GameFramework
 * @brief Begin a profiler frame with an explicit frame index.
 * @param FrameIndex Frame identifier supplied by the caller.
 */

/**
 * @def SNAPI_GF_PROFILE_END_FRAME
 * @ingroup SnAPI_GameFramework
 * @brief End the current profiler frame.
 */

#if defined(SNAPI_GF_ENABLE_PROFILER) && SNAPI_GF_ENABLE_PROFILER

#include <SnAPI/Profiler/Profiler.h>
#include <SnAPI/Profiler/ProfilerMacros.h>

#define SNAPI_GF_PROFILE_SCOPE(Name, Category) SNAPI_PROFILE_SCOPE_CAT((Name), (Category))
#define SNAPI_GF_PROFILE_FUNCTION(Category) SNAPI_PROFILE_SCOPE_CAT(__func__, (Category))
#define SNAPI_GF_PROFILE_SET_THREAD_NAME(Name) SNAPI_PROFILE_SET_THREAD_NAME((Name))
#define SNAPI_GF_PROFILE_BEGIN_FRAME_AUTO() SNAPI_PROFILE_BEGIN_FRAME(::SnAPI::Profiler::kAutoFrameIndex)
#define SNAPI_GF_PROFILE_BEGIN_FRAME(FrameIndex) SNAPI_PROFILE_BEGIN_FRAME((FrameIndex))
#define SNAPI_GF_PROFILE_END_FRAME() static_cast<void>(SNAPI_PROFILE_END_FRAME())

#else

#define SNAPI_GF_PROFILE_SCOPE(Name, Category) (void)0
#define SNAPI_GF_PROFILE_FUNCTION(Category) (void)0
#define SNAPI_GF_PROFILE_SET_THREAD_NAME(Name) (void)0
#define SNAPI_GF_PROFILE_BEGIN_FRAME_AUTO() (void)0
#define SNAPI_GF_PROFILE_BEGIN_FRAME(FrameIndex) (void)0
#define SNAPI_GF_PROFILE_END_FRAME() (void)0

#endif
