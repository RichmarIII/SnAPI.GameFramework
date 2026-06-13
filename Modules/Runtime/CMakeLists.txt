include("${CMAKE_CURRENT_LIST_DIR}/Dependencies.cmake")

add_executable(SnAPI.GameFramework.Runtime
    Private/main.cpp
)

target_include_directories(SnAPI.GameFramework.Runtime
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Private"
)

target_link_libraries(SnAPI.GameFramework.Runtime
    PRIVATE
        SnAPI.GameFramework.FeatureSet
)

SnAPIGameFrameworkApplyRuntimeDependencies(SnAPI.GameFramework.Runtime)
