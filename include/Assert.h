#pragma once

#include <cstdlib>
#include <format>
#include <iostream>
#include <string>

namespace SnAPI::GameFramework::detail
{
/**
 * @brief Internal handler for failed debug assertions.
 * @param File Source file where the assertion failed.
 * @param Line Line number where the assertion failed.
 * @param Condition Stringified assertion condition.
 * @param Message Formatted diagnostic message.
 * @remarks This is only invoked by the DEBUG_ASSERT macro in debug builds.
 * @note This function always terminates the process via std::abort().
 */
inline void DebugAssertFail(const char* File, int Line, const char* Condition, const std::string& Message)
{
    std::cerr << "DEBUG_ASSERT failed: " << Condition << "\n"
              << "  File: " << File << ":" << Line << "\n"
              << "  Message: " << Message << std::endl;
    std::abort();
}
} // namespace SnAPI::GameFramework::detail

#ifndef NDEBUG
/**
 * @brief Debug-only assertion with formatted diagnostic message.
 * @param condition Expression that must evaluate to true.
 * @param fmt std::format-style format string.
 * @remarks When the condition is false, a detailed message is printed and the
 * process is aborted.
 * @note Compiled out when NDEBUG is defined.
 */
    #define DEBUG_ASSERT(condition, fmt, ...) \
        do { \
            if (!(condition)) { \
                ::SnAPI::GameFramework::detail::DebugAssertFail( \
                    __FILE__, __LINE__, #condition, std::format((fmt) __VA_OPT__(,) __VA_ARGS__)); \
            } \
        } while (0)
#else
/**
 * @brief Release-build no-op for DEBUG_ASSERT.
 * @param condition Expression that is ignored in release builds.
 * @param fmt Unused.
 * @remarks Keeps call sites intact while removing runtime cost.
 * @note Evaluates sizeof(condition) to avoid unused warnings.
 */
    #define DEBUG_ASSERT(condition, fmt, ...) do { (void)sizeof(condition); } while (0)
#endif
