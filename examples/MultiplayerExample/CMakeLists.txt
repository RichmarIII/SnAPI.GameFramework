add_executable(MultiplayerExample
    main.cpp
)

target_link_libraries(MultiplayerExample PRIVATE SnAPI.GameFramework)

add_custom_command(TARGET MultiplayerExample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:MultiplayerExample>/assets
    COMMENT "Copy MultiplayerExample assets"
)
