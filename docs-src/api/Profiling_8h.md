# File `Profiling.h`

Optional profiler-integration macros for instrumenting GameFramework code.

These macros wrap the profiler backend behind compile-time feature gates so call sites can stay uniform whether profiling support is enabled or compiled out.

Disabled-build semantics:
- Every macro becomes a no-op expression.
- Call sites do not need additional `#if` guards.
