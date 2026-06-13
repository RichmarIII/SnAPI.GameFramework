include("${CMAKE_CURRENT_LIST_DIR}/Dependencies.cmake")

file(GLOB_RECURSE SNAPI_GF_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp"
)

add_library(SnAPI.GameFramework ${SNAPI_GF_SOURCES})

target_include_directories(SnAPI.GameFramework
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Public>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Private"
)

target_link_libraries(SnAPI.GameFramework
    PUBLIC
        SnAPI.GameFramework.FeatureSet
)

SnAPIGameFrameworkApplyBaseDependencies(SnAPI.GameFramework)

set(SNAPI_GF_DEFAULT_ASSET_ROOT "${CMAKE_BINARY_DIR}/Assets")
file(TO_CMAKE_PATH "${SNAPI_GF_DEFAULT_ASSET_ROOT}" SNAPI_GF_DEFAULT_ASSET_ROOT)
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}" SNAPI_GF_SOURCE_ROOT_DIR)
target_compile_definitions(SnAPI.GameFramework PUBLIC
    SNAPI_GF_DEFAULT_ASSET_ROOT="${SNAPI_GF_DEFAULT_ASSET_ROOT}"
)
target_compile_definitions(SnAPI.GameFramework PRIVATE
    SNAPI_GF_SOURCE_ROOT_DIR="${SNAPI_GF_SOURCE_ROOT_DIR}"
    SNAPI_GAMEFRAMEWORK_BUILD_DLL
)

if (SNAPI_GF_BUILD_EDITOR)
    target_compile_definitions(SnAPI.GameFramework PUBLIC WITH_EDITOR=1)
else()
    target_compile_definitions(SnAPI.GameFramework PUBLIC WITH_EDITOR=0)
endif()

if (SNAPI_GF_ENABLE_SWIG)
    target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_SWIG=1)
else()
    target_compile_definitions(SnAPI.GameFramework PUBLIC SNAPI_GF_ENABLE_SWIG=0)
endif()

if (SNAPI_GF_BUILD_REFLECTION_GEN)
    file(GLOB_RECURSE SNAPI_GF_REFLECTION_HEADERS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.hpp")

    snapi_add_reflection_codegen(
        TARGET SnAPI.GameFramework
        OUTPUT SnAPI.GameFramework.reflection.generated.cpp
        BUILD_DIR "${CMAKE_BINARY_DIR}"
        SEED_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Private/GameFramework.cpp"
        PROJECT_ROOT "${PROJECT_SOURCE_DIR}"
        HEADER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Public"
        GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/reflection/include"
        EXTRA_ARGS
            "-I${CMAKE_CURRENT_SOURCE_DIR}/Public"
            "-I${SNAPI_GF_MATH_SOURCE_ROOT}/include"
            "-I${stduuid_SOURCE_DIR}/include"
        HEADERS ${SNAPI_GF_REFLECTION_HEADERS}
    )
endif()
