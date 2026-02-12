cmake_minimum_required(VERSION 3.24)
project(SnAPI.GameFramework LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SNAPI_GF_BUILD_TESTS "Build tests" ON)
option(SNAPI_GF_BUILD_EXAMPLES "Build examples" ON)
option(SNAPI_GF_BUILD_DOCS "Build docs (Doxygen + MkDocs)" OFF)
option(SNAPI_GF_ENABLE_LUA "Enable Lua scripting" ON)
option(SNAPI_GF_ENABLE_SWIG "Enable SWIG bindings" OFF)

include(FetchContent)

# stduuid
FetchContent_Declare(
    stduuid
    GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
    GIT_TAG v1.2.3
)
FetchContent_MakeAvailable(stduuid)

set(SNAPI_GF_SOURCES
    src/AudioListenerComponent.cpp
    src/AudioSourceComponent.cpp
    src/AudioSystem.cpp
    src/AssetPipelinePlugin.cpp
    src/AssetPipelineFactories.cpp
    src/AssetPipelineSerializers.cpp
    src/BaseNode.cpp
    src/GameFramework.cpp
    src/IComponent.cpp
    src/Level.cpp
    src/NodeGraph.cpp
    src/NetworkSystem.cpp
    src/NetReplication.cpp
    src/NetRpc.cpp
    src/Serialization.cpp
    src/ScriptABI.cpp
    src/TypeAutoRegistry.cpp
    src/TypeRegistry.cpp
    src/World.cpp
)

add_library(SnAPI.GameFramework ${SNAPI_GF_SOURCES})

target_include_directories(SnAPI.GameFramework PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(SnAPI.GameFramework PUBLIC stduuid)

target_compile_definitions(SnAPI.GameFramework PRIVATE SNAPI_GAMEFRAMEWORK_BUILD_DLL)

# Lua
if (SNAPI_GF_ENABLE_LUA)
    FetchContent_Declare(
        lua
        GIT_REPOSITORY https://github.com/lua/lua.git
        GIT_TAG v5.4.6
    )
    FetchContent_GetProperties(lua)
    if (NOT lua_POPULATED)
        FetchContent_Populate(lua)
    endif()

    set(LUA_SOURCES
        ${lua_SOURCE_DIR}/lapi.c
        ${lua_SOURCE_DIR}/lauxlib.c
        ${lua_SOURCE_DIR}/lbaselib.c
        ${lua_SOURCE_DIR}/lcode.c
        ${lua_SOURCE_DIR}/lcorolib.c
        ${lua_SOURCE_DIR}/lctype.c
        ${lua_SOURCE_DIR}/ldblib.c
        ${lua_SOURCE_DIR}/ldebug.c
        ${lua_SOURCE_DIR}/ldo.c
        ${lua_SOURCE_DIR}/ldump.c
        ${lua_SOURCE_DIR}/lfunc.c
        ${lua_SOURCE_DIR}/lgc.c
        ${lua_SOURCE_DIR}/linit.c
        ${lua_SOURCE_DIR}/liolib.c
        ${lua_SOURCE_DIR}/llex.c
        ${lua_SOURCE_DIR}/lmathlib.c
        ${lua_SOURCE_DIR}/lmem.c
        ${lua_SOURCE_DIR}/loadlib.c
        ${lua_SOURCE_DIR}/lobject.c
        ${lua_SOURCE_DIR}/lopcodes.c
        ${lua_SOURCE_DIR}/loslib.c
        ${lua_SOURCE_DIR}/lparser.c
        ${lua_SOURCE_DIR}/lstate.c
        ${lua_SOURCE_DIR}/lstring.c
        ${lua_SOURCE_DIR}/lstrlib.c
        ${lua_SOURCE_DIR}/ltable.c
        ${lua_SOURCE_DIR}/ltablib.c
        ${lua_SOURCE_DIR}/ltm.c
        ${lua_SOURCE_DIR}/lundump.c
        ${lua_SOURCE_DIR}/lutf8lib.c
        ${lua_SOURCE_DIR}/lvm.c
        ${lua_SOURCE_DIR}/lzio.c
    )

    add_library(lua STATIC ${LUA_SOURCES})
    target_include_directories(lua PUBLIC ${lua_SOURCE_DIR})
    if (UNIX)
        target_compile_definitions(lua PRIVATE LUA_USE_POSIX)
    endif()
    target_link_libraries(SnAPI.GameFramework PUBLIC lua)
endif()

# AssetPipeline integration (required)
set(SNAPI_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(SNAPI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SNAPI_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
set(SNAPI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

set(SNAPI_GF_ASSETPipeline_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.AssetPipeline")
if (SNAPI_GF_ASSETPipeline_SOURCE_DIR)
    FetchContent_Declare(
        SnAPI.AssetPipeline
        SOURCE_DIR ${SNAPI_GF_ASSETPipeline_SOURCE_DIR}
    )
elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.AssetPipeline/CMakeLists.txt")
    FetchContent_Declare(
        SnAPI.AssetPipeline
        SOURCE_DIR /mnt/Dev/CodeProjects/SnAPI.AssetPipeline
    )
else()
    FetchContent_Declare(
        SnAPI.AssetPipeline
        GIT_REPOSITORY git@github.com:RichmarIII/SnAPI.AssetPipeline.git
        GIT_TAG master
    )
endif()
FetchContent_MakeAvailable(SnAPI.AssetPipeline)
target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.AssetPipeline)
target_link_libraries(SnAPI.GameFramework PUBLIC cereal)
target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_ASSETPipeline)

# SnAPI.Audio integration (required)
set(SNAPI_AUDIO_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(SNAPI_AUDIO_BUILD_BACKEND_MINIAUDIO ON CACHE BOOL "" FORCE)
set(SNAPI_AUDIO_ENABLE_FLAC OFF CACHE BOOL "" FORCE)
set(SNAPI_AUDIO_ENABLE_MP3 OFF CACHE BOOL "" FORCE)
set(SNAPI_AUDIO_ENABLE_VORBIS OFF CACHE BOOL "" FORCE)

set(SNAPI_GF_AUDIO_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.Audio")
if (SNAPI_GF_AUDIO_SOURCE_DIR)
    FetchContent_Declare(
        SnAPI.Audio
        SOURCE_DIR ${SNAPI_GF_AUDIO_SOURCE_DIR}
    )
elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.Audio/CMakeLists.txt")
    FetchContent_Declare(
        SnAPI.Audio
        SOURCE_DIR /mnt/Dev/CodeProjects/SnAPI.Audio
    )
else()
    FetchContent_Declare(
        SnAPI.Audio
        GIT_REPOSITORY git@github.com:RichmarIII/SnAPI.Audio.git
        GIT_TAG master
    )
endif()
FetchContent_MakeAvailable(SnAPI.Audio)
target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.Audio SnAPI.Audio.Backend.Miniaudio)
target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_AUDIO)

# SnAPI.Networking integration (required)
set(SNAPI_NETWORKING_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SNAPI_NETWORKING_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SNAPI_NETWORKING_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(SNAPI_NETWORKING_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(SNAPI_NETWORKING_ENABLE_FUZZ OFF CACHE BOOL "" FORCE)

set(SNAPI_GF_NETWORKING_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.Networking")
if (SNAPI_GF_NETWORKING_SOURCE_DIR)
    FetchContent_Declare(
        SnAPI.Networking
        SOURCE_DIR ${SNAPI_GF_NETWORKING_SOURCE_DIR}
    )
elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.Networking/CMakeLists.txt")
    FetchContent_Declare(
        SnAPI.Networking
        SOURCE_DIR /mnt/Dev/CodeProjects/SnAPI.Networking
    )
else()
    FetchContent_Declare(
        SnAPI.Networking
        GIT_REPOSITORY git@github.com:RichmarIII/SnAPI.Networking.git
        GIT_TAG master
    )
endif()
FetchContent_MakeAvailable(SnAPI.Networking)
target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.Networking)
target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_NETWORKING)

# SWIG bindings (optional)
if (SNAPI_GF_ENABLE_SWIG)
    find_package(SWIG REQUIRED)
    include(UseSWIG)
endif()

# Tests
if (SNAPI_GF_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.0
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

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
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/src"
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
