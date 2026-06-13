cmake_minimum_required(VERSION 3.24)
project(SnAPI.GameFramework LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SNAPI_ENABLE_IPO "Enable interprocedural optimization for Release builds" ON)
option(SNAPI_GF_BUILD_TESTS "Build tests" ON)
option(SNAPI_GF_BUILD_EXAMPLES "Build examples" ON)
option(SNAPI_GF_BUILD_DOCS "Build docs (Doxygen + MkDocs)" ON)
option(SNAPI_GF_BUILD_EDITOR "Build editor target" ON)
option(SNAPI_GF_BUILD_RUNTIME "Build runtime engine target" ON)
option(SNAPI_GF_BUILD_BUILDCLI "Build the build/package CLI target" ON)
option(SNAPI_GF_BUILD_REFLECTION_GEN "Build the libclang-based reflection generator" ON)
option(SNAPI_GF_ENABLE_LUA "Enable Lua scripting" ON)
option(SNAPI_GF_ENABLE_SWIG "Enable SWIG bindings" ON)
option(SNAPI_GF_ENABLE_PROFILER "Enable SnAPI.Profiler integration/instrumentation" ON)
option(SNAPI_GF_INCLUDE_RENDERER_NEW_WORKSPACE "Load SnAPI.Renderer.New as a sibling CMake project for IDE visibility" ON)
set(SNAPI_PROJECT_ROOT_DIR "" CACHE PATH "Optional external project root integrated into SnAPI.GameFramework targets")

if (SNAPI_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT SNAPI_IPO_SUPPORTED OUTPUT SNAPI_IPO_OUTPUT LANGUAGES C CXX)
    if (SNAPI_IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL OFF)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)
    else()
        message(WARNING "SNAPI_ENABLE_IPO is ON, but IPO is not supported: ${SNAPI_IPO_OUTPUT}")
    endif()
endif()

if (SNAPI_GF_ENABLE_LUA AND NOT SNAPI_GF_ENABLE_SWIG)
    message(FATAL_ERROR "Lua scripting now requires SWIG bindings. Enable SNAPI_GF_ENABLE_SWIG.")
endif()

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(SnAPIReflectionGen)

add_library(SnAPI.GameFramework.FeatureSet INTERFACE)
target_compile_features(SnAPI.GameFramework.FeatureSet INTERFACE cxx_std_23)

if (SNAPI_GF_BUILD_REFLECTION_GEN)
    add_subdirectory(Modules/ReflectionGen)
endif()

add_subdirectory(Modules/GameFramework)

if (SNAPI_GF_BUILD_RUNTIME)
    add_subdirectory(Modules/Runtime)
endif()

if (SNAPI_GF_BUILD_BUILDCLI)
    add_subdirectory(Modules/Build)
endif()

if (SNAPI_GF_BUILD_EDITOR)
    add_subdirectory(Modules/Editor)
endif()

if (SNAPI_PROJECT_ROOT_DIR)
    get_filename_component(SNAPI_PROJECT_ROOT_DIR "${SNAPI_PROJECT_ROOT_DIR}" ABSOLUTE)
    file(TO_CMAKE_PATH "${SNAPI_PROJECT_ROOT_DIR}" SNAPI_PROJECT_ROOT_DIR)
    set(SNAPI_GF_EXTERNAL_PROJECT_CODE_DIR "${SNAPI_PROJECT_ROOT_DIR}/Modules")
    if (EXISTS "${SNAPI_GF_EXTERNAL_PROJECT_CODE_DIR}/CMakeLists.txt")
        add_subdirectory("${SNAPI_GF_EXTERNAL_PROJECT_CODE_DIR}" "${CMAKE_BINARY_DIR}/ExternalProject/Modules")
    else()
        message(WARNING
            "SNAPI_PROJECT_ROOT_DIR was set to '${SNAPI_PROJECT_ROOT_DIR}', but "
            "'${SNAPI_GF_EXTERNAL_PROJECT_CODE_DIR}/CMakeLists.txt' was not found. "
            "External project modules will not be integrated.")
    endif()
endif()

# Tests
if (SNAPI_GF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if (SNAPI_GF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if (SNAPI_GF_BUILD_DOCS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    set(SNAPI_GF_DOCS_VENV_DIR "${CMAKE_BINARY_DIR}/docs-venv")
    if (WIN32)
        set(SNAPI_GF_DOCS_PYTHON "${SNAPI_GF_DOCS_VENV_DIR}/Scripts/python.exe")
        set(SNAPI_GF_DOCS_PIP "${SNAPI_GF_DOCS_VENV_DIR}/Scripts/pip.exe")
        set(SNAPI_GF_DOCS_MKDOCS "${SNAPI_GF_DOCS_VENV_DIR}/Scripts/mkdocs.exe")
    else()
        set(SNAPI_GF_DOCS_PYTHON "${SNAPI_GF_DOCS_VENV_DIR}/bin/python")
        set(SNAPI_GF_DOCS_PIP "${SNAPI_GF_DOCS_VENV_DIR}/bin/pip")
        set(SNAPI_GF_DOCS_MKDOCS "${SNAPI_GF_DOCS_VENV_DIR}/bin/mkdocs")
    endif()

    if (NOT EXISTS "${SNAPI_GF_DOCS_PYTHON}")
        execute_process(
            COMMAND ${Python3_EXECUTABLE} -m venv "${SNAPI_GF_DOCS_VENV_DIR}"
            RESULT_VARIABLE SNAPI_GF_DOCS_VENV_RESULT
        )
        if (NOT SNAPI_GF_DOCS_VENV_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to create docs virtual environment.")
        endif()
    endif()

    set(SNAPI_GF_DOCS_REQUIREMENTS "${CMAKE_SOURCE_DIR}/docs-requirements.txt")
    set(SNAPI_GF_DOCS_VENV_MARKER "${SNAPI_GF_DOCS_VENV_DIR}/.snapi_docs_ready")
    if (NOT EXISTS "${SNAPI_GF_DOCS_VENV_MARKER}"
        OR "${SNAPI_GF_DOCS_REQUIREMENTS}" IS_NEWER_THAN "${SNAPI_GF_DOCS_VENV_MARKER}")
        execute_process(
            COMMAND ${SNAPI_GF_DOCS_PYTHON} -m pip install --disable-pip-version-check -r "${SNAPI_GF_DOCS_REQUIREMENTS}"
            RESULT_VARIABLE SNAPI_GF_DOCS_PIP_RESULT
        )
        if (NOT SNAPI_GF_DOCS_PIP_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to install MkDocs dependencies.")
        endif()
        file(WRITE "${SNAPI_GF_DOCS_VENV_MARKER}" "ok")
    endif()

    find_package(Doxygen QUIET)
    if (NOT DOXYGEN_FOUND)
        include(FetchContent)
        set(SNAPI_GF_DOXYGEN_VERSION "1.16.1")
        if (WIN32)
            set(SNAPI_GF_DOXYGEN_ARCHIVE "doxygen-${SNAPI_GF_DOXYGEN_VERSION}.windows.x64.bin.zip")
            set(SNAPI_GF_DOXYGEN_BIN_REL "doxygen.exe")
        elseif(APPLE)
            if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64" OR CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
                set(SNAPI_GF_DOXYGEN_ARCHIVE "doxygen-${SNAPI_GF_DOXYGEN_VERSION}-mac-arm.zip")
            else()
                set(SNAPI_GF_DOXYGEN_ARCHIVE "doxygen-${SNAPI_GF_DOXYGEN_VERSION}-mac-intel.zip")
            endif()
            set(SNAPI_GF_DOXYGEN_BIN_REL "doxygen-${SNAPI_GF_DOXYGEN_VERSION}/doxygen")
        else()
            set(SNAPI_GF_DOXYGEN_ARCHIVE "doxygen-${SNAPI_GF_DOXYGEN_VERSION}.linux.bin.tar.gz")
            set(SNAPI_GF_DOXYGEN_BIN_REL "doxygen-${SNAPI_GF_DOXYGEN_VERSION}/bin/doxygen")
        endif()

        string(REPLACE "." "_" SNAPI_GF_DOXYGEN_TAG_SUFFIX "${SNAPI_GF_DOXYGEN_VERSION}")
        set(SNAPI_GF_DOXYGEN_TAG "Release_${SNAPI_GF_DOXYGEN_TAG_SUFFIX}")
        set(SNAPI_GF_DOXYGEN_URL
            "https://github.com/doxygen/doxygen/releases/download/${SNAPI_GF_DOXYGEN_TAG}/${SNAPI_GF_DOXYGEN_ARCHIVE}")

        FetchContent_Declare(snapi_gf_doxygen URL ${SNAPI_GF_DOXYGEN_URL})
        FetchContent_MakeAvailable(snapi_gf_doxygen)

        set(DOXYGEN_EXECUTABLE "${snapi_gf_doxygen_SOURCE_DIR}/${SNAPI_GF_DOXYGEN_BIN_REL}")
        if (EXISTS "${DOXYGEN_EXECUTABLE}" AND NOT WIN32)
            execute_process(COMMAND ${CMAKE_COMMAND} -E chmod +x "${DOXYGEN_EXECUTABLE}")
        endif()
        if (NOT EXISTS "${DOXYGEN_EXECUTABLE}")
            file(GLOB_RECURSE SNAPI_GF_DOXYGEN_CANDIDATES
                "${snapi_gf_doxygen_SOURCE_DIR}/doxygen"
                "${snapi_gf_doxygen_SOURCE_DIR}/doxygen.exe"
                "${snapi_gf_doxygen_SOURCE_DIR}/**/doxygen"
                "${snapi_gf_doxygen_SOURCE_DIR}/**/doxygen.exe")
            list(LENGTH SNAPI_GF_DOXYGEN_CANDIDATES SNAPI_GF_DOXYGEN_CANDIDATE_COUNT)
            if (SNAPI_GF_DOXYGEN_CANDIDATE_COUNT GREATER 0)
                list(GET SNAPI_GF_DOXYGEN_CANDIDATES 0 DOXYGEN_EXECUTABLE)
            endif()
        endif()
        if (NOT EXISTS "${DOXYGEN_EXECUTABLE}")
            message(FATAL_ERROR "Failed to locate downloaded Doxygen binary.")
        endif()
        set(DOXYGEN_FOUND TRUE)
    endif()

    if (NOT EXISTS "${SNAPI_GF_DOCS_MKDOCS}")
        message(FATAL_ERROR "MkDocs was not installed into the docs virtual environment.")
    endif()

    set(SNAPI_GF_DOXYGEN_OUTPUT "${CMAKE_BINARY_DIR}/docs-doxygen")
    set(SNAPI_GF_DOXYGEN_INPUT_DIRS
        "${CMAKE_SOURCE_DIR}/Modules"
        "${CMAKE_SOURCE_DIR}/examples")
    string(JOIN " " SNAPI_GF_DOXYGEN_INPUT ${SNAPI_GF_DOXYGEN_INPUT_DIRS})
    set(SNAPI_GF_DOXYGEN_XML_DIR "${SNAPI_GF_DOXYGEN_OUTPUT}/xml")

    set(SNAPI_GF_DOXYFILE_IN "${CMAKE_SOURCE_DIR}/docs-src/Doxyfile.in")
    set(SNAPI_GF_DOXYFILE_OUT "${CMAKE_BINARY_DIR}/Doxyfile.SnAPI.GameFramework")

    configure_file(${SNAPI_GF_DOXYFILE_IN} ${SNAPI_GF_DOXYFILE_OUT} @ONLY)

    add_custom_target(SnAPI.GameFramework.Docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${SNAPI_GF_DOXYFILE_OUT}
        COMMAND ${SNAPI_GF_DOCS_PYTHON} ${CMAKE_SOURCE_DIR}/tools/GenerateMkDocsApi.py
            --xml-dir ${SNAPI_GF_DOXYGEN_XML_DIR}
            --out-dir ${CMAKE_SOURCE_DIR}/docs-src/api
        COMMAND ${SNAPI_GF_DOCS_MKDOCS} build -f ${CMAKE_SOURCE_DIR}/mkdocs.yml --clean
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating SnAPI.GameFramework documentation"
        VERBATIM
    )
endif()
