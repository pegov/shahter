cmake_minimum_required(VERSION 3.10)

project(shahter
    VERSION 0.0.1
)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Freetype REQUIRED)

add_executable(shahter
        src/shader.cpp
        src/main.cpp
    )

target_include_directories(shahter PRIVATE ${THIRD_PARTY_DIR}/include ${FREETYPE_INCLUDE_DIRS})
target_link_directories(shahter PRIVATE ${THIRD_PARTY_DIR}/lib)

target_link_libraries(shahter PRIVATE glfw3 GLEW EGL GL GLU OpenGL pthread X11 assimp z ${FREETYPE_LIBRARIES})

if (${CMAKE_BUILD_TYPE} MATCHES Debug)
    target_compile_definitions(shahter PRIVATE SR_DEBUG)
endif()
