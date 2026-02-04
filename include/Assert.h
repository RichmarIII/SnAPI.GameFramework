#pragma once

#include <cstdlib>
#include <format>
#include <iostream>
#include <string>

namespace SnAPI::GameFramework::detail
{
inline void DebugAssertFail(const char* File, int Line, const char* Condition, const std::string& Message)
{
    std::cerr << "DEBUG_ASSERT failed: " << Condition << "\n"
              << "  File: " << File << ":" << Line << "\n"
              << "  Message: " << Message << std::endl;
    std::abort();
}
} // namespace SnAPI::GameFramework::detail

#ifndef NDEBUG
    #define DEBUG_ASSERT(condition, fmt, ...) \
        do { \
            if (!(condition)) { \
                ::SnAPI::GameFramework::detail::DebugAssertFail( \
                    __FILE__, __LINE__, #condition, std::format((fmt) __VA_OPT__(,) __VA_ARGS__)); \
            } \
        } while (0)
#else
    #define DEBUG_ASSERT(condition, fmt, ...) do { (void)sizeof(condition); } while (0)
#endif
