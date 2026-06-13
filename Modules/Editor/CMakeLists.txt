include("${CMAKE_CURRENT_LIST_DIR}/Dependencies.cmake")

file(GLOB_RECURSE SNAPI_GF_EDITOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp"
)

add_executable(SnAPI.GameFramework.Editor ${SNAPI_GF_EDITOR_SOURCES})

target_include_directories(SnAPI.GameFramework.Editor
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Public"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Private"
)

target_link_libraries(SnAPI.GameFramework.Editor
    PRIVATE
        SnAPI.GameFramework.FeatureSet
)
target_compile_definitions(SnAPI.GameFramework.Editor PRIVATE WITH_EDITOR=1)

SnAPIGameFrameworkApplyEditorDependencies(SnAPI.GameFramework.Editor)

set(SNAPI_GF_EDITOR_ASSET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Private/Editor/Assets")
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
