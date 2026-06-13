include("${CMAKE_CURRENT_LIST_DIR}/Dependencies.cmake")

add_executable(SnAPI.GameFramework.ReflectionGen
    Private/ReflectionGen.cpp
)

target_include_directories(SnAPI.GameFramework.ReflectionGen
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Private"
)

target_link_libraries(SnAPI.GameFramework.ReflectionGen
    PRIVATE
        SnAPI.GameFramework.FeatureSet
)

SnAPIGameFrameworkApplyReflectionGenDependencies(SnAPI.GameFramework.ReflectionGen)
