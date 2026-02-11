find_package(OpenGL REQUIRED)

add_executable(MultiplayerExample
    main.cpp
)

target_link_libraries(MultiplayerExample PRIVATE SnAPI.GameFramework glfw OpenGL::GL)
