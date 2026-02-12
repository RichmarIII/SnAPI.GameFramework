cmake_minimum_required(VERSION 3.24)
project(SnAPI.GameFramework LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SNAPI_GF_BUILD_TESTS "Build tests" ON)
option(SNAPI_GF_BUILD_EXAMPLES "Build examples" ON)
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
    src/Level.cpp
    src/NodeGraph.cpp
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
