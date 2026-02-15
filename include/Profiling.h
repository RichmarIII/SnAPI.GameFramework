#pragma once

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
