add_executable(GameFrameworkTests
    HandleTests.cpp
    GameRuntimeTests.cpp
    LevelWorldSerializationTests.cpp
    ReflectionTests.cpp
    NetReplicationTests.cpp
    NodeGraphTests.cpp
    RelevanceTests.cpp
    SerializationTests.cpp
    WorldNetworkingTests.cpp
)

target_link_libraries(GameFrameworkTests PRIVATE SnAPI.GameFramework Catch2::Catch2WithMain)

target_include_directories(GameFrameworkTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(CTest)
include(Catch)
catch_discover_tests(GameFrameworkTests)
