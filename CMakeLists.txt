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
option(SNAPI_GF_USE_RENDERER_NEW "Use SnAPI.Renderer.New as the active GameFramework renderer integration" ON)
option(SNAPI_GF_ENABLE_LEGACY_RENDERER "Build the legacy SnAPI.Renderer integration" OFF)
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

include(FetchContent)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(SnAPIReflectionGen)

# Use double-precision math across SnAPI modules.
set(SNAPI_MATH_USES_DOUBLE ON CACHE BOOL "" FORCE)
# Keep Eigen alignment policy consistent across all fetched targets in this build.
add_compile_definitions(EIGEN_MAX_ALIGN_BYTES=0 EIGEN_MAX_STATIC_ALIGN_BYTES=0)

# stduuid
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

# SnAPI.Math integration (required)
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

# SnAPI.Profiler integration (optional)
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
        # SnAPI.Profiler is consumed by shared objects (for example SnAPI.Renderer).
        set_target_properties(SnAPI.Profiler PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()

    if (NOT SNAPI_GF_EFFECTIVE_PROFILER_SOURCE_DIR)
        FetchContent_GetProperties(SnAPI.Profiler SOURCE_DIR SNAPI_GF_EFFECTIVE_PROFILER_SOURCE_DIR)
    endif()
endif()

set(SNAPI_GF_SOURCES
    src/AudioListenerComponent.cpp
    src/AudioSourceComponent.cpp
    src/AudioSystem.cpp
    src/AuthoredAssetRegistry.cpp
    src/AuthoredAssetJson.cpp
    src/AuthoredAssetLoading.cpp
    src/AuthoredAssetSourcePipeline.cpp
    src/AssetRef.cpp
    src/AssetPipelinePlugin.cpp
    src/AssetPipelineFactories.cpp
    src/AssetPipelineSerializers.cpp
    src/BaseNode.cpp
    src/BuildProfile.cpp
    src/BuildPlanner.cpp
    src/BuildRequest.cpp
    src/BuildCache.cpp
    src/BuildCli.cpp
    src/BuildExecution.cpp
    src/BuildHistory.cpp
    src/AssetCookServiceAdapter.cpp
    src/CodeBuildServiceAdapter.cpp
    src/CharacterMovementController.cpp
    src/CameraComponent.cpp
    src/ColliderComponent.cpp
    src/Conduit.cpp
    src/ConduitAsset.cpp
    src/ConduitClassComponent.cpp
    src/DirectionalLightComponent.cpp
    src/EnvironmentCaptureComponent.cpp
    src/EnvironmentProbeNode.cpp
    src/FollowTargetComponent.cpp
    src/FrameGraphNode.cpp
    src/GameplayHost.cpp
    src/GameplayRpcGateway.cpp
    src/GameFramework.cpp
    src/GameProjectRuntime.cpp
    src/GameRuntime.cpp
    src/BaseComponent.cpp
    src/InputComponent.cpp
    src/InputIntentComponent.cpp
    src/InputSystem.cpp
    src/UISystem.cpp
    src/UIPropertyPanel.cpp
    src/UIRenderViewport.cpp
    src/Level.cpp
    src/LocalPlayer.cpp
    src/LocalPlayerService.cpp
    src/MultiplayerConfigNode.cpp
    src/ModuleCreationService.cpp
    src/ModuleScaffoldingShared.cpp
    src/NodeAsset.cpp
    src/PackageManifest.cpp
    src/PackageOutput.cpp
    src/DeferredShadingParamsNode.cpp
    src/SSAOParamsNode.cpp
    src/SSGIParamsNode.cpp
    src/SSRParamsNode.cpp
    src/TAAParamsNode.cpp
    src/BloomParamsNode.cpp
    src/AtmosphereParamsNode.cpp
    src/AtmosphereCompositeParamsNode.cpp
    src/HeightFogParamsNode.cpp
    src/ToneMapParamsNode.cpp
    src/WorldRenderSettings.cpp
    src/PawnBase.cpp
    src/PlayerStart.cpp
    src/ProjectCreationService.cpp
    src/ProjectBuildGenerationShared.cpp
    src/ProjectDescriptor.cpp
    src/PluginCreationService.cpp
    src/PluginDescriptor.cpp
    src/NetworkSystem.cpp
    src/PhysicsSystem.cpp
    src/NetReplication.cpp
    src/NetRpc.cpp
    src/RendererSystem.cpp
    src/RenderAssetAssimpImporter.cpp
    src/RenderAssets/AnimationPayload.cpp
    src/RenderAssets/AssetRefPayload.cpp
    src/RenderAssets/ImportedAssetProvenancePayload.cpp
    src/RenderAssets/MaterialAsset.cpp
    src/RenderAssets/MaterialInstanceAsset.cpp
    src/RenderAssets/MeshImportSettingsPayload.cpp
    src/RenderAssets/MeshStreamSourcePayload.cpp
    src/RenderAssets/SkeletalAnimationAsset.cpp
    src/RenderAssets/SkeletalMeshAsset.cpp
    src/RenderAssets/SkeletalMeshPayload.cpp
    src/RenderAssets/SkeletonAsset.cpp
    src/RenderAssets/SkeletonPayload.cpp
    src/RenderAssets/StaticMeshAsset.cpp
    src/RenderAssets/StaticMeshPayload.cpp
    src/RenderAssets/TextureAsset.cpp
    src/RenderAssets/TextureSourceImagePayload.cpp
    src/RenderAssetSerializers/AnimationPayloadSerializer.cpp
    src/RenderAssetSerializers/MaterialInstancePayloadSerializer.cpp
    src/RenderAssetSerializers/MaterialPayloadSerializer.cpp
    src/RenderAssetSerializers/SkeletalMeshPayloadSerializer.cpp
    src/RenderAssetSerializers/SkeletalMeshSourcePayloadSerializer.cpp
    src/RenderAssetSerializers/SkeletonPayloadSerializer.cpp
    src/RenderAssetSerializers/StaticMeshPayloadSerializer.cpp
    src/RenderAssetSerializers/StaticMeshSourcePayloadSerializer.cpp
    src/RenderAssetSerializers/TextureSourcePayloadSerializer.cpp
    src/RenderAssetSourcePipeline.cpp
    src/RigidBodyComponent.cpp
    src/SkeletalMeshComponent.cpp
    src/SprintArmComponent.cpp
    src/StaticMeshComponent.cpp
    src/PathResolver.cpp
    src/TransformComponent.cpp
    src/Serialization.cpp
    src/ScriptABI.cpp
    src/ScriptComponent.cpp
    src/ScriptRuntime.cpp
    src/TypeAutoRegistry.cpp
    src/TypeRegistry.cpp
    src/World.cpp
)

if (SNAPI_GF_USE_RENDERER_NEW)
    list(REMOVE_ITEM SNAPI_GF_SOURCES
        src/DeferredShadingParamsNode.cpp
        src/SSAOParamsNode.cpp
        src/SSGIParamsNode.cpp
        src/SSRParamsNode.cpp
        src/TAAParamsNode.cpp
        src/BloomParamsNode.cpp
        src/AtmosphereParamsNode.cpp
        src/AtmosphereCompositeParamsNode.cpp
        src/HeightFogParamsNode.cpp
        src/ToneMapParamsNode.cpp
        src/RendererSystem.cpp
        src/SkeletalMeshComponent.cpp
        src/StaticMeshComponent.cpp
    )
    list(APPEND SNAPI_GF_SOURCES
        src/RendererNewPassParamNodeStubs.cpp
        src/RendererSystemRendererNew.cpp
        src/MeshComponentsRendererNew.cpp
    )
endif()

add_library(SnAPI.GameFramework ${SNAPI_GF_SOURCES})

target_include_directories(SnAPI.GameFramework PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(SnAPI.GameFramework PUBLIC stduuid SnAPI.Math nlohmann_json::nlohmann_json)

set(SNAPI_GF_DEFAULT_ASSET_ROOT "${CMAKE_BINARY_DIR}/Assets")
file(TO_CMAKE_PATH "${SNAPI_GF_DEFAULT_ASSET_ROOT}" SNAPI_GF_DEFAULT_ASSET_ROOT)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" SNAPI_GF_SOURCE_ROOT_DIR)
target_compile_definitions(SnAPI.GameFramework PUBLIC
    SNAPI_GF_DEFAULT_ASSET_ROOT="${SNAPI_GF_DEFAULT_ASSET_ROOT}"
)
target_compile_definitions(SnAPI.GameFramework PRIVATE
    SNAPI_GF_SOURCE_ROOT_DIR="${SNAPI_GF_SOURCE_ROOT_DIR}"
)

if (SNAPI_GF_ENABLE_PROFILER)
    target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.Profiler)
    target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_PROFILER=1)
else()
    target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_PROFILER=0)
endif()

if (SNAPI_GF_BUILD_RUNTIME)
    add_executable(SnAPI.GameFramework.Runtime
        src/Runtime/main.cpp
    )
    target_link_libraries(SnAPI.GameFramework.Runtime PRIVATE SnAPI.GameFramework)
endif()

if (SNAPI_GF_BUILD_BUILDCLI)
    add_executable(SnAPI.GameFramework.Build
        tools/BuildCliMain.cpp
    )
    target_link_libraries(SnAPI.GameFramework.Build PRIVATE SnAPI.GameFramework)
endif()

if (SNAPI_GF_ENABLE_SWIG)
    target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_SWIG=1)
else()
    target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_SWIG=0)
endif()

target_compile_definitions(SnAPI.GameFramework PRIVATE SNAPI_GAMEFRAMEWORK_BUILD_DLL)

if (SNAPI_GF_BUILD_REFLECTION_GEN)
    find_path(SNAPI_GF_LIBCLANG_INCLUDE_DIR clang-c/Index.h)
    find_library(SNAPI_GF_LIBCLANG_LIBRARY NAMES clang libclang libclang.so clang-21 libclang-21)
    if (NOT SNAPI_GF_LIBCLANG_INCLUDE_DIR OR NOT SNAPI_GF_LIBCLANG_LIBRARY)
        message(FATAL_ERROR "SNAPI_GF_BUILD_REFLECTION_GEN is ON, but libclang headers/library were not found.")
    endif()
    find_package(Threads REQUIRED)

    add_executable(SnAPI.GameFramework.ReflectionGen
        tools/ReflectionGen.cpp
    )
    target_include_directories(SnAPI.GameFramework.ReflectionGen PRIVATE ${SNAPI_GF_LIBCLANG_INCLUDE_DIR})
    target_link_libraries(SnAPI.GameFramework.ReflectionGen PRIVATE ${SNAPI_GF_LIBCLANG_LIBRARY} Threads::Threads)

    file(GLOB_RECURSE SNAPI_GF_REFLECTION_HEADERS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

snapi_add_reflection_codegen(
        TARGET SnAPI.GameFramework
        OUTPUT SnAPI.GameFramework.reflection.generated.cpp
        BUILD_DIR "${CMAKE_BINARY_DIR}"
        SEED_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/GameFramework.cpp"
        PROJECT_ROOT "${CMAKE_SOURCE_DIR}"
        HEADER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/include"
        GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/reflection/include"
        EXTRA_ARGS
            "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
            "-I${SNAPI_GF_MATH_SOURCE_ROOT}/include"
            "-I${stduuid_SOURCE_DIR}/include"
        HEADERS ${SNAPI_GF_REFLECTION_HEADERS}
    )
endif()

if (SNAPI_GF_BUILD_EDITOR)
    target_compile_definitions(SnAPI.GameFramework PUBLIC WITH_EDITOR=1)
else()
    target_compile_definitions(SnAPI.GameFramework PUBLIC WITH_EDITOR=0)
endif()

if (SNAPI_GF_BUILD_EDITOR)
    target_sources(SnAPI.GameFramework PRIVATE
        src/Editor/EditorCameraComponent.cpp
        src/Editor/EditorRenderPasses.cpp
    )

    set(SNAPI_GF_EDITOR_SOURCES
        include/Editor/EditorCameraComponent.h
        include/Editor/EditorAssetService.h
        include/Editor/EditorBuildService.h
        include/Editor/EditorExport.h
        include/Editor/EditorCoreServices.h
        include/Editor/EditorLayout.h
        include/Editor/EditorRenderPasses.h
        include/Editor/EditorSelectionModel.h
        include/Editor/EditorSceneBootstrap.h
        include/Editor/EditorTheme.h
        include/Editor/EditorViewportBinding.h
        include/Editor/GameEditor.h
        include/Editor/GameFrameworkEditor.hpp
        include/Editor/IEditorService.h
        include/Editor/EditorWorld.h
        include/Conduit/Editor.h
        include/Conduit/Editor/Types.h
        include/Conduit/Editor/Document.h
        include/Conduit/Editor/Schema.h
        include/Conduit/Editor/CompilerBridge.h
        include/Conduit/Editor/GraphCanvas.h
        include/Conduit/Editor/Service.h
        src/Editor/main.cpp
        src/Editor/EditorCameraComponent.cpp
        src/Editor/EditorAssetService.cpp
        src/Editor/EditorBuildService.cpp
        src/Editor/EditorAssetIconService.cpp
        src/Editor/ConduitEditor.cpp
        src/Editor/ConduitGraphCanvas.cpp
        src/Editor/EditorCommandService.cpp
        src/Editor/EditorGameViewportOverlayService.cpp
        src/Editor/EditorLayout.cpp
        src/Editor/EditorLayoutService.cpp
        src/Editor/EditorPieService.cpp
        src/Editor/EditorRootViewportService.cpp
        src/Editor/EditorSceneService.cpp
        src/Editor/UIHelpTooltip.cpp
        src/Editor/EditorSelectionInteractionService.cpp
        src/Editor/EditorSelectionModel.cpp
        src/Editor/EditorSelectionService.cpp
        src/Editor/EditorSceneBootstrap.cpp
        src/Editor/EditorTheme.cpp
        src/Editor/EditorThemeService.cpp
        src/Editor/EditorTypeRegistration.cpp
        src/Editor/EditorTransformInteractionService.cpp
        src/Editor/EditorViewportBinding.cpp
        src/Editor/EditorWorld.cpp
        src/Editor/GameEditor.cpp
    )

    add_executable(SnAPI.GameFramework.Editor ${SNAPI_GF_EDITOR_SOURCES})
    target_link_libraries(SnAPI.GameFramework.Editor PRIVATE SnAPI.GameFramework)
    target_compile_definitions(SnAPI.GameFramework.Editor PRIVATE WITH_EDITOR=1)

    set(SNAPI_GF_EDITOR_ASSET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/Editor/Assets")
    set(SNAPI_GF_EDITOR_ASSET_OUTPUT_DEFAULT "")
    if (WIN32)
        if (DEFINED ENV{LOCALAPPDATA} AND NOT "$ENV{LOCALAPPDATA}" STREQUAL "")
            set(SNAPI_GF_EDITOR_ASSET_OUTPUT_DEFAULT "$ENV{LOCALAPPDATA}/SnAPI/GameFramework/Editor/Assets")
        elseif (DEFINED ENV{APPDATA} AND NOT "$ENV{APPDATA}" STREQUAL "")
            set(SNAPI_GF_EDITOR_ASSET_OUTPUT_DEFAULT "$ENV{APPDATA}/SnAPI/GameFramework/Editor/Assets")
        endif()
    elseif (APPLE)
        if (DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
            set(SNAPI_GF_EDITOR_ASSET_OUTPUT_DEFAULT "$ENV{HOME}/Library/Application Support/SnAPI/GameFramework/Editor/Assets")
        endif()
    else()
        if (DEFINED ENV{XDG_DATA_HOME} AND NOT "$ENV{XDG_DATA_HOME}" STREQUAL "")
            set(SNAPI_GF_EDITOR_ASSET_OUTPUT_DEFAULT "$ENV{XDG_DATA_HOME}/SnAPI/GameFramework/Editor/Assets")
        elseif (DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
            set(SNAPI_GF_EDITOR_ASSET_OUTPUT_DEFAULT "$ENV{HOME}/.local/share/SnAPI/GameFramework/Editor/Assets")
        endif()
    endif()

    set(SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR
        "${SNAPI_GF_EDITOR_ASSET_OUTPUT_DEFAULT}"
        CACHE PATH "Destination directory for editor template assets")

    if (EXISTS "${SNAPI_GF_EDITOR_ASSET_SOURCE_DIR}")
        if (SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR)
            add_custom_command(TARGET SnAPI.GameFramework.Editor POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${SNAPI_GF_EDITOR_ASSET_SOURCE_DIR}"
                    "${SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR}"
                COMMENT "Copying editor assets to ${SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR}"
                VERBATIM
            )
        else()
            message(STATUS "SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR is empty; skipping editor asset copy step.")
        endif()
    endif()
endif()

if (SNAPI_PROJECT_ROOT_DIR)
    get_filename_component(SNAPI_PROJECT_ROOT_DIR "${SNAPI_PROJECT_ROOT_DIR}" ABSOLUTE)
    file(TO_CMAKE_PATH "${SNAPI_PROJECT_ROOT_DIR}" SNAPI_PROJECT_ROOT_DIR)
    set(SNAPI_GF_EXTERNAL_PROJECT_CODE_DIR "${SNAPI_PROJECT_ROOT_DIR}/Code")
    if (EXISTS "${SNAPI_GF_EXTERNAL_PROJECT_CODE_DIR}/CMakeLists.txt")
        add_subdirectory("${SNAPI_GF_EXTERNAL_PROJECT_CODE_DIR}" "${CMAKE_BINARY_DIR}/ExternalProject/Code")
    else()
        message(WARNING
            "SNAPI_PROJECT_ROOT_DIR was set to '${SNAPI_PROJECT_ROOT_DIR}', but "
            "'${SNAPI_GF_EXTERNAL_PROJECT_CODE_DIR}/CMakeLists.txt' was not found. "
            "External project modules will not be integrated.")
    endif()
endif()

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
    target_include_directories(SnAPI.GameFramework BEFORE PUBLIC ${lua_SOURCE_DIR})
    target_link_libraries(SnAPI.GameFramework PUBLIC lua)
    target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_LUA=1)
endif()

# AssetPipeline integration (required)
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

target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.AssetPipeline)
target_link_libraries(SnAPI.GameFramework PUBLIC TextureCompressorPlugin)
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

# SnAPI.Physics integration (required)
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
target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.Physics)
target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_PHYSICS)

# SnAPI.Input integration (required)
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
    # SDL3 statically embeds HIDAPI symbols; standalone HIDAPI backend links hidapi as well.
    # Disable SDL's internal HIDAPI path to avoid duplicate symbols (e.g. hid_version/hid_version_str).
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
target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.Input)
if (TARGET SnAPI.Input.Backend.SDL3)
    target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.Input.Backend.SDL3)
endif()
if (TARGET SnAPI.Input.HID.Backend.HIDAPI)
    target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.Input.HID.Backend.HIDAPI)
endif()
if (TARGET SnAPI.Input.USB.Backend.LIBUSB)
    target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.Input.USB.Backend.LIBUSB)
endif()
target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_INPUT)

# SnAPI.UI integration (required)
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
target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.UI)
target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_UI)

# SnAPI.Renderer integration (required when source is available)
set(SNAPI_RENDERER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SNAPI_GF_RENDERER_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.Renderer")
set(SNAPI_RENDERER_NEW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SNAPI_RENDERER_NEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SNAPI_GF_RENDERER_NEW_SOURCE_DIR "" CACHE PATH "Local override for SnAPI.Renderer.New")

set(SNAPI_GF_RENDERER_ROOT "")
if (SNAPI_GF_RENDERER_SOURCE_DIR)
    set(SNAPI_GF_RENDERER_ROOT ${SNAPI_GF_RENDERER_SOURCE_DIR})
elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.Renderer/CMakeLists.txt")
    set(SNAPI_GF_RENDERER_ROOT "/mnt/Dev/CodeProjects/SnAPI.Renderer")
endif()

if (SNAPI_GF_ENABLE_LEGACY_RENDERER AND SNAPI_GF_RENDERER_ROOT)
    if (TARGET lz4)
        set(SLANG_USE_SYSTEM_LZ4 ON CACHE BOOL "" FORCE)
    endif()
    set(SNAPI_GF_RENDERER_SHADER_SOURCE_DIR "${SNAPI_GF_RENDERER_ROOT}/shaders")
    file(TO_CMAKE_PATH "${SNAPI_GF_RENDERER_SHADER_SOURCE_DIR}" SNAPI_GF_RENDERER_SHADER_SOURCE_DIR)
    FetchContent_Declare(
        SnAPI.Renderer
        SOURCE_DIR ${SNAPI_GF_RENDERER_ROOT}
    )
    FetchContent_MakeAvailable(SnAPI.Renderer)

    if (TARGET SnAPI.Renderer)
        # SnAPI.Renderer uses fmt internally but does not currently declare it on the target.
        # Link the packaged library when available and fall back to header-only mode otherwise.
        find_package(fmt CONFIG QUIET)
        if (TARGET fmt::fmt)
            target_link_libraries(SnAPI.Renderer PUBLIC fmt::fmt)
        elseif (TARGET fmt::fmt-header-only)
            target_link_libraries(SnAPI.Renderer PUBLIC fmt::fmt-header-only)
        else()
            target_compile_definitions(SnAPI.Renderer PRIVATE FMT_HEADER_ONLY)
        endif()

        target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.Renderer)
        target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_RENDERER SNAPI_GF_ENABLE_LEGACY_RENDERER)
        target_compile_definitions(SnAPI.GameFramework PRIVATE SNAPI_GF_RENDERER_SHADER_SOURCE_DIR="${SNAPI_GF_RENDERER_SHADER_SOURCE_DIR}")
        if (SNAPI_GF_BUILD_EDITOR AND TARGET SnAPI.GameFramework.Editor)
            if (SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR)
                add_custom_command(TARGET SnAPI.GameFramework.Editor POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR}/Shaders"
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${SNAPI_GF_RENDERER_SHADER_SOURCE_DIR}"
                        "${SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR}/Shaders"
                    COMMENT "Copying renderer shaders to ${SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR}/Shaders"
                    VERBATIM
                )
            else()
                message(STATUS "SNAPI_GF_EDITOR_ASSET_OUTPUT_DIR is empty; skipping renderer shader copy step.")
            endif()
        endif()
    else()
        message(FATAL_ERROR "SnAPI.Renderer source was found, but target SnAPI.Renderer was not created.")
    endif()
elseif(NOT SNAPI_GF_USE_RENDERER_NEW)
    message(WARNING "SnAPI.Renderer source not found; set SNAPI_GF_RENDERER_SOURCE_DIR to enable renderer integration.")
endif()

# SnAPI.Renderer.New workspace integration.
set(SNAPI_GF_RENDERER_NEW_ROOT "")
if (SNAPI_GF_RENDERER_NEW_SOURCE_DIR)
    set(SNAPI_GF_RENDERER_NEW_ROOT ${SNAPI_GF_RENDERER_NEW_SOURCE_DIR})
elseif (EXISTS "/mnt/Dev/CodeProjects/SnAPI.Renderer.New/CMakeLists.txt")
    set(SNAPI_GF_RENDERER_NEW_ROOT "/mnt/Dev/CodeProjects/SnAPI.Renderer.New")
endif()

if (SNAPI_GF_INCLUDE_RENDERER_NEW_WORKSPACE AND SNAPI_GF_RENDERER_NEW_ROOT)
    FetchContent_Declare(
        SnAPI.Renderer.New
        SOURCE_DIR ${SNAPI_GF_RENDERER_NEW_ROOT}
    )
    FetchContent_MakeAvailable(SnAPI.Renderer.New)

    if (TARGET SnAPI.Renderer.New)
        add_dependencies(SnAPI.GameFramework SnAPI.Renderer.New)
        if (SNAPI_GF_USE_RENDERER_NEW)
            target_link_libraries(SnAPI.GameFramework PUBLIC SnAPI.Renderer.New)
            target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_RENDERER SNAPI_GF_ENABLE_RENDERER_NEW)
            set_source_files_properties(src/RendererSystemRendererNew.cpp PROPERTIES
                COMPILE_OPTIONS "-iquote;${SNAPI_GF_RENDERER_NEW_ROOT}/Modules/RenderCore/Public;-iquote;${SNAPI_GF_RENDERER_NEW_ROOT}/Modules/Platform/Public"
            )

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

            snapi_gf_link_renderer_new_static_group(SnAPI.GameFramework.Runtime)
            snapi_gf_link_renderer_new_static_group(SnAPI.GameFramework.Build)
            snapi_gf_link_renderer_new_static_group(SnAPI.GameFramework.Editor)
        endif()
    else()
        message(FATAL_ERROR "SnAPI.Renderer.New source was found, but target SnAPI.Renderer.New was not created.")
    endif()
endif()

if (TARGET FreeImage)
    target_link_libraries(SnAPI.GameFramework PRIVATE FreeImage)
    target_compile_definitions(SnAPI.GameFramework PRIVATE SNAPI_GF_HAS_FREEIMAGE=1 HAS_FREEIMAGE=1)
else()
    target_compile_definitions(SnAPI.GameFramework PRIVATE SNAPI_GF_HAS_FREEIMAGE=0 HAS_FREEIMAGE=0)
endif()

# SWIG bindings (required for Lua scripting)
if (SNAPI_GF_ENABLE_SWIG)
    find_package(SWIG REQUIRED)
    include(UseSWIG)

    if (SNAPI_GF_ENABLE_LUA)
        set(SNAPI_GF_SWIG_INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/swig/SnAPIGameFramework.i")
        if (EXISTS "${SNAPI_GF_SWIG_INTERFACE}")
            set_source_files_properties("${SNAPI_GF_SWIG_INTERFACE}" PROPERTIES
                CPLUSPLUS ON
                SWIG_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
            )
            set(CMAKE_SWIG_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/swig/lua")

            swig_add_library(snapi_gf_lua
                TYPE STATIC
                LANGUAGE lua
                SOURCES "${SNAPI_GF_SWIG_INTERFACE}"
            )

            target_include_directories(${SWIG_MODULE_snapi_gf_lua_REAL_NAME}
                PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/include
            )

            target_link_libraries(${SWIG_MODULE_snapi_gf_lua_REAL_NAME}
                PRIVATE
                    lua
                    SnAPI.GameFramework
            )

            target_link_libraries(SnAPI.GameFramework PRIVATE ${SWIG_MODULE_snapi_gf_lua_REAL_NAME})
        else()
            message(FATAL_ERROR "SNAPI_GF_ENABLE_SWIG is ON but ${SNAPI_GF_SWIG_INTERFACE} was not found.")
        endif()
    else()
        message(STATUS "SNAPI_GF_ENABLE_SWIG is ON but SNAPI_GF_ENABLE_LUA is OFF; scripting wrappers are skipped.")
    endif()
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
