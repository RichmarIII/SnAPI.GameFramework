# GameFramework module dependency wiring.

include(FetchContent)

get_filename_component(SNAPI_GF_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Use double-precision math for the GameFramework dependency graph.
set(SNAPI_MATH_USES_DOUBLE ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    stduuid
    GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
    GIT_TAG v1.2.3
)
FetchContent_MakeAvailable(stduuid)

if (NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

set(SNAPI_GF_MATH_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.Math")
if (SNAPI_GF_MATH_SOURCE_DIR)
    FetchContent_Declare(
        SnAPI.Math
        SOURCE_DIR ${SNAPI_GF_MATH_SOURCE_DIR}
    )
elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.Math/CMakeLists.txt")
    FetchContent_Declare(
        SnAPI.Math
        SOURCE_DIR /mnt/Dev/CodeProjects/SnAPI.Math
    )
else()
    FetchContent_Declare(
        SnAPI.Math
        GIT_REPOSITORY git@github.com:RichmarIII/SnAPI.Math.git
        GIT_TAG master
    )
endif()
FetchContent_MakeAvailable(SnAPI.Math)
FetchContent_GetProperties(SnAPI.Math SOURCE_DIR SNAPI_GF_MATH_SOURCE_ROOT)

if (SNAPI_GF_ENABLE_PROFILER)
    set(SNAPI_GF_EFFECTIVE_PROFILER_SOURCE_DIR "")
    set(SNAPI_PROFILER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SNAPI_PROFILER_ENABLE_DASHBOARD_TARGETS ON CACHE BOOL "" FORCE)
    set(SNAPI_GF_PROFILER_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.Profiler")
    if (SNAPI_GF_PROFILER_SOURCE_DIR)
        set(SNAPI_GF_EFFECTIVE_PROFILER_SOURCE_DIR ${SNAPI_GF_PROFILER_SOURCE_DIR})
        FetchContent_Declare(
            SnAPI.Profiler
            SOURCE_DIR ${SNAPI_GF_PROFILER_SOURCE_DIR}
        )
    elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.Profiler/CMakeLists.txt")
        set(SNAPI_GF_EFFECTIVE_PROFILER_SOURCE_DIR /mnt/Dev/CodeProjects/SnAPI.Profiler)
        FetchContent_Declare(
            SnAPI.Profiler
            SOURCE_DIR /mnt/Dev/CodeProjects/SnAPI.Profiler
        )
    else()
        FetchContent_Declare(
            SnAPI.Profiler
            GIT_REPOSITORY git@github.com:RichmarIII/SnAPI.Profiler.git
            GIT_TAG master
        )
    endif()
    FetchContent_MakeAvailable(SnAPI.Profiler)
    if (TARGET SnAPI.Profiler)
        set_target_properties(SnAPI.Profiler PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()

    if (NOT SNAPI_GF_EFFECTIVE_PROFILER_SOURCE_DIR)
        FetchContent_GetProperties(SnAPI.Profiler SOURCE_DIR SNAPI_GF_EFFECTIVE_PROFILER_SOURCE_DIR)
    endif()
endif()

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
endif()

set(SNAPI_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(SNAPI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SNAPI_BUILD_PLUGINS ON CACHE BOOL "" FORCE)
set(SNAPI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_OPENEXR OFF CACHE BOOL "" FORCE)

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
if (TARGET FreeImage)
    set_target_properties(FreeImage PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if (NOT TARGET TextureCompressorPlugin)
    message(FATAL_ERROR
        "TextureCompressorPlugin target is unavailable. "
        "Ensure SnAPI.AssetPipeline plugins are enabled and TextureCompressor plugin CMake is discoverable.")
endif()

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

set(SNAPI_PHYSICS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SNAPI_PHYSICS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SNAPI_PHYSICS_BACKEND_JOLT ON CACHE BOOL "" FORCE)

set(SNAPI_GF_PHYSICS_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.Physics")
if (SNAPI_GF_PHYSICS_SOURCE_DIR)
    FetchContent_Declare(
        SnAPI.Physics
        SOURCE_DIR ${SNAPI_GF_PHYSICS_SOURCE_DIR}
    )
elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.Physics/CMakeLists.txt")
    FetchContent_Declare(
        SnAPI.Physics
        SOURCE_DIR /mnt/Dev/CodeProjects/SnAPI.Physics
    )
else()
    FetchContent_Declare(
        SnAPI.Physics
        GIT_REPOSITORY git@github.com:RichmarIII/SnAPI.Physics.git
        GIT_TAG master
    )
endif()
FetchContent_MakeAvailable(SnAPI.Physics)

set(SNAPI_INPUT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SNAPI_INPUT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SNAPI_INPUT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
if (NOT DEFINED SNAPI_INPUT_ENABLE_BACKEND_SDL3)
    set(SNAPI_INPUT_ENABLE_BACKEND_SDL3 ON CACHE BOOL "Build SnAPI.Input SDL3 backend")
endif()
if (NOT DEFINED SNAPI_INPUT_ENABLE_BACKEND_HIDAPI)
    set(SNAPI_INPUT_ENABLE_BACKEND_HIDAPI ON CACHE BOOL "Build SnAPI.Input HIDAPI backend")
endif()
if (SNAPI_INPUT_ENABLE_BACKEND_SDL3 AND SNAPI_INPUT_ENABLE_BACKEND_HIDAPI)
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI_JOYSTICK OFF CACHE BOOL "" FORCE)
endif()

set(SNAPI_GF_INPUT_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.Input")
if (SNAPI_GF_INPUT_SOURCE_DIR)
    FetchContent_Declare(
        SnAPI.Input
        SOURCE_DIR ${SNAPI_GF_INPUT_SOURCE_DIR}
    )
elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.Input/CMakeLists.txt")
    FetchContent_Declare(
        SnAPI.Input
        SOURCE_DIR /mnt/Dev/CodeProjects/SnAPI.Input
    )
else()
    FetchContent_Declare(
        SnAPI.Input
        GIT_REPOSITORY git@github.com:RichmarIII/SnAPI.Input.git
        GIT_TAG master
    )
endif()
FetchContent_MakeAvailable(SnAPI.Input)

set(SNAPI_UI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

set(SNAPI_GF_UI_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.UI")
if (SNAPI_GF_UI_SOURCE_DIR)
    FetchContent_Declare(
        SnAPI.UI
        SOURCE_DIR ${SNAPI_GF_UI_SOURCE_DIR}
    )
elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.UI/CMakeLists.txt")
    FetchContent_Declare(
        SnAPI.UI
        SOURCE_DIR /mnt/Dev/CodeProjects/SnAPI.UI
    )
else()
    FetchContent_Declare(
        SnAPI.UI
        GIT_REPOSITORY git@github.com:RichmarIII/SnAPI.UI.git
        GIT_TAG master
    )
endif()
FetchContent_MakeAvailable(SnAPI.UI)

set(SNAPI_RENDERER_NEW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SNAPI_RENDERER_NEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SNAPI_GF_RENDERER_NEW_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.Renderer.New")

set(SNAPI_GF_RENDERER_NEW_ROOT "")
if (SNAPI_GF_RENDERER_NEW_SOURCE_DIR)
    set(SNAPI_GF_RENDERER_NEW_ROOT ${SNAPI_GF_RENDERER_NEW_SOURCE_DIR})
elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.Renderer.New/CMakeLists.txt")
    set(SNAPI_GF_RENDERER_NEW_ROOT "/mnt/Dev/CodeProjects/SnAPI.Renderer.New")
endif()

if (NOT SNAPI_GF_RENDERER_NEW_ROOT)
    message(FATAL_ERROR "SnAPI.Renderer.New source not found; set SNAPI_GF_RENDERER_NEW_SOURCE_DIR.")
endif()
if (NOT SNAPI_GF_INCLUDE_RENDERER_NEW_WORKSPACE)
    message(FATAL_ERROR "SnAPI.GameFramework requires SnAPI.Renderer.New; keep SNAPI_GF_INCLUDE_RENDERER_NEW_WORKSPACE enabled or provide an imported SnAPI.Renderer.New target before this block.")
endif()

FetchContent_Declare(
    SnAPI.Renderer.New
    SOURCE_DIR ${SNAPI_GF_RENDERER_NEW_ROOT}
)
FetchContent_MakeAvailable(SnAPI.Renderer.New)

if (NOT TARGET SnAPI.Renderer.New)
    message(FATAL_ERROR "SnAPI.Renderer.New source was found, but target SnAPI.Renderer.New was not created.")
endif()

function(SnAPIGameFrameworkApplyBaseDependencies TargetName)
    if (NOT TARGET ${TargetName})
        message(FATAL_ERROR "SnAPIGameFrameworkApplyBaseDependencies expected target ${TargetName} to exist.")
    endif()

    target_link_libraries(${TargetName}
        PUBLIC
            stduuid
            SnAPI.Math
            nlohmann_json::nlohmann_json
            SnAPI.AssetPipeline
            TextureCompressorPlugin
            cereal
            SnAPI.Audio
            SnAPI.Audio.Backend.Miniaudio
            SnAPI.Networking
            SnAPI.Physics
            SnAPI.Input
            SnAPI.UI
            SnAPI.Renderer.New
    )

    target_compile_definitions(${TargetName}
        PUBLIC
            EIGEN_MAX_ALIGN_BYTES=0
            EIGEN_MAX_STATIC_ALIGN_BYTES=0
            SNAPI_GF_ENABLE_ASSETPipeline
            SNAPI_GF_ENABLE_AUDIO
            SNAPI_GF_ENABLE_NETWORKING
            SNAPI_GF_ENABLE_PHYSICS
            SNAPI_GF_ENABLE_INPUT
            SNAPI_GF_ENABLE_UI
            SNAPI_GF_ENABLE_RENDERER
    )

    if (SNAPI_GF_ENABLE_PROFILER)
        target_link_libraries(${TargetName} PUBLIC SnAPI.Profiler)
        target_compile_definitions(${TargetName} PUBLIC SNAPI_GF_ENABLE_PROFILER=1)
    else()
        target_compile_definitions(${TargetName} PUBLIC SNAPI_GF_ENABLE_PROFILER=0)
    endif()

    if (SNAPI_GF_ENABLE_LUA)
        target_include_directories(${TargetName} BEFORE PUBLIC ${lua_SOURCE_DIR})
        target_link_libraries(${TargetName} PUBLIC lua)
        target_compile_definitions(${TargetName} PUBLIC SNAPI_GF_ENABLE_LUA=1)
    endif()

    if (TARGET SnAPI.Input.Backend.SDL3)
        target_link_libraries(${TargetName} PUBLIC SnAPI.Input.Backend.SDL3)
    endif()
    if (TARGET SnAPI.Input.HID.Backend.HIDAPI)
        target_link_libraries(${TargetName} PUBLIC SnAPI.Input.HID.Backend.HIDAPI)
    endif()
    if (TARGET SnAPI.Input.USB.Backend.LIBUSB)
        target_link_libraries(${TargetName} PUBLIC SnAPI.Input.USB.Backend.LIBUSB)
    endif()

    add_dependencies(${TargetName} SnAPI.Renderer.New)
    set_source_files_properties(
        "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Private/RendererSystemRendererNew.cpp"
        "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Private/RendererNewPassParamNodes.cpp"
        "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Private/Rendering/GameRenderCamera.cpp"
        "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Private/Rendering/GameRenderMesh.cpp"
        "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Private/Rendering/GameRenderObject.cpp"
        "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Private/Rendering/GameRenderOutput.cpp"
        "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Private/Rendering/GameRenderLight.cpp"
        "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Private/Rendering/GameRenderWindow.cpp"
        "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Private/MeshComponentsRendererNew.cpp"
        DIRECTORY "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}"
        PROPERTIES
        COMPILE_OPTIONS "-iquote;${SNAPI_GF_RENDERER_NEW_ROOT}/Modules/RenderCore/Public;-iquote;${SNAPI_GF_RENDERER_NEW_ROOT}/Modules/Platform/Public"
    )

    if (TARGET FreeImage)
        target_link_libraries(${TargetName} PRIVATE FreeImage)
        target_compile_definitions(${TargetName} PRIVATE SNAPI_GF_HAS_FREEIMAGE=1 HAS_FREEIMAGE=1)
    else()
        target_compile_definitions(${TargetName} PRIVATE SNAPI_GF_HAS_FREEIMAGE=0 HAS_FREEIMAGE=0)
    endif()

    if (SNAPI_GF_ENABLE_SWIG)
        SnAPIGameFrameworkApplySwigDependencies(${TargetName})
    endif()
endfunction()

function(SnAPIGameFrameworkApplySwigDependencies TargetName)
    find_package(SWIG REQUIRED)
    include(UseSWIG)

    if (SNAPI_GF_ENABLE_LUA)
        set(SNAPI_GF_SWIG_INTERFACE "${SNAPI_GF_REPO_ROOT}/swig/SnAPIGameFramework.i")
        if (EXISTS "${SNAPI_GF_SWIG_INTERFACE}")
            set_source_files_properties("${SNAPI_GF_SWIG_INTERFACE}" PROPERTIES
                CPLUSPLUS ON
                SWIG_INCLUDE_DIRECTORIES "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Public"
            )
            set(CMAKE_SWIG_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/swig/lua")

            swig_add_library(snapi_gf_lua
                TYPE STATIC
                LANGUAGE lua
                SOURCES "${SNAPI_GF_SWIG_INTERFACE}"
            )

            target_include_directories(${SWIG_MODULE_snapi_gf_lua_REAL_NAME}
                PRIVATE
                    "${SNAPI_GF_GAMEFRAMEWORK_MODULE_DIR}/Public"
            )

            target_link_libraries(${SWIG_MODULE_snapi_gf_lua_REAL_NAME}
                PRIVATE
                    lua
                    ${TargetName}
            )

            target_link_libraries(${TargetName} PRIVATE ${SWIG_MODULE_snapi_gf_lua_REAL_NAME})
        else()
            message(FATAL_ERROR "SNAPI_GF_ENABLE_SWIG is ON but ${SNAPI_GF_SWIG_INTERFACE} was not found.")
        endif()
    else()
        message(STATUS "SNAPI_GF_ENABLE_SWIG is ON but SNAPI_GF_ENABLE_LUA is OFF; scripting wrappers are skipped.")
    endif()
endfunction()

function(snapi_gf_link_renderer_new_static_group TargetName)
    if (NOT TARGET ${TargetName})
        return()
    endif()
    if (APPLE OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        return()
    endif()

    set(RendererNewStaticArchives)
    foreach(RendererNewStaticTarget IN ITEMS
        SnAPI.Renderer.New.Core
        SnAPI.Renderer.New.Platform
        SnAPI.Renderer.New.Backend.Registry
        SnAPI.Renderer.New.Backend.Null
        SnAPI.Renderer.New.Backend.Vulkan
        SnAPI.Renderer.New.Upscaling.FidelityFXFsr3
        SnAPI.Renderer.New.FidelityFX.FSR3
        SnAPI.Renderer.New.SceneImport.Assimp
    )
        if (TARGET ${RendererNewStaticTarget})
            list(APPEND RendererNewStaticArchives "$<TARGET_FILE:${RendererNewStaticTarget}>")
            add_dependencies(${TargetName} ${RendererNewStaticTarget})
        endif()
    endforeach()

    if (RendererNewStaticArchives)
        target_link_libraries(${TargetName}
            PRIVATE
                "-Wl,--start-group"
                ${RendererNewStaticArchives}
                "-Wl,--end-group"
        )
    endif()
endfunction()
