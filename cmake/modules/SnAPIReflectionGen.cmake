include_guard(GLOBAL)

function(snapi_add_reflection_codegen)
    if (NOT SNAPI_GF_BUILD_REFLECTION_GEN)
        return()
    endif()

    set(options)
    set(oneValueArgs TARGET OUTPUT SEED_SOURCE BUILD_DIR PROJECT_ROOT)
    set(multiValueArgs HEADERS EXTRA_ARGS)
    cmake_parse_arguments(SNAPI_REFLECT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT SNAPI_REFLECT_TARGET)
        message(FATAL_ERROR "snapi_add_reflection_codegen requires TARGET")
    endif()
    if (NOT SNAPI_REFLECT_OUTPUT)
        message(FATAL_ERROR "snapi_add_reflection_codegen requires OUTPUT")
    endif()
    if (NOT SNAPI_REFLECT_SEED_SOURCE)
        message(FATAL_ERROR "snapi_add_reflection_codegen requires SEED_SOURCE")
    endif()
    if (NOT SNAPI_REFLECT_BUILD_DIR)
        message(FATAL_ERROR "snapi_add_reflection_codegen requires BUILD_DIR")
    endif()
    if (NOT SNAPI_REFLECT_PROJECT_ROOT)
        set(SNAPI_REFLECT_PROJECT_ROOT "${CMAKE_SOURCE_DIR}")
    endif()

    set(SNAPI_REFLECT_OUTPUT_PATH "${SNAPI_REFLECT_OUTPUT}")
    if (NOT IS_ABSOLUTE "${SNAPI_REFLECT_OUTPUT_PATH}")
        set(SNAPI_REFLECT_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${SNAPI_REFLECT_OUTPUT_PATH}")
    endif()

    set(SNAPI_REFLECT_EXTRA_ARG_FLAGS)
    foreach(SNAPI_REFLECT_ARG IN LISTS SNAPI_REFLECT_EXTRA_ARGS)
        list(APPEND SNAPI_REFLECT_EXTRA_ARG_FLAGS --extra-arg "${SNAPI_REFLECT_ARG}")
    endforeach()

    add_custom_command(
        OUTPUT "${SNAPI_REFLECT_OUTPUT_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${CMAKE_CURRENT_BINARY_DIR}>"
        COMMAND $<TARGET_FILE:SnAPI.GameFramework.ReflectionGen>
            --output "$<SHELL_PATH:${SNAPI_REFLECT_OUTPUT_PATH}>"
            --build-dir "$<SHELL_PATH:${SNAPI_REFLECT_BUILD_DIR}>"
            --seed-source "$<SHELL_PATH:${SNAPI_REFLECT_SEED_SOURCE}>"
            --project-root "$<SHELL_PATH:${SNAPI_REFLECT_PROJECT_ROOT}>"
            ${SNAPI_REFLECT_EXTRA_ARG_FLAGS}
            ${SNAPI_REFLECT_HEADERS}
        DEPENDS
            SnAPI.GameFramework.ReflectionGen
            "${SNAPI_REFLECT_SEED_SOURCE}"
            ${SNAPI_REFLECT_HEADERS}
        VERBATIM
    )

    set_source_files_properties("${SNAPI_REFLECT_OUTPUT_PATH}" PROPERTIES GENERATED TRUE)
    target_sources(${SNAPI_REFLECT_TARGET} PRIVATE "${SNAPI_REFLECT_OUTPUT_PATH}")
endfunction()
