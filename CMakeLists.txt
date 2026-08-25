cmake_minimum_required(VERSION 3.10)
project(broadside)
set(CMAKE_CXX_STANDARD 17)

add_executable(broadside
    src/main.cpp
    src/glad.c
)

target_include_directories(broadside PRIVATE
    include
    external/glm
)

find_package(OpenGL REQUIRED)

if(WIN32)
    target_include_directories(broadside PRIVATE external/glfw/include)
    target_link_libraries(broadside
        ${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2022/glfw3.lib
        OpenGL::GL)
else()
    find_package(glfw3 REQUIRED)
    target_link_libraries(broadside glfw OpenGL::GL dl)
endif()

# Copy shaders next to the executable so relative paths work
add_custom_command(TARGET broadside POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:broadside>/shaders)
