include("${CMAKE_CURRENT_LIST_DIR}/Dependencies.cmake")

add_executable(SnAPI.GameFramework.Build
    Private/main.cpp
)

target_include_directories(SnAPI.GameFramework.Build
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Private"
)

target_link_libraries(SnAPI.GameFramework.Build
    PRIVATE
        SnAPI.GameFramework.FeatureSet
)

SnAPIGameFrameworkApplyBuildDependencies(SnAPI.GameFramework.Build)
