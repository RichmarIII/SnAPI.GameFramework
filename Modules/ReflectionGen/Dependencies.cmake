# ReflectionGen module dependency wiring.

find_path(SNAPI_GF_LIBCLANG_INCLUDE_DIR clang-c/Index.h)
find_library(SNAPI_GF_LIBCLANG_LIBRARY NAMES clang libclang libclang.so clang-21 libclang-21)
if (NOT SNAPI_GF_LIBCLANG_INCLUDE_DIR OR NOT SNAPI_GF_LIBCLANG_LIBRARY)
    message(FATAL_ERROR "SNAPI_GF_BUILD_REFLECTION_GEN is ON, but libclang headers/library were not found.")
endif()
find_package(Threads REQUIRED)

function(SnAPIGameFrameworkApplyReflectionGenDependencies TargetName)
    if (NOT TARGET ${TargetName})
        message(FATAL_ERROR "SnAPIGameFrameworkApplyReflectionGenDependencies expected target ${TargetName} to exist.")
    endif()

    target_include_directories(${TargetName} PRIVATE ${SNAPI_GF_LIBCLANG_INCLUDE_DIR})
    target_link_libraries(${TargetName}
        PRIVATE
            ${SNAPI_GF_LIBCLANG_LIBRARY}
            Threads::Threads
    )
endfunction()
