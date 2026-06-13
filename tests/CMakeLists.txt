include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_executable(GameFrameworkTests
    AssetCookServiceAdapterTests.cpp
    AuthoredAssetTests.cpp
    BuildCliTests.cpp
    CodeBuildServiceAdapterTests.cpp
    BuildExecutionTests.cpp
    BuildHistoryTests.cpp
    PackageOutputTests.cpp
    PackageManifestTests.cpp
    BuildProfileTests.cpp
    BuildPlannerTests.cpp
    BuildRequestTests.cpp
    ConduitTests.cpp
    EcsOnlyWorldTests.cpp
    GeneratedReflectionFixture.h
    GameProjectRuntimeTests.cpp
    GameplayHostTests.cpp
    HandleTests.cpp
    ModuleCreationServiceTests.cpp
    PluginCreationServiceTests.cpp
    PluginDescriptorTests.cpp
    ProjectCreationServiceTests.cpp
    ProjectDescriptorTests.cpp
    ReflectionTests.cpp
    WorldEcsRuntimeTests.cpp
    SerializationTests.cpp
    LevelWorldSerializationTests.cpp
    RenderAssetAssimpImporterTests.cpp
    VariantTests.cpp
)

target_link_libraries(GameFrameworkTests PRIVATE SnAPI.GameFramework Catch2::Catch2WithMain)
snapi_gf_link_renderer_new_static_group(GameFrameworkTests)

target_include_directories(GameFrameworkTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if (SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR)
    target_include_directories(GameFrameworkTests PRIVATE ${SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR})
endif()

if (SNAPI_GF_BUILD_REFLECTION_GEN)
    snapi_add_reflection_codegen(
        TARGET GameFrameworkTests
        OUTPUT GameFrameworkTests.reflection.generated.cpp
        BUILD_DIR "${CMAKE_BINARY_DIR}"
        SEED_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/ReflectionTests.cpp"
        PROJECT_ROOT "${CMAKE_SOURCE_DIR}"
        HEADER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
        GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/reflection/tests"
        EXTRA_ARGS
            "-I${CMAKE_SOURCE_DIR}/Modules/GameFramework/Public"
            "-I${CMAKE_CURRENT_SOURCE_DIR}"
        HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/GeneratedReflectionFixture.h"
    )
endif()

include(CTest)
include(Catch)
catch_discover_tests(GameFrameworkTests)

add_executable(GameFrameworkEditorTests
    EditorBuildServiceTests.cpp
    SourceAssetEditorTests.cpp
    ConduitEditorTests.cpp
    EditorServiceTypeInfoStubs.cpp
    ../Modules/Editor/Private/Editor/EditorCameraComponent.cpp
    ../Modules/Editor/Private/Editor/EditorAssetIconService.cpp
    ../Modules/Editor/Private/Editor/EditorAssetService.cpp
    ../Modules/Editor/Private/Editor/EditorBuildService.cpp
    ../Modules/Editor/Private/Editor/EditorPieService.cpp
    ../Modules/Editor/Private/Editor/ConduitEditor.cpp
    ../Modules/Editor/Private/Editor/ConduitGraphCanvas.cpp
)

target_link_libraries(GameFrameworkEditorTests PRIVATE SnAPI.GameFramework Catch2::Catch2WithMain)
snapi_gf_link_renderer_new_static_group(GameFrameworkEditorTests)
target_include_directories(GameFrameworkEditorTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/Editor/Public
    ${CMAKE_SOURCE_DIR}/Modules/Editor/Private
)
if (SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR)
    target_include_directories(GameFrameworkEditorTests PRIVATE ${SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR})
endif()
catch_discover_tests(GameFrameworkEditorTests)

add_executable(ConduitPerf
    ConduitPerf.cpp
    ConduitPerfNative.cpp
)

target_link_libraries(ConduitPerf PRIVATE SnAPI.GameFramework)
snapi_gf_link_renderer_new_static_group(ConduitPerf)
target_include_directories(ConduitPerf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(ConduitPerf PRIVATE SNAPI_BENCH_BUILD_LABEL="${CMAKE_BUILD_TYPE}")
if (SNAPI_ENABLE_IPO AND SNAPI_IPO_SUPPORTED)
    set_target_properties(ConduitPerf PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION FALSE
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE
        INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL FALSE
        INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE
    )
endif()
if (SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR)
    target_include_directories(ConduitPerf PRIVATE ${SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR})
endif()

add_executable(EcsStoragePerf
    EcsStoragePerf.cpp
)
target_compile_definitions(EcsStoragePerf PRIVATE SNAPI_BENCH_BUILD_LABEL="${CMAKE_BUILD_TYPE}")
if (SNAPI_ENABLE_IPO AND SNAPI_IPO_SUPPORTED)
    set_target_properties(EcsStoragePerf PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION FALSE
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE
        INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL FALSE
        INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE
    )
endif()

add_executable(GameThreadingTests
    GameThreadingTests.cpp
)

target_link_libraries(GameThreadingTests PRIVATE Catch2::Catch2WithMain)
target_include_directories(GameThreadingTests PRIVATE ${CMAKE_SOURCE_DIR}/Modules/GameFramework/Public)
catch_discover_tests(GameThreadingTests)
