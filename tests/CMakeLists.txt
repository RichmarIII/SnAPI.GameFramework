add_executable(GameFrameworkTests
    ComponentStorageFallbackTests.cpp
    HandleTests.cpp
    GameRuntimeTests.cpp
    GameplayHostTests.cpp
    InputSystemTests.cpp
    LevelWorldSerializationTests.cpp
    ReflectionTests.cpp
    NetReplicationTests.cpp
    NodeGraphTests.cpp
    PhysicsIntegrationTests.cpp
    RelevanceTests.cpp
    SerializationTests.cpp
    WorldNetworkingTests.cpp
)

target_link_libraries(GameFrameworkTests PRIVATE SnAPI.GameFramework Catch2::Catch2WithMain)

target_include_directories(GameFrameworkTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(CTest)
include(Catch)
catch_discover_tests(GameFrameworkTests)

add_executable(GameThreadingTests
    GameThreadingTests.cpp
)

target_link_libraries(GameThreadingTests PRIVATE Catch2::Catch2WithMain)
target_include_directories(GameThreadingTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
catch_discover_tests(GameThreadingTests)
