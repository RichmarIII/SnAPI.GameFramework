add_executable(GameFrameworkTests
    AuthoredAssetTests.cpp
    ConduitTests.cpp
    EcsOnlyWorldTests.cpp
    WorldEcsRuntimeTests.cpp
    SerializationTests.cpp
    LevelWorldSerializationTests.cpp
    RenderAssetAssimpImporterTests.cpp
    VariantTests.cpp
)

target_link_libraries(GameFrameworkTests PRIVATE SnAPI.GameFramework Catch2::Catch2WithMain)

target_include_directories(GameFrameworkTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if (SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR)
    target_include_directories(GameFrameworkTests PRIVATE ${SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR})
endif()

include(CTest)
include(Catch)
catch_discover_tests(GameFrameworkTests)

add_executable(GameFrameworkEditorTests
    SourceAssetEditorTests.cpp
    ConduitEditorTests.cpp
    ../src/Editor/EditorAssetService.cpp
    ../src/Editor/ConduitEditor.cpp
)

target_link_libraries(GameFrameworkEditorTests PRIVATE SnAPI.GameFramework Catch2::Catch2WithMain)
target_include_directories(GameFrameworkEditorTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if (SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR)
    target_include_directories(GameFrameworkEditorTests PRIVATE ${SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR})
endif()
catch_discover_tests(GameFrameworkEditorTests)

add_executable(ConduitPerf
    ConduitPerf.cpp
    ConduitPerfNative.cpp
)

target_link_libraries(ConduitPerf PRIVATE SnAPI.GameFramework)
target_include_directories(ConduitPerf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(ConduitPerf PRIVATE SNAPI_BENCH_BUILD_LABEL="${CMAKE_BUILD_TYPE}")
if (SNAPI_ENABLE_IPO AND SNAPI_IPO_SUPPORTED)
    set_target_properties(ConduitPerf PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
    )
endif()
if (SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR)
    target_include_directories(ConduitPerf PRIVATE ${SNAPI_GF_TEXTURE_COMPRESSOR_PLUGIN_INCLUDE_DIR})
endif()

add_executable(GameThreadingTests
    GameThreadingTests.cpp
)

target_link_libraries(GameThreadingTests PRIVATE Catch2::Catch2WithMain)
target_include_directories(GameThreadingTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
catch_discover_tests(GameThreadingTests)
